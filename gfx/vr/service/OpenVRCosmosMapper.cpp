/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OpenVRCosmosMapper.h"

#include "moz_external_vr.h"
#include "VRSession.h"

namespace mozilla::gfx {

void OpenVRCosmosMapper::UpdateButtons(VRControllerState& aControllerState,
                                       ControllerInfo& aControllerInfo) {
  mNumButtons = mNumAxes = 0;
  // Button 0: Trigger
  GetTriggerValueFromAction(aControllerState,
                            aControllerInfo.mActionTrigger_Value);
  // Button 1: Grip
  GetButtonValueFromAction(aControllerState,
                           aControllerInfo.mActionGrip_Pressed,
                           aControllerInfo.mActionGrip_Touched);
  // Button 2: a placeholder button for trackpad.
  ++mNumButtons;
  // Button 3: Thumbstick
  GetButtonValueFromAction(aControllerState,
                           aControllerInfo.mActionThumbstick_Pressed,
                           aControllerInfo.mActionThumbstick_Touched);
  // Button 4: A
  GetButtonValueFromAction(aControllerState, aControllerInfo.mActionA_Pressed,
                           aControllerInfo.mActionA_Touched);
  // Button 5: B
  GetButtonValueFromAction(aControllerState, aControllerInfo.mActionB_Pressed,
                           aControllerInfo.mActionB_Touched);
  // Button 6: Bumper
  GetButtonValueFromAction(aControllerState,
                           aControllerInfo.mActionBumper_Pressed,
                           aControllerInfo.mActionBumper_Pressed);

  // Axis 0, 1: placeholder axes for touchpad.
  mNumAxes += 2;
  // Axis 2, 3: Thumbstick
  GetAxisValueFromAction(aControllerState,
                         aControllerInfo.mActionThumbstick_Analog);

  aControllerState.numButtons = mNumButtons;
  aControllerState.numAxes = mNumAxes;
}

}  // namespace mozilla::gfx
