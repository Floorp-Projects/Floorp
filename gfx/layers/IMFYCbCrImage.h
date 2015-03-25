/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_IMFYCBCRIMAGE_H
#define GFX_IMFYCBCRIMAGE_H

#include "mozilla/RefPtr.h"
#include "ImageContainer.h"
#include "Mfidl.h"

namespace mozilla {
namespace layers {

class IMFYCbCrImage : public PlanarYCbCrImage
{
public:
  IMFYCbCrImage(IMFMediaBuffer* aBuffer, IMF2DBuffer* a2DBuffer)
    : PlanarYCbCrImage(nullptr)
    , mBuffer(aBuffer)
    , m2DBuffer(a2DBuffer)
  {}

  virtual bool IsValid() { return true; }

protected:
  virtual uint8_t* AllocateBuffer(uint32_t aSize) override {
    MOZ_CRASH("Can't do manual allocations with IMFYCbCrImage");
    return nullptr;
  }

  ~IMFYCbCrImage()
  {
    if (m2DBuffer) {
      m2DBuffer->Unlock2D();
    }
    else {
      mBuffer->Unlock();
    }
  }

  RefPtr<IMFMediaBuffer> mBuffer;
  RefPtr<IMF2DBuffer> m2DBuffer;
};

} // namepace layers
} // namespace mozilla

#endif // GFX_D3DSURFACEIMAGE_H
