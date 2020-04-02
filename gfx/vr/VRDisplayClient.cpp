/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "prlink.h"
#include "prenv.h"

#include "nsIGlobalObject.h"
#include "nsRefPtrHashtable.h"
#include "nsString.h"
#include "mozilla/dom/GamepadManager.h"
#include "mozilla/dom/Gamepad.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "nsServiceManagerUtils.h"

#ifdef XP_WIN
#  include "../layers/d3d11/CompositorD3D11.h"
#endif

#include "VRDisplayClient.h"
#include "VRDisplayPresentation.h"
#include "VRManagerChild.h"
#include "VRLayerChild.h"

using namespace mozilla;
using namespace mozilla::gfx;

VRDisplayClient::VRDisplayClient(const VRDisplayInfo& aDisplayInfo)
    : mDisplayInfo(aDisplayInfo),
      bLastEventWasMounted(false),
      bLastEventWasPresenting(false),
      mPresentationCount(0),
      mLastEventFrameId(0),
      mLastPresentingGeneration(0),
      mLastEventControllerState{} {
  MOZ_COUNT_CTOR(VRDisplayClient);
}

VRDisplayClient::~VRDisplayClient() { MOZ_COUNT_DTOR(VRDisplayClient); }

void VRDisplayClient::UpdateDisplayInfo(const VRDisplayInfo& aDisplayInfo) {
  mDisplayInfo = aDisplayInfo;
  FireEvents();
}

already_AddRefed<VRDisplayPresentation> VRDisplayClient::BeginPresentation(
    const nsTArray<mozilla::dom::VRLayer>& aLayers, uint32_t aGroup) {
  ++mPresentationCount;
  RefPtr<VRDisplayPresentation> presentation =
      new VRDisplayPresentation(this, aLayers, aGroup);
  return presentation.forget();
}

void VRDisplayClient::PresentationDestroyed() { --mPresentationCount; }

void VRDisplayClient::SetGroupMask(uint32_t aGroupMask) {
  VRManagerChild* vm = VRManagerChild::Get();
  vm->SendSetGroupMask(mDisplayInfo.mDisplayID, aGroupMask);
}

bool VRDisplayClient::IsPresentationGenerationCurrent() const {
  if (mLastPresentingGeneration !=
      mDisplayInfo.mDisplayState.presentingGeneration) {
    return false;
  }

  return true;
}

void VRDisplayClient::MakePresentationGenerationCurrent() {
  mLastPresentingGeneration = mDisplayInfo.mDisplayState.presentingGeneration;
}

void VRDisplayClient::FireEvents() {
  RefPtr<VRManagerChild> vm = VRManagerChild::Get();
  // Only fire these events for non-chrome VR sessions
  bool isPresenting = (mDisplayInfo.mPresentingGroups & kVRGroupContent) != 0;

  // Check if we need to trigger onVRDisplayPresentChange event
  if (bLastEventWasPresenting != isPresenting) {
    bLastEventWasPresenting = isPresenting;
    vm->FireDOMVRDisplayPresentChangeEvent(mDisplayInfo.mDisplayID);
  }

  // Check if we need to trigger onvrdisplayactivate event
  if (!bLastEventWasMounted && mDisplayInfo.mDisplayState.isMounted) {
    bLastEventWasMounted = true;
    if (StaticPrefs::dom_vr_autoactivate_enabled()) {
      vm->FireDOMVRDisplayMountedEvent(mDisplayInfo.mDisplayID);
    }
  }

  // Check if we need to trigger onvrdisplaydeactivate event
  if (bLastEventWasMounted && !mDisplayInfo.mDisplayState.isMounted) {
    bLastEventWasMounted = false;
    if (StaticPrefs::dom_vr_autoactivate_enabled()) {
      vm->FireDOMVRDisplayUnmountedEvent(mDisplayInfo.mDisplayID);
    }
  }

  if (mLastPresentingGeneration !=
      mDisplayInfo.mDisplayState.presentingGeneration) {
    mLastPresentingGeneration = mDisplayInfo.mDisplayState.presentingGeneration;
    vm->NotifyPresentationGenerationChanged(mDisplayInfo.mDisplayID);
  }

  // Check if we need to trigger VRDisplay.requestAnimationFrame
  if (mLastEventFrameId != mDisplayInfo.mFrameId) {
    mLastEventFrameId = mDisplayInfo.mFrameId;
    vm->RunFrameRequestCallbacks();
  }

  // We only call FireGamepadEvents() in WebVR instead of WebXR
  FireGamepadEvents();
}

void VRDisplayClient::GamepadMappingForWebVR(
    VRControllerState& aControllerState) {
  float triggerValue[kVRControllerMaxButtons];
  memcpy(triggerValue, aControllerState.triggerValue,
         sizeof(aControllerState.triggerValue));
  const uint64_t buttonPressed = aControllerState.buttonPressed;
  const uint64_t buttonTouched = aControllerState.buttonTouched;

  auto SetTriggerValue = [&](uint64_t newSlot, uint64_t oldSlot) {
    aControllerState.triggerValue[newSlot] = triggerValue[oldSlot];
  };
  auto ShiftButtonBitForNewSlot = [&](uint64_t newSlot, uint64_t oldSlot,
                                      bool aIsTouch = false) {
    if (aIsTouch) {
      return ((buttonTouched & (1ULL << oldSlot)) != 0) * (1ULL << newSlot);
    }
    SetTriggerValue(newSlot, oldSlot);
    return ((buttonPressed & (1ULL << oldSlot)) != 0) * (1ULL << newSlot);
  };

  switch (aControllerState.type) {
    case VRControllerType::HTCVive:
      aControllerState.buttonPressed =
          ShiftButtonBitForNewSlot(1, 0) | ShiftButtonBitForNewSlot(2, 1) |
          ShiftButtonBitForNewSlot(0, 2) | ShiftButtonBitForNewSlot(3, 4);
      aControllerState.buttonTouched = ShiftButtonBitForNewSlot(0, 1, true) |
                                       ShiftButtonBitForNewSlot(2, 1, true) |
                                       ShiftButtonBitForNewSlot(0, 2, true) |
                                       ShiftButtonBitForNewSlot(3, 4, true);
      aControllerState.numButtons = 4;
      aControllerState.numAxes = 2;
      break;
    case VRControllerType::MSMR:
      aControllerState.buttonPressed =
          ShiftButtonBitForNewSlot(1, 0) | ShiftButtonBitForNewSlot(2, 1) |
          ShiftButtonBitForNewSlot(0, 2) | ShiftButtonBitForNewSlot(3, 3) |
          ShiftButtonBitForNewSlot(4, 4);
      aControllerState.buttonTouched = ShiftButtonBitForNewSlot(1, 0, true) |
                                       ShiftButtonBitForNewSlot(2, 1, true) |
                                       ShiftButtonBitForNewSlot(0, 2, true) |
                                       ShiftButtonBitForNewSlot(3, 3, true) |
                                       ShiftButtonBitForNewSlot(4, 4, true);
      aControllerState.numButtons = 5;
      aControllerState.numAxes = 4;
      break;
    case VRControllerType::HTCViveCosmos:
      aControllerState.buttonPressed =
          ShiftButtonBitForNewSlot(0, 0) | ShiftButtonBitForNewSlot(1, 1) |
          ShiftButtonBitForNewSlot(4, 3) | ShiftButtonBitForNewSlot(2, 4) |
          ShiftButtonBitForNewSlot(3, 5) | ShiftButtonBitForNewSlot(5, 6);
      aControllerState.buttonTouched = ShiftButtonBitForNewSlot(0, 0, true) |
                                       ShiftButtonBitForNewSlot(1, 1, true) |
                                       ShiftButtonBitForNewSlot(4, 3, true) |
                                       ShiftButtonBitForNewSlot(2, 4, true) |
                                       ShiftButtonBitForNewSlot(3, 5, true) |
                                       ShiftButtonBitForNewSlot(5, 6, true);
      aControllerState.axisValue[0] = aControllerState.axisValue[2];
      aControllerState.axisValue[1] = aControllerState.axisValue[3];
      aControllerState.numButtons = 6;
      aControllerState.numAxes = 2;
      break;
    case VRControllerType::HTCViveFocus:
      aControllerState.buttonPressed =
          ShiftButtonBitForNewSlot(0, 2) | ShiftButtonBitForNewSlot(1, 0);
      aControllerState.buttonTouched = ShiftButtonBitForNewSlot(0, 2, true) |
                                       ShiftButtonBitForNewSlot(1, 0, true);
      aControllerState.numButtons = 2;
      aControllerState.numAxes = 2;
      break;
    case VRControllerType::HTCViveFocusPlus:
      aControllerState.buttonPressed = ShiftButtonBitForNewSlot(0, 2) |
                                       ShiftButtonBitForNewSlot(1, 0) |
                                       ShiftButtonBitForNewSlot(2, 1);
      aControllerState.buttonTouched = ShiftButtonBitForNewSlot(0, 2, true) |
                                       ShiftButtonBitForNewSlot(1, 0, true) |
                                       ShiftButtonBitForNewSlot(2, 1, true);
      aControllerState.numButtons = 3;
      aControllerState.numAxes = 2;
      break;
    case VRControllerType::OculusGo:
      aControllerState.buttonPressed =
          ShiftButtonBitForNewSlot(0, 2) | ShiftButtonBitForNewSlot(1, 0);
      aControllerState.buttonTouched = ShiftButtonBitForNewSlot(0, 2, true) |
                                       ShiftButtonBitForNewSlot(1, 0, true);
      aControllerState.numButtons = 2;
      aControllerState.numAxes = 2;
      break;
    case VRControllerType::OculusTouch:
      aControllerState.buttonPressed =
          ShiftButtonBitForNewSlot(0, 3) | ShiftButtonBitForNewSlot(1, 0) |
          ShiftButtonBitForNewSlot(2, 1) | ShiftButtonBitForNewSlot(3, 4) |
          ShiftButtonBitForNewSlot(4, 5) | ShiftButtonBitForNewSlot(5, 6);
      aControllerState.buttonTouched = ShiftButtonBitForNewSlot(0, 3, true) |
                                       ShiftButtonBitForNewSlot(1, 0, true) |
                                       ShiftButtonBitForNewSlot(2, 1, true) |
                                       ShiftButtonBitForNewSlot(3, 4, true) |
                                       ShiftButtonBitForNewSlot(4, 5, true) |
                                       ShiftButtonBitForNewSlot(5, 6, true);
      aControllerState.axisValue[0] = aControllerState.axisValue[2];
      aControllerState.axisValue[1] = aControllerState.axisValue[3];
      aControllerState.numButtons = 6;
      aControllerState.numAxes = 2;
      break;
    case VRControllerType::OculusTouch2:
      aControllerState.buttonPressed =
          ShiftButtonBitForNewSlot(0, 3) | ShiftButtonBitForNewSlot(1, 0) |
          ShiftButtonBitForNewSlot(2, 1) | ShiftButtonBitForNewSlot(3, 4) |
          ShiftButtonBitForNewSlot(4, 5) | ShiftButtonBitForNewSlot(5, 6);
      aControllerState.buttonTouched = ShiftButtonBitForNewSlot(0, 3, true) |
                                       ShiftButtonBitForNewSlot(1, 0, true) |
                                       ShiftButtonBitForNewSlot(2, 1, true) |
                                       ShiftButtonBitForNewSlot(3, 4, true) |
                                       ShiftButtonBitForNewSlot(4, 5, true) |
                                       ShiftButtonBitForNewSlot(5, 6, true);
      aControllerState.axisValue[0] = aControllerState.axisValue[2];
      aControllerState.axisValue[1] = aControllerState.axisValue[3];
      aControllerState.numButtons = 6;
      aControllerState.numAxes = 2;
      break;
    case VRControllerType::ValveIndex:
      aControllerState.buttonPressed =
          ShiftButtonBitForNewSlot(1, 0) | ShiftButtonBitForNewSlot(2, 1) |
          ShiftButtonBitForNewSlot(0, 2) | ShiftButtonBitForNewSlot(5, 3) |
          ShiftButtonBitForNewSlot(3, 4) | ShiftButtonBitForNewSlot(4, 5) |
          ShiftButtonBitForNewSlot(6, 6) | ShiftButtonBitForNewSlot(7, 7) |
          ShiftButtonBitForNewSlot(8, 8) | ShiftButtonBitForNewSlot(9, 9);
      aControllerState.buttonTouched = ShiftButtonBitForNewSlot(1, 0, true) |
                                       ShiftButtonBitForNewSlot(2, 1, true) |
                                       ShiftButtonBitForNewSlot(0, 2, true) |
                                       ShiftButtonBitForNewSlot(5, 3, true) |
                                       ShiftButtonBitForNewSlot(3, 4, true) |
                                       ShiftButtonBitForNewSlot(4, 5, true) |
                                       ShiftButtonBitForNewSlot(6, 6, true) |
                                       ShiftButtonBitForNewSlot(7, 7, true) |
                                       ShiftButtonBitForNewSlot(8, 8, true) |
                                       ShiftButtonBitForNewSlot(9, 9, true);
      aControllerState.numButtons = 10;
      aControllerState.numAxes = 4;
      break;
    case VRControllerType::PicoGaze:
      aControllerState.buttonPressed = ShiftButtonBitForNewSlot(0, 0);
      aControllerState.buttonTouched = ShiftButtonBitForNewSlot(0, 0, true);
      aControllerState.numButtons = 1;
      aControllerState.numAxes = 0;
      break;
    case VRControllerType::PicoG2:
      aControllerState.buttonPressed =
          ShiftButtonBitForNewSlot(0, 2) | ShiftButtonBitForNewSlot(1, 0);
      aControllerState.buttonTouched = ShiftButtonBitForNewSlot(0, 2, true) |
                                       ShiftButtonBitForNewSlot(1, 0, true);
      aControllerState.numButtons = 2;
      aControllerState.numAxes = 2;
      break;
    case VRControllerType::PicoNeo2:
      aControllerState.buttonPressed =
          ShiftButtonBitForNewSlot(0, 3) | ShiftButtonBitForNewSlot(1, 0) |
          ShiftButtonBitForNewSlot(2, 1) | ShiftButtonBitForNewSlot(3, 4) |
          ShiftButtonBitForNewSlot(4, 5) | ShiftButtonBitForNewSlot(5, 6);
      aControllerState.buttonTouched = ShiftButtonBitForNewSlot(0, 3, true) |
                                       ShiftButtonBitForNewSlot(1, 0, true) |
                                       ShiftButtonBitForNewSlot(2, 1, true) |
                                       ShiftButtonBitForNewSlot(3, 4, true) |
                                       ShiftButtonBitForNewSlot(4, 5, true) |
                                       ShiftButtonBitForNewSlot(5, 6, true);
      aControllerState.axisValue[0] = aControllerState.axisValue[2];
      aControllerState.axisValue[1] = aControllerState.axisValue[3];
      aControllerState.numButtons = 6;
      aControllerState.numAxes = 2;
      break;
    default:
      // Undefined controller types, we will keep its the same order.
      break;
  }
}

void VRDisplayClient::FireGamepadEvents() {
  RefPtr<dom::GamepadManager> gamepadManager(dom::GamepadManager::GetService());
  if (!gamepadManager) {
    return;
  }
  for (int stateIndex = 0; stateIndex < kVRControllerMaxCount; stateIndex++) {
    VRControllerState state = {}, lastState = {};
    memcpy(&state, &mDisplayInfo.mControllerState[stateIndex],
           sizeof(VRControllerState));
    memcpy(&lastState, &mLastEventControllerState[stateIndex],
           sizeof(VRControllerState));
    GamepadMappingForWebVR(state);
    GamepadMappingForWebVR(lastState);

    uint32_t gamepadId =
        mDisplayInfo.mDisplayID * kVRControllerMaxCount + stateIndex;
    bool bIsNew = false;

    // Send events to notify that controllers are removed
    if (state.controllerName[0] == '\0') {
      // Controller is not present
      if (lastState.controllerName[0] != '\0') {
        // Controller has been removed
        dom::GamepadRemoved info;
        dom::GamepadChangeEventBody body(info);
        dom::GamepadChangeEvent event(gamepadId, dom::GamepadServiceType::VR,
                                      body);
        gamepadManager->Update(event);
      }
      // Do not process any further events for removed controllers
      continue;
    }

    // Send events to notify that new controllers are added
    RefPtr<dom::Gamepad> existing =
        gamepadManager->GetGamepad(gamepadId, dom::GamepadServiceType::VR);
    // ControllerState in OpenVR action-based API gets delay to query btn and
    // axis count. So, we need to check if they are more than zero.
    if ((lastState.controllerName[0] == '\0' || !existing) &&
        (state.numButtons > 0 || state.numAxes > 0)) {
      dom::GamepadAdded info(NS_ConvertUTF8toUTF16(state.controllerName),
                             dom::GamepadMappingType::_empty, state.hand,
                             mDisplayInfo.mDisplayID, state.numButtons,
                             state.numAxes, state.numHaptics, 0, 0);
      dom::GamepadChangeEventBody body(info);
      dom::GamepadChangeEvent event(gamepadId, dom::GamepadServiceType::VR,
                                    body);
      gamepadManager->Update(event);
      bIsNew = true;
    }

    // Send events for handedness changes
    if (state.hand != lastState.hand) {
      dom::GamepadHandInformation info(state.hand);
      dom::GamepadChangeEventBody body(info);
      dom::GamepadChangeEvent event(gamepadId, dom::GamepadServiceType::VR,
                                    body);
      gamepadManager->Update(event);
    }

    // Send events for axis value changes
    for (uint32_t axisIndex = 0; axisIndex < state.numAxes; axisIndex++) {
      if (state.axisValue[axisIndex] != lastState.axisValue[axisIndex]) {
        dom::GamepadAxisInformation info(axisIndex, state.axisValue[axisIndex]);
        dom::GamepadChangeEventBody body(info);
        dom::GamepadChangeEvent event(gamepadId, dom::GamepadServiceType::VR,
                                      body);
        gamepadManager->Update(event);
      }
    }

    // Send events for trigger, touch, and button value changes
    if (!bIsNew) {
      // When a new controller is added, we do not emit button events for
      // the initial state of the inputs.
      for (uint32_t buttonIndex = 0; buttonIndex < state.numButtons;
           buttonIndex++) {
        bool bPressed = (state.buttonPressed & (1ULL << buttonIndex)) != 0;
        bool bTouched = (state.buttonTouched & (1ULL << buttonIndex)) != 0;
        bool bLastPressed =
            (lastState.buttonPressed & (1ULL << buttonIndex)) != 0;
        bool bLastTouched =
            (lastState.buttonTouched & (1ULL << buttonIndex)) != 0;

        if (state.triggerValue[buttonIndex] !=
                lastState.triggerValue[buttonIndex] ||
            bPressed != bLastPressed || bTouched != bLastTouched) {
          dom::GamepadButtonInformation info(
              buttonIndex, state.triggerValue[buttonIndex], bPressed, bTouched);
          dom::GamepadChangeEventBody body(info);
          dom::GamepadChangeEvent event(gamepadId, dom::GamepadServiceType::VR,
                                        body);
          gamepadManager->Update(event);
        }
      }
    }

    // Send events for pose changes
    // Note that VRPose is asserted to be a POD type so memcmp is safe
    if (state.flags != lastState.flags ||
        state.isPositionValid != lastState.isPositionValid ||
        state.isOrientationValid != lastState.isOrientationValid ||
        memcmp(&state.pose, &lastState.pose, sizeof(VRPose)) != 0) {
      // Convert pose to GamepadPoseState
      dom::GamepadPoseState poseState;
      poseState.Clear();
      poseState.flags = state.flags;

      // Orientation values
      poseState.isOrientationValid = state.isOrientationValid;
      poseState.orientation[0] = state.pose.orientation[0];
      poseState.orientation[1] = state.pose.orientation[1];
      poseState.orientation[2] = state.pose.orientation[2];
      poseState.orientation[3] = state.pose.orientation[3];
      poseState.angularVelocity[0] = state.pose.angularVelocity[0];
      poseState.angularVelocity[1] = state.pose.angularVelocity[1];
      poseState.angularVelocity[2] = state.pose.angularVelocity[2];
      poseState.angularAcceleration[0] = state.pose.angularAcceleration[0];
      poseState.angularAcceleration[1] = state.pose.angularAcceleration[1];
      poseState.angularAcceleration[2] = state.pose.angularAcceleration[2];

      // Position values
      poseState.isPositionValid = state.isPositionValid;
      poseState.position[0] = state.pose.position[0];
      poseState.position[1] = state.pose.position[1];
      poseState.position[2] = state.pose.position[2];
      poseState.linearVelocity[0] = state.pose.linearVelocity[0];
      poseState.linearVelocity[1] = state.pose.linearVelocity[1];
      poseState.linearVelocity[2] = state.pose.linearVelocity[2];
      poseState.linearAcceleration[0] = state.pose.linearAcceleration[0];
      poseState.linearAcceleration[1] = state.pose.linearAcceleration[1];
      poseState.linearAcceleration[2] = state.pose.linearAcceleration[2];

      // Send the event
      dom::GamepadPoseInformation info(poseState);
      dom::GamepadChangeEventBody body(info);
      dom::GamepadChangeEvent event(gamepadId, dom::GamepadServiceType::VR,
                                    body);
      gamepadManager->Update(event);
    }
  }

  // Note that VRControllerState is asserted to be a POD type and memcpy is
  // safe.
  memcpy(mLastEventControllerState, mDisplayInfo.mControllerState,
         sizeof(VRControllerState) * kVRControllerMaxCount);
}

const VRHMDSensorState& VRDisplayClient::GetSensorState() const {
  return mDisplayInfo.GetSensorState();
}

bool VRDisplayClient::GetIsConnected() const {
  return mDisplayInfo.GetIsConnected();
}

bool VRDisplayClient::IsPresenting() {
  return mDisplayInfo.mPresentingGroups != 0;
}

void VRDisplayClient::NotifyDisconnected() {
  mDisplayInfo.mDisplayState.isConnected = false;
}

void VRDisplayClient::UpdateSubmitFrameResult(
    const VRSubmitFrameResultInfo& aResult) {
  mSubmitFrameResult = aResult;
}

void VRDisplayClient::GetSubmitFrameResult(VRSubmitFrameResultInfo& aResult) {
  aResult = mSubmitFrameResult;
}

void VRDisplayClient::StartVRNavigation() {
  /**
   * A VR-to-VR site navigation has started, notify VRManager
   * so we don't drop out of VR during the transition
   */
  VRManagerChild* vm = VRManagerChild::Get();
  vm->SendStartVRNavigation(mDisplayInfo.mDisplayID);
}

void VRDisplayClient::StopVRNavigation(const TimeDuration& aTimeout) {
  /**
   * A VR-to-VR site navigation has ended and the new site
   * has received a vrdisplayactivate event.
   * Don't actually consider the navigation transition over
   * until aTimeout has elapsed.
   * This may be called multiple times, in which case the timeout
   * should be reset to aTimeout.
   * When aTimeout is TimeDuration(0), we should consider the
   * transition immediately ended.
   */
  VRManagerChild* vm = VRManagerChild::Get();
  vm->SendStopVRNavigation(mDisplayInfo.mDisplayID, aTimeout);
}
