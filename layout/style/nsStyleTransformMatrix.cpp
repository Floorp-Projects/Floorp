/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class used for intermediate representations of the -moz-transform property.
 */

#include "nsStyleTransformMatrix.h"
#include "nsCSSValue.h"
#include "nsPresContext.h"
#include "nsRuleNode.h"
#include "nsCSSKeywords.h"
#include "mozilla/StyleAnimationValue.h"
#include "gfxMatrix.h"

using namespace mozilla;

namespace nsStyleTransformMatrix {

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

float
ProcessTranslatePart(const nsCSSValue& aValue,
                     nsStyleContext* aContext,
                     nsPresContext* aPresContext,
                     bool& aCanStoreInRuleTree,
                     nscoord aSize)
{
  nscoord offset = 0;
  float percent = 0.0f;

  if (aValue.GetUnit() == eCSSUnit_Percent) {
    percent = aValue.GetPercentValue();
  } else if (aValue.GetUnit() == eCSSUnit_Pixel ||
             aValue.GetUnit() == eCSSUnit_Number) {
    // Handle this here (even though nsRuleNode::CalcLength handles it
    // fine) so that callers are allowed to pass a null style context
    // and pres context to SetToTransformFunction if they know (as
    // StyleAnimationValue does) that all lengths within the transform
    // function have already been computed to pixels and percents.
    //
    // Raw numbers are treated as being pixels.
    //
    // Don't convert to aValue to AppUnits here to avoid precision issues.
    return aValue.GetFloatValue();
  } else if (aValue.IsCalcUnit()) {
    nsRuleNode::ComputedCalc result =
      nsRuleNode::SpecifiedCalcToComputedCalc(aValue, aContext, aPresContext,
                                              aCanStoreInRuleTree);
    percent = result.mPercent;
    offset = result.mLength;
  } else {
    offset = nsRuleNode::CalcLength(aValue, aContext, aPresContext,
                                    aCanStoreInRuleTree);
  }

  return (percent * NSAppUnitsToFloatPixels(aSize, nsPresContext::AppUnitsPerCSSPixel())) +
         NSAppUnitsToFloatPixels(offset, nsPresContext::AppUnitsPerCSSPixel());
}

/**
 * Helper functions to process all the transformation function types.
 *
 * These take a matrix parameter to accumulate the current matrix.
 */

/* Helper function to process a matrix entry. */
static void
ProcessMatrix(gfx3DMatrix& aMatrix,
              const nsCSSValue::Array* aData,
              nsStyleContext* aContext,
              nsPresContext* aPresContext,
              bool& aCanStoreInRuleTree,
              nsRect& aBounds)
{
  NS_PRECONDITION(aData->Count() == 7, "Invalid array!");

  gfxMatrix result;

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
  result._31 = ProcessTranslatePart(aData->Item(5),
                                   aContext, aPresContext, aCanStoreInRuleTree,
                                   aBounds.Width());
  result._32 = ProcessTranslatePart(aData->Item(6),
                                   aContext, aPresContext, aCanStoreInRuleTree,
                                   aBounds.Height());

  aMatrix.PreMultiply(result);
}

static void 
ProcessMatrix3D(gfx3DMatrix& aMatrix,
                const nsCSSValue::Array* aData,
                nsStyleContext* aContext,
                nsPresContext* aPresContext,
                bool& aCanStoreInRuleTree,
                nsRect& aBounds)
{
  NS_PRECONDITION(aData->Count() == 17, "Invalid array!");

  gfx3DMatrix temp;

  temp._11 = aData->Item(1).GetFloatValue();
  temp._12 = aData->Item(2).GetFloatValue();
  temp._13 = aData->Item(3).GetFloatValue();
  temp._14 = aData->Item(4).GetFloatValue();
  temp._21 = aData->Item(5).GetFloatValue();
  temp._22 = aData->Item(6).GetFloatValue();
  temp._23 = aData->Item(7).GetFloatValue();
  temp._24 = aData->Item(8).GetFloatValue();
  temp._31 = aData->Item(9).GetFloatValue();
  temp._32 = aData->Item(10).GetFloatValue();
  temp._33 = aData->Item(11).GetFloatValue();
  temp._34 = aData->Item(12).GetFloatValue();
  temp._44 = aData->Item(16).GetFloatValue();

  temp._41 = ProcessTranslatePart(aData->Item(13),
                                  aContext, aPresContext, aCanStoreInRuleTree,
                                  aBounds.Width());
  temp._42 = ProcessTranslatePart(aData->Item(14),
                                  aContext, aPresContext, aCanStoreInRuleTree,
                                  aBounds.Height());
  temp._43 = ProcessTranslatePart(aData->Item(15),
                                  aContext, aPresContext, aCanStoreInRuleTree,
                                  aBounds.Height());

  aMatrix.PreMultiply(temp);
}

/* Helper function to process two matrices that we need to interpolate between */
void
ProcessInterpolateMatrix(gfx3DMatrix& aMatrix,
                         const nsCSSValue::Array* aData,
                         nsStyleContext* aContext,
                         nsPresContext* aPresContext,
                         bool& aCanStoreInRuleTree,
                         nsRect& aBounds)
{
  NS_PRECONDITION(aData->Count() == 4, "Invalid array!");

  gfx3DMatrix matrix1, matrix2;
  if (aData->Item(1).GetUnit() == eCSSUnit_List) {
    matrix1 = nsStyleTransformMatrix::ReadTransforms(aData->Item(1).GetListValue(),
                             aContext, aPresContext,
                             aCanStoreInRuleTree,
                             aBounds, nsPresContext::AppUnitsPerCSSPixel());
  }
  if (aData->Item(2).GetUnit() == eCSSUnit_List) {
    matrix2 = ReadTransforms(aData->Item(2).GetListValue(),
                             aContext, aPresContext,
                             aCanStoreInRuleTree,
                             aBounds, nsPresContext::AppUnitsPerCSSPixel());
  }
  double progress = aData->Item(3).GetPercentValue();

  aMatrix =
    StyleAnimationValue::InterpolateTransformMatrix(matrix1, matrix2, progress)
    * aMatrix;
}

/* Helper function to process a translatex function. */
static void
ProcessTranslateX(gfx3DMatrix& aMatrix,
                  const nsCSSValue::Array* aData,
                  nsStyleContext* aContext,
                  nsPresContext* aPresContext,
                  bool& aCanStoreInRuleTree,
                  nsRect& aBounds)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");

  gfxPoint3D temp;

  temp.x = ProcessTranslatePart(aData->Item(1),
                                aContext, aPresContext, aCanStoreInRuleTree,
                                aBounds.Width());
  aMatrix.Translate(temp);
}

/* Helper function to process a translatey function. */
static void
ProcessTranslateY(gfx3DMatrix& aMatrix,
                  const nsCSSValue::Array* aData,
                  nsStyleContext* aContext,
                  nsPresContext* aPresContext,
                  bool& aCanStoreInRuleTree,
                  nsRect& aBounds)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");

  gfxPoint3D temp;

  temp.y = ProcessTranslatePart(aData->Item(1),
                                aContext, aPresContext, aCanStoreInRuleTree,
                                aBounds.Height());
  aMatrix.Translate(temp);
}

static void 
ProcessTranslateZ(gfx3DMatrix& aMatrix,
                  const nsCSSValue::Array* aData,
                  nsStyleContext* aContext,
                  nsPresContext* aPresContext,
                  bool& aCanStoreInRuleTree)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");

  gfxPoint3D temp;

  temp.z = ProcessTranslatePart(aData->Item(1), aContext,
                                aPresContext, aCanStoreInRuleTree, 0);
  aMatrix.Translate(temp);
}

/* Helper function to process a translate function. */
static void
ProcessTranslate(gfx3DMatrix& aMatrix,
                 const nsCSSValue::Array* aData,
                 nsStyleContext* aContext,
                 nsPresContext* aPresContext,
                 bool& aCanStoreInRuleTree,
                 nsRect& aBounds)
{
  NS_PRECONDITION(aData->Count() == 2 || aData->Count() == 3, "Invalid array!");

  gfxPoint3D temp;

  temp.x = ProcessTranslatePart(aData->Item(1),
                                aContext, aPresContext, aCanStoreInRuleTree,
                                aBounds.Width());

  /* If we read in a Y component, set it appropriately */
  if (aData->Count() == 3) {
    temp.y = ProcessTranslatePart(aData->Item(2),
                                  aContext, aPresContext, aCanStoreInRuleTree,
                                  aBounds.Height());
  }
  aMatrix.Translate(temp);
}

static void
ProcessTranslate3D(gfx3DMatrix& aMatrix,
                   const nsCSSValue::Array* aData,
                   nsStyleContext* aContext,
                   nsPresContext* aPresContext,
                   bool& aCanStoreInRuleTree,
                   nsRect& aBounds)
{
  NS_PRECONDITION(aData->Count() == 4, "Invalid array!");

  gfxPoint3D temp;

  temp.x = ProcessTranslatePart(aData->Item(1),
                                aContext, aPresContext, aCanStoreInRuleTree,
                                aBounds.Width());

  temp.y = ProcessTranslatePart(aData->Item(2),
                                aContext, aPresContext, aCanStoreInRuleTree,
                                aBounds.Height());

  temp.z = ProcessTranslatePart(aData->Item(3),
                                aContext, aPresContext, aCanStoreInRuleTree,
                                0);

  aMatrix.Translate(temp);
}

/* Helper function to set up a scale matrix. */
static void
ProcessScaleHelper(gfx3DMatrix& aMatrix,
                   float aXScale, 
                   float aYScale, 
                   float aZScale)
{
  aMatrix.Scale(aXScale, aYScale, aZScale);
}

/* Process a scalex function. */
static void
ProcessScaleX(gfx3DMatrix& aMatrix, const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2, "Bad array!");
  ProcessScaleHelper(aMatrix, aData->Item(1).GetFloatValue(), 1.0f, 1.0f);
}

/* Process a scaley function. */
static void
ProcessScaleY(gfx3DMatrix& aMatrix, const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2, "Bad array!");
  ProcessScaleHelper(aMatrix, 1.0f, aData->Item(1).GetFloatValue(), 1.0f);
}

static void
ProcessScaleZ(gfx3DMatrix& aMatrix, const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2, "Bad array!");
  ProcessScaleHelper(aMatrix, 1.0f, 1.0f, aData->Item(1).GetFloatValue());
}

static void
ProcessScale3D(gfx3DMatrix& aMatrix, const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 4, "Bad array!");
  ProcessScaleHelper(aMatrix,
                     aData->Item(1).GetFloatValue(),
                     aData->Item(2).GetFloatValue(),
                     aData->Item(3).GetFloatValue());
}

/* Process a scale function. */
static void
ProcessScale(gfx3DMatrix& aMatrix, const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2 || aData->Count() == 3, "Bad array!");
  /* We either have one element or two.  If we have one, it's for both X and Y.
   * Otherwise it's one for each.
   */
  const nsCSSValue& scaleX = aData->Item(1);
  const nsCSSValue& scaleY = (aData->Count() == 2 ? scaleX :
                              aData->Item(2));

  ProcessScaleHelper(aMatrix, 
                     scaleX.GetFloatValue(),
                     scaleY.GetFloatValue(),
                     1.0f);
}

/* Helper function that, given a set of angles, constructs the appropriate
 * skew matrix.
 */
static void
ProcessSkewHelper(gfx3DMatrix& aMatrix, double aXAngle, double aYAngle)
{
  aMatrix.SkewXY(aXAngle, aYAngle);
}

/* Function that converts a skewx transform into a matrix. */
static void
ProcessSkewX(gfx3DMatrix& aMatrix, const nsCSSValue::Array* aData)
{
  NS_ASSERTION(aData->Count() == 2, "Bad array!");
  ProcessSkewHelper(aMatrix, aData->Item(1).GetAngleValueInRadians(), 0.0);
}

/* Function that converts a skewy transform into a matrix. */
static void
ProcessSkewY(gfx3DMatrix& aMatrix, const nsCSSValue::Array* aData)
{
  NS_ASSERTION(aData->Count() == 2, "Bad array!");
  ProcessSkewHelper(aMatrix, 0.0, aData->Item(1).GetAngleValueInRadians());
}

/* Function that converts a skew transform into a matrix. */
static void
ProcessSkew(gfx3DMatrix& aMatrix, const nsCSSValue::Array* aData)
{
  NS_ASSERTION(aData->Count() == 2 || aData->Count() == 3, "Bad array!");

  double xSkew = aData->Item(1).GetAngleValueInRadians();
  double ySkew = (aData->Count() == 2
                  ? 0.0 : aData->Item(2).GetAngleValueInRadians());

  ProcessSkewHelper(aMatrix, xSkew, ySkew);
}

/* Function that converts a rotate transform into a matrix. */
static void
ProcessRotateZ(gfx3DMatrix& aMatrix, const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");
  double theta = aData->Item(1).GetAngleValueInRadians();
  aMatrix.RotateZ(theta);
}

static void
ProcessRotateX(gfx3DMatrix& aMatrix, const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");
  double theta = aData->Item(1).GetAngleValueInRadians();
  aMatrix.RotateX(theta);
}

static void
ProcessRotateY(gfx3DMatrix& aMatrix, const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");
  double theta = aData->Item(1).GetAngleValueInRadians();
  aMatrix.RotateY(theta);
}

static void
ProcessRotate3D(gfx3DMatrix& aMatrix, const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 5, "Invalid array!");

  /* We want our matrix to look like this:
   * |       1 + (1-cos(angle))*(x*x-1)   -z*sin(angle)+(1-cos(angle))*x*y   y*sin(angle)+(1-cos(angle))*x*z   0 |
   * |  z*sin(angle)+(1-cos(angle))*x*y         1 + (1-cos(angle))*(y*y-1)  -x*sin(angle)+(1-cos(angle))*y*z   0 |
   * | -y*sin(angle)+(1-cos(angle))*x*z    x*sin(angle)+(1-cos(angle))*y*z        1 + (1-cos(angle))*(z*z-1)   0 |
   * |                                0                                  0                                 0   1 |
   * (see http://www.w3.org/TR/css3-3d-transforms/#transform-functions)
   */

  /* The current spec specifies a matrix that rotates in the wrong direction. For now we just negate
   * the angle provided to get the correct rotation direction until the spec is updated.
   * See bug 704468.
   */
  double theta = -aData->Item(4).GetAngleValueInRadians();
  float cosTheta = FlushToZero(cos(theta));
  float sinTheta = FlushToZero(sin(theta));

  float x = aData->Item(1).GetFloatValue();
  float y = aData->Item(2).GetFloatValue();
  float z = aData->Item(3).GetFloatValue();

  /* Normalize [x,y,z] */
  float length = sqrt(x*x + y*y + z*z);
  if (length == 0.0) {
    return;
  }
  x /= length;
  y /= length;
  z /= length;

  gfx3DMatrix temp;

  /* Create our matrix */
  temp._11 = 1 + (1 - cosTheta) * (x * x - 1);
  temp._12 = -z * sinTheta + (1 - cosTheta) * x * y;
  temp._13 = y * sinTheta + (1 - cosTheta) * x * z;
  temp._14 = 0.0f;
  temp._21 = z * sinTheta + (1 - cosTheta) * x * y;
  temp._22 = 1 + (1 - cosTheta) * (y * y - 1);
  temp._23 = -x * sinTheta + (1 - cosTheta) * y * z;
  temp._24 = 0.0f;
  temp._31 = -y * sinTheta + (1 - cosTheta) * x * z;
  temp._32 = x * sinTheta + (1 - cosTheta) * y * z;
  temp._33 = 1 + (1 - cosTheta) * (z * z - 1);
  temp._34 = 0.0f;
  temp._41 = 0.0f;
  temp._42 = 0.0f;
  temp._43 = 0.0f;
  temp._44 = 1.0f;

  aMatrix = temp * aMatrix;
}

static void 
ProcessPerspective(gfx3DMatrix& aMatrix, 
                   const nsCSSValue::Array* aData,
                   nsStyleContext *aContext,
                   nsPresContext *aPresContext,
                   bool &aCanStoreInRuleTree)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");

  float depth = ProcessTranslatePart(aData->Item(1), aContext,
                                     aPresContext, aCanStoreInRuleTree,
                                     0);
  aMatrix.Perspective(depth);
}


/**
 * SetToTransformFunction is essentially a giant switch statement that fans
 * out to many smaller helper functions.
 */
static void
MatrixForTransformFunction(gfx3DMatrix& aMatrix,
                           const nsCSSValue::Array * aData,
                           nsStyleContext* aContext,
                           nsPresContext* aPresContext,
                           bool& aCanStoreInRuleTree,
                           nsRect& aBounds)
{
  NS_PRECONDITION(aData, "Why did you want to get data from a null array?");
  // It's OK if aContext and aPresContext are null if the caller already
  // knows that all length units have been converted to pixels (as
  // StyleAnimationValue does).


  /* Get the keyword for the transform. */
  switch (TransformFunctionOf(aData)) {
  case eCSSKeyword_translatex:
    ProcessTranslateX(aMatrix, aData, aContext, aPresContext,
                      aCanStoreInRuleTree, aBounds);
    break;
  case eCSSKeyword_translatey:
    ProcessTranslateY(aMatrix, aData, aContext, aPresContext,
                      aCanStoreInRuleTree, aBounds);
    break;
  case eCSSKeyword_translatez:
    ProcessTranslateZ(aMatrix, aData, aContext, aPresContext,
                      aCanStoreInRuleTree);
    break;
  case eCSSKeyword_translate:
    ProcessTranslate(aMatrix, aData, aContext, aPresContext,
                     aCanStoreInRuleTree, aBounds);
    break;
  case eCSSKeyword_translate3d:
    ProcessTranslate3D(aMatrix, aData, aContext, aPresContext,
                       aCanStoreInRuleTree, aBounds);
    break;
  case eCSSKeyword_scalex:
    ProcessScaleX(aMatrix, aData);
    break;
  case eCSSKeyword_scaley:
    ProcessScaleY(aMatrix, aData);
    break;
  case eCSSKeyword_scalez:
    ProcessScaleZ(aMatrix, aData);
    break;
  case eCSSKeyword_scale:
    ProcessScale(aMatrix, aData);
    break;
  case eCSSKeyword_scale3d:
    ProcessScale3D(aMatrix, aData);
    break;
  case eCSSKeyword_skewx:
    ProcessSkewX(aMatrix, aData);
    break;
  case eCSSKeyword_skewy:
    ProcessSkewY(aMatrix, aData);
    break;
  case eCSSKeyword_skew:
    ProcessSkew(aMatrix, aData);
    break;
  case eCSSKeyword_rotatex:
    ProcessRotateX(aMatrix, aData);
    break;
  case eCSSKeyword_rotatey:
    ProcessRotateY(aMatrix, aData);
    break;
  case eCSSKeyword_rotatez:
  case eCSSKeyword_rotate:
    ProcessRotateZ(aMatrix, aData);
    break;
  case eCSSKeyword_rotate3d:
    ProcessRotate3D(aMatrix, aData);
    break;
  case eCSSKeyword_matrix:
    ProcessMatrix(aMatrix, aData, aContext, aPresContext,
                  aCanStoreInRuleTree, aBounds);
    break;
  case eCSSKeyword_matrix3d:
    ProcessMatrix3D(aMatrix, aData, aContext, aPresContext,
                    aCanStoreInRuleTree, aBounds);
    break;
  case eCSSKeyword_interpolatematrix:
    ProcessInterpolateMatrix(aMatrix, aData, aContext, aPresContext,
                             aCanStoreInRuleTree, aBounds);
    break;
  case eCSSKeyword_perspective:
    ProcessPerspective(aMatrix, aData, aContext, aPresContext, 
                       aCanStoreInRuleTree);
    break;
  default:
    NS_NOTREACHED("Unknown transform function!");
  }
}

/**
 * Return the transform function, as an nsCSSKeyword, for the given
 * nsCSSValue::Array from a transform list.
 */
nsCSSKeyword
TransformFunctionOf(const nsCSSValue::Array* aData)
{
  MOZ_ASSERT(aData->Item(0).GetUnit() == eCSSUnit_Enumerated);
  return aData->Item(0).GetKeywordValue();
}

gfx3DMatrix
ReadTransforms(const nsCSSValueList* aList,
               nsStyleContext* aContext,
               nsPresContext* aPresContext,
               bool &aCanStoreInRuleTree,
               nsRect& aBounds,
               float aAppUnitsPerMatrixUnit)
{
  gfx3DMatrix result;

  for (const nsCSSValueList* curr = aList; curr != nullptr; curr = curr->mNext) {
    const nsCSSValue &currElem = curr->mValue;
    NS_ASSERTION(currElem.GetUnit() == eCSSUnit_Function,
                 "Stream should consist solely of functions!");
    NS_ASSERTION(currElem.GetArrayValue()->Count() >= 1,
                 "Incoming function is too short!");

    /* Read in a single transform matrix. */
    MatrixForTransformFunction(result, currElem.GetArrayValue(), aContext,
                               aPresContext, aCanStoreInRuleTree, aBounds);
  }

  float scale = float(nsPresContext::AppUnitsPerCSSPixel()) / aAppUnitsPerMatrixUnit;
  result.Scale(1/scale, 1/scale, 1/scale);
  result.ScalePost(scale, scale, scale);
  
  return result;
}

} // namespace nsStyleTransformMatrix
