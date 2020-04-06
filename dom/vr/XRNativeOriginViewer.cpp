/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XRNativeOriginViewer.h"

namespace mozilla {
namespace dom {

XRNativeOriginViewer::XRNativeOriginViewer(VRDisplayClient* aDisplay)
    : mDisplay(aDisplay) {
  MOZ_ASSERT(aDisplay);
}

gfx::PointDouble3D XRNativeOriginViewer::GetPosition() {
  const gfx::VRHMDSensorState& sensorState = mDisplay->GetSensorState();
  return gfx::PointDouble3D(sensorState.pose.position[0],
                            sensorState.pose.position[1],
                            sensorState.pose.position[2]);
}

}  // namespace dom
}  // namespace mozilla
