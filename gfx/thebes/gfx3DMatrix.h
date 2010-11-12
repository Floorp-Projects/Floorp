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
 *   Bas Schouten <bschouten@mozilla.com
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
#include <gfxMatrix.h>
#include <math.h>

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
  inline gfx3DMatrix(void);

  /**
   * Matrix multiplication.
   */
  inline gfx3DMatrix operator*(const gfx3DMatrix &aMatrix) const;

  /**
   * Return true if this matrix and |aMatrix| are the same matrix.
   */
  inline bool operator==(const gfx3DMatrix& aMatrix) const;

  /**
   * Create a 3D matrix from a gfxMatrix 2D affine transformation.
   *
   * \param aMatrix gfxMatrix 2D affine transformation.
   */
  static inline gfx3DMatrix From2D(const gfxMatrix &aMatrix);

  /**
   * Returns true if the matrix is isomorphic to a 2D affine transformation
   * (i.e. as obtained by From2D). If it is, optionally returns the 2D
   * matrix in aMatrix.
   */
  PRBool Is2D(gfxMatrix* aMatrix = nsnull) const;

  /**
   * Returns true if the matrix is the identity matrix. The most important
   * property we require is that gfx3DMatrix().IsIdentity() returns true.
   */
  inline PRBool IsIdentity() const;

  /**
   * Create a translation matrix.
   *
   * \param aX Translation on X-axis.
   * \param aY Translation on Y-axis.
   * \param aZ Translation on Z-axis.
   */
  static inline gfx3DMatrix Translation(float aX, float aY, float aZ);

  /**
   * Create a scale matrix. Scales uniformly along all axes.
   *
   * \param aScale Scale factor
   */
  static inline gfx3DMatrix Scale(float aFactor);

  /**
   * Create a scale matrix.
   */
  static inline gfx3DMatrix Scale(float aX, float aY, float aZ);

  /** Matrix elements */
  float _11, _12, _13, _14;
  float _21, _22, _23, _24;
  float _31, _32, _33, _34;
  float _41, _42, _43, _44;
};

inline
gfx3DMatrix::gfx3DMatrix(void)
{
  _11 = _22 = _33 = _44 = 1.0f;
  _12 = _13 = _14 = 0.0f;
  _21 = _23 = _24 = 0.0f;
  _31 = _32 = _34 = 0.0f;
  _41 = _42 = _43 = 0.0f;
}

inline gfx3DMatrix
gfx3DMatrix::operator*(const gfx3DMatrix &aMatrix) const
{
  gfx3DMatrix matrix;

  matrix._11 = _11 * aMatrix._11 + _12 * aMatrix._21 + _13 * aMatrix._31 + _14 * aMatrix._41;
  matrix._21 = _21 * aMatrix._11 + _22 * aMatrix._21 + _23 * aMatrix._31 + _24 * aMatrix._41;
  matrix._31 = _31 * aMatrix._11 + _32 * aMatrix._21 + _33 * aMatrix._31 + _34 * aMatrix._41;
  matrix._41 = _41 * aMatrix._11 + _42 * aMatrix._21 + _43 * aMatrix._31 + _44 * aMatrix._41;
  matrix._12 = _11 * aMatrix._12 + _12 * aMatrix._22 + _13 * aMatrix._32 + _14 * aMatrix._42;
  matrix._22 = _21 * aMatrix._12 + _22 * aMatrix._22 + _23 * aMatrix._32 + _24 * aMatrix._42;
  matrix._32 = _31 * aMatrix._12 + _32 * aMatrix._22 + _33 * aMatrix._32 + _34 * aMatrix._42;
  matrix._42 = _41 * aMatrix._12 + _42 * aMatrix._22 + _43 * aMatrix._32 + _44 * aMatrix._42;
  matrix._13 = _11 * aMatrix._13 + _12 * aMatrix._23 + _13 * aMatrix._33 + _14 * aMatrix._43;
  matrix._23 = _21 * aMatrix._13 + _22 * aMatrix._23 + _23 * aMatrix._33 + _24 * aMatrix._43;
  matrix._33 = _31 * aMatrix._13 + _32 * aMatrix._23 + _33 * aMatrix._33 + _34 * aMatrix._43;
  matrix._43 = _41 * aMatrix._13 + _42 * aMatrix._23 + _43 * aMatrix._33 + _44 * aMatrix._43;
  matrix._14 = _11 * aMatrix._14 + _12 * aMatrix._24 + _13 * aMatrix._34 + _14 * aMatrix._44;
  matrix._24 = _21 * aMatrix._14 + _22 * aMatrix._24 + _23 * aMatrix._34 + _24 * aMatrix._44;
  matrix._34 = _31 * aMatrix._14 + _32 * aMatrix._24 + _33 * aMatrix._34 + _34 * aMatrix._44;
  matrix._44 = _41 * aMatrix._14 + _42 * aMatrix._24 + _43 * aMatrix._34 + _44 * aMatrix._44;

  return matrix;
}

inline bool
gfx3DMatrix::operator==(const gfx3DMatrix& o) const
{
  // XXX would be nice to memcmp here, but that breaks IEEE 754 semantics
  return _11 == o._11 && _12 == o._12 && _13 == o._13 && _14 == o._14 &&
         _21 == o._21 && _22 == o._22 && _23 == o._23 && _24 == o._24 &&
         _31 == o._31 && _32 == o._32 && _33 == o._33 && _34 == o._34 &&
         _41 == o._41 && _42 == o._42 && _43 == o._43 && _44 == o._44;
}

inline gfx3DMatrix
gfx3DMatrix::From2D(const gfxMatrix &aMatrix)
{
  gfx3DMatrix matrix;
  matrix._11 = (float)aMatrix.xx;
  matrix._12 = (float)aMatrix.yx;
  matrix._21 = (float)aMatrix.xy;
  matrix._22 = (float)aMatrix.yy;
  matrix._41 = (float)aMatrix.x0;
  matrix._42 = (float)aMatrix.y0;
  return matrix;
}

inline PRBool
gfx3DMatrix::IsIdentity() const
{
  return _11 == 1.0f && _12 == 0.0f && _13 == 0.0f && _14 == 0.0f &&
         _21 == 0.0f && _22 == 1.0f && _23 == 0.0f && _24 == 0.0f &&
         _31 == 0.0f && _32 == 0.0f && _33 == 1.0f && _34 == 0.0f &&
         _41 == 0.0f && _42 == 0.0f && _43 == 0.0f && _44 == 1.0f;
}

inline gfx3DMatrix
gfx3DMatrix::Translation(float aX, float aY, float aZ)
{
  gfx3DMatrix matrix;

  matrix._41 = aX;
  matrix._42 = aY;
  matrix._43 = aZ;
  return matrix;
}

inline gfx3DMatrix
gfx3DMatrix::Scale(float aFactor)
{
  gfx3DMatrix matrix;

  matrix._11 = matrix._22 = matrix._33 = aFactor;
  return matrix;
}

inline gfx3DMatrix
gfx3DMatrix::Scale(float aX, float aY, float aZ)
{
  gfx3DMatrix matrix;

  matrix._11 = aX;
  matrix._22 = aY;
  matrix._33 = aZ;

  return matrix;
}

#endif /* GFX_3DMATRIX_H */
