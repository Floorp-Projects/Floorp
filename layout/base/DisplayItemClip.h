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
class DisplayItemClip {
public:
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

  // Constructs a DisplayItemClip that does no clipping at all.
  DisplayItemClip() : mHaveClipRect(false), mHasBeenDestroyed(false) {}
  ~DisplayItemClip() { mHasBeenDestroyed = true; }

  void MaybeDestroy() const
  {
    if (!mHasBeenDestroyed) {
      this->~DisplayItemClip();
    }
  }

  void SetTo(const nsRect& aRect);
  void SetTo(const nsRect& aRect, const nscoord* aRadii);
  void IntersectWith(const DisplayItemClip& aOther);

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

  // Returns true if the intersection of aRect and this clip region is
  // non-empty. This is precise for DisplayItemClips with at most one
  // rounded rectangle. When multiple rounded rectangles are present, we just
  // check that the rectangle intersects all of them (but possibly in different
  // places). So it may return true when the correct answer is false.
  bool MayIntersect(const nsRect& aRect) const;
  // Return a rectangle contained in the intersection of aRect with this
  // clip region. Tries to return the largest possible rectangle, but may
  // not succeed.
  nsRect ApproximateIntersectInward(const nsRect& aRect) const;

  /*
   * Computes a region which contains the clipped area of this DisplayItemClip,
   * or if aOldClip is non-null, the union of the clipped area of this
   * DisplayItemClip with the clipped area of aOldClip translated by aShift.
   * The result is stored in aCombined. If the result would be infinite
   * (because one or both of the clips does no clipping), returns false.
   */
  bool ComputeRegionInClips(DisplayItemClip* aOldClip,
                            const nsPoint& aShift,
                            nsRegion* aCombined) const;

  // Returns false if aRect is definitely not clipped by a rounded corner in
  // this clip. Returns true if aRect is clipped by a rounded corner in this
  // clip or it can not be quickly determined that it is not clipped by a
  // rounded corner in this clip.
  bool IsRectClippedByRoundedCorner(const nsRect& aRect) const;

  // Returns false if aRect is definitely not clipped by anything in this clip.
  // Fast but not necessarily accurate.
  bool IsRectAffectedByClip(const nsRect& aRect) const;

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

  bool HasClip() const { return mHaveClipRect; }
  const nsRect& GetClipRect() const
  {
    NS_ASSERTION(HasClip(), "No clip rect!");
    return mClipRect;
  }

  void MoveBy(nsPoint aPoint);

#ifdef MOZ_DUMP_PAINTING
  nsCString ToString() const;
#endif

  /**
   * Find the largest N such that the first N rounded rects in 'this' are
   * equal to the first N rounded rects in aOther, and N <= aMax.
   */
  uint32_t GetCommonRoundedRectCount(const DisplayItemClip& aOther,
                                     uint32_t aMax) const;
  uint32_t GetRoundedRectCount() const { return mRoundedClipRects.Length(); }
  void AppendRoundedRects(nsTArray<RoundedRect>* aArray, uint32_t aCount) const;

  static const DisplayItemClip& NoClip();

  static void Shutdown();

private:
  nsRect mClipRect;
  nsTArray<RoundedRect> mRoundedClipRects;
  // If mHaveClipRect is false then this object represents no clipping at all
  // and mRoundedClipRects must be empty.
  bool mHaveClipRect;
  // Set to true when the destructor has run. This is a bit of a hack
  // to ensure that we can easily share arena-allocated DisplayItemClips.
  bool mHasBeenDestroyed;
};

}

#endif /* DISPLAYITEMCLIP_H_ */
