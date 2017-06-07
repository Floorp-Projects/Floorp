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
                                             WebRenderLayer* aLayer,
                                             const Maybe<gfx::Matrix4x4>& aTransform,
                                             const nsTArray<WrFilterOp>& aFilters)
  : mBuilder(&aBuilder)
{
  WrRect scBounds = aParentSC.ToRelativeWrRect(aLayer->BoundsForStackingContext());
  Layer* layer = aLayer->GetLayer();
  mTransform = aTransform.valueOr(layer->GetTransform());

  float opacity = 1.0f;
  mBuilder->PushStackingContext(scBounds, 0, &opacity,
                                mTransform.IsIdentity() ? nullptr : &mTransform,
                                wr::ToWrMixBlendMode(layer->GetMixBlendMode()),
                                aFilters);
  mOrigin = aLayer->Bounds().TopLeft();
}

StackingContextHelper::StackingContextHelper(const StackingContextHelper& aParentSC,
                                             wr::DisplayListBuilder& aBuilder,
                                             WebRenderLayer* aLayer,
                                             uint64_t aAnimationsId,
                                             float* aOpacityPtr,
                                             gfx::Matrix4x4* aTransformPtr,
                                             const nsTArray<WrFilterOp>& aFilters)
  : mBuilder(&aBuilder)
{
  WrRect scBounds = aParentSC.ToRelativeWrRect(aLayer->BoundsForStackingContext());
  if (aTransformPtr) {
    mTransform = *aTransformPtr;
  }

  mBuilder->PushStackingContext(scBounds,
                                aAnimationsId,
                                aOpacityPtr,
                                aTransformPtr,
                                wr::ToWrMixBlendMode(aLayer->GetLayer()->GetMixBlendMode()),
                                aFilters);
  mOrigin = aLayer->Bounds().TopLeft();
}

StackingContextHelper::~StackingContextHelper()
{
  if (mBuilder) {
    mBuilder->PopStackingContext();
  }
}

WrRect
StackingContextHelper::ToRelativeWrRect(const LayerRect& aRect) const
{
  return wr::ToWrRect(aRect - mOrigin);
}

WrRect
StackingContextHelper::ToRelativeWrRect(const LayoutDeviceRect& aRect) const
{
  return wr::ToWrRect(ViewAs<LayerPixel>(aRect, PixelCastJustification::WebRenderHasUnitResolution) - mOrigin);
}

WrPoint
StackingContextHelper::ToRelativeWrPoint(const LayerPoint& aPoint) const
{
  return wr::ToWrPoint(aPoint - mOrigin);
}

WrRect
StackingContextHelper::ToRelativeWrRectRounded(const LayoutDeviceRect& aRect) const
{
  return wr::ToWrRect(RoundedToInt(ViewAs<LayerPixel>(aRect, PixelCastJustification::WebRenderHasUnitResolution) - mOrigin));
}

gfx::Matrix4x4
StackingContextHelper::TransformToRoot() const
{
  gfx::Matrix4x4 inv = mTransform.Inverse();
  inv.PostTranslate(-mOrigin.x, -mOrigin.y, 0);
  return inv;
}

} // namespace layers
} // namespace mozilla
