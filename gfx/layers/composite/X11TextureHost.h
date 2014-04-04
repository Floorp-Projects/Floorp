/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_X11TEXTUREHOST__H
#define MOZILLA_GFX_X11TEXTUREHOST__H

#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/gfx/Types.h"

class gfxXlibSurface;

namespace mozilla {
namespace layers {

// TextureHost for Xlib-backed TextureSources.
class X11TextureHost : public TextureHost
{
public:
  X11TextureHost(TextureFlags aFlags,
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
  virtual const char* Name() { return "X11TextureHost"; }
#endif

protected:
  Compositor* mCompositor;
  RefPtr<NewTextureSource> mTextureSource;
  RefPtr<gfxXlibSurface> mSurface;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_GFX_X11TEXTUREHOST__H
