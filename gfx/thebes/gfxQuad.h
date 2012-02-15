/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#ifndef GFX_QUAD_H
#define GFX_QUAD_H

#include "gfxTypes.h"
#include "gfxLineSegment.h"

struct THEBES_API gfxQuad {
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
          min_x = NS_MIN(mPoints[i].x, min_x);
          max_x = NS_MAX(mPoints[i].x, max_x);
          min_y = NS_MIN(mPoints[i].y, min_y);
          max_y = NS_MAX(mPoints[i].y, max_y);
        }
        return gfxRect(min_x, min_y, max_x - min_x, max_y - min_y);
    }

    gfxPoint mPoints[4];
};

#endif /* GFX_QUAD_H */
