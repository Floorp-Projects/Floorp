/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderDisplayItemLayer.h"

#include "LayersLogging.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "nsDisplayList.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/layers/UpdateImageHelper.h"
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
WebRenderDisplayItemLayer::RenderLayer(wr::DisplayListBuilder& aBuilder)
{
  if (mVisibleRegion.IsEmpty()) {
    return;
  }

  Maybe<WrImageMask> mask = BuildWrMaskLayer(false);
  WrImageMask* imageMask = mask.ptrOr(nullptr);
  if (imageMask) {
    gfx::Rect rect = TransformedVisibleBoundsRelativeToParent();
    gfx::Rect clip(0.0, 0.0, rect.width, rect.height);
    aBuilder.PushClip(wr::ToWrRect(clip), imageMask);
  }

  if (mItem) {
    wr::DisplayListBuilder builder(WrBridge()->GetPipeline());
    // We might have recycled this layer. Throw away the old commands.
    mParentCommands.Clear();
    mItem->CreateWebRenderCommands(builder, mParentCommands, this);
    mBuiltDisplayList = builder.Finalize();
  } else {
    // else we have an empty transaction and just use the
    // old commands.
    WebRenderLayerManager* manager = static_cast<WebRenderLayerManager*>(Manager());
    MOZ_ASSERT(manager);

    // Since our recording relies on our parent layer's transform and stacking context
    // If this layer or our parent changed, this empty transaction won't work.
    if (manager->IsMutatedLayer(this) || manager->IsMutatedLayer(GetParent())) {
      manager->SetTransactionIncomplete();
      return;
    }
  }

  aBuilder.PushBuiltDisplayList(Move(mBuiltDisplayList));
  WrBridge()->AddWebRenderParentCommands(mParentCommands);

  if (imageMask) {
    aBuilder.PopClip();
  }
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
WebRenderDisplayItemLayer::PushItemAsImage(wr::DisplayListBuilder& aBuilder,
                                           nsTArray<layers::WebRenderParentCommand>& aParentCommands)
{
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
    MOZ_ASSERT(mImageClient);
    mExternalImageId = Some(WrBridge()->AllocExternalImageIdForCompositable(mImageClient));
  }

  const int32_t appUnitsPerDevPixel = mItem->Frame()->PresContext()->AppUnitsPerDevPixel();

  bool snap;
  LayerRect bounds = ViewAs<LayerPixel>(
      LayoutDeviceRect::FromAppUnits(mItem->GetBounds(mBuilder, &snap), appUnitsPerDevPixel),
      PixelCastJustification::WebRenderHasUnitResolution);
  LayerIntSize imageSize = RoundedToInt(bounds.Size());
  LayerRect imageRect;
  imageRect.SizeTo(LayerSize(imageSize));

  UpdateImageHelper helper(mImageContainer, mImageClient, imageSize.ToUnknownSize());
  LayerPoint offset = ViewAs<LayerPixel>(
      LayoutDevicePoint::FromAppUnits(mItem->ToReferenceFrame(), appUnitsPerDevPixel),
      PixelCastJustification::WebRenderHasUnitResolution);

  {
    RefPtr<gfx::DrawTarget> target = helper.GetDrawTarget();
    if (!target) {
      return false;
    }

    target->ClearRect(imageRect.ToUnknownRect());
    RefPtr<gfxContext> context = gfxContext::CreateOrNull(target, offset.ToUnknownPoint());
    MOZ_ASSERT(context);

    nsRenderingContext ctx(context);
    mItem->Paint(mBuilder, &ctx);
  }

  if (!helper.UpdateImage()) {
    return false;
  }

  gfx::Rect dest = RelativeToParent(imageRect.ToUnknownRect()) + offset.ToUnknownPoint();
  WrClipRegion clipRegion = aBuilder.BuildClipRegion(wr::ToWrRect(dest));
  WrImageKey key = GetImageKey();
  aParentCommands.AppendElement(layers::OpAddExternalImage(
                                mExternalImageId.value(),
                                key));
  aBuilder.PushImage(wr::ToWrRect(dest),
                     clipRegion,
                     WrImageRendering::Auto,
                     key);

  return true;
}

} // namespace layers
} // namespace mozilla
