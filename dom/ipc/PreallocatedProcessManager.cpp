/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PreallocatedProcessManager.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsIPropertyBag2.h"
#include "ProcessPriorityManager.h"
#include "nsServiceManagerUtils.h"
#include "nsIXULRuntime.h"
#include <deque>

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
  bool Provide(ContentParent* aParent);
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
  void AllocateAfterDelay();
  void AllocateOnIdle();
  void AllocateNow();

  void RereadPrefs();
  void Enable(uint32_t aProcesses);
  void Disable();
  void CloseProcesses();

  bool IsEmpty() const {
    return mPreallocatedProcesses.empty() && !mLaunchInProgress;
  }

  bool mEnabled;
  static bool sShutdown;
  bool mLaunchInProgress;
  uint32_t mNumberPreallocs;
  std::deque<RefPtr<ContentParent>> mPreallocatedProcesses;
  RefPtr<ContentParent> mPreallocatedE10SProcess;  // There can be only one
  // Even if we have multiple PreallocatedProcessManagerImpls, we'll have
  // one blocker counter
  static uint32_t sNumBlockers;
  TimeStamp mBlockingStartTime;
};

/* static */
uint32_t PreallocatedProcessManagerImpl::sNumBlockers = 0;
bool PreallocatedProcessManagerImpl::sShutdown = false;

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
    : mEnabled(false), mLaunchInProgress(false), mNumberPreallocs(1) {}

PreallocatedProcessManagerImpl::~PreallocatedProcessManagerImpl() {
  // This shouldn't happen, because the promise callbacks should
  // hold strong references, but let't make absolutely sure:
  MOZ_RELEASE_ASSERT(!mLaunchInProgress);
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
    // Let's prevent any new preallocated processes from starting. ContentParent
    // will handle the shutdown of the existing process and the
    // mPreallocatedProcesses reference will be cleared by the ClearOnShutdown
    // of the manager singleton.
    sShutdown = true;
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
    }
    if (number >= 0) {
      Enable(number);
      // We have one prealloc queue for all types except File now
      if (static_cast<uint64_t>(number) < mPreallocatedProcesses.size()) {
        CloseProcesses();
      }
    }
  } else {
    Disable();
  }
}

already_AddRefed<ContentParent> PreallocatedProcessManagerImpl::Take(
    const nsACString& aRemoteType) {
  if (!mEnabled || sShutdown) {
    return nullptr;
  }
  RefPtr<ContentParent> process;
  if (aRemoteType == DEFAULT_REMOTE_TYPE) {
    // we can recycle processes via Provide() for e10s only
    process = mPreallocatedE10SProcess.forget();
    if (process) {
      MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
              ("Reuse web process %p", process.get()));
    }
  }
  if (!process && !mPreallocatedProcesses.empty()) {
    process = mPreallocatedProcesses.front().forget();
    mPreallocatedProcesses.pop_front();  // holds a nullptr
    // We took a preallocated process. Let's try to start up a new one
    // soon.
    AllocateOnIdle();
    MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
            ("Use prealloc process %p", process.get()));
  }
  if (process) {
    ProcessPriorityManager::SetProcessPriority(process,
                                               PROCESS_PRIORITY_FOREGROUND);
  }
  return process.forget();
}

bool PreallocatedProcessManagerImpl::Provide(ContentParent* aParent) {
  MOZ_DIAGNOSTIC_ASSERT(aParent->GetRemoteType() == DEFAULT_REMOTE_TYPE);

  // This will take the already-running process even if there's a
  // launch in progress; if that process hasn't been taken by the
  // time the launch completes, the new process will be shut down.
  if (mEnabled && !sShutdown && !mPreallocatedE10SProcess) {
    MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
            ("Store for reuse web process %p", aParent));
    ProcessPriorityManager::SetProcessPriority(aParent,
                                               PROCESS_PRIORITY_BACKGROUND);
    mPreallocatedE10SProcess = aParent;
    return true;
  }

  // We might get a call from both NotifyTabDestroying and NotifyTabDestroyed
  // with the same ContentParent. Returning true here for both calls is
  // important to avoid the cached process to be destroyed.
  return aParent == mPreallocatedE10SProcess;
}

void PreallocatedProcessManagerImpl::Erase(ContentParent* aParent) {
  // Ensure this ContentParent isn't cached
  for (auto it = mPreallocatedProcesses.begin();
       it != mPreallocatedProcesses.end(); it++) {
    if (*it == aParent) {
      mPreallocatedProcesses.erase(it);
      break;
    }
  }
  if (mPreallocatedE10SProcess == aParent) {
    mPreallocatedE10SProcess = nullptr;
  }
}

void PreallocatedProcessManagerImpl::Enable(uint32_t aProcesses) {
  mNumberPreallocs = aProcesses;
  if (mEnabled) {
    return;
  }

  mEnabled = true;
  AllocateAfterDelay();
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
  // (And preallocated processes can't AddBlocker when taken, because
  // it's possible for a short-lived process to be recycled through
  // Provide() and Take() before reaching RecvFirstIdle.)

  MOZ_DIAGNOSTIC_ASSERT(sNumBlockers > 0);
  sNumBlockers--;
  if (sNumBlockers == 0) {
    MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
            ("Blocked preallocation for %fms",
             (TimeStamp::Now() - mBlockingStartTime).ToMilliseconds()));
    PROFILER_ADD_TEXT_MARKER("Process", "Blocked preallocation"_ns,
                             JS::ProfilingCategoryPair::DOM, mBlockingStartTime,
                             TimeStamp::Now());
    if (IsEmpty()) {
      AllocateAfterDelay();
    }
  }
}

bool PreallocatedProcessManagerImpl::CanAllocate() {
  return mEnabled && sNumBlockers == 0 &&
         mPreallocatedProcesses.size() < mNumberPreallocs && !sShutdown &&
         (FissionAutostart() ||
          !ContentParent::IsMaxProcessCountReached(DEFAULT_REMOTE_TYPE));
}

void PreallocatedProcessManagerImpl::AllocateAfterDelay() {
  if (!mEnabled) {
    return;
  }

  NS_DelayedDispatchToCurrentThread(
      NewRunnableMethod("PreallocatedProcessManagerImpl::AllocateOnIdle", this,
                        &PreallocatedProcessManagerImpl::AllocateOnIdle),
      StaticPrefs::dom_ipc_processPrelaunch_delayMs());
}

void PreallocatedProcessManagerImpl::AllocateOnIdle() {
  if (!mEnabled) {
    return;
  }

  NS_DispatchToCurrentThreadQueue(
      NewRunnableMethod("PreallocatedProcessManagerImpl::AllocateNow", this,
                        &PreallocatedProcessManagerImpl::AllocateNow),
      EventQueuePriority::Idle);
}

void PreallocatedProcessManagerImpl::AllocateNow() {
  if (!CanAllocate()) {
    if (mEnabled && !sShutdown && IsEmpty() && sNumBlockers > 0) {
      // If it's too early to allocate a process let's retry later.
      AllocateAfterDelay();
    }
    return;
  }

  RefPtr<PreallocatedProcessManagerImpl> self(this);
  mLaunchInProgress = true;

  ContentParent::PreallocateProcess()->Then(
      GetCurrentSerialEventTarget(), __func__,

      [self, this](const RefPtr<ContentParent>& process) {
        mLaunchInProgress = false;
        if (process->IsDead()) {
          // Process died in startup (before we could add it).  If it
          // dies after this, MarkAsDead() will Erase() this entry.
          // Shouldn't be in the sBrowserContentParents, so we don't need
          // RemoveFromList().  We won't try to kick off a new
          // preallocation here, to avoid possible looping if something is
          // causing them to consistently fail; if everything is ok on the
          // next allocation request we'll kick off creation.
        } else {
          if (CanAllocate()) {
            // slight perf reason for push_back - while the cpu cache
            // probably has stack/etc associated with the most recent
            // process created, we don't know that it has finished startup.
            // If we added it to the queue on completion of startup, we
            // could push_front it, but that would require a bunch more
            // logic.
            mPreallocatedProcesses.push_back(process);
            MOZ_LOG(ContentParent::GetLog(), LogLevel::Debug,
                    ("Preallocated = %lu of %d processes",
                     (unsigned long)mPreallocatedProcesses.size(),
                     mNumberPreallocs));

            // Continue prestarting processes if needed
            if (mPreallocatedProcesses.size() < mNumberPreallocs) {
              AllocateOnIdle();
            }
          } else {
            process->ShutDownProcess(ContentParent::SEND_SHUTDOWN_MESSAGE);
          }
        }
      },

      [self, this](ContentParent::LaunchError err) {
        mLaunchInProgress = false;
      });
}

void PreallocatedProcessManagerImpl::Disable() {
  if (!mEnabled) {
    return;
  }

  mEnabled = false;
  CloseProcesses();
}

void PreallocatedProcessManagerImpl::CloseProcesses() {
  while (!mPreallocatedProcesses.empty()) {
    RefPtr<ContentParent> process(mPreallocatedProcesses.front().forget());
    mPreallocatedProcesses.pop_front();
    process->ShutDownProcess(ContentParent::SEND_SHUTDOWN_MESSAGE);
    // drop ref and let it free
  }
  if (mPreallocatedE10SProcess) {
    mPreallocatedE10SProcess->ShutDownProcess(
        ContentParent::SEND_SHUTDOWN_MESSAGE);
    mPreallocatedE10SProcess = nullptr;
  }
}

inline PreallocatedProcessManagerImpl*
PreallocatedProcessManager::GetPPMImpl() {
  if (PreallocatedProcessManagerImpl::sShutdown) {
    return nullptr;
  }
  return PreallocatedProcessManagerImpl::Singleton();
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
bool PreallocatedProcessManager::Provide(ContentParent* aParent) {
  if (auto impl = GetPPMImpl()) {
    return impl->Provide(aParent);
  }
  return false;  // We didn't take the ContentParent
}

/* static */
void PreallocatedProcessManager::Erase(ContentParent* aParent) {
  if (auto impl = GetPPMImpl()) {
    impl->Erase(aParent);
  }
}

}  // namespace mozilla
