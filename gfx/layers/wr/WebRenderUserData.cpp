/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderUserData.h"

namespace mozilla {
namespace layers {

WebRenderBridgeChild*
WebRenderUserData::WrBridge() const
{
  return mWRManager->WrBridge();
}

WebRenderImageData::WebRenderImageData(WebRenderLayerManager* aWRManager)
  : WebRenderUserData(aWRManager)
{
}

WebRenderImageData::~WebRenderImageData()
{
  if (mKey) {
    mWRManager->AddImageKeyForDiscard(mKey.value());
  }

  if (mExternalImageId) {
    WrBridge()->DeallocExternalImageId(mExternalImageId.ref());
  }

  if (mPipelineId) {
    WrBridge()->RemovePipelineIdForAsyncCompositable(mPipelineId.ref());
  }
}

Maybe<wr::ImageKey>
WebRenderImageData::UpdateImageKey(ImageContainer* aContainer)
{
  CreateImageClientIfNeeded();
  CreateExternalImageIfNeeded();

  if (!mImageClient || !mExternalImageId) {
    return Nothing();
  }

  MOZ_ASSERT(mImageClient->AsImageClientSingle());
  MOZ_ASSERT(aContainer);

  ImageClientSingle* imageClient = mImageClient->AsImageClientSingle();
  uint32_t oldCounter = imageClient->GetLastUpdateGenerationCounter();

  bool ret = imageClient->UpdateImage(aContainer, /* unused */0);
  if (!ret || imageClient->IsEmpty()) {
    // Delete old key
    if (mKey) {
      mWRManager->AddImageKeyForDiscard(mKey.value());
      mKey = Nothing();
    }
    return Nothing();
  }

  // Reuse old key if generation is not updated.
  if (oldCounter == imageClient->GetLastUpdateGenerationCounter() && mKey) {
    return mKey;
  }

  // Delete old key, we are generating a new key.
  if (mKey) {
    mWRManager->AddImageKeyForDiscard(mKey.value());
  }

  WrImageKey key = WrBridge()->GetNextImageKey();
  mWRManager->WrBridge()->AddWebRenderParentCommand(OpAddExternalImage(mExternalImageId.value(), key));
  mKey = Some(key);

  return mKey;
}

void
WebRenderImageData::CreateAsyncImageWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                                      ImageContainer* aContainer,
                                                      const StackingContextHelper& aSc,
                                                      const LayerRect& aBounds,
                                                      const LayerRect& aSCBounds,
                                                      const Matrix4x4& aSCTransform,
                                                      const MaybeIntSize& aScaleToSize,
                                                      const WrImageRendering& aFilter,
                                                      const WrMixBlendMode& aMixBlendMode)
{
  MOZ_ASSERT(aContainer->IsAsync());
  if (!mPipelineId) {
    // Alloc async image pipeline id.
    mPipelineId = Some(WrBridge()->GetCompositorBridgeChild()->GetNextPipelineId());
    WrBridge()->AddPipelineIdForAsyncCompositable(mPipelineId.ref(),
                                                  aContainer->GetAsyncContainerHandle());
  }
  MOZ_ASSERT(!mImageClient);
  MOZ_ASSERT(!mExternalImageId);

  // Push IFrame for async image pipeline.
  //
  // We don't push a stacking context for this async image pipeline here.
  // Instead, we do it inside the iframe that hosts the image. As a result,
  // a bunch of the calculations normally done as part of that stacking
  // context need to be done manually and pushed over to the parent side,
  // where it will be done when we build the display list for the iframe.
  // That happens in WebRenderCompositableHolder.
  WrRect r = aSc.ToRelativeWrRect(aBounds);
  aBuilder.PushIFrame(r, r, mPipelineId.ref());

  WrBridge()->AddWebRenderParentCommand(OpUpdateAsyncImagePipeline(mPipelineId.value(),
                                                                   aSCBounds,
                                                                   aSCTransform,
                                                                   aScaleToSize,
                                                                   aFilter,
                                                                   aMixBlendMode));
}

void
WebRenderImageData::CreateImageClientIfNeeded()
{
  if (!mImageClient) {
    mImageClient = ImageClient::CreateImageClient(CompositableType::IMAGE,
                                                  WrBridge(),
                                                  TextureFlags::DEFAULT);
    if (!mImageClient) {
      return;
    }

    mImageClient->Connect();
  }
}

void
WebRenderImageData::CreateExternalImageIfNeeded()
{
  if (!mExternalImageId)  {
    mExternalImageId = Some(WrBridge()->AllocExternalImageIdForCompositable(mImageClient));
  }
}

} // namespace layers
} // namespace mozilla
