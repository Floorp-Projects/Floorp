/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GRALLOCTEXTUREHOST_H
#define MOZILLA_GFX_GRALLOCTEXTUREHOST_H
#ifdef MOZ_WIDGET_GONK

#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/TextureHostOGL.h"
#include "mozilla/layers/ShadowLayerUtilsGralloc.h"
#include <ui/GraphicBuffer.h>

namespace mozilla {
namespace layers {

class GrallocTextureHostOGL : public TextureHost
{
  friend class GrallocBufferActor;
public:
  GrallocTextureHostOGL(TextureFlags aFlags,
                        const SurfaceDescriptorGralloc& aDescriptor);

  virtual ~GrallocTextureHostOGL();

  virtual bool Lock() override;

  virtual void Unlock() override;

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual Compositor* GetCompositor() override { return mCompositor; }

  virtual void DeallocateSharedData() override;

  virtual void ForgetSharedData() override;

  virtual void DeallocateDeviceData() override;

  virtual gfx::SurfaceFormat GetFormat() const;

  virtual gfx::IntSize GetSize() const override { return mCropSize; }

  virtual LayerRenderState GetRenderState() override;

  virtual void PrepareTextureSource(CompositableTextureSourceRef& aTextureSource) override;

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTextureSource) override;

  virtual void UnbindTextureSource() override;

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override;

  virtual void WaitAcquireFenceHandleSyncComplete() override;

  virtual void SetCropRect(nsIntRect aCropRect) override;

  bool IsValid() const;

  virtual const char* Name() override { return "GrallocTextureHostOGL"; }

  gl::GLContext* GetGLContext() const { return mCompositor ? mCompositor->gl() : nullptr; }

  virtual bool NeedsFenceHandle() override
  {
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
    return true;
#else
    return false;
#endif
  }

  virtual FenceHandle GetCompositorReleaseFence() override;

private:
  void DestroyEGLImage();

  SurfaceDescriptorGralloc mGrallocHandle;
  RefPtr<GLTextureSource> mGLTextureSource;
  RefPtr<CompositorOGL> mCompositor;
  // Size reported by the GraphicBuffer
  gfx::IntSize mSize;
  // Size reported by TextureClient, can be different in some cases (video?),
  // used by LayerRenderState.
  gfx::IntSize mCropSize;
  gfx::SurfaceFormat mFormat;
  EGLImage mEGLImage;
  bool mIsOpaque;
};

} // namespace layers
} // namespace mozilla

#endif
#endif
