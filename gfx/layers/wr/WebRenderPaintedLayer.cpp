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

bool
WebRenderPaintedLayer::SetupExternalImages()
{
  // XXX We won't keep using ContentClient for WebRenderPaintedLayer in the future and
  // there is a crash problem for ContentClient on MacOS. So replace ContentClient with
  // ImageClient. See bug 1341001.

  if (!mImageContainer) {
    mImageContainer = LayerManager::CreateImageContainer();
  }

  if (!mImageClient) {
    mImageClient = ImageClient::CreateImageClient(CompositableType::IMAGE,
                                                  WrBridge(),
                                                  TextureFlags::DEFAULT);
    if (!mImageClient) {
      return false;
    }
    mImageClient->Connect();
  }

  if (mExternalImageId.isNothing()) {
    mExternalImageId = Some(WrBridge()->AllocExternalImageIdForCompositable(mImageClient));
  }

  return true;
}

bool
WebRenderPaintedLayer::UpdateImageClient()
{
  MOZ_ASSERT(Manager()->GetPaintedLayerCallback());
  LayerIntRegion visibleRegion = GetVisibleRegion();
  LayerIntRect bounds = visibleRegion.GetBounds();
  LayerIntSize size = bounds.Size();
  IntSize imageSize(size.width, size.height);

  UpdateImageHelper helper(mImageContainer, mImageClient, imageSize);

  {
    RefPtr<DrawTarget> target = helper.GetDrawTarget();
    if (!target) {
      return false;
    }

    target->ClearRect(Rect(0, 0, imageSize.width, imageSize.height));
    target->SetTransform(Matrix().PreTranslate(-bounds.x, -bounds.y));
    RefPtr<gfxContext> ctx =
        gfxContext::CreatePreservingTransformOrNull(target);
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

  return true;
}

void
WebRenderPaintedLayer::CreateWebRenderDisplayList(wr::DisplayListBuilder& aBuilder)
{
  LayerIntRegion visibleRegion = GetVisibleRegion();
  LayerIntRect bounds = visibleRegion.GetBounds();
  LayerIntSize size = bounds.Size();

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

void
WebRenderPaintedLayer::RenderLayer(wr::DisplayListBuilder& aBuilder)
{
  if (!SetupExternalImages()) {
    return;
  }

  if (GetVisibleRegion().IsEmpty()) {
    if (gfxPrefs::LayersDump()) {
      printf_stderr("PaintedLayer %p skipping\n", this->GetLayer());
    }
    return;
  }

  nsIntRegion regionToPaint;
  regionToPaint.Sub(mVisibleRegion.ToUnknownRegion(), mValidRegion);

  // We have something to paint but can't. This usually happens only in
  // empty transactions
  if (!regionToPaint.IsEmpty() && !Manager()->GetPaintedLayerCallback()) {
    Manager()->SetTransactionIncomplete();
    return;
  }

  if (!regionToPaint.IsEmpty() && Manager()->GetPaintedLayerCallback()) {
    if (!UpdateImageClient()) {
      return;
    }
  } else {
    // We have an empty transaction, just reuse the old image we had before.
    MOZ_ASSERT(mExternalImageId);
    MOZ_ASSERT(mImageContainer->HasCurrentImage());
    MOZ_ASSERT(GetInvalidRegion().IsEmpty());
  }

  CreateWebRenderDisplayList(aBuilder);
}

} // namespace layers
} // namespace mozilla
