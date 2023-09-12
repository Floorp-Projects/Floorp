/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollSnap.h"

#include "FrameMetrics.h"

#include "mozilla/ScrollSnapInfo.h"
#include "mozilla/ServoStyleConsts.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsTArray.h"
#include "mozilla/StaticPrefs_layout.h"

namespace mozilla {

/**
 * Keeps track of the current best edge to snap to. The criteria for
 * adding an edge depends on the scrolling unit.
 */
class CalcSnapPoints final {
  using SnapTarget = ScrollSnapInfo::SnapTarget;

 public:
  CalcSnapPoints(ScrollUnit aUnit, ScrollSnapFlags aSnapFlags,
                 const nsPoint& aDestination, const nsPoint& aStartPos);
  struct SnapPosition : public SnapTarget {
    SnapPosition(const SnapTarget& aSnapTarget, nscoord aPosition,
                 nscoord aDistanceOnOtherAxis)
        : SnapTarget(aSnapTarget),
          mPosition(aPosition),
          mDistanceOnOtherAxis(aDistanceOnOtherAxis) {}

    nscoord mPosition;
    // The distance from the scroll destination to this snap position on the
    // other axis. This value is used if there are multiple SnapPositions on
    // this axis, but the positions on the other axis are different.
    nscoord mDistanceOnOtherAxis;
  };

  void AddHorizontalEdge(const SnapTarget& aTarget);
  void AddVerticalEdge(const SnapTarget& aTarget);

  struct CandidateTracker {
    // keeps track of the position of the current second best edge on the
    // opposite side of the best edge on this axis.
    // We use NSCoordSaturatingSubtract to calculate the distance between a
    // given position and this second best edge position so that it can be an
    // uninitialized value as the maximum possible value, because the first
    // distance calculation would always be nscoord_MAX.
    nscoord mSecondBestEdge = nscoord_MAX;

    // Assuming in most cases there's no multiple coincide snap points.
    AutoTArray<ScrollSnapTargetId, 1> mTargetIds;
    // keeps track of the positions of the current best edge on this axis.
    // NOTE: Each SnapPosition.mPosition points the same snap position on this
    // axis but other member variables of SnapPosition may have different
    // values.
    AutoTArray<SnapPosition, 1> mBestEdges;
    bool EdgeFound() const { return !mBestEdges.IsEmpty(); }
  };
  void AddEdge(const SnapPosition& aEdge, nscoord aDestination,
               nscoord aStartPos, nscoord aScrollingDirection,
               CandidateTracker* aCandidateTracker);
  SnapDestination GetBestEdge(const nsSize& aSnapportSize) const;
  nscoord XDistanceBetweenBestAndSecondEdge() const {
    return std::abs(NSCoordSaturatingSubtract(
        mTrackerOnX.mSecondBestEdge,
        mTrackerOnX.EdgeFound() ? mTrackerOnX.mBestEdges[0].mPosition
                                : mDestination.x,
        nscoord_MAX));
  }
  nscoord YDistanceBetweenBestAndSecondEdge() const {
    return std::abs(NSCoordSaturatingSubtract(
        mTrackerOnY.mSecondBestEdge,
        mTrackerOnY.EdgeFound() ? mTrackerOnY.mBestEdges[0].mPosition
                                : mDestination.y,
        nscoord_MAX));
  }
  const nsPoint& Destination() const { return mDestination; }

 protected:
  ScrollUnit mUnit;
  ScrollSnapFlags mSnapFlags;
  nsPoint mDestination;  // gives the position after scrolling but before
                         // snapping
  nsPoint mStartPos;     // gives the position before scrolling
  nsIntPoint mScrollingDirection;  // always -1, 0, or 1
  CandidateTracker mTrackerOnX;
  CandidateTracker mTrackerOnY;
};

CalcSnapPoints::CalcSnapPoints(ScrollUnit aUnit, ScrollSnapFlags aSnapFlags,
                               const nsPoint& aDestination,
                               const nsPoint& aStartPos)
    : mUnit(aUnit),
      mSnapFlags(aSnapFlags),
      mDestination(aDestination),
      mStartPos(aStartPos) {
  MOZ_ASSERT(aSnapFlags != ScrollSnapFlags::Disabled);

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
}

SnapDestination CalcSnapPoints::GetBestEdge(const nsSize& aSnapportSize) const {
  if (mTrackerOnX.EdgeFound() && mTrackerOnY.EdgeFound()) {
    nsPoint bestCandidate(mTrackerOnX.mBestEdges[0].mPosition,
                          mTrackerOnY.mBestEdges[0].mPosition);
    nsRect snappedPort = nsRect(bestCandidate, aSnapportSize);

    // If we've found the candidates on both axes, it's possible some of
    // candidates will be outside of the snapport if we snap to the point
    // (mTrackerOnX.mBestEdges[0].mPosition,
    // mTrackerOnY.mBestEdges[0].mPosition). So we need to get the intersection
    // of the snap area of each snap target element on each axis and the
    // snapport to tell whether it's outside of the snapport or not.
    //
    // Also if at least either one of the elements will be outside of the
    // snapport if we snap to (mTrackerOnX.mBestEdges[0].mPosition,
    // mTrackerOnY.mBestEdges[0].mPosition). We need to choose one of
    // combinations of the candidates which is closest to the destination.
    //
    // So here we iterate over mTrackerOnX and mTrackerOnY just once
    // respectively for both purposes to avoid iterating over them again and
    // again.
    //
    // NOTE: Ideally we have to iterate over every possible combinations of
    // (mTrackerOnX.mBestEdges[i].mSnapPoint.mY,
    //  mTrackerOnY.mBestEdges[j].mSnapPoint.mX) and tell whether the given
    // combination will be visible in the snapport or not (maybe we should
    // choose the one that the visible area, i.e., the intersection area of
    // the snap target elements and the snapport, is the largest one rather than
    // the closest one?). But it will be inefficient, so here we will not
    // iterate all the combinations, we just iterate all the snap target
    // elements in each axis respectively.

    AutoTArray<ScrollSnapTargetId, 1> visibleTargetIdsOnX;
    nscoord minimumDistanceOnY = nscoord_MAX;
    size_t minimumXIndex = 0;
    AutoTArray<ScrollSnapTargetId, 1> minimumDistanceTargetIdsOnX;
    for (size_t i = 0; i < mTrackerOnX.mBestEdges.Length(); i++) {
      const auto& targetX = mTrackerOnX.mBestEdges[i];
      if (targetX.mSnapArea.Intersects(snappedPort)) {
        visibleTargetIdsOnX.AppendElement(targetX.mTargetId);
      }

      if (targetX.mDistanceOnOtherAxis < minimumDistanceOnY) {
        minimumDistanceOnY = targetX.mDistanceOnOtherAxis;
        minimumXIndex = i;
        minimumDistanceTargetIdsOnX =
            AutoTArray<ScrollSnapTargetId, 1>{targetX.mTargetId};
      } else if (minimumDistanceOnY != nscoord_MAX &&
                 targetX.mDistanceOnOtherAxis == minimumDistanceOnY) {
        minimumDistanceTargetIdsOnX.AppendElement(targetX.mTargetId);
      }
    }

    AutoTArray<ScrollSnapTargetId, 1> visibleTargetIdsOnY;
    nscoord minimumDistanceOnX = nscoord_MAX;
    size_t minimumYIndex = 0;
    AutoTArray<ScrollSnapTargetId, 1> minimumDistanceTargetIdsOnY;
    for (size_t i = 0; i < mTrackerOnY.mBestEdges.Length(); i++) {
      const auto& targetY = mTrackerOnY.mBestEdges[i];
      if (targetY.mSnapArea.Intersects(snappedPort)) {
        visibleTargetIdsOnY.AppendElement(targetY.mTargetId);
      }

      if (targetY.mDistanceOnOtherAxis < minimumDistanceOnX) {
        minimumDistanceOnX = targetY.mDistanceOnOtherAxis;
        minimumYIndex = i;
        minimumDistanceTargetIdsOnY =
            AutoTArray<ScrollSnapTargetId, 1>{targetY.mTargetId};
      } else if (minimumDistanceOnX != nscoord_MAX &&
                 targetY.mDistanceOnOtherAxis == minimumDistanceOnX) {
        minimumDistanceTargetIdsOnY.AppendElement(targetY.mTargetId);
      }
    }

    // If we have the target ids on both axes, it means the target elements
    // (ids) specifying the best edge on X axis and the target elements
    // specifying the best edge on Y axis are visible if we snap to the best
    // edge. Thus they are valid snap positions.
    if (!visibleTargetIdsOnX.IsEmpty() && !visibleTargetIdsOnY.IsEmpty()) {
      return SnapDestination{
          bestCandidate,
          ScrollSnapTargetIds{visibleTargetIdsOnX, visibleTargetIdsOnY}};
    }

    // Now we've already known that snapping to
    // (mTrackerOnX.mBestEdges[0].mPosition,
    // mTrackerOnY.mBestEdges[0].mPosition) will make all candidates of
    // mTrackerX or mTrackerY (or both) outside of the snapport. We need to
    // choose another combination where candidates of both mTrackerX/Y are
    // inside the snapport.

    // There are three possibilities;
    // 1) There's no candidate on X axis in mTrackerOnY (that means
    //    each candidate's scroll-snap-align is `none` on X axis), but there's
    //    any candidate in mTrackerOnX, the closest candidates of mTrackerOnX
    //    should be used.
    // 2) There's no candidate on Y axis in mTrackerOnX (that means
    //    each candidate's scroll-snap-align is `none` on Y axis), but there's
    //    any candidate in mTrackerOnY, the closest candidates of mTrackerOnY
    //    should be used.
    // 3) There are candidates on both axes. Choosing a combination such as
    //    (mTrackerOnX.mBestEdges[i].mSnapPoint.mX,
    //     mTrackerOnY.mBestEdges[i].mSnapPoint.mY)
    //    would require us to iterate over the candidates again if the
    //    combination position is outside the snapport, which we don't want to
    //    do. Instead, we choose either one of the axis' candidates.
    if ((minimumDistanceOnX == nscoord_MAX) &&
        minimumDistanceOnY != nscoord_MAX) {
      bestCandidate.y = *mTrackerOnX.mBestEdges[minimumXIndex].mSnapPoint.mY;
      return SnapDestination{bestCandidate,
                             ScrollSnapTargetIds{minimumDistanceTargetIdsOnX,
                                                 minimumDistanceTargetIdsOnX}};
    }

    if (minimumDistanceOnX != nscoord_MAX &&
        minimumDistanceOnY == nscoord_MAX) {
      bestCandidate.x = *mTrackerOnY.mBestEdges[minimumYIndex].mSnapPoint.mX;
      return SnapDestination{bestCandidate,
                             ScrollSnapTargetIds{minimumDistanceTargetIdsOnY,
                                                 minimumDistanceTargetIdsOnY}};
    }

    if (minimumDistanceOnX != nscoord_MAX &&
        minimumDistanceOnY != nscoord_MAX) {
      // If we've found candidates on both axes, choose the closest point either
      // on X axis or Y axis from the scroll destination. I.e. choose
      // `minimumXIndex` one or `minimumYIndex` one to make at least one of
      // snap target elements visible inside the snapport.
      //
      // For example,
      // [bestCandidate.x, mTrackerOnX.mBestEdges[minimumXIndex].mSnapPoint.mY]
      // is a candidate generated from a single element, thus snapping to the
      // point would definitely make the element visible inside the snapport.
      if (hypotf(NSCoordToFloat(mDestination.x -
                                mTrackerOnX.mBestEdges[0].mPosition),
                 NSCoordToFloat(minimumDistanceOnY)) <
          hypotf(NSCoordToFloat(minimumDistanceOnX),
                 NSCoordToFloat(mDestination.y -
                                mTrackerOnY.mBestEdges[0].mPosition))) {
        bestCandidate.y = *mTrackerOnX.mBestEdges[minimumXIndex].mSnapPoint.mY;
      } else {
        bestCandidate.x = *mTrackerOnY.mBestEdges[minimumYIndex].mSnapPoint.mX;
      }
      return SnapDestination{bestCandidate,
                             ScrollSnapTargetIds{minimumDistanceTargetIdsOnX,
                                                 minimumDistanceTargetIdsOnY}};
    }
    MOZ_ASSERT_UNREACHABLE("There's at least one candidate on either axis");
    // `minimumDistanceOnX == nscoord_MAX && minimumDistanceOnY == nscoord_MAX`
    // should not happen but we fall back for safety.
  }

  return SnapDestination{
      nsPoint(
          mTrackerOnX.EdgeFound() ? mTrackerOnX.mBestEdges[0].mPosition
          // In the case of IntendedEndPosition (i.e. the destination point is
          // explicitely specied, e.g. scrollTo) use the destination point if we
          // didn't find any candidates.
          : !(mSnapFlags & ScrollSnapFlags::IntendedDirection) ? mDestination.x
                                                               : mStartPos.x,
          mTrackerOnY.EdgeFound() ? mTrackerOnY.mBestEdges[0].mPosition
          // Same as above X axis case, use the destination point if we didn't
          // find any candidates.
          : !(mSnapFlags & ScrollSnapFlags::IntendedDirection) ? mDestination.y
                                                               : mStartPos.y),
      ScrollSnapTargetIds{mTrackerOnX.mTargetIds, mTrackerOnY.mTargetIds}};
}

void CalcSnapPoints::AddHorizontalEdge(const SnapTarget& aTarget) {
  MOZ_ASSERT(aTarget.mSnapPoint.mY);
  AddEdge(SnapPosition{aTarget, *aTarget.mSnapPoint.mY,
                       aTarget.mSnapPoint.mX
                           ? std::abs(mDestination.x - *aTarget.mSnapPoint.mX)
                           : nscoord_MAX},
          mDestination.y, mStartPos.y, mScrollingDirection.y, &mTrackerOnY);
}

void CalcSnapPoints::AddVerticalEdge(const SnapTarget& aTarget) {
  MOZ_ASSERT(aTarget.mSnapPoint.mX);
  AddEdge(SnapPosition{aTarget, *aTarget.mSnapPoint.mX,
                       aTarget.mSnapPoint.mY
                           ? std::abs(mDestination.y - *aTarget.mSnapPoint.mY)
                           : nscoord_MAX},
          mDestination.x, mStartPos.x, mScrollingDirection.x, &mTrackerOnX);
}

void CalcSnapPoints::AddEdge(const SnapPosition& aEdge, nscoord aDestination,
                             nscoord aStartPos, nscoord aScrollingDirection,
                             CandidateTracker* aCandidateTracker) {
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

  if (!aCandidateTracker->EdgeFound()) {
    aCandidateTracker->mBestEdges = AutoTArray<SnapPosition, 1>{aEdge};
    aCandidateTracker->mTargetIds =
        AutoTArray<ScrollSnapTargetId, 1>{aEdge.mTargetId};
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

  const bool isOnOppositeSide =
      ((aEdge.mPosition - aDestination) > 0) !=
      ((aCandidateTracker->mBestEdges[0].mPosition - aDestination) > 0);
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
        aCandidateTracker->mSecondBestEdge = aEdge.mPosition;
      } else if (isOnOppositeSide) {
        // Replace the second best edge with the current best edge only if the
        // new best edge (aEdge) is on the opposite side of the current best
        // edge.
        aCandidateTracker->mSecondBestEdge =
            aCandidateTracker->mBestEdges[0].mPosition;
      }
      aCandidateTracker->mBestEdges = AutoTArray<SnapPosition, 1>{aEdge};
      aCandidateTracker->mTargetIds =
          AutoTArray<ScrollSnapTargetId, 1>{aEdge.mTargetId};
    } else {
      if (aEdge.mPosition == aCandidateTracker->mBestEdges[0].mPosition) {
        aCandidateTracker->mTargetIds.AppendElement(aEdge.mTargetId);
        aCandidateTracker->mBestEdges.AppendElement(aEdge);
      }
      if (aIsCloserThanSecond && isOnOppositeSide) {
        aCandidateTracker->mSecondBestEdge = aEdge.mPosition;
      }
    }
  };

  bool isCandidateOfBest = false;
  bool isCandidateOfSecondBest = false;
  switch (mUnit) {
    case ScrollUnit::DEVICE_PIXELS:
    case ScrollUnit::LINES:
    case ScrollUnit::WHOLE: {
      isCandidateOfBest =
          std::abs(distanceFromDestination) <
          std::abs(aCandidateTracker->mBestEdges[0].mPosition - aDestination);
      isCandidateOfSecondBest =
          std::abs(distanceFromDestination) <
          std::abs(NSCoordSaturatingSubtract(aCandidateTracker->mSecondBestEdge,
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
          (aCandidateTracker->mBestEdges[0].mPosition - aDestination) *
          aScrollingDirection;

      nscoord secondOvershoot =
          NSCoordSaturatingSubtract(aCandidateTracker->mSecondBestEdge,
                                    aDestination, nscoord_MAX) *
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
      isCandidateOfBest =
          std::abs(distanceFromStart) <
          std::abs(aCandidateTracker->mBestEdges[0].mPosition - aStartPos);
    } else if (isPreferredStopAlways(aCandidateTracker->mBestEdges[0])) {
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
  aSnapInfo.ForEachValidTargetFor(
      aCalcSnapPoints.Destination(), [&](const auto& aTarget) -> bool {
        if (aTarget.mSnapPoint.mX && aSnapInfo.mScrollSnapStrictnessX !=
                                         StyleScrollSnapStrictness::None) {
          aCalcSnapPoints.AddVerticalEdge(aTarget);
        }
        if (aTarget.mSnapPoint.mY && aSnapInfo.mScrollSnapStrictnessY !=
                                         StyleScrollSnapStrictness::None) {
          aCalcSnapPoints.AddHorizontalEdge(aTarget);
        }
        return true;
      });
}

Maybe<SnapDestination> ScrollSnapUtils::GetSnapPointForDestination(
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
      calcSnapPoints.AddVerticalEdge(ScrollSnapInfo::SnapTarget{
          Some(clampedDestination.x), Nothing(), range.mSnapArea,
          StyleScrollSnapStop::Normal, range.mTargetId});
      break;
    }
  }
  for (auto range : aSnapInfo.mYRangeWiderThanSnapport) {
    if (range.IsValid(clampedDestination.y, aSnapInfo.mSnapportSize.height) &&
        calcSnapPoints.YDistanceBetweenBestAndSecondEdge() >
            aSnapInfo.mSnapportSize.height) {
      calcSnapPoints.AddHorizontalEdge(ScrollSnapInfo::SnapTarget{
          Nothing(), Some(clampedDestination.y), range.mSnapArea,
          StyleScrollSnapStop::Normal, range.mTargetId});
      break;
    }
  }

  bool snapped = false;
  auto finalPos = calcSnapPoints.GetBestEdge(aSnapInfo.mSnapportSize);
  constexpr float proximityRatio = 0.3;
  if (aSnapInfo.mScrollSnapStrictnessY ==
          StyleScrollSnapStrictness::Proximity &&
      std::abs(aDestination.y - finalPos.mPosition.y) >
          aSnapInfo.mSnapportSize.height * proximityRatio) {
    finalPos.mPosition.y = aDestination.y;
  } else if (aSnapInfo.mScrollSnapStrictnessY !=
                 StyleScrollSnapStrictness::None &&
             aDestination.y != finalPos.mPosition.y) {
    snapped = true;
  }
  if (aSnapInfo.mScrollSnapStrictnessX ==
          StyleScrollSnapStrictness::Proximity &&
      std::abs(aDestination.x - finalPos.mPosition.x) >
          aSnapInfo.mSnapportSize.width * proximityRatio) {
    finalPos.mPosition.x = aDestination.x;
  } else if (aSnapInfo.mScrollSnapStrictnessX !=
                 StyleScrollSnapStrictness::None &&
             aDestination.x != finalPos.mPosition.x) {
    snapped = true;
  }
  return snapped ? Some(finalPos) : Nothing();
}

ScrollSnapTargetId ScrollSnapUtils::GetTargetIdFor(const nsIFrame* aFrame) {
  MOZ_ASSERT(aFrame && aFrame->GetContent());
  return ScrollSnapTargetId{reinterpret_cast<uintptr_t>(aFrame->GetContent())};
}

static std::pair<Maybe<nscoord>, Maybe<nscoord>> GetCandidateInLastTargets(
    const ScrollSnapInfo& aSnapInfo, const nsPoint& aCurrentPosition,
    const UniquePtr<ScrollSnapTargetIds>& aLastSnapTargetIds,
    const nsIContent* aFocusedContent) {
  ScrollSnapTargetId targetIdForFocusedContent = ScrollSnapTargetId::None;
  if (aFocusedContent && aFocusedContent->GetPrimaryFrame()) {
    targetIdForFocusedContent =
        ScrollSnapUtils::GetTargetIdFor(aFocusedContent->GetPrimaryFrame());
  }

  // Note: Below algorithm doesn't care about cases where the last snap point
  // was on an element larger than the snapport since it's not clear to us
  // what we should do for now.
  // https://github.com/w3c/csswg-drafts/issues/7438
  const ScrollSnapInfo::SnapTarget* focusedTarget = nullptr;
  Maybe<nscoord> x, y;
  aSnapInfo.ForEachValidTargetFor(
      aCurrentPosition, [&](const auto& aTarget) -> bool {
        if (aTarget.mSnapPoint.mX && aSnapInfo.mScrollSnapStrictnessX !=
                                         StyleScrollSnapStrictness::None) {
          if (aLastSnapTargetIds->mIdsOnX.Contains(aTarget.mTargetId)) {
            if (targetIdForFocusedContent == aTarget.mTargetId) {
              // If we've already found the candidate on Y axis, but if snapping
              // to the point results this target is scrolled out, we can't use
              // it.
              if ((y && !aTarget.mSnapArea.Intersects(
                            nsRect(nsPoint(*aTarget.mSnapPoint.mX, *y),
                                   aSnapInfo.mSnapportSize)))) {
                y.reset();
              }

              focusedTarget = &aTarget;
              // If the focused one is valid, then it's the candidate.
              x = aTarget.mSnapPoint.mX;
            }

            if (!x) {
              // Update the candidate on X axis only if
              // 1) we haven't yet found the candidate on Y axis
              // 2) or if we've found the candiate on Y axis and if snapping to
              // the
              //    candidate position result the target element is visible
              //    inside the snapport.
              if (!y || (y && aTarget.mSnapArea.Intersects(
                                  nsRect(nsPoint(*aTarget.mSnapPoint.mX, *y),
                                         aSnapInfo.mSnapportSize)))) {
                x = aTarget.mSnapPoint.mX;
              }
            }
          }
        }
        if (aTarget.mSnapPoint.mY && aSnapInfo.mScrollSnapStrictnessY !=
                                         StyleScrollSnapStrictness::None) {
          if (aLastSnapTargetIds->mIdsOnY.Contains(aTarget.mTargetId)) {
            if (targetIdForFocusedContent == aTarget.mTargetId) {
              NS_ASSERTION(
                  !focusedTarget || focusedTarget == &aTarget,
                  "If the focused target has been found on X axis, the "
                  "target should be same");
              // If we've already found the candidate on X axis other than the
              // focused one, but if snapping to the point results this target
              // is scrolled out, we can't use it.
              if (!focusedTarget &&
                  (x && !aTarget.mSnapArea.Intersects(
                            nsRect(nsPoint(*x, *aTarget.mSnapPoint.mY),
                                   aSnapInfo.mSnapportSize)))) {
                x.reset();
              }

              focusedTarget = &aTarget;
              y = aTarget.mSnapPoint.mY;
            }

            if (!y) {
              if (!x || (x && aTarget.mSnapArea.Intersects(
                                  nsRect(nsPoint(*x, *aTarget.mSnapPoint.mY),
                                         aSnapInfo.mSnapportSize)))) {
                y = aTarget.mSnapPoint.mY;
              }
            }
          }
        }

        // If we found candidates on both axes, it's the one we need.
        if (x && y &&
            // If we haven't found the focused target, it's possible that we
            // haven't iterated it, don't break in such case.
            (targetIdForFocusedContent == ScrollSnapTargetId::None ||
             focusedTarget)) {
          return false;
        }
        return true;
      });

  return {x, y};
}

Maybe<SnapDestination> ScrollSnapUtils::GetSnapPointForResnap(
    const ScrollSnapInfo& aSnapInfo, const nsRect& aScrollRange,
    const nsPoint& aCurrentPosition,
    const UniquePtr<ScrollSnapTargetIds>& aLastSnapTargetIds,
    const nsIContent* aFocusedContent) {
  if (!aLastSnapTargetIds) {
    return GetSnapPointForDestination(aSnapInfo, ScrollUnit::DEVICE_PIXELS,
                                      ScrollSnapFlags::IntendedEndPosition,
                                      aScrollRange, aCurrentPosition,
                                      aCurrentPosition);
  }

  auto [x, y] = GetCandidateInLastTargets(aSnapInfo, aCurrentPosition,
                                          aLastSnapTargetIds, aFocusedContent);
  if (!x && !y) {
    // In the worst case there's no longer valid snap points previously snapped,
    // try to find new valid snap points.
    return GetSnapPointForDestination(aSnapInfo, ScrollUnit::DEVICE_PIXELS,
                                      ScrollSnapFlags::IntendedEndPosition,
                                      aScrollRange, aCurrentPosition,
                                      aCurrentPosition);
  }

  // If there's no candidate on one of the axes in the last snap points, try
  // to find a new candidate.
  if (!x || !y) {
    nsPoint newPosition =
        nsPoint(x ? *x : aCurrentPosition.x, y ? *y : aCurrentPosition.y);
    CalcSnapPoints calcSnapPoints(ScrollUnit::DEVICE_PIXELS,
                                  ScrollSnapFlags::IntendedEndPosition,
                                  newPosition, newPosition);

    aSnapInfo.ForEachValidTargetFor(
        newPosition, [&, &x = x, &y = y](const auto& aTarget) -> bool {
          if (!x && aTarget.mSnapPoint.mX &&
              aSnapInfo.mScrollSnapStrictnessX !=
                  StyleScrollSnapStrictness::None) {
            calcSnapPoints.AddVerticalEdge(aTarget);
          }
          if (!y && aTarget.mSnapPoint.mY &&
              aSnapInfo.mScrollSnapStrictnessY !=
                  StyleScrollSnapStrictness::None) {
            calcSnapPoints.AddHorizontalEdge(aTarget);
          }
          return true;
        });

    auto finalPos = calcSnapPoints.GetBestEdge(aSnapInfo.mSnapportSize);
    if (!x) {
      x = Some(finalPos.mPosition.x);
    }
    if (!y) {
      y = Some(finalPos.mPosition.y);
    }
  }

  SnapDestination snapTarget{nsPoint(*x, *y)};
  // Collect snap points where the position is still same as the new snap
  // position.
  aSnapInfo.ForEachValidTargetFor(
      snapTarget.mPosition, [&, &x = x, &y = y](const auto& aTarget) -> bool {
        if (aTarget.mSnapPoint.mX &&
            aSnapInfo.mScrollSnapStrictnessX !=
                StyleScrollSnapStrictness::None &&
            aTarget.mSnapPoint.mX == x) {
          snapTarget.mTargetIds.mIdsOnX.AppendElement(aTarget.mTargetId);
        }

        if (aTarget.mSnapPoint.mY &&
            aSnapInfo.mScrollSnapStrictnessY !=
                StyleScrollSnapStrictness::None &&
            aTarget.mSnapPoint.mY == y) {
          snapTarget.mTargetIds.mIdsOnY.AppendElement(aTarget.mTargetId);
        }
        return true;
      });
  return Some(snapTarget);
}

void ScrollSnapUtils::PostPendingResnapIfNeededFor(nsIFrame* aFrame) {
  ScrollSnapTargetId id = GetTargetIdFor(aFrame);
  if (id == ScrollSnapTargetId::None) {
    return;
  }

  if (nsIScrollableFrame* sf = nsLayoutUtils::GetNearestScrollableFrame(
          aFrame, nsLayoutUtils::SCROLLABLE_SAME_DOC |
                      nsLayoutUtils::SCROLLABLE_INCLUDE_HIDDEN)) {
    sf->PostPendingResnapIfNeeded(aFrame);
  }
}

void ScrollSnapUtils::PostPendingResnapFor(nsIFrame* aFrame) {
  if (nsIScrollableFrame* sf = nsLayoutUtils::GetNearestScrollableFrame(
          aFrame, nsLayoutUtils::SCROLLABLE_SAME_DOC |
                      nsLayoutUtils::SCROLLABLE_INCLUDE_HIDDEN)) {
    sf->PostPendingResnap();
  }
}

bool ScrollSnapUtils::NeedsToRespectTargetWritingMode(
    const nsSize& aSnapAreaSize, const nsSize& aSnapportSize) {
  // Use the writing-mode on the target element if the snap area is larger than
  // the snapport.
  // https://drafts.csswg.org/css-scroll-snap/#snap-scope
  //
  // It's unclear `larger` means that the size is larger than only on the target
  // axis. If it doesn't, it will pick the same axis in the case where only one
  // axis is larger. For example, if an element size is (200 x 10) and the
  // snapport size is (100 x 100) and if the element's writing mode is different
  // from the scroller's writing mode, then `scroll-snap-align: start start`
  // will be conflict.
  return aSnapAreaSize.width > aSnapportSize.width ||
         aSnapAreaSize.height > aSnapportSize.height;
}

static nsRect InflateByScrollMargin(const nsRect& aTargetRect,
                                    const nsMargin& aScrollMargin,
                                    const nsRect& aScrolledRect) {
  // Inflate the rect by scroll-margin.
  nsRect result = aTargetRect;
  result.Inflate(aScrollMargin);

  // But don't be beyond the limit boundary.
  return result.Intersect(aScrolledRect);
}

nsRect ScrollSnapUtils::GetSnapAreaFor(const nsIFrame* aFrame,
                                       const nsIFrame* aScrolledFrame,
                                       const nsRect& aScrolledRect) {
  nsRect targetRect = nsLayoutUtils::TransformFrameRectToAncestor(
      aFrame, aFrame->GetRectRelativeToSelf(), aScrolledFrame);

  // The snap area contains scroll-margin values.
  // https://drafts.csswg.org/css-scroll-snap-1/#scroll-snap-area
  nsMargin scrollMargin = aFrame->StyleMargin()->GetScrollMargin();
  return InflateByScrollMargin(targetRect, scrollMargin, aScrolledRect);
}

}  // namespace mozilla
