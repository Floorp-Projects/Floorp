/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRect.h"
#include "mozilla/gfx/Types.h"          // for NS_SIDE_BOTTOM, etc
#include "nsDeviceContext.h"            // for nsDeviceContext
#include "nsString.h"               // for nsAutoString, etc
#include "nsMargin.h"                   // for nsMargin

static_assert((int(NS_SIDE_TOP) == 0) &&
              (int(NS_SIDE_RIGHT) == 1) &&
              (int(NS_SIDE_BOTTOM) == 2) &&
              (int(NS_SIDE_LEFT) == 3),
              "The mozilla::css::Side sequence must match the nsMargin nscoord sequence");

nsRect
ToAppUnits(const nsIntRect& aRect, nscoord aAppUnitsPerPixel)
{
  return nsRect(NSIntPixelsToAppUnits(aRect.x, aAppUnitsPerPixel),
                NSIntPixelsToAppUnits(aRect.y, aAppUnitsPerPixel),
                NSIntPixelsToAppUnits(aRect.width, aAppUnitsPerPixel),
                NSIntPixelsToAppUnits(aRect.height, aAppUnitsPerPixel));
}

const nsIntRect& GetMaxSizedIntRect() {
  static const nsIntRect r(0, 0, INT32_MAX, INT32_MAX);
  return r;
}

#ifdef DEBUG
// Diagnostics

FILE* operator<<(FILE* out, const nsRect& rect)
{
  nsAutoString tmp;

  // Output the coordinates in fractional pixels so they're easier to read
  tmp.Append('{');
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
  tmp.Append('}');
  fputs(NS_LossyConvertUTF16toASCII(tmp).get(), out);
  return out;
}

#endif // DEBUG
