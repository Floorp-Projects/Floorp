/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTUREHOSTX11__H
#define MOZILLA_GFX_TEXTUREHOSTX11__H

#include "mozilla/layers/TextureHostBasic.h"
#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace layers {

class BasicCompositor;

// TextureSource for Xlib-backed surfaces.
class TextureSourceX11
  : public TextureSourceBasic,
    public NewTextureSource
{
public:
  TextureSourceX11(BasicCompositor* aCompositor, gfxXlibSurface* aSurface);

  virtual TextureSourceX11* AsSourceBasic() MOZ_OVERRIDE { return this; }

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;
  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE;
  virtual gfx::SourceSurface* GetSurface() MOZ_OVERRIDE;

  virtual void DeallocateDeviceData() MOZ_OVERRIDE { }

  virtual void SetCompositor(Compositor* aCompositor);

protected:
  BasicCompositor* mCompositor;
  RefPtr<gfxXlibSurface> mSurface;
  RefPtr<gfx::SourceSurface> mSourceSurface;
};

// TextureSource for Xlib-backed TextureSources.
class TextureHostX11 : public TextureHost
{
public:
  TextureHostX11(TextureFlags aFlags,
                 const SurfaceDescriptorX11& aDescriptor);

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;
  virtual bool Lock() MOZ_OVERRIDE;
  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE;
  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

  virtual NewTextureSource* GetTextureSources() MOZ_OVERRIDE
  {
    return mTextureSource;
  }

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() MOZ_OVERRIDE
  {
    return nullptr; // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() { return "TextureHostX11"; }
#endif

protected:
  BasicCompositor* mCompositor;
  RefPtr<TextureSourceX11> mTextureSource;
  RefPtr<gfxXlibSurface> mSurface;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_GFX_TEXTUREHOSTX11__H
