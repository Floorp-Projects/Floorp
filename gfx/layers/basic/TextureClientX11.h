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
class TextureClientX11 : public TextureClient
{
 public:
  TextureClientX11(ISurfaceAllocator* aAllocator, gfx::SurfaceFormat format, TextureFlags aFlags = TextureFlags::DEFAULT);

  ~TextureClientX11();

  // TextureClient

  virtual bool IsAllocated() const override;

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) override;

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual bool Lock(OpenMode aMode) override;

  virtual void Unlock() override;

  virtual bool IsLocked() const override { return mLocked; }

  virtual bool AllocateForSurface(gfx::IntSize aSize, TextureAllocationFlags flags) override;

  virtual bool CanExposeDrawTarget() const override { return true; }

  virtual gfx::DrawTarget* BorrowDrawTarget() override;

  virtual void UpdateFromSurface(gfx::SourceSurface* aSurface) override;

  virtual gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  virtual bool HasInternalBuffer() const override { return false; }

  virtual already_AddRefed<TextureClient>
  CreateSimilar(TextureFlags aFlags = TextureFlags::DEFAULT,
                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override;

 private:
  gfx::SurfaceFormat mFormat;
  gfx::IntSize mSize;
  RefPtr<gfxXlibSurface> mSurface;
  RefPtr<gfx::DrawTarget> mDrawTarget;
  bool mLocked;
};

} // namespace layers
} // namespace mozilla

#endif
