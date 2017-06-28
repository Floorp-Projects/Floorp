/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerManagerMLGPU.h"
#include "RenderPassMLGPU.h"
#include "RenderViewMLGPU.h"
#include "FrameBuilder.h"
#include "mozilla/layers/ImageHost.h"

namespace mozilla {
namespace layers {

using namespace gfx;

uint64_t LayerMLGPU::sFrameKey = 0;

LayerMLGPU::LayerMLGPU(LayerManagerMLGPU* aManager)
 : HostLayer(aManager),
   mFrameKey(0),
   mPrepared(false)
{
}

/* static */ void
LayerMLGPU::BeginFrame()
{
  sFrameKey++;
}

LayerManagerMLGPU*
LayerMLGPU::GetManager()
{
  return static_cast<LayerManagerMLGPU*>(mCompositorManager);
}

bool
LayerMLGPU::PrepareToRender(FrameBuilder* aBuilder, const RenderTargetIntRect& aClipRect)
{
  if (mFrameKey == sFrameKey) {
    return mPrepared;
  }
  mFrameKey = sFrameKey;
  mPrepared = false;

  Layer* layer = GetLayer();

  // Only container layers may have mixed blend modes.
  MOZ_ASSERT_IF(layer->GetMixBlendMode() != CompositionOp::OP_OVER,
                layer->GetType() == Layer::TYPE_CONTAINER);

  mComputedClipRect = aClipRect;

  if (layer->HasMaskLayers()) {
    mMask = aBuilder->AddMaskOperation(this);
  } else {
    mMask = nullptr;
  }

  if (!OnPrepareToRender(aBuilder)) {
    return false;
  }

  mComputedOpacity = layer->GetEffectiveOpacity();
  mPrepared = true;
  return true;
}

void
LayerMLGPU::AssignToView(FrameBuilder* aBuilder,
                         RenderViewMLGPU* aView,
                         Maybe<gfx::Polygon>&& aGeometry)
{
  AddBoundsToView(aBuilder, aView, Move(aGeometry));
}

void
LayerMLGPU::AddBoundsToView(FrameBuilder* aBuilder,
                            RenderViewMLGPU* aView,
                            Maybe<gfx::Polygon>&& aGeometry)
{
  IntRect bounds = GetClippedBoundingBox(aView, aGeometry);
  aView->AddItem(this, bounds, Move(aGeometry));
}

IntRect
LayerMLGPU::GetClippedBoundingBox(RenderViewMLGPU* aView,
                                  const Maybe<gfx::Polygon>& aGeometry)
{
  MOZ_ASSERT(IsPrepared());

  Layer* layer = GetLayer();
  const Matrix4x4& transform = layer->GetEffectiveTransform();

  Rect rect = aGeometry
              ? aGeometry->BoundingBox()
              : Rect(layer->GetLocalVisibleRegion().GetBounds().ToUnknownRect());
  rect = transform.TransformBounds(rect);
  rect.MoveBy(-aView->GetTargetOffset());
  rect = rect.Intersect(Rect(mComputedClipRect.ToUnknownRect()));

  IntRect bounds;
  rect.RoundOut();
  rect.ToIntRect(&bounds);
  return bounds;
}

void
LayerMLGPU::MarkPrepared()
{
  mFrameKey = sFrameKey;
  mPrepared = true;
}

bool
LayerMLGPU::IsContentOpaque()
{
  return GetLayer()->IsOpaque();
}

void
LayerMLGPU::SetRegionToRender(LayerIntRegion&& aRegion)
{
  SetShadowVisibleRegion(Move(aRegion));
}

void
LayerMLGPU::SetLayerManager(HostLayerManager* aManager)
{
  LayerManagerMLGPU* manager = aManager->AsLayerManagerMLGPU();
  MOZ_RELEASE_ASSERT(manager);

  HostLayer::SetLayerManager(aManager);
  GetLayer()->SetManager(manager, this);

  if (CompositableHost* host = GetCompositableHost()) {
    host->SetTextureSourceProvider(manager->GetTextureSourceProvider());
  }

  OnLayerManagerChange(manager);
}

RefLayerMLGPU::RefLayerMLGPU(LayerManagerMLGPU* aManager)
  : RefLayer(aManager, static_cast<HostLayer*>(this))
  , LayerMLGPU(aManager)
{
}

RefLayerMLGPU::~RefLayerMLGPU()
{
}

ColorLayerMLGPU::ColorLayerMLGPU(LayerManagerMLGPU* aManager)
  : ColorLayer(aManager, static_cast<HostLayer*>(this))
  , LayerMLGPU(aManager)
{
}

ColorLayerMLGPU::~ColorLayerMLGPU()
{
}

} // namespace layers
} // namespace mozilla
