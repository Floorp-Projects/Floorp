/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderViewMLGPU.h"
#include "ContainerLayerMLGPU.h"
#include "FrameBuilder.h"
#include "gfxPrefs.h"
#include "LayersHelpers.h"
#include "LayersLogging.h"
#include "MLGDevice.h"
#include "RenderPassMLGPU.h"
#include "ShaderDefinitionsMLGPU.h"
#include "Units.h"
#include "UnitTransforms.h"
#include "UtilityMLGPU.h"

namespace mozilla {
namespace layers {

using namespace gfx;

RenderViewMLGPU::RenderViewMLGPU(FrameBuilder* aBuilder,
                                 MLGRenderTarget* aTarget,
                                 const nsIntRegion& aInvalidRegion)
 : RenderViewMLGPU(aBuilder, nullptr)
{
  mTarget = aTarget;
  mInvalidBounds = aInvalidRegion.GetBounds();

  // The clear region on the layer manager is the area that must be clear after
  // we finish drawing.
  mPostClearRegion = aBuilder->GetManager()->GetRegionToClear();

  // Clamp the post-clear region to the invalid bounds, since clears don't go
  // through the scissor rect if using ClearView.
  mPostClearRegion.AndWith(mInvalidBounds);

  // Since the post-clear will occlude everything, we include it in the final
  // opaque area.
  mOccludedRegion.OrWith(
    ViewAs<LayerPixel>(mPostClearRegion, PixelCastJustification::RenderTargetIsParentLayerForRoot));

  AL_LOG("RenderView %p root with invalid area %s, clear area %s\n",
    this,
    Stringify(mInvalidBounds).c_str(),
    Stringify(mPostClearRegion).c_str());
}

RenderViewMLGPU::RenderViewMLGPU(FrameBuilder* aBuilder,
                                 ContainerLayerMLGPU* aContainer,
                                 RenderViewMLGPU* aParent)
 : RenderViewMLGPU(aBuilder, aParent)
{
  mContainer = aContainer;
  mTargetOffset = aContainer->GetTargetOffset();
  mInvalidBounds = aContainer->GetInvalidRect();
  MOZ_ASSERT(!mInvalidBounds.IsEmpty());

  AL_LOG("RenderView %p starting with container %p and invalid area %s\n",
    this,
    aContainer->GetLayer(),
    Stringify(mInvalidBounds).c_str());
}

RenderViewMLGPU::RenderViewMLGPU(FrameBuilder* aBuilder, RenderViewMLGPU* aParent)
 : mBuilder(aBuilder),
   mDevice(aBuilder->GetDevice()),
   mParent(aParent),
   mContainer(nullptr),
   mFinishedBuilding(false),
   mCurrentLayerBufferIndex(kInvalidResourceIndex),
   mCurrentMaskRectBufferIndex(kInvalidResourceIndex),
   mNextSortIndex(1),
   mUseDepthBuffer(gfxPrefs::AdvancedLayersEnableDepthBuffer()),
   mDepthBufferNeedsClear(false)
{
  if (aParent) {
    aParent->AddChild(this);
  }
}

RenderViewMLGPU::~RenderViewMLGPU()
{
  for (const auto& child : mChildren) {
    child->mParent = nullptr;
  }
}

IntSize
RenderViewMLGPU::GetSize() const
{
  MOZ_ASSERT(mFinishedBuilding);
  return mTarget->GetSize();
}

MLGRenderTarget*
RenderViewMLGPU::GetRenderTarget() const
{
  MOZ_ASSERT(mFinishedBuilding);
  return mTarget;
}

void
RenderViewMLGPU::AddChild(RenderViewMLGPU* aParent)
{
  mChildren.push_back(aParent);
}

void
RenderViewMLGPU::Render()
{
  // We render tiles front-to-back, depth-first, to minimize render target switching.
  for (const auto& child : mChildren) {
    child->Render();
  }

  ExecuteRendering();
}

void
RenderViewMLGPU::FinishBuilding()
{
  MOZ_ASSERT(!mFinishedBuilding);
  mFinishedBuilding = true;

  if (mContainer) {
    MOZ_ASSERT(!mTarget);

    MLGRenderTargetFlags flags = MLGRenderTargetFlags::Default;
    if (mUseDepthBuffer) {
      flags |= MLGRenderTargetFlags::ZBuffer;
    }
    mTarget = mContainer->UpdateRenderTarget(mDevice, flags);
  }
}

void
RenderViewMLGPU::AddItem(LayerMLGPU* aItem,
                         const IntRect& aRect,
                         Maybe<Polygon>&& aGeometry)
{
  AL_LOG("RenderView %p analyzing layer %p\n", this, aItem->GetLayer());

  // If the item is not visible at all, skip it.
  if (aItem->GetComputedOpacity() == 0.0f) {
    AL_LOG("RenderView %p culling item %p with no opacity\n",
      this,
      aItem->GetLayer());
    return;
  }

  // When using the depth buffer, the z-index for items is important.
  //
  // Sort order starts at 1 and goes to positive infinity, with smaller values
  // being closer to the screen. Our viewport is the same, with anything
  // outside of [0.0, 1.0] being culled, and lower values occluding higher
  // values. To make this work our projection transform scales the z-axis.
  // Note that we do not use 0 as a sorting index (when depth-testing is
  // enabled) because this would result in a z-value of 1.0, which would be
  // culled.
  ItemInfo info(mBuilder, this, aItem, mNextSortIndex++, aRect, Move(aGeometry));

  // If the item is not visible, or we can't add it to the layer constant
  // buffer for some reason, bail out.
  if (!UpdateVisibleRegion(info) || !mBuilder->AddLayerToConstantBuffer(info)) {
    AL_LOG("RenderView %p culled item %p!\n", this, aItem->GetLayer());
    return;
  }

  // We support all layer types now.
  MOZ_ASSERT(info.type != RenderPassType::Unknown);

  if (info.renderOrder == RenderOrder::FrontToBack) {
    AddItemFrontToBack(aItem, info);
  } else {
    AddItemBackToFront(aItem, info);
  }
}

bool
RenderViewMLGPU::UpdateVisibleRegion(ItemInfo& aItem)
{
  // If the item has some kind of complex transform, we perform a very
  // simple occlusion test and move on. We using a depth buffer we skip
  // CPU-based occlusion culling as well, since the GPU will do most of our
  // culling work for us.
  if (mUseDepthBuffer ||
      !aItem.translation ||
      !gfxPrefs::AdvancedLayersEnableCPUOcclusion())
  {
    // Update the render region even if we won't compute visibility, since some
    // layer types (like Canvas and Image) need to have the visible region
    // clamped.
    LayerIntRegion region = Move(aItem.layer->GetShadowVisibleRegion());
    aItem.layer->SetRegionToRender(Move(region));

    AL_LOG("RenderView %p simple occlusion test, bounds=%s, translation?=%d\n",
      this,
      Stringify(aItem.bounds).c_str(),
      aItem.translation ? 1 : 0);
    return mInvalidBounds.Intersects(aItem.bounds);
  }

  MOZ_ASSERT(aItem.rectilinear);

  AL_LOG("RenderView %p starting visibility tests:\n", this);
  AL_LOG("  occluded=%s\n", Stringify(mOccludedRegion).c_str());

  // Compute the translation into render target space.
  LayerIntPoint translation =
    LayerIntPoint::FromUnknownPoint(aItem.translation.value() - mTargetOffset);
  AL_LOG("  translation=%s\n", Stringify(translation).c_str());

  IntRect clip = aItem.layer->GetComputedClipRect().ToUnknownRect();
  AL_LOG("  clip=%s\n", Stringify(translation).c_str());

  LayerIntRegion region = Move(aItem.layer->GetShadowVisibleRegion());
  region.MoveBy(translation);
  AL_LOG("  effective-visible=%s\n", Stringify(region).c_str());

  region.SubOut(mOccludedRegion);
  region.AndWith(LayerIntRect::FromUnknownRect(mInvalidBounds));
  region.AndWith(LayerIntRect::FromUnknownRect(clip));
  if (region.IsEmpty()) {
    return false;
  }

  // Move the visible region back into layer space.
  region.MoveBy(-translation);
  AL_LOG("  new-local-visible=%s\n", Stringify(region).c_str());

  aItem.layer->SetRegionToRender(Move(region));

  // Apply the new occluded area. We do another dance with the translation to
  // avoid copying the region. We do this after the SetRegionToRender call to
  // accomodate the possiblity of a layer changing its visible region.
  if (aItem.opaque) {
    mOccludedRegion.MoveBy(-translation);
    mOccludedRegion.OrWith(aItem.layer->GetShadowVisibleRegion());
    mOccludedRegion.MoveBy(translation);
    AL_LOG("  new-occluded=%s\n", Stringify(mOccludedRegion).c_str());

    // If the occluded region gets too complicated, we reset it.
    if (mOccludedRegion.GetNumRects() >= 32) {
      mOccludedRegion.SetEmpty();
      AL_LOG("  clear-occluded, too many rects\n");
    }
  }
  return true;
}

void
RenderViewMLGPU::AddItemFrontToBack(LayerMLGPU* aLayer, ItemInfo& aItem)
{
  // We receive items in front-to-back order. Ideally we want to push items
  // as far back into batches impossible, to ensure the GPU can do a good
  // job at culling. However we also want to make sure we actually batch
  // items versus drawing one primitive per pass.
  //
  // As a compromise we look at the most 3 recent batches and then give up.
  // This can be tweaked in the future.
  static const size_t kMaxSearch = 3;
  size_t iterations = 0;
  for (auto iter = mFrontToBack.rbegin(); iter != mFrontToBack.rend(); iter++) {
    RenderPassMLGPU* pass = (*iter);
    if (pass->IsCompatible(aItem) && pass->AcceptItem(aItem)) {
      AL_LOG("RenderView %p added layer %p to pass %p (%d)\n",
        this, aLayer->GetLayer(), pass, int(pass->GetType()));
      return;
    }
    if (++iterations > kMaxSearch) {
      break;
    }
  }

  RefPtr<RenderPassMLGPU> pass = RenderPassMLGPU::CreatePass(mBuilder, aItem);
  if (!pass || !pass->AcceptItem(aItem)) {
    MOZ_ASSERT_UNREACHABLE("Could not build a pass for item!");
    return;
  }
  AL_LOG("RenderView %p added layer %p to new pass %p (%d)\n",
    this, aLayer->GetLayer(), pass.get(), int(pass->GetType()));

  mFrontToBack.push_back(pass);
}

void
RenderViewMLGPU::AddItemBackToFront(LayerMLGPU* aLayer, ItemInfo& aItem)
{
  // We receive layers in front-to-back order, but there are two cases when we
  // actually draw back-to-front: when the depth buffer is disabled, or when
  // using the depth buffer and the item has transparent pixels (and therefore
  // requires blending). In these cases we will build vertex and constant
  // buffers in reverse, as well as execute batches in reverse, to ensure the
  // correct ordering.
  //
  // Note: We limit the number of batches we search through, since it's better
  // to add new draw calls than spend too much time finding compatible
  // batches further down.
  static const size_t kMaxSearch = 10;
  size_t iterations = 0;
  for (auto iter = mBackToFront.begin(); iter != mBackToFront.end(); iter++) {
    RenderPassMLGPU* pass = (*iter);
    if (pass->IsCompatible(aItem) && pass->AcceptItem(aItem)) {
      AL_LOG("RenderView %p added layer %p to pass %p (%d)\n",
        this, aLayer->GetLayer(), pass, int(pass->GetType()));
      return;
    }
    if (pass->Intersects(aItem)) {
      break;
    }
    if (++iterations > kMaxSearch) {
      break;
    }
  }

  RefPtr<RenderPassMLGPU> pass = RenderPassMLGPU::CreatePass(mBuilder, aItem);
  if (!pass || !pass->AcceptItem(aItem)) {
    MOZ_ASSERT_UNREACHABLE("Could not build a pass for item!");
    return;
  }
  AL_LOG("RenderView %p added layer %p to new pass %p (%d)\n",
    this, aLayer->GetLayer(), pass.get(), int(pass->GetType()));

  mBackToFront.push_front(pass);
}

void
RenderViewMLGPU::Prepare()
{
  if (!mTarget) {
    return;
  }

  // Prepare front-to-back passes. These are only present when using the depth
  // buffer, and they contain only opaque data.
  for (RefPtr<RenderPassMLGPU>& pass : mFrontToBack) {
    pass->PrepareForRendering();
  }

  // Prepare the Clear buffer, which will fill the render target with transparent
  // pixels. This must happen before we set up world constants, since it can
  // create new z-indices.
  PrepareClears();

  // Prepare the world constant buffer. This must be called after we've
  // finished allocating all z-indices.
  {
    WorldConstants vsConstants;
    Matrix4x4 projection = Matrix4x4::Translation(-1.0, 1.0, 0.0);
    projection.PreScale(2.0 / float(mTarget->GetSize().width),
                        2.0 / float(mTarget->GetSize().height),
                        1.0f);
    projection.PreScale(1.0f, -1.0f, 1.0f);

    memcpy(vsConstants.projection, &projection._11, 64);
    vsConstants.targetOffset = Point(mTargetOffset);
    vsConstants.sortIndexOffset = PrepareDepthBuffer();
    vsConstants.debugFrameNumber = mBuilder->GetManager()->GetDebugFrameNumber();

    SharedConstantBuffer* shared = mDevice->GetSharedVSBuffer();
    if (!shared->Allocate(&mWorldConstants, vsConstants)) {
      return;
    }
  }

  // Prepare back-to-front passes. In depth buffer mode, these contain draw
  // calls that might produce transparent pixels. When using CPU-based occlusion
  // culling, all draw calls are back-to-front.
  for (RefPtr<RenderPassMLGPU>& pass : mBackToFront) {
    pass->PrepareForRendering();
  }

  // Now, process children.
  for (const auto& iter : mChildren) {
    iter->Prepare();
  }
}

void
RenderViewMLGPU::ExecuteRendering()
{
  if (!mTarget) {
    return;
  }

  // Note: we unbind slot 0 (which is where the render target could have been
  // bound on a previous frame). Otherwise we trigger D3D11_DEVICE_PSSETSHADERRESOURCES_HAZARD.
  mDevice->UnsetPSTexture(0);
  mDevice->SetRenderTarget(mTarget);
  mDevice->SetViewport(IntRect(IntPoint(0, 0), mTarget->GetSize()));
  mDevice->SetScissorRect(Some(mInvalidBounds));

  if (!mWorldConstants.IsValid()) {
    gfxWarning() << "Failed to allocate constant buffer for world transform";
    return;
  }
  mDevice->SetVSConstantBuffer(kWorldConstantBufferSlot, &mWorldConstants);

  // If using the depth buffer, clear it (if needed) and enable writes.
  if (mUseDepthBuffer) {
    if (mDepthBufferNeedsClear) {
      mDevice->ClearDepthBuffer(mTarget);
    }
    mDevice->SetDepthTestMode(MLGDepthTestMode::Write);
  }

  // Opaque items, rendered front-to-back.
  for (auto iter = mFrontToBack.begin(); iter != mFrontToBack.end(); iter++) {
    ExecutePass(*iter);
  }

  if (mUseDepthBuffer) {
    // From now on we might be rendering transparent pixels, so we disable
    // writing to the z-buffer.
    mDevice->SetDepthTestMode(MLGDepthTestMode::ReadOnly);
  }

  // Clear any pixels that are not occluded, and therefore might require
  // blending.
  mDevice->DrawClearRegion(mPreClear);

  // Render back-to-front passes.
  for (auto iter = mBackToFront.begin(); iter != mBackToFront.end(); iter++) {
    ExecutePass(*iter);
  }

  // Make sure the post-clear area has no pixels.
  if (!mPostClearRegion.IsEmpty()) {
    mDevice->DrawClearRegion(mPostClear);
  }

  // We repaint the entire invalid region, even if it is partially occluded.
  // Thus it's safe for us to clear the invalid area here. If we ever switch
  // to nsIntRegions, we will have to take the difference between the paitned
  // area and the invalid area.
  if (mContainer) {
    mContainer->ClearInvalidRect();
  }
}

void
RenderViewMLGPU::ExecutePass(RenderPassMLGPU* aPass)
{
  if (!aPass->IsPrepared()) {
    return;
  }

  // Change the layer buffer if needed.
  if (aPass->GetLayerBufferIndex() != mCurrentLayerBufferIndex) {
    mCurrentLayerBufferIndex = aPass->GetLayerBufferIndex();

    ConstantBufferSection section = mBuilder->GetLayerBufferByIndex(mCurrentLayerBufferIndex);
    mDevice->SetVSConstantBuffer(kLayerBufferSlot, &section);
  }

  // Change the mask rect buffer if needed.
  if (aPass->GetMaskRectBufferIndex() &&
      aPass->GetMaskRectBufferIndex().value() != mCurrentMaskRectBufferIndex)
  {
    mCurrentMaskRectBufferIndex = aPass->GetMaskRectBufferIndex().value();

    ConstantBufferSection section = mBuilder->GetMaskRectBufferByIndex(mCurrentMaskRectBufferIndex);
    mDevice->SetVSConstantBuffer(kMaskBufferSlot, &section);
  }

  // Change the blend state if needed.
  if (Maybe<MLGBlendState> blendState = aPass->GetBlendState()) {
    mDevice->SetBlendState(blendState.value());
  }

  aPass->ExecuteRendering();
}

int32_t
RenderViewMLGPU::PrepareDepthBuffer()
{
  if (!mUseDepthBuffer) {
    return 0;
  }

  // Rather than clear the depth buffer every frame, we offset z-indices each
  // frame, starting with indices far away from the screen and moving toward
  // the user each successive frame. This ensures that frames can re-use the
  // depth buffer but never collide with previously written values.
  //
  // Once a frame runs out of sort indices, we finally clear the depth buffer
  // and start over again.

  // Note: the lowest sort index (kDepthLimit) is always occluded since it will
  // resolve to the clear value - kDepthLimit / kDepthLimit == 1.0.
  //
  // If we don't have any more indices to allocate, we need to clear the depth
  // buffer and start fresh.
  int32_t highestIndex = mTarget->GetLastDepthStart();
  if (highestIndex < mNextSortIndex) {
    mDepthBufferNeedsClear = true;
    highestIndex = kDepthLimit;
  }

  // We should not have more than kDepthLimit layers to draw. The last known
  // sort index might appear in the depth buffer and occlude something, so
  // we subtract 1. This ensures all our indices will compare less than all
  // old indices.
  int32_t sortOffset = highestIndex - mNextSortIndex - 1;
  MOZ_ASSERT(sortOffset >= 0);

  mTarget->SetLastDepthStart(sortOffset);
  return sortOffset;
}

void
RenderViewMLGPU::PrepareClears()
{
  // Get the list of rects to clear. If using the depth buffer, we don't
  // care if it's accurate since the GPU will do occlusion testing for us.
  // If not using the depth buffer, we subtract out the occluded region.
  LayerIntRegion region = LayerIntRect::FromUnknownRect(mInvalidBounds);
  if (!mUseDepthBuffer) {
    // Don't let the clear region become too complicated.
    region.SubOut(mOccludedRegion);
    region.SimplifyOutward(kMaxClearViewRects);
  }

  Maybe<int32_t> sortIndex;
  if (mUseDepthBuffer) {
    // Note that we use the lowest available sorting index, to ensure that when
    // using the z-buffer, we don't draw over already-drawn content.
    sortIndex = Some(mNextSortIndex++);
  }

  nsTArray<IntRect> rects = ToRectArray(region);
  mDevice->PrepareClearRegion(&mPreClear, Move(rects), sortIndex);

  if (!mPostClearRegion.IsEmpty()) {
    // Prepare the final clear as well. Note that we always do this clear at the
    // very end, even when the depth buffer is enabled, so we don't bother
    // setting a useful sorting index. If and when we try to ship the depth
    // buffer, we would execute this clear earlier in the pipeline and give it
    // the closest possible z-ordering to the screen.
    nsTArray<IntRect> rects = ToRectArray(mPostClearRegion);
    mDevice->PrepareClearRegion(&mPostClear, Move(rects), Nothing());
  }
}

} // namespace layers
} // namespace mozilla
