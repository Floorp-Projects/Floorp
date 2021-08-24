/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class used for intermediate representations of the -moz-transform property.
 */

#include "nsStyleTransformMatrix.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "mozilla/MotionPathUtils.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/SVGUtils.h"
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

void TransformReferenceBox::EnsureDimensionsAreCached() {
  if (mIsCached) {
    return;
  }

  MOZ_ASSERT(mFrame);

  mIsCached = true;

  if (mFrame->HasAnyStateBits(NS_FRAME_SVG_LAYOUT)) {
    if (mFrame->StyleDisplay()->mTransformBox == StyleGeometryBox::FillBox) {
      // Percentages in transforms resolve against the SVG bbox, and the
      // transform is relative to the top-left of the SVG bbox.
      nsRect bboxInAppUnits = nsLayoutUtils::ComputeGeometryBox(
          const_cast<nsIFrame*>(mFrame), StyleGeometryBox::FillBox);
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
      MOZ_ASSERT(
          mFrame->StyleDisplay()->mTransformBox == StyleGeometryBox::ViewBox ||
              mFrame->StyleDisplay()->mTransformBox ==
                  StyleGeometryBox::BorderBox,
          "Unexpected value for 'transform-box'");
      // Percentages in transforms resolve against the width/height of the
      // nearest viewport (or its viewBox if one is applied), and the
      // transform is relative to {0,0} in current user space.
      mX = -mFrame->GetPosition().x;
      mY = -mFrame->GetPosition().y;
      Size contextSize = SVGUtils::GetContextSize(mFrame);
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
  for (const nsIFrame* currFrame = mFrame->FirstContinuation();
       currFrame != nullptr; currFrame = currFrame->GetNextContinuation()) {
    // Get the frame rect in local coordinates, then translate back to the
    // original coordinates:
    rect.UnionRect(
        result, nsRect(currFrame->GetOffsetTo(mFrame), currFrame->GetSize()));
  }
#endif

  mX = 0;
  mY = 0;
  mWidth = rect.Width();
  mHeight = rect.Height();
}

void TransformReferenceBox::Init(const nsRect& aDimensions) {
  MOZ_ASSERT(!mFrame && !mIsCached);

  mX = aDimensions.x;
  mY = aDimensions.y;
  mWidth = aDimensions.width;
  mHeight = aDimensions.height;
  mIsCached = true;
}

float ProcessTranslatePart(
    const LengthPercentage& aValue, TransformReferenceBox* aRefBox,
    TransformReferenceBox::DimensionGetter aDimensionGetter) {
  return aValue.ResolveToCSSPixelsWith([&] {
    return aRefBox && !aRefBox->IsEmpty()
               ? CSSPixel::FromAppUnits((aRefBox->*aDimensionGetter)())
               : CSSCoord(0);
  });
}

/**
 * Helper functions to process all the transformation function types.
 *
 * These take a matrix parameter to accumulate the current matrix.
 */

/* Helper function to process a matrix entry. */
static void ProcessMatrix(Matrix4x4& aMatrix,
                          const StyleTransformOperation& aOp) {
  const auto& matrix = aOp.AsMatrix();
  gfxMatrix result;

  result._11 = matrix.a;
  result._12 = matrix.b;
  result._21 = matrix.c;
  result._22 = matrix.d;
  result._31 = matrix.e;
  result._32 = matrix.f;

  aMatrix = result * aMatrix;
}

static void ProcessMatrix3D(Matrix4x4& aMatrix,
                            const StyleTransformOperation& aOp) {
  Matrix4x4 temp;

  const auto& matrix = aOp.AsMatrix3D();

  temp._11 = matrix.m11;
  temp._12 = matrix.m12;
  temp._13 = matrix.m13;
  temp._14 = matrix.m14;
  temp._21 = matrix.m21;
  temp._22 = matrix.m22;
  temp._23 = matrix.m23;
  temp._24 = matrix.m24;
  temp._31 = matrix.m31;
  temp._32 = matrix.m32;
  temp._33 = matrix.m33;
  temp._34 = matrix.m34;

  temp._41 = matrix.m41;
  temp._42 = matrix.m42;
  temp._43 = matrix.m43;
  temp._44 = matrix.m44;

  aMatrix = temp * aMatrix;
}

// For accumulation for transform functions, |aOne| corresponds to |aB| and
// |aTwo| corresponds to |aA| for StyleAnimationValue::Accumulate().
class Accumulate {
 public:
  template <typename T>
  static T operate(const T& aOne, const T& aTwo, double aCoeff) {
    return aOne + aTwo * aCoeff;
  }

  static Point4D operateForPerspective(const Point4D& aOne, const Point4D& aTwo,
                                       double aCoeff) {
    return (aOne - Point4D(0, 0, 0, 1)) +
           (aTwo - Point4D(0, 0, 0, 1)) * aCoeff + Point4D(0, 0, 0, 1);
  }
  static Point3D operateForScale(const Point3D& aOne, const Point3D& aTwo,
                                 double aCoeff) {
    // For scale, the identify element is 1, see AddTransformScale in
    // StyleAnimationValue.cpp.
    return (aOne - Point3D(1, 1, 1)) + (aTwo - Point3D(1, 1, 1)) * aCoeff +
           Point3D(1, 1, 1);
  }

  static Matrix4x4 operateForRotate(const gfxQuaternion& aOne,
                                    const gfxQuaternion& aTwo, double aCoeff) {
    if (aCoeff == 0.0) {
      return aOne.ToMatrix();
    }

    double theta = acos(mozilla::clamped(aTwo.w, -1.0, 1.0));
    double scale = (theta != 0.0) ? 1.0 / sin(theta) : 0.0;
    theta *= aCoeff;
    scale *= sin(theta);

    gfxQuaternion result = gfxQuaternion(scale * aTwo.x, scale * aTwo.y,
                                         scale * aTwo.z, cos(theta)) *
                           aOne;
    return result.ToMatrix();
  }

  static Matrix4x4 operateForFallback(const Matrix4x4& aMatrix1,
                                      const Matrix4x4& aMatrix2,
                                      double aProgress) {
    return aMatrix1;
  }

  static Matrix4x4 operateByServo(const Matrix4x4& aMatrix1,
                                  const Matrix4x4& aMatrix2, double aCount) {
    Matrix4x4 result;
    Servo_MatrixTransform_Operate(MatrixTransformOperator::Accumulate,
                                  &aMatrix1.components, &aMatrix2.components,
                                  aCount, &result.components);
    return result;
  }
};

class Interpolate {
 public:
  template <typename T>
  static T operate(const T& aOne, const T& aTwo, double aCoeff) {
    return aOne + (aTwo - aOne) * aCoeff;
  }

  static Point4D operateForPerspective(const Point4D& aOne, const Point4D& aTwo,
                                       double aCoeff) {
    return aOne + (aTwo - aOne) * aCoeff;
  }

  static Point3D operateForScale(const Point3D& aOne, const Point3D& aTwo,
                                 double aCoeff) {
    return aOne + (aTwo - aOne) * aCoeff;
  }

  static Matrix4x4 operateForRotate(const gfxQuaternion& aOne,
                                    const gfxQuaternion& aTwo, double aCoeff) {
    return aOne.Slerp(aTwo, aCoeff).ToMatrix();
  }

  static Matrix4x4 operateForFallback(const Matrix4x4& aMatrix1,
                                      const Matrix4x4& aMatrix2,
                                      double aProgress) {
    return aProgress < 0.5 ? aMatrix1 : aMatrix2;
  }

  static Matrix4x4 operateByServo(const Matrix4x4& aMatrix1,
                                  const Matrix4x4& aMatrix2, double aProgress) {
    Matrix4x4 result;
    Servo_MatrixTransform_Operate(MatrixTransformOperator::Interpolate,
                                  &aMatrix1.components, &aMatrix2.components,
                                  aProgress, &result.components);
    return result;
  }
};

template <typename Operator>
static void ProcessMatrixOperator(Matrix4x4& aMatrix,
                                  const StyleTransform& aFrom,
                                  const StyleTransform& aTo, float aProgress,
                                  TransformReferenceBox& aRefBox) {
  float appUnitPerCSSPixel = AppUnitsPerCSSPixel();
  Matrix4x4 matrix1 = ReadTransforms(aFrom, aRefBox, appUnitPerCSSPixel);
  Matrix4x4 matrix2 = ReadTransforms(aTo, aRefBox, appUnitPerCSSPixel);
  aMatrix = Operator::operateByServo(matrix1, matrix2, aProgress) * aMatrix;
}

/* Helper function to process two matrices that we need to interpolate between
 */
void ProcessInterpolateMatrix(Matrix4x4& aMatrix,
                              const StyleTransformOperation& aOp,
                              TransformReferenceBox& aRefBox) {
  const auto& args = aOp.AsInterpolateMatrix();
  ProcessMatrixOperator<Interpolate>(aMatrix, args.from_list, args.to_list,
                                     args.progress._0, aRefBox);
}

void ProcessAccumulateMatrix(Matrix4x4& aMatrix,
                             const StyleTransformOperation& aOp,
                             TransformReferenceBox& aRefBox) {
  const auto& args = aOp.AsAccumulateMatrix();
  ProcessMatrixOperator<Accumulate>(aMatrix, args.from_list, args.to_list,
                                    args.count, aRefBox);
}

/* Helper function to process a translatex function. */
static void ProcessTranslateX(Matrix4x4& aMatrix,
                              const LengthPercentage& aLength,
                              TransformReferenceBox& aRefBox) {
  Point3D temp;
  temp.x =
      ProcessTranslatePart(aLength, &aRefBox, &TransformReferenceBox::Width);
  aMatrix.PreTranslate(temp);
}

/* Helper function to process a translatey function. */
static void ProcessTranslateY(Matrix4x4& aMatrix,
                              const LengthPercentage& aLength,
                              TransformReferenceBox& aRefBox) {
  Point3D temp;
  temp.y =
      ProcessTranslatePart(aLength, &aRefBox, &TransformReferenceBox::Height);
  aMatrix.PreTranslate(temp);
}

static void ProcessTranslateZ(Matrix4x4& aMatrix, const Length& aLength) {
  Point3D temp;
  temp.z = aLength.ToCSSPixels();
  aMatrix.PreTranslate(temp);
}

/* Helper function to process a translate function. */
static void ProcessTranslate(Matrix4x4& aMatrix, const LengthPercentage& aX,
                             const LengthPercentage& aY,
                             TransformReferenceBox& aRefBox) {
  Point3D temp;
  temp.x = ProcessTranslatePart(aX, &aRefBox, &TransformReferenceBox::Width);
  temp.y = ProcessTranslatePart(aY, &aRefBox, &TransformReferenceBox::Height);
  aMatrix.PreTranslate(temp);
}

static void ProcessTranslate3D(Matrix4x4& aMatrix, const LengthPercentage& aX,
                               const LengthPercentage& aY, const Length& aZ,
                               TransformReferenceBox& aRefBox) {
  Point3D temp;

  temp.x = ProcessTranslatePart(aX, &aRefBox, &TransformReferenceBox::Width);
  temp.y = ProcessTranslatePart(aY, &aRefBox, &TransformReferenceBox::Height);
  temp.z = aZ.ToCSSPixels();

  aMatrix.PreTranslate(temp);
}

/* Helper function to set up a scale matrix. */
static void ProcessScaleHelper(Matrix4x4& aMatrix, float aXScale, float aYScale,
                               float aZScale) {
  aMatrix.PreScale(aXScale, aYScale, aZScale);
}

static void ProcessScale3D(Matrix4x4& aMatrix,
                           const StyleTransformOperation& aOp) {
  const auto& scale = aOp.AsScale3D();
  ProcessScaleHelper(aMatrix, scale._0, scale._1, scale._2);
}

/* Helper function that, given a set of angles, constructs the appropriate
 * skew matrix.
 */
static void ProcessSkewHelper(Matrix4x4& aMatrix, const StyleAngle& aXAngle,
                              const StyleAngle& aYAngle) {
  aMatrix.SkewXY(aXAngle.ToRadians(), aYAngle.ToRadians());
}

static void ProcessRotate3D(Matrix4x4& aMatrix, float aX, float aY, float aZ,
                            const StyleAngle& aAngle) {
  Matrix4x4 temp;
  temp.SetRotateAxisAngle(aX, aY, aZ, aAngle.ToRadians());
  aMatrix = temp * aMatrix;
}

static void ProcessPerspective(Matrix4x4& aMatrix, const StyleGenericPerspectiveFunction<Length>& aPerspective) {
  if (aPerspective.IsNone()) {
    return;
  }
  float p = aPerspective.AsLength().ToCSSPixels();
  if (!std::isinf(p)) {
    aMatrix.Perspective(std::max(p, 1.0f));
  }
}

static void MatrixForTransformFunction(Matrix4x4& aMatrix,
                                       const StyleTransformOperation& aOp,
                                       TransformReferenceBox& aRefBox) {
  /* Get the keyword for the transform. */
  switch (aOp.tag) {
    case StyleTransformOperation::Tag::TranslateX:
      ProcessTranslateX(aMatrix, aOp.AsTranslateX(), aRefBox);
      break;
    case StyleTransformOperation::Tag::TranslateY:
      ProcessTranslateY(aMatrix, aOp.AsTranslateY(), aRefBox);
      break;
    case StyleTransformOperation::Tag::TranslateZ:
      ProcessTranslateZ(aMatrix, aOp.AsTranslateZ());
      break;
    case StyleTransformOperation::Tag::Translate:
      ProcessTranslate(aMatrix, aOp.AsTranslate()._0, aOp.AsTranslate()._1,
                       aRefBox);
      break;
    case StyleTransformOperation::Tag::Translate3D:
      return ProcessTranslate3D(aMatrix, aOp.AsTranslate3D()._0,
                                aOp.AsTranslate3D()._1, aOp.AsTranslate3D()._2,
                                aRefBox);
      break;
    case StyleTransformOperation::Tag::ScaleX:
      ProcessScaleHelper(aMatrix, aOp.AsScaleX(), 1.0f, 1.0f);
      break;
    case StyleTransformOperation::Tag::ScaleY:
      ProcessScaleHelper(aMatrix, 1.0f, aOp.AsScaleY(), 1.0f);
      break;
    case StyleTransformOperation::Tag::ScaleZ:
      ProcessScaleHelper(aMatrix, 1.0f, 1.0f, aOp.AsScaleZ());
      break;
    case StyleTransformOperation::Tag::Scale:
      ProcessScaleHelper(aMatrix, aOp.AsScale()._0, aOp.AsScale()._1, 1.0f);
      break;
    case StyleTransformOperation::Tag::Scale3D:
      ProcessScale3D(aMatrix, aOp);
      break;
    case StyleTransformOperation::Tag::SkewX:
      ProcessSkewHelper(aMatrix, aOp.AsSkewX(), StyleAngle::Zero());
      break;
    case StyleTransformOperation::Tag::SkewY:
      ProcessSkewHelper(aMatrix, StyleAngle::Zero(), aOp.AsSkewY());
      break;
    case StyleTransformOperation::Tag::Skew:
      ProcessSkewHelper(aMatrix, aOp.AsSkew()._0, aOp.AsSkew()._1);
      break;
    case StyleTransformOperation::Tag::RotateX:
      aMatrix.RotateX(aOp.AsRotateX().ToRadians());
      break;
    case StyleTransformOperation::Tag::RotateY:
      aMatrix.RotateY(aOp.AsRotateY().ToRadians());
      break;
    case StyleTransformOperation::Tag::RotateZ:
      aMatrix.RotateZ(aOp.AsRotateZ().ToRadians());
      break;
    case StyleTransformOperation::Tag::Rotate:
      aMatrix.RotateZ(aOp.AsRotate().ToRadians());
      break;
    case StyleTransformOperation::Tag::Rotate3D:
      ProcessRotate3D(aMatrix, aOp.AsRotate3D()._0, aOp.AsRotate3D()._1,
                      aOp.AsRotate3D()._2, aOp.AsRotate3D()._3);
      break;
    case StyleTransformOperation::Tag::Matrix:
      ProcessMatrix(aMatrix, aOp);
      break;
    case StyleTransformOperation::Tag::Matrix3D:
      ProcessMatrix3D(aMatrix, aOp);
      break;
    case StyleTransformOperation::Tag::InterpolateMatrix:
      ProcessInterpolateMatrix(aMatrix, aOp, aRefBox);
      break;
    case StyleTransformOperation::Tag::AccumulateMatrix:
      ProcessAccumulateMatrix(aMatrix, aOp, aRefBox);
      break;
    case StyleTransformOperation::Tag::Perspective:
      ProcessPerspective(aMatrix, aOp.AsPerspective());
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown transform function!");
  }
}

Matrix4x4 ReadTransforms(const StyleTransform& aTransform,
                         TransformReferenceBox& aRefBox,
                         float aAppUnitsPerMatrixUnit) {
  Matrix4x4 result;

  for (const StyleTransformOperation& op : aTransform.Operations()) {
    MatrixForTransformFunction(result, op, aRefBox);
  }

  float scale = float(AppUnitsPerCSSPixel()) / aAppUnitsPerMatrixUnit;
  result.PreScale(1 / scale, 1 / scale, 1 / scale);
  result.PostScale(scale, scale, scale);

  return result;
}

static void ProcessTranslate(Matrix4x4& aMatrix,
                             const StyleTranslate& aTranslate,
                             TransformReferenceBox& aRefBox) {
  switch (aTranslate.tag) {
    case StyleTranslate::Tag::None:
      return;
    case StyleTranslate::Tag::Translate:
      return ProcessTranslate3D(aMatrix, aTranslate.AsTranslate()._0,
                                aTranslate.AsTranslate()._1,
                                aTranslate.AsTranslate()._2, aRefBox);
    default:
      MOZ_ASSERT_UNREACHABLE("Huh?");
  }
}

static void ProcessRotate(Matrix4x4& aMatrix, const StyleRotate& aRotate) {
  switch (aRotate.tag) {
    case StyleRotate::Tag::None:
      return;
    case StyleRotate::Tag::Rotate:
      aMatrix.RotateZ(aRotate.AsRotate().ToRadians());
      return;
    case StyleRotate::Tag::Rotate3D:
      return ProcessRotate3D(aMatrix, aRotate.AsRotate3D()._0,
                             aRotate.AsRotate3D()._1, aRotate.AsRotate3D()._2,
                             aRotate.AsRotate3D()._3);
    default:
      MOZ_ASSERT_UNREACHABLE("Huh?");
  }
}

static void ProcessScale(Matrix4x4& aMatrix, const StyleScale& aScale) {
  switch (aScale.tag) {
    case StyleScale::Tag::None:
      return;
    case StyleScale::Tag::Scale:
      return ProcessScaleHelper(aMatrix, aScale.AsScale()._0,
                                aScale.AsScale()._1, aScale.AsScale()._2);
    default:
      MOZ_ASSERT_UNREACHABLE("Huh?");
  }
}

Matrix4x4 ReadTransforms(const StyleTranslate& aTranslate,
                         const StyleRotate& aRotate, const StyleScale& aScale,
                         const Maybe<ResolvedMotionPathData>& aMotion,
                         const StyleTransform& aTransform,
                         TransformReferenceBox& aRefBox,
                         float aAppUnitsPerMatrixUnit) {
  Matrix4x4 result;

  ProcessTranslate(result, aTranslate, aRefBox);
  ProcessRotate(result, aRotate);
  ProcessScale(result, aScale);

  if (aMotion.isSome()) {
    // Create the equivalent translate and rotate function, according to the
    // order in spec. We combine the translate and then the rotate.
    // https://drafts.fxtf.org/motion-1/#calculating-path-transform
    //
    // Besides, we have to shift the object by the delta between anchor-point
    // and transform-origin, to make sure we rotate the object according to
    // anchor-point.
    result.PreTranslate(aMotion->mTranslate.x + aMotion->mShift.x,
                        aMotion->mTranslate.y + aMotion->mShift.y, 0.0);
    if (aMotion->mRotate != 0.0) {
      result.RotateZ(aMotion->mRotate);
    }
    // Shift the origin back to transform-origin.
    result.PreTranslate(-aMotion->mShift.x, -aMotion->mShift.y, 0.0);
  }

  for (const StyleTransformOperation& op : aTransform.Operations()) {
    MatrixForTransformFunction(result, op, aRefBox);
  }

  float scale = float(AppUnitsPerCSSPixel()) / aAppUnitsPerMatrixUnit;
  result.PreScale(1 / scale, 1 / scale, 1 / scale);
  result.PostScale(scale, scale, scale);

  return result;
}

mozilla::CSSPoint Convert2DPosition(const mozilla::LengthPercentage& aX,
                                    const mozilla::LengthPercentage& aY,
                                    const CSSSize& aSize) {
  return {
      aX.ResolveToCSSPixels(aSize.width),
      aY.ResolveToCSSPixels(aSize.height),
  };
}

CSSPoint Convert2DPosition(const LengthPercentage& aX,
                           const LengthPercentage& aY,
                           TransformReferenceBox& aRefBox) {
  return {
      aX.ResolveToCSSPixelsWith(
          [&] { return CSSPixel::FromAppUnits(aRefBox.Width()); }),
      aY.ResolveToCSSPixelsWith(
          [&] { return CSSPixel::FromAppUnits(aRefBox.Height()); }),
  };
}

Point Convert2DPosition(const LengthPercentage& aX, const LengthPercentage& aY,
                        TransformReferenceBox& aRefBox,
                        int32_t aAppUnitsPerPixel) {
  float scale = mozilla::AppUnitsPerCSSPixel() / float(aAppUnitsPerPixel);
  CSSPoint p = Convert2DPosition(aX, aY, aRefBox);
  return {p.x * scale, p.y * scale};
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
 *     In step 3, the X scale factor is sqrt(1+tan²(φ)) = sqrt(sec²(φ)) =
 * sec(φ). Thus, after step 3, A = 1/sec(φ) = cos(φ) and B = tan(φ) / sec(φ) =
 * sin(φ). In step 4, the XY shear is sin(φ). Thus, after step 4, C =
 * -cos(φ)sin(φ) and D = 1 - sin²(φ) = cos²(φ). Thus, in step 5, the Y scale is
 * sqrt(cos²(φ)(sin²(φ) + cos²(φ)) = cos(φ). Thus, after step 5, C = -sin(φ), D
 * = cos(φ), and the XY shear is tan(φ). Thus, in step 6, A * D - B * C =
 * cos²(φ) + sin²(φ) = 1. In step 7, the rotation is thus φ.
 *
 *   skew(θ, φ), which is matrix(1, tan(φ), tan(θ), 1, 0, 0), which decomposes
 *   to 'rotate(φ) skewX(θ + φ) scale(sec(φ), cos(φ))' since (ignoring
 *   the alternate sign possibilities that would get fixed in step 6):
 *     In step 3, the X scale factor is sqrt(1+tan²(φ)) = sqrt(sec²(φ)) =
 * sec(φ). Thus, after step 3, A = 1/sec(φ) = cos(φ) and B = tan(φ) / sec(φ) =
 * sin(φ). In step 4, the XY shear is cos(φ)tan(θ) + sin(φ). Thus, after step 4,
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

bool Decompose2DMatrix(const Matrix& aMatrix, Point3D& aScale,
                       ShearArray& aShear, gfxQuaternion& aRotate,
                       Point3D& aTranslate) {
  float A = aMatrix._11, B = aMatrix._12, C = aMatrix._21, D = aMatrix._22;
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

  float determinant = A * D - B * C;
  // Determinant should now be 1 or -1.
  if (0.99 > Abs(determinant) || Abs(determinant) > 1.01) {
    return false;
  }

  if (determinant < 0) {
    A = -A;
    B = -B;
    C = -C;
    D = -D;
    XYshear = -XYshear;
    scaleX = -scaleX;
  }

  float rotate = atan2f(B, A);
  aRotate = gfxQuaternion(0, 0, sin(rotate / 2), cos(rotate / 2));
  aShear[ShearType::XY] = XYshear;
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
bool Decompose3DMatrix(const Matrix4x4& aMatrix, Point3D& aScale,
                       ShearArray& aShear, gfxQuaternion& aRotate,
                       Point3D& aTranslate, Point4D& aPerspective) {
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
  if (local[0][3] != 0 || local[1][3] != 0 || local[2][3] != 0) {
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
  aShear[ShearType::XY] = local[0].DotProduct(local[1]);
  local[1] -= local[0] * aShear[ShearType::XY];

  /* Now, compute Y scale and normalize 2nd local. */
  aScale.y = local[1].Length();
  local[1] /= aScale.y;
  aShear[ShearType::XY] /= aScale.y;

  /* Compute XZ and YZ shears, make 3rd local orthogonal */
  aShear[ShearType::XZ] = local[0].DotProduct(local[2]);
  local[2] -= local[0] * aShear[ShearType::XZ];
  aShear[ShearType::YZ] = local[1].DotProduct(local[2]);
  local[2] -= local[1] * aShear[ShearType::YZ];

  /* Next, get Z scale and normalize 3rd local. */
  aScale.z = local[2].Length();
  local[2] /= aScale.z;

  aShear[ShearType::XZ] /= aScale.z;
  aShear[ShearType::YZ] /= aScale.z;

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

}  // namespace nsStyleTransformMatrix
