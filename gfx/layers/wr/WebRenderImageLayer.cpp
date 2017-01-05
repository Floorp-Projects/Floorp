/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderImageLayer.h"

#include "LayersLogging.h"
#include "mozilla/layers/ImageClient.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"
#include "mozilla/layers/TextureWrapperImage.h"
#include "mozilla/layers/WebRenderBridgeChild.h"

namespace mozilla {
namespace layers {

using namespace gfx;

WebRenderImageLayer::WebRenderImageLayer(WebRenderLayerManager* aLayerManager)
  : ImageLayer(aLayerManager, static_cast<WebRenderLayer*>(this))
  , mExternalImageId(0)
{
  MOZ_COUNT_CTOR(WebRenderImageLayer);
}

WebRenderImageLayer::~WebRenderImageLayer()
{
  MOZ_COUNT_DTOR(WebRenderImageLayer);
  if (mExternalImageId) {
    WRBridge()->DeallocExternalImageId(mExternalImageId);
  }
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
WebRenderImageLayer::RenderLayer()
{
  RefPtr<gfx::SourceSurface> surface = GetAsSourceSurface();
  if (!surface) {
    return;
  }

  if (!mImageContainerForWR) {
    mImageContainerForWR = LayerManager::CreateImageContainer();
  }

  if (!mImageClient) {
    mImageClient = ImageClient::CreateImageClient(CompositableType::IMAGE,
                                                  WRBridge(),
                                                  TextureFlags::DEFAULT);
    if (!mImageClient) {
      return;
    }
    mImageClient->Connect();
  }

  // XXX Enable external image id for async image container.

  // XXX update async ImageContainer rendering path
  //if (mContainer->IsAsync() && !mImageId) {
  //  mExternalImageId = WRBridge()->AllocExternalImageId(mContainer->GetAsyncContainerID());
  //  MOZ_ASSERT(mImageId);
  //}

  if (!mExternalImageId) {
    mExternalImageId = WRBridge()->AllocExternalImageIdForCompositable(mImageClient);
    MOZ_ASSERT(mExternalImageId);
  }

  gfx::IntSize size = surface->GetSize();

  RefPtr<TextureClient> texture = mImageClient->GetTextureClientRecycler()
    ->CreateOrRecycle(surface->GetFormat(),
                      size,
                      BackendSelector::Content,
                      TextureFlags::DEFAULT);
  if (!texture) {
    return;
  }

  MOZ_ASSERT(texture->CanExposeDrawTarget());
  {
    TextureClientAutoLock autoLock(texture, OpenMode::OPEN_WRITE_ONLY);
    if (!autoLock.Succeeded()) {
      return;
    }
    RefPtr<DrawTarget> drawTarget = texture->BorrowDrawTarget();
    if (!drawTarget || !drawTarget->IsValid()) {
      return;
    }
    drawTarget->CopySurface(surface, IntRect(IntPoint(), size), IntPoint());
  }
  RefPtr<TextureWrapperImage> image =
    new TextureWrapperImage(texture, IntRect(IntPoint(0, 0), size));
  mImageContainerForWR->SetCurrentImageInTransaction(image);

  if (!mImageClient->UpdateImage(mImageContainerForWR, /* unused */0)) {
    return;
  }

  WRScrollFrameStackingContextGenerator scrollFrames(this);

  //XXX
  MOZ_RELEASE_ASSERT(surface->GetFormat() == SurfaceFormat::B8G8R8X8 ||
                     surface->GetFormat() == SurfaceFormat::B8G8R8A8, "bad format");

  Rect rect(0, 0, size.width, size.height);

  Rect clip;
  if (GetClipRect().isSome()) {
      clip = RelativeToTransformedVisible(IntRectToRect(GetClipRect().ref().ToUnknownRect()));
  } else {
      clip = rect;
  }
  if (gfxPrefs::LayersDump()) printf_stderr("ImageLayer %p using rect:%s clip:%s\n", this, Stringify(rect).c_str(), Stringify(clip).c_str());
  Rect relBounds = TransformedVisibleBoundsRelativeToParent();
  Rect overflow(0, 0, relBounds.width, relBounds.height);
  Matrix4x4 transform;// = GetTransform();
  WRBridge()->AddWebRenderCommand(
    OpDPPushStackingContext(toWrRect(relBounds), toWrRect(overflow), transform, FrameMetrics::NULL_SCROLL_ID));

  WRBridge()->AddWebRenderCommand(OpDPPushExternalImageId(toWrRect(rect), toWrRect(clip), Nothing(), mExternalImageId));


  if (gfxPrefs::LayersDump()) printf_stderr("ImageLayer %p using %s as bounds/overflow, %s for transform\n", this, Stringify(relBounds).c_str(), Stringify(transform).c_str());
  WRBridge()->AddWebRenderCommand(OpDPPopStackingContext());

  //mContainer->SetImageFactory(originalIF);
}

} // namespace layers
} // namespace mozilla
