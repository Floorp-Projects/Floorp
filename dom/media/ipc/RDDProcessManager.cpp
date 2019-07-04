/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RDDProcessManager.h"

#include "mozilla/MemoryReportingProcess.h"
#include "mozilla/RemoteDecoderManagerChild.h"
#include "mozilla/RemoteDecoderManagerParent.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/layers/VideoBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "nsAppRunner.h"
#include "nsContentUtils.h"
#include "RDDChild.h"
#include "RDDProcessHost.h"

namespace mozilla {

using namespace mozilla::layers;

static StaticAutoPtr<RDDProcessManager> sRDDSingleton;

RDDProcessManager* RDDProcessManager::Get() { return sRDDSingleton; }

void RDDProcessManager::Initialize() {
  MOZ_ASSERT(XRE_IsParentProcess());
  sRDDSingleton = new RDDProcessManager();
}

void RDDProcessManager::Shutdown() { sRDDSingleton = nullptr; }

RDDProcessManager::RDDProcessManager()
    : mTaskFactory(this),
      mNumProcessAttempts(0),
      mProcess(nullptr),
      mProcessToken(0),
      mRDDChild(nullptr) {
  MOZ_COUNT_CTOR(RDDProcessManager);

  mObserver = new Observer(this);
  nsContentUtils::RegisterShutdownObserver(mObserver);
  Preferences::AddStrongObserver(mObserver, "");
}

RDDProcessManager::~RDDProcessManager() {
  MOZ_COUNT_DTOR(RDDProcessManager);

  // The RDD process should have already been shut down.
  MOZ_ASSERT(!mProcess && !mRDDChild);

  // We should have already removed observers.
  MOZ_ASSERT(!mObserver);
}

NS_IMPL_ISUPPORTS(RDDProcessManager::Observer, nsIObserver);

RDDProcessManager::Observer::Observer(RDDProcessManager* aManager)
    : mManager(aManager) {}

NS_IMETHODIMP
RDDProcessManager::Observer::Observe(nsISupports* aSubject, const char* aTopic,
                                     const char16_t* aData) {
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    mManager->OnXPCOMShutdown();
  } else if (!strcmp(aTopic, "nsPref:changed")) {
    mManager->OnPreferenceChange(aData);
  }
  return NS_OK;
}

void RDDProcessManager::OnXPCOMShutdown() {
  if (mObserver) {
    nsContentUtils::UnregisterShutdownObserver(mObserver);
    Preferences::RemoveObserver(mObserver, "");
    mObserver = nullptr;
  }

  CleanShutdown();
}

void RDDProcessManager::OnPreferenceChange(const char16_t* aData) {
  // A pref changed. If it's not on the blacklist, inform child processes.
  if (!dom::ContentParent::ShouldSyncPreference(aData)) {
    return;
  }

  // We know prefs are ASCII here.
  NS_LossyConvertUTF16toASCII strData(aData);

  mozilla::dom::Pref pref(strData, /* isLocked */ false, Nothing(), Nothing());
  Preferences::GetPreference(&pref);
  if (!!mRDDChild) {
    MOZ_ASSERT(mQueuedPrefs.IsEmpty());
    mRDDChild->SendPreferenceUpdate(pref);
  } else {
    mQueuedPrefs.AppendElement(pref);
  }
}

void RDDProcessManager::LaunchRDDProcess() {
  if (mProcess) {
    return;
  }

  mNumProcessAttempts++;

  std::vector<std::string> extraArgs;
  nsCString parentBuildID(mozilla::PlatformBuildID());
  extraArgs.push_back("-parentBuildID");
  extraArgs.push_back(parentBuildID.get());

  // The subprocess is launched asynchronously, so we wait for a callback to
  // acquire the IPDL actor.
  mProcess = new RDDProcessHost(this);
  if (!mProcess->Launch(extraArgs)) {
    DestroyProcess();
  }
}

bool RDDProcessManager::EnsureRDDReady() {
  if (mProcess && !mProcess->IsConnected()) {
    if (!mProcess->WaitForLaunch()) {
      // If this fails, we should have fired OnProcessLaunchComplete and
      // removed the process.
      MOZ_ASSERT(!mProcess && !mRDDChild);
      return false;
    }
  }

  if (mRDDChild) {
    if (mRDDChild->EnsureRDDReady()) {
      return true;
    }

    // If the initialization above fails, we likely have a RDD process teardown
    // waiting in our message queue (or will soon).
    DestroyProcess();
  }

  return false;
}

void RDDProcessManager::OnProcessLaunchComplete(RDDProcessHost* aHost) {
  MOZ_ASSERT(mProcess && mProcess == aHost);

  if (!mProcess->IsConnected()) {
    DestroyProcess();
    return;
  }

  mRDDChild = mProcess->GetActor();
  mProcessToken = mProcess->GetProcessToken();

  // Flush any pref updates that happened during launch and weren't
  // included in the blobs set up in LaunchRDDProcess.
  for (const mozilla::dom::Pref& pref : mQueuedPrefs) {
    Unused << NS_WARN_IF(!mRDDChild->SendPreferenceUpdate(pref));
  }
  mQueuedPrefs.Clear();

  ipc::Endpoint<PVideoBridgeParent> parentPipe;
  ipc::Endpoint<PVideoBridgeChild> childPipe;

  // Currently hardcoded to use the current process as the parent side,
  // we need to add support for the GPU process.
  nsresult rv = PVideoBridge::CreateEndpoints(
      base::GetCurrentProcId(), mRDDChild->OtherPid(), &parentPipe, &childPipe);
  if (NS_FAILED(rv)) {
    MOZ_LOG(sPDMLog, LogLevel::Debug,
            ("Could not create video bridge: %d", int(rv)));
    DestroyProcess();
    return;
  }

  mRDDChild->SendCreateVideoBridgeToParentProcess(std::move(childPipe));

  CompositorThreadHolder::Loop()->PostTask(
      NewRunnableFunction("gfx::VideoBridgeParent::Open",
                          &VideoBridgeParent::Open, std::move(parentPipe)));

  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::RDDProcessStatus,
      NS_LITERAL_CSTRING("Running"));
}

void RDDProcessManager::OnProcessUnexpectedShutdown(RDDProcessHost* aHost) {
  MOZ_ASSERT(mProcess && mProcess == aHost);

  DestroyProcess();
}

void RDDProcessManager::NotifyRemoteActorDestroyed(
    const uint64_t& aProcessToken) {
  if (!NS_IsMainThread()) {
    RefPtr<Runnable> task = mTaskFactory.NewRunnableMethod(
        &RDDProcessManager::NotifyRemoteActorDestroyed, aProcessToken);
    NS_DispatchToMainThread(task.forget());
    return;
  }

  if (mProcessToken != aProcessToken) {
    // This token is for an older process; we can safely ignore it.
    return;
  }

  // One of the bridged top-level actors for the RDD process has been
  // prematurely terminated, and we're receiving a notification. This
  // can happen if the ActorDestroy for a bridged protocol fires
  // before the ActorDestroy for PRDDChild.
  OnProcessUnexpectedShutdown(mProcess);
}

void RDDProcessManager::CleanShutdown() { DestroyProcess(); }

void RDDProcessManager::KillProcess() {
  if (!mProcess) {
    return;
  }

  mProcess->KillProcess();
}

void RDDProcessManager::DestroyProcess() {
  if (!mProcess) {
    return;
  }

  mProcess->Shutdown();
  mProcessToken = 0;
  mProcess = nullptr;
  mRDDChild = nullptr;

  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::RDDProcessStatus,
      NS_LITERAL_CSTRING("Destroyed"));
}

bool RDDProcessManager::CreateContentBridge(
    base::ProcessId aOtherProcess,
    ipc::Endpoint<PRemoteDecoderManagerChild>* aOutRemoteDecoderManager) {
  if (!EnsureRDDReady() || !StaticPrefs::media_rdd_process_enabled()) {
    return false;
  }

  ipc::Endpoint<PRemoteDecoderManagerParent> parentPipe;
  ipc::Endpoint<PRemoteDecoderManagerChild> childPipe;

  nsresult rv = PRemoteDecoderManager::CreateEndpoints(
      mRDDChild->OtherPid(), aOtherProcess, &parentPipe, &childPipe);
  if (NS_FAILED(rv)) {
    MOZ_LOG(sPDMLog, LogLevel::Debug,
            ("Could not create content remote decoder: %d", int(rv)));
    return false;
  }

  mRDDChild->SendNewContentRemoteDecoderManager(std::move(parentPipe));

  *aOutRemoteDecoderManager = std::move(childPipe);
  return true;
}

base::ProcessId RDDProcessManager::RDDProcessPid() {
  base::ProcessId rddPid = mRDDChild ? mRDDChild->OtherPid() : -1;
  return rddPid;
}

class RDDMemoryReporter : public MemoryReportingProcess {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RDDMemoryReporter, override)

  bool IsAlive() const override { return !!GetChild(); }

  bool SendRequestMemoryReport(
      const uint32_t& aGeneration, const bool& aAnonymize,
      const bool& aMinimizeMemoryUsage,
      const Maybe<ipc::FileDescriptor>& aDMDFile) override {
    RDDChild* child = GetChild();
    if (!child) {
      return false;
    }

    return child->SendRequestMemoryReport(aGeneration, aAnonymize,
                                          aMinimizeMemoryUsage, aDMDFile);
  }

  int32_t Pid() const override {
    if (RDDChild* child = GetChild()) {
      return (int32_t)child->OtherPid();
    }
    return 0;
  }

 private:
  RDDChild* GetChild() const {
    if (RDDProcessManager* rddpm = RDDProcessManager::Get()) {
      if (RDDChild* child = rddpm->GetRDDChild()) {
        return child;
      }
    }
    return nullptr;
  }

 protected:
  ~RDDMemoryReporter() = default;
};

RefPtr<MemoryReportingProcess> RDDProcessManager::GetProcessMemoryReporter() {
  if (!EnsureRDDReady()) {
    return nullptr;
  }
  return new RDDMemoryReporter();
}

}  // namespace mozilla
