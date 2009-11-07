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
 * A class used for intermediate representations of the -moz-transform property.
 */

#include "nsStyleTransformMatrix.h"
#include "nsAutoPtr.h"
#include "nsCSSValue.h"
#include "nsStyleContext.h"
#include "nsPresContext.h"
#include "nsRuleNode.h"
#include "nsCSSKeywords.h"
#include "nsMathUtils.h"

/* Note on floating point precision: The transform matrix is an array
 * of single precision 'float's, and so are most of the input values
 * we get from the style system, but intermediate calculations
 * involving angles need to be done in 'double'.
 */

/* Force small values to zero.  We do this to avoid having sin(360deg)
 * evaluate to a tiny but nonzero value.
 */
static double FlushToZero(double aVal)
{
  if (-FLT_EPSILON < aVal && aVal < FLT_EPSILON)
    return 0.0f;
  else
    return aVal;
}

/* Computes tan(aTheta).  For values of aTheta such that tan(aTheta) is
 * undefined or very large, SafeTangent returns a manageably large value
 * of the correct sign.
 */
static double SafeTangent(double aTheta)
{
  const double kEpsilon = 0.0001;

  /* tan(theta) = sin(theta)/cos(theta); problems arise when
   * cos(theta) is too close to zero.  Limit cos(theta) to the
   * range [-1, -epsilon] U [epsilon, 1].
   */
  double sinTheta = sin(aTheta);
  double cosTheta = cos(aTheta);

  if (cosTheta >= 0 && cosTheta < kEpsilon)
    cosTheta = kEpsilon;
  else if (cosTheta < 0 && cosTheta >= -kEpsilon)
    cosTheta = -kEpsilon;

  return FlushToZero(sinTheta / cosTheta);
}

/* Constructor sets the data to the identity matrix. */
nsStyleTransformMatrix::nsStyleTransformMatrix()
{
  SetToIdentity();
}

/* SetToIdentity just fills in the appropriate values. */
void nsStyleTransformMatrix::SetToIdentity()
{
    /* Set the main matrix to the identity. */
  mMain[0] = 1.0f;
  mMain[1] = 0.0f;
  mMain[2] = 0.0f;
  mMain[3] = 1.0f;
  mDelta[0] = 0;
  mDelta[1] = 0;

  /* Both translation matrices are zero. */
  mX[0] = 0.0f;
  mX[1] = 0.0f;
  mY[0] = 0.0f;
  mY[1] = 0.0f;
}

/* Adds the constant translation to the scale factor translation components. */
nscoord nsStyleTransformMatrix::GetXTranslation(const nsRect& aBounds) const
{
  return NSToCoordRound(aBounds.width * mX[0] + aBounds.height * mY[0]) +
    mDelta[0];
}
nscoord nsStyleTransformMatrix::GetYTranslation(const nsRect& aBounds) const
{
  return NSToCoordRound(aBounds.width * mX[1] + aBounds.height * mY[1]) +
    mDelta[1];
}

/* GetThebesMatrix converts the stored matrix in a few steps. */
gfxMatrix nsStyleTransformMatrix::GetThebesMatrix(const nsRect& aBounds,
                                                  float aScale) const
{
  /* Compute the graphics matrix.  We take the stored main elements, along with
   * the delta, and add in the matrices:
   *
   * | 0 0 dx1|
   * | 0 0 dx2| * width
   * | 0 0   0|
   *
   * | 0 0 dy1|
   * | 0 0 dy2| * height
   * | 0 0   0|
   */
  return gfxMatrix(mMain[0], mMain[1], mMain[2], mMain[3],
                   NSAppUnitsToFloatPixels(GetXTranslation(aBounds), aScale),
                   NSAppUnitsToFloatPixels(GetYTranslation(aBounds), aScale));
}

/* Performs the matrix multiplication necessary to multiply the two matrices,
 * then hands back a reference to ourself.
 */
nsStyleTransformMatrix&
nsStyleTransformMatrix::operator *= (const nsStyleTransformMatrix &aOther)
{
  /* We'll buffer all of our results into a temporary storage location
   * during this operation since we don't want to overwrite the values of
   * the old matrix with the values of the new.
   */
  float newMatrix[4];
  nscoord newDelta[2];
  float newX[2];
  float newY[2];
  
  /*   [this]    [aOther]
   * |a1 c1 e1| |a0 c0 e0|   |a0a1 + b0c1    c0a1 + d0c1     e0a1 + f0c1 + e1|
   * |b1 d1 f1|x|b0 d0 f0| = |a0b1 + b0d1    c0b1 + d0d1     e0b1 + f0d1 + f1|
   * |0  0  1 | | 0  0  1|   |          0              0                    1|
   */
  newMatrix[0] = aOther.mMain[0] * mMain[0] + aOther.mMain[1] * mMain[2];
  newMatrix[1] = aOther.mMain[0] * mMain[1] + aOther.mMain[1] * mMain[3];
  newMatrix[2] = aOther.mMain[2] * mMain[0] + aOther.mMain[3] * mMain[2];
  newMatrix[3] = aOther.mMain[2] * mMain[1] + aOther.mMain[3] * mMain[3];
  newDelta[0] = NSToCoordRound(aOther.mDelta[0] * mMain[0] +
                               aOther.mDelta[1] * mMain[2]) + mDelta[0];
  newDelta[1] = NSToCoordRound(aOther.mDelta[0] * mMain[1] +
                               aOther.mDelta[1] * mMain[3]) + mDelta[1];

  /* For consistent terminology, let u0, u1, v0, and v1 be the four transform
   * coordinates from our matrix, and let x0, x1, y0, and y1 be the four
   * transform coordinates from the other  matrix.  Then the new transform
   * coordinates are:
   *
   * u0' = a1u0 + c1u1 + x0
   * u1' = b1u0 + d1u1 + x1
   * v0' = a1v0 + c1v1 + y0
   * v1' = b1v0 + d1v1 + y1
   */
  newX[0] = mMain[0] * aOther.mX[0] + mMain[2] * aOther.mX[1] + mX[0];
  newX[1] = mMain[1] * aOther.mX[0] + mMain[3] * aOther.mX[1] + mX[1];
  newY[0] = mMain[0] * aOther.mY[0] + mMain[2] * aOther.mY[1] + mY[0];
  newY[1] = mMain[1] * aOther.mY[0] + mMain[3] * aOther.mY[1] + mY[1];

  /* Now, write everything back in. */
  for (PRInt32 index = 0; index < 4; ++index)
    mMain[index] = newMatrix[index];
  for (PRInt32 index = 0; index < 2; ++index) {
    mDelta[index] = newDelta[index];
    mX[index] = newX[index];
    mY[index] = newY[index];
  }

  /* As promised, return a reference to ourselves. */
  return *this;
}

/* op* is implemented in terms of op*=. */
const nsStyleTransformMatrix
nsStyleTransformMatrix::operator *(const nsStyleTransformMatrix &aOther) const
{
  return nsStyleTransformMatrix(*this) *= aOther;
}

/* Helper function to fill in an nscoord with the specified nsCSSValue. */
static void SetCoordToValue(const nsCSSValue &aValue,
                            nsStyleContext* aContext,
                            nsPresContext* aPresContext,
                            PRBool &aCanStoreInRuleTree, nscoord &aOut)
{
  aOut = nsRuleNode::CalcLength(aValue, aContext, aPresContext,
                                aCanStoreInRuleTree);
}

/* Helper function to process a matrix entry. */
static void ProcessMatrix(float aMain[4], nscoord aDelta[2],
                          float aX[2], float aY[2],
                          const nsCSSValue::Array* aData,
                          nsStyleContext* aContext,
                          nsPresContext* aPresContext,
                          PRBool& aCanStoreInRuleTree)
{
  NS_PRECONDITION(aData->Count() == 7, "Invalid array!");

  /* Take the first four elements out of the array as floats and store
   * them in aMain.
   */
  for (PRUint16 index = 1; index <= 4; ++index)
    aMain[index - 1] = aData->Item(index).GetFloatValue();

  /* For the fifth element, if it's a percentage, store it in aX[0].
   * Otherwise, it's a length that needs to go in aDelta[0]
   */
  if (aData->Item(5).GetUnit() == eCSSUnit_Percent)
    aX[0] = aData->Item(5).GetPercentValue();
  else
    SetCoordToValue(aData->Item(5), aContext, aPresContext, aCanStoreInRuleTree,
                    aDelta[0]);

  /* For the final element, if it's a percentage, store it in aY[1].
   * Otherwise, it's a length that needs to go in aDelta[1].
   */
  if (aData->Item(6).GetUnit() == eCSSUnit_Percent)
    aY[1] = aData->Item(6).GetPercentValue();
  else
    SetCoordToValue(aData->Item(6), aContext, aPresContext, aCanStoreInRuleTree,
                    aDelta[1]);
}

/* Helper function to process a translatex function. */
static void ProcessTranslateX(nscoord aDelta[2], float aX[2],
                              const nsCSSValue::Array* aData,
                              nsStyleContext* aContext,
                              nsPresContext* aPresContext,
                              PRBool& aCanStoreInRuleTree)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");

  /* There are two cases.  If we have a number, we want our matrix to look
   * like this:
   *
   * |  1  0 dx|
   * |  0  1  0|
   * |  0  0  1|
   * So E = value
   * 
   * Otherwise, we might have a percentage, so we want to set the dX component
   * to the percent.
   */
  if (aData->Item(1).GetUnit() != eCSSUnit_Percent)
    SetCoordToValue(aData->Item(1), aContext, aPresContext, aCanStoreInRuleTree,
                    aDelta[0]);
  else
    aX[0] = aData->Item(1).GetPercentValue();
}

/* Helper function to process a translatey function. */
static void ProcessTranslateY(nscoord aDelta[2], float aY[2],
                              const nsCSSValue::Array* aData,
                              nsStyleContext* aContext,
                              nsPresContext* aPresContext,
                              PRBool& aCanStoreInRuleTree)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");

  /* There are two cases.  If we have a number, we want our matrix to look
   * like this:
   *
   * |  1  0  0|
   * |  0  1 dy|
   * |  0  0  1|
   * So E = value
   * 
   * Otherwise, we might have a percentage, so we want to set the dY component
   * to the percent.
   */
  if (aData->Item(1).GetUnit() != eCSSUnit_Percent)
    SetCoordToValue(aData->Item(1), aContext, aPresContext, aCanStoreInRuleTree,
                    aDelta[1]);
  else
    aY[1] = aData->Item(1).GetPercentValue();
}

/* Helper function to process a translate function. */
static void ProcessTranslate(nscoord aDelta[2], float aX[2], float aY[2],
                             const nsCSSValue::Array* aData,
                             nsStyleContext* aContext,
                             nsPresContext* aPresContext,
                             PRBool& aCanStoreInRuleTree)
{
  NS_PRECONDITION(aData->Count() == 2 || aData->Count() == 3, "Invalid array!");

  /* There are several cases to consider.
   * First, we might have one value, or we might have two.  If we have
   * two, we need to consider both dX and dY components.
   * Next, the values might be lengths, or they might be percents.  If they're
   * percents, store them in the dX and dY components.  Otherwise, store them in
   * the main matrix.
   */

  const nsCSSValue &dx = aData->Item(1);
  if (dx.GetUnit() == eCSSUnit_Percent)
    aX[0] = dx.GetPercentValue();
  else
    SetCoordToValue(dx, aContext, aPresContext, aCanStoreInRuleTree, aDelta[0]);

  /* If we read in a Y component, set it appropriately */
  if (aData->Count() == 3) {
    const nsCSSValue &dy = aData->Item(2);
    if (dy.GetUnit() == eCSSUnit_Percent)
      aY[1] = dy.GetPercentValue();
    else
      SetCoordToValue(dy, aContext, aPresContext, aCanStoreInRuleTree,
                      aDelta[1]); 
  }
}

/* Helper function to set up a scale matrix. */
static void ProcessScaleHelper(float aXScale, float aYScale, float aMain[4])
{
  /* We want our matrix to look like this:
   * | dx  0  0|
   * |  0 dy  0|
   * |  0  0  1|
   * So A = value
   */
  aMain[0] = aXScale;
  aMain[3] = aYScale;
}

/* Process a scalex function. */
static void ProcessScaleX(float aMain[4], const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2, "Bad array!");
  ProcessScaleHelper(aData->Item(1).GetFloatValue(), 1.0f, aMain);
}

/* Process a scaley function. */
static void ProcessScaleY(float aMain[4], const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2, "Bad array!");
  ProcessScaleHelper(1.0f, aData->Item(1).GetFloatValue(), aMain);
}

/* Process a scale function. */
static void ProcessScale(float aMain[4], const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2 || aData->Count() == 3, "Bad array!");
  /* We either have one element or two.  If we have one, it's for both X and Y.
   * Otherwise it's one for each.
   */
  const nsCSSValue& scaleX = aData->Item(1);
  const nsCSSValue& scaleY = (aData->Count() == 2 ? scaleX :
                              aData->Item(2));

  ProcessScaleHelper(scaleX.GetFloatValue(),
                     scaleY.GetFloatValue(), aMain);
}

/* Helper function that, given a set of angles, constructs the appropriate
 * skew matrix.
 */
static void ProcessSkewHelper(double aXAngle, double aYAngle, float aMain[4])
{
  /* We want our matrix to look like this:
   * |  1           tan(ThetaX)  0|
   * |  tan(ThetaY) 1            0|
   * |  0           0            1|
   * However, to avoid infinite values, we'll use the SafeTangent function
   * instead of the C standard tan function.
   */
  aMain[2] = SafeTangent(aXAngle);
  aMain[1] = SafeTangent(aYAngle);
}

/* Function that converts a skewx transform into a matrix. */
static void ProcessSkewX(float aMain[4], const nsCSSValue::Array* aData)
{
  NS_ASSERTION(aData->Count() == 2, "Bad array!");
  ProcessSkewHelper(aData->Item(1).GetAngleValueInRadians(), 0.0, aMain);
}

/* Function that converts a skewy transform into a matrix. */
static void ProcessSkewY(float aMain[4], const nsCSSValue::Array* aData)
{
  NS_ASSERTION(aData->Count() == 2, "Bad array!");
  ProcessSkewHelper(0.0, aData->Item(1).GetAngleValueInRadians(), aMain);
}

/* Function that converts a skew transform into a matrix. */
static void ProcessSkew(float aMain[4], const nsCSSValue::Array* aData)
{
  NS_ASSERTION(aData->Count() == 2 || aData->Count() == 3, "Bad array!");

  double xSkew = aData->Item(1).GetAngleValueInRadians();
  double ySkew = (aData->Count() == 2
                  ? 0.0 : aData->Item(2).GetAngleValueInRadians());

  ProcessSkewHelper(xSkew, ySkew, aMain);
}

/* Function that converts a rotate transform into a matrix. */
static void ProcessRotate(float aMain[4], const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");

  /* We want our matrix to look like this:
   * |  cos(theta)  -sin(theta)  0|
   * |  sin(theta)   cos(theta)  0|
   * |           0            0  1|
   * (see http://www.w3.org/TR/SVG/coords.html#RotationDefined)
   */
  double theta = aData->Item(1).GetAngleValueInRadians();
  float cosTheta = FlushToZero(cos(theta));
  float sinTheta = FlushToZero(sin(theta));

  aMain[0] = cosTheta;
  aMain[1] = sinTheta;
  aMain[2] = -sinTheta;
  aMain[3] = cosTheta;
}

/**
 * SetToTransformFunction is essentially a giant switch statement that fans
 * out to many smaller helper functions.
 */
void
nsStyleTransformMatrix::SetToTransformFunction(const nsCSSValue::Array * aData,
                                               nsStyleContext* aContext,
                                               nsPresContext* aPresContext,
                                               PRBool& aCanStoreInRuleTree)
{
  NS_PRECONDITION(aData, "Why did you want to get data from a null array?");
  NS_PRECONDITION(aContext, "Need a context for unit conversion!");
  NS_PRECONDITION(aPresContext, "Need a context for unit conversion!");
  
  /* Reset the matrix to the identity so that each subfunction can just
   * worry about its own components.
   */
  SetToIdentity();

  /* Get the keyword for the transform. */
  nsAutoString keyword;
  aData->Item(0).GetStringValue(keyword);
  switch (nsCSSKeywords::LookupKeyword(keyword)) {
  case eCSSKeyword_translatex:
    ProcessTranslateX(mDelta, mX, aData, aContext, aPresContext,
                      aCanStoreInRuleTree);
    break;
  case eCSSKeyword_translatey:
    ProcessTranslateY(mDelta, mY, aData, aContext, aPresContext,
                      aCanStoreInRuleTree);
    break;
  case eCSSKeyword_translate:
    ProcessTranslate(mDelta, mX, mY, aData, aContext, aPresContext,
                     aCanStoreInRuleTree);
    break;
  case eCSSKeyword_scalex:
    ProcessScaleX(mMain, aData);
    break;
  case eCSSKeyword_scaley:
    ProcessScaleY(mMain, aData);
    break;
  case eCSSKeyword_scale:
    ProcessScale(mMain, aData);
    break;
  case eCSSKeyword_skewx:
    ProcessSkewX(mMain, aData);
    break;
  case eCSSKeyword_skewy:
    ProcessSkewY(mMain, aData);
    break;
  case eCSSKeyword_skew:
    ProcessSkew(mMain, aData);
    break;
  case eCSSKeyword_rotate:
    ProcessRotate(mMain, aData);
    break;
  case eCSSKeyword_matrix:
    ProcessMatrix(mMain, mDelta, mX, mY, aData, aContext, aPresContext,
                  aCanStoreInRuleTree);
    break;
  default:
    NS_NOTREACHED("Unknown transform function!");
  }
}

/* Does an element-by-element comparison and returns whether or not the
 * matrices are equal.
 */
PRBool
nsStyleTransformMatrix::operator ==(const nsStyleTransformMatrix &aOther) const
{
  for (PRInt32 index = 0; index < 4; ++index)
    if (mMain[index] != aOther.mMain[index])
      return PR_FALSE;

  for (PRInt32 index = 0; index < 2; ++index)
    if (mDelta[index] != aOther.mDelta[index] ||
        mX[index] != aOther.mX[index] ||
        mY[index] != aOther.mY[index])
      return PR_FALSE;

  return PR_TRUE;
}
