/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OpenVRViveMapper.h"

#include "moz_external_vr.h"
#include "VRSession.h"

namespace mozilla::gfx {

void OpenVRViveMapper::UpdateButtons(VRControllerState& aControllerState,
                                     ControllerInfo& aControllerInfo) {
  mNumButtons = mNumAxes = 0;
  // Button 0: Trigger
  GetTriggerValueFromAction(aControllerState,
                            aControllerInfo.mActionTrigger_Value);
  // Button 1: Grip
  GetButtonValueFromAction(aControllerState,
                           aControllerInfo.mActionGrip_Pressed,
                           aControllerInfo.mActionGrip_Touched);
  // Button 2: Trackpad
  GetButtonValueFromAction(aControllerState,
                           aControllerInfo.mActionTrackpad_Pressed,
                           aControllerInfo.mActionTrackpad_Touched);
  // Button 3: a placeholder button for thumbstick.
  ++mNumButtons;
  // Button 4: Menu
  GetButtonValueFromAction(aControllerState,
                           aControllerInfo.mActionMenu_Pressed,
                           aControllerInfo.mActionMenu_Touched);

  // Axis 0, 1: Trackpad
  GetAxisValueFromAction(aControllerState,
                         aControllerInfo.mActionTrackpad_Analog);

  aControllerState.numButtons = mNumButtons;
  aControllerState.numAxes = mNumAxes;
}

}  // namespace mozilla::gfx
