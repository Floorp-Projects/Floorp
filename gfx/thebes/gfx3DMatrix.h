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
 *   Bas Schouten <bschouten@mozilla.com>
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

#ifndef GFX_3DMATRIX_H
#define GFX_3DMATRIX_H

#include <gfxTypes.h>
#include <gfxPoint3D.h>
#include <gfxPointH3D.h>
#include <gfxMatrix.h>

/**
 * This class represents a 3D transformation. The matrix is laid
 * out as follows:
 *
 * _11 _12 _13 _14
 * _21 _22 _23 _24
 * _31 _32 _33 _34
 * _41 _42 _43 _44
 *
 * This matrix is treated as row-major. Assuming we consider our vectors row
 * vectors, this matrix type will be identical in memory to the OpenGL and D3D
 * matrices. OpenGL matrices are column-major, however OpenGL also treats
 * vectors as column vectors, the double transposition makes everything work
 * out nicely.
 */
class THEBES_API gfx3DMatrix
{
public:
  /**
   * Create matrix.
   */
  gfx3DMatrix(void);

  /**
   * Matrix multiplication.
   */
  gfx3DMatrix operator*(const gfx3DMatrix &aMatrix) const;
  gfx3DMatrix& operator*=(const gfx3DMatrix &aMatrix);

  gfxPointH3D& operator[](int aIndex)
  {
      NS_ABORT_IF_FALSE(aIndex >= 0 && aIndex <= 3, "Invalid matrix array index");
      return *reinterpret_cast<gfxPointH3D*>((&_11)+4*aIndex);
  }
  const gfxPointH3D& operator[](int aIndex) const
  {
      NS_ABORT_IF_FALSE(aIndex >= 0 && aIndex <= 3, "Invalid matrix array index");
      return *reinterpret_cast<const gfxPointH3D*>((&_11)+4*aIndex);
  }

  /**
   * Return true if this matrix and |aMatrix| are the same matrix.
   */
  bool operator==(const gfx3DMatrix& aMatrix) const;
  
  /**
   * Divide all values in the matrix by a scalar value
   */
  gfx3DMatrix& operator/=(gfxFloat scalar);

  /**
   * Create a 3D matrix from a gfxMatrix 2D affine transformation.
   *
   * \param aMatrix gfxMatrix 2D affine transformation.
   */
  static gfx3DMatrix From2D(const gfxMatrix &aMatrix);

  /**
   * Returns true if the matrix is isomorphic to a 2D affine transformation
   * (i.e. as obtained by From2D). If it is, optionally returns the 2D
   * matrix in aMatrix.
   */
  PRBool Is2D(gfxMatrix* aMatrix) const;
  PRBool Is2D() const;

  /**
   * Returns true if the matrix can be reduced to a 2D affine transformation
   * (i.e. as obtained by From2D). If it is, optionally returns the 2D
   * matrix in aMatrix. This should only be used on matrices required for
   * rendering, not for intermediate calculations.
   *
   * Since drawing is to a 2d plane, any 3d transform without perspective
   * can be reduced by dropping the z row and column.
   */
  PRBool CanDraw2D(gfxMatrix* aMatrix = nsnull) const;

  /**
   * Returns true if the matrix is the identity matrix. The most important
   * property we require is that gfx3DMatrix().IsIdentity() returns true.
   */
  PRBool IsIdentity() const;

  /**
   * Add a translation by aPoint to the matrix.
   * This is functionally equivalent to:
   * gfx3DMatrix::Translation(aPoint) * matrix
   */
  void Translate(const gfxPoint3D& aPoint);

  /**
   * Add a translation by aPoint after the matrix.
   * This is functionally equivalent to:
   * matrix *gfx3DMatrix::Translation(aPoint)
   */
  void TranslatePost(const gfxPoint3D& aPoint);

  void SkewXY(float aSkew);
  void SkewXZ(float aSkew);
  void SkewYZ(float aSkew);

  void Scale(float aX, float aY, float aZ);

  /**
   * Transforms a point according to this matrix.
   */
  gfxPoint Transform(const gfxPoint& point) const;

  /**
   * Transforms a rectangle according to this matrix
   */
  gfxRect TransformBounds(const gfxRect& rect) const;

  /** 
   * Transforms a 3D vector according to this matrix.
   */
  gfxPoint3D Transform3D(const gfxPoint3D& point) const;
  gfxPointH3D Transform4D(const gfxPointH3D& aPoint) const;
  gfxPointH3D TransposeTransform4D(const gfxPointH3D& aPoint) const;

  gfxPoint ProjectPoint(const gfxPoint& aPoint) const;
  gfxRect ProjectRectBounds(const gfxRect& aRect) const;


  /**
   * Inverts this matrix, if possible. Otherwise, the matrix is left
   * unchanged.
   */
  gfx3DMatrix Inverse() const;

  gfx3DMatrix& Invert()
  {
      *this = Inverse();
      return *this;
  }

  gfx3DMatrix& Normalize();

  gfxPointH3D TransposedVector(int aIndex) const
  {
      NS_ABORT_IF_FALSE(aIndex >= 0 && aIndex <= 3, "Invalid matrix array index");
      return gfxPointH3D(*((&_11)+aIndex), *((&_21)+aIndex), *((&_31)+aIndex), *((&_41)+aIndex));
  }

  void SetTransposedVector(int aIndex, gfxPointH3D &aVector)
  {
      NS_ABORT_IF_FALSE(aIndex >= 0 && aIndex <= 3, "Invalid matrix array index");
      *((&_11)+aIndex) = aVector.x;
      *((&_21)+aIndex) = aVector.y;
      *((&_31)+aIndex) = aVector.z;
      *((&_41)+aIndex) = aVector.w;
  }

  gfx3DMatrix& Transpose();
  gfx3DMatrix Transposed() const;

  /**
   * Returns a unit vector that is perpendicular to the plane formed
   * by transform the screen plane (z=0) by this matrix.
   */
  gfxPoint3D GetNormalVector() const;

  /**
   * Check if matrix is singular (no inverse exists).
   */
  PRBool IsSingular() const;

  /**
   * Create a translation matrix.
   *
   * \param aX Translation on X-axis.
   * \param aY Translation on Y-axis.
   * \param aZ Translation on Z-axis.
   */
  static gfx3DMatrix Translation(float aX, float aY, float aZ);
  static gfx3DMatrix Translation(const gfxPoint3D& aPoint);

  /**
   * Create a scale matrix. Scales uniformly along all axes.
   *
   * \param aScale Scale factor
   */
  static gfx3DMatrix ScalingMatrix(float aFactor);

  /**
   * Create a scale matrix.
   */
  static gfx3DMatrix ScalingMatrix(float aX, float aY, float aZ);

  gfxFloat Determinant() const;

private:

  gfxFloat Determinant3x3() const;
  gfx3DMatrix Inverse3x3() const;

  gfx3DMatrix Multiply2D(const gfx3DMatrix &aMatrix) const;

public:

  /** Matrix elements */
  float _11, _12, _13, _14;
  float _21, _22, _23, _24;
  float _31, _32, _33, _34;
  float _41, _42, _43, _44;
};

#endif /* GFX_3DMATRIX_H */
