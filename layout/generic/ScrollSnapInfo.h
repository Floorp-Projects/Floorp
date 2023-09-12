/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_ScrollSnapInfo_h_
#define mozilla_layout_ScrollSnapInfo_h_

#include <memory>
#include "mozilla/ScrollTypes.h"
#include "mozilla/ScrollSnapTargetId.h"
#include "mozilla/ServoStyleConsts.h"
#include "mozilla/Maybe.h"

class nsIContent;
class nsIFrame;
struct nsPoint;
struct nsRect;
struct nsSize;
struct nsStyleDisplay;

namespace mozilla {

enum class StyleScrollSnapStrictness : uint8_t;
class WritingMode;

struct ScrollSnapInfo {
  ScrollSnapInfo();

  bool operator==(const ScrollSnapInfo& aOther) const {
    return mScrollSnapStrictnessX == aOther.mScrollSnapStrictnessX &&
           mScrollSnapStrictnessY == aOther.mScrollSnapStrictnessY &&
           mSnapTargets == aOther.mSnapTargets &&
           mXRangeWiderThanSnapport == aOther.mXRangeWiderThanSnapport &&
           mYRangeWiderThanSnapport == aOther.mYRangeWiderThanSnapport &&
           mSnapportSize == aOther.mSnapportSize;
  }

  bool HasScrollSnapping() const;
  bool HasSnapPositions() const;

  void InitializeScrollSnapStrictness(WritingMode aWritingMode,
                                      const nsStyleDisplay* aDisplay);

  // The scroll frame's scroll-snap-type.
  StyleScrollSnapStrictness mScrollSnapStrictnessX;
  StyleScrollSnapStrictness mScrollSnapStrictnessY;

  struct SnapTarget {
    // The scroll positions corresponding to scroll-snap-align values.
    Maybe<nscoord> mSnapPositionX;
    Maybe<nscoord> mSnapPositionY;

    // https://drafts.csswg.org/css-scroll-snap/#scroll-snap-area
    nsRect mSnapArea;

    // https://drafts.csswg.org/css-scroll-snap/#propdef-scroll-snap-stop
    StyleScrollSnapStop mScrollSnapStop = StyleScrollSnapStop::Normal;

    // Use for tracking the last snapped target.
    ScrollSnapTargetId mTargetId = ScrollSnapTargetId::None;

    SnapTarget() = default;

    SnapTarget(Maybe<nscoord>&& aSnapPositionX, Maybe<nscoord>&& aSnapPositionY,
               nsRect&& aSnapArea, StyleScrollSnapStop aScrollSnapStop,
               ScrollSnapTargetId aTargetId)
        : mSnapPositionX(std::move(aSnapPositionX)),
          mSnapPositionY(std::move(aSnapPositionY)),
          mSnapArea(aSnapArea),
          mScrollSnapStop(aScrollSnapStop),
          mTargetId(aTargetId) {}

    bool operator==(const SnapTarget& aOther) const {
      return mSnapPositionX == aOther.mSnapPositionX &&
             mSnapPositionY == aOther.mSnapPositionY &&
             mSnapArea == aOther.mSnapArea &&
             mScrollSnapStop == aOther.mScrollSnapStop &&
             mTargetId == aOther.mTargetId;
    }
  };

  CopyableTArray<SnapTarget> mSnapTargets;

  // A utility function to iterate over the valid snap targets for the given
  // |aDestination| until |aFunc| returns false.
  void ForEachValidTargetFor(
      const nsPoint& aDestination,
      const std::function<bool(const SnapTarget&)>& aFunc) const;

  struct ScrollSnapRange {
    ScrollSnapRange() = default;

    ScrollSnapRange(nscoord aStart, nscoord aEnd, ScrollSnapTargetId aTargetId)
        : mStart(aStart), mEnd(aEnd), mTargetId(aTargetId) {}

    nscoord mStart;
    nscoord mEnd;
    ScrollSnapTargetId mTargetId;

    bool operator==(const ScrollSnapRange& aOther) const {
      return mStart == aOther.mStart && mEnd == aOther.mEnd &&
             mTargetId == aOther.mTargetId;
    }

    // Returns true if |aPoint| is a valid snap position in this range.
    bool IsValid(nscoord aPoint, nscoord aSnapportSize) const {
      MOZ_ASSERT(mEnd - mStart > aSnapportSize);
      return mStart <= aPoint && aPoint <= mEnd - aSnapportSize;
    }
  };
  // An array of the range that the target element is larger than the snapport
  // on the axis.
  // Snap positions in this range will be valid snap positions in the case where
  // the distance between the closest snap position and the second closest snap
  // position is still larger than the snapport size.
  // See https://drafts.csswg.org/css-scroll-snap-1/#snap-overflow
  //
  // Note: This range contains scroll-margin values.
  CopyableTArray<ScrollSnapRange> mXRangeWiderThanSnapport;
  CopyableTArray<ScrollSnapRange> mYRangeWiderThanSnapport;

  // Note: This snapport size has been already deflated by scroll-padding.
  nsSize mSnapportSize;
};

}  // namespace mozilla

#endif  // mozilla_layout_ScrollSnapInfo_h_
