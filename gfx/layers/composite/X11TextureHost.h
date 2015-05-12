/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_X11TEXTUREHOST__H
#define MOZILLA_GFX_X11TEXTUREHOST__H

#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/gfx/Types.h"

#include "gfxXlibSurface.h"

namespace mozilla {
namespace layers {

// TextureHost for Xlib-backed TextureSources.
class X11TextureHost : public TextureHost
{
public:
  X11TextureHost(TextureFlags aFlags,
                 const SurfaceDescriptorX11& aDescriptor);

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual bool Lock() override;

  virtual gfx::SurfaceFormat GetFormat() const override;

  virtual gfx::IntSize GetSize() const override;

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTexture) override
  {
    aTexture = mTextureSource;
    return !!aTexture;
  }

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() override;

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() override { return "X11TextureHost"; }
#endif

protected:
  RefPtr<Compositor> mCompositor;
  RefPtr<TextureSource> mTextureSource;
  RefPtr<gfxXlibSurface> mSurface;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_GFX_X11TEXTUREHOST__H
