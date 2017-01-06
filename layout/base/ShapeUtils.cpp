/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ShapeUtils.h"

#include <cstdlib>

#include "nsCSSRendering.h"
#include "nsRuleNode.h"
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
ShapeUtils::ComputeCircleOrEllipseCenter(StyleBasicShape* const aBasicShape,
                                         const nsRect& aRefBox)
{
  nsPoint topLeft, anchor;
  nsSize size(aRefBox.Size());
  nsImageRenderer::ComputeObjectAnchorPoint(aBasicShape->GetPosition(),
                                            size, size,
                                            &topLeft, &anchor);
  return nsPoint(anchor.x + aRefBox.x, anchor.y + aRefBox.y);
}

nscoord
ShapeUtils::ComputeCircleRadius(StyleBasicShape* const aBasicShape,
                                const nsPoint& aCenter,
                                const nsRect& aRefBox)
{
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
    r = nsRuleNode::ComputeCoordPercentCalc(coords[0],
                                            NSToCoordRound(referenceLength));
  }
  return r;
}

} // namespace mozilla
