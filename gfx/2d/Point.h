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

/// Use this for parameters of functions to allow implicit conversions to
/// integer types but not floating point types.
/// We use this wrapper to prevent IntSize and IntPoint's constructors to
/// take foating point values as parameters, and not require their constructors
/// to have implementations for each permutation of integer types.
template<typename T>
struct IntParam {
  constexpr MOZ_IMPLICIT IntParam(char val) : value(val) {}
  constexpr MOZ_IMPLICIT IntParam(unsigned char val) : value(val) {}
  constexpr MOZ_IMPLICIT IntParam(short val) : value(val) {}
  constexpr MOZ_IMPLICIT IntParam(unsigned short val) : value(val) {}
  constexpr MOZ_IMPLICIT IntParam(int val) : value(val) {}
  constexpr MOZ_IMPLICIT IntParam(unsigned int val) : value(val) {}
  constexpr MOZ_IMPLICIT IntParam(long val) : value(val) {}
  constexpr MOZ_IMPLICIT IntParam(unsigned long val) : value(val) {}
  constexpr MOZ_IMPLICIT IntParam(long long val) : value(val) {}
  constexpr MOZ_IMPLICIT IntParam(unsigned long long val) : value(val) {}
  template<typename Unit>
  constexpr MOZ_IMPLICIT IntParam(IntCoordTyped<Unit> val) : value(val) {}

  // Disable the evil ones!
  MOZ_IMPLICIT IntParam(float val) = delete;
  MOZ_IMPLICIT IntParam(double val) = delete;

  T value;
};

template<class units, class> struct PointTyped;
template<class units, class> struct SizeTyped;

template<class units>
struct IntPointTyped :
  public BasePoint< int32_t, IntPointTyped<units>, IntCoordTyped<units> >,
  public units {
  static_assert(IsPixel<units>::value,
                "'units' must be a coordinate system tag");

  typedef IntParam<int32_t> ToInt;
  typedef IntCoordTyped<units> Coord;
  typedef BasePoint< int32_t, IntPointTyped<units>, IntCoordTyped<units> > Super;

  constexpr IntPointTyped() : Super() {}
  constexpr IntPointTyped(ToInt aX, ToInt aY) : Super(Coord(aX.value), Coord(aY.value)) {}

  static IntPointTyped<units> Round(float aX, float aY) {
    return IntPointTyped(int32_t(floorf(aX + 0.5)), int32_t(floorf(aY + 0.5)));
  }

  static IntPointTyped<units> Ceil(float aX, float aY) {
    return IntPointTyped(int32_t(ceil(aX)), int32_t(ceil(aY)));
  }

  static IntPointTyped<units> Floor(float aX, float aY) {
    return IntPointTyped(int32_t(floorf(aX)), int32_t(floorf(aY)));
  }

  static IntPointTyped<units> Truncate(float aX, float aY) {
    return IntPointTyped(int32_t(aX), int32_t(aY));
  }

  static IntPointTyped<units> Round(const PointTyped<units, float>& aPoint);
  static IntPointTyped<units> Ceil(const PointTyped<units, float>& aPoint);
  static IntPointTyped<units> Floor(const PointTyped<units, float>& aPoint);
  static IntPointTyped<units> Truncate(const PointTyped<units, float>& aPoint);

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

  constexpr PointTyped() : Super() {}
  constexpr PointTyped(F aX, F aY) : Super(Coord(aX), Coord(aY)) {}
  // The mixed-type constructors (Float, Coord) and (Coord, Float) are needed to
  // avoid ambiguities because Coord is implicitly convertible to Float.
  constexpr PointTyped(F aX, Coord aY) : Super(Coord(aX), aY) {}
  constexpr PointTyped(Coord aX, F aY) : Super(aX, Coord(aY)) {}
  constexpr PointTyped(Coord aX, Coord aY) : Super(aX.value, aY.value) {}
  constexpr MOZ_IMPLICIT PointTyped(const IntPointTyped<units>& point) : Super(F(point.x), F(point.y)) {}

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
  return IntPointTyped<units>::Round(aPoint.x, aPoint.y);
}

template<class units>
IntPointTyped<units> TruncatedToInt(const PointTyped<units>& aPoint) {
  return IntPointTyped<units>::Truncate(aPoint.x, aPoint.y);
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

template<typename units>
IntPointTyped<units>
IntPointTyped<units>::Round(const PointTyped<units, float>& aPoint)
{
  return IntPointTyped::Round(aPoint.x, aPoint.y);
}

template<typename units>
IntPointTyped<units>
IntPointTyped<units>::Ceil(const PointTyped<units, float>& aPoint)
{
  return IntPointTyped::Ceil(aPoint.x, aPoint.y);
}

template<typename units>
IntPointTyped<units>
IntPointTyped<units>::Floor(const PointTyped<units, float>& aPoint)
{
  return IntPointTyped::Floor(aPoint.x, aPoint.y);
}

template<typename units>
IntPointTyped<units>
IntPointTyped<units>::Truncate(const PointTyped<units, float>& aPoint)
{
  return IntPointTyped::Truncate(aPoint.x, aPoint.y);
}

template<class units, class F = Float>
struct Point4DTyped :
  public BasePoint4D< F, Point4DTyped<units, F> > {
  static_assert(IsPixel<units>::value,
                "'units' must be a coordinate system tag");

  typedef BasePoint4D< F, Point4DTyped<units, F> > Super;

  Point4DTyped() : Super() {}
  Point4DTyped(F aX, F aY, F aZ, F aW) : Super(aX, aY, aZ, aW) {}

  explicit Point4DTyped(const Point3DTyped<units, F>& aPoint)
    : Super(aPoint.x, aPoint.y, aPoint.z, 1) {}

  // XXX When all of the code is ported, the following functions to convert to and from
  // unknown types should be removed.

  static Point4DTyped<units, F> FromUnknownPoint(const Point4DTyped<UnknownUnits, F>& aPoint) {
    return Point4DTyped<units, F>(aPoint.x, aPoint.y, aPoint.z, aPoint.w);
  }

  Point4DTyped<UnknownUnits, F> ToUnknownPoint() const {
    return Point4DTyped<UnknownUnits, F>(this->x, this->y, this->z, this->w);
  }

  PointTyped<units, F> As2DPoint() const {
    return PointTyped<units, F>(this->x / this->w,
                                this->y / this->w);
  }

  Point3DTyped<units, F> As3DPoint() const {
    return Point3DTyped<units, F>(this->x / this->w,
                                  this->y / this->w,
                                  this->z / this->w);
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

  typedef IntParam<int32_t> ToInt;
  typedef BaseSize< int32_t, IntSizeTyped<units> > Super;

  constexpr IntSizeTyped() : Super() {}
  constexpr IntSizeTyped(ToInt aWidth, ToInt aHeight) : Super(aWidth.value, aHeight.value) {}

  static IntSizeTyped<units> Round(float aWidth, float aHeight) {
    return IntSizeTyped(int32_t(floorf(aWidth + 0.5)), int32_t(floorf(aHeight + 0.5)));
  }

  static IntSizeTyped<units> Truncate(float aWidth, float aHeight) {
    return IntSizeTyped(int32_t(aWidth), int32_t(aHeight));
  }

  static IntSizeTyped<units> Ceil(float aWidth, float aHeight) {
    return IntSizeTyped(int32_t(ceil(aWidth)), int32_t(ceil(aHeight)));
  }

  static IntSizeTyped<units> Floor(float aWidth, float aHeight) {
    return IntSizeTyped(int32_t(floorf(aWidth)), int32_t(floorf(aHeight)));
  }

  static IntSizeTyped<units> Round(const SizeTyped<units, float>& aSize);
  static IntSizeTyped<units> Ceil(const SizeTyped<units, float>& aSize);
  static IntSizeTyped<units> Floor(const SizeTyped<units, float>& aSize);
  static IntSizeTyped<units> Truncate(const SizeTyped<units, float>& aSize);

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
  public BaseSize< F, SizeTyped<units, F> >,
  public units {
  static_assert(IsPixel<units>::value,
                "'units' must be a coordinate system tag");

  typedef BaseSize< F, SizeTyped<units, F> > Super;

  constexpr SizeTyped() : Super() {}
  constexpr SizeTyped(F aWidth, F aHeight) : Super(aWidth, aHeight) {}
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

template<typename units> IntSizeTyped<units>
IntSizeTyped<units>::Round(const SizeTyped<units, float>& aSize) {
  return IntSizeTyped::Round(aSize.width, aSize.height);
}

template<typename units> IntSizeTyped<units>
IntSizeTyped<units>::Ceil(const SizeTyped<units, float>& aSize) {
  return IntSizeTyped::Ceil(aSize.width, aSize.height);
}

template<typename units> IntSizeTyped<units>
IntSizeTyped<units>::Floor(const SizeTyped<units, float>& aSize) {
  return IntSizeTyped::Floor(aSize.width, aSize.height);
}

template<typename units> IntSizeTyped<units>
IntSizeTyped<units>::Truncate(const SizeTyped<units, float>& aSize) {
  return IntSizeTyped::Truncate(aSize.width, aSize.height);
}

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_POINT_H_ */
