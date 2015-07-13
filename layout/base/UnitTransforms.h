/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_UNIT_TRANSFORMS_H_
#define MOZ_UNIT_TRANSFORMS_H_

#include "Units.h"
#include "mozilla/gfx/Matrix.h"

namespace mozilla {

// Convenience functions for converting an entity from one strongly-typed
// coordinate system to another without changing the values it stores (this
// can be thought of as a cast).
// To use these functions, you must provide a justification for each use!
// Feel free to add more justifications to PixelCastJustification, along with
// a comment that explains under what circumstances it is appropriate to use.

enum class PixelCastJustification : uint8_t {
  // For the root layer, Screen Pixel = Parent Layer Pixel.
  ScreenIsParentLayerForRoot,
  // On the layout side, Screen Pixel = LayoutDevice at the outer-window level.
  LayoutDeviceIsScreenForBounds,
  // For the root layer, Render Target Pixel = Parent Layer Pixel.
  RenderTargetIsParentLayerForRoot,
  // For the root composition size we want to view it as layer pixels in any layer
  ParentLayerToLayerForRootComposition,
  // The Layer coordinate space for one layer is the ParentLayer coordinate
  // space for its children
  MovingDownToChildren,
  // The transform that is usually used to convert between two coordinate
  // systems is not available (for example, because the object that stores it
  // is being destroyed), so fall back to the identity.
  TransformNotAvailable,
  // When an OS event is initially constructed, its reference point is
  // technically in screen pixels, as it has not yet accounted for any
  // asynchronous transforms. This justification is for viewing the initial
  // reference point as a screen point.
  LayoutDeviceToScreenForUntransformedEvent,
  // Similar to LayoutDeviceToScreenForUntransformedEvent, PBrowser handles
  // some widget/tab dimension information as the OS does -- in screen units.
  LayoutDeviceIsScreenForTabDims
};

template <class TargetUnits, class SourceUnits>
gfx::SizeTyped<TargetUnits> ViewAs(const gfx::SizeTyped<SourceUnits>& aSize, PixelCastJustification) {
  return gfx::SizeTyped<TargetUnits>(aSize.width, aSize.height);
}
template <class TargetUnits, class SourceUnits>
gfx::IntSizeTyped<TargetUnits> ViewAs(const gfx::IntSizeTyped<SourceUnits>& aSize, PixelCastJustification) {
  return gfx::IntSizeTyped<TargetUnits>(aSize.width, aSize.height);
}
template <class TargetUnits, class SourceUnits>
gfx::PointTyped<TargetUnits> ViewAs(const gfx::PointTyped<SourceUnits>& aPoint, PixelCastJustification) {
  return gfx::PointTyped<TargetUnits>(aPoint.x, aPoint.y);
}
template <class TargetUnits, class SourceUnits>
gfx::IntPointTyped<TargetUnits> ViewAs(const gfx::IntPointTyped<SourceUnits>& aPoint, PixelCastJustification) {
  return gfx::IntPointTyped<TargetUnits>(aPoint.x, aPoint.y);
}
template <class TargetUnits, class SourceUnits>
gfx::RectTyped<TargetUnits> ViewAs(const gfx::RectTyped<SourceUnits>& aRect, PixelCastJustification) {
  return gfx::RectTyped<TargetUnits>(aRect.x, aRect.y, aRect.width, aRect.height);
}
template <class TargetUnits, class SourceUnits>
gfx::IntRectTyped<TargetUnits> ViewAs(const gfx::IntRectTyped<SourceUnits>& aRect, PixelCastJustification) {
  return gfx::IntRectTyped<TargetUnits>(aRect.x, aRect.y, aRect.width, aRect.height);
}
template <class NewTargetUnits, class OldTargetUnits, class SourceUnits>
gfx::ScaleFactor<SourceUnits, NewTargetUnits> ViewTargetAs(
    const gfx::ScaleFactor<SourceUnits, OldTargetUnits>& aScaleFactor,
    PixelCastJustification) {
  return gfx::ScaleFactor<SourceUnits, NewTargetUnits>(aScaleFactor.scale);
}

// Convenience functions for casting untyped entities to typed entities.
// Using these functions does not require a justification, but once we convert
// all code to use strongly typed units they should not be needed any longer.
template <class TargetUnits>
gfx::PointTyped<TargetUnits> ViewAs(const gfxPoint& aPoint) {
  return gfx::PointTyped<TargetUnits>(aPoint.x, aPoint.y);
}
template <class TargetUnits>
gfx::PointTyped<TargetUnits> ViewAs(const gfx::Point& aPoint) {
  return gfx::PointTyped<TargetUnits>(aPoint.x, aPoint.y);
}
template <class TargetUnits>
gfx::RectTyped<TargetUnits> ViewAs(const gfx::Rect& aRect) {
  return gfx::RectTyped<TargetUnits>(aRect.x, aRect.y, aRect.width, aRect.height);
}
template <class TargetUnits>
gfx::IntSizeTyped<TargetUnits> ViewAs(const nsIntSize& aSize) {
  return gfx::IntSizeTyped<TargetUnits>(aSize.width, aSize.height);
}
template <class TargetUnits>
gfx::IntPointTyped<TargetUnits> ViewAs(const nsIntPoint& aPoint) {
  return gfx::IntPointTyped<TargetUnits>(aPoint.x, aPoint.y);
}
template <class TargetUnits>
gfx::IntRectTyped<TargetUnits> ViewAs(const nsIntRect& aRect) {
  return gfx::IntRectTyped<TargetUnits>(aRect.x, aRect.y, aRect.width, aRect.height);
}

// Convenience functions for transforming an entity from one strongly-typed
// coordinate system to another using the provided transformation matrix.
template <typename TargetUnits, typename SourceUnits>
static gfx::PointTyped<TargetUnits> TransformTo(const gfx::Matrix4x4& aTransform,
                                                const gfx::PointTyped<SourceUnits>& aPoint)
{
  return ViewAs<TargetUnits>(aTransform * aPoint.ToUnknownPoint());
}
template <typename TargetUnits, typename SourceUnits>
static gfx::IntPointTyped<TargetUnits> TransformTo(const gfx::Matrix4x4& aTransform,
                                                   const gfx::IntPointTyped<SourceUnits>& aPoint)
{
  return RoundedToInt(TransformTo<TargetUnits>(aTransform, gfx::PointTyped<SourceUnits>(aPoint)));
}
template <typename TargetUnits, typename SourceUnits>
static gfx::RectTyped<TargetUnits> TransformTo(const gfx::Matrix4x4& aTransform,
                                               const gfx::RectTyped<SourceUnits>& aRect)
{
  return ViewAs<TargetUnits>(aTransform.TransformBounds(aRect.ToUnknownRect()));
}
template <typename TargetUnits, typename SourceUnits>
static gfx::IntRectTyped<TargetUnits> TransformTo(const gfx::Matrix4x4& aTransform,
                                                  const gfx::IntRectTyped<SourceUnits>& aRect)
{
  gfx::Rect rect(aRect.ToUnknownRect());
  return RoundedToInt(ViewAs<TargetUnits>(aTransform.TransformBounds(rect)));
}

// Transform |aVector|, which is anchored at |aAnchor|, by the given transform
// matrix, yielding a point in |TargetUnits|.
// The anchor is necessary because with 3D tranforms, the location of the
// vector can affect the result of the transform.
template <typename TargetUnits, typename SourceUnits>
static gfx::PointTyped<TargetUnits> TransformVector(const gfx::Matrix4x4& aTransform,
                                                    const gfx::PointTyped<SourceUnits>& aVector,
                                                    const gfx::PointTyped<SourceUnits>& aAnchor) {
  gfx::PointTyped<TargetUnits> transformedStart = TransformTo<TargetUnits>(aTransform, aAnchor);
  gfx::PointTyped<TargetUnits> transformedEnd = TransformTo<TargetUnits>(aTransform, aAnchor + aVector);
  return transformedEnd - transformedStart;
}

// UntransformTo() and UntransformVector() are like TransformTo() and 
// TransformVector(), respectively, but are intended for cases where
// the transformation matrix is the inverse of a 3D projection. When
// using such transforms, the resulting Point4D is only meaningful
// if it has a positive w-coordinate. To handle this, these functions
// return a Maybe object which contains a value if and only if the
// result is meaningful
template <typename TargetUnits, typename SourceUnits>
static Maybe<gfx::PointTyped<TargetUnits>> UntransformTo(const gfx::Matrix4x4& aTransform,
                                                const gfx::PointTyped<SourceUnits>& aPoint)
{
  gfx::Point4D point = aTransform.ProjectPoint(aPoint.ToUnknownPoint());
  if (!point.HasPositiveWCoord()) {
    return Nothing();
  }
  return Some(ViewAs<TargetUnits>(point.As2DPoint()));
}
template <typename TargetUnits, typename SourceUnits>
static Maybe<gfx::IntPointTyped<TargetUnits>> UntransformTo(const gfx::Matrix4x4& aTransform,
                                                const gfx::IntPointTyped<SourceUnits>& aPoint)
{
  gfx::Point4D point = aTransform.ProjectPoint(aPoint.ToUnknownPoint());
  if (!point.HasPositiveWCoord()) {
    return Nothing();
  }
  return Some(RoundedToInt(ViewAs<TargetUnits>(point.As2DPoint())));
}
template <typename TargetUnits, typename SourceUnits>
static Maybe<gfx::PointTyped<TargetUnits>> UntransformVector(const gfx::Matrix4x4& aTransform,
                                                    const gfx::PointTyped<SourceUnits>& aVector,
                                                    const gfx::PointTyped<SourceUnits>& aAnchor) {
  gfx::Point4D point = aTransform.ProjectPoint(aAnchor.ToUnknownPoint() + aVector.ToUnknownPoint()) 
    - aTransform.ProjectPoint(aAnchor.ToUnknownPoint());
  if (!point.HasPositiveWCoord()){
    return Nothing();
  }
  return Some(ViewAs<TargetUnits>(point.As2DPoint()));
}

} // namespace mozilla

#endif
