/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GPUChild.h"
#include "gfxConfig.h"
#include "gfxPrefs.h"
#include "GPUProcessHost.h"
#include "GPUProcessManager.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryIPC.h"
#include "mozilla/dom/CheckerboardReportService.h"
#include "mozilla/dom/MemoryReportRequest.h"
#include "mozilla/gfx/gfxVars.h"
#if defined(XP_WIN)
# include "mozilla/gfx/DeviceManagerDx.h"
#endif
#include "mozilla/ipc/CrashReporterHost.h"
#include "mozilla/layers/LayerTreeOwnerTracker.h"
#include "mozilla/Unused.h"
#include "mozilla/HangDetails.h"
#include "nsIObserverService.h"

#ifdef MOZ_GECKO_PROFILER
#include "ProfilerParent.h"
#endif

namespace mozilla {
namespace gfx {

using namespace layers;

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
  devicePrefs.oglCompositing() = gfxConfig::GetValue(Feature::OPENGL_COMPOSITING);
  devicePrefs.advancedLayers() = gfxConfig::GetValue(Feature::ADVANCED_LAYERS);
  devicePrefs.useD2D1() = gfxConfig::GetValue(Feature::DIRECT2D);

  nsTArray<LayerTreeIdMapping> mappings;
  LayerTreeOwnerTracker::Get()->Iterate([&](uint64_t aLayersId, base::ProcessId aProcessId) {
    mappings.AppendElement(LayerTreeIdMapping(aLayersId, aProcessId));
  });

  SendInit(prefs, updates, devicePrefs, mappings);

  gfxVars::AddReceiver(this);

#ifdef MOZ_GECKO_PROFILER
  Unused << SendInitProfiler(ProfilerParent::CreateForProcess(OtherPid()));
#endif
}

void
GPUChild::OnVarChanged(const GfxVarUpdate& aVar)
{
  SendUpdateVar(aVar);
}

bool
GPUChild::EnsureGPUReady()
{
  if (mGPUReady) {
    return true;
  }

  GPUDeviceData data;
  if (!SendGetDeviceStatus(&data)) {
    return false;
  }

  gfxPlatform::GetPlatform()->ImportGPUDeviceData(data);
  Telemetry::AccumulateTimeDelta(Telemetry::GPU_PROCESS_LAUNCH_TIME_MS_2, mHost->GetLaunchTime());
  mGPUReady = true;
  return true;
}

mozilla::ipc::IPCResult
GPUChild::RecvInitComplete(const GPUDeviceData& aData)
{
  // We synchronously requested GPU parameters before this arrived.
  if (mGPUReady) {
    return IPC_OK();
  }

  gfxPlatform::GetPlatform()->ImportGPUDeviceData(aData);
  Telemetry::AccumulateTimeDelta(Telemetry::GPU_PROCESS_LAUNCH_TIME_MS_2, mHost->GetLaunchTime());
  mGPUReady = true;
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUChild::RecvReportCheckerboard(const uint32_t& aSeverity, const nsCString& aLog)
{
  layers::CheckerboardEventStorage::Report(aSeverity, std::string(aLog.get()));
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUChild::RecvGraphicsError(const nsCString& aError)
{
  gfx::LogForwarder* lf = gfx::Factory::GetLogForwarder();
  if (lf) {
    std::stringstream message;
    message << "GP+" << aError.get();
    lf->UpdateStringsVector(message.str());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUChild::RecvInitCrashReporter(Shmem&& aShmem, const NativeThreadId& aThreadId)
{
  mCrashReporter = MakeUnique<ipc::CrashReporterHost>(
    GeckoProcessType_GPU,
    aShmem,
    aThreadId);

  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUChild::RecvNotifyUiObservers(const nsCString& aTopic)
{
  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  MOZ_ASSERT(obsSvc);
  if (obsSvc) {
    obsSvc->NotifyObservers(nullptr, aTopic.get(), nullptr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUChild::RecvAccumulateChildHistograms(InfallibleTArray<HistogramAccumulation>&& aAccumulations)
{
  TelemetryIPC::AccumulateChildHistograms(Telemetry::ProcessID::Gpu, aAccumulations);
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUChild::RecvAccumulateChildKeyedHistograms(InfallibleTArray<KeyedHistogramAccumulation>&& aAccumulations)
{
  TelemetryIPC::AccumulateChildKeyedHistograms(Telemetry::ProcessID::Gpu, aAccumulations);
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUChild::RecvUpdateChildScalars(InfallibleTArray<ScalarAction>&& aScalarActions)
{
  TelemetryIPC::UpdateChildScalars(Telemetry::ProcessID::Gpu, aScalarActions);
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUChild::RecvUpdateChildKeyedScalars(InfallibleTArray<KeyedScalarAction>&& aScalarActions)
{
  TelemetryIPC::UpdateChildKeyedScalars(Telemetry::ProcessID::Gpu, aScalarActions);
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUChild::RecvRecordChildEvents(nsTArray<mozilla::Telemetry::ChildEventData>&& aEvents)
{
  TelemetryIPC::RecordChildEvents(Telemetry::ProcessID::Gpu, aEvents);
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUChild::RecvRecordDiscardedData(const mozilla::Telemetry::DiscardedData& aDiscardedData)
{
  TelemetryIPC::RecordDiscardedData(Telemetry::ProcessID::Gpu, aDiscardedData);
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUChild::RecvNotifyDeviceReset(const GPUDeviceData& aData)
{
  gfxPlatform::GetPlatform()->ImportGPUDeviceData(aData);
  mHost->mListener->OnRemoteProcessDeviceReset(mHost);
  return IPC_OK();
}

bool
GPUChild::SendRequestMemoryReport(const uint32_t& aGeneration,
                                  const bool& aAnonymize,
                                  const bool& aMinimizeMemoryUsage,
                                  const MaybeFileDesc& aDMDFile)
{
  mMemoryReportRequest = MakeUnique<MemoryReportRequestHost>(aGeneration);
  Unused << PGPUChild::SendRequestMemoryReport(
    aGeneration,
    aAnonymize,
    aMinimizeMemoryUsage,
    aDMDFile);
  return true;
}

mozilla::ipc::IPCResult
GPUChild::RecvAddMemoryReport(const MemoryReport& aReport)
{
  if (mMemoryReportRequest) {
    mMemoryReportRequest->RecvReport(aReport);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUChild::RecvFinishMemoryReport(const uint32_t& aGeneration)
{
  if (mMemoryReportRequest) {
    mMemoryReportRequest->Finish(aGeneration);
    mMemoryReportRequest = nullptr;
  }
  return IPC_OK();
}

void
GPUChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (aWhy == AbnormalShutdown) {
    if (mCrashReporter) {
      mCrashReporter->GenerateCrashReport(OtherPid());
      mCrashReporter = nullptr;
    }

    Telemetry::Accumulate(Telemetry::SUBPROCESS_ABNORMAL_ABORT,
        nsDependentCString(XRE_ChildProcessTypeToString(GeckoProcessType_GPU)), 1);

    // Notify the Telemetry environment so that we can refresh and do a subsession split
    if (nsCOMPtr<nsIObserverService> obsvc = services::GetObserverService()) {
      obsvc->NotifyObservers(nullptr, "compositor:process-aborted", nullptr);
    }

  }

  gfxVars::RemoveReceiver(this);
  mHost->OnChannelClosed();
}

mozilla::ipc::IPCResult
GPUChild::RecvUpdateFeature(const Feature& aFeature, const FeatureFailure& aChange)
{
  gfxConfig::SetFailed(aFeature, aChange.status(), aChange.message().get(), aChange.failureId());
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUChild::RecvUsedFallback(const Fallback& aFallback, const nsCString& aMessage)
{
  gfxConfig::EnableFallback(aFallback, aMessage.get());
  return IPC_OK();
}

mozilla::ipc::IPCResult
GPUChild::RecvBHRThreadHang(const HangDetails& aDetails)
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    // Copy the HangDetails recieved over the network into a nsIHangDetails, and
    // then fire our own observer notification.
    // XXX: We should be able to avoid this potentially expensive copy here by
    // moving our deserialized argument.
    nsCOMPtr<nsIHangDetails> hangDetails =
      new nsHangDetails(HangDetails(aDetails));
    obs->NotifyObservers(hangDetails, "bhr-thread-hang", nullptr);
  }
  return IPC_OK();
}

class DeferredDeleteGPUChild : public Runnable
{
public:
  explicit DeferredDeleteGPUChild(UniquePtr<GPUChild>&& aChild)
    : Runnable("gfx::DeferredDeleteGPUChild")
    , mChild(Move(aChild))
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
