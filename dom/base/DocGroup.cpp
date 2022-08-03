/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DocGroup.h"

#include "mozilla/AbstractThread.h"
#include "mozilla/PerformanceUtils.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Telemetry.h"
#include "mozilla/ThrottledEventQueue.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/CustomElementRegistry.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/JSExecutionManager.h"
#include "mozilla/dom/WindowContext.h"
#include "nsDOMMutationObserver.h"
#include "nsIDirectTaskDispatcher.h"
#include "nsIXULRuntime.h"
#include "nsProxyRelease.h"
#include "nsThread.h"
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
class LabellingEventTarget final : public nsISerialEventTarget,
                                   public nsIDirectTaskDispatcher {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_LABELLINGEVENTTARGET_IID)

  explicit LabellingEventTarget(
      mozilla::PerformanceCounter* aPerformanceCounter)
      : mPerformanceCounter(aPerformanceCounter),
        mMainThread(
            static_cast<nsThread*>(mozilla::GetMainThreadSerialEventTarget())) {
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET_FULL
  NS_DECL_NSIDIRECTTASKDISPATCHER

 private:
  ~LabellingEventTarget() = default;
  const RefPtr<mozilla::PerformanceCounter> mPerformanceCounter;
  const RefPtr<nsThread> mMainThread;
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

  return mozilla::SchedulerGroup::LabeledDispatch(
      mozilla::TaskCategory::Other, std::move(aRunnable), mPerformanceCounter);
}

NS_IMETHODIMP
LabellingEventTarget::DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LabellingEventTarget::RegisterShutdownTask(nsITargetShutdownTask*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LabellingEventTarget::UnregisterShutdownTask(nsITargetShutdownTask*) {
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

//-----------------------------------------------------------------------------
// nsIDirectTaskDispatcher
//-----------------------------------------------------------------------------
// We are always running on the main thread, forward to the nsThread's
// MainThread
NS_IMETHODIMP
LabellingEventTarget::DispatchDirectTask(already_AddRefed<nsIRunnable> aEvent) {
  return mMainThread->DispatchDirectTask(std::move(aEvent));
}

NS_IMETHODIMP LabellingEventTarget::DrainDirectTasks() {
  return mMainThread->DrainDirectTasks();
}

NS_IMETHODIMP LabellingEventTarget::HaveDirectTasks(bool* aValue) {
  return mMainThread->HaveDirectTasks(aValue);
}

NS_IMPL_ISUPPORTS(LabellingEventTarget, nsIEventTarget, nsISerialEventTarget,
                  nsIDirectTaskDispatcher)

namespace mozilla::dom {

AutoTArray<RefPtr<DocGroup>, 2>* DocGroup::sPendingDocGroups = nullptr;

NS_IMPL_CYCLE_COLLECTION_CLASS(DocGroup)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DocGroup)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSignalSlotList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowsingContextGroup)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DocGroup)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSignalSlotList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBrowsingContextGroup)

  // If we still have any documents in this array, they were just unlinked, so
  // clear out our weak pointers to them.
  tmp->mDocuments.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(DocGroup, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(DocGroup, Release)

/* static */
already_AddRefed<DocGroup> DocGroup::Create(
    BrowsingContextGroup* aBrowsingContextGroup, const nsACString& aKey) {
  RefPtr<DocGroup> docGroup = new DocGroup(aBrowsingContextGroup, aKey);
  docGroup->mEventTarget =
      new LabellingEventTarget(docGroup->GetPerformanceCounter());
  return docGroup.forget();
}

/* static */
nsresult DocGroup::GetKey(nsIPrincipal* aPrincipal, bool aCrossOriginIsolated,
                          nsACString& aKey) {
  // Use GetBaseDomain() to handle things like file URIs, IP address URIs,
  // etc. correctly.
  nsresult rv = aCrossOriginIsolated ? aPrincipal->GetOrigin(aKey)
                                     : aPrincipal->GetSiteOrigin(aKey);
  if (NS_FAILED(rv)) {
    aKey.Truncate();
  }

  return rv;
}

void DocGroup::SetExecutionManager(JSExecutionManager* aManager) {
  mExecutionManager = aManager;
}

mozilla::dom::CustomElementReactionsStack*
DocGroup::CustomElementReactionsStack() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mReactionsStack) {
    mReactionsStack = new mozilla::dom::CustomElementReactionsStack();
  }

  return mReactionsStack;
}

void DocGroup::AddDocument(Document* aDocument) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mDocuments.Contains(aDocument));
  MOZ_ASSERT(mBrowsingContextGroup);
  // If the document is loaded as data it may not have a container, in which
  // case it can be difficult to determine the BrowsingContextGroup it's
  // associated with. XSLT can also add the document to the DocGroup before it
  // gets a container in some cases, in which case this will be asserted
  // elsewhere.
  MOZ_ASSERT_IF(
      aDocument->GetBrowsingContext(),
      aDocument->GetBrowsingContext()->Group() == mBrowsingContextGroup);
  mDocuments.AppendElement(aDocument);
}

void DocGroup::RemoveDocument(Document* aDocument) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDocuments.Contains(aDocument));
  mDocuments.RemoveElement(aDocument);

  if (mDocuments.IsEmpty()) {
    mBrowsingContextGroup = nullptr;
  }
}

DocGroup::DocGroup(BrowsingContextGroup* aBrowsingContextGroup,
                   const nsACString& aKey)
    : mKey(aKey),
      mBrowsingContextGroup(aBrowsingContextGroup),
      mAgentClusterId(nsID::GenerateUUID()) {
  // This method does not add itself to
  // mBrowsingContextGroup->mDocGroups as the caller does it for us.
  MOZ_ASSERT(NS_IsMainThread());
  if (StaticPrefs::dom_arena_allocator_enabled_AtStartup()) {
    mArena = new mozilla::dom::DOMArena();
  }

  mPerformanceCounter = new mozilla::PerformanceCounter("DocGroup:"_ns + aKey);
}

DocGroup::~DocGroup() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(mDocuments.IsEmpty());

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
  nsCString host;
  bool isTopLevel = false;
  RefPtr<BrowsingContext> top;
  RefPtr<AbstractThread> mainThread =
      AbstractMainThreadFor(TaskCategory::Performance);

  for (const auto& document : *this) {
    if (host.IsEmpty()) {
      nsCOMPtr<nsIURI> docURI = document->GetDocumentURI();
      if (!docURI) {
        continue;
      }

      docURI->GetHost(host);
      if (host.IsEmpty()) {
        host = docURI->GetSpecOrDefault();
      }
    }

    BrowsingContext* context = document->GetBrowsingContext();
    if (!context) {
      continue;
    }

    top = context->Top();

    if (!top || !top->GetCurrentWindowContext()) {
      continue;
    }

    isTopLevel = context->IsTop();
    windowID = top->GetCurrentWindowContext()->OuterWindowId();
    break;
  };

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

  if (!isTopLevel && top && top->IsInProcess()) {
    return PerformanceInfoPromise::CreateAndResolve(
        PerformanceInfo(host, pid, windowID, duration,
                        mPerformanceCounter->GetID(), false, isTopLevel,
                        PerformanceMemoryInfo(),  // Empty memory info
                        items),
        __func__);
  }

  MOZ_ASSERT(mainThread);
  RefPtr<DocGroup> self = this;
  return (isTopLevel ? CollectMemoryInfo(top, mainThread)
                     : CollectMemoryInfo(self, mainThread))
      ->Then(
          mainThread, __func__,
          [self, host, pid, windowID, duration, isTopLevel,
           items = std::move(items)](const PerformanceMemoryInfo& aMemoryInfo) {
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
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (mPerformanceCounter) {
    mPerformanceCounter->IncrementDispatchCounter(DispatchCategory(aCategory));
  }
  return SchedulerGroup::LabeledDispatch(aCategory, std::move(aRunnable),
                                         mPerformanceCounter);
}

nsISerialEventTarget* DocGroup::EventTargetFor(TaskCategory aCategory) const {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mDocuments.IsEmpty());

  // Here we have the same event target for every TaskCategory. The
  // reason for that is that currently TaskCategory isn't used, and
  // it's unsure if it ever will be (See Bug 1624819).
  return mEventTarget;
}

AbstractThread* DocGroup::AbstractMainThreadFor(TaskCategory aCategory) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mDocuments.IsEmpty());

  // Here we have the same thread for every TaskCategory. The reason
  // for that is that currently TaskCategory isn't used, and it's
  // unsure if it ever will be (See Bug 1624819).
  return AbstractThread::MainThread();
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
  return !FissionAutostart() &&
         StaticPrefs::dom_separate_event_queue_for_post_message_enabled() &&
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

    mIframesUsedPostMessageQueue.Insert(aWindowId);

    mIframePostMessageQueue->Dispatch(std::move(aRunnable), NS_DISPATCH_NORMAL);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

void DocGroup::TryFlushIframePostMessages(uint64_t aWindowId) {
  if (DocGroup::TryToLoadIframesInBackground()) {
    mIframesUsedPostMessageQueue.Remove(aWindowId);
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

nsTArray<RefPtr<HTMLSlotElement>> DocGroup::MoveSignalSlotList() {
  for (const RefPtr<HTMLSlotElement>& slot : mSignalSlotList) {
    slot->RemovedFromSignalSlotList();
  }
  return std::move(mSignalSlotList);
}

bool DocGroup::IsActive() const {
  for (Document* doc : mDocuments) {
    if (doc->IsCurrentActiveDocument()) {
      return true;
    }
  }

  return false;
}

}  // namespace mozilla::dom
