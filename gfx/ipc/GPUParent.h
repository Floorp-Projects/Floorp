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
namespace gfx {

class VsyncBridgeParent;

class GPUParent final : public PGPUParent
{
public:
  GPUParent();
  ~GPUParent();

  bool Init(base::ProcessId aParentPid,
            MessageLoop* aIOLoop,
            IPC::Channel* aChannel);

  bool RecvInit(nsTArray<GfxPrefSetting>&& prefs) override;
  bool RecvInitVsyncBridge(Endpoint<PVsyncBridgeParent>&& aVsyncEndpoint) override;
  bool RecvInitImageBridge(Endpoint<PImageBridgeParent>&& aEndpoint) override;
  bool RecvInitVRManager(Endpoint<PVRManagerParent>&& aEndpoint) override;
  bool RecvUpdatePref(const GfxPrefSetting& pref) override;
  bool RecvNewWidgetCompositor(
    Endpoint<PCompositorBridgeParent>&& aEndpoint,
    const CSSToLayoutDeviceScale& aScale,
    const TimeDuration& aVsyncRate,
    const bool& aUseExternalSurface,
    const IntSize& aSurfaceSize) override;
  bool RecvNewContentCompositorBridge(Endpoint<PCompositorBridgeParent>&& aEndpoint) override;
  bool RecvNewContentImageBridge(Endpoint<PImageBridgeParent>&& aEndpoint) override;
  bool RecvNewContentVRManager(Endpoint<PVRManagerParent>&& aEndpoint) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

private:
  RefPtr<VsyncBridgeParent> mVsyncBridge;
};

} // namespace gfx
} // namespace mozilla

#endif // _include_gfx_ipc_GPUParent_h__
