/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FrameBuilder.h"
#include "ContainerLayerMLGPU.h"
#include "GeckoProfiler.h"              // for profiler_*
#include "LayerMLGPU.h"
#include "LayerManagerMLGPU.h"
#include "MaskOperation.h"
#include "RenderPassMLGPU.h"
#include "RenderViewMLGPU.h"
#include "mozilla/gfx/Polygon.h"
#include "mozilla/layers/BSPTree.h"
#include "mozilla/layers/LayersHelpers.h"

namespace mozilla {
namespace layers {

using namespace mlg;

FrameBuilder::FrameBuilder(LayerManagerMLGPU* aManager, MLGSwapChain* aSwapChain)
 : mManager(aManager),
   mDevice(aManager->GetDevice()),
   mSwapChain(aSwapChain)
{
  // test_bug1124898.html has a root ColorLayer, so we don't assume the root is
  // a container.
  mRoot = mManager->GetRoot()->AsHostLayer()->AsLayerMLGPU();
}

FrameBuilder::~FrameBuilder()
{
}

bool
FrameBuilder::Build()
{
  AUTO_PROFILER_LABEL("FrameBuilder::Build", GRAPHICS);

  // AcquireBackBuffer can fail, so we check the result here.
  RefPtr<MLGRenderTarget> target = mSwapChain->AcquireBackBuffer();
  if (!target) {
    return false;
  }

  // This updates the frame sequence number, so layers can quickly check if
  // they've already been prepared.
  LayerMLGPU::BeginFrame();

  // Note: we don't clip draw calls to the invalid region per se, but instead
  // the region bounds. Clipping all draw calls would incur a significant
  // CPU cost on large layer trees, and would greatly complicate how draw
  // rects are added in RenderPassMLGPU, since we would need to break
  // each call into additional items based on the intersection with the
  // invalid region.
  //
  // Instead we scissor to the invalid region bounds. As a result, all items
  // affecting the invalid bounds are redrawn, even if not all are in the
  // precise region.
  const nsIntRegion& region = mSwapChain->GetBackBufferInvalidRegion();

  mWidgetRenderView = new RenderViewMLGPU(this, target, region);

  // Traverse the layer tree and assign each layer to tiles.
  {
    Maybe<gfx::Polygon> geometry;
    RenderTargetIntRect clip(0, 0, target->GetSize().width, target->GetSize().height);

    AssignLayer(mRoot->GetLayer(), mWidgetRenderView, clip, Move(geometry));
  }

  // Build the default mask buffer.
  {
    MaskInformation defaultMaskInfo(1.0f, false);
    if (!mDevice->GetSharedPSBuffer()->Allocate(&mDefaultMaskInfo, defaultMaskInfo)) {
      return false;
    }
  }

  // Build render passes and buffer information for each pass.
  mWidgetRenderView->FinishBuilding();
  mWidgetRenderView->Prepare();

  // Prepare masks that need to be combined.
  for (const auto& pair : mCombinedTextureMasks) {
    pair.second->PrepareForRendering();
  }

  FinishCurrentLayerBuffer();
  FinishCurrentMaskRectBuffer();
  return true;
}

void
FrameBuilder::Render()
{
  AUTO_PROFILER_LABEL("FrameBuilder::Render", GRAPHICS);

  // Render combined masks into single mask textures.
  for (const auto& pair : mCombinedTextureMasks) {
    pair.second->Render();
  }

  // Render to all targets, front-to-back.
  mWidgetRenderView->Render();
}

void
FrameBuilder::AssignLayer(Layer* aLayer,
                          RenderViewMLGPU* aView,
                          const RenderTargetIntRect& aClipRect,
                          Maybe<gfx::Polygon>&& aGeometry)
{
  LayerMLGPU* layer = aLayer->AsHostLayer()->AsLayerMLGPU();

  if (ContainerLayer* container = aLayer->AsContainerLayer()) {
    // This returns false if we don't need to (or can't) process the layer any
    // further. This always returns false for non-leaf ContainerLayers.
    if (!ProcessContainerLayer(container, aView, aClipRect, aGeometry)) {
      return;
    }
  } else {
    // Set the precomputed clip and any textures/resources that are needed.
    if (!layer->PrepareToRender(this, aClipRect)) {
      return;
    }
  }

  // If we are dealing with a nested 3D context, we might need to transform
  // the geometry back to the coordinate space of the current layer.
  if (aGeometry) {
    TransformLayerGeometry(aLayer, aGeometry);
  }

  // Finally, assign the layer to a rendering batch in the current render
  // target.
  layer->AssignToView(this, aView, Move(aGeometry));
}

bool
FrameBuilder::ProcessContainerLayer(ContainerLayer* aContainer,
                                    RenderViewMLGPU* aView,
                                    const RenderTargetIntRect& aClipRect,
                                    Maybe<gfx::Polygon>& aGeometry)
{
  LayerMLGPU* layer = aContainer->AsHostLayer()->AsLayerMLGPU();

  // We don't want to traverse containers twice, so we only traverse them if
  // they haven't been prepared yet.
  bool isFirstVisit = !layer->IsPrepared();
  if (isFirstVisit && !layer->PrepareToRender(this, aClipRect)) {
    return false;
  }

  // If the container is not part of the invalid region, we don't draw it
  // or traverse it. Note that we do not pass the geometry here. Otherwise
  // we could decide the particular split is not visible, and because of the
  // check above, never bother traversing the container again.
  gfx::IntRect boundingBox = layer->GetClippedBoundingBox(aView, Nothing());
  const gfx::IntRect& invalidRect = aView->GetInvalidRect();
  if (boundingBox.IsEmpty() || !invalidRect.Intersects(boundingBox)) {
    return false;
  }

  if (!aContainer->UseIntermediateSurface()) {
    // In case the layer previously required an intermediate surface, we
    // clear any intermediate render targets here.
    layer->ClearCachedResources();

    // This is a pass-through container, so we just process children and
    // instruct AssignLayer to early-return.
    ProcessChildList(aContainer, aView, aClipRect, aGeometry);
    return false;
  }

  // If this is the first visit of the container this frame, and the
  // container has an unpainted area, we traverse the container. Note that
  // RefLayers do not have intermediate surfaces so this is guaranteed
  // to be a full-fledged ContainerLayerMLGPU.
  ContainerLayerMLGPU* viewContainer = layer->AsContainerLayerMLGPU();
  if (isFirstVisit && !viewContainer->GetInvalidRect().IsEmpty()) {
    // The RenderView constructor automatically attaches itself to the parent.
    RefPtr<RenderViewMLGPU> view = new RenderViewMLGPU(this, viewContainer, aView);
    ProcessChildList(aContainer, view, aClipRect, Nothing());
    view->FinishBuilding();
  }
  return true;
}

void
FrameBuilder::ProcessChildList(ContainerLayer* aContainer,
                               RenderViewMLGPU* aView,
                               const RenderTargetIntRect& aParentClipRect,
                               const Maybe<gfx::Polygon>& aParentGeometry)
{
  nsTArray<LayerPolygon> polygons =
    aContainer->SortChildrenBy3DZOrder(ContainerLayer::SortMode::WITH_GEOMETRY);

  // Visit layers in front-to-back order.
  for (auto iter = polygons.rbegin(); iter != polygons.rend(); iter++) {
    LayerPolygon& entry = *iter;
    Layer* child = entry.layer;
    if (child->IsBackfaceHidden() || !child->IsVisible()) {
      continue;
    }

    RenderTargetIntRect clip = child->CalculateScissorRect(aParentClipRect);
    if (clip.IsEmpty()) {
      continue;
    }

    Maybe<gfx::Polygon> geometry;
    if (aParentGeometry && entry.geometry) {
      // Both parent and child are split.
      geometry = Some(aParentGeometry->ClipPolygon(*entry.geometry));
    } else if (aParentGeometry) {
      geometry = aParentGeometry;
    } else if (entry.geometry) {
      geometry = Move(entry.geometry);
    }

    AssignLayer(child, aView, clip, Move(geometry));
  }
}

bool
FrameBuilder::AddLayerToConstantBuffer(ItemInfo& aItem)
{
  LayerMLGPU* layer = aItem.layer;

  // If this layer could appear multiple times, cache it.
  if (aItem.geometry) {
    if (mLayerBufferMap.Get(layer, &aItem.layerIndex)) {
      return true;
    }
  }

  LayerConstants* info = AllocateLayerInfo(aItem);
  if (!info) {
    return false;
  }

  // Note we do not use GetEffectiveTransformForBuffer, since we calculate
  // the correct scaling when we build texture coordinates.
  Layer* baseLayer = layer->GetLayer();
  const gfx::Matrix4x4& transform = baseLayer->GetEffectiveTransform();

  memcpy(&info->transform, &transform._11, 64);
  info->clipRect = gfx::Rect(layer->GetComputedClipRect().ToUnknownRect());
  info->maskIndex = 0;
  if (MaskOperation* op = layer->GetMask()) {
    // Note: we use 0 as an invalid index, and so indices are offset by 1.
    gfx::Rect rect = op->ComputeMaskRect(baseLayer);
    AddMaskRect(rect, &info->maskIndex);
  }

  if (aItem.geometry) {
    mLayerBufferMap.Put(layer, aItem.layerIndex);
  }
  return true;
}

MaskOperation*
FrameBuilder::AddMaskOperation(LayerMLGPU* aLayer)
{
  Layer* layer = aLayer->GetLayer();
  MOZ_ASSERT(layer->HasMaskLayers());

  // Multiple masks are combined into a single mask.
  if ((layer->GetMaskLayer() && layer->GetAncestorMaskLayerCount()) ||
      layer->GetAncestorMaskLayerCount() > 1)
  {
    // Since each mask can be moved independently of the other, we must create
    // a separate combined mask for every new positioning we encounter.
    MaskTextureList textures;
    if (Layer* maskLayer = layer->GetMaskLayer()) {
      AppendToMaskTextureList(textures, maskLayer);
    }
    for (size_t i = 0; i < layer->GetAncestorMaskLayerCount(); i++) {
      AppendToMaskTextureList(textures, layer->GetAncestorMaskLayerAt(i));
    }

    auto iter = mCombinedTextureMasks.find(textures);
    if (iter != mCombinedTextureMasks.end()) {
      return iter->second;
    }

    RefPtr<MaskCombineOperation> op = new MaskCombineOperation(this);
    op->Init(textures);

    mCombinedTextureMasks[textures] = op;
    return op;
  }

  Layer* maskLayer = layer->GetMaskLayer()
                     ? layer->GetMaskLayer()
                     : layer->GetAncestorMaskLayerAt(0);
  RefPtr<TextureSource> texture = GetMaskLayerTexture(maskLayer);
  if (!texture) {
    return nullptr;
  }

  RefPtr<MaskOperation> op;
  mSingleTextureMasks.Get(texture, getter_AddRefs(op));
  if (op) {
    return op;
  }

  RefPtr<MLGTexture> wrapped = mDevice->CreateTexture(texture);

  op = new MaskOperation(this, wrapped);
  mSingleTextureMasks.Put(texture, op);
  return op;
}

void
FrameBuilder::RetainTemporaryLayer(LayerMLGPU* aLayer)
{
  // This should only be used with temporary layers. Temporary layers do not
  // have parents.
  MOZ_ASSERT(!aLayer->GetLayer()->GetParent());
  mTemporaryLayers.push_back(aLayer->GetLayer());
}

LayerConstants*
FrameBuilder::AllocateLayerInfo(ItemInfo& aItem)
{
  if (((mCurrentLayerBuffer.Length() + 1) * sizeof(LayerConstants)) >
      mDevice->GetMaxConstantBufferBindSize())
  {
    FinishCurrentLayerBuffer();
    mLayerBufferMap.Clear();
    mCurrentLayerBuffer.ClearAndRetainStorage();
  }

  LayerConstants* info = mCurrentLayerBuffer.AppendElement(mozilla::fallible);
  if (!info) {
    return nullptr;
  }

  aItem.layerIndex = mCurrentLayerBuffer.Length() - 1;
  return info;
}

void
FrameBuilder::FinishCurrentLayerBuffer()
{
  if (mCurrentLayerBuffer.IsEmpty()) {
    return;
  }

  // Note: we append the buffer even if we couldn't allocate one, since
  // that keeps the indices sane.
  ConstantBufferSection section;
  mDevice->GetSharedVSBuffer()->Allocate(
    &section,
    mCurrentLayerBuffer.Elements(),
    mCurrentLayerBuffer.Length());
  mLayerBuffers.AppendElement(section);
}

size_t
FrameBuilder::CurrentLayerBufferIndex() const
{
  // The mask rect buffer list doesn't contain the buffer currently being
  // built, so we don't subtract 1 here.
  return mLayerBuffers.Length();
}

ConstantBufferSection
FrameBuilder::GetLayerBufferByIndex(size_t aIndex) const
{
  if (aIndex >= mLayerBuffers.Length()) {
    return ConstantBufferSection();
  }
  return mLayerBuffers[aIndex];
}

bool
FrameBuilder::AddMaskRect(const gfx::Rect& aRect, uint32_t* aOutIndex)
{
  if (((mCurrentMaskRectList.Length() + 1) * sizeof(gfx::Rect)) >
      mDevice->GetMaxConstantBufferBindSize())
  {
    FinishCurrentMaskRectBuffer();
    mCurrentMaskRectList.ClearAndRetainStorage();
  }

  mCurrentMaskRectList.AppendElement(aRect);

  // Mask indices start at 1 so the shader can use 0 as a no-mask indicator.
  *aOutIndex = mCurrentMaskRectList.Length();
  return true;
}

void
FrameBuilder::FinishCurrentMaskRectBuffer()
{
  if (mCurrentMaskRectList.IsEmpty()) {
    return;
  }

  // Note: we append the buffer even if we couldn't allocate one, since
  // that keeps the indices sane.
  ConstantBufferSection section;
  mDevice->GetSharedVSBuffer()->Allocate(
    &section,
    mCurrentMaskRectList.Elements(),
    mCurrentMaskRectList.Length());
  mMaskRectBuffers.AppendElement(section);
}

size_t
FrameBuilder::CurrentMaskRectBufferIndex() const
{
  // The mask rect buffer list doesn't contain the buffer currently being
  // built, so we don't subtract 1 here.
  return mMaskRectBuffers.Length();
}

ConstantBufferSection
FrameBuilder::GetMaskRectBufferByIndex(size_t aIndex) const
{
  if (aIndex >= mMaskRectBuffers.Length()) {
    return ConstantBufferSection();
  }
  return mMaskRectBuffers[aIndex];
}

} // namespace layers
} // namespace mozilla
