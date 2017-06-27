/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_MACIOSURFACETEXTUREHOSTOGL_H
#define MOZILLA_GFX_MACIOSURFACETEXTUREHOSTOGL_H

#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/TextureHostOGL.h"

class MacIOSurface;

namespace mozilla {
namespace layers {

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
  virtual ~MacIOSurfaceTextureHostOGL();

  // MacIOSurfaceTextureSourceOGL doesn't own any GL texture
  virtual void DeallocateDeviceData() override {}

  virtual void SetTextureSourceProvider(TextureSourceProvider* aProvider) override;

  virtual bool Lock() override;

  virtual gfx::SurfaceFormat GetFormat() const override;
  virtual gfx::SurfaceFormat GetReadFormat() const override;

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTexture) override
  {
    aTexture = mTextureSource;
    return !!aTexture;
  }

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override
  {
    return nullptr; // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

  gl::GLContext* gl() const;

  virtual gfx::IntSize GetSize() const override;

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() override { return "MacIOSurfaceTextureHostOGL"; }
#endif

  virtual MacIOSurfaceTextureHostOGL* AsMacIOSurfaceTextureHost() override { return this; }

  MacIOSurface* GetMacIOSurface()
  {
    return mSurface;
  }

  virtual void CreateRenderTexture(const wr::ExternalImageId& aExternalImageId) override;

  virtual void GetWRImageKeys(nsTArray<wr::ImageKey>& aImageKeys,
                              const std::function<wr::ImageKey()>& aImageKeyAllocator) override;

  virtual void AddWRImage(wr::WebRenderAPI* aAPI,
                          Range<const wr::ImageKey>& aImageKeys,
                          const wr::ExternalImageId& aExtID) override;

  virtual void PushExternalImage(wr::DisplayListBuilder& aBuilder,
                                 const WrRect& aBounds,
                                 const WrRect& aClip,
                                 wr::ImageRendering aFilter,
                                 Range<const wr::ImageKey>& aImageKeys) override;

protected:
  GLTextureSource* CreateTextureSourceForPlane(size_t aPlane);

  RefPtr<GLTextureSource> mTextureSource;
  RefPtr<MacIOSurface> mSurface;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_GFX_MACIOSURFACETEXTUREHOSTOGL_H
