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

template <>
bool StyleColor::MaybeTransparent() const {
  // We know that the color is opaque when it's a numeric color with
  // alpha == 255.
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
  MOZ_ASSERT(IsColorMix());
  return Servo_ResolveColor(this, &aForegroundColor);
}

template <>
nscolor StyleColor::CalcColor(const ComputedStyle& aStyle) const {
  // Common case that is numeric color, which is pure background, we
  // can skip resolving StyleText().
  if (IsNumeric()) {
    return AsNumeric().ToColor();
  }
  return CalcColor(aStyle.StyleText()->mColor);
}

template <>
nscolor StyleColor::CalcColor(const nsIFrame* aFrame) const {
  if (IsNumeric()) {
    return AsNumeric().ToColor();
  }
  return CalcColor(aFrame->StyleText()->mColor);
}

}  // namespace mozilla
