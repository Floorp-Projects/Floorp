/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MotionPathUtils.h"

#include "gfxPlatform.h"
#include "mozilla/dom/SVGPathData.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/layers/LayersMessages.h"
#include "mozilla/RefPtr.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsStyleTransformMatrix.h"

#include <math.h>

namespace mozilla {

using nsStyleTransformMatrix::TransformReferenceBox;

RayReferenceData::RayReferenceData(const nsIFrame* aFrame) {
  // FIXME: Bug 1581237: Should use view-box for the SVG context.
  const nsIFrame* container = aFrame->GetContainingBlock();
  if (!container) {
    // If there is no parent frame, it's impossible to calculate the path
    // length, so does the path.
    return;
  }

  // The initial position is (0, 0) in |aFrame|, and we have to transform it
  // into the space of |container|, so use GetOffsetsTo() to get the delta
  // value.
  // FIXME: Bug 1559232: The initial position will be adjusted by offset
  // starting position.
  // https://drafts.fxtf.org/motion-1/#offset-position-property
  mInitialPosition = CSSPoint::FromAppUnits(aFrame->GetOffsetTo(container));
  // FIXME: Bug 1581237: We should use <coord-box> as the box of its containing
  // block. https://drafts.fxtf.org/motion-1/#valdef-offset-path-coord-box
  mContainingBlockRect =
      CSSRect::FromAppUnits(container->GetRectRelativeToSelf());
  // We use the border-box size to calculate the reduced path length when using
  // "contain" keyword.
  // https://drafts.fxtf.org/motion-1/#valdef-ray-contain
  //
  // Note: Per the spec, border-box is treated as stroke-box in the SVG context,
  // Also, SVGUtils::GetBBox() may cache the box via the frame property, so we
  // have to do const-casting.
  // https://drafts.csswg.org/css-box-4/#valdef-box-border-box
  mBorderBoxSize = CSSSize::FromAppUnits(
      nsLayoutUtils::ComputeGeometryBox(const_cast<nsIFrame*>(aFrame),
                                        // StrokeBox and BorderBox are in the
                                        // same switch case for CSS context.
                                        StyleGeometryBox::StrokeBox)
          .Size());
}

// The distance is measured between the initial position and the intersection of
// the ray with the box
// https://drafts.fxtf.org/motion-1/#size-sides
static CSSCoord ComputeSides(const CSSPoint& aInitialPosition,
                             const CSSSize& aContainerSize,
                             const StyleAngle& aAngle) {
  // Given an acute angle |theta| (i.e. |t|) of a right-angled triangle, the
  // hypotenuse |h| is the side that connects the two acute angles. The side
  // |b| adjacent to |theta| is the side of the triangle that connects |theta|
  // to the right angle.
  //
  // e.g. if the angle |t| is 0 ~ 90 degrees, and b * tan(theta) <= b',
  //      h = b / cos(t):
  //                       b*tan(t)
  //       (0, 0) #--------*-----*--# (aContainerSize.width, 0)
  //              |        |    /   |
  //              |        |   /    |
  //              |        b  h     |
  //              |        |t/      |
  //              |        |/       |
  //    (aInitialPosition) *---b'---* (aContainerSize.width, aInitialPosition.y)
  //              |        |        |
  //              |        |        |
  //              |        |        |
  //              |        |        |
  //              |        |        |
  //              #-----------------# (aContainerSize.width,
  //  (0, aContainerSize.height)       aContainerSize.height)
  double theta = aAngle.ToRadians();
  double sint = std::sin(theta);
  double cost = std::cos(theta);

  double b = cost >= 0 ? aInitialPosition.y.value
                       : aContainerSize.height - aInitialPosition.y.value;
  double bPrime = sint >= 0 ? aContainerSize.width - aInitialPosition.x.value
                            : aInitialPosition.x.value;
  sint = std::fabs(sint);
  cost = std::fabs(cost);

  // If |b * tan(theta)| is larger than |bPrime|, the intersection is
  // on the other side, and |b'| is the opposite side of angle |theta| in this
  // case.
  //
  // e.g. If b * tan(theta) > b', h = b' / sin(theta):
  //   *----*
  //   |    |
  //   |   /|
  //   b  /t|
  //   |t/  |
  //   |/   |
  //   *-b'-*
  if (b * sint > bPrime * cost) {
    return bPrime / sint;
  }
  return b / cost;
}

static CSSCoord ComputeRayPathLength(const StyleRaySize aRaySizeType,
                                     const StyleAngle& aAngle,
                                     const RayReferenceData& aRayData) {
  if (aRaySizeType == StyleRaySize::Sides) {
    // If the initial position is not within the box, the distance is 0.
    if (!aRayData.mContainingBlockRect.Contains(aRayData.mInitialPosition)) {
      return 0.0;
    }

    return ComputeSides(aRayData.mInitialPosition,
                        aRayData.mContainingBlockRect.Size(), aAngle);
  }

  // left: the length between the initial point and the left side.
  // right: the length between the initial point and the right side.
  // top: the length between the initial point and the top side.
  // bottom: the lenght between the initial point and the bottom side.
  CSSCoord left = std::abs(aRayData.mInitialPosition.x);
  CSSCoord right = std::abs(aRayData.mContainingBlockRect.width -
                            aRayData.mInitialPosition.x);
  CSSCoord top = std::abs(aRayData.mInitialPosition.y);
  CSSCoord bottom = std::abs(aRayData.mContainingBlockRect.height -
                             aRayData.mInitialPosition.y);

  switch (aRaySizeType) {
    case StyleRaySize::ClosestSide:
      return std::min({left, right, top, bottom});

    case StyleRaySize::FarthestSide:
      return std::max({left, right, top, bottom});

    case StyleRaySize::ClosestCorner:
    case StyleRaySize::FarthestCorner: {
      CSSCoord h = 0;
      CSSCoord v = 0;
      if (aRaySizeType == StyleRaySize::ClosestCorner) {
        h = std::min(left, right);
        v = std::min(top, bottom);
      } else {
        h = std::max(left, right);
        v = std::max(top, bottom);
      }
      return sqrt(h.value * h.value + v.value * v.value);
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported ray size");
  }

  return 0.0;
}

static CSSCoord ComputeRayUsedDistance(const RayFunction& aRay,
                                       const LengthPercentage& aDistance,
                                       const CSSCoord& aPathLength,
                                       const CSSSize& aBorderBoxSize) {
  CSSCoord usedDistance = aDistance.ResolveToCSSPixels(aPathLength);
  if (!aRay.contain) {
    return usedDistance;
  }

  // The length of the offset path is reduced so that the element stays within
  // the containing block even at offset-distance: 100%. Specifically, the
  // path’s length is reduced by half the width or half the height of the
  // element’s border box, whichever is larger, and floored at zero.
  // https://drafts.fxtf.org/motion-1/#valdef-ray-contain
  return std::max(
      usedDistance -
          std::max(aBorderBoxSize.width, aBorderBoxSize.height) / 2.0f,
      0.0f);
}

/* static */
CSSPoint MotionPathUtils::ComputeAnchorPointAdjustment(const nsIFrame& aFrame) {
  if (!aFrame.HasAnyStateBits(NS_FRAME_SVG_LAYOUT)) {
    return {};
  }

  auto transformBox = aFrame.StyleDisplay()->mTransformBox;
  if (transformBox == StyleGeometryBox::ViewBox ||
      transformBox == StyleGeometryBox::BorderBox) {
    return {};
  }

  if (aFrame.IsFrameOfType(nsIFrame::eSVGContainer)) {
    nsRect boxRect = nsLayoutUtils::ComputeGeometryBox(
        const_cast<nsIFrame*>(&aFrame), StyleGeometryBox::FillBox);
    return CSSPoint::FromAppUnits(boxRect.TopLeft());
  }
  return CSSPoint::FromAppUnits(aFrame.GetPosition());
}

/* static */
Maybe<ResolvedMotionPathData> MotionPathUtils::ResolveMotionPath(
    const OffsetPathData& aPath, const LengthPercentage& aDistance,
    const StyleOffsetRotate& aRotate, const StylePositionOrAuto& aAnchor,
    const CSSPoint& aTransformOrigin, TransformReferenceBox& aRefBox,
    const CSSPoint& aAnchorPointAdjustment) {
  if (aPath.IsNone()) {
    return Nothing();
  }

  // Compute the point and angle for creating the equivalent translate and
  // rotate.
  double directionAngle = 0.0;
  gfx::Point point;
  if (aPath.IsPath()) {
    const auto& path = aPath.AsPath();
    if (!path.mGfxPath) {
      // Empty gfx::Path means it is path('') (i.e. empty path string).
      return Nothing();
    }

    // Per the spec, we have to convert offset distance to pixels, with 100%
    // being converted to total length. So here |gfxPath| is built with CSS
    // pixel, and we calculate |pathLength| and |computedDistance| with CSS
    // pixel as well.
    gfx::Float pathLength = path.mGfxPath->ComputeLength();
    gfx::Float usedDistance =
        aDistance.ResolveToCSSPixels(CSSCoord(pathLength));
    if (path.mIsClosedIntervals) {
      // Per the spec, let used offset distance be equal to offset distance
      // modulus the total length of the path. If the total length of the path
      // is 0, used offset distance is also 0.
      usedDistance = pathLength > 0.0 ? fmod(usedDistance, pathLength) : 0.0;
      // We make sure |usedDistance| is 0.0 or a positive value.
      // https://github.com/w3c/fxtf-drafts/issues/339
      if (usedDistance < 0.0) {
        usedDistance += pathLength;
      }
    } else {
      // Per the spec, for unclosed interval, let used offset distance be equal
      // to offset distance clamped by 0 and the total length of the path.
      usedDistance = clamped(usedDistance, 0.0f, pathLength);
    }
    gfx::Point tangent;
    point = path.mGfxPath->ComputePointAtLength(usedDistance, &tangent);
    directionAngle = (double)atan2(tangent.y, tangent.x);  // In Radian.
  } else if (aPath.IsRay()) {
    const auto& ray = aPath.AsRay();
    MOZ_ASSERT(ray.mRay);

    CSSCoord pathLength =
        ComputeRayPathLength(ray.mRay->size, ray.mRay->angle, ray.mData);
    CSSCoord usedDistance = ComputeRayUsedDistance(
        *ray.mRay, aDistance, pathLength, ray.mData.mBorderBoxSize);

    // 0deg pointing up and positive angles representing clockwise rotation.
    directionAngle =
        StyleAngle{ray.mRay->angle.ToDegrees() - 90.0f}.ToRadians();

    point.x = usedDistance * cos(directionAngle);
    point.y = usedDistance * sin(directionAngle);
  } else {
    MOZ_ASSERT_UNREACHABLE("Unsupported offset-path value");
    return Nothing();
  }

  // If |rotate.auto_| is true, the element should be rotated by the angle of
  // the direction (i.e. directional tangent vector) of the offset-path, and the
  // computed value of <angle> is added to this.
  // Otherwise, the element has a constant clockwise rotation transformation
  // applied to it by the specified rotation angle. (i.e. Don't need to
  // consider the direction of the path.)
  gfx::Float angle = static_cast<gfx::Float>(
      (aRotate.auto_ ? directionAngle : 0.0) + aRotate.angle.ToRadians());

  // Compute the offset for motion path translate.
  // Bug 1559232: the translate parameters will be adjusted more after we
  // support offset-position.
  // Per the spec, the default offset-anchor is `auto`, so initialize the anchor
  // point to transform-origin.
  CSSPoint anchorPoint(aTransformOrigin);
  gfx::Point shift;
  if (!aAnchor.IsAuto()) {
    const auto& pos = aAnchor.AsPosition();
    anchorPoint = nsStyleTransformMatrix::Convert2DPosition(
        pos.horizontal, pos.vertical, aRefBox);
    // We need this value to shift the origin from transform-origin to
    // offset-anchor (and vice versa).
    // See nsStyleTransformMatrix::ReadTransform for more details.
    shift = (anchorPoint - aTransformOrigin).ToUnknownPoint();
  }

  anchorPoint += aAnchorPointAdjustment;

  return Some(ResolvedMotionPathData{point - anchorPoint.ToUnknownPoint(),
                                     angle, shift});
}

static OffsetPathData GenerateOffsetPathData(const nsIFrame* aFrame) {
  const StyleOffsetPath& path = aFrame->StyleDisplay()->mOffsetPath;
  switch (path.tag) {
    case StyleOffsetPath::Tag::Path: {
      const StyleSVGPathData& pathData = path.AsPath();
      RefPtr<gfx::Path> gfxPath =
          aFrame->GetProperty(nsIFrame::OffsetPathCache());
      MOZ_ASSERT(
          gfxPath || pathData._0.IsEmpty(),
          "Should have a valid cached gfx::Path or an empty path string");
      return OffsetPathData::Path(pathData, gfxPath.forget());
    }
    case StyleOffsetPath::Tag::Ray:
      return OffsetPathData::Ray(path.AsRay(), RayReferenceData(aFrame));
    case StyleOffsetPath::Tag::None:
      return OffsetPathData::None();
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown offset-path");
      return OffsetPathData::None();
  }
}

/* static*/
Maybe<ResolvedMotionPathData> MotionPathUtils::ResolveMotionPath(
    const nsIFrame* aFrame, TransformReferenceBox& aRefBox) {
  MOZ_ASSERT(aFrame);

  const nsStyleDisplay* display = aFrame->StyleDisplay();

  // FIXME: It's possible to refactor the calculation of transform-origin, so we
  // could calculate from the caller, and reuse the value in nsDisplayList.cpp.
  CSSPoint transformOrigin = nsStyleTransformMatrix::Convert2DPosition(
      display->mTransformOrigin.horizontal, display->mTransformOrigin.vertical,
      aRefBox);

  return ResolveMotionPath(GenerateOffsetPathData(aFrame),
                           display->mOffsetDistance, display->mOffsetRotate,
                           display->mOffsetAnchor, transformOrigin, aRefBox,
                           ComputeAnchorPointAdjustment(*aFrame));
}

static OffsetPathData GenerateOffsetPathData(
    const StyleOffsetPath& aPath, const RayReferenceData& aRayReferenceData,
    gfx::Path* aCachedMotionPath) {
  switch (aPath.tag) {
    case StyleOffsetPath::Tag::Path: {
      const StyleSVGPathData& pathData = aPath.AsPath();
      // If aCachedMotionPath is valid, we have a fixed path.
      // This means we have pre-built it already and no need to update.
      RefPtr<gfx::Path> path = aCachedMotionPath;
      if (!path) {
        RefPtr<gfx::PathBuilder> builder =
            MotionPathUtils::GetCompositorPathBuilder();
        path = MotionPathUtils::BuildPath(pathData, builder);
      }
      return OffsetPathData::Path(pathData, path.forget());
    }
    case StyleOffsetPath::Tag::Ray:
      return OffsetPathData::Ray(aPath.AsRay(), aRayReferenceData);
    case StyleOffsetPath::Tag::None:
    default:
      return OffsetPathData::None();
  }
}

/* static */
Maybe<ResolvedMotionPathData> MotionPathUtils::ResolveMotionPath(
    const StyleOffsetPath* aPath, const StyleLengthPercentage* aDistance,
    const StyleOffsetRotate* aRotate, const StylePositionOrAuto* aAnchor,
    const Maybe<layers::MotionPathData>& aMotionPathData,
    TransformReferenceBox& aRefBox, gfx::Path* aCachedMotionPath) {
  if (!aPath) {
    return Nothing();
  }

  MOZ_ASSERT(aMotionPathData);

  auto zeroOffsetDistance = LengthPercentage::Zero();
  auto autoOffsetRotate = StyleOffsetRotate{true, StyleAngle::Zero()};
  auto autoOffsetAnchor = StylePositionOrAuto::Auto();
  return ResolveMotionPath(
      GenerateOffsetPathData(*aPath, aMotionPathData->rayReferenceData(),
                             aCachedMotionPath),
      aDistance ? *aDistance : zeroOffsetDistance,
      aRotate ? *aRotate : autoOffsetRotate,
      aAnchor ? *aAnchor : autoOffsetAnchor, aMotionPathData->origin(), aRefBox,
      aMotionPathData->anchorAdjustment());
}

/* static */
StyleSVGPathData MotionPathUtils::NormalizeSVGPathData(
    const StyleSVGPathData& aPath) {
  StyleSVGPathData n;
  Servo_SVGPathData_Normalize(&aPath, &n);
  return n;
}

/* static */
already_AddRefed<gfx::Path> MotionPathUtils::BuildPath(
    const StyleSVGPathData& aPath, gfx::PathBuilder* aPathBuilder) {
  if (!aPathBuilder) {
    return nullptr;
  }

  const Span<const StylePathCommand>& path = aPath._0.AsSpan();
  return SVGPathData::BuildPath(path, aPathBuilder, StyleStrokeLinecap::Butt,
                                0.0);
}

/* static */
already_AddRefed<gfx::PathBuilder> MotionPathUtils::GetCompositorPathBuilder() {
  // FIXME: Perhaps we need a PathBuilder which is independent on the backend.
  RefPtr<gfx::PathBuilder> builder =
      gfxPlatform::Initialized()
          ? gfxPlatform::GetPlatform()
                ->ScreenReferenceDrawTarget()
                ->CreatePathBuilder(gfx::FillRule::FILL_WINDING)
          : gfx::Factory::CreateSimplePathBuilder();
  return builder.forget();
}

}  // namespace mozilla
