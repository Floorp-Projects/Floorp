/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StyleColorInlines.h"

#include "mozilla/ComputedStyle.h"
#include "mozilla/ComputedStyleInlines.h"
#include "nsIFrame.h"
#include "nsStyleStruct.h"

namespace mozilla {

// Blend one RGBA color with another based on a given ratios.
// It is a linear combination of each channel with alpha premultipled.
static nscolor LinearBlendColors(const StyleRGBA& aBg, float aBgRatio,
                                 const StyleRGBA& aFg, float aFgRatio) {
  constexpr float kFactor = 1.0f / 255.0f;

  float p1 = aBgRatio;
  float a1 = kFactor * aBg.alpha;
  float r1 = a1 * aBg.red;
  float g1 = a1 * aBg.green;
  float b1 = a1 * aBg.blue;

  float p2 = aFgRatio;
  float a2 = kFactor * aFg.alpha;
  float r2 = a2 * aFg.red;
  float g2 = a2 * aFg.green;
  float b2 = a2 * aFg.blue;

  float a = p1 * a1 + p2 * a2;
  if (a <= 0.f) {
    return NS_RGBA(0, 0, 0, 0);
  }

  if (a > 1.f) {
    a = 1.f;
  }

  auto r = ClampColor((p1 * r1 + p2 * r2) / a);
  auto g = ClampColor((p1 * g1 + p2 * g2) / a);
  auto b = ClampColor((p1 * b1 + p2 * b2) / a);
  return NS_RGBA(r, g, b, NSToIntRound(a * 255));
}

template <>
bool StyleColor::MaybeTransparent() const {
  // We know that the color is opaque when it's a numeric color with
  // alpha == 255.
  // TODO(djg): Should we extend this to check Complex with bgRatio =
  // 0, and fgRatio * alpha >= 255?
  return !IsNumeric() || AsNumeric().alpha != 255;
}

template <>
nscolor StyleColor::CalcColor(nscolor aColor) const {
  return CalcColor(StyleRGBA::FromColor(aColor));
}

template <>
nscolor StyleColor::CalcColor(const StyleRGBA& aForegroundColor) const {
  if (IsNumeric()) {
    return AsNumeric().ToColor();
  }
  if (IsCurrentColor()) {
    return aForegroundColor.ToColor();
  }
  MOZ_ASSERT(IsComplex());
  const auto& complex = AsComplex();
  return LinearBlendColors(complex.color, complex.ratios.bg, aForegroundColor,
                           complex.ratios.fg);
}

template <>
nscolor StyleColor::CalcColor(const ComputedStyle& aStyle) const {
  // Common case that is numeric color, which is pure background, we
  // can skip resolving StyleColor().
  // TODO(djg): Is this optimization worth it?
  if (IsNumeric()) {
    return AsNumeric().ToColor();
  }
  return CalcColor(aStyle.StyleColor()->mColor);
}

template <>
nscolor StyleColor::CalcColor(const nsIFrame* aFrame) const {
  return CalcColor(*aFrame->Style());
}

}  // namespace mozilla
