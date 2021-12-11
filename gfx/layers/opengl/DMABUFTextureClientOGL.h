/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_MACIOSURFACETEXTURECLIENTOGL_H
#define MOZILLA_GFX_MACIOSURFACETEXTURECLIENTOGL_H

#include "mozilla/layers/TextureClientOGL.h"

class DMABufSurface;

namespace mozilla {
namespace layers {

class DMABUFTextureData : public TextureData {
 public:
  static DMABUFTextureData* Create(const gfx::IntSize& aSize,
                                   gfx::SurfaceFormat aFormat,
                                   gfx::BackendType aBackend);

  static DMABUFTextureData* Create(DMABufSurface* aSurface,
                                   gfx::BackendType aBackend) {
    return new DMABUFTextureData(aSurface, aBackend);
  }

  ~DMABUFTextureData();

  virtual TextureData* CreateSimilar(
      LayersIPCChannel* aAllocator, LayersBackend aLayersBackend,
      TextureFlags aFlags = TextureFlags::DEFAULT,
      TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override;

  void FillInfo(TextureData::Info& aInfo) const override;

  bool Lock(OpenMode) override;

  void Unlock() override;

  already_AddRefed<gfx::DrawTarget> BorrowDrawTarget() override;

  bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  void Deallocate(LayersIPCChannel*) override;

  void Forget(LayersIPCChannel*) override;

  bool UpdateFromSurface(gfx::SourceSurface* aSurface) override;

  // For debugging purposes only.
  already_AddRefed<gfx::DataSourceSurface> GetAsSurface();

 protected:
  DMABUFTextureData(DMABufSurface* aSurface, gfx::BackendType aBackend);

  RefPtr<DMABufSurface> mSurface;
  gfx::BackendType mBackend;
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_MACIOSURFACETEXTURECLIENTOGL_H
