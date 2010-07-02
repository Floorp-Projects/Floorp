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

#include "gfxPoint.h"
#include "gfxTypes.h"
#include "gfxRect.h"
#include "gfxUtils.h"
#include "nsMathUtils.h"

// XX - I don't think this class should use gfxFloat at all,
// but should use 'double' and be called gfxDoubleMatrix;
// we can then typedef that to gfxMatrix where we typedef
// double to be gfxFloat.

/**
 * A matrix that represents an affine transformation. Projective
 * transformations are not supported. This matrix looks like:
 *
 * / a  b  0 \
 * | c  d  0 |
 * \ tx ty 1 /
 *
 * So, transforming a point (x, y) results in:
 *
 *           / a  b  0 \   / a * x + c * y + tx \ T
 * (x y 1) * | c  d  0 | = | b * x + d * y + ty |
 *           \ tx ty 1 /   \         1          /
 *
 */
struct THEBES_API gfxMatrix {
    double xx; double yx;
    double xy; double yy;
    double x0; double y0;

public:
    /**
     * Initializes this matrix as the identity matrix.
     */
    gfxMatrix() { Reset(); }

    /**
     * Initializes the matrix from individual components. See the class
     * description for the layout of the matrix.
     */
    gfxMatrix(gfxFloat a, gfxFloat b, gfxFloat c, gfxFloat d, gfxFloat tx, gfxFloat ty) :
        xx(a),  yx(b),
        xy(c),  yy(d),
        x0(tx), y0(ty) { }

    /**
     * Post-multiplies m onto the matrix.
     */
    const gfxMatrix& operator *= (const gfxMatrix& m) {
        return Multiply(m);
    }

    /**
     * Multiplies *this with m and returns the result.
     */
    gfxMatrix operator * (const gfxMatrix& m) const {
        return gfxMatrix(*this).Multiply(m);
    }

    // matrix operations
    /**
     * Resets this matrix to the identity matrix.
     */
    const gfxMatrix& Reset();

    /**
     * Inverts this matrix, if possible. Otherwise, the matrix is left
     * unchanged.
     *
     * XXX should this do something with the return value of
     * cairo_matrix_invert?
     */
    const gfxMatrix& Invert();

    /**
     * Check if matrix is singular (no inverse exists).
     */
    PRBool IsSingular() const {
        // if the determinant (ad - bc) is zero it's singular
        return (xx * yy) == (yx * xy);
    }

    /**
     * Scales this matrix. The scale is pre-multiplied onto this matrix,
     * i.e. the scaling takes place before the other transformations.
     */
    const gfxMatrix& Scale(gfxFloat x, gfxFloat y);

    /**
     * Translates this matrix. The translation is pre-multiplied onto this matrix,
     * i.e. the translation takes place before the other transformations.
     */
    const gfxMatrix& Translate(const gfxPoint& pt);

    /**
     * Rotates this matrix. The rotation is pre-multiplied onto this matrix,
     * i.e. the translation takes place after the other transformations.
     *
     * @param radians Angle in radians.
     */
    const gfxMatrix& Rotate(gfxFloat radians);

     /**
      * Multiplies the current matrix with m.
      * This is a post-multiplication, i.e. the transformations of m are
      * applied _after_ the existing transformations.
      *
      * XXX is that difference (compared to Rotate etc) a good thing?
      */
    const gfxMatrix& Multiply(const gfxMatrix& m);

    /**
     * Multiplies the current matrix with m.
     * This is a pre-multiplication, i.e. the transformations of m are
     * applied _before_ the existing transformations.
     */
    const gfxMatrix& PreMultiply(const gfxMatrix& m);

    /**
     * Transforms a point according to this matrix.
     */
    gfxPoint Transform(const gfxPoint& point) const;


    /**
     * Transform a distance according to this matrix. This does not apply
     * any translation components.
     */
    gfxSize Transform(const gfxSize& size) const;

    /**
     * Transforms both the point and distance according to this matrix.
     */
    gfxRect Transform(const gfxRect& rect) const;

    gfxRect TransformBounds(const gfxRect& rect) const;

    /**
     * Returns the translation component of this matrix.
     */
    gfxPoint GetTranslation() const {
        return gfxPoint(x0, y0);
    }

    /**
     * Returns true if the matrix is anything other than a straight
     * translation by integers.
     */
    PRBool HasNonIntegerTranslation() const {
        return HasNonTranslation() ||
            !gfxUtils::FuzzyEqual(x0, NS_floor(x0 + 0.5)) ||
            !gfxUtils::FuzzyEqual(y0, NS_floor(y0 + 0.5));
    }

    /**
     * Returns true if the matrix has any transform other
     * than a straight translation
     */
    PRBool HasNonTranslation() const {
        return !gfxUtils::FuzzyEqual(xx, 1.0) || !gfxUtils::FuzzyEqual(yy, 1.0) ||
               !gfxUtils::FuzzyEqual(xy, 0.0) || !gfxUtils::FuzzyEqual(yx, 0.0);
    }

    /**
     * Returns true if the matrix has any transform other
     * than a translation or a -1 y scale (y axis flip)
     */
    PRBool HasNonTranslationOrFlip() const {
        return !gfxUtils::FuzzyEqual(xx, 1.0) ||
               (!gfxUtils::FuzzyEqual(yy, 1.0) && !gfxUtils::FuzzyEqual(yy, -1.0)) ||
               !gfxUtils::FuzzyEqual(xy, 0.0) || !gfxUtils::FuzzyEqual(yx, 0.0);
    }

    /**
     * Returns true if the matrix has any transform other
     * than a translation or scale; this is, if there is
     * no rotation.
     */
    PRBool HasNonAxisAlignedTransform() const {
        return !gfxUtils::FuzzyEqual(xy, 0.0) || !gfxUtils::FuzzyEqual(yx, 0.0);
    }

    /**
     * Computes the determinant of this matrix.
     */
    double Determinant() const {
        return xx*yy - yx*xy;
    }

    /* Computes the scale factors of this matrix; that is,
     * the amounts each basis vector is scaled by.
     * The xMajor parameter indicates if the larger scale is
     * to be assumed to be in the X direction or not.
     */
    gfxSize ScaleFactors(PRBool xMajor) const {
        double det = Determinant();

        if (det == 0.0)
            return gfxSize(0.0, 0.0);

        gfxSize sz((xMajor != 0 ? 1.0 : 0.0),
                        (xMajor != 0 ? 0.0 : 1.0));
        sz = Transform(sz);

        double major = sqrt(sz.width * sz.width + sz.height * sz.height);
        double minor = 0.0;

        // ignore mirroring
        if (det < 0.0)
            det = - det;

        if (major)
            minor = det / major;

        if (xMajor)
            return gfxSize(major, minor);

        return gfxSize(minor, major);
    }
};

#endif /* GFX_MATRIX_H */
