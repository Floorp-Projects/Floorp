/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XRNativeOriginViewer.h"
#include "VRDisplayClient.h"

namespace mozilla::dom {

XRNativeOriginViewer::XRNativeOriginViewer(gfx::VRDisplayClient* aDisplay)
    : mDisplay(aDisplay) {
  MOZ_ASSERT(aDisplay);
}

gfx::PointDouble3D XRNativeOriginViewer::GetPosition() {
  const gfx::VRHMDSensorState& sensorState = mDisplay->GetSensorState();
  return gfx::PointDouble3D(sensorState.pose.position[0],
                            sensorState.pose.position[1],
                            sensorState.pose.position[2]);
}

gfx::QuaternionDouble XRNativeOriginViewer::GetOrientation() {
  const gfx::VRHMDSensorState& sensorState = mDisplay->GetSensorState();
  return gfx::QuaternionDouble(
      sensorState.pose.orientation[0], sensorState.pose.orientation[1],
      sensorState.pose.orientation[2], sensorState.pose.orientation[3]);
}

}  // namespace mozilla::dom
