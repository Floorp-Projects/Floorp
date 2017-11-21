/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ShapeUtils.h"

#include <cstdlib>

#include "nsCSSRendering.h"
#include "nsMargin.h"
#include "nsStyleCoord.h"
#include "nsStyleStruct.h"
#include "SVGContentUtils.h"

namespace mozilla {

nscoord
ShapeUtils::ComputeShapeRadius(const StyleShapeRadius aType,
                               const nscoord aCenter,
                               const nscoord aPosMin,
                               const nscoord aPosMax)
{
  nscoord dist1 = std::abs(aPosMin - aCenter);
  nscoord dist2 = std::abs(aPosMax - aCenter);
  nscoord length = 0;
  switch (aType) {
    case StyleShapeRadius::FarthestSide:
      length = dist1 > dist2 ? dist1 : dist2;
      break;
    case StyleShapeRadius::ClosestSide:
      length = dist1 > dist2 ? dist2 : dist1;
      break;
  }
  return length;
}

nsPoint
ShapeUtils::ComputeCircleOrEllipseCenter(const UniquePtr<StyleBasicShape>& aBasicShape,
                                         const nsRect& aRefBox)
{
  MOZ_ASSERT(aBasicShape->GetShapeType() == StyleBasicShapeType::Circle ||
             aBasicShape->GetShapeType() == StyleBasicShapeType::Ellipse,
             "The basic shape must be circle() or ellipse!");

  nsPoint topLeft, anchor;
  nsSize size(aRefBox.Size());
  nsImageRenderer::ComputeObjectAnchorPoint(aBasicShape->GetPosition(),
                                            size, size,
                                            &topLeft, &anchor);
  return anchor + aRefBox.TopLeft();
}

nscoord
ShapeUtils::ComputeCircleRadius(const UniquePtr<StyleBasicShape>& aBasicShape,
                                const nsPoint& aCenter,
                                const nsRect& aRefBox)
{
  MOZ_ASSERT(aBasicShape->GetShapeType() == StyleBasicShapeType::Circle,
             "The basic shape must be circle()!");

  const nsTArray<nsStyleCoord>& coords = aBasicShape->Coordinates();
  MOZ_ASSERT(coords.Length() == 1, "wrong number of arguments");
  nscoord r = 0;
  if (coords[0].GetUnit() == eStyleUnit_Enumerated) {
    const auto styleShapeRadius = coords[0].GetEnumValue<StyleShapeRadius>();
    nscoord horizontal =
      ComputeShapeRadius(styleShapeRadius, aCenter.x, aRefBox.x, aRefBox.XMost());
    nscoord vertical =
      ComputeShapeRadius(styleShapeRadius, aCenter.y, aRefBox.y, aRefBox.YMost());
    r = styleShapeRadius == StyleShapeRadius::FarthestSide
          ? std::max(horizontal, vertical)
          : std::min(horizontal, vertical);
  } else {
    // We resolve percent <shape-radius> value for circle() as defined here:
    // https://drafts.csswg.org/css-shapes/#funcdef-circle
    double referenceLength =
      SVGContentUtils::ComputeNormalizedHypotenuse(aRefBox.width,
                                                   aRefBox.height);
    r = coords[0].ComputeCoordPercentCalc(NSToCoordRound(referenceLength));
  }
  return r;
}

nsSize
ShapeUtils::ComputeEllipseRadii(const UniquePtr<StyleBasicShape>& aBasicShape,
                                const nsPoint& aCenter,
                                const nsRect& aRefBox)
{
  MOZ_ASSERT(aBasicShape->GetShapeType() == StyleBasicShapeType::Ellipse,
             "The basic shape must be ellipse()!");

  const nsTArray<nsStyleCoord>& coords = aBasicShape->Coordinates();
  MOZ_ASSERT(coords.Length() == 2, "wrong number of arguments");
  nsSize radii;

  if (coords[0].GetUnit() == eStyleUnit_Enumerated) {
    const StyleShapeRadius radiusX = coords[0].GetEnumValue<StyleShapeRadius>();
    radii.width = ComputeShapeRadius(radiusX, aCenter.x, aRefBox.x,
                                     aRefBox.XMost());
  } else {
    radii.width = coords[0].ComputeCoordPercentCalc(aRefBox.width);
  }

  if (coords[1].GetUnit() == eStyleUnit_Enumerated) {
    const StyleShapeRadius radiusY = coords[1].GetEnumValue<StyleShapeRadius>();
    radii.height = ComputeShapeRadius(radiusY, aCenter.y, aRefBox.y,
                                      aRefBox.YMost());
  } else {
    radii.height = coords[1].ComputeCoordPercentCalc(aRefBox.height);
  }

  return radii;
}

/* static */ nsRect
ShapeUtils::ComputeInsetRect(const UniquePtr<StyleBasicShape>& aBasicShape,
                             const nsRect& aRefBox)
{
  MOZ_ASSERT(aBasicShape->GetShapeType() == StyleBasicShapeType::Inset,
             "The basic shape must be inset()!");

  const nsTArray<nsStyleCoord>& coords = aBasicShape->Coordinates();
  MOZ_ASSERT(coords.Length() == 4, "wrong number of arguments");

  nsMargin inset(coords[0].ComputeCoordPercentCalc(aRefBox.height),
                 coords[1].ComputeCoordPercentCalc(aRefBox.width),
                 coords[2].ComputeCoordPercentCalc(aRefBox.height),
                 coords[3].ComputeCoordPercentCalc(aRefBox.width));

  nsRect insetRect(aRefBox);
  insetRect.Deflate(inset);

  return insetRect;
}

/* static */ bool
ShapeUtils::ComputeInsetRadii(const UniquePtr<StyleBasicShape>& aBasicShape,
                              const nsRect& aInsetRect,
                              const nsRect& aRefBox,
                              nscoord aRadii[8])
{
  const nsStyleCorners& radius = aBasicShape->GetRadius();
  return nsIFrame::ComputeBorderRadii(radius, aInsetRect.Size(), aRefBox.Size(),
                                      Sides(), aRadii);

}

/* static */ nsTArray<nsPoint>
ShapeUtils::ComputePolygonVertices(const UniquePtr<StyleBasicShape>& aBasicShape,
                                   const nsRect& aRefBox)
{
  MOZ_ASSERT(aBasicShape->GetShapeType() == StyleBasicShapeType::Polygon,
             "The basic shape must be polygon()!");

  const nsTArray<nsStyleCoord>& coords = aBasicShape->Coordinates();
  MOZ_ASSERT(coords.Length() % 2 == 0 &&
             coords.Length() >= 2, "Wrong number of arguments!");

  nsTArray<nsPoint> vertices(coords.Length() / 2);
  for (size_t i = 0; i + 1 < coords.Length(); i += 2) {
    vertices.AppendElement(
      nsPoint(coords[i].ComputeCoordPercentCalc(aRefBox.width),
              coords[i + 1].ComputeCoordPercentCalc(aRefBox.height))
      + aRefBox.TopLeft());
  }
  return vertices;
}

} // namespace mozilla
