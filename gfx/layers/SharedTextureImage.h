/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_SHAREDTEXTUREIMAGE_H
#define GFX_SHAREDTEXTUREIMAGE_H

#include "GLContextProvider.h"          // for GLContextProvider
#include "ImageContainer.h"             // for Image
#include "ImageTypes.h"                 // for ImageFormat::SHARED_TEXTURE
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "mozilla/gfx/Point.h"          // for IntSize

class gfxASurface;

// Split into a separate header from ImageLayers.h due to GLContext.h dependence
// Implementation remains in ImageLayers.cpp

namespace mozilla {

namespace layers {

class SharedTextureImage : public Image {
public:
  struct Data {
    gl::SharedTextureHandle mHandle;
    gl::SharedTextureShareType mShareType;
    gfx::IntSize mSize;
    bool mInverted;
  };

  void SetData(const Data& aData) { mData = aData; }
  const Data* GetData() { return &mData; }

  gfx::IntSize GetSize() { return mData.mSize; }

  virtual TemporaryRef<gfx::SourceSurface> GetAsSourceSurface() MOZ_OVERRIDE
  {
    return nullptr;
  }

  SharedTextureImage() : Image(nullptr, ImageFormat::SHARED_TEXTURE) {}

private:
  Data mData;
};

} // layers
} // mozilla

#endif // GFX_SHAREDTEXTUREIMAGE_H
