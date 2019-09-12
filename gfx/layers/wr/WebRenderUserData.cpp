/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderUserData.h"

#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ImageClient.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/layers/WebRenderMessages.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/SharedSurfacesChild.h"
#include "nsDisplayListInvalidation.h"
#include "nsIFrame.h"
#include "WebRenderCanvasRenderer.h"

namespace mozilla {
namespace layers {

void WebRenderBackgroundData::AddWebRenderCommands(
    wr::DisplayListBuilder& aBuilder) {
  aBuilder.PushRect(mBounds, mBounds, true, mColor);
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
bool WebRenderUserData::ProcessInvalidateForImage(
    nsIFrame* aFrame, DisplayItemType aType, ContainerProducerID aProducerId) {
  MOZ_ASSERT(aFrame);

  if (!aFrame->HasProperty(WebRenderUserDataProperty::Key())) {
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

  RefPtr<WebRenderImageData> image =
      GetWebRenderUserData<WebRenderImageData>(aFrame, type);
  if (image && image->UsingSharedSurface(aProducerId)) {
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

WebRenderUserData::~WebRenderUserData() {}

void WebRenderUserData::RemoveFromTable() { mTable->RemoveEntry(this); }

WebRenderBridgeChild* WebRenderUserData::WrBridge() const {
  return mManager->WrBridge();
}

WebRenderImageData::WebRenderImageData(RenderRootStateManager* aManager,
                                       nsDisplayItem* aItem)
    : WebRenderUserData(aManager, aItem), mOwnsKey(false) {}

WebRenderImageData::WebRenderImageData(RenderRootStateManager* aManager,
                                       uint32_t aDisplayItemKey,
                                       nsIFrame* aFrame)
    : WebRenderUserData(aManager, aDisplayItemKey, aFrame), mOwnsKey(false) {}

WebRenderImageData::~WebRenderImageData() {
  ClearImageKey();

  if (mPipelineId) {
    mManager->RemovePipelineIdForCompositable(mPipelineId.ref());
  }
}

bool WebRenderImageData::UsingSharedSurface(
    ContainerProducerID aProducerId) const {
  if (!mContainer || !mKey || mOwnsKey) {
    return false;
  }

  // If this is just an update with the same image key, then we know that the
  // share request initiated an asynchronous update so that we don't need to
  // rebuild the scene.
  wr::ImageKey key;
  nsresult rv = SharedSurfacesChild::Share(
      mContainer, mManager, mManager->AsyncResourceUpdates(), key, aProducerId);
  return NS_SUCCEEDED(rv) && mKey.ref() == key;
}

void WebRenderImageData::ClearImageKey() {
  if (mKey) {
    // If we don't own the key, then the owner is responsible for discarding the
    // key when appropriate.
    if (mOwnsKey) {
      mManager->AddImageKeyForDiscard(mKey.value());
      if (mTextureOfImage) {
        WrBridge()->ReleaseTextureOfImage(mKey.value(),
                                          mManager->GetRenderRoot());
        mTextureOfImage = nullptr;
      }
    }
    mKey.reset();
  }
  mOwnsKey = false;
  MOZ_ASSERT(!mTextureOfImage);
}

Maybe<wr::ImageKey> WebRenderImageData::UpdateImageKey(
    ImageContainer* aContainer, wr::IpcResourceUpdateQueue& aResources,
    bool aFallback) {
  MOZ_ASSERT(aContainer);

  if (mContainer != aContainer) {
    mContainer = aContainer;
  }

  wr::WrImageKey key;
  if (!aFallback) {
    nsresult rv = SharedSurfacesChild::Share(aContainer, mManager, aResources,
                                             key, kContainerProducerID_Invalid);
    if (NS_SUCCEEDED(rv)) {
      // Ensure that any previously owned keys are released before replacing. We
      // don't own this key, the surface itself owns it, so that it can be
      // shared across multiple elements.
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

  bool ret = imageClient->UpdateImage(aContainer, /* unused */ 0,
                                      Some(mManager->GetRenderRoot()));
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
    key = WrBridge()->GetNextImageKey();
    aResources.PushExternalImageForTexture(extId.ref(), key, currentTexture,
                                           /* aIsUpdate */ false);
    mKey = Some(key);
  }

  mTextureOfImage = currentTexture;
  mOwnsKey = true;

  return mKey;
}

already_AddRefed<ImageClient> WebRenderImageData::GetImageClient() {
  RefPtr<ImageClient> imageClient = mImageClient;
  return imageClient.forget();
}

void WebRenderImageData::CreateAsyncImageWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder, ImageContainer* aContainer,
    const StackingContextHelper& aSc, const LayoutDeviceRect& aBounds,
    const LayoutDeviceRect& aSCBounds, const gfx::Matrix4x4& aSCTransform,
    const gfx::MaybeIntSize& aScaleToSize, const wr::ImageRendering& aFilter,
    const wr::MixBlendMode& aMixBlendMode, bool aIsBackfaceVisible) {
  MOZ_ASSERT(aContainer->IsAsync());

  if (mPipelineId.isSome() && mContainer != aContainer) {
    // In this case, we need to remove the existed pipeline and create new one
    // because the ImageContainer is changed.
    WrBridge()->RemovePipelineIdForCompositable(mPipelineId.ref(),
                                                mManager->GetRenderRoot());
    mPipelineId.reset();
  }

  if (!mPipelineId) {
    // Alloc async image pipeline id.
    mPipelineId =
        Some(WrBridge()->GetCompositorBridgeChild()->GetNextPipelineId());
    WrBridge()->AddPipelineIdForAsyncCompositable(
        mPipelineId.ref(), aContainer->GetAsyncContainerHandle(),
        mManager->GetRenderRoot());
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
  aBuilder.PushIFrame(r, aIsBackfaceVisible, mPipelineId.ref(),
                      /*ignoreMissingPipelines*/ false);

  WrBridge()->AddWebRenderParentCommand(
      OpUpdateAsyncImagePipeline(mPipelineId.value(), aSCBounds, aSCTransform,
                                 aScaleToSize, aFilter, aMixBlendMode),
      mManager->GetRenderRoot());
}

void WebRenderImageData::CreateImageClientIfNeeded() {
  if (!mImageClient) {
    mImageClient = ImageClient::CreateImageClient(
        CompositableType::IMAGE, WrBridge(), TextureFlags::DEFAULT);
    if (!mImageClient) {
      return;
    }

    mImageClient->Connect();
  }
}

WebRenderFallbackData::WebRenderFallbackData(RenderRootStateManager* aManager,
                                             nsDisplayItem* aItem)
    : WebRenderUserData(aManager, aItem), mInvalid(false) {}

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
  mCanvasRenderer = MakeUnique<WebRenderCanvasRendererAsync>(mManager);
  return mCanvasRenderer.get();
}

WebRenderRemoteData::WebRenderRemoteData(RenderRootStateManager* aManager,
                                         nsDisplayItem* aItem)
    : WebRenderUserData(aManager, aItem) {}

WebRenderRemoteData::~WebRenderRemoteData() {
  if (mRemoteBrowser) {
    mRemoteBrowser->UpdateEffects(mozilla::dom::EffectsInfo::FullyHidden());
  }
}

WebRenderRenderRootData::WebRenderRenderRootData(
    RenderRootStateManager* aManager, nsDisplayItem* aItem)
    : WebRenderUserData(aManager, aItem) {}

RenderRootBoundary& WebRenderRenderRootData::EnsureHasBoundary(
    wr::RenderRoot aChildType) {
  if (mBoundary) {
    MOZ_ASSERT(mBoundary->GetChildType() == aChildType);
  } else {
    mBoundary.emplace(aChildType);
  }
  return mBoundary.ref();
}

WebRenderRenderRootData::~WebRenderRenderRootData() {}

void DestroyWebRenderUserDataTable(WebRenderUserDataTable* aTable) {
  for (auto iter = aTable->Iter(); !iter.Done(); iter.Next()) {
    iter.UserData()->RemoveFromTable();
  }
  delete aTable;
}

}  // namespace layers
}  // namespace mozilla
