/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_mozilla_gfx_ipc_GPUChild_h_
#define _include_mozilla_gfx_ipc_GPUChild_h_

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/ipc/CrashReporterHelper.h"
#include "mozilla/gfx/PGPUChild.h"
#include "mozilla/gfx/gfxVarReceiver.h"

namespace mozilla {

namespace dom {
class MemoryReportRequestHost;
}  // namespace dom
namespace gfx {

class GPUProcessHost;

class GPUChild final : public ipc::CrashReporterHelper<GeckoProcessType_GPU>,
                       public PGPUChild,
                       public gfxVarReceiver {
  typedef mozilla::dom::MemoryReportRequestHost MemoryReportRequestHost;

 public:
  NS_INLINE_DECL_REFCOUNTING(GPUChild, final)

  explicit GPUChild(GPUProcessHost* aHost);

  void Init();

  bool EnsureGPUReady();
  void MarkWaitForVarUpdate() { mWaitForVarUpdate = true; }

  // Notifies that an unexpected GPU process shutdown has been noticed by a
  // different IPDL actor, and the GPU process is being torn down as a result.
  // ActorDestroy may receive either NormalShutdown or AbnormalShutdown as a
  // reason, depending on timings, but either way we treat the shutdown as
  // abnormal.
  void OnUnexpectedShutdown();

  // gfxVarReceiver overrides.
  void OnVarChanged(const GfxVarUpdate& aVar) override;

  // PGPUChild overrides.
  mozilla::ipc::IPCResult RecvInitComplete(const GPUDeviceData& aData);
  mozilla::ipc::IPCResult RecvDeclareStable();
  mozilla::ipc::IPCResult RecvReportCheckerboard(const uint32_t& aSeverity,
                                                 const nsCString& aLog);
  mozilla::ipc::IPCResult RecvCreateVRProcess();
  mozilla::ipc::IPCResult RecvShutdownVRProcess();

  mozilla::ipc::IPCResult RecvAccumulateChildHistograms(
      nsTArray<HistogramAccumulation>&& aAccumulations);
  mozilla::ipc::IPCResult RecvAccumulateChildKeyedHistograms(
      nsTArray<KeyedHistogramAccumulation>&& aAccumulations);
  mozilla::ipc::IPCResult RecvUpdateChildScalars(
      nsTArray<ScalarAction>&& aScalarActions);
  mozilla::ipc::IPCResult RecvUpdateChildKeyedScalars(
      nsTArray<KeyedScalarAction>&& aScalarActions);
  mozilla::ipc::IPCResult RecvRecordChildEvents(
      nsTArray<ChildEventData>&& events);
  mozilla::ipc::IPCResult RecvRecordDiscardedData(
      const DiscardedData& aDiscardedData);

  void ActorDestroy(ActorDestroyReason aWhy) override;
  mozilla::ipc::IPCResult RecvGraphicsError(const nsCString& aError);
  mozilla::ipc::IPCResult RecvNotifyUiObservers(const nsCString& aTopic);
  mozilla::ipc::IPCResult RecvNotifyDeviceReset(const GPUDeviceData& aData);
  mozilla::ipc::IPCResult RecvNotifyOverlayInfo(const OverlayInfo aInfo);
  mozilla::ipc::IPCResult RecvNotifySwapChainInfo(const SwapChainInfo aInfo);
  mozilla::ipc::IPCResult RecvNotifyDisableRemoteCanvas();
  mozilla::ipc::IPCResult RecvFlushMemory(const nsString& aReason);
  mozilla::ipc::IPCResult RecvAddMemoryReport(const MemoryReport& aReport);
  mozilla::ipc::IPCResult RecvUpdateFeature(const Feature& aFeature,
                                            const FeatureFailure& aChange);
  mozilla::ipc::IPCResult RecvUsedFallback(const Fallback& aFallback,
                                           const nsCString& aMessage);
  mozilla::ipc::IPCResult RecvBHRThreadHang(const HangDetails& aDetails);
  mozilla::ipc::IPCResult RecvUpdateMediaCodecsSupported(
      const media::MediaCodecsSupported& aSupported);
  mozilla::ipc::IPCResult RecvFOGData(ByteBuf&& aBuf);

  bool SendRequestMemoryReport(const uint32_t& aGeneration,
                               const bool& aAnonymize,
                               const bool& aMinimizeMemoryUsage,
                               const Maybe<ipc::FileDescriptor>& aDMDFile);

  static void Destroy(RefPtr<GPUChild>&& aChild);

 private:
  virtual ~GPUChild();

  GPUProcessHost* mHost;
  UniquePtr<MemoryReportRequestHost> mMemoryReportRequest;
  bool mGPUReady;
  bool mWaitForVarUpdate = false;
  bool mUnexpectedShutdown = false;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // _include_mozilla_gfx_ipc_GPUChild_h_
