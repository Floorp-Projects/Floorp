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
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
                            , public TextureHostOGL
#endif
{
  friend class GrallocBufferActor;
public:
  GrallocTextureHostOGL(TextureFlags aFlags,
                        const NewSurfaceDescriptorGralloc& aDescriptor);

  virtual ~GrallocTextureHostOGL();

  virtual void Updated(const nsIntRegion* aRegion) override {}

  virtual bool Lock() override;

  virtual void Unlock() override;

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual void DeallocateSharedData() override;

  virtual void ForgetSharedData() override;

  virtual void DeallocateDeviceData() override;

  virtual gfx::SurfaceFormat GetFormat() const;

  virtual gfx::IntSize GetSize() const override { return mDescriptorSize; }

  virtual LayerRenderState GetRenderState() override;

  virtual void PrepareTextureSource(CompositableTextureSourceRef& aTextureSource) override;

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTextureSource) override;

  virtual void UnbindTextureSource() override;

  virtual FenceHandle GetAndResetReleaseFenceHandle() override;

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
  virtual TextureHostOGL* AsHostOGL() override
  {
    return this;
  }
#endif

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() override;

  bool IsValid() const;

  virtual const char* Name() override { return "GrallocTextureHostOGL"; }

  gl::GLContext* GetGLContext() const { return mCompositor ? mCompositor->gl() : nullptr; }

private:
  void DestroyEGLImage();

  NewSurfaceDescriptorGralloc mGrallocHandle;
  RefPtr<GLTextureSource> mGLTextureSource;
  RefPtr<CompositorOGL> mCompositor;
  // Size reported by the GraphicBuffer
  gfx::IntSize mSize;
  // Size reported by TextureClient, can be different in some cases (video?),
  // used by LayerRenderState.
  gfx::IntSize mDescriptorSize;
  gfx::SurfaceFormat mFormat;
  EGLImage mEGLImage;
  bool mIsOpaque;
};

} // namespace layers
} // namespace mozilla

#endif
#endif
