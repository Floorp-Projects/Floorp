/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef GFX_LAYERMANAGEROGL_H
#define GFX_LAYERMANAGEROGL_H

#include "Layers.h"

#ifdef XP_WIN
#include <windows.h>
#endif

typedef unsigned int GLuint;
typedef int GLint;
typedef float GLfloat;
typedef char GLchar;

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

#include "gfxContext.h"
#include "nsIWidget.h"

namespace mozilla {
namespace layers {

class LayerOGL;

struct GLvec2
{
  GLfloat mX;
  GLfloat mY;
};

/**
 * Helper class for Layer Programs.
 */
class LayerProgram
{
public:
  LayerProgram();
  virtual ~LayerProgram();

  PRBool Initialize(GLuint aVertexShader, GLuint aFragmentShader);

  virtual void UpdateLocations();

  void Activate();

  void SetMatrixUniform(GLint aLocation, const GLfloat *aValue);
  void SetInt(GLint aLocation, GLint aValue);

  void SetMatrixProj(GLfloat *aValue)
  {
    SetMatrixUniform(mMatrixProjLocation, aValue);
  }

  void SetLayerQuadTransform(GLfloat *aValue)
  {
    SetMatrixUniform(mLayerQuadTransformLocation, aValue);
  }

  void SetLayerTransform(const GLfloat *aValue)
  {
    SetMatrixUniform(mLayerTransformLocation, aValue);
  }

  void SetLayerOpacity(GLfloat aValue);

  void PushRenderTargetOffset(GLfloat aValueX, GLfloat aValueY);
  void PopRenderTargetOffset();

  void Apply();

protected:
  GLuint mProgram;
  GLint mMatrixProjLocation;
  GLint mLayerQuadTransformLocation;
  GLint mLayerTransformLocation;
  GLint mRenderTargetOffsetLocation;
  GLint mLayerOpacityLocation;

  nsTArray<GLvec2> mRenderTargetOffsetStack;
};

class RGBLayerProgram : public LayerProgram
{
public:
  void UpdateLocations();

  void SetLayerTexture(GLint aValue)
  {
    SetInt(mLayerTextureLocation, aValue);
  }
protected:
  GLint mLayerTextureLocation;
};

class YCbCrLayerProgram : public LayerProgram
{
public:
  void UpdateLocations();

  void SetYTexture(GLint aValue)
  {
    SetInt(mYTextureLocation, aValue);
  }
  void SetCbTexture(GLint aValue)
  {
    SetInt(mCbTextureLocation, aValue);
  }
  void SetCrTexture(GLint aValue)
  {
    SetInt(mCrTextureLocation, aValue);
  }
protected:
  GLint mYTextureLocation;
  GLint mCbTextureLocation;
  GLint mCrTextureLocation;
};

/**
 * This is the LayerManager used for OpenGL 2.1. For now this will render on
 * the main thread.
 */
class THEBES_API LayerManagerOGL : public LayerManager {
public:
  LayerManagerOGL(nsIWidget *aWidget);
  virtual ~LayerManagerOGL();

  /**
   * Initializes the layer manager, this is when the layer manager will
   * actually access the device and attempt to create the swap chain used
   * to draw to the window. If this method fails the device cannot be used.
   * This function is not threadsafe.
   *
   * \return True is initialization was succesful, false when it was not.
   */
  PRBool Initialize();

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

  void EndTransaction();

  void SetRoot(Layer* aLayer);
  
  virtual already_AddRefed<ThebesLayer> CreateThebesLayer();

  virtual already_AddRefed<ContainerLayer> CreateContainerLayer();

  virtual already_AddRefed<ImageLayer> CreateImageLayer();

  virtual already_AddRefed<ImageContainer> CreateImageContainer();

  virtual LayersBackend GetBackendType() { return LAYERS_OPENGL; }

  /**
   * Helper methods.
   */
  void SetClippingEnabled(PRBool aEnabled);

  void MakeCurrent();

  RGBLayerProgram *GetRGBLayerProgram() { return mRGBLayerProgram; }
  YCbCrLayerProgram *GetYCbCrLayerProgram() { return mYCbCrLayerProgram; }

private:
  /** Widget associated with this layer manager */
  nsIWidget *mWidget;
  /** 
   * Context target, NULL when drawing directly to our swap chain.
   */
  nsRefPtr<gfxContext> mTarget;

#ifdef XP_WIN
  /** Windows Device Context */
  HDC mDC;

  /** OpenGL Context */
  HGLRC mContext;
#endif

  /** Backbuffer */
  GLuint mBackBuffer;
  /** Backbuffer size */
  nsIntSize mBackBufferSize;
  /** Framebuffer */
  GLuint mFrameBuffer;
  /** RGB Layer Program */
  RGBLayerProgram *mRGBLayerProgram;
  /** YUV Layer Program */
  YCbCrLayerProgram *mYCbCrLayerProgram;
  /** Vertex Shader */
  GLuint mVertexShader;
  /** RGB fragment shader */
  GLuint mRGBShader;
  /** YUV fragment shader */
  GLuint mYUVShader;
  /** Current root layer. */
  LayerOGL *mRootLayer;
  /** Vertex buffer */
  GLuint mVBO;

  /**
   * Region we're clipping our current drawing to.
   */
  nsIntRegion mClippingRegion;
  /**
   * Render the current layer tree to the active target.
   */
  void Render();
  /**
   * Setup the pipeline.
   */
  void SetupPipeline();
  /**
   * Setup the backbuffer.
   *
   * \return PR_TRUE if setup was succesful
   */
  PRBool SetupBackBuffer();
  /**
   * Copies the content of our backbuffer to the set transaction target.
   */
  void CopyToTarget();
};

/**
 * General information and tree management for OGL layers.
 */
class LayerOGL
{
public:
  LayerOGL();

  enum LayerType { TYPE_THEBES, TYPE_CONTAINER, TYPE_IMAGE };
  
  virtual LayerType GetType() = 0;

  LayerOGL *GetNextSibling();
  virtual LayerOGL *GetFirstChildOGL() { return nsnull; }

  void SetNextSibling(LayerOGL *aParent);
  void SetFirstChild(LayerOGL *aParent);

  virtual Layer* GetLayer() = 0;

  virtual void RenderLayer(int aPreviousFrameBuffer) = 0;

protected:
  LayerOGL *mNextSibling;
};

} /* layers */
} /* mozilla */

#endif /* GFX_LAYERMANAGEROGL_H */
