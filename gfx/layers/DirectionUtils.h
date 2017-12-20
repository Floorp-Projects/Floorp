/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_DIRECTIONUTILS_H
#define GFX_DIRECTIONUTILS_H

#include "LayersTypes.h"  // for ScrollDirection
#include "Units.h"        // for Coord, Point, and Rect types

namespace mozilla {
namespace layers {

template <typename PointOrRect>
CoordOf<PointOrRect> GetAxisStart(ScrollDirection aDir, const PointOrRect& aValue) {
  if (aDir == ScrollDirection::eHorizontal) {
    return aValue.X();
  } else {
    return aValue.Y();
  }
}

template <typename Rect>
CoordOf<Rect> GetAxisEnd(ScrollDirection aDir, const Rect& aValue) {
  if (aDir == ScrollDirection::eHorizontal) {
    return aValue.XMost();
  } else {
    return aValue.YMost();
  }
}

template <typename Rect>
CoordOf<Rect> GetAxisLength(ScrollDirection aDir, const Rect& aValue) {
  if (aDir == ScrollDirection::eHorizontal) {
    return aValue.Width();
  } else {
    return aValue.Height();
  }
}

template <typename FromUnits, typename ToUnits>
float GetAxisScale(ScrollDirection aDir, const gfx::ScaleFactors2D<FromUnits, ToUnits>& aValue) {
  if (aDir == ScrollDirection::eHorizontal) {
    return aValue.xScale;
  } else {
    return aValue.yScale;
  }
}

inline ScrollDirection GetPerpendicularDirection(ScrollDirection aDir) {
  return aDir == ScrollDirection::eHorizontal
       ? ScrollDirection::eVertical
       : ScrollDirection::eHorizontal;
}

} // namespace layers
} // namespace mozilla

#endif /* GFX_DIRECTIONUTILS_H */
