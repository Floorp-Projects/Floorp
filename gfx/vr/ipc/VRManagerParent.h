/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_VR_VRMANAGERPARENT_H
#define MOZILLA_GFX_VR_VRMANAGERPARENT_H

#include "mozilla/layers/CompositableTransactionParent.h"  // need?
#include "mozilla/gfx/PVRManagerParent.h" // for PVRManagerParent
#include "mozilla/gfx/PVRLayerParent.h"   // for PVRLayerParent
#include "mozilla/ipc/ProtocolUtils.h"    // for IToplevelProtocol
#include "mozilla/TimeStamp.h"            // for TimeStamp
#include "gfxVR.h"                        // for VRFieldOfView
#include "VRThread.h"                     // for VRListenerThreadHolder

namespace mozilla {
using namespace layers;
namespace gfx {

class VRManager;

namespace impl {
class VRDisplayPuppet;
class VRControllerPuppet;
} // namespace impl

class VRManagerParent final : public PVRManagerParent
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRManagerParent);
public:
  explicit VRManagerParent(ProcessId aChildProcessId, bool aIsContentChild);

  static VRManagerParent* CreateSameProcess();
  static bool CreateForGPUProcess(Endpoint<PVRManagerParent>&& aEndpoint);
  static bool CreateForContent(Endpoint<PVRManagerParent>&& aEndpoint);

  bool IsSameProcess() const;
  bool HaveEventListener();
  bool HaveControllerListener();
  bool SendGamepadUpdate(const GamepadChangeEvent& aGamepadEvent);
  bool SendReplyGamepadVibrateHaptic(const uint32_t& aPromiseID);

protected:
  ~VRManagerParent();

  virtual PVRLayerParent* AllocPVRLayerParent(const uint32_t& aDisplayID,
                                              const uint32_t& aGroup) override;
  virtual bool DeallocPVRLayerParent(PVRLayerParent* actor) override;

  virtual void ActorDestroy(ActorDestroyReason why) override;
  void OnChannelConnected(int32_t pid) override;

  virtual mozilla::ipc::IPCResult RecvRefreshDisplays() override;
  virtual mozilla::ipc::IPCResult RecvResetSensor(const uint32_t& aDisplayID) override;
  virtual mozilla::ipc::IPCResult RecvSetGroupMask(const uint32_t& aDisplayID, const uint32_t& aGroupMask) override;
  virtual mozilla::ipc::IPCResult RecvSetHaveEventListener(const bool& aHaveEventListener) override;
  virtual mozilla::ipc::IPCResult RecvControllerListenerAdded() override;
  virtual mozilla::ipc::IPCResult RecvControllerListenerRemoved() override;
  virtual mozilla::ipc::IPCResult RecvVibrateHaptic(const uint32_t& aControllerIdx, const uint32_t& aHapticIndex,
                                                    const double& aIntensity, const double& aDuration, const uint32_t& aPromiseID) override;
  virtual mozilla::ipc::IPCResult RecvStopVibrateHaptic(const uint32_t& aControllerIdx) override;
  virtual mozilla::ipc::IPCResult RecvCreateVRTestSystem() override;
  virtual mozilla::ipc::IPCResult RecvCreateVRServiceTestDisplay(const nsCString& aID, const uint32_t& aPromiseID) override;
  virtual mozilla::ipc::IPCResult RecvCreateVRServiceTestController(const nsCString& aID, const uint32_t& aPromiseID) override;
  virtual mozilla::ipc::IPCResult RecvSetDisplayInfoToMockDisplay(const uint32_t& aDeviceID,
                                                                  const VRDisplayInfo& aDisplayInfo) override;
  virtual mozilla::ipc::IPCResult RecvSetSensorStateToMockDisplay(const uint32_t& aDeviceID,
                                                                  const VRHMDSensorState& aSensorState) override;
  virtual mozilla::ipc::IPCResult RecvNewButtonEventToMockController(const uint32_t& aDeviceID, const long& aButton,
                                                                     const bool& aPressed) override;
  virtual mozilla::ipc::IPCResult RecvNewAxisMoveEventToMockController(const uint32_t& aDeviceID, const long& aAxis,
                                                                       const double& aValue) override;
  virtual mozilla::ipc::IPCResult RecvNewPoseMoveToMockController(const uint32_t& aDeviceID, const GamepadPoseState& pose) override;

private:
  void RegisterWithManager();
  void UnregisterFromManager();

  void Bind(Endpoint<PVRManagerParent>&& aEndpoint);

  static void RegisterVRManagerInVRListenerThread(VRManagerParent* aVRManager);

  void DeferredDestroy();

  // This keeps us alive until ActorDestroy(), at which point we do a
  // deferred destruction of ourselves.
  RefPtr<VRManagerParent> mSelfRef;
  RefPtr<VRListenerThreadHolder> mVRListenerThreadHolder;

  // Keep the VRManager alive, until we have destroyed ourselves.
  RefPtr<VRManager> mVRManagerHolder;
  nsRefPtrHashtable<nsUint32HashKey, impl::VRDisplayPuppet> mVRDisplayTests;
  nsRefPtrHashtable<nsUint32HashKey, impl::VRControllerPuppet> mVRControllerTests;
  uint32_t mDisplayTestID;
  uint32_t mControllerTestID;
  bool mHaveEventListener;
  bool mHaveControllerListener;
  bool mIsContentChild;
};

class VRManagerPromise final
{
  friend class VRManager;

public:
  explicit VRManagerPromise(RefPtr<VRManagerParent> aParent, uint32_t aPromiseID)
  : mParent(aParent), mPromiseID(aPromiseID)
  {}
  ~VRManagerPromise() {
    mParent = nullptr;
  }

private:
  RefPtr<VRManagerParent> mParent;
  uint32_t mPromiseID;
};

} // namespace mozilla
} // namespace gfx

#endif // MOZILLA_GFX_VR_VRMANAGERPARENT_H
