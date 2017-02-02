/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(XP_WIN)
#include "CompositorD3D11.h"
#include "TextureD3D11.h"
#endif // XP_WIN

#include "gfxVRPuppet.h"

#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/dom/GamepadBinding.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::gfx::impl;
using namespace mozilla::layers;


// Reminder: changing the order of these buttons may break web content
static const uint64_t kPuppetButtonMask[] = {
  1,
  2,
  4,
  8
};

static const uint32_t kNumPuppetButtonMask = sizeof(kPuppetButtonMask) /
                                             sizeof(uint64_t);

static const uint32_t kPuppetAxes[] = {
  0,
  1,
  2
};

static const uint32_t kNumPuppetAxis = sizeof(kPuppetAxes) /
                                       sizeof(uint32_t);

static const uint32_t kNumPuppetHaptcs = 1;

VRDisplayPuppet::VRDisplayPuppet()
 : VRDisplayHost(VRDeviceType::Puppet)
 , mIsPresenting(false)
{
  MOZ_COUNT_CTOR_INHERITED(VRDisplayPuppet, VRDisplayHost);

  mDisplayInfo.mDisplayName.AssignLiteral("Puppet HMD");
  mDisplayInfo.mIsConnected = true;
  mDisplayInfo.mIsMounted = false;
  mDisplayInfo.mCapabilityFlags = VRDisplayCapabilityFlags::Cap_None |
                                  VRDisplayCapabilityFlags::Cap_Orientation |
                                  VRDisplayCapabilityFlags::Cap_Position |
                                  VRDisplayCapabilityFlags::Cap_External |
                                  VRDisplayCapabilityFlags::Cap_Present |
                                  VRDisplayCapabilityFlags::Cap_StageParameters;
  mDisplayInfo.mEyeResolution.width = 1836; // 1080 * 1.7
  mDisplayInfo.mEyeResolution.height = 2040; // 1200 * 1.7

  // SteamVR gives the application a single FOV to use; it's not configurable as with Oculus
  for (uint32_t eye = 0; eye < 2; ++eye) {
    mDisplayInfo.mEyeTranslation[eye].x = 0.0f;
    mDisplayInfo.mEyeTranslation[eye].y = 0.0f;
    mDisplayInfo.mEyeTranslation[eye].z = 0.0f;
    mDisplayInfo.mEyeFOV[eye] = VRFieldOfView(45.0, 45.0, 45.0, 45.0);
  }

  // default: 1m x 1m space, 0.75m high in seated position
  mDisplayInfo.mStageSize.width = 1.0f;
  mDisplayInfo.mStageSize.height = 1.0f;

  mDisplayInfo.mSittingToStandingTransform._11 = 1.0f;
  mDisplayInfo.mSittingToStandingTransform._12 = 0.0f;
  mDisplayInfo.mSittingToStandingTransform._13 = 0.0f;
  mDisplayInfo.mSittingToStandingTransform._14 = 0.0f;

  mDisplayInfo.mSittingToStandingTransform._21 = 0.0f;
  mDisplayInfo.mSittingToStandingTransform._22 = 1.0f;
  mDisplayInfo.mSittingToStandingTransform._23 = 0.0f;
  mDisplayInfo.mSittingToStandingTransform._24 = 0.0f;

  mDisplayInfo.mSittingToStandingTransform._31 = 0.0f;
  mDisplayInfo.mSittingToStandingTransform._32 = 0.0f;
  mDisplayInfo.mSittingToStandingTransform._33 = 1.0f;
  mDisplayInfo.mSittingToStandingTransform._34 = 0.0f;

  mDisplayInfo.mSittingToStandingTransform._41 = 0.0f;
  mDisplayInfo.mSittingToStandingTransform._42 = 0.75f;
  mDisplayInfo.mSittingToStandingTransform._43 = 0.0f;

  mSensorState.Clear();

  gfx::Quaternion rot;

  mSensorState.flags |= VRDisplayCapabilityFlags::Cap_Orientation;
  mSensorState.orientation[0] = rot.x;
  mSensorState.orientation[1] = rot.y;
  mSensorState.orientation[2] = rot.z;
  mSensorState.orientation[3] = rot.w;
  mSensorState.angularVelocity[0] = 0.0f;
  mSensorState.angularVelocity[1] = 0.0f;
  mSensorState.angularVelocity[2] = 0.0f;

  mSensorState.flags |= VRDisplayCapabilityFlags::Cap_Position;
  mSensorState.position[0] = 0.0f;
  mSensorState.position[1] = 0.0f;
  mSensorState.position[2] = 0.0f;
  mSensorState.linearVelocity[0] = 0.0f;
  mSensorState.linearVelocity[1] = 0.0f;
  mSensorState.linearVelocity[2] = 0.0f;
}

VRDisplayPuppet::~VRDisplayPuppet()
{
  MOZ_COUNT_DTOR_INHERITED(VRDisplayPuppet, VRDisplayHost);
}

void
VRDisplayPuppet::SetDisplayInfo(const VRDisplayInfo& aDisplayInfo)
{
  // We are only interested in the eye info of the display info.
  mDisplayInfo.mEyeResolution = aDisplayInfo.mEyeResolution;
  memcpy(&mDisplayInfo.mEyeFOV, &aDisplayInfo.mEyeFOV,
         sizeof(mDisplayInfo.mEyeFOV[0]) * VRDisplayInfo::NumEyes);
  memcpy(&mDisplayInfo.mEyeTranslation, &aDisplayInfo.mEyeTranslation,
         sizeof(mDisplayInfo.mEyeTranslation[0]) * VRDisplayInfo::NumEyes);
}

void
VRDisplayPuppet::Destroy()
{
  StopPresentation();
}

void
VRDisplayPuppet::ZeroSensor()
{
}

VRHMDSensorState
VRDisplayPuppet::GetSensorState()
{
  return GetSensorState(0.0f);
}

VRHMDSensorState
VRDisplayPuppet::GetSensorState(double timeOffset)
{
  return mSensorState;
}

void
VRDisplayPuppet::SetSensorState(const VRHMDSensorState& aSensorState)
{
  memcpy(&mSensorState, &aSensorState, sizeof(mSensorState));
}

void
VRDisplayPuppet::StartPresentation()
{
  if (mIsPresenting) {
    return;
  }
  mIsPresenting = true;
}

void
VRDisplayPuppet::StopPresentation()
{
  if (!mIsPresenting) {
    return;
  }

  mIsPresenting = false;
}

#if defined(XP_WIN)
void
VRDisplayPuppet::SubmitFrame(TextureSourceD3D11* aSource,
                             const IntSize& aSize,
                             const VRHMDSensorState& aSensorState,
                             const gfx::Rect& aLeftEyeRect,
                             const gfx::Rect& aRightEyeRect)
{
  if (!mIsPresenting) {
    return;
  }

  ID3D11Texture2D* tex = aSource->GetD3D11Texture();
  MOZ_ASSERT(tex);

  // TODO: Bug 1343730, Need to block until the next simulated
  // vblank interval and capture frames for use in reftests.

  // Trigger the next VSync immediately
  VRManager *vm = VRManager::Get();
  MOZ_ASSERT(vm);
  vm->NotifyVRVsync(mDisplayInfo.mDisplayID);
}
#else
void
VRDisplayPuppet::SubmitFrame(TextureSourceOGL* aSource,
                             const IntSize& aSize,
                             const VRHMDSensorState& aSensorState,
                             const gfx::Rect& aLeftEyeRect,
                             const gfx::Rect& aRightEyeRect)
{
  if (!mIsPresenting) {
    return;
  }

  // TODO: Bug 1343730, Need to block until the next simulated
  // vblank interval and capture frames for use in reftests.

  // Trigger the next VSync immediately
  VRManager *vm = VRManager::Get();
  MOZ_ASSERT(vm);
  vm->NotifyVRVsync(mDisplayInfo.mDisplayID);
}
#endif

void
VRDisplayPuppet::NotifyVSync()
{
  // We update mIsConneced once per frame.
  mDisplayInfo.mIsConnected = true;
}

VRControllerPuppet::VRControllerPuppet(dom::GamepadHand aHand)
  : VRControllerHost(VRDeviceType::Puppet)
  , mButtonPressState(0)
{
  MOZ_COUNT_CTOR_INHERITED(VRControllerPuppet, VRControllerHost);
  mControllerInfo.mControllerName.AssignLiteral("Puppet Gamepad");
  mControllerInfo.mMappingType = GamepadMappingType::_empty;
  mControllerInfo.mHand = aHand;
  mControllerInfo.mNumButtons = kNumPuppetButtonMask;
  mControllerInfo.mNumAxes = kNumPuppetAxis;
  mControllerInfo.mNumHaptics = kNumPuppetHaptcs;
}

VRControllerPuppet::~VRControllerPuppet()
{
  MOZ_COUNT_DTOR_INHERITED(VRControllerPuppet, VRControllerHost);
}

void
VRControllerPuppet::SetButtonPressState(uint32_t aButton, bool aPressed)
{
  const uint64_t buttonMask = kPuppetButtonMask[aButton];
  uint64_t pressedBit = GetButtonPressed();

  if (aPressed) {
    pressedBit |= kPuppetButtonMask[aButton];
  } else if (pressedBit & buttonMask) {
    // this button was pressed but is released now.
    uint64_t mask = 0xff ^ buttonMask;
    pressedBit &= mask;
  }

  mButtonPressState = pressedBit;
}

uint64_t
VRControllerPuppet::GetButtonPressState()
{
  return mButtonPressState;
}

void
VRControllerPuppet::SetAxisMoveState(uint32_t aAxis, double aValue)
{
  MOZ_ASSERT((sizeof(mAxisMoveState) / sizeof(float)) == kNumPuppetAxis);
  MOZ_ASSERT(aAxis <= kNumPuppetAxis);

  mAxisMoveState[aAxis] = aValue;
}

double
VRControllerPuppet::GetAxisMoveState(uint32_t aAxis)
{
  return mAxisMoveState[aAxis];
}

void
VRControllerPuppet::SetPoseMoveState(const dom::GamepadPoseState& aPose)
{
  mPoseState = aPose;
}

const dom::GamepadPoseState&
VRControllerPuppet::GetPoseMoveState()
{
  return mPoseState;
}

float
VRControllerPuppet::GetAxisMove(uint32_t aAxis)
{
  return mAxisMove[aAxis];
}

void
VRControllerPuppet::SetAxisMove(uint32_t aAxis, float aValue)
{
  mAxisMove[aAxis] = aValue;
}

VRSystemManagerPuppet::VRSystemManagerPuppet()
{
}

/*static*/ already_AddRefed<VRSystemManagerPuppet>
VRSystemManagerPuppet::Create()
{
  if (!gfxPrefs::VREnabled() || !gfxPrefs::VRPuppetEnabled()) {
    return nullptr;
  }

  RefPtr<VRSystemManagerPuppet> manager = new VRSystemManagerPuppet();
  return manager.forget();
}

bool
VRSystemManagerPuppet::Init()
{
  return true;
}

void
VRSystemManagerPuppet::Destroy()
{
  mPuppetHMD = nullptr;
}

void
VRSystemManagerPuppet::GetHMDs(nsTArray<RefPtr<VRDisplayHost>>& aHMDResult)
{
  if (mPuppetHMD == nullptr) {
    mPuppetHMD = new VRDisplayPuppet();
  }
  aHMDResult.AppendElement(mPuppetHMD);
}

bool
VRSystemManagerPuppet::GetIsPresenting()
{
  if (mPuppetHMD) {
    VRDisplayInfo displayInfo(mPuppetHMD->GetDisplayInfo());
    return displayInfo.GetIsPresenting();
  }

  return false;
}

void
VRSystemManagerPuppet::HandleInput()
{
  RefPtr<impl::VRControllerPuppet> controller;
  for (uint32_t i = 0; i < mPuppetController.Length(); ++i) {
    controller = mPuppetController[i];
    for (uint32_t j = 0; j < kNumPuppetButtonMask; ++j) {
      HandleButtonPress(i, j, kPuppetButtonMask[i], controller->GetButtonPressState());
    }
    controller->SetButtonPressed(controller->GetButtonPressState());

    for (uint32_t j = 0; j < kNumPuppetAxis; ++j) {
      HandleAxisMove(i, j, controller->GetAxisMoveState(j));
    }
    HandlePoseTracking(i, controller->GetPoseMoveState(), controller);
  }
}

void
VRSystemManagerPuppet::HandleButtonPress(uint32_t aControllerIdx,
                                         uint32_t aButton,
                                         uint64_t aButtonMask,
                                         uint64_t aButtonPressed)
{
  RefPtr<impl::VRControllerPuppet> controller(mPuppetController[aControllerIdx]);
  MOZ_ASSERT(controller);
  const uint64_t diff = (controller->GetButtonPressed() ^ aButtonPressed);

  if (!diff) {
    return;
  }

  if (diff & aButtonMask) {
    // diff & aButtonPressed would be true while a new button press
    // event, otherwise it is an old press event and needs to notify
    // the button has been released.
    NewButtonEvent(aControllerIdx, aButton, aButtonMask & aButtonPressed,
                   (aButtonMask & aButtonPressed) ? 1.0L : 0.0L);
  }
}

void
VRSystemManagerPuppet::HandleAxisMove(uint32_t aControllerIdx, uint32_t aAxis,
                                      float aValue)
{
  RefPtr<impl::VRControllerPuppet> controller(mPuppetController[aControllerIdx]);
  MOZ_ASSERT(controller);

  if (controller->GetAxisMove(aAxis) != aValue) {
    NewAxisMove(aControllerIdx, aAxis, aValue);
    controller->SetAxisMove(aAxis, aValue);
  }
}

void
VRSystemManagerPuppet::HandlePoseTracking(uint32_t aControllerIdx,
                                          const GamepadPoseState& aPose,
                                          VRControllerHost* aController)
{
  MOZ_ASSERT(aController);
  if (aPose != aController->GetPose()) {
    aController->SetPose(aPose);
    NewPoseState(aControllerIdx, aPose);
  }
}

void
VRSystemManagerPuppet::VibrateHaptic(uint32_t aControllerIdx,
                                     uint32_t aHapticIndex,
                                     double aIntensity,
                                     double aDuration,
                                     uint32_t aPromiseID)
{
}

void
VRSystemManagerPuppet::StopVibrateHaptic(uint32_t aControllerIdx)
{
}

void
VRSystemManagerPuppet::GetControllers(nsTArray<RefPtr<VRControllerHost>>& aControllerResult)
{
  aControllerResult.Clear();
  for (uint32_t i = 0; i < mPuppetController.Length(); ++i) {
    aControllerResult.AppendElement(mPuppetController[i]);
  }
}

void
VRSystemManagerPuppet::ScanForControllers()
{
  // We make VRSystemManagerPuppet has two controllers always.
  const uint32_t newControllerCount = 2;

  if (newControllerCount != mControllerCount) {
    // controller count is changed, removing the existing gamepads first.
    for (uint32_t i = 0; i < mPuppetController.Length(); ++i) {
      RemoveGamepad(i);
    }

    mControllerCount = 0;
    mPuppetController.Clear();

    // Re-adding controllers to VRControllerManager.
    for (uint32_t i = 0; i < newControllerCount; ++i) {
      dom::GamepadHand hand = (i % 2) ? dom::GamepadHand::Right :
                                        dom::GamepadHand::Left;
      RefPtr<VRControllerPuppet> puppetController = new VRControllerPuppet(hand);
      mPuppetController.AppendElement(puppetController);

      // Not already present, add it.
      AddGamepad(puppetController->GetControllerInfo());
      ++mControllerCount;
    }
  }
}

void
VRSystemManagerPuppet::RemoveControllers()
{
  mPuppetController.Clear();
  mControllerCount = 0;
}
