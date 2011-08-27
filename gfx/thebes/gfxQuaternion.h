/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Matt Woodrow <mwoodrow@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef GFX_QUATERNION_H
#define GFX_QUATERNION_H

#include "mozilla/gfx/BasePoint4D.h"
#include "gfx3DMatrix.h"

struct THEBES_API gfxQuaternion : public mozilla::gfx::BasePoint4D<gfxFloat, gfxQuaternion> {
    typedef mozilla::gfx::BasePoint4D<gfxFloat, gfxQuaternion> Super;

    gfxQuaternion() : Super() {}
    gfxQuaternion(gfxFloat aX, gfxFloat aY, gfxFloat aZ, gfxFloat aW) : Super(aX, aY, aZ, aW) {}

    gfxQuaternion(const gfx3DMatrix& aMatrix) {
        w = 0.5 * sqrt(NS_MAX(1 + aMatrix[0][0] + aMatrix[1][1] + aMatrix[2][2], 0.0f));
        x = 0.5 * sqrt(NS_MAX(1 + aMatrix[0][0] - aMatrix[1][1] - aMatrix[2][2], 0.0f));
        y = 0.5 * sqrt(NS_MAX(1 - aMatrix[0][0] + aMatrix[1][1] - aMatrix[2][2], 0.0f));
        z = 0.5 * sqrt(NS_MAX(1 - aMatrix[0][0] - aMatrix[1][1] + aMatrix[2][2], 0.0f));

        if(aMatrix[2][1] > aMatrix[1][2])
            x = -x;
        if(aMatrix[0][2] > aMatrix[2][0])
            y = -y;
        if(aMatrix[1][0] > aMatrix[0][1])
            z = -z;
    }

    gfxQuaternion Slerp(const gfxQuaternion &aOther, gfxFloat aCoeff) {
        gfxFloat dot = NS_CLAMP(DotProduct(aOther), -1.0, 1.0);
        if (dot == 1.0) {
            return *this;
        }

        gfxFloat theta = acos(dot);
        gfxFloat rsintheta = 1/sqrt(1 - dot*dot);
        gfxFloat w = sin(aCoeff*theta)*rsintheta;

        gfxQuaternion left = *this;
        gfxQuaternion right = aOther;

        left *= cos(aCoeff*theta) - dot*w;
        right *= w;

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