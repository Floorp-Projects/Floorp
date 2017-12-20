/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef NSRECT_H
#define NSRECT_H

#include <stdio.h>                      // for FILE
#include <stdint.h>                     // for int32_t, int64_t
#include <algorithm>                    // for min/max
#include "mozilla/Likely.h"             // for MOZ_UNLIKELY
#include "mozilla/gfx/Rect.h"
#include "nsCoord.h"                    // for nscoord, etc
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsPoint.h"                    // for nsIntPoint, nsPoint
#include "nsMargin.h"                   // for nsIntMargin, nsMargin
#include "nsSize.h"                     // for IntSize, nsSize
#include "nscore.h"                     // for NS_BUILD_REFCNT_LOGGING

typedef mozilla::gfx::IntRect nsIntRect;

struct nsRect :
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

  MOZ_MUST_USE nsRect SaturatingUnion(const nsRect& aRect) const
  {
    if (IsEmpty()) {
      return aRect;
    } else if (aRect.IsEmpty()) {
      return *static_cast<const nsRect*>(this);
    } else {
      return SaturatingUnionEdges(aRect);
    }
  }

  MOZ_MUST_USE nsRect SaturatingUnionEdges(const nsRect& aRect) const
  {
#ifdef NS_COORD_IS_FLOAT
    return UnionEdges(aRect);
#else
    nscoord resultX = std::min(aRect.X(), x);
    int64_t w = std::max(int64_t(aRect.X()) + aRect.Width(), int64_t(x) + width) - resultX;
    if (MOZ_UNLIKELY(w > nscoord_MAX)) {
      // Clamp huge negative x to nscoord_MIN / 2 and try again.
      resultX = std::max(resultX, nscoord_MIN / 2);
      w = std::max(int64_t(aRect.X()) + aRect.Width(), int64_t(x) + width) - resultX;
      if (MOZ_UNLIKELY(w > nscoord_MAX)) {
        w = nscoord_MAX;
      }
    }

    nscoord resultY = std::min(aRect.y, y);
    int64_t h = std::max(int64_t(aRect.Y()) + aRect.Height(), int64_t(y) + height) - resultY;
    if (MOZ_UNLIKELY(h > nscoord_MAX)) {
      // Clamp huge negative y to nscoord_MIN / 2 and try again.
      resultY = std::max(resultY, nscoord_MIN / 2);
      h = std::max(int64_t(aRect.Y()) + aRect.Height(), int64_t(y) + height) - resultY;
      if (MOZ_UNLIKELY(h > nscoord_MAX)) {
        h = nscoord_MAX;
      }
    }
    return nsRect(resultX, resultY, nscoord(w), nscoord(h));
#endif
  }

#ifndef NS_COORD_IS_FLOAT
  // Make all nsRect Union methods be saturating.
  MOZ_MUST_USE nsRect UnionEdges(const nsRect& aRect) const
  {
    return SaturatingUnionEdges(aRect);
  }
  void UnionRectEdges(const nsRect& aRect1, const nsRect& aRect2)
  {
    *this = aRect1.UnionEdges(aRect2);
  }
  MOZ_MUST_USE nsRect Union(const nsRect& aRect) const
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

  // Return whether this rect's right or bottom edge overflow int32.
  bool Overflows() const;

  /**
   * Return this rect scaled to a different appunits per pixel (APP) ratio.
   * In the RoundOut version we make the rect the smallest rect containing the
   * unrounded result. In the RoundIn version we make the rect the largest rect
   * contained in the unrounded result.
   * @param aFromAPP the APP to scale from
   * @param aToAPP the APP to scale to
   * @note this can turn an empty rectangle into a non-empty rectangle
   */
  MOZ_MUST_USE inline nsRect
    ScaleToOtherAppUnitsRoundOut(int32_t aFromAPP, int32_t aToAPP) const;
  MOZ_MUST_USE inline nsRect
    ScaleToOtherAppUnitsRoundIn(int32_t aFromAPP, int32_t aToAPP) const;

  MOZ_MUST_USE inline mozilla::gfx::IntRect
  ScaleToNearestPixels(float aXScale, float aYScale,
                       nscoord aAppUnitsPerPixel) const;

  MOZ_MUST_USE inline mozilla::gfx::IntRect
  ToNearestPixels(nscoord aAppUnitsPerPixel) const;

  // Note: this can turn an empty rectangle into a non-empty rectangle
  MOZ_MUST_USE inline mozilla::gfx::IntRect
  ScaleToOutsidePixels(float aXScale, float aYScale,
                       nscoord aAppUnitsPerPixel) const;

  // Note: this can turn an empty rectangle into a non-empty rectangle
  MOZ_MUST_USE inline mozilla::gfx::IntRect
  ToOutsidePixels(nscoord aAppUnitsPerPixel) const;

  MOZ_MUST_USE inline mozilla::gfx::IntRect
  ScaleToInsidePixels(float aXScale, float aYScale,
                      nscoord aAppUnitsPerPixel) const;

  MOZ_MUST_USE inline mozilla::gfx::IntRect
  ToInsidePixels(nscoord aAppUnitsPerPixel) const;

  // This is here only to keep IPDL-generated code happy. DO NOT USE.
  bool operator==(const nsRect& aRect) const
  {
    return IsEqualEdges(aRect);
  }

  MOZ_MUST_USE inline nsRect RemoveResolution(const float aResolution) const;
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
  rect.SetBox(NSToCoordFloor(NSCoordScale(x, aFromAPP, aToAPP)),
              NSToCoordFloor(NSCoordScale(y, aFromAPP, aToAPP)),
              NSToCoordCeil(NSCoordScale(XMost(), aFromAPP, aToAPP)),
              NSToCoordCeil(NSCoordScale(YMost(), aFromAPP, aToAPP)));
  return rect;
}

inline nsRect
nsRect::ScaleToOtherAppUnitsRoundIn(int32_t aFromAPP, int32_t aToAPP) const
{
  if (aFromAPP == aToAPP) {
    return *this;
  }

  nsRect rect;
  rect.SetBox(NSToCoordCeil(NSCoordScale(x, aFromAPP, aToAPP)),
              NSToCoordCeil(NSCoordScale(y, aFromAPP, aToAPP)),
              NSToCoordFloor(NSCoordScale(XMost(), aFromAPP, aToAPP)),
              NSToCoordFloor(NSCoordScale(YMost(), aFromAPP, aToAPP)));
  return rect;
}

// scale the rect but round to preserve centers
inline mozilla::gfx::IntRect
nsRect::ScaleToNearestPixels(float aXScale, float aYScale,
                             nscoord aAppUnitsPerPixel) const
{
  mozilla::gfx::IntRect rect;
  rect.SetNonEmptyBox(NSToIntRoundUp(NSAppUnitsToDoublePixels(x,
                                     aAppUnitsPerPixel) * aXScale),
                      NSToIntRoundUp(NSAppUnitsToDoublePixels(y,
                                     aAppUnitsPerPixel) * aYScale),
                      NSToIntRoundUp(NSAppUnitsToDoublePixels(XMost(),
                                     aAppUnitsPerPixel) * aXScale),
                      NSToIntRoundUp(NSAppUnitsToDoublePixels(YMost(),
                                     aAppUnitsPerPixel) * aYScale));
  return rect;
}

// scale the rect but round to smallest containing rect
inline mozilla::gfx::IntRect
nsRect::ScaleToOutsidePixels(float aXScale, float aYScale,
                             nscoord aAppUnitsPerPixel) const
{
  mozilla::gfx::IntRect rect;
  rect.SetNonEmptyBox(NSToIntFloor(NSAppUnitsToFloatPixels(x,
                                   float(aAppUnitsPerPixel)) * aXScale),
                      NSToIntFloor(NSAppUnitsToFloatPixels(y,
                                   float(aAppUnitsPerPixel)) * aYScale),
                      NSToIntCeil(NSAppUnitsToFloatPixels(XMost(),
                                   float(aAppUnitsPerPixel)) * aXScale),
                      NSToIntCeil(NSAppUnitsToFloatPixels(YMost(),
                                   float(aAppUnitsPerPixel)) * aYScale));
  return rect;
}

// scale the rect but round to largest contained rect
inline mozilla::gfx::IntRect
nsRect::ScaleToInsidePixels(float aXScale, float aYScale,
                            nscoord aAppUnitsPerPixel) const
{
  mozilla::gfx::IntRect rect;
  rect.SetNonEmptyBox(NSToIntCeil(NSAppUnitsToFloatPixels(x,
                                  float(aAppUnitsPerPixel)) * aXScale),
                      NSToIntCeil(NSAppUnitsToFloatPixels(y,
                                  float(aAppUnitsPerPixel)) * aYScale),
                      NSToIntFloor(NSAppUnitsToFloatPixels(XMost(),
                                   float(aAppUnitsPerPixel)) * aXScale),
                      NSToIntFloor(NSAppUnitsToFloatPixels(YMost(),
                                   float(aAppUnitsPerPixel)) * aYScale));
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

inline nsRect
nsRect::RemoveResolution(const float aResolution) const
{
  MOZ_ASSERT(aResolution > 0.0f);
  nsRect rect;
  rect.MoveTo(NSToCoordRound(NSCoordToFloat(x) / aResolution),
              NSToCoordRound(NSCoordToFloat(y) / aResolution));
  // A 1x1 rect indicates we are just hit testing a point, so pass down a 1x1
  // rect as well instead of possibly rounding the width or height to zero.
  if (width == 1 && height == 1) {
    rect.SizeTo(1, 1);
  } else {
    rect.SizeTo(NSToCoordCeil(NSCoordToFloat(width) / aResolution),
                NSToCoordCeil(NSCoordToFloat(height) / aResolution));
  }

  return rect;
}

const mozilla::gfx::IntRect& GetMaxSizedIntRect();

// app units are integer multiples of pixels, so no rounding needed
template<class units>
nsRect
ToAppUnits(const mozilla::gfx::IntRectTyped<units>& aRect, nscoord aAppUnitsPerPixel)
{
  return nsRect(NSIntPixelsToAppUnits(aRect.X(), aAppUnitsPerPixel),
                NSIntPixelsToAppUnits(aRect.Y(), aAppUnitsPerPixel),
                NSIntPixelsToAppUnits(aRect.Width(), aAppUnitsPerPixel),
                NSIntPixelsToAppUnits(aRect.Height(), aAppUnitsPerPixel));
}

#ifdef DEBUG
// Diagnostics
extern FILE* operator<<(FILE* out, const nsRect& rect);
#endif // DEBUG

#endif /* NSRECT_H */
