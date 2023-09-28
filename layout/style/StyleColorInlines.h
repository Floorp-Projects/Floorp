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
  return StyleAbsoluteColor::SrgbLegacy(
      NS_GET_R(aColor) / 255.0f, NS_GET_G(aColor) / 255.0f,
      NS_GET_B(aColor) / 255.0f, NS_GET_A(aColor) / 255.0f);
}

// static
inline StyleAbsoluteColor StyleAbsoluteColor::SrgbLegacy(float red, float green,
                                                         float blue,
                                                         float alpha) {
  const auto ToLegacyComponent = [](float aF) {
    if (MOZ_UNLIKELY(!std::isfinite(aF))) {
      return 0.0f;
    }
    return aF;
  };

  return StyleAbsoluteColor{
      StyleColorComponents{ToLegacyComponent(red), ToLegacyComponent(green),
                           ToLegacyComponent(blue)},
      alpha, StyleColorSpace::Srgb, StyleColorFlags::IS_LEGACY_SRGB};
}

template <>
inline StyleColor StyleColor::FromColor(nscolor aColor) {
  return StyleColor::Absolute(StyleAbsoluteColor::FromColor(aColor));
}

template <>
inline StyleColor StyleColor::Transparent() {
  return StyleColor::Absolute(StyleAbsoluteColor::TRANSPARENT_BLACK);
}

template <>
inline StyleColor StyleColor::Black() {
  return StyleColor::Absolute(StyleAbsoluteColor::BLACK);
}

template <>
inline StyleColor StyleColor::White() {
  return StyleColor::Absolute(StyleAbsoluteColor::WHITE);
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
