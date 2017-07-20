/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/StackingContextHelper.h"

#include "mozilla/layers/WebRenderLayer.h"
#include "UnitTransforms.h"

namespace mozilla {
namespace layers {

StackingContextHelper::StackingContextHelper()
  : mBuilder(nullptr)
{
  // mOrigin remains at 0,0
}

StackingContextHelper::StackingContextHelper(const StackingContextHelper& aParentSC,
                                             wr::DisplayListBuilder& aBuilder,
                                             LayerRect aBoundForSC,
                                             LayerPoint aOrigin,
                                             uint64_t aAnimationsId,
                                             float* aOpacityPtr,
                                             gfx::Matrix4x4* aTransformPtr,
                                             const nsTArray<wr::WrFilterOp>& aFilters)
  : mBuilder(&aBuilder)
{
  wr::LayoutRect scBounds = aParentSC.ToRelativeLayoutRect(aBoundForSC);
  if (aTransformPtr) {
    mTransform = *aTransformPtr;
  }

  mBuilder->PushStackingContext(scBounds,
                                aAnimationsId,
                                aOpacityPtr,
                                aTransformPtr,
                                wr::TransformStyle::Flat,
                                // TODO: set correct blend mode.
                                wr::ToMixBlendMode(gfx::CompositionOp::OP_OVER),
                                aFilters);

  mOrigin = aOrigin;
}

StackingContextHelper::StackingContextHelper(const StackingContextHelper& aParentSC,
                                             wr::DisplayListBuilder& aBuilder,
                                             WebRenderLayer* aLayer,
                                             const Maybe<gfx::Matrix4x4>& aTransform,
                                             const nsTArray<wr::WrFilterOp>& aFilters)
  : mBuilder(&aBuilder)
{
  wr::LayoutRect scBounds = aParentSC.ToRelativeLayoutRect(aLayer->BoundsForStackingContext());
  Layer* layer = aLayer->GetLayer();
  mTransform = aTransform.valueOr(layer->GetTransform());

  float opacity = 1.0f;
  mBuilder->PushStackingContext(scBounds, 0, &opacity,
                                mTransform.IsIdentity() ? nullptr : &mTransform,
                                wr::TransformStyle::Flat,
                                wr::ToMixBlendMode(layer->GetMixBlendMode()),
                                aFilters);
  mOrigin = aLayer->Bounds().TopLeft();
}

StackingContextHelper::StackingContextHelper(const StackingContextHelper& aParentSC,
                                             wr::DisplayListBuilder& aBuilder,
                                             WebRenderLayer* aLayer,
                                             uint64_t aAnimationsId,
                                             float* aOpacityPtr,
                                             gfx::Matrix4x4* aTransformPtr,
                                             const nsTArray<wr::WrFilterOp>& aFilters)
  : mBuilder(&aBuilder)
{
  wr::LayoutRect scBounds = aParentSC.ToRelativeLayoutRect(aLayer->BoundsForStackingContext());
  if (aTransformPtr) {
    mTransform = *aTransformPtr;
  }

  mBuilder->PushStackingContext(scBounds,
                                aAnimationsId,
                                aOpacityPtr,
                                aTransformPtr,
                                wr::TransformStyle::Flat,
                                wr::ToMixBlendMode(aLayer->GetLayer()->GetMixBlendMode()),
                                aFilters);
  mOrigin = aLayer->Bounds().TopLeft();
}

StackingContextHelper::~StackingContextHelper()
{
  if (mBuilder) {
    mBuilder->PopStackingContext();
  }
}

wr::LayoutRect
StackingContextHelper::ToRelativeLayoutRect(const LayerRect& aRect) const
{
  return wr::ToLayoutRect(aRect - mOrigin);
}

wr::LayoutRect
StackingContextHelper::ToRelativeLayoutRect(const LayoutDeviceRect& aRect) const
{
  return wr::ToLayoutRect(ViewAs<LayerPixel>(aRect, PixelCastJustification::WebRenderHasUnitResolution) - mOrigin);
}

wr::LayoutPoint
StackingContextHelper::ToRelativeLayoutPoint(const LayerPoint& aPoint) const
{
  return wr::ToLayoutPoint(aPoint - mOrigin);
}

wr::LayoutRect
StackingContextHelper::ToRelativeLayoutRectRounded(const LayoutDeviceRect& aRect) const
{
  return wr::ToLayoutRect(RoundedToInt(ViewAs<LayerPixel>(aRect, PixelCastJustification::WebRenderHasUnitResolution) - mOrigin));
}

} // namespace layers
} // namespace mozilla
