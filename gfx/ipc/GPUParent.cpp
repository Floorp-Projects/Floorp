/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef XP_WIN
#  include "WMF.h"
#  include "WMFDecoderModule.h"
#endif
#include "GLContextProvider.h"
#include "GPUParent.h"
#include "GPUProcessHost.h"
#include "GPUProcessManager.h"
#include "gfxGradientCache.h"
#include "GfxInfoBase.h"
#include "CanvasManagerParent.h"
#include "VRGPUChild.h"
#include "VRManager.h"
#include "VRManagerParent.h"
#include "VsyncBridgeParent.h"
#include "cairo.h"
#include "gfxConfig.h"
#include "gfxCrashReporterUtils.h"
#include "gfxPlatform.h"
#include "mozilla/Assertions.h"
#include "mozilla/Components.h"
#include "mozilla/FOGIPC.h"
#include "mozilla/HangDetails.h"
#include "mozilla/PerfStats.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProcessPriorityManager.h"
#include "mozilla/RemoteDecoderManagerChild.h"
#include "mozilla/RemoteDecoderManagerParent.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/MemoryReportRequest.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/glean/GleanMetrics.h"
#include "mozilla/image/ImageMemoryReporter.h"
#include "mozilla/ipc/CrashReporterClient.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/ipc/ProcessUtils.h"
#include "mozilla/layers/APZInputBridgeParent.h"
#include "mozilla/layers/APZPublicUtils.h"  // for apz::InitializeGlobalState
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorManagerParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/LayerTreeOwnerTracker.h"
#include "mozilla/layers/RemoteTextureMap.h"
#include "mozilla/layers/UiCompositorControllerParent.h"
#include "mozilla/layers/VideoBridgeParent.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "nsDebugImpl.h"
#include "nsIGfxInfo.h"
#include "nsIXULRuntime.h"
#include "nsThreadManager.h"
#include "nscore.h"
#include "prenv.h"
#include "skia/include/core/SkGraphics.h"
#if defined(XP_WIN)
#  include <dwrite.h>
#  include <process.h>
#  include <windows.h>

#  include "gfxDWriteFonts.h"
#  include "gfxWindowsPlatform.h"
#  include "mozilla/WindowsVersion.h"
#  include "mozilla/gfx/DeviceManagerDx.h"
#  include "mozilla/layers/TextureD3D11.h"
#  include "mozilla/widget/WinCompositorWindowThread.h"
#else
#  include <unistd.h>
#endif
#ifdef MOZ_WIDGET_GTK
#  include <gtk/gtk.h>

#  include "skia/include/ports/SkTypeface_cairo.h"
#endif
#ifdef ANDROID
#  include "mozilla/layers/AndroidHardwareBuffer.h"
#  include "skia/include/ports/SkTypeface_cairo.h"
#endif
#include "ChildProfilerController.h"
#include "nsAppRunner.h"

#if defined(MOZ_SANDBOX) && defined(MOZ_DEBUG) && defined(ENABLE_TESTS)
#  include "mozilla/SandboxTestingChild.h"
#endif

namespace mozilla::gfx {

using namespace ipc;
using namespace layers;

static GPUParent* sGPUParent;

GPUParent::GPUParent() : mLaunchTime(TimeStamp::Now()) { sGPUParent = this; }

GPUParent::~GPUParent() { sGPUParent = nullptr; }

/* static */
GPUParent* GPUParent::GetSingleton() {
  MOZ_DIAGNOSTIC_ASSERT(sGPUParent);
  return sGPUParent;
}

/* static */ bool GPUParent::MaybeFlushMemory() {
#if defined(XP_WIN) && !defined(HAVE_64BIT_BUILD)
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  if (!XRE_IsGPUProcess()) {
    return false;
  }

  MEMORYSTATUSEX stat;
  stat.dwLength = sizeof(stat);
  if (!GlobalMemoryStatusEx(&stat)) {
    return false;
  }

  // We only care about virtual process memory space in the GPU process because
  // the UI process is already watching total memory usage.
  static const size_t kLowVirtualMemoryThreshold = 384 * 1024 * 1024;
  bool lowMemory = stat.ullAvailVirtual < kLowVirtualMemoryThreshold;

  // We suppress more than one low memory notification until we exit the
  // condition. The UI process goes through more effort, reporting on-going
  // memory pressure, but rather than try to manage a shared state, we just
  // send one notification here to try to resolve it.
  static bool sLowMemory = false;
  if (lowMemory && !sLowMemory) {
    NS_DispatchToMainThread(
        NS_NewRunnableFunction("gfx::GPUParent::FlushMemory", []() -> void {
          Unused << GPUParent::GetSingleton()->SendFlushMemory(
              u"low-memory"_ns);
        }));
  }
  sLowMemory = lowMemory;
  return lowMemory;
#else
  return false;
#endif
}

bool GPUParent::Init(base::ProcessId aParentPid, const char* aParentBuildID,
                     mozilla::ipc::ScopedPort aPort) {
  // Initialize the thread manager before starting IPC. Otherwise, messages
  // may be posted to the main thread and we won't be able to process them.
  if (NS_WARN_IF(NS_FAILED(nsThreadManager::get().Init()))) {
    return false;
  }

  // Now it's safe to start IPC.
  if (NS_WARN_IF(!Open(std::move(aPort), aParentPid))) {
    return false;
  }

  nsDebugImpl::SetMultiprocessMode("GPU");

  // This must be checked before any IPDL message, which may hit sentinel
  // errors due to parent and content processes having different
  // versions.
  MessageChannel* channel = GetIPCChannel();
  if (channel && !channel->SendBuildIDsMatchMessage(aParentBuildID)) {
    // We need to quit this process if the buildID doesn't match the parent's.
    // This can occur when an update occurred in the background.
    ProcessChild::QuickExit();
  }

  if (NS_FAILED(NS_InitMinimalXPCOM())) {
    return false;
  }

  // Ensure the observer service exists.
  ProcessPriorityManager::Init();

  // Init crash reporter support.
  CrashReporterClient::InitSingleton(this);

  gfxConfig::Init();
  gfxVars::Initialize();
  gfxPlatform::InitNullMetadata();
  // Ensure our Factory is initialised, mainly for gfx logging to work.
  gfxPlatform::InitMoz2DLogging();
#if defined(XP_WIN)
  gfxWindowsPlatform::InitMemoryReportersForGPUProcess();
  DeviceManagerDx::Init();
  GpuProcessD3D11TextureMap::Init();
#endif

  CompositorThreadHolder::Start();
  RemoteTextureMap::Init();
  APZThreadUtils::SetControllerThread(NS_GetCurrentThread());
  apz::InitializeGlobalState();
  LayerTreeOwnerTracker::Initialize();
  CompositorBridgeParent::InitializeStatics();
  gfxGradientCache::Init();
  mozilla::ipc::SetThisProcessName("GPU Process");

  return true;
}

void GPUParent::NotifyDeviceReset() {
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "gfx::GPUParent::NotifyDeviceReset",
        []() -> void { GPUParent::GetSingleton()->NotifyDeviceReset(); }));
    return;
  }

  // Reset and reinitialize the compositor devices
#ifdef XP_WIN
  if (!DeviceManagerDx::Get()->MaybeResetAndReacquireDevices()) {
    // If the device doesn't need to be reset then the device
    // has already been reset by a previous NotifyDeviceReset message.
    return;
  }
#endif

  // Notify the main process that there's been a device reset
  // and that they should reset their compositors and repaint
  GPUDeviceData data;
  RecvGetDeviceStatus(&data);
  Unused << SendNotifyDeviceReset(data);
}

void GPUParent::NotifyOverlayInfo(layers::OverlayInfo aInfo) {
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "gfx::GPUParent::NotifyOverlayInfo", [aInfo]() -> void {
          GPUParent::GetSingleton()->NotifyOverlayInfo(aInfo);
        }));
    return;
  }
  Unused << SendNotifyOverlayInfo(aInfo);
}

mozilla::ipc::IPCResult GPUParent::RecvInit(
    nsTArray<GfxVarUpdate>&& vars, const DevicePrefs& devicePrefs,
    nsTArray<LayerTreeIdMapping>&& aMappings,
    nsTArray<GfxInfoFeatureStatus>&& aFeatures, uint32_t aWrNamespace) {
  for (const auto& var : vars) {
    gfxVars::ApplyUpdate(var);
  }

  // Inherit device preferences.
  gfxConfig::Inherit(Feature::HW_COMPOSITING, devicePrefs.hwCompositing());
  gfxConfig::Inherit(Feature::D3D11_COMPOSITING,
                     devicePrefs.d3d11Compositing());
  gfxConfig::Inherit(Feature::OPENGL_COMPOSITING, devicePrefs.oglCompositing());
  gfxConfig::Inherit(Feature::DIRECT2D, devicePrefs.useD2D1());
  gfxConfig::Inherit(Feature::D3D11_HW_ANGLE, devicePrefs.d3d11HwAngle());

  {  // Let the crash reporter know if we've got WR enabled or not. For other
    // processes this happens in gfxPlatform::InitWebRenderConfig.
    ScopedGfxFeatureReporter reporter("WR",
                                      gfxPlatform::WebRenderPrefEnabled());
    if (gfxVars::UseWebRender()) {
      reporter.SetSuccessful();
    }
  }

  for (const LayerTreeIdMapping& map : aMappings) {
    LayerTreeOwnerTracker::Get()->Map(map.layersId(), map.ownerId());
  }

  widget::GfxInfoBase::SetFeatureStatus(std::move(aFeatures));

  // We bypass gfxPlatform::Init, so we must initialize any relevant libraries
  // here that would normally be initialized there.
  SkGraphics::Init();

#if defined(XP_WIN)
  if (gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING)) {
    if (DeviceManagerDx::Get()->CreateCompositorDevices() &&
        gfxVars::RemoteCanvasEnabled()) {
      if (DeviceManagerDx::Get()->CreateCanvasDevice()) {
        gfxDWriteFont::InitDWriteSupport();
      } else {
        gfxWarning() << "Failed to create canvas device.";
      }
    }
  }
  if (gfxVars::UseWebRender()) {
    DeviceManagerDx::Get()->CreateDirectCompositionDevice();
    // Ensure to initialize GfxInfo
    nsCOMPtr<nsIGfxInfo> gfxInfo = components::GfxInfo::Service();
    Unused << gfxInfo;

    Factory::EnsureDWriteFactory();
  }
#endif

#if defined(MOZ_WIDGET_GTK)
  char* display_name = PR_GetEnv("MOZ_GDK_DISPLAY");
  if (!display_name) {
    bool waylandEnabled = false;
#  ifdef MOZ_WAYLAND
    waylandEnabled = IsWaylandEnabled();
#  endif
    if (!waylandEnabled) {
      display_name = PR_GetEnv("DISPLAY");
    }
  }
  if (display_name) {
    int argc = 3;
    char option_name[] = "--display";
    char* argv[] = {// argv0 is unused because g_set_prgname() was called in
                    // XRE_InitChildProcess().
                    nullptr, option_name, display_name, nullptr};
    char** argvp = argv;
    gtk_init(&argc, &argvp);
  } else {
    gtk_init(nullptr, nullptr);
  }

  // Ensure we have an FT library for font instantiation.
  // This would normally be set by gfxPlatform::Init().
  // Since we bypass that, we must do it here instead.
  if (gfxVars::UseWebRender()) {
    FT_Library library = Factory::NewFTLibrary();
    MOZ_ASSERT(library);
    Factory::SetFTLibrary(library);

    // true to match gfxPlatform::FontHintingEnabled(). We must hardcode
    // this value because we do not have a gfxPlatform instance.
    SkInitCairoFT(true);
  }

  // Ensure that GfxInfo::Init is called on the main thread.
  nsCOMPtr<nsIGfxInfo> gfxInfo = components::GfxInfo::Service();
  Unused << gfxInfo;
#endif

#ifdef ANDROID
  // Ensure we have an FT library for font instantiation.
  // This would normally be set by gfxPlatform::Init().
  // Since we bypass that, we must do it here instead.
  FT_Library library = Factory::NewFTLibrary();
  MOZ_ASSERT(library);
  Factory::SetFTLibrary(library);

  // false to match gfxAndroidPlatform::FontHintingEnabled(). We must
  // hardcode this value because we do not have a gfxPlatform instance.
  SkInitCairoFT(false);

  if (gfxVars::UseAHardwareBufferContent() ||
      gfxVars::UseAHardwareBufferSharedSurface()) {
    layers::AndroidHardwareBufferApi::Init();
    layers::AndroidHardwareBufferManager::Init();
  }

#endif

  // Make sure to do this *after* we update gfxVars above.
  if (gfxVars::UseWebRender()) {
    wr::RenderThread::Start(aWrNamespace);
    image::ImageMemoryReporter::InitForWebRender();
  }
#ifdef XP_WIN
  else {
    if (gfxVars::UseDoubleBufferingWithCompositor()) {
      // This is needed to avoid freezing the window on a device crash on double
      // buffering, see bug 1549674.
      widget::WinCompositorWindowThread::Start();
    }
  }
#endif

  VRManager::ManagerInit();
  // Send a message to the UI process that we're done.
  GPUDeviceData data;
  RecvGetDeviceStatus(&data);
  Unused << SendInitComplete(data);

  // Dispatch a task to run when idle that will determine which codecs are
  // usable. The primary goal is to determine if the media feature pack is
  // installed.
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThreadQueue(
      NS_NewRunnableFunction(
          "GPUParent::Supported",
          []() {
            auto supported = PDMFactory::Supported();
            Unused << GPUParent::GetSingleton()->SendUpdateMediaCodecsSupported(
                supported);
          }),
      2000 /* 2 seconds timeout */, EventQueuePriority::Idle));

  Telemetry::AccumulateTimeDelta(Telemetry::GPU_PROCESS_INITIALIZATION_TIME_MS,
                                 mLaunchTime);
  return IPC_OK();
}

#if defined(MOZ_SANDBOX) && defined(MOZ_DEBUG) && defined(ENABLE_TESTS)
mozilla::ipc::IPCResult GPUParent::RecvInitSandboxTesting(
    Endpoint<PSandboxTestingChild>&& aEndpoint) {
  if (!SandboxTestingChild::Initialize(std::move(aEndpoint))) {
    return IPC_FAIL(
        this, "InitSandboxTesting failed to initialise the child process.");
  }
  return IPC_OK();
}
#endif

mozilla::ipc::IPCResult GPUParent::RecvInitCompositorManager(
    Endpoint<PCompositorManagerParent>&& aEndpoint) {
  CompositorManagerParent::Create(std::move(aEndpoint), /* aIsRoot */ true);
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvInitVsyncBridge(
    Endpoint<PVsyncBridgeParent>&& aVsyncEndpoint) {
  mVsyncBridge = VsyncBridgeParent::Start(std::move(aVsyncEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvInitImageBridge(
    Endpoint<PImageBridgeParent>&& aEndpoint) {
  ImageBridgeParent::CreateForGPUProcess(std::move(aEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvInitVideoBridge(
    Endpoint<PVideoBridgeParent>&& aEndpoint) {
  VideoBridgeParent::Open(std::move(aEndpoint), VideoBridgeSource::RddProcess);
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvInitVRManager(
    Endpoint<PVRManagerParent>&& aEndpoint) {
  VRManagerParent::CreateForGPUProcess(std::move(aEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvInitVR(
    Endpoint<PVRGPUChild>&& aEndpoint) {
  gfx::VRGPUChild::InitForGPUProcess(std::move(aEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvInitUiCompositorController(
    const LayersId& aRootLayerTreeId,
    Endpoint<PUiCompositorControllerParent>&& aEndpoint) {
  UiCompositorControllerParent::Start(aRootLayerTreeId, std::move(aEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvInitAPZInputBridge(
    const LayersId& aRootLayerTreeId,
    Endpoint<PAPZInputBridgeParent>&& aEndpoint) {
  APZInputBridgeParent::Create(aRootLayerTreeId, std::move(aEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvInitProfiler(
    Endpoint<PProfilerChild>&& aEndpoint) {
  mProfilerController = ChildProfilerController::Create(std::move(aEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvUpdateVar(const GfxVarUpdate& aUpdate) {
#if defined(XP_WIN)
  auto scopeExit = MakeScopeExit(
      [couldUseHWDecoder = gfx::gfxVars::CanUseHardwareVideoDecoding()] {
        if (couldUseHWDecoder != gfx::gfxVars::CanUseHardwareVideoDecoding()) {
          // The capabilities of the system may have changed, force a refresh by
          // re-initializing the WMF PDM.
          WMFDecoderModule::Init();
          Unused << GPUParent::GetSingleton()->SendUpdateMediaCodecsSupported(
              PDMFactory::Supported(true /* force refresh */));
        }
      });
#endif
  gfxVars::ApplyUpdate(aUpdate);
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvPreferenceUpdate(const Pref& aPref) {
  Preferences::SetPreference(aPref);
  return IPC_OK();
}

static void CopyFeatureChange(Feature aFeature, Maybe<FeatureFailure>* aOut) {
  FeatureState& feature = gfxConfig::GetFeature(aFeature);
  if (feature.DisabledByDefault() || feature.IsEnabled()) {
    // No change:
    //   - Disabled-by-default means the parent process told us not to use this
    //   feature.
    //   - Enabled means we were told to use this feature, and we didn't
    //   discover anything
    //     that would prevent us from doing so.
    *aOut = Nothing();
    return;
  }

  MOZ_ASSERT(!feature.IsEnabled());

  nsCString message;
  message.AssignASCII(feature.GetFailureMessage());

  *aOut =
      Some(FeatureFailure(feature.GetValue(), message, feature.GetFailureId()));
}

mozilla::ipc::IPCResult GPUParent::RecvGetDeviceStatus(GPUDeviceData* aOut) {
  CopyFeatureChange(Feature::D3D11_COMPOSITING, &aOut->d3d11Compositing());
  CopyFeatureChange(Feature::OPENGL_COMPOSITING, &aOut->oglCompositing());

#if defined(XP_WIN)
  if (DeviceManagerDx* dm = DeviceManagerDx::Get()) {
    D3D11DeviceStatus deviceStatus;
    dm->ExportDeviceInfo(&deviceStatus);
    aOut->gpuDevice() = Some(deviceStatus);
  }
#else
  aOut->gpuDevice() = Nothing();
#endif

  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvSimulateDeviceReset() {
#if defined(XP_WIN)
  DeviceManagerDx::Get()->ForceDeviceReset(
      ForcedDeviceResetReason::COMPOSITOR_UPDATED);
#endif
  wr::RenderThread::Get()->SimulateDeviceReset();
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvNewContentCompositorManager(
    Endpoint<PCompositorManagerParent>&& aEndpoint) {
  CompositorManagerParent::Create(std::move(aEndpoint), /* aIsRoot */ false);
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvNewContentImageBridge(
    Endpoint<PImageBridgeParent>&& aEndpoint) {
  if (!ImageBridgeParent::CreateForContent(std::move(aEndpoint))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvNewContentVRManager(
    Endpoint<PVRManagerParent>&& aEndpoint) {
  if (!VRManagerParent::CreateForContent(std::move(aEndpoint))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvNewContentRemoteDecoderManager(
    Endpoint<PRemoteDecoderManagerParent>&& aEndpoint) {
  if (!RemoteDecoderManagerParent::CreateForContent(std::move(aEndpoint))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvAddLayerTreeIdMapping(
    const LayerTreeIdMapping& aMapping) {
  LayerTreeOwnerTracker::Get()->Map(aMapping.layersId(), aMapping.ownerId());
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvRemoveLayerTreeIdMapping(
    const LayerTreeIdMapping& aMapping) {
  LayerTreeOwnerTracker::Get()->Unmap(aMapping.layersId(), aMapping.ownerId());
  CompositorBridgeParent::DeallocateLayerTreeId(aMapping.layersId());
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvNotifyGpuObservers(
    const nsCString& aTopic) {
  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  MOZ_ASSERT(obsSvc);
  if (obsSvc) {
    obsSvc->NotifyObservers(nullptr, aTopic.get(), nullptr);
  }
  return IPC_OK();
}

/* static */
void GPUParent::GetGPUProcessName(nsACString& aStr) {
  auto processType = XRE_GetProcessType();
  unsigned pid = 0;
  if (processType == GeckoProcessType_GPU) {
    pid = getpid();
  } else {
    MOZ_DIAGNOSTIC_ASSERT(processType == GeckoProcessType_Default);
    pid = GPUProcessManager::Get()->GPUProcessPid();
  }

  nsPrintfCString processName("GPU (pid %u)", pid);
  aStr.Assign(processName);
}

mozilla::ipc::IPCResult GPUParent::RecvRequestMemoryReport(
    const uint32_t& aGeneration, const bool& aAnonymize,
    const bool& aMinimizeMemoryUsage, const Maybe<FileDescriptor>& aDMDFile,
    const RequestMemoryReportResolver& aResolver) {
  nsAutoCString processName;
  GetGPUProcessName(processName);

  mozilla::dom::MemoryReportRequestClient::Start(
      aGeneration, aAnonymize, aMinimizeMemoryUsage, aDMDFile, processName,
      [&](const MemoryReport& aReport) {
        Unused << GetSingleton()->SendAddMemoryReport(aReport);
      },
      aResolver);
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvShutdownVR() {
  if (StaticPrefs::dom_vr_process_enabled_AtStartup()) {
    VRGPUChild::Shutdown();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvUpdatePerfStatsCollectionMask(
    const uint64_t& aMask) {
  PerfStats::SetCollectionMask(static_cast<PerfStats::MetricMask>(aMask));
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvCollectPerfStatsJSON(
    CollectPerfStatsJSONResolver&& aResolver) {
  aResolver(PerfStats::CollectLocalPerfStatsJSON());
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvFlushFOGData(
    FlushFOGDataResolver&& aResolver) {
  glean::FlushFOGData(std::move(aResolver));
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvTestTriggerMetrics(
    TestTriggerMetricsResolver&& aResolve) {
  mozilla::glean::test_only_ipc::a_counter.Add(nsIXULRuntime::PROCESS_TYPE_GPU);
  aResolve(true);
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvCrashProcess() {
  MOZ_CRASH("Deliberate GPU process crash");
  return IPC_OK();
}

void GPUParent::ActorDestroy(ActorDestroyReason aWhy) {
  if (AbnormalShutdown == aWhy) {
    NS_WARNING("Shutting down GPU process early due to a crash!");
    ProcessChild::QuickExit();
  }

  // Send the last bits of Glean data over to the main process.
  glean::FlushFOGData(
      [](ByteBuf&& aBuf) { glean::SendFOGData(std::move(aBuf)); });

#ifndef NS_FREE_PERMANENT_DATA
  // No point in going through XPCOM shutdown because we don't keep persistent
  // state.
  ProcessChild::QuickExit();
#endif

  // Wait until all RemoteDecoderManagerParent have closed.
  mShutdownBlockers.WaitUntilClear(10 * 1000 /* 10s timeout*/)
      ->Then(GetCurrentSerialEventTarget(), __func__, [this]() {
        if (mProfilerController) {
          mProfilerController->Shutdown();
          mProfilerController = nullptr;
        }

        if (mVsyncBridge) {
          mVsyncBridge->Shutdown();
          mVsyncBridge = nullptr;
        }
        RemoteDecoderManagerParent::ShutdownVideoBridge();
        // This could be running on either the Compositor or the Renderer
        // thread.
        CanvasManagerParent::Shutdown();
        RemoteTextureMap::Shutdown();
        CompositorThreadHolder::Shutdown();
        // There is a case that RenderThread exists when gfxVars::UseWebRender()
        // is false. This could happen when WebRender was fallbacked to
        // compositor.
        if (wr::RenderThread::Get()) {
          wr::RenderThread::ShutDown();
        }
#ifdef XP_WIN
        if (widget::WinCompositorWindowThread::Get()) {
          widget::WinCompositorWindowThread::ShutDown();
        }
#endif

        image::ImageMemoryReporter::ShutdownForWebRender();

        // Shut down the default GL context provider.
        gl::GLContextProvider::Shutdown();

#if defined(XP_WIN)
        // The above shutdown calls operate on the available context providers
        // on most platforms.  Windows is a "special snowflake", though, and has
        // three context providers available, so we have to shut all of them
        // down. We should only support the default GL provider on Windows;
        // then, this could go away. Unfortunately, we currently support WGL
        // (the default) for WebGL on Optimus.
        gl::GLContextProviderEGL::Shutdown();
#endif

        Factory::ShutDown();

    // We bypass gfxPlatform shutdown, so we must shutdown any libraries here
    // that would normally be handled by it.
#ifdef NS_FREE_PERMANENT_DATA
        SkGraphics::PurgeFontCache();
        cairo_debug_reset_static_data();
#endif

#if defined(XP_WIN)
        GpuProcessD3D11TextureMap::Shutdown();
        DeviceManagerDx::Shutdown();
#endif
        LayerTreeOwnerTracker::Shutdown();
        gfxVars::Shutdown();
        gfxConfig::Shutdown();
        CrashReporterClient::DestroySingleton();
        XRE_ShutdownChildProcess();
      });
}

}  // namespace mozilla::gfx
