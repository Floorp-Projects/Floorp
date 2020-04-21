/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XRNativeOriginLocal.h"

namespace mozilla {
namespace dom {

XRNativeOriginLocal::XRNativeOriginLocal(VRDisplayClient* aDisplay)
    : mDisplay(aDisplay), mInitialPositionValid(false) {
  MOZ_ASSERT(aDisplay);
}

gfx::PointDouble3D XRNativeOriginLocal::GetPosition() {
  // Keep returning {0,0,0} until a position can be found
  if (!mInitialPositionValid) {
    const gfx::VRHMDSensorState& sensorState = mDisplay->GetSensorState();
    gfx::PointDouble3D origin;
    if (sensorState.flags & VRDisplayCapabilityFlags::Cap_Position ||
        sensorState.flags & VRDisplayCapabilityFlags::Cap_PositionEmulated) {
      mInitialPosition.x = sensorState.pose.position[0];
      mInitialPosition.y = sensorState.pose.position[1];
      mInitialPosition.z = sensorState.pose.position[2];
      mInitialPositionValid = true;
    }
  }
  return mInitialPosition;
}

}  // namespace dom
}  // namespace mozilla
