/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderUserData.h"

#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ImageClient.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/WebRenderMessages.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/SharedSurfacesChild.h"
#include "nsDisplayListInvalidation.h"
#include "nsIFrame.h"
#include "WebRenderCanvasRenderer.h"

namespace mozilla {
namespace layers {

void
WebRenderBackgroundData::AddWebRenderCommands(wr::DisplayListBuilder& aBuilder)
{
  aBuilder.PushRect(mBounds,
                    mBounds,
                    true,
                    mColor);
}

/* static */ bool
WebRenderUserData::SupportsAsyncUpdate(nsIFrame* aFrame)
{
  if (!aFrame) {
    return false;
  }
  RefPtr<WebRenderImageData> data = GetWebRenderUserData<WebRenderImageData>(aFrame, static_cast<uint32_t>(DisplayItemType::TYPE_VIDEO));
  if (data) {
    return data->IsAsync();
  }

  return false;
}

WebRenderUserData::WebRenderUserData(WebRenderLayerManager* aWRManager, nsDisplayItem* aItem)
  : mWRManager(aWRManager)
  , mFrame(aItem->Frame())
  , mDisplayItemKey(aItem->GetPerFrameKey())
  , mTable(aWRManager->GetWebRenderUserDataTable())
  , mUsed(false)
{
}

WebRenderUserData::~WebRenderUserData()
{
}

void
WebRenderUserData::RemoveFromTable()
{
  mTable->RemoveEntry(this);
}

WebRenderBridgeChild*
WebRenderUserData::WrBridge() const
{
  return mWRManager->WrBridge();
}

WebRenderImageData::WebRenderImageData(WebRenderLayerManager* aWRManager, nsDisplayItem* aItem)
  : WebRenderUserData(aWRManager, aItem)
  , mOwnsKey(false)
{
}

WebRenderImageData::~WebRenderImageData()
{
  ClearImageKey();

  if (mPipelineId) {
    WrBridge()->RemovePipelineIdForCompositable(mPipelineId.ref());
  }
}

void
WebRenderImageData::ClearImageKey()
{
  if (mKey) {
    // If we don't own the key, then the owner is responsible for discarding the
    // key when appropriate.
    if (mOwnsKey) {
      mWRManager->AddImageKeyForDiscard(mKey.value());
      if (mTextureOfImage) {
        WrBridge()->ReleaseTextureOfImage(mKey.value());
        mTextureOfImage = nullptr;
      }
    }
    mKey.reset();
  }
  mOwnsKey = false;
  MOZ_ASSERT(!mTextureOfImage);
}

Maybe<wr::ImageKey>
WebRenderImageData::UpdateImageKey(ImageContainer* aContainer,
                                   wr::IpcResourceUpdateQueue& aResources,
                                   bool aFallback)
{
  MOZ_ASSERT(aContainer);

  if (mContainer != aContainer) {
    mContainer = aContainer;
  }

  wr::WrImageKey key;
  if (!aFallback) {
    nsresult rv = SharedSurfacesChild::Share(aContainer, mWRManager, aResources, key);
    if (NS_SUCCEEDED(rv)) {
      // Ensure that any previously owned keys are released before replacing. We
      // don't own this key, the surface itself owns it, so that it can be shared
      // across multiple elements.
      ClearImageKey();
      mKey = Some(key);
      return mKey;
    }

    if (rv != NS_ERROR_NOT_IMPLEMENTED) {
      // We should be using the shared surface but somehow sharing it failed.
      ClearImageKey();
      return Nothing();
    }
  }

  CreateImageClientIfNeeded();
  if (!mImageClient) {
    return Nothing();
  }

  MOZ_ASSERT(mImageClient->AsImageClientSingle());

  ImageClientSingle* imageClient = mImageClient->AsImageClientSingle();
  uint32_t oldCounter = imageClient->GetLastUpdateGenerationCounter();

  bool ret = imageClient->UpdateImage(aContainer, /* unused */0);
  RefPtr<TextureClient> currentTexture = imageClient->GetForwardedTexture();
  if (!ret || !currentTexture) {
    // Delete old key
    ClearImageKey();
    return Nothing();
  }

  // Reuse old key if generation is not updated.
  if (!aFallback && oldCounter == imageClient->GetLastUpdateGenerationCounter() && mKey) {
    return mKey;
  }

  // If we already had a texture and the format hasn't changed, better to reuse the image keys
  // than create new ones.
  bool useUpdate = mKey.isSome()
                   && !!mTextureOfImage
                   && !!currentTexture
                   && mTextureOfImage->GetSize() == currentTexture->GetSize()
                   && mTextureOfImage->GetFormat() == currentTexture->GetFormat();

  wr::MaybeExternalImageId extId = currentTexture->GetExternalImageKey();
  MOZ_RELEASE_ASSERT(extId.isSome());

  if (useUpdate) {
    MOZ_ASSERT(mKey.isSome());
    MOZ_ASSERT(mTextureOfImage);
    aResources.PushExternalImageForTexture(extId.ref(), mKey.ref(), currentTexture, /* aIsUpdate */ true);
  } else {
    ClearImageKey();
    key = WrBridge()->GetNextImageKey();
    aResources.PushExternalImageForTexture(extId.ref(), key, currentTexture, /* aIsUpdate */ false);
    mKey = Some(key);
  }

  mTextureOfImage = currentTexture;
  mOwnsKey = true;

  return mKey;
}

void
WebRenderImageData::SetKey(const wr::ImageKey& aKey)
{
  MOZ_ASSERT_IF(mKey, mKey.value() != aKey);
  ClearImageKey();
  mKey = Some(aKey);
  mOwnsKey = true;
}

already_AddRefed<ImageClient>
WebRenderImageData::GetImageClient()
{
  RefPtr<ImageClient> imageClient = mImageClient;
  return imageClient.forget();
}

void
WebRenderImageData::CreateAsyncImageWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                                      ImageContainer* aContainer,
                                                      const StackingContextHelper& aSc,
                                                      const LayoutDeviceRect& aBounds,
                                                      const LayoutDeviceRect& aSCBounds,
                                                      const gfx::Matrix4x4& aSCTransform,
                                                      const gfx::MaybeIntSize& aScaleToSize,
                                                      const wr::ImageRendering& aFilter,
                                                      const wr::MixBlendMode& aMixBlendMode,
                                                      bool aIsBackfaceVisible)
{
  MOZ_ASSERT(aContainer->IsAsync());

  if (mPipelineId.isSome() && mContainer != aContainer) {
    // In this case, we need to remove the existed pipeline and create new one
    // because the ImageContainer is changed.
    WrBridge()->RemovePipelineIdForCompositable(mPipelineId.ref());
    mPipelineId.reset();
  }

  if (!mPipelineId) {
    // Alloc async image pipeline id.
    mPipelineId = Some(WrBridge()->GetCompositorBridgeChild()->GetNextPipelineId());
    WrBridge()->AddPipelineIdForAsyncCompositable(mPipelineId.ref(),
                                                  aContainer->GetAsyncContainerHandle());
    mContainer = aContainer;
  }
  MOZ_ASSERT(!mImageClient);

  // Push IFrame for async image pipeline.
  //
  // We don't push a stacking context for this async image pipeline here.
  // Instead, we do it inside the iframe that hosts the image. As a result,
  // a bunch of the calculations normally done as part of that stacking
  // context need to be done manually and pushed over to the parent side,
  // where it will be done when we build the display list for the iframe.
  // That happens in AsyncImagePipelineManager.
  wr::LayoutRect r = wr::ToRoundedLayoutRect(aBounds);
  aBuilder.PushIFrame(r, aIsBackfaceVisible, mPipelineId.ref(), /*ignoreMissingPipelines*/ false);

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

WebRenderFallbackData::WebRenderFallbackData(WebRenderLayerManager* aWRManager, nsDisplayItem* aItem)
  : WebRenderImageData(aWRManager, aItem)
  , mInvalid(false)
{
}

WebRenderFallbackData::~WebRenderFallbackData()
{
}

nsDisplayItemGeometry*
WebRenderFallbackData::GetGeometry()
{
  return mGeometry.get();
}

void
WebRenderFallbackData::SetGeometry(nsAutoPtr<nsDisplayItemGeometry> aGeometry)
{
  mGeometry = aGeometry;
}

WebRenderAnimationData::WebRenderAnimationData(WebRenderLayerManager* aWRManager, nsDisplayItem* aItem)
  : WebRenderUserData(aWRManager, aItem)
  , mAnimationInfo(aWRManager)
{
}

WebRenderAnimationData::~WebRenderAnimationData()
{
  // It may be the case that nsDisplayItem that created this WebRenderUserData
  // gets destroyed without getting a chance to discard the compositor animation
  // id, so we should do it as part of cleanup here.
  uint64_t animationId = mAnimationInfo.GetCompositorAnimationsId();
  // animationId might be 0 if mAnimationInfo never held any active animations.
  if (animationId) {
    mWRManager->AddCompositorAnimationsIdForDiscard(animationId);
  }
}

WebRenderCanvasData::WebRenderCanvasData(WebRenderLayerManager* aWRManager, nsDisplayItem* aItem)
  : WebRenderUserData(aWRManager, aItem)
{
}

WebRenderCanvasData::~WebRenderCanvasData()
{
  if (mCanvasRenderer) {
    mCanvasRenderer->ClearCachedResources();
  }
}

void
WebRenderCanvasData::ClearCanvasRenderer()
{
  mCanvasRenderer = nullptr;
}

WebRenderCanvasRendererAsync*
WebRenderCanvasData::GetCanvasRenderer()
{
  return mCanvasRenderer.get();
}

WebRenderCanvasRendererAsync*
WebRenderCanvasData::CreateCanvasRenderer()
{
  mCanvasRenderer = MakeUnique<WebRenderCanvasRendererAsync>(mWRManager);
  return mCanvasRenderer.get();
}

void
DestroyWebRenderUserDataTable(WebRenderUserDataTable* aTable)
{
  for (auto iter = aTable->Iter(); !iter.Done(); iter.Next()) {
    iter.UserData()->RemoveFromTable();
  }
  delete aTable;
}


} // namespace layers
} // namespace mozilla
