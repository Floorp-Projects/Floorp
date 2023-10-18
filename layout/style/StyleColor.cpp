/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StyleColorInlines.h"

#include "mozilla/ComputedStyle.h"
#include "mozilla/StaticPrefs_layout.h"
#include "nsIFrame.h"
#include "nsStyleStruct.h"

namespace mozilla {

template <>
bool StyleColor::MaybeTransparent() const {
  // We know that the color is opaque when it's a numeric color with
  // alpha == 1.0.
  return !IsAbsolute() || AsAbsolute().alpha != 1.0f;
}

template <>
StyleAbsoluteColor StyleColor::ResolveColor(
    const StyleAbsoluteColor& aForegroundColor) const {
  if (IsAbsolute()) {
    return AsAbsolute();
  }

  if (IsCurrentColor()) {
    return aForegroundColor;
  }

  MOZ_ASSERT(IsColorMix(), "should be the only type left at this point.");
  return Servo_ResolveColor(this, &aForegroundColor);
}

template <>
nscolor StyleColor::CalcColor(nscolor aColor) const {
  return ResolveColor(StyleAbsoluteColor::FromColor(aColor)).ToColor();
}

template <>
nscolor StyleColor::CalcColor(
    const StyleAbsoluteColor& aForegroundColor) const {
  return ResolveColor(aForegroundColor).ToColor();
}

template <>
nscolor StyleColor::CalcColor(const ComputedStyle& aStyle) const {
  return ResolveColor(aStyle.StyleText()->mColor).ToColor();
}

template <>
nscolor StyleColor::CalcColor(const nsIFrame* aFrame) const {
  return ResolveColor(aFrame->StyleText()->mColor).ToColor();
}

StyleAbsoluteColor StyleAbsoluteColor::ToColorSpace(
    StyleColorSpace aColorSpace) const {
  return Servo_ConvertColorSpace(this, aColorSpace);
}

nscolor StyleAbsoluteColor::ToColor() const {
  auto srgb = ToColorSpace(StyleColorSpace::Srgb);

  constexpr float MIN = 0.0f;
  constexpr float MAX = 1.0f;

  // We KNOW the values are in srgb so we can do a quick gamut limit check
  // here and avoid calling into Servo_MapColorIntoGamutLimits and let it
  // return early anyway.
  auto isColorInGamut =
      (srgb.components._0 >= MIN && srgb.components._0 <= MAX &&
       srgb.components._1 >= MIN && srgb.components._1 <= MAX &&
       srgb.components._2 >= MIN && srgb.components._2 <= MAX);

  if (!isColorInGamut) {
    if (StaticPrefs::layout_css_gamut_map_for_rendering_enabled()) {
      srgb = Servo_MapColorIntoGamutLimits(&srgb);
    } else {
      // If gamut mapping is not enabled, we just naively clip the colors at
      // sRGB gamut limits. This will go away completely when gamut mapping is
      // enabled.
      srgb.components._0 = std::clamp(srgb.components._0, 0.0f, 1.0f);
      srgb.components._1 = std::clamp(srgb.components._1, 0.0f, 1.0f);
      srgb.components._2 = std::clamp(srgb.components._2, 0.0f, 1.0f);
    }
  }

  return NS_RGBA(nsStyleUtil::FloatToColorComponent(srgb.components._0),
                 nsStyleUtil::FloatToColorComponent(srgb.components._1),
                 nsStyleUtil::FloatToColorComponent(srgb.components._2),
                 nsStyleUtil::FloatToColorComponent(srgb.alpha));
}

}  // namespace mozilla
