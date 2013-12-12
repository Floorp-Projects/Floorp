/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_MACIOSURFACETEXTUREHOST_BASIC_H
#define MOZILLA_GFX_MACIOSURFACETEXTUREHOST_BASIC_H

#include "mozilla/layers/TextureHostBasic.h"

class MacIOSurface;

namespace mozilla {
namespace layers {

class BasicCompositor;

/**
 * A texture source meant for use with BasicCompositor.
 *
 * It does not own any GL texture, and attaches its shared handle to one of
 * the compositor's temporary textures when binding.
 */
class MacIOSurfaceTextureSourceBasic
  : public TextureSourceBasic,
    public NewTextureSource
{
public:
  MacIOSurfaceTextureSourceBasic(BasicCompositor* aCompositor,
                                 MacIOSurface* aSurface);
  virtual ~MacIOSurfaceTextureSourceBasic();

  virtual TextureSourceBasic* AsSourceBasic() MOZ_OVERRIDE { return this; }

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;
  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE;
  virtual gfx::SourceSurface* GetSurface() MOZ_OVERRIDE;

  virtual void DeallocateDeviceData() MOZ_OVERRIDE { }

  void SetCompositor(BasicCompositor* aCompositor) {
    mCompositor = aCompositor;
  }

protected:
  BasicCompositor* mCompositor;
  RefPtr<MacIOSurface> mSurface;
  RefPtr<gfx::SourceSurface> mSourceSurface;
};

/**
 * A TextureHost for shared MacIOSurface
 *
 * Most of the logic actually happens in MacIOSurfaceTextureSourceBasic.
 */
class MacIOSurfaceTextureHostBasic : public TextureHost
{
public:
  MacIOSurfaceTextureHostBasic(TextureFlags aFlags,
                               const SurfaceDescriptorMacIOSurface& aDescriptor);

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  virtual bool Lock() MOZ_OVERRIDE;

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE;

  virtual NewTextureSource* GetTextureSources() MOZ_OVERRIDE
  {
    return mTextureSource;
  }

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() MOZ_OVERRIDE
  {
    return nullptr; // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() { return "MacIOSurfaceTextureHostBasic"; }
#endif

protected:
  BasicCompositor* mCompositor;
  RefPtr<MacIOSurfaceTextureSourceBasic> mTextureSource;
  RefPtr<MacIOSurface> mSurface;
};

}
}

#endif // MOZILLA_GFX_MACIOSURFACETEXTUREHOSTOGL_BASIC_H
