/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ShapeUtils_h
#define mozilla_ShapeUtils_h

#include "nsCoord.h"
#include "nsSize.h"
#include "nsStyleConsts.h"
#include "nsTArray.h"

struct nsPoint;
struct nsRect;

namespace mozilla {
class StyleBasicShape;

// ShapeUtils is a namespace class containing utility functions related to
// processing basic shapes in the CSS Shapes Module.
// https://drafts.csswg.org/css-shapes/#basic-shape-functions
//
struct ShapeUtils final
{
  // Compute the length of a keyword <shape-radius>, i.e. closest-side or
  // farthest-side, for a circle or an ellipse on a single dimension. The
  // caller needs to call for both dimensions and combine the result.
  // https://drafts.csswg.org/css-shapes/#typedef-shape-radius.
  // @return The length of the radius in app units.
  static nscoord ComputeShapeRadius(const StyleShapeRadius aType,
                                    const nscoord aCenter,
                                    const nscoord aPosMin,
                                    const nscoord aPosMax);

  // Compute the center of a circle or an ellipse.
  // @param aRefBox The reference box of the basic shape.
  // @return The point of the center.
  static nsPoint ComputeCircleOrEllipseCenter(
    const UniquePtr<StyleBasicShape>& aBasicShape,
    const nsRect& aRefBox);

  // Compute the radius for a circle.
  // @param aCenter the center of the circle.
  // @param aRefBox the reference box of the circle.
  // @return The length of the radius in app units.
  static nscoord ComputeCircleRadius(
    const UniquePtr<StyleBasicShape>& aBasicShape,
    const nsPoint& aCenter, const nsRect& aRefBox);

  // Compute the radii for an ellipse.
  // @param aCenter the center of the ellipse.
  // @param aRefBox the reference box of the ellipse.
  // @return The radii of the ellipse in app units. The width and height
  // represent the x-axis and y-axis radii of the ellipse.
  static nsSize ComputeEllipseRadii(
    const UniquePtr<StyleBasicShape>& aBasicShape,
    const nsPoint& aCenter, const nsRect& aRefBox);

  // Compute the rect for an inset. If the inset amount is larger than
  // aRefBox itself, this will return a rect the same shape as the inverse
  // rect that would be created by insetting aRefBox by the inset amount.
  // This process is *not* what is called for by the current spec at
  // https://drafts.csswg.org/css-shapes-1/#supported-basic-shapes.
  // The spec currently treats empty shapes, including overly-inset rects, as
  // defining 'empty float areas' that don't affect layout. However, it is
  // practically useful to treat empty shapes as having edges for purposes of
  // affecting layout, and there is growing momentum for the approach we
  // are taking here.
  // @param aRefBox the reference box of the inset.
  // @return The inset rect in app units.
  static nsRect ComputeInsetRect(
    const UniquePtr<StyleBasicShape>& aBasicShape,
    const nsRect& aRefBox);

  // Compute the radii for an inset.
  // @param aRefBox the reference box of the inset.
  // @param aInsetRect the inset rect computed by ComputeInsetRect().
  // @param aRadii the returned radii in app units.
  // @return true if any of the radii is nonzero; false otherwise.
  static bool ComputeInsetRadii(
    const UniquePtr<StyleBasicShape>& aBasicShape,
    const nsRect& aInsetRect,
    const nsRect& aRefBox,
    nscoord aRadii[8]);

  // Compute the vertices for a polygon.
  // @param aRefBox the reference box of the polygon.
  // @return The vertices in app units; the coordinate space is the same
  //         as aRefBox.
  static nsTArray<nsPoint> ComputePolygonVertices(
    const UniquePtr<StyleBasicShape>& aBasicShape,
    const nsRect& aRefBox);
};

} // namespace mozilla

#endif // mozilla_ShapeUtils_h
