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

#ifndef MATRIX_H
#define MATRIX_H

#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif
#include <math.h>

#include <cairo.h>

class gfxMatrix {
private:
    double m0, m1, m2, m3, m4, m5;

public:
    gfxMatrix() { Reset(); }
    gfxMatrix(const gfxMatrix& m) :
        m0(m.m0), m1(m.m1), m2(m.m2), m3(m.m3), m4(m.m4), m5(m.m5) {}
    gfxMatrix(double a, double b, double c, double d, double tx, double ty) :
        m0(a), m1(b), m2(c), m3(d), m4(tx), m5(ty) {}

    gfxMatrix(const cairo_matrix_t* m) {
        cairo_matrix_get_affine(const_cast<cairo_matrix_t*>(m),
                                &m0, &m1, &m2, &m3, &m4, &m5);
    }

    gfxMatrix& operator=(const cairo_matrix_t* m) {
        cairo_matrix_get_affine(const_cast<cairo_matrix_t*>(m),
                                &m0, &m1, &m2, &m3, &m4, &m5);
        return *this;
    }

    const gfxMatrix& operator *= (const gfxMatrix& m) {
        return Multiply(m);
    }

    gfxMatrix operator * (const gfxMatrix& m) {
        return gfxMatrix(*this).Multiply(m);
    }

    // this name sucks ass. pick something better
    void FillInCairoMatrix(cairo_matrix_t* m) const {
        cairo_matrix_set_affine(m, m0, m1, m2, m3, m4, m5);
    }

    const gfxMatrix& Reset() {
        m0 = m3 = 1.0;
        m1 = m2 = m4 = m5 = 0.0;
        return *this;
    }

    const gfxMatrix& Invert() {
        // impl me
        return *this;
    }

    const gfxMatrix& Scale(double x, double y) {
        gfxMatrix t(x, 0, 0, y, 0, 0);
        return *this = t.Multiply(*this);
    }

    const gfxMatrix& Translate(double x, double y) {
        gfxMatrix t(1, 0, 0, 1, x, y);
        return *this = t.Multiply(*this);
    }

    const gfxMatrix& Rotate(double radians) {
        double s = sin(radians);
        double c = cos(radians);
        gfxMatrix t( c, s,
                 -s, c,
                  0, 0);
        return *this = t.Multiply(*this);
    }

    const gfxMatrix& Multiply(const gfxMatrix& m) {
        double t0 = m0 * m.m0 + m1 * m.m2;
        double t2 = m2 * m.m0 + m3 * m.m2;
        double t4 = m4 * m.m0 + m5 * m.m2 + m.m4;
        m1 = m0 * m.m1 + m1 * m.m3;
        m3 = m2 * m.m1 + m3 * m.m3;
        m5 = m4 * m.m1 + m5 * m.m3 + m.m5;
        m0 = t0;
        m2 = t2;
        m4 = t4;
        return *this;
    }

    void TransformDistance(double *dx, double *dy) const {
        double new_x = m0 * *dx  +  m2 * *dy;
        double new_y = m1 * *dx  +  m3 * *dy;
        *dx = new_x;
        *dy = new_y;
    }

    void TransformPoint(double *x, double *y) const {
        TransformDistance(x, y);
        *x += m4;
        *y += m5;
    }
};

#endif /* MATRIX_H */
