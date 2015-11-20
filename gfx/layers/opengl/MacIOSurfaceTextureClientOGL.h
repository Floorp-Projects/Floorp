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
  static MacIOSurfaceTextureData* Create(MacIOSurface* aSurface);

  ~MacIOSurfaceTextureData();

  virtual gfx::IntSize GetSize() const override;

  virtual gfx::SurfaceFormat GetFormat() const override;

  virtual bool Lock(OpenMode, FenceHandle*) override { return true; }

  virtual void Unlock() override {}

  virtual bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  virtual bool HasInternalBuffer() const override { return false; }

  virtual void Deallocate(ISurfaceAllocator* aAllocator) override { mSurface = nullptr; }

  virtual void Forget(ISurfaceAllocator* aAllocator) override { mSurface = nullptr; }

  // For debugging purposes only.
  already_AddRefed<gfx::DataSourceSurface> GetAsSurface();

protected:
  explicit MacIOSurfaceTextureData(MacIOSurface* aSurface);

  virtual void FinalizeOnIPDLThread(TextureClient* aWrapper) override;

  RefPtr<MacIOSurface> mSurface;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_GFX_MACIOSURFACETEXTURECLIENTOGL_H
