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

namespace mozilla {
namespace layers {

WebRenderDisplayItemLayer::~WebRenderDisplayItemLayer()
{
  MOZ_COUNT_DTOR(WebRenderDisplayItemLayer);
  if (mKey.isSome()) {
    WrManager()->AddImageKeyForDiscard(mKey.value());
  }
  if (mExternalImageId) {
    WrBridge()->DeallocExternalImageId(mExternalImageId);
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
  }
  // else we have an empty transaction and just use the
  // old commands.

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

    if (!mExternalImageId) {
      MOZ_ASSERT(mImageClient);
      mExternalImageId = WrBridge()->AllocExternalImageIdForCompositable(mImageClient);
    }
    MOZ_ASSERT(mExternalImageId);
    MOZ_ASSERT(mImageClient->AsImageClientSingle());

    mKey = UpdateImageKey(mImageClient->AsImageClientSingle(),
                          aContainer,
                          mKey,
                          mExternalImageId);

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

  if (!mExternalImageId) {
    MOZ_ASSERT(mImageClient);
    mExternalImageId = WrBridge()->AllocExternalImageIdForCompositable(mImageClient);
  }

  bool snap;
  gfx::Rect bounds = mozilla::NSRectToRect(mItem->GetBounds(mBuilder, &snap),
                                      mItem->Frame()->PresContext()->AppUnitsPerDevPixel());
  gfx::IntSize imageSize = RoundedToInt(bounds.Size());

  UpdateImageHelper helper(mImageContainer, mImageClient, imageSize);
  const int32_t appUnitsPerDevPixel = mItem->Frame()->PresContext()->AppUnitsPerDevPixel();
  gfx::Point offset = NSPointToPoint(mItem->ToReferenceFrame(), appUnitsPerDevPixel);

  {
    RefPtr<gfx::DrawTarget> target = helper.GetDrawTarget();
    if (!target) {
      return false;
    }

    target->ClearRect(gfx::Rect(0, 0, imageSize.width, imageSize.height));
    RefPtr<gfxContext> context = gfxContext::CreateOrNull(target, offset);
    MOZ_ASSERT(context);

    nsRenderingContext ctx(context);
    mItem->Paint(mBuilder, &ctx);
  }

  if (!helper.UpdateImage()) {
    return false;
  }

  gfx::Rect dest = RelativeToParent(gfx::Rect(0, 0, imageSize.width, imageSize.height)) + offset;
  WrClipRegion clipRegion = aBuilder.BuildClipRegion(wr::ToWrRect(dest));
  WrImageKey key = GetImageKey();
  aParentCommands.AppendElement(layers::OpAddExternalImage(
                                mExternalImageId,
                                key));
  aBuilder.PushImage(wr::ToWrRect(dest),
                     clipRegion,
                     WrImageRendering::Auto,
                     key);

  return true;
}

} // namespace layers
} // namespace mozilla
