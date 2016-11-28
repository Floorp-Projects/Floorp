/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRect.h"
#include "mozilla/gfx/Types.h"          // for eSideBottom, etc
#include "mozilla/CheckedInt.h"         // for CheckedInt
#include "nsDeviceContext.h"            // for nsDeviceContext
#include "nsString.h"               // for nsAutoString, etc
#include "nsMargin.h"                   // for nsMargin

static_assert((int(eSideTop) == 0) &&
              (int(eSideRight) == 1) &&
              (int(eSideBottom) == 2) &&
              (int(eSideLeft) == 3),
              "The mozilla::Side sequence must match the nsMargin nscoord sequence");

const mozilla::gfx::IntRect& GetMaxSizedIntRect() {
  static const mozilla::gfx::IntRect r(0, 0, INT32_MAX, INT32_MAX);
  return r;
}


bool nsRect::Overflows() const {
#ifdef NS_COORD_IS_FLOAT
  return false;
#else
  mozilla::CheckedInt<int32_t> xMost = this->x;
  xMost += this->width;
  mozilla::CheckedInt<int32_t> yMost = this->y;
  yMost += this->height;
  return !xMost.isValid() || !yMost.isValid();
#endif
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
