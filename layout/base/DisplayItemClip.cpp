/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DisplayItemClip.h"

#include "gfxContext.h"
#include "nsPresContext.h"
#include "nsDisplayList.h"
#include "nsCSSRendering.h"
#include "nsLayoutUtils.h"

namespace mozilla {

DisplayItemClip::DisplayItemClip(const DisplayItemClip& aOther, nsDisplayItem* aClipItem)
  : mRoundedClipRects(aOther.mRoundedClipRects),
    mHaveClipRect(true)
{
  nsDisplayItem::Type type = aClipItem->GetType();
  NS_ABORT_IF_FALSE(type == nsDisplayItem::TYPE_CLIP ||
                    type == nsDisplayItem::TYPE_CLIP_ROUNDED_RECT,
                    "unexpected display item type");
  nsDisplayClip* item = static_cast<nsDisplayClip*>(aClipItem);
  // Always intersect with mClipRect, even if we're going to add a
  // rounded rect.
  if (aOther.mHaveClipRect) {
    mClipRect.IntersectRect(aOther.mClipRect, item->GetClipRect());
  } else {
    mClipRect = item->GetClipRect();
  }

  if (type == nsDisplayItem::TYPE_CLIP_ROUNDED_RECT) {
    RoundedRect *rr = mRoundedClipRects.AppendElement();
    if (rr) {
      rr->mRect = item->GetClipRect();
      static_cast<nsDisplayClipRoundedRect*>(item)->GetRadii(rr->mRadii);
    }
  }

  // FIXME: Optimize away excess rounded rectangles due to the new addition.
}

void
DisplayItemClip::ApplyTo(gfxContext* aContext,
                         nsPresContext* aPresContext,
                         uint32_t aBegin, uint32_t aEnd)
{
  int32_t A2D = aPresContext->AppUnitsPerDevPixel();
  ApplyRectTo(aContext, A2D);
  ApplyRoundedRectsTo(aContext, A2D, aBegin, aEnd);
}

void
DisplayItemClip::ApplyRectTo(gfxContext* aContext, int32_t A2D) const
{
  aContext->NewPath();
  gfxRect clip = nsLayoutUtils::RectToGfxRect(mClipRect, A2D);
  aContext->Rectangle(clip, true);
  aContext->Clip();
}

void
DisplayItemClip::ApplyRoundedRectsTo(gfxContext* aContext,
                                     int32_t A2D,
                                     uint32_t aBegin, uint32_t aEnd) const
{
  aEnd = std::min<uint32_t>(aEnd, mRoundedClipRects.Length());

  for (uint32_t i = aBegin; i < aEnd; ++i) {
    AddRoundedRectPathTo(aContext, A2D, mRoundedClipRects[i]);
    aContext->Clip();
  }
}

void
DisplayItemClip::DrawRoundedRectsTo(gfxContext* aContext,
                                    int32_t A2D,
                                    uint32_t aBegin, uint32_t aEnd) const
{
  aEnd = std::min<uint32_t>(aEnd, mRoundedClipRects.Length());

  if (aEnd - aBegin == 0)
    return;

  // If there is just one rounded rect we can just fill it, if there are more then we
  // must clip the rest to get the intersection of clips
  ApplyRoundedRectsTo(aContext, A2D, aBegin, aEnd - 1);
  AddRoundedRectPathTo(aContext, A2D, mRoundedClipRects[aEnd - 1]);
  aContext->Fill();
}

void
DisplayItemClip::AddRoundedRectPathTo(gfxContext* aContext,
                                      int32_t A2D,
                                      const RoundedRect &aRoundRect) const
{
  gfxCornerSizes pixelRadii;
  nsCSSRendering::ComputePixelRadii(aRoundRect.mRadii, A2D, &pixelRadii);

  gfxRect clip = nsLayoutUtils::RectToGfxRect(aRoundRect.mRect, A2D);
  clip.Round();
  clip.Condition();

  aContext->NewPath();
  aContext->RoundedRectangle(clip, pixelRadii);
}

nsRect
DisplayItemClip::ApproximateIntersect(const nsRect& aRect) const
{
  nsRect r = aRect;
  if (mHaveClipRect) {
    r.IntersectRect(r, mClipRect);
  }
  for (uint32_t i = 0, iEnd = mRoundedClipRects.Length();
       i < iEnd; ++i) {
    const RoundedRect &rr = mRoundedClipRects[i];
    nsRegion rgn = nsLayoutUtils::RoundedRectIntersectRect(rr.mRect, rr.mRadii, r);
    r = rgn.GetLargestRectangle();
  }
  return r;
}

// Test if (aXPoint, aYPoint) is in the ellipse with center (aXCenter, aYCenter)
// and radii aXRadius, aYRadius.
bool IsInsideEllipse(nscoord aXRadius, nscoord aXCenter, nscoord aXPoint,
                     nscoord aYRadius, nscoord aYCenter, nscoord aYPoint)
{
  float scaledX = float(aXPoint - aXCenter) / float(aXRadius);
  float scaledY = float(aYPoint - aYCenter) / float(aYRadius);
  return scaledX * scaledX + scaledY * scaledY < 1.0f;
}

bool
DisplayItemClip::IsRectClippedByRoundedCorner(const nsRect& aRect) const
{
  if (mRoundedClipRects.IsEmpty())
    return false;

  nsRect rect;
  rect.IntersectRect(aRect, NonRoundedIntersection());
  for (uint32_t i = 0, iEnd = mRoundedClipRects.Length();
       i < iEnd; ++i) {
    const RoundedRect &rr = mRoundedClipRects[i];
    // top left
    if (rect.x < rr.mRect.x + rr.mRadii[NS_CORNER_TOP_LEFT_X] &&
        rect.y < rr.mRect.y + rr.mRadii[NS_CORNER_TOP_LEFT_Y]) {
      if (!IsInsideEllipse(rr.mRadii[NS_CORNER_TOP_LEFT_X],
                           rr.mRect.x + rr.mRadii[NS_CORNER_TOP_LEFT_X],
                           rect.x,
                           rr.mRadii[NS_CORNER_TOP_LEFT_Y],
                           rr.mRect.y + rr.mRadii[NS_CORNER_TOP_LEFT_Y],
                           rect.y)) {
        return true;
      }
    }
    // top right
    if (rect.XMost() > rr.mRect.XMost() - rr.mRadii[NS_CORNER_TOP_RIGHT_X] &&
        rect.y < rr.mRect.y + rr.mRadii[NS_CORNER_TOP_RIGHT_Y]) {
      if (!IsInsideEllipse(rr.mRadii[NS_CORNER_TOP_RIGHT_X],
                           rr.mRect.XMost() - rr.mRadii[NS_CORNER_TOP_RIGHT_X],
                           rect.XMost(),
                           rr.mRadii[NS_CORNER_TOP_RIGHT_Y],
                           rr.mRect.y + rr.mRadii[NS_CORNER_TOP_RIGHT_Y],
                           rect.y)) {
        return true;
      }
    }
    // bottom left
    if (rect.x < rr.mRect.x + rr.mRadii[NS_CORNER_BOTTOM_LEFT_X] &&
        rect.YMost() > rr.mRect.YMost() - rr.mRadii[NS_CORNER_BOTTOM_LEFT_Y]) {
      if (!IsInsideEllipse(rr.mRadii[NS_CORNER_BOTTOM_LEFT_X],
                           rr.mRect.x + rr.mRadii[NS_CORNER_BOTTOM_LEFT_X],
                           rect.x,
                           rr.mRadii[NS_CORNER_BOTTOM_LEFT_Y],
                           rr.mRect.YMost() - rr.mRadii[NS_CORNER_BOTTOM_LEFT_Y],
                           rect.YMost())) {
        return true;
      }
    }
    // bottom right
    if (rect.XMost() > rr.mRect.XMost() - rr.mRadii[NS_CORNER_BOTTOM_RIGHT_X] &&
        rect.YMost() > rr.mRect.YMost() - rr.mRadii[NS_CORNER_BOTTOM_RIGHT_Y]) {
      if (!IsInsideEllipse(rr.mRadii[NS_CORNER_BOTTOM_RIGHT_X],
                           rr.mRect.XMost() - rr.mRadii[NS_CORNER_BOTTOM_RIGHT_X],
                           rect.XMost(),
                           rr.mRadii[NS_CORNER_BOTTOM_RIGHT_Y],
                           rr.mRect.YMost() - rr.mRadii[NS_CORNER_BOTTOM_RIGHT_Y],
                           rect.YMost())) {
        return true;
      }
    }
  }
  return false;
}

nsRect
DisplayItemClip::NonRoundedIntersection() const
{
  NS_ASSERTION(mHaveClipRect, "Must have a clip rect!");
  nsRect result = mClipRect;
  for (uint32_t i = 0, iEnd = mRoundedClipRects.Length();
       i < iEnd; ++i) {
    result.IntersectRect(result, mRoundedClipRects[i].mRect);
  }
  return result;
}

nsRect
DisplayItemClip::ApplyNonRoundedIntersection(const nsRect& aRect) const
{
  if (!mHaveClipRect) {
    return aRect;
  }

  nsRect result = aRect.Intersect(mClipRect);
  for (uint32_t i = 0, iEnd = mRoundedClipRects.Length();
       i < iEnd; ++i) {
    result.Intersect(mRoundedClipRects[i].mRect);
  }
  return result;
}

void
DisplayItemClip::RemoveRoundedCorners()
{
  if (mRoundedClipRects.IsEmpty())
    return;

  mClipRect = NonRoundedIntersection();
  mRoundedClipRects.Clear();
}

static void
AccumulateRectDifference(const nsRect& aR1, const nsRect& aR2, nsRegion* aOut)
{
  if (aR1.IsEqualInterior(aR2))
    return;
  nsRegion r;
  r.Xor(aR1, aR2);
  aOut->Or(*aOut, r);
}

void
DisplayItemClip::AddOffsetAndComputeDifference(const nsPoint& aOffset,
                                               const nsRect& aBounds,
                                               const DisplayItemClip& aOther,
                                               const nsRect& aOtherBounds,
                                               nsRegion* aDifference)
{
  if (mHaveClipRect != aOther.mHaveClipRect ||
      mRoundedClipRects.Length() != aOther.mRoundedClipRects.Length()) {
    aDifference->Or(*aDifference, aBounds);
    aDifference->Or(*aDifference, aOtherBounds);
    return;
  }
  if (mHaveClipRect) {
    AccumulateRectDifference((mClipRect + aOffset).Intersect(aBounds),
                             aOther.mClipRect.Intersect(aOtherBounds),
                             aDifference);
  }
  for (uint32_t i = 0; i < mRoundedClipRects.Length(); ++i) {
    if (mRoundedClipRects[i] + aOffset != aOther.mRoundedClipRects[i]) {
      // The corners make it tricky so we'll just add both rects here.
      aDifference->Or(*aDifference, mRoundedClipRects[i].mRect.Intersect(aBounds));
      aDifference->Or(*aDifference, aOther.mRoundedClipRects[i].mRect.Intersect(aOtherBounds));
    }
  }
}

}
