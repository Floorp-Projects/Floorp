/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderPaintedLayer.h"

#include "WebRenderLayersLogging.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "gfxUtils.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

void
WebRenderPaintedLayer::PaintThebes()
{
  PROFILER_LABEL("WebRenderPaintedLayer", "PaintThebes",
    js::ProfileEntry::Category::GRAPHICS);

  uint32_t flags = RotatedContentBuffer::PAINT_CAN_DRAW_ROTATED;

  PaintState state =
    mContentClient->BeginPaintBuffer(this, flags);
  mValidRegion.Sub(mValidRegion, state.mRegionToInvalidate);

  if (!state.mRegionToDraw.IsEmpty() && !Manager()->GetPaintedLayerCallback()) {
    return;
  }

  // The area that became invalid and is visible needs to be repainted
  // (this could be the whole visible area if our buffer switched
  // from RGB to RGBA, because we might need to repaint with
  // subpixel AA)
  state.mRegionToInvalidate.And(state.mRegionToInvalidate,
                                GetLocalVisibleRegion().ToUnknownRegion());

  bool didUpdate = false;
  RotatedContentBuffer::DrawIterator iter;
  while (DrawTarget* target = mContentClient->BorrowDrawTargetForPainting(state, &iter)) {
    if (!target || !target->IsValid()) {
      if (target) {
        mContentClient->ReturnDrawTargetToBuffer(target);
      }
      continue;
    }

    SetAntialiasingFlags(this, target);

    RefPtr<gfxContext> ctx = gfxContext::CreatePreservingTransformOrNull(target);
    MOZ_ASSERT(ctx); // already checked the target above
    Manager()->GetPaintedLayerCallback()(this,
                                              ctx,
                                              iter.mDrawRegion,
                                              iter.mDrawRegion,
                                              state.mClip,
                                              state.mRegionToInvalidate,
                                              Manager()->GetPaintedLayerCallbackData());

    ctx = nullptr;
    mContentClient->ReturnDrawTargetToBuffer(target);
    didUpdate = true;
  }
  if (didUpdate) {
    Mutated();

    ContentClientRemote* contentClientRemote = static_cast<ContentClientRemote*>(mContentClient.get());

    // Hold(this) ensures this layer is kept alive through the current transaction
    // The ContentClient assumes this layer is kept alive (e.g., in CreateBuffer),
    // so deleting this Hold for whatever reason will break things.
    Manager()->Hold(this);

    contentClientRemote->Updated(state.mRegionToDraw,
                                 mVisibleRegion.ToUnknownRegion(),
                                 state.mDidSelfCopy);
  }
}

void
WebRenderPaintedLayer::RenderLayerWithReadback(ReadbackProcessor *aReadback)
{
  if (!mContentClient) {
    mContentClient = ContentClient::CreateContentClient(Manager()->WrBridge());
    if (!mContentClient) {
      return;
    }
    mContentClient->Connect();
    MOZ_ASSERT(mContentClient->GetForwarder());
  }

  nsTArray<ReadbackProcessor::Update> readbackUpdates;
  nsIntRegion readbackRegion;
  if (aReadback && UsedForReadback()) {
    aReadback->GetPaintedLayerUpdates(this, &readbackUpdates);
  }

  IntPoint origin(mVisibleRegion.GetBounds().x, mVisibleRegion.GetBounds().y);
  mContentClient->BeginPaint();
  PaintThebes();
  mContentClient->EndPaint(&readbackUpdates);
}

void
WebRenderPaintedLayer::RenderLayer()
{
  RenderLayerWithReadback(nullptr);

  if (!mExternalImageId) {
    mExternalImageId = WrBridge()->AllocExternalImageIdForCompositable(mContentClient);
    MOZ_ASSERT(mExternalImageId);
  }

  LayerIntRegion visibleRegion = GetVisibleRegion();
  LayerIntRect bounds = visibleRegion.GetBounds();
  LayerIntSize size = bounds.Size();
  if (size.IsEmpty()) {
      if (gfxPrefs::LayersDump()) {
        printf_stderr("PaintedLayer %p skipping\n", this->GetLayer());
      }
      return;
  }

  WrScrollFrameStackingContextGenerator scrollFrames(this);

  // Since we are creating a stacking context below using the visible region of
  // this layer, we need to make sure the image display item has coordinates
  // relative to the visible region.
  Rect rect = RelativeToVisible(IntRectToRect(bounds.ToUnknownRect()));
  Rect clip;
  if (GetClipRect().isSome()) {
      clip = RelativeToTransformedVisible(IntRectToRect(GetClipRect().ref().ToUnknownRect()));
  } else {
      clip = rect;
  }

  Maybe<WrImageMask> mask = buildMaskLayer();
  Rect relBounds = TransformedVisibleBoundsRelativeToParent();
  Rect overflow(0, 0, relBounds.width, relBounds.height);
  Matrix4x4 transform;// = GetTransform();
  WrMixBlendMode mixBlendMode = wr::ToWrMixBlendMode(GetMixBlendMode());

  if (gfxPrefs::LayersDump()) {
    printf_stderr("PaintedLayer %p using bounds=%s, overflow=%s, transform=%s, rect=%s, clip=%s, mix-blend-mode=%s\n",
                  this->GetLayer(),
                  Stringify(relBounds).c_str(),
                  Stringify(overflow).c_str(),
                  Stringify(transform).c_str(),
                  Stringify(rect).c_str(),
                  Stringify(clip).c_str(),
                  Stringify(mixBlendMode).c_str());
  }

  ContentClientRemoteBuffer* contentClientRemote = static_cast<ContentClientRemoteBuffer*>(mContentClient.get());
  visibleRegion.MoveBy(-contentClientRemote->BufferRect().x, -contentClientRemote->BufferRect().y);

  WrBridge()->AddWebRenderCommand(
      OpDPPushStackingContext(wr::ToWrRect(relBounds),
                              wr::ToWrRect(overflow),
                              mask,
                              1.0f,
                              GetAnimations(),
                              transform,
                              mixBlendMode,
                              FrameMetrics::NULL_SCROLL_ID));
  WrBridge()->AddWebRenderCommand(OpDPPushExternalImageId(visibleRegion, wr::ToWrRect(rect), wr::ToWrRect(clip), Nothing(), WrTextureFilter::Linear, mExternalImageId));
  WrBridge()->AddWebRenderCommand(OpDPPopStackingContext());
}

} // namespace layers
} // namespace mozilla
