/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_QUATERNION_H
#define GFX_QUATERNION_H

#include "mozilla/gfx/BasePoint4D.h"
#include "gfx3DMatrix.h"
#include "nsAlgorithm.h"
#include <algorithm>

struct gfxQuaternion : public mozilla::gfx::BasePoint4D<gfxFloat, gfxQuaternion> {
    typedef mozilla::gfx::BasePoint4D<gfxFloat, gfxQuaternion> Super;

    gfxQuaternion() : Super() {}
    gfxQuaternion(gfxFloat aX, gfxFloat aY, gfxFloat aZ, gfxFloat aW) : Super(aX, aY, aZ, aW) {}

    gfxQuaternion(const gfx3DMatrix& aMatrix) {
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

    gfxQuaternion Slerp(const gfxQuaternion &aOther, gfxFloat aCoeff) {
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

    gfx3DMatrix ToMatrix() {
        gfx3DMatrix temp;

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
