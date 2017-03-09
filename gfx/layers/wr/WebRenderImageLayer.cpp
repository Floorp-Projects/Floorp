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
  , mExternalImageId(0)
  , mImageClientTypeContainer(CompositableType::UNKNOWN)
{
  MOZ_COUNT_CTOR(WebRenderImageLayer);
}

WebRenderImageLayer::~WebRenderImageLayer()
{
  MOZ_COUNT_DTOR(WebRenderImageLayer);
  if (mExternalImageId) {
    WrBridge()->DeallocExternalImageId(mExternalImageId);
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

  if (GetImageClientType() == CompositableType::IMAGE && !mImageClient) {
    mImageClient = ImageClient::CreateImageClient(CompositableType::IMAGE,
                                                  WrBridge(),
                                                  TextureFlags::DEFAULT);
    if (!mImageClient) {
      return;
    }
    mImageClient->Connect();
  }

  if (!mExternalImageId) {
    if (GetImageClientType() == CompositableType::IMAGE_BRIDGE) {
      MOZ_ASSERT(!mImageClient);
      mExternalImageId = WrBridge()->AllocExternalImageId(mContainer->GetAsyncContainerHandle());
    } else {
      // Handle CompositableType::IMAGE case
      MOZ_ASSERT(mImageClient);
      mExternalImageId = WrBridge()->AllocExternalImageIdForCompositable(mImageClient);
    }
  }
  MOZ_ASSERT(mExternalImageId);

  // XXX Not good for async ImageContainer case.
  AutoLockImage autoLock(mContainer);
  Image* image = autoLock.GetImage();
  if (!image) {
    return;
  }
  gfx::IntSize size = image->GetSize();

  if (mImageClient && !mImageClient->UpdateImage(mContainer, /* unused */0)) {
    return;
  }

  WrScrollFrameStackingContextGenerator scrollFrames(this);

  Matrix4x4 transform = GetTransform();
  Rect rect(0, 0, size.width, size.height);
  Rect clip = GetWrClipRect(rect);
  Rect relBounds = GetWrRelBounds();
  Rect overflow(0, 0, relBounds.width, relBounds.height);

  Maybe<WrImageMask> mask = buildMaskLayer();
  WrClipRegion clipRegion = aBuilder.BuildClipRegion(wr::ToWrRect(clip));
  wr::ImageRendering filter = wr::ToImageRendering(mSamplingFilter);
  WrMixBlendMode mixBlendMode = wr::ToWrMixBlendMode(GetMixBlendMode());

  DumpLayerInfo("Image Layer", rect);
  if (gfxPrefs::LayersDump()) {
    printf_stderr("ImageLayer %p texture-filter=%s \n",
                  GetLayer(),
                  Stringify(filter).c_str());
  }

  WrImageKey key;
  key.mNamespace = WrBridge()->GetNamespace();
  key.mHandle = WrBridge()->GetNextResourceId();
  WrBridge()->AddWebRenderParentCommand(OpAddExternalImage(mExternalImageId, key));

  aBuilder.PushStackingContext(wr::ToWrRect(relBounds),
                            wr::ToWrRect(overflow),
                            mask.ptrOr(nullptr),
                            1.0f,
                            //GetAnimations(),
                            transform,
                            mixBlendMode);
  aBuilder.PushImage(wr::ToWrRect(rect), clipRegion, filter, key);
  aBuilder.PopStackingContext();

  //mContainer->SetImageFactory(originalIF);
}

} // namespace layers
} // namespace mozilla
