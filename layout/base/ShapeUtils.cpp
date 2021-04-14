/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ShapeUtils.h"

#include <cstdlib>

#include "nsCSSRendering.h"
#include "nsMargin.h"
#include "nsStyleStruct.h"
#include "mozilla/SVGContentUtils.h"

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

nsPoint ShapeUtils::ComputeCircleOrEllipseCenter(
    const StyleBasicShape& aBasicShape, const nsRect& aRefBox) {
  MOZ_ASSERT(aBasicShape.IsCircle() || aBasicShape.IsEllipse(),
             "The basic shape must be circle() or ellipse!");

  const auto& position = aBasicShape.IsCircle()
                             ? aBasicShape.AsCircle().position
                             : aBasicShape.AsEllipse().position;

  nsPoint topLeft, anchor;
  nsSize size(aRefBox.Size());
  nsImageRenderer::ComputeObjectAnchorPoint(position, size, size, &topLeft,
                                            &anchor);
  return anchor + aRefBox.TopLeft();
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
nsRect ShapeUtils::ComputeInsetRect(const StyleBasicShape& aBasicShape,
                                    const nsRect& aRefBox) {
  MOZ_ASSERT(aBasicShape.IsInset(), "The basic shape must be inset()!");
  const auto& rect = aBasicShape.AsInset().rect;
  nsMargin inset(
      rect._0.Resolve(aRefBox.Height()), rect._1.Resolve(aRefBox.Width()),
      rect._2.Resolve(aRefBox.Height()), rect._3.Resolve(aRefBox.Width()));

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
bool ShapeUtils::ComputeInsetRadii(const StyleBasicShape& aBasicShape,
                                   const nsRect& aRefBox, nscoord aRadii[8]) {
  const auto& radius = aBasicShape.AsInset().round;
  return nsIFrame::ComputeBorderRadii(radius, aRefBox.Size(), aRefBox.Size(),
                                      Sides(), aRadii);
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

}  // namespace mozilla
