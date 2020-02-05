/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_WAYLANDDMABUFTEXTUREHOSTOGL_H
#define MOZILLA_GFX_WAYLANDDMABUFTEXTUREHOSTOGL_H

#include "mozilla/gfx/2D.h"
#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/TextureHostOGL.h"

class WaylandDMABufSurface;

namespace mozilla {
namespace layers {

/**
 * A TextureHost for shared class WaylandDMABufSurface;
 */
class WaylandDMABUFTextureHostOGL : public TextureHost {
 public:
  WaylandDMABUFTextureHostOGL(TextureFlags aFlags,
                              const SurfaceDescriptor& aDesc);
  virtual ~WaylandDMABUFTextureHostOGL();

  void SetTextureSourceProvider(TextureSourceProvider* aProvider) override;

  bool Lock() override;

  void Unlock() override;

  gfx::SurfaceFormat GetFormat() const override;

  bool BindTextureSource(CompositableTextureSourceRef& aTexture) override {
    aTexture = mTextureSource;
    return !!aTexture;
  }

  already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override {
    return nullptr;  // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

  gl::GLContext* gl() const;

  gfx::IntSize GetSize() const override;

#ifdef MOZ_LAYERS_HAVE_LOG
  const char* Name() override { return "WaylandDMABUFTextureHostOGL"; }
#endif

  void CreateRenderTexture(
      const wr::ExternalImageId& aExternalImageId) override;

  void PushResourceUpdates(wr::TransactionBuilder& aResources,
                           ResourceUpdateOp aOp,
                           const Range<wr::ImageKey>& aImageKeys,
                           const wr::ExternalImageId& aExtID,
                           const bool aPreferCompositorSurface) override;

  void PushDisplayItems(wr::DisplayListBuilder& aBuilder,
                        const wr::LayoutRect& aBounds,
                        const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
                        const Range<wr::ImageKey>& aImageKeys) override;

 protected:
  RefPtr<EGLImageTextureSource> mTextureSource;
  RefPtr<WaylandDMABufSurface> mSurface;
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_WAYLANDDMABUFTEXTUREHOSTOGL_H
