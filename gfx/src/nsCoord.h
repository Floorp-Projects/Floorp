/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSCOORD_H
#define NSCOORD_H

#include "nsAlgorithm.h"
#include "nscore.h"
#include "nsMathUtils.h"
#include <math.h>
#include <float.h>
#include <stdlib.h>

#include "nsDebug.h"
#include <algorithm>

/*
 * Basic type used for the geometry classes.
 *
 * Normally all coordinates are maintained in an app unit coordinate
 * space. An app unit is 1/60th of a CSS device pixel, which is, in turn
 * an integer number of device pixels, such at the CSS DPI is as close to
 * 96dpi as possible.
 */

// This controls whether we're using integers or floats for coordinates. We
// want to eventually use floats.
//#define NS_COORD_IS_FLOAT

inline float NS_IEEEPositiveInfinity() {
  union { uint32_t mPRUint32; float mFloat; } pun;
  pun.mPRUint32 = 0x7F800000;
  return pun.mFloat;
}
inline bool NS_IEEEIsNan(float aF) {
  union { uint32_t mBits; float mFloat; } pun;
  pun.mFloat = aF;
  return (pun.mBits & 0x7F800000) == 0x7F800000 &&
    (pun.mBits & 0x007FFFFF) != 0;
}

#ifdef NS_COORD_IS_FLOAT
typedef float nscoord;
#define nscoord_MAX NS_IEEEPositiveInfinity()
#else
typedef int32_t nscoord;
#define nscoord_MAX nscoord((1 << 30) - 1)
#endif

#define nscoord_MIN (-nscoord_MAX)

inline void VERIFY_COORD(nscoord aCoord) {
#ifdef NS_COORD_IS_FLOAT
  NS_ASSERTION(floorf(aCoord) == aCoord,
               "Coords cannot have fractions");
#endif
}

/**
 * Divide aSpace by aN.  Assign the resulting quotient to aQuotient and
 * return the remainder.
 */
inline nscoord NSCoordDivRem(nscoord aSpace, size_t aN, nscoord* aQuotient)
{
#ifdef NS_COORD_IS_FLOAT
  *aQuotient = aSpace / aN;
  return 0.0f;
#else
  div_t result = div(aSpace, aN);
  *aQuotient = nscoord(result.quot);
  return nscoord(result.rem);
#endif
}

inline nscoord NSCoordMulDiv(nscoord aMult1, nscoord aMult2, nscoord aDiv) {
#ifdef NS_COORD_IS_FLOAT
  return (aMult1 * aMult2 / aDiv);
#else
  return (int64_t(aMult1) * int64_t(aMult2) / int64_t(aDiv));
#endif
}

inline nscoord NSToCoordRound(float aValue)
{
#if defined(XP_WIN32) && defined(_M_IX86) && !defined(__GNUC__) && !defined(__clang__)
  return NS_lroundup30(aValue);
#else
  return nscoord(floorf(aValue + 0.5f));
#endif /* XP_WIN32 && _M_IX86 && !__GNUC__ */
}

inline nscoord NSToCoordRound(double aValue)
{
#if defined(XP_WIN32) && defined(_M_IX86) && !defined(__GNUC__) && !defined(__clang__)
  return NS_lroundup30((float)aValue);
#else
  return nscoord(floor(aValue + 0.5f));
#endif /* XP_WIN32 && _M_IX86 && !__GNUC__ */
}

inline nscoord NSToCoordRoundWithClamp(float aValue)
{
#ifndef NS_COORD_IS_FLOAT
  // Bounds-check before converting out of float, to avoid overflow
  if (aValue >= nscoord_MAX) {
    return nscoord_MAX;
  }
  if (aValue <= nscoord_MIN) {
    return nscoord_MIN;
  }
#endif
  return NSToCoordRound(aValue);
}

/**
 * Returns aCoord * aScale, capping the product to nscoord_MAX or nscoord_MIN as
 * appropriate for the signs of aCoord and aScale.  If requireNotNegative is
 * true, this method will enforce that aScale is not negative; use that
 * parametrization to get a check of that fact in debug builds.
 */
inline nscoord _nscoordSaturatingMultiply(nscoord aCoord, float aScale,
                                          bool requireNotNegative) {
  VERIFY_COORD(aCoord);
  if (requireNotNegative) {
    MOZ_ASSERT(aScale >= 0.0f,
               "negative scaling factors must be handled manually");
  }
#ifdef NS_COORD_IS_FLOAT
  return floorf(aCoord * aScale);
#else
  float product = aCoord * aScale;
  if (requireNotNegative ? aCoord > 0 : (aCoord > 0) == (aScale > 0))
    return NSToCoordRoundWithClamp(std::min<float>(nscoord_MAX, product));
  return NSToCoordRoundWithClamp(std::max<float>(nscoord_MIN, product));
#endif
}

/**
 * Returns aCoord * aScale, capping the product to nscoord_MAX or nscoord_MIN as
 * appropriate for the sign of aCoord.  This method requires aScale to not be
 * negative; use this method when you know that aScale should never be
 * negative to get a sanity check of that invariant in debug builds.
 */
inline nscoord NSCoordSaturatingNonnegativeMultiply(nscoord aCoord, float aScale) {
  return _nscoordSaturatingMultiply(aCoord, aScale, true);
}

/**
 * Returns aCoord * aScale, capping the product to nscoord_MAX or nscoord_MIN as
 * appropriate for the signs of aCoord and aScale.
 */
inline nscoord NSCoordSaturatingMultiply(nscoord aCoord, float aScale) {
  return _nscoordSaturatingMultiply(aCoord, aScale, false);
}

/**
 * Returns a + b, capping the sum to nscoord_MAX.
 *
 * This function assumes that neither argument is nscoord_MIN.
 *
 * Note: If/when we start using floats for nscoords, this function won't be as
 * necessary.  Normal float addition correctly handles adding with infinity,
 * assuming we aren't adding nscoord_MIN. (-infinity)
 */
inline nscoord
NSCoordSaturatingAdd(nscoord a, nscoord b)
{
  VERIFY_COORD(a);
  VERIFY_COORD(b);

#ifdef NS_COORD_IS_FLOAT
  // Float math correctly handles a+b, given that neither is -infinity.
  return a + b;
#else
  if (a == nscoord_MAX || b == nscoord_MAX) {
    // infinity + anything = anything + infinity = infinity
    return nscoord_MAX;
  } else {
    // a + b = a + b
    // Cap the result, just in case we're dealing with numbers near nscoord_MAX
    return std::min(nscoord_MAX, a + b);
  }
#endif
}

/**
 * Returns a - b, gracefully handling cases involving nscoord_MAX.
 * This function assumes that neither argument is nscoord_MIN.
 *
 * The behavior is as follows:
 *
 *  a)  infinity - infinity -> infMinusInfResult
 *  b)  N - infinity        -> 0  (unexpected -- triggers NOTREACHED)
 *  c)  infinity - N        -> infinity
 *  d)  N1 - N2             -> N1 - N2
 *
 * Note: For float nscoords, cases (c) and (d) are handled by normal float
 * math.  We still need to explicitly specify the behavior for cases (a)
 * and (b), though.  (Under normal float math, those cases would return NaN
 * and -infinity, respectively.)
 */
inline nscoord 
NSCoordSaturatingSubtract(nscoord a, nscoord b, 
                          nscoord infMinusInfResult)
{
  VERIFY_COORD(a);
  VERIFY_COORD(b);

  if (b == nscoord_MAX) {
    if (a == nscoord_MAX) {
      // case (a)
      return infMinusInfResult;
    } else {
      // case (b)
      NS_NOTREACHED("Attempted to subtract [n - nscoord_MAX]");
      return 0;
    }
  } else {
#ifdef NS_COORD_IS_FLOAT
    // case (c) and (d) for floats.  (float math handles both)
    return a - b;
#else
    if (a == nscoord_MAX) {
      // case (c) for integers
      return nscoord_MAX;
    } else {
      // case (d) for integers
      // Cap the result, in case we're dealing with numbers near nscoord_MAX
      return std::min(nscoord_MAX, a - b);
    }
#endif
  }
}

inline float NSCoordToFloat(nscoord aCoord) {
  VERIFY_COORD(aCoord);
#ifdef NS_COORD_IS_FLOAT
  NS_ASSERTION(!NS_IEEEIsNan(aCoord), "NaN encountered in float conversion");
#endif
  return (float)aCoord;
}

/*
 * Coord Rounding Functions
 */
inline nscoord NSToCoordFloor(float aValue)
{
  return nscoord(floorf(aValue));
}

inline nscoord NSToCoordFloor(double aValue)
{
  return nscoord(floor(aValue));
}

inline nscoord NSToCoordFloorClamped(float aValue)
{
#ifndef NS_COORD_IS_FLOAT
  // Bounds-check before converting out of float, to avoid overflow
  if (aValue >= nscoord_MAX) {
    return nscoord_MAX;
  }
  if (aValue <= nscoord_MIN) {
    return nscoord_MIN;
  }
#endif
  return NSToCoordFloor(aValue);
}

inline nscoord NSToCoordCeil(float aValue)
{
  return nscoord(ceilf(aValue));
}

inline nscoord NSToCoordCeil(double aValue)
{
  return nscoord(ceil(aValue));
}

inline nscoord NSToCoordCeilClamped(double aValue)
{
#ifndef NS_COORD_IS_FLOAT
  // Bounds-check before converting out of double, to avoid overflow
  if (aValue >= nscoord_MAX) {
    return nscoord_MAX;
  }
  if (aValue <= nscoord_MIN) {
    return nscoord_MIN;
  }
#endif
  return NSToCoordCeil(aValue);
}

// The NSToCoordTrunc* functions remove the fractional component of
// aValue, and are thus equivalent to NSToCoordFloor* for positive
// values and NSToCoordCeil* for negative values.

inline nscoord NSToCoordTrunc(float aValue)
{
  // There's no need to use truncf() since it matches the default
  // rules for float to integer conversion.
  return nscoord(aValue);
}

inline nscoord NSToCoordTrunc(double aValue)
{
  // There's no need to use trunc() since it matches the default
  // rules for float to integer conversion.
  return nscoord(aValue);
}

inline nscoord NSToCoordTruncClamped(float aValue)
{
#ifndef NS_COORD_IS_FLOAT
  // Bounds-check before converting out of float, to avoid overflow
  if (aValue >= nscoord_MAX) {
    return nscoord_MAX;
  }
  if (aValue <= nscoord_MIN) {
    return nscoord_MIN;
  }
#endif
  return NSToCoordTrunc(aValue);
}

inline nscoord NSToCoordTruncClamped(double aValue)
{
#ifndef NS_COORD_IS_FLOAT
  // Bounds-check before converting out of double, to avoid overflow
  if (aValue >= nscoord_MAX) {
    return nscoord_MAX;
  }
  if (aValue <= nscoord_MIN) {
    return nscoord_MIN;
  }
#endif
  return NSToCoordTrunc(aValue);
}

/*
 * Int Rounding Functions
 */
inline int32_t NSToIntFloor(float aValue)
{
  return int32_t(floorf(aValue));
}

inline int32_t NSToIntCeil(float aValue)
{
  return int32_t(ceilf(aValue));
}

inline int32_t NSToIntRound(float aValue)
{
  return NS_lroundf(aValue);
}

inline int32_t NSToIntRound(double aValue)
{
  return NS_lround(aValue);
}

inline int32_t NSToIntRoundUp(double aValue)
{
  return int32_t(floor(aValue + 0.5));
}

/* 
 * App Unit/Pixel conversions
 */
inline nscoord NSFloatPixelsToAppUnits(float aPixels, float aAppUnitsPerPixel)
{
  return NSToCoordRoundWithClamp(aPixels * aAppUnitsPerPixel);
}

inline nscoord NSIntPixelsToAppUnits(int32_t aPixels, int32_t aAppUnitsPerPixel)
{
  // The cast to nscoord makes sure we don't overflow if we ever change
  // nscoord to float
  nscoord r = aPixels * (nscoord)aAppUnitsPerPixel;
  VERIFY_COORD(r);
  return r;
}

inline float NSAppUnitsToFloatPixels(nscoord aAppUnits, float aAppUnitsPerPixel)
{
  return (float(aAppUnits) / aAppUnitsPerPixel);
}

inline double NSAppUnitsToDoublePixels(nscoord aAppUnits, double aAppUnitsPerPixel)
{
  return (double(aAppUnits) / aAppUnitsPerPixel);
}

inline int32_t NSAppUnitsToIntPixels(nscoord aAppUnits, float aAppUnitsPerPixel)
{
  return NSToIntRound(float(aAppUnits) / aAppUnitsPerPixel);
}

inline float NSCoordScale(nscoord aCoord, int32_t aFromAPP, int32_t aToAPP)
{
  return (NSCoordToFloat(aCoord) * aToAPP) / aFromAPP;
}

/// handy constants
#define TWIPS_PER_POINT_INT           20
#define TWIPS_PER_POINT_FLOAT         20.0f
#define POINTS_PER_INCH_INT           72
#define POINTS_PER_INCH_FLOAT         72.0f
#define CM_PER_INCH_FLOAT             2.54f
#define MM_PER_INCH_FLOAT             25.4f

/* 
 * Twips/unit conversions
 */
inline float NSUnitsToTwips(float aValue, float aPointsPerUnit)
{
  return aValue * aPointsPerUnit * TWIPS_PER_POINT_FLOAT;
}

inline float NSTwipsToUnits(float aTwips, float aUnitsPerPoint)
{
  return (aTwips * (aUnitsPerPoint / TWIPS_PER_POINT_FLOAT));
}

/// Unit conversion macros
//@{
#define NS_POINTS_TO_TWIPS(x)         NSUnitsToTwips((x), 1.0f)
#define NS_INCHES_TO_TWIPS(x)         NSUnitsToTwips((x), POINTS_PER_INCH_FLOAT)                      // 72 points per inch

#define NS_MILLIMETERS_TO_TWIPS(x)    NSUnitsToTwips((x), (POINTS_PER_INCH_FLOAT * 0.03937f))

#define NS_POINTS_TO_INT_TWIPS(x)     NSToIntRound(NS_POINTS_TO_TWIPS(x))
#define NS_INCHES_TO_INT_TWIPS(x)     NSToIntRound(NS_INCHES_TO_TWIPS(x))

#define NS_TWIPS_TO_INCHES(x)         NSTwipsToUnits((x), 1.0f / POINTS_PER_INCH_FLOAT)

#define NS_TWIPS_TO_MILLIMETERS(x)    NSTwipsToUnits((x), 1.0f / (POINTS_PER_INCH_FLOAT * 0.03937f))
//@}

#endif /* NSCOORD_H */
