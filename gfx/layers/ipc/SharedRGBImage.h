/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHAREDRGBIMAGE_H_
#define SHAREDRGBIMAGE_H_

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint8_t
#include "ImageContainer.h"             // for ISharedImage, Image, etc
#include "gfxTypes.h"
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Types.h"          // for SurfaceFormat
#include "nsCOMPtr.h"                   // for already_AddRefed

class gfxASurface;

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
                                             gfxImageFormat aImageFormat);

/**
 * Stores RGB data in shared memory
 * It is assumed that the image width and stride are equal
 */
class SharedRGBImage : public Image
                     , public ISharedImage
{
public:
  SharedRGBImage(ImageClient* aCompositable);
  ~SharedRGBImage();

  virtual ISharedImage* AsSharedImage() MOZ_OVERRIDE { return this; }

  virtual TextureClient* GetTextureClient(CompositableClient* aClient) MOZ_OVERRIDE;

  virtual uint8_t* GetBuffer() MOZ_OVERRIDE;

  gfx::IntSize GetSize();

  size_t GetBufferSize();

  TemporaryRef<gfx::SourceSurface> GetAsSourceSurface();

  bool Allocate(gfx::IntSize aSize, gfx::SurfaceFormat aFormat);
private:
  gfx::IntSize mSize;
  RefPtr<ImageClient> mCompositable;
  RefPtr<BufferTextureClient> mTextureClient;
};

} // namespace layers
} // namespace mozilla

#endif
