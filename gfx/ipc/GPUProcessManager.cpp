/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GPUProcessManager.h"

#include "GPUProcessHost.h"
#include "GPUProcessListener.h"
#include "mozilla/MemoryReportingProcess.h"
#include "mozilla/Preferences.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/RemoteDecoderManagerChild.h"
#include "mozilla/RemoteDecoderManagerParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/APZCTreeManagerChild.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorManagerChild.h"
#include "mozilla/layers/CompositorManagerParent.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/InProcessCompositorSession.h"
#include "mozilla/layers/LayerTreeOwnerTracker.h"
#include "mozilla/layers/RemoteCompositorSession.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#include "nsAppRunner.h"
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
#  include "mozilla/widget/AndroidUiThread.h"
#  include "mozilla/layers/UiCompositorControllerChild.h"
#endif  // defined(MOZ_WIDGET_ANDROID)

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
      mNumProcessAttempts(0),
      mDeviceResetCount(0),
      mProcess(nullptr),
      mProcessToken(0),
      mGPUChild(nullptr) {
  MOZ_COUNT_CTOR(GPUProcessManager);

  mIdNamespace = AllocateNamespace();
  mObserver = new Observer(this);
  nsContentUtils::RegisterShutdownObserver(mObserver);
  Preferences::AddStrongObserver(mObserver, "");

  mDeviceResetLastTime = TimeStamp::Now();

  LayerTreeOwnerTracker::Initialize();
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
  }
  return NS_OK;
}

void GPUProcessManager::OnXPCOMShutdown() {
  if (mObserver) {
    nsContentUtils::UnregisterShutdownObserver(mObserver);
    Preferences::RemoveObserver(mObserver, "");
    mObserver = nullptr;
  }

  CleanShutdown();
}

void GPUProcessManager::OnPreferenceChange(const char16_t* aData) {
  // A pref changed. If it's not on the blacklist, inform child processes.
  if (!dom::ContentParent::ShouldSyncPreference(aData)) {
    return;
  }

  // We know prefs are ASCII here.
  NS_LossyConvertUTF16toASCII strData(aData);

  mozilla::dom::Pref pref(strData, /* isLocked */ false, Nothing(), Nothing());
  Preferences::GetPreference(&pref);
  if (!!mGPUChild) {
    MOZ_ASSERT(mQueuedPrefs.IsEmpty());
    mGPUChild->SendPreferenceUpdate(pref);
  } else {
    mQueuedPrefs.AppendElement(pref);
  }
}

void GPUProcessManager::LaunchGPUProcess() {
  if (mProcess) {
    return;
  }

  // Start the Vsync I/O thread so can use it as soon as the process launches.
  EnsureVsyncIOThread();

  mNumProcessAttempts++;

  std::vector<std::string> extraArgs;
  nsCString parentBuildID(mozilla::PlatformBuildID());
  extraArgs.push_back("-parentBuildID");
  extraArgs.push_back(parentBuildID.get());

  // The subprocess is launched asynchronously, so we wait for a callback to
  // acquire the IPDL actor.
  mProcess = new GPUProcessHost(this);
  if (!mProcess->Launch(extraArgs)) {
    DisableGPUProcess("Failed to launch GPU process");
  }
}

void GPUProcessManager::DisableGPUProcess(const char* aMessage) {
  if (!gfxConfig::IsEnabled(Feature::GPU_PROCESS)) {
    return;
  }

  gfxConfig::SetFailed(Feature::GPU_PROCESS, FeatureStatus::Failed, aMessage);
  gfxCriticalNote << aMessage;

  gfxPlatform::NotifyGPUProcessDisabled();

  Telemetry::Accumulate(Telemetry::GPU_PROCESS_CRASH_FALLBACKS,
                        uint32_t(FallbackType::DISABLED));

  DestroyProcess();
  ShutdownVsyncIOThread();

  // We may have been in the middle of guaranteeing our various services are
  // available when one failed. Some callers may fallback to using the same
  // process equivalent, and we need to make sure those services are setup
  // correctly. We cannot re-enter DisableGPUProcess from this call because we
  // know that it is disabled in the config above.
  EnsureProtocolsReady();

  // If we disable the GPU process during reinitialization after a previous
  // crash, then we need to tell the content processes again, because they
  // need to rebind to the UI process.
  HandleProcessLost();

  // On Windows and Linux, always fallback to software.
  // The assumption is that something in the graphics driver is crashing.
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
  FallbackToSoftware("GPU Process is disabled, fallback to software solution.");
#endif
}

bool GPUProcessManager::EnsureGPUReady() {
  if (mProcess && !mProcess->IsConnected()) {
    if (!mProcess->WaitForLaunch()) {
      // If this fails, we should have fired OnProcessLaunchComplete and
      // removed the process.
      MOZ_ASSERT(!mProcess && !mGPUChild);
      return false;
    }
  }

  if (mGPUChild) {
    if (mGPUChild->EnsureGPUReady()) {
      return true;
    }

    // If the initialization above fails, we likely have a GPU process teardown
    // waiting in our message queue (or will soon). We need to ensure we don't
    // restart it later because if we fail here, our callers assume they should
    // fall back to a combined UI/GPU process. This also ensures our internal
    // state is consistent (e.g. process token is reset).
    DisableGPUProcess("Failed to initialize GPU process");
  }

  return false;
}

void GPUProcessManager::EnsureProtocolsReady() {
  EnsureCompositorManagerChild();
  EnsureImageBridgeChild();
  EnsureVRManager();
}

void GPUProcessManager::EnsureCompositorManagerChild() {
  bool gpuReady = EnsureGPUReady();
  if (CompositorManagerChild::IsInitialized(mProcessToken)) {
    return;
  }

  if (!gpuReady) {
    CompositorManagerChild::InitSameProcess(AllocateNamespace(), mProcessToken);
    return;
  }

  ipc::Endpoint<PCompositorManagerParent> parentPipe;
  ipc::Endpoint<PCompositorManagerChild> childPipe;
  nsresult rv = PCompositorManager::CreateEndpoints(
      mGPUChild->OtherPid(), base::GetCurrentProcId(), &parentPipe, &childPipe);
  if (NS_FAILED(rv)) {
    DisableGPUProcess("Failed to create PCompositorManager endpoints");
    return;
  }

  mGPUChild->SendInitCompositorManager(std::move(parentPipe));
  CompositorManagerChild::Init(std::move(childPipe), AllocateNamespace(),
                               mProcessToken);
}

void GPUProcessManager::EnsureImageBridgeChild() {
  if (ImageBridgeChild::GetSingleton()) {
    return;
  }

  if (!EnsureGPUReady()) {
    ImageBridgeChild::InitSameProcess(AllocateNamespace());
    return;
  }

  ipc::Endpoint<PImageBridgeParent> parentPipe;
  ipc::Endpoint<PImageBridgeChild> childPipe;
  nsresult rv = PImageBridge::CreateEndpoints(
      mGPUChild->OtherPid(), base::GetCurrentProcId(), &parentPipe, &childPipe);
  if (NS_FAILED(rv)) {
    DisableGPUProcess("Failed to create PImageBridge endpoints");
    return;
  }

  mGPUChild->SendInitImageBridge(std::move(parentPipe));
  ImageBridgeChild::InitWithGPUProcess(std::move(childPipe),
                                       AllocateNamespace());
}

void GPUProcessManager::EnsureVRManager() {
  if (VRManagerChild::IsCreated()) {
    return;
  }

  if (!EnsureGPUReady()) {
    VRManagerChild::InitSameProcess();
    return;
  }

  ipc::Endpoint<PVRManagerParent> parentPipe;
  ipc::Endpoint<PVRManagerChild> childPipe;
  nsresult rv = PVRManager::CreateEndpoints(
      mGPUChild->OtherPid(), base::GetCurrentProcId(), &parentPipe, &childPipe);
  if (NS_FAILED(rv)) {
    DisableGPUProcess("Failed to create PVRManager endpoints");
    return;
  }

  mGPUChild->SendInitVRManager(std::move(parentPipe));
  VRManagerChild::InitWithGPUProcess(std::move(childPipe));
}

#if defined(MOZ_WIDGET_ANDROID)
already_AddRefed<UiCompositorControllerChild>
GPUProcessManager::CreateUiCompositorController(nsBaseWidget* aWidget,
                                                const LayersId aId) {
  RefPtr<UiCompositorControllerChild> result;

  if (!EnsureGPUReady()) {
    result = UiCompositorControllerChild::CreateForSameProcess(aId);
  } else {
    ipc::Endpoint<PUiCompositorControllerParent> parentPipe;
    ipc::Endpoint<PUiCompositorControllerChild> childPipe;
    nsresult rv = PUiCompositorController::CreateEndpoints(
        mGPUChild->OtherPid(), base::GetCurrentProcId(), &parentPipe,
        &childPipe);
    if (NS_FAILED(rv)) {
      DisableGPUProcess("Failed to create PUiCompositorController endpoints");
      return nullptr;
    }

    mGPUChild->SendInitUiCompositorController(aId, std::move(parentPipe));
    result = UiCompositorControllerChild::CreateForGPUProcess(
        mProcessToken, std::move(childPipe));
  }
  if (result) {
    result->SetBaseWidget(aWidget);
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

  Endpoint<PVsyncBridgeParent> vsyncParent;
  Endpoint<PVsyncBridgeChild> vsyncChild;
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
      CrashReporter::Annotation::GPUProcessStatus,
      NS_LITERAL_CSTRING("Running"));

  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::GPUProcessLaunchCount,
      static_cast<int>(mNumProcessAttempts));
}

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
    GPUDeviceData data;
    if (mGPUChild->SendSimulateDeviceReset(&data)) {
      gfxPlatform::GetPlatform()->ImportGPUDeviceData(data);
    }
    OnRemoteProcessDeviceReset(mProcess);
  } else {
    OnInProcessDeviceReset();
  }
}

void GPUProcessManager::DisableWebRender(wr::WebRenderError aError) {
  if (!gfx::gfxVars::UseWebRender()) {
    return;
  }
  // Disable WebRender
  if (aError == wr::WebRenderError::INITIALIZE) {
    gfx::gfxConfig::GetFeature(gfx::Feature::WEBRENDER)
        .ForceDisable(
            gfx::FeatureStatus::Unavailable, "WebRender initialization failed",
            NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBRENDER_INITIALIZE"));
  } else if (aError == wr::WebRenderError::MAKE_CURRENT) {
    gfx::gfxConfig::GetFeature(gfx::Feature::WEBRENDER)
        .ForceDisable(
            gfx::FeatureStatus::Unavailable,
            "Failed to make render context current",
            NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBRENDER_MAKE_CURRENT"));
  } else if (aError == wr::WebRenderError::RENDER) {
    gfx::gfxConfig::GetFeature(gfx::Feature::WEBRENDER)
        .ForceDisable(gfx::FeatureStatus::Unavailable,
                      "Failed to render WebRender",
                      NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBRENDER_RENDER"));
  } else if (aError == wr::WebRenderError::NEW_SURFACE) {
    gfx::gfxConfig::GetFeature(gfx::Feature::WEBRENDER)
        .ForceDisable(
            gfx::FeatureStatus::Unavailable, "Failed to create new surface",
            NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBRENDER_NEW_SURFACE"));
  } else {
    MOZ_ASSERT_UNREACHABLE("Invalid value");
  }
  gfx::gfxVars::SetUseWebRender(false);

#if defined(MOZ_WIDGET_ANDROID)
  // If aError is not wr::WebRenderError::INITIALIZE, nsWindow does not
  // re-create LayerManager. Needs to trigger re-creating LayerManager on
  // android
  if (aError != wr::WebRenderError::INITIALIZE) {
    NotifyDisablingWebRender();
  }
#endif

  if (mProcess) {
    OnRemoteProcessDeviceReset(mProcess);
  } else {
    OnInProcessDeviceReset();
  }
}

void GPUProcessManager::NotifyWebRenderError(wr::WebRenderError aError) {
  DisableWebRender(aError);
}

void GPUProcessManager::OnInProcessDeviceReset() {
  RebuildInProcessSessions();
  NotifyListenersOnCompositeDeviceReset();
}

void GPUProcessManager::OnRemoteProcessDeviceReset(GPUProcessHost* aHost) {
  // Detect whether the device is resetting too quickly or too much
  // indicating that we should give up and use software
  mDeviceResetCount++;

  // Disable double buffering when device reset happens.
  if (!gfxVars::UseWebRender() && gfxVars::UseDoubleBufferingWithCompositor()) {
    gfxVars::SetUseDoubleBufferingWithCompositor(false);
  }

  auto newTime = TimeStamp::Now();
  auto delta = (int32_t)(newTime - mDeviceResetLastTime).ToMilliseconds();
  mDeviceResetLastTime = newTime;

  if (ShouldLimitDeviceResets(mDeviceResetCount, delta)) {
    DestroyProcess();
    DisableGPUProcess("GPU processed experienced too many device resets");
    HandleProcessLost();
    return;
  }

  RebuildRemoteSessions();
  NotifyListenersOnCompositeDeviceReset();
}

void GPUProcessManager::FallbackToSoftware(const char* aMessage) {
  gfxConfig::SetFailed(Feature::HW_COMPOSITING, FeatureStatus::Blocked,
                       aMessage);
#ifdef XP_WIN
  gfxConfig::SetFailed(Feature::D3D11_COMPOSITING, FeatureStatus::Blocked,
                       aMessage);
  gfxConfig::SetFailed(Feature::DIRECT2D, FeatureStatus::Blocked, aMessage);
#endif
}

void GPUProcessManager::NotifyListenersOnCompositeDeviceReset() {
  for (const auto& listener : mListeners) {
    listener->OnCompositorDeviceReset();
  }
}

void GPUProcessManager::OnProcessUnexpectedShutdown(GPUProcessHost* aHost) {
  MOZ_ASSERT(mProcess && mProcess == aHost);

  CompositorManagerChild::OnGPUProcessLost(aHost->GetProcessToken());
  DestroyProcess();

  if (mNumProcessAttempts >
      uint32_t(StaticPrefs::layers_gpu_process_max_restarts())) {
    char disableMessage[64];
    SprintfLiteral(disableMessage, "GPU process disabled after %d attempts",
                   mNumProcessAttempts);
    DisableGPUProcess(disableMessage);
  } else if (mNumProcessAttempts >
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
  if (gfxConfig::IsEnabled(Feature::GPU_PROCESS)) {
    LaunchGPUProcess();
  }

  // The shutdown and restart sequence for the GPU process is as follows:
  //
  //  (1) The GPU process dies. IPDL will enqueue an ActorDestroy message on
  //      each channel owning a bridge to the GPU process, on the thread
  //      owning that channel.
  //
  //  (2) The first channel to process its ActorDestroy message will post a
  //      message to the main thread to call NotifyRemoteActorDestroyed on
  //      the GPUProcessManager, which calls OnProcessUnexpectedShutdown if
  //      it has not handled shutdown for this process yet.
  //
  //  (3) We then notify each widget that its session with the compositor is
  //      now invalid. The widget is responsible for destroying its layer
  //      manager and CompositorBridgeChild. Note that at this stage, not
  //      all actors may have received ActorDestroy yet. CompositorBridgeChild
  //      may attempt to send messages, and if this happens, it will probably
  //      report a MsgDropped error. This is okay.
  //
  //  (4) At this point, the UI process has a clean slate: no layers should
  //      exist for the old compositor. We may make a decision on whether or
  //      not to re-launch the GPU process. Currently, we do not relaunch it,
  //      and any new compositors will be created in-process and will default
  //      to software.
  //
  //  (5) Next we notify each ContentParent of the lost connection. It will
  //      request new endpoints from the GPUProcessManager and forward them
  //      to its ContentChild. The parent-side of these endpoints may come
  //      from the compositor thread of the UI process, or the compositor
  //      thread of the GPU process. However, no actual compositors should
  //      exist yet.
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
  //        (e)      [UI] RenderFrame::EnsureLayersConnected
  //        (f)      [UI] CompositorBridgeChild::SendNotifyChildRecreated
  //
  //      Note that at step (e), RenderFrame will call GetLayerManager
  //      on the nsIWidget owning the tab. This step ensures that a compositor
  //      exists for the window. If we decided to launch a new GPU Process,
  //      at this point we block until the process has launched and we're
  //      able to create a new window compositor. Otherwise, if compositing
  //      is now in-process, this will simply create a new
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
  //      applied, and the window will not have a new compositor just yet.
  //      The next refresh tick and paint will ensure that one exists, again
  //      via nsIWidget::GetLayerManager.
  RebuildRemoteSessions();

  // Notify content. This will ensure that each content process re-establishes
  // a connection to the compositor thread (whether it's in-process or in a
  // newly launched GPU process).
  for (const auto& listener : mListeners) {
    listener->OnCompositorUnexpectedShutdown();
  }
}

void GPUProcessManager::RebuildRemoteSessions() {
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

void GPUProcessManager::RebuildInProcessSessions() {
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
}

void GPUProcessManager::NotifyDisablingWebRender() {
#if defined(MOZ_WIDGET_ANDROID)
  for (const auto& session : mRemoteSessions) {
    session->NotifyDisablingWebRender();
  }

  for (const auto& session : mInProcessSessions) {
    session->NotifyDisablingWebRender();
  }
#endif
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

void GPUProcessManager::DestroyProcess() {
  if (!mProcess) {
    return;
  }

  mProcess->Shutdown();
  mProcessToken = 0;
  mProcess = nullptr;
  mGPUChild = nullptr;
  if (mVsyncBridge) {
    mVsyncBridge->Close();
    mVsyncBridge = nullptr;
  }

  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::GPUProcessStatus,
      NS_LITERAL_CSTRING("Destroyed"));
}

already_AddRefed<CompositorSession> GPUProcessManager::CreateTopLevelCompositor(
    nsBaseWidget* aWidget, LayerManager* aLayerManager,
    CSSToLayoutDeviceScale aScale, const CompositorOptions& aOptions,
    bool aUseExternalSurfaceSize, const gfx::IntSize& aSurfaceSize,
    bool* aRetryOut) {
  MOZ_ASSERT(aRetryOut);

  LayersId layerTreeId = AllocateLayerTreeId();

  EnsureProtocolsReady();

  RefPtr<CompositorSession> session;

  if (EnsureGPUReady()) {
    session =
        CreateRemoteSession(aWidget, aLayerManager, layerTreeId, aScale,
                            aOptions, aUseExternalSurfaceSize, aSurfaceSize);
    if (!session) {
      // We couldn't create a remote compositor, so abort the process.
      DisableGPUProcess("Failed to create remote compositor");
      *aRetryOut = true;
      return nullptr;
    }
  } else {
    session = InProcessCompositorSession::Create(
        aWidget, aLayerManager, layerTreeId, aScale, aOptions,
        aUseExternalSurfaceSize, aSurfaceSize, AllocateNamespace());
  }

#if defined(MOZ_WIDGET_ANDROID)
  if (session) {
    // Nothing to do if controller gets a nullptr
    RefPtr<UiCompositorControllerChild> controller =
        CreateUiCompositorController(aWidget, session->RootLayerTreeId());
    session->SetUiCompositorControllerChild(controller);
  }
#endif  // defined(MOZ_WIDGET_ANDROID)

  *aRetryOut = false;
  return session.forget();
}

RefPtr<CompositorSession> GPUProcessManager::CreateRemoteSession(
    nsBaseWidget* aWidget, LayerManager* aLayerManager,
    const LayersId& aRootLayerTreeId, CSSToLayoutDeviceScale aScale,
    const CompositorOptions& aOptions, bool aUseExternalSurfaceSize,
    const gfx::IntSize& aSurfaceSize) {
#ifdef MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING
  CompositorWidgetInitData initData;
  aWidget->GetCompositorWidgetInitData(&initData);

  RefPtr<CompositorBridgeChild> child =
      CompositorManagerChild::CreateWidgetCompositorBridge(
          mProcessToken, aLayerManager, AllocateNamespace(), aScale, aOptions,
          aUseExternalSurfaceSize, aSurfaceSize);
  if (!child) {
    gfxCriticalNote << "Failed to create CompositorBridgeChild";
    return nullptr;
  }

  RefPtr<CompositorVsyncDispatcher> dispatcher =
      aWidget->GetCompositorVsyncDispatcher();
  RefPtr<CompositorWidgetVsyncObserver> observer =
      new CompositorWidgetVsyncObserver(mVsyncBridge, aRootLayerTreeId);

  CompositorWidgetChild* widget =
      new CompositorWidgetChild(dispatcher, observer);
  if (!child->SendPCompositorWidgetConstructor(widget, initData)) {
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

    RefPtr<APZInputBridgeChild> pinput = new APZInputBridgeChild();
    if (!mGPUChild->SendPAPZInputBridgeConstructor(pinput, aRootLayerTreeId)) {
      return nullptr;
    }
    apz->SetInputBridge(pinput);
  }

  RefPtr<RemoteCompositorSession> session = new RemoteCompositorSession(
      aWidget, child, widget, apz, aRootLayerTreeId);
  return session.forget();
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
    nsTArray<uint32_t>* aNamespaces) {
  if (!CreateContentCompositorManager(aOtherProcess, aOutCompositor) ||
      !CreateContentImageBridge(aOtherProcess, aOutImageBridge) ||
      !CreateContentVRManager(aOtherProcess, aOutVRBridge)) {
    return false;
  }
  // VideoDeocderManager is only supported in the GPU process, so we allow this
  // to be fallible.
  CreateContentRemoteDecoderManager(aOtherProcess, aOutVideoManager);
  // Allocates 3 namespaces(for CompositorManagerChild, CompositorBridgeChild
  // and ImageBridgeChild)
  aNamespaces->AppendElement(AllocateNamespace());
  aNamespaces->AppendElement(AllocateNamespace());
  aNamespaces->AppendElement(AllocateNamespace());
  return true;
}

bool GPUProcessManager::CreateContentCompositorManager(
    base::ProcessId aOtherProcess,
    ipc::Endpoint<PCompositorManagerChild>* aOutEndpoint) {
  ipc::Endpoint<PCompositorManagerParent> parentPipe;
  ipc::Endpoint<PCompositorManagerChild> childPipe;

  base::ProcessId parentPid =
      EnsureGPUReady() ? mGPUChild->OtherPid() : base::GetCurrentProcId();

  nsresult rv = PCompositorManager::CreateEndpoints(parentPid, aOtherProcess,
                                                    &parentPipe, &childPipe);
  if (NS_FAILED(rv)) {
    gfxCriticalNote << "Could not create content compositor manager: "
                    << hexa(int(rv));
    return false;
  }

  if (mGPUChild) {
    mGPUChild->SendNewContentCompositorManager(std::move(parentPipe));
  } else if (!CompositorManagerParent::Create(std::move(parentPipe),
                                              /* aIsRoot */ false)) {
    return false;
  }

  *aOutEndpoint = std::move(childPipe);
  return true;
}

bool GPUProcessManager::CreateContentImageBridge(
    base::ProcessId aOtherProcess,
    ipc::Endpoint<PImageBridgeChild>* aOutEndpoint) {
  EnsureImageBridgeChild();

  base::ProcessId parentPid =
      EnsureGPUReady() ? mGPUChild->OtherPid() : base::GetCurrentProcId();

  ipc::Endpoint<PImageBridgeParent> parentPipe;
  ipc::Endpoint<PImageBridgeChild> childPipe;
  nsresult rv = PImageBridge::CreateEndpoints(parentPid, aOtherProcess,
                                              &parentPipe, &childPipe);
  if (NS_FAILED(rv)) {
    gfxCriticalNote << "Could not create content compositor bridge: "
                    << hexa(int(rv));
    return false;
  }

  if (mGPUChild) {
    mGPUChild->SendNewContentImageBridge(std::move(parentPipe));
  } else {
    if (!ImageBridgeParent::CreateForContent(std::move(parentPipe))) {
      return false;
    }
  }

  *aOutEndpoint = std::move(childPipe);
  return true;
}

base::ProcessId GPUProcessManager::GPUProcessPid() {
  base::ProcessId gpuPid = mGPUChild ? mGPUChild->OtherPid() : -1;
  return gpuPid;
}

bool GPUProcessManager::CreateContentVRManager(
    base::ProcessId aOtherProcess,
    ipc::Endpoint<PVRManagerChild>* aOutEndpoint) {
  EnsureVRManager();

  base::ProcessId parentPid =
      EnsureGPUReady() ? mGPUChild->OtherPid() : base::GetCurrentProcId();

  ipc::Endpoint<PVRManagerParent> parentPipe;
  ipc::Endpoint<PVRManagerChild> childPipe;
  nsresult rv = PVRManager::CreateEndpoints(parentPid, aOtherProcess,
                                            &parentPipe, &childPipe);
  if (NS_FAILED(rv)) {
    gfxCriticalNote << "Could not create content compositor bridge: "
                    << hexa(int(rv));
    return false;
  }

  if (mGPUChild) {
    mGPUChild->SendNewContentVRManager(std::move(parentPipe));
  } else {
    if (!VRManagerParent::CreateForContent(std::move(parentPipe))) {
      return false;
    }
  }

  *aOutEndpoint = std::move(childPipe);
  return true;
}

void GPUProcessManager::CreateContentRemoteDecoderManager(
    base::ProcessId aOtherProcess,
    ipc::Endpoint<PRemoteDecoderManagerChild>* aOutEndpoint) {
  if (!EnsureGPUReady() || !StaticPrefs::media_gpu_process_decoder() ||
      !mDecodeVideoOnGpuProcess) {
    return;
  }

  ipc::Endpoint<PRemoteDecoderManagerParent> parentPipe;
  ipc::Endpoint<PRemoteDecoderManagerChild> childPipe;

  nsresult rv = PRemoteDecoderManager::CreateEndpoints(
      mGPUChild->OtherPid(), aOtherProcess, &parentPipe, &childPipe);
  if (NS_FAILED(rv)) {
    gfxCriticalNote << "Could not create content video decoder: "
                    << hexa(int(rv));
    return;
  }

  mGPUChild->SendNewContentRemoteDecoderManager(std::move(parentPipe));

  *aOutEndpoint = std::move(childPipe);
}

void GPUProcessManager::MapLayerTreeId(LayersId aLayersId,
                                       base::ProcessId aOwningId) {
  LayerTreeOwnerTracker::Get()->Map(aLayersId, aOwningId);

  if (EnsureGPUReady()) {
    mGPUChild->SendAddLayerTreeIdMapping(
        LayerTreeIdMapping(aLayersId, aOwningId));
  }
}

void GPUProcessManager::UnmapLayerTreeId(LayersId aLayersId,
                                         base::ProcessId aOwningId) {
  LayerTreeOwnerTracker::Get()->Unmap(aLayersId, aOwningId);

  if (EnsureGPUReady()) {
    mGPUChild->SendRemoveLayerTreeIdMapping(
        LayerTreeIdMapping(aLayersId, aOwningId));
    return;
  }
  CompositorBridgeParent::DeallocateLayerTreeId(aLayersId);
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
  if (!EnsureGPUReady()) {
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

  bool SendRequestMemoryReport(const uint32_t& aGeneration,
                               const bool& aAnonymize,
                               const bool& aMinimizeMemoryUsage,
                               const Maybe<FileDescriptor>& aDMDFile) override {
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
  if (!EnsureGPUReady()) {
    return nullptr;
  }
  return new GPUMemoryReporter();
}

}  // namespace gfx
}  // namespace mozilla
