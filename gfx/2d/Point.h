/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_POINT_H_
#define MOZILLA_GFX_POINT_H_

#include "Types.h"
#include "BasePoint.h"
#include "BaseSize.h"

namespace mozilla {
namespace gfx {

struct IntPoint :
  public BasePoint<int32_t, IntPoint> {
  typedef BasePoint<int32_t, IntPoint> Super;

  IntPoint() : Super() {}
  IntPoint(int32_t aX, int32_t aY) : Super(aX, aY) {}
};

struct Point :
  public BasePoint<Float, Point> {
  typedef BasePoint<Float, Point> Super;

  Point() : Super() {}
  Point(Float aX, Float aY) : Super(aX, aY) {}
  Point(const IntPoint& point) : Super(float(point.x), float(point.y)) {}
};

struct IntSize :
  public BaseSize<int32_t, IntSize> {
  typedef BaseSize<int32_t, IntSize> Super;

  IntSize() : Super() {}
  IntSize(int32_t aWidth, int32_t aHeight) : Super(aWidth, aHeight) {}
};

struct Size :
  public BaseSize<Float, Size> {
  typedef BaseSize<Float, Size> Super;

  Size() : Super() {}
  Size(Float aWidth, Float aHeight) : Super(aWidth, aHeight) {}
  explicit Size(const IntSize& size) :
    Super(float(size.width), float(size.height)) {}
};

}
}

#endif /* MOZILLA_GFX_POINT_H_ */
