/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContainerLayerMLGPU.h"
#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "LayerManagerMLGPU.h"
#include "MLGDevice.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Types.h"
#include "UnitTransforms.h"
#include "UtilityMLGPU.h"

namespace mozilla {
namespace layers {

using namespace gfx;

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

  if ((!mRenderTarget || mChildrenChanged) &&
      gfxPrefs::AdvancedLayersEnableContainerResizing())
  {
    // Try to compute a more accurate visible region.
    AL_LOG("Computing new surface size for container %p:\n", GetLayer());

    Maybe<IntRect> bounds = ComputeIntermediateSurfaceBounds();
    if (bounds) {
      LayerIntRegion region = Move(GetShadowVisibleRegion());
      region.AndWith(LayerIntRect::FromUnknownRect(bounds.value()));
      AL_LOG("  computed bounds: %s\n", Stringify(bounds.value()).c_str());
      AL_LOG("  new visible: %s\n", Stringify(region).c_str());

      SetShadowVisibleRegion(Move(region));
    }
  }
  mChildrenChanged = false;

  mTargetOffset = GetIntermediateSurfaceRect().TopLeft().ToUnknownPoint();
  mTargetSize = GetIntermediateSurfaceRect().Size().ToUnknownSize();

  if (mRenderTarget && mRenderTarget->GetSize() != mTargetSize) {
    mRenderTarget = nullptr;
  }

  gfx::IntRect viewport(gfx::IntPoint(0, 0), mTargetSize);
  if (!mRenderTarget || !gfxPrefs::AdvancedLayersUseInvalidation()) {
    // Fine-grained invalidation is disabled, invalidate everything.
    mInvalidRect = viewport;
  } else {
    // Clamp the invalid rect to the viewport.
    mInvalidRect = mInvalidRect.Intersect(viewport);
  }
  return true;
}

static IntRect
GetTransformedBounds(Layer* aLayer)
{
  IntRect bounds = aLayer->GetLocalVisibleRegion().GetBounds().ToUnknownRect();
  if (bounds.IsEmpty()) {
    return bounds;
  }

  const Matrix4x4& transform = aLayer->GetEffectiveTransform();
  Rect rect = transform.TransformAndClipBounds(Rect(bounds), Rect::MaxIntRect());
  rect.RoundOut();
  rect.ToIntRect(&bounds);
  return bounds;
}

static Maybe<IntRect>
FindVisibleBounds(Layer* aLayer, const Maybe<RenderTargetIntRect>& aClip)
{
  AL_LOG("  visiting child %p\n", aLayer);
  AL_LOG_IF(aClip, "  parent clip: %s\n", Stringify(aClip.value()).c_str());

  ContainerLayer* container = aLayer->AsContainerLayer();
  if (container && !container->UseIntermediateSurface()) {
    Maybe<IntRect> accumulated = Some(IntRect());

    // Traverse children.
    for (Layer* child = container->GetFirstChild(); child; child = child->GetNextSibling()) {
      Maybe<RenderTargetIntRect> clip = aClip;
      if (const Maybe<ParentLayerIntRect>& childClip = child->AsHostLayer()->GetShadowClipRect()) {
        RenderTargetIntRect rtChildClip =
          TransformBy(ViewAs<ParentLayerToRenderTargetMatrix4x4>(
                        aLayer->GetEffectiveTransform(),
                        PixelCastJustification::RenderTargetIsParentLayerForRoot),
                      childClip.value());
        clip = IntersectMaybeRects(clip, Some(rtChildClip));
        AL_LOG("    target clip: %s\n", Stringify(rtChildClip).c_str());
        AL_LOG_IF(clip, "    full clip: %s\n", Stringify(clip.value()).c_str());
      }

      Maybe<IntRect> childBounds = FindVisibleBounds(child, clip);
      if (!childBounds) {
        return Nothing();
      }

      accumulated = accumulated->SafeUnion(childBounds.value());
      if (!accumulated) {
        return Nothing();
      }
    }
    return accumulated;
  }

  IntRect bounds = GetTransformedBounds(aLayer);
  AL_LOG("    layer bounds: %s\n", Stringify(bounds).c_str());

  if (aClip) {
    bounds = bounds.Intersect(aClip.value().ToUnknownRect());
    AL_LOG("    clipped bounds: %s\n", Stringify(bounds).c_str());
  }
  return Some(bounds);
}

Maybe<IntRect>
ContainerLayerMLGPU::ComputeIntermediateSurfaceBounds()
{
  Maybe<IntRect> bounds = Some(IntRect());
  for (Layer* child = GetFirstChild(); child; child = child->GetNextSibling()) {
    Maybe<RenderTargetIntRect> clip =
       ViewAs<RenderTargetPixel>(child->AsHostLayer()->GetShadowClipRect(),
                                 PixelCastJustification::RenderTargetIsParentLayerForRoot);
    Maybe<IntRect> childBounds = FindVisibleBounds(child, clip);
    if (!childBounds) {
      return Nothing();
    }

    bounds = bounds->SafeUnion(childBounds.value());
    if (!bounds) {
      return Nothing();
    }
  }
  return bounds;
}

void
ContainerLayerMLGPU::OnLayerManagerChange(LayerManagerMLGPU* aManager)
{
  ClearCachedResources();
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
  gfx::IntRect bounds = aRect;
  bounds.MoveBy(-GetTargetOffset());

  // Note we add the bounds to the invalid rect from the last frame, since we
  // only clear the area that we actually paint.
  if (Maybe<gfx::IntRect> result = mInvalidRect.SafeUnion(bounds)) {
    mInvalidRect = result.value();
  } else {
    mInvalidRect = gfx::IntRect(gfx::IntPoint(0, 0), GetTargetSize());
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
  if (GetMixBlendMode() != gfx::CompositionOp::OP_OVER) {
    // We need to read from what's underneath us, so we consider our content to
    // be not opaque.
    return false;
  }
  return LayerMLGPU::IsContentOpaque();
}

} // namespace layers
} // namespace mozilla
