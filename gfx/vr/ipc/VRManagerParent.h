/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_VR_VRMANAGERPARENT_H
#define MOZILLA_GFX_VR_VRMANAGERPARENT_H

#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/layers/CompositorThread.h"  // for CompositorThreadHolder
#include "mozilla/layers/CompositableTransactionParent.h"  // need?
#include "mozilla/gfx/PVRManagerParent.h"  // for PVRManagerParent
#include "mozilla/gfx/PVRLayerParent.h"    // for PVRLayerParent
#include "mozilla/ipc/ProtocolUtils.h"     // for IToplevelProtocol
#include "mozilla/TimeStamp.h"             // for TimeStamp
#include "gfxVR.h"                         // for VRFieldOfView

namespace mozilla {
using namespace layers;
namespace gfx {

class VRManager;

class VRManagerParent final : public PVRManagerParent {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRManagerParent, final);

  friend class PVRManagerParent;

 public:
  explicit VRManagerParent(ProcessId aChildProcessId,
                           dom::ContentParentId aChildId, bool aIsContentChild);

  static already_AddRefed<VRManagerParent> CreateSameProcess();
  static bool CreateForGPUProcess(Endpoint<PVRManagerParent>&& aEndpoint);
  static bool CreateForContent(Endpoint<PVRManagerParent>&& aEndpoint,
                               dom::ContentParentId aChildId);
  static void Shutdown();

  bool IsSameProcess() const;
  bool HaveEventListener();
  bool HaveControllerListener();
  bool GetVRActiveStatus();
  bool SendReplyGamepadVibrateHaptic(const uint32_t& aPromiseID);

 protected:
  ~VRManagerParent();

  PVRLayerParent* AllocPVRLayerParent(const uint32_t& aDisplayID,
                                      const uint32_t& aGroup);
  bool DeallocPVRLayerParent(PVRLayerParent* actor);

  virtual void ActorDestroy(ActorDestroyReason why) override;

  mozilla::ipc::IPCResult RecvDetectRuntimes();
  mozilla::ipc::IPCResult RecvRefreshDisplays();
  mozilla::ipc::IPCResult RecvSetGroupMask(const uint32_t& aDisplayID,
                                           const uint32_t& aGroupMask);
  mozilla::ipc::IPCResult RecvSetHaveEventListener(
      const bool& aHaveEventListener);
  mozilla::ipc::IPCResult RecvControllerListenerAdded();
  mozilla::ipc::IPCResult RecvControllerListenerRemoved();
  mozilla::ipc::IPCResult RecvVibrateHaptic(
      const mozilla::dom::GamepadHandle& aGamepadHandle,
      const uint32_t& aHapticIndex, const double& aIntensity,
      const double& aDuration, const uint32_t& aPromiseID);
  mozilla::ipc::IPCResult RecvStopVibrateHaptic(
      const mozilla::dom::GamepadHandle& aGamepadHandle);
  mozilla::ipc::IPCResult RecvStartVRNavigation(const uint32_t& aDeviceID);
  mozilla::ipc::IPCResult RecvStopVRNavigation(const uint32_t& aDeviceID,
                                               const TimeDuration& aTimeout);
  mozilla::ipc::IPCResult RecvStartActivity();
  mozilla::ipc::IPCResult RecvStopActivity();

  mozilla::ipc::IPCResult RecvRunPuppet(const nsTArray<uint64_t>& aBuffer);
  mozilla::ipc::IPCResult RecvResetPuppet();

 private:
  void RegisterWithManager();
  void UnregisterFromManager();

  void Bind(Endpoint<PVRManagerParent>&& aEndpoint);

  static void RegisterVRManagerInCompositorThread(VRManagerParent* aVRManager);

  // Keep the compositor thread alive, until we have destroyed ourselves.
  RefPtr<CompositorThreadHolder> mCompositorThreadHolder;

  // Keep the VRManager alive, until we have destroyed ourselves.
  RefPtr<VRManager> mVRManagerHolder;
  dom::ContentParentId mChildId;
  bool mHaveEventListener;
  bool mHaveControllerListener;
  bool mIsContentChild;

  // When VR tabs are switched the background, we won't need to
  // initialize its session in VRService thread.
  bool mVRActiveStatus;
};

class VRManagerPromise final {
  friend class VRManager;

 public:
  explicit VRManagerPromise(RefPtr<VRManagerParent> aParent,
                            uint32_t aPromiseID)
      : mParent(aParent), mPromiseID(aPromiseID) {}
  ~VRManagerPromise() { mParent = nullptr; }
  bool operator==(const VRManagerPromise& aOther) const {
    return mParent == aOther.mParent && mPromiseID == aOther.mPromiseID;
  }

 private:
  RefPtr<VRManagerParent> mParent;
  uint32_t mPromiseID;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // MOZILLA_GFX_VR_VRMANAGERPARENT_H
