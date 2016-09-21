/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* represent a color combines a numeric color and currentcolor */

#ifndef mozilla_StyleComplexColor_h_
#define mozilla_StyleComplexColor_h_

#include "nsColor.h"

namespace mozilla {

/**
 * This struct represents a combined color from a numeric color and
 * the current foreground color (currentcolor keyword).
 * Conceptually, the formula is "color * (1 - p) + currentcolor * p"
 * where p is mForegroundRatio. See mozilla::LinearBlendColors for
 * the actual algorithm.
 */
struct StyleComplexColor
{
  nscolor mColor;
  uint8_t mForegroundRatio;

  StyleComplexColor() {}
  StyleComplexColor(nscolor aColor, uint_fast8_t aForegroundRatio)
    : mColor(aColor), mForegroundRatio(aForegroundRatio) {}

  static StyleComplexColor FromColor(nscolor aColor)
    { return StyleComplexColor(aColor, 0); }
  static StyleComplexColor CurrentColor()
    { return StyleComplexColor(NS_RGBA(0, 0, 0, 0), 255); }

  bool IsNumericColor() const { return mForegroundRatio == 0; }
  bool IsCurrentColor() const { return mForegroundRatio == 255; }

  bool operator==(const StyleComplexColor& aOther) const {
    return mForegroundRatio == aOther.mForegroundRatio &&
           (IsCurrentColor() || mColor == aOther.mColor);
  }
  bool operator!=(const StyleComplexColor& aOther) const {
    return !(*this == aOther);
  }
};

}

#endif // mozilla_StyleComplexColor_h_
