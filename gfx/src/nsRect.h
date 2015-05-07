/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef NSRECT_H
#define NSRECT_H

#include <stdio.h>                      // for FILE
#include <stdint.h>                     // for int32_t, int64_t
#include <algorithm>                    // for min/max
#include "nsDebug.h"                    // for NS_WARNING
#include "gfxCore.h"                    // for NS_GFX
#include "mozilla/Likely.h"             // for MOZ_UNLIKELY
#include "mozilla/gfx/Rect.h"
#include "nsCoord.h"                    // for nscoord, etc
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsPoint.h"                    // for nsIntPoint, nsPoint
#include "nsSize.h"                     // for nsIntSize, nsSize
#include "nscore.h"                     // for NS_BUILD_REFCNT_LOGGING

struct nsMargin;
struct nsIntMargin;

typedef mozilla::gfx::IntRect nsIntRect;

struct NS_GFX nsRect :
  public mozilla::gfx::BaseRect<nscoord, nsRect, nsPoint, nsSize, nsMargin> {
  typedef mozilla::gfx::BaseRect<nscoord, nsRect, nsPoint, nsSize, nsMargin> Super;

  static void VERIFY_COORD(nscoord aValue) { ::VERIFY_COORD(aValue); }

  // Constructors
  nsRect() : Super()
  {
    MOZ_COUNT_CTOR(nsRect);
  }
  nsRect(const nsRect& aRect) : Super(aRect)
  {
    MOZ_COUNT_CTOR(nsRect);
  }
  nsRect(const nsPoint& aOrigin, const nsSize &aSize) : Super(aOrigin, aSize)
  {
    MOZ_COUNT_CTOR(nsRect);
  }
  nsRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight) :
      Super(aX, aY, aWidth, aHeight)
  {
    MOZ_COUNT_CTOR(nsRect);
  }

#ifdef NS_BUILD_REFCNT_LOGGING
  ~nsRect() {
    MOZ_COUNT_DTOR(nsRect);
  }
#endif

  // We have saturating versions of all the Union methods. These avoid
  // overflowing nscoord values in the 'width' and 'height' fields by
  // clamping the width and height values to nscoord_MAX if necessary.

  MOZ_WARN_UNUSED_RESULT nsRect SaturatingUnion(const nsRect& aRect) const
  {
    if (IsEmpty()) {
      return aRect;
    } else if (aRect.IsEmpty()) {
      return *static_cast<const nsRect*>(this);
    } else {
      return SaturatingUnionEdges(aRect);
    }
  }

  MOZ_WARN_UNUSED_RESULT nsRect SaturatingUnionEdges(const nsRect& aRect) const
  {
#ifdef NS_COORD_IS_FLOAT
    return UnionEdges(aRect);
#else
    nsRect result;
    result.x = std::min(aRect.x, x);
    int64_t w = std::max(int64_t(aRect.x) + aRect.width, int64_t(x) + width) - result.x;
    if (MOZ_UNLIKELY(w > nscoord_MAX)) {
      NS_WARNING("Overflowed nscoord_MAX in conversion to nscoord width");
      // Clamp huge negative x to nscoord_MIN / 2 and try again.
      result.x = std::max(result.x, nscoord_MIN / 2);
      w = std::max(int64_t(aRect.x) + aRect.width, int64_t(x) + width) - result.x;
      if (MOZ_UNLIKELY(w > nscoord_MAX)) {
        w = nscoord_MAX;
      }
    }
    result.width = nscoord(w);

    result.y = std::min(aRect.y, y);
    int64_t h = std::max(int64_t(aRect.y) + aRect.height, int64_t(y) + height) - result.y;
    if (MOZ_UNLIKELY(h > nscoord_MAX)) {
      NS_WARNING("Overflowed nscoord_MAX in conversion to nscoord height");
      // Clamp huge negative y to nscoord_MIN / 2 and try again.
      result.y = std::max(result.y, nscoord_MIN / 2);
      h = std::max(int64_t(aRect.y) + aRect.height, int64_t(y) + height) - result.y;
      if (MOZ_UNLIKELY(h > nscoord_MAX)) {
        h = nscoord_MAX;
      }
    }
    result.height = nscoord(h);
    return result;
#endif
  }

#ifndef NS_COORD_IS_FLOAT
  // Make all nsRect Union methods be saturating.
  MOZ_WARN_UNUSED_RESULT nsRect UnionEdges(const nsRect& aRect) const
  {
    return SaturatingUnionEdges(aRect);
  }
  void UnionRectEdges(const nsRect& aRect1, const nsRect& aRect2)
  {
    *this = aRect1.UnionEdges(aRect2);
  }
  MOZ_WARN_UNUSED_RESULT nsRect Union(const nsRect& aRect) const
  {
    return SaturatingUnion(aRect);
  }
  void UnionRect(const nsRect& aRect1, const nsRect& aRect2)
  {
    *this = aRect1.Union(aRect2);
  }
#endif

  void SaturatingUnionRect(const nsRect& aRect1, const nsRect& aRect2)
  {
    *this = aRect1.SaturatingUnion(aRect2);
  }
  void SaturatingUnionRectEdges(const nsRect& aRect1, const nsRect& aRect2)
  {
    *this = aRect1.SaturatingUnionEdges(aRect2);
  }

  /**
   * Return this rect scaled to a different appunits per pixel (APP) ratio.
   * In the RoundOut version we make the rect the smallest rect containing the
   * unrounded result. In the RoundIn version we make the rect the largest rect
   * contained in the unrounded result.
   * @param aFromAPP the APP to scale from
   * @param aToAPP the APP to scale to
   * @note this can turn an empty rectangle into a non-empty rectangle
   */
  MOZ_WARN_UNUSED_RESULT inline nsRect
    ScaleToOtherAppUnitsRoundOut(int32_t aFromAPP, int32_t aToAPP) const;
  MOZ_WARN_UNUSED_RESULT inline nsRect
    ScaleToOtherAppUnitsRoundIn(int32_t aFromAPP, int32_t aToAPP) const;

  MOZ_WARN_UNUSED_RESULT inline mozilla::gfx::IntRect
  ScaleToNearestPixels(float aXScale, float aYScale,
                       nscoord aAppUnitsPerPixel) const;

  MOZ_WARN_UNUSED_RESULT inline mozilla::gfx::IntRect
  ToNearestPixels(nscoord aAppUnitsPerPixel) const;

  // Note: this can turn an empty rectangle into a non-empty rectangle
  MOZ_WARN_UNUSED_RESULT inline mozilla::gfx::IntRect
  ScaleToOutsidePixels(float aXScale, float aYScale,
                       nscoord aAppUnitsPerPixel) const;

  // Note: this can turn an empty rectangle into a non-empty rectangle
  MOZ_WARN_UNUSED_RESULT inline mozilla::gfx::IntRect
  ToOutsidePixels(nscoord aAppUnitsPerPixel) const;

  MOZ_WARN_UNUSED_RESULT inline mozilla::gfx::IntRect
  ScaleToInsidePixels(float aXScale, float aYScale,
                      nscoord aAppUnitsPerPixel) const;

  MOZ_WARN_UNUSED_RESULT inline mozilla::gfx::IntRect
  ToInsidePixels(nscoord aAppUnitsPerPixel) const;

  // This is here only to keep IPDL-generated code happy. DO NOT USE.
  bool operator==(const nsRect& aRect) const
  {
    return IsEqualEdges(aRect);
  }
};

/*
 * App Unit/Pixel conversions
 */

inline nsRect
nsRect::ScaleToOtherAppUnitsRoundOut(int32_t aFromAPP, int32_t aToAPP) const
{
  if (aFromAPP == aToAPP) {
    return *this;
  }

  nsRect rect;
  nscoord right = NSToCoordCeil(NSCoordScale(XMost(), aFromAPP, aToAPP));
  nscoord bottom = NSToCoordCeil(NSCoordScale(YMost(), aFromAPP, aToAPP));
  rect.x = NSToCoordFloor(NSCoordScale(x, aFromAPP, aToAPP));
  rect.y = NSToCoordFloor(NSCoordScale(y, aFromAPP, aToAPP));
  rect.width = (right - rect.x);
  rect.height = (bottom - rect.y);

  return rect;
}

inline nsRect
nsRect::ScaleToOtherAppUnitsRoundIn(int32_t aFromAPP, int32_t aToAPP) const
{
  if (aFromAPP == aToAPP) {
    return *this;
  }

  nsRect rect;
  nscoord right = NSToCoordFloor(NSCoordScale(XMost(), aFromAPP, aToAPP));
  nscoord bottom = NSToCoordFloor(NSCoordScale(YMost(), aFromAPP, aToAPP));
  rect.x = NSToCoordCeil(NSCoordScale(x, aFromAPP, aToAPP));
  rect.y = NSToCoordCeil(NSCoordScale(y, aFromAPP, aToAPP));
  rect.width = (right - rect.x);
  rect.height = (bottom - rect.y);

  return rect;
}

// scale the rect but round to preserve centers
inline mozilla::gfx::IntRect
nsRect::ScaleToNearestPixels(float aXScale, float aYScale,
                             nscoord aAppUnitsPerPixel) const
{
  mozilla::gfx::IntRect rect;
  rect.x = NSToIntRoundUp(NSAppUnitsToDoublePixels(x, aAppUnitsPerPixel) * aXScale);
  rect.y = NSToIntRoundUp(NSAppUnitsToDoublePixels(y, aAppUnitsPerPixel) * aYScale);
  // Avoid negative widths and heights due to overflow
  rect.width  = std::max(0, NSToIntRoundUp(NSAppUnitsToDoublePixels(XMost(),
                               aAppUnitsPerPixel) * aXScale) - rect.x);
  rect.height = std::max(0, NSToIntRoundUp(NSAppUnitsToDoublePixels(YMost(),
                               aAppUnitsPerPixel) * aYScale) - rect.y);
  return rect;
}

// scale the rect but round to smallest containing rect
inline mozilla::gfx::IntRect
nsRect::ScaleToOutsidePixels(float aXScale, float aYScale,
                             nscoord aAppUnitsPerPixel) const
{
  mozilla::gfx::IntRect rect;
  rect.x = NSToIntFloor(NSAppUnitsToFloatPixels(x, float(aAppUnitsPerPixel)) * aXScale);
  rect.y = NSToIntFloor(NSAppUnitsToFloatPixels(y, float(aAppUnitsPerPixel)) * aYScale);
  // Avoid negative widths and heights due to overflow
  rect.width  = std::max(0, NSToIntCeil(NSAppUnitsToFloatPixels(XMost(),
                            float(aAppUnitsPerPixel)) * aXScale) - rect.x);
  rect.height = std::max(0, NSToIntCeil(NSAppUnitsToFloatPixels(YMost(),
                            float(aAppUnitsPerPixel)) * aYScale) - rect.y);
  return rect;
}

// scale the rect but round to largest contained rect
inline mozilla::gfx::IntRect
nsRect::ScaleToInsidePixels(float aXScale, float aYScale,
                            nscoord aAppUnitsPerPixel) const
{
  mozilla::gfx::IntRect rect;
  rect.x = NSToIntCeil(NSAppUnitsToFloatPixels(x, float(aAppUnitsPerPixel)) * aXScale);
  rect.y = NSToIntCeil(NSAppUnitsToFloatPixels(y, float(aAppUnitsPerPixel)) * aYScale);
  // Avoid negative widths and heights due to overflow
  rect.width  = std::max(0, NSToIntFloor(NSAppUnitsToFloatPixels(XMost(),
                             float(aAppUnitsPerPixel)) * aXScale) - rect.x);
  rect.height = std::max(0, NSToIntFloor(NSAppUnitsToFloatPixels(YMost(),
                             float(aAppUnitsPerPixel)) * aYScale) - rect.y);
  return rect;
}

inline mozilla::gfx::IntRect
nsRect::ToNearestPixels(nscoord aAppUnitsPerPixel) const
{
  return ScaleToNearestPixels(1.0f, 1.0f, aAppUnitsPerPixel);
}

inline mozilla::gfx::IntRect
nsRect::ToOutsidePixels(nscoord aAppUnitsPerPixel) const
{
  return ScaleToOutsidePixels(1.0f, 1.0f, aAppUnitsPerPixel);
}

inline mozilla::gfx::IntRect
nsRect::ToInsidePixels(nscoord aAppUnitsPerPixel) const
{
  return ScaleToInsidePixels(1.0f, 1.0f, aAppUnitsPerPixel);
}

const mozilla::gfx::IntRect& GetMaxSizedIntRect();

// app units are integer multiples of pixels, so no rounding needed
nsRect
ToAppUnits(const mozilla::gfx::IntRect& aRect, nscoord aAppUnitsPerPixel);

#ifdef DEBUG
// Diagnostics
extern NS_GFX FILE* operator<<(FILE* out, const nsRect& rect);
#endif // DEBUG

#endif /* NSRECT_H */
