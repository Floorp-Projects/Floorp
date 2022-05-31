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
  struct SnapPosition {
    SnapPosition() = default;

    SnapPosition(nscoord aPosition, StyleScrollSnapStop aScrollSnapStop)
        : mPosition(aPosition), mScrollSnapStop(aScrollSnapStop) {}

    nscoord mPosition;
    StyleScrollSnapStop mScrollSnapStop;
  };

  void AddHorizontalEdge(const SnapPosition& aEdge);
  void AddVerticalEdge(const SnapPosition& aEdge);
  void AddEdge(const SnapPosition& aEdge, nscoord aDestination,
               nscoord aStartPos, nscoord aScrollingDirection,
               SnapPosition* aBestEdge, SnapPosition* aSecondBestEdge,
               bool* aEdgeFound);

  void AddEdge(nscoord aEdge, nscoord aDestination, nscoord aStartPos,
               nscoord aScrollingDirection, nscoord* aBestEdge,
               nscoord* aSecondBestEdge, bool* aEdgeFound);
  nsPoint GetBestEdge() const;
  nscoord XDistanceBetweenBestAndSecondEdge() const {
    return std::abs(NSCoordSaturatingSubtract(
        mSecondBestEdgeX.mPosition, mBestEdgeX.mPosition, nscoord_MAX));
  }
  nscoord YDistanceBetweenBestAndSecondEdge() const {
    return std::abs(NSCoordSaturatingSubtract(
        mSecondBestEdgeY.mPosition, mBestEdgeY.mPosition, nscoord_MAX));
  }
  const nsPoint& Destination() const { return mDestination; }

 protected:
  ScrollUnit mUnit;
  ScrollSnapFlags mSnapFlags;
  nsPoint mDestination;  // gives the position after scrolling but before
                         // snapping
  nsPoint mStartPos;     // gives the position before scrolling
  nsIntPoint mScrollingDirection;  // always -1, 0, or 1
  SnapPosition mBestEdgeX;  // keeps track of the position of the current best
                            // edge on X axis
  SnapPosition mBestEdgeY;  // keeps track of the position of the current best
                            // edge on Y axis
  SnapPosition mSecondBestEdgeX;  // keeps track of the position of the current
                                  // second best edge on the opposite side of
                                  // the best edge on X axis
  SnapPosition mSecondBestEdgeY;  // keeps track of the position of the current
                                  // second best edge on the opposite side of
                                  // the best edge on Y axis
  bool mHorizontalEdgeFound;      // true if mBestEdge.x is storing a valid
                                  // horizontal edge
  bool mVerticalEdgeFound;  // true if mBestEdge.y is storing a valid vertical
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
  mBestEdgeX = SnapPosition{aDestination.x, StyleScrollSnapStop::Normal};
  mBestEdgeY = SnapPosition{aDestination.y, StyleScrollSnapStop::Normal};
  // We use NSCoordSaturatingSubtract to calculate the distance between a given
  // position and this second best edge position so that it can be an
  // uninitialized value as the maximum possible value, because the first
  // distance calculation would always be nscoord_MAX.
  mSecondBestEdgeX = SnapPosition{nscoord_MAX, StyleScrollSnapStop::Normal};
  mSecondBestEdgeY = SnapPosition{nscoord_MAX, StyleScrollSnapStop::Normal};
  mHorizontalEdgeFound = false;
  mVerticalEdgeFound = false;
}

nsPoint CalcSnapPoints::GetBestEdge() const {
  return nsPoint(mVerticalEdgeFound ? mBestEdgeX.mPosition : mStartPos.x,
                 mHorizontalEdgeFound ? mBestEdgeY.mPosition : mStartPos.y);
}

void CalcSnapPoints::AddHorizontalEdge(const SnapPosition& aEdge) {
  AddEdge(aEdge, mDestination.y, mStartPos.y, mScrollingDirection.y,
          &mBestEdgeY, &mSecondBestEdgeY, &mHorizontalEdgeFound);
}

void CalcSnapPoints::AddVerticalEdge(const SnapPosition& aEdge) {
  AddEdge(aEdge, mDestination.x, mStartPos.x, mScrollingDirection.x,
          &mBestEdgeX, &mSecondBestEdgeX, &mVerticalEdgeFound);
}

void CalcSnapPoints::AddEdge(const SnapPosition& aEdge, nscoord aDestination,
                             nscoord aStartPos, nscoord aScrollingDirection,
                             SnapPosition* aBestEdge,
                             SnapPosition* aSecondBestEdge, bool* aEdgeFound) {
  if (mSnapFlags & ScrollSnapFlags::IntendedDirection) {
    // In the case of intended direction, we only want to snap to points ahead
    // of the direction we are scrolling.
    if (aScrollingDirection == 0 ||
        (aEdge.mPosition - aStartPos) * aScrollingDirection <= 0) {
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

  auto isPreferredStopAlways = [&](const SnapPosition& aSnapPosition) -> bool {
    MOZ_ASSERT(mSnapFlags & ScrollSnapFlags::IntendedDirection);
    // In the case of intended direction scroll operations, `scroll-snap-stop:
    // always` snap points in between the start point and the scroll destination
    // are preferable preferable. In other words any `scroll-snap-stop: always`
    // snap points can be handled as if it's `scroll-snap-stop: normal`.
    return aSnapPosition.mScrollSnapStop == StyleScrollSnapStop::Always &&
           std::abs(aSnapPosition.mPosition - aStartPos) <
               std::abs(aDestination - aStartPos);
  };

  const bool isOnOppositeSide = ((aEdge.mPosition - aDestination) > 0) !=
                                ((aBestEdge->mPosition - aDestination) > 0);
  const nscoord distanceFromStart = aEdge.mPosition - aStartPos;
  // A utility function to update the best and the second best edges in the
  // given conditions.
  // |aIsCloserThanBest| True if the current candidate is closer than the best
  // edge.
  // |aIsCloserThanSecond| True if the current candidate is closer than
  // the second best edge.
  const nscoord distanceFromDestination = aEdge.mPosition - aDestination;
  auto updateBestEdges = [&](bool aIsCloserThanBest, bool aIsCloserThanSecond) {
    if (aIsCloserThanBest) {
      if (mSnapFlags & ScrollSnapFlags::IntendedDirection &&
          isPreferredStopAlways(aEdge)) {
        // In the case of intended direction scroll operations and the new best
        // candidate is `scroll-snap-stop: always` and if it's closer to the
        // start position than the destination, thus we won't use the second
        // best edge since even if the snap port of the best edge covers entire
        // snapport, the `scroll-snap-stop: always` snap point is preferred than
        // any points.
        // NOTE: We've already ignored snap points behind start points so that
        // we can use std::abs here in the comparison.
        //
        // For example, if there's a `scroll-snap-stop: always` in between the
        // start point and destination, no `snap-overflow` mechanism should
        // happen, if there's `scroll-snap-stop: always` further than the
        // destination, `snap-overflow` might happen something like below
        // diagram.
        // start        always    dest   other always
        //   |------------|---------|------|
        *aSecondBestEdge = aEdge;
      } else if (isOnOppositeSide) {
        // Replace the second best edge with the current best edge only if the
        // new best edge (aEdge) is on the opposite side of the current best
        // edge.
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
      isCandidateOfBest = std::abs(distanceFromDestination) <
                          std::abs(aBestEdge->mPosition - aDestination);
      isCandidateOfSecondBest =
          std::abs(distanceFromDestination) <
          std::abs(NSCoordSaturatingSubtract(aSecondBestEdge->mPosition,
                                             aDestination, nscoord_MAX));
      break;
    }
    case ScrollUnit::PAGES: {
      // distance to the edge from the scrolling destination in the direction of
      // scrolling
      nscoord overshoot = distanceFromDestination * aScrollingDirection;
      // distance to the current best edge from the scrolling destination in the
      // direction of scrolling
      nscoord curOvershoot =
          (aBestEdge->mPosition - aDestination) * aScrollingDirection;

      nscoord secondOvershoot =
          NSCoordSaturatingSubtract(aSecondBestEdge->mPosition, aDestination,
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

  if (mSnapFlags & ScrollSnapFlags::IntendedDirection) {
    if (isPreferredStopAlways(aEdge)) {
      // If the given position is `scroll-snap-stop: always` and if the position
      // is in between the start and the destination positions, update the best
      // position based on the distance from the __start__ point.
      isCandidateOfBest = std::abs(distanceFromStart) <
                          std::abs(aBestEdge->mPosition - aStartPos);
    } else if (isPreferredStopAlways(*aBestEdge)) {
      // If we've found a preferable `scroll-snap-stop:always` position as the
      // best, do not update it unless the given position is also
      // `scroll-snap-stop: always`.
      isCandidateOfBest = false;
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
      aCalcSnapPoints.AddVerticalEdge(
          {*target.mSnapPositionX, target.mScrollSnapStop});
    }
    if (target.mSnapPositionY &&
        aSnapInfo.mScrollSnapStrictnessY != StyleScrollSnapStrictness::None) {
      aCalcSnapPoints.AddHorizontalEdge(
          {*target.mSnapPositionY, target.mScrollSnapStop});
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
      calcSnapPoints.AddVerticalEdge(CalcSnapPoints::SnapPosition{
          clampedDestination.x, StyleScrollSnapStop::Normal});
      break;
    }
  }
  for (auto range : aSnapInfo.mYRangeWiderThanSnapport) {
    if (range.IsValid(clampedDestination.y, aSnapInfo.mSnapportSize.height) &&
        calcSnapPoints.YDistanceBetweenBestAndSecondEdge() >
            aSnapInfo.mSnapportSize.height) {
      calcSnapPoints.AddHorizontalEdge(CalcSnapPoints::SnapPosition{
          clampedDestination.y, StyleScrollSnapStop::Normal});
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
