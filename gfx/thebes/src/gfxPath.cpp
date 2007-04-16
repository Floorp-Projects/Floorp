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
 * The Original Code is IBM Corporation code.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "gfxPath.h"
#include "gfxPoint.h"

#include "cairo.h"

gfxFlattenedPath::gfxFlattenedPath(cairo_path_t *aPath) : mPath(aPath)
{
}

gfxFlattenedPath::~gfxFlattenedPath()
{
    cairo_path_destroy(mPath);
}

static gfxFloat
CalcSubLengthAndAdvance(cairo_path_data_t *aData,
                        gfxPoint &aPathStart, gfxPoint &aCurrent)
{
    float sublength = 0;

    switch (aData->header.type) {
        case CAIRO_PATH_MOVE_TO:
        {
            aCurrent = aPathStart = gfxPoint(aData[1].point.x,
                                             aData[1].point.y);
            break;
        }
        case CAIRO_PATH_LINE_TO:
        {
            gfxPoint diff =
                gfxPoint(aData[1].point.x, aData[1].point.y) - aCurrent;
            sublength = sqrt(diff.x * diff.x + diff.y * diff.y);
            aCurrent = gfxPoint(aData[1].point.x, aData[1].point.y);
            break;
        }
        case CAIRO_PATH_CURVE_TO:
            /* should never happen with a flattened path */
            NS_WARNING("curve_to in flattened path");
            break;
        case CAIRO_PATH_CLOSE_PATH:
        {
            gfxPoint diff = aPathStart - aCurrent;
            sublength = sqrt(diff.x * diff.x + diff.y * diff.y);
            aCurrent = aPathStart;
            break;
        }
    }
    return sublength;
}

gfxFloat
gfxFlattenedPath::GetLength()
{
    gfxPoint start(0, 0);     // start of current subpath
    gfxPoint current(0, 0);   // current point
    gfxFloat length = 0;      // current summed length

    for (PRInt32 i = 0;
         i < mPath->num_data;
         i += mPath->data[i].header.length) {
        length += CalcSubLengthAndAdvance(&mPath->data[i], start, current);
    }
    return length;
}

gfxPoint
gfxFlattenedPath::FindPoint(gfxPoint aOffset, gfxFloat *aAngle)
{
    gfxPoint start(0, 0);     // start of current subpath
    gfxPoint current(0, 0);   // current point
    gfxFloat length = 0;      // current summed length

    for (PRInt32 i = 0;
         i < mPath->num_data;
         i += mPath->data[i].header.length) {
        gfxPoint prev = current;

        gfxFloat sublength = CalcSubLengthAndAdvance(&mPath->data[i],
                                                     start, current);

        gfxPoint diff = current - prev;
        if (aAngle)
            *aAngle = atan2(diff.y, diff.x);

        if (sublength != 0 && length + sublength >= aOffset.x) {
            gfxFloat ratio = (aOffset.x - length) / sublength;
            gfxFloat normalization =
                1.0 / sqrt(diff.x * diff.x + diff.y * diff.y);

            return prev * (1.0f - ratio) + current * ratio +
                gfxPoint(-diff.y, diff.x) * aOffset.y * normalization;
        }
        length += sublength;
    }

    // requested offset is past the end of the path - return last point
    return current;
}
