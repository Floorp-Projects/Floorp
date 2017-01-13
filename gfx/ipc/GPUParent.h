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
                                   const DevicePrefs& devicePrefs) override;
  mozilla::ipc::IPCResult RecvInitVsyncBridge(Endpoint<PVsyncBridgeParent>&& aVsyncEndpoint) override;
  mozilla::ipc::IPCResult RecvInitImageBridge(Endpoint<PImageBridgeParent>&& aEndpoint) override;
  mozilla::ipc::IPCResult RecvInitVRManager(Endpoint<PVRManagerParent>&& aEndpoint) override;
  mozilla::ipc::IPCResult RecvUpdatePref(const GfxPrefSetting& pref) override;
  mozilla::ipc::IPCResult RecvUpdateVar(const GfxVarUpdate& pref) override;
  mozilla::ipc::IPCResult RecvNewWidgetCompositor(
      Endpoint<PCompositorBridgeParent>&& aEndpoint,
      const CSSToLayoutDeviceScale& aScale,
      const TimeDuration& aVsyncRate,
      const CompositorOptions& aOptions,
      const bool& aUseExternalSurface,
      const IntSize& aSurfaceSize) override;
  mozilla::ipc::IPCResult RecvNewContentCompositorBridge(Endpoint<PCompositorBridgeParent>&& aEndpoint) override;
  mozilla::ipc::IPCResult RecvNewContentImageBridge(Endpoint<PImageBridgeParent>&& aEndpoint) override;
  mozilla::ipc::IPCResult RecvNewContentVRManager(Endpoint<PVRManagerParent>&& aEndpoint) override;
  mozilla::ipc::IPCResult RecvNewContentVideoDecoderManager(Endpoint<PVideoDecoderManagerParent>&& aEndpoint) override;
  mozilla::ipc::IPCResult RecvGetDeviceStatus(GPUDeviceData* aOutStatus) override;
  mozilla::ipc::IPCResult RecvAddLayerTreeIdMapping(nsTArray<LayerTreeIdMapping>&& aMappings) override;
  mozilla::ipc::IPCResult RecvRemoveLayerTreeIdMapping(const LayerTreeIdMapping& aMapping) override;
  mozilla::ipc::IPCResult RecvNotifyGpuObservers(const nsCString& aTopic) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

private:
  const TimeStamp mLaunchTime;
  RefPtr<VsyncBridgeParent> mVsyncBridge;
};

} // namespace gfx
} // namespace mozilla

#endif // _include_gfx_ipc_GPUParent_h__
