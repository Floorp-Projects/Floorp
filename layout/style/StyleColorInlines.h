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

namespace mozilla {

template<>
inline StyleColor StyleColor::FromColor(nscolor aColor) {
  return StyleColor::Numeric({NS_GET_R(aColor), NS_GET_G(aColor),
                              NS_GET_B(aColor), NS_GET_A(aColor)});
}

template<>
inline StyleColor StyleColor::Black() {
  return FromColor(NS_RGB(0, 0, 0));
}

template<>
inline StyleColor StyleColor::White() {
  return FromColor(NS_RGB(255, 255, 255));
}

template<>
inline StyleColor StyleColor::Transparent() {
  return FromColor(NS_RGBA(0, 0, 0, 0));
}

template <>
nscolor StyleColor::CalcColor(nscolor aForegroundColor) const;

template <>
nscolor StyleColor::CalcColor(const ComputedStyle&) const;

template <>
nscolor StyleColor::CalcColor(const nsIFrame*) const;

}  // namespace mozilla

#endif  // mozilla_StyleColor_h_
