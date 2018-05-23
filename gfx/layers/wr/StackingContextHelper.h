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

class nsDisplayTransform;

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
                        const LayoutDeviceRect& aBounds = LayoutDeviceRect(),
                        const gfx::Matrix4x4* aBoundTransform = nullptr,
                        const wr::WrAnimationProperty* aAnimation = nullptr,
                        const float* aOpacityPtr = nullptr,
                        const gfx::Matrix4x4* aTransformPtr = nullptr,
                        const gfx::Matrix4x4* aPerspectivePtr = nullptr,
                        const gfx::CompositionOp& aMixBlendMode = gfx::CompositionOp::OP_OVER,
                        bool aBackfaceVisible = true,
                        bool aIsPreserve3D = false,
                        const Maybe<nsDisplayTransform*>& aDeferredTransformItem = Nothing(),
                        const wr::WrClipId* aClipNodeId = nullptr,
                        bool aRasterizeLocally = false);
  // This version of the constructor should only be used at the root level
  // of the tree, so that we have a StackingContextHelper to pass down into
  // the RenderLayer traversal, but don't actually want it to push a stacking
  // context on the display list builder.
  StackingContextHelper();

  // Pops the stacking context, if one was pushed during the constructor.
  ~StackingContextHelper();

  // Export the inherited scale
  gfx::Size GetInheritedScale() const { return mScale; }

  const gfx::Matrix& GetInheritedTransform() const
  {
    return mInheritedTransform;
  }

  const gfx::Matrix& GetSnappingSurfaceTransform() const
  {
    return mSnappingSurfaceTransform;
  }

  const Maybe<nsDisplayTransform*>& GetDeferredTransformItem() const;

  bool AffectsClipPositioning() const { return mAffectsClipPositioning; }
  Maybe<wr::WrClipId> ReferenceFrameId() const { return mReferenceFrameId; }

private:
  wr::DisplayListBuilder* mBuilder;
  gfx::Size mScale;
  gfx::Matrix mInheritedTransform;

  // The "snapping surface" defines the space that we want to snap in.
  // You can think of it as the nearest physical surface.
  // Animated transforms create a new snapping surface, so that changes to their transform don't affect the snapping of their contents.
  // Non-animated transforms do *not* create a new snapping surface,
  // so that for example the existence of a non-animated identity transform does not affect snapping.
  gfx::Matrix mSnappingSurfaceTransform;
  bool mAffectsClipPositioning;
  Maybe<wr::WrClipId> mReferenceFrameId;
  Maybe<nsDisplayTransform*> mDeferredTransformItem;
  bool mIsPreserve3D;
  bool mRasterizeLocally;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_STACKINGCONTEXTHELPER_H */
