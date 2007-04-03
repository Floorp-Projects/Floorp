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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
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

#include "gfxMatrix.h"
#include "cairo.h"

#define CAIRO_MATRIX(x) reinterpret_cast<cairo_matrix_t*>((x))
#define CONST_CAIRO_MATRIX(x) reinterpret_cast<const cairo_matrix_t*>((x))

const gfxMatrix&
gfxMatrix::Reset()
{
    cairo_matrix_init_identity(CAIRO_MATRIX(this));
    return *this;
}

const gfxMatrix&
gfxMatrix::Invert()
{
    cairo_matrix_invert(CAIRO_MATRIX(this));
    return *this;
}

const gfxMatrix&
gfxMatrix::Scale(gfxFloat x, gfxFloat y)
{
    cairo_matrix_scale(CAIRO_MATRIX(this), x, y);
    return *this;
}

const gfxMatrix&
gfxMatrix::Translate(const gfxPoint& pt)
{
    cairo_matrix_translate(CAIRO_MATRIX(this), pt.x, pt.y);
    return *this;
}

const gfxMatrix&
gfxMatrix::Rotate(gfxFloat radians)
{
    cairo_matrix_rotate(CAIRO_MATRIX(this), radians);
    return *this;
}

const gfxMatrix&
gfxMatrix::Multiply(const gfxMatrix& m)
{
    cairo_matrix_multiply(CAIRO_MATRIX(this), CAIRO_MATRIX(this), CONST_CAIRO_MATRIX(&m));
    return *this;
}

gfxPoint
gfxMatrix::Transform(const gfxPoint& point) const
{
    gfxPoint ret = point;
    cairo_matrix_transform_point(CONST_CAIRO_MATRIX(this), &ret.x, &ret.y);
    return ret;
}

gfxSize
gfxMatrix::Transform(const gfxSize& size) const
{
    gfxSize ret = size;
    cairo_matrix_transform_distance(CONST_CAIRO_MATRIX(this), &ret.width, &ret.height);
    return ret;
}

gfxRect
gfxMatrix::Transform(const gfxRect& rect) const
{
    return gfxRect(Transform(rect.pos), Transform(rect.size));
}

gfxRect
gfxMatrix::TransformBounds(const gfxRect& rect) const
{
    /* Code taken from cairo-matrix.c, _cairo_matrix_transform_bounding_box isn't public */
    int i;
    double quad_x[4], quad_y[4];
    double min_x, max_x;
    double min_y, max_y;

    quad_x[0] = rect.pos.x;
    quad_y[0] = rect.pos.y;
    cairo_matrix_transform_point (CONST_CAIRO_MATRIX(this), &quad_x[0], &quad_y[0]);

    quad_x[1] = rect.pos.x + rect.size.width;
    quad_y[1] = rect.pos.y;
    cairo_matrix_transform_point (CONST_CAIRO_MATRIX(this), &quad_x[1], &quad_y[1]);

    quad_x[2] = rect.pos.x;
    quad_y[2] = rect.pos.y + rect.size.height;
    cairo_matrix_transform_point (CONST_CAIRO_MATRIX(this), &quad_x[2], &quad_y[2]);

    quad_x[3] = rect.pos.x + rect.size.width;
    quad_y[3] = rect.pos.y + rect.size.height;
    cairo_matrix_transform_point (CONST_CAIRO_MATRIX(this), &quad_x[3], &quad_y[3]);

    min_x = max_x = quad_x[0];
    min_y = max_y = quad_y[0];

    for (i = 1; i < 4; i++) {
        if (quad_x[i] < min_x)
            min_x = quad_x[i];
        if (quad_x[i] > max_x)
            max_x = quad_x[i];

        if (quad_y[i] < min_y)
            min_y = quad_y[i];
        if (quad_y[i] > max_y)
            max_y = quad_y[i];
    }

    // we don't compute this now
#if 0
    if (is_tight) {
        /* it's tight if and only if the four corner points form an axis-aligned
           rectangle.
           And that's true if and only if we can derive corners 0 and 3 from
           corners 1 and 2 in one of two straightforward ways...
           We could use a tolerance here but for now we'll fall back to FALSE in the case
           of floating point error.
        */
        *is_tight =
            (quad_x[1] == quad_x[0] && quad_y[1] == quad_y[3] &&
             quad_x[2] == quad_x[3] && quad_y[2] == quad_y[0]) ||
            (quad_x[1] == quad_x[3] && quad_y[1] == quad_y[0] &&
             quad_x[2] == quad_x[0] && quad_y[2] == quad_y[3]);
    }
#endif

    return gfxRect(min_x, min_y, max_x - min_x, max_y - min_y);
}
