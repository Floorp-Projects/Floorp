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

  PRBool IsAllocated() { return mTexture != 0; }
  GLuint GetTextureID() { return mTexture; }
  GLContext *GetGLContext() { return mContext; }

private:
  void Release();

  nsRefPtr<GLContext> mContext;
  GLuint mTexture;
};

/**
 * A RecycleBin is owned by an ImageContainerOGL. We store buffers
 * and textures in it that we want to recycle from one image to the next.
 * It's a separate object from ImageContainerOGL because images need to store
 * a strong ref to their RecycleBin and we must avoid creating a
 * reference loop between an ImageContainerOGL and its active image.
 */
class RecycleBin {
  THEBES_INLINE_DECL_THREADSAFE_REFCOUNTING(RecycleBin)

  typedef mozilla::gl::GLContext GLContext;

public:
  RecycleBin();

  void RecycleBuffer(PRUint8* aBuffer, PRUint32 aSize);
  // Returns a recycled buffer of the right size, or allocates a new buffer.
  PRUint8* GetBuffer(PRUint32 aSize);

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

  // We should probably do something to prune this list on a timer so we don't
  // eat excess memory while video is paused...
  nsTArray<nsAutoArrayPtr<PRUint8> > mRecycledBuffers;
  // This is only valid if mRecycledBuffers is non-empty
  PRUint32 mRecycledBufferSize;

  nsTArray<GLTexture> mRecycledTextures[2];
  gfxIntSize mRecycledTextureSizes[2];
};

class THEBES_API ImageContainerOGL : public ImageContainer
{
public:
  ImageContainerOGL(LayerManagerOGL *aManager);
  virtual ~ImageContainerOGL();

  virtual already_AddRefed<Image> CreateImage(const Image::Format* aFormats,
                                              PRUint32 aNumFormats);

  virtual void SetCurrentImage(Image* aImage);

  virtual already_AddRefed<Image> GetCurrentImage();

  virtual already_AddRefed<gfxASurface> GetCurrentAsSurface(gfxIntSize* aSize);

  virtual gfxIntSize GetCurrentSize();

  virtual PRBool SetLayerManager(LayerManager *aManager);

  virtual LayerManager::LayersBackend GetBackendType() { return LayerManager::LAYERS_OPENGL; }

private:

  nsRefPtr<RecycleBin> mRecycleBin;
  nsRefPtr<Image> mActiveImage;
};

class THEBES_API ImageLayerOGL : public ImageLayer,
                                 public LayerOGL
{
public:
  ImageLayerOGL(LayerManagerOGL *aManager)
    : ImageLayer(aManager, NULL)
    , LayerOGL(aManager)
  { 
    mImplData = static_cast<LayerOGL*>(this);
  }
  ~ImageLayerOGL() { Destroy(); }

  // LayerOGL Implementation
  virtual void Destroy() { mDestroyed = PR_TRUE; }
  virtual Layer* GetLayer();

  virtual void RenderLayer(int aPreviousFrameBuffer,
                           const nsIntPoint& aOffset);
};

class THEBES_API PlanarYCbCrImageOGL : public PlanarYCbCrImage
{
  typedef mozilla::gl::GLContext GLContext;

public:
  PlanarYCbCrImageOGL(LayerManagerOGL *aManager,
                      RecycleBin *aRecycleBin);
  ~PlanarYCbCrImageOGL();

  virtual void SetData(const Data &aData);

  /**
   * Upload the data from out mData into our textures. For now we use this to
   * make sure the textures are created and filled on the main thread.
   */
  void AllocateTextures(GLContext *gl);
  void UpdateTextures(GLContext *gl);

  PRBool HasData() { return mHasData; }
  PRBool HasTextures()
  {
    return mTextures[0].IsAllocated() && mTextures[1].IsAllocated() &&
           mTextures[2].IsAllocated();
  }

  nsAutoArrayPtr<PRUint8> mBuffer;
  PRUint32 mBufferSize;
  nsRefPtr<RecycleBin> mRecycleBin;
  GLTexture mTextures[3];
  Data mData;
  gfxIntSize mSize;
  PRPackedBool mHasData;
  gfx::YUVType mType; 
};


class THEBES_API CairoImageOGL : public CairoImage
{
  typedef mozilla::gl::GLContext GLContext;

public:
  CairoImageOGL(LayerManagerOGL *aManager);

  void SetData(const Data &aData);

  GLTexture mTexture;
  gfxIntSize mSize;
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
  virtual PRBool Init(gfxSharedImageSurface* aFront, const nsIntSize& aSize);

  virtual already_AddRefed<gfxSharedImageSurface>
  Swap(gfxSharedImageSurface* aNewFront);

  virtual void DestroyFrontBuffer();

  virtual void Disconnect();

  // LayerOGL impl
  virtual void Destroy();

  virtual Layer* GetLayer();

  virtual void RenderLayer(int aPreviousFrameBuffer,
                           const nsIntPoint& aOffset);

private:
  nsRefPtr<TextureImage> mTexImage;


  // XXX FIXME holding to free
  nsRefPtr<gfxSharedImageSurface> mDeadweight;


};

} /* layers */
} /* mozilla */
#endif /* GFX_IMAGELAYEROGL_H */
