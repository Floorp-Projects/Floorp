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
 *   Frederic Plourde <frederic.plourde@collabora.co.uk>
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#ifndef GFX_LAYERMANAGEROGL_H
#define GFX_LAYERMANAGEROGL_H

#include "Layers.h"

#ifdef XP_WIN
#include <windows.h>
#endif

/**
 * We don't include GLDefs.h here since we don't want to drag in all defines
 * in for all our users.
 */
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

#include "gfxContext.h"
#include "gfx3DMatrix.h"
#include "nsIWidget.h"
#include "GLContext.h"

#include "LayerManagerOGLProgram.h"

namespace mozilla {
namespace layers {

class LayerOGL;

/**
 * This is the LayerManager used for OpenGL 2.1. For now this will render on
 * the main thread.
 */
class THEBES_API LayerManagerOGL : public LayerManager {
  typedef mozilla::gl::GLContext GLContext;

public:
  LayerManagerOGL(nsIWidget *aWidget);
  virtual ~LayerManagerOGL();

  void CleanupResources();

  void Destroy();

  /**
   * Initializes the layer manager, this is when the layer manager will
   * actually access the device and attempt to create the swap chain used
   * to draw to the window. If this method fails the device cannot be used.
   * This function is not threadsafe.
   *
   * \param aExistingContext an existing GL context to use, instead of creating
   * our own for the widget.
   *
   * \return True is initialization was succesful, false when it was not.
   */
  PRBool Initialize(GLContext *aExistingContext = nsnull);

  /**
   * Sets the clipping region for this layer manager. This is important on 
   * windows because using OGL we no longer have GDI's native clipping. Therefor
   * widget must tell us what part of the screen is being invalidated,
   * and we should clip to this.
   *
   * \param aClippingRegion Region to clip to. Setting an empty region
   * will disable clipping.
   */
  void SetClippingRegion(const nsIntRegion& aClippingRegion);

  /**
   * LayerManager implementation.
   */
  void BeginTransaction();

  void BeginTransactionWithTarget(gfxContext* aTarget);

  void EndConstruction();

  virtual void EndTransaction(DrawThebesLayerCallback aCallback,
                              void* aCallbackData);

  virtual void SetRoot(Layer* aLayer) { mRoot = aLayer; }

  virtual already_AddRefed<ThebesLayer> CreateThebesLayer();

  virtual already_AddRefed<ContainerLayer> CreateContainerLayer();

  virtual already_AddRefed<ImageLayer> CreateImageLayer();

  virtual already_AddRefed<ColorLayer> CreateColorLayer();

  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer();

  virtual already_AddRefed<ImageContainer> CreateImageContainer();

  virtual LayersBackend GetBackendType() { return LAYERS_OPENGL; }
  virtual void GetBackendName(nsAString& name) { name.AssignLiteral("OpenGL"); }

  /**
   * Image Container management.
   */

  /* Forget this image container.  Should be called by ImageContainerOGL
   * on its current layer manager before switching to a new one.
   */
  void ForgetImageContainer(ImageContainer* aContainer);
  void RememberImageContainer(ImageContainer* aContainer);

  /**
   * Helper methods.
   */
  void MakeCurrent(PRBool aForce = PR_FALSE) {
    if (mDestroyed) {
      NS_WARNING("Call on destroyed layer manager");
      return;
    }
    mGLContext->MakeCurrent(aForce);
  }

  ColorTextureLayerProgram *GetRGBALayerProgram() {
    return static_cast<ColorTextureLayerProgram*>(mPrograms[RGBALayerProgramType]);
  }
  ColorTextureLayerProgram *GetBGRALayerProgram() {
    return static_cast<ColorTextureLayerProgram*>(mPrograms[BGRALayerProgramType]);
  }
  ColorTextureLayerProgram *GetRGBXLayerProgram() {
    return static_cast<ColorTextureLayerProgram*>(mPrograms[RGBXLayerProgramType]);
  }
  ColorTextureLayerProgram *GetBGRXLayerProgram() {
    return static_cast<ColorTextureLayerProgram*>(mPrograms[BGRXLayerProgramType]);
  }
  ColorTextureLayerProgram *GetBasicLayerProgram(PRBool aOpaque, PRBool aIsRGB)
  {
    if (aIsRGB) {
      return aOpaque
        ? GetRGBXLayerProgram()
        : GetRGBALayerProgram();
    } else {
      return aOpaque
        ? GetBGRXLayerProgram()
        : GetBGRALayerProgram();
    }
  }

  ColorTextureLayerProgram *GetRGBARectLayerProgram() {
    return static_cast<ColorTextureLayerProgram*>(mPrograms[RGBARectLayerProgramType]);
  }
  SolidColorLayerProgram *GetColorLayerProgram() {
    return static_cast<SolidColorLayerProgram*>(mPrograms[ColorLayerProgramType]);
  }
  YCbCrTextureLayerProgram *GetYCbCrLayerProgram() {
    return static_cast<YCbCrTextureLayerProgram*>(mPrograms[YCbCrLayerProgramType]);
  }
  CopyProgram *GetCopy2DProgram() {
    return static_cast<CopyProgram*>(mPrograms[Copy2DProgramType]);
  }
  CopyProgram *GetCopy2DRectProgram() {
    return static_cast<CopyProgram*>(mPrograms[Copy2DRectProgramType]);
  }

  ColorTextureLayerProgram *GetFBOLayerProgram() {
    if (mFBOTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE_ARB)
      return static_cast<ColorTextureLayerProgram*>(mPrograms[RGBARectLayerProgramType]);
    return static_cast<ColorTextureLayerProgram*>(mPrograms[RGBALayerProgramType]);
  }

  GLContext *gl() const { return mGLContext; }

  DrawThebesLayerCallback GetThebesLayerCallback() const
  { return mThebesLayerCallback; }

  void* GetThebesLayerCallbackData() const
  { return mThebesLayerCallbackData; }

  // This is a GLContext that can be used for resource
  // management (creation, destruction).  It is guaranteed
  // to be either the same as the gl() context, or a context
  // that is in the same share pool.
  GLContext *glForResources() const {
    if (mGLContext->GetSharedContext())
      return mGLContext->GetSharedContext();
    return mGLContext;
  }

  /*
   * Helper functions for our layers
   */
  void CallThebesLayerDrawCallback(ThebesLayer* aLayer,
                                   gfxContext* aContext,
                                   const nsIntRegion& aRegionToDraw)
  {
    NS_ASSERTION(mThebesLayerCallback,
                 "CallThebesLayerDrawCallback without callback!");
    mThebesLayerCallback(aLayer, aContext,
                         aRegionToDraw, nsIntRegion(),
                         mThebesLayerCallbackData);
  }

  GLenum FBOTextureTarget() { return mFBOTextureTarget; }

  /* Create a FBO backed by a texture; will leave the FBO
   * bound.  Note that the texture target type will be
   * of the type returned by FBOTextureTarget; different
   * shaders are required to sample from the different
   * texture types.
   */
  void CreateFBOWithTexture(int aWidth, int aHeight,
                            GLuint *aFBO, GLuint *aTexture);

  GLuint QuadVBO() { return mQuadVBO; }
  GLintptr QuadVBOVertexOffset() { return 0; }
  GLintptr QuadVBOTexCoordOffset() { return sizeof(float)*4*2; }
  GLintptr QuadVBOFlippedTexCoordOffset() { return sizeof(float)*8*2; }

  void BindQuadVBO() {
    mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mQuadVBO);
  }

  void QuadVBOVerticesAttrib(GLuint aAttribIndex) {
    mGLContext->fVertexAttribPointer(aAttribIndex, 2,
                                     LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                                     (GLvoid*) QuadVBOVertexOffset());
  }

  void QuadVBOTexCoordsAttrib(GLuint aAttribIndex) {
    mGLContext->fVertexAttribPointer(aAttribIndex, 2,
                                     LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                                     (GLvoid*) QuadVBOTexCoordOffset());
  }

  void QuadVBOFlippedTexCoordsAttrib(GLuint aAttribIndex) {
    mGLContext->fVertexAttribPointer(aAttribIndex, 2,
                                     LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                                     (GLvoid*) QuadVBOFlippedTexCoordOffset());
  }

  // Super common

  void BindAndDrawQuad(GLuint aVertAttribIndex,
                       GLuint aTexCoordAttribIndex,
                       bool aFlipped = false)
  {
    BindQuadVBO();
    QuadVBOVerticesAttrib(aVertAttribIndex);

    if (aTexCoordAttribIndex != GLuint(-1)) {
      if (aFlipped)
        QuadVBOFlippedTexCoordsAttrib(aTexCoordAttribIndex);
      else
        QuadVBOTexCoordsAttrib(aTexCoordAttribIndex);

      mGLContext->fEnableVertexAttribArray(aTexCoordAttribIndex);
    }

    mGLContext->fEnableVertexAttribArray(aVertAttribIndex);

    mGLContext->fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);

    mGLContext->fDisableVertexAttribArray(aVertAttribIndex);

    if (aTexCoordAttribIndex != GLuint(-1)) {
      mGLContext->fDisableVertexAttribArray(aTexCoordAttribIndex);
    }
  }

  void BindAndDrawQuad(LayerProgram *aProg,
                       bool aFlipped = false)
  {
    BindAndDrawQuad(aProg->AttribLocation(LayerProgram::VertexAttrib),
                    aProg->AttribLocation(LayerProgram::TexCoordAttrib),
                    aFlipped);
  }

#ifdef MOZ_LAYERS_HAVE_LOG
   virtual const char* Name() const { return "OGL"; }
#endif // MOZ_LAYERS_HAVE_LOG

private:
  /** Widget associated with this layer manager */
  nsIWidget *mWidget;
  nsIntSize mWidgetSize;

  /** 
   * Context target, NULL when drawing directly to our swap chain.
   */
  nsRefPtr<gfxContext> mTarget;

  nsRefPtr<GLContext> mGLContext;

  // The image containers that this layer manager has created.
  // The destructor will tell the layer manager to remove
  // it from the list.
  nsTArray<ImageContainer*> mImageContainers;

  enum ProgramType {
    RGBALayerProgramType,
    BGRALayerProgramType,
    RGBXLayerProgramType,
    BGRXLayerProgramType,
    RGBARectLayerProgramType,
    ColorLayerProgramType,
    YCbCrLayerProgramType,
    Copy2DProgramType,
    Copy2DRectProgramType,
    NumProgramTypes
  };

  static ProgramType sLayerProgramTypes[];

  /** Backbuffer */
  GLuint mBackBufferFBO;
  GLuint mBackBufferTexture;
  nsIntSize mBackBufferSize;

  /** Shader Programs */
  nsTArray<LayerManagerOGLProgram*> mPrograms;

  /** Texture target to use for FBOs */
  GLenum mFBOTextureTarget;

  /** VBO that has some basics in it for a textured quad,
   *  including vertex coords and texcoords for both
   *  flipped and unflipped textures */
  GLuint mQuadVBO;

  /** Region we're clipping our current drawing to. */
  nsIntRegion mClippingRegion;

  /** Misc */
  PRPackedBool mHasBGRA;

  /** Current root layer. */
  LayerOGL *RootLayer() const;

  /**
   * Render the current layer tree to the active target.
   */
  void Render();
  /**
   * Setup the viewport and projection matrix for rendering
   * to a window of the given dimensions.
   */
  void SetupPipeline(int aWidth, int aHeight);
  /**
   * Setup a backbuffer of the given dimensions.
   */
  void SetupBackBuffer(int aWidth, int aHeight);
  /**
   * Copies the content of our backbuffer to the set transaction target.
   */
  void CopyToTarget();

  /**
   * Updates all layer programs with a new projection matrix.
   *
   * XXX we need a way to be able to delay setting this until
   * the program is actually used.  Maybe a DelayedSetUniform
   * on Program, that will delay the set until the next Activate?
   *
   * XXX this is only called once per frame, so it's not awful.
   * If we have any more similar updates, then we should delay.
   */
  void SetLayerProgramProjectionMatrix(const gfx3DMatrix& aMatrix);

  /* Thebes layer callbacks; valid at the end of a transaciton,
   * while rendering */
  DrawThebesLayerCallback mThebesLayerCallback;
  void *mThebesLayerCallbackData;
};

/**
 * General information and tree management for OGL layers.
 */
class LayerOGL
{
public:
  LayerOGL(LayerManagerOGL *aManager)
    : mOGLManager(aManager), mDestroyed(PR_FALSE)
  { }

  virtual ~LayerOGL() { }

  virtual LayerOGL *GetFirstChildOGL() {
    return nsnull;
  }

  /* Do NOT call this from the generic LayerOGL destructor.  Only from the
   * concrete class destructor
   */
  virtual void Destroy() = 0;

  virtual Layer* GetLayer() = 0;

  virtual void RenderLayer(int aPreviousFrameBuffer,
                           const nsIntPoint& aOffset) = 0;

  typedef mozilla::gl::GLContext GLContext;

  GLContext *gl() const { return mOGLManager->gl(); }
protected:
  LayerManagerOGL *mOGLManager;
  PRPackedBool mDestroyed;
};

} /* layers */
} /* mozilla */

#endif /* GFX_LAYERMANAGEROGL_H */
