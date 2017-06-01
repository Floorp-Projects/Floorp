/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderImageLayer.h"

#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ImageClient.h"
#include "mozilla/layers/ScrollingLayersHelper.h"
#include "mozilla/layers/StackingContextHelper.h"
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

  for (auto key : mVideoKeys) {
    WrManager()->AddImageKeyForDiscard(key);
  }
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
WebRenderImageLayer::AddWRVideoImage(size_t aChannelNumber)
{
  for (size_t i = 0; i < aChannelNumber; ++i) {
    WrImageKey key = GetImageKey();
    WrManager()->AddImageKeyForDiscard(key);
    mVideoKeys.AppendElement(key);
  }
  WrBridge()->AddWebRenderParentCommand(OpAddExternalVideoImage(mExternalImageId.value(), mVideoKeys));
}

void
WebRenderImageLayer::RenderLayer(wr::DisplayListBuilder& aBuilder,
                                 const StackingContextHelper& aSc)
{
  if (!mContainer) {
     return;
  }

  CompositableType type = GetImageClientType();
  if (type == CompositableType::UNKNOWN) {
    return;
  }

  MOZ_ASSERT(GetImageClientType() != CompositableType::UNKNOWN);

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
      // Alloc async image pipeline id.
      mPipelineId = Some(WrBridge()->GetCompositorBridgeChild()->GetNextPipelineId());
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

  if (GetImageClientType() != CompositableType::IMAGE_BRIDGE) {
    // Handle CompositableType::IMAGE case
    MOZ_ASSERT(mImageClient->AsImageClientSingle());
    mKey = UpdateImageKey(mImageClient->AsImageClientSingle(),
                          mContainer,
                          mKey,
                          mExternalImageId.ref());
    if (mKey.isNothing()) {
      return;
    }
  } else {
    // Always allocate key.
    mVideoKeys.Clear();

    // XXX (Jerry): Remove the hardcode image format setting.
#if defined(XP_WIN)
    // Use libyuv to convert the buffer to rgba format. So, use 1 image key here.
    AddWRVideoImage(1);
#elif defined(XP_MACOSX)
    if (gfx::gfxVars::CanUseHardwareVideoDecoding()) {
      // Use the hardware MacIOSurface with YCbCr interleaved format. It uses 1
      // image key.
      AddWRVideoImage(1);
    } else {
      // Use libyuv.
      AddWRVideoImage(1);
    }
#elif defined(MOZ_WIDGET_GTK)
    // Use libyuv.
    AddWRVideoImage(1);
#elif defined(ANDROID)
    // Use libyuv.
    AddWRVideoImage(1);
#endif
  }

  ScrollingLayersHelper scroller(this, aBuilder, aSc);
  StackingContextHelper sc(aSc, aBuilder, this);

  LayerRect rect(0, 0, size.width, size.height);
  if (mScaleMode != ScaleMode::SCALE_NONE) {
    NS_ASSERTION(mScaleMode == ScaleMode::STRETCH,
                 "No other scalemodes than stretch and none supported yet.");
    rect = LayerRect(0, 0, mScaleToSize.width, mScaleToSize.height);
  }

  LayerRect clipRect = ClipRect().valueOr(rect);
  Maybe<WrImageMask> mask = BuildWrMaskLayer(&sc);
  WrClipRegionToken clip = aBuilder.PushClipRegion(
      sc.ToRelativeWrRect(clipRect),
      mask.ptrOr(nullptr));

  wr::ImageRendering filter = wr::ToImageRendering(mSamplingFilter);

  DumpLayerInfo("Image Layer", rect);
  if (gfxPrefs::LayersDump()) {
    printf_stderr("ImageLayer %p texture-filter=%s \n",
                  GetLayer(),
                  Stringify(filter).c_str());
  }

  if (GetImageClientType() != CompositableType::IMAGE_BRIDGE) {
    aBuilder.PushImage(sc.ToRelativeWrRect(rect), clip, filter, mKey.value());
  } else {
    // XXX (Jerry): Remove the hardcode image format setting. The format of
    // textureClient could change from time to time. So, we just set the most
    // usable format here.
#if defined(XP_WIN)
    // Use libyuv to convert the buffer to rgba format.
    MOZ_ASSERT(mVideoKeys.Length() == 1);
    aBuilder.PushImage(sc.ToRelativeWrRect(rect), clip, filter, mVideoKeys[0]);
#elif defined(XP_MACOSX)
    if (gfx::gfxVars::CanUseHardwareVideoDecoding()) {
      // Use the hardware MacIOSurface with YCbCr interleaved format.
      MOZ_ASSERT(mVideoKeys.Length() == 1);
      aBuilder.PushYCbCrInterleavedImage(sc.ToRelativeWrRect(rect), clip, mVideoKeys[0], WrYuvColorSpace::Rec601, filter);
    } else {
      // Use libyuv to convert the buffer to rgba format.
      MOZ_ASSERT(mVideoKeys.Length() == 1);
      aBuilder.PushImage(sc.ToRelativeWrRect(rect), clip, filter, mVideoKeys[0]);
    }
#elif defined(MOZ_WIDGET_GTK)
    // Use libyuv to convert the buffer to rgba format.
    MOZ_ASSERT(mVideoKeys.Length() == 1);
    aBuilder.PushImage(sc.ToRelativeWrRect(rect), clip, filter, mVideoKeys[0]);
#elif defined(ANDROID)
    // Use libyuv to convert the buffer to rgba format.
    MOZ_ASSERT(mVideoKeys.Length() == 1);
    aBuilder.PushImage(sc.ToRelativeWrRect(rect), clip, filter, mVideoKeys[0]);
#endif
  }
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
