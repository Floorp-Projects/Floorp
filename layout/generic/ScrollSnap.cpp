/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollSnap.h"

#include "FrameMetrics.h"

#include "mozilla/ServoStyleConsts.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsTArray.h"
#include "mozilla/StaticPrefs_layout.h"

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

    SnapPosition(nscoord aPosition, StyleScrollSnapStop aScrollSnapStop,
                 ScrollSnapTargetId aTargetId)
        : mPosition(aPosition),
          mScrollSnapStop(aScrollSnapStop),
          mTargetId(aTargetId) {}

    nscoord mPosition;
    StyleScrollSnapStop mScrollSnapStop;
    ScrollSnapTargetId mTargetId;
  };

  void AddHorizontalEdge(const SnapPosition& aEdge);
  void AddVerticalEdge(const SnapPosition& aEdge);

  struct CandidateTracker {
    explicit CandidateTracker(nscoord aDestination)
        : mBestEdge(SnapPosition{aDestination, StyleScrollSnapStop::Normal,
                                 ScrollSnapTargetId::None}) {
      // We use NSCoordSaturatingSubtract to calculate the distance between a
      // given position and this second best edge position so that it can be an
      // uninitialized value as the maximum possible value, because the first
      // distance calculation would always be nscoord_MAX.
      mSecondBestEdge = SnapPosition{nscoord_MAX, StyleScrollSnapStop::Normal,
                                     ScrollSnapTargetId::None};
      mEdgeFound = false;
    }

    // keeps track of the position of the current best edge on this axis.
    SnapPosition mBestEdge;
    // keeps track of the position of the current second best edge on the
    // opposite side of the best edge on this axis.
    SnapPosition mSecondBestEdge;
    bool mEdgeFound;  // true if mBestEdge is storing a valid edge.

    // Assuming in most cases there's no multiple coincide snap points.
    AutoTArray<ScrollSnapTargetId, 1> mTargetIds;
  };
  void AddEdge(const SnapPosition& aEdge, nscoord aDestination,
               nscoord aStartPos, nscoord aScrollingDirection,
               CandidateTracker* aCandidateTracker);
  SnapTarget GetBestEdge() const;
  nscoord XDistanceBetweenBestAndSecondEdge() const {
    return std::abs(NSCoordSaturatingSubtract(
        mTrackerOnX.mSecondBestEdge.mPosition, mTrackerOnX.mBestEdge.mPosition,
        nscoord_MAX));
  }
  nscoord YDistanceBetweenBestAndSecondEdge() const {
    return std::abs(NSCoordSaturatingSubtract(
        mTrackerOnY.mSecondBestEdge.mPosition, mTrackerOnY.mBestEdge.mPosition,
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
      mStartPos(aStartPos),
      mTrackerOnX(aDestination.x),
      mTrackerOnY(aDestination.y) {
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

SnapTarget CalcSnapPoints::GetBestEdge() const {
  return SnapTarget{
      nsPoint(mTrackerOnX.mEdgeFound ? mTrackerOnX.mBestEdge.mPosition
                                     : mStartPos.x,
              mTrackerOnY.mEdgeFound ? mTrackerOnY.mBestEdge.mPosition
                                     : mStartPos.y),
      ScrollSnapTargetIds{mTrackerOnX.mTargetIds, mTrackerOnY.mTargetIds}};
}

void CalcSnapPoints::AddHorizontalEdge(const SnapPosition& aEdge) {
  AddEdge(aEdge, mDestination.y, mStartPos.y, mScrollingDirection.y,
          &mTrackerOnY);
}

void CalcSnapPoints::AddVerticalEdge(const SnapPosition& aEdge) {
  AddEdge(aEdge, mDestination.x, mStartPos.x, mScrollingDirection.x,
          &mTrackerOnX);
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

  if (!aCandidateTracker->mEdgeFound) {
    aCandidateTracker->mBestEdge = aEdge;
    aCandidateTracker->mTargetIds =
        AutoTArray<ScrollSnapTargetId, 1>{aEdge.mTargetId};
    aCandidateTracker->mEdgeFound = true;
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
      ((aCandidateTracker->mBestEdge.mPosition - aDestination) > 0);
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
        aCandidateTracker->mSecondBestEdge = aEdge;
      } else if (isOnOppositeSide) {
        // Replace the second best edge with the current best edge only if the
        // new best edge (aEdge) is on the opposite side of the current best
        // edge.
        aCandidateTracker->mSecondBestEdge = aCandidateTracker->mBestEdge;
      }
      aCandidateTracker->mBestEdge = aEdge;
      aCandidateTracker->mTargetIds =
          AutoTArray<ScrollSnapTargetId, 1>{aEdge.mTargetId};
    } else {
      if (aEdge.mPosition == aCandidateTracker->mBestEdge.mPosition) {
        aCandidateTracker->mTargetIds.AppendElement(aEdge.mTargetId);
      }
      if (aIsCloserThanSecond && isOnOppositeSide) {
        aCandidateTracker->mSecondBestEdge = aEdge;
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
          std::abs(aCandidateTracker->mBestEdge.mPosition - aDestination);
      isCandidateOfSecondBest =
          std::abs(distanceFromDestination) <
          std::abs(NSCoordSaturatingSubtract(
              aCandidateTracker->mSecondBestEdge.mPosition, aDestination,
              nscoord_MAX));
      break;
    }
    case ScrollUnit::PAGES: {
      // distance to the edge from the scrolling destination in the direction of
      // scrolling
      nscoord overshoot = distanceFromDestination * aScrollingDirection;
      // distance to the current best edge from the scrolling destination in the
      // direction of scrolling
      nscoord curOvershoot =
          (aCandidateTracker->mBestEdge.mPosition - aDestination) *
          aScrollingDirection;

      nscoord secondOvershoot =
          NSCoordSaturatingSubtract(
              aCandidateTracker->mSecondBestEdge.mPosition, aDestination,
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
      isCandidateOfBest =
          std::abs(distanceFromStart) <
          std::abs(aCandidateTracker->mBestEdge.mPosition - aStartPos);
    } else if (isPreferredStopAlways(aCandidateTracker->mBestEdge)) {
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
    if (!target.IsValidFor(aCalcSnapPoints.Destination(),
                           aSnapInfo.mSnapportSize)) {
      continue;
    }

    if (target.mSnapPositionX &&
        aSnapInfo.mScrollSnapStrictnessX != StyleScrollSnapStrictness::None) {
      aCalcSnapPoints.AddVerticalEdge(
          {*target.mSnapPositionX, target.mScrollSnapStop, target.mTargetId});
    }
    if (target.mSnapPositionY &&
        aSnapInfo.mScrollSnapStrictnessY != StyleScrollSnapStrictness::None) {
      aCalcSnapPoints.AddHorizontalEdge(
          {*target.mSnapPositionY, target.mScrollSnapStop, target.mTargetId});
    }
  }
}

Maybe<SnapTarget> ScrollSnapUtils::GetSnapPointForDestination(
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
          clampedDestination.x, StyleScrollSnapStop::Normal, range.mTargetId});
      break;
    }
  }
  for (auto range : aSnapInfo.mYRangeWiderThanSnapport) {
    if (range.IsValid(clampedDestination.y, aSnapInfo.mSnapportSize.height) &&
        calcSnapPoints.YDistanceBetweenBestAndSecondEdge() >
            aSnapInfo.mSnapportSize.height) {
      calcSnapPoints.AddHorizontalEdge(CalcSnapPoints::SnapPosition{
          clampedDestination.y, StyleScrollSnapStop::Normal, range.mTargetId});
      break;
    }
  }

  bool snapped = false;
  auto finalPos = calcSnapPoints.GetBestEdge();
  nscoord proximityThreshold =
      StaticPrefs::layout_css_scroll_snap_proximity_threshold();
  proximityThreshold = nsPresContext::CSSPixelsToAppUnits(proximityThreshold);
  if (aSnapInfo.mScrollSnapStrictnessY ==
          StyleScrollSnapStrictness::Proximity &&
      std::abs(aDestination.y - finalPos.mPosition.y) > proximityThreshold) {
    finalPos.mPosition.y = aDestination.y;
  } else if (aSnapInfo.mScrollSnapStrictnessY !=
                 StyleScrollSnapStrictness::None &&
             aDestination.y != finalPos.mPosition.y) {
    snapped = true;
  }
  if (aSnapInfo.mScrollSnapStrictnessX ==
          StyleScrollSnapStrictness::Proximity &&
      std::abs(aDestination.x - finalPos.mPosition.x) > proximityThreshold) {
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
    const layers::ScrollSnapInfo& aSnapInfo, const nsPoint& aCurrentPosition,
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
  for (const auto& target : aSnapInfo.mSnapTargets) {
    if (!target.IsValidFor(aCurrentPosition, aSnapInfo.mSnapportSize)) {
      continue;
    }

    if (target.mSnapPositionX &&
        aSnapInfo.mScrollSnapStrictnessX != StyleScrollSnapStrictness::None) {
      if (aLastSnapTargetIds->mIdsOnX.Contains(target.mTargetId)) {
        if (targetIdForFocusedContent == target.mTargetId) {
          // If we've already found the candidate on Y axis, but if snapping to
          // the point results this target is scrolled out, we can't use it.
          if ((y && !target.mSnapArea.Intersects(
                        nsRect(nsPoint(*target.mSnapPositionX, *y),
                               aSnapInfo.mSnapportSize)))) {
            y.reset();
          }

          focusedTarget = &target;
          // If the focused one is valid, then it's the candidate.
          x = target.mSnapPositionX;
        }

        if (!x) {
          // Update the candidate on X axis only if
          // 1) we haven't yet found the candidate on Y axis
          // 2) or if we've found the candiate on Y axis and if snapping to the
          //    candidate position result the target element is visible inside
          //    the snapport.
          if (!y || (y && target.mSnapArea.Intersects(
                              nsRect(nsPoint(*target.mSnapPositionX, *y),
                                     aSnapInfo.mSnapportSize)))) {
            x = target.mSnapPositionX;
          }
        }
      }
    }
    if (target.mSnapPositionY &&
        aSnapInfo.mScrollSnapStrictnessY != StyleScrollSnapStrictness::None) {
      if (aLastSnapTargetIds->mIdsOnY.Contains(target.mTargetId)) {
        if (targetIdForFocusedContent == target.mTargetId) {
          MOZ_ASSERT(!focusedTarget || focusedTarget == &target,
                     "If the focused target has been found on X axis, the "
                     "target should "
                     "be same");
          // If we've already found the candidate on X axis other than the
          // focused one, but if snapping to the point results this target is
          // scrolled out, we can't use it.
          if (!focusedTarget && (x && !target.mSnapArea.Intersects(nsRect(
                                          nsPoint(*x, *target.mSnapPositionY),
                                          aSnapInfo.mSnapportSize)))) {
            x.reset();
          }

          focusedTarget = &target;
          y = target.mSnapPositionY;
        }

        if (!y) {
          if (!x || (x && target.mSnapArea.Intersects(
                              nsRect(nsPoint(*x, *target.mSnapPositionY),
                                     aSnapInfo.mSnapportSize)))) {
            y = target.mSnapPositionY;
          }
        }
      }
    }

    // If we found candidates on both axes, it's the one we need.
    if (x && y &&
        // If we haven't found the focused target, it's possible that we haven't
        // iterated it, don't break in such case.
        (targetIdForFocusedContent == ScrollSnapTargetId::None ||
         focusedTarget)) {
      break;
    }
  }

  return {x, y};
}

Maybe<mozilla::SnapTarget> ScrollSnapUtils::GetSnapPointForResnap(
    const layers::ScrollSnapInfo& aSnapInfo, const nsRect& aScrollRange,
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
    for (const auto& target : aSnapInfo.mSnapTargets) {
      if (!target.IsValidFor(newPosition, aSnapInfo.mSnapportSize)) {
        continue;
      }

      if (!x && target.mSnapPositionX &&
          aSnapInfo.mScrollSnapStrictnessX != StyleScrollSnapStrictness::None) {
        calcSnapPoints.AddVerticalEdge(
            {*target.mSnapPositionX, target.mScrollSnapStop, target.mTargetId});
      }
      if (!y && target.mSnapPositionY &&
          aSnapInfo.mScrollSnapStrictnessY != StyleScrollSnapStrictness::None) {
        calcSnapPoints.AddHorizontalEdge(
            {*target.mSnapPositionY, target.mScrollSnapStop, target.mTargetId});
      }
    }
    auto finalPos = calcSnapPoints.GetBestEdge();
    if (!x) {
      x = Some(finalPos.mPosition.x);
    }
    if (!y) {
      y = Some(finalPos.mPosition.y);
    }
  }

  SnapTarget snapTarget{nsPoint(*x, *y)};
  // Collect snap points where the position is still same as the new snap
  // position.
  for (const auto& target : aSnapInfo.mSnapTargets) {
    if (!target.IsValidFor(snapTarget.mPosition, aSnapInfo.mSnapportSize)) {
      continue;
    }

    if (target.mSnapPositionX &&
        aSnapInfo.mScrollSnapStrictnessX != StyleScrollSnapStrictness::None &&
        target.mSnapPositionX == x) {
      snapTarget.mTargetIds.mIdsOnX.AppendElement(target.mTargetId);
    }

    if (target.mSnapPositionY &&
        aSnapInfo.mScrollSnapStrictnessY != StyleScrollSnapStrictness::None &&
        target.mSnapPositionY == y) {
      snapTarget.mTargetIds.mIdsOnY.AppendElement(target.mTargetId);
    }
  }
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
