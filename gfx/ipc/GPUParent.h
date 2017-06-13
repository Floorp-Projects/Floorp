/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_gfx_ipc_GPUParent_h__
#define _include_gfx_ipc_GPUParent_h__

#include "mozilla/RefPtr.h"
#include "mozilla/gfx/PGPUParent.h"

namespace mozilla {

class TimeStamp;
class ChildProfilerController;

namespace gfx {

class VsyncBridgeParent;

class GPUParent final : public PGPUParent
{
public:
  GPUParent();
  ~GPUParent();

  static GPUParent* GetSingleton();

  bool Init(base::ProcessId aParentPid,
            MessageLoop* aIOLoop,
            IPC::Channel* aChannel);
  void NotifyDeviceReset();

  mozilla::ipc::IPCResult RecvInit(nsTArray<GfxPrefSetting>&& prefs,
                                   nsTArray<GfxVarUpdate>&& vars,
                                   const DevicePrefs& devicePrefs,
                                   nsTArray<LayerTreeIdMapping>&& mappings) override;
  mozilla::ipc::IPCResult RecvInitCompositorManager(Endpoint<PCompositorManagerParent>&& aEndpoint) override;
  mozilla::ipc::IPCResult RecvInitVsyncBridge(Endpoint<PVsyncBridgeParent>&& aVsyncEndpoint) override;
  mozilla::ipc::IPCResult RecvInitImageBridge(Endpoint<PImageBridgeParent>&& aEndpoint) override;
  mozilla::ipc::IPCResult RecvInitVRManager(Endpoint<PVRManagerParent>&& aEndpoint) override;
  mozilla::ipc::IPCResult RecvInitUiCompositorController(const uint64_t& aRootLayerTreeId, Endpoint<PUiCompositorControllerParent>&& aEndpoint) override;
  mozilla::ipc::IPCResult RecvInitProfiler(Endpoint<PProfilerChild>&& aEndpoint) override;
  mozilla::ipc::IPCResult RecvUpdatePref(const GfxPrefSetting& pref) override;
  mozilla::ipc::IPCResult RecvUpdateVar(const GfxVarUpdate& pref) override;
  mozilla::ipc::IPCResult RecvNewContentCompositorManager(Endpoint<PCompositorManagerParent>&& aEndpoint) override;
  mozilla::ipc::IPCResult RecvNewContentImageBridge(Endpoint<PImageBridgeParent>&& aEndpoint) override;
  mozilla::ipc::IPCResult RecvNewContentVRManager(Endpoint<PVRManagerParent>&& aEndpoint) override;
  mozilla::ipc::IPCResult RecvNewContentVideoDecoderManager(Endpoint<PVideoDecoderManagerParent>&& aEndpoint) override;
  mozilla::ipc::IPCResult RecvGetDeviceStatus(GPUDeviceData* aOutStatus) override;
  mozilla::ipc::IPCResult RecvAddLayerTreeIdMapping(const LayerTreeIdMapping& aMapping) override;
  mozilla::ipc::IPCResult RecvRemoveLayerTreeIdMapping(const LayerTreeIdMapping& aMapping) override;
  mozilla::ipc::IPCResult RecvNotifyGpuObservers(const nsCString& aTopic) override;
  mozilla::ipc::IPCResult RecvRequestMemoryReport(
    const uint32_t& generation,
    const bool& anonymize,
    const bool& minimizeMemoryUsage,
    const MaybeFileDesc& DMDFile) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

private:
  const TimeStamp mLaunchTime;
  RefPtr<VsyncBridgeParent> mVsyncBridge;
#ifdef MOZ_GECKO_PROFILER
  RefPtr<ChildProfilerController> mProfilerController;
#endif
};

} // namespace gfx
} // namespace mozilla

#endif // _include_gfx_ipc_GPUParent_h__
