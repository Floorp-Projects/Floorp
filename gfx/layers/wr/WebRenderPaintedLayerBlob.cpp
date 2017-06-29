/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderPaintedLayerBlob.h"

#include "gfxPrefs.h"
#include "gfxUtils.h"
#include "LayersLogging.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/layers/ScrollingLayersHelper.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/UpdateImageHelper.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

void
WebRenderPaintedLayerBlob::RenderLayer(wr::DisplayListBuilder& aBuilder,
                                       const StackingContextHelper& aSc)
{
  LayerIntRegion visibleRegion = GetVisibleRegion();
  LayerIntRect bounds = visibleRegion.GetBounds();
  LayerIntSize size = bounds.Size();

  if (visibleRegion.IsEmpty()) {
    if (gfxPrefs::LayersDump()) {
      printf_stderr("PaintedLayer %p skipping\n", this->GetLayer());
    }
    return;
  }

  nsIntRegion regionToPaint;
  regionToPaint.Sub(mVisibleRegion.ToUnknownRegion(), GetValidRegion());

  // We have something to paint but can't. This usually happens only in
  // empty transactions
  if (!regionToPaint.IsEmpty() && !WrManager()->GetPaintedLayerCallback()) {
    WrManager()->SetTransactionIncomplete();
    return;
  }

  IntSize imageSize(size.ToUnknownSize());
  if (!regionToPaint.IsEmpty() && WrManager()->GetPaintedLayerCallback()) {
    RefPtr<gfx::DrawEventRecorderMemory> recorder = MakeAndAddRef<gfx::DrawEventRecorderMemory>();
    RefPtr<gfx::DrawTarget> dummyDt = gfx::Factory::CreateDrawTarget(gfx::BackendType::SKIA, IntSize(1, 1), gfx::SurfaceFormat::B8G8R8X8);
    RefPtr<gfx::DrawTarget> dt = gfx::Factory::CreateRecordingDrawTarget(recorder, dummyDt, imageSize);

    dt->ClearRect(Rect(0, 0, imageSize.width, imageSize.height));
    dt->SetTransform(Matrix().PreTranslate(-bounds.x, -bounds.y));
    RefPtr<gfxContext> ctx = gfxContext::CreatePreservingTransformOrNull(dt);
    MOZ_ASSERT(ctx); // already checked the target above

    WrManager()->GetPaintedLayerCallback()(this,
                                           ctx,
                                           visibleRegion.ToUnknownRegion(), visibleRegion.ToUnknownRegion(),
                                           DrawRegionClip::DRAW, nsIntRegion(), WrManager()->GetPaintedLayerCallbackData());

    if (gfxPrefs::WebRenderHighlightPaintedLayers()) {
      dt->SetTransform(Matrix());
      dt->FillRect(Rect(0, 0, imageSize.width, imageSize.height), ColorPattern(Color(1.0, 0.0, 0.0, 0.5)));
    }

    recorder->Finish();

    AddToValidRegion(regionToPaint);

    wr::ByteBuffer bytes(recorder->mOutputStream.mLength, (uint8_t*)recorder->mOutputStream.mData);

    //XXX: We should switch to updating the blob image instead of adding a new one
    //     That will get rid of this discard bit
    if (mImageKey.isSome()) {
      WrManager()->AddImageKeyForDiscard(mImageKey.value());
    }
    mImageKey = Some(GetImageKey());
    WrBridge()->SendAddBlobImage(mImageKey.value(), imageSize, size.width * 4, dt->GetFormat(), bytes);
    mImageBounds = visibleRegion.GetBounds();
  } else {
    MOZ_ASSERT(GetInvalidRegion().IsEmpty());
  }

  ScrollingLayersHelper scroller(this, aBuilder, aSc);
  StackingContextHelper sc(aSc, aBuilder, this);
  LayerRect rect = Bounds();
  DumpLayerInfo("PaintedLayer", rect);

  aBuilder.PushImage(sc.ToRelativeWrRect(LayerRect(mImageBounds)),
                     sc.ToRelativeWrRect(rect),
                     wr::ImageRendering::Auto, mImageKey.value());
}

} // namespace layers
} // namespace mozilla
