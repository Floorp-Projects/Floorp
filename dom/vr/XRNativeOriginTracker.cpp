/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XRNativeOriginTracker.h"

namespace mozilla {
namespace dom {

XRNativeOriginTracker::XRNativeOriginTracker(const gfx::VRPose* aPose)
    : mPose(aPose) {
  MOZ_ASSERT(aPose);
}

gfx::PointDouble3D XRNativeOriginTracker::GetPosition() {
  MOZ_ASSERT(mPose);
  return gfx::PointDouble3D(mPose->position[0],
                            mPose->position[1],
                            mPose->position[2]);
}

gfx::QuaternionDouble XRNativeOriginTracker::GetOrientation() {
  MOZ_ASSERT(mPose);
  gfx::QuaternionDouble orientation(mPose->orientation[0],
    mPose->orientation[1], mPose->orientation[2], mPose->orientation[3]);
  // Quaternion was inverted for WebVR in XXXVRSession when handling controller poses.
  // We need to re-invert it here again.
  // TODO: Remove those extra inverts when WebVR support is disabled.
  orientation.Invert();
  return orientation;
}

}  // namespace dom
}  // namespace mozilla
