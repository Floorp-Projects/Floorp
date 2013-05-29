/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_QUAD_H
#define GFX_QUAD_H

#include "gfxTypes.h"
#include "gfxLineSegment.h"
#include <algorithm>

struct gfxQuad {
    gfxQuad(const gfxPoint& aOne, const gfxPoint& aTwo, const gfxPoint& aThree, const gfxPoint& aFour)
    {
        mPoints[0] = aOne;
        mPoints[1] = aTwo;
        mPoints[2] = aThree;
        mPoints[3] = aFour;
    }

    bool Contains(const gfxPoint& aPoint)
    {
        return (gfxLineSegment(mPoints[0], mPoints[1]).PointsOnSameSide(aPoint, mPoints[2]) &&
                gfxLineSegment(mPoints[1], mPoints[2]).PointsOnSameSide(aPoint, mPoints[3]) &&
                gfxLineSegment(mPoints[2], mPoints[3]).PointsOnSameSide(aPoint, mPoints[0]) &&
                gfxLineSegment(mPoints[3], mPoints[0]).PointsOnSameSide(aPoint, mPoints[1]));
    }

    gfxRect GetBounds()
    {
        gfxFloat min_x, max_x;
        gfxFloat min_y, max_y;

        min_x = max_x = mPoints[0].x;
        min_y = max_y = mPoints[0].y;

        for (int i=1; i<4; i++) {
          min_x = std::min(mPoints[i].x, min_x);
          max_x = std::max(mPoints[i].x, max_x);
          min_y = std::min(mPoints[i].y, min_y);
          max_y = std::max(mPoints[i].y, max_y);
        }
        return gfxRect(min_x, min_y, max_x - min_x, max_y - min_y);
    }

    gfxPoint mPoints[4];
};

#endif /* GFX_QUAD_H */
