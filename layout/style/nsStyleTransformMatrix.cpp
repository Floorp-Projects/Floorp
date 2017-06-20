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
#include "mozilla/ServoBindings.h"
#include "mozilla/StyleAnimationValue.h"
#include "gfxMatrix.h"
#include "gfxQuaternion.h"

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
    if (mFrame->StyleDisplay()->mTransformBox == StyleGeometryBox::FillBox) {
      // Percentages in transforms resolve against the SVG bbox, and the
      // transform is relative to the top-left of the SVG bbox.
      nsRect bboxInAppUnits =
        nsLayoutUtils::ComputeGeometryBox(const_cast<nsIFrame*>(mFrame),
                                          StyleGeometryBox::FillBox);
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
                   StyleGeometryBox::ViewBox ||
                 mFrame->StyleDisplay()->mTransformBox ==
                   StyleGeometryBox::BorderBox,
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
  if (percent != 0.0f && aRefBox && !aRefBox->IsEmpty()) {
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

// For accumulation for transform functions, |aOne| corresponds to |aB| and
// |aTwo| corresponds to |aA| for StyleAnimationValue::Accumulate().
class Accumulate {
public:
  template<typename T>
  static T operate(const T& aOne, const T& aTwo, double aCoeff)
  {
    return aOne + aTwo * aCoeff;
  }

  static Point4D operateForPerspective(const Point4D& aOne,
                                       const Point4D& aTwo,
                                       double aCoeff)
  {
    return (aOne - Point4D(0, 0, 0, 1)) +
           (aTwo - Point4D(0, 0, 0, 1)) * aCoeff +
           Point4D(0, 0, 0, 1);
  }
  static Point3D operateForScale(const Point3D& aOne,
                                 const Point3D& aTwo,
                                 double aCoeff)
  {
    // For scale, the identify element is 1, see AddTransformScale in
    // StyleAnimationValue.cpp.
    return (aOne - Point3D(1, 1, 1)) +
           (aTwo - Point3D(1, 1, 1)) * aCoeff +
           Point3D(1, 1, 1);
  }

  static Matrix4x4 operateForRotate(const gfxQuaternion& aOne,
                                    const gfxQuaternion& aTwo,
                                    double aCoeff)
  {
    if (aCoeff == 0.0) {
      return aOne.ToMatrix();
    }

    double theta = acos(mozilla::clamped(aTwo.w, -1.0, 1.0));
    double scale = (theta != 0.0) ? 1.0 / sin(theta) : 0.0;
    theta *= aCoeff;
    scale *= sin(theta);

    gfxQuaternion result = gfxQuaternion(scale * aTwo.x,
                                         scale * aTwo.y,
                                         scale * aTwo.z,
                                         cos(theta)) * aOne;
    return result.ToMatrix();
  }

  static Matrix4x4 operateByServo(const Matrix4x4& aMatrix1,
                                  const Matrix4x4& aMatrix2,
                                  double aCount)
  {
    Matrix4x4 result;
    Servo_MatrixTransform_Operate(MatrixTransformOperator::Accumulate,
                                  &aMatrix1.components,
                                  &aMatrix2.components,
                                  aCount,
                                  &result.components);
    return result;
  }
};

class Interpolate {
public:
  template<typename T>
  static T operate(const T& aOne, const T& aTwo, double aCoeff)
  {
    return aOne + (aTwo - aOne) * aCoeff;
  }

  static Point4D operateForPerspective(const Point4D& aOne,
                                       const Point4D& aTwo,
                                       double aCoeff)
  {
    return aOne + (aTwo - aOne) * aCoeff;
  }

  static Point3D operateForScale(const Point3D& aOne,
                                 const Point3D& aTwo,
                                 double aCoeff)
  {
    return aOne + (aTwo - aOne) * aCoeff;
  }

  static Matrix4x4 operateForRotate(const gfxQuaternion& aOne,
                                    const gfxQuaternion& aTwo,
                                    double aCoeff)
  {
    return aOne.Slerp(aTwo, aCoeff).ToMatrix();
  }

  static Matrix4x4 operateByServo(const Matrix4x4& aMatrix1,
                                  const Matrix4x4& aMatrix2,
                                  double aProgress)
  {
    Matrix4x4 result;
    Servo_MatrixTransform_Operate(MatrixTransformOperator::Interpolate,
                                  &aMatrix1.components,
                                  &aMatrix2.components,
                                  aProgress,
                                  &result.components);
    return result;
  }
};

/**
 * Calculate 2 matrices by decomposing them with Operator.
 *
 * @param aMatrix1   First matrix, using CSS pixel units.
 * @param aMatrix2   Second matrix, using CSS pixel units.
 * @param aProgress  Coefficient for the Operator.
 */
template <typename Operator>
static Matrix4x4
OperateTransformMatrix(const Matrix4x4 &aMatrix1,
                       const Matrix4x4 &aMatrix2,
                       double aProgress)
{
  // Decompose both matrices

  // TODO: What do we do if one of these returns false (singular matrix)
  Point3D scale1(1, 1, 1), translate1;
  Point4D perspective1(0, 0, 0, 1);
  gfxQuaternion rotate1;
  nsStyleTransformMatrix::ShearArray shear1{0.0f, 0.0f, 0.0f};

  Point3D scale2(1, 1, 1), translate2;
  Point4D perspective2(0, 0, 0, 1);
  gfxQuaternion rotate2;
  nsStyleTransformMatrix::ShearArray shear2{0.0f, 0.0f, 0.0f};

  Matrix matrix2d1, matrix2d2;
  if (aMatrix1.Is2D(&matrix2d1) && aMatrix2.Is2D(&matrix2d2)) {
    Decompose2DMatrix(matrix2d1, scale1, shear1, rotate1, translate1);
    Decompose2DMatrix(matrix2d2, scale2, shear2, rotate2, translate2);
  } else {
    Decompose3DMatrix(aMatrix1, scale1, shear1,
                      rotate1, translate1, perspective1);
    Decompose3DMatrix(aMatrix2, scale2, shear2,
                      rotate2, translate2, perspective2);
  }

  Matrix4x4 result;

  // Operate each of the pieces in response to |Operator|.
  Point4D perspective =
    Operator::operateForPerspective(perspective1, perspective2, aProgress);
  result.SetTransposedVector(3, perspective);

  Point3D translate =
    Operator::operate(translate1, translate2, aProgress);
  result.PreTranslate(translate.x, translate.y, translate.z);

  Matrix4x4 rotate = Operator::operateForRotate(rotate1, rotate2, aProgress);
  if (!rotate.IsIdentity()) {
    result = rotate * result;
  }

  // TODO: Would it be better to operate these as angles?
  //       How do we convert back to angles?
  float yzshear =
    Operator::operate(shear1[ShearType::YZSHEAR],
                      shear2[ShearType::YZSHEAR],
                      aProgress);
  if (yzshear != 0.0) {
    result.SkewYZ(yzshear);
  }

  float xzshear =
    Operator::operate(shear1[ShearType::XZSHEAR],
                      shear2[ShearType::XZSHEAR],
                      aProgress);
  if (xzshear != 0.0) {
    result.SkewXZ(xzshear);
  }

  float xyshear =
    Operator::operate(shear1[ShearType::XYSHEAR],
                      shear2[ShearType::XYSHEAR],
                      aProgress);
  if (xyshear != 0.0) {
    result.SkewXY(xyshear);
  }

  Point3D scale =
    Operator::operateForScale(scale1, scale2, aProgress);
  if (scale != Point3D(1.0, 1.0, 1.0)) {
    result.PreScale(scale.x, scale.y, scale.z);
  }

  return result;
}

template <typename Operator>
static Matrix4x4
OperateTransformMatrixByServo(const Matrix4x4 &aMatrix1,
                              const Matrix4x4 &aMatrix2,
                              double aProgress)
{
  return Operator::operateByServo(aMatrix1, aMatrix2, aProgress);
}

template <typename Operator>
static void
ProcessMatrixOperator(Matrix4x4& aMatrix,
                      const nsCSSValue::Array* aData,
                      nsStyleContext* aContext,
                      nsPresContext* aPresContext,
                      RuleNodeCacheConditions& aConditions,
                      TransformReferenceBox& aRefBox,
                      bool* aContains3dTransform)
{
  NS_PRECONDITION(aData->Count() == 4, "Invalid array!");

  auto readTransform = [&](const nsCSSValue& aValue) -> Matrix4x4 {
    const nsCSSValueList* list = nullptr;
    switch (aValue.GetUnit()) {
      case eCSSUnit_List:
        // For Gecko style backend.
        list = aValue.GetListValue();
        break;
      case eCSSUnit_SharedList:
        // For Servo style backend. The transform lists of interpolatematrix
        // are not created on the main thread (i.e. during parallel traversal),
        // and nsCSSValueList_heap is not thread safe. Therefore, we use
        // nsCSSValueSharedList as a workaround.
        list = aValue.GetSharedListValue()->mHead;
        break;
      default:
        list = nullptr;
    }

    Matrix4x4 matrix;
    if (!list) {
      return matrix;
    }

    float appUnitPerCSSPixel = nsPresContext::AppUnitsPerCSSPixel();
    matrix = nsStyleTransformMatrix::ReadTransforms(list,
                                                    aContext,
                                                    aPresContext,
                                                    aConditions,
                                                    aRefBox,
                                                    appUnitPerCSSPixel,
                                                    aContains3dTransform);
    return matrix;
  };

  Matrix4x4 matrix1 = readTransform(aData->Item(1));
  Matrix4x4 matrix2 = readTransform(aData->Item(2));
  double progress = aData->Item(3).GetPercentValue();

  if (aContext && aContext->IsServo()) {
    aMatrix =
      OperateTransformMatrixByServo<Operator>(matrix1, matrix2, progress)
        * aMatrix;
    return;
  }

  aMatrix =
    OperateTransformMatrix<Operator>(matrix1, matrix2, progress) * aMatrix;
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
  ProcessMatrixOperator<Interpolate>(aMatrix, aData, aContext, aPresContext,
                                     aConditions, aRefBox,
                                     aContains3dTransform);
}

void
ProcessAccumulateMatrix(Matrix4x4& aMatrix,
                        const nsCSSValue::Array* aData,
                        nsStyleContext* aContext,
                        nsPresContext* aPresContext,
                        RuleNodeCacheConditions& aConditions,
                        TransformReferenceBox& aRefBox,
                        bool* aContains3dTransform)
{
  ProcessMatrixOperator<Accumulate>(aMatrix, aData, aContext, aPresContext,
                                    aConditions, aRefBox,
                                    aContains3dTransform);
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

  double theta = aData->Item(4).GetAngleValueInRadians();
  float x = aData->Item(1).GetFloatValue();
  float y = aData->Item(2).GetFloatValue();
  float z = aData->Item(3).GetFloatValue();

  Matrix4x4 temp;
  temp.SetRotateAxisAngle(x, y, z, theta);

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
                                     aPresContext, aConditions, nullptr);
  ApplyPerspectiveToMatrix(aMatrix, depth);
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
    MOZ_FALLTHROUGH;
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
    ProcessMatrixOperator<Interpolate>(aMatrix, aData, aContext, aPresContext,
                                       aConditions, aRefBox,
                                       aContains3dTransform);
    break;
  case eCSSKeyword_accumulatematrix:
    ProcessMatrixOperator<Accumulate>(aMatrix, aData, aContext, aPresContext,
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

void
SetIdentityMatrix(nsCSSValue::Array* aMatrix)
{
  MOZ_ASSERT(aMatrix, "aMatrix should be non-null");

  nsCSSKeyword tfunc = TransformFunctionOf(aMatrix);
  MOZ_ASSERT(tfunc == eCSSKeyword_matrix ||
             tfunc == eCSSKeyword_matrix3d,
             "Only accept matrix and matrix3d");

  if (tfunc == eCSSKeyword_matrix) {
    MOZ_ASSERT(aMatrix->Count() == 7, "Invalid matrix");
    Matrix m;
    for (size_t i = 0; i < 6; ++i) {
      aMatrix->Item(i + 1).SetFloatValue(m.components[i], eCSSUnit_Number);
    }
    return;
  }

  MOZ_ASSERT(aMatrix->Count() == 17, "Invalid matrix3d");
  Matrix4x4 m;
  for (size_t i = 0; i < 16; ++i) {
    aMatrix->Item(i + 1).SetFloatValue(m.components[i], eCSSUnit_Number);
  }
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

Point
Convert2DPosition(nsStyleCoord const (&aValue)[2],
                  TransformReferenceBox& aRefBox,
                  int32_t aAppUnitsPerDevPixel)
{
  float position[2];
  nsStyleTransformMatrix::TransformReferenceBox::DimensionGetter dimensionGetter[] =
    { &nsStyleTransformMatrix::TransformReferenceBox::Width,
      &nsStyleTransformMatrix::TransformReferenceBox::Height };
  for (uint8_t index = 0; index < 2; ++index) {
    const nsStyleCoord& value  = aValue[index];
    if (value.GetUnit() == eStyleUnit_Calc) {
      const nsStyleCoord::Calc *calc = value.GetCalcValue();
      position[index] =
        NSAppUnitsToFloatPixels((aRefBox.*dimensionGetter[index])(), aAppUnitsPerDevPixel) *
          calc->mPercent +
        NSAppUnitsToFloatPixels(calc->mLength, aAppUnitsPerDevPixel);
    } else if (value.GetUnit() == eStyleUnit_Percent) {
      position[index] =
        NSAppUnitsToFloatPixels((aRefBox.*dimensionGetter[index])(), aAppUnitsPerDevPixel) *
        value.GetPercentValue();
    } else {
      MOZ_ASSERT(value.GetUnit() == eStyleUnit_Coord,
                "unexpected unit");
      position[index] =
        NSAppUnitsToFloatPixels(value.GetCoordValue(),
                                aAppUnitsPerDevPixel);
    }
  }

  return Point(position[0], position[1]);
}

/*
 * The relevant section of the transitions specification:
 * http://dev.w3.org/csswg/css3-transitions/#animation-of-property-types-
 * defers all of the details to the 2-D and 3-D transforms specifications.
 * For the 2-D transforms specification (all that's relevant for us, right
 * now), the relevant section is:
 * http://dev.w3.org/csswg/css3-2d-transforms/#animation
 * This, in turn, refers to the unmatrix program in Graphics Gems,
 * available from http://tog.acm.org/resources/GraphicsGems/ , and in
 * particular as the file GraphicsGems/gemsii/unmatrix.c
 * in http://tog.acm.org/resources/GraphicsGems/AllGems.tar.gz
 *
 * The unmatrix reference is for general 3-D transform matrices (any of the
 * 16 components can have any value).
 *
 * For CSS 2-D transforms, we have a 2-D matrix with the bottom row constant:
 *
 * [ A C E ]
 * [ B D F ]
 * [ 0 0 1 ]
 *
 * For that case, I believe the algorithm in unmatrix reduces to:
 *
 *  (1) If A * D - B * C == 0, the matrix is singular.  Fail.
 *
 *  (2) Set translation components (Tx and Ty) to the translation parts of
 *      the matrix (E and F) and then ignore them for the rest of the time.
 *      (For us, E and F each actually consist of three constants:  a
 *      length, a multiplier for the width, and a multiplier for the
 *      height.  This actually requires its own decomposition, but I'll
 *      keep that separate.)
 *
 *  (3) Let the X scale (Sx) be sqrt(A^2 + B^2).  Then divide both A and B
 *      by it.
 *
 *  (4) Let the XY shear (K) be A * C + B * D.  From C, subtract A times
 *      the XY shear.  From D, subtract B times the XY shear.
 *
 *  (5) Let the Y scale (Sy) be sqrt(C^2 + D^2).  Divide C, D, and the XY
 *      shear (K) by it.
 *
 *  (6) At this point, A * D - B * C is either 1 or -1.  If it is -1,
 *      negate the XY shear (K), the X scale (Sx), and A, B, C, and D.
 *      (Alternatively, we could negate the XY shear (K) and the Y scale
 *      (Sy).)
 *
 *  (7) Let the rotation be R = atan2(B, A).
 *
 * Then the resulting decomposed transformation is:
 *
 *   translate(Tx, Ty) rotate(R) skewX(atan(K)) scale(Sx, Sy)
 *
 * An interesting result of this is that all of the simple transform
 * functions (i.e., all functions other than matrix()), in isolation,
 * decompose back to themselves except for:
 *   'skewY(φ)', which is 'matrix(1, tan(φ), 0, 1, 0, 0)', which decomposes
 *   to 'rotate(φ) skewX(φ) scale(sec(φ), cos(φ))' since (ignoring the
 *   alternate sign possibilities that would get fixed in step 6):
 *     In step 3, the X scale factor is sqrt(1+tan²(φ)) = sqrt(sec²(φ)) = sec(φ).
 *     Thus, after step 3, A = 1/sec(φ) = cos(φ) and B = tan(φ) / sec(φ) = sin(φ).
 *     In step 4, the XY shear is sin(φ).
 *     Thus, after step 4, C = -cos(φ)sin(φ) and D = 1 - sin²(φ) = cos²(φ).
 *     Thus, in step 5, the Y scale is sqrt(cos²(φ)(sin²(φ) + cos²(φ)) = cos(φ).
 *     Thus, after step 5, C = -sin(φ), D = cos(φ), and the XY shear is tan(φ).
 *     Thus, in step 6, A * D - B * C = cos²(φ) + sin²(φ) = 1.
 *     In step 7, the rotation is thus φ.
 *
 *   skew(θ, φ), which is matrix(1, tan(φ), tan(θ), 1, 0, 0), which decomposes
 *   to 'rotate(φ) skewX(θ + φ) scale(sec(φ), cos(φ))' since (ignoring
 *   the alternate sign possibilities that would get fixed in step 6):
 *     In step 3, the X scale factor is sqrt(1+tan²(φ)) = sqrt(sec²(φ)) = sec(φ).
 *     Thus, after step 3, A = 1/sec(φ) = cos(φ) and B = tan(φ) / sec(φ) = sin(φ).
 *     In step 4, the XY shear is cos(φ)tan(θ) + sin(φ).
 *     Thus, after step 4,
 *     C = tan(θ) - cos(φ)(cos(φ)tan(θ) + sin(φ)) = tan(θ)sin²(φ) - cos(φ)sin(φ)
 *     D = 1 - sin(φ)(cos(φ)tan(θ) + sin(φ)) = cos²(φ) - sin(φ)cos(φ)tan(θ)
 *     Thus, in step 5, the Y scale is sqrt(C² + D²) =
 *     sqrt(tan²(θ)(sin⁴(φ) + sin²(φ)cos²(φ)) -
 *          2 tan(θ)(sin³(φ)cos(φ) + sin(φ)cos³(φ)) +
 *          (sin²(φ)cos²(φ) + cos⁴(φ))) =
 *     sqrt(tan²(θ)sin²(φ) - 2 tan(θ)sin(φ)cos(φ) + cos²(φ)) =
 *     cos(φ) - tan(θ)sin(φ) (taking the negative of the obvious solution so
 *     we avoid flipping in step 6).
 *     After step 5, C = -sin(φ) and D = cos(φ), and the XY shear is
 *     (cos(φ)tan(θ) + sin(φ)) / (cos(φ) - tan(θ)sin(φ)) =
 *     (dividing both numerator and denominator by cos(φ))
 *     (tan(θ) + tan(φ)) / (1 - tan(θ)tan(φ)) = tan(θ + φ).
 *     (See http://en.wikipedia.org/wiki/List_of_trigonometric_identities .)
 *     Thus, in step 6, A * D - B * C = cos²(φ) + sin²(φ) = 1.
 *     In step 7, the rotation is thus φ.
 *
 *     To check this result, we can multiply things back together:
 *
 *     [ cos(φ) -sin(φ) ] [ 1 tan(θ + φ) ] [ sec(φ)    0   ]
 *     [ sin(φ)  cos(φ) ] [ 0      1     ] [   0    cos(φ) ]
 *
 *     [ cos(φ)      cos(φ)tan(θ + φ) - sin(φ) ] [ sec(φ)    0   ]
 *     [ sin(φ)      sin(φ)tan(θ + φ) + cos(φ) ] [   0    cos(φ) ]
 *
 *     but since tan(θ + φ) = (tan(θ) + tan(φ)) / (1 - tan(θ)tan(φ)),
 *     cos(φ)tan(θ + φ) - sin(φ)
 *      = cos(φ)(tan(θ) + tan(φ)) - sin(φ) + sin(φ)tan(θ)tan(φ)
 *      = cos(φ)tan(θ) + sin(φ) - sin(φ) + sin(φ)tan(θ)tan(φ)
 *      = cos(φ)tan(θ) + sin(φ)tan(θ)tan(φ)
 *      = tan(θ) (cos(φ) + sin(φ)tan(φ))
 *      = tan(θ) sec(φ) (cos²(φ) + sin²(φ))
 *      = tan(θ) sec(φ)
 *     and
 *     sin(φ)tan(θ + φ) + cos(φ)
 *      = sin(φ)(tan(θ) + tan(φ)) + cos(φ) - cos(φ)tan(θ)tan(φ)
 *      = tan(θ) (sin(φ) - sin(φ)) + sin(φ)tan(φ) + cos(φ)
 *      = sec(φ) (sin²(φ) + cos²(φ))
 *      = sec(φ)
 *     so the above is:
 *     [ cos(φ)  tan(θ) sec(φ) ] [ sec(φ)    0   ]
 *     [ sin(φ)     sec(φ)     ] [   0    cos(φ) ]
 *
 *     [    1   tan(θ) ]
 *     [ tan(φ)    1   ]
 */

/*
 * Decompose2DMatrix implements the above decomposition algorithm.
 */

bool
Decompose2DMatrix(const Matrix& aMatrix,
                  Point3D& aScale,
                  ShearArray& aShear,
                  gfxQuaternion& aRotate,
                  Point3D& aTranslate)
{
  float A = aMatrix._11,
        B = aMatrix._12,
        C = aMatrix._21,
        D = aMatrix._22;
  if (A * D == B * C) {
    // singular matrix
    return false;
  }

  float scaleX = sqrt(A * A + B * B);
  A /= scaleX;
  B /= scaleX;

  float XYshear = A * C + B * D;
  C -= A * XYshear;
  D -= B * XYshear;

  float scaleY = sqrt(C * C + D * D);
  C /= scaleY;
  D /= scaleY;
  XYshear /= scaleY;

  // A*D - B*C should now be 1 or -1
  NS_ASSERTION(0.99 < Abs(A*D - B*C) && Abs(A*D - B*C) < 1.01,
               "determinant should now be 1 or -1");
  if (A * D < B * C) {
    A = -A;
    B = -B;
    C = -C;
    D = -D;
    XYshear = -XYshear;
    scaleX = -scaleX;
  }

  float rotate = atan2f(B, A);
  aRotate = gfxQuaternion(0, 0, sin(rotate/2), cos(rotate/2));
  aShear[ShearType::XYSHEAR] = XYshear;
  aScale.x = scaleX;
  aScale.y = scaleY;
  aTranslate.x = aMatrix._31;
  aTranslate.y = aMatrix._32;
  return true;
}

/**
 * Implementation of the unmatrix algorithm, specified by:
 *
 * http://dev.w3.org/csswg/css3-2d-transforms/#unmatrix
 *
 * This, in turn, refers to the unmatrix program in Graphics Gems,
 * available from http://tog.acm.org/resources/GraphicsGems/ , and in
 * particular as the file GraphicsGems/gemsii/unmatrix.c
 * in http://tog.acm.org/resources/GraphicsGems/AllGems.tar.gz
 */
bool
Decompose3DMatrix(const Matrix4x4& aMatrix,
                  Point3D& aScale,
                  ShearArray& aShear,
                  gfxQuaternion& aRotate,
                  Point3D& aTranslate,
                  Point4D& aPerspective)
{
  Matrix4x4 local = aMatrix;

  if (local[3][3] == 0) {
    return false;
  }
  /* Normalize the matrix */
  local.Normalize();

  /**
   * perspective is used to solve for perspective, but it also provides
   * an easy way to test for singularity of the upper 3x3 component.
   */
  Matrix4x4 perspective = local;
  Point4D empty(0, 0, 0, 1);
  perspective.SetTransposedVector(3, empty);

  if (perspective.Determinant() == 0.0) {
    return false;
  }

  /* First, isolate perspective. */
  if (local[0][3] != 0 || local[1][3] != 0 ||
      local[2][3] != 0) {
    /* aPerspective is the right hand side of the equation. */
    aPerspective = local.TransposedVector(3);

    /**
     * Solve the equation by inverting perspective and multiplying
     * aPerspective by the inverse.
     */
    perspective.Invert();
    aPerspective = perspective.TransposeTransform4D(aPerspective);

    /* Clear the perspective partition */
    local.SetTransposedVector(3, empty);
  } else {
    aPerspective = Point4D(0, 0, 0, 1);
  }

  /* Next take care of translation */
  for (int i = 0; i < 3; i++) {
    aTranslate[i] = local[3][i];
    local[3][i] = 0;
  }

  /* Now get scale and shear. */

  /* Compute X scale factor and normalize first row. */
  aScale.x = local[0].Length();
  local[0] /= aScale.x;

  /* Compute XY shear factor and make 2nd local orthogonal to 1st. */
  aShear[ShearType::XYSHEAR] = local[0].DotProduct(local[1]);
  local[1] -= local[0] * aShear[ShearType::XYSHEAR];

  /* Now, compute Y scale and normalize 2nd local. */
  aScale.y = local[1].Length();
  local[1] /= aScale.y;
  aShear[ShearType::XYSHEAR] /= aScale.y;

  /* Compute XZ and YZ shears, make 3rd local orthogonal */
  aShear[ShearType::XZSHEAR] = local[0].DotProduct(local[2]);
  local[2] -= local[0] * aShear[ShearType::XZSHEAR];
  aShear[ShearType::YZSHEAR] = local[1].DotProduct(local[2]);
  local[2] -= local[1] * aShear[ShearType::YZSHEAR];

  /* Next, get Z scale and normalize 3rd local. */
  aScale.z = local[2].Length();
  local[2] /= aScale.z;

  aShear[ShearType::XZSHEAR] /= aScale.z;
  aShear[ShearType::YZSHEAR] /= aScale.z;

  /**
   * At this point, the matrix (in locals) is orthonormal.
   * Check for a coordinate system flip.  If the determinant
   * is -1, then negate the matrix and the scaling factors.
   */
  if (local[0].DotProduct(local[1].CrossProduct(local[2])) < 0) {
    aScale *= -1;
    for (int i = 0; i < 3; i++) {
      local[i] *= -1;
    }
  }

  /* Now, get the rotations out */
  aRotate = gfxQuaternion(local);

  return true;
}

Matrix
CSSValueArrayTo2DMatrix(nsCSSValue::Array* aArray)
{
  MOZ_ASSERT(aArray &&
             TransformFunctionOf(aArray) == eCSSKeyword_matrix &&
             aArray->Count() == 7);
  Matrix m(aArray->Item(1).GetFloatValue(),
           aArray->Item(2).GetFloatValue(),
           aArray->Item(3).GetFloatValue(),
           aArray->Item(4).GetFloatValue(),
           aArray->Item(5).GetFloatValue(),
           aArray->Item(6).GetFloatValue());
  return m;
}

Matrix4x4
CSSValueArrayTo3DMatrix(nsCSSValue::Array* aArray)
{
  MOZ_ASSERT(aArray &&
             TransformFunctionOf(aArray) == eCSSKeyword_matrix3d &&
             aArray->Count() == 17);
  gfx::Float array[16];
  for (size_t i = 0; i < 16; ++i) {
    array[i] = aArray->Item(i+1).GetFloatValue();
  }
  Matrix4x4 m(array);
  return m;
}

gfxSize
GetScaleValue(const nsCSSValueSharedList* aList,
              const nsIFrame* aForFrame)
{
  MOZ_ASSERT(aList && aList->mHead);
  MOZ_ASSERT(aForFrame);

  RuleNodeCacheConditions dontCare;
  bool dontCareBool;
  TransformReferenceBox refBox(aForFrame);
  Matrix4x4 transform = ReadTransforms(
                          aList->mHead,
                          aForFrame->StyleContext(),
                          aForFrame->PresContext(), dontCare, refBox,
                          aForFrame->PresContext()->AppUnitsPerDevPixel(),
                          &dontCareBool);
  Matrix transform2d;
  bool canDraw2D = transform.CanDraw2D(&transform2d);
  if (!canDraw2D) {
    return gfxSize();
  }

  return ThebesMatrix(transform2d).ScaleFactors(true);
}

} // namespace nsStyleTransformMatrix
