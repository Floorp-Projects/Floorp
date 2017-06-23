/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContainerLayerMLGPU.h"
#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "MLGDevice.h"

namespace mozilla {
namespace layers {

ContainerLayerMLGPU::ContainerLayerMLGPU(LayerManagerMLGPU* aManager)
  : ContainerLayer(aManager, nullptr)
  , LayerMLGPU(aManager)
{
}

ContainerLayerMLGPU::~ContainerLayerMLGPU()
{
  while (mFirstChild) {
    RemoveChild(mFirstChild);
  }
}

bool
ContainerLayerMLGPU::OnPrepareToRender(FrameBuilder* aBuilder)
{
  if (!UseIntermediateSurface()) {
    return true;
  }

  mTargetOffset = GetIntermediateSurfaceRect().TopLeft().ToUnknownPoint();
  mTargetSize = GetIntermediateSurfaceRect().Size().ToUnknownSize();

  if (mRenderTarget && mRenderTarget->GetSize() != mTargetSize) {
    mRenderTarget = nullptr;
  }

  IntRect viewport(IntPoint(0, 0), mTargetSize);
  if (!mRenderTarget || !gfxPrefs::AdvancedLayersUseInvalidation()) {
    // Fine-grained invalidation is disabled, invalidate everything.
    mInvalidRect = viewport;
  } else {
    // Clamp the invalid rect to the viewport.
    mInvalidRect = mInvalidRect.Intersect(viewport);
  }
  return true;
}

RefPtr<MLGRenderTarget>
ContainerLayerMLGPU::UpdateRenderTarget(MLGDevice* aDevice, MLGRenderTargetFlags aFlags)
{
  if (mRenderTarget) {
    return mRenderTarget;
  }

  mRenderTarget = aDevice->CreateRenderTarget(mTargetSize, aFlags);
  if (!mRenderTarget) {
    gfxWarning() << "Failed to create an intermediate render target for ContainerLayer";
    return nullptr;
  }

  return mRenderTarget;
}

void
ContainerLayerMLGPU::SetInvalidCompositeRect(const gfx::IntRect& aRect)
{
  // For simplicity we only track the bounds of the invalid area, since regions
  // are expensive. We can adjust this in the future if needed.
  IntRect bounds = aRect;
  bounds.MoveBy(-GetTargetOffset());

  // Note we add the bounds to the invalid rect from the last frame, since we
  // only clear the area that we actually paint.
  if (Maybe<IntRect> result = mInvalidRect.SafeUnion(bounds)) {
    mInvalidRect = result.value();
  } else {
    mInvalidRect = IntRect(IntPoint(0, 0), GetTargetSize());
  }
}

void
ContainerLayerMLGPU::ClearCachedResources()
{
  mRenderTarget = nullptr;
}

bool
ContainerLayerMLGPU::IsContentOpaque()
{
  if (GetMixBlendMode() != CompositionOp::OP_OVER) {
    // We need to read from what's underneath us, so we consider our content to
    // be not opaque.
    return false;
  }
  return LayerMLGPU::IsContentOpaque();
}

} // namespace layers
} // namespace mozilla
