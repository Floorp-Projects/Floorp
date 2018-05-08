/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/StackingContextHelper.h"

#include "UnitTransforms.h"
#include "nsDisplayList.h"

namespace mozilla {
namespace layers {

StackingContextHelper::StackingContextHelper()
  : mBuilder(nullptr)
  , mScale(1.0f, 1.0f)
  , mAffectsClipPositioning(false)
  , mIsPreserve3D(false)
  , mRasterizeLocally(false)
{
  // mOrigin remains at 0,0
}

StackingContextHelper::StackingContextHelper(const StackingContextHelper& aParentSC,
                                             wr::DisplayListBuilder& aBuilder,
                                             const nsTArray<wr::WrFilterOp>& aFilters,
                                             const LayoutDeviceRect& aBounds,
                                             const gfx::Matrix4x4* aBoundTransform,
                                             const wr::WrAnimationProperty* aAnimation,
                                             float* aOpacityPtr,
                                             gfx::Matrix4x4* aTransformPtr,
                                             gfx::Matrix4x4* aPerspectivePtr,
                                             const gfx::CompositionOp& aMixBlendMode,
                                             bool aBackfaceVisible,
                                             bool aIsPreserve3D,
                                             const Maybe<gfx::Matrix4x4>& aTransformForScrollData,
                                             const wr::WrClipId* aClipNodeId,
                                             bool aRasterizeLocally)
  : mBuilder(&aBuilder)
  , mScale(1.0f, 1.0f)
  , mTransformForScrollData(aTransformForScrollData)
  , mIsPreserve3D(aIsPreserve3D)
  , mRasterizeLocally(aRasterizeLocally || aParentSC.mRasterizeLocally)
{
  // Compute scale for fallback rendering. We don't try to guess a scale for 3d
  // transformed items
  gfx::Matrix transform2d;
  if (aBoundTransform && aBoundTransform->CanDraw2D(&transform2d)
      && !aPerspectivePtr
      && !aParentSC.mIsPreserve3D) {
    mInheritedTransform = transform2d * aParentSC.mInheritedTransform;
    mScale = mInheritedTransform.ScaleFactors(true);
  } else {
    mInheritedTransform = aParentSC.mInheritedTransform;
    mScale = aParentSC.mScale;
  }

  auto rasterSpace = mRasterizeLocally
    ? wr::GlyphRasterSpace::Local(std::max(mScale.width, mScale.height))
    : wr::GlyphRasterSpace::Screen();

  mReferenceFrameId = mBuilder->PushStackingContext(
          wr::ToLayoutRect(aBounds),
          aClipNodeId,
          aAnimation,
          aOpacityPtr,
          aTransformPtr,
          aIsPreserve3D ? wr::TransformStyle::Preserve3D : wr::TransformStyle::Flat,
          aPerspectivePtr,
          wr::ToMixBlendMode(aMixBlendMode),
          aFilters,
          aBackfaceVisible,
          rasterSpace);

  mAffectsClipPositioning = mReferenceFrameId.isSome() ||
      (aBounds.TopLeft() != LayoutDevicePoint());
}

StackingContextHelper::~StackingContextHelper()
{
  if (mBuilder) {
    mBuilder->PopStackingContext();
  }
}

const Maybe<gfx::Matrix4x4>&
StackingContextHelper::GetTransformForScrollData() const
{
  return mTransformForScrollData;
}

} // namespace layers
} // namespace mozilla
