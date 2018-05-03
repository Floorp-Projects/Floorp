/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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

#if defined(XP_MACOSX)
class MacIOSurface;
#endif
namespace mozilla {
namespace gfx {
class VRThread;

namespace impl {

class VRDisplayOpenVR : public VRDisplayHost
{
public:
  void ZeroSensor() override;
  bool GetIsHmdPresent();

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
#endif

public:
  explicit VRDisplayOpenVR(::vr::IVRSystem *aVRSystem,
                           ::vr::IVRChaperone *aVRChaperone,
                           ::vr::IVRCompositor *aVRCompositor);
  void Refresh();
protected:
  virtual ~VRDisplayOpenVR();
  void Destroy();

  // not owned by us; global from OpenVR
  ::vr::IVRSystem *mVRSystem;
  ::vr::IVRChaperone *mVRChaperone;
  ::vr::IVRCompositor *mVRCompositor;

  VRTelemetry mTelemetry;
  bool mIsPresenting;
  bool mIsHmdPresent;

  void UpdateStageParameters();
  void UpdateEyeParameters(gfx::Matrix4x4* aHeadToEyeTransforms = nullptr);
  bool SubmitFrame(void* aTextureHandle,
                   ::vr::ETextureType aTextureType,
                   const IntSize& aSize,
                   const gfx::Rect& aLeftEyeRect,
                   const gfx::Rect& aRightEyeRect);
};

class VRControllerOpenVR : public VRControllerHost
{
public:
  explicit VRControllerOpenVR(dom::GamepadHand aHand, uint32_t aDisplayID, uint32_t aNumButtons,
                              uint32_t aNumTriggers, uint32_t aNumAxes,
                              const nsCString& aId);
  void SetTrackedIndex(uint32_t aTrackedIndex);
  uint32_t GetTrackedIndex();
  float GetAxisMove(uint32_t aAxis);
  void SetAxisMove(uint32_t aAxis, float aValue);
  void SetTrigger(uint32_t aButton, float aValue);
  float GetTrigger(uint32_t aButton);
  void SetHand(dom::GamepadHand aHand);
  void VibrateHaptic(::vr::IVRSystem* aVRSystem,
                     uint32_t aHapticIndex,
                     double aIntensity,
                     double aDuration,
                     const VRManagerPromise& aPromise);
  void StopVibrateHaptic();
  void ShutdownVibrateHapticThread();

protected:
  virtual ~VRControllerOpenVR();

private:
  void UpdateVibrateHaptic(::vr::IVRSystem* aVRSystem,
                           uint32_t aHapticIndex,
                           double aIntensity,
                           double aDuration,
                           uint64_t aVibrateIndex,
                           const VRManagerPromise& aPromise);
  void VibrateHapticComplete(const VRManagerPromise& aPromise);

  // The index of tracked devices from ::vr::IVRSystem.
  uint32_t mTrackedIndex;
  RefPtr<VRThread> mVibrateThread;
  Atomic<bool> mIsVibrateStopped;
};

} // namespace impl

class VRSystemManagerOpenVR : public VRSystemManager
{
public:
  static already_AddRefed<VRSystemManagerOpenVR> Create();

  virtual void Destroy() override;
  virtual void Shutdown() override;
  virtual void NotifyVSync() override;
  virtual void Enumerate() override;
  virtual bool ShouldInhibitEnumeration() override;
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
                          uint32_t aTrigger,
                          float aValue);
  void HandleAxisMove(uint32_t aControllerIdx, uint32_t aAxis,
                      float aValue);
  void HandlePoseTracking(uint32_t aControllerIdx,
                          const dom::GamepadPoseState& aPose,
                          VRControllerHost* aController);
  dom::GamepadHand GetGamepadHandFromControllerRole(
                          ::vr::ETrackedControllerRole aRole);
  void GetControllerDeviceId(::vr::ETrackedDeviceClass aDeviceType,
                             ::vr::TrackedDeviceIndex_t aDeviceIndex,
                             nsCString& aId);

  // there can only be one
  RefPtr<impl::VRDisplayOpenVR> mOpenVRHMD;
  nsTArray<RefPtr<impl::VRControllerOpenVR>> mOpenVRController;
  ::vr::IVRSystem *mVRSystem;
  bool mRuntimeCheckFailed;
  bool mIsWindowsMR;
};

} // namespace gfx
} // namespace mozilla


#endif /* GFX_VR_OPENVR_H */
