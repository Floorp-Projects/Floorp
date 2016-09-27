/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_MACIOSURFACETEXTURECLIENTOGL_H
#define MOZILLA_GFX_MACIOSURFACETEXTURECLIENTOGL_H

#include "mozilla/layers/TextureClientOGL.h"

class MacIOSurface;

namespace mozilla {
namespace layers {

class MacIOSurfaceTextureData : public TextureData
{
public:
  static MacIOSurfaceTextureData* Create(MacIOSurface* aSurface,
                                         gfx::BackendType aBackend);

  static MacIOSurfaceTextureData*
  Create(const gfx::IntSize& aSize, gfx::SurfaceFormat aFormat,
         gfx::BackendType aBackend);

  ~MacIOSurfaceTextureData();

  virtual void FillInfo(TextureData::Info& aInfo) const override;

  virtual bool Lock(OpenMode, FenceHandle*) override;

  virtual void Unlock() override;

  virtual already_AddRefed<gfx::DrawTarget> BorrowDrawTarget() override;

  virtual bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  virtual void Deallocate(LayersIPCChannel*) override;

  virtual void Forget(LayersIPCChannel*) override;

  virtual bool UpdateFromSurface(gfx::SourceSurface* aSurface) override;

  // For debugging purposes only.
  already_AddRefed<gfx::DataSourceSurface> GetAsSurface();

protected:
  MacIOSurfaceTextureData(MacIOSurface* aSurface,
                          gfx::BackendType aBackend);

  RefPtr<MacIOSurface> mSurface;
  gfx::BackendType mBackend;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_GFX_MACIOSURFACETEXTURECLIENTOGL_H
