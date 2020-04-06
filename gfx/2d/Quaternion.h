/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_QUATERNION_H_
#define MOZILLA_GFX_QUATERNION_H_

#include "Types.h"
#include <math.h>
#include <ostream>
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/gfx/MatrixFwd.h"
#include "mozilla/gfx/Point.h"

namespace mozilla {
namespace gfx {

template <class T>
class BaseQuaternion {
 public:
  BaseQuaternion() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}

  BaseQuaternion(T aX, T aY, T aZ, T aW) : x(aX), y(aY), z(aZ), w(aW) {}

  BaseQuaternion(const BaseQuaternion& aOther) {
    x = aOther.x;
    y = aOther.y;
    z = aOther.z;
    w = aOther.w;
  }

  T x, y, z, w;

  template <class U>
  friend std::ostream& operator<<(std::ostream& aStream,
                                  const BaseQuaternion<U>& aQuat);

  void Set(T aX, T aY, T aZ, T aW) {
    x = aX;
    y = aY;
    z = aZ;
    w = aW;
  }

  // Assumes upper 3x3 of aMatrix is a pure rotation matrix (no scaling)
  void SetFromRotationMatrix(
      const Matrix4x4Typed<UnknownUnits, UnknownUnits, T>& m) {
    // see
    // http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/index.htm
    const T trace = m._11 + m._22 + m._33;
    if (trace > 0.0) {
      const T s = 0.5f / sqrt(trace + 1.0f);
      w = 0.25f / s;
      x = (m._32 - m._23) * s;
      y = (m._13 - m._31) * s;
      z = (m._21 - m._12) * s;
    } else if (m._11 > m._22 && m._11 > m._33) {
      const T s = 2.0f * sqrt(1.0f + m._11 - m._22 - m._33);
      w = (m._32 - m._23) / s;
      x = 0.25f * s;
      y = (m._12 + m._21) / s;
      z = (m._13 + m._31) / s;
    } else if (m._22 > m._33) {
      const T s = 2.0 * sqrt(1.0f + m._22 - m._11 - m._33);
      w = (m._13 - m._31) / s;
      x = (m._12 + m._21) / s;
      y = 0.25f * s;
      z = (m._23 + m._32) / s;
    } else {
      const T s = 2.0 * sqrt(1.0f + m._33 - m._11 - m._22);
      w = (m._21 - m._12) / s;
      x = (m._13 + m._31) / s;
      y = (m._23 + m._32) / s;
      z = 0.25f * s;
    }
  }

  // result = this * aQuat
  BaseQuaternion operator*(const BaseQuaternion& aQuat) const {
    BaseQuaternion o;
    const T bx = aQuat.x, by = aQuat.y, bz = aQuat.z, bw = aQuat.w;

    o.x = x * bw + w * bx + y * bz - z * by;
    o.y = y * bw + w * by + z * bx - x * bz;
    o.z = z * bw + w * bz + x * by - y * bx;
    o.w = w * bw - x * bx - y * by - z * bz;
    return o;
  }

  BaseQuaternion& operator*=(const BaseQuaternion& aQuat) {
    *this = *this * aQuat;
    return *this;
  }

  T Length() const { return sqrt(x * x + y * y + z * z + w * w); }

  BaseQuaternion& Conjugate() {
    x *= -1.f;
    y *= -1.f;
    z *= -1.f;
    return *this;
  }

  BaseQuaternion& Normalize() {
    T l = Length();
    if (l) {
      l = 1.0f / l;
      x *= l;
      y *= l;
      z *= l;
      w *= l;
    } else {
      x = y = z = 0.f;
      w = 1.f;
    }
    return *this;
  }

  BaseQuaternion& Invert() { return Conjugate().Normalize(); }

  Point3DTyped<UnknownUnits, T> RotatePoint(
      const Point3DTyped<UnknownUnits, T>& aPoint) const {
    T uvx = T(2.0) * (y * aPoint.z - z * aPoint.y);
    T uvy = T(2.0) * (z * aPoint.x - x * aPoint.z);
    T uvz = T(2.0) * (x * aPoint.y - y * aPoint.x);

    return Point3DTyped<UnknownUnits, T>(
        aPoint.x + w * uvx + y * uvz - z * uvy,
        aPoint.y + w * uvy + z * uvx - x * uvz,
        aPoint.z + w * uvz + x * uvy - y * uvx);
  }
};

typedef BaseQuaternion<Float> Quaternion;
typedef BaseQuaternion<Double> QuaternionDouble;

}  // namespace gfx
}  // namespace mozilla

#endif
