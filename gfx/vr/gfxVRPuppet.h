/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_PUPPET_H
#define GFX_VR_PUPPET_H

#include "nsTArray.h"
#include "mozilla/RefPtr.h"
#include "nsRefPtrHashtable.h"

#include "gfxVR.h"
#include "VRDisplayLocal.h"

#if defined(XP_MACOSX)
class MacIOSurface;
#endif
namespace mozilla {
namespace gfx {
namespace impl {

class VRDisplayPuppet : public VRDisplayLocal
{
public:
  void SetDisplayInfo(const VRDisplayInfo& aDisplayInfo);
  void SetSensorState(const VRHMDSensorState& aSensorState);
  void ZeroSensor() override;

protected:
  virtual VRHMDSensorState GetSensorState() override;
  virtual void StartPresentation() override;
  virtual void StopPresentation() override;
#if defined(XP_WIN)
  virtual bool SubmitFrame(ID3D11Texture2D* aSource,
                           const IntSize& aSize,
                           const gfx::Rect& aLeftEyeRect,
                           const gfx::Rect& aRightEyeRect) override;
#elif defined(XP_MACOSX)
  virtual bool SubmitFrame(MacIOSurface* aMacIOSurface,
                           const IntSize& aSize,
                           const gfx::Rect& aLeftEyeRect,
                           const gfx::Rect& aRightEyeRect) override;
#elif defined(MOZ_WIDGET_ANDROID)
  virtual bool SubmitFrame(const mozilla::layers::SurfaceTextureDescriptor& aDescriptor,
                           const gfx::Rect& aLeftEyeRect,
                           const gfx::Rect& aRightEyeRect) override;
#endif

public:
  explicit VRDisplayPuppet();
  void Refresh();

protected:
  virtual ~VRDisplayPuppet();
  void Destroy();

  bool mIsPresenting;

private:
#if defined(XP_WIN)
  bool UpdateConstantBuffers();

  ID3D11VertexShader* mQuadVS;
  ID3D11PixelShader* mQuadPS;
  RefPtr<ID3D11SamplerState> mLinearSamplerState;
  layers::VertexShaderConstants mVSConstants;
  layers::PixelShaderConstants mPSConstants;
  RefPtr<ID3D11Buffer> mVSConstantBuffer;
  RefPtr<ID3D11Buffer> mPSConstantBuffer;
  RefPtr<ID3D11Buffer> mVertexBuffer;
  RefPtr<ID3D11InputLayout> mInputLayout;
#endif

  VRHMDSensorState mSensorState;
};

class VRControllerPuppet : public VRControllerHost
{
public:
  explicit VRControllerPuppet(dom::GamepadHand aHand, uint32_t aDisplayID);
  void SetButtonPressState(uint32_t aButton, bool aPressed);
  uint64_t GetButtonPressState();
  void SetButtonTouchState(uint32_t aButton, bool aTouched);
  uint64_t GetButtonTouchState();
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
  uint64_t mButtonTouchState;
  float mAxisMoveState[3];
  dom::GamepadPoseState mPoseState;
};

} // namespace impl

class VRSystemManagerPuppet : public VRSystemManager
{
public:
  static already_AddRefed<VRSystemManagerPuppet> Create();
  uint32_t CreateTestDisplay();
  void ClearTestDisplays();
  void SetPuppetDisplayInfo(const uint32_t& aDeviceID,
                            const VRDisplayInfo& aDisplayInfo);
  void SetPuppetDisplaySensorState(const uint32_t& aDeviceID,
                                   const VRHMDSensorState& aSensorState);

  virtual void Destroy() override;
  virtual void Shutdown() override;
  virtual void Enumerate() override;
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
                             const VRManagerPromise& aPromise) override;
  virtual void StopVibrateHaptic(uint32_t aControllerIdx) override;
  virtual void NotifyVSync() override;

protected:
  VRSystemManagerPuppet();

private:
  void HandleButtonPress(uint32_t aControllerIdx,
                         uint32_t aButton,
                         uint64_t aButtonMask,
                         uint64_t aButtonPressed,
                         uint64_t aButtonTouched);
  void HandleAxisMove(uint32_t aControllerIndex, uint32_t aAxis,
                      float aValue);
  void HandlePoseTracking(uint32_t aControllerIndex,
                          const dom::GamepadPoseState& aPose,
                          VRControllerHost* aController);

  // Enumerated puppet hardware devices, as seen by Web APIs:
  nsTArray<RefPtr<impl::VRDisplayPuppet>> mPuppetHMDs;
  nsTArray<RefPtr<impl::VRControllerPuppet>> mPuppetController;

  // Emulated hardware state, persistent through VRSystemManager::Shutdown():
  static const uint32_t kMaxPuppetDisplays = 5;
  uint32_t mPuppetDisplayCount;
  VRDisplayInfo mPuppetDisplayInfo[kMaxPuppetDisplays];
  VRHMDSensorState mPuppetDisplaySensorState[kMaxPuppetDisplays];
};

} // namespace gfx
} // namespace mozilla

#endif  /* GFX_VR_PUPPET_H*/
