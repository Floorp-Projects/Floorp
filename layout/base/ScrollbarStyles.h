/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScrollbarStyles_h
#define ScrollbarStyles_h

#include <stdint.h>
#include "nsStyleConsts.h" // for NS_STYLE_SCROLL_SNAP_*
#include "nsStyleCoord.h" // for nsStyleCoord
#include "mozilla/dom/WindowBinding.h"

// Forward declarations
struct nsStyleDisplay;

namespace mozilla {

struct ScrollbarStyles
{
  // Always one of NS_STYLE_OVERFLOW_SCROLL, NS_STYLE_OVERFLOW_HIDDEN,
  // or NS_STYLE_OVERFLOW_AUTO.
  uint8_t mHorizontal;
  uint8_t mVertical;
  // Always one of NS_STYLE_SCROLL_BEHAVIOR_AUTO or
  // NS_STYLE_SCROLL_BEHAVIOR_SMOOTH
  uint8_t mScrollBehavior;
  // Always one of NS_STYLE_SCROLL_SNAP_NONE, NS_STYLE_SCROLL_SNAP_MANDATORY,
  // or NS_STYLE_SCROLL_SNAP_PROXIMITY.
  uint8_t mScrollSnapTypeX;
  uint8_t mScrollSnapTypeY;
  nsStyleCoord mScrollSnapPointsX;
  nsStyleCoord mScrollSnapPointsY;
  nsStyleCoord::CalcValue mScrollSnapDestinationX;
  nsStyleCoord::CalcValue mScrollSnapDestinationY;

  ScrollbarStyles(uint8_t aH, uint8_t aV)
    : mHorizontal(aH), mVertical(aV),
      mScrollBehavior(NS_STYLE_SCROLL_BEHAVIOR_AUTO),
      mScrollSnapTypeX(NS_STYLE_SCROLL_SNAP_TYPE_NONE),
      mScrollSnapTypeY(NS_STYLE_SCROLL_SNAP_TYPE_NONE),
      mScrollSnapPointsX(nsStyleCoord(eStyleUnit_None)),
      mScrollSnapPointsY(nsStyleCoord(eStyleUnit_None)) {

    mScrollSnapDestinationX.mPercent = 0;
    mScrollSnapDestinationX.mLength = nscoord(0.0f);
    mScrollSnapDestinationX.mHasPercent = false;
    mScrollSnapDestinationY.mPercent = 0;
    mScrollSnapDestinationY.mLength = nscoord(0.0f);
    mScrollSnapDestinationY.mHasPercent = false;
  }

  explicit ScrollbarStyles(const nsStyleDisplay* aDisplay);
  ScrollbarStyles(uint8_t aH, uint8_t aV, const nsStyleDisplay* aDisplay);
  ScrollbarStyles() {}
  bool operator==(const ScrollbarStyles& aStyles) const {
    return aStyles.mHorizontal == mHorizontal && aStyles.mVertical == mVertical &&
           aStyles.mScrollBehavior == mScrollBehavior &&
           aStyles.mScrollSnapTypeX == mScrollSnapTypeX &&
           aStyles.mScrollSnapTypeY == mScrollSnapTypeY &&
           aStyles.mScrollSnapPointsX == mScrollSnapPointsX &&
           aStyles.mScrollSnapPointsY == mScrollSnapPointsY &&
           aStyles.mScrollSnapDestinationX == mScrollSnapDestinationX &&
           aStyles.mScrollSnapDestinationY == mScrollSnapDestinationY;
  }
  bool operator!=(const ScrollbarStyles& aStyles) const {
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

} // namespace mozilla

#endif
