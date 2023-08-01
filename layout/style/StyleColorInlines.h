/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Inline functions for StyleColor (aka values::computed::Color) */

#ifndef mozilla_StyleColorInlines_h_
#define mozilla_StyleColorInlines_h_

#include "nsColor.h"
#include "mozilla/ServoStyleConsts.h"
#include "nsStyleUtil.h"

namespace mozilla {

inline StyleAbsoluteColor StyleAbsoluteColor::FromColor(nscolor aColor) {
  return StyleAbsoluteColor::Srgb(
      NS_GET_R(aColor) / 255.0f, NS_GET_G(aColor) / 255.0f,
      NS_GET_B(aColor) / 255.0f, NS_GET_A(aColor) / 255.0f);
}

// static
inline StyleAbsoluteColor StyleAbsoluteColor::Srgb(float red, float green,
                                                   float blue, float alpha) {
  return StyleAbsoluteColor{StyleColorComponents{red, green, blue}, alpha,
                            StyleColorSpace::Srgb, StyleColorFlags{0}};
}

template <>
inline StyleColor StyleColor::FromColor(nscolor aColor) {
  return StyleColor::Absolute(StyleAbsoluteColor::FromColor(aColor));
}

// Workaround for window.h conflict.
#pragma push_macro("TRANSPARENT")
#undef TRANSPARENT

// static
template <>
inline const StyleColor StyleColor::TRANSPARENT =
    StyleColor::Absolute(StyleAbsoluteColor::TRANSPARENT);

#pragma pop_macro("TRANSPARENT")

// static
template <>
inline const StyleColor StyleColor::BLACK =
    StyleColor::Absolute(StyleAbsoluteColor::BLACK);

// static
template <>
inline const StyleColor StyleColor::WHITE =
    StyleColor::Absolute(StyleAbsoluteColor::WHITE);

template <>
StyleAbsoluteColor StyleColor::ResolveColor(const StyleAbsoluteColor&) const;

template <>
nscolor StyleColor::CalcColor(const StyleAbsoluteColor&) const;

template <>
nscolor StyleColor::CalcColor(nscolor) const;

template <>
nscolor StyleColor::CalcColor(const ComputedStyle&) const;

template <>
nscolor StyleColor::CalcColor(const nsIFrame*) const;

}  // namespace mozilla

#endif  // mozilla_StyleColor_h_
