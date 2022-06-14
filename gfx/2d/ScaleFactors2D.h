/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEFACTORS2D_H_
#define MOZILLA_GFX_SCALEFACTORS2D_H_

#include <ostream>

#include "mozilla/Attributes.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/gfx/ScaleFactor.h"
#include "mozilla/gfx/Point.h"

#include "gfxPoint.h"

namespace mozilla {
namespace gfx {

/*
 * This class is like ScaleFactor, but allows different scales on the x and
 * y axes.
 */
template <class Src, class Dst, class T>
struct BaseScaleFactors2D {
  T xScale;
  T yScale;

  constexpr BaseScaleFactors2D() : xScale(1.0), yScale(1.0) {}
  constexpr BaseScaleFactors2D(const BaseScaleFactors2D& aCopy)
      : xScale(aCopy.xScale), yScale(aCopy.yScale) {}
  constexpr BaseScaleFactors2D(T aXScale, T aYScale)
      : xScale(aXScale), yScale(aYScale) {}
  // Layout code often uses gfxSize to represent a pair of x/y scales.
  explicit constexpr BaseScaleFactors2D(const gfxSize& aSize)
      : xScale(aSize.width), yScale(aSize.height) {}

  // "Upgrade" from a ScaleFactor.
  // This is deliberately 'explicit' so that the treatment of a single scale
  // number as both the x- and y-scale in a context where they are allowed to
  // be different, is more visible.
  explicit constexpr BaseScaleFactors2D(const ScaleFactor<Src, Dst>& aScale)
      : xScale(aScale.scale), yScale(aScale.scale) {}

  bool AreScalesSame() const {
    return FuzzyEqualsMultiplicative(xScale, yScale);
  }

  // Convert the underlying floating point type storing the scale factors
  // to that of NewT.
  template <typename NewT>
  BaseScaleFactors2D<Src, Dst, NewT> ConvertTo() const {
    return BaseScaleFactors2D<Src, Dst, NewT>(NewT(xScale), NewT(yScale));
  }

  // Convert to a ScaleFactor. Asserts that the scales are, in fact, equal.
  ScaleFactor<Src, Dst> ToScaleFactor() const {
    // Avoid implicit narrowing from double to float. An explicit conversion
    // may be done with `scales.ConvertTo<float>().ToScaleFactor()` if desired.
    static_assert(std::is_same_v<T, float>);
    MOZ_ASSERT(AreScalesSame());
    return ScaleFactor<Src, Dst>(xScale);
  }

  // Convert to a SizeTyped. Eventually, we should replace all uses of SizeTyped
  // to represent scales with ScaleFactors2D, and remove this function.
  SizeTyped<UnknownUnits, T> ToSize() const {
    return SizeTyped<UnknownUnits, T>(xScale, yScale);
  }

  BaseScaleFactors2D& operator=(const BaseScaleFactors2D&) = default;

  bool operator==(const BaseScaleFactors2D& aOther) const {
    return xScale == aOther.xScale && yScale == aOther.yScale;
  }

  bool operator!=(const BaseScaleFactors2D& aOther) const {
    return !(*this == aOther);
  }

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const BaseScaleFactors2D& aScale) {
    if (aScale.AreScalesSame()) {
      return aStream << aScale.xScale;
    } else {
      return aStream << '(' << aScale.xScale << ',' << aScale.yScale << ')';
    }
  }

  template <class Other>
  BaseScaleFactors2D<Other, Dst, T> operator/(
      const BaseScaleFactors2D<Src, Other, T>& aOther) const {
    return BaseScaleFactors2D<Other, Dst, T>(xScale / aOther.xScale,
                                             yScale / aOther.yScale);
  }

  template <class Other>
  BaseScaleFactors2D<Src, Other, T> operator/(
      const BaseScaleFactors2D<Other, Dst, T>& aOther) const {
    return BaseScaleFactors2D<Src, Other, T>(xScale / aOther.xScale,
                                             yScale / aOther.yScale);
  }

  template <class Other>
  BaseScaleFactors2D<Src, Other, T> operator*(
      const BaseScaleFactors2D<Dst, Other, T>& aOther) const {
    return BaseScaleFactors2D<Src, Other, T>(xScale * aOther.xScale,
                                             yScale * aOther.yScale);
  }

  template <class Other>
  BaseScaleFactors2D<Other, Dst, T> operator*(
      const BaseScaleFactors2D<Other, Src, T>& aOther) const {
    return BaseScaleFactors2D<Other, Dst, T>(xScale * aOther.xScale,
                                             yScale * aOther.yScale);
  }

  BaseScaleFactors2D<Src, Src, T> operator*(
      const BaseScaleFactors2D<Dst, Src, T>& aOther) const {
    return BaseScaleFactors2D<Src, Src, T>(xScale * aOther.xScale,
                                           yScale * aOther.yScale);
  }

  template <class Other>
  BaseScaleFactors2D<Src, Other, T> operator*(
      const ScaleFactor<Dst, Other>& aOther) const {
    return *this * BaseScaleFactors2D<Dst, Other, T>(aOther);
  }

  template <class Other>
  BaseScaleFactors2D<Other, Dst, T> operator*(
      const ScaleFactor<Other, Src>& aOther) const {
    return *this * BaseScaleFactors2D<Other, Src, T>(aOther);
  }

  BaseScaleFactors2D<Src, Src, T> operator*(
      const ScaleFactor<Dst, Src>& aOther) const {
    return *this * BaseScaleFactors2D<Dst, Src, T>(aOther);
  }

  template <class Other>
  BaseScaleFactors2D<Src, Other, T> operator/(
      const ScaleFactor<Other, Dst>& aOther) const {
    return *this / BaseScaleFactors2D<Other, Dst, T>(aOther);
  }

  template <class Other>
  BaseScaleFactors2D<Other, Dst, T> operator/(
      const ScaleFactor<Src, Other>& aOther) const {
    return *this / BaseScaleFactors2D<Src, Other, T>(aOther);
  }

  template <class Other>
  friend BaseScaleFactors2D<Other, Dst, T> operator*(
      const ScaleFactor<Other, Src>& aA, const BaseScaleFactors2D& aB) {
    return BaseScaleFactors2D<Other, Src, T>(aA) * aB;
  }

  template <class Other>
  friend BaseScaleFactors2D<Other, Src, T> operator/(
      const ScaleFactor<Other, Dst>& aA, const BaseScaleFactors2D& aB) {
    return BaseScaleFactors2D<Other, Src, T>(aA) / aB;
  }

  static BaseScaleFactors2D<Src, Dst, T> FromUnknownScale(
      const BaseScaleFactors2D<UnknownUnits, UnknownUnits, T>& scale) {
    return BaseScaleFactors2D<Src, Dst, T>(scale.xScale, scale.yScale);
  }

  BaseScaleFactors2D<UnknownUnits, UnknownUnits, T> ToUnknownScale() const {
    return BaseScaleFactors2D<UnknownUnits, UnknownUnits, T>(xScale, yScale);
  }

  friend BaseScaleFactors2D Min(const BaseScaleFactors2D& aA,
                                const BaseScaleFactors2D& aB) {
    return BaseScaleFactors2D(std::min(aA.xScale, aB.xScale),
                              std::min(aA.yScale, aB.yScale));
  }

  friend BaseScaleFactors2D Max(const BaseScaleFactors2D& aA,
                                const BaseScaleFactors2D& aB) {
    return BaseScaleFactors2D(std::max(aA.xScale, aB.xScale),
                              std::max(aA.yScale, aB.yScale));
  }
};

template <class Src, class Dst>
using ScaleFactors2D = BaseScaleFactors2D<Src, Dst, float>;

template <class Src, class Dst>
using ScaleFactors2DDouble = BaseScaleFactors2D<Src, Dst, double>;

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_SCALEFACTORS2D_H_ */
