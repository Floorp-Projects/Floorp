/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XRInputSpace.h"

namespace mozilla {
namespace dom {

XRInputSpace::XRInputSpace(nsIGlobalObject* aParent, XRSession* aSession,
                           XRNativeOrigin* aNativeOrigin,
                           int32_t aControllerIndex)
    : XRSpace(aParent, aSession, aNativeOrigin), mIndex(aControllerIndex) {}

bool XRInputSpace::IsPositionEmulated() const {
  gfx::VRDisplayClient* display = mSession->GetDisplayClient();
  if (!display) {
    // When there are no sensors, the position is considered emulated.
    return true;
  }
  const gfx::VRDisplayInfo& displayInfo = display->GetDisplayInfo();
  const gfx::VRControllerState& controllerState =
      displayInfo.mControllerState[mIndex];
  MOZ_ASSERT(controllerState.controllerName[0] != '\0');

  return (
      (controllerState.flags & GamepadCapabilityFlags::Cap_PositionEmulated) !=
      GamepadCapabilityFlags::Cap_None);
}

}  // namespace dom
}  // namespace mozilla
