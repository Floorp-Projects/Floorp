/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_SERVICE_OPENVRCONTROLLERMAPPER_H
#define GFX_VR_SERVICE_OPENVRCONTROLLERMAPPER_H

#include "openvr.h"
#include "nsString.h"

#include "moz_external_vr.h"

namespace mozilla {
namespace gfx {

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
  // It seems like there's no way to get response from a sys. btn.
  ControllerAction mActionSystem_Pressed;
  ControllerAction mActionSystem_Touched;

  // --- Knuckles & Cosmos
  ControllerAction mActionA_Pressed;
  ControllerAction mActionA_Touched;
  ControllerAction mActionB_Pressed;
  ControllerAction mActionB_Touched;

  // --- Knuckles, Cosmos, and WMR
  ControllerAction mActionThumbstick_Analog;
  ControllerAction mActionThumbstick_Pressed;
  ControllerAction mActionThumbstick_Touched;

  // --- Knuckles
  ControllerAction mActionFingerIndex_Value;
  ControllerAction mActionFingerMiddle_Value;
  ControllerAction mActionFingerRing_Value;
  ControllerAction mActionFingerPinky_Value;

  // --- Cosmos
  ControllerAction mActionBumper_Pressed;
};

class OpenVRControllerMapper {
 public:
  OpenVRControllerMapper();
  virtual ~OpenVRControllerMapper() = default;

  virtual void UpdateButtons(VRControllerState& aControllerState,
                             ControllerInfo& aControllerInfo) = 0;
  uint32_t GetButtonCount() { return mNumButtons; }
  uint32_t GetAxisCount() { return mNumAxes; }

 protected:
  void GetButtonValueFromAction(VRControllerState& aControllerState,
                                const ControllerAction& aPressAction,
                                const ControllerAction& aTouchAction);
  void GetTriggerValueFromAction(VRControllerState& aControllerState,
                                 const ControllerAction& aAction);
  void GetAxisValueFromAction(VRControllerState& aControllerState,
                              const ControllerAction& aAction,
                              bool aInvertAxis = false);
  uint32_t mNumButtons;
  uint32_t mNumAxes;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // GFX_VR_SERVICE_OPENVRCONTROLLERMAPPER_H
