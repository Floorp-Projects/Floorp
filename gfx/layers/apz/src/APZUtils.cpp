/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZUtils.h"

#include "AsyncPanZoomController.h"

namespace mozilla {
namespace layers {
namespace apz {

/*static*/ void InitializeGlobalState() {
  MOZ_ASSERT(NS_IsMainThread());
  AsyncPanZoomController::InitializeGlobalState();
}

/*static*/ const ScreenMargin CalculatePendingDisplayPort(
    const FrameMetrics& aFrameMetrics, const ParentLayerPoint& aVelocity) {
  return AsyncPanZoomController::CalculatePendingDisplayPort(aFrameMetrics,
                                                             aVelocity);
}

/*static*/ bool IsCloseToHorizontal(float aAngle, float aThreshold) {
  return (aAngle < aThreshold || aAngle > (M_PI - aThreshold));
}

/*static*/ bool IsCloseToVertical(float aAngle, float aThreshold) {
  return (fabs(aAngle - (M_PI / 2)) < aThreshold);
}

}  // namespace apz
}  // namespace layers
}  // namespace mozilla
