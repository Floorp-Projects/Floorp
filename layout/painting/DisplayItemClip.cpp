/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DisplayItemClip.h"

#include "gfxContext.h"
#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "nsPresContext.h"
#include "nsCSSRendering.h"
#include "nsLayoutUtils.h"
#include "nsRegion.h"

using namespace mozilla::gfx;

namespace mozilla {

void
DisplayItemClip::SetTo(const nsRect& aRect)
{
  SetTo(aRect, nullptr);
}

void
DisplayItemClip::SetTo(const nsRect& aRect, const nscoord* aRadii)
{
  mHaveClipRect = true;
  mClipRect = aRect;
  if (aRadii) {
    mRoundedClipRects.SetLength(1);
    mRoundedClipRects[0].mRect = aRect;
    memcpy(mRoundedClipRects[0].mRadii, aRadii, sizeof(nscoord)*8);
  } else {
    mRoundedClipRects.Clear();
  }
}

void
DisplayItemClip::SetTo(const nsRect& aRect,
                       const nsRect& aRoundedRect,
                       const nscoord* aRadii)
{
  mHaveClipRect = true;
  mClipRect = aRect;
  mRoundedClipRects.SetLength(1);
  mRoundedClipRects[0].mRect = aRoundedRect;
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
                         int32_t A2D) const
{
  ApplyRectTo(aContext, A2D);
  ApplyRoundedRectClipsTo(aContext, A2D, 0, mRoundedClipRects.Length());
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
DisplayItemClip::ApplyRoundedRectClipsTo(gfxContext* aContext,
                                         int32_t A2D,
                                         uint32_t aBegin, uint32_t aEnd) const
{
  DrawTarget& aDrawTarget = *aContext->GetDrawTarget();

  aEnd = std::min<uint32_t>(aEnd, mRoundedClipRects.Length());

  for (uint32_t i = aBegin; i < aEnd; ++i) {
    RefPtr<Path> roundedRect =
      MakeRoundedRectPath(aDrawTarget, A2D, mRoundedClipRects[i]);
    aContext->Clip(roundedRect);
  }
}

void
DisplayItemClip::FillIntersectionOfRoundedRectClips(gfxContext* aContext,
                                                    const Color& aColor,
                                                    int32_t aAppUnitsPerDevPixel) const
{
  DrawTarget& aDrawTarget = *aContext->GetDrawTarget();

  uint32_t end = mRoundedClipRects.Length();
  if (!end) {
    return;
  }

  // Push clips for any rects that come BEFORE the rect at |aEnd - 1|, if any:
  ApplyRoundedRectClipsTo(aContext, aAppUnitsPerDevPixel, 0, end - 1);

  // Now fill the rect at |aEnd - 1|:
  RefPtr<Path> roundedRect = MakeRoundedRectPath(aDrawTarget,
                                                 aAppUnitsPerDevPixel,
                                                 mRoundedClipRects[end - 1]);
  ColorPattern color(ToDeviceColor(aColor));
  aDrawTarget.Fill(roundedRect, color);

  // Finally, pop any clips that we may have pushed:
  for (uint32_t i = 0; i < end - 1; ++i) {
    aContext->PopClip();
  }
}

already_AddRefed<Path>
DisplayItemClip::MakeRoundedRectPath(DrawTarget& aDrawTarget,
                                     int32_t A2D,
                                     const RoundedRect &aRoundRect) const
{
  RectCornerRadii pixelRadii;
  nsCSSRendering::ComputePixelRadii(aRoundRect.mRadii, A2D, &pixelRadii);

  Rect rect = NSRectToSnappedRect(aRoundRect.mRect, A2D, aDrawTarget);

  return MakePathForRoundedRect(aDrawTarget, rect, pixelRadii);
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
static bool
IsInsideEllipse(nscoord aXRadius, nscoord aXCenter, nscoord aXPoint,
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
    if (rect.x < rr.mRect.x + rr.mRadii[eCornerTopLeftX] &&
        rect.y < rr.mRect.y + rr.mRadii[eCornerTopLeftY]) {
      if (!IsInsideEllipse(rr.mRadii[eCornerTopLeftX],
                           rr.mRect.x + rr.mRadii[eCornerTopLeftX],
                           rect.x,
                           rr.mRadii[eCornerTopLeftY],
                           rr.mRect.y + rr.mRadii[eCornerTopLeftY],
                           rect.y)) {
        return true;
      }
    }
    // top right
    if (rect.XMost() > rr.mRect.XMost() - rr.mRadii[eCornerTopRightX] &&
        rect.y < rr.mRect.y + rr.mRadii[eCornerTopRightY]) {
      if (!IsInsideEllipse(rr.mRadii[eCornerTopRightX],
                           rr.mRect.XMost() - rr.mRadii[eCornerTopRightX],
                           rect.XMost(),
                           rr.mRadii[eCornerTopRightY],
                           rr.mRect.y + rr.mRadii[eCornerTopRightY],
                           rect.y)) {
        return true;
      }
    }
    // bottom left
    if (rect.x < rr.mRect.x + rr.mRadii[eCornerBottomLeftX] &&
        rect.YMost() > rr.mRect.YMost() - rr.mRadii[eCornerBottomLeftY]) {
      if (!IsInsideEllipse(rr.mRadii[eCornerBottomLeftX],
                           rr.mRect.x + rr.mRadii[eCornerBottomLeftX],
                           rect.x,
                           rr.mRadii[eCornerBottomLeftY],
                           rr.mRect.YMost() - rr.mRadii[eCornerBottomLeftY],
                           rect.YMost())) {
        return true;
      }
    }
    // bottom right
    if (rect.XMost() > rr.mRect.XMost() - rr.mRadii[eCornerBottomRightX] &&
        rect.YMost() > rr.mRect.YMost() - rr.mRadii[eCornerBottomRightY]) {
      if (!IsInsideEllipse(rr.mRadii[eCornerBottomRightX],
                           rr.mRect.XMost() - rr.mRadii[eCornerBottomRightX],
                           rect.XMost(),
                           rr.mRadii[eCornerBottomRightY],
                           rr.mRect.YMost() - rr.mRadii[eCornerBottomRightY],
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

bool
DisplayItemClip::IsRectAffectedByClip(const nsIntRect& aRect,
                                      float aXScale,
                                      float aYScale,
                                      int32_t A2D) const
{
  if (mHaveClipRect) {
    nsIntRect pixelClipRect = mClipRect.ScaleToNearestPixels(aXScale, aYScale, A2D);
    if (!pixelClipRect.Contains(aRect)) {
      return true;
    }
  }

  // Rounded rect clipping only snaps to user-space pixels, not device space.
  nsIntRect unscaled = aRect;
  unscaled.Scale(1/aXScale, 1/aYScale);

  for (uint32_t i = 0, iEnd = mRoundedClipRects.Length();
       i < iEnd; ++i) {
    const RoundedRect &rr = mRoundedClipRects[i];

    nsIntRect pixelRect = rr.mRect.ToNearestPixels(A2D);

    RectCornerRadii pixelRadii;
    nsCSSRendering::ComputePixelRadii(rr.mRadii, A2D, &pixelRadii);

    nsIntRegion rgn = nsLayoutUtils::RoundedRectIntersectIntRect(pixelRect, pixelRadii, unscaled);
    if (!rgn.Contains(unscaled)) {
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
    result = result.Intersect(mRoundedClipRects[i].mRect);
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

// Computes the difference between aR1 and aR2, limited to aBounds.
static void
AccumulateRectDifference(const nsRect& aR1, const nsRect& aR2, const nsRect& aBounds, nsRegion* aOut)
{
  if (aR1.IsEqualInterior(aR2))
    return;
  nsRegion r;
  r.Xor(aR1, aR2);
  r.And(r, aBounds);
  aOut->Or(*aOut, r);
}

static void
AccumulateRoundedRectDifference(const DisplayItemClip::RoundedRect& aR1,
                                const DisplayItemClip::RoundedRect& aR2,
                                const nsRect& aBounds,
                                const nsRect& aOtherBounds,
                                nsRegion* aOut)
{
  const nsRect& rect1 = aR1.mRect;
  const nsRect& rect2 = aR2.mRect;

  // If the two rectangles are totally disjoint, just add them both - otherwise we'd
  // end up adding one big enclosing rect
  if (!rect1.Intersects(rect2) || memcmp(aR1.mRadii, aR2.mRadii, sizeof(aR1.mRadii))) {
    aOut->Or(*aOut, rect1.Intersect(aBounds));
    aOut->Or(*aOut, rect2.Intersect(aOtherBounds));
    return;
  }

  nscoord lowestBottom = std::max(rect1.YMost(), rect2.YMost());
  nscoord highestTop = std::min(rect1.Y(), rect2.Y());
  nscoord maxRight = std::max(rect1.XMost(), rect2.XMost());
  nscoord minLeft = std::min(rect1.X(), rect2.X());

  // At this point, we know that the radii haven't changed, and that the bounds
  // are different in some way. To explain how this works, consider the case
  // where the rounded rect has just been translated along the X direction.
  // |          ______________________ _ _ _ _ _ _            |
  // |        /           /            \           \          |
  // |       |                          |                     |
  // |       |     aR1   |              |     aR2   |         |
  // |       |                          |                     |
  // |        \ __________\___________ / _ _ _ _ _ /          |
  // |                                                        |
  // The invalidation region will be as if we lopped off the left rounded part
  // of aR2, and the right rounded part of aR1, and XOR'd them:
  // |          ______________________ _ _ _ _ _ _            |
  // |       -/-----------/-          -\-----------\-         |
  // |       |--------------          --|------------         |
  // |       |-----aR1---|--          --|-----aR2---|         |
  // |       |--------------          --|------------         |
  // |       -\ __________\-__________-/ _ _ _ _ _ /-          |
  // |                                                        |
  // The logic below just implements this idea, but generalized to both the
  // X and Y dimensions. The "(...)Adjusted(...)" values represent the lopped
  // off sides.
  nscoord highestAdjustedBottom =
    std::min(rect1.YMost() - aR1.mRadii[eCornerBottomLeftY],
             std::min(rect1.YMost() - aR1.mRadii[eCornerBottomRightY],
                      std::min(rect2.YMost() - aR2.mRadii[eCornerBottomLeftY],
                               rect2.YMost() - aR2.mRadii[eCornerBottomRightY])));
  nscoord lowestAdjustedTop =
    std::max(rect1.Y() + aR1.mRadii[eCornerTopLeftY],
             std::max(rect1.Y() + aR1.mRadii[eCornerTopRightY],
                      std::max(rect2.Y() + aR2.mRadii[eCornerTopLeftY],
                               rect2.Y() + aR2.mRadii[eCornerTopRightY])));

  nscoord minAdjustedRight =
    std::min(rect1.XMost() - aR1.mRadii[eCornerTopRightX],
             std::min(rect1.XMost() - aR1.mRadii[eCornerBottomRightX],
                      std::min(rect2.XMost() - aR2.mRadii[eCornerTopRightX],
                               rect2.XMost() - aR2.mRadii[eCornerBottomRightX])));
  nscoord maxAdjustedLeft =
    std::max(rect1.X() + aR1.mRadii[eCornerTopLeftX],
             std::max(rect1.X() + aR1.mRadii[eCornerBottomLeftX],
                      std::max(rect2.X() + aR2.mRadii[eCornerTopLeftX],
                               rect2.X() + aR2.mRadii[eCornerBottomLeftX])));

  // We only want to add an invalidation rect if the bounds have changed. If we always
  // added all of the 4 rects below, we would always be invalidating a border around the
  // rects, even in cases where we just translated along the X or Y axis.
  nsRegion r;
  // First, or with the Y delta rects, wide along the X axis
  if (rect1.Y() != rect2.Y()) {
    r.Or(r, nsRect(minLeft, highestTop,
                   maxRight - minLeft, lowestAdjustedTop - highestTop));
  }
  if (rect1.YMost() != rect2.YMost()) {
    r.Or(r, nsRect(minLeft, highestAdjustedBottom,
                   maxRight - minLeft, lowestBottom - highestAdjustedBottom));
  }
  // Then, or with the X delta rects, narrow along the Y axis
  if (rect1.X() != rect2.X()) {
    r.Or(r, nsRect(minLeft, lowestAdjustedTop,
                   maxAdjustedLeft - minLeft, highestAdjustedBottom - lowestAdjustedTop));
  }
  if (rect1.XMost() != rect2.XMost()) {
    r.Or(r, nsRect(minAdjustedRight, lowestAdjustedTop,
                   maxRight - minAdjustedRight, highestAdjustedBottom - lowestAdjustedTop));
  }

  r.And(r, aBounds.Union(aOtherBounds));
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
    AccumulateRectDifference(mClipRect + aOffset, aOther.mClipRect,
                             aBounds.Union(aOtherBounds),
                             aDifference);
  }
  for (uint32_t i = 0; i < mRoundedClipRects.Length(); ++i) {
    if (mRoundedClipRects[i] + aOffset != aOther.mRoundedClipRects[i]) {
      AccumulateRoundedRectDifference(mRoundedClipRects[i] + aOffset,
                                      aOther.mRoundedClipRects[i],
                                      aBounds,
                                      aOtherBounds,
                                      aDifference);
    }
  }
}

void
DisplayItemClip::AppendRoundedRects(nsTArray<RoundedRect>* aArray) const
{
  aArray->AppendElements(mRoundedClipRects.Elements(), mRoundedClipRects.Length());
}

bool
DisplayItemClip::ComputeRegionInClips(const DisplayItemClip* aOldClip,
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
DisplayItemClip::MoveBy(const nsPoint& aPoint)
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

void
DisplayItemClip::ToComplexClipRegions(int32_t aAppUnitsPerDevPixel,
                                      const layers::StackingContextHelper& aSc,
                                      nsTArray<wr::ComplexClipRegion>& aOutArray) const
{
  for (uint32_t i = 0; i < mRoundedClipRects.Length(); i++) {
    wr::ComplexClipRegion* region = aOutArray.AppendElement();
    region->rect = wr::ToRoundedLayoutRect(LayoutDeviceRect::FromAppUnits(
        mRoundedClipRects[i].mRect, aAppUnitsPerDevPixel));
    const nscoord* radii = mRoundedClipRects[i].mRadii;
    region->radii = wr::ToBorderRadius(
        LayoutDeviceSize::FromAppUnits(nsSize(radii[eCornerTopLeftX], radii[eCornerTopLeftY]), aAppUnitsPerDevPixel),
        LayoutDeviceSize::FromAppUnits(nsSize(radii[eCornerTopRightX], radii[eCornerTopRightY]), aAppUnitsPerDevPixel),
        LayoutDeviceSize::FromAppUnits(nsSize(radii[eCornerBottomLeftX], radii[eCornerBottomLeftY]), aAppUnitsPerDevPixel),
        LayoutDeviceSize::FromAppUnits(nsSize(radii[eCornerBottomRightX], radii[eCornerBottomRightY]), aAppUnitsPerDevPixel));
    region->mode = wr::ClipMode::Clip;
  }
}

} // namespace mozilla
