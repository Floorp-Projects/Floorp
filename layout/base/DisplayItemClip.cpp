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

void
DisplayItemClip::SetTo(const nsRect& aRect)
{
  mHaveClipRect = true;
  mClipRect = aRect;
  mRoundedClipRects.Clear();
}

void
DisplayItemClip::SetTo(const nsRect& aRect, const nscoord* aRadii)
{
  mHaveClipRect = true;
  mClipRect = aRect;
  mRoundedClipRects.SetLength(1);
  mRoundedClipRects[0].mRect = aRect;
  memcpy(mRoundedClipRects[0].mRadii, aRadii, sizeof(nscoord)*8);
}

bool
DisplayItemClip::MayIntersect(const nsRect& aRect) const
{
  if (!mHaveClipRect) {
    return !aRect.IsEmpty();
  }
  nsRect r = aRect.Intersect(mClipRect);
  if (r.IsEmpty()) {
    return false;
  }
  for (uint32_t i = 0; i < mRoundedClipRects.Length(); ++i) {
    const RoundedRect& rr = mRoundedClipRects[i];
    if (!nsLayoutUtils::RoundedRectIntersectsRect(rr.mRect, rr.mRadii, r)) {
      return false;
    }
  }
  return true;
}

void
DisplayItemClip::IntersectWith(const DisplayItemClip& aOther)
{
  if (!aOther.mHaveClipRect) {
    return;
  }
  if (!mHaveClipRect) {
    *this = aOther;
    return;
  }
  if (!mClipRect.IntersectRect(mClipRect, aOther.mClipRect)) {
    mRoundedClipRects.Clear();
    return;
  }
  mRoundedClipRects.AppendElements(aOther.mRoundedClipRects);
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
DisplayItemClip::ApproximateIntersectInward(const nsRect& aRect) const
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

bool
DisplayItemClip::IsRectAffectedByClip(const nsRect& aRect) const
{
  if (mHaveClipRect && !mClipRect.Contains(aRect)) {
    return true;
  }
  for (uint32_t i = 0, iEnd = mRoundedClipRects.Length();
       i < iEnd; ++i) {
    const RoundedRect &rr = mRoundedClipRects[i];
    nsRegion rgn = nsLayoutUtils::RoundedRectIntersectRect(rr.mRect, rr.mRadii, aRect);
    if (!rgn.Contains(aRect)) {
      return true;
    }
  }
  return false;
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

uint32_t
DisplayItemClip::GetCommonRoundedRectCount(const DisplayItemClip& aOther,
                                           uint32_t aMax) const
{
  uint32_t end = std::min(std::min(mRoundedClipRects.Length(), aMax),
                          aOther.mRoundedClipRects.Length());
  uint32_t clipCount = 0;
  for (; clipCount < end; ++clipCount) {
    if (mRoundedClipRects[clipCount] !=
        aOther.mRoundedClipRects[clipCount]) {
      return clipCount;
    }
  }
  return clipCount;
}

void
DisplayItemClip::AppendRoundedRects(nsTArray<RoundedRect>* aArray, uint32_t aCount) const
{
  uint32_t count = std::min(mRoundedClipRects.Length(), aCount);
  for (uint32_t i = 0; i < count; ++i) {
    *aArray->AppendElement() = mRoundedClipRects[i];
  }
}

bool
DisplayItemClip::ComputeRegionInClips(DisplayItemClip* aOldClip,
                                      const nsPoint& aShift,
                                      nsRegion* aCombined) const
{
  if (!mHaveClipRect || (aOldClip && !aOldClip->mHaveClipRect)) {
    return false;
  }

  if (aOldClip) {
    *aCombined = aOldClip->NonRoundedIntersection();
    aCombined->MoveBy(aShift);
    aCombined->Or(*aCombined, NonRoundedIntersection());
  } else {
    *aCombined = NonRoundedIntersection();
  }
  return true;
}

void
DisplayItemClip::MoveBy(nsPoint aPoint)
{
  if (!mHaveClipRect)
    return;
  mClipRect += aPoint;
  for (uint32_t i = 0; i < mRoundedClipRects.Length(); ++i) {
    mRoundedClipRects[i].mRect += aPoint;
  }
}

static DisplayItemClip* gNoClip;

const DisplayItemClip&
DisplayItemClip::NoClip()
{
  if (!gNoClip) {
    gNoClip = new DisplayItemClip();
  }
  return *gNoClip;
}

void
DisplayItemClip::Shutdown()
{
  delete gNoClip;
  gNoClip = nullptr;
}

#ifdef DEBUG
nsCString
DisplayItemClip::ToString() const
{
  nsAutoCString str;
  if (mHaveClipRect) {
    str.AppendPrintf("%d,%d,%d,%d", mClipRect.x, mClipRect.y,
                     mClipRect.width, mClipRect.height);
    for (uint32_t i = 0; i < mRoundedClipRects.Length(); ++i) {
      const RoundedRect& r = mRoundedClipRects[i];
      str.AppendPrintf(" [%d,%d,%d,%d corners %d,%d,%d,%d,%d,%d,%d,%d]",
                       r.mRect.x, r.mRect.y, r.mRect.width, r.mRect.height,
                       r.mRadii[0], r.mRadii[1], r.mRadii[2], r.mRadii[3],
                       r.mRadii[4], r.mRadii[5], r.mRadii[6], r.mRadii[7]);
    }
  }
  return str;
}
#endif

}
