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

#include "mozilla/layers/ShadowLayers.h"

#include "mozilla/TimeStamp.h"

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
class ShadowThebesLayer;
class ShadowContainerLayer;
class ShadowImageLayer;
class ShadowCanvasLayer;
class ShadowColorLayer;

/**
 * This is the LayerManager used for OpenGL 2.1. For now this will render on
 * the main thread.
 */
class THEBES_API LayerManagerOGL :
    public ShadowLayerManager
{
  typedef mozilla::gl::GLContext GLContext;
  typedef mozilla::gl::ShaderProgramType ProgramType;

public:
  LayerManagerOGL(nsIWidget *aWidget);
  virtual ~LayerManagerOGL();

  void CleanupResources();

  void Destroy();


  /**
   * Initializes the layer manager with a given GLContext. If aContext is null
   * then the layer manager will try to create one for the associated widget.
   *
   * \param aContext an existing GL context to use. Can be created with CreateContext()
   *
   * \return True is initialization was succesful, false when it was not.
   */
  bool Initialize(bool force = false) {
    return Initialize(CreateContext(), force);
  }

  bool Initialize(nsRefPtr<GLContext> aContext, bool force = false);

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
  virtual ShadowLayerManager* AsShadowManager()
  {
    return this;
  }

  void BeginTransaction();

  void BeginTransactionWithTarget(gfxContext* aTarget);

  void EndConstruction();

  virtual bool EndEmptyTransaction();
  virtual void EndTransaction(DrawThebesLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT);

  virtual void SetRoot(Layer* aLayer) { mRoot = aLayer; }

  virtual bool CanUseCanvasLayerForSize(const gfxIntSize &aSize)
  {
      if (!mGLContext)
          return false;
      PRInt32 maxSize = mGLContext->GetMaxTextureSize();
      return aSize <= gfxIntSize(maxSize, maxSize);
  }

  virtual already_AddRefed<ThebesLayer> CreateThebesLayer();

  virtual already_AddRefed<ContainerLayer> CreateContainerLayer();

  virtual already_AddRefed<ImageLayer> CreateImageLayer();

  virtual already_AddRefed<ColorLayer> CreateColorLayer();

  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer();

  virtual already_AddRefed<ShadowThebesLayer> CreateShadowThebesLayer();
  virtual already_AddRefed<ShadowContainerLayer> CreateShadowContainerLayer();
  virtual already_AddRefed<ShadowImageLayer> CreateShadowImageLayer();
  virtual already_AddRefed<ShadowColorLayer> CreateShadowColorLayer();
  virtual already_AddRefed<ShadowCanvasLayer> CreateShadowCanvasLayer();

  virtual LayersBackend GetBackendType() { return LAYERS_OPENGL; }
  virtual void GetBackendName(nsAString& name) { name.AssignLiteral("OpenGL"); }

  /**
   * Helper methods.
   */
  void MakeCurrent(bool aForce = false) {
    if (mDestroyed) {
      NS_WARNING("Call on destroyed layer manager");
      return;
    }
    mGLContext->MakeCurrent(aForce);
  }

  ColorTextureLayerProgram *GetColorTextureLayerProgram(ProgramType type){
    return static_cast<ColorTextureLayerProgram*>(mPrograms[type]);
  }

  ColorTextureLayerProgram *GetRGBALayerProgram() {
    return static_cast<ColorTextureLayerProgram*>(mPrograms[gl::RGBALayerProgramType]);
  }
  ColorTextureLayerProgram *GetBGRALayerProgram() {
    return static_cast<ColorTextureLayerProgram*>(mPrograms[gl::BGRALayerProgramType]);
  }
  ColorTextureLayerProgram *GetRGBXLayerProgram() {
    return static_cast<ColorTextureLayerProgram*>(mPrograms[gl::RGBXLayerProgramType]);
  }
  ColorTextureLayerProgram *GetBGRXLayerProgram() {
    return static_cast<ColorTextureLayerProgram*>(mPrograms[gl::BGRXLayerProgramType]);
  }
  ColorTextureLayerProgram *GetBasicLayerProgram(bool aOpaque, bool aIsRGB)
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
    return static_cast<ColorTextureLayerProgram*>(mPrograms[gl::RGBARectLayerProgramType]);
  }
  SolidColorLayerProgram *GetColorLayerProgram() {
    return static_cast<SolidColorLayerProgram*>(mPrograms[gl::ColorLayerProgramType]);
  }
  YCbCrTextureLayerProgram *GetYCbCrLayerProgram() {
    return static_cast<YCbCrTextureLayerProgram*>(mPrograms[gl::YCbCrLayerProgramType]);
  }
  ComponentAlphaTextureLayerProgram *GetComponentAlphaPass1LayerProgram() {
    return static_cast<ComponentAlphaTextureLayerProgram*>
             (mPrograms[gl::ComponentAlphaPass1ProgramType]);
  }
  ComponentAlphaTextureLayerProgram *GetComponentAlphaPass2LayerProgram() {
    return static_cast<ComponentAlphaTextureLayerProgram*>
             (mPrograms[gl::ComponentAlphaPass2ProgramType]);
  }
  CopyProgram *GetCopy2DProgram() {
    return static_cast<CopyProgram*>(mPrograms[gl::Copy2DProgramType]);
  }
  CopyProgram *GetCopy2DRectProgram() {
    return static_cast<CopyProgram*>(mPrograms[gl::Copy2DRectProgramType]);
  }

  ColorTextureLayerProgram *GetFBOLayerProgram() {
    if (mFBOTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE_ARB)
      return static_cast<ColorTextureLayerProgram*>(mPrograms[gl::RGBARectLayerProgramType]);
    return static_cast<ColorTextureLayerProgram*>(mPrograms[gl::RGBALayerProgramType]);
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

  /**
   * Controls how to initialize the texture / FBO created by
   * CreateFBOWithTexture.
   *  - InitModeNone: No initialization, contents are undefined.
   *  - InitModeClear: Clears the FBO.
   *  - InitModeCopy: Copies the contents of the current glReadBuffer into the
   *    texture.
   */
  enum InitMode {
    InitModeNone,
    InitModeClear,
    InitModeCopy
  };

  /* Create a FBO backed by a texture; will leave the FBO
   * bound.  Note that the texture target type will be
   * of the type returned by FBOTextureTarget; different
   * shaders are required to sample from the different
   * texture types.
   */
  void CreateFBOWithTexture(const nsIntRect& aRect, InitMode aInit,
                            GLuint aCurrentFrameBuffer,
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

  void BindAndDrawQuadWithTextureRect(LayerProgram *aProg,
                                      const nsIntRect& aTexCoordRect,
                                      const nsIntSize& aTexSize,
                                      GLenum aWrapMode = LOCAL_GL_REPEAT,
                                      bool aFlipped = false);


#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() const { return "OGL"; }
#endif // MOZ_LAYERS_HAVE_LOG

  const nsIntSize& GetWigetSize() {
    return mWidgetSize;
  }

  enum WorldTransforPolicy {
    ApplyWorldTransform,
    DontApplyWorldTransform
  };

  /**
   * Setup the viewport and projection matrix for rendering
   * to a window of the given dimensions.
   */
  void SetupPipeline(int aWidth, int aHeight, WorldTransforPolicy aTransformPolicy);

  /**
   * Setup World transform matrix.
   * Transform will be ignored if it is not PreservesAxisAlignedRectangles
   * or has non integer scale
   */
  void SetWorldTransform(const gfxMatrix& aMatrix);
  gfxMatrix& GetWorldTransform(void);
  void WorldTransformRect(nsIntRect& aRect);

private:
  /** Widget associated with this layer manager */
  nsIWidget *mWidget;
  nsIntSize mWidgetSize;

  /** 
   * Context target, NULL when drawing directly to our swap chain.
   */
  nsRefPtr<gfxContext> mTarget;

  nsRefPtr<GLContext> mGLContext;

  already_AddRefed<mozilla::gl::GLContext> CreateContext();

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
  bool mHasBGRA;

  /** Current root layer. */
  LayerOGL *RootLayer() const;

  /**
   * Render the current layer tree to the active target.
   */
  void Render();

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
  gfxMatrix mWorldMatrix;

  struct FPSState
  {
      GLuint texture;
      int fps;
      bool initialized;
      int fcount;
      TimeStamp last;

      FPSState()
        : texture(0)
        , fps(0)
        , initialized(false)
        , fcount(0)
      {
        last = TimeStamp::Now();
      }
      void DrawFPS(GLContext*, CopyProgram*);
  } mFPS;

  static bool sDrawFPS;
};

/**
 * General information and tree management for OGL layers.
 */
class LayerOGL
{
public:
  LayerOGL(LayerManagerOGL *aManager)
    : mOGLManager(aManager), mDestroyed(false)
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

  LayerManagerOGL* OGLManager() const { return mOGLManager; }
  GLContext *gl() const { return mOGLManager->gl(); }
  virtual void CleanupResources() = 0;

protected:
  LayerManagerOGL *mOGLManager;
  bool mDestroyed;
};

} /* layers */
} /* mozilla */

#endif /* GFX_LAYERMANAGEROGL_H */
