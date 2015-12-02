/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_QUATERNION_H_
#define MOZILLA_GFX_QUATERNION_H_

#include "Types.h"
#include <math.h>
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/gfx/MatrixFwd.h"

namespace mozilla {
namespace gfx {

class Quaternion
{
public:
  Quaternion()
    : x(0.0f), y(0.0f), z(0.0f), w(1.0f)
  {}

  Quaternion(Float aX, Float aY, Float aZ, Float aW)
    : x(aX), y(aY), z(aZ), w(aW)
  {}


  Quaternion(const Quaternion& aOther)
  {
    memcpy(this, &aOther, sizeof(*this));
  }

  Float x, y, z, w;

  friend std::ostream& operator<<(std::ostream& aStream, const Quaternion& aQuat);

  void Set(Float aX, Float aY, Float aZ, Float aW)
  {
    x = aX; y = aY; z = aZ; w = aW;
  }

  // Assumes upper 3x3 of aMatrix is a pure rotation matrix (no scaling)
  void SetFromRotationMatrix(const Matrix4x4& aMatrix);

  // result = this * aQuat
  Quaternion operator*(const Quaternion &aQuat) const
  {
    Quaternion o;
    const Float bx = aQuat.x, by = aQuat.y, bz = aQuat.z, bw = aQuat.w;

    o.x = x*bw + w*bx + y*bz - z*by;
    o.y = y*bw + w*by + z*bx - x*bz;
    o.z = z*bw + w*bz + x*by - y*bx;
    o.w = w*bw - x*bx - y*by - z*bz;
    return o;
  }

  Quaternion& operator*=(const Quaternion &aQuat)
  {
    *this = *this * aQuat;
    return *this;
  }

  Float Length() const
  {
    return sqrt(x*x + y*y + z*z + w*w);
  }

  Quaternion& Conjugate()
  {
    x *= -1.f; y *= -1.f; z *= -1.f;
    return *this;
  }

  Quaternion& Normalize()
  {
    Float l = Length();
    if (l) {
      l = 1.0f / l;
      x *= l; y *= l; z *= l; w *= l;
    } else {
      x = y = z = 0.f;
      w = 1.f;
    }
    return *this;
  }

  Quaternion& Invert()
  {
    return Conjugate().Normalize();
  }

  Point3D RotatePoint(const Point3D& aPoint) {
    Float uvx = Float(2.0) * (y*aPoint.z - z*aPoint.y);
    Float uvy = Float(2.0) * (z*aPoint.x - x*aPoint.z);
    Float uvz = Float(2.0) * (x*aPoint.y - y*aPoint.x);

    return Point3D(aPoint.x + w*uvx + y*uvz - z*uvy,
                   aPoint.y + w*uvy + z*uvx - x*uvz,
                   aPoint.z + w*uvz + x*uvy - y*uvx);
  }
};

} // namespace gfx
} // namespace mozilla

#endif
