/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_MACIOSURFACETEXTUREHOSTOGL_H
#define MOZILLA_GFX_MACIOSURFACETEXTUREHOSTOGL_H

#include "MacIOSurfaceHelpers.h"
#include "mozilla/gfx/2D.h"
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
class MacIOSurfaceTextureHostOGL : public TextureHost {
 public:
  MacIOSurfaceTextureHostOGL(TextureFlags aFlags,
                             const SurfaceDescriptorMacIOSurface& aDescriptor);
  virtual ~MacIOSurfaceTextureHostOGL();

  // MacIOSurfaceTextureSourceOGL doesn't own any GL texture
  void DeallocateDeviceData() override {}

  gfx::SurfaceFormat GetFormat() const override;
  gfx::SurfaceFormat GetReadFormat() const override;

  already_AddRefed<gfx::DataSourceSurface> GetAsSurface(
      gfx::DataSourceSurface* aSurface) override {
    RefPtr<gfx::SourceSurface> surf =
        CreateSourceSurfaceFromMacIOSurface(GetMacIOSurface(), aSurface);
    return surf->GetDataSurface();
  }

  gl::GLContext* gl() const;

  gfx::IntSize GetSize() const override;

#ifdef MOZ_LAYERS_HAVE_LOG
  const char* Name() override { return "MacIOSurfaceTextureHostOGL"; }
#endif

  MacIOSurfaceTextureHostOGL* AsMacIOSurfaceTextureHost() override {
    return this;
  }

  MacIOSurface* GetMacIOSurface() override { return mSurface; }

  void CreateRenderTexture(
      const wr::ExternalImageId& aExternalImageId) override;

  uint32_t NumSubTextures() override;

  void PushResourceUpdates(wr::TransactionBuilder& aResources,
                           ResourceUpdateOp aOp,
                           const Range<wr::ImageKey>& aImageKeys,
                           const wr::ExternalImageId& aExtID) override;

  void PushDisplayItems(wr::DisplayListBuilder& aBuilder,
                        const wr::LayoutRect& aBounds,
                        const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
                        const Range<wr::ImageKey>& aImageKeys,
                        PushDisplayItemFlagSet aFlags) override;

  gfx::YUVColorSpace GetYUVColorSpace() const override;
  gfx::ColorRange GetColorRange() const override;

 protected:
  RefPtr<GLTextureSource> mTextureSource;
  RefPtr<MacIOSurface> mSurface;
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_MACIOSURFACETEXTUREHOSTOGL_H
