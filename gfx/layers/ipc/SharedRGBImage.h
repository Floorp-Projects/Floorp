/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHAREDRGBIMAGE_H_
#define SHAREDRGBIMAGE_H_

#include "ImageContainer.h"
#include "ISurfaceAllocator.h"

namespace mozilla {
namespace ipc {
class Shmem;
}
namespace layers {

/**
 * Stores RGB data in shared memory
 * It is assumed that the image width and stride are equal
 */
class SharedRGBImage : public Image
{
  typedef gfxASurface::gfxImageFormat gfxImageFormat;
public:
  struct Header {
    gfxImageFormat mImageFormat;
  };

  SharedRGBImage(ISurfaceAllocator *aAllocator);
  ~SharedRGBImage();

  static already_AddRefed<SharedRGBImage> Create(ImageContainer* aImageContainer,
                                                 nsIntSize aSize,
                                                 gfxImageFormat aImageFormat);
  uint8_t *GetBuffer();

  gfxIntSize GetSize();
  size_t GetBufferSize();

  static uint8_t BytesPerPixel(gfxImageFormat aImageFormat);
  already_AddRefed<gfxASurface> GetAsSurface();

  /**
   * Setup the Surface descriptor to contain this image's shmem, while keeping
   * ownership of the shmem.
   * if the operation succeeds, return true and AddRef this SharedRGBImage.
   */
  bool ToSurfaceDescriptor(SurfaceDescriptor& aResult);

  /**
   * Setup the Surface descriptor to contain this image's shmem, and loose
   * ownership of the shmem.
   * if the operation succeeds, return true (and does _not_ AddRef this
   * SharedRGBImage).
   */
  bool DropToSurfaceDescriptor(SurfaceDescriptor& aResult);

  /**
   * Returns a SharedRGBImage* iff the descriptor was initialized with
   * ToSurfaceDescriptor.
   */
  static SharedRGBImage* FromSurfaceDescriptor(const SurfaceDescriptor& aDescriptor);

private:
  bool AllocateBuffer(nsIntSize aSize, gfxImageFormat aImageFormat);

  gfxIntSize mSize;
  gfxImageFormat mImageFormat;
  ISurfaceAllocator* mSurfaceAllocator;

  bool mAllocated;
  ipc::Shmem *mShmem;
};

} // namespace layers
} // namespace mozilla

#endif
