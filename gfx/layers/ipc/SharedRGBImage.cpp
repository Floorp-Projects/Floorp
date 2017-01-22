/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedRGBImage.h"
#include "ImageTypes.h"                 // for ImageFormat::SHARED_RGB, etc
#include "Shmem.h"                      // for Shmem
#include "gfx2DGlue.h"                  // for ImageFormatToSurfaceFormat, etc
#include "gfxPlatform.h"                // for gfxPlatform, gfxImageFormat
#include "mozilla/gfx/Point.h"          // for IntSIze
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator, etc
#include "mozilla/layers/ImageClient.h"  // for ImageClient
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#include "mozilla/layers/TextureClient.h"  // for BufferTextureClient, etc
#include "mozilla/layers/ImageBridgeChild.h"  // for ImageBridgeChild
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsDebug.h"                    // for NS_WARNING, NS_ASSERTION
#include "nsISupportsImpl.h"            // for Image::AddRef, etc
#include "nsRect.h"                     // for mozilla::gfx::IntRect

// Just big enough for a 1080p RGBA32 frame
#define MAX_FRAME_SIZE (16 * 1024 * 1024)

namespace mozilla {
namespace layers {

already_AddRefed<Image>
CreateSharedRGBImage(ImageContainer *aImageContainer,
                     gfx::IntSize aSize,
                     gfxImageFormat aImageFormat)
{
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
  if (!rgbImage->Allocate(aSize, gfx::ImageFormatToSurfaceFormat(aImageFormat))) {
    NS_WARNING("Failed to allocate a shared image");
    return nullptr;
  }
  return rgbImage.forget();
}

SharedRGBImage::SharedRGBImage(ImageClient* aCompositable)
: Image(nullptr, ImageFormat::SHARED_RGB)
, mCompositable(aCompositable)
{
  MOZ_COUNT_CTOR(SharedRGBImage);
}

SharedRGBImage::~SharedRGBImage()
{
  MOZ_COUNT_DTOR(SharedRGBImage);

  if (mCompositable->GetAsyncHandle() && !InImageBridgeChildThread()) {
    ADDREF_MANUALLY(mTextureClient);
    ImageBridgeChild::DispatchReleaseTextureClient(mTextureClient);
    mTextureClient = nullptr;
  }
}

bool
SharedRGBImage::Allocate(gfx::IntSize aSize, gfx::SurfaceFormat aFormat)
{
  mSize = aSize;
  mTextureClient = mCompositable->CreateBufferTextureClient(aFormat, aSize,
                                                            gfx::BackendType::NONE,
                                                            TextureFlags::DEFAULT);
  return !!mTextureClient;
}

uint8_t*
SharedRGBImage::GetBuffer()
{
  MappedTextureData mapped;
  if (mTextureClient && mTextureClient->BorrowMappedData(mapped)) {
    return mapped.data;
  }
  return 0;
}

gfx::IntSize
SharedRGBImage::GetSize()
{
  return mSize;
}

TextureClient*
SharedRGBImage::GetTextureClient(KnowsCompositor* aForwarder)
{
  return mTextureClient.get();
}

already_AddRefed<gfx::SourceSurface>
SharedRGBImage::GetAsSourceSurface()
{
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
  }

  mSourceSurface = surface;
  return surface.forget();
}

} // namespace layers
} // namespace mozilla
