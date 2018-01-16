/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_STACKINGCONTEXTHELPER_H
#define GFX_STACKINGCONTEXTHELPER_H

#include "mozilla/Attributes.h"
#include "mozilla/gfx/MatrixFwd.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "Units.h"

class nsDisplayListBuilder;
class nsDisplayItem;
class nsDisplayList;

namespace mozilla {
namespace layers {

/**
 * This is a helper class that pushes/pops a stacking context, and manages
 * some of the coordinate space transformations needed.
 */
class MOZ_RAII StackingContextHelper
{
public:
  StackingContextHelper(const StackingContextHelper& aParentSC,
                        wr::DisplayListBuilder& aBuilder,
                        const nsTArray<wr::WrFilterOp>& aFilters = nsTArray<wr::WrFilterOp>(),
                        const gfx::Matrix4x4* aBoundTransform = nullptr,
                        const wr::WrAnimationProperty* aAnimation = nullptr,
                        float* aOpacityPtr = nullptr,
                        gfx::Matrix4x4* aTransformPtr = nullptr,
                        gfx::Matrix4x4* aPerspectivePtr = nullptr,
                        const gfx::CompositionOp& aMixBlendMode = gfx::CompositionOp::OP_OVER,
                        bool aBackfaceVisible = true,
                        bool aIsPreserve3D = false);
  // This version of the constructor should only be used at the root level
  // of the tree, so that we have a StackingContextHelper to pass down into
  // the RenderLayer traversal, but don't actually want it to push a stacking
  // context on the display list builder.
  StackingContextHelper();

  // Pops the stacking context, if one was pushed during the constructor.
  ~StackingContextHelper();

  // When this StackingContextHelper is in scope, this function can be used
  // to convert a rect from the layer system's coordinate space to a LayoutRect
  // that is relative to the stacking context. This is useful because most
  // things that are pushed inside the stacking context need to be relative
  // to the stacking context.
  // We allow passing in a LayoutDeviceRect for convenience because in a lot of
  // cases with WebRender display item generate the layout device space is the
  // same as the layer space. (TODO: try to make this more explicit somehow).
  // We also round the rectangle to ints after transforming since the output
  // is the final destination rect.
  wr::LayoutRect ToRelativeLayoutRect(const LayoutDeviceRect& aRect) const;
  // Same but for points
  wr::LayoutPoint ToRelativeLayoutPoint(const LayoutDevicePoint& aPoint) const
  {
    return wr::ToLayoutPoint(aPoint);
  }


  // Export the inherited scale
  gfx::Size GetInheritedScale() const { return mScale; }

  const gfx::Matrix& GetInheritedTransform() const
  {
    return mInheritedTransform;
  }

  bool IsBackfaceVisible() const { return mTransform.IsBackfaceVisible(); }
  bool IsReferenceFrame() const { return !mTransform.IsIdentity(); }

private:
  wr::DisplayListBuilder* mBuilder;
  gfx::Matrix4x4 mTransform;
  gfx::Size mScale;
  gfx::Matrix mInheritedTransform;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_STACKINGCONTEXTHELPER_H */
