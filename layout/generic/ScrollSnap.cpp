/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FrameMetrics.h"
#include "ScrollSnap.h"
#include "gfxPrefs.h"
#include "mozilla/Maybe.h"
#include "mozilla/Preferences.h"
#include "nsLineLayout.h"

namespace mozilla {

using layers::ScrollSnapInfo;

/**
 * Stores candidate snapping edges.
 */
class SnappingEdgeCallback {
public:
  virtual void AddHorizontalEdge(nscoord aEdge) = 0;
  virtual void AddVerticalEdge(nscoord aEdge) = 0;
  virtual void AddHorizontalEdgeInterval(const nsRect &aScrollRange,
                                         nscoord aInterval,
                                         nscoord aOffset) = 0;
  virtual void AddVerticalEdgeInterval(const nsRect &aScrollRange,
                                       nscoord aInterval,
                                       nscoord aOffset) = 0;
};

/**
 * Keeps track of the current best edge to snap to. The criteria for
 * adding an edge depends on the scrolling unit.
 */
class CalcSnapPoints : public SnappingEdgeCallback {
public:
  CalcSnapPoints(nsIScrollableFrame::ScrollUnit aUnit,
                 const nsPoint& aDestination,
                 const nsPoint& aStartPos);
  virtual void AddHorizontalEdge(nscoord aEdge) override;
  virtual void AddVerticalEdge(nscoord aEdge) override;
  virtual void AddHorizontalEdgeInterval(const nsRect &aScrollRange,
                                         nscoord aInterval, nscoord aOffset)
                                         override;
  virtual void AddVerticalEdgeInterval(const nsRect &aScrollRange,
                                       nscoord aInterval, nscoord aOffset)
                                       override;
  void AddEdge(nscoord aEdge,
               nscoord aDestination,
               nscoord aStartPos,
               nscoord aScrollingDirection,
               nscoord* aBestEdge,
               bool* aEdgeFound);
  void AddEdgeInterval(nscoord aInterval,
                       nscoord aMinPos,
                       nscoord aMaxPos,
                       nscoord aOffset,
                       nscoord aDestination,
                       nscoord aStartPos,
                       nscoord aScrollingDirection,
                       nscoord* aBestEdge,
                       bool* aEdgeFound);
  nsPoint GetBestEdge() const;
protected:
  nsIScrollableFrame::ScrollUnit mUnit;
  nsPoint mDestination;            // gives the position after scrolling but before snapping
  nsPoint mStartPos;               // gives the position before scrolling
  nsIntPoint mScrollingDirection;  // always -1, 0, or 1
  nsPoint mBestEdge;               // keeps track of the position of the current best edge
  bool mHorizontalEdgeFound;       // true if mBestEdge.x is storing a valid horizontal edge
  bool mVerticalEdgeFound;         // true if mBestEdge.y is storing a valid vertical edge
};

CalcSnapPoints::CalcSnapPoints(nsIScrollableFrame::ScrollUnit aUnit,
                               const nsPoint& aDestination,
                               const nsPoint& aStartPos)
{
  mUnit = aUnit;
  mDestination = aDestination;
  mStartPos = aStartPos;

  nsPoint direction = aDestination - aStartPos;
  mScrollingDirection = nsIntPoint(0,0);
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
  mHorizontalEdgeFound = false;
  mVerticalEdgeFound = false;
}

nsPoint
CalcSnapPoints::GetBestEdge() const
{
  return nsPoint(mVerticalEdgeFound ? mBestEdge.x : mStartPos.x,
                 mHorizontalEdgeFound ? mBestEdge.y : mStartPos.y);
}

void
CalcSnapPoints::AddHorizontalEdge(nscoord aEdge)
{
  AddEdge(aEdge, mDestination.y, mStartPos.y, mScrollingDirection.y, &mBestEdge.y,
          &mHorizontalEdgeFound);
}

void
CalcSnapPoints::AddVerticalEdge(nscoord aEdge)
{
  AddEdge(aEdge, mDestination.x, mStartPos.x, mScrollingDirection.x, &mBestEdge.x,
          &mVerticalEdgeFound);
}

void
CalcSnapPoints::AddHorizontalEdgeInterval(const nsRect &aScrollRange,
                                          nscoord aInterval, nscoord aOffset)
{
  AddEdgeInterval(aInterval, aScrollRange.y, aScrollRange.YMost(), aOffset,
                  mDestination.y, mStartPos.y, mScrollingDirection.y,
                  &mBestEdge.y, &mHorizontalEdgeFound);
}

void
CalcSnapPoints::AddVerticalEdgeInterval(const nsRect &aScrollRange,
                                        nscoord aInterval, nscoord aOffset)
{
  AddEdgeInterval(aInterval, aScrollRange.x, aScrollRange.XMost(), aOffset,
                  mDestination.x, mStartPos.x, mScrollingDirection.x,
                  &mBestEdge.x, &mVerticalEdgeFound);
}

void
CalcSnapPoints::AddEdge(nscoord aEdge, nscoord aDestination, nscoord aStartPos,
                        nscoord aScrollingDirection, nscoord* aBestEdge,
                        bool *aEdgeFound)
{
  // nsIScrollableFrame::DEVICE_PIXELS indicates that we are releasing a drag
  // gesture or any other user input event that sets an absolute scroll
  // position.  In this case, scroll snapping is expected to travel in any
  // direction.  Otherwise, we will restrict the direction of the scroll
  // snapping movement based on aScrollingDirection.
  if (mUnit != nsIScrollableFrame::DEVICE_PIXELS) {
    // Unless DEVICE_PIXELS, we only want to snap to points ahead of the
    // direction we are scrolling
    if (aScrollingDirection == 0) {
      // The scroll direction is neutral - will not hit a snap point.
      return;
    }
    // nsIScrollableFrame::WHOLE indicates that we are navigating to "home" or
    // "end".  In this case, we will always select the first or last snap point
    // regardless of the direction of the scroll.  Otherwise, we will select
    // scroll snapping points only in the direction specified by
    // aScrollingDirection.
    if (mUnit != nsIScrollableFrame::WHOLE) {
      // Direction of the edge from the current position (before scrolling) in
      // the direction of scrolling
      nscoord direction = (aEdge - aStartPos) * aScrollingDirection;
      if (direction <= 0) {
        // The edge is not in the direction we are scrolling, skip it.
        return;
      }
    }
  }
  if (!*aEdgeFound) {
    *aBestEdge = aEdge;
    *aEdgeFound = true;
    return;
  }
  if (mUnit == nsIScrollableFrame::DEVICE_PIXELS ||
      mUnit == nsIScrollableFrame::LINES) {
    if (std::abs(aEdge - aDestination) < std::abs(*aBestEdge - aDestination)) {
      *aBestEdge = aEdge;
    }
  } else if (mUnit == nsIScrollableFrame::PAGES) {
    // distance to the edge from the scrolling destination in the direction of scrolling
    nscoord overshoot = (aEdge - aDestination) * aScrollingDirection;
    // distance to the current best edge from the scrolling destination in the direction of scrolling
    nscoord curOvershoot = (*aBestEdge - aDestination) * aScrollingDirection;

    // edges between the current position and the scrolling destination are favoured
    // to preserve context
    if (overshoot < 0 && (overshoot > curOvershoot || curOvershoot >= 0)) {
      *aBestEdge = aEdge;
    }
    // if there are no edges between the current position and the scrolling destination
    // the closest edge beyond the destination is used
    if (overshoot > 0 && overshoot < curOvershoot) {
      *aBestEdge = aEdge;
    }
  } else if (mUnit == nsIScrollableFrame::WHOLE) {
    // the edge closest to the top/bottom/left/right is used, depending on scrolling direction
    if (aScrollingDirection > 0 && aEdge > *aBestEdge) {
      *aBestEdge = aEdge;
    } else if (aScrollingDirection < 0 && aEdge < *aBestEdge) {
      *aBestEdge = aEdge;
    }
  } else {
    NS_ERROR("Invalid scroll mode");
    return;
  }
}

void
CalcSnapPoints::AddEdgeInterval(nscoord aInterval, nscoord aMinPos,
                                nscoord aMaxPos, nscoord aOffset,
                                nscoord aDestination, nscoord aStartPos,
                                nscoord aScrollingDirection,
                                nscoord* aBestEdge, bool *aEdgeFound)
{
  if (aInterval == 0) {
    // When interval is 0, there are no scroll snap points.
    // Avoid division by zero and bail.
    return;
  }

  // The only possible candidate interval snap points are the edges immediately
  // surrounding aDestination.

  // aDestination must be clamped to the scroll
  // range in order to handle cases where the best matching snap point would
  // result in scrolling out of bounds.  This clamping must be prior to
  // selecting the two interval edges.
  nscoord clamped = std::max(std::min(aDestination, aMaxPos), aMinPos);

  // Add each edge in the interval immediately before aTarget and after aTarget
  // Do not add edges that are out of range.
  nscoord r = (clamped + aOffset) % aInterval;
  if (r < aMinPos) {
    r += aInterval;
  }
  nscoord edge = clamped - r;
  if (edge >= aMinPos && edge <= aMaxPos) {
    AddEdge(edge, aDestination, aStartPos, aScrollingDirection, aBestEdge,
            aEdgeFound);
  }
  edge += aInterval;
  if (edge >= aMinPos && edge <= aMaxPos) {
    AddEdge(edge, aDestination, aStartPos, aScrollingDirection, aBestEdge,
            aEdgeFound);
  }
}

static void
ProcessScrollSnapCoordinates(SnappingEdgeCallback& aCallback,
                             const nsTArray<nsPoint>& aScrollSnapCoordinates,
                             const nsPoint& aScrollSnapDestination) {
  for (nsPoint snapCoords : aScrollSnapCoordinates) {
    // Make them relative to the scroll snap destination.
    snapCoords -= aScrollSnapDestination;

    aCallback.AddVerticalEdge(snapCoords.x);
    aCallback.AddHorizontalEdge(snapCoords.y);
  }
}

Maybe<nsPoint> ScrollSnapUtils::GetSnapPointForDestination(
    const ScrollSnapInfo& aSnapInfo,
    nsIScrollableFrame::ScrollUnit aUnit,
    const nsSize& aScrollPortSize,
    const nsRect& aScrollRange,
    const nsPoint& aStartPos,
    const nsPoint& aDestination)
{
  if (aSnapInfo.mScrollSnapTypeY == NS_STYLE_SCROLL_SNAP_TYPE_NONE &&
      aSnapInfo.mScrollSnapTypeX == NS_STYLE_SCROLL_SNAP_TYPE_NONE) {
    return Nothing();
  }

  nsPoint destPos = aSnapInfo.mScrollSnapDestination;

  CalcSnapPoints calcSnapPoints(aUnit, aDestination, aStartPos);

  if (aSnapInfo.mScrollSnapIntervalX.isSome()) {
    nscoord interval = aSnapInfo.mScrollSnapIntervalX.value();
    calcSnapPoints.AddVerticalEdgeInterval(aScrollRange, interval, destPos.x);
  }
  if (aSnapInfo.mScrollSnapIntervalY.isSome()) {
    nscoord interval = aSnapInfo.mScrollSnapIntervalY.value();
    calcSnapPoints.AddHorizontalEdgeInterval(aScrollRange, interval, destPos.y);
  }

  ProcessScrollSnapCoordinates(calcSnapPoints, aSnapInfo.mScrollSnapCoordinates, destPos);
  bool snapped = false;
  nsPoint finalPos = calcSnapPoints.GetBestEdge();
  nscoord proximityThreshold = gfxPrefs::ScrollSnapProximityThreshold();
  proximityThreshold = nsPresContext::CSSPixelsToAppUnits(proximityThreshold);
  if (aSnapInfo.mScrollSnapTypeY == NS_STYLE_SCROLL_SNAP_TYPE_PROXIMITY &&
      std::abs(aDestination.y - finalPos.y) > proximityThreshold) {
    finalPos.y = aDestination.y;
  } else {
    snapped = true;
  }
  if (aSnapInfo.mScrollSnapTypeX == NS_STYLE_SCROLL_SNAP_TYPE_PROXIMITY &&
      std::abs(aDestination.x - finalPos.x) > proximityThreshold) {
    finalPos.x = aDestination.x;
  } else {
    snapped = true;
  }
  return snapped ? Some(finalPos) : Nothing();
}

}  // namespace mozilla
