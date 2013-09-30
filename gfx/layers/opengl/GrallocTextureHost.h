/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GRALLOCTEXTUREHOST_H
#define MOZILLA_GFX_GRALLOCTEXTUREHOST_H
#ifdef MOZ_WIDGET_GONK

#include "mozilla/layers/TextureHostOGL.h"
#include "mozilla/layers/ShadowLayerUtilsGralloc.h"
#include <ui/GraphicBuffer.h>

namespace mozilla {
namespace layers {

class GrallocTextureHostOGL;

class GrallocTextureSourceOGL : public NewTextureSource
                              , public TextureSourceOGL
{
public:
  friend class GrallocTextureHostOGL;

  GrallocTextureSourceOGL(CompositorOGL* aCompositor,
                          android::GraphicBuffer* aGraphicBuffer,
                          gfx::SurfaceFormat aFormat);

  virtual ~GrallocTextureSourceOGL();

  virtual bool IsValid() const MOZ_OVERRIDE;

  virtual void BindTexture(GLenum aTextureUnit) MOZ_OVERRIDE;

  virtual void UnbindTexture() MOZ_OVERRIDE {}

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

  virtual TextureSourceOGL* AsSourceOGL() MOZ_OVERRIDE { return this; }

  virtual GLenum GetTextureTarget() const MOZ_OVERRIDE;

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE;

  virtual GLenum GetWrapMode() const MOZ_OVERRIDE
  {
    return LOCAL_GL_CLAMP_TO_EDGE;
  }

  virtual void SetCompositableBackendSpecificData(CompositableBackendSpecificData* aBackendData) MOZ_OVERRIDE;

  void DeallocateDeviceData();

  gl::GLContext* gl() const;

  void SetCompositor(CompositorOGL* aCompositor);

  void ForgetBuffer()
  {
    mGraphicBuffer = nullptr;
  }

  already_AddRefed<gfxImageSurface> GetAsSurface();

  GLuint GetGLTexture();

protected:
  CompositorOGL* mCompositor;
  android::sp<android::GraphicBuffer> mGraphicBuffer;
  EGLImage mEGLImage;
  gfx::SurfaceFormat mFormat;
};

class GrallocTextureHostOGL : public TextureHost
{
  friend class GrallocBufferActor;
public:
  GrallocTextureHostOGL(uint64_t aID,
                        TextureFlags aFlags,
                        const NewSurfaceDescriptorGralloc& aDescriptor);

  virtual ~GrallocTextureHostOGL();

  virtual void Updated(const nsIntRegion* aRegion) MOZ_OVERRIDE {}

  virtual bool Lock() MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  virtual void DeallocateSharedData() MOZ_OVERRIDE;

  virtual void DeallocateDeviceData() MOZ_OVERRIDE;

  virtual gfx::SurfaceFormat GetFormat() const;

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE { return mSize; }

  virtual LayerRenderState GetRenderState() MOZ_OVERRIDE;

  virtual NewTextureSource* GetTextureSources() MOZ_OVERRIDE
  {
    return mTextureSource;
  }

  virtual already_AddRefed<gfxImageSurface> GetAsSurface() MOZ_OVERRIDE;

  virtual void SetCompositableBackendSpecificData(CompositableBackendSpecificData* aBackendData) MOZ_OVERRIDE;

  bool IsValid() const;

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() { return "GrallocTextureHostOGL"; }
#endif

private:
  GrallocBufferActor* mGrallocActor;
  RefPtr<GrallocTextureSourceOGL> mTextureSource;
  gfx::IntSize mSize; // See comment in textureClientOGL.h
};

} // namespace layers
} // namespace mozilla

#endif
#endif
