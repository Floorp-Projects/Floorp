/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURECLIENT_X11_H
#define MOZILLA_GFX_TEXTURECLIENT_X11_H

#include "mozilla/layers/TextureClient.h"
#include "ISurfaceAllocator.h" // For IsSurfaceDescriptorValid
#include "mozilla/layers/ShadowLayerUtilsX11.h"

namespace mozilla {
namespace layers {

/**
 * A TextureClient implementation based on Xlib.
 */
class TextureClientX11
 : public TextureClient,
   public TextureClientSurface
{
 public:
  TextureClientX11(gfx::SurfaceFormat format, TextureFlags aFlags = TEXTURE_FLAGS_DEFAULT);
  ~TextureClientX11();

  // TextureClient

  TextureClientSurface* AsTextureClientSurface() MOZ_OVERRIDE { return this; }

  bool IsAllocated() const MOZ_OVERRIDE;
  bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) MOZ_OVERRIDE;
  TextureClientData* DropTextureData() MOZ_OVERRIDE;
  gfx::IntSize GetSize() const {
    return mSize;
  }

  bool Lock(OpenMode aMode) MOZ_OVERRIDE;
  void Unlock() MOZ_OVERRIDE;
  bool IsLocked() const MOZ_OVERRIDE { return mLocked; }

  // TextureClientSurface

  bool UpdateSurface(gfxASurface* aSurface) MOZ_OVERRIDE;
  already_AddRefed<gfxASurface> GetAsSurface() MOZ_OVERRIDE;
  bool AllocateForSurface(gfx::IntSize aSize, TextureAllocationFlags flags) MOZ_OVERRIDE;

  TemporaryRef<gfx::DrawTarget> GetAsDrawTarget() MOZ_OVERRIDE;
  gfx::SurfaceFormat GetFormat() const {
    return mFormat;
  }

  virtual bool HasInternalBuffer() const MOZ_OVERRIDE { return false; }

 private:
  gfx::SurfaceFormat mFormat;
  gfx::IntSize mSize;
  RefPtr<gfxXlibSurface> mSurface;
  bool mLocked;
};

} // namespace layers
} // namespace mozilla

#endif
