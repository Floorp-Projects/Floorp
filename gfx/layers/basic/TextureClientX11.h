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

  virtual TextureClientSurface* AsTextureClientSurface() MOZ_OVERRIDE { return this; }

  virtual bool IsAllocated() const MOZ_OVERRIDE;

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) MOZ_OVERRIDE;

  virtual TextureClientData* DropTextureData() MOZ_OVERRIDE;

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE { return mSize; }

  virtual bool Lock(OpenMode aMode) MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual bool IsLocked() const MOZ_OVERRIDE { return mLocked; }

  virtual bool AllocateForSurface(gfx::IntSize aSize, TextureAllocationFlags flags) MOZ_OVERRIDE;

  virtual bool CanExposeDrawTarget() const MOZ_OVERRIDE { return true; }

  virtual TemporaryRef<gfx::DrawTarget> GetAsDrawTarget() MOZ_OVERRIDE;

  virtual gfx::SurfaceFormat GetFormat() const { return mFormat; }

  virtual bool UpdateSurface(gfxASurface* aSurface) MOZ_OVERRIDE;

  virtual already_AddRefed<gfxASurface> GetAsSurface() MOZ_OVERRIDE;

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
