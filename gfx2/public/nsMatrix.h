/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */


/**
 * @file nsMatrix.h
 */

/**
 * nsMatrix class
 * @class nsMatrix
 *
 * A 2x3 matrices of the form: 
 *
 * \f$
\left[ \begin{array}{ccc}
a & c & e\\
b & d & f
\end{array} \right]
 * \f$
 *
 * which, when expanded into a 3x3 matrix for the purposes of matrix arithmetic, become: 
 *
 * \f$
\left[ \begin{array}{ccc}
a & c & e\\
b & d & f\\
0 & 0 & 1
\end{array} \right]
 * \f$
 *
 * @see nsTransform
 */
class nsMatrix { 

 private:
  float a;
  float b;
  float c;
  float d;
  float e;
  float f;

 public:
  nsMatrix();
  virtual ~nsMatrix();

  float GetA() const { return a; };
  float GetB() const { return b; };
  float GetC() const { return c; };
  float GetD() const { return d; };
  float GetE() const { return e; };
  float GetF() const { return f; };


  /**
   * Performs matrix multiplication. This matrix is post-multiplied by another matrix, returning the resulting new matrix. 
   *
   * @param secondMatrix The matrix which is post-multiplied to this matrix. 
   *
   * @return The resulting matrix.
   */
  nsMatrix *Multiply(const nsMatrix &secondMatrix);

  /**
   * Returns the inverse matrix.
   *
   * @return The inverse matrix
   */
  nsMatrix *Inverse();

  /**
   * Post-multiplies a translation transformation on the current matrix and returns the resulting matrix. 
   * 
   * @param x The distance to translate along the x-axis.
   * @param y The distance to translate along the y-axis.
   *
   * @return The resulting matrix.
   */
  nsMatrix *Translate(float x, float y);

  /**
   * Post-multiplies a uniform scale transformation on the current matrix and returns the resulting matrix.
   *
   * @param scaleFactor Scale factor in both X and Y.
   *
   * @return The resulting matrix.
   */
  nsMatrix *Scale(float scaleFactor);


  /**
   * Post-multiplies a non-uniform scale transformation on the current matrix and returns the resulting matrix.
   *
   * @param scaleFactorX Scale factor in X.
   * @param scaleFactorY Scale factor in Y.
   *
   * @return The resulting matrix.
   */
  nsMatrix *ScaleNonUniform(float scaleFactorX, float scaleFactorY);

  /**
   * Post-multiplies a rotation transformation on the current matrix and returns the resulting matrix.
   * 
   * @param angle Rotation angle.
   *
   * @return The resulting matrix.
   */
  nsMatrix *Rotate(float angle);

  /**
   * Post-multiplies a rotation transformation on the current matrix and returns the resulting matrix.
   * The rotation angle is determined by taking (+/-) atan(\a y/ \a x). The direction of the vector (\a x, \a y) determines
   * whether the positive or negative angle value is used.
   *
   * @param x The X coordinate of the vector (\a x, \a y). Must not be zero. 
   * @param y The Y coordinate of the vector (\a x, \a y). Must not be zero. 
   *
   * @return The resulting matrix.
   */
  nsMatrix *RotateFromVector(float x, float y);

  /**
   * Post-multiplies the transformation \f$
\left[ \begin{array}{ccc}
-1 & 0 & 0\\
1 & 0 & 0\\
\end{array} \right]
   * \f$ and returns the resulting matrix. 
   *
   * @return The resulting matrix.
   */
  nsMatrix *FlipX();

  /**
   * Post-multiplies the transformation \f$
\left[ \begin{array}{ccc}
1 & 0 & 0\\
-1 & 0 & 0\\
\end{array} \right]
   * \f$ and returns the resulting matrix. 
   *
   * @return The resulting matrix.
   */
  nsMatrix *FlipY();

  /**
   * Post-multiplies a skewX transformation on the current matrix and returns the resulting matrix.
   *
   * @param angle Skew angle.
   *
   * @return The resulting matrix.
   */
  nsMatrix *SkewX(float angle);

  /**
   * Post-multiplies a skewY transformation on the current matrix and returns the resulting matrix.
   *
   * @param angle Skew angle.
   *
   * @return The resulting matrix.
   */
  nsMatrix *SkewY(float angle);
};
