/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_POINT_H_
#define MOZILLA_GFX_POINT_H_

#include "mozilla/Attributes.h"
#include "Types.h"
#include "Coord.h"
#include "BaseCoord.h"
#include "BasePoint.h"
#include "BasePoint3D.h"
#include "BasePoint4D.h"
#include "BaseSize.h"
#include "mozilla/TypeTraits.h"

#include <cmath>

namespace mozilla {

template <typename> struct IsPixel;

namespace gfx {

// This should only be used by the typedefs below.
struct UnknownUnits {};

} // namespace gfx

template<> struct IsPixel<gfx::UnknownUnits> : TrueType {};

namespace gfx {

template<class units>
struct IntPointTyped :
  public BasePoint< int32_t, IntPointTyped<units>, IntCoordTyped<units> >,
  public units {
  static_assert(IsPixel<units>::value,
                "'units' must be a coordinate system tag");

  typedef IntCoordTyped<units> Coord;
  typedef BasePoint< int32_t, IntPointTyped<units>, IntCoordTyped<units> > Super;

  MOZ_CONSTEXPR IntPointTyped() : Super() {}
  MOZ_CONSTEXPR IntPointTyped(int32_t aX, int32_t aY) : Super(Coord(aX), Coord(aY)) {}
  // The mixed-type constructors (int, Coord) and (Coord, int) are needed to
  // avoid ambiguities because Coord is implicitly convertible to int.
  MOZ_CONSTEXPR IntPointTyped(int32_t aX, Coord aY) : Super(Coord(aX), aY) {}
  MOZ_CONSTEXPR IntPointTyped(Coord aX, int32_t aY) : Super(aX, Coord(aY)) {}
  MOZ_CONSTEXPR IntPointTyped(Coord aX, Coord aY) : Super(aX, aY) {}

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

template<class units, class F = Float>
struct PointTyped :
  public BasePoint< F, PointTyped<units, F>, CoordTyped<units, F> >,
  public units {
  static_assert(IsPixel<units>::value,
                "'units' must be a coordinate system tag");

  typedef CoordTyped<units, F> Coord;
  typedef BasePoint< F, PointTyped<units, F>, CoordTyped<units, F> > Super;

  MOZ_CONSTEXPR PointTyped() : Super() {}
  MOZ_CONSTEXPR PointTyped(F aX, F aY) : Super(Coord(aX), Coord(aY)) {}
  // The mixed-type constructors (Float, Coord) and (Coord, Float) are needed to
  // avoid ambiguities because Coord is implicitly convertible to Float.
  MOZ_CONSTEXPR PointTyped(F aX, Coord aY) : Super(Coord(aX), aY) {}
  MOZ_CONSTEXPR PointTyped(Coord aX, F aY) : Super(aX, Coord(aY)) {}
  MOZ_CONSTEXPR PointTyped(Coord aX, Coord aY) : Super(aX.value, aY.value) {}
  MOZ_CONSTEXPR MOZ_IMPLICIT PointTyped(const IntPointTyped<units>& point) : Super(F(point.x), F(point.y)) {}

  // XXX When all of the code is ported, the following functions to convert to and from
  // unknown types should be removed.

  static PointTyped<units, F> FromUnknownPoint(const PointTyped<UnknownUnits, F>& aPoint) {
    return PointTyped<units, F>(aPoint.x, aPoint.y);
  }

  PointTyped<UnknownUnits, F> ToUnknownPoint() const {
    return PointTyped<UnknownUnits, F>(this->x, this->y);
  }
};
typedef PointTyped<UnknownUnits> Point;
typedef PointTyped<UnknownUnits, double> PointDouble;

template<class units>
IntPointTyped<units> RoundedToInt(const PointTyped<units>& aPoint) {
  return IntPointTyped<units>(int32_t(floorf(aPoint.x + 0.5f)),
                              int32_t(floorf(aPoint.y + 0.5f)));
}

template<class units>
IntPointTyped<units> TruncatedToInt(const PointTyped<units>& aPoint) {
  return IntPointTyped<units>(int32_t(aPoint.x),
                              int32_t(aPoint.y));
}

template<class units, class F = Float>
struct Point3DTyped :
  public BasePoint3D< F, Point3DTyped<units, F> > {
  static_assert(IsPixel<units>::value,
                "'units' must be a coordinate system tag");

  typedef BasePoint3D< F, Point3DTyped<units, F> > Super;

  Point3DTyped() : Super() {}
  Point3DTyped(F aX, F aY, F aZ) : Super(aX, aY, aZ) {}

  // XXX When all of the code is ported, the following functions to convert to and from
  // unknown types should be removed.

  static Point3DTyped<units, F> FromUnknownPoint(const Point3DTyped<UnknownUnits, F>& aPoint) {
    return Point3DTyped<units, F>(aPoint.x, aPoint.y, aPoint.z);
  }

  Point3DTyped<UnknownUnits, F> ToUnknownPoint() const {
    return Point3DTyped<UnknownUnits, F>(this->x, this->y, this->z);
  }
};
typedef Point3DTyped<UnknownUnits> Point3D;
typedef Point3DTyped<UnknownUnits, double> PointDouble3D;

template<class units, class F = Float>
struct Point4DTyped :
  public BasePoint4D< F, Point4DTyped<units, F> > {
  static_assert(IsPixel<units>::value,
                "'units' must be a coordinate system tag");

  typedef BasePoint4D< F, Point4DTyped<units, F> > Super;

  Point4DTyped() : Super() {}
  Point4DTyped(F aX, F aY, F aZ, F aW) : Super(aX, aY, aZ, aW) {}

  // XXX When all of the code is ported, the following functions to convert to and from
  // unknown types should be removed.

  static Point4DTyped<units, F> FromUnknownPoint(const Point4DTyped<UnknownUnits, F>& aPoint) {
    return Point4DTyped<units, F>(aPoint.x, aPoint.y, aPoint.z, aPoint.w);
  }

  Point4DTyped<UnknownUnits, F> ToUnknownPoint() const {
    return Point4DTyped<UnknownUnits, F>(this->x, this->y, this->z, this->w);
  }

  PointTyped<units, F> As2DPoint() {
    return PointTyped<units, F>(this->x / this->w, this->y / this->w);
  }
};
typedef Point4DTyped<UnknownUnits> Point4D;
typedef Point4DTyped<UnknownUnits, double> PointDouble4D;

template<class units>
struct IntSizeTyped :
  public BaseSize< int32_t, IntSizeTyped<units> >,
  public units {
  static_assert(IsPixel<units>::value,
                "'units' must be a coordinate system tag");

  typedef BaseSize< int32_t, IntSizeTyped<units> > Super;

  MOZ_CONSTEXPR IntSizeTyped() : Super() {}
  MOZ_CONSTEXPR IntSizeTyped(int32_t aWidth, int32_t aHeight) : Super(aWidth, aHeight) {}

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

template<class units, class F = Float>
struct SizeTyped :
  public BaseSize< F, SizeTyped<units> >,
  public units {
  static_assert(IsPixel<units>::value,
                "'units' must be a coordinate system tag");

  typedef BaseSize< F, SizeTyped<units, F> > Super;

  MOZ_CONSTEXPR SizeTyped() : Super() {}
  MOZ_CONSTEXPR SizeTyped(F aWidth, F aHeight) : Super(aWidth, aHeight) {}
  explicit SizeTyped(const IntSizeTyped<units>& size) :
    Super(F(size.width), F(size.height)) {}

  // XXX When all of the code is ported, the following functions to convert to and from
  // unknown types should be removed.

  static SizeTyped<units, F> FromUnknownSize(const SizeTyped<UnknownUnits, F>& aSize) {
    return SizeTyped<units, F>(aSize.width, aSize.height);
  }

  SizeTyped<UnknownUnits, F> ToUnknownSize() const {
    return SizeTyped<UnknownUnits, F>(this->width, this->height);
  }
};
typedef SizeTyped<UnknownUnits> Size;
typedef SizeTyped<UnknownUnits, double> SizeDouble;

template<class units>
IntSizeTyped<units> RoundedToInt(const SizeTyped<units>& aSize) {
  return IntSizeTyped<units>(int32_t(floorf(aSize.width + 0.5f)),
                             int32_t(floorf(aSize.height + 0.5f)));
}

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_POINT_H_ */
