/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_ZOOM_SCALE_H_
#define MOZILLA_GFX_ZOOM_SCALE_H_

#include "Point.h"
#include "Rect.h"

namespace mozilla {
namespace gfx {

struct ZoomScale :
  public BaseSize<Float, ZoomScale> {
  typedef BaseSize<Float, ZoomScale> Super;

  ZoomScale() : Super(1, 1) {}
  ZoomScale(Float aWidth, Float aHeight) : Super(aWidth, aHeight) {}
};


inline Point
operator*(const Point& aPoint, const ZoomScale& aSize)
{
  return Point(aPoint.x * aSize.width, aPoint.y * aSize.height);
}

inline Rect
operator*(const Rect& aValue, const ZoomScale& aScale)
{
  return Rect(aValue.x * aScale.width,
              aValue.y * aScale.height,
              aValue.width * aScale.width,
              aValue.height * aScale.height);
}

inline Point
operator/(const Point& aPoint, const ZoomScale& aSize)
{
  return Point(aPoint.x / aSize.width, aPoint.y / aSize.height);
}

inline ZoomScale
operator/(Float aValue, const ZoomScale& aSize)
{
  return ZoomScale(aValue / aSize.width, aValue / aSize.height);
}

inline Rect
operator/(const Rect& aValue, const ZoomScale& aScale)
{
  return Rect(aValue.x / aScale.width,
              aValue.y / aScale.height,
              aValue.width / aScale.width,
              aValue.height / aScale.height);
}

}
}

#endif /* MOZILLA_GFX_ZOOMSCALE_H_ */
