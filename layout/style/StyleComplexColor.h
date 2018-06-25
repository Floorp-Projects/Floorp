/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* represent a color combines a numeric color and currentcolor */

#ifndef mozilla_StyleComplexColor_h_
#define mozilla_StyleComplexColor_h_

#include "nsColor.h"

class nsIFrame;

namespace mozilla {

class ComputedStyle;

/**
 * This struct represents a combined color from a numeric color and
 * the current foreground color (currentcolor keyword).
 * Conceptually, the formula is "color * q + currentcolor * p"
 * where p is mFgRatio and q is mBgRatio.
 *
 * It can also represent an "auto" value, which is valid for some
 * properties. See comment of `Tag::eAuto`.
 */
class StyleComplexColor final
{
public:
  static StyleComplexColor FromColor(nscolor aColor) {
    return {aColor, 0, eNumeric};
  }
  static StyleComplexColor CurrentColor() {
    return {NS_RGBA(0, 0, 0, 0), 1, eForeground};
  }
  static StyleComplexColor Auto() {
    return {NS_RGBA(0, 0, 0, 0), 1, eAuto};
  }

  bool IsAuto() const { return mTag == eAuto; }
  bool IsCurrentColor() const { return mTag == eForeground; }

  bool operator==(const StyleComplexColor& aOther) const {
    if (mTag != aOther.mTag) {
      return false;
    }

    switch (mTag) {
    case eAuto:
    case eForeground:
      return true;
    case eNumeric:
      return mColor == aOther.mColor;
    case eComplex:
      return (mBgRatio == aOther.mBgRatio &&
              mFgRatio == aOther.mFgRatio &&
              mColor == aOther.mColor);
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected StyleComplexColor type.");
      return false;
    }
  }

  bool operator!=(const StyleComplexColor& aOther) const {
    return !(*this == aOther);
  }

  /**
   * Is it possible that this StyleComplexColor is transparent?
   */
  bool MaybeTransparent() const;

  /**
   * Compute the color for this StyleComplexColor, taking into account
   * the foreground color, aForegroundColor.
   */
  nscolor CalcColor(nscolor aForegroundColor) const;

  /**
   * Compute the color for this StyleComplexColor, taking into account
   * the foreground color from aStyle.
   */
  nscolor CalcColor(mozilla::ComputedStyle* aStyle) const;

  /**
   * Compute the color for this StyleComplexColor, taking into account
   * the foreground color from aFrame's ComputedStyle.
   */
  nscolor CalcColor(const nsIFrame* aFrame) const;

private:
  enum Tag : uint8_t {
    // This represents a computed-value time auto value. This
    // indicates that this value should not be interpolatable with
    // other colors. Other fields represent a currentcolor and
    // properties can decide whether that should be used.
    eAuto,
    // This represents a numeric color; no currentcolor component.
    eNumeric,
    // This represents the current foreground color, currentcolor; no
    // numeric color component.
    eForeground,
    // This represents a linear combination of numeric color and the
    // foreground color: "mColor * mBgRatio + currentcolor *
    // mFgRatio".
    eComplex,
  };

  StyleComplexColor(nscolor aColor,
                    float aFgRatio,
                    Tag aTag)
    : mColor(aColor)
    , mBgRatio(1.f - aFgRatio)
    , mFgRatio(aFgRatio)
    , mTag(aTag)
  {
    MOZ_ASSERT(mTag != eNumeric || aFgRatio == 0.);
    MOZ_ASSERT(!(mTag == eAuto || mTag == eForeground) || aFgRatio == 1.);
  }

  nscolor mColor;
  float mBgRatio;
  float mFgRatio;
  Tag mTag;
};

} // namespace mozilla

#endif // mozilla_StyleComplexColor_h_
