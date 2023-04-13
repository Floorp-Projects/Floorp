/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PreallocatedProcessManager.h"

#include "mozilla/AppShutdown.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsIPropertyBag2.h"
#include "ProcessPriorityManager.h"
#include "nsServiceManagerUtils.h"
#include "nsIXULRuntime.h"
#include "nsTArray.h"
#include "prsystem.h"

using namespace mozilla::hal;
using namespace mozilla::dom;

namespace mozilla {
/**
 * This singleton class implements the static methods on
 * PreallocatedProcessManager.
 */
class PreallocatedProcessManagerImpl final : public nsIObserver {
  friend class PreallocatedProcessManager;

 public:
  static PreallocatedProcessManagerImpl* Singleton();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // See comments on PreallocatedProcessManager for these methods.
  void AddBlocker(ContentParent* aParent);
  void RemoveBlocker(ContentParent* aParent);
  already_AddRefed<ContentParent> Take(const nsACString& aRemoteType);
  void Erase(ContentParent* aParent);

 private:
  static const char* const kObserverTopics[];

  static StaticRefPtr<PreallocatedProcessManagerImpl> sSingleton;

  PreallocatedProcessManagerImpl();
  ~PreallocatedProcessManagerImpl();
  PreallocatedProcessManagerImpl(const PreallocatedProcessManagerImpl&) =
      delete;

  const PreallocatedProcessManagerImpl& operator=(
      const PreallocatedProcessManagerImpl&) = delete;

  void Init();

  bool CanAllocate();
  void AllocateAfterDelay(bool aStartup = false);
  void AllocateOnIdle();
  void AllocateNow();

  void RereadPrefs();
  void Enable(uint32_t aProcesses);
  void Disable();
  void CloseProcesses();

  bool IsEmpty() const { return mPreallocatedProcesses.IsEmpty(); }
  static bool IsShutdown() {
    return AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed);
  }
  bool IsEnabled() { return mEnabled && !IsShutdown(); }

  bool mEnabled;
  uint32_t mNumberPreallocs;
  AutoTArray<RefPtr<ContentParent>, 3> mPreallocatedProcesses;
  // Even if we have multiple PreallocatedProcessManagerImpls, we'll have
  // one blocker counter
  static uint32_t sNumBlockers;
  TimeStamp mBlockingStartTime;
};

/* static */
uint32_t PreallocatedProcessManagerImpl::sNumBlockers = 0;

const char* const PreallocatedProcessManagerImpl::kObserverTopics[] = {
    "memory-pressure",
    "profile-change-teardown",
    NS_XPCOM_SHUTDOWN_OBSERVER_ID,
};

/* static */
StaticRefPtr<PreallocatedProcessManagerImpl>
    PreallocatedProcessManagerImpl::sSingleton;

/* static */
PreallocatedProcessManagerImpl* PreallocatedProcessManagerImpl::Singleton() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!sSingleton) {
    sSingleton = new PreallocatedProcessManagerImpl;
    sSingleton->Init();
    ClearOnShutdown(&sSingleton);
  }
  return sSingleton;
  //  PreallocatedProcessManagers live until shutdown
}

NS_IMPL_ISUPPORTS(PreallocatedProcessManagerImpl, nsIObserver)

PreallocatedProcessManagerImpl::PreallocatedProcessManagerImpl()
    : mEnabled(false), mNumberPreallocs(1) {}

PreallocatedProcessManagerImpl::~PreallocatedProcessManagerImpl() {
  // Note: mPreallocatedProcesses may not be null, but all processes should
  // be dead (IsDead==true).  We block Erase() when our observer sees
  // shutdown starting.
}

void PreallocatedProcessManagerImpl::Init() {
  Preferences::AddStrongObserver(this, "dom.ipc.processPrelaunch.enabled");
  // We have to respect processCount at all time. This is especially important
  // for testing.
  Preferences::AddStrongObserver(this, "dom.ipc.processCount");
  // A StaticPref, but we need to adjust the number of preallocated processes
  // if the value goes up or down, so we need to run code on change.
  Preferences::AddStrongObserver(this,
                                 "dom.ipc.processPrelaunch.fission.number");

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  MOZ_ASSERT(os);
  for (auto topic : kObserverTopics) {
    os->AddObserver(this, topic, /* ownsWeak */ false);
  }
  RereadPrefs();
}

NS_IMETHODIMP
PreallocatedProcessManagerImpl::Observe(nsISupports* aSubject,
                                        const char* aTopic,
                                        const char16_t* aData) {
  if (!strcmp("nsPref:changed", aTopic)) {
    // The only other observer we registered was for our prefs.
    RereadPrefs();
  } else if (!strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, aTopic) ||
             !strcmp("profile-change-teardown", aTopic)) {
    Preferences::RemoveObserver(this, "dom.ipc.processPrelaunch.enabled");
    Preferences::RemoveObserver(this, "dom.ipc.processCount");
    Preferences::RemoveObserver(this,
                                "dom.ipc.processPrelaunch.fission.number");

    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    MOZ_ASSERT(os);
    for (auto topic : kObserverTopics) {
      os->RemoveObserver(this, topic);
    }
  } else if (!strcmp("memory-pressure", aTopic)) {
    CloseProcesses();
  } else {
    MOZ_ASSERT_UNREACHABLE("Unknown topic");
  }

  return NS_OK;
}

void PreallocatedProcessManagerImpl::RereadPrefs() {
  if (mozilla::BrowserTabsRemoteAutostart() &&
      Preferences::GetBool("dom.ipc.processPrelaunch.enabled")) {
    int32_t number = 1;
    if (mozilla::FissionAutostart()) {
      number = StaticPrefs::dom_ipc_processPrelaunch_fission_number();
      // limit preallocated processes on low-mem machines
      PRUint64 bytes = PR_GetPhysicalMemorySize();
      if (bytes > 0 &&
          bytes <=
              StaticPrefs::dom_ipc_processPrelaunch_lowmem_mb() * 1024 * 1024) {
        number = 1;
      }
    }
    if (number >= 0) {
      Enable(number);
      // We have one prealloc queue for all types except File now
      if (static_cast<uint64_t>(number) < mPreallocatedProcesses.Length()) {
        CloseProcesses();
      }
    }
  } else {
    Disable();
  }
}

already_AddRefed<ContentParent> PreallocatedProcessManagerImpl::Take(
    const nsACString& aRemoteType) {
  if (!IsEnabled()) {
    return nullptr;
  }
  RefPtr<ContentParent> process;
  if (!IsEmpty()) {
    process = mPreallocatedProcesses.ElementAt(0);
    mPreallocatedProcesses.RemoveElementAt(0);

    // Don't set the priority to FOREGROUND here, since it may not have
    // finished starting

    // We took a preallocated process. Let's try to start up a new one
    // soon.
    ContentParent* last = mPreallocatedProcesses.SafeLastElement(nullptr);
    // There could be a launching process that isn't the last, but that's
    // ok (and unlikely)
    if (!last || !last->IsLaunching()) {
      AllocateAfterDelay();
    }
    MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
            ("Use prealloc process %p%s, %lu available", process.get(),
             process->IsLaunching() ? " (still launching)" : "",
             (unsigned long)mPreallocatedProcesses.Length()));
  }
  if (process && !process->IsLaunching()) {
    ProcessPriorityManager::SetProcessPriority(process,
                                               PROCESS_PRIORITY_FOREGROUND);
  }  // else this will get set by the caller when they call InitInternal()

  return process.forget();
}

void PreallocatedProcessManagerImpl::Erase(ContentParent* aParent) {
  (void)mPreallocatedProcesses.RemoveElement(aParent);
}

void PreallocatedProcessManagerImpl::Enable(uint32_t aProcesses) {
  mNumberPreallocs = aProcesses;
  MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
          ("Enabling preallocation: %u", aProcesses));
  if (mEnabled || IsShutdown()) {
    return;
  }

  mEnabled = true;
  AllocateAfterDelay(/* aStartup */ true);
}

void PreallocatedProcessManagerImpl::AddBlocker(ContentParent* aParent) {
  if (sNumBlockers == 0) {
    mBlockingStartTime = TimeStamp::Now();
  }
  sNumBlockers++;
}

void PreallocatedProcessManagerImpl::RemoveBlocker(ContentParent* aParent) {
  // This used to assert that the blocker existed, but preallocated
  // processes aren't blockers anymore because it's not useful and
  // interferes with async launch, and it's simpler if content
  // processes don't need to remember whether they were preallocated.

  MOZ_DIAGNOSTIC_ASSERT(sNumBlockers > 0);
  sNumBlockers--;
  if (sNumBlockers == 0) {
    MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
            ("Blocked preallocation for %fms",
             (TimeStamp::Now() - mBlockingStartTime).ToMilliseconds()));
    PROFILER_MARKER_TEXT("Process", DOM,
                         MarkerTiming::IntervalUntilNowFrom(mBlockingStartTime),
                         "Blocked preallocation");
    if (IsEmpty()) {
      AllocateAfterDelay();
    }
  }
}

bool PreallocatedProcessManagerImpl::CanAllocate() {
  return IsEnabled() && sNumBlockers == 0 &&
         mPreallocatedProcesses.Length() < mNumberPreallocs && !IsShutdown() &&
         (FissionAutostart() ||
          !ContentParent::IsMaxProcessCountReached(DEFAULT_REMOTE_TYPE));
}

void PreallocatedProcessManagerImpl::AllocateAfterDelay(bool aStartup) {
  if (!IsEnabled()) {
    return;
  }
  long delay = aStartup ? StaticPrefs::dom_ipc_processPrelaunch_startupDelayMs()
                        : StaticPrefs::dom_ipc_processPrelaunch_delayMs();
  MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
          ("Starting delayed process start, delay=%ld", delay));
  NS_DelayedDispatchToCurrentThread(
      NewRunnableMethod("PreallocatedProcessManagerImpl::AllocateOnIdle", this,
                        &PreallocatedProcessManagerImpl::AllocateOnIdle),
      delay);
}

void PreallocatedProcessManagerImpl::AllocateOnIdle() {
  if (!IsEnabled()) {
    return;
  }

  MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
          ("Starting process allocate on idle"));
  NS_DispatchToCurrentThreadQueue(
      NewRunnableMethod("PreallocatedProcessManagerImpl::AllocateNow", this,
                        &PreallocatedProcessManagerImpl::AllocateNow),
      EventQueuePriority::Idle);
}

void PreallocatedProcessManagerImpl::AllocateNow() {
  MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
          ("Trying to start process now"));
  if (!CanAllocate()) {
    if (IsEnabled() && IsEmpty() && sNumBlockers > 0) {
      // If it's too early to allocate a process let's retry later.
      AllocateAfterDelay();
    }
    return;
  }

  RefPtr<ContentParent> process = ContentParent::MakePreallocProcess();
  mPreallocatedProcesses.AppendElement(process);
  MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
          ("Preallocated = %lu of %d processes",
           (unsigned long)mPreallocatedProcesses.Length(), mNumberPreallocs));

  RefPtr<PreallocatedProcessManagerImpl> self(this);
  process->LaunchSubprocessAsync(PROCESS_PRIORITY_PREALLOC)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self, this, process](const RefPtr<ContentParent>&) {
            if (process->IsDead()) {
              Erase(process);
              // Process died in startup (before we could add it).  If it
              // dies after this, MarkAsDead() will Erase() this entry.
              // Shouldn't be in the sBrowserContentParents, so we don't need
              // RemoveFromList().  We won't try to kick off a new
              // preallocation here, to avoid possible looping if something is
              // causing them to consistently fail; if everything is ok on the
              // next allocation request we'll kick off creation.
            } else {
              // Continue prestarting processes if needed
              if (CanAllocate()) {
                if (mPreallocatedProcesses.Length() < mNumberPreallocs) {
                  AllocateOnIdle();
                }
              } else if (!IsEnabled()) {
                // if this has a remote type set, it's been allocated for use
                // already
                if (process->mRemoteType == PREALLOC_REMOTE_TYPE) {
                  // This will Erase() it
                  process->ShutDownProcess(
                      ContentParent::SEND_SHUTDOWN_MESSAGE);
                }
              }
            }
          },
          [self, this, process]() { Erase(process); });
}

void PreallocatedProcessManagerImpl::Disable() {
  if (!mEnabled) {
    return;
  }

  mEnabled = false;
  CloseProcesses();
}

void PreallocatedProcessManagerImpl::CloseProcesses() {
  while (!IsEmpty()) {
    RefPtr<ContentParent> process(mPreallocatedProcesses.ElementAt(0));
    mPreallocatedProcesses.RemoveElementAt(0);
    process->ShutDownProcess(ContentParent::SEND_SHUTDOWN_MESSAGE);
    // drop ref and let it free
  }

  // Make sure to also clear out the recycled E10S process cache, as it's also
  // controlled by the same preference, and can be cleaned up due to memory
  // pressure.
  if (RefPtr<ContentParent> recycled =
          ContentParent::sRecycledE10SProcess.forget()) {
    recycled->MaybeBeginShutDown();
  }
}

inline PreallocatedProcessManagerImpl*
PreallocatedProcessManager::GetPPMImpl() {
  if (PreallocatedProcessManagerImpl::IsShutdown()) {
    return nullptr;
  }
  return PreallocatedProcessManagerImpl::Singleton();
}

/* static */
bool PreallocatedProcessManager::Enabled() {
  if (auto impl = GetPPMImpl()) {
    return impl->IsEnabled();
  }
  return false;
}

/* static */
void PreallocatedProcessManager::AddBlocker(const nsACString& aRemoteType,
                                            ContentParent* aParent) {
  MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
          ("AddBlocker: %s %p (sNumBlockers=%d)",
           PromiseFlatCString(aRemoteType).get(), aParent,
           PreallocatedProcessManagerImpl::sNumBlockers));
  if (auto impl = GetPPMImpl()) {
    impl->AddBlocker(aParent);
  }
}

/* static */
void PreallocatedProcessManager::RemoveBlocker(const nsACString& aRemoteType,
                                               ContentParent* aParent) {
  MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
          ("RemoveBlocker: %s %p (sNumBlockers=%d)",
           PromiseFlatCString(aRemoteType).get(), aParent,
           PreallocatedProcessManagerImpl::sNumBlockers));
  if (auto impl = GetPPMImpl()) {
    impl->RemoveBlocker(aParent);
  }
}

/* static */
already_AddRefed<ContentParent> PreallocatedProcessManager::Take(
    const nsACString& aRemoteType) {
  if (auto impl = GetPPMImpl()) {
    return impl->Take(aRemoteType);
  }
  return nullptr;
}

/* static */
void PreallocatedProcessManager::Erase(ContentParent* aParent) {
  if (auto impl = GetPPMImpl()) {
    impl->Erase(aParent);
  }
}

}  // namespace mozilla
