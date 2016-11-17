/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_QUATERNION_H
#define GFX_QUATERNION_H

#include "mozilla/gfx/BasePoint4D.h"
#include "mozilla/gfx/Matrix.h"
#include "nsAlgorithm.h"
#include <algorithm>

struct gfxQuaternion : public mozilla::gfx::BasePoint4D<gfxFloat, gfxQuaternion> {
    typedef mozilla::gfx::BasePoint4D<gfxFloat, gfxQuaternion> Super;

    gfxQuaternion() : Super() {}
    gfxQuaternion(gfxFloat aX, gfxFloat aY, gfxFloat aZ, gfxFloat aW) : Super(aX, aY, aZ, aW) {}

    explicit gfxQuaternion(const mozilla::gfx::Matrix4x4& aMatrix) {
        w = 0.5 * sqrt(std::max(1 + aMatrix[0][0] + aMatrix[1][1] + aMatrix[2][2], 0.0f));
        x = 0.5 * sqrt(std::max(1 + aMatrix[0][0] - aMatrix[1][1] - aMatrix[2][2], 0.0f));
        y = 0.5 * sqrt(std::max(1 - aMatrix[0][0] + aMatrix[1][1] - aMatrix[2][2], 0.0f));
        z = 0.5 * sqrt(std::max(1 - aMatrix[0][0] - aMatrix[1][1] + aMatrix[2][2], 0.0f));

        if(aMatrix[2][1] > aMatrix[1][2])
            x = -x;
        if(aMatrix[0][2] > aMatrix[2][0])
            y = -y;
        if(aMatrix[1][0] > aMatrix[0][1])
            z = -z;
    }

    // Convert from |direction axis, angle| pair to gfxQuaternion.
    //
    // Reference:
    // https://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation
    //
    // if the direction axis is (x, y, z) = xi + yj + zk,
    // and the angle is |theta|, this formula can be done using
    // an extension of Euler's formula:
    //   q = cos(theta/2) + (xi + yj + zk)(sin(theta/2))
    //     = cos(theta/2) +
    //       x*sin(theta/2)i + y*sin(theta/2)j + z*sin(theta/2)k
    // Note: aDirection should be an unit vector and
    //       the unit of aAngle should be Radian.
    gfxQuaternion(const mozilla::gfx::Point3D &aDirection, gfxFloat aAngle) {
        MOZ_ASSERT(mozilla::gfx::FuzzyEqual(aDirection.Length(), 1.0f),
                   "aDirection should be an unit vector");
        x = aDirection.x * sin(aAngle/2.0);
        y = aDirection.y * sin(aAngle/2.0);
        z = aDirection.z * sin(aAngle/2.0);
        w = cos(aAngle/2.0);
    }

    gfxQuaternion Slerp(const gfxQuaternion &aOther, gfxFloat aCoeff) const {
        gfxFloat dot = mozilla::clamped(DotProduct(aOther), -1.0, 1.0);
        if (dot == 1.0) {
            return *this;
        }

        gfxFloat theta = acos(dot);
        gfxFloat rsintheta = 1/sqrt(1 - dot*dot);
        gfxFloat rightWeight = sin(aCoeff*theta)*rsintheta;

        gfxQuaternion left = *this;
        gfxQuaternion right = aOther;

        left *= cos(aCoeff*theta) - dot*rightWeight;
        right *= rightWeight;

        return left + right;
    }

    using Super::operator*=;

    // Quaternion multiplication
    // Reference:
    // https://en.wikipedia.org/wiki/Quaternion#Ordered_list_form
    //
    // (w1, x1, y1, z1)(w2, x2, y2, z2) = (w1w2 - x1x2 - y1y2 - z1z2,
    //                                     w1x2 + x1w2 + y1z2 - z1y2,
    //                                     w1y2 - x1z2 + y1w2 + z1x2,
    //                                     w1z2 + x1y2 - y1x2 + z1w2)
    gfxQuaternion operator*(const gfxQuaternion& aOther) const {
        return gfxQuaternion(
          w * aOther.x + x * aOther.w + y * aOther.z - z * aOther.y,
          w * aOther.y - x * aOther.z + y * aOther.w + z * aOther.x,
          w * aOther.z + x * aOther.y - y * aOther.x + z * aOther.w,
          w * aOther.w - x * aOther.x - y * aOther.y - z * aOther.z
        );
    }
    gfxQuaternion& operator*=(const gfxQuaternion& aOther) {
        *this = *this * aOther;
        return *this;
    }

    mozilla::gfx::Matrix4x4 ToMatrix() const {
      mozilla::gfx::Matrix4x4 temp;

        temp[0][0] = 1 - 2 * (y * y + z * z);
        temp[0][1] = 2 * (x * y + w * z);
        temp[0][2] = 2 * (x * z - w * y);
        temp[1][0] = 2 * (x * y - w * z);
        temp[1][1] = 1 - 2 * (x * x + z * z);
        temp[1][2] = 2 * (y * z + w * x);
        temp[2][0] = 2 * (x * z + w * y);
        temp[2][1] = 2 * (y * z - w * x);
        temp[2][2] = 1 - 2 * (x * x + y * y);

        return temp;
    }

};

#endif /* GFX_QUATERNION_H */
