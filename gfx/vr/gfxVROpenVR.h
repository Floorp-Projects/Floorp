/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_OPENVR_H
#define GFX_VR_OPENVR_H

#include "nsTArray.h"
#include "nsIScreen.h"
#include "nsCOMPtr.h"
#include "mozilla/RefPtr.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/EnumeratedArray.h"

#include "openvr.h"
#include "gfxVR.h"
#include "VRDisplayHost.h"

namespace mozilla {
namespace gfx {
namespace impl {

class VRDisplayOpenVR : public VRDisplayHost
{
public:
  virtual void NotifyVSync() override;
  virtual VRHMDSensorState GetSensorState() override;
  void ZeroSensor() override;

protected:
  virtual void StartPresentation() override;
  virtual void StopPresentation() override;
#if defined(XP_WIN)
  virtual void SubmitFrame(mozilla::layers::TextureSourceD3D11* aSource,
                           const IntSize& aSize,
                           const VRHMDSensorState& aSensorState,
                           const gfx::Rect& aLeftEyeRect,
                           const gfx::Rect& aRightEyeRect) override;
#endif

public:
  explicit VRDisplayOpenVR(::vr::IVRSystem *aVRSystem,
                           ::vr::IVRChaperone *aVRChaperone,
                           ::vr::IVRCompositor *aVRCompositor);

protected:
  virtual ~VRDisplayOpenVR();
  void Destroy();

  VRHMDSensorState GetSensorState(double timeOffset);

  // not owned by us; global from OpenVR
  ::vr::IVRSystem *mVRSystem;
  ::vr::IVRChaperone *mVRChaperone;
  ::vr::IVRCompositor *mVRCompositor;

  bool mIsPresenting;

  void UpdateStageParameters();
  void PollEvents();
};

class VRControllerOpenVR : public VRControllerHost
{
public:
  explicit VRControllerOpenVR(dom::GamepadHand aHand, uint32_t aNumButtons,
                              uint32_t aNumAxes, ::vr::ETrackedDeviceClass aDeviceType);
  void SetTrackedIndex(uint32_t aTrackedIndex);
  uint32_t GetTrackedIndex();
  void SetTrigger(float aValue);
  float GetTrigger();
  void SetHand(dom::GamepadHand aHand);
  void VibrateHaptic(::vr::IVRSystem* aVRSystem,
                     uint32_t aHapticIndex,
                     double aIntensity,
                     double aDuration,
                     uint32_t aPromiseID);
  void StopVibrateHaptic();

protected:
  virtual ~VRControllerOpenVR();

private:
  void UpdateVibrateHaptic(::vr::IVRSystem* aVRSystem,
                           uint32_t aHapticIndex,
                           double aIntensity,
                           double aDuration,
                           uint64_t aVibrateIndex,
                           uint32_t aPromiseID);
  void VibrateHapticComplete(uint32_t aPromiseID);

  // The index of tracked devices from ::vr::IVRSystem.
  uint32_t mTrackedIndex;
  float mTrigger;
  nsCOMPtr<nsIThread> mVibrateThread;
  Atomic<bool> mIsVibrateStopped;
};

} // namespace impl

class VRSystemManagerOpenVR : public VRSystemManager
{
public:
  static already_AddRefed<VRSystemManagerOpenVR> Create();

  virtual void Destroy() override;
  virtual void Shutdown() override;
  virtual void GetHMDs(nsTArray<RefPtr<VRDisplayHost> >& aHMDResult) override;
  virtual bool GetIsPresenting() override;
  virtual void HandleInput() override;
  virtual void GetControllers(nsTArray<RefPtr<VRControllerHost>>&
                              aControllerResult) override;
  virtual void ScanForControllers() override;
  virtual void RemoveControllers() override;
  virtual void VibrateHaptic(uint32_t aControllerIdx,
                             uint32_t aHapticIndex,
                             double aIntensity,
                             double aDuration,
                             uint32_t aPromiseID) override;
  virtual void StopVibrateHaptic(uint32_t aControllerIdx) override;

protected:
  VRSystemManagerOpenVR();

private:
  void HandleButtonPress(uint32_t aControllerIdx,
                         uint32_t aButton,
                         uint64_t aButtonMask,
                         uint64_t aButtonPressed,
                         uint64_t aButtonTouched);
  void HandleTriggerPress(uint32_t aControllerIdx,
                          uint32_t aButton,
                          float aValue);
  void HandleAxisMove(uint32_t aControllerIdx, uint32_t aAxis,
                      float aValue);
  void HandlePoseTracking(uint32_t aControllerIdx,
                          const dom::GamepadPoseState& aPose,
                          VRControllerHost* aController);
  dom::GamepadHand GetGamepadHandFromControllerRole(
                          ::vr::ETrackedControllerRole aRole);

  // there can only be one
  RefPtr<impl::VRDisplayOpenVR> mOpenVRHMD;
  nsTArray<RefPtr<impl::VRControllerOpenVR>> mOpenVRController;
  ::vr::IVRSystem *mVRSystem;
};

} // namespace gfx
} // namespace mozilla


#endif /* GFX_VR_OPENVR_H */
