/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowsingContextGroup.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/InputTaskManager.h"
#include "mozilla/Preferences.h"
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

// Maximum number of successive dialogs before we prompt users to disable
// dialogs for this window.
#define MAX_SUCCESSIVE_DIALOG_COUNT 5

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

already_AddRefed<BrowsingContextGroup> BrowsingContextGroup::GetExisting(
    uint64_t aId) {
  if (sBrowsingContextGroups) {
    return do_AddRef(sBrowsingContextGroups->Get(aId));
  }
  return nullptr;
}

// Only use 53 bits for the BrowsingContextGroup ID.
static constexpr uint64_t kBrowsingContextGroupIdTotalBits = 53;
static constexpr uint64_t kBrowsingContextGroupIdProcessBits = 22;
static constexpr uint64_t kBrowsingContextGroupIdFlagBits = 1;
static constexpr uint64_t kBrowsingContextGroupIdBits =
    kBrowsingContextGroupIdTotalBits - kBrowsingContextGroupIdProcessBits -
    kBrowsingContextGroupIdFlagBits;

// IDs for the relevant flags
static constexpr uint64_t kPotentiallyCrossOriginIsolatedFlag = 0x1;

// The next ID value which will be used.
static uint64_t sNextBrowsingContextGroupId = 1;

// Generate the next ID with the given flags.
static uint64_t GenerateBrowsingContextGroupId(uint64_t aFlags) {
  MOZ_RELEASE_ASSERT(aFlags < (uint64_t(1) << kBrowsingContextGroupIdFlagBits));
  uint64_t childId = XRE_IsContentProcess()
                         ? ContentChild::GetSingleton()->GetID()
                         : uint64_t(0);
  MOZ_RELEASE_ASSERT(childId <
                     (uint64_t(1) << kBrowsingContextGroupIdProcessBits));
  uint64_t id = sNextBrowsingContextGroupId++;
  MOZ_RELEASE_ASSERT(id < (uint64_t(1) << kBrowsingContextGroupIdBits));

  return (childId << (kBrowsingContextGroupIdBits +
                      kBrowsingContextGroupIdFlagBits)) |
         (id << kBrowsingContextGroupIdFlagBits) | aFlags;
}

// Extract flags from the given ID.
static uint64_t GetBrowsingContextGroupIdFlags(uint64_t aId) {
  return aId & ((uint64_t(1) << kBrowsingContextGroupIdFlagBits) - 1);
}

uint64_t BrowsingContextGroup::CreateId(bool aPotentiallyCrossOriginIsolated) {
  // We encode the potentially cross-origin isolated bit within the ID so that
  // the information can be recovered whenever the group needs to be re-created
  // due to e.g. being garbage-collected.
  //
  // In the future if we end up needing more complex information stored within
  // the ID, we can consider converting it to a more complex type, like a
  // string.
  uint64_t flags =
      aPotentiallyCrossOriginIsolated ? kPotentiallyCrossOriginIsolatedFlag : 0;
  uint64_t id = GenerateBrowsingContextGroupId(flags);
  MOZ_ASSERT(GetBrowsingContextGroupIdFlags(id) == flags);
  return id;
}

already_AddRefed<BrowsingContextGroup> BrowsingContextGroup::Create(
    bool aPotentiallyCrossOriginIsolated) {
  return GetOrCreate(CreateId(aPotentiallyCrossOriginIsolated));
}

BrowsingContextGroup::BrowsingContextGroup(uint64_t aId) : mId(aId) {
  mTimerEventQueue = ThrottledEventQueue::Create(
      GetMainThreadSerialEventTarget(), "BrowsingContextGroup timer queue");

  mWorkerEventQueue = ThrottledEventQueue::Create(
      GetMainThreadSerialEventTarget(), "BrowsingContextGroup worker queue");
}

void BrowsingContextGroup::Register(nsISupports* aContext) {
  MOZ_DIAGNOSTIC_ASSERT(!mDestroyed);
  MOZ_DIAGNOSTIC_ASSERT(aContext);
  mContexts.Insert(aContext);
}

void BrowsingContextGroup::Unregister(nsISupports* aContext) {
  MOZ_DIAGNOSTIC_ASSERT(!mDestroyed);
  MOZ_DIAGNOSTIC_ASSERT(aContext);
  mContexts.Remove(aContext);

  MaybeDestroy();
}

void BrowsingContextGroup::EnsureHostProcess(ContentParent* aProcess) {
  MOZ_DIAGNOSTIC_ASSERT(!mDestroyed);
  MOZ_DIAGNOSTIC_ASSERT(this != sChromeGroup,
                        "cannot have content host for chrome group");
  MOZ_DIAGNOSTIC_ASSERT(aProcess->GetRemoteType() != PREALLOC_REMOTE_TYPE,
                        "cannot use preallocated process as host");
  MOZ_DIAGNOSTIC_ASSERT(!aProcess->GetRemoteType().IsEmpty(),
                        "host process must have remote type");

  // XXX: The diagnostic crashes in bug 1816025 seemed to come through caller
  // ContentParent::GetNewOrUsedLaunchingBrowserProcess where we already
  // did AssertAlive, so IsDead should be irrelevant here. Still it reads
  // wrong that we ever might do AddBrowsingContextGroup if aProcess->IsDead().
  if (aProcess->IsDead() ||
      mHosts.WithEntryHandle(aProcess->GetRemoteType(), [&](auto&& entry) {
        if (entry) {
          // We know from bug 1816025 that this happens quite often and we have
          // bug 1815480 on file that should harden the entire flow. But in the
          // meantime we can just live with NOT replacing the found host
          // process with a new one here if it is still alive.
          MOZ_ASSERT(
              entry.Data() == aProcess,
              "There's already another host process for this remote type");
          if (!entry.Data()->IsShuttingDown()) {
            return false;
          }
        }

        // This process wasn't already marked as our host, so insert it (or
        // update if the old process is shutting down), and begin subscribing,
        // unless the process is still launching.
        entry.InsertOrUpdate(do_AddRef(aProcess));

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
    for (const auto& window : context->GetWindowContexts()) {
      aInits.AppendElement(window->GetIPCInitializer());
      CollectContextInitializers(window->Children(), aInits);
    }
  }
}

void BrowsingContextGroup::Subscribe(ContentParent* aProcess) {
  MOZ_DIAGNOSTIC_ASSERT(!mDestroyed);
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
  mSubscribers.Remove(aProcess);
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
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  if (mDestroyed) {
    MOZ_DIAGNOSTIC_ASSERT(mHosts.Count() == 0);
    MOZ_DIAGNOSTIC_ASSERT(mSubscribers.Count() == 0);
    MOZ_DIAGNOSTIC_ASSERT_IF(sBrowsingContextGroups,
                             !sBrowsingContextGroups->Contains(Id()) ||
                                 *sBrowsingContextGroups->Lookup(Id()) != this);
  }
  mDestroyed = true;
#endif

  // Make sure to call `RemoveBrowsingContextGroup` for every entry in both
  // `mHosts` and `mSubscribers`. This will visit most entries twice, but
  // `RemoveBrowsingContextGroup` is safe to call multiple times.
  for (const auto& entry : mHosts.Values()) {
    entry->RemoveBrowsingContextGroup(this);
  }
  for (const auto& key : mSubscribers) {
    key->RemoveBrowsingContextGroup(this);
  }
  mHosts.Clear();
  mSubscribers.Clear();

  if (sBrowsingContextGroups) {
    sBrowsingContextGroups->Remove(Id());
  }
}

void BrowsingContextGroup::AddKeepAlive() {
  MOZ_DIAGNOSTIC_ASSERT(!mDestroyed);
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  mKeepAliveCount++;
}

void BrowsingContextGroup::RemoveKeepAlive() {
  MOZ_DIAGNOSTIC_ASSERT(!mDestroyed);
  MOZ_DIAGNOSTIC_ASSERT(mKeepAliveCount > 0);
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  mKeepAliveCount--;

  MaybeDestroy();
}

auto BrowsingContextGroup::MakeKeepAlivePtr() -> KeepAlivePtr {
  AddKeepAlive();
  return KeepAlivePtr{do_AddRef(this).take()};
}

void BrowsingContextGroup::MaybeDestroy() {
  // Once there are no synced contexts referencing a `BrowsingContextGroup`, we
  // can clear subscribers and destroy this group. We only do this in the parent
  // process, as it will orchestrate destruction of BCGs in content processes.
  if (XRE_IsParentProcess() && mContexts.IsEmpty() && mKeepAliveCount == 0 &&
      this != sChromeGroup) {
    Destroy();

    // We may have been deleted here, as `Destroy()` will clear references. Do
    // not access any members at this point.
  }
}

void BrowsingContextGroup::ChildDestroy() {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsContentProcess());
  MOZ_DIAGNOSTIC_ASSERT(!mDestroyed);
  MOZ_DIAGNOSTIC_ASSERT(mContexts.IsEmpty());
  Destroy();
}

nsISupports* BrowsingContextGroup::GetParentObject() const {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

JSObject* BrowsingContextGroup::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return BrowsingContextGroup_Binding::Wrap(aCx, this, aGivenProto);
}

nsresult BrowsingContextGroup::QueuePostMessageEvent(nsIRunnable* aRunnable) {
  MOZ_ASSERT(StaticPrefs::dom_separate_event_queue_for_post_message_enabled());

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

  return mPostMessageEventQueue->Dispatch(aRunnable, NS_DISPATCH_NORMAL);
}

void BrowsingContextGroup::FlushPostMessageEvents() {
  if (!mPostMessageEventQueue) {
    return;
  }
  nsresult rv = mPostMessageEventQueue->SetIsPaused(true);
  MOZ_ALWAYS_SUCCEEDS(rv);
  nsCOMPtr<nsIRunnable> event;
  while ((event = mPostMessageEventQueue->GetEvent())) {
    NS_DispatchToMainThread(event.forget());
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
  AppendToArray(aDocGroups, mDocGroups.Values());
}

already_AddRefed<DocGroup> BrowsingContextGroup::AddDocument(
    const nsACString& aKey, Document* aDocument) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<DocGroup>& docGroup = mDocGroups.LookupOrInsertWith(
      aKey, [&] { return DocGroup::Create(this, aKey); });

  docGroup->AddDocument(aDocument);
  return do_AddRef(docGroup);
}

void BrowsingContextGroup::RemoveDocument(Document* aDocument,
                                          DocGroup* aDocGroup) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<DocGroup> docGroup = aDocGroup;
  // Removing the last document in DocGroup might decrement the
  // DocGroup BrowsingContextGroup's refcount to 0.
  RefPtr<BrowsingContextGroup> kungFuDeathGrip(this);
  docGroup->RemoveDocument(aDocument);

  if (docGroup->IsEmpty()) {
    mDocGroups.Remove(docGroup->GetKey());
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

  aGroups = ToArray(sBrowsingContextGroups->Values());
}

// For tests only.
void BrowsingContextGroup::ResetDialogAbuseState() {
  mDialogAbuseCount = 0;
  // Reset the timer.
  mLastDialogQuitTime =
      TimeStamp::Now() -
      TimeDuration::FromSeconds(DEFAULT_SUCCESSIVE_DIALOG_TIME_LIMIT);
}

bool BrowsingContextGroup::DialogsAreBeingAbused() {
  if (mLastDialogQuitTime.IsNull() || nsContentUtils::IsCallerChrome()) {
    return false;
  }

  TimeDuration dialogInterval(TimeStamp::Now() - mLastDialogQuitTime);
  if (dialogInterval.ToSeconds() <
      Preferences::GetInt("dom.successive_dialog_time_limit",
                          DEFAULT_SUCCESSIVE_DIALOG_TIME_LIMIT)) {
    mDialogAbuseCount++;

    return PopupBlocker::GetPopupControlState() > PopupBlocker::openAllowed ||
           mDialogAbuseCount > MAX_SUCCESSIVE_DIALOG_COUNT;
  }

  // Reset the abuse counter
  mDialogAbuseCount = 0;

  return false;
}

bool BrowsingContextGroup::IsPotentiallyCrossOriginIsolated() {
  return GetBrowsingContextGroupIdFlags(mId) &
         kPotentiallyCrossOriginIsolatedFlag;
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(BrowsingContextGroup, mContexts,
                                      mToplevels, mHosts, mSubscribers,
                                      mTimerEventQueue, mWorkerEventQueue,
                                      mDocGroups)

}  // namespace dom
}  // namespace mozilla
