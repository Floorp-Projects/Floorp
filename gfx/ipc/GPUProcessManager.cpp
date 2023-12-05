/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GPUProcessManager.h"

#include "gfxConfig.h"
#include "gfxPlatform.h"
#include "GPUProcessHost.h"
#include "GPUProcessListener.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/MemoryReportingProcess.h"
#include "mozilla/Preferences.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/RemoteDecoderManagerChild.h"
#include "mozilla/RemoteDecoderManagerParent.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/GPUChild.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/layers/APZCTreeManagerChild.h"
#include "mozilla/layers/APZInputBridgeChild.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorManagerChild.h"
#include "mozilla/layers/CompositorManagerParent.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/InProcessCompositorSession.h"
#include "mozilla/layers/LayerTreeOwnerTracker.h"
#include "mozilla/layers/RemoteCompositorSession.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#include "nsAppRunner.h"
#include "mozilla/widget/CompositorWidget.h"
#ifdef MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING
#  include "mozilla/widget/CompositorWidgetChild.h"
#endif
#include "nsBaseWidget.h"
#include "nsContentUtils.h"
#include "VRManagerChild.h"
#include "VRManagerParent.h"
#include "VsyncBridgeChild.h"
#include "VsyncIOThreadHolder.h"
#include "VsyncSource.h"
#include "nsExceptionHandler.h"
#include "nsPrintfCString.h"

#if defined(MOZ_WIDGET_ANDROID)
#  include "mozilla/java/SurfaceControlManagerWrappers.h"
#  include "mozilla/widget/AndroidUiThread.h"
#  include "mozilla/layers/UiCompositorControllerChild.h"
#endif  // defined(MOZ_WIDGET_ANDROID)

#if defined(XP_WIN)
#  include "gfxWindowsPlatform.h"
#endif

namespace mozilla {
namespace gfx {

using namespace mozilla::layers;

enum class FallbackType : uint32_t {
  NONE = 0,
  DECODINGDISABLED,
  DISABLED,
};

static StaticAutoPtr<GPUProcessManager> sSingleton;

GPUProcessManager* GPUProcessManager::Get() { return sSingleton; }

void GPUProcessManager::Initialize() {
  MOZ_ASSERT(XRE_IsParentProcess());
  sSingleton = new GPUProcessManager();
}

void GPUProcessManager::Shutdown() { sSingleton = nullptr; }

GPUProcessManager::GPUProcessManager()
    : mTaskFactory(this),
      mNextNamespace(0),
      mIdNamespace(0),
      mResourceId(0),
      mUnstableProcessAttempts(0),
      mTotalProcessAttempts(0),
      mDeviceResetCount(0),
      mAppInForeground(true),
      mProcess(nullptr),
      mProcessToken(0),
      mProcessStable(true),
      mGPUChild(nullptr) {
  MOZ_COUNT_CTOR(GPUProcessManager);

  mIdNamespace = AllocateNamespace();

  mDeviceResetLastTime = TimeStamp::Now();

  LayerTreeOwnerTracker::Initialize();
  CompositorBridgeParent::InitializeStatics();
}

GPUProcessManager::~GPUProcessManager() {
  MOZ_COUNT_DTOR(GPUProcessManager);

  LayerTreeOwnerTracker::Shutdown();

  // The GPU process should have already been shut down.
  MOZ_ASSERT(!mProcess && !mGPUChild);

  // We should have already removed observers.
  MOZ_ASSERT(!mObserver);
}

NS_IMPL_ISUPPORTS(GPUProcessManager::Observer, nsIObserver);

GPUProcessManager::Observer::Observer(GPUProcessManager* aManager)
    : mManager(aManager) {}

NS_IMETHODIMP
GPUProcessManager::Observer::Observe(nsISupports* aSubject, const char* aTopic,
                                     const char16_t* aData) {
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    mManager->OnXPCOMShutdown();
  } else if (!strcmp(aTopic, "nsPref:changed")) {
    mManager->OnPreferenceChange(aData);
  } else if (!strcmp(aTopic, "application-foreground")) {
    mManager->mAppInForeground = true;
    if (!mManager->mProcess && gfxConfig::IsEnabled(Feature::GPU_PROCESS)) {
      Unused << mManager->LaunchGPUProcess();
    }
  } else if (!strcmp(aTopic, "application-background")) {
    mManager->mAppInForeground = false;
  }
  return NS_OK;
}

void GPUProcessManager::OnXPCOMShutdown() {
  if (mObserver) {
    nsContentUtils::UnregisterShutdownObserver(mObserver);
    Preferences::RemoveObserver(mObserver, "");
    nsCOMPtr<nsIObserverService> obsServ = services::GetObserverService();
    if (obsServ) {
      obsServ->RemoveObserver(mObserver, "application-foreground");
      obsServ->RemoveObserver(mObserver, "application-background");
    }
    mObserver = nullptr;
  }

  CleanShutdown();
}

void GPUProcessManager::OnPreferenceChange(const char16_t* aData) {
  // We know prefs are ASCII here.
  NS_LossyConvertUTF16toASCII strData(aData);

  mozilla::dom::Pref pref(strData, /* isLocked */ false,
                          /* isSanitized */ false, Nothing(), Nothing());

  Preferences::GetPreference(&pref, GeckoProcessType_GPU,
                             /* remoteType */ ""_ns);
  if (!!mGPUChild) {
    MOZ_ASSERT(mQueuedPrefs.IsEmpty());
    mGPUChild->SendPreferenceUpdate(pref);
  } else if (IsGPUProcessLaunching()) {
    mQueuedPrefs.AppendElement(pref);
  }
}

void GPUProcessManager::ResetProcessStable() {
  mTotalProcessAttempts++;
  mProcessStable = false;
  mProcessAttemptLastTime = TimeStamp::Now();
}

bool GPUProcessManager::IsProcessStable(const TimeStamp& aNow) {
  if (mTotalProcessAttempts > 0) {
    auto delta = (int32_t)(aNow - mProcessAttemptLastTime).ToMilliseconds();
    if (delta < StaticPrefs::layers_gpu_process_stable_min_uptime_ms()) {
      return false;
    }
  }
  return mProcessStable;
}

bool GPUProcessManager::LaunchGPUProcess() {
  if (mProcess) {
    return true;
  }

  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdown)) {
    return false;
  }

  // Start listening for pref changes so we can
  // forward them to the process once it is running.
  if (!mObserver) {
    mObserver = new Observer(this);
    nsContentUtils::RegisterShutdownObserver(mObserver);
    Preferences::AddStrongObserver(mObserver, "");
    nsCOMPtr<nsIObserverService> obsServ = services::GetObserverService();
    if (obsServ) {
      obsServ->AddObserver(mObserver, "application-foreground", false);
      obsServ->AddObserver(mObserver, "application-background", false);
    }
  }

  // Start the Vsync I/O thread so can use it as soon as the process launches.
  EnsureVsyncIOThread();

  // If the process didn't live long enough, reset the stable flag so that we
  // don't end up in a restart loop.
  auto newTime = TimeStamp::Now();
  if (!IsProcessStable(newTime)) {
    mUnstableProcessAttempts++;
  }
  mTotalProcessAttempts++;
  mProcessAttemptLastTime = newTime;
  mProcessStable = false;

  std::vector<std::string> extraArgs;
  ipc::ProcessChild::AddPlatformBuildID(extraArgs);

  // The subprocess is launched asynchronously, so we wait for a callback to
  // acquire the IPDL actor.
  mProcess = new GPUProcessHost(this);
  if (!mProcess->Launch(extraArgs)) {
    DisableGPUProcess("Failed to launch GPU process");
  }

  return true;
}

bool GPUProcessManager::IsGPUProcessLaunching() {
  MOZ_ASSERT(NS_IsMainThread());
  return !!mProcess && !mGPUChild;
}

void GPUProcessManager::DisableGPUProcess(const char* aMessage) {
  MaybeDisableGPUProcess(aMessage, /* aAllowRestart */ false);
}

bool GPUProcessManager::MaybeDisableGPUProcess(const char* aMessage,
                                               bool aAllowRestart) {
  if (!gfxConfig::IsEnabled(Feature::GPU_PROCESS)) {
    return true;
  }

  if (!aAllowRestart) {
    gfxConfig::SetFailed(Feature::GPU_PROCESS, FeatureStatus::Failed, aMessage);
  }

  bool wantRestart;
  if (mLastError) {
    wantRestart =
        FallbackFromAcceleration(mLastError.value(), mLastErrorMsg.ref());
    mLastError.reset();
    mLastErrorMsg.reset();
  } else {
    wantRestart = gfxPlatform::FallbackFromAcceleration(
        FeatureStatus::Unavailable, "GPU Process is disabled",
        "FEATURE_FAILURE_GPU_PROCESS_DISABLED"_ns);
  }
  if (aAllowRestart && wantRestart) {
    // The fallback method can make use of the GPU process.
    return false;
  }

  if (aAllowRestart) {
    gfxConfig::SetFailed(Feature::GPU_PROCESS, FeatureStatus::Failed, aMessage);
  }

  MOZ_ASSERT(!gfxConfig::IsEnabled(Feature::GPU_PROCESS));

  gfxCriticalNote << aMessage;

  gfxPlatform::DisableGPUProcess();

  Telemetry::Accumulate(Telemetry::GPU_PROCESS_CRASH_FALLBACKS,
                        uint32_t(FallbackType::DISABLED));

  DestroyProcess();
  ShutdownVsyncIOThread();

  // Now the stability state is based upon the in process compositor session.
  ResetProcessStable();

  // We may have been in the middle of guaranteeing our various services are
  // available when one failed. Some callers may fallback to using the same
  // process equivalent, and we need to make sure those services are setup
  // correctly. We cannot re-enter DisableGPUProcess from this call because we
  // know that it is disabled in the config above.
  DebugOnly<bool> ready = EnsureProtocolsReady();
  MOZ_ASSERT_IF(!ready,
                AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdown));

  // If we disable the GPU process during reinitialization after a previous
  // crash, then we need to tell the content processes again, because they
  // need to rebind to the UI process.
  HandleProcessLost();
  return true;
}

nsresult GPUProcessManager::EnsureGPUReady() {
  MOZ_ASSERT(NS_IsMainThread());

  // We only wait to fail with NS_ERROR_ILLEGAL_DURING_SHUTDOWN if we would
  // cause a state change or if we are in the middle of relaunching the GPU
  // process.
  bool inShutdown = AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdown);

  // Launch the GPU process if it is enabled but hasn't been (re-)launched yet.
  if (!mProcess && gfxConfig::IsEnabled(Feature::GPU_PROCESS)) {
    if (NS_WARN_IF(inShutdown)) {
      return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
    }

    if (!LaunchGPUProcess()) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  if (mProcess && !mProcess->IsConnected()) {
    if (NS_WARN_IF(inShutdown)) {
      return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
    }

    if (!mProcess->WaitForLaunch()) {
      // If this fails, we should have fired OnProcessLaunchComplete and
      // removed the process.
      MOZ_ASSERT(!mProcess && !mGPUChild);
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  if (mGPUChild) {
    if (mGPUChild->EnsureGPUReady()) {
      return NS_OK;
    }

    // If the initialization above fails, we likely have a GPU process teardown
    // waiting in our message queue (or will soon). We need to ensure we don't
    // restart it later because if we fail here, our callers assume they should
    // fall back to a combined UI/GPU process. This also ensures our internal
    // state is consistent (e.g. process token is reset).
    DisableGPUProcess("Failed to initialize GPU process");
  }

  // This is the first time we are trying to use the in-process compositor.
  if (mTotalProcessAttempts == 0) {
    if (NS_WARN_IF(inShutdown)) {
      return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
    }
    ResetProcessStable();
  }
  return NS_ERROR_NOT_AVAILABLE;
}

bool GPUProcessManager::EnsureProtocolsReady() {
  return EnsureCompositorManagerChild() && EnsureImageBridgeChild() &&
         EnsureVRManager();
}

bool GPUProcessManager::EnsureCompositorManagerChild() {
  nsresult rv = EnsureGPUReady();
  if (NS_WARN_IF(rv == NS_ERROR_ILLEGAL_DURING_SHUTDOWN)) {
    return false;
  }

  if (CompositorManagerChild::IsInitialized(mProcessToken)) {
    return true;
  }

  if (NS_FAILED(rv)) {
    CompositorManagerChild::InitSameProcess(AllocateNamespace(), mProcessToken);
    return true;
  }

  ipc::Endpoint<PCompositorManagerParent> parentPipe;
  ipc::Endpoint<PCompositorManagerChild> childPipe;
  rv = PCompositorManager::CreateEndpoints(
      mGPUChild->OtherPid(), base::GetCurrentProcId(), &parentPipe, &childPipe);
  if (NS_FAILED(rv)) {
    DisableGPUProcess("Failed to create PCompositorManager endpoints");
    return true;
  }

  mGPUChild->SendInitCompositorManager(std::move(parentPipe));
  CompositorManagerChild::Init(std::move(childPipe), AllocateNamespace(),
                               mProcessToken);
  return true;
}

bool GPUProcessManager::EnsureImageBridgeChild() {
  if (ImageBridgeChild::GetSingleton()) {
    return true;
  }

  nsresult rv = EnsureGPUReady();
  if (NS_WARN_IF(rv == NS_ERROR_ILLEGAL_DURING_SHUTDOWN)) {
    return false;
  }

  if (NS_FAILED(rv)) {
    ImageBridgeChild::InitSameProcess(AllocateNamespace());
    return true;
  }

  ipc::Endpoint<PImageBridgeParent> parentPipe;
  ipc::Endpoint<PImageBridgeChild> childPipe;
  rv = PImageBridge::CreateEndpoints(
      mGPUChild->OtherPid(), base::GetCurrentProcId(), &parentPipe, &childPipe);
  if (NS_FAILED(rv)) {
    DisableGPUProcess("Failed to create PImageBridge endpoints");
    return true;
  }

  mGPUChild->SendInitImageBridge(std::move(parentPipe));
  ImageBridgeChild::InitWithGPUProcess(std::move(childPipe),
                                       AllocateNamespace());
  return true;
}

bool GPUProcessManager::EnsureVRManager() {
  if (VRManagerChild::IsCreated()) {
    return true;
  }

  nsresult rv = EnsureGPUReady();
  if (NS_WARN_IF(rv == NS_ERROR_ILLEGAL_DURING_SHUTDOWN)) {
    return false;
  }

  if (NS_FAILED(rv)) {
    VRManagerChild::InitSameProcess();
    return true;
  }

  ipc::Endpoint<PVRManagerParent> parentPipe;
  ipc::Endpoint<PVRManagerChild> childPipe;
  rv = PVRManager::CreateEndpoints(
      mGPUChild->OtherPid(), base::GetCurrentProcId(), &parentPipe, &childPipe);
  if (NS_FAILED(rv)) {
    DisableGPUProcess("Failed to create PVRManager endpoints");
    return true;
  }

  mGPUChild->SendInitVRManager(std::move(parentPipe));
  VRManagerChild::InitWithGPUProcess(std::move(childPipe));
  return true;
}

#if defined(MOZ_WIDGET_ANDROID)
already_AddRefed<UiCompositorControllerChild>
GPUProcessManager::CreateUiCompositorController(nsBaseWidget* aWidget,
                                                const LayersId aId) {
  RefPtr<UiCompositorControllerChild> result;

  nsresult rv = EnsureGPUReady();
  if (NS_WARN_IF(rv == NS_ERROR_ILLEGAL_DURING_SHUTDOWN)) {
    return nullptr;
  }

  if (NS_FAILED(rv)) {
    result = UiCompositorControllerChild::CreateForSameProcess(aId, aWidget);
  } else {
    ipc::Endpoint<PUiCompositorControllerParent> parentPipe;
    ipc::Endpoint<PUiCompositorControllerChild> childPipe;
    rv = PUiCompositorController::CreateEndpoints(mGPUChild->OtherPid(),
                                                  base::GetCurrentProcId(),
                                                  &parentPipe, &childPipe);
    if (NS_FAILED(rv)) {
      DisableGPUProcess("Failed to create PUiCompositorController endpoints");
      return nullptr;
    }

    mGPUChild->SendInitUiCompositorController(aId, std::move(parentPipe));
    result = UiCompositorControllerChild::CreateForGPUProcess(
        mProcessToken, std::move(childPipe), aWidget);

    if (result) {
      result->SetCompositorSurfaceManager(
          mProcess->GetCompositorSurfaceManager());
    }
  }
  return result.forget();
}
#endif  // defined(MOZ_WIDGET_ANDROID)

void GPUProcessManager::OnProcessLaunchComplete(GPUProcessHost* aHost) {
  MOZ_ASSERT(mProcess && mProcess == aHost);

  if (!mProcess->IsConnected()) {
    DisableGPUProcess("Failed to connect GPU process");
    return;
  }

  mGPUChild = mProcess->GetActor();
  mProcessToken = mProcess->GetProcessToken();
#if defined(XP_WIN)
  if (mAppInForeground) {
    SetProcessIsForeground();
  }
#endif

  ipc::Endpoint<PVsyncBridgeParent> vsyncParent;
  ipc::Endpoint<PVsyncBridgeChild> vsyncChild;
  nsresult rv = PVsyncBridge::CreateEndpoints(mGPUChild->OtherPid(),
                                              base::GetCurrentProcId(),
                                              &vsyncParent, &vsyncChild);
  if (NS_FAILED(rv)) {
    DisableGPUProcess("Failed to create PVsyncBridge endpoints");
    return;
  }

  mVsyncBridge = VsyncBridgeChild::Create(mVsyncIOThread, mProcessToken,
                                          std::move(vsyncChild));
  mGPUChild->SendInitVsyncBridge(std::move(vsyncParent));

  // Flush any pref updates that happened during launch and weren't
  // included in the blobs set up in LaunchGPUProcess.
  for (const mozilla::dom::Pref& pref : mQueuedPrefs) {
    Unused << NS_WARN_IF(!mGPUChild->SendPreferenceUpdate(pref));
  }
  mQueuedPrefs.Clear();

  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::GPUProcessStatus, "Running"_ns);

  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::GPUProcessLaunchCount,
      static_cast<int>(mTotalProcessAttempts));

  ReinitializeRendering();
}

void GPUProcessManager::OnProcessDeclaredStable() { mProcessStable = true; }

static bool ShouldLimitDeviceResets(uint32_t count, int32_t deltaMilliseconds) {
  // We decide to limit by comparing the amount of resets that have happened
  // and time since the last reset to two prefs.
  int32_t timeLimit = StaticPrefs::gfx_device_reset_threshold_ms_AtStartup();
  int32_t countLimit = StaticPrefs::gfx_device_reset_limit_AtStartup();

  bool hasTimeLimit = timeLimit >= 0;
  bool hasCountLimit = countLimit >= 0;

  bool triggeredTime = deltaMilliseconds < timeLimit;
  bool triggeredCount = count > (uint32_t)countLimit;

  // If we have both prefs set then it needs to trigger both limits,
  // otherwise we only test the pref that is set or none
  if (hasTimeLimit && hasCountLimit) {
    return triggeredTime && triggeredCount;
  } else if (hasTimeLimit) {
    return triggeredTime;
  } else if (hasCountLimit) {
    return triggeredCount;
  }

  return false;
}

void GPUProcessManager::ResetCompositors() {
  // Note: this will recreate devices in addition to recreating compositors.
  // This isn't optimal, but this is only used on linux where acceleration
  // isn't enabled by default, and this way we don't need a new code path.
  SimulateDeviceReset();
}

void GPUProcessManager::SimulateDeviceReset() {
  // Make sure we rebuild environment and configuration for accelerated
  // features.
  gfxPlatform::GetPlatform()->CompositorUpdated();

  if (mProcess) {
    if (mGPUChild) {
      mGPUChild->SendSimulateDeviceReset();
    }
  } else {
    wr::RenderThread::Get()->SimulateDeviceReset();
  }
}

bool GPUProcessManager::FallbackFromAcceleration(wr::WebRenderError aError,
                                                 const nsCString& aMsg) {
  if (aError == wr::WebRenderError::INITIALIZE) {
    return gfxPlatform::FallbackFromAcceleration(
        gfx::FeatureStatus::Unavailable, "WebRender initialization failed",
        aMsg);
  } else if (aError == wr::WebRenderError::MAKE_CURRENT) {
    return gfxPlatform::FallbackFromAcceleration(
        gfx::FeatureStatus::Unavailable,
        "Failed to make render context current",
        "FEATURE_FAILURE_WEBRENDER_MAKE_CURRENT"_ns);
  } else if (aError == wr::WebRenderError::RENDER) {
    return gfxPlatform::FallbackFromAcceleration(
        gfx::FeatureStatus::Unavailable, "Failed to render WebRender",
        "FEATURE_FAILURE_WEBRENDER_RENDER"_ns);
  } else if (aError == wr::WebRenderError::NEW_SURFACE) {
    // If we cannot create a new Surface even in the final fallback
    // configuration then force a crash.
    return gfxPlatform::FallbackFromAcceleration(
        gfx::FeatureStatus::Unavailable, "Failed to create new surface",
        "FEATURE_FAILURE_WEBRENDER_NEW_SURFACE"_ns,
        /* aCrashAfterFinalFallback */ true);
  } else if (aError == wr::WebRenderError::BEGIN_DRAW) {
    return gfxPlatform::FallbackFromAcceleration(
        gfx::FeatureStatus::Unavailable, "BeginDraw() failed",
        "FEATURE_FAILURE_WEBRENDER_BEGIN_DRAW"_ns);
  } else if (aError == wr::WebRenderError::EXCESSIVE_RESETS) {
    return gfxPlatform::FallbackFromAcceleration(
        gfx::FeatureStatus::Unavailable, "Device resets exceeded threshold",
        "FEATURE_FAILURE_WEBRENDER_EXCESSIVE_RESETS"_ns);
  } else {
    MOZ_ASSERT_UNREACHABLE("Invalid value");
    return gfxPlatform::FallbackFromAcceleration(
        gfx::FeatureStatus::Unavailable, "Unhandled failure reason",
        "FEATURE_FAILURE_WEBRENDER_UNHANDLED"_ns);
  }
}

bool GPUProcessManager::DisableWebRenderConfig(wr::WebRenderError aError,
                                               const nsCString& aMsg) {
  // If we have a stable compositor process, this may just be due to an OOM or
  // bad driver state. In that case, we should consider restarting the GPU
  // process, or simulating a device reset to teardown the compositors to
  // hopefully alleviate the situation.
  if (IsProcessStable(TimeStamp::Now())) {
    if (mProcess) {
      mProcess->KillProcess();
    } else {
      SimulateDeviceReset();
    }

    mLastError = Some(aError);
    mLastErrorMsg = Some(aMsg);
    return false;
  }

  mLastError.reset();
  mLastErrorMsg.reset();

  // Disable WebRender
  bool wantRestart = FallbackFromAcceleration(aError, aMsg);
  gfxVars::SetUseWebRenderDCompVideoHwOverlayWin(false);
  gfxVars::SetUseWebRenderDCompVideoSwOverlayWin(false);

  // If we still have the GPU process, and we fallback to a new configuration
  // that prefers to have the GPU process, reset the counter. Because we
  // updated the gfxVars, we want to flag the GPUChild to wait for the update
  // to be processed before creating new compositor sessions, otherwise we risk
  // them being out of sync with the content/parent processes.
  if (wantRestart && mProcess && mGPUChild) {
    mUnstableProcessAttempts = 1;
    mGPUChild->MarkWaitForVarUpdate();
  }

  return true;
}

void GPUProcessManager::DisableWebRender(wr::WebRenderError aError,
                                         const nsCString& aMsg) {
  if (DisableWebRenderConfig(aError, aMsg)) {
    if (mProcess) {
      DestroyRemoteCompositorSessions();
    } else {
      DestroyInProcessCompositorSessions();
    }
    NotifyListenersOnCompositeDeviceReset();
  }
}

void GPUProcessManager::NotifyWebRenderError(wr::WebRenderError aError) {
  gfxCriticalNote << "Handling webrender error " << (unsigned int)aError;
#ifdef XP_WIN
  if (aError == wr::WebRenderError::VIDEO_OVERLAY) {
    gfxVars::SetUseWebRenderDCompVideoHwOverlayWin(false);
    gfxVars::SetUseWebRenderDCompVideoSwOverlayWin(false);
    return;
  }
  if (aError == wr::WebRenderError::VIDEO_HW_OVERLAY) {
    gfxVars::SetUseWebRenderDCompVideoHwOverlayWin(false);
    return;
  }
  if (aError == wr::WebRenderError::VIDEO_SW_OVERLAY) {
    gfxVars::SetUseWebRenderDCompVideoSwOverlayWin(false);
    return;
  }
#else
  if (aError == wr::WebRenderError::VIDEO_OVERLAY ||
      aError == wr::WebRenderError::VIDEO_HW_OVERLAY ||
      aError == wr::WebRenderError::VIDEO_SW_OVERLAY) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }
#endif

  DisableWebRender(aError, nsCString());
}

/* static */ void GPUProcessManager::RecordDeviceReset(
    DeviceResetReason aReason) {
  if (aReason != DeviceResetReason::FORCED_RESET) {
    Telemetry::Accumulate(Telemetry::DEVICE_RESET_REASON, uint32_t(aReason));
  }

  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::DeviceResetReason, int(aReason));
}

bool GPUProcessManager::OnDeviceReset(bool aTrackThreshold) {
  // Ignore resets for thresholding if requested.
  if (!aTrackThreshold) {
    return false;
  }

  // Detect whether the device is resetting too quickly or too much
  // indicating that we should give up and use software
  mDeviceResetCount++;

  auto newTime = TimeStamp::Now();
  auto delta = (int32_t)(newTime - mDeviceResetLastTime).ToMilliseconds();
  mDeviceResetLastTime = newTime;

  // Returns true if we should disable acceleration due to the reset.
  return ShouldLimitDeviceResets(mDeviceResetCount, delta);
}

void GPUProcessManager::OnInProcessDeviceReset(bool aTrackThreshold) {
  if (OnDeviceReset(aTrackThreshold)) {
    gfxCriticalNoteOnce << "In-process device reset threshold exceeded";
#ifdef MOZ_WIDGET_GTK
    // FIXME(aosmond): Should we disable WebRender on other platforms?
    DisableWebRenderConfig(wr::WebRenderError::EXCESSIVE_RESETS, nsCString());
#endif
  }
#ifdef XP_WIN
  // Ensure device reset handling before re-creating in process sessions.
  // Normally nsWindow::OnPaint() already handled it.
  gfxWindowsPlatform::GetPlatform()->HandleDeviceReset();
#endif
  DestroyInProcessCompositorSessions();
  NotifyListenersOnCompositeDeviceReset();
}

void GPUProcessManager::OnRemoteProcessDeviceReset(GPUProcessHost* aHost) {
  if (OnDeviceReset(/* aTrackThreshold */ true)) {
    DestroyProcess();
    DisableGPUProcess("GPU processed experienced too many device resets");
    HandleProcessLost();
    return;
  }

  DestroyRemoteCompositorSessions();
  NotifyListenersOnCompositeDeviceReset();
}

void GPUProcessManager::NotifyListenersOnCompositeDeviceReset() {
  for (const auto& listener : mListeners) {
    listener->OnCompositorDeviceReset();
  }
}

void GPUProcessManager::OnProcessUnexpectedShutdown(GPUProcessHost* aHost) {
  MOZ_ASSERT(mProcess && mProcess == aHost);

  if (StaticPrefs::layers_gpu_process_crash_also_crashes_browser()) {
    MOZ_CRASH("GPU process crashed and pref is set to crash the browser.");
  }

  CompositorManagerChild::OnGPUProcessLost(aHost->GetProcessToken());
  DestroyProcess(/* aUnexpectedShutdown */ true);

  if (mUnstableProcessAttempts >
      uint32_t(StaticPrefs::layers_gpu_process_max_restarts())) {
    char disableMessage[64];
    SprintfLiteral(disableMessage, "GPU process disabled after %d attempts",
                   mTotalProcessAttempts);
    if (!MaybeDisableGPUProcess(disableMessage, /* aAllowRestart */ true)) {
      // Fallback wants the GPU process. Reset our counter.
      mUnstableProcessAttempts = 0;
      HandleProcessLost();
    }
  } else if (mUnstableProcessAttempts >
                 uint32_t(StaticPrefs::
                              layers_gpu_process_max_restarts_with_decoder()) &&
             mDecodeVideoOnGpuProcess) {
    mDecodeVideoOnGpuProcess = false;
    Telemetry::Accumulate(Telemetry::GPU_PROCESS_CRASH_FALLBACKS,
                          uint32_t(FallbackType::DECODINGDISABLED));
    HandleProcessLost();
  } else {
    Telemetry::Accumulate(Telemetry::GPU_PROCESS_CRASH_FALLBACKS,
                          uint32_t(FallbackType::NONE));
    HandleProcessLost();
  }
}

void GPUProcessManager::HandleProcessLost() {
  MOZ_ASSERT(NS_IsMainThread());

  // The shutdown and restart sequence for the GPU process is as follows:
  //
  //  (1) The GPU process dies. IPDL will enqueue an ActorDestroy message on
  //      each channel owning a bridge to the GPU process, on the thread owning
  //      that channel.
  //
  //  (2) The first channel to process its ActorDestroy message will post a
  //      message to the main thread to call NotifyRemoteActorDestroyed on the
  //      GPUProcessManager, which calls OnProcessUnexpectedShutdown if it has
  //      not handled shutdown for this process yet. OnProcessUnexpectedShutdown
  //      is responsible for tearing down the old process and deciding whether
  //      or not to disable the GPU process. It then calls this function,
  //      HandleProcessLost.
  //
  //  (3) We then notify each widget that its session with the compositor is now
  //      invalid. The widget is responsible for destroying its layer manager
  //      and CompositorBridgeChild. Note that at this stage, not all actors may
  //      have received ActorDestroy yet. CompositorBridgeChild may attempt to
  //      send messages, and if this happens, it will probably report a
  //      MsgDropped error. This is okay.
  //
  //  (4) At this point, the UI process has a clean slate: no layers should
  //      exist for the old compositor. We may make a decision on whether or not
  //      to re-launch the GPU process. Or, on Android if the app is in the
  //      background we may decide to wait until it comes to the foreground
  //      before re-launching.
  //
  //  (5) When we do decide to re-launch, or continue without a GPU process, we
  //      notify each ContentParent of the lost connection. It will request new
  //      endpoints from the GPUProcessManager and forward them to its
  //      ContentChild. The parent-side of these endpoints may come from the
  //      compositor thread of the UI process, or the compositor thread of the
  //      GPU process. However, no actual compositors should exist yet.
  //
  //  (6) Each ContentChild will receive new endpoints. It will destroy its
  //      Compositor/ImageBridgeChild singletons and recreate them, as well
  //      as invalidate all retained layers.
  //
  //  (7) In addition, each ContentChild will ask each of its BrowserChildren
  //      to re-request association with the compositor for the window
  //      owning the tab. The sequence of calls looks like:
  //        (a) [CONTENT] ContentChild::RecvReinitRendering
  //        (b) [CONTENT] BrowserChild::ReinitRendering
  //        (c) [CONTENT] BrowserChild::SendEnsureLayersConnected
  //        (d)      [UI] BrowserParent::RecvEnsureLayersConnected
  //        (e)      [UI] RemoteLayerTreeOwner::EnsureLayersConnected
  //        (f)      [UI] CompositorBridgeChild::SendNotifyChildRecreated
  //
  //      Note that at step (e), RemoteLayerTreeOwner will call
  //      GetWindowRenderer on the nsIWidget owning the tab. This step ensures
  //      that a compositor exists for the window. If we decided to launch a new
  //      GPU Process, at this point we block until the process has launched and
  //      we're able to create a new window compositor. Otherwise, if
  //      compositing is now in-process, this will simply create a new
  //      CompositorBridgeParent in the UI process. If there are multiple tabs
  //      in the same window, additional tabs will simply return the already-
  //      established compositor.
  //
  //      Finally, this step serves one other crucial function: tabs must be
  //      associated with a window compositor or else they can't forward
  //      layer transactions. So this step both ensures that a compositor
  //      exists, and that the tab can forward layers.
  //
  //  (8) Last, if the window had no remote tabs, step (7) will not have
  //      applied, and the window will not have a new compositor just yet. The
  //      next refresh tick and paint will ensure that one exists, again via
  //      nsIWidget::GetWindowRenderer. On Android, we called
  //      nsIWidgetListener::RequestRepaint back in step (3) to ensure this
  //      tick occurs, but on other platforms this is not necessary.

  DestroyRemoteCompositorSessions();

#ifdef MOZ_WIDGET_ANDROID
  java::SurfaceControlManager::GetInstance()->OnGpuProcessLoss();
#endif

  // Re-launch the process if immediately if the GPU process is still enabled.
  // Except on Android if the app is in the background, where we want to wait
  // until the app is in the foreground again.
  if (gfxConfig::IsEnabled(Feature::GPU_PROCESS)) {
#ifdef MOZ_WIDGET_ANDROID
    if (mAppInForeground) {
#else
    {
#endif
      Unused << LaunchGPUProcess();
    }
  } else {
    // If the GPU process is disabled we can reinitialize rendering immediately.
    // This will be handled in OnProcessLaunchComplete() if the GPU process is
    // enabled.
    ReinitializeRendering();
  }
}

void GPUProcessManager::ReinitializeRendering() {
  // Notify content. This will ensure that each content process re-establishes
  // a connection to the compositor thread (whether it's in-process or in a
  // newly launched GPU process).
  for (const auto& listener : mListeners) {
    listener->OnCompositorUnexpectedShutdown();
  }

  // Notify any observers that the compositor has been reinitialized,
  // eg the ZoomConstraintsClients for parent process documents.
  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (observerService) {
    observerService->NotifyObservers(nullptr, "compositor-reinitialized",
                                     nullptr);
  }
}

void GPUProcessManager::DestroyRemoteCompositorSessions() {
  // Build a list of sessions to notify, since notification might delete
  // entries from the list.
  nsTArray<RefPtr<RemoteCompositorSession>> sessions;
  for (auto& session : mRemoteSessions) {
    sessions.AppendElement(session);
  }

  // Notify each widget that we have lost the GPU process. This will ensure
  // that each widget destroys its layer manager and CompositorBridgeChild.
  for (const auto& session : sessions) {
    session->NotifySessionLost();
  }
}

void GPUProcessManager::DestroyInProcessCompositorSessions() {
  // Build a list of sessions to notify, since notification might delete
  // entries from the list.
  nsTArray<RefPtr<InProcessCompositorSession>> sessions;
  for (auto& session : mInProcessSessions) {
    sessions.AppendElement(session);
  }

  // Notify each widget that we have lost the GPU process. This will ensure
  // that each widget destroys its layer manager and CompositorBridgeChild.
  for (const auto& session : sessions) {
    session->NotifySessionLost();
  }

  // Ensure our stablility state is reset so that we don't necessarily crash
  // right away on some WebRender errors.
  CompositorBridgeParent::ResetStable();
  ResetProcessStable();
}

void GPUProcessManager::NotifyRemoteActorDestroyed(
    const uint64_t& aProcessToken) {
  if (!NS_IsMainThread()) {
    RefPtr<Runnable> task = mTaskFactory.NewRunnableMethod(
        &GPUProcessManager::NotifyRemoteActorDestroyed, aProcessToken);
    NS_DispatchToMainThread(task.forget());
    return;
  }

  if (mProcessToken != aProcessToken) {
    // This token is for an older process; we can safely ignore it.
    return;
  }

  // One of the bridged top-level actors for the GPU process has been
  // prematurely terminated, and we're receiving a notification. This
  // can happen if the ActorDestroy for a bridged protocol fires
  // before the ActorDestroy for PGPUChild.
  OnProcessUnexpectedShutdown(mProcess);
}

void GPUProcessManager::CleanShutdown() {
  DestroyProcess();
  mVsyncIOThread = nullptr;
}

void GPUProcessManager::KillProcess() {
  if (!mProcess) {
    return;
  }

  mProcess->KillProcess();
}

void GPUProcessManager::CrashProcess() {
  if (!mProcess) {
    return;
  }

  mProcess->CrashProcess();
}

void GPUProcessManager::DestroyProcess(bool aUnexpectedShutdown) {
  if (!mProcess) {
    return;
  }

  mProcess->Shutdown(aUnexpectedShutdown);
  mProcessToken = 0;
  mProcess = nullptr;
  mGPUChild = nullptr;
  mQueuedPrefs.Clear();
  if (mVsyncBridge) {
    mVsyncBridge->Close();
    mVsyncBridge = nullptr;
  }

  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::GPUProcessStatus, "Destroyed"_ns);
}

already_AddRefed<CompositorSession> GPUProcessManager::CreateTopLevelCompositor(
    nsBaseWidget* aWidget, WebRenderLayerManager* aLayerManager,
    CSSToLayoutDeviceScale aScale, const CompositorOptions& aOptions,
    bool aUseExternalSurfaceSize, const gfx::IntSize& aSurfaceSize,
    uint64_t aInnerWindowId, bool* aRetryOut) {
  MOZ_ASSERT(aRetryOut);

  LayersId layerTreeId = AllocateLayerTreeId();

  if (!EnsureProtocolsReady()) {
    *aRetryOut = false;
    return nullptr;
  }

  RefPtr<CompositorSession> session;

  nsresult rv = EnsureGPUReady();
  if (NS_WARN_IF(rv == NS_ERROR_ILLEGAL_DURING_SHUTDOWN)) {
    *aRetryOut = false;
    return nullptr;
  }

  if (NS_SUCCEEDED(rv)) {
    session = CreateRemoteSession(aWidget, aLayerManager, layerTreeId, aScale,
                                  aOptions, aUseExternalSurfaceSize,
                                  aSurfaceSize, aInnerWindowId);
    if (!session) {
      // We couldn't create a remote compositor, so abort the process.
      DisableGPUProcess("Failed to create remote compositor");
      *aRetryOut = true;
      return nullptr;
    }
  } else {
    session = InProcessCompositorSession::Create(
        aWidget, aLayerManager, layerTreeId, aScale, aOptions,
        aUseExternalSurfaceSize, aSurfaceSize, AllocateNamespace(),
        aInnerWindowId);
  }

#if defined(MOZ_WIDGET_ANDROID)
  if (session) {
    // Nothing to do if controller gets a nullptr
    RefPtr<UiCompositorControllerChild> controller =
        CreateUiCompositorController(aWidget, session->RootLayerTreeId());
    MOZ_ASSERT(controller);
    session->SetUiCompositorControllerChild(controller);
  }
#endif  // defined(MOZ_WIDGET_ANDROID)

  *aRetryOut = false;
  return session.forget();
}

RefPtr<CompositorSession> GPUProcessManager::CreateRemoteSession(
    nsBaseWidget* aWidget, WebRenderLayerManager* aLayerManager,
    const LayersId& aRootLayerTreeId, CSSToLayoutDeviceScale aScale,
    const CompositorOptions& aOptions, bool aUseExternalSurfaceSize,
    const gfx::IntSize& aSurfaceSize, uint64_t aInnerWindowId) {
#ifdef MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING
  widget::CompositorWidgetInitData initData;
  aWidget->GetCompositorWidgetInitData(&initData);

  RefPtr<CompositorBridgeChild> child =
      CompositorManagerChild::CreateWidgetCompositorBridge(
          mProcessToken, aLayerManager, AllocateNamespace(), aScale, aOptions,
          aUseExternalSurfaceSize, aSurfaceSize, aInnerWindowId);
  if (!child) {
    gfxCriticalNote << "Failed to create CompositorBridgeChild";
    return nullptr;
  }

  RefPtr<CompositorVsyncDispatcher> dispatcher =
      aWidget->GetCompositorVsyncDispatcher();
  RefPtr<widget::CompositorWidgetVsyncObserver> observer =
      new widget::CompositorWidgetVsyncObserver(mVsyncBridge, aRootLayerTreeId);

  widget::CompositorWidgetChild* widget =
      new widget::CompositorWidgetChild(dispatcher, observer, initData);
  if (!child->SendPCompositorWidgetConstructor(widget, initData)) {
    return nullptr;
  }
  if (!widget->Initialize()) {
    return nullptr;
  }
  if (!child->SendInitialize(aRootLayerTreeId)) {
    return nullptr;
  }

  RefPtr<APZCTreeManagerChild> apz = nullptr;
  if (aOptions.UseAPZ()) {
    PAPZCTreeManagerChild* papz =
        child->SendPAPZCTreeManagerConstructor(LayersId{0});
    if (!papz) {
      return nullptr;
    }
    apz = static_cast<APZCTreeManagerChild*>(papz);

    ipc::Endpoint<PAPZInputBridgeParent> parentPipe;
    ipc::Endpoint<PAPZInputBridgeChild> childPipe;
    nsresult rv = PAPZInputBridge::CreateEndpoints(mGPUChild->OtherPid(),
                                                   base::GetCurrentProcId(),
                                                   &parentPipe, &childPipe);
    if (NS_FAILED(rv)) {
      return nullptr;
    }
    mGPUChild->SendInitAPZInputBridge(aRootLayerTreeId, std::move(parentPipe));

    RefPtr<APZInputBridgeChild> inputBridge =
        APZInputBridgeChild::Create(mProcessToken, std::move(childPipe));
    if (!inputBridge) {
      return nullptr;
    }

    apz->SetInputBridge(inputBridge);
  }

  return new RemoteCompositorSession(aWidget, child, widget, apz,
                                     aRootLayerTreeId);
#else
  gfxCriticalNote << "Platform does not support out-of-process compositing";
  return nullptr;
#endif
}

bool GPUProcessManager::CreateContentBridges(
    base::ProcessId aOtherProcess,
    ipc::Endpoint<PCompositorManagerChild>* aOutCompositor,
    ipc::Endpoint<PImageBridgeChild>* aOutImageBridge,
    ipc::Endpoint<PVRManagerChild>* aOutVRBridge,
    ipc::Endpoint<PRemoteDecoderManagerChild>* aOutVideoManager,
    dom::ContentParentId aChildId, nsTArray<uint32_t>* aNamespaces) {
  if (!CreateContentCompositorManager(aOtherProcess, aChildId,
                                      aOutCompositor) ||
      !CreateContentImageBridge(aOtherProcess, aChildId, aOutImageBridge) ||
      !CreateContentVRManager(aOtherProcess, aChildId, aOutVRBridge)) {
    return false;
  }
  // VideoDeocderManager is only supported in the GPU process, so we allow this
  // to be fallible.
  CreateContentRemoteDecoderManager(aOtherProcess, aChildId, aOutVideoManager);
  // Allocates 3 namespaces(for CompositorManagerChild, CompositorBridgeChild
  // and ImageBridgeChild)
  aNamespaces->AppendElement(AllocateNamespace());
  aNamespaces->AppendElement(AllocateNamespace());
  aNamespaces->AppendElement(AllocateNamespace());
  return true;
}

bool GPUProcessManager::CreateContentCompositorManager(
    base::ProcessId aOtherProcess, dom::ContentParentId aChildId,
    ipc::Endpoint<PCompositorManagerChild>* aOutEndpoint) {
  ipc::Endpoint<PCompositorManagerParent> parentPipe;
  ipc::Endpoint<PCompositorManagerChild> childPipe;

  nsresult rv = EnsureGPUReady();
  if (NS_WARN_IF(rv == NS_ERROR_ILLEGAL_DURING_SHUTDOWN)) {
    return false;
  }

  base::ProcessId parentPid =
      NS_SUCCEEDED(rv) ? mGPUChild->OtherPid() : base::GetCurrentProcId();

  rv = PCompositorManager::CreateEndpoints(parentPid, aOtherProcess,
                                           &parentPipe, &childPipe);
  if (NS_FAILED(rv)) {
    gfxCriticalNote << "Could not create content compositor manager: "
                    << hexa(int(rv));
    return false;
  }

  if (mGPUChild) {
    mGPUChild->SendNewContentCompositorManager(std::move(parentPipe), aChildId);
  } else if (!CompositorManagerParent::Create(std::move(parentPipe), aChildId,
                                              /* aIsRoot */ false)) {
    return false;
  }

  *aOutEndpoint = std::move(childPipe);
  return true;
}

bool GPUProcessManager::CreateContentImageBridge(
    base::ProcessId aOtherProcess, dom::ContentParentId aChildId,
    ipc::Endpoint<PImageBridgeChild>* aOutEndpoint) {
  if (!EnsureImageBridgeChild()) {
    return false;
  }

  nsresult rv = EnsureGPUReady();
  if (NS_WARN_IF(rv == NS_ERROR_ILLEGAL_DURING_SHUTDOWN)) {
    return false;
  }

  base::ProcessId parentPid =
      NS_SUCCEEDED(rv) ? mGPUChild->OtherPid() : base::GetCurrentProcId();

  ipc::Endpoint<PImageBridgeParent> parentPipe;
  ipc::Endpoint<PImageBridgeChild> childPipe;
  rv = PImageBridge::CreateEndpoints(parentPid, aOtherProcess, &parentPipe,
                                     &childPipe);
  if (NS_FAILED(rv)) {
    gfxCriticalNote << "Could not create content compositor bridge: "
                    << hexa(int(rv));
    return false;
  }

  if (mGPUChild) {
    mGPUChild->SendNewContentImageBridge(std::move(parentPipe), aChildId);
  } else {
    if (!ImageBridgeParent::CreateForContent(std::move(parentPipe), aChildId)) {
      return false;
    }
  }

  *aOutEndpoint = std::move(childPipe);
  return true;
}

base::ProcessId GPUProcessManager::GPUProcessPid() {
  base::ProcessId gpuPid =
      mGPUChild ? mGPUChild->OtherPid() : base::kInvalidProcessId;
  return gpuPid;
}

bool GPUProcessManager::CreateContentVRManager(
    base::ProcessId aOtherProcess, dom::ContentParentId aChildId,
    ipc::Endpoint<PVRManagerChild>* aOutEndpoint) {
  if (NS_WARN_IF(!EnsureVRManager())) {
    return false;
  }

  nsresult rv = EnsureGPUReady();
  if (NS_WARN_IF(rv == NS_ERROR_ILLEGAL_DURING_SHUTDOWN)) {
    return false;
  }

  base::ProcessId parentPid =
      NS_SUCCEEDED(rv) ? mGPUChild->OtherPid() : base::GetCurrentProcId();

  ipc::Endpoint<PVRManagerParent> parentPipe;
  ipc::Endpoint<PVRManagerChild> childPipe;
  rv = PVRManager::CreateEndpoints(parentPid, aOtherProcess, &parentPipe,
                                   &childPipe);
  if (NS_FAILED(rv)) {
    gfxCriticalNote << "Could not create content compositor bridge: "
                    << hexa(int(rv));
    return false;
  }

  if (mGPUChild) {
    mGPUChild->SendNewContentVRManager(std::move(parentPipe), aChildId);
  } else {
    if (!VRManagerParent::CreateForContent(std::move(parentPipe), aChildId)) {
      return false;
    }
  }

  *aOutEndpoint = std::move(childPipe);
  return true;
}

void GPUProcessManager::CreateContentRemoteDecoderManager(
    base::ProcessId aOtherProcess, dom::ContentParentId aChildId,
    ipc::Endpoint<PRemoteDecoderManagerChild>* aOutEndpoint) {
  nsresult rv = EnsureGPUReady();
  if (NS_WARN_IF(rv == NS_ERROR_ILLEGAL_DURING_SHUTDOWN)) {
    return;
  }

  if (NS_FAILED(rv) || !StaticPrefs::media_gpu_process_decoder() ||
      !mDecodeVideoOnGpuProcess) {
    return;
  }

  ipc::Endpoint<PRemoteDecoderManagerParent> parentPipe;
  ipc::Endpoint<PRemoteDecoderManagerChild> childPipe;

  rv = PRemoteDecoderManager::CreateEndpoints(
      mGPUChild->OtherPid(), aOtherProcess, &parentPipe, &childPipe);
  if (NS_FAILED(rv)) {
    gfxCriticalNote << "Could not create content video decoder: "
                    << hexa(int(rv));
    return;
  }

  mGPUChild->SendNewContentRemoteDecoderManager(std::move(parentPipe),
                                                aChildId);

  *aOutEndpoint = std::move(childPipe);
}

void GPUProcessManager::InitVideoBridge(
    ipc::Endpoint<PVideoBridgeParent>&& aVideoBridge,
    layers::VideoBridgeSource aSource) {
  nsresult rv = EnsureGPUReady();
  if (NS_WARN_IF(rv == NS_ERROR_ILLEGAL_DURING_SHUTDOWN)) {
    return;
  }

  if (NS_SUCCEEDED(rv)) {
    mGPUChild->SendInitVideoBridge(std::move(aVideoBridge), aSource);
  }
}

void GPUProcessManager::MapLayerTreeId(LayersId aLayersId,
                                       base::ProcessId aOwningId) {
  nsresult rv = EnsureGPUReady();
  if (NS_WARN_IF(rv == NS_ERROR_ILLEGAL_DURING_SHUTDOWN)) {
    return;
  }

  if (NS_SUCCEEDED(rv)) {
    mGPUChild->SendAddLayerTreeIdMapping(
        LayerTreeIdMapping(aLayersId, aOwningId));
  }

  // Must do this *after* the call to EnsureGPUReady, so that if the
  // process is launched as a result then it is initialized without this
  // LayersId, meaning it can be successfully mapped.
  LayerTreeOwnerTracker::Get()->Map(aLayersId, aOwningId);
}

void GPUProcessManager::UnmapLayerTreeId(LayersId aLayersId,
                                         base::ProcessId aOwningId) {
  nsresult rv = EnsureGPUReady();
  if (NS_WARN_IF(rv == NS_ERROR_ILLEGAL_DURING_SHUTDOWN)) {
    return;
  }

  if (NS_SUCCEEDED(rv)) {
    mGPUChild->SendRemoveLayerTreeIdMapping(
        LayerTreeIdMapping(aLayersId, aOwningId));
  } else {
    CompositorBridgeParent::DeallocateLayerTreeId(aLayersId);
  }

  // Must do this *after* the call to EnsureGPUReady, so that if the
  // process is launched as a result then it is initialized with this
  // LayersId, meaning it can be successfully unmapped.
  LayerTreeOwnerTracker::Get()->Unmap(aLayersId, aOwningId);
}

bool GPUProcessManager::IsLayerTreeIdMapped(LayersId aLayersId,
                                            base::ProcessId aRequestingId) {
  return LayerTreeOwnerTracker::Get()->IsMapped(aLayersId, aRequestingId);
}

LayersId GPUProcessManager::AllocateLayerTreeId() {
  // Allocate tree id by using id namespace.
  // By it, tree id does not conflict with external image id and
  // async image pipeline id.
  MOZ_ASSERT(NS_IsMainThread());
  ++mResourceId;
  if (mResourceId == UINT32_MAX) {
    // Move to next id namespace.
    mIdNamespace = AllocateNamespace();
    mResourceId = 1;
  }

  uint64_t layerTreeId = mIdNamespace;
  layerTreeId = (layerTreeId << 32) | mResourceId;
  return LayersId{layerTreeId};
}

uint32_t GPUProcessManager::AllocateNamespace() {
  MOZ_ASSERT(NS_IsMainThread());
  return ++mNextNamespace;
}

bool GPUProcessManager::AllocateAndConnectLayerTreeId(
    PCompositorBridgeChild* aCompositorBridge, base::ProcessId aOtherPid,
    LayersId* aOutLayersId, CompositorOptions* aOutCompositorOptions) {
  LayersId layersId = AllocateLayerTreeId();
  *aOutLayersId = layersId;

  if (!mGPUChild || !aCompositorBridge) {
    // If we're not remoting to another process, or there is no compositor,
    // then we'll send at most one message. In this case we can just keep
    // the old behavior of making sure the mapping occurs, and maybe sending
    // a creation notification.
    MapLayerTreeId(layersId, aOtherPid);
    if (!aCompositorBridge) {
      return false;
    }
    return aCompositorBridge->SendNotifyChildCreated(layersId,
                                                     aOutCompositorOptions);
  }

  // Use the combined message path.
  LayerTreeOwnerTracker::Get()->Map(layersId, aOtherPid);
  return aCompositorBridge->SendMapAndNotifyChildCreated(layersId, aOtherPid,
                                                         aOutCompositorOptions);
}

void GPUProcessManager::EnsureVsyncIOThread() {
  if (mVsyncIOThread) {
    return;
  }

  mVsyncIOThread = new VsyncIOThreadHolder();
  MOZ_RELEASE_ASSERT(mVsyncIOThread->Start());
}

void GPUProcessManager::ShutdownVsyncIOThread() { mVsyncIOThread = nullptr; }

void GPUProcessManager::RegisterRemoteProcessSession(
    RemoteCompositorSession* aSession) {
  mRemoteSessions.AppendElement(aSession);
}

void GPUProcessManager::UnregisterRemoteProcessSession(
    RemoteCompositorSession* aSession) {
  mRemoteSessions.RemoveElement(aSession);
}

void GPUProcessManager::RegisterInProcessSession(
    InProcessCompositorSession* aSession) {
  mInProcessSessions.AppendElement(aSession);
}

void GPUProcessManager::UnregisterInProcessSession(
    InProcessCompositorSession* aSession) {
  mInProcessSessions.RemoveElement(aSession);
}

void GPUProcessManager::AddListener(GPUProcessListener* aListener) {
  mListeners.AppendElement(aListener);
}

void GPUProcessManager::RemoveListener(GPUProcessListener* aListener) {
  mListeners.RemoveElement(aListener);
}

bool GPUProcessManager::NotifyGpuObservers(const char* aTopic) {
  if (NS_FAILED(EnsureGPUReady())) {
    return false;
  }
  nsCString topic(aTopic);
  mGPUChild->SendNotifyGpuObservers(topic);
  return true;
}

class GPUMemoryReporter : public MemoryReportingProcess {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GPUMemoryReporter, override)

  bool IsAlive() const override {
    if (GPUProcessManager* gpm = GPUProcessManager::Get()) {
      return !!gpm->GetGPUChild();
    }
    return false;
  }

  bool SendRequestMemoryReport(
      const uint32_t& aGeneration, const bool& aAnonymize,
      const bool& aMinimizeMemoryUsage,
      const Maybe<ipc::FileDescriptor>& aDMDFile) override {
    GPUChild* child = GetChild();
    if (!child) {
      return false;
    }

    return child->SendRequestMemoryReport(aGeneration, aAnonymize,
                                          aMinimizeMemoryUsage, aDMDFile);
  }

  int32_t Pid() const override {
    if (GPUChild* child = GetChild()) {
      return (int32_t)child->OtherPid();
    }
    return 0;
  }

 private:
  GPUChild* GetChild() const {
    if (GPUProcessManager* gpm = GPUProcessManager::Get()) {
      if (GPUChild* child = gpm->GetGPUChild()) {
        return child;
      }
    }
    return nullptr;
  }

 protected:
  ~GPUMemoryReporter() = default;
};

RefPtr<MemoryReportingProcess> GPUProcessManager::GetProcessMemoryReporter() {
  // Ensure mProcess is non-null before calling EnsureGPUReady, to avoid
  // launching the process if it has not already been launched.
  if (!mProcess || NS_FAILED(EnsureGPUReady())) {
    return nullptr;
  }
  return new GPUMemoryReporter();
}

void GPUProcessManager::SetAppInForeground(bool aInForeground) {
  if (mAppInForeground == aInForeground) {
    return;
  }

  mAppInForeground = aInForeground;
#if defined(XP_WIN)
  SetProcessIsForeground();
#endif
}

#if defined(XP_WIN)
void GPUProcessManager::SetProcessIsForeground() {
  NTSTATUS WINAPI NtSetInformationProcess(
      IN HANDLE process_handle, IN ULONG info_class,
      IN PVOID process_information, IN ULONG information_length);
  constexpr unsigned int NtProcessInformationForeground = 25;

  static bool alreadyInitialized = false;
  static decltype(NtSetInformationProcess)* setInformationProcess = nullptr;
  if (!alreadyInitialized) {
    alreadyInitialized = true;
    nsModuleHandle module(LoadLibrary(L"ntdll.dll"));
    if (module) {
      setInformationProcess =
          (decltype(NtSetInformationProcess)*)GetProcAddress(
              module, "NtSetInformationProcess");
    }
  }
  if (MOZ_UNLIKELY(!setInformationProcess)) {
    return;
  }

  unsigned pid = GPUProcessPid();
  if (pid <= 0) {
    return;
  }
  // Using the handle from mProcess->GetChildProcessHandle() fails;
  // the PROCESS_SET_INFORMATION permission is probably missing.
  nsAutoHandle processHandle(
      ::OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid));
  if (!processHandle) {
    return;
  }

  BOOLEAN foreground = mAppInForeground;
  setInformationProcess(processHandle, NtProcessInformationForeground,
                        (PVOID)&foreground, sizeof(foreground));
}
#endif

RefPtr<PGPUChild::TestTriggerMetricsPromise>
GPUProcessManager::TestTriggerMetrics() {
  if (!NS_WARN_IF(!mGPUChild)) {
    return mGPUChild->SendTestTriggerMetrics();
  }

  return PGPUChild::TestTriggerMetricsPromise::CreateAndReject(
      ipc::ResponseRejectReason::SendError, __func__);
}

}  // namespace gfx
}  // namespace mozilla
