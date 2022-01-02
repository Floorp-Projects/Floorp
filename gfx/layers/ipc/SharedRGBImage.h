/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHAREDRGBIMAGE_H_
#define SHAREDRGBIMAGE_H_

#include <stddef.h>  // for size_t
#include <stdint.h>  // for uint8_t

#include "ImageContainer.h"  // for ISharedImage, Image, etc
#include "gfxTypes.h"
#include "mozilla/Attributes.h"  // for override
#include "mozilla/RefPtr.h"      // for RefPtr
#include "mozilla/gfx/Point.h"   // for IntSize
#include "mozilla/gfx/Types.h"   // for SurfaceFormat
#include "nsCOMPtr.h"            // for already_AddRefed

namespace mozilla {
namespace layers {

class ImageClient;
class TextureClient;

/**
 * Stores RGB data in shared memory
 * It is assumed that the image width and stride are equal
 */
class SharedRGBImage : public Image {
 public:
  explicit SharedRGBImage(ImageClient* aCompositable);
  explicit SharedRGBImage(TextureClientRecycleAllocator* aRecycleAllocator);

 protected:
  virtual ~SharedRGBImage();

 public:
  TextureClient* GetTextureClient(KnowsCompositor* aKnowsCompositor) override;

  gfx::IntSize GetSize() const override;

  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;

  bool Allocate(gfx::IntSize aSize, gfx::SurfaceFormat aFormat);

 private:
  TextureClientRecycleAllocator* RecycleAllocator();

  gfx::IntSize mSize;
  RefPtr<TextureClient> mTextureClient;
  RefPtr<ImageClient> mCompositable;
  RefPtr<TextureClientRecycleAllocator> mRecycleAllocator;
  RefPtr<gfx::SourceSurface> mSourceSurface;
};

}  // namespace layers
}  // namespace mozilla

#endif
