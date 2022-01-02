/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BASECOORD_H_
#define MOZILLA_GFX_BASECOORD_H_

#include <ostream>

#include "mozilla/Attributes.h"

namespace mozilla {
namespace gfx {

/**
 * Do not use this class directly. Subclass it, pass that subclass as the
 * Sub parameter, and only use that subclass. This allows methods to safely
 * cast 'this' to 'Sub*'.
 */
template <class T, class Sub>
struct BaseCoord {
  T value;

  // Constructors
  constexpr BaseCoord() : value(0) {}
  explicit constexpr BaseCoord(T aValue) : value(aValue) {}

  // Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator

  constexpr operator T() const { return value; }

  friend constexpr bool operator==(Sub aA, Sub aB) {
    return aA.value == aB.value;
  }
  friend constexpr bool operator!=(Sub aA, Sub aB) {
    return aA.value != aB.value;
  }

  friend constexpr Sub operator+(Sub aA, Sub aB) {
    return Sub(aA.value + aB.value);
  }
  friend constexpr Sub operator-(Sub aA, Sub aB) {
    return Sub(aA.value - aB.value);
  }
  friend constexpr Sub operator*(Sub aCoord, T aScale) {
    return Sub(aCoord.value * aScale);
  }
  friend constexpr Sub operator*(T aScale, Sub aCoord) {
    return Sub(aScale * aCoord.value);
  }
  friend constexpr Sub operator/(Sub aCoord, T aScale) {
    return Sub(aCoord.value / aScale);
  }
  // 'scale / coord' is intentionally omitted because it doesn't make sense.

  constexpr Sub& operator+=(Sub aCoord) {
    value += aCoord.value;
    return *static_cast<Sub*>(this);
  }
  constexpr Sub& operator-=(Sub aCoord) {
    value -= aCoord.value;
    return *static_cast<Sub*>(this);
  }
  constexpr Sub& operator*=(T aScale) {
    value *= aScale;
    return *static_cast<Sub*>(this);
  }
  constexpr Sub& operator/=(T aScale) {
    value /= aScale;
    return *static_cast<Sub*>(this);
  }

  // Since BaseCoord is implicitly convertible to its value type T, we need
  // mixed-type operator overloads to avoid ambiguities at mixed-type call
  // sites. As we transition more of our code to strongly-typed classes, we
  // may be able to remove some or all of these overloads.
  friend constexpr bool operator==(Sub aA, T aB) { return aA.value == aB; }
  friend constexpr bool operator==(T aA, Sub aB) { return aA == aB.value; }
  friend constexpr bool operator!=(Sub aA, T aB) { return aA.value != aB; }
  friend constexpr bool operator!=(T aA, Sub aB) { return aA != aB.value; }
  friend constexpr T operator+(Sub aA, T aB) { return aA.value + aB; }
  friend constexpr T operator+(T aA, Sub aB) { return aA + aB.value; }
  friend constexpr T operator-(Sub aA, T aB) { return aA.value - aB; }
  friend constexpr T operator-(T aA, Sub aB) { return aA - aB.value; }

  constexpr Sub operator-() const { return Sub(-value); }

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const BaseCoord<T, Sub>& aCoord) {
    return aStream << aCoord.value;
  }
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_BASECOORD_H_ */
