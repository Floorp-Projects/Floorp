/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_DIRECTIONUTILS_H
#define GFX_DIRECTIONUTILS_H

#include "LayersTypes.h"  // for ScrollDirection
#include "Units.h"        // for Coord, Point, and Rect types

namespace mozilla {
namespace gfx {

using layers::ScrollDirection;

template <typename PointOrRect>
CoordOf<PointOrRect> GetAxisStart(ScrollDirection aDir, const PointOrRect& aValue) {
  if (aDir == ScrollDirection::HORIZONTAL) {
    return aValue.x;
  } else {
    return aValue.y;
  }
}

template <typename Rect>
CoordOf<Rect> GetAxisEnd(ScrollDirection aDir, const Rect& aValue) {
  if (aDir == ScrollDirection::HORIZONTAL) {
    return aValue.x + aValue.width;
  } else {
    return aValue.y + aValue.height;
  }
}

template <typename Rect>
CoordOf<Rect> GetAxisLength(ScrollDirection aDir, const Rect& aValue) {
  if (aDir == ScrollDirection::HORIZONTAL) {
    return aValue.width;
  } else {
    return aValue.height;
  }
}

template <typename FromUnits, typename ToUnits>
float GetAxisScale(ScrollDirection aDir, const ScaleFactors2D<FromUnits, ToUnits>& aValue) {
  if (aDir == ScrollDirection::HORIZONTAL) {
    return aValue.xScale;
  } else {
    return aValue.yScale;
  }
}

inline ScrollDirection GetPerpendicularDirection(ScrollDirection aDir) {
  return aDir == ScrollDirection::HORIZONTAL
       ? ScrollDirection::VERTICAL
       : ScrollDirection::HORIZONTAL;
}

} // namespace layers
} // namespace mozilla

#endif /* GFX_DIRECTIONUTILS_H */
