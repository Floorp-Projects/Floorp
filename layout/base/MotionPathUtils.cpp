/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MotionPathUtils.h"

#include "gfxPlatform.h"
#include "mozilla/dom/SVGPathData.h"
#include "mozilla/dom/SVGViewportElement.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/layers/LayersMessages.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ShapeUtils.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsStyleTransformMatrix.h"

#include <math.h>

namespace mozilla {

using nsStyleTransformMatrix::TransformReferenceBox;

// In CSS context, this returns the the box being referenced from the element
// that establishes the containing block for this element.
// In SVG context, we always use view-box.
// https://drafts.fxtf.org/motion-1/#valdef-offset-path-coord-box
static const nsIFrame* GetOffsetPathReferenceBox(const nsIFrame* aFrame,
                                                 nsRect& aOutputRect) {
  const StyleOffsetPath& offsetPath = aFrame->StyleDisplay()->mOffsetPath;
  if (offsetPath.IsNone()) {
    return nullptr;
  }

  if (aFrame->HasAnyStateBits(NS_FRAME_SVG_LAYOUT)) {
    MOZ_ASSERT(aFrame->GetContent()->IsSVGElement());
    auto* viewportElement =
        dom::SVGElement::FromNode(aFrame->GetContent())->GetCtx();
    aOutputRect = nsLayoutUtils::ComputeSVGViewBox(viewportElement);
    return viewportElement ? viewportElement->GetPrimaryFrame() : nullptr;
  }

  const nsIFrame* containingBlock = aFrame->GetContainingBlock();
  const StyleCoordBox coordBox = offsetPath.IsCoordBox()
                                     ? offsetPath.AsCoordBox()
                                     : offsetPath.AsOffsetPath().coord_box;
  aOutputRect = nsLayoutUtils::ComputeHTMLReferenceRect(
      containingBlock, nsLayoutUtils::CoordBoxToGeometryBox(coordBox));
  return containingBlock;
}

RayReferenceData::RayReferenceData(const nsIFrame* aFrame) {
  nsRect coordBox;
  const nsIFrame* containingBlock = GetOffsetPathReferenceBox(aFrame, coordBox);
  if (!containingBlock) {
    // If there is no parent frame, it's impossible to calculate the path
    // length, so does the path.
    return;
  }

  mContainingBlockRect = CSSRect::FromAppUnits(coordBox);

  // The current position of |aFrame| in the coordinate system of
  // |containingBlock|.
  mInitialPosition =
      CSSPoint::FromAppUnits(aFrame->GetOffsetTo(containingBlock));

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

// The distance is measured between the origin and the intersection of the ray
// with the reference box of the containing block.
// Note: |aOrigin| and |aContaingBlock| should be in the same coordinate system
// (i.e. the nsIFrame::mRect of the containing block).
// https://drafts.fxtf.org/motion-1/#size-sides
static CSSCoord ComputeSides(const CSSPoint& aOrigin,
                             const CSSRect& aContainingBlock,
                             const StyleAngle& aAngle) {
  const CSSPoint& topLeft = aContainingBlock.TopLeft();
  // Given an acute angle |theta| (i.e. |t|) of a right-angled triangle, the
  // hypotenuse |h| is the side that connects the two acute angles. The side
  // |b| adjacent to |theta| is the side of the triangle that connects |theta|
  // to the right angle.
  //
  // e.g. if the angle |t| is 0 ~ 90 degrees, and b * tan(theta) <= b',
  //      h = b / cos(t):
  //                       b*tan(t)
  //    (topLeft) #--------*-----*--# (aContainingBlock.XMost(), topLeft.y)
  //              |        |    /   |
  //              |        |   /    |
  //              |        b  h     |
  //              |        |t/      |
  //              |        |/       |
  //             (aOrigin) *---b'---* (aContainingBlock.XMost(), aOrigin.y)
  //              |        |        |
  //              |        |        |
  //              |        |        |
  //              |        |        |
  //              |        |        |
  //              #-----------------# (aContainingBlock.XMost(),
  //        (topLeft.x,                aContainingBlock.YMost())
  //         aContainingBlock.YMost())
  const double theta = aAngle.ToRadians();
  double sint = std::sin(theta);
  double cost = std::cos(theta);

  const double b = cost >= 0 ? aOrigin.y.value - topLeft.y
                             : aContainingBlock.YMost() - aOrigin.y.value;
  const double bPrime = sint >= 0 ? aContainingBlock.XMost() - aOrigin.x.value
                                  : aOrigin.x.value - topLeft.x;
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

// Compute the origin, where the ray’s line begins (the 0% position).
// https://drafts.fxtf.org/motion-1/#ray-origin
static CSSPoint ComputeRayOrigin(const StylePositionOrAuto& aAtPosition,
                                 const StyleOffsetPosition& aOffsetPosition,
                                 const RayReferenceData& aData) {
  const nsRect& coordBox = CSSPixel::ToAppUnits(aData.mContainingBlockRect);

  if (aAtPosition.IsPosition()) {
    // Resolve this by using the <position> to position a 0x0 object area within
    // the box’s containing block.
    return CSSPoint::FromAppUnits(
        ShapeUtils::ComputePosition(aAtPosition.AsPosition(), coordBox));
  }

  MOZ_ASSERT(aAtPosition.IsAuto(), "\"at <position>\" should be omitted");

  // Use the offset starting position of the element, given by offset-position.
  // https://drafts.fxtf.org/motion-1/#valdef-ray-at-position
  if (aOffsetPosition.IsPosition()) {
    return CSSPoint::FromAppUnits(
        ShapeUtils::ComputePosition(aOffsetPosition.AsPosition(), coordBox));
  }

  if (aOffsetPosition.IsNormal()) {
    // If the element doesn’t have an offset starting position either, it
    // behaves as at center.
    static const StylePosition center = StylePosition::FromPercentage(0.5);
    return CSSPoint::FromAppUnits(
        ShapeUtils::ComputePosition(center, coordBox));
  }

  MOZ_ASSERT(aOffsetPosition.IsAuto());
  return aData.mInitialPosition;
}

static CSSCoord ComputeRayPathLength(const StyleRaySize aRaySizeType,
                                     const StyleAngle& aAngle,
                                     const CSSPoint& aOrigin,
                                     const CSSRect& aContainingBlock) {
  if (aRaySizeType == StyleRaySize::Sides) {
    // If the initial position is not within the box, the distance is 0.
    if (!aContainingBlock.Contains(aOrigin)) {
      return 0.0;
    }

    return ComputeSides(aOrigin, aContainingBlock, aAngle);
  }

  // left: the length between the origin and the left side.
  // right: the length between the origin and the right side.
  // top: the length between the origin and the top side.
  // bottom: the lenght between the origin and the bottom side.
  const CSSPoint& topLeft = aContainingBlock.TopLeft();
  const CSSCoord left = std::abs(aOrigin.x - topLeft.x);
  const CSSCoord right = std::abs(aContainingBlock.XMost() - aOrigin.x);
  const CSSCoord top = std::abs(aOrigin.y - topLeft.y);
  const CSSCoord bottom = std::abs(aContainingBlock.YMost() - aOrigin.y);

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
    case StyleRaySize::Sides:
      MOZ_ASSERT_UNREACHABLE("Unsupported ray size");
  }

  return 0.0;
}

static CSSCoord ComputeRayUsedDistance(const StyleRayFunction& aRay,
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
    const StyleOffsetPosition& aPosition, const CSSPoint& aTransformOrigin,
    TransformReferenceBox& aRefBox, const CSSPoint& aAnchorPointAdjustment) {
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

    const CSSPoint origin =
        ComputeRayOrigin(ray.mRay->position, aPosition, ray.mData);
    const CSSCoord pathLength =
        ComputeRayPathLength(ray.mRay->size, ray.mRay->angle, origin,
                             ray.mData.mContainingBlockRect);
    const CSSCoord usedDistance = ComputeRayUsedDistance(
        *ray.mRay, aDistance, pathLength, ray.mData.mBorderBoxSize);

    // 0deg pointing up and positive angles representing clockwise rotation.
    directionAngle =
        StyleAngle{ray.mRay->angle.ToDegrees() - 90.0f}.ToRadians();

    // The vector from the current position of this box to the origin of this
    // polar coordinate system.
    const gfx::Point vectorToOrigin =
        (origin - ray.mData.mInitialPosition).ToUnknownPoint();
    // |vectorToOrigin| + The vector from the origin to this polar coordinate,
    // (|usedDistance|, |directionAngle|), i.e. the vector from the current
    // position to this polar coordinate.
    point =
        vectorToOrigin +
        gfx::Point(usedDistance * static_cast<gfx::Float>(cos(directionAngle)),
                   usedDistance * static_cast<gfx::Float>(sin(directionAngle)));
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
  const StyleOffsetPath& offsetPath = aFrame->StyleDisplay()->mOffsetPath;
  if (offsetPath.IsNone()) {
    return OffsetPathData::None();
  }

  // FIXME: Bug 1598156. Handle IsCoordBox().
  if (offsetPath.IsCoordBox()) {
    return OffsetPathData::None();
  }

  const auto& function = offsetPath.AsOffsetPath().path;
  if (function->IsRay()) {
    return OffsetPathData::Ray(function->AsRay(), RayReferenceData(aFrame));
  }

  MOZ_ASSERT(function->IsShape());
  const StyleBasicShape& shape = function->AsShape();
  if (shape.IsPath()) {
    const StyleSVGPathData& pathData = shape.AsPath().path;
    RefPtr<gfx::Path> gfxPath =
        aFrame->GetProperty(nsIFrame::OffsetPathCache());
    MOZ_ASSERT(gfxPath || pathData._0.IsEmpty(),
               "Should have a valid cached gfx::Path or an empty path string");
    return OffsetPathData::Path(pathData, gfxPath.forget());
  }

  // FIXME: Bug 1598156. Handle other basic shapes.
  return OffsetPathData::None();
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

  return ResolveMotionPath(
      GenerateOffsetPathData(aFrame), display->mOffsetDistance,
      display->mOffsetRotate, display->mOffsetAnchor, display->mOffsetPosition,
      transformOrigin, aRefBox, ComputeAnchorPointAdjustment(*aFrame));
}

static OffsetPathData GenerateOffsetPathData(
    const StyleOffsetPath& aOffsetPath,
    const RayReferenceData& aRayReferenceData, gfx::Path* aCachedMotionPath) {
  if (aOffsetPath.IsNone()) {
    return OffsetPathData::None();
  }

  // FIXME: Bug 1598156. Handle IsCoordBox().
  if (aOffsetPath.IsCoordBox()) {
    return OffsetPathData::None();
  }

  const auto& function = aOffsetPath.AsOffsetPath().path;
  if (function->IsRay()) {
    return OffsetPathData::Ray(function->AsRay(), aRayReferenceData);
  }

  MOZ_ASSERT(function->IsShape());
  const StyleBasicShape& shape = function->AsShape();
  if (shape.IsPath()) {
    const StyleSVGPathData& pathData = shape.AsPath().path;
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

  // FIXME: Bug 1598156. Handle other basic shapes.
  return OffsetPathData::None();
}

/* static */
Maybe<ResolvedMotionPathData> MotionPathUtils::ResolveMotionPath(
    const StyleOffsetPath* aPath, const StyleLengthPercentage* aDistance,
    const StyleOffsetRotate* aRotate, const StylePositionOrAuto* aAnchor,
    const StyleOffsetPosition* aPosition,
    const Maybe<layers::MotionPathData>& aMotionPathData,
    TransformReferenceBox& aRefBox, gfx::Path* aCachedMotionPath) {
  if (!aPath) {
    return Nothing();
  }

  MOZ_ASSERT(aMotionPathData);

  auto zeroOffsetDistance = LengthPercentage::Zero();
  auto autoOffsetRotate = StyleOffsetRotate{true, StyleAngle::Zero()};
  auto autoOffsetAnchor = StylePositionOrAuto::Auto();
  auto autoOffsetPosition = StyleOffsetPosition::Auto();
  return ResolveMotionPath(
      GenerateOffsetPathData(*aPath, aMotionPathData->rayReferenceData(),
                             aCachedMotionPath),
      aDistance ? *aDistance : zeroOffsetDistance,
      aRotate ? *aRotate : autoOffsetRotate,
      aAnchor ? *aAnchor : autoOffsetAnchor,
      aPosition ? *aPosition : autoOffsetPosition, aMotionPathData->origin(),
      aRefBox, aMotionPathData->anchorAdjustment());
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
