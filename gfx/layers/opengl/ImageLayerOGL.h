/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef GFX_IMAGELAYEROGL_H
#define GFX_IMAGELAYEROGL_H

#include "mozilla/layers/PLayers.h"
#include "mozilla/layers/ShadowLayers.h"

#include "LayerManagerOGL.h"
#include "ImageLayers.h"
#include "yuv_convert.h"
#include "mozilla/Mutex.h"

namespace mozilla {
namespace layers {

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
class GLTexture {
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

private:
  void Release();

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

class THEBES_API ImageLayerOGL : public ImageLayer,
                                 public LayerOGL
{
public:
  ImageLayerOGL(LayerManagerOGL *aManager);
  ~ImageLayerOGL() { Destroy(); }

  // LayerOGL Implementation
  virtual void Destroy() { mDestroyed = true; }
  virtual Layer* GetLayer();

  virtual void RenderLayer(int aPreviousFrameBuffer,
                           const nsIntPoint& aOffset);
  virtual void CleanupResources() {}

  void AllocateTexturesYCbCr(PlanarYCbCrImage *aImage);
  void AllocateTexturesCairo(CairoImage *aImage);

protected:
  nsRefPtr<TextureRecycleBin> mTextureRecycleBin;
};

struct THEBES_API PlanarYCbCrOGLBackendData : public ImageBackendData
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
};

class ShadowImageLayerOGL : public ShadowImageLayer,
                            public LayerOGL
{
  typedef gl::TextureImage TextureImage;

public:
  ShadowImageLayerOGL(LayerManagerOGL* aManager);
  virtual ~ShadowImageLayerOGL();

  // ShadowImageLayer impl
  virtual void Swap(const SharedImage& aFront,
                    SharedImage* aNewBack);

  virtual void Disconnect();

  // LayerOGL impl
  virtual void Destroy();

  virtual Layer* GetLayer();

  virtual void RenderLayer(int aPreviousFrameBuffer,
                           const nsIntPoint& aOffset);

  virtual void CleanupResources();

private:
  bool Init(const SharedImage& aFront);

  nsRefPtr<TextureImage> mTexImage;
  GLTexture mYUVTexture[3];
  gfxIntSize mSize;
  gfxIntSize mCbCrSize;
  nsIntRect mPictureRect;
};

} /* layers */
} /* mozilla */
#endif /* GFX_IMAGELAYEROGL_H */
