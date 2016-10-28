/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifdef XP_WIN
#include "WMF.h"
#endif
#include "GPUParent.h"
#include "gfxConfig.h"
#include "gfxPlatform.h"
#include "gfxPrefs.h"
#include "GPUProcessHost.h"
#include "mozilla/Assertions.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/ipc/CrashReporterClient.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/dom/VideoDecoderManagerParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/LayerTreeOwnerTracker.h"
#include "nsDebugImpl.h"
#include "nsExceptionHandler.h"
#include "nsThreadManager.h"
#include "prenv.h"
#include "ProcessUtils.h"
#include "VRManager.h"
#include "VRManagerParent.h"
#include "VsyncBridgeParent.h"
#if defined(XP_WIN)
# include "DeviceManagerD3D9.h"
# include "mozilla/gfx/DeviceManagerDx.h"
#endif
#ifdef MOZ_WIDGET_GTK
# include <gtk/gtk.h>
#endif

namespace mozilla {
namespace gfx {

using namespace ipc;
using namespace layers;

static GPUParent* sGPUParent;

GPUParent::GPUParent()
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

#ifdef MOZ_CRASHREPORTER
  // Init crash reporter support.
  CrashReporterClient::InitSingleton(this);
#endif

  // Ensure gfxPrefs are initialized.
  gfxPrefs::GetSingleton();
  gfxConfig::Init();
  gfxVars::Initialize();
  gfxPlatform::InitNullMetadata();
  // Ensure our Factory is initialised, mainly for gfx logging to work.
  gfxPlatform::InitMoz2D();
#if defined(XP_WIN)
  DeviceManagerDx::Init();
  DeviceManagerD3D9::Init();
#endif
  if (NS_FAILED(NS_InitMinimalXPCOM())) {
    return false;
  }
  CompositorThreadHolder::Start();
  APZThreadUtils::SetControllerThread(CompositorThreadHolder::Loop());
  APZCTreeManager::InitializeGlobalState();
  VRManager::ManagerInit();
  LayerTreeOwnerTracker::Initialize();
  mozilla::ipc::SetThisProcessName("GPU Process");
#ifdef XP_WIN
  wmf::MFStartup();
#endif
  return true;
}

bool
GPUParent::RecvInit(nsTArray<GfxPrefSetting>&& prefs,
                    nsTArray<GfxVarUpdate>&& vars,
                    const DevicePrefs& devicePrefs)
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
  gfxConfig::Inherit(Feature::D3D9_COMPOSITING, devicePrefs.d3d9Compositing());
  gfxConfig::Inherit(Feature::OPENGL_COMPOSITING, devicePrefs.oglCompositing());
  gfxConfig::Inherit(Feature::DIRECT2D, devicePrefs.useD2D1());

#if defined(XP_WIN)
  if (gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING)) {
    DeviceManagerDx::Get()->CreateCompositorDevices();
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
#endif

  // Send a message to the UI process that we're done.
  GPUDeviceData data;
  RecvGetDeviceStatus(&data);
  Unused << SendInitComplete(data);

  return true;
}

bool
GPUParent::RecvInitVsyncBridge(Endpoint<PVsyncBridgeParent>&& aVsyncEndpoint)
{
  mVsyncBridge = VsyncBridgeParent::Start(Move(aVsyncEndpoint));
  return true;
}

bool
GPUParent::RecvInitImageBridge(Endpoint<PImageBridgeParent>&& aEndpoint)
{
  ImageBridgeParent::CreateForGPUProcess(Move(aEndpoint));
  return true;
}

bool
GPUParent::RecvInitVRManager(Endpoint<PVRManagerParent>&& aEndpoint)
{
  VRManagerParent::CreateForGPUProcess(Move(aEndpoint));
  return true;
}

bool
GPUParent::RecvUpdatePref(const GfxPrefSetting& setting)
{
  gfxPrefs::Pref* pref = gfxPrefs::all()[setting.index()];
  pref->SetCachedValue(setting.value());
  return true;
}

bool
GPUParent::RecvUpdateVar(const GfxVarUpdate& aUpdate)
{
  gfxVars::ApplyUpdate(aUpdate);
  return true;
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

bool
GPUParent::RecvGetDeviceStatus(GPUDeviceData* aOut)
{
  CopyFeatureChange(Feature::D3D11_COMPOSITING, &aOut->d3d11Compositing());
  CopyFeatureChange(Feature::D3D9_COMPOSITING, &aOut->d3d9Compositing());
  CopyFeatureChange(Feature::OPENGL_COMPOSITING, &aOut->oglCompositing());

#if defined(XP_WIN)
  if (DeviceManagerDx* dm = DeviceManagerDx::Get()) {
    D3D11DeviceStatus deviceStatus;
    dm->ExportDeviceInfo(&deviceStatus);
    aOut->gpuDevice() = deviceStatus;
  }
#else
  aOut->gpuDevice() = null_t();
#endif

  return true;
}

static void
OpenParent(RefPtr<CompositorBridgeParent> aParent,
           Endpoint<PCompositorBridgeParent>&& aEndpoint)
{
  if (!aParent->Bind(Move(aEndpoint))) {
    MOZ_CRASH("Failed to bind compositor");
  }
}

bool
GPUParent::RecvNewWidgetCompositor(Endpoint<layers::PCompositorBridgeParent>&& aEndpoint,
                                   const CSSToLayoutDeviceScale& aScale,
                                   const TimeDuration& aVsyncRate,
                                   const bool& aUseExternalSurfaceSize,
                                   const IntSize& aSurfaceSize)
{
  RefPtr<CompositorBridgeParent> cbp =
    new CompositorBridgeParent(aScale, aVsyncRate, aUseExternalSurfaceSize, aSurfaceSize);

  MessageLoop* loop = CompositorThreadHolder::Loop();
  loop->PostTask(NewRunnableFunction(OpenParent, cbp, Move(aEndpoint)));
  return true;
}

bool
GPUParent::RecvNewContentCompositorBridge(Endpoint<PCompositorBridgeParent>&& aEndpoint)
{
  return CompositorBridgeParent::CreateForContent(Move(aEndpoint));
}

bool
GPUParent::RecvNewContentImageBridge(Endpoint<PImageBridgeParent>&& aEndpoint)
{
  return ImageBridgeParent::CreateForContent(Move(aEndpoint));
}

bool
GPUParent::RecvNewContentVRManager(Endpoint<PVRManagerParent>&& aEndpoint)
{
  return VRManagerParent::CreateForContent(Move(aEndpoint));
}

bool
GPUParent::RecvNewContentVideoDecoderManager(Endpoint<PVideoDecoderManagerParent>&& aEndpoint)
{
  return dom::VideoDecoderManagerParent::CreateForContent(Move(aEndpoint));
}

bool
GPUParent::RecvDeallocateLayerTreeId(const uint64_t& aLayersId)
{
  CompositorBridgeParent::DeallocateLayerTreeId(aLayersId);
  return true;
}

bool
GPUParent::RecvAddLayerTreeIdMapping(const uint64_t& aLayersId, const ProcessId& aOwnerId)
{
  LayerTreeOwnerTracker::Get()->Map(aLayersId, aOwnerId);
  return true;
}

bool
GPUParent::RecvNotifyGpuObservers(const nsCString& aTopic)
{
  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  MOZ_ASSERT(obsSvc);
  if (obsSvc) {
    obsSvc->NotifyObservers(nullptr, aTopic.get(), nullptr);
  }
  return true;
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

  if (mVsyncBridge) {
    mVsyncBridge->Shutdown();
    mVsyncBridge = nullptr;
  }
  CompositorThreadHolder::Shutdown();
  Factory::ShutDown();
#if defined(XP_WIN)
  DeviceManagerDx::Shutdown();
  DeviceManagerD3D9::Shutdown();
#endif
  LayerTreeOwnerTracker::Shutdown();
  gfxVars::Shutdown();
  gfxConfig::Shutdown();
  gfxPrefs::DestroySingleton();
#ifdef MOZ_CRASHREPORTER
  CrashReporterClient::DestroySingleton();
#endif
  XRE_ShutdownChildProcess();
}

} // namespace gfx
} // namespace mozilla
