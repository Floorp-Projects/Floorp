/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ShapeUtils_h
#define mozilla_ShapeUtils_h

#include "nsCoord.h"
#include "nsStyleConsts.h"

namespace mozilla {

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
  //
  // @return The length of the radius in app units.
  static nscoord ComputeShapeRadius(const StyleShapeRadius aType,
                                    const nscoord aCenter,
                                    const nscoord aPosMin,
                                    const nscoord aPosMax);
};

} // namespace mozilla

#endif // mozilla_ShapeUtils_h
