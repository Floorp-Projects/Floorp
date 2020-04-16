/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/JSExecutionManager.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/PerformanceUtils.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/ThrottledEventQueue.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Telemetry.h"
#include "nsDOMMutationObserver.h"
#include "nsProxyRelease.h"
#if defined(XP_WIN)
#  include <processthreadsapi.h>  // for GetCurrentProcessId()
#else
#  include <unistd.h>  // for getpid()
#endif                 // defined(XP_WIN)

namespace {

#define NS_LABELLINGEVENTTARGET_IID                  \
  {                                                  \
    0x6087fa50, 0xe387, 0x45c8, {                    \
      0xab, 0x72, 0xd2, 0x1f, 0x69, 0xee, 0xd3, 0x15 \
    }                                                \
  }

// LabellingEventTarget labels all dispatches with the DocGroup that
// created it.
class LabellingEventTarget final : public nsISerialEventTarget {
  // This creates a cycle with DocGroup. Therefore, when DocGroup
  // looses its last Document, the DocGroup of the
  // LabellingEventTarget needs to be cleared.
  RefPtr<mozilla::dom::DocGroup> mDocGroup;

 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_LABELLINGEVENTTARGET_IID)

  explicit LabellingEventTarget(mozilla::dom::DocGroup* aDocGroup)
      : mDocGroup(aDocGroup) {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET_FULL

 private:
  ~LabellingEventTarget() = default;
};

NS_DEFINE_STATIC_IID_ACCESSOR(LabellingEventTarget, NS_LABELLINGEVENTTARGET_IID)

}  // namespace

NS_IMETHODIMP
LabellingEventTarget::DispatchFromScript(nsIRunnable* aRunnable,
                                         uint32_t aFlags) {
  return Dispatch(do_AddRef(aRunnable), aFlags);
}

NS_IMETHODIMP
LabellingEventTarget::Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                               uint32_t aFlags) {
  if (NS_WARN_IF(aFlags != NS_DISPATCH_NORMAL)) {
    return NS_ERROR_UNEXPECTED;
  }

  return mozilla::SchedulerGroup::DispatchWithDocGroup(
      mozilla::TaskCategory::Other, std::move(aRunnable), mDocGroup);
}

NS_IMETHODIMP
LabellingEventTarget::DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LabellingEventTarget::IsOnCurrentThread(bool* aIsOnCurrentThread) {
  *aIsOnCurrentThread = NS_IsMainThread();
  return NS_OK;
}

NS_IMETHODIMP_(bool)
LabellingEventTarget::IsOnCurrentThreadInfallible() {
  return NS_IsMainThread();
}

NS_IMPL_ISUPPORTS(LabellingEventTarget, LabellingEventTarget, nsIEventTarget,
                  nsISerialEventTarget)

namespace mozilla {
namespace dom {

AutoTArray<RefPtr<DocGroup>, 2>* DocGroup::sPendingDocGroups = nullptr;

/* static */
already_AddRefed<DocGroup> DocGroup::Create(
    BrowsingContextGroup* aBrowsingContextGroup, const nsACString& aKey) {
  RefPtr<DocGroup> docGroup = new DocGroup(aBrowsingContextGroup, aKey);
  docGroup->mEventTarget = new LabellingEventTarget(docGroup);
  return docGroup.forget();
}

/* static */
nsresult DocGroup::GetKey(nsIPrincipal* aPrincipal, nsACString& aKey) {
  // Use GetBaseDomain() to handle things like file URIs, IP address URIs,
  // etc. correctly.
  nsresult rv = aPrincipal->GetBaseDomain(aKey);
  if (NS_FAILED(rv)) {
    // We don't really know what to do here.  But we should be conservative,
    // otherwise it would be possible to reorder two events incorrectly in the
    // future if we interrupt at the DocGroup level, so to be safe, use an
    // empty string to classify all such documents as belonging to the same
    // DocGroup.
    aKey.Truncate();
  }

  return rv;
}

void DocGroup::SetExecutionManager(JSExecutionManager* aManager) {
  mExecutionManager = aManager;
}

void DocGroup::AddDocument(Document* aDocument) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mDocuments.Contains(aDocument));
  MOZ_ASSERT(mBrowsingContextGroup);
  mDocuments.AppendElement(aDocument);
}

void DocGroup::RemoveDocument(Document* aDocument) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDocuments.Contains(aDocument));
  mDocuments.RemoveElement(aDocument);

  if (mDocuments.IsEmpty()) {
    mBrowsingContextGroup = nullptr;
    // This clears the cycle DocGroup has with LabellingEventTarget.
    mEventTarget = nullptr;
    mAbstractThread = nullptr;
  }
}

DocGroup::DocGroup(BrowsingContextGroup* aBrowsingContextGroup,
                   const nsACString& aKey)
    : mKey(aKey),
      mBrowsingContextGroup(aBrowsingContextGroup),
      mAgentClusterId(nsContentUtils::GenerateUUID()) {
  // This method does not add itself to
  // mBrowsingContextGroup->mDocGroups as the caller does it for us.
  MOZ_ASSERT(NS_IsMainThread());
  if (StaticPrefs::dom_arena_allocator_enabled_AtStartup()) {
    mArena = new mozilla::dom::DOMArena();
  }

  mPerformanceCounter =
      new mozilla::PerformanceCounter(NS_LITERAL_CSTRING("DocGroup:") + aKey);
}

DocGroup::~DocGroup() {
  MOZ_RELEASE_ASSERT(mDocuments.IsEmpty());
  MOZ_RELEASE_ASSERT(!mBrowsingContextGroup);

  if (!NS_IsMainThread()) {
    nsIEventTarget* target = EventTargetFor(TaskCategory::Other);
    NS_ProxyRelease("DocGroup::mReactionsStack", target,
                    mReactionsStack.forget());
  }

  if (mIframePostMessageQueue) {
    FlushIframePostMessageQueue();
  }
}

RefPtr<PerformanceInfoPromise> DocGroup::ReportPerformanceInfo() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mPerformanceCounter);
#if defined(XP_WIN)
  uint32_t pid = GetCurrentProcessId();
#else
  uint32_t pid = getpid();
#endif
  uint64_t windowID = 0;
  uint16_t count = 0;
  uint64_t duration = 0;
  bool isTopLevel = false;
  nsCString host;
  nsCOMPtr<nsPIDOMWindowOuter> top;
  RefPtr<AbstractThread> mainThread;

  // iterating on documents until we find the top window
  for (const auto& document : *this) {
    nsCOMPtr<Document> doc = document;
    MOZ_ASSERT(doc);
    nsCOMPtr<nsIURI> docURI = doc->GetDocumentURI();
    if (!docURI) {
      continue;
    }
    docURI->GetHost(host);
    // If the host is empty, using the url
    if (host.IsEmpty()) {
      host = docURI->GetSpecOrDefault();
    }
    // looking for the top level document URI
    nsPIDOMWindowOuter* win = doc->GetWindow();
    if (!win) {
      continue;
    }
    top = win->GetInProcessTop();
    if (!top) {
      continue;
    }
    windowID = top->WindowID();
    isTopLevel = win->IsTopLevelWindow();
    mainThread = AbstractMainThreadFor(TaskCategory::Performance);
    break;
  }

  MOZ_ASSERT(!host.IsEmpty());
  duration = mPerformanceCounter->GetExecutionDuration();
  FallibleTArray<CategoryDispatch> items;

  // now that we have the host and window ids, let's look at the perf counters
  for (uint32_t index = 0; index < (uint32_t)TaskCategory::Count; index++) {
    TaskCategory category = static_cast<TaskCategory>(index);
    count = mPerformanceCounter->GetDispatchCount(DispatchCategory(category));
    CategoryDispatch item = CategoryDispatch(index, count);
    if (!items.AppendElement(item, fallible)) {
      NS_ERROR("Could not complete the operation");
      break;
    }
  }

  if (!isTopLevel) {
    return PerformanceInfoPromise::CreateAndResolve(
        PerformanceInfo(host, pid, windowID, duration,
                        mPerformanceCounter->GetID(), false, isTopLevel,
                        PerformanceMemoryInfo(),  // Empty memory info
                        items),
        __func__);
  }

  MOZ_ASSERT(mainThread);
  RefPtr<DocGroup> self = this;

  return CollectMemoryInfo(top, mainThread)
      ->Then(
          mainThread, __func__,
          [self, host, pid, windowID, duration, isTopLevel,
           items](const PerformanceMemoryInfo& aMemoryInfo) {
            PerformanceInfo info =
                PerformanceInfo(host, pid, windowID, duration,
                                self->mPerformanceCounter->GetID(), false,
                                isTopLevel, aMemoryInfo, items);

            return PerformanceInfoPromise::CreateAndResolve(std::move(info),
                                                            __func__);
          },
          [self](const nsresult rv) {
            return PerformanceInfoPromise::CreateAndReject(rv, __func__);
          });
}

nsresult DocGroup::Dispatch(TaskCategory aCategory,
                            already_AddRefed<nsIRunnable>&& aRunnable) {
  if (mPerformanceCounter) {
    mPerformanceCounter->IncrementDispatchCounter(DispatchCategory(aCategory));
  }
  return SchedulerGroup::DispatchWithDocGroup(aCategory, std::move(aRunnable),
                                              this);
}

nsISerialEventTarget* DocGroup::EventTargetFor(TaskCategory aCategory) const {
  MOZ_ASSERT(!mDocuments.IsEmpty());
  // Here we have the same event target for every TaskCategory. The
  // reason for that is that currently TaskCategory isn't used, and
  // it's unsure if it ever will be (See Bug 1624819).
  if (mEventTarget) {
    return mEventTarget;
  }

  return GetMainThreadSerialEventTarget();
}

AbstractThread* DocGroup::AbstractMainThreadFor(TaskCategory aCategory) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mDocuments.IsEmpty());

  if (!mEventTarget) {
    return AbstractThread::MainThread();
  }

  // Here we have the same thread for every TaskCategory. The reason
  // for that is that currently TaskCategory isn't used, and it's
  // unsure if it ever will be (See Bug 1624819).
  if (!mAbstractThread) {
    mAbstractThread = AbstractThread::CreateEventTargetWrapper(
        mEventTarget,
        /* aRequireTailDispatch = */ true);
  }

  return mAbstractThread;
}

void DocGroup::SignalSlotChange(HTMLSlotElement& aSlot) {
  MOZ_ASSERT(!mSignalSlotList.Contains(&aSlot));
  mSignalSlotList.AppendElement(&aSlot);

  if (!sPendingDocGroups) {
    // Queue a mutation observer compound microtask.
    nsDOMMutationObserver::QueueMutationObserverMicroTask();
    sPendingDocGroups = new AutoTArray<RefPtr<DocGroup>, 2>;
  }

  sPendingDocGroups->AppendElement(this);
}

bool DocGroup::TryToLoadIframesInBackground() {
  return StaticPrefs::dom_separate_event_queue_for_post_message_enabled() &&
         StaticPrefs::dom_cross_origin_iframes_loaded_in_background();
}

nsresult DocGroup::QueueIframePostMessages(
    already_AddRefed<nsIRunnable>&& aRunnable, uint64_t aWindowId) {
  if (DocGroup::TryToLoadIframesInBackground()) {
    if (!mIframePostMessageQueue) {
      nsCOMPtr<nsISerialEventTarget> target = GetMainThreadSerialEventTarget();
      mIframePostMessageQueue = ThrottledEventQueue::Create(
          target, "Background Loading Iframe PostMessage Queue",
          nsIRunnablePriority::PRIORITY_DEFERRED_TIMERS);
      nsresult rv = mIframePostMessageQueue->SetIsPaused(true);
      MOZ_ALWAYS_SUCCEEDS(rv);
    }

    // Ensure the queue is disabled. Unlike the postMessageEvent queue
    // in BrowsingContextGroup, this postMessage queue should always
    // be paused, because if we leave it open, the postMessage may get
    // dispatched to an unloaded iframe
    MOZ_ASSERT(mIframePostMessageQueue);
    MOZ_ASSERT(mIframePostMessageQueue->IsPaused());

    mIframesUsedPostMessageQueue.PutEntry(aWindowId);

    mIframePostMessageQueue->Dispatch(std::move(aRunnable), NS_DISPATCH_NORMAL);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

void DocGroup::TryFlushIframePostMessages(uint64_t aWindowId) {
  if (DocGroup::TryToLoadIframesInBackground()) {
    mIframesUsedPostMessageQueue.RemoveEntry(aWindowId);
    if (mIframePostMessageQueue && mIframesUsedPostMessageQueue.IsEmpty()) {
      MOZ_ASSERT(mIframePostMessageQueue->IsPaused());
      nsresult rv = mIframePostMessageQueue->SetIsPaused(true);
      MOZ_ALWAYS_SUCCEEDS(rv);
      FlushIframePostMessageQueue();
    }
  }
}

void DocGroup::FlushIframePostMessageQueue() {
  nsCOMPtr<nsIRunnable> event;
  while ((event = mIframePostMessageQueue->GetEvent())) {
    Dispatch(TaskCategory::Other, event.forget());
  }
}

void DocGroup::MoveSignalSlotListTo(nsTArray<RefPtr<HTMLSlotElement>>& aDest) {
  aDest.SetCapacity(aDest.Length() + mSignalSlotList.Length());
  for (RefPtr<HTMLSlotElement>& slot : mSignalSlotList) {
    slot->RemovedFromSignalSlotList();
    aDest.AppendElement(std::move(slot));
  }
  mSignalSlotList.Clear();
}

bool DocGroup::IsActive() const {
  for (Document* doc : mDocuments) {
    if (doc->IsCurrentActiveDocument()) {
      return true;
    }
  }

  return false;
}

}  // namespace dom
}  // namespace mozilla
