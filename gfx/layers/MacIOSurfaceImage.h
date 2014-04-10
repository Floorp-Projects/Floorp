/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_MACIOSURFACEIMAGE_H
#define GFX_MACIOSURFACEIMAGE_H

#include "ImageContainer.h"
#include "mozilla/gfx/MacIOSurface.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/layers/TextureClient.h"
#include "gfxImageSurface.h"

namespace mozilla {

namespace layers {

class MacIOSurfaceImage : public Image,
                          public ISharedImage {
public:
  void SetSurface(MacIOSurface* aSurface) { mSurface = aSurface; }
  MacIOSurface* GetSurface() { return mSurface; }

  gfx::IntSize GetSize() {
    return gfx::IntSize(mSurface->GetDevicePixelWidth(), mSurface->GetDevicePixelHeight());
  }

  virtual ISharedImage* AsSharedImage() MOZ_OVERRIDE { return this; }

  virtual TemporaryRef<gfx::SourceSurface> GetAsSourceSurface();

  virtual TextureClient* GetTextureClient(CompositableClient* aClient) MOZ_OVERRIDE;
  virtual uint8_t* GetBuffer() MOZ_OVERRIDE { return nullptr; }

  MacIOSurfaceImage() : Image(nullptr, ImageFormat::MAC_IOSURFACE) {}

private:
  RefPtr<MacIOSurface> mSurface;
  RefPtr<TextureClient> mTextureClient;
};

} // layers
} // mozilla

#endif // GFX_SHAREDTEXTUREIMAGE_H
