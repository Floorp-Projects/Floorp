/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OpenVRControllerMapper.h"
#include "mozilla/StaticPrefs_dom.h"

#include "VRSession.h"

namespace mozilla::gfx {

OpenVRControllerMapper::OpenVRControllerMapper()
    : mNumButtons(0), mNumAxes(0) {}

void OpenVRControllerMapper::GetButtonValueFromAction(
    VRControllerState& aControllerState, const ControllerAction& aPressAction,
    const ControllerAction& aTouchAction) {
  vr::InputDigitalActionData_t actionData = {};
  bool bPressed = false;
  bool bTouched = false;
  uint64_t mask = 0;

  if (aPressAction.handle &&
      vr::VRInput()->GetDigitalActionData(
          aPressAction.handle, &actionData, sizeof(actionData),
          vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
      actionData.bActive) {
    bPressed = actionData.bState;
    mask = (1ULL << mNumButtons);
    aControllerState.triggerValue[mNumButtons] = bPressed ? 1.0 : 0.0f;
    if (bPressed) {
      aControllerState.buttonPressed |= mask;
    } else {
      aControllerState.buttonPressed &= ~mask;
    }
    if (aTouchAction.handle &&
        vr::VRInput()->GetDigitalActionData(
            aTouchAction.handle, &actionData, sizeof(actionData),
            vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None) {
      bTouched = actionData.bActive && actionData.bState;
      mask = (1ULL << mNumButtons);
      if (bTouched) {
        aControllerState.buttonTouched |= mask;
      } else {
        aControllerState.buttonTouched &= ~mask;
      }
    }
    ++mNumButtons;
  }
}

void OpenVRControllerMapper::GetTriggerValueFromAction(
    VRControllerState& aControllerState, const ControllerAction& aAction) {
  vr::InputAnalogActionData_t analogData = {};
  const float triggerThreshold =
      StaticPrefs::dom_vr_controller_trigger_threshold();

  if (aAction.handle &&
      vr::VRInput()->GetAnalogActionData(
          aAction.handle, &analogData, sizeof(analogData),
          vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
      analogData.bActive) {
    VRSession::UpdateTrigger(aControllerState, mNumButtons, analogData.x,
                             triggerThreshold);
    ++mNumButtons;
  }
}

void OpenVRControllerMapper::GetAxisValueFromAction(
    VRControllerState& aControllerState, const ControllerAction& aAction,
    bool aInvertAxis) {
  vr::InputAnalogActionData_t analogData = {};
  const float yAxisInvert = (aInvertAxis) ? -1.0f : 1.0f;

  if (aAction.handle &&
      vr::VRInput()->GetAnalogActionData(
          aAction.handle, &analogData, sizeof(analogData),
          vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
      analogData.bActive) {
    aControllerState.axisValue[mNumAxes] = analogData.x;
    ++mNumAxes;
    aControllerState.axisValue[mNumAxes] = analogData.y * yAxisInvert;
    ++mNumAxes;
  }
}

}  // namespace mozilla::gfx
