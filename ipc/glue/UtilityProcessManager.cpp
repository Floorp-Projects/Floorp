/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "UtilityProcessManager.h"

#include "mozilla/ipc/UtilityProcessHost.h"
#include "mozilla/MemoryReportingProcess.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/SyncRunnable.h"  // for LaunchUtilityProcess
#include "mozilla/ipc/UtilityProcessParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/UtilityProcessSandboxing.h"
#include "mozilla/ipc/ProcessChild.h"
#include "nsAppRunner.h"
#include "nsContentUtils.h"

#include "mozilla/GeckoArgs.h"

namespace mozilla::ipc {

static StaticRefPtr<UtilityProcessManager> sSingleton;

static bool sXPCOMShutdown = false;

bool UtilityProcessManager::IsShutdown() const {
  MOZ_ASSERT(NS_IsMainThread());
  return sXPCOMShutdown || !sSingleton;
}

RefPtr<UtilityProcessManager> UtilityProcessManager::GetSingleton() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!sXPCOMShutdown && sSingleton == nullptr) {
    sSingleton = new UtilityProcessManager();
  }
  return sSingleton;
}

UtilityProcessManager::UtilityProcessManager() : mObserver(new Observer(this)) {
  // Start listening for pref changes so we can
  // forward them to the process once it is running.
  nsContentUtils::RegisterShutdownObserver(mObserver);
  Preferences::AddStrongObserver(mObserver, "");
}

UtilityProcessManager::~UtilityProcessManager() {
  // The Utility process should have already been shut down.
  MOZ_ASSERT(!mProcess && !mProcessParent);
}

NS_IMPL_ISUPPORTS(UtilityProcessManager::Observer, nsIObserver);

UtilityProcessManager::Observer::Observer(
    RefPtr<UtilityProcessManager> aManager)
    : mManager(std::move(aManager)) {}

NS_IMETHODIMP
UtilityProcessManager::Observer::Observe(nsISupports* aSubject,
                                         const char* aTopic,
                                         const char16_t* aData) {
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    mManager->OnXPCOMShutdown();
  } else if (!strcmp(aTopic, "nsPref:changed")) {
    mManager->OnPreferenceChange(aData);
  }
  return NS_OK;
}

void UtilityProcessManager::OnXPCOMShutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  sXPCOMShutdown = true;
  nsContentUtils::UnregisterShutdownObserver(mObserver);
  CleanShutdown();
}

void UtilityProcessManager::OnPreferenceChange(const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mProcess) {
    // Process hasn't been launched yet
    return;
  }
  // A pref changed. If it is useful to do so, inform child processes.
  if (!dom::ContentParent::ShouldSyncPreference(aData)) {
    return;
  }

  // We know prefs are ASCII here.
  NS_LossyConvertUTF16toASCII strData(aData);

  mozilla::dom::Pref pref(strData, /* isLocked */ false, Nothing(), Nothing());
  Preferences::GetPreference(&pref);
  if (bool(mProcessParent)) {
    MOZ_ASSERT(mQueuedPrefs.IsEmpty());
    Unused << mProcessParent->SendPreferenceUpdate(pref);
  } else if (IsProcessLaunching()) {
    mQueuedPrefs.AppendElement(pref);
  }
}

RefPtr<GenericNonExclusivePromise> UtilityProcessManager::LaunchProcess(
    SandboxingKind aSandbox) {
  MOZ_ASSERT(NS_IsMainThread());

  if (IsShutdown()) {
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE,
                                                       __func__);
  }

  if (mNumProcessAttempts) {
    // We failed to start the Utility process earlier, abort now.
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE,
                                                       __func__);
  }

  if (mLaunchPromise && mProcess) {
    return mLaunchPromise;
  }

  std::vector<std::string> extraArgs;
  ProcessChild::AddPlatformBuildID(extraArgs);
  geckoargs::sSandboxingKind.Put(aSandbox, extraArgs);

  // The subprocess is launched asynchronously, so we
  // wait for the promise to be resolved to acquire the IPDL actor.
  mProcess = new UtilityProcessHost(aSandbox, this);
  if (!mProcess->Launch(extraArgs)) {
    mNumProcessAttempts++;
    DestroyProcess();
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE,
                                                       __func__);
  }

  RefPtr<UtilityProcessManager> self = this;
  mLaunchPromise = mProcess->LaunchPromise()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [self](bool) {
        if (self->IsShutdown()) {
          return GenericNonExclusivePromise::CreateAndReject(
              NS_ERROR_NOT_AVAILABLE, __func__);
        }

        if (self->IsProcessDestroyed()) {
          return GenericNonExclusivePromise::CreateAndReject(
              NS_ERROR_NOT_AVAILABLE, __func__);
        }

        self->mProcessParent = self->mProcess->GetActor();

        // Flush any pref updates that happened during
        // launch and weren't included in the blobs set
        // up in LaunchUtilityProcess.
        for (const mozilla::dom::Pref& pref : self->mQueuedPrefs) {
          Unused << NS_WARN_IF(
              !self->mProcessParent->SendPreferenceUpdate(pref));
        }
        self->mQueuedPrefs.Clear();

        CrashReporter::AnnotateCrashReport(
            CrashReporter::Annotation::UtilityProcessStatus, "Running"_ns);

        return GenericNonExclusivePromise::CreateAndResolve(true, __func__);
      },
      [self](nsresult aError) {
        if (GetSingleton()) {
          self->mNumProcessAttempts++;
          self->DestroyProcess();
        }
        return GenericNonExclusivePromise::CreateAndReject(aError, __func__);
      });
  return mLaunchPromise;
}

bool UtilityProcessManager::IsProcessLaunching() {
  MOZ_ASSERT(NS_IsMainThread());
  return mProcess && !mProcessParent;
}

bool UtilityProcessManager::IsProcessDestroyed() const {
  MOZ_ASSERT(NS_IsMainThread());
  return !mProcessParent && !mProcess;
}

void UtilityProcessManager::OnProcessUnexpectedShutdown(
    UtilityProcessHost* aHost) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mProcess && mProcess == aHost);

  mNumUnexpectedCrashes++;

  DestroyProcess();
}

void UtilityProcessManager::NotifyRemoteActorDestroyed() {
  if (!NS_IsMainThread()) {
    RefPtr<UtilityProcessManager> self = this;
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "UtilityProcessManager::NotifyRemoteActorDestroyed()",
        [self]() { self->NotifyRemoteActorDestroyed(); }));
    return;
  }

  // One of the bridged top-level actors for the Utility process has been
  // prematurely terminated, and we're receiving a notification. This
  // can happen if the ActorDestroy for a bridged protocol fires
  // before the ActorDestroy for PUtilityProcessParent.
  OnProcessUnexpectedShutdown(mProcess);
}

void UtilityProcessManager::CleanShutdown() { DestroyProcess(); }

void UtilityProcessManager::DestroyProcess() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  mQueuedPrefs.Clear();
  if (mObserver) {
    Preferences::RemoveObserver(mObserver, "");
  }

  mObserver = nullptr;
  mProcessParent = nullptr;
  sSingleton = nullptr;

  if (!mProcess) {
    return;
  }

  mProcess->Shutdown();
  mProcess = nullptr;

  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::UtilityProcessStatus, "Destroyed"_ns);
}

Maybe<base::ProcessId> UtilityProcessManager::ProcessPid() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mProcessParent) {
    return Some(mProcessParent->OtherPid());
  }
  return Nothing();
}

class UtilityMemoryReporter : public MemoryReportingProcess {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UtilityMemoryReporter, override)

  bool IsAlive() const override { return bool(GetChild()); }

  bool SendRequestMemoryReport(
      const uint32_t& aGeneration, const bool& aAnonymize,
      const bool& aMinimizeMemoryUsage,
      const Maybe<ipc::FileDescriptor>& aDMDFile) override {
    UtilityProcessParent* child = GetChild();
    if (!child) {
      return false;
    }

    return child->SendRequestMemoryReport(aGeneration, aAnonymize,
                                          aMinimizeMemoryUsage, aDMDFile);
  }

  int32_t Pid() const override {
    if (UtilityProcessParent* child = GetChild()) {
      return (int32_t)child->OtherPid();
    }
    return 0;
  }

 private:
  UtilityProcessParent* GetChild() const {
    if (RefPtr<UtilityProcessManager> utilitypm =
            UtilityProcessManager::GetSingleton()) {
      if (UtilityProcessParent* child = utilitypm->GetProcessParent()) {
        return child;
      }
    }
    return nullptr;
  }

 protected:
  ~UtilityMemoryReporter() = default;
};

RefPtr<MemoryReportingProcess>
UtilityProcessManager::GetProcessMemoryReporter() {
  if (!mProcess || !mProcess->IsConnected()) {
    return nullptr;
  }
  return new UtilityMemoryReporter();
}

}  // namespace mozilla::ipc
