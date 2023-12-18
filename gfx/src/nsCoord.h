/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSCOORD_H
#define NSCOORD_H

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <math.h>

#include "mozilla/Assertions.h"
#include "mozilla/gfx/Coord.h"
#include "nsMathUtils.h"

/*
 * Basic type used for the geometry classes.
 *
 * Normally all coordinates are maintained in an app unit coordinate
 * space. An app unit is 1/60th of a CSS device pixel, which is, in turn
 * an integer number of device pixels, such at the CSS DPI is as close to
 * 96dpi as possible.
 */

using nscoord = int32_t;
inline constexpr nscoord nscoord_MAX = (1 << 30) - 1;
inline constexpr nscoord nscoord_MIN = -nscoord_MAX;

namespace mozilla {
struct AppUnit {};

// Declare AppUnit as a coordinate system tag.
template <>
struct IsPixel<AppUnit> : std::true_type {};

namespace detail {
template <typename Rep>
struct AuCoordImpl : public gfx::IntCoordTyped<AppUnit, Rep> {
  using Super = gfx::IntCoordTyped<AppUnit, Rep>;

  constexpr AuCoordImpl() : Super() {}
  constexpr MOZ_IMPLICIT AuCoordImpl(Rep aValue) : Super(aValue) {}
  constexpr MOZ_IMPLICIT AuCoordImpl(Super aValue) : Super(aValue) {}

  template <typename F>
  static AuCoordImpl FromRound(F aValue) {
    // Note: aValue is *not* rounding to nearest integer if it is negative. See
    // https://bugzilla.mozilla.org/show_bug.cgi?id=410748#c14
    return AuCoordImpl(std::floor(aValue + 0.5f));
  }

  template <typename F>
  static AuCoordImpl FromTruncate(F aValue) {
    return AuCoordImpl(std::trunc(aValue));
  }

  template <typename F>
  static AuCoordImpl FromCeil(F aValue) {
    return AuCoordImpl(std::ceil(aValue));
  }

  template <typename F>
  static AuCoordImpl FromFloor(F aValue) {
    return AuCoordImpl(std::floor(aValue));
  }

  // Note: this returns the result of the operation, without modifying the
  // original value.
  [[nodiscard]] AuCoordImpl ToMinMaxClamped() const {
    return std::clamp(this->value, kMin, kMax);
  }

  static constexpr Rep kMax = nscoord_MAX;
  static constexpr Rep kMin = nscoord_MIN;
};
}  // namespace detail

using AuCoord = detail::AuCoordImpl<int32_t>;
using AuCoord64 = detail::AuCoordImpl<int64_t>;

}  // namespace mozilla

/**
 * Divide aSpace by aN.  Assign the resulting quotient to aQuotient and
 * return the remainder.
 */
inline nscoord NSCoordDivRem(nscoord aSpace, size_t aN, nscoord* aQuotient) {
  div_t result = div(aSpace, aN);
  *aQuotient = nscoord(result.quot);
  return nscoord(result.rem);
}

inline nscoord NSCoordMulDiv(nscoord aMult1, nscoord aMult2, nscoord aDiv) {
  return (int64_t(aMult1) * int64_t(aMult2) / int64_t(aDiv));
}

inline nscoord NSToCoordRound(float aValue) {
#if defined(XP_WIN) && defined(_M_IX86) && !defined(__GNUC__) && \
    !defined(__clang__)
  return NS_lroundup30(aValue);
#else
  return nscoord(floorf(aValue + 0.5f));
#endif /* XP_WIN && _M_IX86 && !__GNUC__ */
}

inline nscoord NSToCoordRound(double aValue) {
#if defined(XP_WIN) && defined(_M_IX86) && !defined(__GNUC__) && \
    !defined(__clang__)
  return NS_lroundup30((float)aValue);
#else
  return nscoord(floor(aValue + 0.5f));
#endif /* XP_WIN && _M_IX86 && !__GNUC__ */
}

inline nscoord NSToCoordRoundWithClamp(float aValue) {
  // Bounds-check before converting out of float, to avoid overflow
  if (aValue >= float(nscoord_MAX)) {
    return nscoord_MAX;
  }
  if (aValue <= float(nscoord_MIN)) {
    return nscoord_MIN;
  }
  return NSToCoordRound(aValue);
}

inline nscoord NSToCoordRoundWithClamp(double aValue) {
  // Bounds-check before converting out of double, to avoid overflow
  if (aValue >= double(nscoord_MAX)) {
    return nscoord_MAX;
  }
  if (aValue <= double(nscoord_MIN)) {
    return nscoord_MIN;
  }
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
  if (requireNotNegative) {
    MOZ_ASSERT(aScale >= 0.0f,
               "negative scaling factors must be handled manually");
  }
  float product = aCoord * aScale;
  if (requireNotNegative ? aCoord > 0 : (aCoord > 0) == (aScale > 0))
    return NSToCoordRoundWithClamp(
        std::min<float>((float)nscoord_MAX, product));
  return NSToCoordRoundWithClamp(std::max<float>((float)nscoord_MIN, product));
}

/**
 * Returns aCoord * aScale, capping the product to nscoord_MAX or nscoord_MIN as
 * appropriate for the sign of aCoord.  This method requires aScale to not be
 * negative; use this method when you know that aScale should never be
 * negative to get a sanity check of that invariant in debug builds.
 */
inline nscoord NSCoordSaturatingNonnegativeMultiply(nscoord aCoord,
                                                    float aScale) {
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
 */
inline nscoord NSCoordSaturatingAdd(nscoord a, nscoord b) {
  if (a == nscoord_MAX || b == nscoord_MAX) {
    // infinity + anything = anything + infinity = infinity
    return nscoord_MAX;
  } else {
    // a + b = a + b
    // Cap the result, just in case we're dealing with numbers near nscoord_MAX
    return std::min(nscoord_MAX, a + b);
  }
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
 */
inline nscoord NSCoordSaturatingSubtract(nscoord a, nscoord b,
                                         nscoord infMinusInfResult) {
  if (b == nscoord_MAX) {
    if (a == nscoord_MAX) {
      // case (a)
      return infMinusInfResult;
    } else {
      // case (b)
      return 0;
    }
  } else {
    if (a == nscoord_MAX) {
      // case (c) for integers
      return nscoord_MAX;
    } else {
      // case (d) for integers
      // Cap the result, in case we're dealing with numbers near nscoord_MAX
      return std::min(nscoord_MAX, a - b);
    }
  }
}

inline float NSCoordToFloat(nscoord aCoord) { return (float)aCoord; }

/*
 * Coord Rounding Functions
 */
inline nscoord NSToCoordFloor(float aValue) { return nscoord(floorf(aValue)); }

inline nscoord NSToCoordFloor(double aValue) { return nscoord(floor(aValue)); }

inline nscoord NSToCoordFloorClamped(float aValue) {
  // Bounds-check before converting out of float, to avoid overflow
  if (aValue >= float(nscoord_MAX)) {
    return nscoord_MAX;
  }
  if (aValue <= float(nscoord_MIN)) {
    return nscoord_MIN;
  }
  return NSToCoordFloor(aValue);
}

inline nscoord NSToCoordCeil(float aValue) { return nscoord(ceilf(aValue)); }

inline nscoord NSToCoordCeil(double aValue) { return nscoord(ceil(aValue)); }

inline nscoord NSToCoordCeilClamped(double aValue) {
  // Bounds-check before converting out of double, to avoid overflow
  if (aValue >= nscoord_MAX) {
    return nscoord_MAX;
  }
  if (aValue <= nscoord_MIN) {
    return nscoord_MIN;
  }
  return NSToCoordCeil(aValue);
}

// The NSToCoordTrunc* functions remove the fractional component of
// aValue, and are thus equivalent to NSToCoordFloor* for positive
// values and NSToCoordCeil* for negative values.

inline nscoord NSToCoordTrunc(float aValue) {
  // There's no need to use truncf() since it matches the default
  // rules for float to integer conversion.
  return nscoord(aValue);
}

inline nscoord NSToCoordTrunc(double aValue) {
  // There's no need to use trunc() since it matches the default
  // rules for float to integer conversion.
  return nscoord(aValue);
}

inline nscoord NSToCoordTruncClamped(float aValue) {
  // Bounds-check before converting out of float, to avoid overflow
  if (aValue >= float(nscoord_MAX)) {
    return nscoord_MAX;
  }
  if (aValue <= float(nscoord_MIN)) {
    return nscoord_MIN;
  }
  return NSToCoordTrunc(aValue);
}

inline nscoord NSToCoordTruncClamped(double aValue) {
  // Bounds-check before converting out of double, to avoid overflow
  if (aValue >= float(nscoord_MAX)) {
    return nscoord_MAX;
  }
  if (aValue <= float(nscoord_MIN)) {
    return nscoord_MIN;
  }
  return NSToCoordTrunc(aValue);
}

/*
 * Int Rounding Functions
 */
inline int32_t NSToIntFloor(float aValue) { return int32_t(floorf(aValue)); }

inline int32_t NSToIntCeil(float aValue) { return int32_t(ceilf(aValue)); }

inline int32_t NSToIntRound(float aValue) { return NS_lroundf(aValue); }

inline int32_t NSToIntRound(double aValue) { return NS_lround(aValue); }

inline int32_t NSToIntRoundUp(double aValue) {
  return int32_t(floor(aValue + 0.5));
}

/*
 * App Unit/Pixel conversions
 */
inline nscoord NSFloatPixelsToAppUnits(float aPixels, float aAppUnitsPerPixel) {
  return NSToCoordRoundWithClamp(aPixels * aAppUnitsPerPixel);
}

inline nscoord NSIntPixelsToAppUnits(int32_t aPixels,
                                     int32_t aAppUnitsPerPixel) {
  // The cast to nscoord makes sure we don't overflow if we ever change
  // nscoord to float
  nscoord r = aPixels * (nscoord)aAppUnitsPerPixel;
  return r;
}

inline float NSAppUnitsToFloatPixels(nscoord aAppUnits,
                                     float aAppUnitsPerPixel) {
  return (float(aAppUnits) / aAppUnitsPerPixel);
}

inline double NSAppUnitsToDoublePixels(nscoord aAppUnits,
                                       double aAppUnitsPerPixel) {
  return (double(aAppUnits) / aAppUnitsPerPixel);
}

inline int32_t NSAppUnitsToIntPixels(nscoord aAppUnits,
                                     float aAppUnitsPerPixel) {
  return NSToIntRound(float(aAppUnits) / aAppUnitsPerPixel);
}

inline float NSCoordScale(nscoord aCoord, int32_t aFromAPP, int32_t aToAPP) {
  return (NSCoordToFloat(aCoord) * aToAPP) / aFromAPP;
}

/// handy constants
#define TWIPS_PER_POINT_INT 20
#define TWIPS_PER_POINT_FLOAT 20.0f
#define POINTS_PER_INCH_INT 72
#define POINTS_PER_INCH_FLOAT 72.0f
#define CM_PER_INCH_FLOAT 2.54f
#define MM_PER_INCH_FLOAT 25.4f

/*
 * Twips/unit conversions
 */
inline float NSUnitsToTwips(float aValue, float aPointsPerUnit) {
  return aValue * aPointsPerUnit * TWIPS_PER_POINT_FLOAT;
}

inline float NSTwipsToUnits(float aTwips, float aUnitsPerPoint) {
  return (aTwips * (aUnitsPerPoint / TWIPS_PER_POINT_FLOAT));
}

/// Unit conversion macros
//@{
#define NS_POINTS_TO_TWIPS(x) NSUnitsToTwips((x), 1.0f)
#define NS_INCHES_TO_TWIPS(x) \
  NSUnitsToTwips((x), POINTS_PER_INCH_FLOAT)  // 72 points per inch

#define NS_MILLIMETERS_TO_TWIPS(x) \
  NSUnitsToTwips((x), (POINTS_PER_INCH_FLOAT * 0.03937f))

#define NS_POINTS_TO_INT_TWIPS(x) NSToIntRound(NS_POINTS_TO_TWIPS(x))
#define NS_INCHES_TO_INT_TWIPS(x) NSToIntRound(NS_INCHES_TO_TWIPS(x))

#define NS_TWIPS_TO_INCHES(x) NSTwipsToUnits((x), 1.0f / POINTS_PER_INCH_FLOAT)

#define NS_TWIPS_TO_MILLIMETERS(x) \
  NSTwipsToUnits((x), 1.0f / (POINTS_PER_INCH_FLOAT * 0.03937f))
//@}

#endif /* NSCOORD_H */
