/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class used for intermediate representations of the -moz-transform property.
 */

#include "nsStyleTransformMatrix.h"
#include "nsCSSValue.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsRuleNode.h"
#include "nsSVGUtils.h"
#include "nsCSSKeywords.h"
#include "mozilla/StyleAnimationValue.h"
#include "gfxMatrix.h"

using namespace mozilla;
using namespace mozilla::gfx;

namespace nsStyleTransformMatrix {

/* Note on floating point precision: The transform matrix is an array
 * of single precision 'float's, and so are most of the input values
 * we get from the style system, but intermediate calculations
 * involving angles need to be done in 'double'.
 */


// Define UNIFIED_CONTINUATIONS here and in nsDisplayList.cpp
// to have the transform property try
// to transform content with continuations as one unified block instead of
// several smaller ones.  This is currently disabled because it doesn't work
// correctly, since when the frames are initially being reflowed, their
// continuations all compute their bounding rects independently of each other
// and consequently get the wrong value.
//#define UNIFIED_CONTINUATIONS

void
TransformReferenceBox::EnsureDimensionsAreCached()
{
  if (mIsCached) {
    return;
  }

  MOZ_ASSERT(mFrame);

  mIsCached = true;

  if (mFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT) {
    if (!nsLayoutUtils::SVGTransformBoxEnabled()) {
      mX = -mFrame->GetPosition().x;
      mY = -mFrame->GetPosition().y;
      Size contextSize = nsSVGUtils::GetContextSize(mFrame);
      mWidth = nsPresContext::CSSPixelsToAppUnits(contextSize.width);
      mHeight = nsPresContext::CSSPixelsToAppUnits(contextSize.height);
    } else
    if (mFrame->StyleDisplay()->mTransformBox ==
          NS_STYLE_TRANSFORM_BOX_FILL_BOX) {
      // Percentages in transforms resolve against the SVG bbox, and the
      // transform is relative to the top-left of the SVG bbox.
      gfxRect bbox = nsSVGUtils::GetBBox(const_cast<nsIFrame*>(mFrame));
      nsRect bboxInAppUnits =
        nsLayoutUtils::RoundGfxRectToAppRect(bbox,
                                             mFrame->PresContext()->AppUnitsPerCSSPixel());
      // The mRect of an SVG nsIFrame is its user space bounds *including*
      // stroke and markers, whereas bboxInAppUnits is its user space bounds
      // including fill only.  We need to note the offset of the reference box
      // from the frame's mRect in mX/mY.
      mX = bboxInAppUnits.x - mFrame->GetPosition().x;
      mY = bboxInAppUnits.y - mFrame->GetPosition().y;
      mWidth = bboxInAppUnits.width;
      mHeight = bboxInAppUnits.height;
    } else {
      // The value 'border-box' is treated as 'view-box' for SVG content.
      MOZ_ASSERT(mFrame->StyleDisplay()->mTransformBox ==
                   NS_STYLE_TRANSFORM_BOX_VIEW_BOX ||
                 mFrame->StyleDisplay()->mTransformBox ==
                   NS_STYLE_TRANSFORM_BOX_BORDER_BOX,
                 "Unexpected value for 'transform-box'");
      // Percentages in transforms resolve against the width/height of the
      // nearest viewport (or its viewBox if one is applied), and the
      // transform is relative to {0,0} in current user space.
      mX = -mFrame->GetPosition().x;
      mY = -mFrame->GetPosition().y;
      Size contextSize = nsSVGUtils::GetContextSize(mFrame);
      mWidth = nsPresContext::CSSPixelsToAppUnits(contextSize.width);
      mHeight = nsPresContext::CSSPixelsToAppUnits(contextSize.height);
    }
    return;
  }

  // If UNIFIED_CONTINUATIONS is not defined, this is simply the frame's
  // bounding rectangle, translated to the origin.  Otherwise, it is the
  // smallest rectangle containing a frame and all of its continuations.  For
  // example, if there is a <span> element with several continuations split
  // over several lines, this function will return the rectangle containing all
  // of those continuations.

  nsRect rect;

#ifndef UNIFIED_CONTINUATIONS
  rect = mFrame->GetRect();
#else
  // Iterate the continuation list, unioning together the bounding rects:
  for (const nsIFrame *currFrame = mFrame->FirstContinuation();
       currFrame != nullptr;
       currFrame = currFrame->GetNextContinuation())
  {
    // Get the frame rect in local coordinates, then translate back to the
    // original coordinates:
    rect.UnionRect(result, nsRect(currFrame->GetOffsetTo(mFrame),
                                  currFrame->GetSize()));
  }
#endif

  mX = 0;
  mY = 0;
  mWidth = rect.Width();
  mHeight = rect.Height();
}

void
TransformReferenceBox::Init(const nsSize& aDimensions)
{
  MOZ_ASSERT(!mFrame && !mIsCached);

  mX = 0;
  mY = 0;
  mWidth = aDimensions.width;
  mHeight = aDimensions.height;
  mIsCached = true;
}

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
                     RuleNodeCacheConditions& aConditions,
                     TransformReferenceBox* aRefBox,
                     TransformReferenceBox::DimensionGetter aDimensionGetter)
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
                                              aConditions);
    percent = result.mPercent;
    offset = result.mLength;
  } else {
    offset = nsRuleNode::CalcLength(aValue, aContext, aPresContext,
                                    aConditions);
  }

  float translation = NSAppUnitsToFloatPixels(offset,
                                              nsPresContext::AppUnitsPerCSSPixel());
  // We want to avoid calling aDimensionGetter if there's no percentage to be
  // resolved (for performance reasons - see TransformReferenceBox).
  if (percent != 0.0f && aRefBox) {
    translation += percent *
                     NSAppUnitsToFloatPixels((aRefBox->*aDimensionGetter)(),
                                             nsPresContext::AppUnitsPerCSSPixel());
  }
  return translation;
}

/**
 * Helper functions to process all the transformation function types.
 *
 * These take a matrix parameter to accumulate the current matrix.
 */

/* Helper function to process a matrix entry. */
static void
ProcessMatrix(Matrix4x4& aMatrix,
              const nsCSSValue::Array* aData,
              nsStyleContext* aContext,
              nsPresContext* aPresContext,
              RuleNodeCacheConditions& aConditions,
              TransformReferenceBox& aRefBox)
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
                                   aContext, aPresContext, aConditions,
                                   &aRefBox, &TransformReferenceBox::Width);
  result._32 = ProcessTranslatePart(aData->Item(6),
                                   aContext, aPresContext, aConditions,
                                   &aRefBox, &TransformReferenceBox::Height);

  aMatrix = result * aMatrix;
}

static void 
ProcessMatrix3D(Matrix4x4& aMatrix,
                const nsCSSValue::Array* aData,
                nsStyleContext* aContext,
                nsPresContext* aPresContext,
                RuleNodeCacheConditions& aConditions,
                TransformReferenceBox& aRefBox)
{
  NS_PRECONDITION(aData->Count() == 17, "Invalid array!");

  Matrix4x4 temp;

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
                                  aContext, aPresContext, aConditions,
                                  &aRefBox, &TransformReferenceBox::Width);
  temp._42 = ProcessTranslatePart(aData->Item(14),
                                  aContext, aPresContext, aConditions,
                                  &aRefBox, &TransformReferenceBox::Height);
  temp._43 = ProcessTranslatePart(aData->Item(15),
                                  aContext, aPresContext, aConditions,
                                  nullptr);

  aMatrix = temp * aMatrix;
}

/* Helper function to process two matrices that we need to interpolate between */
void
ProcessInterpolateMatrix(Matrix4x4& aMatrix,
                         const nsCSSValue::Array* aData,
                         nsStyleContext* aContext,
                         nsPresContext* aPresContext,
                         RuleNodeCacheConditions& aConditions,
                         TransformReferenceBox& aRefBox,
                         bool* aContains3dTransform)
{
  NS_PRECONDITION(aData->Count() == 4, "Invalid array!");

  Matrix4x4 matrix1, matrix2;
  if (aData->Item(1).GetUnit() == eCSSUnit_List) {
    matrix1 = nsStyleTransformMatrix::ReadTransforms(aData->Item(1).GetListValue(),
                             aContext, aPresContext,
                             aConditions,
                             aRefBox, nsPresContext::AppUnitsPerCSSPixel(),
                             aContains3dTransform);
  }
  if (aData->Item(2).GetUnit() == eCSSUnit_List) {
    matrix2 = ReadTransforms(aData->Item(2).GetListValue(),
                             aContext, aPresContext,
                             aConditions,
                             aRefBox, nsPresContext::AppUnitsPerCSSPixel(),
                             aContains3dTransform);
  }
  double progress = aData->Item(3).GetPercentValue();

  aMatrix =
    StyleAnimationValue::InterpolateTransformMatrix(matrix1, matrix2, progress)
    * aMatrix;
}

/* Helper function to process a translatex function. */
static void
ProcessTranslateX(Matrix4x4& aMatrix,
                  const nsCSSValue::Array* aData,
                  nsStyleContext* aContext,
                  nsPresContext* aPresContext,
                  RuleNodeCacheConditions& aConditions,
                  TransformReferenceBox& aRefBox)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");

  Point3D temp;

  temp.x = ProcessTranslatePart(aData->Item(1),
                                aContext, aPresContext, aConditions,
                                &aRefBox, &TransformReferenceBox::Width);
  aMatrix.PreTranslate(temp);
}

/* Helper function to process a translatey function. */
static void
ProcessTranslateY(Matrix4x4& aMatrix,
                  const nsCSSValue::Array* aData,
                  nsStyleContext* aContext,
                  nsPresContext* aPresContext,
                  RuleNodeCacheConditions& aConditions,
                  TransformReferenceBox& aRefBox)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");

  Point3D temp;

  temp.y = ProcessTranslatePart(aData->Item(1),
                                aContext, aPresContext, aConditions,
                                &aRefBox, &TransformReferenceBox::Height);
  aMatrix.PreTranslate(temp);
}

static void 
ProcessTranslateZ(Matrix4x4& aMatrix,
                  const nsCSSValue::Array* aData,
                  nsStyleContext* aContext,
                  nsPresContext* aPresContext,
                  RuleNodeCacheConditions& aConditions)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");

  Point3D temp;

  temp.z = ProcessTranslatePart(aData->Item(1), aContext,
                                aPresContext, aConditions,
                                nullptr);
  aMatrix.PreTranslate(temp);
}

/* Helper function to process a translate function. */
static void
ProcessTranslate(Matrix4x4& aMatrix,
                 const nsCSSValue::Array* aData,
                 nsStyleContext* aContext,
                 nsPresContext* aPresContext,
                 RuleNodeCacheConditions& aConditions,
                 TransformReferenceBox& aRefBox)
{
  NS_PRECONDITION(aData->Count() == 2 || aData->Count() == 3, "Invalid array!");

  Point3D temp;

  temp.x = ProcessTranslatePart(aData->Item(1),
                                aContext, aPresContext, aConditions,
                                &aRefBox, &TransformReferenceBox::Width);

  /* If we read in a Y component, set it appropriately */
  if (aData->Count() == 3) {
    temp.y = ProcessTranslatePart(aData->Item(2),
                                  aContext, aPresContext, aConditions,
                                  &aRefBox, &TransformReferenceBox::Height);
  }
  aMatrix.PreTranslate(temp);
}

static void
ProcessTranslate3D(Matrix4x4& aMatrix,
                   const nsCSSValue::Array* aData,
                   nsStyleContext* aContext,
                   nsPresContext* aPresContext,
                   RuleNodeCacheConditions& aConditions,
                   TransformReferenceBox& aRefBox)
{
  NS_PRECONDITION(aData->Count() == 4, "Invalid array!");

  Point3D temp;

  temp.x = ProcessTranslatePart(aData->Item(1),
                                aContext, aPresContext, aConditions,
                                &aRefBox, &TransformReferenceBox::Width);

  temp.y = ProcessTranslatePart(aData->Item(2),
                                aContext, aPresContext, aConditions,
                                &aRefBox, &TransformReferenceBox::Height);

  temp.z = ProcessTranslatePart(aData->Item(3),
                                aContext, aPresContext, aConditions,
                                nullptr);

  aMatrix.PreTranslate(temp);
}

/* Helper function to set up a scale matrix. */
static void
ProcessScaleHelper(Matrix4x4& aMatrix,
                   float aXScale, 
                   float aYScale, 
                   float aZScale)
{
  aMatrix.PreScale(aXScale, aYScale, aZScale);
}

/* Process a scalex function. */
static void
ProcessScaleX(Matrix4x4& aMatrix, const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2, "Bad array!");
  ProcessScaleHelper(aMatrix, aData->Item(1).GetFloatValue(), 1.0f, 1.0f);
}

/* Process a scaley function. */
static void
ProcessScaleY(Matrix4x4& aMatrix, const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2, "Bad array!");
  ProcessScaleHelper(aMatrix, 1.0f, aData->Item(1).GetFloatValue(), 1.0f);
}

static void
ProcessScaleZ(Matrix4x4& aMatrix, const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2, "Bad array!");
  ProcessScaleHelper(aMatrix, 1.0f, 1.0f, aData->Item(1).GetFloatValue());
}

static void
ProcessScale3D(Matrix4x4& aMatrix, const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 4, "Bad array!");
  ProcessScaleHelper(aMatrix,
                     aData->Item(1).GetFloatValue(),
                     aData->Item(2).GetFloatValue(),
                     aData->Item(3).GetFloatValue());
}

/* Process a scale function. */
static void
ProcessScale(Matrix4x4& aMatrix, const nsCSSValue::Array* aData)
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
ProcessSkewHelper(Matrix4x4& aMatrix, double aXAngle, double aYAngle)
{
  aMatrix.SkewXY(aXAngle, aYAngle);
}

/* Function that converts a skewx transform into a matrix. */
static void
ProcessSkewX(Matrix4x4& aMatrix, const nsCSSValue::Array* aData)
{
  NS_ASSERTION(aData->Count() == 2, "Bad array!");
  ProcessSkewHelper(aMatrix, aData->Item(1).GetAngleValueInRadians(), 0.0);
}

/* Function that converts a skewy transform into a matrix. */
static void
ProcessSkewY(Matrix4x4& aMatrix, const nsCSSValue::Array* aData)
{
  NS_ASSERTION(aData->Count() == 2, "Bad array!");
  ProcessSkewHelper(aMatrix, 0.0, aData->Item(1).GetAngleValueInRadians());
}

/* Function that converts a skew transform into a matrix. */
static void
ProcessSkew(Matrix4x4& aMatrix, const nsCSSValue::Array* aData)
{
  NS_ASSERTION(aData->Count() == 2 || aData->Count() == 3, "Bad array!");

  double xSkew = aData->Item(1).GetAngleValueInRadians();
  double ySkew = (aData->Count() == 2
                  ? 0.0 : aData->Item(2).GetAngleValueInRadians());

  ProcessSkewHelper(aMatrix, xSkew, ySkew);
}

/* Function that converts a rotate transform into a matrix. */
static void
ProcessRotateZ(Matrix4x4& aMatrix, const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");
  double theta = aData->Item(1).GetAngleValueInRadians();
  aMatrix.RotateZ(theta);
}

static void
ProcessRotateX(Matrix4x4& aMatrix, const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");
  double theta = aData->Item(1).GetAngleValueInRadians();
  aMatrix.RotateX(theta);
}

static void
ProcessRotateY(Matrix4x4& aMatrix, const nsCSSValue::Array* aData)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");
  double theta = aData->Item(1).GetAngleValueInRadians();
  aMatrix.RotateY(theta);
}

static void
ProcessRotate3D(Matrix4x4& aMatrix, const nsCSSValue::Array* aData)
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

  Point3D vector(aData->Item(1).GetFloatValue(),
                 aData->Item(2).GetFloatValue(),
                 aData->Item(3).GetFloatValue());

  if (!vector.Length()) {
    return;
  }
  vector.Normalize();

  Matrix4x4 temp;

  /* Create our matrix */
  temp._11 = 1 + (1 - cosTheta) * (vector.x * vector.x - 1);
  temp._12 = -vector.z * sinTheta + (1 - cosTheta) * vector.x * vector.y;
  temp._13 = vector.y * sinTheta + (1 - cosTheta) * vector.x * vector.z;
  temp._14 = 0.0f;
  temp._21 = vector.z * sinTheta + (1 - cosTheta) * vector.x * vector.y;
  temp._22 = 1 + (1 - cosTheta) * (vector.y * vector.y - 1);
  temp._23 = -vector.x * sinTheta + (1 - cosTheta) * vector.y * vector.z;
  temp._24 = 0.0f;
  temp._31 = -vector.y * sinTheta + (1 - cosTheta) * vector.x * vector.z;
  temp._32 = vector.x * sinTheta + (1 - cosTheta) * vector.y * vector.z;
  temp._33 = 1 + (1 - cosTheta) * (vector.z * vector.z - 1);
  temp._34 = 0.0f;
  temp._41 = 0.0f;
  temp._42 = 0.0f;
  temp._43 = 0.0f;
  temp._44 = 1.0f;

  aMatrix = temp * aMatrix;
}

static void 
ProcessPerspective(Matrix4x4& aMatrix,
                   const nsCSSValue::Array* aData,
                   nsStyleContext *aContext,
                   nsPresContext *aPresContext,
                   RuleNodeCacheConditions& aConditions)
{
  NS_PRECONDITION(aData->Count() == 2, "Invalid array!");

  float depth = ProcessTranslatePart(aData->Item(1), aContext,
                                     aPresContext, aConditions,
                                     nullptr);
  aMatrix.Perspective(depth);
}


/**
 * SetToTransformFunction is essentially a giant switch statement that fans
 * out to many smaller helper functions.
 */
static void
MatrixForTransformFunction(Matrix4x4& aMatrix,
                           const nsCSSValue::Array * aData,
                           nsStyleContext* aContext,
                           nsPresContext* aPresContext,
                           RuleNodeCacheConditions& aConditions,
                           TransformReferenceBox& aRefBox,
                           bool* aContains3dTransform)
{
  MOZ_ASSERT(aContains3dTransform);
  NS_PRECONDITION(aData, "Why did you want to get data from a null array?");
  // It's OK if aContext and aPresContext are null if the caller already
  // knows that all length units have been converted to pixels (as
  // StyleAnimationValue does).


  /* Get the keyword for the transform. */
  switch (TransformFunctionOf(aData)) {
  case eCSSKeyword_translatex:
    ProcessTranslateX(aMatrix, aData, aContext, aPresContext,
                      aConditions, aRefBox);
    break;
  case eCSSKeyword_translatey:
    ProcessTranslateY(aMatrix, aData, aContext, aPresContext,
                      aConditions, aRefBox);
    break;
  case eCSSKeyword_translatez:
    *aContains3dTransform = true;
    ProcessTranslateZ(aMatrix, aData, aContext, aPresContext,
                      aConditions);
    break;
  case eCSSKeyword_translate:
    ProcessTranslate(aMatrix, aData, aContext, aPresContext,
                     aConditions, aRefBox);
    break;
  case eCSSKeyword_translate3d:
    *aContains3dTransform = true;
    ProcessTranslate3D(aMatrix, aData, aContext, aPresContext,
                       aConditions, aRefBox);
    break;
  case eCSSKeyword_scalex:
    ProcessScaleX(aMatrix, aData);
    break;
  case eCSSKeyword_scaley:
    ProcessScaleY(aMatrix, aData);
    break;
  case eCSSKeyword_scalez:
    *aContains3dTransform = true;
    ProcessScaleZ(aMatrix, aData);
    break;
  case eCSSKeyword_scale:
    ProcessScale(aMatrix, aData);
    break;
  case eCSSKeyword_scale3d:
    *aContains3dTransform = true;
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
    *aContains3dTransform = true;
    ProcessRotateX(aMatrix, aData);
    break;
  case eCSSKeyword_rotatey:
    *aContains3dTransform = true;
    ProcessRotateY(aMatrix, aData);
    break;
  case eCSSKeyword_rotatez:
    *aContains3dTransform = true;
  case eCSSKeyword_rotate:
    ProcessRotateZ(aMatrix, aData);
    break;
  case eCSSKeyword_rotate3d:
    *aContains3dTransform = true;
    ProcessRotate3D(aMatrix, aData);
    break;
  case eCSSKeyword_matrix:
    ProcessMatrix(aMatrix, aData, aContext, aPresContext,
                  aConditions, aRefBox);
    break;
  case eCSSKeyword_matrix3d:
    *aContains3dTransform = true;
    ProcessMatrix3D(aMatrix, aData, aContext, aPresContext,
                    aConditions, aRefBox);
    break;
  case eCSSKeyword_interpolatematrix:
    ProcessInterpolateMatrix(aMatrix, aData, aContext, aPresContext,
                             aConditions, aRefBox,
                             aContains3dTransform);
    break;
  case eCSSKeyword_perspective:
    *aContains3dTransform = true;
    ProcessPerspective(aMatrix, aData, aContext, aPresContext, 
                       aConditions);
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

Matrix4x4
ReadTransforms(const nsCSSValueList* aList,
               nsStyleContext* aContext,
               nsPresContext* aPresContext,
               RuleNodeCacheConditions& aConditions,
               TransformReferenceBox& aRefBox,
               float aAppUnitsPerMatrixUnit,
               bool* aContains3dTransform)
{
  Matrix4x4 result;

  for (const nsCSSValueList* curr = aList; curr != nullptr; curr = curr->mNext) {
    const nsCSSValue &currElem = curr->mValue;
    if (currElem.GetUnit() != eCSSUnit_Function) {
      NS_ASSERTION(currElem.GetUnit() == eCSSUnit_None &&
                   !aList->mNext,
                   "stream should either be a list of functions or a "
                   "lone None");
      continue;
    }
    NS_ASSERTION(currElem.GetArrayValue()->Count() >= 1,
                 "Incoming function is too short!");

    /* Read in a single transform matrix. */
    MatrixForTransformFunction(result, currElem.GetArrayValue(), aContext,
                               aPresContext, aConditions, aRefBox,
                               aContains3dTransform);
  }

  float scale = float(nsPresContext::AppUnitsPerCSSPixel()) / aAppUnitsPerMatrixUnit;
  result.PreScale(1/scale, 1/scale, 1/scale);
  result.PostScale(scale, scale, scale);

  return result;
}

} // namespace nsStyleTransformMatrix
