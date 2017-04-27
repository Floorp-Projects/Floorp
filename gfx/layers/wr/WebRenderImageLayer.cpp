/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderImageLayer.h"

#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "mozilla/layers/ImageClient.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"
#include "mozilla/layers/TextureWrapperImage.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace layers {

using namespace gfx;

WebRenderImageLayer::WebRenderImageLayer(WebRenderLayerManager* aLayerManager)
  : ImageLayer(aLayerManager, static_cast<WebRenderLayer*>(this))
  , mImageClientTypeContainer(CompositableType::UNKNOWN)
{
  MOZ_COUNT_CTOR(WebRenderImageLayer);
}

WebRenderImageLayer::~WebRenderImageLayer()
{
  MOZ_COUNT_DTOR(WebRenderImageLayer);
  mPipelineIdRequest.DisconnectIfExists();
  if (mKey.isSome()) {
    WrManager()->AddImageKeyForDiscard(mKey.value());
  }
  if (mExternalImageId.isSome()) {
    WrBridge()->DeallocExternalImageId(mExternalImageId.ref());
  }
}

CompositableType
WebRenderImageLayer::GetImageClientType()
{
  if (mImageClientTypeContainer != CompositableType::UNKNOWN) {
    return mImageClientTypeContainer;
  }

  if (mContainer->IsAsync()) {
    mImageClientTypeContainer = CompositableType::IMAGE_BRIDGE;
    return mImageClientTypeContainer;
  }

  AutoLockImage autoLock(mContainer);

  mImageClientTypeContainer = autoLock.HasImage()
    ? CompositableType::IMAGE : CompositableType::UNKNOWN;
  return mImageClientTypeContainer;
}

already_AddRefed<gfx::SourceSurface>
WebRenderImageLayer::GetAsSourceSurface()
{
  if (!mContainer) {
    return nullptr;
  }
  AutoLockImage autoLock(mContainer);
  Image *image = autoLock.GetImage();
  if (!image) {
    return nullptr;
  }
  RefPtr<gfx::SourceSurface> surface = image->GetAsSourceSurface();
  if (!surface || !surface->IsValid()) {
    return nullptr;
  }
  return surface.forget();
}

void
WebRenderImageLayer::ClearCachedResources()
{
  if (mImageClient) {
    mImageClient->ClearCachedResources();
  }
}

void
WebRenderImageLayer::RenderLayer(wr::DisplayListBuilder& aBuilder)
{
  if (!mContainer) {
     return;
  }

  CompositableType type = GetImageClientType();
  if (type == CompositableType::UNKNOWN) {
    return;
  }

  MOZ_ASSERT(GetImageClientType() != CompositableType::UNKNOWN);

  // Allocate PipelineId if necessary
  if (GetImageClientType() == CompositableType::IMAGE_BRIDGE &&
      mPipelineId.isNothing() && !mPipelineIdRequest.Exists()) {
    // Use Holder to pass this pointer to lambda.
    // Static anaysis tool does not permit to pass refcounted variable to lambda.
    // And we do not want to use RefPtr<WebRenderImageLayer> here.
    Holder holder(this);
    Manager()->AllocPipelineId()
      ->Then(AbstractThread::GetCurrent(), __func__,
      [holder] (const wr::PipelineId& aPipelineId) {
        holder->mPipelineIdRequest.Complete();
        holder->mPipelineId = Some(aPipelineId);
      },
      [holder] (const ipc::PromiseRejectReason &aReason) {
        holder->mPipelineIdRequest.Complete();
      })->Track(mPipelineIdRequest);
  }

  if (GetImageClientType() == CompositableType::IMAGE && !mImageClient) {
    mImageClient = ImageClient::CreateImageClient(CompositableType::IMAGE,
                                                  WrBridge(),
                                                  TextureFlags::DEFAULT);
    if (!mImageClient) {
      return;
    }
    mImageClient->Connect();
  }

  if (mExternalImageId.isNothing()) {
    if (GetImageClientType() == CompositableType::IMAGE_BRIDGE) {
      MOZ_ASSERT(!mImageClient);
      mExternalImageId = Some(WrBridge()->AllocExternalImageId(mContainer->GetAsyncContainerHandle()));
    } else {
      // Handle CompositableType::IMAGE case
      MOZ_ASSERT(mImageClient);
      mExternalImageId = Some(WrBridge()->AllocExternalImageIdForCompositable(mImageClient));
    }
  }
  MOZ_ASSERT(mExternalImageId.isSome());

  // XXX Not good for async ImageContainer case.
  AutoLockImage autoLock(mContainer);
  Image* image = autoLock.GetImage();
  if (!image) {
    return;
  }
  gfx::IntSize size = image->GetSize();

  if (GetImageClientType() == CompositableType::IMAGE_BRIDGE) {
    // Always allocate key
    WrImageKey key = GetImageKey();
    WrBridge()->AddWebRenderParentCommand(OpAddExternalImage(mExternalImageId.value(), key));
    Manager()->AddImageKeyForDiscard(key);
    mKey = Some(key);
  } else {
    // Handle CompositableType::IMAGE case
    MOZ_ASSERT(mImageClient->AsImageClientSingle());
    mKey = UpdateImageKey(mImageClient->AsImageClientSingle(),
                          mContainer,
                          mKey,
                          mExternalImageId.ref());
  }

  if (mKey.isNothing()) {
    return;
  }

  gfx::Matrix4x4 transform = GetTransform();
  gfx::Rect relBounds = GetWrRelBounds();

  LayerRect rect(0, 0, size.width, size.height);
  if (mScaleMode != ScaleMode::SCALE_NONE) {
    NS_ASSERTION(mScaleMode == ScaleMode::STRETCH,
                 "No other scalemodes than stretch and none supported yet.");
    rect = LayerRect(0, 0, mScaleToSize.width, mScaleToSize.height);
  }
  rect = RelativeToVisible(rect);

  LayerRect clipRect = GetWrClipRect(rect);
  Maybe<WrImageMask> mask = BuildWrMaskLayer(true);
  WrClipRegion clip = aBuilder.BuildClipRegion(wr::ToWrRect(clipRect), mask.ptrOr(nullptr));

  wr::ImageRendering filter = wr::ToImageRendering(mSamplingFilter);
  wr::MixBlendMode mixBlendMode = wr::ToWrMixBlendMode(GetMixBlendMode());

  DumpLayerInfo("Image Layer", rect);
  if (gfxPrefs::LayersDump()) {
    printf_stderr("ImageLayer %p texture-filter=%s \n",
                  GetLayer(),
                  Stringify(filter).c_str());
  }

  aBuilder.PushStackingContext(wr::ToWrRect(relBounds),
                            1.0f,
                            transform,
                            mixBlendMode);
  aBuilder.PushImage(wr::ToWrRect(rect), clip, filter, mKey.value());
  aBuilder.PopStackingContext();
}

Maybe<WrImageMask>
WebRenderImageLayer::RenderMaskLayer(const gfx::Matrix4x4& aTransform)
{
  if (!mContainer) {
     return Nothing();
  }

  CompositableType type = GetImageClientType();
  if (type == CompositableType::UNKNOWN) {
    return Nothing();
  }

  MOZ_ASSERT(GetImageClientType() == CompositableType::IMAGE);
  if (GetImageClientType() != CompositableType::IMAGE) {
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
    mExternalImageId = Some(WrBridge()->AllocExternalImageIdForCompositable(mImageClient));
  }

  AutoLockImage autoLock(mContainer);
  Image* image = autoLock.GetImage();
  if (!image) {
    return Nothing();
  }

  MOZ_ASSERT(mImageClient->AsImageClientSingle());
  mKey = UpdateImageKey(mImageClient->AsImageClientSingle(),
                        mContainer,
                        mKey,
                        mExternalImageId.ref());
  if (mKey.isNothing()) {
    return Nothing();
  }

  gfx::IntSize size = image->GetSize();
  WrImageMask imageMask;
  imageMask.image = mKey.value();
  Rect maskRect = aTransform.TransformBounds(Rect(0, 0, size.width, size.height));
  imageMask.rect = wr::ToWrRect(maskRect);
  imageMask.repeat = false;
  return Some(imageMask);
}

} // namespace layers
} // namespace mozilla
