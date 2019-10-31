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
#include "mozilla/RefPtr.h"
#include "mozilla/ServoStyleConsts.h"
#include "nsIFrame.h"
#include "nsStyleTransformMatrix.h"
#include "Units.h"

#include <math.h>

namespace mozilla {

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

  double b = cost >= 0 ? aInitialPosition.y
                       : aContainerSize.height - aInitialPosition.y;
  double bPrime = sint >= 0 ? aContainerSize.width - aInitialPosition.x
                            : aInitialPosition.x;
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

static CSSCoord ComputeRayPathLength(const nsIFrame* aFrame,
                                     const StyleRaySize aRaySizeType,
                                     const StyleAngle& aAngle) {
  // We use GetContainingBlock() for now. TYLin said this function is buggy in
  // modern CSS layout, but is ok for most cases.
  // FIXME: Bug 1581237: This is still not clear that which box we should use
  // for calculating the path length. We may need to update this.
  // https://github.com/w3c/fxtf-drafts/issues/369
  // FIXME: Bug 1579294: SVG layout may get a |container| with empty mRect
  // (e.g. nsSVGOuterSVGAnonChildFrame), which makes the path length zero.
  const nsIFrame* container = aFrame->GetContainingBlock();
  if (!container) {
    // If there is no parent frame, it's impossible to calculate the path
    // length, so return 0.0.
    return 0.0;
  }

  // The initial position is (0, 0) in |aFrame|, and we have to transform it
  // into the space of |container|, so use GetOffsetsTo() to get the delta
  // value.
  // FIXME: Bug 1559232: The initial position will be adjusted after
  // supporting `offset-position`.
  const CSSPoint initialPos =
      CSSPixel::FromAppUnits(aFrame->GetOffsetTo(container));
  // FIXME: We need a better definition for containing box in the spec. For now,
  // we use border box for calculation.
  // https://github.com/w3c/fxtf-drafts/issues/369
  const CSSRect containerRect =
      CSSPixel::FromAppUnits(container->GetRectRelativeToSelf());
  if (aRaySizeType == StyleRaySize::Sides) {
    // If the initial position is not within the box, the distance is 0.
    if (!containerRect.Contains(initialPos)) {
      return 0.0;
    }

    return ComputeSides(initialPos, containerRect.Size(), aAngle);
  }

  // left: the length between the initial point and the left side.
  // right: the length between the initial point and the right side.
  // top: the length between the initial point and the top side.
  // bottom: the lenght between the initial point and the bottom side.
  CSSCoord left = std::abs(initialPos.x);
  CSSCoord right = std::abs(containerRect.width - initialPos.x);
  CSSCoord top = std::abs(initialPos.y);
  CSSCoord bottom = std::abs(containerRect.height - initialPos.y);

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

static void ApplyRotationAndMoveRayToXAxis(
    const StyleOffsetRotate& aOffsetRotate, const StyleAngle& aRayAngle,
    AutoTArray<gfx::Point, 4>& aVertices) {
  const StyleAngle directionAngle = aRayAngle - StyleAngle{90.0f};
  // Get the final rotation which includes the direction angle and
  // offset-rotate.
  const StyleAngle rotateAngle =
      (aOffsetRotate.auto_ ? directionAngle : StyleAngle{0.0f}) +
      aOffsetRotate.angle;
  // This is the rotation to rotate ray to positive x-axis (i.e. 90deg).
  const StyleAngle rayToXAxis = StyleAngle{90.0} - aRayAngle;

  gfx::Matrix m;
  m.PreRotate((rotateAngle + rayToXAxis).ToRadians());
  for (gfx::Point& p : aVertices) {
    p = m.TransformPoint(p);
  }
}

class RayPointComparator {
 public:
  bool Equals(const gfx::Point& a, const gfx::Point& b) const {
    return std::fabs(a.y) == std::fabs(b.y);
  }

  bool LessThan(const gfx::Point& a, const gfx::Point& b) const {
    return std::fabs(a.y) > std::fabs(b.y);
  }
};
// Note: the calculation of contain doesn't take other transform-like properties
// into account. The spec doesn't mention the co-operation for this, so for now,
// we assume we only need to take motion-path into account.
static CSSCoord ComputeRayUsedDistance(const nsStyleDisplay* aDisplay,
                                       const nsSize& aSize,
                                       const CSSCoord& aPathLength) {
  MOZ_ASSERT(aDisplay->mOffsetPath.IsRay());

  CSSCoord usedDistance =
      aDisplay->mOffsetDistance.ResolveToCSSPixels(aPathLength);
  if (!aDisplay->mOffsetPath.AsRay().contain) {
    return usedDistance;
  }

  // We have to simulate the 4 vertices to check if any of them is outside the
  // path circle. Here, we create a 2D Cartesian coordinate system and its
  // origin is at the anchor point of the box. And then apply the rotation on
  // these 4 vertices, calculate the range of |usedDistance| which makes the box
  // entirely contained within the path.
  // Note:
  // "Contained within the path" means the rectangle is inside a circle whose
  // radius is |aPathLength|.
  const StylePositionOrAuto& anchor = aDisplay->mOffsetAnchor;
  const StyleTransformOrigin& origin = aDisplay->mTransformOrigin;
  const StyleLengthPercentage& anchorX =
      anchor.IsAuto() ? origin.horizontal : anchor.AsPosition().horizontal;
  const StyleLengthPercentage& anchorY =
      anchor.IsAuto() ? origin.vertical : anchor.AsPosition().vertical;

  const CSSCoord width = CSSPixel::FromAppUnits(aSize.width);
  const CSSCoord height = CSSPixel::FromAppUnits(aSize.height);
  const CSSPoint usedAnchor = {anchorX.ResolveToCSSPixels(width),
                               anchorY.ResolveToCSSPixels(height)};
  AutoTArray<gfx::Point, 4> vertices = {
      {-usedAnchor.x, -usedAnchor.y},
      {width - usedAnchor.x, -usedAnchor.y},
      {width - usedAnchor.x, height - usedAnchor.y},
      {-usedAnchor.x, height - usedAnchor.y}};

  ApplyRotationAndMoveRayToXAxis(aDisplay->mOffsetRotate,
                                 aDisplay->mOffsetPath.AsRay().angle, vertices);

  // We have to check if all 4 vertices are inside the circle with radius |r|.
  // Assume the position of the vertex is (x, y), and the box is moved by
  // |usedDistance| along the path:
  //
  //       (usedDistance + x)^2 + y^2 <= r^2
  //   ==> (usedDistance + x)^2 <= r^2 - y^2 = d
  //   ==> -x - sqrt(d) <= used distance <= -x + sqrt(d)
  //
  // Note: |usedDistance| is added into |x| because we convert the ray function
  // to 90deg, x-axis):
  float upperMin = std::numeric_limits<float>::max();
  float lowerMax = std::numeric_limits<float>::min();
  bool shouldIncreasePathLength = false;
  for (const gfx::Point& p : vertices) {
    float d = aPathLength.value * aPathLength.value - p.y * p.y;
    if (d < 0) {
      // Impossible to make the box inside the path circle. Need to increase
      // the path length.
      shouldIncreasePathLength = true;
      break;
    }
    float sqrtD = sqrt(d);
    upperMin = std::min(upperMin, -p.x + sqrtD);
    lowerMax = std::max(lowerMax, -p.x - sqrtD);
  }

  if (!shouldIncreasePathLength) {
    return std::max(lowerMax, std::min(upperMin, (float)usedDistance));
  }

  // Sort by the absolute value of y, so the first vertex of the each pair of
  // vertices we check has a larger y value. (i.e. |yi| is always larger than or
  // equal to |yj|.)
  vertices.Sort(RayPointComparator());

  // Assume we set |usedDistance| to |-vertices[0].x|, so the current radius is
  // fabs(vertices[0].y). This is a possible solution.
  double radius = std::fabs(vertices[0].y);
  usedDistance = -vertices[0].x;
  const double epsilon = 1e-5;

  for (size_t i = 0; i < 3; ++i) {
    for (size_t j = i + 1; j < 4; ++j) {
      double xi = vertices[i].x;
      double yi = vertices[i].y;
      double xj = vertices[j].x;
      double yj = vertices[j].y;
      double dx = xi - xj;

      // Check if any path that enclosed vertices[i] would also enclose
      // vertices[j].
      //
      // For example, the initial setup:
      //                 * (0, yi)
      //                 |
      //                 r
      //                 |          * (xj - xi, yj)
      //           xi    |     dx
      // ----*-----------*----------*---
      // (anchor point)  | (0, 0)
      //
      // Assuming (0, yi) is on the path and (xj - xi, yj) is inside the path
      // circle, we should use the inequality to check this:
      //   (xj - xi)^2 + yj^2 <= yi^2
      //
      // After the first iterations, the updated inequality is:
      //       (dx + d)^2 + yj^2 <= yi^2 + d^2
      //   ==> dx^2 + 2dx*d + yj^2 <= yi^2
      //   ==> dx^2 + yj^2 <= yi^2 - 2dx*d <= yi^2
      // , |d| is the difference (or offset) between the old |usedDistance| and
      // new |usedDistance|.
      //
      // Note: `2dx * d` must be positive because
      // 1. if |xj| is larger than |xi|, only negative |d| could be used to get
      //    a new path length which encloses both vertices.
      // 2. if |xj| is smaller than |xi|, only positive |d| could be used to get
      //    a new path length which encloses both vertices.
      if (dx * dx + yj * yj <= yi * yi + epsilon) {
        continue;
      }

      // We have to find a new usedDistance which let both vertices[i] and
      // vertices[j] be on the path.
      //       (usedDistance + xi)^2 + yi^2 = (usedDistance + xj)^2 + yj^2
      //                                    = radius^2
      //   ==> usedDistance = (xj^2 + yj^2 - xi^2 - yi^2) / 2(xi-xj)
      //
      // Note: it's impossible to have a "divide by zero" problem here.
      // If |dx| is zero, the if-condition above should always be true and so
      // we skip the calculation.
      double newUsedDistance =
          (xj * xj + yj * yj - xi * xi - yi * yi) / dx / 2.0;
      // Then, move vertices[i] and vertices[j] by |newUsedDistance|.
      xi += newUsedDistance;  // or xj += newUsedDistance; if we use |xj| to get
                              // |newRadius|.
      double newRadius = sqrt(xi * xi + yi * yi);
      if (newRadius > radius) {
        // We have to increase the path length to make sure both vertices[i] and
        // vertices[j] are contained by this new path length.
        radius = newRadius;
        usedDistance = (float)newUsedDistance;
      }
    }
  }

  return usedDistance;
}

/* static */
Maybe<MotionPathData> MotionPathUtils::ResolveMotionPath(
    const nsIFrame* aFrame) {
  MOZ_ASSERT(aFrame);

  const nsStyleDisplay* display = aFrame->StyleDisplay();
  if (display->mOffsetPath.IsNone()) {
    return Nothing();
  }

  double directionAngle = 0.0;
  gfx::Point point;
  if (display->mOffsetPath.IsPath()) {
    const Span<const StylePathCommand>& path =
        display->mOffsetPath.AsPath()._0.AsSpan();
    // Build the path and compute the point and angle for creating the
    // equivalent translate and rotate.
    // Here we only need to build a valid path for motion path, so
    // using the default values of stroke-width, stoke-linecap, and fill-rule
    // is fine for now because what we want is get the point and its normal
    // vector along the path, instead of rendering it.
    // FIXME: Bug 1484780, we should cache the path to avoid rebuilding it here
    // at every restyle. (Caching the path avoids the cost of flattening it
    // again each time.)
    RefPtr<gfx::DrawTarget> drawTarget =
        gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();
    RefPtr<gfx::PathBuilder> builder =
        drawTarget->CreatePathBuilder(gfx::FillRule::FILL_WINDING);
    RefPtr<gfx::Path> gfxPath = SVGPathData::BuildPath(
        path, builder, NS_STYLE_STROKE_LINECAP_BUTT, 0.0);
    if (!gfxPath) {
      return Nothing();
    }

    // Per the spec, we have to convert offset distance to pixels, with 100%
    // being converted to total length. So here |gfxPath| is built with CSS
    // pixel, and we calculate |pathLength| and |computedDistance| with CSS
    // pixel as well.
    gfx::Float pathLength = gfxPath->ComputeLength();
    gfx::Float usedDistance =
        display->mOffsetDistance.ResolveToCSSPixels(CSSCoord(pathLength));
    if (!path.empty() && path.rbegin()->IsClosePath()) {
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
    point = gfxPath->ComputePointAtLength(usedDistance, &tangent);
    directionAngle = (double)atan2(tangent.y, tangent.x);  // In Radian.
  } else if (display->mOffsetPath.IsRay()) {
    const auto& ray = display->mOffsetPath.AsRay();
    CSSCoord pathLength = ComputeRayPathLength(aFrame, ray.size, ray.angle);
    CSSCoord usedDistance =
        ComputeRayUsedDistance(display, aFrame->GetSize(), pathLength);

    // 0deg pointing up and positive angles representing clockwise rotation.
    directionAngle = StyleAngle{ray.angle.ToDegrees() - 90.0f}.ToRadians();

    point.x = usedDistance * cos(directionAngle);
    point.y = usedDistance * sin(directionAngle);
  } else {
    MOZ_ASSERT_UNREACHABLE("Unsupported offset-path value");
    return Nothing();
  }

  const StyleOffsetRotate& rotate = display->mOffsetRotate;
  // If |rotate.auto_| is true, the element should be rotated by the angle of
  // the direction (i.e. directional tangent vector) of the offset-path, and the
  // computed value of <angle> is added to this.
  // Otherwise, the element has a constant clockwise rotation transformation
  // applied to it by the specified rotation angle. (i.e. Don't need to
  // consider the direction of the path.)
  gfx::Float angle = static_cast<gfx::Float>(
      (rotate.auto_ ? directionAngle : 0.0) + rotate.angle.ToRadians());

  // Compute the offset for motion path translate.
  // Bug 1559232: the translate parameters will be adjusted more after we
  // support offset-position.
  // FIXME: It's possible to refactor the calculation of transform-origin, so we
  // could calculate from the caller, and reuse the value in nsDisplayList.cpp.
  nsStyleTransformMatrix::TransformReferenceBox refBox(aFrame);
  const auto& transformOrigin = display->mTransformOrigin;
  const CSSPoint origin = nsStyleTransformMatrix::Convert2DPosition(
      transformOrigin.horizontal, transformOrigin.vertical, refBox);

  // Per the spec, the default offset-anchor is `auto`, so initialize the anchor
  // point to transform-origin.
  CSSPoint anchorPoint(origin);
  gfx::Point shift;
  if (!display->mOffsetAnchor.IsAuto()) {
    const auto& pos = display->mOffsetAnchor.AsPosition();
    anchorPoint = nsStyleTransformMatrix::Convert2DPosition(
        pos.horizontal, pos.vertical, refBox);
    // We need this value to shift the origin from transform-origin to
    // offset-anchor (and vice versa).
    // See nsStyleTransformMatrix::ReadTransform for more details.
    shift = (anchorPoint - origin).ToUnknownPoint();
  }

  // SVG frames (unlike other frames) have a reference box that can be (and
  // typically is) offset from the TopLeft() of the frame.
  // In motion path, we have to make sure the object is aligned with offset-path
  // when using content area, so we should tweak the anchor point by the offset.
  // Note: This may need to be updated if we support more transform-box.
  if ((aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT) &&
      display->mTransformBox != StyleGeometryBox::ViewBox &&
      display->mTransformBox != StyleGeometryBox::BorderBox) {
    anchorPoint.x += CSSPixel::FromAppUnits(aFrame->GetPosition().x);
    anchorPoint.y += CSSPixel::FromAppUnits(aFrame->GetPosition().y);
  }

  return Some(
      MotionPathData{point - anchorPoint.ToUnknownPoint(), angle, shift});
}

}  // namespace mozilla
