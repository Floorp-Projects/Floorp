/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BASESIZE_H_
#define MOZILLA_GFX_BASESIZE_H_

#include <algorithm>
#include <ostream>

#include "mozilla/Attributes.h"

namespace mozilla::gfx {

/**
 * Do not use this class directly. Subclass it, pass that subclass as the
 * Sub parameter, and only use that subclass. This allows methods to safely
 * cast 'this' to 'Sub*'.
 */
template <class T, class Sub, class Coord = T>
struct BaseSize {
  union {
    struct {
      T width, height;
    };
    T components[2];
  };

  // Constructors
  constexpr BaseSize() : width(0), height(0) {}
  constexpr BaseSize(Coord aWidth, Coord aHeight)
      : width(aWidth), height(aHeight) {}

  void SizeTo(T aWidth, T aHeight) {
    width = aWidth;
    height = aHeight;
  }

  bool IsEmpty() const { return width <= 0 || height <= 0; }

  bool IsSquare() const { return width == height; }

  MOZ_ALWAYS_INLINE T Width() const { return width; }
  MOZ_ALWAYS_INLINE T Height() const { return height; }

  // Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator

  bool operator==(const Sub& aSize) const {
    return width == aSize.width && height == aSize.height;
  }
  bool operator!=(const Sub& aSize) const {
    return width != aSize.width || height != aSize.height;
  }
  bool operator<=(const Sub& aSize) const {
    return width <= aSize.width && height <= aSize.height;
  }
  bool operator<(const Sub& aSize) const {
    return *this <= aSize && *this != aSize;
  }

  Sub operator+(const Sub& aSize) const {
    return Sub(width + aSize.width, height + aSize.height);
  }
  Sub operator-(const Sub& aSize) const {
    return Sub(width - aSize.width, height - aSize.height);
  }
  Sub& operator+=(const Sub& aSize) {
    width += aSize.width;
    height += aSize.height;
    return *static_cast<Sub*>(this);
  }
  Sub& operator-=(const Sub& aSize) {
    width -= aSize.width;
    height -= aSize.height;
    return *static_cast<Sub*>(this);
  }

  Sub operator*(T aScale) const { return Sub(width * aScale, height * aScale); }
  Sub operator/(T aScale) const { return Sub(width / aScale, height / aScale); }
  friend Sub operator*(T aScale, const Sub& aSize) {
    return Sub(aScale * aSize.width, aScale * aSize.height);
  }
  void Scale(T aXScale, T aYScale) {
    width *= aXScale;
    height *= aYScale;
  }

  Sub operator*(const Sub& aSize) const {
    return Sub(width * aSize.width, height * aSize.height);
  }
  Sub operator/(const Sub& aSize) const {
    return Sub(width / aSize.width, height / aSize.height);
  }

  friend Sub Min(const Sub& aA, const Sub& aB) {
    return Sub(std::min(aA.width, aB.width), std::min(aA.height, aB.height));
  }

  friend Sub Max(const Sub& aA, const Sub& aB) {
    return Sub(std::max(aA.width, aB.width), std::max(aA.height, aB.height));
  }

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const BaseSize<T, Sub, Coord>& aSize) {
    return aStream << '(' << aSize.width << " x " << aSize.height << ')';
  }
};

}  // namespace mozilla::gfx

#endif /* MOZILLA_GFX_BASESIZE_H_ */
