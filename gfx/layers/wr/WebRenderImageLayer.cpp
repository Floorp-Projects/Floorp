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
WebRenderImageLayer::RenderLayer()
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
  Rect clip;
  if (GetClipRect().isSome()) {
      clip = RelativeToVisible(transform.Inverse().TransformBounds(IntRectToRect(GetClipRect().ref().ToUnknownRect())));
  } else {
      clip = rect;
  }

  Rect relBounds = VisibleBoundsRelativeToParent();
  if (!transform.IsIdentity()) {
    // WR will only apply the 'translate' of the transform, so we need to do the scale/rotation manually.
    gfx::Matrix4x4 boundTransform = transform;
    boundTransform._41 = 0.0f;
    boundTransform._42 = 0.0f;
    boundTransform._43 = 0.0f;
    relBounds.MoveTo(boundTransform.TransformPoint(relBounds.TopLeft()));
  }

  Rect overflow(0, 0, relBounds.width, relBounds.height);
  Maybe<WrImageMask> mask = buildMaskLayer();
  wr::ImageRendering filter = wr::ToImageRendering(mSamplingFilter);
  WrMixBlendMode mixBlendMode = wr::ToWrMixBlendMode(GetMixBlendMode());

  if (gfxPrefs::LayersDump()) {
    printf_stderr("ImageLayer %p using bounds=%s, overflow=%s, transform=%s, rect=%s, clip=%s, texture-filter=%s, mix-blend-mode=%s\n",
                  this->GetLayer(),
                  Stringify(relBounds).c_str(),
                  Stringify(overflow).c_str(),
                  Stringify(transform).c_str(),
                  Stringify(rect).c_str(),
                  Stringify(clip).c_str(),
                  Stringify(filter).c_str(),
                  Stringify(mixBlendMode).c_str());
  }

  WrBridge()->AddWebRenderCommand(
    OpDPPushStackingContext(wr::ToWrRect(relBounds),
                            wr::ToWrRect(overflow),
                            mask,
                            1.0f,
                            GetAnimations(),
                            transform,
                            mixBlendMode,
                            FrameMetrics::NULL_SCROLL_ID));
  WrImageKey key;
  key.mNamespace = WrBridge()->GetNamespace();
  key.mHandle = WrBridge()->GetNextResourceId();
  WrBridge()->AddWebRenderParentCommand(OpAddExternalImage(mExternalImageId, key));
  WrBridge()->AddWebRenderCommand(OpDPPushImage(wr::ToWrRect(rect), wr::ToWrRect(clip), Nothing(), filter, key));
  WrBridge()->AddWebRenderCommand(OpDPPopStackingContext());

  //mContainer->SetImageFactory(originalIF);
}

} // namespace layers
} // namespace mozilla
