/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_MACIOSURFACETEXTUREHOST_BASIC_H
#define MOZILLA_GFX_MACIOSURFACETEXTUREHOST_BASIC_H

#include "mozilla/layers/BasicCompositor.h"
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
    public TextureSource
{
public:
  MacIOSurfaceTextureSourceBasic(BasicCompositor* aCompositor,
                                 MacIOSurface* aSurface);
  virtual ~MacIOSurfaceTextureSourceBasic();

  virtual const char* Name() const override { return "MacIOSurfaceTextureSourceBasic"; }

  virtual TextureSourceBasic* AsSourceBasic() override { return this; }

  virtual gfx::IntSize GetSize() const override;
  virtual gfx::SurfaceFormat GetFormat() const override;
  virtual gfx::SourceSurface* GetSurface(gfx::DrawTarget* aTarget) override;

  virtual void DeallocateDeviceData() override { }

  virtual void SetCompositor(Compositor* aCompositor) override;

protected:
  RefPtr<BasicCompositor> mCompositor;
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

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual Compositor* GetCompositor() override { return mCompositor; }

  virtual bool Lock() override;

  virtual gfx::SurfaceFormat GetFormat() const override;

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTexture) override
  {
    aTexture = mTextureSource;
    return !!aTexture;
  }

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override
  {
    return nullptr; // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

  virtual gfx::IntSize GetSize() const override;

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() override { return "MacIOSurfaceTextureHostBasic"; }
#endif

protected:
  RefPtr<BasicCompositor> mCompositor;
  RefPtr<MacIOSurfaceTextureSourceBasic> mTextureSource;
  RefPtr<MacIOSurface> mSurface;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_GFX_MACIOSURFACETEXTUREHOSTOGL_BASIC_H
