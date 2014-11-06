/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_MACIOSURFACETEXTUREHOSTOGL_H
#define MOZILLA_GFX_MACIOSURFACETEXTUREHOSTOGL_H

#include "mozilla/layers/TextureHostOGL.h"

class MacIOSurface;

namespace mozilla {
namespace layers {

/**
 * A texture source meant for use with MacIOSurfaceTextureHostOGL.
 *
 * It does not own any GL texture, and attaches its shared handle to one of
 * the compositor's temporary textures when binding.
 */
class MacIOSurfaceTextureSourceOGL : public NewTextureSource
                                   , public TextureSourceOGL
{
public:
  MacIOSurfaceTextureSourceOGL(CompositorOGL* aCompositor,
                               MacIOSurface* aSurface);
  virtual ~MacIOSurfaceTextureSourceOGL();

  virtual TextureSourceOGL* AsSourceOGL() { return this; }

  virtual void BindTexture(GLenum activetex, gfx::Filter aFilter) MOZ_OVERRIDE;

  virtual bool IsValid() const MOZ_OVERRIDE { return !!gl(); }

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE;

  virtual GLenum GetTextureTarget() const { return LOCAL_GL_TEXTURE_RECTANGLE_ARB; }

  virtual GLenum GetWrapMode() const MOZ_OVERRIDE { return LOCAL_GL_CLAMP_TO_EDGE; }

  // MacIOSurfaceTextureSourceOGL doesn't own any gl texture
  virtual void DeallocateDeviceData() {}

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  gl::GLContext* gl() const;

protected:
  CompositorOGL* mCompositor;
  RefPtr<MacIOSurface> mSurface;
};

/**
 * A TextureHost for shared MacIOSurface
 *
 * Most of the logic actually happens in MacIOSurfaceTextureSourceOGL.
 */
class MacIOSurfaceTextureHostOGL : public TextureHost
{
public:
  MacIOSurfaceTextureHostOGL(TextureFlags aFlags,
                             const SurfaceDescriptorMacIOSurface& aDescriptor);

  // SharedTextureHostOGL doesn't own any GL texture
  virtual void DeallocateDeviceData() MOZ_OVERRIDE {}

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

  gl::GLContext* gl() const;

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() { return "MacIOSurfaceTextureHostOGL"; }
#endif

protected:
  CompositorOGL* mCompositor;
  RefPtr<MacIOSurfaceTextureSourceOGL> mTextureSource;
  RefPtr<MacIOSurface> mSurface;
};

}
}

#endif // MOZILLA_GFX_MACIOSURFACETEXTUREHOSTOGL_H
