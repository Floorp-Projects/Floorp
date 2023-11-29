/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class used for intermediate representations of the transform and
 * transform-like properties.
 */

#include "nsStyleTransformMatrix.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "mozilla/MotionPathUtils.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/StaticPrefs_layout.h"
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
// #define UNIFIED_CONTINUATIONS

static nsRect GetSVGBox(const nsIFrame* aFrame) {
  auto computeViewBox = [&]() {
    // Percentages in transforms resolve against the width/height of the
    // nearest viewport (or its viewBox if one is applied), and the
    // transform is relative to {0,0} in current user space.
    CSSSize size = CSSSize::FromUnknownSize(SVGUtils::GetContextSize(aFrame));
    return nsRect(-aFrame->GetPosition(), CSSPixel::ToAppUnits(size));
  };

  auto transformBox = aFrame->StyleDisplay()->mTransformBox;
  if ((transformBox == StyleTransformBox::StrokeBox ||
       transformBox == StyleTransformBox::BorderBox) &&
      aFrame->StyleSVGReset()->HasNonScalingStroke()) {
    // To calculate stroke bounds for an element with `non-scaling-stroke` we
    // need to resolve its transform to its outer-svg, but to resolve that
    // transform when it has `transform-box:stroke-box` (or `border-box`)
    // may require its stroke bounds. There's no ideal way to break this
    // cyclical dependency, but we break it by converting to
    // `transform-box:fill-box` here.
    // https://github.com/w3c/csswg-drafts/issues/9640
    transformBox = StyleTransformBox::FillBox;
  }

  // For SVG elements without associated CSS layout box, the used value for
  // content-box is fill-box and for border-box is stroke-box.
  // https://drafts.csswg.org/css-transforms-1/#transform-box
  switch (transformBox) {
    case StyleTransformBox::ContentBox:
    case StyleTransformBox::FillBox: {
      // Percentages in transforms resolve against the SVG bbox, and the
      // transform is relative to the top-left of the SVG bbox.
      nsRect bboxInAppUnits = nsLayoutUtils::ComputeSVGReferenceRect(
          const_cast<nsIFrame*>(aFrame), StyleGeometryBox::FillBox);
      // The mRect of an SVG nsIFrame is its user space bounds *including*
      // stroke and markers, whereas bboxInAppUnits is its user space bounds
      // including fill only.  We need to note the offset of the reference box
      // from the frame's mRect in mX/mY.
      return {bboxInAppUnits.x - aFrame->GetPosition().x,
              bboxInAppUnits.y - aFrame->GetPosition().y, bboxInAppUnits.width,
              bboxInAppUnits.height};
    }
    case StyleTransformBox::BorderBox:
      if (!StaticPrefs::layout_css_transform_box_content_stroke_enabled()) {
        // If stroke-box is disabled, we shouldn't use it and fall back to
        // view-box.
        return computeViewBox();
      }
      [[fallthrough]];
    case StyleTransformBox::StrokeBox: {
      // We are using SVGUtils::PathExtentsToMaxStrokeExtents() to compute the
      // bbox contribution for stroke box (if it doesn't have simple bounds),
      // so the |strokeBox| here may be larger than the author's expectation.
      // Using Moz2D to compute the tighter bounding box is another way but it
      // has some potential issues (see SVGGeometryFrame::GetBBoxContribution()
      // for more details), and its result depends on the drawing backend. So
      // for now we still rely on our default calcuclation for SVG geometry
      // frame reflow code. At least this works for the shape elements which
      // have simple bounds.
      // FIXME: Bug 1849054. We may have to update
      // SVGGeometryFrame::GetBBoxContribution() to get tighter stroke bounds.
      nsRect strokeBox = nsLayoutUtils::ComputeSVGReferenceRect(
          const_cast<nsIFrame*>(aFrame), StyleGeometryBox::StrokeBox);
      // The |nsIFrame::mRect| includes markers, so we have to compute the
      // offsets without markers.
      return nsRect{strokeBox.x - aFrame->GetPosition().x,
                    strokeBox.y - aFrame->GetPosition().y, strokeBox.width,
                    strokeBox.height};
    }
    case StyleTransformBox::ViewBox:
      return computeViewBox();
  }

  MOZ_ASSERT_UNREACHABLE("All transform box should be handled.");
  return {};
}

void TransformReferenceBox::EnsureDimensionsAreCached() {
  if (mIsCached) {
    return;
  }

  MOZ_ASSERT(mFrame);

  mIsCached = true;

  if (mFrame->HasAnyStateBits(NS_FRAME_SVG_LAYOUT)) {
    mBox = GetSVGBox(mFrame);
    return;
  }

  // For elements with associated CSS layout box, the used value for fill-box is
  // content-box and for stroke-box and view-box is border-box.
  // https://drafts.csswg.org/css-transforms-1/#transform-box
  switch (mFrame->StyleDisplay()->mTransformBox) {
    case StyleTransformBox::FillBox:
    case StyleTransformBox::ContentBox: {
      mBox = mFrame->GetContentRectRelativeToSelf();
      return;
    }
    case StyleTransformBox::StrokeBox:
      // TODO: Implement this in the following patches.
      return;
    case StyleTransformBox::ViewBox:
    case StyleTransformBox::BorderBox: {
      // If UNIFIED_CONTINUATIONS is not defined, this is simply the frame's
      // bounding rectangle, translated to the origin.  Otherwise, it is the
      // smallest rectangle containing a frame and all of its continuations. For
      // example, if there is a <span> element with several continuations split
      // over several lines, this function will return the rectangle containing
      // all of those continuations.

      nsRect rect;

#ifndef UNIFIED_CONTINUATIONS
      rect = mFrame->GetRect();
#else
      // Iterate the continuation list, unioning together the bounding rects:
      for (const nsIFrame* currFrame = mFrame->FirstContinuation();
           currFrame != nullptr; currFrame = currFrame->GetNextContinuation()) {
        // Get the frame rect in local coordinates, then translate back to the
        // original coordinates:
        rect.UnionRect(result, nsRect(currFrame->GetOffsetTo(mFrame),
                                      currFrame->GetSize()));
      }
#endif

      mBox = {0, 0, rect.Width(), rect.Height()};
      return;
    }
  }
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

static void ProcessPerspective(
    Matrix4x4& aMatrix,
    const StyleGenericPerspectiveFunction<Length>& aPerspective) {
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
                         const ResolvedMotionPathData* aMotion,
                         const StyleTransform& aTransform,
                         TransformReferenceBox& aRefBox,
                         float aAppUnitsPerMatrixUnit) {
  Matrix4x4 result;

  ProcessTranslate(result, aTranslate, aRefBox);
  ProcessRotate(result, aRotate);
  ProcessScale(result, aScale);

  if (aMotion) {
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

}  // namespace nsStyleTransformMatrix
