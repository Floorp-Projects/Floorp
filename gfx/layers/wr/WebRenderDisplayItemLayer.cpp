/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderDisplayItemLayer.h"

#include "LayersLogging.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/layers/ScrollingLayersHelper.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "nsDisplayList.h"
#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/gfx/Matrix.h"
#include "UnitTransforms.h"

namespace mozilla {
namespace layers {

WebRenderDisplayItemLayer::~WebRenderDisplayItemLayer()
{
  MOZ_COUNT_DTOR(WebRenderDisplayItemLayer);
  if (mKey.isSome()) {
    WrManager()->AddImageKeyForDiscard(mKey.value());
  }
  if (mExternalImageId.isSome()) {
    WrBridge()->DeallocExternalImageId(mExternalImageId.ref());
  }
}

void
WebRenderDisplayItemLayer::RenderLayer(wr::DisplayListBuilder& aBuilder,
                                       const StackingContextHelper& aSc)
{
  if (mVisibleRegion.IsEmpty()) {
    return;
  }

  ScrollingLayersHelper scroller(this, aBuilder, aSc);

  if (mItem) {
    WrSize contentSize; // this won't actually be used by anything
    wr::DisplayListBuilder builder(WrBridge()->GetPipeline(), contentSize);
    // We might have recycled this layer. Throw away the old commands.
    mParentCommands.Clear();
    mItem->CreateWebRenderCommands(builder, aSc, mParentCommands, this);
    builder.Finalize(contentSize, mBuiltDisplayList);
  } else {
    // else we have an empty transaction and just use the
    // old commands.
    WebRenderLayerManager* manager = WrManager();
    MOZ_ASSERT(manager);

    // Since our recording relies on our parent layer's transform and stacking context
    // If this layer or our parent changed, this empty transaction won't work.
    if (manager->IsMutatedLayer(this) || manager->IsMutatedLayer(GetParent())) {
      manager->SetTransactionIncomplete();
      return;
    }
  }

  aBuilder.PushBuiltDisplayList(mBuiltDisplayList);
  WrBridge()->AddWebRenderParentCommands(mParentCommands);
}

Maybe<wr::ImageKey>
WebRenderDisplayItemLayer::SendImageContainer(ImageContainer* aContainer,
                                              nsTArray<layers::WebRenderParentCommand>& aParentCommands)
{
  MOZ_ASSERT(aContainer);

  if (mImageContainer != aContainer) {
    AutoLockImage autoLock(aContainer);
    Image* image = autoLock.GetImage();
    if (!image) {
      return Nothing();
    }

    if (!mImageClient) {
      mImageClient = ImageClient::CreateImageClient(CompositableType::IMAGE,
                                                    WrBridge(),
                                                    TextureFlags::DEFAULT);
      if (!mImageClient) {
        return Nothing();
      }
      mImageClient->Connect();
    }

    if (mExternalImageId.isNothing()) {
      MOZ_ASSERT(mImageClient);
      mExternalImageId = Some(WrBridge()->AllocExternalImageIdForCompositable(mImageClient));
    }
    MOZ_ASSERT(mExternalImageId.isSome());
    MOZ_ASSERT(mImageClient->AsImageClientSingle());

    mKey = UpdateImageKey(mImageClient->AsImageClientSingle(),
                          aContainer,
                          mKey,
                          mExternalImageId.ref());

    mImageContainer = aContainer;
  }

  return mKey;
}

bool
WebRenderDisplayItemLayer::PushItemAsBlobImage(wr::DisplayListBuilder& aBuilder,
                                               const StackingContextHelper& aSc)
{
  const int32_t appUnitsPerDevPixel = mItem->Frame()->PresContext()->AppUnitsPerDevPixel();

  bool snap;
  LayerRect bounds = ViewAs<LayerPixel>(
      LayoutDeviceRect::FromAppUnits(mItem->GetBounds(mBuilder, &snap), appUnitsPerDevPixel),
      PixelCastJustification::WebRenderHasUnitResolution);
  LayerIntSize imageSize = RoundedToInt(bounds.Size());
  LayerRect imageRect;
  imageRect.SizeTo(LayerSize(imageSize));

  RefPtr<gfx::DrawEventRecorderMemory> recorder = MakeAndAddRef<gfx::DrawEventRecorderMemory>();
  RefPtr<gfx::DrawTarget> dummyDt =
    gfx::Factory::CreateDrawTarget(gfx::BackendType::SKIA, IntSize(1, 1), gfx::SurfaceFormat::B8G8R8X8);
  RefPtr<gfx::DrawTarget> dt = gfx::Factory::CreateRecordingDrawTarget(recorder, dummyDt, imageSize.ToUnknownSize());
  LayerPoint offset = ViewAs<LayerPixel>(
      LayoutDevicePoint::FromAppUnits(mItem->ToReferenceFrame(), appUnitsPerDevPixel),
      PixelCastJustification::WebRenderHasUnitResolution);

  {
    dt->ClearRect(imageRect.ToUnknownRect());
    RefPtr<gfxContext> context = gfxContext::CreateOrNull(dt, offset.ToUnknownPoint());
    MOZ_ASSERT(context);

    mItem->Paint(mBuilder, context);
  }

  wr::ByteBuffer bytes(recorder->mOutputStream.mLength, (uint8_t*)recorder->mOutputStream.mData);

  WrRect dest = aSc.ToRelativeWrRect(imageRect + offset);
  WrClipRegionToken clipRegion = aBuilder.PushClipRegion(dest);
  WrImageKey key = GetImageKey();
  WrBridge()->SendAddBlobImage(key, imageSize.ToUnknownSize(), imageSize.width * 4, dt->GetFormat(), bytes);
  WrManager()->AddImageKeyForDiscard(key);

  aBuilder.PushImage(dest,
                     clipRegion,
                     wr::ImageRendering::Auto,
                     key);
  return true;
}

} // namespace layers
} // namespace mozilla
