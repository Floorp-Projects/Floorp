/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/VRServiceTest.h"
#include "mozilla/dom/VRServiceTestBinding.h"
#include "mozilla/dom/GamepadPoseState.h"
#include "mozilla/dom/Promise.h"
#include "VRManagerChild.h"
#include "VRPuppetCommandBuffer.h"
#include <type_traits>

namespace mozilla {
using namespace gfx;
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(VRMockDisplay, DOMEventTargetHelper,
                                   mVRServiceTest)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(VRMockDisplay,
                                               DOMEventTargetHelper)

namespace {
template <class T>
bool ReadFloat32Array(T& aDestination, const Float32Array& aSource,
                      ErrorResult& aRv) {
  constexpr size_t length = std::extent<T>::value;
  aSource.ComputeState();
  if (aSource.Length() != length) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    // We don't want to MOZ_ASSERT here, as that would cause the
    // browser to crash, making it difficult to debug the problem
    // in JS code calling this API.
    return false;
  }
  for (size_t i = 0; i < length; i++) {
    aDestination[i] = aSource.Data()[i];
  }
  return true;
}
};  // anonymous namespace

VRMockDisplay::VRMockDisplay(VRServiceTest* aVRServiceTest)
    : DOMEventTargetHelper(aVRServiceTest->GetOwner()),
      mVRServiceTest(aVRServiceTest) {}

JSObject* VRMockDisplay::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return VRMockDisplay_Binding::Wrap(aCx, this, aGivenProto);
}

VRHMDSensorState& VRMockDisplay::SensorState() const {
  return mVRServiceTest->SystemState().sensorState;
}

VRDisplayState& VRMockDisplay::DisplayState() const {
  return mVRServiceTest->SystemState().displayState;
}

void VRMockDisplay::Clear() {
  VRDisplayState& displayState = DisplayState();
  displayState.Clear();
  VRHMDSensorState& sensorState = SensorState();
  sensorState.Clear();
}

void VRMockDisplay::Create() {
  Clear();
  VRDisplayState& state = DisplayState();

  strncpy(state.displayName, "Puppet HMD", kVRDisplayNameMaxLen);
  state.eightCC = GFX_VR_EIGHTCC('P', 'u', 'p', 'p', 'e', 't', ' ', ' ');
  state.isConnected = true;
  state.isMounted = false;
  state.capabilityFlags = VRDisplayCapabilityFlags::Cap_None |
                          VRDisplayCapabilityFlags::Cap_Orientation |
                          VRDisplayCapabilityFlags::Cap_Position |
                          VRDisplayCapabilityFlags::Cap_External |
                          VRDisplayCapabilityFlags::Cap_Present |
                          VRDisplayCapabilityFlags::Cap_StageParameters |
                          VRDisplayCapabilityFlags::Cap_MountDetection |
                          VRDisplayCapabilityFlags::Cap_ImmersiveVR;
  state.blendMode = VRDisplayBlendMode::Opaque;

  // 1836 x 2040 resolution is arbitrary and can be overridden.
  // This default resolution was chosen to be within range of a
  // typical VR eye buffer size.  This value is derived by
  // scaling a 1080x1200 per-eye panel resolution by the
  // commonly used pre-lens-distortion pass scaling factor of 1.7x.
  // 1.7x is commonly used in HMD's employing fresnel lenses to ensure
  // a sufficient fragment shading rate in the peripheral area of the
  // post-warp eye buffers.
  state.eyeResolution.width = 1836;   // 1080 * 1.7
  state.eyeResolution.height = 2040;  // 1200 * 1.7

  for (uint32_t eye = 0; eye < VRDisplayState::NumEyes; ++eye) {
    state.eyeTranslation[eye].x = 0.0f;
    state.eyeTranslation[eye].y = 0.0f;
    state.eyeTranslation[eye].z = 0.0f;
    state.eyeFOV[eye] = gfx::VRFieldOfView(45.0, 45.0, 45.0, 45.0);
  }

  // default: 1m x 1m space, 0.75m high in seated position
  state.stageSize.width = 1.0f;
  state.stageSize.height = 1.0f;

  state.sittingToStandingTransform[0] = 1.0f;
  state.sittingToStandingTransform[1] = 0.0f;
  state.sittingToStandingTransform[2] = 0.0f;
  state.sittingToStandingTransform[3] = 0.0f;

  state.sittingToStandingTransform[4] = 0.0f;
  state.sittingToStandingTransform[5] = 1.0f;
  state.sittingToStandingTransform[6] = 0.0f;
  state.sittingToStandingTransform[7] = 0.0f;

  state.sittingToStandingTransform[8] = 0.0f;
  state.sittingToStandingTransform[9] = 0.0f;
  state.sittingToStandingTransform[10] = 1.0f;
  state.sittingToStandingTransform[11] = 0.0f;

  state.sittingToStandingTransform[12] = 0.0f;
  state.sittingToStandingTransform[13] = 0.75f;
  state.sittingToStandingTransform[14] = 0.0f;
  state.sittingToStandingTransform[15] = 1.0f;

  VRHMDSensorState& sensorState = SensorState();
  gfx::Quaternion rot;
  sensorState.flags |= VRDisplayCapabilityFlags::Cap_Orientation;
  sensorState.pose.orientation[0] = rot.x;
  sensorState.pose.orientation[1] = rot.y;
  sensorState.pose.orientation[2] = rot.z;
  sensorState.pose.orientation[3] = rot.w;
  sensorState.pose.angularVelocity[0] = 0.0f;
  sensorState.pose.angularVelocity[1] = 0.0f;
  sensorState.pose.angularVelocity[2] = 0.0f;

  sensorState.flags |= VRDisplayCapabilityFlags::Cap_Position;
  sensorState.pose.position[0] = 0.0f;
  sensorState.pose.position[1] = 0.0f;
  sensorState.pose.position[2] = 0.0f;
  sensorState.pose.linearVelocity[0] = 0.0f;
  sensorState.pose.linearVelocity[1] = 0.0f;
  sensorState.pose.linearVelocity[2] = 0.0f;
}

void VRMockDisplay::SetConnected(bool aConnected) {
  DisplayState().isConnected = aConnected;
}
bool VRMockDisplay::Connected() const { return DisplayState().isConnected; }

void VRMockDisplay::SetMounted(bool aMounted) {
  DisplayState().isMounted = aMounted;
}

bool VRMockDisplay::Mounted() const { return DisplayState().isMounted; }

void VRMockDisplay::SetCapFlag(VRDisplayCapabilityFlags aFlag, bool aEnabled) {
  if (aEnabled) {
    DisplayState().capabilityFlags |= aFlag;
  } else {
    DisplayState().capabilityFlags &= ~aFlag;
  }
}
bool VRMockDisplay::GetCapFlag(VRDisplayCapabilityFlags aFlag) const {
  return ((DisplayState().capabilityFlags & aFlag) !=
          VRDisplayCapabilityFlags::Cap_None);
}

void VRMockDisplay::SetCapPosition(bool aEnabled) {
  SetCapFlag(VRDisplayCapabilityFlags::Cap_Position, aEnabled);
}

void VRMockDisplay::SetCapOrientation(bool aEnabled) {
  SetCapFlag(VRDisplayCapabilityFlags::Cap_Orientation, aEnabled);
}

void VRMockDisplay::SetCapPresent(bool aEnabled) {
  SetCapFlag(VRDisplayCapabilityFlags::Cap_Present, aEnabled);
}

void VRMockDisplay::SetCapExternal(bool aEnabled) {
  SetCapFlag(VRDisplayCapabilityFlags::Cap_External, aEnabled);
}

void VRMockDisplay::SetCapAngularAcceleration(bool aEnabled) {
  SetCapFlag(VRDisplayCapabilityFlags::Cap_AngularAcceleration, aEnabled);
}

void VRMockDisplay::SetCapLinearAcceleration(bool aEnabled) {
  SetCapFlag(VRDisplayCapabilityFlags::Cap_LinearAcceleration, aEnabled);
}

void VRMockDisplay::SetCapStageParameters(bool aEnabled) {
  SetCapFlag(VRDisplayCapabilityFlags::Cap_StageParameters, aEnabled);
}

void VRMockDisplay::SetCapMountDetection(bool aEnabled) {
  SetCapFlag(VRDisplayCapabilityFlags::Cap_MountDetection, aEnabled);
}

void VRMockDisplay::SetCapPositionEmulated(bool aEnabled) {
  SetCapFlag(VRDisplayCapabilityFlags::Cap_PositionEmulated, aEnabled);
}

void VRMockDisplay::SetEyeFOV(VREye aEye, double aUpDegree, double aRightDegree,
                              double aDownDegree, double aLeftDegree) {
  gfx::VRDisplayState::Eye eye = aEye == VREye::Left
                                     ? gfx::VRDisplayState::Eye_Left
                                     : gfx::VRDisplayState::Eye_Right;
  VRDisplayState& state = DisplayState();
  state.eyeFOV[eye] =
      gfx::VRFieldOfView(aUpDegree, aRightDegree, aDownDegree, aLeftDegree);
}

void VRMockDisplay::SetEyeOffset(VREye aEye, double aOffsetX, double aOffsetY,
                                 double aOffsetZ) {
  gfx::VRDisplayState::Eye eye = aEye == VREye::Left
                                     ? gfx::VRDisplayState::Eye_Left
                                     : gfx::VRDisplayState::Eye_Right;
  VRDisplayState& state = DisplayState();
  state.eyeTranslation[eye].x = (float)aOffsetX;
  state.eyeTranslation[eye].y = (float)aOffsetY;
  state.eyeTranslation[eye].z = (float)aOffsetZ;
}

bool VRMockDisplay::CapPosition() const {
  return GetCapFlag(VRDisplayCapabilityFlags::Cap_Position);
}

bool VRMockDisplay::CapOrientation() const {
  return GetCapFlag(VRDisplayCapabilityFlags::Cap_Orientation);
}

bool VRMockDisplay::CapPresent() const {
  return GetCapFlag(VRDisplayCapabilityFlags::Cap_Present);
}

bool VRMockDisplay::CapExternal() const {
  return GetCapFlag(VRDisplayCapabilityFlags::Cap_External);
}

bool VRMockDisplay::CapAngularAcceleration() const {
  return GetCapFlag(VRDisplayCapabilityFlags::Cap_AngularAcceleration);
}

bool VRMockDisplay::CapLinearAcceleration() const {
  return GetCapFlag(VRDisplayCapabilityFlags::Cap_LinearAcceleration);
}

bool VRMockDisplay::CapStageParameters() const {
  return GetCapFlag(VRDisplayCapabilityFlags::Cap_StageParameters);
}

bool VRMockDisplay::CapMountDetection() const {
  return GetCapFlag(VRDisplayCapabilityFlags::Cap_MountDetection);
}

bool VRMockDisplay::CapPositionEmulated() const {
  return GetCapFlag(VRDisplayCapabilityFlags::Cap_PositionEmulated);
}

void VRMockDisplay::SetEyeResolution(uint32_t aRenderWidth,
                                     uint32_t aRenderHeight) {
  DisplayState().eyeResolution.width = aRenderWidth;
  DisplayState().eyeResolution.height = aRenderHeight;
}

void VRMockDisplay::SetStageSize(double aWidth, double aHeight) {
  VRDisplayState& displayState = DisplayState();
  displayState.stageSize.width = (float)aWidth;
  displayState.stageSize.height = (float)aHeight;
}

void VRMockDisplay::SetSittingToStandingTransform(
    const Float32Array& aTransform, ErrorResult& aRv) {
  Unused << ReadFloat32Array(DisplayState().sittingToStandingTransform,
                             aTransform, aRv);
}

void VRMockDisplay::SetPose(const Nullable<Float32Array>& aPosition,
                            const Nullable<Float32Array>& aLinearVelocity,
                            const Nullable<Float32Array>& aLinearAcceleration,
                            const Nullable<Float32Array>& aOrientation,
                            const Nullable<Float32Array>& aAngularVelocity,
                            const Nullable<Float32Array>& aAngularAcceleration,
                            ErrorResult& aRv) {
  VRHMDSensorState& sensorState = mVRServiceTest->SystemState().sensorState;
  sensorState.Clear();
  sensorState.flags = VRDisplayCapabilityFlags::Cap_None;
  // sensorState.timestamp will be set automatically during
  // puppet script execution

  if (!aOrientation.IsNull()) {
    if (!ReadFloat32Array(sensorState.pose.orientation, aOrientation.Value(),
                          aRv)) {
      return;
    }
    sensorState.flags |= VRDisplayCapabilityFlags::Cap_Orientation;
  }
  if (!aAngularVelocity.IsNull()) {
    if (!ReadFloat32Array(sensorState.pose.angularVelocity,
                          aAngularVelocity.Value(), aRv)) {
      return;
    }
    sensorState.flags |= VRDisplayCapabilityFlags::Cap_AngularAcceleration;
  }
  if (!aAngularAcceleration.IsNull()) {
    if (!ReadFloat32Array(sensorState.pose.angularAcceleration,
                          aAngularAcceleration.Value(), aRv)) {
      return;
    }
    sensorState.flags |= VRDisplayCapabilityFlags::Cap_AngularAcceleration;
  }
  if (!aPosition.IsNull()) {
    if (!ReadFloat32Array(sensorState.pose.position, aPosition.Value(), aRv)) {
      return;
    }
    sensorState.flags |= VRDisplayCapabilityFlags::Cap_Position;
  }
  if (!aLinearVelocity.IsNull()) {
    if (!ReadFloat32Array(sensorState.pose.linearVelocity,
                          aLinearVelocity.Value(), aRv)) {
      return;
    }
    sensorState.flags |= VRDisplayCapabilityFlags::Cap_LinearAcceleration;
  }
  if (!aLinearAcceleration.IsNull()) {
    if (!ReadFloat32Array(sensorState.pose.linearAcceleration,
                          aLinearAcceleration.Value(), aRv)) {
      return;
    }
    sensorState.flags |= VRDisplayCapabilityFlags::Cap_LinearAcceleration;
  }
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(VRMockController, DOMEventTargetHelper,
                                   mVRServiceTest)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(VRMockController,
                                               DOMEventTargetHelper)

VRMockController::VRMockController(VRServiceTest* aVRServiceTest,
                                   uint32_t aControllerIdx)
    : DOMEventTargetHelper(aVRServiceTest->GetOwner()),
      mVRServiceTest(aVRServiceTest),
      mControllerIdx(aControllerIdx) {
  MOZ_ASSERT(aControllerIdx < kVRControllerMaxCount);
}

JSObject* VRMockController::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return VRMockController_Binding::Wrap(aCx, this, aGivenProto);
}

VRControllerState& VRMockController::ControllerState() const {
  return mVRServiceTest->SystemState().controllerState[mControllerIdx];
}

void VRMockController::Create() {
  // Initialize with a 6dof, left-handed gamepad with one haptic actuator
  // Tests are expected to modify the controller before it is sent to the
  // puppet.
  Clear();
  VRControllerState& state = ControllerState();
  strncpy(state.controllerName, "Puppet Gamepad", kVRControllerNameMaxLen);
  state.hand = GamepadHand::Left;
  state.flags = GamepadCapabilityFlags::Cap_Position |
                GamepadCapabilityFlags::Cap_Orientation;
  state.numButtons = 1;
  state.numHaptics = 1;
  state.triggerValue[0] = 0.0f;
}

void VRMockController::Clear() {
  mVRServiceTest->ClearController(mControllerIdx);
}

void VRMockController::SetCapFlag(GamepadCapabilityFlags aFlag, bool aEnabled) {
  if (aEnabled) {
    ControllerState().flags |= aFlag;
  } else {
    ControllerState().flags &= ~aFlag;
  }
}
bool VRMockController::GetCapFlag(GamepadCapabilityFlags aFlag) const {
  return (ControllerState().flags & aFlag) != GamepadCapabilityFlags::Cap_None;
}

void VRMockController::SetHand(GamepadHand aHand) {
  ControllerState().hand = aHand;
}

GamepadHand VRMockController::Hand() const { return ControllerState().hand; }

void VRMockController::SetCapPosition(bool aEnabled) {
  SetCapFlag(GamepadCapabilityFlags::Cap_Position, aEnabled);
}

bool VRMockController::CapPosition() const {
  return GetCapFlag(GamepadCapabilityFlags::Cap_Position);
}

void VRMockController::SetCapOrientation(bool aEnabled) {
  SetCapFlag(GamepadCapabilityFlags::Cap_Orientation, aEnabled);
}

bool VRMockController::CapOrientation() const {
  return GetCapFlag(GamepadCapabilityFlags::Cap_Orientation);
}

void VRMockController::SetCapAngularAcceleration(bool aEnabled) {
  SetCapFlag(GamepadCapabilityFlags::Cap_AngularAcceleration, aEnabled);
}

bool VRMockController::CapAngularAcceleration() const {
  return GetCapFlag(GamepadCapabilityFlags::Cap_AngularAcceleration);
}

void VRMockController::SetCapLinearAcceleration(bool aEnabled) {
  SetCapFlag(GamepadCapabilityFlags::Cap_LinearAcceleration, aEnabled);
}

bool VRMockController::CapLinearAcceleration() const {
  return GetCapFlag(GamepadCapabilityFlags::Cap_LinearAcceleration);
}

void VRMockController::SetAxisCount(uint32_t aCount) {
  MOZ_ASSERT(aCount <= kVRControllerMaxAxis);
  ControllerState().numAxes = aCount;
}

uint32_t VRMockController::AxisCount() const {
  return ControllerState().numAxes;
}

void VRMockController::SetButtonCount(uint32_t aCount) {
  MOZ_ASSERT(aCount <= kVRControllerMaxButtons);
  ControllerState().numButtons = aCount;
}

uint32_t VRMockController::ButtonCount() const {
  return ControllerState().numButtons;
}

void VRMockController::SetHapticCount(uint32_t aCount) {
  ControllerState().numHaptics = aCount;
}

uint32_t VRMockController::HapticCount() const {
  return ControllerState().numHaptics;
}

void VRMockController::SetButtonPressed(uint32_t aButtonIdx, bool aPressed) {
  MOZ_ASSERT(aButtonIdx < kVRControllerMaxButtons);
  if (aPressed) {
    ControllerState().buttonPressed |= (1 << aButtonIdx);
  } else {
    ControllerState().buttonPressed &= ~(1 << aButtonIdx);
  }
}

void VRMockController::SetButtonTouched(uint32_t aButtonIdx, bool aTouched) {
  MOZ_ASSERT(aButtonIdx < kVRControllerMaxButtons);
  if (aTouched) {
    ControllerState().buttonTouched |= (1 << aButtonIdx);
  } else {
    ControllerState().buttonTouched &= ~(1 << aButtonIdx);
  }
}

void VRMockController::SetButtonTrigger(uint32_t aButtonIdx, double aTrigger) {
  MOZ_ASSERT(aButtonIdx < kVRControllerMaxButtons);

  ControllerState().triggerValue[aButtonIdx] = (float)aTrigger;
}

void VRMockController::SetAxisValue(uint32_t aAxisIdx, double aValue) {
  MOZ_ASSERT(aAxisIdx < kVRControllerMaxAxis);
  ControllerState().axisValue[aAxisIdx] = (float)aValue;
}

void VRMockController::SetPose(
    const Nullable<Float32Array>& aPosition,
    const Nullable<Float32Array>& aLinearVelocity,
    const Nullable<Float32Array>& aLinearAcceleration,
    const Nullable<Float32Array>& aOrientation,
    const Nullable<Float32Array>& aAngularVelocity,
    const Nullable<Float32Array>& aAngularAcceleration, ErrorResult& aRv) {
  VRControllerState& controllerState = ControllerState();
  controllerState.flags = GamepadCapabilityFlags::Cap_None;

  if (!aOrientation.IsNull()) {
    if (!ReadFloat32Array(controllerState.pose.orientation,
                          aOrientation.Value(), aRv)) {
      return;
    }
    controllerState.flags |= GamepadCapabilityFlags::Cap_Orientation;
  }
  if (!aAngularVelocity.IsNull()) {
    if (!ReadFloat32Array(controllerState.pose.angularVelocity,
                          aAngularVelocity.Value(), aRv)) {
      return;
    }
    controllerState.flags |= GamepadCapabilityFlags::Cap_AngularAcceleration;
  }
  if (!aAngularAcceleration.IsNull()) {
    if (!ReadFloat32Array(controllerState.pose.angularAcceleration,
                          aAngularAcceleration.Value(), aRv)) {
      return;
    }
    controllerState.flags |= GamepadCapabilityFlags::Cap_AngularAcceleration;
  }
  if (!aPosition.IsNull()) {
    if (!ReadFloat32Array(controllerState.pose.position, aPosition.Value(),
                          aRv)) {
      return;
    }
    controllerState.flags |= GamepadCapabilityFlags::Cap_Position;
  }
  if (!aLinearVelocity.IsNull()) {
    if (!ReadFloat32Array(controllerState.pose.linearVelocity,
                          aLinearVelocity.Value(), aRv)) {
      return;
    }
    controllerState.flags |= GamepadCapabilityFlags::Cap_LinearAcceleration;
  }
  if (!aLinearAcceleration.IsNull()) {
    if (!ReadFloat32Array(controllerState.pose.linearAcceleration,
                          aLinearAcceleration.Value(), aRv)) {
      return;
    }
    controllerState.flags |= GamepadCapabilityFlags::Cap_LinearAcceleration;
  }
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(VRServiceTest, DOMEventTargetHelper,
                                   mDisplay, mControllers, mWindow)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(VRServiceTest,
                                               DOMEventTargetHelper)

JSObject* VRServiceTest::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return VRServiceTest_Binding::Wrap(aCx, this, aGivenProto);
}

// static
already_AddRefed<VRServiceTest> VRServiceTest::CreateTestService(
    nsPIDOMWindowInner* aWindow) {
  MOZ_ASSERT(aWindow);
  RefPtr<VRServiceTest> service = new VRServiceTest(aWindow);
  return service.forget();
}

VRServiceTest::VRServiceTest(nsPIDOMWindowInner* aWindow)
    : mWindow(aWindow), mPendingState{}, mEncodedState{}, mShuttingDown(false) {
  mDisplay = new VRMockDisplay(this);
  for (int i = 0; i < kVRControllerMaxCount; i++) {
    mControllers.AppendElement(new VRMockController(this, i));
  }
  ClearAll();
}

gfx::VRSystemState& VRServiceTest::SystemState() { return mPendingState; }

VRMockDisplay* VRServiceTest::GetVRDisplay() { return mDisplay; }

VRMockController* VRServiceTest::GetVRController(uint32_t aControllerIdx,
                                                 ErrorResult& aRv) {
  if (aControllerIdx >= kVRControllerMaxCount) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return nullptr;
  }
  return mControllers[aControllerIdx];
}

void VRServiceTest::Shutdown() {
  MOZ_ASSERT(!mShuttingDown);
  mShuttingDown = true;
  mWindow = nullptr;
}

void VRServiceTest::AddCommand(uint64_t aCommand) {
  EncodeData();
  mCommandBuffer.AppendElement(aCommand);
}

void VRServiceTest::End() {
  AddCommand((uint64_t)VRPuppet_Command::VRPuppet_End);
}

void VRServiceTest::ClearAll() {
  memset(&mPendingState, 0, sizeof(VRSystemState));
  memset(&mEncodedState, 0, sizeof(VRSystemState));
  AddCommand((uint64_t)VRPuppet_Command::VRPuppet_ClearAll);
}

void VRServiceTest::ClearController(uint32_t aControllerIdx) {
  MOZ_ASSERT(aControllerIdx < kVRControllerMaxCount);
  mPendingState.controllerState[aControllerIdx].Clear();
  mEncodedState.controllerState[aControllerIdx].Clear();
  AddCommand((uint64_t)VRPuppet_Command::VRPuppet_ClearController |
             (uint64_t)aControllerIdx);
}

void VRServiceTest::Timeout(uint32_t aDuration) {
  AddCommand((uint64_t)VRPuppet_Command::VRPuppet_Timeout |
             (uint64_t)aDuration);
}

void VRServiceTest::Wait(uint32_t aDuration) {
  AddCommand((uint64_t)VRPuppet_Command::VRPuppet_Wait | (uint64_t)aDuration);
}

void VRServiceTest::WaitHapticIntensity(uint32_t aControllerIdx,
                                        uint32_t aHapticIdx, double aIntensity,
                                        ErrorResult& aRv) {
  if (aControllerIdx >= kVRControllerMaxCount) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }
  if (aHapticIdx >= kVRHapticsMaxCount) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }
  // convert to 16.16 fixed point.  This must match conversion in
  // VRPuppetCommandBuffer::RunCommand
  uint64_t iIntensity = round((float)aIntensity * (1 << 16));
  if (iIntensity > 0xffffffff) {
    iIntensity = 0xffffffff;
  }
  AddCommand((uint64_t)VRPuppet_Command::VRPuppet_WaitHapticIntensity |
             ((uint64_t)aControllerIdx << 40) | ((uint64_t)aHapticIdx << 32) |
             iIntensity);
}

void VRServiceTest::WaitSubmit() {
  AddCommand((uint64_t)VRPuppet_Command::VRPuppet_WaitSubmit);
}

void VRServiceTest::WaitPresentationStart() {
  AddCommand((uint64_t)VRPuppet_Command::VRPuppet_WaitPresentationStart);
}
void VRServiceTest::WaitPresentationEnd() {
  AddCommand((uint64_t)VRPuppet_Command::VRPuppet_WaitPresentationEnd);
}

void VRServiceTest::EncodeData() {
  VRPuppetCommandBuffer::EncodeStruct(
      mCommandBuffer, (uint8_t*)&mPendingState.displayState,
      (uint8_t*)&mEncodedState.displayState, sizeof(VRDisplayState),
      VRPuppet_Command::VRPuppet_UpdateDisplay);
  VRPuppetCommandBuffer::EncodeStruct(
      mCommandBuffer, (uint8_t*)&mPendingState.sensorState,
      (uint8_t*)&mEncodedState.sensorState, sizeof(VRHMDSensorState),
      VRPuppet_Command::VRPuppet_UpdateSensor);
  VRPuppetCommandBuffer::EncodeStruct(
      mCommandBuffer, (uint8_t*)&mPendingState.controllerState,
      (uint8_t*)&mEncodedState.controllerState,
      sizeof(VRControllerState) * kVRControllerMaxCount,
      VRPuppet_Command::VRPuppet_UpdateControllers);
}

void VRServiceTest::CaptureFrame() {
  AddCommand((uint64_t)VRPuppet_Command::VRPuppet_CaptureFrame);
}

void VRServiceTest::AcknowledgeFrame() {
  AddCommand((uint64_t)VRPuppet_Command::VRPuppet_AcknowledgeFrame);
}

void VRServiceTest::RejectFrame() {
  AddCommand((uint64_t)VRPuppet_Command::VRPuppet_RejectFrame);
}

void VRServiceTest::StartTimer() {
  AddCommand((uint64_t)VRPuppet_Command::VRPuppet_StartTimer);
}

void VRServiceTest::StopTimer() {
  AddCommand((uint64_t)VRPuppet_Command::VRPuppet_StopTimer);
}

void VRServiceTest::Commit() {
  AddCommand((uint64_t)VRPuppet_Command::VRPuppet_Commit);
}

already_AddRefed<Promise> VRServiceTest::Run(ErrorResult& aRv) {
  if (mShuttingDown) {
    return nullptr;
  }

  AddCommand((uint64_t)VRPuppet_Command::VRPuppet_End);

  RefPtr<dom::Promise> runPuppetPromise =
      Promise::Create(mWindow->AsGlobal(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  gfx::VRManagerChild* vm = gfx::VRManagerChild::Get();
  vm->RunPuppet(mCommandBuffer, runPuppetPromise, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  mCommandBuffer.Clear();

  return runPuppetPromise.forget();
}

already_AddRefed<Promise> VRServiceTest::Reset(ErrorResult& aRv) {
  if (mShuttingDown) {
    return nullptr;
  }

  RefPtr<dom::Promise> resetPuppetPromise =
      Promise::Create(mWindow->AsGlobal(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  gfx::VRManagerChild* vm = gfx::VRManagerChild::Get();
  vm->ResetPuppet(resetPuppetPromise, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  memset(&mPendingState, 0, sizeof(VRSystemState));
  memset(&mEncodedState, 0, sizeof(VRSystemState));
  mCommandBuffer.Clear();

  return resetPuppetPromise.forget();
}

}  // namespace dom
}  // namespace mozilla
