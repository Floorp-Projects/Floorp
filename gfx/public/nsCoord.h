/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef NSCOORD_H
#define NSCOORD_H

#include "nscore.h"
#include "nsMathUtils.h"
#include <math.h>
#include <float.h>

#include "nsDebug.h"

/*
 * Basic type used for the geometry classes.
 *
 * Normally all coordinates are maintained in an app unit coordinate
 * space. An app unit is 1/60th of a CSS device pixel, which is, in turn
 * an integer number of device pixels, such at the CSS DPI is as close to
 * 96dpi as possible.
 */

// This controls whether we're using integers or floats for coordinates. We
// want to eventually use floats. If you change this, you need to manually
// change the definition of nscoord in gfx/idl/gfxidltypes.idl.
//#define NS_COORD_IS_FLOAT

inline float NS_IEEEPositiveInfinity() {
  union { PRUint32 mPRUint32; float mFloat; } pun;
  pun.mPRUint32 = 0x7F800000;
  return pun.mFloat;
}
inline PRBool NS_IEEEIsNan(float aF) {
  union { PRUint32 mBits; float mFloat; } pun;
  pun.mFloat = aF;
  return (pun.mBits & 0x7F800000) == 0x7F800000 &&
    (pun.mBits & 0x007FFFFF) != 0;
}

#ifdef NS_COORD_IS_FLOAT
typedef float nscoord;
#define nscoord_MAX NS_IEEEPositiveInfinity()
#else
typedef PRInt32 nscoord;
#define nscoord_MAX nscoord(1 << 30)
#endif

#define nscoord_MIN (-nscoord_MAX)

inline void VERIFY_COORD(nscoord aCoord) {
#ifdef NS_COORD_IS_FLOAT
  NS_ASSERTION(floorf(aCoord) == aCoord,
               "Coords cannot have fractions");
#endif
}

/**
 * Returns aCoord * aVal, capping the product to nscoord_MAX.
 *
 * Note: If/when we start using floats for nscoords, this function won't be
 * necessary.  Normal float multiplication correctly handles overflowing
 * multiplications, automatically saturating to infinity.
 */
inline nscoord NSCoordSaturatingMultiply(nscoord aCoord, float aVal) {
  VERIFY_COORD(aCoord);
  NS_ASSERTION(aVal >= 0.0f,
               "negative scaling factors must be handled manually");
#ifdef NS_COORD_IS_FLOAT
  return floorf(aCoord * aVal);
#else
  // This one's only a warning because it's possible to trigger
  NS_WARN_IF_FALSE(aCoord > 0
                   ? floorf(aCoord * aVal) < nscoord_MAX
                   : ceilf(aCoord * aVal) > nscoord_MIN,
                   "nscoord multiplication capped");

  if (aCoord > 0)
    return PRInt32(PR_MIN(nscoord_MAX, aCoord * aVal));
  return PRInt32(PR_MAX(nscoord_MIN, aCoord * aVal));
#endif
}

inline nscoord NSCoordMultiply(nscoord aCoord, PRInt32 aVal) {
  VERIFY_COORD(aCoord);
  return aCoord*aVal;
}

inline nscoord NSCoordDivide(nscoord aCoord, float aVal) {
  VERIFY_COORD(aCoord);
#ifdef NS_COORD_IS_FLOAT
  return floorf(aCoord/aVal);
#else
  return (PRInt32)(aCoord/aVal);
#endif
}

inline nscoord NSCoordDivide(nscoord aCoord, PRInt32 aVal) {
  VERIFY_COORD(aCoord);
#ifdef NS_COORD_IS_FLOAT
  return floorf(aCoord/aVal);
#else
  return aCoord/aVal;
#endif
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
  NS_ASSERTION(a != nscoord_MIN && b != nscoord_MIN,
               "NSCoordSaturatingAdd got nscoord_MIN as argument");

#ifdef NS_COORD_IS_FLOAT
  // Float math correctly handles a+b, given that neither is -infinity.
  return a + b;
#else
  if (a == nscoord_MAX || b == nscoord_MAX) {
    // infinity + anything = anything + infinity = infinity
    return nscoord_MAX;
  } else {
    // a + b = a + b
    NS_ASSERTION(a < nscoord_MAX && b < nscoord_MAX,
                 "Doing nscoord addition with values > nscoord_MAX");
    NS_ASSERTION((PRInt64)a + (PRInt64)b > (PRInt64)nscoord_MIN,
                 "nscoord addition will reach or pass nscoord_MIN");
    // This one's only a warning because the PR_MIN below means that
    // we'll handle this case correctly.
    NS_WARN_IF_FALSE((PRInt64)a + (PRInt64)b < (PRInt64)nscoord_MAX,
                     "nscoord addition capped to nscoord_MAX");

    // Cap the result, just in case we're dealing with numbers near nscoord_MAX
    return PR_MIN(nscoord_MAX, a + b);
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
  NS_ASSERTION(a != nscoord_MIN && b != nscoord_MIN,
               "NSCoordSaturatingSubtract got nscoord_MIN as argument");

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
      NS_ASSERTION(a < nscoord_MAX && b < nscoord_MAX,
                   "Doing nscoord subtraction with values > nscoord_MAX");
      NS_ASSERTION((PRInt64)a - (PRInt64)b > (PRInt64)nscoord_MIN,
                   "nscoord subtraction will reach or pass nscoord_MIN");
      // This one's only a warning because the PR_MIN below means that
      // we'll handle this case correctly.
      NS_WARN_IF_FALSE((PRInt64)a - (PRInt64)b < (PRInt64)nscoord_MAX,
                       "nscoord subtraction capped to nscoord_MAX");

      // Cap the result, in case we're dealing with numbers near nscoord_MAX
      return PR_MIN(nscoord_MAX, a - b);
    }
  }
#endif
}
/** compare against a nscoord "b", which might be unconstrained
  * "a" must not be unconstrained.
  * Every number is smaller than a unconstrained one
  */
inline PRBool
NSCoordLessThan(nscoord a,nscoord b)
{
  NS_ASSERTION(a != nscoord_MAX, 
               "This coordinate should be constrained");
  return ((a < b) || (b == nscoord_MAX));
}

/** compare against a nscoord "b", which might be unconstrained
  * "a" must not be unconstrained
  * No number is larger than a unconstrained one.
  */
inline PRBool
NSCoordGreaterThan(nscoord a,nscoord b)
{
  NS_ASSERTION(a != nscoord_MAX, 
               "This coordinate should be constrained");
  return ((a > b) && (b != nscoord_MAX));
}

/**
 * Convert an nscoord to a PRInt32. This *does not* do rounding because
 * coords are never fractional. They can be out of range, so this does
 * clamp out of bounds coord values to PR_INT32_MIN and PR_INT32_MAX.
 */
inline PRInt32 NSCoordToInt(nscoord aCoord) {
  VERIFY_COORD(aCoord);
#ifdef NS_COORD_IS_FLOAT
  NS_ASSERTION(!NS_IEEEIsNan(aCoord), "NaN encountered in int conversion");
  if (aCoord < -2147483648.0f) {
    // -2147483648 is the smallest 32-bit signed integer that can be
    // exactly represented as a float
    return PR_INT32_MIN;
  } else if (aCoord > 2147483520.0f) {
    // 2147483520 is the largest 32-bit signed integer that can be
    // exactly represented as an IEEE float
    return PR_INT32_MAX;
  } else {
    return (PRInt32)aCoord;
  }
#else
  return aCoord;
#endif
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
  return nscoord(NS_floorf(aValue));
}

inline nscoord NSToCoordFloorClamped(float aValue)
{
#ifndef NS_COORD_IS_FLOAT
  // Bounds-check before converting out of float, to avoid overflow
  if (aValue >= nscoord_MAX) {
    NS_WARNING("Overflowed nscoord_MAX in conversion to nscoord");
    return nscoord_MAX;
  }
  if (aValue <= nscoord_MIN) {
    NS_WARNING("Overflowed nscoord_MIN in conversion to nscoord");
    return nscoord_MIN;
  }
#endif
  return NSToCoordFloor(aValue);
}

inline nscoord NSToCoordCeil(float aValue)
{
  return nscoord(NS_ceilf(aValue));
}

inline nscoord NSToCoordCeilClamped(float aValue)
{
#ifndef NS_COORD_IS_FLOAT
  // Bounds-check before converting out of float, to avoid overflow
  if (aValue >= nscoord_MAX) {
    NS_WARNING("Overflowed nscoord_MAX in conversion to nscoord");
    return nscoord_MAX;
  }
  if (aValue <= nscoord_MIN) {
    NS_WARNING("Overflowed nscoord_MIN in conversion to nscoord");
    return nscoord_MIN;
  }
#endif
  return NSToCoordCeil(aValue);
}

inline nscoord NSToCoordRound(float aValue)
{
#if defined(XP_WIN32) && defined(_M_IX86) && !defined(__GNUC__)
  return NS_lroundup30(aValue);
#else
  return nscoord(NS_floorf(aValue + 0.5f));
#endif /* XP_WIN32 && _M_IX86 && !__GNUC__ */
}

inline nscoord NSToCoordRoundWithClamp(float aValue)
{
#ifndef NS_COORD_IS_FLOAT
  // Bounds-check before converting out of float, to avoid overflow
  if (aValue >= nscoord_MAX) {
    NS_WARNING("Overflowed nscoord_MAX in conversion to nscoord");
    return nscoord_MAX;
  }
  if (aValue <= nscoord_MIN) {
    NS_WARNING("Overflowed nscoord_MIN in conversion to nscoord");
    return nscoord_MIN;
  }
#endif
  return NSToCoordRound(aValue);
}

/*
 * Int Rounding Functions
 */
inline PRInt32 NSToIntFloor(float aValue)
{
  return PRInt32(NS_floorf(aValue));
}

inline PRInt32 NSToIntCeil(float aValue)
{
  return PRInt32(NS_ceilf(aValue));
}

inline PRInt32 NSToIntRound(float aValue)
{
  return NS_lroundf(aValue);
}

inline PRInt32 NSToIntRoundUp(float aValue)
{
  return PRInt32(NS_floorf(aValue + 0.5f));
}

/* 
 * App Unit/Pixel conversions
 */
inline nscoord NSFloatPixelsToAppUnits(float aPixels, float aAppUnitsPerPixel)
{
  return NSToCoordRoundWithClamp(aPixels * aAppUnitsPerPixel);
}

inline nscoord NSIntPixelsToAppUnits(PRInt32 aPixels, PRInt32 aAppUnitsPerPixel)
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

inline PRInt32 NSAppUnitsToIntPixels(nscoord aAppUnits, float aAppUnitsPerPixel)
{
  return NSToIntRound(float(aAppUnits) / aAppUnitsPerPixel);
}

/// handy constants
#define TWIPS_PER_POINT_INT           20
#define TWIPS_PER_POINT_FLOAT         20.0f
#define POINTS_PER_INCH_INT           72
#define POINTS_PER_INCH_FLOAT         72.0f

/* 
 * Twips/unit conversions
 */
inline nscoord NSUnitsToTwips(float aValue, float aPointsPerUnit)
{
  return NSToCoordRoundWithClamp(aValue * aPointsPerUnit * TWIPS_PER_POINT_FLOAT);
}

inline float NSTwipsToUnits(nscoord aTwips, float aUnitsPerPoint)
{
  return (aTwips * (aUnitsPerPoint / TWIPS_PER_POINT_FLOAT));
}


/// Unit conversion macros
//@{
#define NS_POINTS_TO_TWIPS(x)         NSUnitsToTwips((x), 1.0f)
#define NS_INCHES_TO_TWIPS(x)         NSUnitsToTwips((x), POINTS_PER_INCH_FLOAT)                      // 72 points per inch

#define NS_MILLIMETERS_TO_TWIPS(x)    NSUnitsToTwips((x), (POINTS_PER_INCH_FLOAT * 0.03937f))
#define NS_CENTIMETERS_TO_TWIPS(x)    NSUnitsToTwips((x), (POINTS_PER_INCH_FLOAT * 0.3937f))

#define NS_PICAS_TO_TWIPS(x)          NSUnitsToTwips((x), 12.0f)                      // 12 points per pica

#define NS_TWIPS_TO_POINTS(x)         NSTwipsToUnits((x), 1.0f)
#define NS_TWIPS_TO_INCHES(x)         NSTwipsToUnits((x), 1.0f / POINTS_PER_INCH_FLOAT)

#define NS_TWIPS_TO_MILLIMETERS(x)    NSTwipsToUnits((x), 1.0f / (POINTS_PER_INCH_FLOAT * 0.03937f))
#define NS_TWIPS_TO_CENTIMETERS(x)    NSTwipsToUnits((x), 1.0f / (POINTS_PER_INCH_FLOAT * 0.3937f))

#define NS_TWIPS_TO_PICAS(x)          NSTwipsToUnits((x), 1.0f / 12.0f)
//@}

#endif /* NSCOORD_H */
