/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_PUPPET_H
#define GFX_VR_PUPPET_H

#include "nsTArray.h"
#include "mozilla/RefPtr.h"

#include "gfxVR.h"

namespace mozilla {
namespace gfx {
namespace impl {

class VRDisplayPuppet : public VRDisplayHost
{
public:
  void SetDisplayInfo(const VRDisplayInfo& aDisplayInfo);
  virtual void NotifyVSync() override;
  virtual VRHMDSensorState GetSensorState() override;
  void SetSensorState(const VRHMDSensorState& aSensorState);
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
#else
  virtual void SubmitFrame(mozilla::layers::TextureSourceOGL* aSource,
                           const IntSize& aSize,
                           const VRHMDSensorState& aSensorState,
                           const gfx::Rect& aLeftEyeRect,
                           const gfx::Rect& aRightEyeRect);
#endif // XP_WIN

public:
  explicit VRDisplayPuppet();

protected:
  virtual ~VRDisplayPuppet();
  void Destroy();

  VRHMDSensorState GetSensorState(double timeOffset);

  bool mIsPresenting;

private:
  VRHMDSensorState mSensorState;
};

class VRControllerPuppet : public VRControllerHost
{
public:
  explicit VRControllerPuppet(dom::GamepadHand aHand);
  void SetButtonPressState(uint32_t aButton, bool aPressed);
  uint64_t GetButtonPressState();
  void SetAxisMoveState(uint32_t aAxis, double aValue);
  double GetAxisMoveState(uint32_t aAxis);
  void SetPoseMoveState(const dom::GamepadPoseState& aPose);
  const dom::GamepadPoseState& GetPoseMoveState();
  float GetAxisMove(uint32_t aAxis);
  void SetAxisMove(uint32_t aAxis, float aValue);

protected:
  virtual ~VRControllerPuppet();

private:
  uint64_t mButtonPressState;
  float mAxisMoveState[3];
  float mAxisMove[3];
  dom::GamepadPoseState mPoseState;
};

} // namespace impl

class VRSystemManagerPuppet : public VRSystemManager
{
public:
  static already_AddRefed<VRSystemManagerPuppet> Create();

  virtual bool Init() override;
  virtual void Destroy() override;
  virtual void GetHMDs(nsTArray<RefPtr<VRDisplayHost>>& aHMDResult) override;
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
  VRSystemManagerPuppet();

private:
  void HandleButtonPress(uint32_t aControllerIdx,
                         uint32_t aButton,
                         uint64_t aButtonMask,
                         uint64_t aButtonPressed);
  void HandleAxisMove(uint32_t aControllerIndex, uint32_t aAxis,
                      float aValue);
  void HandlePoseTracking(uint32_t aControllerIndex,
                          const dom::GamepadPoseState& aPose,
                          VRControllerHost* aController);

  // there can only be one
  RefPtr<impl::VRDisplayPuppet> mPuppetHMD;
  nsTArray<RefPtr<impl::VRControllerPuppet>> mPuppetController;
};

} // namespace gfx
} // namespace mozilla

#endif  /* GFX_VR_PUPPET_H*/