/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GPUVIDEOTEXTUREHOST_H
#define MOZILLA_GFX_GPUVIDEOTEXTUREHOST_H

#include "mozilla/layers/TextureHost.h"

namespace mozilla {
namespace layers {

class GPUVideoTextureHost : public TextureHost
{
public:
 GPUVideoTextureHost(TextureFlags aFlags,
                     const SurfaceDescriptorGPUVideo& aDescriptor);
  virtual ~GPUVideoTextureHost();

  virtual void DeallocateDeviceData() override {}

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
  virtual const char* Name() override { return "GPUVideoTextureHost"; }
#endif

protected:
  RefPtr<Compositor> mCompositor;
  RefPtr<TextureSource> mTextureSource;
  RefPtr<Image> mImage;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_GFX_GPUVIDEOTEXTUREHOST_H
