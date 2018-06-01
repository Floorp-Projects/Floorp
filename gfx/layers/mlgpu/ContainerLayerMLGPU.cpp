/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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
 : ContainerLayer(aManager, nullptr),
   LayerMLGPU(aManager),
   mInvalidateEntireSurface(false),
   mSurfaceCopyNeeded(false)
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
  mView = nullptr;

  if (!UseIntermediateSurface()) {
    // Set this so we invalidate the entire cached render target (if any)
    // if our container uses an intermediate surface again later.
    mInvalidateEntireSurface = true;
    return true;
  }

  if ((!mRenderTarget || mChildrenChanged) &&
      gfxPrefs::AdvancedLayersEnableContainerResizing())
  {
    // Try to compute a more accurate visible region.
    AL_LOG("Computing new surface size for container %p:\n", GetLayer());

    Maybe<IntRect> bounds = ComputeIntermediateSurfaceBounds();
    if (bounds) {
      LayerIntRegion region = std::move(GetShadowVisibleRegion());
      region.AndWith(LayerIntRect::FromUnknownRect(bounds.value()));
      AL_LOG("  computed bounds: %s\n", Stringify(bounds.value()).c_str());
      AL_LOG("  new visible: %s\n", Stringify(region).c_str());

      SetShadowVisibleRegion(std::move(region));
    }
  }
  mChildrenChanged = false;

  mTargetOffset = GetIntermediateSurfaceRect().TopLeft().ToUnknownPoint();
  mTargetSize = GetIntermediateSurfaceRect().Size().ToUnknownSize();

  if (mRenderTarget && mRenderTarget->GetSize() != mTargetSize) {
    mRenderTarget = nullptr;
  }

  // Note that if a surface copy is needed, we always redraw the
  // whole surface (on-demand). This is a rare case - the old
  // Compositor already does this - and it saves us having to
  // do much more complicated invalidation.
  bool surfaceCopyNeeded = false;
  DefaultComputeSupportsComponentAlphaChildren(&surfaceCopyNeeded);
  if (surfaceCopyNeeded != mSurfaceCopyNeeded ||
      surfaceCopyNeeded)
  {
    mInvalidateEntireSurface = true;
  }
  mSurfaceCopyNeeded = surfaceCopyNeeded;

  gfx::IntRect viewport(gfx::IntPoint(0, 0), mTargetSize);
  if (!mRenderTarget ||
      !gfxPrefs::AdvancedLayersUseInvalidation() ||
      mInvalidateEntireSurface)
  {
    // Fine-grained invalidation is disabled, invalidate everything.
    mInvalidRect = viewport;
  } else {
    // Clamp the invalid rect to the viewport.
    mInvalidRect -= mTargetOffset;
    mInvalidRect = mInvalidRect.Intersect(viewport);
  }

  mInvalidateEntireSurface = false;
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
ContainerLayerMLGPU::SetInvalidCompositeRect(const gfx::IntRect* aRect)
{
  // For simplicity we only track the bounds of the invalid area, since regions
  // are expensive.
  //
  // Note we add the bounds to the invalid rect from the last frame, since we
  // only clear the area that we actually paint. If this overflows we use the
  // last render target size, since if that changes we'll invalidate everything
  // anyway.
  if (aRect) {
    if (Maybe<gfx::IntRect> result = mInvalidRect.SafeUnion(*aRect)) {
      mInvalidRect = result.value();
    } else {
      mInvalidateEntireSurface = true;
    }
  } else {
    mInvalidateEntireSurface = true;
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

const LayerIntRegion&
ContainerLayerMLGPU::GetShadowVisibleRegion()
{
  if (!UseIntermediateSurface()) {
    RecomputeShadowVisibleRegionFromChildren();
  }

  return mShadowVisibleRegion;
}

const LayerIntRegion&
RefLayerMLGPU::GetShadowVisibleRegion()
{
  if (!UseIntermediateSurface()) {
    RecomputeShadowVisibleRegionFromChildren();
  }

  return mShadowVisibleRegion;
}

} // namespace layers
} // namespace mozilla
