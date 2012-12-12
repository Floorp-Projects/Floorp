/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHAREDRGBIMAGE_H_
#define SHAREDRGBIMAGE_H_

#include "ImageContainer.h"

namespace mozilla {
namespace ipc {
class Shmem;
}
namespace layers {

class ImageContainerChild;
class SharedImage;

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

  SharedRGBImage(ImageContainerChild *aImageContainerChild);
  ~SharedRGBImage();

  static already_AddRefed<SharedRGBImage> Create(ImageContainer *aImageContainer,
                                                 nsIntSize aSize,
                                                 gfxImageFormat aImageFormat);
  uint8_t *GetBuffer();

  gfxIntSize GetSize();
  size_t GetBufferSize();

  static uint8_t BytesPerPixel(gfxImageFormat aImageFormat);
  already_AddRefed<gfxASurface> GetAsSurface();

  SharedImage *ToSharedImage();

private:
  bool AllocateBuffer(nsIntSize aSize, gfxImageFormat aImageFormat);

  gfxIntSize mSize;
  gfxImageFormat mImageFormat;
  ImageContainerChild *mImageContainerChild;

  bool mAllocated;
  ipc::Shmem *mShmem;
};

} // namespace layers
} // namespace mozilla

#endif
