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

#ifndef GFX_MATRIX_H
#define GFX_MATRIX_H

#include <cairo.h>

#include "gfxPoint.h"
#include "gfxTypes.h"

// XX - I don't think this class should use gfxFloat at all,
// but should use 'double' and be called gfxDoubleMatrix;
// we can then typedef that to gfxMatrix where we typedef
// double to be gfxFloat.

class gfxMatrix {
protected:
    cairo_matrix_t mat;

public:
    gfxMatrix() { Reset(); }
    gfxMatrix(const gfxMatrix& m) : mat(m.mat) {}
    gfxMatrix(gfxFloat a, gfxFloat b, gfxFloat c, gfxFloat d, gfxFloat tx, gfxFloat ty) {
        mat.xx = a; mat.yx = b; mat.xy = c; mat.yy = d; mat.x0 = tx; mat.y0 = ty;
    }

    gfxMatrix(const cairo_matrix_t& m) {
        mat = m;
    }

    gfxMatrix& operator=(const cairo_matrix_t& m) {
        mat = m;
        return *this;
    }

    const gfxMatrix& operator *= (const gfxMatrix& m) {
        return Multiply(m);
    }

    gfxMatrix operator * (const gfxMatrix& m) {
        return gfxMatrix(*this).Multiply(m);
    }

    // conversion to other types
    cairo_matrix_t ToCairoMatrix() const {
        return mat;
    }

    void ToValues(gfxFloat *xx, gfxFloat *yx,
                  gfxFloat *xy, gfxFloat *yy,
                  gfxFloat *x0, gfxFloat *y0) const
    {
        *xx = mat.xx;
        *yx = mat.yx;
        *xy = mat.xy;
        *yy = mat.yy;
        *x0 = mat.x0;
        *y0 = mat.y0;
    }

    // matrix operations
    const gfxMatrix& Reset() {
        cairo_matrix_init_identity(&mat);
        return *this;
    }

    const gfxMatrix& Invert() {
        cairo_matrix_invert(&mat);
        return *this;
    }

    const gfxMatrix& Scale(gfxFloat x, gfxFloat y) {
        cairo_matrix_scale(&mat, x, y);
        return *this;
    }

    const gfxMatrix& Translate(const gfxPoint& pt) {
        cairo_matrix_translate(&mat, pt.x, pt.y);
        return *this;
    }

    const gfxMatrix& Rotate(gfxFloat radians) {
        gfxFloat s = sin(radians);
        gfxFloat c = cos(radians);
        gfxMatrix t( c, s,
                    -s, c,
                     0, 0);
        return *this = t.Multiply(*this);
    }

    const gfxMatrix& Multiply(const gfxMatrix& m) {
        cairo_matrix_multiply(&mat, &mat, &m.mat);
        return *this;
    }

    void TransformDistance(gfxFloat *dx, gfxFloat *dy) const {
        cairo_matrix_transform_distance(&mat, dx, dy);
    }

    void TransformPoint(gfxFloat *x, gfxFloat *y) const {
        cairo_matrix_transform_point(&mat, x, y);
    }

    gfxSize GetScaling() const {
        return gfxSize(mat.xx, mat.yy);
    }

    gfxPoint GetTranslation() const {
        return gfxPoint(mat.x0, mat.y0);
    }

    bool HasShear() const {
        return ((mat.xy != 0.0) || (mat.yx != 0.0));
    }
};

#endif /* GFX_MATRIX_H */
