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
 *   Matt Woodrow <mwoodrow@mozilla.com>
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
#include "CSSCalc.h"
#include "nsStyleAnimation.h"

namespace css = mozilla::css;

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

/* Helper function to fill in an nscoord with the specified nsCSSValue. */
static nscoord CalcLength(const nsCSSValue &aValue,
                          nsStyleContext* aContext,
                          nsPresContext* aPresContext,
                          PRBool &aCanStoreInRuleTree)
{
  if (aValue.GetUnit() == eCSSUnit_Pixel) {
    // Handle this here (even though nsRuleNode::CalcLength handles it
    // fine) so that callers are allowed to pass a null style context
    // and pres context to SetToTransformFunction if they know (as
    // nsStyleAnimation does) that all lengths within the transform
    // function have already been computed to pixels and percents.
    return nsPresContext::CSSPixelsToAppUnits(aValue.GetFloatValue());
  }
  return nsRuleNode::CalcLength(aValue, aContext, aPresContext,
                                aCanStoreInRuleTree);
}

static void
ProcessTranslatePart(float& aResult,
                     const nsCSSValue& aValue,
                     nsStyleContext* aContext,
                     nsPresContext* aPresContext,
                     PRBool& aCanStoreInRuleTree,
                     nscoord aSize, float aAppUnitsPerMatrixUnit)
{
  nscoord offset = 0;
  float percent = 0.0f;

  if (aValue.GetUnit() == eCSSUnit_Percent) {
    percent = aValue.GetPercentValue();
  } else if (aValue.IsCalcUnit()) {
    nsRuleNode::ComputedCalc result =
      nsRuleNode::SpecifiedCalcToComputedCalc(aValue, aContext, aPresContext,
                                              aCanStoreInRuleTree);
    percent = result.mPercent;
    offset = result.mLength;
  } else {
    offset = CalcLength(aValue, aContext, aPresContext,
                         aCanStoreInRuleTree);
  }

  aResult = (percent * NSAppUnitsToFloatPixels(aSize, aAppUnitsPerMatrixUnit)) + 
            NSAppUnitsToFloatPixels(offset, aAppUnitsPerMatrixUnit);
}

/* Helper function to process a matrix entry. */
/* static */ gfx3DMatrix
nsStyleTransformMatrix::ProcessMatrix(const nsCSSValue::Array* aData,
                                      nsStyleContext* aContext,
                                      nsPresContext* aPresContext,
                                      PRBool& aCanStoreInRuleTree,
                                      nsRect& aBounds, float aAppUnitsPerMatrixUnit,
                                      PRBool *aPercentX, PRBool *aPercentY)
{
  NS_PRECONDITION(aData->Count() == 7, "Invalid array!");

  gfx3DMatrix result;

  /* Take the first four elements out of the array as floats and store
   * them.
   */
  result._11 = aData->Item(1).GetFloatValue();
  result._12 = aData->Item(2).GetFloatValue();
  result._21 = aData->Item(3).GetFloatValue();
  result._22 = aData->Item(4).GetFloatValue();

  /* The last two elements have their length parts stored in aDelta
   * and their percent parts stored in aX[0] and aY[1].
   */
  ProcessTranslatePart(result._41, aData->Item(5),
                       aContext, aPresContext, aCanStoreInRuleTree,
                       aBounds.Width(), aAppUnitsPerMatrixUnit);
  ProcessTranslatePart(result._42, aData->Item(6),
                       aContext, aPresContext, aCanStoreInRuleTree,
                       aBounds.Height(), aAppUnitsPerMatrixUnit);

  return result;
}

/* Helper function to process two matrices that we need to interpolate between */
/* static */ gfx3DMatrix
nsStyleTransformMatrix::ProcessInterpolateMatrix(const nsCSSValue::Array* aData,
                                                 nsStyleContext* aContext,
                                                 nsPresContext* aPresContext,
                                                 PRBool& aCanStoreInRuleTree,
                                                 nsRect& aBounds, float aAppUnitsPerMatrixUnit)
{
  NS_PRECONDITION(aData->Count() == 5, "Invalid array!");

  double coeff1 = aData->Item(1).GetPercentValue();
  gfx3DMatrix matrix1 = ReadTransforms(aData->Item(2).GetListValue(),
                                       aContext, aPresContext,
                                       aCanStoreInRuleTree,
                                       aBounds, aAppUnitsPerMatrixUnit);
  double coeff2 = aData->Item(3).GetPercentValue();
  gfx3DMatrix matrix2 = ReadTransforms(aData->Item(4).GetListValue(),
                                       aContext, aPresContext,
                                       aCanStoreInRuleTree,
                                       aBounds, aAppUnitsPerMatrixUnit);

  gfxMatrix matrix2d1, matrix2d2;
#ifdef DEBUG
  PRBool is2d =
#endif
  matrix1.Is2D(&matrix2d1);
  NS_ABORT_IF_FALSE(is2d, "Can't do animations with 3d transforms!");
#ifdef DEBUG
  is2d =
#endif
  matrix2.Is2D(&matrix2d2);
  NS_ABORT_IF_FALSE(is2d, "Can't do animations with 3d transforms!");

  return gfx3DMatrix::From2D(
    nsStyleAnimation::InterpolateTransformMatrix(matrix2d1, coeff1, 
                                                 matrix2d2, coeff2));
}

/* Helper function to process a translatex function. */
/* static */ gfx3DMatrix
nsStyleTransformMatrix::ProcessTranslateX(const nsCSSValue::Array* aData,
                                          nsStyleContext* aContext,
                                          nsPresContext* aPresContext,
                                          PRBool& aCanStoreInRuleTree,
                                          nsRect& aBounds, float aAppUnitsPerMatrixUnit)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");

  gfx3DMatrix temp;

  /* There are two cases.  If we have a number, we want our matrix to look
   * like this:
   *
   * |  1  0  0  0 |
   * |  0  1  0  0 |
   * |  0  0  1  0 |
   * | dx  0  0  1 |
   * So E = value
   * 
   * Otherwise, we might have a percentage, so we want to set the dX component
   * to the percent.
   */
  ProcessTranslatePart(temp._41, aData->Item(1),
                       aContext, aPresContext, aCanStoreInRuleTree,
                       aBounds.Width(), aAppUnitsPerMatrixUnit);
  return temp;
}

/* Helper function to process a translatey function. */
/* static */ gfx3DMatrix
nsStyleTransformMatrix::ProcessTranslateY(const nsCSSValue::Array* aData,
                                          nsStyleContext* aContext,
                                          nsPresContext* aPresContext,
                                          PRBool& aCanStoreInRuleTree,
                                          nsRect& aBounds, float aAppUnitsPerMatrixUnit)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");

  gfx3DMatrix temp;

  /* There are two cases.  If we have a number, we want our matrix to look
   * like this:
   *
   * |  1  0  0  0 |
   * |  0  1  0  0 |
   * |  0  0  1  0 |
   * |  0 dy  0  1 |
   * So E = value
   * 
   * Otherwise, we might have a percentage, so we want to set the dY component
   * to the percent.
   */
  ProcessTranslatePart(temp._42, aData->Item(1),
                       aContext, aPresContext, aCanStoreInRuleTree,
                       aBounds.Height(), aAppUnitsPerMatrixUnit);
  return temp;
}

/* Helper function to process a translate function. */
/* static */ gfx3DMatrix
nsStyleTransformMatrix::ProcessTranslate(const nsCSSValue::Array* aData,
                                         nsStyleContext* aContext,
                                         nsPresContext* aPresContext,
                                         PRBool& aCanStoreInRuleTree,
                                         nsRect& aBounds, float aAppUnitsPerMatrixUnit)
{
  NS_PRECONDITION(aData->Count() == 2 || aData->Count() == 3, "Invalid array!");

  gfx3DMatrix temp;

  /* There are several cases to consider.
   * First, we might have one value, or we might have two.  If we have
   * two, we need to consider both dX and dY components.
   * Next, the values might be lengths, or they might be percents.  If they're
   * percents, store them in the dX and dY components.  Otherwise, store them in
   * the main matrix.
   */

  ProcessTranslatePart(temp._41, aData->Item(1),
                       aContext, aPresContext, aCanStoreInRuleTree,
                       aBounds.Width(), aAppUnitsPerMatrixUnit);

  /* If we read in a Y component, set it appropriately */
  if (aData->Count() == 3) {
    ProcessTranslatePart(temp._42, aData->Item(2),
                         aContext, aPresContext, aCanStoreInRuleTree,
                         aBounds.Height(), aAppUnitsPerMatrixUnit);
  }
  return temp;
}

/* Helper function to set up a scale matrix. */
/* static */ gfx3DMatrix
nsStyleTransformMatrix::ProcessScaleHelper(float aXScale, float aYScale, float aZScale)
{
  /* We want our matrix to look like this:
   * | dx  0   0 0 |
   * |  0 dy   0 0 |
   * |  0  0  dz 0 |
   * |  0  0   0 1 |
   * So A = value
   */
  gfx3DMatrix temp;
  temp._11 = aXScale;
  temp._22 = aYScale;
  temp._33 = aZScale;
  return temp;
}

/* Process a scalex function. */
/* static */ gfx3DMatrix
nsStyleTransformMatrix::ProcessScaleX(const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2, "Bad array!");
  return ProcessScaleHelper(aData->Item(1).GetFloatValue(), 1.0f, 1.0f);
}

/* Process a scaley function. */
/* static */ gfx3DMatrix
nsStyleTransformMatrix::ProcessScaleY(const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2, "Bad array!");
  return ProcessScaleHelper(1.0f, aData->Item(1).GetFloatValue(), 1.0f);
}

/* Process a scale function. */
/* static */ gfx3DMatrix
nsStyleTransformMatrix::ProcessScale(const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2 || aData->Count() == 3, "Bad array!");
  /* We either have one element or two.  If we have one, it's for both X and Y.
   * Otherwise it's one for each.
   */
  const nsCSSValue& scaleX = aData->Item(1);
  const nsCSSValue& scaleY = (aData->Count() == 2 ? scaleX :
                              aData->Item(2));

  return ProcessScaleHelper(scaleX.GetFloatValue(),
                            scaleY.GetFloatValue(),
                            1.0f);
}

/* Helper function that, given a set of angles, constructs the appropriate
 * skew matrix.
 */
/* static */ gfx3DMatrix
nsStyleTransformMatrix::ProcessSkewHelper(double aXAngle, double aYAngle)
{
  /* We want our matrix to look like this:
   * | 1           tan(ThetaY) 0 0 |
   * | tan(ThetaX) 1           0 0 |
   * | 0           0           1 0 |
   * | 0           0           0 1 |
   * However, to avoid infinite values, we'll use the SafeTangent function
   * instead of the C standard tan function.
   */
  gfx3DMatrix temp;
  temp._12 = SafeTangent(aYAngle);
  temp._21 = SafeTangent(aXAngle);
  return temp;
}

/* Function that converts a skewx transform into a matrix. */
/* static */ gfx3DMatrix
nsStyleTransformMatrix::ProcessSkewX(const nsCSSValue::Array* aData)
{
  NS_ASSERTION(aData->Count() == 2, "Bad array!");
  return ProcessSkewHelper(aData->Item(1).GetAngleValueInRadians(), 0.0);
}

/* Function that converts a skewy transform into a matrix. */
/* static */ gfx3DMatrix
nsStyleTransformMatrix::ProcessSkewY(const nsCSSValue::Array* aData)
{
  NS_ASSERTION(aData->Count() == 2, "Bad array!");
  return ProcessSkewHelper(0.0, aData->Item(1).GetAngleValueInRadians());
}

/* Function that converts a skew transform into a matrix. */
/* static */ gfx3DMatrix
nsStyleTransformMatrix::ProcessSkew(const nsCSSValue::Array* aData)
{
  NS_ASSERTION(aData->Count() == 2 || aData->Count() == 3, "Bad array!");

  double xSkew = aData->Item(1).GetAngleValueInRadians();
  double ySkew = (aData->Count() == 2
                  ? 0.0 : aData->Item(2).GetAngleValueInRadians());

  return ProcessSkewHelper(xSkew, ySkew);
}

/* Function that converts a rotate transform into a matrix. */
/* static */ gfx3DMatrix
nsStyleTransformMatrix::ProcessRotateZ(const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");

  /* We want our matrix to look like this:
   * |  cos(theta)  sin(theta)  0 0 |
   * | -sin(theta)  cos(theta)  0 0 |
   * |           0           0  1 0 |
   * |           0           0  0 1 |
   * (see http://www.w3.org/TR/SVG/coords.html#RotationDefined)
   */
  double theta = aData->Item(1).GetAngleValueInRadians();
  float cosTheta = FlushToZero(cos(theta));
  float sinTheta = FlushToZero(sin(theta));

  gfx3DMatrix temp;

  temp._11 = cosTheta;
  temp._12 = sinTheta;
  temp._21 = -sinTheta;
  temp._22 = cosTheta;
  return temp;
}

/**
 * Return the transform function, as an nsCSSKeyword, for the given
 * nsCSSValue::Array from a transform list.
 */
/* static */ nsCSSKeyword
nsStyleTransformMatrix::TransformFunctionOf(const nsCSSValue::Array* aData)
{
  nsAutoString keyword;
  aData->Item(0).GetStringValue(keyword);
  return nsCSSKeywords::LookupKeyword(keyword);
}

/**
 * SetToTransformFunction is essentially a giant switch statement that fans
 * out to many smaller helper functions.
 */
gfx3DMatrix
nsStyleTransformMatrix::MatrixForTransformFunction(const nsCSSValue::Array * aData,
                                                   nsStyleContext* aContext,
                                                   nsPresContext* aPresContext,
                                                   PRBool& aCanStoreInRuleTree,
                                                   nsRect& aBounds, 
                                                   float aAppUnitsPerMatrixUnit)
{
  NS_PRECONDITION(aData, "Why did you want to get data from a null array?");
  // It's OK if aContext and aPresContext are null if the caller already
  // knows that all length units have been converted to pixels (as
  // nsStyleAnimation does).


  /* Get the keyword for the transform. */
  switch (TransformFunctionOf(aData)) {
  case eCSSKeyword_translatex:
    return ProcessTranslateX(aData, aContext, aPresContext,
                             aCanStoreInRuleTree, aBounds, aAppUnitsPerMatrixUnit);
  case eCSSKeyword_translatey:
    return ProcessTranslateY(aData, aContext, aPresContext,
                             aCanStoreInRuleTree, aBounds, aAppUnitsPerMatrixUnit);
  case eCSSKeyword_translate:
    return ProcessTranslate(aData, aContext, aPresContext,
                            aCanStoreInRuleTree, aBounds, aAppUnitsPerMatrixUnit);
  case eCSSKeyword_scalex:
    return ProcessScaleX(aData);
  case eCSSKeyword_scaley:
    return ProcessScaleY(aData);
  case eCSSKeyword_scale:
    return ProcessScale(aData);
  case eCSSKeyword_skewx:
    return ProcessSkewX(aData);
  case eCSSKeyword_skewy:
    return ProcessSkewY(aData);
  case eCSSKeyword_skew:
    return ProcessSkew(aData);
  case eCSSKeyword_rotate:
    return ProcessRotateZ(aData);
  case eCSSKeyword_matrix:
    return ProcessMatrix(aData, aContext, aPresContext,
                         aCanStoreInRuleTree, aBounds, aAppUnitsPerMatrixUnit);
  case eCSSKeyword_interpolatematrix:
    return ProcessInterpolateMatrix(aData, aContext, aPresContext,
                                    aCanStoreInRuleTree, aBounds, aAppUnitsPerMatrixUnit);
  default:
    NS_NOTREACHED("Unknown transform function!");
  }
  return gfx3DMatrix();
}

/* static */ gfx3DMatrix
nsStyleTransformMatrix::ReadTransforms(const nsCSSValueList* aList,
                                       nsStyleContext* aContext,
                                       nsPresContext* aPresContext,
                                       PRBool &aCanStoreInRuleTree,
                                       nsRect& aBounds,
                                       float aAppUnitsPerMatrixUnit)
{
  gfx3DMatrix result;

  for (const nsCSSValueList* curr = aList; curr != nsnull; curr = curr->mNext) {
    const nsCSSValue &currElem = curr->mValue;
    NS_ASSERTION(currElem.GetUnit() == eCSSUnit_Function,
                 "Stream should consist solely of functions!");
    NS_ASSERTION(currElem.GetArrayValue()->Count() >= 1,
                 "Incoming function is too short!");

    /* Read in a single transform matrix, then accumulate it with the total. */
    result = MatrixForTransformFunction(currElem.GetArrayValue(), aContext,
                                        aPresContext, aCanStoreInRuleTree,
                                        aBounds, aAppUnitsPerMatrixUnit) * result;
  }
  
  return result;
}

