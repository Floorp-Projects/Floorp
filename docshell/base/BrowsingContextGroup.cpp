/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowsingContextGroup.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/InputTaskManager.h"
#include "mozilla/dom/BrowsingContextBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/ThrottledEventQueue.h"
#include "nsFocusManager.h"
#include "nsTHashMap.h"

namespace mozilla {
namespace dom {

static StaticRefPtr<BrowsingContextGroup> sChromeGroup;

static StaticAutoPtr<nsTHashMap<uint64_t, RefPtr<BrowsingContextGroup>>>
    sBrowsingContextGroups;

already_AddRefed<BrowsingContextGroup> BrowsingContextGroup::GetOrCreate(
    uint64_t aId) {
  if (!sBrowsingContextGroups) {
    sBrowsingContextGroups =
        new nsTHashMap<nsUint64HashKey, RefPtr<BrowsingContextGroup>>();
    ClearOnShutdown(&sBrowsingContextGroups);
  }

  return do_AddRef(sBrowsingContextGroups->LookupOrInsertWith(
      aId, [&aId] { return do_AddRef(new BrowsingContextGroup(aId)); }));
}

already_AddRefed<BrowsingContextGroup> BrowsingContextGroup::Create() {
  return GetOrCreate(nsContentUtils::GenerateBrowsingContextId());
}

BrowsingContextGroup::BrowsingContextGroup(uint64_t aId) : mId(aId) {
  mTimerEventQueue = ThrottledEventQueue::Create(
      GetMainThreadSerialEventTarget(), "BrowsingContextGroup timer queue");

  mWorkerEventQueue = ThrottledEventQueue::Create(
      GetMainThreadSerialEventTarget(), "BrowsingContextGroup worker queue");
}

void BrowsingContextGroup::Register(nsISupports* aContext) {
  MOZ_ASSERT(!mDestroyed);
  MOZ_DIAGNOSTIC_ASSERT(aContext);
  mContexts.PutEntry(aContext);
}

void BrowsingContextGroup::Unregister(nsISupports* aContext) {
  MOZ_ASSERT(!mDestroyed);
  MOZ_DIAGNOSTIC_ASSERT(aContext);
  mContexts.RemoveEntry(aContext);

  MaybeDestroy();
}

void BrowsingContextGroup::EnsureHostProcess(ContentParent* aProcess) {
  MOZ_ASSERT(!mDestroyed);
  MOZ_DIAGNOSTIC_ASSERT(this != sChromeGroup,
                        "cannot have content host for chrome group");
  MOZ_DIAGNOSTIC_ASSERT(aProcess->GetRemoteType() != PREALLOC_REMOTE_TYPE,
                        "cannot use preallocated process as host");
  MOZ_DIAGNOSTIC_ASSERT(!aProcess->GetRemoteType().IsEmpty(),
                        "host process must have remote type");

  if (aProcess->IsDead() ||
      mHosts.WithEntryHandle(aProcess->GetRemoteType(), [&](auto&& entry) {
        if (entry) {
          MOZ_DIAGNOSTIC_ASSERT(
              entry.Data() == aProcess,
              "There's already another host process for this remote type");
          return false;
        }

        // This process wasn't already marked as our host, so insert it, and
        // begin subscribing, unless the process is still launching.
        entry.Insert(do_AddRef(aProcess));

        return true;
      })) {
    aProcess->AddBrowsingContextGroup(this);
  }
}

void BrowsingContextGroup::RemoveHostProcess(ContentParent* aProcess) {
  MOZ_DIAGNOSTIC_ASSERT(aProcess);
  MOZ_DIAGNOSTIC_ASSERT(aProcess->GetRemoteType() != PREALLOC_REMOTE_TYPE);
  auto entry = mHosts.Lookup(aProcess->GetRemoteType());
  if (entry && entry.Data() == aProcess) {
    entry.Remove();
  }
}

static void CollectContextInitializers(
    Span<RefPtr<BrowsingContext>> aContexts,
    nsTArray<SyncedContextInitializer>& aInits) {
  // The order that we record these initializers is important, as it will keep
  // the order that children are attached to their parent in the newly connected
  // content process consistent.
  for (auto& context : aContexts) {
    aInits.AppendElement(context->GetIPCInitializer());
    for (auto& window : context->GetWindowContexts()) {
      aInits.AppendElement(window->GetIPCInitializer());
      CollectContextInitializers(window->Children(), aInits);
    }
  }
}

void BrowsingContextGroup::Subscribe(ContentParent* aProcess) {
  MOZ_ASSERT(!mDestroyed);
  MOZ_DIAGNOSTIC_ASSERT(aProcess && !aProcess->IsLaunching());
  MOZ_DIAGNOSTIC_ASSERT(aProcess->GetRemoteType() != PREALLOC_REMOTE_TYPE);

  // Check if we're already subscribed to this process.
  if (!mSubscribers.EnsureInserted(aProcess)) {
    return;
  }

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  // If the process is already marked as dead, we won't be the host, but may
  // still need to subscribe to the process due to creating a popup while
  // shutting down.
  if (!aProcess->IsDead()) {
    auto hostEntry = mHosts.Lookup(aProcess->GetRemoteType());
    MOZ_DIAGNOSTIC_ASSERT(hostEntry && hostEntry.Data() == aProcess,
                          "Cannot subscribe a non-host process");
  }
#endif

  // FIXME: This won't send non-discarded children of discarded BCs, but those
  // BCs will be in the process of being destroyed anyway.
  // FIXME: Prevent that situation from occuring.
  nsTArray<SyncedContextInitializer> inits(mContexts.Count());
  CollectContextInitializers(mToplevels, inits);

  // Send all of our contexts to the target content process.
  Unused << aProcess->SendRegisterBrowsingContextGroup(Id(), inits);

  // If the focused or active BrowsingContexts belong in this group, tell the
  // newly subscribed process.
  if (nsFocusManager* fm = nsFocusManager::GetFocusManager()) {
    BrowsingContext* focused = fm->GetFocusedBrowsingContextInChrome();
    if (focused && focused->Group() != this) {
      focused = nullptr;
    }
    BrowsingContext* active = fm->GetActiveBrowsingContextInChrome();
    if (active && active->Group() != this) {
      active = nullptr;
    }

    if (focused || active) {
      Unused << aProcess->SendSetupFocusedAndActive(
          focused, fm->GetActionIdForFocusedBrowsingContextInChrome(), active,
          fm->GetActionIdForActiveBrowsingContextInChrome());
    }
  }
}

void BrowsingContextGroup::Unsubscribe(ContentParent* aProcess) {
  MOZ_DIAGNOSTIC_ASSERT(aProcess);
  MOZ_DIAGNOSTIC_ASSERT(aProcess->GetRemoteType() != PREALLOC_REMOTE_TYPE);
  mSubscribers.RemoveEntry(aProcess);
  aProcess->RemoveBrowsingContextGroup(this);

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  auto hostEntry = mHosts.Lookup(aProcess->GetRemoteType());
  MOZ_DIAGNOSTIC_ASSERT(!hostEntry || hostEntry.Data() != aProcess,
                        "Unsubscribing existing host entry");
#endif
}

ContentParent* BrowsingContextGroup::GetHostProcess(
    const nsACString& aRemoteType) {
  return mHosts.GetWeak(aRemoteType);
}

void BrowsingContextGroup::UpdateToplevelsSuspendedIfNeeded() {
  if (!StaticPrefs::dom_suspend_inactive_enabled()) {
    return;
  }

  mToplevelsSuspended = ShouldSuspendAllTopLevelContexts();
  for (const auto& context : mToplevels) {
    nsPIDOMWindowOuter* outer = context->GetDOMWindow();
    if (!outer) {
      continue;
    }
    nsCOMPtr<nsPIDOMWindowInner> inner = outer->GetCurrentInnerWindow();
    if (!inner) {
      continue;
    }
    if (mToplevelsSuspended && !inner->GetWasSuspendedByGroup()) {
      inner->Suspend();
      inner->SetWasSuspendedByGroup(true);
    } else if (!mToplevelsSuspended && inner->GetWasSuspendedByGroup()) {
      inner->Resume();
      inner->SetWasSuspendedByGroup(false);
    }
  }
}

bool BrowsingContextGroup::ShouldSuspendAllTopLevelContexts() const {
  for (const auto& context : mToplevels) {
    if (!context->InactiveForSuspend()) {
      return false;
    }
  }
  return true;
}

BrowsingContextGroup::~BrowsingContextGroup() { Destroy(); }

void BrowsingContextGroup::Destroy() {
#ifdef DEBUG
  if (mDestroyed) {
    MOZ_ASSERT(mHosts.Count() == 0);
    MOZ_ASSERT(mSubscribers.Count() == 0);
    MOZ_ASSERT_IF(sBrowsingContextGroups,
                  !sBrowsingContextGroups->Contains(Id()) ||
                      *sBrowsingContextGroups->Lookup(Id()) != this);
  }
  mDestroyed = true;
#endif

  // Make sure to call `RemoveBrowsingContextGroup` for every entry in both
  // `mHosts` and `mSubscribers`. This will visit most entries twice, but
  // `RemoveBrowsingContextGroup` is safe to call multiple times.
  for (auto& entry : mHosts) {
    entry.GetData()->RemoveBrowsingContextGroup(this);
  }
  for (auto& entry : mSubscribers) {
    entry.GetKey()->RemoveBrowsingContextGroup(this);
  }
  mHosts.Clear();
  mSubscribers.Clear();

  if (sBrowsingContextGroups) {
    sBrowsingContextGroups->Remove(Id());
  }
}

void BrowsingContextGroup::AddKeepAlive() {
  MOZ_ASSERT(!mDestroyed);
  mKeepAliveCount++;
}

void BrowsingContextGroup::RemoveKeepAlive() {
  MOZ_ASSERT(!mDestroyed);
  MOZ_DIAGNOSTIC_ASSERT(mKeepAliveCount > 0);
  mKeepAliveCount--;

  MaybeDestroy();
}

void BrowsingContextGroup::MaybeDestroy() {
  if (mContexts.IsEmpty() && mKeepAliveCount == 0 && this != sChromeGroup) {
    // There are no synced contexts still referencing this group. We can clear
    // all subscribers, and destroy ourselves.
    Destroy();

    // We may have been deleted here as the ContentChild/Parent may
    // have held the last references to `this`.
    // Do not access any members at this point.
  }
}

nsISupports* BrowsingContextGroup::GetParentObject() const {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

JSObject* BrowsingContextGroup::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return BrowsingContextGroup_Binding::Wrap(aCx, this, aGivenProto);
}

nsresult BrowsingContextGroup::QueuePostMessageEvent(
    already_AddRefed<nsIRunnable>&& aRunnable) {
  if (StaticPrefs::dom_separate_event_queue_for_post_message_enabled()) {
    if (!mPostMessageEventQueue) {
      nsCOMPtr<nsISerialEventTarget> target = GetMainThreadSerialEventTarget();
      mPostMessageEventQueue = ThrottledEventQueue::Create(
          target, "PostMessage Queue",
          nsIRunnablePriority::PRIORITY_DEFERRED_TIMERS);
      nsresult rv = mPostMessageEventQueue->SetIsPaused(false);
      MOZ_ALWAYS_SUCCEEDS(rv);
    }

    // Ensure the queue is enabled
    if (mPostMessageEventQueue->IsPaused()) {
      nsresult rv = mPostMessageEventQueue->SetIsPaused(false);
      MOZ_ALWAYS_SUCCEEDS(rv);
    }

    if (mPostMessageEventQueue) {
      mPostMessageEventQueue->Dispatch(std::move(aRunnable),
                                       NS_DISPATCH_NORMAL);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

void BrowsingContextGroup::FlushPostMessageEvents() {
  if (StaticPrefs::dom_separate_event_queue_for_post_message_enabled()) {
    if (mPostMessageEventQueue) {
      nsresult rv = mPostMessageEventQueue->SetIsPaused(true);
      MOZ_ALWAYS_SUCCEEDS(rv);
      nsCOMPtr<nsIRunnable> event;
      while ((event = mPostMessageEventQueue->GetEvent())) {
        NS_DispatchToMainThread(event.forget());
      }
    }
  }
}

bool BrowsingContextGroup::HasActiveBC() {
  for (auto& topLevelBC : Toplevels()) {
    if (topLevelBC->IsActive()) {
      return true;
    }
  }
  return false;
}

void BrowsingContextGroup::IncInputEventSuspensionLevel() {
  MOZ_ASSERT(StaticPrefs::dom_input_events_canSuspendInBCG_enabled());
  if (!mHasIncreasedInputTaskManagerSuspensionLevel && HasActiveBC()) {
    IncInputTaskManagerSuspensionLevel();
  }
  ++mInputEventSuspensionLevel;
}

void BrowsingContextGroup::DecInputEventSuspensionLevel() {
  MOZ_ASSERT(StaticPrefs::dom_input_events_canSuspendInBCG_enabled());
  --mInputEventSuspensionLevel;
  if (!mInputEventSuspensionLevel &&
      mHasIncreasedInputTaskManagerSuspensionLevel) {
    DecInputTaskManagerSuspensionLevel();
  }
}

void BrowsingContextGroup::DecInputTaskManagerSuspensionLevel() {
  MOZ_ASSERT(StaticPrefs::dom_input_events_canSuspendInBCG_enabled());
  MOZ_ASSERT(mHasIncreasedInputTaskManagerSuspensionLevel);

  InputTaskManager::Get()->DecSuspensionLevel();
  mHasIncreasedInputTaskManagerSuspensionLevel = false;
}

void BrowsingContextGroup::IncInputTaskManagerSuspensionLevel() {
  MOZ_ASSERT(StaticPrefs::dom_input_events_canSuspendInBCG_enabled());
  MOZ_ASSERT(!mHasIncreasedInputTaskManagerSuspensionLevel);
  MOZ_ASSERT(HasActiveBC());

  InputTaskManager::Get()->IncSuspensionLevel();
  mHasIncreasedInputTaskManagerSuspensionLevel = true;
}

void BrowsingContextGroup::UpdateInputTaskManagerIfNeeded(bool aIsActive) {
  MOZ_ASSERT(StaticPrefs::dom_input_events_canSuspendInBCG_enabled());
  if (!aIsActive) {
    if (mHasIncreasedInputTaskManagerSuspensionLevel) {
      MOZ_ASSERT(mInputEventSuspensionLevel > 0);
      if (!HasActiveBC()) {
        DecInputTaskManagerSuspensionLevel();
      }
    }
  } else {
    if (mInputEventSuspensionLevel &&
        !mHasIncreasedInputTaskManagerSuspensionLevel) {
      IncInputTaskManagerSuspensionLevel();
    }
  }
}

/* static */
BrowsingContextGroup* BrowsingContextGroup::GetChromeGroup() {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  if (!sChromeGroup && XRE_IsParentProcess()) {
    sChromeGroup = BrowsingContextGroup::Create();
    ClearOnShutdown(&sChromeGroup);
  }

  return sChromeGroup;
}

void BrowsingContextGroup::GetDocGroups(nsTArray<DocGroup*>& aDocGroups) {
  MOZ_ASSERT(NS_IsMainThread());
  for (auto iter = mDocGroups.ConstIter(); !iter.Done(); iter.Next()) {
    aDocGroups.AppendElement(iter.Data());
  }
}

already_AddRefed<DocGroup> BrowsingContextGroup::AddDocument(
    const nsACString& aKey, Document* aDocument) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<DocGroup>& docGroup = mDocGroups.LookupOrInsertWith(
      aKey, [&] { return DocGroup::Create(this, aKey); });

  docGroup->AddDocument(aDocument);
  return do_AddRef(docGroup);
}

void BrowsingContextGroup::RemoveDocument(const nsACString& aKey,
                                          Document* aDocument) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<DocGroup> docGroup = aDocument->GetDocGroup();
  // Removing the last document in DocGroup might decrement the
  // DocGroup BrowsingContextGroup's refcount to 0.
  RefPtr<BrowsingContextGroup> kungFuDeathGrip(this);
  docGroup->RemoveDocument(aDocument);

  if (docGroup->IsEmpty()) {
    mDocGroups.Remove(aKey);
  }
}

already_AddRefed<BrowsingContextGroup> BrowsingContextGroup::Select(
    WindowContext* aParent, BrowsingContext* aOpener) {
  if (aParent) {
    return do_AddRef(aParent->Group());
  }
  if (aOpener) {
    return do_AddRef(aOpener->Group());
  }
  return Create();
}

void BrowsingContextGroup::GetAllGroups(
    nsTArray<RefPtr<BrowsingContextGroup>>& aGroups) {
  aGroups.Clear();
  if (!sBrowsingContextGroups) {
    return;
  }

  aGroups.SetCapacity(sBrowsingContextGroups->Count());
  for (auto& group : *sBrowsingContextGroups) {
    aGroups.AppendElement(group.GetData());
  }
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(BrowsingContextGroup, mContexts,
                                      mToplevels, mHosts, mSubscribers,
                                      mTimerEventQueue, mWorkerEventQueue)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(BrowsingContextGroup, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(BrowsingContextGroup, Release)

}  // namespace dom
}  // namespace mozilla
