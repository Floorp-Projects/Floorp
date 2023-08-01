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

inline StyleAbsoluteColor StyleAbsoluteColor::Transparent() {
  return StyleAbsoluteColor::Srgb(0.0f, 0.0f, 0.0f, 0.0f);
}

inline StyleAbsoluteColor StyleAbsoluteColor::Black() {
  return StyleAbsoluteColor::Srgb(0.0f, 0.0f, 0.0f, 1.0f);
}

inline StyleAbsoluteColor StyleAbsoluteColor::White() {
  return StyleAbsoluteColor::Srgb(1.0f, 1.0f, 1.0f, 1.0f);
}

template <>
inline StyleColor StyleColor::FromColor(nscolor aColor) {
  return StyleColor::Absolute(StyleAbsoluteColor::FromColor(aColor));
}

template <>
inline StyleColor StyleColor::Black() {
  return FromColor(NS_RGB(0, 0, 0));
}

template <>
inline StyleColor StyleColor::White() {
  return FromColor(NS_RGB(255, 255, 255));
}

template <>
inline StyleColor StyleColor::Transparent() {
  return FromColor(NS_RGBA(0, 0, 0, 0));
}

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
