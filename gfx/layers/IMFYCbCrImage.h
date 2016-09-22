/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_IMFYCBCRIMAGE_H
#define GFX_IMFYCBCRIMAGE_H

#include "mozilla/RefPtr.h"
#include "ImageContainer.h"
#include "mfidl.h"

namespace mozilla {
namespace layers {

class IMFYCbCrImage : public RecyclingPlanarYCbCrImage
{
public:
  IMFYCbCrImage(IMFMediaBuffer* aBuffer, IMF2DBuffer* a2DBuffer);

  virtual bool IsValid() { return true; }

  virtual TextureClient* GetTextureClient(TextureForwarder* aForwarder) override;

protected:

  TextureClient* GetD3D9TextureClient(TextureForwarder* aForwarder);

  ~IMFYCbCrImage();

  RefPtr<IMFMediaBuffer> mBuffer;
  RefPtr<IMF2DBuffer> m2DBuffer;
  RefPtr<TextureClient> mTextureClient;
};

} // namepace layers
} // namespace mozilla

#endif // GFX_D3DSURFACEIMAGE_H
