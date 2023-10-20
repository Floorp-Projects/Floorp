/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MotionPathUtils.h"

#include "gfxPlatform.h"
#include "mozilla/dom/SVGGeometryElement.h"
#include "mozilla/dom/SVGPathData.h"
#include "mozilla/dom/SVGViewportElement.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/layers/LayersMessages.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SVGObserverUtils.h"
#include "mozilla/ShapeUtils.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsStyleTransformMatrix.h"

#include <math.h>

namespace mozilla {

using nsStyleTransformMatrix::TransformReferenceBox;

/* static */
CSSPoint MotionPathUtils::ComputeAnchorPointAdjustment(const nsIFrame& aFrame) {
  if (!aFrame.HasAnyStateBits(NS_FRAME_SVG_LAYOUT)) {
    return {};
  }

  auto transformBox = aFrame.StyleDisplay()->mTransformBox;
  if (transformBox == StyleTransformBox::ViewBox ||
      transformBox == StyleTransformBox::BorderBox) {
    return {};
  }

  if (aFrame.IsFrameOfType(nsIFrame::eSVGContainer)) {
    nsRect boxRect = nsLayoutUtils::ComputeSVGReferenceRect(
        const_cast<nsIFrame*>(&aFrame), StyleGeometryBox::FillBox);
    return CSSPoint::FromAppUnits(boxRect.TopLeft());
  }
  return CSSPoint::FromAppUnits(aFrame.GetPosition());
}

// Convert the StyleCoordBox into the StyleGeometryBox in CSS layout.
// https://drafts.csswg.org/css-box-4/#keywords
static StyleGeometryBox CoordBoxToGeometryBoxInCSSLayout(
    StyleCoordBox aCoordBox) {
  switch (aCoordBox) {
    case StyleCoordBox::ContentBox:
      return StyleGeometryBox::ContentBox;
    case StyleCoordBox::PaddingBox:
      return StyleGeometryBox::PaddingBox;
    case StyleCoordBox::BorderBox:
      return StyleGeometryBox::BorderBox;
    case StyleCoordBox::FillBox:
      return StyleGeometryBox::ContentBox;
    case StyleCoordBox::StrokeBox:
    case StyleCoordBox::ViewBox:
      return StyleGeometryBox::BorderBox;
  }
  MOZ_ASSERT_UNREACHABLE("Unknown coord-box type");
  return StyleGeometryBox::BorderBox;
}

/* static */
const nsIFrame* MotionPathUtils::GetOffsetPathReferenceBox(
    const nsIFrame* aFrame, nsRect& aOutputRect) {
  const StyleOffsetPath& offsetPath = aFrame->StyleDisplay()->mOffsetPath;
  if (offsetPath.IsNone()) {
    return nullptr;
  }

  if (aFrame->HasAnyStateBits(NS_FRAME_SVG_LAYOUT)) {
    MOZ_ASSERT(aFrame->GetContent()->IsSVGElement());
    auto* viewportElement =
        dom::SVGElement::FromNode(aFrame->GetContent())->GetCtx();
    aOutputRect = nsLayoutUtils::ComputeSVGOriginBox(viewportElement);
    return viewportElement ? viewportElement->GetPrimaryFrame() : nullptr;
  }

  const nsIFrame* containingBlock = aFrame->GetContainingBlock();
  const StyleCoordBox coordBox = offsetPath.IsCoordBox()
                                     ? offsetPath.AsCoordBox()
                                     : offsetPath.AsOffsetPath().coord_box;
  aOutputRect = nsLayoutUtils::ComputeHTMLReferenceRect(
      containingBlock, CoordBoxToGeometryBoxInCSSLayout(coordBox));
  return containingBlock;
}

/* static */
CSSCoord MotionPathUtils::GetRayContainReferenceSize(nsIFrame* aFrame) {
  // We use the border-box size to calculate the reduced path length when using
  // "contain" keyword.
  // https://drafts.fxtf.org/motion-1/#valdef-ray-contain
  //
  // Note: Per the spec, border-box is treated as stroke-box in the SVG context,
  // https://drafts.csswg.org/css-box-4/#valdef-box-border-box
  const auto size =
      CSSSize::FromAppUnits((aFrame->HasAnyStateBits(NS_FRAME_SVG_LAYOUT)
                                 ? nsLayoutUtils::ComputeSVGReferenceRect(
                                       aFrame, StyleGeometryBox::StrokeBox)
                                 : nsLayoutUtils::ComputeHTMLReferenceRect(
                                       aFrame, StyleGeometryBox::BorderBox))
                                .Size());
  return std::max(size.width, size.height);
}

/* static */
nsTArray<nscoord> MotionPathUtils::ComputeBorderRadii(
    const StyleBorderRadius& aBorderRadius, const nsRect& aCoordBox) {
  const nsRect insetRect = ShapeUtils::ComputeInsetRect(
      StyleRect<LengthPercentage>::WithAllSides(LengthPercentage::Zero()),
      aCoordBox);
  nsTArray<nscoord> result(8);
  result.SetLength(8);
  if (!ShapeUtils::ComputeRectRadii(aBorderRadius, aCoordBox, insetRect,
                                    result.Elements())) {
    result.Clear();
  }
  return result;
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

  // The trigonometric formula here doesn't work well if |theta| is 0deg or
  // 90deg, so we handle these edge cases first.
  if (sint < std::numeric_limits<double>::epsilon()) {
    // For 0deg (or 180deg), we use |b| directly.
    return static_cast<float>(b);
  }

  if (cost < std::numeric_limits<double>::epsilon()) {
    // For 90deg (or 270deg), we use |bPrime| directly. This can also avoid 0/0
    // if both |b| and |cost| are 0.0. (i.e. b / cost).
    return static_cast<float>(bPrime);
  }

  // Note: The following formula works well only when 0 < theta < 90deg. So we
  // handle 0deg and 90deg above first.
  //
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

// Compute the position of "at <position>" together with offset starting
// position (i.e. offset-position).
static nsPoint ComputePosition(const StylePositionOrAuto& aAtPosition,
                               const StyleOffsetPosition& aOffsetPosition,
                               const nsRect& aCoordBox,
                               const nsPoint& aCurrentCoord) {
  if (aAtPosition.IsPosition()) {
    // Resolve this by using the <position> to position a 0x0 object area within
    // the box’s containing block.
    return ShapeUtils::ComputePosition(aAtPosition.AsPosition(), aCoordBox);
  }

  MOZ_ASSERT(aAtPosition.IsAuto(), "\"at <position>\" should be omitted");

  // Use the offset starting position of the element, given by offset-position.
  // https://drafts.fxtf.org/motion-1/#valdef-ray-at-position
  if (aOffsetPosition.IsPosition()) {
    return ShapeUtils::ComputePosition(aOffsetPosition.AsPosition(), aCoordBox);
  }

  if (aOffsetPosition.IsNormal()) {
    // If the element doesn’t have an offset starting position either, it
    // behaves as at center.
    const StylePosition& center = StylePosition::FromPercentage(0.5);
    return ShapeUtils::ComputePosition(center, aCoordBox);
  }

  MOZ_ASSERT(aOffsetPosition.IsAuto());
  return aCurrentCoord;
}

static CSSCoord ComputeRayPathLength(const StyleRaySize aRaySizeType,
                                     const StyleAngle& aAngle,
                                     const CSSPoint& aOrigin,
                                     const CSSRect& aContainingBlock) {
  if (aRaySizeType == StyleRaySize::Sides) {
    // If the initial position is not within the box, the distance is 0.
    //
    // Note: If the origin is at XMost() (and/or YMost()), we should consider it
    // to be inside containing block (because we expect 100% x (or y) coordinate
    // is still to be considered inside the containing block.
    if (!aContainingBlock.ContainsInclusively(aOrigin)) {
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

static CSSCoord ComputeRayUsedDistance(
    const StyleRayFunction& aRay, const LengthPercentage& aDistance,
    const CSSCoord& aPathLength, const CSSCoord& aRayContainReferenceLength) {
  CSSCoord usedDistance = aDistance.ResolveToCSSPixels(aPathLength);
  if (!aRay.contain) {
    return usedDistance;
  }

  // The length of the offset path is reduced so that the element stays within
  // the containing block even at offset-distance: 100%. Specifically, the
  // path’s length is reduced by half the width or half the height of the
  // element’s border box, whichever is larger, and floored at zero.
  // https://drafts.fxtf.org/motion-1/#valdef-ray-contain
  return std::max((usedDistance - aRayContainReferenceLength / 2.0f).value,
                  0.0f);
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
  if (aPath.IsShape()) {
    const auto& data = aPath.AsShape();
    RefPtr<gfx::Path> path = data.mGfxPath;
    MOZ_ASSERT(path, "The empty path is not allowed");

    // Per the spec, we have to convert offset distance to pixels, with 100%
    // being converted to total length. So here |gfxPath| is built with CSS
    // pixel, and we calculate |pathLength| and |computedDistance| with CSS
    // pixel as well.
    gfx::Float pathLength = path->ComputeLength();
    gfx::Float usedDistance =
        aDistance.ResolveToCSSPixels(CSSCoord(pathLength));
    if (data.mIsClosedLoop) {
      // Per the spec, let used offset distance be equal to offset distance
      // modulus the total length of the path. If the total length of the path
      // is 0, used offset distance is also 0.
      usedDistance = pathLength > 0.0 ? fmod(usedDistance, pathLength) : 0.0;
      // We make sure |usedDistance| is 0.0 or a positive value.
      if (usedDistance < 0.0) {
        usedDistance += pathLength;
      }
    } else {
      // Per the spec, for unclosed interval, let used offset distance be equal
      // to offset distance clamped by 0 and the total length of the path.
      usedDistance = clamped(usedDistance, 0.0f, pathLength);
    }
    gfx::Point tangent;
    point = path->ComputePointAtLength(usedDistance, &tangent);
    // Basically, |point| should be a relative distance between the current
    // position and the target position. The built |path| is in the coordinate
    // system of its containing block. Therefore, we have to take the current
    // position of this box into account to offset the translation so it's final
    // position is not affected by other boxes in the same containing block.
    point -= NSPointToPoint(data.mCurrentPosition, AppUnitsPerCSSPixel());
    directionAngle = atan2((double)tangent.y, (double)tangent.x);  // in Radian.
  } else if (aPath.IsRay()) {
    const auto& ray = aPath.AsRay();
    MOZ_ASSERT(ray.mRay);

    // Compute the origin, where the ray’s line begins (the 0% position).
    // https://drafts.fxtf.org/motion-1/#ray-origin
    const CSSPoint origin = CSSPoint::FromAppUnits(ComputePosition(
        ray.mRay->position, aPosition, ray.mCoordBox, ray.mCurrentPosition));
    const CSSCoord pathLength =
        ComputeRayPathLength(ray.mRay->size, ray.mRay->angle, origin,
                             CSSRect::FromAppUnits(ray.mCoordBox));
    const CSSCoord usedDistance = ComputeRayUsedDistance(
        *ray.mRay, aDistance, pathLength, ray.mContainReferenceLength);

    // 0deg pointing up and positive angles representing clockwise rotation.
    directionAngle =
        StyleAngle{ray.mRay->angle.ToDegrees() - 90.0f}.ToRadians();

    // The vector from the current position of this box to the origin of this
    // polar coordinate system.
    const gfx::Point vectorToOrigin =
        (origin - CSSPoint::FromAppUnits(ray.mCurrentPosition))
            .ToUnknownPoint();
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

static inline bool IsClosedLoop(const StyleSVGPathData& aPathData) {
  return !aPathData._0.AsSpan().empty() &&
         aPathData._0.AsSpan().rbegin()->IsClosePath();
}

// Create a path for "inset(0 round X)", where X is the value of border-radius
// on the element that establishes the containing block for this element.
static already_AddRefed<gfx::Path> BuildSimpleInsetPath(
    const StyleBorderRadius& aBorderRadius, const nsRect& aCoordBox,
    gfx::PathBuilder* aPathBuilder) {
  if (!aPathBuilder) {
    return nullptr;
  }

  const nsRect insetRect = ShapeUtils::ComputeInsetRect(
      StyleRect<LengthPercentage>::WithAllSides(LengthPercentage::Zero()),
      aCoordBox);
  nscoord radii[8];
  const bool hasRadii =
      ShapeUtils::ComputeRectRadii(aBorderRadius, aCoordBox, insetRect, radii);
  return ShapeUtils::BuildRectPath(insetRect, hasRadii ? radii : nullptr,
                                   aCoordBox, AppUnitsPerCSSPixel(),
                                   aPathBuilder);
}

// Create a path for `path("m 0 0")`, which is the default URL path if we cannot
// resolve a SVG shape element.
// https://drafts.fxtf.org/motion-1/#valdef-offset-path-url
static already_AddRefed<gfx::Path> BuildDefaultPathForURL(
    gfx::PathBuilder* aBuilder) {
  if (!aBuilder) {
    return nullptr;
  }

  Array<const StylePathCommand, 1> array(StylePathCommand::MoveTo(
      StyleCoordPair(gfx::Point{0.0, 0.0}), StyleIsAbsolute::No));
  return SVGPathData::BuildPath(array, aBuilder, StyleStrokeLinecap::Butt, 0.0);
}

// Generate data for motion path on the main thread.
static OffsetPathData GenerateOffsetPathData(const nsIFrame* aFrame) {
  const StyleOffsetPath& offsetPath = aFrame->StyleDisplay()->mOffsetPath;
  if (offsetPath.IsNone()) {
    return OffsetPathData::None();
  }

  // Handle ray().
  if (offsetPath.IsRay()) {
    nsRect coordBox;
    const nsIFrame* containingBlockFrame =
        MotionPathUtils::GetOffsetPathReferenceBox(aFrame, coordBox);
    return !containingBlockFrame
               ? OffsetPathData::None()
               : OffsetPathData::Ray(
                     offsetPath.AsRay(), std::move(coordBox),
                     aFrame->GetOffsetTo(containingBlockFrame),
                     MotionPathUtils::GetRayContainReferenceSize(
                         const_cast<nsIFrame*>(aFrame)));
  }

  // Handle path(). We cache it so we handle it separately.
  // FIXME: Bug 1837042, cache gfx::Path for shapes other than path(). Once we
  // cache all basic shapes, we can merge this branch into other basic shapes.
  if (offsetPath.IsPath()) {
    const StyleSVGPathData& pathData = offsetPath.AsSVGPathData();
    RefPtr<gfx::Path> gfxPath =
        aFrame->GetProperty(nsIFrame::OffsetPathCache());
    MOZ_ASSERT(gfxPath || pathData._0.IsEmpty(),
               "Should have a valid cached gfx::Path or an empty path string");
    // FIXME: Bug 1836847. Once we support "at <position>" for path(), we have
    // to give it the current box position.
    return OffsetPathData::Shape(gfxPath.forget(), {}, IsClosedLoop(pathData));
  }

  nsRect coordBox;
  const nsIFrame* containingFrame =
      MotionPathUtils::GetOffsetPathReferenceBox(aFrame, coordBox);
  if (!containingFrame || coordBox.IsEmpty()) {
    return OffsetPathData::None();
  }
  nsPoint currentPosition = aFrame->GetOffsetTo(containingFrame);
  RefPtr<gfx::PathBuilder> builder = MotionPathUtils::GetPathBuilder();

  if (offsetPath.IsUrl()) {
    dom::SVGGeometryElement* element =
        SVGObserverUtils::GetAndObserveGeometry(const_cast<nsIFrame*>(aFrame));
    if (!element) {
      // Note: This behaves as path("m 0 0") (a <basic-shape>).
      RefPtr<gfx::Path> path = BuildDefaultPathForURL(builder);
      // FIXME: Bug 1836847. Once we support "at <position>" for path(), we have
      // to give it the current box position.
      return path ? OffsetPathData::Shape(path.forget(), {}, false)
                  : OffsetPathData::None();
    }

    // We just need this path to calculate the specific point and direction
    // angle, so use measuring function and get the benefit of caching the path
    // in the SVG shape element.
    RefPtr<gfx::Path> path = element->GetOrBuildPathForMeasuring();

    // The built |path| from SVG shape element doesn't take |coordBox| into
    // account. It uses the SVG viewport as its coordinate system. So after
    // mapping it into the CSS layout, we should use |coordBox| as its viewport
    // and user coordinate system. |currentPosition| is based on the border-box
    // of the containing block. Therefore, we have to apply an extra translation
    // to put it at the correct position based on |coordBox|.
    //
    // Note: we reuse |OffsetPathData::ShapeData::mCurrentPosition| to include
    // this extra translation, so we don't have to add an extra field.
    nsPoint positionInCoordBox = currentPosition - coordBox.TopLeft();
    return path ? OffsetPathData::Shape(path.forget(),
                                        std::move(positionInCoordBox),
                                        element->IsClosedLoop())
                : OffsetPathData::None();
  }

  // The rest part is to handle "<basic-shape> || <coord-box>".
  MOZ_ASSERT(offsetPath.IsBasicShapeOrCoordBox());

  const nsStyleDisplay* disp = aFrame->StyleDisplay();
  RefPtr<gfx::Path> path =
      disp->mOffsetPath.IsCoordBox()
          ? BuildSimpleInsetPath(containingFrame->StyleBorder()->mBorderRadius,
                                 coordBox, builder)
          : MotionPathUtils::BuildPath(
                disp->mOffsetPath.AsOffsetPath().path->AsShape(),
                disp->mOffsetPosition, coordBox, currentPosition, builder);
  return path ? OffsetPathData::Shape(path.forget(), std::move(currentPosition),
                                      true)
              : OffsetPathData::None();
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

// Generate data for motion path on the compositor thread.
static OffsetPathData GenerateOffsetPathData(
    const StyleOffsetPath& aOffsetPath,
    const StyleOffsetPosition& aOffsetPosition,
    const layers::MotionPathData& aMotionPathData,
    gfx::Path* aCachedMotionPath) {
  if (aOffsetPath.IsNone()) {
    return OffsetPathData::None();
  }

  // Handle ray().
  if (aOffsetPath.IsRay()) {
    return aMotionPathData.coordBox().IsEmpty()
               ? OffsetPathData::None()
               : OffsetPathData::Ray(
                     aOffsetPath.AsRay(), aMotionPathData.coordBox(),
                     aMotionPathData.currentPosition(),
                     aMotionPathData.rayContainReferenceLength());
  }

  // Handle path().
  // FIXME: Bug 1837042, cache gfx::Path for shapes other than path().
  if (aOffsetPath.IsPath()) {
    const StyleSVGPathData& pathData = aOffsetPath.AsSVGPathData();
    // If aCachedMotionPath is valid, we have a fixed path.
    // This means we have pre-built it already and no need to update.
    RefPtr<gfx::Path> path = aCachedMotionPath;
    if (!path) {
      RefPtr<gfx::PathBuilder> builder =
          MotionPathUtils::GetCompositorPathBuilder();
      path = MotionPathUtils::BuildSVGPath(pathData, builder);
    }
    // FIXME: Bug 1836847. Once we support "at <position>" for path(), we have
    // to give it the current box position.
    return OffsetPathData::Shape(path.forget(), {}, IsClosedLoop(pathData));
  }

  // The rest part is to handle "<basic-shape> || <coord-box>".
  MOZ_ASSERT(aOffsetPath.IsBasicShapeOrCoordBox());

  const nsRect& coordBox = aMotionPathData.coordBox();
  if (coordBox.IsEmpty()) {
    return OffsetPathData::None();
  }

  RefPtr<gfx::PathBuilder> builder =
      MotionPathUtils::GetCompositorPathBuilder();
  if (!builder) {
    return OffsetPathData::None();
  }

  RefPtr<gfx::Path> path;
  if (aOffsetPath.IsCoordBox()) {
    const nsRect insetRect = ShapeUtils::ComputeInsetRect(
        StyleRect<LengthPercentage>::WithAllSides(LengthPercentage::Zero()),
        coordBox);
    const nsTArray<nscoord>& radii = aMotionPathData.coordBoxInsetRadii();
    path = ShapeUtils::BuildRectPath(
        insetRect, radii.IsEmpty() ? nullptr : radii.Elements(), coordBox,
        AppUnitsPerCSSPixel(), builder);
  } else {
    path = MotionPathUtils::BuildPath(
        aOffsetPath.AsOffsetPath().path->AsShape(), aOffsetPosition, coordBox,
        aMotionPathData.currentPosition(), builder);
  }

  return path ? OffsetPathData::Shape(
                    path.forget(), nsPoint(aMotionPathData.currentPosition()),
                    true)
              : OffsetPathData::None();
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
      GenerateOffsetPathData(*aPath,
                             aPosition ? *aPosition : autoOffsetPosition,
                             *aMotionPathData, aCachedMotionPath),
      aDistance ? *aDistance : zeroOffsetDistance,
      aRotate ? *aRotate : autoOffsetRotate,
      aAnchor ? *aAnchor : autoOffsetAnchor,
      aPosition ? *aPosition : autoOffsetPosition, aMotionPathData->origin(),
      aRefBox, aMotionPathData->anchorAdjustment());
}

/* static */
already_AddRefed<gfx::Path> MotionPathUtils::BuildSVGPath(
    const StyleSVGPathData& aPath, gfx::PathBuilder* aPathBuilder) {
  if (!aPathBuilder) {
    return nullptr;
  }

  const Span<const StylePathCommand>& path = aPath._0.AsSpan();
  return SVGPathData::BuildPath(path, aPathBuilder, StyleStrokeLinecap::Butt,
                                0.0);
}

/* static */
already_AddRefed<gfx::Path> MotionPathUtils::BuildPath(
    const StyleBasicShape& aBasicShape,
    const StyleOffsetPosition& aOffsetPosition, const nsRect& aCoordBox,
    const nsPoint& aCurrentPosition, gfx::PathBuilder* aPathBuilder) {
  if (!aPathBuilder) {
    return nullptr;
  }

  switch (aBasicShape.tag) {
    case StyleBasicShape::Tag::Circle: {
      const nsPoint center =
          ComputePosition(aBasicShape.AsCircle().position, aOffsetPosition,
                          aCoordBox, aCurrentPosition);
      return ShapeUtils::BuildCirclePath(aBasicShape, aCoordBox, center,
                                         AppUnitsPerCSSPixel(), aPathBuilder);
    }
    case StyleBasicShape::Tag::Ellipse: {
      const nsPoint center =
          ComputePosition(aBasicShape.AsEllipse().position, aOffsetPosition,
                          aCoordBox, aCurrentPosition);
      return ShapeUtils::BuildEllipsePath(aBasicShape, aCoordBox, center,
                                          AppUnitsPerCSSPixel(), aPathBuilder);
    }
    case StyleBasicShape::Tag::Rect:
      return ShapeUtils::BuildInsetPath(aBasicShape, aCoordBox,
                                        AppUnitsPerCSSPixel(), aPathBuilder);
    case StyleBasicShape::Tag::Polygon:
      return ShapeUtils::BuildPolygonPath(aBasicShape, aCoordBox,
                                          AppUnitsPerCSSPixel(), aPathBuilder);
    case StyleBasicShape::Tag::Path:
      // FIXME: Bug 1836847. Once we support "at <position>" for path(), we have
      // to also check its containing block as well. For now, we are still
      // building its gfx::Path directly by its SVGPathData without other
      // reference. https://github.com/w3c/fxtf-drafts/issues/504
      return BuildSVGPath(aBasicShape.AsPath().path, aPathBuilder);
  }

  return nullptr;
}

/* static */
already_AddRefed<gfx::PathBuilder> MotionPathUtils::GetPathBuilder() {
  // Here we only need to build a valid path for motion path, so
  // using the default values of stroke-width, stoke-linecap, and fill-rule
  // is fine for now because what we want is to get the point and its normal
  // vector along the path, instead of rendering it.
  RefPtr<gfx::PathBuilder> builder =
      gfxPlatform::GetPlatform()
          ->ScreenReferenceDrawTarget()
          ->CreatePathBuilder(gfx::FillRule::FILL_WINDING);
  return builder.forget();
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
