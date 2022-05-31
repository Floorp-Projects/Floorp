/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollSnap.h"

#include "FrameMetrics.h"

#include "mozilla/Maybe.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_layout.h"
#include "nsLineLayout.h"

namespace mozilla {

using layers::ScrollSnapInfo;

/**
 * Keeps track of the current best edge to snap to. The criteria for
 * adding an edge depends on the scrolling unit.
 */
class CalcSnapPoints final {
 public:
  CalcSnapPoints(ScrollUnit aUnit, ScrollSnapFlags aSnapFlags,
                 const nsPoint& aDestination, const nsPoint& aStartPos);
  void AddHorizontalEdge(nscoord aEdge);
  void AddVerticalEdge(nscoord aEdge);
  void AddEdge(nscoord aEdge, nscoord aDestination, nscoord aStartPos,
               nscoord aScrollingDirection, nscoord* aBestEdge,
               nscoord* aSecondBestEdge, bool* aEdgeFound);
  nsPoint GetBestEdge() const;
  nscoord XDistanceBetweenBestAndSecondEdge() const {
    return std::abs(
        NSCoordSaturatingSubtract(mSecondBestEdge.x, mBestEdge.x, nscoord_MAX));
  }
  nscoord YDistanceBetweenBestAndSecondEdge() const {
    return std::abs(
        NSCoordSaturatingSubtract(mSecondBestEdge.y, mBestEdge.y, nscoord_MAX));
  }
  const nsPoint& Destination() const { return mDestination; }

 protected:
  ScrollUnit mUnit;
  ScrollSnapFlags mSnapFlags;
  nsPoint mDestination;  // gives the position after scrolling but before
                         // snapping
  nsPoint mStartPos;     // gives the position before scrolling
  nsIntPoint mScrollingDirection;  // always -1, 0, or 1
  nsPoint mBestEdge;  // keeps track of the position of the current best edge
  nsPoint mSecondBestEdge;  // keeps track of the position of the current
                            // second best edge on the opposite side of the best
                            // edge
  bool mHorizontalEdgeFound;  // true if mBestEdge.x is storing a valid
                              // horizontal edge
  bool mVerticalEdgeFound;    // true if mBestEdge.y is storing a valid vertical
                              // edge
};

CalcSnapPoints::CalcSnapPoints(ScrollUnit aUnit, ScrollSnapFlags aSnapFlags,
                               const nsPoint& aDestination,
                               const nsPoint& aStartPos) {
  MOZ_ASSERT(aSnapFlags != ScrollSnapFlags::Disabled);

  mUnit = aUnit;
  mSnapFlags = aSnapFlags;
  mDestination = aDestination;
  mStartPos = aStartPos;

  nsPoint direction = aDestination - aStartPos;
  mScrollingDirection = nsIntPoint(0, 0);
  if (direction.x < 0) {
    mScrollingDirection.x = -1;
  }
  if (direction.x > 0) {
    mScrollingDirection.x = 1;
  }
  if (direction.y < 0) {
    mScrollingDirection.y = -1;
  }
  if (direction.y > 0) {
    mScrollingDirection.y = 1;
  }
  mBestEdge = aDestination;
  // We use NSCoordSaturatingSubtract to calculate the distance between a given
  // position and this second best edge position so that it can be an
  // uninitialized value as the maximum possible value, because the first
  // distance calculation would always be nscoord_MAX.
  mSecondBestEdge = nsPoint(nscoord_MAX, nscoord_MAX);
  mHorizontalEdgeFound = false;
  mVerticalEdgeFound = false;
}

nsPoint CalcSnapPoints::GetBestEdge() const {
  return nsPoint(mVerticalEdgeFound ? mBestEdge.x : mStartPos.x,
                 mHorizontalEdgeFound ? mBestEdge.y : mStartPos.y);
}

void CalcSnapPoints::AddHorizontalEdge(nscoord aEdge) {
  AddEdge(aEdge, mDestination.y, mStartPos.y, mScrollingDirection.y,
          &mBestEdge.y, &mSecondBestEdge.y, &mHorizontalEdgeFound);
}

void CalcSnapPoints::AddVerticalEdge(nscoord aEdge) {
  AddEdge(aEdge, mDestination.x, mStartPos.x, mScrollingDirection.x,
          &mBestEdge.x, &mSecondBestEdge.x, &mVerticalEdgeFound);
}

void CalcSnapPoints::AddEdge(nscoord aEdge, nscoord aDestination,
                             nscoord aStartPos, nscoord aScrollingDirection,
                             nscoord* aBestEdge, nscoord* aSecondBestEdge,
                             bool* aEdgeFound) {
  if (mSnapFlags & ScrollSnapFlags::IntendedDirection) {
    // In the case of intended direction, we only want to snap to points ahead
    // of the direction we are scrolling.
    if (aScrollingDirection == 0 ||
        (aEdge - aStartPos) * aScrollingDirection <= 0) {
      // The scroll direction is neutral - will not hit a snap point, or the
      // edge is not in the direction we are scrolling, skip it.
      return;
    }
  }

  if (!*aEdgeFound) {
    *aBestEdge = aEdge;
    *aEdgeFound = true;
    return;
  }

  const bool isOnOppositeSide =
      ((aEdge - aDestination) > 0) != ((*aBestEdge - aDestination) > 0);
  // A utility function to update the best and the second best edges in the
  // given conditions.
  // |aIsCloserThanBest| True if the current candidate is closer than the best
  // edge.
  // |aIsCloserThanSecond| True if the current candidate is closer than
  // the second best edge.
  auto updateBestEdges = [&](bool aIsCloserThanBest, bool aIsCloserThanSecond) {
    if (aIsCloserThanBest) {
      // Replace the second best edge with the current best edge only if the new
      // best edge (aEdge) is on the opposite side of the current best edge.
      if (isOnOppositeSide) {
        *aSecondBestEdge = *aBestEdge;
      }
      *aBestEdge = aEdge;
    } else if (aIsCloserThanSecond) {
      if (isOnOppositeSide) {
        *aSecondBestEdge = aEdge;
      }
    }
  };

  bool isCandidateOfBest = false;
  bool isCandidateOfSecondBest = false;
  switch (mUnit) {
    case ScrollUnit::DEVICE_PIXELS:
    case ScrollUnit::LINES:
    case ScrollUnit::WHOLE: {
      nscoord distance = std::abs(aEdge - aDestination);
      isCandidateOfBest = distance < std::abs(*aBestEdge - aDestination);
      isCandidateOfSecondBest =
          distance < std::abs(NSCoordSaturatingSubtract(
                         *aSecondBestEdge, aDestination, nscoord_MAX));
      break;
    }
    case ScrollUnit::PAGES: {
      // distance to the edge from the scrolling destination in the direction of
      // scrolling
      nscoord overshoot = (aEdge - aDestination) * aScrollingDirection;
      // distance to the current best edge from the scrolling destination in the
      // direction of scrolling
      nscoord curOvershoot = (*aBestEdge - aDestination) * aScrollingDirection;

      nscoord secondOvershoot =
          NSCoordSaturatingSubtract(*aSecondBestEdge, aDestination,
                                    nscoord_MAX) *
          aScrollingDirection;

      // edges between the current position and the scrolling destination are
      // favoured to preserve context
      if (overshoot < 0) {
        isCandidateOfBest = overshoot > curOvershoot || curOvershoot >= 0;
        isCandidateOfSecondBest =
            overshoot > secondOvershoot || secondOvershoot >= 0;
      }
      // if there are no edges between the current position and the scrolling
      // destination the closest edge beyond the destination is used
      if (overshoot > 0) {
        isCandidateOfBest = overshoot < curOvershoot;
        isCandidateOfSecondBest = overshoot < secondOvershoot;
      }
    }
  }

  updateBestEdges(isCandidateOfBest, isCandidateOfSecondBest);
}

static void ProcessSnapPositions(CalcSnapPoints& aCalcSnapPoints,
                                 const ScrollSnapInfo& aSnapInfo) {
  for (const auto& target : aSnapInfo.mSnapTargets) {
    nsPoint snapPoint(target.mSnapPositionX ? *target.mSnapPositionX
                                            : aCalcSnapPoints.Destination().x,
                      target.mSnapPositionY ? *target.mSnapPositionY
                                            : aCalcSnapPoints.Destination().y);
    nsRect snappedPort = nsRect(snapPoint, aSnapInfo.mSnapportSize);
    // Ignore snap points if snapping to the point would leave the snap area
    // outside of the snapport.
    // https://drafts.csswg.org/css-scroll-snap-1/#snap-scope
    if (!snappedPort.Intersects(target.mSnapArea)) {
      continue;
    }

    if (target.mSnapPositionX &&
        aSnapInfo.mScrollSnapStrictnessX != StyleScrollSnapStrictness::None) {
      aCalcSnapPoints.AddVerticalEdge(*target.mSnapPositionX);
    }
    if (target.mSnapPositionY &&
        aSnapInfo.mScrollSnapStrictnessY != StyleScrollSnapStrictness::None) {
      aCalcSnapPoints.AddHorizontalEdge(*target.mSnapPositionY);
    }
  }
}

Maybe<nsPoint> ScrollSnapUtils::GetSnapPointForDestination(
    const ScrollSnapInfo& aSnapInfo, ScrollUnit aUnit,
    ScrollSnapFlags aSnapFlags, const nsRect& aScrollRange,
    const nsPoint& aStartPos, const nsPoint& aDestination) {
  if (aSnapInfo.mScrollSnapStrictnessY == StyleScrollSnapStrictness::None &&
      aSnapInfo.mScrollSnapStrictnessX == StyleScrollSnapStrictness::None) {
    return Nothing();
  }

  if (!aSnapInfo.HasSnapPositions()) {
    return Nothing();
  }

  CalcSnapPoints calcSnapPoints(aUnit, aSnapFlags, aDestination, aStartPos);

  ProcessSnapPositions(calcSnapPoints, aSnapInfo);

  // If the distance between the first and the second candidate snap points
  // is larger than the snapport size and the snapport is covered by larger
  // elements, any points inside the covering area should be valid snap
  // points.
  // https://drafts.csswg.org/css-scroll-snap-1/#snap-overflow
  // NOTE: |aDestination| sometimes points outside of the scroll range, e.g.
  // by the APZC fling, so for the overflow checks we need to clamp it.
  nsPoint clampedDestination = aScrollRange.ClampPoint(aDestination);
  for (auto range : aSnapInfo.mXRangeWiderThanSnapport) {
    if (range.IsValid(clampedDestination.x, aSnapInfo.mSnapportSize.width) &&
        calcSnapPoints.XDistanceBetweenBestAndSecondEdge() >
            aSnapInfo.mSnapportSize.width) {
      calcSnapPoints.AddVerticalEdge(clampedDestination.x);
      break;
    }
  }
  for (auto range : aSnapInfo.mYRangeWiderThanSnapport) {
    if (range.IsValid(clampedDestination.y, aSnapInfo.mSnapportSize.height) &&
        calcSnapPoints.YDistanceBetweenBestAndSecondEdge() >
            aSnapInfo.mSnapportSize.height) {
      calcSnapPoints.AddHorizontalEdge(clampedDestination.y);
      break;
    }
  }

  bool snapped = false;
  nsPoint finalPos = calcSnapPoints.GetBestEdge();
  nscoord proximityThreshold =
      StaticPrefs::layout_css_scroll_snap_proximity_threshold();
  proximityThreshold = nsPresContext::CSSPixelsToAppUnits(proximityThreshold);
  if (aSnapInfo.mScrollSnapStrictnessY ==
          StyleScrollSnapStrictness::Proximity &&
      std::abs(aDestination.y - finalPos.y) > proximityThreshold) {
    finalPos.y = aDestination.y;
  } else {
    snapped = true;
  }
  if (aSnapInfo.mScrollSnapStrictnessX ==
          StyleScrollSnapStrictness::Proximity &&
      std::abs(aDestination.x - finalPos.x) > proximityThreshold) {
    finalPos.x = aDestination.x;
  } else {
    snapped = true;
  }
  return snapped ? Some(finalPos) : Nothing();
}

}  // namespace mozilla
