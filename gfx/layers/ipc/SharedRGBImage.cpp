/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedRGBImage.h"
#include "ImageTypes.h"                 // for ImageFormat::SHARED_RGB, etc
#include "Shmem.h"                      // for Shmem
#include "gfx2DGlue.h"                  // for ImageFormatToSurfaceFormat, etc
#include "gfxPlatform.h"                // for gfxPlatform, gfxImageFormat
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator, etc
#include "mozilla/layers/ImageClient.h"  // for ImageClient
#include "mozilla/layers/ImageDataSerializer.h"  // for ImageDataSerializer
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#include "mozilla/layers/TextureClient.h"  // for BufferTextureClient, etc
#include "mozilla/layers/ImageBridgeChild.h"  // for ImageBridgeChild
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsDebug.h"                    // for NS_WARNING, NS_ASSERTION
#include "nsISupportsImpl.h"            // for Image::AddRef, etc
#include "nsRect.h"                     // for nsIntRect
#include "nsSize.h"                     // for nsIntSize

// Just big enough for a 1080p RGBA32 frame
#define MAX_FRAME_SIZE (16 * 1024 * 1024)

namespace mozilla {
namespace layers {

already_AddRefed<Image>
CreateSharedRGBImage(ImageContainer *aImageContainer,
                     nsIntSize aSize,
                     gfxImageFormat aImageFormat)
{
  NS_ASSERTION(aImageFormat == gfxImageFormat::ARGB32 ||
               aImageFormat == gfxImageFormat::RGB24 ||
               aImageFormat == gfxImageFormat::RGB16_565,
               "RGB formats supported only");

  if (!aImageContainer) {
    NS_WARNING("No ImageContainer to allocate SharedRGBImage");
    return nullptr;
  }

  nsRefPtr<Image> image = aImageContainer->CreateImage(ImageFormat::SHARED_RGB);

  if (!image) {
    NS_WARNING("Failed to create SharedRGBImage");
    return nullptr;
  }

  nsRefPtr<SharedRGBImage> rgbImage = static_cast<SharedRGBImage*>(image.get());
  if (!rgbImage->Allocate(gfx::ToIntSize(aSize),
                          gfx::ImageFormatToSurfaceFormat(aImageFormat))) {
    NS_WARNING("Failed to allocate a shared image");
    return nullptr;
  }
  return image.forget();
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

  if (mCompositable->GetAsyncID() != 0 &&
      !InImageBridgeChildThread()) {
    ImageBridgeChild::DispatchReleaseTextureClient(mTextureClient.forget().drop());
    ImageBridgeChild::DispatchReleaseImageClient(mCompositable.forget().drop());
  }
}

bool
SharedRGBImage::Allocate(gfx::IntSize aSize, gfx::SurfaceFormat aFormat)
{
  mSize = aSize;
  mTextureClient = mCompositable->CreateBufferTextureClient(aFormat);
  return mTextureClient->AllocateForSurface(aSize);
}

uint8_t*
SharedRGBImage::GetBuffer()
{
  if (!mTextureClient) {
    return nullptr;
  }

  ImageDataSerializer serializer(mTextureClient->GetBuffer(), mTextureClient->GetBufferSize());
  return serializer.GetData();
}

gfx::IntSize
SharedRGBImage::GetSize()
{
  return mSize;
}

size_t
SharedRGBImage::GetBufferSize()
{
  return mTextureClient ? mTextureClient->GetBufferSize()
                        : 0;
}

TextureClient*
SharedRGBImage::GetTextureClient(CompositableClient* aClient)
{
  return mTextureClient.get();
}

TemporaryRef<gfx::SourceSurface>
SharedRGBImage::GetAsSourceSurface()
{
  return nullptr;
}

} // namespace layers
} // namespace mozilla
