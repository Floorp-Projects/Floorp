/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedRGBImage.h"
#include "ImageTypes.h"         // for ImageFormat::SHARED_RGB, etc
#include "Shmem.h"              // for Shmem
#include "gfx2DGlue.h"          // for ImageFormatToSurfaceFormat, etc
#include "gfxPlatform.h"        // for gfxPlatform, gfxImageFormat
#include "mozilla/gfx/Point.h"  // for IntSIze
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator, etc
#include "mozilla/layers/ImageClient.h"        // for ImageClient
#include "mozilla/layers/LayersSurfaces.h"     // for SurfaceDescriptor, etc
#include "mozilla/layers/TextureClient.h"      // for BufferTextureClient, etc
#include "mozilla/layers/TextureClientRecycleAllocator.h"  // for ITextureClientAllocationHelper
#include "mozilla/layers/ImageBridgeChild.h"  // for ImageBridgeChild
#include "mozilla/mozalloc.h"                 // for operator delete, etc
#include "nsDebug.h"                          // for NS_WARNING, NS_ASSERTION
#include "nsISupportsImpl.h"                  // for Image::AddRef, etc
#include "nsRect.h"                           // for mozilla::gfx::IntRect

// Just big enough for a 1080p RGBA32 frame
#define MAX_FRAME_SIZE (16 * 1024 * 1024)

namespace mozilla {
namespace layers {

class TextureClientForRawBufferAccessAllocationHelper
    : public ITextureClientAllocationHelper {
 public:
  TextureClientForRawBufferAccessAllocationHelper(gfx::SurfaceFormat aFormat,
                                                  gfx::IntSize aSize,
                                                  TextureFlags aTextureFlags)
      : ITextureClientAllocationHelper(aFormat, aSize, BackendSelector::Content,
                                       aTextureFlags, ALLOC_DEFAULT) {}

  bool IsCompatible(TextureClient* aTextureClient) override {
    bool ret = aTextureClient->GetFormat() == mFormat &&
               aTextureClient->GetSize() == mSize;
    return ret;
  }

  already_AddRefed<TextureClient> Allocate(
      KnowsCompositor* aAllocator) override {
    return TextureClient::CreateForRawBufferAccess(
        aAllocator, mFormat, mSize, gfx::BackendType::NONE, mTextureFlags);
  }
};

already_AddRefed<Image> CreateSharedRGBImage(ImageContainer* aImageContainer,
                                             gfx::IntSize aSize,
                                             gfxImageFormat aImageFormat) {
  NS_ASSERTION(aImageFormat == gfx::SurfaceFormat::A8R8G8B8_UINT32 ||
                   aImageFormat == gfx::SurfaceFormat::X8R8G8B8_UINT32 ||
                   aImageFormat == gfx::SurfaceFormat::R5G6B5_UINT16,
               "RGB formats supported only");

  if (!aImageContainer) {
    NS_WARNING("No ImageContainer to allocate SharedRGBImage");
    return nullptr;
  }

  RefPtr<SharedRGBImage> rgbImage = aImageContainer->CreateSharedRGBImage();
  if (!rgbImage) {
    NS_WARNING("Failed to create SharedRGBImage");
    return nullptr;
  }
  if (!rgbImage->Allocate(aSize,
                          gfx::ImageFormatToSurfaceFormat(aImageFormat))) {
    NS_WARNING("Failed to allocate a shared image");
    return nullptr;
  }
  return rgbImage.forget();
}

SharedRGBImage::SharedRGBImage(ImageClient* aCompositable)
    : Image(nullptr, ImageFormat::SHARED_RGB), mCompositable(aCompositable) {
  MOZ_COUNT_CTOR(SharedRGBImage);
}

SharedRGBImage::~SharedRGBImage() { MOZ_COUNT_DTOR(SharedRGBImage); }

bool SharedRGBImage::Allocate(gfx::IntSize aSize, gfx::SurfaceFormat aFormat) {
  static const uint32_t MAX_POOLED_VIDEO_COUNT = 5;
  mSize = aSize;

  if (!mCompositable->HasTextureClientRecycler()) {
    // Initialize TextureClientRecycler
    mCompositable->GetTextureClientRecycler()->SetMaxPoolSize(
        MAX_POOLED_VIDEO_COUNT);
  }

  {
    TextureClientForRawBufferAccessAllocationHelper helper(
        aFormat, aSize, mCompositable->GetTextureFlags());
    mTextureClient =
        mCompositable->GetTextureClientRecycler()->CreateOrRecycle(helper);
  }

  return !!mTextureClient;
}

gfx::IntSize SharedRGBImage::GetSize() const { return mSize; }

TextureClient* SharedRGBImage::GetTextureClient(
    KnowsCompositor* aKnowsCompositor) {
  return mTextureClient.get();
}

static void ReleaseTextureClient(void* aData) {
  RELEASE_MANUALLY(static_cast<TextureClient*>(aData));
}

static gfx::UserDataKey sTextureClientKey;

already_AddRefed<gfx::SourceSurface> SharedRGBImage::GetAsSourceSurface() {
  NS_ASSERTION(NS_IsMainThread(), "Must be main thread");

  if (mSourceSurface) {
    RefPtr<gfx::SourceSurface> surface(mSourceSurface);
    return surface.forget();
  }

  RefPtr<gfx::SourceSurface> surface;
  {
    // We are 'borrowing' the DrawTarget and retaining a permanent reference to
    // the underlying data (via the surface). It is in this instance since we
    // know that the TextureClient is always wrapping a BufferTextureData and
    // therefore it won't go away underneath us.
    BufferTextureData* decoded_buffer =
        mTextureClient->GetInternalData()->AsBufferTextureData();
    RefPtr<gfx::DrawTarget> drawTarget = decoded_buffer->BorrowDrawTarget();

    if (!drawTarget) {
      return nullptr;
    }

    surface = drawTarget->Snapshot();
    if (!surface) {
      return nullptr;
    }

    // The surface may outlive the owning TextureClient. So, we need to ensure
    // that the surface keeps the TextureClient alive via a reference held in
    // user data. The TextureClient's DrawTarget only has a weak reference to
    // the surface, so we won't create any cycles by just referencing the
    // TextureClient.
    if (!surface->GetUserData(&sTextureClientKey)) {
      surface->AddUserData(&sTextureClientKey, mTextureClient,
                           ReleaseTextureClient);
      ADDREF_MANUALLY(mTextureClient);
    }
  }

  mSourceSurface = surface;
  return surface.forget();
}

}  // namespace layers
}  // namespace mozilla
