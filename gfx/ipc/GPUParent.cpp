/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifdef XP_WIN
#include "WMF.h"
#endif
#include "GPUParent.h"
#include "gfxConfig.h"
#include "gfxCrashReporterUtils.h"
#include "gfxPlatform.h"
#include "gfxPrefs.h"
#include "GPUProcessHost.h"
#include "mozilla/Assertions.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/MemoryReportRequest.h"
#include "mozilla/dom/VideoDecoderManagerChild.h"
#include "mozilla/dom/VideoDecoderManagerParent.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/ipc/CrashReporterClient.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/layers/APZInputBridgeParent.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/APZUtils.h"    // for apz::InitializeGlobalState
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorManagerParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/LayerTreeOwnerTracker.h"
#include "mozilla/layers/UiCompositorControllerParent.h"
#include "mozilla/layers/MemoryReportingMLGPU.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/HangDetails.h"
#include "nsDebugImpl.h"
#include "nsIGfxInfo.h"
#include "nsThreadManager.h"
#include "prenv.h"
#include "ProcessUtils.h"
#include "VRManager.h"
#include "VRManagerParent.h"
#include "VRThread.h"
#include "VsyncBridgeParent.h"
#if defined(XP_WIN)
# include "mozilla/gfx/DeviceManagerDx.h"
# include <process.h>
# include <dwrite.h>
#endif
#ifdef MOZ_WIDGET_GTK
# include <gtk/gtk.h>
#endif
#ifdef MOZ_GECKO_PROFILER
#include "ChildProfilerController.h"
#endif

namespace mozilla {
namespace gfx {

using namespace ipc;
using namespace layers;

static GPUParent* sGPUParent;

GPUParent::GPUParent()
  : mLaunchTime(TimeStamp::Now())
{
  sGPUParent = this;
}

GPUParent::~GPUParent()
{
  sGPUParent = nullptr;
}

/* static */ GPUParent*
GPUParent::GetSingleton()
{
  return sGPUParent;
}

bool
GPUParent::Init(base::ProcessId aParentPid,
                const char* aParentBuildID,
                MessageLoop* aIOLoop,
                IPC::Channel* aChannel)
{
  // Initialize the thread manager before starting IPC. Otherwise, messages
  // may be posted to the main thread and we won't be able to process them.
  if (NS_WARN_IF(NS_FAILED(nsThreadManager::get().Init()))) {
    return false;
  }

  // Now it's safe to start IPC.
  if (NS_WARN_IF(!Open(aChannel, aParentPid, aIOLoop))) {
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

  // Init crash reporter support.
  CrashReporterClient::InitSingleton(this);

  // Ensure gfxPrefs are initialized.
  gfxPrefs::GetSingleton();
  gfxConfig::Init();
  gfxVars::Initialize();
  gfxPlatform::InitNullMetadata();
  // Ensure our Factory is initialised, mainly for gfx logging to work.
  gfxPlatform::InitMoz2DLogging();
  mlg::InitializeMemoryReporters();
#if defined(XP_WIN)
  DeviceManagerDx::Init();
#endif

  if (NS_FAILED(NS_InitMinimalXPCOM())) {
    return false;
  }

  CompositorThreadHolder::Start();
  // TODO: Bug 1406327, Start VRListenerThreadHolder when loading VR content.
  VRListenerThreadHolder::Start();
  APZThreadUtils::SetControllerThread(MessageLoop::current());
  apz::InitializeGlobalState();
  LayerTreeOwnerTracker::Initialize();
  mozilla::ipc::SetThisProcessName("GPU Process");
#ifdef XP_WIN
  wmf::MFStartup();
#endif
  return true;
}

void
GPUParent::NotifyDeviceReset()
{
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(
      NS_NewRunnableFunction("gfx::GPUParent::NotifyDeviceReset", []() -> void {
        GPUParent::GetSingleton()->NotifyDeviceReset();
      }));
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

PAPZInputBridgeParent*
GPUParent::AllocPAPZInputBridgeParent(const LayersId& aLayersId)
{
  APZInputBridgeParent* parent = new APZInputBridgeParent(aLayersId);
  parent->AddRef();
  return parent;
}

bool
GPUParent::DeallocPAPZInputBridgeParent(PAPZInputBridgeParent* aActor)
{
  APZInputBridgeParent* parent = static_cast<APZInputBridgeParent*>(aActor);
  parent->Release();
  return true;
}

mozilla::ipc::IPCResult
GPUParent::RecvInit(nsTArray<GfxPrefSetting>&& prefs,
                    nsTArray<GfxVarUpdate>&& vars,
                    const DevicePrefs& devicePrefs,
                    nsTArray<LayerTreeIdMapping>&& aMappings)
{
  const nsTArray<gfxPrefs::Pref*>& globalPrefs = gfxPrefs::all();
  for (auto& setting : prefs) {
    gfxPrefs::Pref* pref = globalPrefs[setting.index()];
    pref->SetCachedValue(setting.value());
  }
  for (const auto& var : vars) {
    gfxVars::ApplyUpdate(var);
  }

  // Inherit device preferences.
  gfxConfig::Inherit(Feature::HW_COMPOSITING, devicePrefs.hwCompositing());
  gfxConfig::Inherit(Feature::D3D11_COMPOSITING, devicePrefs.d3d11Compositing());
  gfxConfig::Inherit(Feature::OPENGL_COMPOSITING, devicePrefs.oglCompositing());
  gfxConfig::Inherit(Feature::ADVANCED_LAYERS, devicePrefs.advancedLayers());
  gfxConfig::Inherit(Feature::DIRECT2D, devicePrefs.useD2D1());

  { // Let the crash reporter know if we've got WR enabled or not. For other
    // processes this happens in gfxPlatform::InitWebRenderConfig.
    ScopedGfxFeatureReporter reporter("WR", gfxPlatform::WebRenderPrefEnabled());
    if (gfxVars::UseWebRender()) {
      reporter.SetSuccessful();
    }
  }

  for (const LayerTreeIdMapping& map : aMappings) {
    LayerTreeOwnerTracker::Get()->Map(map.layersId(), map.ownerId());
  }

#if defined(XP_WIN)
  if (gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING)) {
    DeviceManagerDx::Get()->CreateCompositorDevices();
  }
  if (gfxVars::UseWebRender()) {
    DeviceManagerDx::Get()->CreateDirectCompositionDevice();
    // Ensure to initialize GfxInfo
    nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
    Unused << gfxInfo;

    Factory::EnsureDWriteFactory();
  }
#endif

#if defined(MOZ_WIDGET_GTK)
  char* display_name = PR_GetEnv("DISPLAY");
  if (display_name) {
    int argc = 3;
    char option_name[] = "--display";
    char* argv[] = {
      // argv0 is unused because g_set_prgname() was called in
      // XRE_InitChildProcess().
      nullptr,
      option_name,
      display_name,
      nullptr
    };
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
  }
#endif

  // Make sure to do this *after* we update gfxVars above.
  if (gfxVars::UseWebRender()) {
    wr::RenderThread::Start();
  }

  VRManager::ManagerInit();
  // Send a message to the UI process that we're done.
  GPUDeviceData data;
  RecvGetDeviceStatus(&data);
  Unused << SendInitComplete(data);

  Telemetry::AccumulateTimeDelta(Telemetry::GPU_PROCESS_INITIALIZATION_TIME_MS, mLaunchTime);
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUParent::RecvInitCompositorManager(Endpoint<PCompositorManagerParent>&& aEndpoint)
{
  CompositorManagerParent::Create(std::move(aEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUParent::RecvInitVsyncBridge(Endpoint<PVsyncBridgeParent>&& aVsyncEndpoint)
{
  mVsyncBridge = VsyncBridgeParent::Start(std::move(aVsyncEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUParent::RecvInitImageBridge(Endpoint<PImageBridgeParent>&& aEndpoint)
{
  ImageBridgeParent::CreateForGPUProcess(std::move(aEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUParent::RecvInitVRManager(Endpoint<PVRManagerParent>&& aEndpoint)
{
  VRManagerParent::CreateForGPUProcess(std::move(aEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUParent::RecvInitUiCompositorController(const LayersId& aRootLayerTreeId, Endpoint<PUiCompositorControllerParent>&& aEndpoint)
{
  UiCompositorControllerParent::Start(aRootLayerTreeId, std::move(aEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUParent::RecvInitProfiler(Endpoint<PProfilerChild>&& aEndpoint)
{
#ifdef MOZ_GECKO_PROFILER
  mProfilerController = ChildProfilerController::Create(std::move(aEndpoint));
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUParent::RecvUpdatePref(const GfxPrefSetting& setting)
{
  gfxPrefs::Pref* pref = gfxPrefs::all()[setting.index()];
  pref->SetCachedValue(setting.value());
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUParent::RecvUpdateVar(const GfxVarUpdate& aUpdate)
{
  gfxVars::ApplyUpdate(aUpdate);
  return IPC_OK();
}

static void
CopyFeatureChange(Feature aFeature, FeatureChange* aOut)
{
  FeatureState& feature = gfxConfig::GetFeature(aFeature);
  if (feature.DisabledByDefault() || feature.IsEnabled()) {
    // No change:
    //   - Disabled-by-default means the parent process told us not to use this feature.
    //   - Enabled means we were told to use this feature, and we didn't discover anything
    //     that would prevent us from doing so.
    *aOut = null_t();
    return;
  }

  MOZ_ASSERT(!feature.IsEnabled());

  nsCString message;
  message.AssignASCII(feature.GetFailureMessage());

  *aOut = FeatureFailure(feature.GetValue(), message, feature.GetFailureId());
}

mozilla::ipc::IPCResult
GPUParent::RecvGetDeviceStatus(GPUDeviceData* aOut)
{
  CopyFeatureChange(Feature::D3D11_COMPOSITING, &aOut->d3d11Compositing());
  CopyFeatureChange(Feature::OPENGL_COMPOSITING, &aOut->oglCompositing());
  CopyFeatureChange(Feature::ADVANCED_LAYERS, &aOut->advancedLayers());

#if defined(XP_WIN)
  if (DeviceManagerDx* dm = DeviceManagerDx::Get()) {
    D3D11DeviceStatus deviceStatus;
    dm->ExportDeviceInfo(&deviceStatus);
    aOut->gpuDevice() = deviceStatus;
  }
#else
  aOut->gpuDevice() = null_t();
#endif

  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUParent::RecvSimulateDeviceReset(GPUDeviceData* aOut)
{
#if defined(XP_WIN)
  DeviceManagerDx::Get()->ForceDeviceReset(ForcedDeviceResetReason::COMPOSITOR_UPDATED);
  DeviceManagerDx::Get()->MaybeResetAndReacquireDevices();
#endif
  RecvGetDeviceStatus(aOut);
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUParent::RecvNewContentCompositorManager(Endpoint<PCompositorManagerParent>&& aEndpoint)
{
  CompositorManagerParent::Create(std::move(aEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUParent::RecvNewContentImageBridge(Endpoint<PImageBridgeParent>&& aEndpoint)
{
  if (!ImageBridgeParent::CreateForContent(std::move(aEndpoint))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUParent::RecvNewContentVRManager(Endpoint<PVRManagerParent>&& aEndpoint)
{
  if (!VRManagerParent::CreateForContent(std::move(aEndpoint))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUParent::RecvNewContentVideoDecoderManager(Endpoint<PVideoDecoderManagerParent>&& aEndpoint)
{
  if (!dom::VideoDecoderManagerParent::CreateForContent(std::move(aEndpoint))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUParent::RecvAddLayerTreeIdMapping(const LayerTreeIdMapping& aMapping)
{
  LayerTreeOwnerTracker::Get()->Map(aMapping.layersId(), aMapping.ownerId());
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUParent::RecvRemoveLayerTreeIdMapping(const LayerTreeIdMapping& aMapping)
{
  LayerTreeOwnerTracker::Get()->Unmap(aMapping.layersId(), aMapping.ownerId());
  CompositorBridgeParent::DeallocateLayerTreeId(aMapping.layersId());
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUParent::RecvNotifyGpuObservers(const nsCString& aTopic)
{
  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  MOZ_ASSERT(obsSvc);
  if (obsSvc) {
    obsSvc->NotifyObservers(nullptr, aTopic.get(), nullptr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUParent::RecvRequestMemoryReport(const uint32_t& aGeneration,
                                   const bool& aAnonymize,
                                   const bool& aMinimizeMemoryUsage,
                                   const MaybeFileDesc& aDMDFile)
{
  nsPrintfCString processName("GPU (pid %u)", (unsigned)getpid());

  mozilla::dom::MemoryReportRequestClient::Start(
    aGeneration, aAnonymize, aMinimizeMemoryUsage, aDMDFile, processName);
  return IPC_OK();
}

void
GPUParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (AbnormalShutdown == aWhy) {
    NS_WARNING("Shutting down GPU process early due to a crash!");
    ProcessChild::QuickExit();
  }

#ifdef XP_WIN
  wmf::MFShutdown();
#endif

#ifndef NS_FREE_PERMANENT_DATA
  // No point in going through XPCOM shutdown because we don't keep persistent
  // state.
  ProcessChild::QuickExit();
#endif

#ifdef MOZ_GECKO_PROFILER
  if (mProfilerController) {
    mProfilerController->Shutdown();
    mProfilerController = nullptr;
  }
#endif

  if (mVsyncBridge) {
    mVsyncBridge->Shutdown();
    mVsyncBridge = nullptr;
  }
  dom::VideoDecoderManagerParent::ShutdownVideoBridge();
  CompositorThreadHolder::Shutdown();
  VRListenerThreadHolder::Shutdown();
  // There is a case that RenderThread exists when gfxVars::UseWebRender() is false.
  // This could happen when WebRender was fallbacked to compositor.
  if (wr::RenderThread::Get()) {
    wr::RenderThread::ShutDown();
  }
  Factory::ShutDown();
#if defined(XP_WIN)
  DeviceManagerDx::Shutdown();
#endif
  LayerTreeOwnerTracker::Shutdown();
  gfxVars::Shutdown();
  gfxConfig::Shutdown();
  gfxPrefs::DestroySingleton();
  CrashReporterClient::DestroySingleton();
  XRE_ShutdownChildProcess();
}

} // namespace gfx
} // namespace mozilla
