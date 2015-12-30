/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_VR_VRMANAGERPARENT_H
#define MOZILLA_GFX_VR_VRMANAGERPARENT_H

#include "mozilla/layers/CompositorParent.h" // for CompositorThreadHolder
#include "mozilla/gfx/PVRManagerParent.h" // for PVRManagerParent
#include "mozilla/ipc/ProtocolUtils.h"    // for IToplevelProtocol
#include "mozilla/TimeStamp.h"            // for TimeStamp
#include "gfxVR.h"                        // for VRFieldOfView

namespace mozilla {
namespace gfx {

class VRManager;

class VRManagerParent final : public PVRManagerParent
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VRManagerParent)
public:
  VRManagerParent(MessageLoop* aLoop, Transport* aTransport, ProcessId aChildProcessId);

  static VRManagerParent* CreateCrossProcess(Transport* aTransport,
                                              ProcessId aOtherProcess);
  static VRManagerParent* CreateSameProcess();


  // Overriden from IToplevelProtocol
  ipc::IToplevelProtocol*
  CloneToplevel(const InfallibleTArray<ipc::ProtocolFdMapping>& aFds,
                base::ProcessHandle aPeerProcess,
                mozilla::ipc::ProtocolCloneContext* aCtx) override;

protected:
  ~VRManagerParent();

  virtual void ActorDestroy(ActorDestroyReason why) override;
  void OnChannelConnected(int32_t pid) override;

  virtual bool RecvRefreshDevices() override;
  virtual bool RecvResetSensor(const uint32_t& aDeviceID) override;
  virtual bool RecvKeepSensorTracking(const uint32_t& aDeviceID) override;
  virtual bool RecvSetFOV(const uint32_t& aDeviceID,
                          const VRFieldOfView& aFOVLeft,
                          const VRFieldOfView& aFOVRight,
                          const double& zNear,
                          const double& zFar) override;

private:

  void RegisterWithManager();
  void UnregisterFromManager();

  static void RegisterVRManagerInCompositorThread(VRManagerParent* aVRManager);
  static void ConnectVRManagerInParentProcess(VRManagerParent* aVRManager,
                                              ipc::Transport* aTransport,
                                              base::ProcessId aOtherPid);

  void DeferredDestroy();

  // This keeps us alive until ActorDestroy(), at which point we do a
  // deferred destruction of ourselves.
  RefPtr<VRManagerParent> mSelfRef;

  // Keep the compositor thread alive, until we have destroyed ourselves.
  RefPtr<layers::CompositorThreadHolder> mCompositorThreadHolder;

  // Keep the VRManager alive, until we have destroyed ourselves.
  RefPtr<VRManager> mVRManagerHolder;
};

} // namespace mozilla
} // namespace gfx

#endif // MOZILLA_GFX_VR_VRMANAGERPARENT_H
