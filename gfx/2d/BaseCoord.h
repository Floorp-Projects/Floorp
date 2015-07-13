/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BASECOORD_H_
#define MOZILLA_GFX_BASECOORD_H_

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
  MOZ_CONSTEXPR BaseCoord() : value(0) {}
  explicit MOZ_CONSTEXPR BaseCoord(T aValue) : value(aValue) {}

  // Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator

  operator T() const { return value; }

  friend bool operator==(Sub aA, Sub aB) {
    return aA.value == aB.value;
  }
  friend bool operator!=(Sub aA, Sub aB) {
    return aA.value != aB.value;
  }

  friend Sub operator+(Sub aA, Sub aB) {
    return Sub(aA.value + aB.value);
  }
  friend Sub operator-(Sub aA, Sub aB) {
    return Sub(aA.value - aB.value);
  }
  friend Sub operator*(Sub aCoord, T aScale) {
    return Sub(aCoord.value * aScale);
  }
  friend Sub operator*(T aScale, Sub aCoord) {
    return Sub(aScale * aCoord.value);
  }
  friend Sub operator/(Sub aCoord, T aScale) {
    return Sub(aCoord.value / aScale);
  }
  // 'scale / coord' is intentionally omitted because it doesn't make sense.

  Sub& operator+=(Sub aCoord) {
    value += aCoord.value;
    return *static_cast<Sub*>(this);
  }
  Sub& operator-=(Sub aCoord) {
    value -= aCoord.value;
    return *static_cast<Sub*>(this);
  }
  Sub& operator*=(T aScale) {
    value *= aScale;
    return *static_cast<Sub*>(this);
  }
  Sub& operator/=(T aScale) {
    value /= aScale;
    return *static_cast<Sub*>(this);
  }

  // Since BaseCoord is implicitly convertible to its value type T, we need
  // mixed-type operator overloads to avoid ambiguities at mixed-type call
  // sites. As we transition more of our code to strongly-typed classes, we
  // may be able to remove some or all of these overloads.
  friend bool operator==(Sub aA, T aB) {
    return aA.value == aB;
  }
  friend bool operator==(T aA, Sub aB) {
    return aA == aB.value;
  }
  friend bool operator!=(Sub aA, T aB) {
    return aA.value != aB;
  }
  friend bool operator!=(T aA, Sub aB) {
    return aA != aB.value;
  }
  friend T operator+(Sub aA, T aB) {
    return aA.value + aB;
  }
  friend T operator+(T aA, Sub aB) {
    return aA + aB.value;
  }
  friend T operator-(Sub aA, T aB) {
    return aA.value - aB;
  }
  friend T operator-(T aA, Sub aB) {
    return aA - aB.value;
  }

  Sub operator-() const {
    return Sub(-value);
  }
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_BASECOORD_H_ */
