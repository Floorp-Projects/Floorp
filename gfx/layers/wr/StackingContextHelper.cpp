/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
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
{
  // mOrigin remains at 0,0
}

StackingContextHelper::StackingContextHelper(const StackingContextHelper& aParentSC,
                                             wr::DisplayListBuilder& aBuilder,
                                             const nsTArray<wr::WrFilterOp>& aFilters,
                                             const gfx::Matrix4x4* aBoundTransform,
                                             uint64_t aAnimationsId,
                                             float* aOpacityPtr,
                                             gfx::Matrix4x4* aTransformPtr,
                                             gfx::Matrix4x4* aPerspectivePtr,
                                             const gfx::CompositionOp& aMixBlendMode,
                                             bool aBackfaceVisible,
                                             bool aIsPreserve3D)
  : mBuilder(&aBuilder)
  , mScale(1.0f, 1.0f)
{
  if (aTransformPtr) {
    mTransform = *aTransformPtr;
  }

  // Compute scale for fallback rendering.
  gfx::Matrix transform2d;
  if (aBoundTransform && aBoundTransform->CanDraw2D(&transform2d)) {
    mScale = transform2d.ScaleFactors(true) * aParentSC.mScale;
  }

  mBuilder->PushStackingContext(wr::LayoutRect(),
                                aAnimationsId,
                                aOpacityPtr,
                                aTransformPtr,
                                aIsPreserve3D ? wr::TransformStyle::Preserve3D : wr::TransformStyle::Flat,
                                aPerspectivePtr,
                                wr::ToMixBlendMode(aMixBlendMode),
                                aFilters,
                                aBackfaceVisible);
}

StackingContextHelper::~StackingContextHelper()
{
  if (mBuilder) {
    mBuilder->PopStackingContext();
  }
}

void
StackingContextHelper::AdjustOrigin(const LayerPoint& aDelta)
{
  mOrigin += aDelta;
}

wr::LayoutRect
StackingContextHelper::ToRelativeLayoutRect(const LayerRect& aRect) const
{
  return wr::ToLayoutRect(RoundedToInt(aRect - mOrigin));
}

wr::LayoutRect
StackingContextHelper::ToRelativeLayoutRect(const LayoutDeviceRect& aRect) const
{
  return wr::ToLayoutRect(RoundedToInt(ViewAs<LayerPixel>(aRect,
                                                          PixelCastJustification::WebRenderHasUnitResolution) - mOrigin));
}

} // namespace layers
} // namespace mozilla
