/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_3DMATRIX_H
#define GFX_3DMATRIX_H

#include <gfxTypes.h>
#include "mozilla/gfx/Point.h"
#include <gfxQuad.h>

class gfxMatrix;

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
class gfx3DMatrix
{
  typedef mozilla::gfx::Point3D Point3D;
  typedef mozilla::gfx::Point4D Point4D;
public:
  /**
   * Create matrix.
   */
  gfx3DMatrix(void);

  friend std::ostream& operator<<(std::ostream& stream, const gfx3DMatrix& m) {
    if (m.IsIdentity()) {
      return stream << "[ I ]";
    }

    if (m.Is2D()) {
      return stream << "["
             << m._11 << " " << m._12 << "; "
             << m._21 << " " << m._22 << "; "
             << m._41 << " " << m._42
             << "]";
    }

    return stream << "["
           << m._11 << " " << m._12 << " " << m._13 << " " << m._14 << "; "
           << m._21 << " " << m._22 << " " << m._23 << " " << m._24 << "; "
           << m._31 << " " << m._32 << " " << m._33 << " " << m._34 << "; "
           << m._41 << " " << m._42 << " " << m._43 << " " << m._44
           << "]";
  }

  /**
   * Matrix multiplication.
   */
  gfx3DMatrix operator*(const gfx3DMatrix &aMatrix) const;
  gfx3DMatrix& operator*=(const gfx3DMatrix &aMatrix);

  Point4D& operator[](int aIndex)
  {
      NS_ABORT_IF_FALSE(aIndex >= 0 && aIndex <= 3, "Invalid matrix array index");
      return *reinterpret_cast<Point4D*>((&_11)+4*aIndex);
  }
  const Point4D& operator[](int aIndex) const
  {
      NS_ABORT_IF_FALSE(aIndex >= 0 && aIndex <= 3, "Invalid matrix array index");
      return *reinterpret_cast<const Point4D*>((&_11)+4*aIndex);
  }

  /**
   * Return true if this matrix and |aMatrix| are the same matrix.
   */
  bool operator==(const gfx3DMatrix& aMatrix) const;
  bool operator!=(const gfx3DMatrix& aMatrix) const;

  bool FuzzyEqual(const gfx3DMatrix& aMatrix) const;
  
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
  bool Is2D(gfxMatrix* aMatrix) const;
  bool Is2D() const;

  /**
   * Returns true if the matrix can be reduced to a 2D affine transformation
   * (i.e. as obtained by From2D). If it is, optionally returns the 2D
   * matrix in aMatrix. This should only be used on matrices required for
   * rendering, not for intermediate calculations. It is assumed that the 2D
   * matrix will only be used for transforming objects on to the z=0 plane,
   * therefore any z-component perspective is ignored. This means that if
   * aMatrix is applied to objects with z != 0, the results may be incorrect.
   *
   * Since drawing is to a 2d plane, any 3d transform without perspective
   * can be reduced by dropping the z row and column.
   */
  bool CanDraw2D(gfxMatrix* aMatrix = nullptr) const;

  /**
   * Converts the matrix to one that doesn't modify the z coordinate of points,
   * but leaves the rest of the transformation unchanged.
   */
  gfx3DMatrix& ProjectTo2D();

  /**
   * Returns true if the matrix is the identity matrix. The most important
   * property we require is that gfx3DMatrix().IsIdentity() returns true.
   */
  bool IsIdentity() const;

  /**
   * Pre-multiplication transformation functions:
   *
   * These functions construct a temporary matrix containing
   * a single transformation and pre-multiply it onto the current
   * matrix.
   */

  /**
   * Add a translation by aPoint to the matrix.
   *
   * This creates this temporary matrix:
   * |  1        0        0         0 |
   * |  0        1        0         0 |
   * |  0        0        1         0 |
   * |  aPoint.x aPoint.y aPoint.z  1 |
   */
  void Translate(const Point3D& aPoint);

  /** 
   * Skew the matrix.
   *
   * This creates this temporary matrix:
   * | 1           tan(aYSkew) 0 0 |
   * | tan(aXSkew) 1           0 0 |
   * | 0           0           1 0 |
   * | 0           0           0 1 |
   */
  void SkewXY(double aXSkew, double aYSkew);

  /**
   * Scale the matrix
   *
   * This creates this temporary matrix:
   * | aX 0  0  0 |
   * | 0  aY 0  0 |
   * | 0  0  aZ 0 |
   * | 0  0  0  1 |
   */
  void Scale(float aX, float aY, float aZ);

  /**
   * Return the currently set scaling factors.
   */
  float GetXScale() const { return _11; }
  float GetYScale() const { return _22; }
  float GetZScale() const { return _33; }

  /**
   * Rotate around the X axis..
   *
   * This creates this temporary matrix:
   * | 1 0            0           0 |
   * | 0 cos(aTheta)  sin(aTheta) 0 |
   * | 0 -sin(aTheta) cos(aTheta) 0 |
   * | 0 0            0           1 |
   */
  void RotateX(double aTheta);
  
  /**
   * Rotate around the Y axis..
   *
   * This creates this temporary matrix:
   * | cos(aTheta) 0 -sin(aTheta) 0 |
   * | 0           1 0            0 |
   * | sin(aTheta) 0 cos(aTheta)  0 |
   * | 0           0 0            1 |
   */
  void RotateY(double aTheta);
  
  /**
   * Rotate around the Z axis..
   *
   * This creates this temporary matrix:
   * | cos(aTheta)  sin(aTheta)  0 0 |
   * | -sin(aTheta) cos(aTheta)  0 0 |
   * | 0            0            1 0 |
   * | 0            0            0 1 |
   */
  void RotateZ(double aTheta);

  /**
   * Apply perspective to the matrix.
   *
   * This creates this temporary matrix:
   * | 1 0 0 0         |
   * | 0 1 0 0         |
   * | 0 0 1 -1/aDepth |
   * | 0 0 0 1         |
   */
  void Perspective(float aDepth);

  /**
   * Pre multiply an existing matrix onto the current
   * matrix
   */
  void PreMultiply(const gfx3DMatrix& aOther);
  void PreMultiply(const gfxMatrix& aOther);

  /**
   * Post-multiplication transformation functions:
   *
   * These functions construct a temporary matrix containing
   * a single transformation and post-multiply it onto the current
   * matrix.
   */

  /**
   * Add a translation by aPoint after the matrix.
   * This is functionally equivalent to:
   * matrix * gfx3DMatrix::Translation(aPoint)
   */
  void TranslatePost(const Point3D& aPoint);

  void ScalePost(float aX, float aY, float aZ);

  /**
   * Let T be the transformation matrix translating points in the coordinate
   * space with origin aOrigin to the coordinate space used by this matrix.
   * If this matrix is M, this function changes it to be (T-1)MT, the matrix
   * that's equivalent to M but in the coordinate space that treats aOrigin
   * as the origin.
   *
   * @param aOrigin The origin to translate to
   * @return The modified matrix
   */
  void ChangeBasis(const Point3D& aOrigin);

  /**
   * Transforms a point according to this matrix.
   */
  gfxPoint Transform(const gfxPoint& point) const;

  /**
   * Transforms a rectangle according to this matrix
   */
  gfxRect TransformBounds(const gfxRect& rect) const;


  gfxQuad TransformRect(const gfxRect& aRect) const;

  /** 
   * Transforms a 3D vector according to this matrix.
   */
  Point3D Transform3D(const Point3D& point) const;
  Point4D Transform4D(const Point4D& aPoint) const;

  /**
   * Given a point (x,y) find a value for z such that (x,y,z,1) transforms
   * into (x',y',0,w') and returns the latter.
   */
  Point4D ProjectPoint(const gfxPoint& aPoint) const;

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

  /**
   * Returns a unit vector that is perpendicular to the plane formed
   * by transform the screen plane (z=0) by this matrix.
   */
  Point3D GetNormalVector() const;

  /**
   * Returns true if a plane transformed by this matrix will
   * have it's back face visible.
   */
  bool IsBackfaceVisible() const;

  /**
   * Check if matrix is singular (no inverse exists).
   */
  bool IsSingular() const;

  /**
   * Create a translation matrix.
   *
   * \param aX Translation on X-axis.
   * \param aY Translation on Y-axis.
   * \param aZ Translation on Z-axis.
   */
  static gfx3DMatrix Translation(float aX, float aY, float aZ);
  static gfx3DMatrix Translation(const Point3D& aPoint);

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

  void NudgeToIntegers(void);
  void NudgeToIntegersFixedEpsilon();

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
