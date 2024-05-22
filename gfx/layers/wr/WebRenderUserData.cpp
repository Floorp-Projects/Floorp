/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderUserData.h"

#include "mozilla/image/WebRenderImageProvider.h"
#include "mozilla/layers/AnimationHelper.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ImageClient.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/layers/WebRenderMessages.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/SharedSurfacesChild.h"
#include "mozilla/webgpu/WebGPUChild.h"
#include "nsDisplayListInvalidation.h"
#include "nsIFrame.h"
#include "WebRenderCanvasRenderer.h"

using namespace mozilla::image;

namespace mozilla {
namespace layers {

void WebRenderBackgroundData::AddWebRenderCommands(
    wr::DisplayListBuilder& aBuilder) {
  aBuilder.PushRect(mBounds, mBounds, true, true, false, mColor);
}

/* static */
bool WebRenderUserData::SupportsAsyncUpdate(nsIFrame* aFrame) {
  if (!aFrame) {
    return false;
  }
  RefPtr<WebRenderImageData> data = GetWebRenderUserData<WebRenderImageData>(
      aFrame, static_cast<uint32_t>(DisplayItemType::TYPE_VIDEO));
  if (data) {
    return data->IsAsync();
  }

  return false;
}

/* static */
bool WebRenderUserData::ProcessInvalidateForImage(nsIFrame* aFrame,
                                                  DisplayItemType aType,
                                                  ImageProviderId aProviderId) {
  MOZ_ASSERT(aFrame);

  if (!aFrame->HasProperty(WebRenderUserDataProperty::Key())) {
    aFrame->SchedulePaint();
    return false;
  }

  auto type = static_cast<uint32_t>(aType);
  RefPtr<WebRenderFallbackData> fallback =
      GetWebRenderUserData<WebRenderFallbackData>(aFrame, type);
  if (fallback) {
    fallback->SetInvalid(true);
    aFrame->SchedulePaint();
    return true;
  }

  RefPtr<WebRenderImageProviderData> image =
      GetWebRenderUserData<WebRenderImageProviderData>(aFrame, type);
  if (image && image->Invalidate(aProviderId)) {
    return true;
  }

  aFrame->SchedulePaint();
  return false;
}

WebRenderUserData::WebRenderUserData(RenderRootStateManager* aManager,
                                     uint32_t aDisplayItemKey, nsIFrame* aFrame)
    : mManager(aManager),
      mFrame(aFrame),
      mDisplayItemKey(aDisplayItemKey),
      mTable(aManager->GetWebRenderUserDataTable()),
      mUsed(false) {}

WebRenderUserData::WebRenderUserData(RenderRootStateManager* aManager,
                                     nsDisplayItem* aItem)
    : mManager(aManager),
      mFrame(aItem->Frame()),
      mDisplayItemKey(aItem->GetPerFrameKey()),
      mTable(aManager->GetWebRenderUserDataTable()),
      mUsed(false) {}

WebRenderUserData::~WebRenderUserData() = default;

void WebRenderUserData::RemoveFromTable() { mTable->Remove(this); }

WebRenderBridgeChild* WebRenderUserData::WrBridge() const {
  return mManager->WrBridge();
}

WebRenderImageData::WebRenderImageData(RenderRootStateManager* aManager,
                                       nsDisplayItem* aItem)
    : WebRenderUserData(aManager, aItem) {}

WebRenderImageData::WebRenderImageData(RenderRootStateManager* aManager,
                                       uint32_t aDisplayItemKey,
                                       nsIFrame* aFrame)
    : WebRenderUserData(aManager, aDisplayItemKey, aFrame) {}

WebRenderImageData::~WebRenderImageData() {
  ClearImageKey();

  if (mPipelineId) {
    mManager->RemovePipelineIdForCompositable(mPipelineId.ref());
  }
}

void WebRenderImageData::ClearImageKey() {
  if (mKey) {
    mManager->AddImageKeyForDiscard(mKey.value());
    if (mTextureOfImage) {
      WrBridge()->ReleaseTextureOfImage(mKey.value());
      mTextureOfImage = nullptr;
    }
    mKey.reset();
  }
  MOZ_ASSERT(!mTextureOfImage);
}

Maybe<wr::ImageKey> WebRenderImageData::UpdateImageKey(
    ImageContainer* aContainer, wr::IpcResourceUpdateQueue& aResources,
    bool aFallback) {
  MOZ_ASSERT(aContainer);

  if (mContainer != aContainer) {
    mContainer = aContainer;
  }

  CreateImageClientIfNeeded();
  if (!mImageClient) {
    return Nothing();
  }

  MOZ_ASSERT(mImageClient->AsImageClientSingle());

  ImageClientSingle* imageClient = mImageClient->AsImageClientSingle();
  uint32_t oldCounter = imageClient->GetLastUpdateGenerationCounter();

  bool ret = imageClient->UpdateImage(aContainer);
  RefPtr<TextureClient> currentTexture = imageClient->GetForwardedTexture();
  if (!ret || !currentTexture) {
    // Delete old key
    ClearImageKey();
    return Nothing();
  }

  // Reuse old key if generation is not updated.
  if (!aFallback &&
      oldCounter == imageClient->GetLastUpdateGenerationCounter() && mKey) {
    return mKey;
  }

  // If we already had a texture and the format hasn't changed, better to reuse
  // the image keys than create new ones.
  bool useUpdate = mKey.isSome() && !!mTextureOfImage && !!currentTexture &&
                   mTextureOfImage->GetSize() == currentTexture->GetSize() &&
                   mTextureOfImage->GetFormat() == currentTexture->GetFormat();

  wr::MaybeExternalImageId extId = currentTexture->GetExternalImageKey();
  MOZ_RELEASE_ASSERT(extId.isSome());

  if (useUpdate) {
    MOZ_ASSERT(mKey.isSome());
    MOZ_ASSERT(mTextureOfImage);
    aResources.PushExternalImageForTexture(
        extId.ref(), mKey.ref(), currentTexture, /* aIsUpdate */ true);
  } else {
    ClearImageKey();
    wr::WrImageKey key = WrBridge()->GetNextImageKey();
    aResources.PushExternalImageForTexture(extId.ref(), key, currentTexture,
                                           /* aIsUpdate */ false);
    mKey = Some(key);
  }

  mTextureOfImage = currentTexture;
  return mKey;
}

already_AddRefed<ImageClient> WebRenderImageData::GetImageClient() {
  RefPtr<ImageClient> imageClient = mImageClient;
  return imageClient.forget();
}

void WebRenderImageData::CreateAsyncImageWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder, ImageContainer* aContainer,
    const StackingContextHelper& aSc, const LayoutDeviceRect& aBounds,
    const LayoutDeviceRect& aSCBounds, wr::WrRotation aRotation,
    const wr::ImageRendering& aFilter, const wr::MixBlendMode& aMixBlendMode,
    bool aIsBackfaceVisible) {
  MOZ_ASSERT(aContainer->IsAsync());

  if (mPipelineId.isSome() && mContainer != aContainer) {
    // In this case, we need to remove the existed pipeline and create new one
    // because the ImageContainer is changed.
    WrBridge()->RemovePipelineIdForCompositable(mPipelineId.ref());
    mPipelineId.reset();
  }

  if (!mPipelineId) {
    // Alloc async image pipeline id.
    mPipelineId =
        Some(WrBridge()->GetCompositorBridgeChild()->GetNextPipelineId());
    WrBridge()->AddPipelineIdForCompositable(
        mPipelineId.ref(), aContainer->GetAsyncContainerHandle(),
        CompositableHandleOwner::ImageBridge);
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
  aBuilder.PushIFrame(aBounds, aIsBackfaceVisible, mPipelineId.ref(),
                      /*ignoreMissingPipelines*/ false);

  WrBridge()->AddWebRenderParentCommand(OpUpdateAsyncImagePipeline(
      mPipelineId.value(), aSCBounds, aRotation, aFilter, aMixBlendMode));
}

void WebRenderImageData::CreateImageClientIfNeeded() {
  if (!mImageClient) {
    mImageClient = ImageClient::CreateImageClient(
        CompositableType::IMAGE, ImageUsageType::WebRenderImageData, WrBridge(),
        TextureFlags::DEFAULT);
    if (!mImageClient) {
      return;
    }

    mImageClient->Connect();
  }
}

WebRenderImageProviderData::WebRenderImageProviderData(
    RenderRootStateManager* aManager, nsDisplayItem* aItem)
    : WebRenderUserData(aManager, aItem) {}

WebRenderImageProviderData::WebRenderImageProviderData(
    RenderRootStateManager* aManager, uint32_t aDisplayItemKey,
    nsIFrame* aFrame)
    : WebRenderUserData(aManager, aDisplayItemKey, aFrame) {}

WebRenderImageProviderData::~WebRenderImageProviderData() = default;

Maybe<wr::ImageKey> WebRenderImageProviderData::UpdateImageKey(
    WebRenderImageProvider* aProvider, ImgDrawResult aDrawResult,
    wr::IpcResourceUpdateQueue& aResources) {
  if (mProvider != aProvider) {
    mProvider = aProvider;
  }

  wr::ImageKey key = {};
  nsresult rv = mProvider ? mProvider->UpdateKey(mManager, aResources, key)
                          : NS_ERROR_FAILURE;
  mKey = NS_SUCCEEDED(rv) ? Some(key) : Nothing();
  mDrawResult = aDrawResult;
  return mKey;
}

bool WebRenderImageProviderData::Invalidate(ImageProviderId aProviderId) const {
  if (!aProviderId || !mProvider || mProvider->GetProviderId() != aProviderId ||
      !mKey) {
    return false;
  }

  if (mDrawResult != ImgDrawResult::SUCCESS &&
      mDrawResult != ImgDrawResult::BAD_IMAGE) {
    return false;
  }

  wr::ImageKey key = {};
  nsresult rv =
      mProvider->UpdateKey(mManager, mManager->AsyncResourceUpdates(), key);
  return NS_SUCCEEDED(rv) && mKey.ref() == key;
}

WebRenderFallbackData::WebRenderFallbackData(RenderRootStateManager* aManager,
                                             nsDisplayItem* aItem)
    : WebRenderUserData(aManager, aItem), mOpacity(1.0f), mInvalid(false) {}

WebRenderFallbackData::~WebRenderFallbackData() { ClearImageKey(); }

void WebRenderFallbackData::SetBlobImageKey(const wr::BlobImageKey& aKey) {
  ClearImageKey();
  mBlobKey = Some(aKey);
}

Maybe<wr::ImageKey> WebRenderFallbackData::GetImageKey() {
  if (mBlobKey) {
    return Some(wr::AsImageKey(mBlobKey.value()));
  }

  if (mImageData) {
    return mImageData->GetImageKey();
  }

  return Nothing();
}

void WebRenderFallbackData::ClearImageKey() {
  if (mImageData) {
    mImageData->ClearImageKey();
    mImageData = nullptr;
  }

  if (mBlobKey) {
    mManager->AddBlobImageKeyForDiscard(mBlobKey.value());
    mBlobKey.reset();
  }
}

WebRenderImageData* WebRenderFallbackData::PaintIntoImage() {
  if (mBlobKey) {
    mManager->AddBlobImageKeyForDiscard(mBlobKey.value());
    mBlobKey.reset();
  }

  if (mImageData) {
    return mImageData.get();
  }

  mImageData = MakeAndAddRef<WebRenderImageData>(mManager.get(),
                                                 mDisplayItemKey, mFrame);

  return mImageData.get();
}

WebRenderAPZAnimationData::WebRenderAPZAnimationData(
    RenderRootStateManager* aManager, nsDisplayItem* aItem)
    : WebRenderUserData(aManager, aItem),
      mAnimationId(AnimationHelper::GetNextCompositorAnimationsId()) {}

WebRenderAnimationData::WebRenderAnimationData(RenderRootStateManager* aManager,
                                               nsDisplayItem* aItem)
    : WebRenderUserData(aManager, aItem) {}

WebRenderAnimationData::~WebRenderAnimationData() {
  // It may be the case that nsDisplayItem that created this WebRenderUserData
  // gets destroyed without getting a chance to discard the compositor animation
  // id, so we should do it as part of cleanup here.
  uint64_t animationId = mAnimationInfo.GetCompositorAnimationsId();
  // animationId might be 0 if mAnimationInfo never held any active animations.
  if (animationId) {
    mManager->AddCompositorAnimationsIdForDiscard(animationId);
  }
}

WebRenderCanvasData::WebRenderCanvasData(RenderRootStateManager* aManager,
                                         nsDisplayItem* aItem)
    : WebRenderUserData(aManager, aItem) {}

WebRenderCanvasData::~WebRenderCanvasData() {
  if (mCanvasRenderer) {
    mCanvasRenderer->ClearCachedResources();
  }
}

void WebRenderCanvasData::ClearCanvasRenderer() { mCanvasRenderer = nullptr; }

WebRenderCanvasRendererAsync* WebRenderCanvasData::GetCanvasRenderer() {
  return mCanvasRenderer.get();
}

WebRenderCanvasRendererAsync* WebRenderCanvasData::CreateCanvasRenderer() {
  mCanvasRenderer = new WebRenderCanvasRendererAsync(mManager);
  return mCanvasRenderer.get();
}

bool WebRenderCanvasData::SetCanvasRenderer(CanvasRenderer* aCanvasRenderer) {
  if (!aCanvasRenderer || !aCanvasRenderer->AsWebRenderCanvasRendererAsync()) {
    return false;
  }

  auto* renderer = aCanvasRenderer->AsWebRenderCanvasRendererAsync();
  if (mManager != renderer->GetRenderRootStateManager()) {
    return false;
  }

  mCanvasRenderer = renderer;
  return true;
}

void WebRenderCanvasData::SetImageContainer(ImageContainer* aImageContainer) {
  mContainer = aImageContainer;
}

ImageContainer* WebRenderCanvasData::GetImageContainer() {
  if (!mContainer) {
    mContainer = MakeAndAddRef<ImageContainer>(ImageUsageType::Canvas,
                                               ImageContainer::SYNCHRONOUS);
  }
  return mContainer;
}

void WebRenderCanvasData::ClearImageContainer() { mContainer = nullptr; }

WebRenderRemoteData::WebRenderRemoteData(RenderRootStateManager* aManager,
                                         nsDisplayItem* aItem)
    : WebRenderUserData(aManager, aItem) {}

WebRenderRemoteData::~WebRenderRemoteData() {
  if (mRemoteBrowser) {
    mRemoteBrowser->UpdateEffects(mozilla::dom::EffectsInfo::FullyHidden());
  }
}

void DestroyWebRenderUserDataTable(WebRenderUserDataTable* aTable) {
  for (const auto& value : aTable->Values()) {
    value->RemoveFromTable();
  }
  delete aTable;
}

}  // namespace layers
}  // namespace mozilla
