/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XRNativeOriginLocalFloor.h"

namespace mozilla {
namespace dom {

XRNativeOriginLocalFloor::XRNativeOriginLocalFloor(VRDisplayClient* aDisplay)
    : mDisplay(aDisplay), mInitialPositionValid(false) {
  MOZ_ASSERT(aDisplay);

  // To avoid fingerprinting, we offset the floor height by 5cm.
  // This will always result in the floor being lower than the
  // real floor in order to avoid breaking content that expects
  // you to pick objects up off the floor.
  //
  // TODO (Bug 1616394) - Convert this constant to a pref.
  const double kFloorFuzz = 0.05f;  // Meters
  mFloorRandom = double(rand()) / double(RAND_MAX) * kFloorFuzz;
}

gfx::PointDouble3D XRNativeOriginLocalFloor::GetPosition() {
  // Keep returning {0,-fuzz,0} until a position can be found
  const auto standing =
      mDisplay->GetDisplayInfo().GetSittingToStandingTransform();
  if (!mInitialPositionValid || standing != mStandingTransform) {
    const gfx::VRHMDSensorState& sensorState = mDisplay->GetSensorState();
    gfx::PointDouble3D origin;
    if (sensorState.flags & VRDisplayCapabilityFlags::Cap_Position) {
      mInitialPosition.x = sensorState.pose.position[0];
      mInitialPosition.y = -mFloorRandom - standing._42;
      mInitialPosition.z = sensorState.pose.position[2];
      mInitialPositionValid = true;
      mStandingTransform = standing;
    }
  }
  return mInitialPosition;
}

}  // namespace dom
}  // namespace mozilla
