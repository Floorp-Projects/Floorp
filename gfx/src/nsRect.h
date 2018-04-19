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
#include "mozilla/gfx/2D.h"
#include "nsCoord.h"                    // for nscoord, etc
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsPoint.h"                    // for nsIntPoint, nsPoint
#include "nsMargin.h"                   // for nsIntMargin, nsMargin
#include "nsSize.h"                     // for IntSize, nsSize
#include "nscore.h"                     // for NS_BUILD_REFCNT_LOGGING
#if !defined(ANDROID) && !defined(MOZ_ASAN) && (defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#include "smmintrin.h"
#endif

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
  MOZ_MUST_USE nsRect UnsafeUnion(const nsRect& aRect) const
  {
    return Super::Union(aRect);
  }
  void UnionRect(const nsRect& aRect1, const nsRect& aRect2)
  {
    *this = aRect1.Union(aRect2);
  }

#if defined(_MSC_VER) && !defined(__clang__)
  // Only MSVC supports inlining intrinsics for archs you're not compiling for.
  MOZ_MUST_USE nsRect Intersect(const nsRect& aRect) const
  {
    nsRect result;
    if (mozilla::gfx::Factory::HasSSE4()) {
      __m128i rect1 = _mm_loadu_si128((__m128i*)&aRect); // x1, y1, w1, h1
      __m128i rect2 = _mm_loadu_si128((__m128i*)this); // x2, y2, w2, h2

      __m128i resultRect = _mm_max_epi32(rect1, rect2); // xr, yr, zz, zz


      // result.width = std::min<int32_t>(x - result.x + width, aRect.x - result.x + aRect.width);
      // result.height = std::min<int32_t>(y - result.y + height, aRect.y - result.y + aRect.height);
      __m128i widthheight = _mm_min_epi32(_mm_add_epi32(_mm_sub_epi32(rect1, resultRect), _mm_srli_si128(rect1, 8)),
                                          _mm_add_epi32(_mm_sub_epi32(rect2, resultRect), _mm_srli_si128(rect2, 8))); // w, h, zz, zz
      widthheight = _mm_slli_si128(widthheight, 8); // 00, 00, wr, hr

      resultRect = _mm_blend_epi16(resultRect, widthheight, 0xF0); // xr, yr, wr, hr

      if ((_mm_movemask_ps(_mm_castsi128_ps(_mm_cmpgt_epi32(resultRect, _mm_setzero_si128()))) & 0xC) != 0xC) {
        // It's potentially more efficient to store all 0s. But the non SSE4 code leaves x/y intact
        // so let's do the same here.
        resultRect = _mm_and_si128(resultRect, _mm_set_epi32(0, 0, 0xFFFFFFFF, 0xFFFFFFFF));
      }

      _mm_storeu_si128((__m128i*)&result, resultRect);

      return result;
    }

    result.x = std::max<int32_t>(x, aRect.x);
    result.y = std::max<int32_t>(y, aRect.y);
    result.width = std::min<int32_t>(x - result.x + width, aRect.x - result.x + aRect.width);
    result.height = std::min<int32_t>(y - result.y + height, aRect.y - result.y + aRect.height);
    if (result.width <= 0 || result.height <= 0) {
      result.SizeTo(0, 0);
    }
    return result;
  }

  bool IntersectRect(const nsRect& aRect1, const nsRect& aRect2)
  {
    if (mozilla::gfx::Factory::HasSSE4()) {
      __m128i rect1 = _mm_loadu_si128((__m128i*)&aRect1); // x1, y1, w1, h1
      __m128i rect2 = _mm_loadu_si128((__m128i*)&aRect2); // x2, y2, w2, h2

      __m128i resultRect = _mm_max_epi32(rect1, rect2); // xr, yr, zz, zz
      // result.width = std::min<int32_t>(x - result.x + width, aRect.x - result.x + aRect.width);
      // result.height = std::min<int32_t>(y - result.y + height, aRect.y - result.y + aRect.height);
      __m128i widthheight = _mm_min_epi32(_mm_add_epi32(_mm_sub_epi32(rect1, resultRect), _mm_srli_si128(rect1, 8)),
                                          _mm_add_epi32(_mm_sub_epi32(rect2, resultRect), _mm_srli_si128(rect2, 8))); // w, h, zz, zz
      widthheight = _mm_slli_si128(widthheight, 8); // 00, 00, wr, hr

      resultRect = _mm_blend_epi16(resultRect, widthheight, 0xF0); // xr, yr, wr, hr

      if ((_mm_movemask_ps(_mm_castsi128_ps(_mm_cmpgt_epi32(resultRect, _mm_setzero_si128()))) & 0xC) != 0xC) {
        // It's potentially more efficient to store all 0s. But the non SSE4 code leaves x/y intact
        // so let's do the same here.
        resultRect = _mm_and_si128(resultRect, _mm_set_epi32(0, 0, 0xFFFFFFFF, 0xFFFFFFFF));
        _mm_storeu_si128((__m128i*)this, resultRect);
        return false;
      }

      _mm_storeu_si128((__m128i*)this, resultRect);

      return true;
    }
    *static_cast<nsRect*>(this) = aRect1.Intersect(aRect2);
    return !IsEmpty();
  }
#endif
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
  // ASAN builds appear not to respect changes to the SSE rounding mode.
  // Android x86 builds have bindgen issues.
#if !defined(ANDROID) && !defined(MOZ_ASAN) && (defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
  __m128 appUnitsPacked = _mm_set_ps(aAppUnitsPerPixel, aAppUnitsPerPixel, aAppUnitsPerPixel, aAppUnitsPerPixel);
  __m128 scalesPacked = _mm_set_ps(aYScale, aXScale, aYScale, aXScale);
  __m128 biasesPacked = _mm_set_ps(0.5f, 0.5f, 0.5f, 0.5f);

  // See Floor section.
  _MM_SET_ROUNDING_MODE(_MM_ROUND_DOWN);

  __m128i rectPacked = _mm_loadu_si128((__m128i*)this);
  __m128i widthHeight = _mm_slli_si128(rectPacked, 8);

  rectPacked = _mm_add_epi32(rectPacked, widthHeight); // X, Y, XMost(), YMost()

  __m128 rectFloat = _mm_cvtepi32_ps(rectPacked);

  // Scale, i.e. ([ x y xmost ymost ] / aAppUnitsPerPixel) * [ aXScale aYScale aXScale aYScale ]
  rectFloat = _mm_div_ps(_mm_mul_ps(rectFloat, scalesPacked), appUnitsPacked);

  // Floor
  // Executed with bias and roundmode down, since round-nearest rounds 0.5 downward half the time.
  rectFloat = _mm_add_ps(rectFloat, biasesPacked);
  rectPacked = _mm_cvtps_epi32(rectFloat);

  widthHeight = _mm_slli_si128(rectPacked, 8);
  rectPacked = _mm_sub_epi32(rectPacked, widthHeight); // X, Y, Width, Height

  // Avoid negative width/height due to overflow.
  __m128i mask = _mm_or_si128(_mm_cmpgt_epi32(rectPacked, _mm_setzero_si128()),
                              _mm_set_epi32(0, 0, 0xFFFFFFFF, 0xFFFFFFFF));
  // Mask will now contain [ 0xFFFFFFFF 0xFFFFFFFF (width <= 0 ? 0 : 0xFFFFFFFF) (height <= 0 ? 0 : 0xFFFFFFFF) ]
  rectPacked = _mm_and_si128(rectPacked, mask);

  _mm_storeu_si128((__m128i*)&rect, rectPacked);

  _MM_SET_ROUNDING_MODE(_MM_ROUND_NEAREST);
#else
  rect.SetNonEmptyBox(NSToIntRoundUp(NSAppUnitsToDoublePixels(x,
                                     aAppUnitsPerPixel) * aXScale),
                      NSToIntRoundUp(NSAppUnitsToDoublePixels(y,
                                     aAppUnitsPerPixel) * aYScale),
                      NSToIntRoundUp(NSAppUnitsToDoublePixels(XMost(),
                                     aAppUnitsPerPixel) * aXScale),
                      NSToIntRoundUp(NSAppUnitsToDoublePixels(YMost(),
                                     aAppUnitsPerPixel) * aYScale));
#endif
  return rect;
}

// scale the rect but round to smallest containing rect
inline mozilla::gfx::IntRect
nsRect::ScaleToOutsidePixels(float aXScale, float aYScale,
                             nscoord aAppUnitsPerPixel) const
{
  mozilla::gfx::IntRect rect;
  // ASAN builds appear not to respect changes to the SSE rounding mode.
  // Android x86 builds have bindgen issues.
#if !defined(ANDROID) && !defined(MOZ_ASAN) && (defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
  __m128 appUnitsPacked = _mm_set_ps(aAppUnitsPerPixel, aAppUnitsPerPixel, aAppUnitsPerPixel, aAppUnitsPerPixel);
  __m128 scalesPacked = _mm_set_ps(aYScale, aXScale, aYScale, aXScale);

  // This is a bit of a rough approximation, i.e. any value under x + (1 - FLT_EPSILON) will
  // be rounded to x instead of x + 1, however this inaccuracy is much smaller than
  // 1/256th (which would indicate any significant coverage of the pixel) and reduces
  // the amount of cycles considerably.
  __m128 biasesPacked = _mm_set_ps(1 - FLT_EPSILON, 1 - FLT_EPSILON, 0, 0);

  // See Floor section.
  _MM_SET_ROUNDING_MODE(_MM_ROUND_DOWN);

  __m128i rectPacked = _mm_loadu_si128((__m128i*)this); // x, y, w, h
  __m128i widthHeight = _mm_slli_si128(rectPacked, 8); // 0, 0, x, y

  rectPacked = _mm_add_epi32(rectPacked, widthHeight); // X, Y, XMost(), YMost()

  __m128 rectFloat = _mm_cvtepi32_ps(rectPacked);

  // Scale i.e. ([ x y xmost ymost ] / aAppUnitsPerPixel) * [ aXScale aYScale aXScale aYScale ]
  rectFloat = _mm_mul_ps(_mm_div_ps(rectFloat, appUnitsPacked), scalesPacked);

  // Floor
  // Executed with bias and roundmode down, since round-nearest rounds 0.5 downward half the time.
  rectFloat = _mm_add_ps(rectFloat, biasesPacked);
  rectPacked = _mm_cvtps_epi32(rectFloat); // r.x, r.y, r.XMost(), r.YMost()

  widthHeight = _mm_slli_si128(rectPacked, 8); // 0, 0, r.x, r.y
  rectPacked = _mm_sub_epi32(rectPacked, widthHeight); // r.x, r.y, r.w, r.h

  // Avoid negative width/height due to overflow.
  __m128i mask = _mm_or_si128(_mm_cmpgt_epi32(rectPacked, _mm_setzero_si128()),
                              _mm_set_epi32(0, 0, 0xFFFFFFFF, 0xFFFFFFFF));
  // Mask will now contain [ 0xFFFFFFFF 0xFFFFFFFF (width <= 0 ? 0 : 0xFFFFFFFF) (height <= 0 ? 0 : 0xFFFFFFFF) ]
  rectPacked = _mm_and_si128(rectPacked, mask);

  _mm_storeu_si128((__m128i*)&rect, rectPacked);

  _MM_SET_ROUNDING_MODE(_MM_ROUND_NEAREST);
#else
  rect.SetNonEmptyBox(NSToIntFloor(NSAppUnitsToFloatPixels(x,
                                   float(aAppUnitsPerPixel)) * aXScale),
                      NSToIntFloor(NSAppUnitsToFloatPixels(y,
                                   float(aAppUnitsPerPixel)) * aYScale),
                      NSToIntCeil(NSAppUnitsToFloatPixels(XMost(),
                                   float(aAppUnitsPerPixel)) * aXScale),
                      NSToIntCeil(NSAppUnitsToFloatPixels(YMost(),
                                   float(aAppUnitsPerPixel)) * aYScale));
#endif
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
