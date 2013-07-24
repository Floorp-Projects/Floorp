/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShadowLayers.h"
#include "SharedRGBImage.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "Shmem.h"
#include "mozilla/layers/ISurfaceAllocator.h"

// Just big enough for a 1080p RGBA32 frame
#define MAX_FRAME_SIZE (16 * 1024 * 1024)

namespace mozilla {
namespace layers {

SharedRGBImage::SharedRGBImage(ISurfaceAllocator *aAllocator) :
  Image(nullptr, SHARED_RGB),
  mSize(0, 0),
  mSurfaceAllocator(aAllocator),
  mAllocated(false),
  mShmem(new ipc::Shmem())
{
  MOZ_COUNT_CTOR(SharedRGBImage);
}

SharedRGBImage::~SharedRGBImage()
{
  MOZ_COUNT_DTOR(SharedRGBImage);

  if (mAllocated) {
    SurfaceDescriptor desc;
    DropToSurfaceDescriptor(desc);
    mSurfaceAllocator->DestroySharedSurface(&desc);
  }
  delete mShmem;
}

already_AddRefed<SharedRGBImage>
SharedRGBImage::Create(ImageContainer *aImageContainer,
                       nsIntSize aSize,
                       gfxImageFormat aImageFormat)
{
  NS_ASSERTION(aImageFormat == gfxASurface::ImageFormatARGB32 ||
               aImageFormat == gfxASurface::ImageFormatRGB24 ||
               aImageFormat == gfxASurface::ImageFormatRGB16_565,
               "RGB formats supported only");

  if (!aImageContainer) {
    NS_WARNING("No ImageContainer to allocate SharedRGBImage");
    return nullptr;
  }

  ImageFormat format = SHARED_RGB;
  nsRefPtr<Image> image = aImageContainer->CreateImage(&format, 1);

  if (!image) {
    NS_WARNING("Failed to create SharedRGBImage");
    return nullptr;
  }

  nsRefPtr<SharedRGBImage> rgbImage = static_cast<SharedRGBImage*>(image.get());
  rgbImage->mSize = gfxIntSize(aSize.width, aSize.height);
  rgbImage->mImageFormat = aImageFormat;

  if (!rgbImage->AllocateBuffer(aSize, aImageFormat)) {
    NS_WARNING("Failed to allocate shared memory for SharedRGBImage");
    return nullptr;
  }

  return rgbImage.forget();
}

uint8_t *
SharedRGBImage::GetBuffer()
{
  return mShmem->get<uint8_t>();
}

size_t
SharedRGBImage::GetBufferSize()
{
  return mSize.width * mSize.height * gfxASurface::BytesPerPixel(mImageFormat);
}

gfxIntSize
SharedRGBImage::GetSize()
{
  return mSize;
}

bool
SharedRGBImage::AllocateBuffer(nsIntSize aSize, gfxImageFormat aImageFormat)
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
SharedRGBImage::GetAsSurface()
{
  return nullptr;
}

bool
SharedRGBImage::ToSurfaceDescriptor(SurfaceDescriptor& aResult)
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
SharedRGBImage::DropToSurfaceDescriptor(SurfaceDescriptor& aResult)
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

SharedRGBImage*
SharedRGBImage::FromSurfaceDescriptor(const SurfaceDescriptor& aDescriptor)
{
  if (aDescriptor.type() != SurfaceDescriptor::TRGBImage) {
    return nullptr;
  }
  const RGBImage& rgb = aDescriptor.get_RGBImage();
  if (rgb.owner() == 0) {
    return nullptr;
  }
  return reinterpret_cast<SharedRGBImage*>(rgb.owner());
}


} // namespace layers
} // namespace mozilla
