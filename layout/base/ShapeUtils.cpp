/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ShapeUtils.h"

#include <cstdlib>

#include "nsCSSRendering.h"
#include "nsStyleStruct.h"

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

} // namespace mozilla
