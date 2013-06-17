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

// This should only be used by the typedefs below.
struct UnknownUnits {};

template<class units>
struct IntPointTyped :
  public BasePoint< int32_t, IntPointTyped<units> >,
  public units {
  typedef BasePoint< int32_t, IntPointTyped<units> > Super;

  IntPointTyped() : Super() {}
  IntPointTyped(int32_t aX, int32_t aY) : Super(aX, aY) {}

  // XXX When all of the code is ported, the following functions to convert to and from
  // unknown types should be removed.

  static IntPointTyped<units> FromUnknownPoint(const IntPointTyped<UnknownUnits>& aPoint) {
    return IntPointTyped<units>(aPoint.x, aPoint.y);
  }

  IntPointTyped<UnknownUnits> ToUnknownPoint() const {
    return IntPointTyped<UnknownUnits>(this->x, this->y);
  }
};
typedef IntPointTyped<UnknownUnits> IntPoint;

template<class units>
struct PointTyped :
  public BasePoint< Float, PointTyped<units> >,
  public units {
  typedef BasePoint< Float, PointTyped<units> > Super;

  PointTyped() : Super() {}
  PointTyped(Float aX, Float aY) : Super(aX, aY) {}
  PointTyped(const IntPointTyped<units>& point) : Super(float(point.x), float(point.y)) {}

  // XXX When all of the code is ported, the following functions to convert to and from
  // unknown types should be removed.

  static PointTyped<units> FromUnknownPoint(const PointTyped<UnknownUnits>& aPoint) {
    return PointTyped<units>(aPoint.x, aPoint.y);
  }

  PointTyped<UnknownUnits> ToUnknownPoint() const {
    return PointTyped<UnknownUnits>(this->x, this->y);
  }
};
typedef PointTyped<UnknownUnits> Point;

template<class units>
IntPointTyped<units> RoundedToInt(const PointTyped<units>& aPoint) {
  return IntPointTyped<units>(NS_lround(aPoint.x),
                              NS_lround(aPoint.y));
}

template<class units>
struct IntSizeTyped :
  public BaseSize< int32_t, IntSizeTyped<units> >,
  public units {
  typedef BaseSize< int32_t, IntSizeTyped<units> > Super;

  IntSizeTyped() : Super() {}
  IntSizeTyped(int32_t aWidth, int32_t aHeight) : Super(aWidth, aHeight) {}

  // XXX When all of the code is ported, the following functions to convert to and from
  // unknown types should be removed.

  static IntSizeTyped<units> FromUnknownSize(const IntSizeTyped<UnknownUnits>& aSize) {
    return IntSizeTyped<units>(aSize.width, aSize.height);
  }

  IntSizeTyped<UnknownUnits> ToUnknownSize() const {
    return IntSizeTyped<UnknownUnits>(this->width, this->height);
  }
};
typedef IntSizeTyped<UnknownUnits> IntSize;

template<class units>
struct SizeTyped :
  public BaseSize< Float, SizeTyped<units> >,
  public units {
  typedef BaseSize< Float, SizeTyped<units> > Super;

  SizeTyped() : Super() {}
  SizeTyped(Float aWidth, Float aHeight) : Super(aWidth, aHeight) {}
  explicit SizeTyped(const IntSizeTyped<units>& size) :
    Super(float(size.width), float(size.height)) {}

  // XXX When all of the code is ported, the following functions to convert to and from
  // unknown types should be removed.

  static SizeTyped<units> FromUnknownSize(const SizeTyped<UnknownUnits>& aSize) {
    return SizeTyped<units>(aSize.width, aSize.height);
  }

  SizeTyped<UnknownUnits> ToUnknownSize() const {
    return SizeTyped<UnknownUnits>(this->width, this->height);
  }
};
typedef SizeTyped<UnknownUnits> Size;

}
}

#endif /* MOZILLA_GFX_POINT_H_ */
