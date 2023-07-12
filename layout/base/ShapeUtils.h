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
namespace gfx {
class Path;
class PathBuilder;
}  // namespace gfx

// ShapeUtils is a namespace class containing utility functions related to
// processing basic shapes in the CSS Shapes Module.
// https://drafts.csswg.org/css-shapes/#basic-shape-functions
//
struct ShapeUtils final {
  // Compute the length of a keyword <shape-radius>, i.e. closest-side or
  // farthest-side, for a circle or an ellipse on a single dimension. The
  // caller needs to call for both dimensions and combine the result.
  // https://drafts.csswg.org/css-shapes/#typedef-shape-radius.
  // @return The length of the radius in app units.
  static nscoord ComputeShapeRadius(const StyleShapeRadius& aType,
                                    const nscoord aCenter,
                                    const nscoord aPosMin,
                                    const nscoord aPosMax);

  // Compute the position based on |aRefBox|.
  // @param aRefBox The reference box for the position.
  // @return The point inside |aRefBox|.
  static nsPoint ComputePosition(const StylePosition&, const nsRect&);

  // Compute the center of a circle or an ellipse.
  // @param aRefBox The reference box of the basic shape.
  // @return The point of the center.
  static nsPoint ComputeCircleOrEllipseCenter(const StyleBasicShape&,
                                              const nsRect& aRefBox);

  // Compute the radius for a circle.
  // @param aCenter the center of the circle.
  // @param aRefBox the reference box of the circle.
  // @return The length of the radius in app units.
  static nscoord ComputeCircleRadius(const StyleBasicShape&,
                                     const nsPoint& aCenter,
                                     const nsRect& aRefBox);

  // Compute the radii for an ellipse.
  // @param aCenter the center of the ellipse.
  // @param aRefBox the reference box of the ellipse.
  // @return The radii of the ellipse in app units. The width and height
  // represent the x-axis and y-axis radii of the ellipse.
  static nsSize ComputeEllipseRadii(const StyleBasicShape&,
                                    const nsPoint& aCenter,
                                    const nsRect& aRefBox);

  // Compute the rect for an inset()/xywh()/rect().
  // If the inset amount is larger than aRefBox itself, this will return a rect
  // the same shape as the inverse rect that would be created by insetting
  // aRefBox by the inset amount. This process is *not* what is called for by
  // the current spec at
  // https://drafts.csswg.org/css-shapes-1/#supported-basic-shapes.
  //
  // The spec currently treats empty shapes, including overly-inset rects, as
  // defining 'empty float areas' that don't affect layout. However, it is
  // practically useful to treat empty shapes as having edges for purposes of
  // affecting layout, and there is growing momentum for the approach we
  // are taking here.
  // @param aRefBox the reference box of the inset/xywh/rect.
  // @return The inset/xywh/rect rect in app units.
  static nsRect ComputeInsetRect(const StyleRect<LengthPercentage>& aStyleRect,
                                 const nsRect& aRefBox);

  // Compute the radii for a rectanglar shape, i.e. inset()/xywh()/rect().
  // @param aRefBox the reference box of the rect.
  // @param aRect the rect we computed from Compute{Inset}Rect(), in app units.
  // @param aRadii the returned radii in app units.
  // @return true if any of the radii is nonzero; false otherwise.
  static bool ComputeRectRadii(const StyleBorderRadius&, const nsRect& aRefBox,
                               const nsRect& aRect, nscoord aRadii[8]);

  // Compute the vertices for a polygon.
  // @param aRefBox the reference box of the polygon.
  // @return The vertices in app units; the coordinate space is the same
  //         as aRefBox.
  static nsTArray<nsPoint> ComputePolygonVertices(const StyleBasicShape&,
                                                  const nsRect& aRefBox);

  // Compute a gfx::path from a circle.
  // @param aRefBox the reference box of the circle.
  // @param aCenter the center point of the circle.
  // @return The gfx::Path of this circle.
  static already_AddRefed<gfx::Path> BuildCirclePath(const StyleBasicShape&,
                                                     const nsRect& aRefBox,
                                                     const nsPoint& aCenter,
                                                     nscoord aAppUnitsPerPixel,
                                                     gfx::PathBuilder*);

  // Compute a gfx::path from an ellipse.
  // @param aRefBox the reference box of the ellipse.
  // @param aCenter the center point of the ellipse.
  // @return The gfx::Path of this ellipse.
  static already_AddRefed<gfx::Path> BuildEllipsePath(const StyleBasicShape&,
                                                      const nsRect& aRefBox,
                                                      const nsPoint& aCenter,
                                                      nscoord aAppUnitsPerPixel,
                                                      gfx::PathBuilder*);

  // Compute a gfx::path from a polygon.
  // @param aRefBox the reference box of the polygon.
  // @return The gfx::Path of this polygon.
  static already_AddRefed<gfx::Path> BuildPolygonPath(const StyleBasicShape&,
                                                      const nsRect& aRefBox,
                                                      nscoord aAppUnitsPerPixel,
                                                      gfx::PathBuilder*);

  // Compute a gfx::path from a StyleBasicShape which is an inset.
  // @param aRefBox the reference box of the inset.
  // @return The gfx::Path of this inset.
  static already_AddRefed<gfx::Path> BuildInsetPath(const StyleBasicShape&,
                                                    const nsRect& aRefBox,
                                                    nscoord aAppUnitsPerPixel,
                                                    gfx::PathBuilder*);

  // Compute a gfx::path from a rectanglar shape (i.e. inset()/xywh()/rect())
  // and the round radii.
  // @param aRect the rect we computed from Compute{Inset}Rect().
  // @param aRadii the radii of the rect. It should be an array with length 8.
  //               If it's nullptr, we don't have the valid radii.
  // @param aRefBox the reference box of the rect.
  // @return The gfx::Path of this rect.
  static already_AddRefed<gfx::Path> BuildRectPath(const nsRect& aRect,
                                                   const nscoord aRadii[8],
                                                   const nsRect& aRefBox,
                                                   nscoord aAppUnitsPerPixel,
                                                   gfx::PathBuilder*);
};

}  // namespace mozilla

#endif  // mozilla_ShapeUtils_h
