/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_BASEPOINT3D_H_
#define MOZILLA_BASEPOINT3D_H_

#include "mozilla/Assertions.h"

namespace mozilla {
namespace gfx {

/**
 * Do not use this class directly. Subclass it, pass that subclass as the
 * Sub parameter, and only use that subclass. This allows methods to safely
 * cast 'this' to 'Sub*'.
 */
template <class T, class Sub>
struct BasePoint3D {
  union {
    struct {
      T x, y, z;
    };
    T components[3];
  };

  // Constructors
  BasePoint3D() : x(0), y(0), z(0) {}
  BasePoint3D(T aX, T aY, T aZ) : x(aX), y(aY), z(aZ) {}

  void MoveTo(T aX, T aY, T aZ) {
    x = aX;
    y = aY;
    z = aZ;
  }
  void MoveBy(T aDx, T aDy, T aDz) {
    x += aDx;
    y += aDy;
    z += aDz;
  }

  // Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator

  T& operator[](int aIndex) {
    MOZ_ASSERT(aIndex >= 0 && aIndex <= 2);
    return *((&x) + aIndex);
  }

  const T& operator[](int aIndex) const {
    MOZ_ASSERT(aIndex >= 0 && aIndex <= 2);
    return *((&x) + aIndex);
  }

  bool operator==(const Sub& aPoint) const {
    return x == aPoint.x && y == aPoint.y && z == aPoint.z;
  }
  bool operator!=(const Sub& aPoint) const {
    return x != aPoint.x || y != aPoint.y || z != aPoint.z;
  }

  Sub operator+(const Sub& aPoint) const {
    return Sub(x + aPoint.x, y + aPoint.y, z + aPoint.z);
  }
  Sub operator-(const Sub& aPoint) const {
    return Sub(x - aPoint.x, y - aPoint.y, z - aPoint.z);
  }
  Sub& operator+=(const Sub& aPoint) {
    x += aPoint.x;
    y += aPoint.y;
    z += aPoint.z;
    return *static_cast<Sub*>(this);
  }
  Sub& operator-=(const Sub& aPoint) {
    x -= aPoint.x;
    y -= aPoint.y;
    z -= aPoint.z;
    return *static_cast<Sub*>(this);
  }

  Sub operator*(T aScale) const {
    return Sub(x * aScale, y * aScale, z * aScale);
  }
  Sub operator/(T aScale) const {
    return Sub(x / aScale, y / aScale, z / aScale);
  }

  Sub& operator*=(T aScale) {
    x *= aScale;
    y *= aScale;
    z *= aScale;
    return *static_cast<Sub*>(this);
  }

  Sub& operator/=(T aScale) {
    x /= aScale;
    y /= aScale;
    z /= aScale;
    return *static_cast<Sub*>(this);
  }

  Sub operator-() const { return Sub(-x, -y, -z); }

  Sub CrossProduct(const Sub& aPoint) const {
    return Sub(y * aPoint.z - aPoint.y * z, z * aPoint.x - aPoint.z * x,
               x * aPoint.y - aPoint.x * y);
  }

  T DotProduct(const Sub& aPoint) const {
    return x * aPoint.x + y * aPoint.y + z * aPoint.z;
  }

  T Length() const { return sqrt(x * x + y * y + z * z); }

  // Invalid for points with distance from origin of 0.
  void Normalize() { *this /= Length(); }

  void RobustNormalize() {
    // If the distance is infinite, we scale it by 1/(the maximum value of T)
    // before doing normalization, so we can avoid getting a zero point.
    T length = Length();
    if (mozilla::IsInfinite(length)) {
      *this /= std::numeric_limits<T>::max();
      length = Length();
    }

    *this /= length;
  }

  friend std::ostream& operator<<(std::ostream& stream,
                                  const BasePoint3D<T, Sub>& aPoint) {
    return stream << '(' << aPoint.x << ',' << aPoint.y << ',' << aPoint.z
                  << ')';
  }
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_BASEPOINT3D_H_ */
