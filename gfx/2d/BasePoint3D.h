/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_BASEPOINT3D_H_
#define MOZILLA_BASEPOINT3D_H_

namespace mozilla {
namespace gfx {

/**
 * Do not use this class directly. Subclass it, pass that subclass as the
 * Sub parameter, and only use that subclass. This allows methods to safely
 * cast 'this' to 'Sub*'.
 */
template <class T, class Sub>
struct BasePoint3D {
  T x, y, z;

  // Constructors
  BasePoint3D() : x(0), y(0), z(0) {}
  BasePoint3D(T aX, T aY, T aZ) : x(aX), y(aY), z(aZ) {}

  void MoveTo(T aX, T aY, T aZ) { x = aX; y = aY; z = aZ; }
  void MoveBy(T aDx, T aDy, T aDz) { x += aDx; y += aDy; z += aDz; }

  // Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator

  T& operator[](int aIndex) {
    NS_ABORT_IF_FALSE(aIndex >= 0 && aIndex <= 2, "Invalid array index");
    return *((&x)+aIndex);
  }

  const T& operator[](int aIndex) const {
    NS_ABORT_IF_FALSE(aIndex >= 0 && aIndex <= 2, "Invalid array index");
    return *((&x)+aIndex);
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

  Sub operator-() const {
    return Sub(-x, -y, -z);
  }

  Sub CrossProduct(const Sub& aPoint) const {
      return Sub(y * aPoint.z - aPoint.y * z,
                 z * aPoint.x - aPoint.z * x,
                 x * aPoint.y - aPoint.x * y);
  }

  T DotProduct(const Sub& aPoint) const {
      return x * aPoint.x + y * aPoint.y + z * aPoint.z;
  }

  T Length() const {
      return sqrt(x*x + y*y + z*z);
  }

  // Invalid for points with distance from origin of 0.
  void Normalize() {
      *this /= Length();
  }
};

}
}

#endif /* MOZILLA_BASEPOINT3D_H_ */
