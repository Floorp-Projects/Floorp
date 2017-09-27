/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURECLIENTOGL_H
#define MOZILLA_GFX_TEXTURECLIENTOGL_H

#include "GLContextTypes.h"             // for SharedTextureHandle, etc
#include "GLImages.h"
#include "gfxTypes.h"
#include "mozilla/Attributes.h"         // for override
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/TextureClient.h"  // for TextureClient, etc
#include "AndroidSurfaceTexture.h"

namespace mozilla {

namespace layers {

#ifdef MOZ_WIDGET_ANDROID

class AndroidSurfaceTextureData : public TextureData
{
public:
  static already_AddRefed<TextureClient>
  CreateTextureClient(AndroidSurfaceTextureHandle aHandle,
                      gfx::IntSize aSize,
                      bool aContinuous,
                      gl::OriginPos aOriginPos,
                      LayersIPCChannel* aAllocator,
                      TextureFlags aFlags);

  ~AndroidSurfaceTextureData();

  virtual void FillInfo(TextureData::Info& aInfo) const override;

  virtual bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  // Useless functions.
  virtual bool Lock(OpenMode) override { return true; }

  virtual void Unlock() override {}

  // Our data is always owned externally.
  virtual void Deallocate(LayersIPCChannel*) override {}

protected:
  AndroidSurfaceTextureData(AndroidSurfaceTextureHandle aHandle, gfx::IntSize aSize, bool aContinuous);

  const AndroidSurfaceTextureHandle mHandle;
  const gfx::IntSize mSize;
  const bool mContinuous;
};

#endif // MOZ_WIDGET_ANDROID

} // namespace layers
} // namespace mozilla

#endif
