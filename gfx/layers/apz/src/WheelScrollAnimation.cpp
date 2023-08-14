/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WheelScrollAnimation.h"

#include <tuple>
#include "AsyncPanZoomController.h"
#include "mozilla/StaticPrefs_general.h"
#include "mozilla/layers/APZPublicUtils.h"
#include "nsPoint.h"
#include "ScrollAnimationBezierPhysics.h"

namespace mozilla {
namespace layers {

static ScrollOrigin OriginForDeltaType(
    ScrollWheelInput::ScrollDeltaType aDeltaType) {
  switch (aDeltaType) {
    case ScrollWheelInput::SCROLLDELTA_PAGE:
      return ScrollOrigin::Pages;
    case ScrollWheelInput::SCROLLDELTA_PIXEL:
      return ScrollOrigin::Pixels;
    case ScrollWheelInput::SCROLLDELTA_LINE:
      return ScrollOrigin::MouseWheel;
  }
  // Shouldn't happen, pick a default.
  return ScrollOrigin::MouseWheel;
}

WheelScrollAnimation::WheelScrollAnimation(
    AsyncPanZoomController& aApzc, const nsPoint& aInitialPosition,
    ScrollWheelInput::ScrollDeltaType aDeltaType)
    : GenericScrollAnimation(aApzc, aInitialPosition,
                             OriginForDeltaType(aDeltaType)) {
  MOZ_ASSERT(StaticPrefs::general_smoothScroll(),
             "We shouldn't be creating a WheelScrollAnimation if smooth "
             "scrolling is disabled");
  mDirectionForcedToOverscroll =
      mApzc.mScrollMetadata.GetDisregardedDirection();
}

}  // namespace layers
}  // namespace mozilla
