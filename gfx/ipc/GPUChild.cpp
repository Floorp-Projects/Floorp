/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GPUChild.h"
#include "gfxConfig.h"
#include "gfxPrefs.h"
#include "GPUProcessHost.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/CheckerboardReportService.h"
#include "mozilla/gfx/gfxVars.h"
#if defined(XP_WIN)
# include "mozilla/gfx/DeviceManagerDx.h"
#endif
#include "mozilla/ipc/CrashReporterHost.h"

namespace mozilla {
namespace gfx {

GPUChild::GPUChild(GPUProcessHost* aHost)
 : mHost(aHost),
   mGPUReady(false)
{
  MOZ_COUNT_CTOR(GPUChild);
}

GPUChild::~GPUChild()
{
  MOZ_COUNT_DTOR(GPUChild);
}

void
GPUChild::Init()
{
  // Build a list of prefs the GPU process will need. Note that because we
  // limit the GPU process to prefs contained in gfxPrefs, we can simplify
  // the message in two ways: one, we only need to send its index in gfxPrefs
  // rather than its name, and two, we only need to send prefs that don't
  // have their default value.
  nsTArray<GfxPrefSetting> prefs;
  for (auto pref : gfxPrefs::all()) {
    if (pref->HasDefaultValue()) {
      continue;
    }

    GfxPrefValue value;
    pref->GetCachedValue(&value);
    prefs.AppendElement(GfxPrefSetting(pref->Index(), value));
  }

  nsTArray<GfxVarUpdate> updates = gfxVars::FetchNonDefaultVars();

  DevicePrefs devicePrefs;
  devicePrefs.hwCompositing() = gfxConfig::GetValue(Feature::HW_COMPOSITING);
  devicePrefs.d3d11Compositing() = gfxConfig::GetValue(Feature::D3D11_COMPOSITING);
  devicePrefs.d3d9Compositing() = gfxConfig::GetValue(Feature::D3D9_COMPOSITING);
  devicePrefs.oglCompositing() = gfxConfig::GetValue(Feature::OPENGL_COMPOSITING);
  devicePrefs.useD2D1() = gfxConfig::GetValue(Feature::DIRECT2D);

  SendInit(prefs, updates, devicePrefs);

  gfxVars::AddReceiver(this);
}

void
GPUChild::OnVarChanged(const GfxVarUpdate& aVar)
{
  SendUpdateVar(aVar);
}

void
GPUChild::EnsureGPUReady()
{
  if (mGPUReady) {
    return;
  }

  GPUDeviceData data;
  SendGetDeviceStatus(&data);

  gfxPlatform::GetPlatform()->ImportGPUDeviceData(data);
  Telemetry::AccumulateTimeDelta(Telemetry::GPU_PROCESS_LAUNCH_TIME_MS, mHost->GetLaunchTime());
  mGPUReady = true;
}

bool
GPUChild::RecvInitComplete(const GPUDeviceData& aData)
{
  // We synchronously requested GPU parameters before this arrived.
  if (mGPUReady) {
    return true;
  }

  gfxPlatform::GetPlatform()->ImportGPUDeviceData(aData);
  Telemetry::AccumulateTimeDelta(Telemetry::GPU_PROCESS_LAUNCH_TIME_MS, mHost->GetLaunchTime());
  mGPUReady = true;
  return true;
}

bool
GPUChild::RecvReportCheckerboard(const uint32_t& aSeverity, const nsCString& aLog)
{
  layers::CheckerboardEventStorage::Report(aSeverity, std::string(aLog.get()));
  return true;
}

bool
GPUChild::RecvGraphicsError(const nsCString& aError)
{
  gfx::LogForwarder* lf = gfx::Factory::GetLogForwarder();
  if (lf) {
    std::stringstream message;
    message << "GP+" << aError.get();
    lf->UpdateStringsVector(message.str());
  }
  return true;
}

bool
GPUChild::RecvInitCrashReporter(Shmem&& aShmem)
{
#ifdef MOZ_CRASHREPORTER
  mCrashReporter = MakeUnique<ipc::CrashReporterHost>(GeckoProcessType_GPU, aShmem);
#endif
  return true;
}

bool
GPUChild::RecvNotifyUiObservers(const nsCString& aTopic)
{
  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  MOZ_ASSERT(obsSvc);
  if (obsSvc) {
    obsSvc->NotifyObservers(nullptr, aTopic.get(), nullptr);
  }
  return true;
}

void
GPUChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (aWhy == AbnormalShutdown) {
#ifdef MOZ_CRASHREPORTER
    if (mCrashReporter) {
      mCrashReporter->GenerateCrashReport(OtherPid());
      mCrashReporter = nullptr;
    }
#endif
    Telemetry::Accumulate(Telemetry::SUBPROCESS_ABNORMAL_ABORT,
        nsDependentCString(XRE_ChildProcessTypeToString(GeckoProcessType_GPU), 1));
  }

  gfxVars::RemoveReceiver(this);
  mHost->OnChannelClosed();
}

class DeferredDeleteGPUChild : public Runnable
{
public:
  explicit DeferredDeleteGPUChild(UniquePtr<GPUChild>&& aChild)
    : mChild(Move(aChild))
  {
  }

  NS_IMETHODIMP Run() override {
    return NS_OK;
  }

private:
  UniquePtr<GPUChild> mChild;
};

/* static */ void
GPUChild::Destroy(UniquePtr<GPUChild>&& aChild)
{
  NS_DispatchToMainThread(new DeferredDeleteGPUChild(Move(aChild)));
}

} // namespace gfx
} // namespace mozilla
