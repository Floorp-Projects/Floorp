/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderLayerManager.h"

#include "apz/src/AsyncPanZoomController.h"
#include "LayersLogging.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/AsyncCompositionManager.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#include "nsThreadUtils.h"
#include "TreeTraversal.h"
#include "WebRenderCanvasLayer.h"
#include "WebRenderColorLayer.h"
#include "WebRenderContainerLayer.h"
#include "WebRenderImageLayer.h"
#include "WebRenderPaintedLayer.h"

namespace mozilla {

using namespace gfx;

namespace layers {

WebRenderLayerManager*
WebRenderLayer::WRManager()
{
  return static_cast<WebRenderLayerManager*>(GetLayer()->Manager());
}

WebRenderBridgeChild*
WebRenderLayer::WRBridge()
{
  return WRManager()->WRBridge();
}

Rect
WebRenderLayer::RelativeToVisible(Rect aRect)
{
  IntRect bounds = GetLayer()->GetVisibleRegion().GetBounds().ToUnknownRect();
  aRect.MoveBy(-bounds.x, -bounds.y);
  return aRect;
}

Rect
WebRenderLayer::RelativeToTransformedVisible(Rect aRect)
{
  IntRect bounds = GetLayer()->GetVisibleRegion().GetBounds().ToUnknownRect();
  Rect transformed = GetLayer()->GetTransform().TransformBounds(IntRectToRect(bounds));
  aRect.MoveBy(-transformed.x, -transformed.y);
  return aRect;
}

Rect
WebRenderLayer::ParentStackingContextBounds(size_t aScrollMetadataIndex)
{
  // Walk up to find the parent stacking context. This will be created either
  // by the nearest scrollable metrics, or by the parent layer which must be a
  // ContainerLayer.
  Layer* layer = GetLayer();
  for (size_t i = aScrollMetadataIndex + 1; i < layer->GetScrollMetadataCount(); i++) {
    if (layer->GetFrameMetrics(i).IsScrollable()) {
      return layer->GetFrameMetrics(i).GetCompositionBounds().ToUnknownRect();
    }
  }
  if (layer->GetParent()) {
    return IntRectToRect(layer->GetParent()->GetVisibleRegion().GetBounds().ToUnknownRect());
  }
  return Rect();
}

Rect
WebRenderLayer::RelativeToParent(Rect aRect)
{
  Rect parentBounds = ParentStackingContextBounds(-1);
  aRect.MoveBy(-parentBounds.x, -parentBounds.y);
  return aRect;
}

Rect
WebRenderLayer::TransformedVisibleBoundsRelativeToParent()
{
  IntRect bounds = GetLayer()->GetVisibleRegion().GetBounds().ToUnknownRect();
  Rect transformed = GetLayer()->GetTransform().TransformBounds(IntRectToRect(bounds));
  return RelativeToParent(transformed);
}

WRScrollFrameStackingContextGenerator::WRScrollFrameStackingContextGenerator(
        WebRenderLayer* aLayer)
  : mLayer(aLayer)
{
  Layer* layer = mLayer->GetLayer();
  for (size_t i = layer->GetScrollMetadataCount(); i > 0; i--) {
    const FrameMetrics& fm = layer->GetFrameMetrics(i - 1);
    if (!fm.IsScrollable()) {
      continue;
    }
    if (gfxPrefs::LayersDump()) printf_stderr("Pushing stacking context id %" PRIu64"\n", fm.GetScrollId());
    mLayer->WRBridge()->AddWebRenderCommand(OpPushDLBuilder());
  }
}

WRScrollFrameStackingContextGenerator::~WRScrollFrameStackingContextGenerator()
{
  Matrix4x4 identity;
  Layer* layer = mLayer->GetLayer();
  for (size_t i = 0; i < layer->GetScrollMetadataCount(); i++) {
    const FrameMetrics& fm = layer->GetFrameMetrics(i);
    if (!fm.IsScrollable()) {
      continue;
    }
    Rect bounds = fm.GetCompositionBounds().ToUnknownRect();
    Rect overflow = (fm.GetExpandedScrollableRect() * fm.LayersPixelsPerCSSPixel()).ToUnknownRect();
    Point scrollPos = (fm.GetScrollOffset() * fm.LayersPixelsPerCSSPixel()).ToUnknownPoint();
    Rect parentBounds = mLayer->ParentStackingContextBounds(i);
    bounds.MoveBy(-parentBounds.x, -parentBounds.y);
    // Subtract the MT scroll position from the overflow here so that the WR
    // scroll offset (which is the APZ async scroll component) always fits in
    // the available overflow. If we didn't do this and WR did bounds checking
    // on the scroll offset, we'd fail those checks.
    overflow.MoveBy(bounds.x - scrollPos.x, bounds.y - scrollPos.y);
    if (gfxPrefs::LayersDump()) {
      printf_stderr("Popping stacking context id %" PRIu64 " with bounds=%s overflow=%s\n",
        fm.GetScrollId(), Stringify(bounds).c_str(), Stringify(overflow).c_str());
    }
    mLayer->WRBridge()->AddWebRenderCommand(
      OpPopDLBuilder(toWrRect(bounds), toWrRect(overflow), identity, fm.GetScrollId()));
  }
}


WebRenderLayerManager::WebRenderLayerManager(nsIWidget* aWidget)
  : mWidget(aWidget)
{
}

void
WebRenderLayerManager::Initialize(PCompositorBridgeChild* aCBChild, uint64_t aLayersId)
{
  MOZ_ASSERT(mWRChild == nullptr);

  PWebRenderBridgeChild* bridge = aCBChild->SendPWebRenderBridgeConstructor(aLayersId);
  MOZ_ASSERT(bridge);
  mWRChild = static_cast<WebRenderBridgeChild*>(bridge);
  LayoutDeviceIntSize size = mWidget->GetClientSize();
  WRBridge()->SendCreate(size.width, size.height);
}

void
WebRenderLayerManager::Destroy()
{
  DiscardImages();
}

WebRenderLayerManager::~WebRenderLayerManager()
{
  WRBridge()->SendDestroy();
}

CompositorBridgeChild*
WebRenderLayerManager::GetCompositorBridgeChild()
{
  return mWidget ? mWidget->GetRemoteRenderer() : nullptr;
}

int32_t
WebRenderLayerManager::GetMaxTextureSize() const
{
  return 4096;
}

bool
WebRenderLayerManager::BeginTransactionWithTarget(gfxContext* aTarget)
{
  return BeginTransaction();
}

bool
WebRenderLayerManager::BeginTransaction()
{
  return true;
}

bool
WebRenderLayerManager::EndEmptyTransaction(EndTransactionFlags aFlags)
{
  return false;
}

void
WebRenderLayerManager::EndTransaction(DrawPaintedLayerCallback aCallback,
                                      void* aCallbackData,
                                      EndTransactionFlags aFlags)
{
  DiscardImages();

  mPaintedLayerCallback = aCallback;
  mPaintedLayerCallbackData = aCallbackData;

  if (gfxPrefs::LayersDump()) {
    this->Dump();
  }

  // Since we don't do repeat transactions right now, just set the time
  mAnimationReadyTime = TimeStamp::Now();

  LayoutDeviceIntSize size = mWidget->GetClientSize();
  if (!WRBridge()->DPBegin(size.width, size.height)) {
    return;
  }

  WebRenderLayer::ToWebRenderLayer(mRoot)->RenderLayer();

  WRBridge()->DPEnd();
}

void
WebRenderLayerManager::AddImageKeyForDiscard(WRImageKey key)
{
  mImageKeys.push_back(key);
}

void
WebRenderLayerManager::DiscardImages()
{
  for (auto key : mImageKeys) {
      WRBridge()->SendDeleteImage(key);
  }
  mImageKeys.clear();
}

void
WebRenderLayerManager::SetRoot(Layer* aLayer)
{
  mRoot = aLayer;
}

already_AddRefed<PaintedLayer>
WebRenderLayerManager::CreatePaintedLayer()
{
  return MakeAndAddRef<WebRenderPaintedLayer>(this);
}

already_AddRefed<ContainerLayer>
WebRenderLayerManager::CreateContainerLayer()
{
  return MakeAndAddRef<WebRenderContainerLayer>(this);
}

already_AddRefed<ImageLayer>
WebRenderLayerManager::CreateImageLayer()
{
  return MakeAndAddRef<WebRenderImageLayer>(this);
}

already_AddRefed<CanvasLayer>
WebRenderLayerManager::CreateCanvasLayer()
{
  return MakeAndAddRef<WebRenderCanvasLayer>(this);
}

already_AddRefed<ReadbackLayer>
WebRenderLayerManager::CreateReadbackLayer()
{
  return nullptr;
}

already_AddRefed<ColorLayer>
WebRenderLayerManager::CreateColorLayer()
{
  return MakeAndAddRef<WebRenderColorLayer>(this);
}

already_AddRefed<RefLayer>
WebRenderLayerManager::CreateRefLayer()
{
  return MakeAndAddRef<WebRenderRefLayer>(this);
}

} // namespace layers
} // namespace mozilla
