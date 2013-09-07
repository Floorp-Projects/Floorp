/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRect.h"
#include "mozilla/gfx/Types.h"          // for NS_SIDE_BOTTOM, etc
#include "nsDeviceContext.h"            // for nsDeviceContext
#include "nsStringGlue.h"               // for nsAutoString, etc
#include "prtypes.h"                    // for PR_STATIC_ASSERT
#include "nsDebug.h"                    // for NS_WARNING
#include "nsMargin.h"                   // for nsMargin
#include <algorithm>                    // for min, max

// the mozilla::css::Side sequence must match the nsMargin nscoord sequence
PR_STATIC_ASSERT((NS_SIDE_TOP == 0) && (NS_SIDE_RIGHT == 1) && (NS_SIDE_BOTTOM == 2) && (NS_SIDE_LEFT == 3));

#ifdef DEBUG
// Diagnostics

FILE* operator<<(FILE* out, const nsRect& rect)
{
  nsAutoString tmp;

  // Output the coordinates in fractional pixels so they're easier to read
  tmp.AppendLiteral("{");
  tmp.AppendFloat(NSAppUnitsToFloatPixels(rect.x,
                       nsDeviceContext::AppUnitsPerCSSPixel()));
  tmp.AppendLiteral(", ");
  tmp.AppendFloat(NSAppUnitsToFloatPixels(rect.y,
                       nsDeviceContext::AppUnitsPerCSSPixel()));
  tmp.AppendLiteral(", ");
  tmp.AppendFloat(NSAppUnitsToFloatPixels(rect.width,
                       nsDeviceContext::AppUnitsPerCSSPixel()));
  tmp.AppendLiteral(", ");
  tmp.AppendFloat(NSAppUnitsToFloatPixels(rect.height,
                       nsDeviceContext::AppUnitsPerCSSPixel()));
  tmp.AppendLiteral("}");
  fputs(NS_LossyConvertUTF16toASCII(tmp).get(), out);
  return out;
}

#endif // DEBUG

nsRect
nsRect::SaturatingUnionEdges(const nsRect& aRect) const
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

void
nsRect::SaturatingInflate(const nsMargin& aMargin)
{
#ifdef NS_COORD_IS_FLOAT
  Inflate(aMargin);
#else
  int64_t nx = int64_t(x) - aMargin.left;
  int64_t w = int64_t(width) + int64_t(aMargin.left) + aMargin.right;
  if (MOZ_UNLIKELY(w > nscoord_MAX)) {
    NS_WARNING("Overflowed nscoord_MAX in conversion to nscoord width");
    int64_t xdiff = nx - nscoord_MIN / 2;
    if (xdiff < 0) {
      // Clamp huge negative x to nscoord_MIN / 2 and try again.
      nx = nscoord_MIN / 2;
      w += xdiff;
    }
    if (MOZ_UNLIKELY(w > nscoord_MAX)) {
      w = nscoord_MAX;
    }
  }
  width = nscoord(w);
  if (MOZ_UNLIKELY(nx < nscoord_MIN)) {
    NS_WARNING("Underflowed nscoord_MIN in conversion to nscoord x");
    nx = nscoord_MIN;
  }
  x = nscoord(nx);

  int64_t ny = int64_t(y) - aMargin.top;
  int64_t h = int64_t(height) + int64_t(aMargin.top) + aMargin.bottom;
  if (MOZ_UNLIKELY(h > nscoord_MAX)) {
    NS_WARNING("Overflowed nscoord_MAX in conversion to nscoord height");
    int64_t ydiff = ny - nscoord_MIN / 2;
    if (ydiff < 0) {
      // Clamp huge negative y to nscoord_MIN / 2 and try again.
      ny = nscoord_MIN / 2;
      h += ydiff;
    }
    if (MOZ_UNLIKELY(h > nscoord_MAX)) {
      h = nscoord_MAX;
    }
  }
  height = nscoord(h);
  if (MOZ_UNLIKELY(ny < nscoord_MIN)) {
    NS_WARNING("Underflowed nscoord_MIN in conversion to nscoord y");
    ny = nscoord_MIN;
  }
  y = nscoord(ny);
#endif
}
