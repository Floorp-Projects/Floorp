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

namespace mozilla {

namespace layers {

class MacIOSurfaceImage : public Image {
public:
  void SetSurface(MacIOSurface* aSurface) { mSurface = aSurface; }
  MacIOSurface* GetSurface() { return mSurface; }

  gfx::IntSize GetSize() override {
    return gfx::IntSize(mSurface->GetDevicePixelWidth(), mSurface->GetDevicePixelHeight());
  }

  virtual already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;

  virtual TextureClient* GetTextureClient(CompositableClient* aClient) override;

  MacIOSurfaceImage() : Image(nullptr, ImageFormat::MAC_IOSURFACE) {}

private:
  RefPtr<MacIOSurface> mSurface;
  RefPtr<TextureClient> mTextureClient;
};

} // namespace layers
} // namespace mozilla

#endif // GFX_SHAREDTEXTUREIMAGE_H
