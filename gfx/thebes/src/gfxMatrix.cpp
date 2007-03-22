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
    double x0 = rect.pos.x;
    double y0 = rect.pos.y;
    double x1 = rect.pos.x + rect.size.width;
    double y1 = rect.pos.y + rect.size.height;

    cairo_matrix_transform_bounding_box(CONST_CAIRO_MATRIX(this), &x0, &y0, &x1, &y1, NULL);

    return gfxRect(x0, y0, x1 - x0, y1 - y0);
}
