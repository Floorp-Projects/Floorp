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
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#include "mozilla/layers/TextureClient.h"  // for BufferTextureClient, etc
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsDebug.h"                    // for NS_WARNING, NS_ASSERTION
#include "nsISupportsImpl.h"            // for Image::AddRef, etc
#include "nsRect.h"                     // for nsIntRect
#include "nsSize.h"                     // for nsIntSize
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR, etc

// Just big enough for a 1080p RGBA32 frame
#define MAX_FRAME_SIZE (16 * 1024 * 1024)

namespace mozilla {
namespace layers {

DeprecatedSharedRGBImage::DeprecatedSharedRGBImage(ISurfaceAllocator *aAllocator) :
  Image(nullptr, SHARED_RGB),
  mSize(0, 0),
  mSurfaceAllocator(aAllocator),
  mAllocated(false),
  mShmem(new ipc::Shmem())
{
  MOZ_COUNT_CTOR(DeprecatedSharedRGBImage);
}

DeprecatedSharedRGBImage::~DeprecatedSharedRGBImage()
{
  MOZ_COUNT_DTOR(DeprecatedSharedRGBImage);

  if (mAllocated) {
    SurfaceDescriptor desc;
    DropToSurfaceDescriptor(desc);
    mSurfaceAllocator->DestroySharedSurface(&desc);
  }
  delete mShmem;
}

already_AddRefed<Image>
CreateSharedRGBImage(ImageContainer *aImageContainer,
                     nsIntSize aSize,
                     gfxImageFormat aImageFormat)
{
  NS_ASSERTION(aImageFormat == gfxASurface::ImageFormatARGB32 ||
               aImageFormat == gfxASurface::ImageFormatRGB24 ||
               aImageFormat == gfxASurface::ImageFormatRGB16_565,
               "RGB formats supported only");

  if (!aImageContainer) {
    NS_WARNING("No ImageContainer to allocate DeprecatedSharedRGBImage");
    return nullptr;
  }

  ImageFormat format = SHARED_RGB;
  nsRefPtr<Image> image = aImageContainer->CreateImage(&format, 1);

  if (!image) {
    NS_WARNING("Failed to create DeprecatedSharedRGBImage");
    return nullptr;
  }

  if (gfxPlatform::GetPlatform()->UseDeprecatedTextures()) {
      nsRefPtr<DeprecatedSharedRGBImage> rgbImageDep = static_cast<DeprecatedSharedRGBImage*>(image.get());
      rgbImageDep->mSize = gfxIntSize(aSize.width, aSize.height);
      rgbImageDep->mImageFormat = aImageFormat;

      if (!rgbImageDep->AllocateBuffer(aSize, aImageFormat)) {
        NS_WARNING("Failed to allocate shared memory for DeprecatedSharedRGBImage");
        return nullptr;
      }
      return rgbImageDep.forget();
  }
  nsRefPtr<SharedRGBImage> rgbImage = static_cast<SharedRGBImage*>(image.get());
  rgbImage->Allocate(gfx::ToIntSize(aSize),
                     gfx::ImageFormatToSurfaceFormat(aImageFormat));
  return image.forget();
}

uint8_t *
DeprecatedSharedRGBImage::GetBuffer()
{
  return mShmem->get<uint8_t>();
}

size_t
DeprecatedSharedRGBImage::GetBufferSize()
{
  return mSize.width * mSize.height * gfxASurface::BytesPerPixel(mImageFormat);
}

gfxIntSize
DeprecatedSharedRGBImage::GetSize()
{
  return mSize;
}

bool
DeprecatedSharedRGBImage::AllocateBuffer(nsIntSize aSize, gfxImageFormat aImageFormat)
{
  if (mAllocated) {
    NS_WARNING("Already allocated shmem");
    return false;
  }

  size_t size = GetBufferSize();

  if (size == 0 || size > MAX_FRAME_SIZE) {
    NS_WARNING("Invalid frame size");
  }
  if (mSurfaceAllocator->AllocUnsafeShmem(size, OptimalShmemType(), mShmem)) {
    mAllocated = true;
  }

  return mAllocated;
}

already_AddRefed<gfxASurface>
DeprecatedSharedRGBImage::GetAsSurface()
{
  return nullptr;
}

bool
DeprecatedSharedRGBImage::ToSurfaceDescriptor(SurfaceDescriptor& aResult)
{
  if (!mAllocated) {
    return false;
  }
  this->AddRef();
  aResult = RGBImage(*mShmem,
                     nsIntRect(0, 0, mSize.width, mSize.height),
                     mImageFormat,
                     reinterpret_cast<uint64_t>(this));
  return true;
}

bool
DeprecatedSharedRGBImage::DropToSurfaceDescriptor(SurfaceDescriptor& aResult)
{
  if (!mAllocated) {
    return false;
  }
  aResult = RGBImage(*mShmem,
                     nsIntRect(0, 0, mSize.width, mSize.height),
                     mImageFormat,
                     0);
  *mShmem = ipc::Shmem();
  mAllocated = false;
  return true;
}

DeprecatedSharedRGBImage*
DeprecatedSharedRGBImage::FromSurfaceDescriptor(const SurfaceDescriptor& aDescriptor)
{
  if (aDescriptor.type() != SurfaceDescriptor::TRGBImage) {
    return nullptr;
  }
  const RGBImage& rgb = aDescriptor.get_RGBImage();
  if (rgb.owner() == 0) {
    return nullptr;
  }
  return reinterpret_cast<DeprecatedSharedRGBImage*>(rgb.owner());
}

SharedRGBImage::SharedRGBImage(ImageClient* aCompositable)
: Image(nullptr, SHARED_RGB)
, mCompositable(aCompositable)
{
  MOZ_COUNT_CTOR(SharedRGBImage);
}

SharedRGBImage::~SharedRGBImage()
{
  MOZ_COUNT_DTOR(SharedRGBImage);
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
  return mTextureClient ? mTextureClient->GetBuffer()
                        : nullptr;
}

gfxIntSize
SharedRGBImage::GetSize()
{
  return ThebesIntSize(mSize);
}

size_t
SharedRGBImage::GetBufferSize()
{
  return mTextureClient ? mTextureClient->GetBufferSize()
                        : 0;
}

TextureClient*
SharedRGBImage::GetTextureClient()
{
  return mTextureClient.get();
}

already_AddRefed<gfxASurface>
SharedRGBImage::GetAsSurface()
{
  return nullptr;
}


} // namespace layers
} // namespace mozilla
