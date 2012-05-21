/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_BASEPOINT4D_H_
#define MOZILLA_BASEPOINT4D_H_

namespace mozilla {
namespace gfx {

/**
 * Do not use this class directly. Subclass it, pass that subclass as the
 * Sub parameter, and only use that subclass. This allows methods to safely
 * cast 'this' to 'Sub*'.
 */
template <class T, class Sub>
struct BasePoint4D {
  T x, y, z, w;

  // Constructors
  BasePoint4D() : x(0), y(0), z(0), w(0) {}
  BasePoint4D(T aX, T aY, T aZ, T aW) : x(aX), y(aY), z(aZ), w(aW) {}

  void MoveTo(T aX, T aY, T aZ, T aW) { x = aX; y = aY; z = aZ; w = aW; }
  void MoveBy(T aDx, T aDy, T aDz, T aDw) { x += aDx; y += aDy; z += aDz; w += aDw; }

  // Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator

  bool operator==(const Sub& aPoint) const {
    return x == aPoint.x && y == aPoint.y && 
           z == aPoint.z && w == aPoint.w;
  }
  bool operator!=(const Sub& aPoint) const {
    return x != aPoint.x || y != aPoint.y || 
           z != aPoint.z || w != aPoint.w;
  }

  Sub operator+(const Sub& aPoint) const {
    return Sub(x + aPoint.x, y + aPoint.y, z + aPoint.z, w + aPoint.w);
  }
  Sub operator-(const Sub& aPoint) const {
    return Sub(x - aPoint.x, y - aPoint.y, z - aPoint.z, w - aPoint.w);
  }
  Sub& operator+=(const Sub& aPoint) {
    x += aPoint.x;
    y += aPoint.y;
    z += aPoint.z;
    w += aPoint.w;
    return *static_cast<Sub*>(this);
  }
  Sub& operator-=(const Sub& aPoint) {
    x -= aPoint.x;
    y -= aPoint.y;
    z -= aPoint.z;
    w -= aPoint.w;
    return *static_cast<Sub*>(this);
  }

  Sub operator*(T aScale) const {
    return Sub(x * aScale, y * aScale, z * aScale, w * aScale);
  }
  Sub operator/(T aScale) const {
    return Sub(x / aScale, y / aScale, z / aScale, w / aScale);
  }

  Sub& operator*=(T aScale) {
    x *= aScale;
    y *= aScale;
    z *= aScale;
    w *= aScale;
    return *static_cast<Sub*>(this);
  }

  Sub& operator/=(T aScale) {
    x /= aScale;
    y /= aScale;
    z /= aScale;
    w /= aScale;
    return *static_cast<Sub*>(this);
  }

  Sub operator-() const {
    return Sub(-x, -y, -z, -w);
  }

  T& operator[](int aIndex) {
    NS_ABORT_IF_FALSE(aIndex >= 0 && aIndex <= 3, "Invalid array index");
    return *((&x)+aIndex);
  }

  const T& operator[](int aIndex) const {
    NS_ABORT_IF_FALSE(aIndex >= 0 && aIndex <= 3, "Invalid array index");
    return *((&x)+aIndex);
  }

  T DotProduct(const Sub& aPoint) const {
    return x * aPoint.x + y * aPoint.y + z * aPoint.z + w * aPoint.w;
  }

  // Ignores the 4th component!
  Sub CrossProduct(const Sub& aPoint) const {
      return Sub(y * aPoint.z - aPoint.y * z,
          z * aPoint.x - aPoint.z * x,
          x * aPoint.y - aPoint.x * y, 
          0);
  }

  T Length() const {
    return sqrt(x*x + y*y + z*z + w*w);
  }

  void Normalize() {
    *this /= Length();
  }
};

}
}

#endif /* MOZILLA_BASEPOINT4D_H_ */
