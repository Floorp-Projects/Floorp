/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_SERVICE_OPENVRSESSION_H
#define GFX_VR_SERVICE_OPENVRSESSION_H

#include "VRSession.h"

#include "openvr.h"
#include "mozilla/TimeStamp.h"
#include "moz_external_vr.h"

#if defined(XP_WIN)
#  include <d3d11_1.h>
#endif
class nsITimer;

namespace mozilla {
namespace gfx {
class VRThread;

static const int kNumOpenVRHaptics = 1;

enum OpenVRHand : int8_t {
  Left = 0,
  Right = 1,
  Total = 2,

  None = -1
};

struct ControllerAction {
  nsCString name;
  nsCString type;
  vr::VRActionHandle_t handle = vr::k_ulInvalidActionHandle;

  ControllerAction() = default;

  ControllerAction(const char* aName, const char* aType)
      : name(aName), type(aType) {}
};

struct ControllerInfo {
  vr::VRInputValueHandle_t mSource = vr::k_ulInvalidInputValueHandle;

  ControllerAction mActionPose;
  ControllerAction mActionHaptic;

  ControllerAction mActionTrackpad_Analog;
  ControllerAction mActionTrackpad_Pressed;
  ControllerAction mActionTrackpad_Touched;

  ControllerAction mActionTrigger_Value;

  ControllerAction mActionGrip_Pressed;
  ControllerAction mActionGrip_Touched;
  ControllerAction mActionMenu_Pressed;
  ControllerAction mActionMenu_Touched;
  ControllerAction mActionSystem_Pressed;
  ControllerAction mActionSystem_Touched;

  // --- Knuckles
  ControllerAction mActionA_Pressed;
  ControllerAction mActionA_Touched;
  ControllerAction mActionB_Pressed;
  ControllerAction mActionB_Touched;

  ControllerAction mActionThumbstick_Analog;
  ControllerAction mActionThumbstick_Pressed;
  ControllerAction mActionThumbstick_Touched;

  ControllerAction mActionFingerIndex_Value;
  ControllerAction mActionFingerMiddle_Value;
  ControllerAction mActionFingerRing_Value;
  ControllerAction mActionFingerPinky_Value;
};

class OpenVRSession : public VRSession {
 public:
  OpenVRSession();
  virtual ~OpenVRSession();

  bool Initialize(mozilla::gfx::VRSystemState& aSystemState) override;
  void Shutdown() override;
  void ProcessEvents(mozilla::gfx::VRSystemState& aSystemState) override;
  void StartFrame(mozilla::gfx::VRSystemState& aSystemState) override;
  bool StartPresentation() override;
  void StopPresentation() override;
  void VibrateHaptic(uint32_t aControllerIdx, uint32_t aHapticIndex,
                     float aIntensity, float aDuration) override;
  void StopVibrateHaptic(uint32_t aControllerIdx) override;
  void StopAllHaptics() override;

 protected:
#if defined(XP_WIN)
  bool SubmitFrame(const mozilla::gfx::VRLayer_Stereo_Immersive& aLayer,
                   ID3D11Texture2D* aTexture) override;
#elif defined(XP_MACOSX)
  bool SubmitFrame(const mozilla::gfx::VRLayer_Stereo_Immersive& aLayer,
                   const VRLayerTextureHandle& aTexture) override;
#endif

 private:
  // OpenVR State
  ::vr::IVRSystem* mVRSystem = nullptr;
  ::vr::IVRChaperone* mVRChaperone = nullptr;
  ::vr::IVRCompositor* mVRCompositor = nullptr;
  ::vr::TrackedDeviceIndex_t
      mControllerDeviceIndexObsolete[kVRControllerMaxCount];
  ::vr::VRActionSetHandle_t mActionsetFirefox = vr::k_ulInvalidActionSetHandle;
  OpenVRHand mControllerDeviceIndex[kVRControllerMaxCount];
  ControllerInfo mControllerHand[OpenVRHand::Total];
  float mHapticPulseRemaining[kVRControllerMaxCount][kNumOpenVRHaptics];
  float mHapticPulseIntensity[kVRControllerMaxCount][kNumOpenVRHaptics];
  bool mIsWindowsMR;
  TimeStamp mLastHapticUpdate;

  bool InitState(mozilla::gfx::VRSystemState& aSystemState);
  void UpdateStageParameters(mozilla::gfx::VRDisplayState& aState);
  void UpdateEyeParameters(mozilla::gfx::VRSystemState& aState);
  void UpdateHeadsetPose(mozilla::gfx::VRSystemState& aState);
  void EnumerateControllers(VRSystemState& aState);
  void EnumerateControllersObsolete(VRSystemState& aState);
  void UpdateControllerPoses(VRSystemState& aState);
  void UpdateControllerPosesObsolete(VRSystemState& aState);
  void UpdateControllerButtons(VRSystemState& aState);
  void UpdateControllerButtonsObsolete(VRSystemState& aState);
  void UpdateTelemetry(VRSystemState& aSystemState);
  bool SetupContollerActions();

  bool SubmitFrame(const VRLayerTextureHandle& aTextureHandle,
                   ::vr::ETextureType aTextureType,
                   const VRLayerEyeRect& aLeftEyeRect,
                   const VRLayerEyeRect& aRightEyeRect);
#if defined(XP_WIN)
  bool CreateD3DObjects();
#endif
  void GetControllerDeviceId(::vr::ETrackedDeviceClass aDeviceType,
                             ::vr::TrackedDeviceIndex_t aDeviceIndex,
                             nsCString& aId);
  void UpdateHaptics();
  void UpdateHapticsObsolete();
  void StartHapticThread();
  void StopHapticThread();
  void StartHapticTimer();
  void StopHapticTimer();
  static void HapticTimerCallback(nsITimer* aTimer, void* aClosure);
  RefPtr<nsITimer> mHapticTimer;
  RefPtr<VRThread> mHapticThread;
  mozilla::Mutex mControllerHapticStateMutex;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // GFX_VR_SERVICE_OPENVRSESSION_H
