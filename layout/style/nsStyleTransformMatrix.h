/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation
 *
 * Contributor(s):
 *   Keith Schwarz <kschwarz@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 * A class representing three matrices that can be used for style transforms.
 */

#ifndef nsStyleTransformMatrix_h_
#define nsStyleTransformMatrix_h_

#include "nsCSSValue.h"
#include "gfxMatrix.h"
#include "nsRect.h"

/**
 * A class representing a style transformation matrix.  The class actually
 * wraps three different matrices, a constant matrix and two matrices
 * whose values are scaled by the width and the height of the bounding
 * rectangle for the object to transform.  Thus, given a frame rectangle
 * of dimensions (width, height) and a point (x, y) to transform, the matrix
 * corresponds to the transform operation
 *
 * | a c e |   |0 0 dX1|           |0 0 dY1|           | x |
 *(| b d f | + |0 0 dX2| (width) + |0 0 dY2| (height)) | y |
 * | 0 0 1 |   |0 0   0|           |0 0   0|           | 1 |
 *
 * Note that unlike the Thebes gfxMatrix, vectors are column vectors and
 * consequently the multiplication of a matrix A and a vector x is Ax, not xA.
 */
class nsStyleContext;
class nsPresContext;
class nsStyleTransformMatrix
{
 public:
  /**
   * Constructor sets the matrix to the identity.
   */
  nsStyleTransformMatrix();

  /**
   * Given a frame's bounding rectangle, returns a gfxMatrix
   * corresponding to the transformation represented by this
   * matrix.  The transformation takes points in the frame's
   * local space and converts them to points in the frame's
   * transformed space.
   *
   * @param aBounds The frame's bounding rectangle.
   * @param aFactor The number of app units per device pixel.
   * @return A Thebes matrix corresponding to the transform.
   */
  gfxMatrix GetThebesMatrix(const nsRect& aBounds, float aFactor) const;

  /**
   * Multiplies this matrix by another matrix, in that order.  If A'
   * is the value of A after A *= B, then for any vector x, the
   * equivalence A'(x) == A(B(x)) holds.
   *
   * @param aOther The matrix to multiply this matrix by.
   * @return A reference to this matrix.
   */
  nsStyleTransformMatrix& operator *= (const nsStyleTransformMatrix &aOther);

  /**
   * Returns a new nsStyleTransformMatrix that is equal to one matrix
   * multiplied by another matrix, in that order.  If C is the result of
   * A * B, then for any vector x, the equivalence C(x) = A(B(x)).
   *
   * @param aOther The matrix to multiply this matrix by.
   * @return A new nsStyleTransformMatrix equal to this matrix multiplied
   *         by the other matrix.
   */
  const nsStyleTransformMatrix
    operator * (const nsStyleTransformMatrix &aOther) const;

  /**
   * Given an nsCSSValue::Array* containing a -moz-transform function,
   * updates this matrix to hold the value of that function.
   *
   * @param aData The nsCSSValue::Array* containing the transform function.
   * @param aContext The style context, used for unit conversion.
   * @param aPresContext The presentation context, used for unit conversion.
   * @param aCanStoreInRuleTree Set to false if the result cannot be cached
   *                            in the rule tree, otherwise untouched.
   */
  void SetToTransformFunction(const nsCSSValue::Array* aData,
                              nsStyleContext* aContext,
                              nsPresContext* aPresContext,
                              PRBool& aCanStoreInRuleTree);

  /**
   * Sets this matrix to be the identity matrix.
   */
  void SetToIdentity();

  /**
   * Returns the value of the entry at the 2x2 submatrix of the
   * transform matrix that defines the non-affine linear transform.
   * The order is given as
   * |elem[0]  elem[2]|
   * |elem[1]  elem[3]|
   *
   * @param aIndex The element index.
   * @return The value of the element at that index.
   */
  float GetMainMatrixEntry(PRInt32 aIndex) const
  {
    NS_PRECONDITION(aIndex >= 0 && aIndex < 4, "Index out of bounds!");
    return mMain[aIndex];
  }

  /**
   * Returns the value of the X or Y translation component of the matrix,
   * given the specified bounds.
   *
   * @param aBounds The bounds of the element.
   * @return The value of the X or Ytranslation component.
   */
  nscoord GetXTranslation(const nsRect& aBounds) const;
  nscoord GetYTranslation(const nsRect& aBounds) const;

  /**
   * Returns whether the two matrices are equal or not.
   *
   * @param aOther The matrix to compare to.
   * @return Whether the two matrices are equal.
   */
  PRBool operator== (const nsStyleTransformMatrix& aOther) const;
  PRBool operator!= (const nsStyleTransformMatrix& aOther) const
  {
    return !(*this == aOther);
  }

 private:
  /* The three matrices look like this:
   * |mMain[0] mMain[2] mDelta[0]|
   * |mMain[1] mMain[3] mDelta[1]| <-- Constant matrix
   * |       0        0         1|
   *
   * |       0        0     mX[0]|
   * |       0        0     mX[1]| <-- Scaled by width of element
   * |       0        0         1|
   *
   * |       0        0     mY[0]|
   * |       0        0     mY[1]| <-- Scaled by height of element
   * |       0        0         1|
   */
  float mMain[4];
  nscoord mDelta[2];
  float mX[2];
  float mY[2];
};

#endif
