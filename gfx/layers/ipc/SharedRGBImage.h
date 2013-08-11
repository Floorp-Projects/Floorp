/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHAREDRGBIMAGE_H_
#define SHAREDRGBIMAGE_H_

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint8_t
#include "ImageContainer.h"             // for ISharedImage, Image, etc
#include "gfxASurface.h"                // for gfxASurface, etc
#include "gfxPoint.h"                   // for gfxIntSize
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Types.h"          // for SurfaceFormat
#include "nsCOMPtr.h"                   // for already_AddRefed

namespace mozilla {
namespace ipc {
class Shmem;
}

namespace layers {

class BufferTextureClient;
class ImageClient;
class ISurfaceAllocator;
class TextureClient;
class SurfaceDescriptor;

already_AddRefed<Image> CreateSharedRGBImage(ImageContainer* aImageContainer,
                                             nsIntSize aSize,
                                             gfxASurface::gfxImageFormat aImageFormat);

/**
 * Stores RGB data in shared memory
 * It is assumed that the image width and stride are equal
 */
class DeprecatedSharedRGBImage : public Image,
                                 public ISharedImage
{
friend already_AddRefed<Image> CreateSharedRGBImage(ImageContainer* aImageContainer,
                                                    nsIntSize aSize,
                                                    gfxASurface::gfxImageFormat aImageFormat);
public:
  typedef gfxASurface::gfxImageFormat gfxImageFormat;
  struct Header {
    gfxImageFormat mImageFormat;
  };

  DeprecatedSharedRGBImage(ISurfaceAllocator *aAllocator);
  ~DeprecatedSharedRGBImage();

  virtual ISharedImage* AsSharedImage() MOZ_OVERRIDE { return this; }

  virtual uint8_t *GetBuffer() MOZ_OVERRIDE;

  gfxIntSize GetSize();
  size_t GetBufferSize();

  static uint8_t BytesPerPixel(gfxImageFormat aImageFormat);
  already_AddRefed<gfxASurface> GetAsSurface();

  /**
   * Setup the Surface descriptor to contain this image's shmem, while keeping
   * ownership of the shmem.
   * if the operation succeeds, return true and AddRef this DeprecatedSharedRGBImage.
   */
  bool ToSurfaceDescriptor(SurfaceDescriptor& aResult);

  /**
   * Setup the Surface descriptor to contain this image's shmem, and loose
   * ownership of the shmem.
   * if the operation succeeds, return true (and does _not_ AddRef this
   * DeprecatedSharedRGBImage).
   */
  bool DropToSurfaceDescriptor(SurfaceDescriptor& aResult);

  /**
   * Returns a DeprecatedSharedRGBImage* iff the descriptor was initialized with
   * ToSurfaceDescriptor.
   */
  static DeprecatedSharedRGBImage* FromSurfaceDescriptor(const SurfaceDescriptor& aDescriptor);

  bool AllocateBuffer(nsIntSize aSize, gfxImageFormat aImageFormat);

  TextureClient* GetTextureClient() MOZ_OVERRIDE { return nullptr; }

protected:
  gfxIntSize mSize;
  gfxImageFormat mImageFormat;
  ISurfaceAllocator* mSurfaceAllocator;

  bool mAllocated;
  ipc::Shmem *mShmem;
};

/**
 * Stores RGB data in shared memory
 * It is assumed that the image width and stride are equal
 */
class SharedRGBImage : public Image
                     , public ISharedImage
{
  typedef gfxASurface::gfxImageFormat gfxImageFormat;
public:
  SharedRGBImage(ImageClient* aCompositable);
  ~SharedRGBImage();

  virtual ISharedImage* AsSharedImage() MOZ_OVERRIDE { return this; }

  virtual TextureClient* GetTextureClient() MOZ_OVERRIDE;

  virtual uint8_t* GetBuffer() MOZ_OVERRIDE;

  gfxIntSize GetSize();

  size_t GetBufferSize();

  already_AddRefed<gfxASurface> GetAsSurface();

  bool Allocate(gfx::IntSize aSize, gfx::SurfaceFormat aFormat);
private:
  gfx::IntSize mSize;
  RefPtr<ImageClient> mCompositable;
  RefPtr<BufferTextureClient> mTextureClient;
};

} // namespace layers
} // namespace mozilla

#endif
