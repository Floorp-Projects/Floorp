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
template <class src, class dst, class T>
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
  explicit constexpr BaseScaleFactors2D(const ScaleFactor<src, dst>& aScale)
      : xScale(aScale.scale), yScale(aScale.scale) {}

  bool AreScalesSame() const {
    return FuzzyEqualsMultiplicative(xScale, yScale);
  }

  // Convert the underlying floating point type storing the scale factors
  // to that of NewT.
  template <typename NewT>
  BaseScaleFactors2D<src, dst, NewT> ConvertTo() const {
    return BaseScaleFactors2D<src, dst, NewT>(NewT(xScale), NewT(yScale));
  }

  // Convert to a ScaleFactor. Asserts that the scales are, in fact, equal.
  ScaleFactor<src, dst> ToScaleFactor() const {
    // Avoid implicit narrowing from double to float. An explicit conversion
    // may be done with `scales.ConvertTo<float>().ToScaleFactor()` if desired.
    static_assert(std::is_same_v<T, float>);
    MOZ_ASSERT(AreScalesSame());
    return ScaleFactor<src, dst>(xScale);
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

  template <class other>
  BaseScaleFactors2D<other, dst, T> operator/(
      const BaseScaleFactors2D<src, other, T>& aOther) const {
    return BaseScaleFactors2D<other, dst, T>(xScale / aOther.xScale,
                                             yScale / aOther.yScale);
  }

  template <class other>
  BaseScaleFactors2D<src, other, T> operator/(
      const BaseScaleFactors2D<other, dst, T>& aOther) const {
    return BaseScaleFactors2D<src, other, T>(xScale / aOther.xScale,
                                             yScale / aOther.yScale);
  }

  template <class other>
  BaseScaleFactors2D<src, other, T> operator*(
      const BaseScaleFactors2D<dst, other, T>& aOther) const {
    return BaseScaleFactors2D<src, other, T>(xScale * aOther.xScale,
                                             yScale * aOther.yScale);
  }

  template <class other>
  BaseScaleFactors2D<other, dst, T> operator*(
      const BaseScaleFactors2D<other, src, T>& aOther) const {
    return BaseScaleFactors2D<other, dst, T>(xScale * aOther.xScale,
                                             yScale * aOther.yScale);
  }

  template <class other>
  BaseScaleFactors2D<src, other, T> operator*(
      const ScaleFactor<dst, other>& aOther) const {
    return *this * BaseScaleFactors2D<dst, other, T>(aOther);
  }

  template <class other>
  BaseScaleFactors2D<other, dst, T> operator*(
      const ScaleFactor<other, src>& aOther) const {
    return *this * BaseScaleFactors2D<other, src, T>(aOther);
  }

  template <class other>
  BaseScaleFactors2D<src, other, T> operator/(
      const ScaleFactor<other, dst>& aOther) const {
    return *this / BaseScaleFactors2D<other, dst, T>(aOther);
  }

  template <class other>
  BaseScaleFactors2D<other, dst, T> operator/(
      const ScaleFactor<src, other>& aOther) const {
    return *this / BaseScaleFactors2D<src, other, T>(aOther);
  }

  template <class other>
  friend BaseScaleFactors2D<other, dst, T> operator*(
      const ScaleFactor<other, src>& aA, const BaseScaleFactors2D& aB) {
    return BaseScaleFactors2D<other, src, T>(aA) * aB;
  }

  template <class other>
  friend BaseScaleFactors2D<other, src, T> operator/(
      const ScaleFactor<other, dst>& aA, const BaseScaleFactors2D& aB) {
    return BaseScaleFactors2D<other, src, T>(aA) / aB;
  }

  // Divide two scales of the same units, yielding a scale with no units,
  // represented as a gfxSize. This can mean e.g. the change in a particular
  // scale from one frame to the next.
  gfxSize operator/(const BaseScaleFactors2D& aOther) const {
    return gfxSize(xScale / aOther.xScale, yScale / aOther.yScale);
  }
};

template <class src, class dst>
using ScaleFactors2D = BaseScaleFactors2D<src, dst, float>;

template <class src, class dst>
using ScaleFactors2DDouble = BaseScaleFactors2D<src, dst, double>;

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_SCALEFACTORS2D_H_ */
