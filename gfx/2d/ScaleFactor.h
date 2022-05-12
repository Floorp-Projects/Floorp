/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEFACTOR_H_
#define MOZILLA_GFX_SCALEFACTOR_H_

#include <ostream>

#include "mozilla/Attributes.h"

#include "gfxPoint.h"

namespace mozilla {
namespace gfx {

/*
 * This class represents a scaling factor between two different pixel unit
 * systems. This is effectively a type-safe float, intended to be used in
 * combination with the known-type instances of gfx::Point, gfx::Rect, etc.
 *
 * This class is meant to be used in cases where a single scale applies to
 * both the x and y axes. For cases where two diferent scales apply, use
 * ScaleFactors2D.
 */
template <class Src, class Dst>
struct ScaleFactor {
  float scale;

  constexpr ScaleFactor() : scale(1.0) {}
  constexpr ScaleFactor(const ScaleFactor<Src, Dst>& aCopy)
      : scale(aCopy.scale) {}
  explicit constexpr ScaleFactor(float aScale) : scale(aScale) {}

  ScaleFactor<Dst, Src> Inverse() { return ScaleFactor<Dst, Src>(1 / scale); }

  ScaleFactor<Src, Dst>& operator=(const ScaleFactor<Src, Dst>&) = default;

  bool operator==(const ScaleFactor<Src, Dst>& aOther) const {
    return scale == aOther.scale;
  }

  bool operator!=(const ScaleFactor<Src, Dst>& aOther) const {
    return !(*this == aOther);
  }

  bool operator<(const ScaleFactor<Src, Dst>& aOther) const {
    return scale < aOther.scale;
  }

  bool operator<=(const ScaleFactor<Src, Dst>& aOther) const {
    return scale <= aOther.scale;
  }

  bool operator>(const ScaleFactor<Src, Dst>& aOther) const {
    return scale > aOther.scale;
  }

  bool operator>=(const ScaleFactor<Src, Dst>& aOther) const {
    return scale >= aOther.scale;
  }

  template <class Other>
  ScaleFactor<Other, Dst> operator/(
      const ScaleFactor<Src, Other>& aOther) const {
    return ScaleFactor<Other, Dst>(scale / aOther.scale);
  }

  template <class Other>
  ScaleFactor<Src, Other> operator/(
      const ScaleFactor<Other, Dst>& aOther) const {
    return ScaleFactor<Src, Other>(scale / aOther.scale);
  }

  template <class Other>
  ScaleFactor<Src, Other> operator*(
      const ScaleFactor<Dst, Other>& aOther) const {
    return ScaleFactor<Src, Other>(scale * aOther.scale);
  }

  template <class Other>
  ScaleFactor<Other, Dst> operator*(
      const ScaleFactor<Other, Src>& aOther) const {
    return ScaleFactor<Other, Dst>(scale * aOther.scale);
  }

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const ScaleFactor<Src, Dst>& aSF) {
    return aStream << aSF.scale;
  }
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_SCALEFACTOR_H_ */
