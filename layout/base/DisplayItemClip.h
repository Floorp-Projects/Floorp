/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DISPLAYITEMCLIP_H_
#define DISPLAYITEMCLIP_H_

#include "nsRect.h"
#include "nsTArray.h"
#include "nsStyleConsts.h"

class gfxContext;
class nsDisplayItem;
class nsPresContext;
class nsRegion;

namespace mozilla {

/**
 * An DisplayItemClip represents the intersection of an optional rectangle
 * with a list of rounded rectangles (which is often empty), all in appunits.
 * It can represent everything CSS clipping can do to an element (except for
 * SVG clip-path), including no clipping at all.
 */
struct DisplayItemClip {
  struct RoundedRect {
    nsRect mRect;
    // Indices into mRadii are the NS_CORNER_* constants in nsStyleConsts.h
    nscoord mRadii[8];

    RoundedRect operator+(const nsPoint& aOffset) const {
      RoundedRect r = *this;
      r.mRect += aOffset;
      return r;
    }
    bool operator==(const RoundedRect& aOther) const {
      if (!mRect.IsEqualInterior(aOther.mRect)) {
        return false;
      }

      NS_FOR_CSS_HALF_CORNERS(corner) {
        if (mRadii[corner] != aOther.mRadii[corner]) {
          return false;
        }
      }
      return true;
    }
    bool operator!=(const RoundedRect& aOther) const {
      return !(*this == aOther);
    }
  };
  nsRect mClipRect;
  nsTArray<RoundedRect> mRoundedClipRects;
  bool mHaveClipRect;

  // Constructs a DisplayItemClip that does no clipping at all.
  DisplayItemClip() : mHaveClipRect(false) {}

  // Construct as the intersection of aOther and aClipItem.
  DisplayItemClip(const DisplayItemClip& aOther, nsDisplayItem* aClipItem);

  // Apply this |DisplayItemClip| to the given gfxContext.  Any saving of state
  // or clearing of other clips must be done by the caller.
  // See aBegin/aEnd note on ApplyRoundedRectsTo.
  void ApplyTo(gfxContext* aContext, nsPresContext* aPresContext,
               uint32_t aBegin = 0, uint32_t aEnd = UINT32_MAX);

  void ApplyRectTo(gfxContext* aContext, int32_t A2D) const;
  // Applies the rounded rects in this Clip to aContext
  // Will only apply rounded rects from aBegin (inclusive) to aEnd
  // (exclusive) or the number of rounded rects, whichever is smaller.
  void ApplyRoundedRectsTo(gfxContext* aContext, int32_t A2DPRInt32,
                           uint32_t aBegin, uint32_t aEnd) const;

  // Draw (fill) the rounded rects in this clip to aContext
  void DrawRoundedRectsTo(gfxContext* aContext, int32_t A2D,
                          uint32_t aBegin, uint32_t aEnd) const;
  // 'Draw' (create as a path, does not stroke or fill) aRoundRect to aContext
  void AddRoundedRectPathTo(gfxContext* aContext, int32_t A2D,
                            const RoundedRect &aRoundRect) const;

  // Return a rectangle contained in the intersection of aRect with this
  // clip region. Tries to return the largest possible rectangle, but may
  // not succeed.
  nsRect ApproximateIntersect(const nsRect& aRect) const;

  // Returns false if aRect is definitely not clipped by a rounded corner in
  // this clip. Returns true if aRect is clipped by a rounded corner in this
  // clip or it can not be quickly determined that it is not clipped by a
  // rounded corner in this clip.
  bool IsRectClippedByRoundedCorner(const nsRect& aRect) const;

  // Intersection of all rects in this clip ignoring any rounded corners.
  nsRect NonRoundedIntersection() const;

  // Intersect the given rects with all rects in this clip, ignoring any
  // rounded corners.
  nsRect ApplyNonRoundedIntersection(const nsRect& aRect) const;

  // Gets rid of any rounded corners in this clip.
  void RemoveRoundedCorners();

  // Adds the difference between Intersect(*this + aPoint, aBounds) and
  // Intersect(aOther, aOtherBounds) to aDifference (or a bounding-box thereof).
  void AddOffsetAndComputeDifference(const nsPoint& aPoint, const nsRect& aBounds,
                                     const DisplayItemClip& aOther, const nsRect& aOtherBounds,
                                     nsRegion* aDifference);

  bool operator==(const DisplayItemClip& aOther) const {
    return mHaveClipRect == aOther.mHaveClipRect &&
           (!mHaveClipRect || mClipRect.IsEqualInterior(aOther.mClipRect)) &&
           mRoundedClipRects == aOther.mRoundedClipRects;
  }
  bool operator!=(const DisplayItemClip& aOther) const {
    return !(*this == aOther);
  }
};

}

#endif /* DISPLAYITEMCLIP_H_ */
