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
  // Whether the complex color represents a computed-value time auto
  // value. This is only a flag indicating that this value should not
  // be interpolatable with other colors, while other fields still
  // represents the actual used color of this value.
  bool mIsAuto;

  static StyleComplexColor FromColor(nscolor aColor) {
    return {aColor, 0, false};
  }
  static StyleComplexColor CurrentColor() {
    return {NS_RGBA(0, 0, 0, 0), 255, false};
  }
  static StyleComplexColor Auto() {
    return {NS_RGBA(0, 0, 0, 0), 255, true};
  }

  bool IsNumericColor() const { return mForegroundRatio == 0; }
  bool IsCurrentColor() const { return mForegroundRatio == 255; }

  bool operator==(const StyleComplexColor& aOther) const {
    return mForegroundRatio == aOther.mForegroundRatio &&
           (IsCurrentColor() || mColor == aOther.mColor) &&
           mIsAuto == aOther.mIsAuto;
  }
  bool operator!=(const StyleComplexColor& aOther) const {
    return !(*this == aOther);
  }
};

}

#endif // mozilla_StyleComplexColor_h_
