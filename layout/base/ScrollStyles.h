/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ScrollStyles_h
#define mozilla_ScrollStyles_h

#include <stdint.h>
#include "nsStyleConsts.h"
#include "nsStyleCoord.h"  // for nsStyleCoord
#include "mozilla/dom/WindowBinding.h"

// Forward declarations
struct nsStyleDisplay;

namespace mozilla {

struct ScrollStyles {
  // Always one of Scroll, Hidden, or Auto
  StyleOverflow mHorizontal;
  StyleOverflow mVertical;
  // Always one of NS_STYLE_SCROLL_BEHAVIOR_AUTO or
  // NS_STYLE_SCROLL_BEHAVIOR_SMOOTH
  uint8_t mScrollBehavior;
  StyleOverscrollBehavior mOverscrollBehaviorX;
  StyleOverscrollBehavior mOverscrollBehaviorY;
  StyleScrollSnapStrictness mScrollSnapTypeX;
  StyleScrollSnapStrictness mScrollSnapTypeY;
  nsStyleCoord mScrollSnapPointsX;
  nsStyleCoord mScrollSnapPointsY;
  LengthPercentage mScrollSnapDestinationX;
  LengthPercentage mScrollSnapDestinationY;

  ScrollStyles(StyleOverflow aH, StyleOverflow aV)
      : mHorizontal(aH),
        mVertical(aV),
        mScrollBehavior(NS_STYLE_SCROLL_BEHAVIOR_AUTO),
        mOverscrollBehaviorX(StyleOverscrollBehavior::Auto),
        mOverscrollBehaviorY(StyleOverscrollBehavior::Auto),
        mScrollSnapTypeX(StyleScrollSnapStrictness::None),
        mScrollSnapTypeY(StyleScrollSnapStrictness::None),
        mScrollSnapPointsX(nsStyleCoord(eStyleUnit_None)),
        mScrollSnapPointsY(nsStyleCoord(eStyleUnit_None)),
        mScrollSnapDestinationX(LengthPercentage::Zero()),
        mScrollSnapDestinationY(LengthPercentage::Zero()) {}

  ScrollStyles(WritingMode aWritingMode, const nsStyleDisplay* aDisplay);
  ScrollStyles(WritingMode aWritingMode, StyleOverflow aH, StyleOverflow aV,
               const nsStyleDisplay* aDisplay);
  void InitializeScrollSnapType(WritingMode aWritingMode,
                                const nsStyleDisplay* aDisplay);
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
    return mHorizontal == StyleOverflow::Hidden &&
           mVertical == StyleOverflow::Hidden;
  }
  bool IsSmoothScroll(dom::ScrollBehavior aBehavior) const {
    return aBehavior == dom::ScrollBehavior::Smooth ||
           (aBehavior == dom::ScrollBehavior::Auto &&
            mScrollBehavior == NS_STYLE_SCROLL_BEHAVIOR_SMOOTH);
  }
};

}  // namespace mozilla

#endif  // mozilla_ScrollStyles_h
