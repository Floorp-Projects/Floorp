/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_UNIT_TRANSFORMS_H_
#define MOZ_UNIT_TRANSFORMS_H_

#include "Units.h"

namespace mozilla {

// Convenience functions for converting an entity from one strongly-typed
// coordinate system to another without changing the values it stores (this
// can be thought of as a cast).
// To use these functions, you must provide a justification for each use!
// Feel free to add more justifications to PixelCastJustification, along with
// a comment that explains under what circumstances it is appropriate to use.

MOZ_BEGIN_ENUM_CLASS(PixelCastJustification, uint8_t)
  // For the root layer, Screen Pixel = Parent Layer Pixel.
  ScreenToParentLayerForRoot
MOZ_END_ENUM_CLASS(PixelCastJustification)

template <class TargetUnits, class SourceUnits>
gfx::SizeTyped<TargetUnits> ViewAs(const gfx::SizeTyped<SourceUnits>& aSize, PixelCastJustification) {
  return gfx::SizeTyped<TargetUnits>(aSize.width, aSize.height);
}
template <class TargetUnits, class SourceUnits>
gfx::IntSizeTyped<TargetUnits> ViewAs(const gfx::IntSizeTyped<SourceUnits>& aSize, PixelCastJustification) {
  return gfx::IntSizeTyped<TargetUnits>(aSize.width, aSize.height);
}

// Convenience functions for casting untyped entities to typed entities.
// Using these functions does not require a justification, but once we convert
// all code to use strongly typed units they should not be needed any longer.
template <class TargetUnits>
gfx::PointTyped<TargetUnits> ViewAs(const gfxPoint& aPoint) {
  return gfx::PointTyped<TargetUnits>(aPoint.x, aPoint.y);
}
template <class TargetUnits>
gfx::RectTyped<TargetUnits> ViewAs(const gfxRect& aRect) {
  return gfx::RectTyped<TargetUnits>(aRect.x, aRect.y, aRect.width, aRect.height);
}

// Convenience functions for casting typed entities to untyped entities.
// Using these functions does not require a justification, but once we convert
// all code to use strongly typed units they should not be needed any longer.
template <class SourceUnits>
gfxPoint ViewAsUntyped(const gfx::PointTyped<SourceUnits>& aPoint) {
  return gfxPoint(aPoint.x, aPoint.y);
}
template <class SourceUnits>
gfxRect ViewAsUntyped(const gfx::RectTyped<SourceUnits>& aRect) {
  return gfxRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

// Convenience functions for transforming an entity from one strongly-typed
// coordinate system to another using the provided transformation matrix.
template <typename TargetUnits, typename SourceUnits>
static gfx::PointTyped<TargetUnits> TransformTo(const gfx3DMatrix& aTransform,
                                                const gfx::PointTyped<SourceUnits>& aPoint)
{
  return ViewAs<TargetUnits>(aTransform.Transform(ViewAsUntyped(aPoint)));
}
template <typename TargetUnits, typename SourceUnits>
static gfx::RectTyped<TargetUnits> TransformTo(const gfx3DMatrix& aTransform,
                                               const gfx::RectTyped<SourceUnits>& aRect)
{
  return ViewAs<TargetUnits>(aTransform.TransformBounds(ViewAsUntyped(aRect)));
}


}

#endif
