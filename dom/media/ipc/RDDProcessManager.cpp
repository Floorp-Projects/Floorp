/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RDDProcessManager.h"

#include "RDDChild.h"
#include "RDDProcessHost.h"
#include "mozilla/MemoryReportingProcess.h"
#include "mozilla/Preferences.h"
#include "mozilla/RemoteDecoderManagerChild.h"
#include "mozilla/RemoteDecoderManagerParent.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/SyncRunnable.h"  // for LaunchRDDProcess
#include "mozilla/dom/ContentParent.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/VideoBridgeParent.h"
#include "nsAppRunner.h"
#include "nsContentUtils.h"

namespace mozilla {

using namespace gfx;
using namespace layers;

static StaticAutoPtr<RDDProcessManager> sRDDSingleton;

static bool sRDDProcessShutdown = false;

bool RDDProcessManager::IsShutdown() const {
  MOZ_ASSERT(NS_IsMainThread());
  return sRDDProcessShutdown || !sRDDSingleton;
}

RDDProcessManager* RDDProcessManager::Get() { return sRDDSingleton; }

void RDDProcessManager::Initialize() {
  MOZ_ASSERT(XRE_IsParentProcess());
  sRDDSingleton = new RDDProcessManager();
}

void RDDProcessManager::Shutdown() { sRDDSingleton = nullptr; }

void RDDProcessManager::RDDProcessShutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  sRDDProcessShutdown = true;
  if (sRDDSingleton) {
    sRDDSingleton->DestroyProcess();
  }
}

RDDProcessManager::RDDProcessManager()
    : mObserver(new Observer(this)), mTaskFactory(this) {
  MOZ_COUNT_CTOR(RDDProcessManager);
  // Start listening for pref changes so we can
  // forward them to the process once it is running.
  nsContentUtils::RegisterShutdownObserver(mObserver);
  Preferences::AddStrongObserver(mObserver, "");
}

RDDProcessManager::~RDDProcessManager() {
  MOZ_COUNT_DTOR(RDDProcessManager);
  MOZ_ASSERT(NS_IsMainThread());

  // The RDD process should have already been shut down.
  MOZ_ASSERT(!mProcess && !mRDDChild);
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
  MOZ_ASSERT(NS_IsMainThread());
  nsContentUtils::UnregisterShutdownObserver(mObserver);
  Preferences::RemoveObserver(mObserver, "");
}

void RDDProcessManager::OnPreferenceChange(const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mProcess) {
    // Process hasn't been launched yet
    return;
  }

  // We know prefs are ASCII here.
  NS_LossyConvertUTF16toASCII strData(aData);

  mozilla::dom::Pref pref(strData, /* isLocked */ false,
                          /* isSanitized */ false, Nothing(), Nothing());

  Preferences::GetPreference(&pref, GeckoProcessType_RDD,
                             /* remoteType */ ""_ns);
  if (!!mRDDChild) {
    MOZ_ASSERT(mQueuedPrefs.IsEmpty());
    mRDDChild->SendPreferenceUpdate(pref);
  } else if (IsRDDProcessLaunching()) {
    mQueuedPrefs.AppendElement(pref);
  }
}

RefPtr<GenericNonExclusivePromise> RDDProcessManager::LaunchRDDProcess() {
  MOZ_ASSERT(NS_IsMainThread());

  if (IsShutdown()) {
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE,
                                                       __func__);
  }

  if (mNumProcessAttempts && !StaticPrefs::media_rdd_retryonfailure_enabled()) {
    // We failed to start the RDD process earlier, abort now.
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE,
                                                       __func__);
  }

  if (mLaunchRDDPromise && mProcess) {
    return mLaunchRDDPromise;
  }

  std::vector<std::string> extraArgs;
  ipc::ProcessChild::AddPlatformBuildID(extraArgs);

  // The subprocess is launched asynchronously, so we
  // wait for the promise to be resolved to acquire the IPDL actor.
  mProcess = new RDDProcessHost(this);
  if (!mProcess->Launch(extraArgs)) {
    mNumProcessAttempts++;
    DestroyProcess();
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE,
                                                       __func__);
  }

  mLaunchRDDPromise = mProcess->LaunchPromise()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [this](bool) {
        if (IsShutdown()) {
          return GenericNonExclusivePromise::CreateAndReject(
              NS_ERROR_NOT_AVAILABLE, __func__);
        }

        if (IsRDDProcessDestroyed()) {
          return GenericNonExclusivePromise::CreateAndReject(
              NS_ERROR_NOT_AVAILABLE, __func__);
        }

        mRDDChild = mProcess->GetActor();
        mProcessToken = mProcess->GetProcessToken();

        // Flush any pref updates that happened during
        // launch and weren't included in the blobs set
        // up in LaunchRDDProcess.
        for (const mozilla::dom::Pref& pref : mQueuedPrefs) {
          Unused << NS_WARN_IF(!mRDDChild->SendPreferenceUpdate(pref));
        }
        mQueuedPrefs.Clear();

        CrashReporter::AnnotateCrashReport(
            CrashReporter::Annotation::RDDProcessStatus, "Running"_ns);

        if (!CreateVideoBridge()) {
          mNumProcessAttempts++;
          DestroyProcess();
          return GenericNonExclusivePromise::CreateAndReject(
              NS_ERROR_NOT_AVAILABLE, __func__);
        }
        return GenericNonExclusivePromise::CreateAndResolve(true, __func__);
      },
      [this](nsresult aError) {
        if (Get()) {
          mNumProcessAttempts++;
          DestroyProcess();
        }
        return GenericNonExclusivePromise::CreateAndReject(aError, __func__);
      });
  return mLaunchRDDPromise;
}

auto RDDProcessManager::EnsureRDDProcessAndCreateBridge(
    base::ProcessId aOtherProcess, dom::ContentParentId aParentId)
    -> RefPtr<EnsureRDDPromise> {
  return InvokeAsync(
      GetMainThreadSerialEventTarget(), __func__,
      [aOtherProcess, aParentId, this]() -> RefPtr<EnsureRDDPromise> {
        return LaunchRDDProcess()->Then(
            GetMainThreadSerialEventTarget(), __func__,
            [aOtherProcess, aParentId, this]() {
              if (IsShutdown()) {
                return EnsureRDDPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE,
                                                         __func__);
              }
              ipc::Endpoint<PRemoteDecoderManagerChild> endpoint;
              if (!CreateContentBridge(aOtherProcess, aParentId, &endpoint)) {
                return EnsureRDDPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE,
                                                         __func__);
              }
              mNumProcessAttempts = 0;
              return EnsureRDDPromise::CreateAndResolve(std::move(endpoint),
                                                        __func__);
            },
            [](nsresult aError) {
              return EnsureRDDPromise::CreateAndReject(aError, __func__);
            });
      });
}

bool RDDProcessManager::IsRDDProcessLaunching() {
  MOZ_ASSERT(NS_IsMainThread());
  return !!mProcess && !mRDDChild;
}

bool RDDProcessManager::IsRDDProcessDestroyed() const {
  MOZ_ASSERT(NS_IsMainThread());
  return !mRDDChild && !mProcess;
}

void RDDProcessManager::OnProcessUnexpectedShutdown(RDDProcessHost* aHost) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mProcess && mProcess == aHost);

  mNumUnexpectedCrashes++;

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

void RDDProcessManager::DestroyProcess() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mProcess) {
    return;
  }

  mProcess->Shutdown();
  mProcessToken = 0;
  mProcess = nullptr;
  mRDDChild = nullptr;
  mQueuedPrefs.Clear();

  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::RDDProcessStatus, "Destroyed"_ns);
}

bool RDDProcessManager::CreateContentBridge(
    base::ProcessId aOtherProcess, dom::ContentParentId aParentId,
    ipc::Endpoint<PRemoteDecoderManagerChild>* aOutRemoteDecoderManager) {
  MOZ_ASSERT(NS_IsMainThread());

  if (IsRDDProcessDestroyed()) {
    MOZ_LOG(sPDMLog, LogLevel::Debug,
            ("RDD shutdown before creating content bridge"));
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

  mRDDChild->SendNewContentRemoteDecoderManager(std::move(parentPipe),
                                                aParentId);

  *aOutRemoteDecoderManager = std::move(childPipe);
  return true;
}

bool RDDProcessManager::CreateVideoBridge() {
  MOZ_ASSERT(NS_IsMainThread());
  ipc::Endpoint<PVideoBridgeParent> parentPipe;
  ipc::Endpoint<PVideoBridgeChild> childPipe;

  GPUProcessManager* gpuManager = GPUProcessManager::Get();
  base::ProcessId gpuProcessPid =
      gpuManager ? gpuManager->GPUProcessPid() : base::kInvalidProcessId;

  // Build content device data first; this ensure that the GPU process is fully
  // ready.
  ContentDeviceData contentDeviceData;
  gfxPlatform::GetPlatform()->BuildContentDeviceData(&contentDeviceData);

  // The child end is the producer of video frames; the parent end is the
  // consumer.
  base::ProcessId childPid = RDDProcessPid();
  base::ProcessId parentPid = gpuProcessPid != base::kInvalidProcessId
                                  ? gpuProcessPid
                                  : base::GetCurrentProcId();

  nsresult rv = PVideoBridge::CreateEndpoints(parentPid, childPid, &parentPipe,
                                              &childPipe);
  if (NS_FAILED(rv)) {
    MOZ_LOG(sPDMLog, LogLevel::Debug,
            ("Could not create video bridge: %d", int(rv)));
    return false;
  }

  mRDDChild->SendInitVideoBridge(std::move(childPipe),
                                 mNumUnexpectedCrashes == 0, contentDeviceData);
  if (gpuProcessPid != base::kInvalidProcessId) {
    gpuManager->InitVideoBridge(std::move(parentPipe),
                                VideoBridgeSource::RddProcess);
  } else {
    VideoBridgeParent::Open(std::move(parentPipe),
                            VideoBridgeSource::RddProcess);
  }

  return true;
}

base::ProcessId RDDProcessManager::RDDProcessPid() {
  MOZ_ASSERT(NS_IsMainThread());
  base::ProcessId rddPid =
      mRDDChild ? mRDDChild->OtherPid() : base::kInvalidProcessId;
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
  if (!mProcess || !mProcess->IsConnected()) {
    return nullptr;
  }
  return new RDDMemoryReporter();
}

RefPtr<PRDDChild::TestTriggerMetricsPromise>
RDDProcessManager::TestTriggerMetrics() {
  if (!NS_WARN_IF(!mRDDChild)) {
    return mRDDChild->SendTestTriggerMetrics();
  }

  return PRDDChild::TestTriggerMetricsPromise::CreateAndReject(
      ipc::ResponseRejectReason::SendError, __func__);
}

}  // namespace mozilla
