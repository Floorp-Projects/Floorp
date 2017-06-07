/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_STACKINGCONTEXTHELPER_H
#define GFX_STACKINGCONTEXTHELPER_H

#include "mozilla/Attributes.h"
#include "mozilla/gfx/MatrixFwd.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "Units.h"

namespace mozilla {
namespace layers {

class WebRenderLayer;

/**
 * This is a helper class that pushes/pops a stacking context, and manages
 * some of the coordinate space transformations needed.
 */
class MOZ_RAII StackingContextHelper
{
public:
  // Pushes a stacking context onto the provided DisplayListBuilder. It uses
  // the transform if provided, otherwise takes the transform from the layer.
  // It also takes the mix-blend-mode and bounds from the layer, and uses 1.0
  // for the opacity.
  StackingContextHelper(const StackingContextHelper& aParentSC,
                        wr::DisplayListBuilder& aBuilder,
                        WebRenderLayer* aLayer,
                        const Maybe<gfx::Matrix4x4>& aTransform = Nothing(),
                        const nsTArray<WrFilterOp>& aFilters = nsTArray<WrFilterOp>());
  // Alternate constructor which invokes the version of PushStackingContext
  // for animations.
  StackingContextHelper(const StackingContextHelper& aParentSC,
                        wr::DisplayListBuilder& aBuilder,
                        WebRenderLayer* aLayer,
                        uint64_t aAnimationsId,
                        float* aOpacityPtr,
                        gfx::Matrix4x4* aTransformPtr,
                        const nsTArray<WrFilterOp>& aFilters = nsTArray<WrFilterOp>());
  // This version of the constructor should only be used at the root level
  // of the tree, so that we have a StackingContextHelper to pass down into
  // the RenderLayer traversal, but don't actually want it to push a stacking
  // context on the display list builder.
  StackingContextHelper();

  // Pops the stacking context, if one was pushed during the constructor.
  ~StackingContextHelper();

  // When this StackingContextHelper is in scope, this function can be used
  // to convert a rect from the layer system's coordinate space to a WrRect
  // that is relative to the stacking context. This is useful because most
  // things that are pushed inside the stacking context need to be relative
  // to the stacking context.
  // We allow passing in a LayoutDeviceRect for convenience because in a lot of
  // cases with WebRender display item generate the layout device space is the
  // same as the layer space. (TODO: try to make this more explicit somehow).
  WrRect ToRelativeWrRect(const LayerRect& aRect) const;
  WrRect ToRelativeWrRect(const LayoutDeviceRect& aRect) const;
  // Same but for points
  WrPoint ToRelativeWrPoint(const LayerPoint& aPoint) const;
  // Same but rounds the rectangle to ints after transforming.
  WrRect ToRelativeWrRectRounded(const LayoutDeviceRect& aRect) const;

  // Produce a transform that converts points from the coordinate space of this
  // stacking context to the coordinate space of the root of the layer tree.
  gfx::Matrix4x4 TransformToRoot() const;

private:
  wr::DisplayListBuilder* mBuilder;
  LayerPoint mOrigin;
  gfx::Matrix4x4 mTransform;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_STACKINGCONTEXTHELPER_H */
