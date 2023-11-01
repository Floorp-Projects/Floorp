/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StyleColorInlines.h"

#include "mozilla/ComputedStyle.h"
#include "mozilla/ComputedStyleInlines.h"
#include "mozilla/dom/BindingDeclarations.h"
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

  // TODO(tlouw): Needs gamut mapping here. Right now we just hard clip the
  //              components to [0..1], which will yield invalid colors.
  //              https://bugzilla.mozilla.org/show_bug.cgi?id=1626624
  auto red = std::clamp(srgb.components._0, 0.0f, 1.0f);
  auto green = std::clamp(srgb.components._1, 0.0f, 1.0f);
  auto blue = std::clamp(srgb.components._2, 0.0f, 1.0f);

  return NS_RGBA(nsStyleUtil::FloatToColorComponent(red),
                 nsStyleUtil::FloatToColorComponent(green),
                 nsStyleUtil::FloatToColorComponent(blue),
                 nsStyleUtil::FloatToColorComponent(srgb.alpha));
}

}  // namespace mozilla
