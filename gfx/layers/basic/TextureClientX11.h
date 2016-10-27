/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURECLIENT_X11_H
#define MOZILLA_GFX_TEXTURECLIENT_X11_H

#include "mozilla/layers/TextureClient.h"
#include "ISurfaceAllocator.h" // For IsSurfaceDescriptorValid
#include "mozilla/layers/ShadowLayerUtilsX11.h"

namespace mozilla {
namespace layers {

class X11TextureData : public TextureData
{
public:
  static X11TextureData* Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                TextureFlags aFlags, LayersIPCChannel* aAllocator);

  virtual bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  virtual bool Lock(OpenMode aMode) override;

  virtual void Unlock() override;

  virtual void FillInfo(TextureData::Info& aInfo) const override;

  virtual already_AddRefed<gfx::DrawTarget> BorrowDrawTarget() override;

  virtual void Deallocate(LayersIPCChannel*) override;

  virtual TextureData*
  CreateSimilar(LayersIPCChannel* aAllocator,
                LayersBackend aLayersBackend,
                TextureFlags aFlags = TextureFlags::DEFAULT,
                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override;

  virtual bool UpdateFromSurface(gfx::SourceSurface* aSurface) override;

protected:
  X11TextureData(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                 bool aClientDeallocation, bool aIsCrossProcess,
                 gfxXlibSurface* aSurface);

  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
  RefPtr<gfxXlibSurface> mSurface;
  bool mClientDeallocation;
  bool mIsCrossProcess;
};

} // namespace layers
} // namespace mozilla

#endif
