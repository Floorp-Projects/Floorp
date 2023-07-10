/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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
#include "mozilla/Maybe.h"
#include "mozilla/gfx/NumericTools.h"

#include <cmath>
#include <type_traits>

namespace mozilla {

template <typename>
struct IsPixel;

template <>
struct IsPixel<gfx::UnknownUnits> : std::true_type {};

namespace gfx {

/// Use this for parameters of functions to allow implicit conversions to
/// integer types but not floating point types.
/// We use this wrapper to prevent IntSize and IntPoint's constructors to
/// take foating point values as parameters, and not require their constructors
/// to have implementations for each permutation of integer types.
template <typename T>
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
  template <typename Unit>
  constexpr MOZ_IMPLICIT IntParam(IntCoordTyped<Unit> val) : value(val) {}

  // Disable the evil ones!
  MOZ_IMPLICIT IntParam(float val) = delete;
  MOZ_IMPLICIT IntParam(double val) = delete;

  T value;
};

template <class Units, class>
struct PointTyped;
template <class Units, class>
struct SizeTyped;

template <class Units>
struct MOZ_EMPTY_BASES IntPointTyped
    : public BasePoint<int32_t, IntPointTyped<Units>, IntCoordTyped<Units> >,
      public Units {
  static_assert(IsPixel<Units>::value,
                "'Units' must be a coordinate system tag");

  typedef IntParam<int32_t> ToInt;
  typedef IntCoordTyped<Units> Coord;
  typedef BasePoint<int32_t, IntPointTyped<Units>, IntCoordTyped<Units> > Super;

  constexpr IntPointTyped() : Super() {
    static_assert(sizeof(IntPointTyped) == sizeof(int32_t) * 2,
                  "Would be unfortunate otherwise!");
  }
  constexpr IntPointTyped(ToInt aX, ToInt aY)
      : Super(Coord(aX.value), Coord(aY.value)) {}

  static IntPointTyped Round(float aX, float aY) {
    return IntPointTyped(int32_t(floorf(aX + 0.5f)),
                         int32_t(floorf(aY + 0.5f)));
  }

  static IntPointTyped Ceil(float aX, float aY) {
    return IntPointTyped(int32_t(ceilf(aX)), int32_t(ceilf(aY)));
  }

  static IntPointTyped Floor(float aX, float aY) {
    return IntPointTyped(int32_t(floorf(aX)), int32_t(floorf(aY)));
  }

  static IntPointTyped Truncate(float aX, float aY) {
    return IntPointTyped(int32_t(aX), int32_t(aY));
  }

  static IntPointTyped Round(const PointTyped<Units, float>& aPoint);
  static IntPointTyped Ceil(const PointTyped<Units, float>& aPoint);
  static IntPointTyped Floor(const PointTyped<Units, float>& aPoint);
  static IntPointTyped Truncate(const PointTyped<Units, float>& aPoint);

  // XXX When all of the code is ported, the following functions to convert to
  // and from unknown types should be removed.

  static IntPointTyped FromUnknownPoint(
      const IntPointTyped<UnknownUnits>& aPoint) {
    return IntPointTyped<Units>(aPoint.x, aPoint.y);
  }

  IntPointTyped<UnknownUnits> ToUnknownPoint() const {
    return IntPointTyped<UnknownUnits>(this->x, this->y);
  }

  IntPointTyped RoundedToMultiple(int32_t aMultiplier) const {
    return {RoundToMultiple(this->x, aMultiplier),
            RoundToMultiple(this->y, aMultiplier)};
  }
};
typedef IntPointTyped<UnknownUnits> IntPoint;

template <class Units, class F = Float>
struct MOZ_EMPTY_BASES PointTyped
    : public BasePoint<F, PointTyped<Units, F>, CoordTyped<Units, F> >,
      public Units {
  static_assert(IsPixel<Units>::value,
                "'Units' must be a coordinate system tag");

  typedef CoordTyped<Units, F> Coord;
  typedef BasePoint<F, PointTyped<Units, F>, CoordTyped<Units, F> > Super;

  constexpr PointTyped() : Super() {
    static_assert(sizeof(PointTyped) == sizeof(F) * 2,
                  "Would be unfortunate otherwise!");
  }
  constexpr PointTyped(F aX, F aY) : Super(Coord(aX), Coord(aY)) {}
  // The mixed-type constructors (Float, Coord) and (Coord, Float) are needed to
  // avoid ambiguities because Coord is implicitly convertible to Float.
  constexpr PointTyped(F aX, Coord aY) : Super(Coord(aX), aY) {}
  constexpr PointTyped(Coord aX, F aY) : Super(aX, Coord(aY)) {}
  constexpr PointTyped(Coord aX, Coord aY) : Super(aX.value, aY.value) {}
  constexpr MOZ_IMPLICIT PointTyped(const IntPointTyped<Units>& point)
      : Super(F(point.x), F(point.y)) {}

  bool WithinEpsilonOf(const PointTyped<Units, F>& aPoint, F aEpsilon) const {
    return fabs(aPoint.x - this->x) < aEpsilon &&
           fabs(aPoint.y - this->y) < aEpsilon;
  }

  // XXX When all of the code is ported, the following functions to convert to
  // and from unknown types should be removed.

  static PointTyped<Units, F> FromUnknownPoint(
      const PointTyped<UnknownUnits, F>& aPoint) {
    return PointTyped<Units, F>(aPoint.x, aPoint.y);
  }

  PointTyped<UnknownUnits, F> ToUnknownPoint() const {
    return PointTyped<UnknownUnits, F>(this->x, this->y);
  }
};
typedef PointTyped<UnknownUnits> Point;
typedef PointTyped<UnknownUnits, double> PointDouble;

template <class Units>
IntPointTyped<Units> RoundedToInt(const PointTyped<Units>& aPoint) {
  return IntPointTyped<Units>::Round(aPoint.x, aPoint.y);
}

template <class Units>
IntPointTyped<Units> TruncatedToInt(const PointTyped<Units>& aPoint) {
  return IntPointTyped<Units>::Truncate(aPoint.x, aPoint.y);
}

template <class Units, class F = Float>
struct Point3DTyped : public BasePoint3D<F, Point3DTyped<Units, F> > {
  static_assert(IsPixel<Units>::value,
                "'Units' must be a coordinate system tag");

  typedef BasePoint3D<F, Point3DTyped<Units, F> > Super;

  Point3DTyped() : Super() {
    static_assert(sizeof(Point3DTyped) == sizeof(F) * 3,
                  "Would be unfortunate otherwise!");
  }
  Point3DTyped(F aX, F aY, F aZ) : Super(aX, aY, aZ) {}

  // XXX When all of the code is ported, the following functions to convert to
  // and from unknown types should be removed.

  static Point3DTyped<Units, F> FromUnknownPoint(
      const Point3DTyped<UnknownUnits, F>& aPoint) {
    return Point3DTyped<Units, F>(aPoint.x, aPoint.y, aPoint.z);
  }

  Point3DTyped<UnknownUnits, F> ToUnknownPoint() const {
    return Point3DTyped<UnknownUnits, F>(this->x, this->y, this->z);
  }
};
typedef Point3DTyped<UnknownUnits> Point3D;
typedef Point3DTyped<UnknownUnits, double> PointDouble3D;

template <typename Units>
IntPointTyped<Units> IntPointTyped<Units>::Round(
    const PointTyped<Units, float>& aPoint) {
  return IntPointTyped::Round(aPoint.x, aPoint.y);
}

template <typename Units>
IntPointTyped<Units> IntPointTyped<Units>::Ceil(
    const PointTyped<Units, float>& aPoint) {
  return IntPointTyped::Ceil(aPoint.x, aPoint.y);
}

template <typename Units>
IntPointTyped<Units> IntPointTyped<Units>::Floor(
    const PointTyped<Units, float>& aPoint) {
  return IntPointTyped::Floor(aPoint.x, aPoint.y);
}

template <typename Units>
IntPointTyped<Units> IntPointTyped<Units>::Truncate(
    const PointTyped<Units, float>& aPoint) {
  return IntPointTyped::Truncate(aPoint.x, aPoint.y);
}

template <class Units, class F = Float>
struct Point4DTyped : public BasePoint4D<F, Point4DTyped<Units, F> > {
  static_assert(IsPixel<Units>::value,
                "'Units' must be a coordinate system tag");

  typedef BasePoint4D<F, Point4DTyped<Units, F> > Super;

  Point4DTyped() : Super() {
    static_assert(sizeof(Point4DTyped) == sizeof(F) * 4,
                  "Would be unfortunate otherwise!");
  }
  Point4DTyped(F aX, F aY, F aZ, F aW) : Super(aX, aY, aZ, aW) {}

  explicit Point4DTyped(const Point3DTyped<Units, F>& aPoint)
      : Super(aPoint.x, aPoint.y, aPoint.z, 1) {}

  // XXX When all of the code is ported, the following functions to convert to
  // and from unknown types should be removed.

  static Point4DTyped<Units, F> FromUnknownPoint(
      const Point4DTyped<UnknownUnits, F>& aPoint) {
    return Point4DTyped<Units, F>(aPoint.x, aPoint.y, aPoint.z, aPoint.w);
  }

  Point4DTyped<UnknownUnits, F> ToUnknownPoint() const {
    return Point4DTyped<UnknownUnits, F>(this->x, this->y, this->z, this->w);
  }

  PointTyped<Units, F> As2DPoint() const {
    return PointTyped<Units, F>(this->x / this->w, this->y / this->w);
  }

  Point3DTyped<Units, F> As3DPoint() const {
    return Point3DTyped<Units, F>(this->x / this->w, this->y / this->w,
                                  this->z / this->w);
  }
};
typedef Point4DTyped<UnknownUnits> Point4D;
typedef Point4DTyped<UnknownUnits, double> PointDouble4D;

template <class Units>
struct MOZ_EMPTY_BASES IntSizeTyped
    : public BaseSize<int32_t, IntSizeTyped<Units>, IntCoordTyped<Units> >,
      public Units {
  static_assert(IsPixel<Units>::value,
                "'Units' must be a coordinate system tag");

  typedef IntCoordTyped<Units> Coord;
  typedef BaseSize<int32_t, IntSizeTyped<Units>, Coord> Super;

  constexpr IntSizeTyped() : Super() {
    static_assert(sizeof(IntSizeTyped) == sizeof(int32_t) * 2,
                  "Would be unfortunate otherwise!");
  }
  constexpr IntSizeTyped(Coord aWidth, Coord aHeight)
      : Super(aWidth.value, aHeight.value) {}

  static IntSizeTyped Round(float aWidth, float aHeight) {
    return IntSizeTyped(int32_t(floorf(aWidth + 0.5)),
                        int32_t(floorf(aHeight + 0.5)));
  }

  static IntSizeTyped Truncate(float aWidth, float aHeight) {
    return IntSizeTyped(int32_t(aWidth), int32_t(aHeight));
  }

  static IntSizeTyped Ceil(float aWidth, float aHeight) {
    return IntSizeTyped(int32_t(ceil(aWidth)), int32_t(ceil(aHeight)));
  }

  static IntSizeTyped Floor(float aWidth, float aHeight) {
    return IntSizeTyped(int32_t(floorf(aWidth)), int32_t(floorf(aHeight)));
  }

  static IntSizeTyped Round(const SizeTyped<Units, float>& aSize);
  static IntSizeTyped Ceil(const SizeTyped<Units, float>& aSize);
  static IntSizeTyped Floor(const SizeTyped<Units, float>& aSize);
  static IntSizeTyped Truncate(const SizeTyped<Units, float>& aSize);

  IntSizeTyped TruncatedToMultiple(int32_t aMultiplier) const {
    if (aMultiplier == 1) {
      return *this;
    }
    return {RoundDownToMultiple(this->width, aMultiplier),
            RoundDownToMultiple(this->height, aMultiplier)};
  }

  IntSizeTyped CeiledToMultiple(int32_t aMultiplier) const {
    if (aMultiplier == 1) {
      return *this;
    }
    return {RoundUpToMultiple(this->width, aMultiplier),
            RoundUpToMultiple(this->height, aMultiplier)};
  }

  // XXX When all of the code is ported, the following functions to convert to
  // and from unknown types should be removed.

  static IntSizeTyped FromUnknownSize(const IntSizeTyped<UnknownUnits>& aSize) {
    return IntSizeTyped(aSize.width, aSize.height);
  }

  IntSizeTyped<UnknownUnits> ToUnknownSize() const {
    return IntSizeTyped<UnknownUnits>(this->width, this->height);
  }
};
typedef IntSizeTyped<UnknownUnits> IntSize;
typedef Maybe<IntSize> MaybeIntSize;

template <class Units, class F = Float>
struct MOZ_EMPTY_BASES SizeTyped
    : public BaseSize<F, SizeTyped<Units, F>, CoordTyped<Units, F> >,
      public Units {
  static_assert(IsPixel<Units>::value,
                "'Units' must be a coordinate system tag");

  typedef CoordTyped<Units, F> Coord;
  typedef BaseSize<F, SizeTyped<Units, F>, Coord> Super;

  constexpr SizeTyped() : Super() {
    static_assert(sizeof(SizeTyped) == sizeof(F) * 2,
                  "Would be unfortunate otherwise!");
  }
  constexpr SizeTyped(Coord aWidth, Coord aHeight) : Super(aWidth, aHeight) {}
  explicit SizeTyped(const IntSizeTyped<Units>& size)
      : Super(F(size.width), F(size.height)) {}

  // XXX When all of the code is ported, the following functions to convert to
  // and from unknown types should be removed.

  static SizeTyped<Units, F> FromUnknownSize(
      const SizeTyped<UnknownUnits, F>& aSize) {
    return SizeTyped<Units, F>(aSize.width, aSize.height);
  }

  SizeTyped<UnknownUnits, F> ToUnknownSize() const {
    return SizeTyped<UnknownUnits, F>(this->width, this->height);
  }
};
typedef SizeTyped<UnknownUnits> Size;
typedef SizeTyped<UnknownUnits, double> SizeDouble;

template <class Units>
IntSizeTyped<Units> RoundedToInt(const SizeTyped<Units>& aSize) {
  return IntSizeTyped<Units>(int32_t(floorf(aSize.width + 0.5f)),
                             int32_t(floorf(aSize.height + 0.5f)));
}

template <typename Units>
IntSizeTyped<Units> IntSizeTyped<Units>::Round(
    const SizeTyped<Units, float>& aSize) {
  return IntSizeTyped::Round(aSize.width, aSize.height);
}

template <typename Units>
IntSizeTyped<Units> IntSizeTyped<Units>::Ceil(
    const SizeTyped<Units, float>& aSize) {
  return IntSizeTyped::Ceil(aSize.width, aSize.height);
}

template <typename Units>
IntSizeTyped<Units> IntSizeTyped<Units>::Floor(
    const SizeTyped<Units, float>& aSize) {
  return IntSizeTyped::Floor(aSize.width, aSize.height);
}

template <typename Units>
IntSizeTyped<Units> IntSizeTyped<Units>::Truncate(
    const SizeTyped<Units, float>& aSize) {
  return IntSizeTyped::Truncate(aSize.width, aSize.height);
}

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_POINT_H_ */
