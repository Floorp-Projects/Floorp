/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_MATRIX_H
#define GFX_MATRIX_H

#include "gfxPoint.h"
#include "gfxTypes.h"
#include "gfxRect.h"
#include "mozilla/Attributes.h"

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
class gfxMatrix {
public:
    double _11; double _12;
    double _21; double _22;
    double _31; double _32;

    /**
     * Initializes this matrix as the identity matrix.
     */
    gfxMatrix() { Reset(); }

    /**
     * Initializes the matrix from individual components. See the class
     * description for the layout of the matrix.
     */
    gfxMatrix(gfxFloat a, gfxFloat b, gfxFloat c, gfxFloat d, gfxFloat tx, gfxFloat ty) :
        _11(a),  _12(b),
        _21(c),  _22(d),
        _31(tx), _32(ty) { }

    MOZ_ALWAYS_INLINE gfxMatrix Copy() const {
        return gfxMatrix(*this);
    }

    friend std::ostream& operator<<(std::ostream& stream, const gfxMatrix& m) {
      if (m.IsIdentity()) {
        return stream << "[identity]";
      }

      return stream << "["
             << m._11 << " " << m._12
             << m._21 << " " << m._22
             << m._31 << " " << m._32
             << "]";
    }

    /**
     * Post-multiplies m onto the matrix.
     */
    const gfxMatrix& operator *= (const gfxMatrix& m);

    /**
     * Multiplies *this with m and returns the result.
     */
    gfxMatrix operator * (const gfxMatrix& m) const {
        return gfxMatrix(*this) *= m;
    }

    /* Returns true if the other matrix is fuzzy-equal to this matrix.
     * Note that this isn't a cheap comparison!
     */
    bool operator==(const gfxMatrix& other) const
    {
      return FuzzyEqual(_11, other._11) && FuzzyEqual(_12, other._12) &&
             FuzzyEqual(_21, other._21) && FuzzyEqual(_22, other._22) &&
             FuzzyEqual(_31, other._31) && FuzzyEqual(_32, other._32);
    }

    bool operator!=(const gfxMatrix& other) const
    {
      return !(*this == other);
    }

    // matrix operations
    /**
     * Resets this matrix to the identity matrix.
     */
    const gfxMatrix& Reset();

    bool IsIdentity() const {
       return _11 == 1.0 && _12 == 0.0 &&
              _21 == 0.0 && _22 == 1.0 &&
              _31 == 0.0 && _32 == 0.0;
    }

    /**
     * Inverts this matrix, if possible. Otherwise, the matrix is left
     * unchanged.
     *
     * XXX should this do something with the return value of
     * cairo_matrix_invert?
     */
    bool Invert();

    /**
     * Check if matrix is singular (no inverse exists).
     */
    bool IsSingular() const {
        // if the determinant (ad - bc) is zero it's singular
        return (_11 * _22) == (_12 * _21);
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
     * This is a pre-multiplication, i.e. the transformations of m are
     * applied _before_ the existing transformations.
     */
    const gfxMatrix& PreMultiply(const gfxMatrix& m);

    static gfxMatrix Translation(gfxFloat aX, gfxFloat aY)
    {
        return gfxMatrix(1.0, 0.0, 0.0, 1.0, aX, aY);
    }

    static gfxMatrix Translation(gfxPoint aPoint)
    {
        return Translation(aPoint.x, aPoint.y);
    }

    static gfxMatrix Rotation(gfxFloat aAngle);

    static gfxMatrix Scaling(gfxFloat aX, gfxFloat aY)
    {
        return gfxMatrix(aX, 0.0, 0.0, aY, 0.0, 0.0);
    }

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
        return gfxPoint(_31, _32);
    }

    /**
     * Returns true if the matrix is anything other than a straight
     * translation by integers.
     */
    bool HasNonIntegerTranslation() const {
        return HasNonTranslation() ||
            !FuzzyEqual(_31, floor(_31 + 0.5)) ||
            !FuzzyEqual(_32, floor(_32 + 0.5));
    }

    /**
     * Returns true if the matrix has any transform other
     * than a straight translation
     */
    bool HasNonTranslation() const {
        return !FuzzyEqual(_11, 1.0) || !FuzzyEqual(_22, 1.0) ||
               !FuzzyEqual(_21, 0.0) || !FuzzyEqual(_12, 0.0);
    }

    /**
     * Returns true if the matrix only has an integer translation.
     */
    bool HasOnlyIntegerTranslation() const {
        return !HasNonIntegerTranslation();
    }

    /**
     * Returns true if the matrix has any transform other
     * than a translation or a -1 y scale (y axis flip)
     */
    bool HasNonTranslationOrFlip() const {
        return !FuzzyEqual(_11, 1.0) ||
               (!FuzzyEqual(_22, 1.0) && !FuzzyEqual(_22, -1.0)) ||
               !FuzzyEqual(_21, 0.0) || !FuzzyEqual(_12, 0.0);
    }

    /**
     * Returns true if the matrix has any transform other
     * than a translation or scale; this is, if there is
     * no rotation.
     */
    bool HasNonAxisAlignedTransform() const {
        return !FuzzyEqual(_21, 0.0) || !FuzzyEqual(_12, 0.0);
    }

    /**
     * Computes the determinant of this matrix.
     */
    double Determinant() const {
        return _11*_22 - _12*_21;
    }

    /* Computes the scale factors of this matrix; that is,
     * the amounts each basis vector is scaled by.
     * The xMajor parameter indicates if the larger scale is
     * to be assumed to be in the X direction or not.
     */
    gfxSize ScaleFactors(bool xMajor) const {
        double det = Determinant();

        if (det == 0.0)
            return gfxSize(0.0, 0.0);

        gfxSize sz = xMajor ? gfxSize(1.0, 0.0) : gfxSize(0.0, 1.0);
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

    /**
     * Snap matrix components that are close to integers
     * to integers. In particular, components that are integral when
     * converted to single precision are set to those integers.
     */
    void NudgeToIntegers(void);

    /**
     * Returns true if matrix is multiple of 90 degrees rotation with flipping,
     * scaling and translation.
     */
    bool PreservesAxisAlignedRectangles() const {
        return ((FuzzyEqual(_11, 0.0) && FuzzyEqual(_22, 0.0))
            || (FuzzyEqual(_21, 0.0) && FuzzyEqual(_12, 0.0)));
    }

    /**
     * Returns true if the matrix has non-integer scale
     */
    bool HasNonIntegerScale() const {
        return !FuzzyEqual(_11, floor(_11 + 0.5)) ||
               !FuzzyEqual(_22, floor(_22 + 0.5));
    }

private:
    static bool FuzzyEqual(gfxFloat aV1, gfxFloat aV2) {
        return fabs(aV2 - aV1) < 1e-6;
    }
};

#endif /* GFX_MATRIX_H */
