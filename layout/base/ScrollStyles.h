/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ScrollStyles_h
#define mozilla_ScrollStyles_h

#include <stdint.h>
#include "nsStyleConsts.h"
#include "nsStyleCoord.h"   // for nsStyleCoord
#include "mozilla/dom/WindowBinding.h"

// Forward declarations
struct nsStyleDisplay;

namespace mozilla {

struct ScrollStyles {
  // Always one of NS_STYLE_OVERFLOW_SCROLL, NS_STYLE_OVERFLOW_HIDDEN,
  // or NS_STYLE_OVERFLOW_AUTO.
  uint8_t mHorizontal;
  uint8_t mVertical;
  // Always one of NS_STYLE_SCROLL_BEHAVIOR_AUTO or
  // NS_STYLE_SCROLL_BEHAVIOR_SMOOTH
  uint8_t mScrollBehavior;
  mozilla::StyleOverscrollBehavior mOverscrollBehaviorX;
  mozilla::StyleOverscrollBehavior mOverscrollBehaviorY;
  mozilla::StyleScrollSnapType mScrollSnapTypeX;
  mozilla::StyleScrollSnapType mScrollSnapTypeY;
  nsStyleCoord mScrollSnapPointsX;
  nsStyleCoord mScrollSnapPointsY;
  nsStyleCoord::CalcValue mScrollSnapDestinationX;
  nsStyleCoord::CalcValue mScrollSnapDestinationY;

  ScrollStyles(uint8_t aH, uint8_t aV)
      : mHorizontal(aH),
        mVertical(aV),
        mScrollBehavior(NS_STYLE_SCROLL_BEHAVIOR_AUTO),
        mOverscrollBehaviorX(StyleOverscrollBehavior::Auto),
        mOverscrollBehaviorY(StyleOverscrollBehavior::Auto),
        mScrollSnapTypeX(mozilla::StyleScrollSnapType::None),
        mScrollSnapTypeY(mozilla::StyleScrollSnapType::None),
        mScrollSnapPointsX(nsStyleCoord(eStyleUnit_None)),
        mScrollSnapPointsY(nsStyleCoord(eStyleUnit_None)) {
    mScrollSnapDestinationX.mPercent = 0;
    mScrollSnapDestinationX.mLength = nscoord(0.0f);
    mScrollSnapDestinationX.mHasPercent = false;
    mScrollSnapDestinationY.mPercent = 0;
    mScrollSnapDestinationY.mLength = nscoord(0.0f);
    mScrollSnapDestinationY.mHasPercent = false;
  }

  explicit ScrollStyles(const nsStyleDisplay* aDisplay);
  ScrollStyles(uint8_t aH, uint8_t aV, const nsStyleDisplay* aDisplay);
  bool operator==(const ScrollStyles& aStyles) const {
    return aStyles.mHorizontal == mHorizontal &&
           aStyles.mVertical == mVertical &&
           aStyles.mScrollBehavior == mScrollBehavior &&
           aStyles.mOverscrollBehaviorX == mOverscrollBehaviorX &&
           aStyles.mOverscrollBehaviorY == mOverscrollBehaviorY &&
           aStyles.mScrollSnapTypeX == mScrollSnapTypeX &&
           aStyles.mScrollSnapTypeY == mScrollSnapTypeY &&
           aStyles.mScrollSnapPointsX == mScrollSnapPointsX &&
           aStyles.mScrollSnapPointsY == mScrollSnapPointsY &&
           aStyles.mScrollSnapDestinationX == mScrollSnapDestinationX &&
           aStyles.mScrollSnapDestinationY == mScrollSnapDestinationY;
  }
  bool operator!=(const ScrollStyles& aStyles) const {
    return !(*this == aStyles);
  }
  bool IsHiddenInBothDirections() const {
    return mHorizontal == NS_STYLE_OVERFLOW_HIDDEN &&
           mVertical == NS_STYLE_OVERFLOW_HIDDEN;
  }
  bool IsSmoothScroll(dom::ScrollBehavior aBehavior) const {
    return aBehavior == dom::ScrollBehavior::Smooth ||
           (aBehavior == dom::ScrollBehavior::Auto &&
            mScrollBehavior == NS_STYLE_SCROLL_BEHAVIOR_SMOOTH);
  }
};

}  // namespace mozilla

#endif  // mozilla_ScrollStyles_h
