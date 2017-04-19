/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderPaintedLayer.h"

#include "LayersLogging.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/UpdateImageHelper.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "gfxPrefs.h"
#include "gfxUtils.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

void
WebRenderPaintedLayer::PaintThebes(nsTArray<ReadbackProcessor::Update>* aReadbackUpdates)
{
  PROFILER_LABEL("WebRenderPaintedLayer", "PaintThebes",
    js::ProfileEntry::Category::GRAPHICS);

  mContentClient->BeginPaint();

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

  mContentClient->EndPaint(aReadbackUpdates);

  if (didUpdate) {
    Mutated();

    // XXX It will cause reftests failures. See Bug 1340798.
    //mValidRegion.Or(mValidRegion, state.mRegionToDraw);

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

  PaintThebes(&readbackUpdates);
}

void
WebRenderPaintedLayer::RenderLayer(wr::DisplayListBuilder& aBuilder)
{
  // XXX We won't keep using ContentClient for WebRenderPaintedLayer in the future and
  // there is a crash problem for ContentClient on MacOS. So replace ContentClient with
  // ImageClient. See bug 1341001.
  //RenderLayerWithReadback(nullptr);

  if (!mImageContainer) {
    mImageContainer = LayerManager::CreateImageContainer();
  }

  if (!mImageClient) {
    mImageClient = ImageClient::CreateImageClient(CompositableType::IMAGE,
                                                  WrBridge(),
                                                  TextureFlags::DEFAULT);
    if (!mImageClient) {
      return;
    }
    mImageClient->Connect();
  }

  if (mExternalImageId.isNothing()) {
    mExternalImageId = Some(WrBridge()->AllocExternalImageIdForCompositable(mImageClient));
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

  IntSize imageSize(size.width, size.height);

  UpdateImageHelper helper(mImageContainer, mImageClient, imageSize);

  {
    RefPtr<DrawTarget> target = helper.GetDrawTarget();
    if (!target) {
      return;
    }

    target->ClearRect(Rect(0, 0, imageSize.width, imageSize.height));
    target->SetTransform(Matrix().PreTranslate(-bounds.x, -bounds.y));
    RefPtr<gfxContext> ctx = gfxContext::CreatePreservingTransformOrNull(target);
    MOZ_ASSERT(ctx); // already checked the target above

    Manager()->GetPaintedLayerCallback()(this,
                                         ctx,
                                         visibleRegion.ToUnknownRegion(), visibleRegion.ToUnknownRegion(),
                                         DrawRegionClip::DRAW, nsIntRegion(), Manager()->GetPaintedLayerCallbackData());

    if (gfxPrefs::WebRenderHighlightPaintedLayers()) {
      target->SetTransform(Matrix());
      target->FillRect(Rect(0, 0, imageSize.width, imageSize.height), ColorPattern(Color(1.0, 0.0, 0.0, 0.5)));
    }
  }

  if (!helper.UpdateImage()) {
    return;
  }

  gfx::Matrix4x4 transform = GetTransform();
  gfx::Rect relBounds = GetWrRelBounds();
  gfx::Rect rect(0, 0, size.width, size.height);

  gfx::Rect clipRect = GetWrClipRect(rect);
  Maybe<WrImageMask> mask = BuildWrMaskLayer(true);
  WrClipRegion clip = aBuilder.BuildClipRegion(wr::ToWrRect(clipRect), mask.ptrOr(nullptr));

  wr::MixBlendMode mixBlendMode = wr::ToWrMixBlendMode(GetMixBlendMode());

  DumpLayerInfo("PaintedLayer", rect);

  WrImageKey key = GetImageKey();
  WrBridge()->AddWebRenderParentCommand(OpAddExternalImage(mExternalImageId.value(), key));
  Manager()->AddImageKeyForDiscard(key);

  aBuilder.PushStackingContext(wr::ToWrRect(relBounds),
                              1.0f,
                              //GetAnimations(),
                              transform,
                              mixBlendMode);
  aBuilder.PushImage(wr::ToWrRect(rect), clip, wr::ImageRendering::Auto, key);
  aBuilder.PopStackingContext();
}

} // namespace layers
} // namespace mozilla
