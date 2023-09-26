/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ShapeUtils.h"

#include <cstdlib>

#include "nsCSSRendering.h"
#include "nsLayoutUtils.h"
#include "nsMargin.h"
#include "nsStyleStruct.h"
#include "mozilla/SVGContentUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/PathHelpers.h"

namespace mozilla {

nscoord ShapeUtils::ComputeShapeRadius(const StyleShapeRadius& aType,
                                       const nscoord aCenter,
                                       const nscoord aPosMin,
                                       const nscoord aPosMax) {
  MOZ_ASSERT(aType.IsFarthestSide() || aType.IsClosestSide());
  nscoord dist1 = std::abs(aPosMin - aCenter);
  nscoord dist2 = std::abs(aPosMax - aCenter);
  nscoord length = 0;
  if (aType.IsFarthestSide()) {
    length = dist1 > dist2 ? dist1 : dist2;
  } else {
    length = dist1 > dist2 ? dist2 : dist1;
  }
  return length;
}

nsPoint ShapeUtils::ComputePosition(const StylePosition& aPosition,
                                    const nsRect& aRefBox) {
  nsPoint topLeft, anchor;
  nsSize size(aRefBox.Size());
  nsImageRenderer::ComputeObjectAnchorPoint(aPosition, size, size, &topLeft,
                                            &anchor);
  return anchor + aRefBox.TopLeft();
}

nsPoint ShapeUtils::ComputeCircleOrEllipseCenter(
    const StyleBasicShape& aBasicShape, const nsRect& aRefBox) {
  MOZ_ASSERT(aBasicShape.IsCircle() || aBasicShape.IsEllipse(),
             "The basic shape must be circle() or ellipse!");

  const auto& position = aBasicShape.IsCircle()
                             ? aBasicShape.AsCircle().position
                             : aBasicShape.AsEllipse().position;
  // If position is not specified, we use 50% 50%.
  if (position.IsAuto()) {
    return ComputePosition(StylePosition::FromPercentage(0.5), aRefBox);
  }

  MOZ_ASSERT(position.IsPosition());
  return ComputePosition(position.AsPosition(), aRefBox);
}

nscoord ShapeUtils::ComputeCircleRadius(const StyleBasicShape& aBasicShape,
                                        const nsPoint& aCenter,
                                        const nsRect& aRefBox) {
  MOZ_ASSERT(aBasicShape.IsCircle(), "The basic shape must be circle()!");
  const auto& radius = aBasicShape.AsCircle().radius;
  if (radius.IsLength()) {
    return radius.AsLength().Resolve([&] {
      // We resolve percent <shape-radius> value for circle() as defined here:
      // https://drafts.csswg.org/css-shapes/#funcdef-circle
      double referenceLength = SVGContentUtils::ComputeNormalizedHypotenuse(
          aRefBox.width, aRefBox.height);
      return NSToCoordRound(referenceLength);
    });
  }

  nscoord horizontal =
      ComputeShapeRadius(radius, aCenter.x, aRefBox.x, aRefBox.XMost());
  nscoord vertical =
      ComputeShapeRadius(radius, aCenter.y, aRefBox.y, aRefBox.YMost());
  return radius.IsFarthestSide() ? std::max(horizontal, vertical)
                                 : std::min(horizontal, vertical);
}

nsSize ShapeUtils::ComputeEllipseRadii(const StyleBasicShape& aBasicShape,
                                       const nsPoint& aCenter,
                                       const nsRect& aRefBox) {
  MOZ_ASSERT(aBasicShape.IsEllipse(), "The basic shape must be ellipse()!");
  const auto& ellipse = aBasicShape.AsEllipse();
  nsSize radii;
  if (ellipse.semiaxis_x.IsLength()) {
    radii.width = ellipse.semiaxis_x.AsLength().Resolve(aRefBox.width);
  } else {
    radii.width = ComputeShapeRadius(ellipse.semiaxis_x, aCenter.x, aRefBox.x,
                                     aRefBox.XMost());
  }

  if (ellipse.semiaxis_y.IsLength()) {
    radii.height = ellipse.semiaxis_y.AsLength().Resolve(aRefBox.height);
  } else {
    radii.height = ComputeShapeRadius(ellipse.semiaxis_y, aCenter.y, aRefBox.y,
                                      aRefBox.YMost());
  }

  return radii;
}

/* static */
nsRect ShapeUtils::ComputeInsetRect(
    const StyleRect<LengthPercentage>& aStyleRect, const nsRect& aRefBox) {
  nsMargin inset(aStyleRect._0.Resolve(aRefBox.Height()),
                 aStyleRect._1.Resolve(aRefBox.Width()),
                 aStyleRect._2.Resolve(aRefBox.Height()),
                 aStyleRect._3.Resolve(aRefBox.Width()));

  nscoord x = aRefBox.X() + inset.left;
  nscoord width = aRefBox.Width() - inset.LeftRight();
  nscoord y = aRefBox.Y() + inset.top;
  nscoord height = aRefBox.Height() - inset.TopBottom();

  // Invert left and right, if necessary.
  if (width < 0) {
    width *= -1;
    x -= width;
  }

  // Invert top and bottom, if necessary.
  if (height < 0) {
    height *= -1;
    y -= height;
  }

  return nsRect(x, y, width, height);
}

/* static */
bool ShapeUtils::ComputeRectRadii(const StyleBorderRadius& aBorderRadius,
                                  const nsRect& aRefBox, const nsRect& aRect,
                                  nscoord aRadii[8]) {
  return nsIFrame::ComputeBorderRadii(aBorderRadius, aRefBox.Size(),
                                      aRect.Size(), Sides(), aRadii);
}

/* static */
nsTArray<nsPoint> ShapeUtils::ComputePolygonVertices(
    const StyleBasicShape& aBasicShape, const nsRect& aRefBox) {
  MOZ_ASSERT(aBasicShape.IsPolygon(), "The basic shape must be polygon()!");

  auto coords = aBasicShape.AsPolygon().coordinates.AsSpan();
  nsTArray<nsPoint> vertices(coords.Length());
  for (const StylePolygonCoord<LengthPercentage>& point : coords) {
    vertices.AppendElement(nsPoint(point._0.Resolve(aRefBox.width),
                                   point._1.Resolve(aRefBox.height)) +
                           aRefBox.TopLeft());
  }
  return vertices;
}

/* static */
static inline gfx::Point ConvertToGfxPoint(const nsPoint& aPoint,
                                           nscoord aAppUnitsPerPixel) {
  return {static_cast<gfx::Float>(aPoint.x) /
              static_cast<gfx::Float>(aAppUnitsPerPixel),
          static_cast<gfx::Float>(aPoint.y) /
              static_cast<gfx::Float>(aAppUnitsPerPixel)};
}

/* static */
already_AddRefed<gfx::Path> ShapeUtils::BuildCirclePath(
    const StyleBasicShape& aShape, const nsRect& aRefBox,
    const nsPoint& aCenter, nscoord aAppUnitsPerPixel,
    gfx::PathBuilder* aPathBuilder) {
  const nscoord r = ComputeCircleRadius(aShape, aCenter, aRefBox);
  aPathBuilder->Arc(
      ConvertToGfxPoint(aCenter, aAppUnitsPerPixel),
      static_cast<float>(r) / static_cast<float>(aAppUnitsPerPixel), 0.0,
      gfx::Float(2.0 * M_PI));
  aPathBuilder->Close();
  return aPathBuilder->Finish();
}

static inline gfx::Size ConvertToGfxSize(const nsSize& aSize,
                                         nscoord aAppUnitsPerPixel) {
  return {static_cast<gfx::Float>(aSize.width) /
              static_cast<gfx::Float>(aAppUnitsPerPixel),
          static_cast<gfx::Float>(aSize.height) /
              static_cast<gfx::Float>(aAppUnitsPerPixel)};
}

/* static */
already_AddRefed<gfx::Path> ShapeUtils::BuildEllipsePath(
    const StyleBasicShape& aShape, const nsRect& aRefBox,
    const nsPoint& aCenter, nscoord aAppUnitsPerPixel,
    gfx::PathBuilder* aPathBuilder) {
  const nsSize radii = ComputeEllipseRadii(aShape, aCenter, aRefBox);
  EllipseToBezier(aPathBuilder, ConvertToGfxPoint(aCenter, aAppUnitsPerPixel),
                  ConvertToGfxSize(radii, aAppUnitsPerPixel));
  aPathBuilder->Close();
  return aPathBuilder->Finish();
}

/* static */
already_AddRefed<gfx::Path> ShapeUtils::BuildPolygonPath(
    const StyleBasicShape& aShape, const nsRect& aRefBox,
    nscoord aAppUnitsPerPixel, gfx::PathBuilder* aPathBuilder) {
  nsTArray<nsPoint> vertices = ComputePolygonVertices(aShape, aRefBox);
  if (vertices.IsEmpty()) {
    MOZ_ASSERT_UNREACHABLE(
        "ComputePolygonVertices() should've given us some vertices!");
  } else {
    aPathBuilder->MoveTo(NSPointToPoint(vertices[0], aAppUnitsPerPixel));
    for (size_t i = 1; i < vertices.Length(); ++i) {
      aPathBuilder->LineTo(NSPointToPoint(vertices[i], aAppUnitsPerPixel));
    }
  }
  aPathBuilder->Close();
  return aPathBuilder->Finish();
}

/* static */
already_AddRefed<gfx::Path> ShapeUtils::BuildInsetPath(
    const StyleBasicShape& aShape, const nsRect& aRefBox,
    nscoord aAppUnitsPerPixel, gfx::PathBuilder* aPathBuilder) {
  const nsRect insetRect = ComputeInsetRect(aShape.AsRect().rect, aRefBox);
  nscoord appUnitsRadii[8];
  const bool hasRadii = ComputeRectRadii(aShape.AsRect().round, aRefBox,
                                         insetRect, appUnitsRadii);
  return BuildRectPath(insetRect, hasRadii ? appUnitsRadii : nullptr, aRefBox,
                       aAppUnitsPerPixel, aPathBuilder);
}

/* static */
already_AddRefed<gfx::Path> ShapeUtils::BuildRectPath(
    const nsRect& aRect, const nscoord aRadii[8], const nsRect& aRefBox,
    nscoord aAppUnitsPerPixel, gfx::PathBuilder* aPathBuilder) {
  const gfx::Rect insetRectPixels = NSRectToRect(aRect, aAppUnitsPerPixel);
  if (aRadii) {
    gfx::RectCornerRadii corners;
    nsCSSRendering::ComputePixelRadii(aRadii, aAppUnitsPerPixel, &corners);

    AppendRoundedRectToPath(aPathBuilder, insetRectPixels, corners, true);
  } else {
    AppendRectToPath(aPathBuilder, insetRectPixels, true);
  }
  return aPathBuilder->Finish();
}

}  // namespace mozilla
