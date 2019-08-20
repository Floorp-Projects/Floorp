/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_UNIT_TRANSFORMS_H_
#define MOZ_UNIT_TRANSFORMS_H_

#include "Units.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/Maybe.h"
#include "nsRegion.h"

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
  // For the root composition size we want to view it as layer pixels in any
  // layer
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
  // reference point as a screen point. The reverse is useful when synthetically
  // created WidgetEvents need to be converted back to InputData.
  LayoutDeviceIsScreenForUntransformedEvent,
  // Similar to LayoutDeviceIsScreenForUntransformedEvent, PBrowser handles
  // some widget/tab dimension information as the OS does -- in screen units.
  LayoutDeviceIsScreenForTabDims,
  // A combination of LayoutDeviceIsScreenForBounds and
  // ScreenIsParentLayerForRoot, which is how we're using it.
  LayoutDeviceIsParentLayerForRCDRSF,
  // Used to treat the product of AsyncTransformComponentMatrix objects
  // as an AsyncTransformMatrix. See the definitions of these matrices in
  // LayersTypes.h for details.
  MultipleAsyncTransforms,
  // We have reason to believe a layer doesn't have a local transform.
  // Should only be used if we've already checked or asserted this.
  NoTransformOnLayer,
  // LayerPixels are ImagePixels
  LayerIsImage,
  // External pixels are the same scale as screen pixels
  ExternalIsScreen,
  // LayerToScreenMatrix is used as LayoutDeviceToLayoutDevice, because
  // out-of-process iframes uses LayoutDevicePixels as the type system-visible
  // type of their top-level event coordinate space even if technically
  // inaccurate.
  ContentProcessIsLayerInUiProcess,
};

template <class TargetUnits, class SourceUnits>
gfx::CoordTyped<TargetUnits> ViewAs(const gfx::CoordTyped<SourceUnits>& aCoord,
                                    PixelCastJustification) {
  return gfx::CoordTyped<TargetUnits>(aCoord.value);
}
template <class TargetUnits, class SourceUnits>
gfx::SizeTyped<TargetUnits> ViewAs(const gfx::SizeTyped<SourceUnits>& aSize,
                                   PixelCastJustification) {
  return gfx::SizeTyped<TargetUnits>(aSize.width, aSize.height);
}
template <class TargetUnits, class SourceUnits>
gfx::IntSizeTyped<TargetUnits> ViewAs(
    const gfx::IntSizeTyped<SourceUnits>& aSize, PixelCastJustification) {
  return gfx::IntSizeTyped<TargetUnits>(aSize.width, aSize.height);
}
template <class TargetUnits, class SourceUnits>
gfx::PointTyped<TargetUnits> ViewAs(const gfx::PointTyped<SourceUnits>& aPoint,
                                    PixelCastJustification) {
  return gfx::PointTyped<TargetUnits>(aPoint.x, aPoint.y);
}
template <class TargetUnits, class SourceUnits>
gfx::IntPointTyped<TargetUnits> ViewAs(
    const gfx::IntPointTyped<SourceUnits>& aPoint, PixelCastJustification) {
  return gfx::IntPointTyped<TargetUnits>(aPoint.x, aPoint.y);
}
template <class TargetUnits, class SourceUnits>
gfx::RectTyped<TargetUnits> ViewAs(const gfx::RectTyped<SourceUnits>& aRect,
                                   PixelCastJustification) {
  return gfx::RectTyped<TargetUnits>(aRect.x, aRect.y, aRect.Width(),
                                     aRect.Height());
}
template <class TargetUnits, class SourceUnits>
gfx::IntRectTyped<TargetUnits> ViewAs(
    const gfx::IntRectTyped<SourceUnits>& aRect, PixelCastJustification) {
  return gfx::IntRectTyped<TargetUnits>(aRect.x, aRect.y, aRect.Width(),
                                        aRect.Height());
}
template <class TargetUnits, class SourceUnits>
gfx::MarginTyped<TargetUnits> ViewAs(
    const gfx::MarginTyped<SourceUnits>& aMargin, PixelCastJustification) {
  return gfx::MarginTyped<TargetUnits>(aMargin.top, aMargin.right,
                                       aMargin.bottom, aMargin.left);
}
template <class TargetUnits, class SourceUnits>
gfx::IntMarginTyped<TargetUnits> ViewAs(
    const gfx::IntMarginTyped<SourceUnits>& aMargin, PixelCastJustification) {
  return gfx::IntMarginTyped<TargetUnits>(aMargin.top, aMargin.right,
                                          aMargin.bottom, aMargin.left);
}
template <class TargetUnits, class SourceUnits>
gfx::IntRegionTyped<TargetUnits> ViewAs(
    const gfx::IntRegionTyped<SourceUnits>& aRegion, PixelCastJustification) {
  return gfx::IntRegionTyped<TargetUnits>::FromUnknownRegion(
      aRegion.ToUnknownRegion());
}
template <class NewTargetUnits, class OldTargetUnits, class SourceUnits>
gfx::ScaleFactor<SourceUnits, NewTargetUnits> ViewTargetAs(
    const gfx::ScaleFactor<SourceUnits, OldTargetUnits>& aScaleFactor,
    PixelCastJustification) {
  return gfx::ScaleFactor<SourceUnits, NewTargetUnits>(aScaleFactor.scale);
}
template <class TargetUnits, class SourceUnits>
Maybe<gfx::IntRectTyped<TargetUnits>> ViewAs(
    const Maybe<gfx::IntRectTyped<SourceUnits>>& aRect,
    PixelCastJustification aJustification) {
  if (aRect.isSome()) {
    return Some(ViewAs<TargetUnits>(aRect.value(), aJustification));
  }
  return Nothing();
}
// Unlike the other functions in this category, these functions take the
// target matrix type, rather than its source and target unit types, as
// the explicit template argument, so an example invocation is:
//    ViewAs<ScreenToLayerMatrix4x4>(otherTypedMatrix, justification)
// The reason is that if it took the source and target unit types as two
// template arguments, there may be some confusion as to which is the
// source and which is the target.
template <class TargetMatrix, class SourceMatrixSourceUnits,
          class SourceMatrixTargetUnits>
TargetMatrix ViewAs(const gfx::Matrix4x4Typed<SourceMatrixSourceUnits,
                                              SourceMatrixTargetUnits>& aMatrix,
                    PixelCastJustification) {
  return TargetMatrix::FromUnknownMatrix(aMatrix.ToUnknownMatrix());
}
template <class TargetMatrix, class SourceMatrixSourceUnits,
          class SourceMatrixTargetUnits>
Maybe<TargetMatrix> ViewAs(
    const Maybe<gfx::Matrix4x4Typed<SourceMatrixSourceUnits,
                                    SourceMatrixTargetUnits>>& aMatrix,
    PixelCastJustification) {
  if (aMatrix.isSome()) {
    return Some(TargetMatrix::FromUnknownMatrix(aMatrix->ToUnknownMatrix()));
  }
  return Nothing();
}

// A non-member overload of ToUnknownMatrix() for use on a Maybe<Matrix>.
// We can't make this a member because we can't inject a member into Maybe.
template <typename SourceUnits, typename TargetUnits>
Maybe<gfx::Matrix4x4> ToUnknownMatrix(
    const Maybe<gfx::Matrix4x4Typed<SourceUnits, TargetUnits>>& aMatrix) {
  if (aMatrix.isSome()) {
    return Some(aMatrix->ToUnknownMatrix());
  }
  return Nothing();
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
  return gfx::RectTyped<TargetUnits>(aRect.x, aRect.y, aRect.Width(),
                                     aRect.Height());
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
  return gfx::IntRectTyped<TargetUnits>(aRect.x, aRect.y, aRect.Width(),
                                        aRect.Height());
}
template <class TargetUnits>
gfx::IntRegionTyped<TargetUnits> ViewAs(const nsIntRegion& aRegion) {
  return gfx::IntRegionTyped<TargetUnits>::FromUnknownRegion(aRegion);
}
// Unlike the other functions in this category, this function takes the
// target matrix type, rather than its source and target unit types, as
// the template argument, so an example invocation is:
//    ViewAs<ScreenToLayerMatrix4x4>(untypedMatrix)
// The reason is that if it took the source and target unit types as two
// template arguments, there may be some confusion as to which is the
// source and which is the target.
template <class TypedMatrix>
TypedMatrix ViewAs(const gfx::Matrix4x4& aMatrix) {
  return TypedMatrix::FromUnknownMatrix(aMatrix);
}

// Convenience functions for transforming an entity from one strongly-typed
// coordinate system to another using the provided transformation matrix.
template <typename TargetUnits, typename SourceUnits>
static gfx::PointTyped<TargetUnits> TransformBy(
    const gfx::Matrix4x4Typed<SourceUnits, TargetUnits>& aTransform,
    const gfx::PointTyped<SourceUnits>& aPoint) {
  return aTransform.TransformPoint(aPoint);
}
template <typename TargetUnits, typename SourceUnits>
static gfx::IntPointTyped<TargetUnits> TransformBy(
    const gfx::Matrix4x4Typed<SourceUnits, TargetUnits>& aTransform,
    const gfx::IntPointTyped<SourceUnits>& aPoint) {
  return RoundedToInt(
      TransformBy(aTransform, gfx::PointTyped<SourceUnits>(aPoint)));
}
template <typename TargetUnits, typename SourceUnits>
static gfx::RectTyped<TargetUnits> TransformBy(
    const gfx::Matrix4x4Typed<SourceUnits, TargetUnits>& aTransform,
    const gfx::RectTyped<SourceUnits>& aRect) {
  return aTransform.TransformBounds(aRect);
}
template <typename TargetUnits, typename SourceUnits>
static gfx::IntRectTyped<TargetUnits> TransformBy(
    const gfx::Matrix4x4Typed<SourceUnits, TargetUnits>& aTransform,
    const gfx::IntRectTyped<SourceUnits>& aRect) {
  return RoundedToInt(
      TransformBy(aTransform, gfx::RectTyped<SourceUnits>(aRect)));
}
template <typename TargetUnits, typename SourceUnits>
static gfx::IntRegionTyped<TargetUnits> TransformBy(
    const gfx::Matrix4x4Typed<SourceUnits, TargetUnits>& aTransform,
    const gfx::IntRegionTyped<SourceUnits>& aRegion) {
  return ViewAs<TargetUnits>(
      aRegion.ToUnknownRegion().Transform(aTransform.ToUnknownMatrix()));
}

// Transform |aVector|, which is anchored at |aAnchor|, by the given transform
// matrix, yielding a point in |TargetUnits|.
// The anchor is necessary because with 3D tranforms, the location of the
// vector can affect the result of the transform.
template <typename TargetUnits, typename SourceUnits>
static gfx::PointTyped<TargetUnits> TransformVector(
    const gfx::Matrix4x4Typed<SourceUnits, TargetUnits>& aTransform,
    const gfx::PointTyped<SourceUnits>& aVector,
    const gfx::PointTyped<SourceUnits>& aAnchor) {
  gfx::PointTyped<TargetUnits> transformedStart =
      TransformBy(aTransform, aAnchor);
  gfx::PointTyped<TargetUnits> transformedEnd =
      TransformBy(aTransform, aAnchor + aVector);
  return transformedEnd - transformedStart;
}

// UntransformBy() and UntransformVector() are like TransformBy() and
// TransformVector(), respectively, but are intended for cases where
// the transformation matrix is the inverse of a 3D projection. When
// using such transforms, the resulting Point4D is only meaningful
// if it has a positive w-coordinate. To handle this, these functions
// return a Maybe object which contains a value if and only if the
// result is meaningful
template <typename TargetUnits, typename SourceUnits>
static Maybe<gfx::PointTyped<TargetUnits>> UntransformBy(
    const gfx::Matrix4x4Typed<SourceUnits, TargetUnits>& aTransform,
    const gfx::PointTyped<SourceUnits>& aPoint) {
  gfx::Point4DTyped<TargetUnits> point = aTransform.ProjectPoint(aPoint);
  if (!point.HasPositiveWCoord()) {
    return Nothing();
  }
  return Some(point.As2DPoint());
}
template <typename TargetUnits, typename SourceUnits>
static Maybe<gfx::IntPointTyped<TargetUnits>> UntransformBy(
    const gfx::Matrix4x4Typed<SourceUnits, TargetUnits>& aTransform,
    const gfx::IntPointTyped<SourceUnits>& aPoint) {
  gfx::PointTyped<SourceUnits> p = aPoint;
  gfx::Point4DTyped<TargetUnits> point = aTransform.ProjectPoint(p);
  if (!point.HasPositiveWCoord()) {
    return Nothing();
  }
  return Some(RoundedToInt(point.As2DPoint()));
}

// The versions of UntransformBy() that take a rectangle also take a clip,
// which represents the bounds within which the target must fall. The
// result of the transform is intersected with this clip, and is considered
// meaningful if the intersection is not empty.
template <typename TargetUnits, typename SourceUnits>
static Maybe<gfx::RectTyped<TargetUnits>> UntransformBy(
    const gfx::Matrix4x4Typed<SourceUnits, TargetUnits>& aTransform,
    const gfx::RectTyped<SourceUnits>& aRect,
    const gfx::RectTyped<TargetUnits>& aClip) {
  gfx::RectTyped<TargetUnits> rect = aTransform.ProjectRectBounds(aRect, aClip);
  if (rect.IsEmpty()) {
    return Nothing();
  }
  return Some(rect);
}
template <typename TargetUnits, typename SourceUnits>
static Maybe<gfx::IntRectTyped<TargetUnits>> UntransformBy(
    const gfx::Matrix4x4Typed<SourceUnits, TargetUnits>& aTransform,
    const gfx::IntRectTyped<SourceUnits>& aRect,
    const gfx::IntRectTyped<TargetUnits>& aClip) {
  gfx::RectTyped<TargetUnits> rect = aTransform.ProjectRectBounds(aRect, aClip);
  if (rect.IsEmpty()) {
    return Nothing();
  }
  return Some(RoundedToInt(rect));
}

template <typename TargetUnits, typename SourceUnits>
static Maybe<gfx::PointTyped<TargetUnits>> UntransformVector(
    const gfx::Matrix4x4Typed<SourceUnits, TargetUnits>& aTransform,
    const gfx::PointTyped<SourceUnits>& aVector,
    const gfx::PointTyped<SourceUnits>& aAnchor) {
  gfx::Point4DTyped<TargetUnits> projectedAnchor =
      aTransform.ProjectPoint(aAnchor);
  gfx::Point4DTyped<TargetUnits> projectedTarget =
      aTransform.ProjectPoint(aAnchor + aVector);
  if (!projectedAnchor.HasPositiveWCoord() ||
      !projectedTarget.HasPositiveWCoord()) {
    return Nothing();
  }
  return Some(projectedTarget.As2DPoint() - projectedAnchor.As2DPoint());
}

}  // namespace mozilla

#endif
