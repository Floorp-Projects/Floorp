/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_mozilla_gfx_ipc_GPUChild_h_
#define _include_mozilla_gfx_ipc_GPUChild_h_

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/gfx/PGPUChild.h"
#include "mozilla/gfx/gfxVarReceiver.h"

namespace mozilla {

namespace ipc {
class CrashReporterHost;
} // namespace ipc
namespace dom {
class MemoryReportRequestHost;
} // namespace dom
namespace gfx {

class GPUProcessHost;

class GPUChild final
  : public PGPUChild
  , public gfxVarReceiver
{
  typedef mozilla::dom::MemoryReportRequestHost MemoryReportRequestHost;

public:
  explicit GPUChild(GPUProcessHost* aHost);
  ~GPUChild();

  void Init();

  bool EnsureGPUReady();

  // gfxVarReceiver overrides.
  void OnVarChanged(const GfxVarUpdate& aVar) override;

  // PGPUChild overrides.
  mozilla::ipc::IPCResult RecvInitComplete(const GPUDeviceData& aData) override;
  mozilla::ipc::IPCResult RecvReportCheckerboard(const uint32_t& aSeverity, const nsCString& aLog) override;
  mozilla::ipc::IPCResult RecvInitCrashReporter(Shmem&& shmem, const NativeThreadId& aThreadId) override;

  mozilla::ipc::IPCResult RecvAccumulateChildHistograms(InfallibleTArray<Accumulation>&& aAccumulations) override;
  mozilla::ipc::IPCResult RecvAccumulateChildKeyedHistograms(InfallibleTArray<KeyedAccumulation>&& aAccumulations) override;
  mozilla::ipc::IPCResult RecvUpdateChildScalars(InfallibleTArray<ScalarAction>&& aScalarActions) override;
  mozilla::ipc::IPCResult RecvUpdateChildKeyedScalars(InfallibleTArray<KeyedScalarAction>&& aScalarActions) override;
  mozilla::ipc::IPCResult RecvRecordChildEvents(nsTArray<ChildEventData>&& events) override;
  mozilla::ipc::IPCResult RecvRecordDiscardedData(const DiscardedData& aDiscardedData) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;
  mozilla::ipc::IPCResult RecvGraphicsError(const nsCString& aError) override;
  mozilla::ipc::IPCResult RecvNotifyUiObservers(const nsCString& aTopic) override;
  mozilla::ipc::IPCResult RecvNotifyDeviceReset(const GPUDeviceData& aData) override;
  mozilla::ipc::IPCResult RecvAddMemoryReport(const MemoryReport& aReport) override;
  mozilla::ipc::IPCResult RecvFinishMemoryReport(const uint32_t& aGeneration) override;
  mozilla::ipc::IPCResult RecvUpdateFeature(const Feature& aFeature, const FeatureFailure& aChange) override;

  bool SendRequestMemoryReport(const uint32_t& aGeneration,
                               const bool& aAnonymize,
                               const bool& aMinimizeMemoryUsage,
                               const MaybeFileDesc& aDMDFile);

  static void Destroy(UniquePtr<GPUChild>&& aChild);

private:
  GPUProcessHost* mHost;
  UniquePtr<ipc::CrashReporterHost> mCrashReporter;
  UniquePtr<MemoryReportRequestHost> mMemoryReportRequest;
  bool mGPUReady;
};

} // namespace gfx
} // namespace mozilla

#endif // _include_mozilla_gfx_ipc_GPUChild_h_
