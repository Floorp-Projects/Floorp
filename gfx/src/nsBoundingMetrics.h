/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsBoundingMetrics_h
#define __nsBoundingMetrics_h

#include "nsCoord.h"

/* Struct used for accurate measurements of a string, in order to
 * allow precise positioning when processing MathML.  This is in its
 * own header file because some very-widely-included headers need it
 * but not the rest of nsFontMetrics, or vice versa.
 */

struct nsBoundingMetrics {

    ///////////
    // Metrics that _exactly_ enclose the text:

    // The character coordinate system is the one used on X Windows:
    // 1. The origin is located at the intersection of the baseline
    //    with the left of the character's cell.
    // 2. All horizontal bearings are oriented from left to right.
    // 2. All horizontal bearings are oriented from left to right.
    // 3. The ascent is oriented from bottom to top (being 0 at the orgin).
    // 4. The descent is oriented from top to bottom (being 0 at the origin).

    // Note that Win32/Mac/PostScript use a different convention for
    // the descent (all vertical measurements are oriented from bottom
    // to top on these palatforms). Make sure to flip the sign of the
    // descent on these platforms for cross-platform compatibility.

    // Any of the following member variables listed here can have
    // positive or negative value.

    nscoord leftBearing;
    /* The horizontal distance from the origin of the drawing
       operation to the left-most part of the drawn string. */

    nscoord rightBearing;
    /* The horizontal distance from the origin of the drawing
       operation to the right-most part of the drawn string.
       The _exact_ width of the string is therefore:
       rightBearing - leftBearing */

    nscoord ascent;
    /* The vertical distance from the origin of the drawing
       operation to the top-most part of the drawn string. */

    nscoord descent;
    /* The vertical distance from the origin of the drawing
       operation to the bottom-most part of the drawn string.
       The _exact_ height of the string is therefore:
       ascent + descent */

    nscoord width;
    /* The horizontal distance from the origin of the drawing
       operation to the correct origin for drawing another string
       to follow the current one. Depending on the font, this
       could be greater than or less than the right bearing. */

    nsBoundingMetrics() : leftBearing(0), rightBearing(0),
                          ascent(0), descent(0), width(0)
    {}

    void
    operator += (const nsBoundingMetrics& bm) {
        if (ascent + descent == 0 && rightBearing - leftBearing == 0) {
            ascent = bm.ascent;
            descent = bm.descent;
            leftBearing = width + bm.leftBearing;
            rightBearing = width + bm.rightBearing;
        }
        else {
            if (ascent < bm.ascent) ascent = bm.ascent;
            if (descent < bm.descent) descent = bm.descent;
            leftBearing = NS_MIN(leftBearing, width + bm.leftBearing);
            rightBearing = NS_MAX(rightBearing, width + bm.rightBearing);
        }
        width += bm.width;
    }
};

#endif // __nsBoundingMetrics_h
