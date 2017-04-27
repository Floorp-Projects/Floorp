/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/StackingContextHelper.h"

#include "mozilla/layers/WebRenderLayer.h"

namespace mozilla {
namespace layers {

StackingContextHelper::StackingContextHelper(wr::DisplayListBuilder& aBuilder,
                                             WebRenderLayer* aLayer,
                                             const Maybe<gfx::Matrix4x4>& aTransform)
  : mBuilder(&aBuilder)
{
  LayerRect scBounds = aLayer->RelativeToParent(aLayer->BoundsForStackingContext());
  Layer* layer = aLayer->GetLayer();
  gfx::Matrix4x4 transform = aTransform.valueOr(layer->GetTransform());
  mBuilder->PushStackingContext(wr::ToWrRect(scBounds),
                                1.0f,
                                transform,
                                wr::ToWrMixBlendMode(layer->GetMixBlendMode()));
  mOrigin = aLayer->Bounds().TopLeft();
}

StackingContextHelper::~StackingContextHelper()
{
  mBuilder->PopStackingContext();
}

WrRect
StackingContextHelper::ToRelativeWrRect(const LayerRect& aRect)
{
  return wr::ToWrRect(aRect - mOrigin);
}

} // namespace layers
} // namespace mozilla
