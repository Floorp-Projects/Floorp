/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GRALLOCTEXTUREHOST_BASIC_H
#define MOZILLA_GFX_GRALLOCTEXTUREHOST_BASIC_H

#include "mozilla/layers/BasicCompositor.h"
#include "mozilla/layers/ShadowLayerUtilsGralloc.h"
#include "mozilla/layers/TextureHostBasic.h"

namespace mozilla {
namespace layers {

class BasicCompositor;

/**
 * A TextureHost for shared gralloc
 *
 * Most of the logic actually happens in GrallocTextureSourceBasic.
 */
class GrallocTextureHostBasic : public TextureHost
{
public:
  GrallocTextureHostBasic(TextureFlags aFlags,
                          const SurfaceDescriptorGralloc& aDescriptor);

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual Compositor* GetCompositor() override;

  virtual bool Lock() override;

  virtual gfx::SurfaceFormat GetFormat() const override;

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTexture) override;

  virtual void UnbindTextureSource() override;

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override
  {
    return nullptr; // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

  virtual void WaitAcquireFenceHandleSyncComplete() override;

  virtual gfx::IntSize GetSize() const override { return mCropSize; }

  virtual void SetCropRect(nsIntRect aCropRect) override;

  virtual void DeallocateSharedData() override;

  virtual void ForgetSharedData() override;

  virtual void DeallocateDeviceData() override;

  virtual LayerRenderState GetRenderState() override;

  bool IsValid() const;

  void ClearTextureSource();

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() override { return "GrallocTextureHostBasic"; }
#endif

protected:
  RefPtr<BasicCompositor> mCompositor;
  RefPtr<DataTextureSource> mTextureSource;
  SurfaceDescriptorGralloc mGrallocHandle;
  // gralloc buffer size.
  gfx::IntSize mSize;
  // Size reported by TextureClient, can be different in some cases (video?),
  // used by LayerRenderState.
  gfx::IntSize mCropSize;
  gfx::SurfaceFormat mFormat;
  bool mIsOpaque;
  /**
   * Points to a mapped gralloc buffer when TextureSource is valid.
   */
  uint8_t* mMappedBuffer;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_GFX_GRALLOCTEXTUREHOST_BASIC_H
