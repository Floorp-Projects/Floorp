/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEFACTOR_H_
#define MOZILLA_GFX_SCALEFACTOR_H_

#include "gfxPoint.h"

namespace mozilla {
namespace gfx {

/*
 * This class represents a scaling factor between two different pixel unit
 * systems. This is effectively a type-safe float, intended to be used in
 * combination with the known-type instances of gfx::Point, gfx::Rect, etc.
 *
 * Note that some parts of the code that pre-date this class used separate
 * scaling factors for the x and y axes. However, at runtime these values
 * were always expected to be the same, so this class uses only one scale
 * factor for both axes. The two constructors that take two-axis scaling
 * factors check to ensure that this assertion holds.
 */
template<class src, class dst>
struct ScaleFactor {
  float scale;

  ScaleFactor() : scale(1.0) {}
  ScaleFactor(const ScaleFactor<src, dst>& aCopy) : scale(aCopy.scale) {}
  explicit ScaleFactor(float aScale) : scale(aScale) {}

  explicit ScaleFactor(float aX, float aY) : scale(aX) {
    MOZ_ASSERT(fabs(aX - aY) < 1e-6);
  }

  explicit ScaleFactor(gfxSize aScale) : scale(aScale.width) {
    MOZ_ASSERT(fabs(aScale.width - aScale.height) < 1e-6);
  }

  ScaleFactor<dst, src> Inverse() {
    return ScaleFactor<dst, src>(1 / scale);
  }

  bool operator==(const ScaleFactor<src, dst>& aOther) const {
    return scale == aOther.scale;
  }

  bool operator!=(const ScaleFactor<src, dst>& aOther) const {
    return !(*this == aOther);
  }

  template<class other>
  ScaleFactor<other, dst> operator/(const ScaleFactor<src, other>& aOther) const {
    return ScaleFactor<other, dst>(scale / aOther.scale);
  }

  template<class other>
  ScaleFactor<src, other> operator/(const ScaleFactor<other, dst>& aOther) const {
    return ScaleFactor<src, other>(scale / aOther.scale);
  }

  template<class other>
  ScaleFactor<src, other> operator*(const ScaleFactor<dst, other>& aOther) const {
    return ScaleFactor<src, other>(scale * aOther.scale);
  }

  template<class other>
  ScaleFactor<other, dst> operator*(const ScaleFactor<other, src>& aOther) const {
    return ScaleFactor<other, dst>(scale * aOther.scale);
  }
};

}
}

#endif /* MOZILLA_GFX_SCALEFACTOR_H_ */
