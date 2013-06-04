/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_SHAREDTEXTUREIMAGE_H
#define GFX_SHAREDTEXTUREIMAGE_H

#include "ImageContainer.h"
#include "GLContext.h"
#include "GLContextProvider.h"

// Split into a separate header from ImageLayers.h due to GLContext.h dependence
// Implementation remains in ImageLayers.cpp

namespace mozilla {

namespace layers {

class SharedTextureImage : public Image {
public:
  struct Data {
    gl::SharedTextureHandle mHandle;
    gl::GLContext::SharedTextureShareType mShareType;
    gfxIntSize mSize;
    bool mInverted;
  };

  void SetData(const Data& aData) { mData = aData; }
  const Data* GetData() { return &mData; }

  gfxIntSize GetSize() { return mData.mSize; }

  virtual already_AddRefed<gfxASurface> GetAsSurface() { 
    return gl::GLContextProvider::GetSharedHandleAsSurface(mData.mShareType, mData.mHandle);
  }

  SharedTextureImage() : Image(NULL, SHARED_TEXTURE) {}

private:
  Data mData;
};

} // layers
} // mozilla

#endif // GFX_SHAREDTEXTUREIMAGE_H
