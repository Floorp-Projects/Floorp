/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_IMAGELAYEROGL_H
#define GFX_IMAGELAYEROGL_H

#include "mozilla/layers/PLayerTransaction.h"

#include "LayerManagerOGL.h"
#include "ImageLayers.h"
#include "ImageContainer.h"
#include "yuv_convert.h"
#include "mozilla/Mutex.h"

namespace mozilla {
namespace layers {

class CairoImage;
class PlanarYCbCrImage;
class BlobYCbCrSurface;

/**
 * This class wraps a GL texture. It includes a GLContext reference
 * so we can use to free the texture when destroyed. The implementation
 * makes sure to always free the texture on the main thread, even if the
 * destructor runs on another thread.
 *
 * We ensure that the GLContext reference is only addrefed and released
 * on the main thread, although it uses threadsafe recounting so we don't
 * really have to.
 *
 * Initially the texture is not allocated --- it's in a "null" state.
 */
class GLTexture
{
  typedef mozilla::gl::GLContext GLContext;

public:
  GLTexture() : mTexture(0) {}
  ~GLTexture() { Release(); }

  /**
   * Allocate the texture. This can only be called on the main thread.
   */
  void Allocate(GLContext *aContext);
  /**
   * Move the state of aOther to this GLTexture. If this GLTexture currently
   * has a texture, it is released. This can be called on any thread.
   */
  void TakeFrom(GLTexture *aOther);

  bool IsAllocated() { return mTexture != 0; }
  GLuint GetTextureID() { return mTexture; }
  GLContext *GetGLContext() { return mContext; }

  void Release();
private:

  nsRefPtr<GLContext> mContext;
  GLuint mTexture;
};

/**
 * A RecycleBin is owned by an ImageLayer. We store textures in it that we
 * want to recycle from one image to the next. It's a separate object from 
 * ImageContainer because images need to store a strong ref to their RecycleBin
 * and we must avoid creating a reference loop between an ImageContainer and
 * its active image.
 */
class TextureRecycleBin {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TextureRecycleBin)

  typedef mozilla::gl::GLContext GLContext;

public:
  TextureRecycleBin();

  enum TextureType {
    TEXTURE_Y,
    TEXTURE_C
  };

  void RecycleTexture(GLTexture *aTexture, TextureType aType,
                      const gfxIntSize& aSize);
  void GetTexture(TextureType aType, const gfxIntSize& aSize,
                  GLContext *aContext, GLTexture *aOutTexture);

private:
  typedef mozilla::Mutex Mutex;

  // This protects mRecycledBuffers, mRecycledBufferSize, mRecycledTextures
  // and mRecycledTextureSizes
  Mutex mLock;

  nsTArray<GLTexture> mRecycledTextures[2];
  gfxIntSize mRecycledTextureSizes[2];
};

class ImageLayerOGL : public ImageLayer,
                      public LayerOGL
{
public:
  ImageLayerOGL(LayerManagerOGL *aManager);
  ~ImageLayerOGL() { Destroy(); }

  // LayerOGL Implementation
  virtual void Destroy() { mDestroyed = true; }
  virtual Layer* GetLayer();
  virtual bool LoadAsTexture(GLuint aTextureUnit, gfxIntSize* aSize);

  virtual void RenderLayer(int aPreviousFrameBuffer,
                           const nsIntPoint& aOffset);
  virtual void CleanupResources() {}


  void AllocateTexturesYCbCr(PlanarYCbCrImage *aImage);
  void AllocateTexturesCairo(CairoImage *aImage);

protected:
  nsRefPtr<TextureRecycleBin> mTextureRecycleBin;
};

struct PlanarYCbCrOGLBackendData : public ImageBackendData
{
  ~PlanarYCbCrOGLBackendData()
  {
    if (HasTextures()) {
      mTextureRecycleBin->RecycleTexture(&mTextures[0], TextureRecycleBin::TEXTURE_Y, mYSize);
      mTextureRecycleBin->RecycleTexture(&mTextures[1], TextureRecycleBin::TEXTURE_C, mCbCrSize);
      mTextureRecycleBin->RecycleTexture(&mTextures[2], TextureRecycleBin::TEXTURE_C, mCbCrSize);
    }
  }

  bool HasTextures()
  {
    return mTextures[0].IsAllocated() && mTextures[1].IsAllocated() &&
           mTextures[2].IsAllocated();
  }

  GLTexture mTextures[3];
  gfxIntSize mYSize, mCbCrSize;
  nsRefPtr<TextureRecycleBin> mTextureRecycleBin;
};


struct CairoOGLBackendData : public ImageBackendData
{
  CairoOGLBackendData() : mLayerProgram(gl::RGBALayerProgramType) {}
  GLTexture mTexture;
  gl::ShaderProgramType mLayerProgram;
  gfxIntSize mTextureSize;
};

} /* layers */
} /* mozilla */
#endif /* GFX_IMAGELAYEROGL_H */
