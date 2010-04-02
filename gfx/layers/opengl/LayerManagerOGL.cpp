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

#include "LayerManagerOGL.h"
#include "ThebesLayerOGL.h"
#include "ContainerLayerOGL.h"
#include "ImageLayerOGL.h"
#include "LayerManagerOGLShaders.h"

#include "gfxContext.h"
#include "nsIWidget.h"

#include "glWrapper.h"

static const GLint VERTEX_ATTRIB_LOCATION = 0;

namespace mozilla {
namespace layers {

/**
 * LayerManagerOGL
 */
LayerManagerOGL::LayerManagerOGL(nsIWidget *aWidget) 
  : mWidget(aWidget)
  , mBackBuffer(0)
  , mFrameBuffer(0)
  , mRGBLayerProgram(NULL)
  , mYCbCrLayerProgram(NULL)
  , mVertexShader(0)
  , mRGBShader(0)
  , mYUVShader(0)
{
}

LayerManagerOGL::~LayerManagerOGL()
{
  MakeCurrent();
  delete mRGBLayerProgram;
  delete mYCbCrLayerProgram;
#ifdef XP_WIN
  BOOL deleted = sglWrapper.wDeleteContext(mContext);
  NS_ASSERTION(deleted, "Error deleting OpenGL context!");
  ::ReleaseDC((HWND)mWidget->GetNativeData(NS_NATIVE_WINDOW), mDC);
#endif
}

PRBool
LayerManagerOGL::Initialize()
{
#ifdef XP_WIN
  mDC = (HDC)mWidget->GetNativeData(NS_NATIVE_GRAPHIC);

  mContext = sglWrapper.wCreateContext(mDC);

  if (!mContext) {
    return PR_FALSE;
  }
#else
  // Don't know how to initialize on this platform!
  return PR_FALSE;
#endif

  MakeCurrent();

  sglWrapper.BlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA, LOCAL_GL_ONE, LOCAL_GL_ONE);
  sglWrapper.Enable(LOCAL_GL_BLEND);
  sglWrapper.Enable(LOCAL_GL_TEXTURE_2D);
  sglWrapper.Enable(LOCAL_GL_SCISSOR_TEST);

  mVertexShader = sglWrapper.CreateShader(LOCAL_GL_VERTEX_SHADER);
  mRGBShader = sglWrapper.CreateShader(LOCAL_GL_FRAGMENT_SHADER);
  mYUVShader = sglWrapper.CreateShader(LOCAL_GL_FRAGMENT_SHADER);

  sglWrapper.ShaderSource(mVertexShader, 1, (const GLchar**)&sVertexShader, NULL);
  sglWrapper.ShaderSource(mRGBShader, 1, (const GLchar**)&sRGBLayerPS, NULL);
  sglWrapper.ShaderSource(mYUVShader, 1, (const GLchar**)&sYUVLayerPS, NULL);

  sglWrapper.CompileShader(mVertexShader);
  sglWrapper.CompileShader(mRGBShader);
  sglWrapper.CompileShader(mYUVShader);

  GLint status;
  sglWrapper.GetShaderiv(mVertexShader, LOCAL_GL_COMPILE_STATUS, &status);
  if (!status) {
    return false;
  }

  sglWrapper.GetShaderiv(mRGBShader, LOCAL_GL_COMPILE_STATUS, &status);
  if (!status) {
    return false;
  }

  sglWrapper.GetShaderiv(mYUVShader, LOCAL_GL_COMPILE_STATUS, &status);

  if (!status) {
    return false;
  }

  mRGBLayerProgram = new RGBLayerProgram();
  if (!mRGBLayerProgram->Initialize(mVertexShader, mRGBShader)) {
    return false;
  }
  mYCbCrLayerProgram = new YCbCrLayerProgram();
  if (!mYCbCrLayerProgram->Initialize(mVertexShader, mYUVShader)) {
    return false;
  }

  mRGBLayerProgram->UpdateLocations();
  mYCbCrLayerProgram->UpdateLocations();

  sglWrapper.GenBuffers(1, &mVBO);
  sglWrapper.BindBuffer(LOCAL_GL_ARRAY_BUFFER, mVBO);
  sglWrapper.EnableClientState(LOCAL_GL_VERTEX_ARRAY);
  sglWrapper.EnableVertexAttribArray(VERTEX_ATTRIB_LOCATION);

  GLfloat vertices[] = { 0, 0, 1.0f, 0, 0, 1.0f, 1.0f, 1.0f };
  sglWrapper.BufferData(LOCAL_GL_ARRAY_BUFFER, sizeof(vertices), vertices, LOCAL_GL_STATIC_DRAW);

  mRGBLayerProgram->Activate();
  sglWrapper.VertexAttribPointer(VERTEX_ATTRIB_LOCATION,
                        2,
                        LOCAL_GL_FLOAT,
                        LOCAL_GL_FALSE,
                        0,
                        0);
  mYCbCrLayerProgram->Activate();
  sglWrapper.VertexAttribPointer(VERTEX_ATTRIB_LOCATION,
                        2,
                        LOCAL_GL_FLOAT,
                        LOCAL_GL_FALSE,
                        0,
                        0);

  mRGBLayerProgram->SetLayerTexture(0);

  mYCbCrLayerProgram->SetYTexture(0);
  mYCbCrLayerProgram->SetCbTexture(1);
  mYCbCrLayerProgram->SetCrTexture(2);

  return true;
}

void
LayerManagerOGL::SetClippingRegion(const nsIntRegion& aClippingRegion)
{
  mClippingRegion = aClippingRegion;
}

void
LayerManagerOGL::BeginTransaction()
{
}

void
LayerManagerOGL::BeginTransactionWithTarget(gfxContext *aTarget)
{
  mTarget = aTarget;
}

void
LayerManagerOGL::EndConstruction()
{
}

void
LayerManagerOGL::EndTransaction()
{
  Render();
  mTarget = NULL;
}

void
LayerManagerOGL::SetRoot(Layer *aLayer)
{
  mRootLayer =  static_cast<LayerOGL*>(aLayer->ImplData());;
}

already_AddRefed<ThebesLayer>
LayerManagerOGL::CreateThebesLayer()
{
  nsRefPtr<ThebesLayer> layer = new ThebesLayerOGL(this);
  return layer.forget();
}

already_AddRefed<ContainerLayer>
LayerManagerOGL::CreateContainerLayer()
{
  nsRefPtr<ContainerLayer> layer = new ContainerLayerOGL(this);
  return layer.forget();
}

already_AddRefed<ImageContainer>
LayerManagerOGL::CreateImageContainer()
{
  nsRefPtr<ImageContainer> container = new ImageContainerOGL(this);
  return container.forget();
}

already_AddRefed<ImageLayer>
LayerManagerOGL::CreateImageLayer()
{
  nsRefPtr<ImageLayer> layer = new ImageLayerOGL(this);
  return layer.forget();
}

void
LayerManagerOGL::SetClippingEnabled(PRBool aEnabled)
{
  if (aEnabled) {
    sglWrapper.Enable(LOCAL_GL_SCISSOR_TEST);
  } else {
    sglWrapper.Disable(LOCAL_GL_SCISSOR_TEST);
  }
}

void
LayerManagerOGL::MakeCurrent()
{
#ifdef XP_WIN
  BOOL succeeded = sglWrapper.wMakeCurrent(mDC, mContext);
  NS_ASSERTION(succeeded, "Failed to make GL context current!");
#endif
}

void
LayerManagerOGL::Render()
{
  nsIntRect rect;
  mWidget->GetBounds(rect);
  GLint width = rect.width;
  GLint height = rect.height;

  MakeCurrent();
  SetupBackBuffer();

  sglWrapper.BindFramebufferEXT(LOCAL_GL_FRAMEBUFFER_EXT, mFrameBuffer);
  sglWrapper.BlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA, LOCAL_GL_ONE, LOCAL_GL_ONE);
  sglWrapper.ClearColor(0.0, 0.0, 0.0, 0.0);
  sglWrapper.Clear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT);

  SetupPipeline();
  SetClippingEnabled(PR_FALSE);

  if (mRootLayer) {
    const nsIntRect *clipRect = mRootLayer->GetLayer()->GetClipRect();
    if (clipRect) {
      sglWrapper.Scissor(clipRect->x, clipRect->y, clipRect->width, clipRect->height);
    } else {
      sglWrapper.Scissor(0, 0, width, height);
    }

    mRootLayer->RenderLayer(mFrameBuffer);
  }

  if (mTarget) {
    CopyToTarget();
  } else {
    /**
     * Draw our backbuffer to the screen without using vertex or fragment
     * shaders. We're fine with just calculating the viewport coordinates
     * in software. And nothing special is required for the texture sampling.
     */
    sglWrapper.BindFramebufferEXT(LOCAL_GL_FRAMEBUFFER_EXT, 0);
    sglWrapper.UseProgram(0);
    sglWrapper.DisableVertexAttribArray(VERTEX_ATTRIB_LOCATION);
    sglWrapper.BindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
    sglWrapper.EnableClientState(LOCAL_GL_VERTEX_ARRAY);
    sglWrapper.EnableClientState(LOCAL_GL_TEXTURE_COORD_ARRAY);
    sglWrapper.BindTexture(LOCAL_GL_TEXTURE_2D, mBackBuffer);

    const nsIntRect *r;
    for (nsIntRegionRectIterator iter(mClippingRegion);
         (r = iter.Next()) != nsnull;) {
      sglWrapper.BlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ZERO, LOCAL_GL_ONE, LOCAL_GL_ZERO);
      float left = (GLfloat)r->x / width;
      float right = (GLfloat)r->XMost() / width;
      float top = (GLfloat)r->y / height;
      float bottom = (GLfloat)r->YMost() / height;

      float vertices[] = { left * 2.0f - 1.0f,
                           -(top * 2.0f - 1.0f),
                           right * 2.0f - 1.0f,
                           -(top * 2.0f - 1.0f),
                           left * 2.0f - 1.0f,
                           -(bottom * 2.0f - 1.0f),
                           right * 2.0f - 1.0f,
                           -(bottom * 2.0f - 1.0f) };
      float coords[] = { left, top, right, top, left, bottom, right, bottom };

      sglWrapper.VertexPointer(2, LOCAL_GL_FLOAT, 0, vertices);
      sglWrapper.TexCoordPointer(2, LOCAL_GL_FLOAT, 0, coords);
      sglWrapper.DrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);
    }
    sglWrapper.BindBuffer(LOCAL_GL_ARRAY_BUFFER, mVBO);
    sglWrapper.EnableVertexAttribArray(VERTEX_ATTRIB_LOCATION);
    sglWrapper.DisableClientState(LOCAL_GL_TEXTURE_COORD_ARRAY);
  }

  sglWrapper.Finish();
}

void
LayerManagerOGL::SetupPipeline()
{
  nsIntRect rect;
  mWidget->GetBounds(rect);

  sglWrapper.Viewport(0, 0, rect.width, rect.height);

  float viewMatrix[4][4];
  /**
   * Matrix to transform to viewport space ( <-1.0, 1.0> topleft, 
   * <1.0, -1.0> bottomright)
   */
  memset(&viewMatrix, 0, sizeof(viewMatrix));
  viewMatrix[0][0] = 2.0f / rect.width;
  viewMatrix[1][1] = 2.0f / rect.height;
  viewMatrix[2][2] = 1.0f;
  viewMatrix[3][0] = -1.0f;
  viewMatrix[3][1] = -1.0f;
  viewMatrix[3][3] = 1.0f;
  
  mRGBLayerProgram->Activate();
  mRGBLayerProgram->SetMatrixProj(&viewMatrix[0][0]);

  mYCbCrLayerProgram->Activate();
  mYCbCrLayerProgram->SetMatrixProj(&viewMatrix[0][0]);
}

PRBool
LayerManagerOGL::SetupBackBuffer()
{
  nsIntRect rect;
  mWidget->GetBounds(rect);
  GLint width = rect.width;
  GLint height = rect.height;

  if (width == mBackBufferSize.width && height == mBackBufferSize.height) {
    return PR_TRUE;
  }

  if (!mBackBuffer) {
    sglWrapper.GenTextures(1, &mBackBuffer);
  }

  /**
   * Setup the texture used as the backbuffer.
   */
  sglWrapper.BindTexture(LOCAL_GL_TEXTURE_2D, mBackBuffer);
  sglWrapper.TexEnvf(LOCAL_GL_TEXTURE_ENV, LOCAL_GL_TEXTURE_ENV_MODE, LOCAL_GL_MODULATE);
  sglWrapper.TexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_NEAREST);
  sglWrapper.TexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_NEAREST);
  sglWrapper.TexImage2D(LOCAL_GL_TEXTURE_2D,
                        0,
                        LOCAL_GL_RGBA,
                        width,
                        height,
                        0,
                        LOCAL_GL_RGBA,
                        LOCAL_GL_UNSIGNED_BYTE,
                        NULL);

  /**
   * Create the framebuffer and bind it to make our content render into our
   * framebuffer.
   */
  if (!mFrameBuffer) {
    sglWrapper.GenFramebuffersEXT(1, &mFrameBuffer);
  }

  sglWrapper.BindFramebufferEXT(LOCAL_GL_FRAMEBUFFER_EXT, mFrameBuffer);
  sglWrapper.FramebufferTexture2DEXT(LOCAL_GL_FRAMEBUFFER_EXT,
                                     LOCAL_GL_COLOR_ATTACHMENT0_EXT,
                                     LOCAL_GL_TEXTURE_2D,
                                     mBackBuffer,
                                     0);

  return PR_TRUE;
}

void
LayerManagerOGL::CopyToTarget()
{
  nsIntRect rect;
  mWidget->GetBounds(rect);
  GLint width = rect.width;
  GLint height = rect.height;

  if ((PRInt64)width * (PRInt64)height > PR_INT32_MAX) {
    NS_ERROR("Widget size too big - integer overflow!");
    return;
  }

  nsRefPtr<gfxImageSurface> imageSurface =
    new gfxImageSurface(gfxIntSize(width, height),
                        gfxASurface::ImageFormatARGB32);

  sglWrapper.ReadBuffer(LOCAL_GL_COLOR_ATTACHMENT0_EXT);

  if (imageSurface->Stride() != width * 4) {
    char *tmpData = new char[width * height * 4];

    sglWrapper.ReadPixels(0,
                          0,
                          width,
                          height,
                          LOCAL_GL_BGRA,
                          LOCAL_GL_UNSIGNED_BYTE,
                          tmpData);
    sglWrapper.Finish();

    for (int y = 0; y < height; y++) {
      memcpy(imageSurface->Data() + imageSurface->Stride() * y,
             tmpData + width * 4 * y,
             width * 4);
    }
    delete [] tmpData;
  } else {
    sglWrapper.ReadPixels(0,
                          0,
                          width,
                          height,
                          LOCAL_GL_BGRA,
                          LOCAL_GL_UNSIGNED_BYTE,
                          imageSurface->Data());
    sglWrapper.Finish();
  }                          

  mTarget->SetOperator(gfxContext::OPERATOR_OVER);
  mTarget->SetSource(imageSurface);
  mTarget->Paint();
}

LayerOGL::LayerOGL()
  : mNextSibling(NULL)
{
}

LayerOGL*
LayerOGL::GetNextSibling()
{
  return mNextSibling;
}

void
LayerOGL::SetNextSibling(LayerOGL *aNextSibling)
{
  mNextSibling = aNextSibling;
}

/**
 * LayerProgram Helpers
 */
LayerProgram::LayerProgram()
  : mProgram(0)
{
}

LayerProgram::~LayerProgram()
{
  sglWrapper.DeleteProgram(mProgram);
}

PRBool
LayerProgram::Initialize(GLuint aVertexShader, GLuint aFragmentShader)
{
  mProgram = sglWrapper.CreateProgram();
  sglWrapper.AttachShader(mProgram, aVertexShader);
  sglWrapper.AttachShader(mProgram, aFragmentShader);

  sglWrapper.BindAttribLocation(mProgram, VERTEX_ATTRIB_LOCATION, "aVertex");
  
  sglWrapper.LinkProgram(mProgram);

  GLint status;
  sglWrapper.GetProgramiv(mProgram, LOCAL_GL_LINK_STATUS, &status);

  if (!status) {
    return false;
  }
  return true;
}

void
LayerProgram::Activate()
{
  sglWrapper.UseProgram(mProgram);
}

void
LayerProgram::UpdateLocations()
{
  mMatrixProjLocation = sglWrapper.GetUniformLocation(mProgram, "uMatrixProj");
  mLayerQuadTransformLocation =
    sglWrapper.GetUniformLocation(mProgram, "uLayerQuadTransform");
  mLayerTransformLocation = sglWrapper.GetUniformLocation(mProgram, "uLayerTransform");
  mRenderTargetOffsetLocation =
    sglWrapper.GetUniformLocation(mProgram, "uRenderTargetOffset");
  mLayerOpacityLocation = sglWrapper.GetUniformLocation(mProgram, "uLayerOpacity");
}

void
LayerProgram::SetMatrixUniform(GLint aLocation, const GLfloat *aValue)
{
  sglWrapper.UniformMatrix4fv(aLocation, 1, false, aValue);
}

void
LayerProgram::SetInt(GLint aLocation, GLint aValue)
{
  sglWrapper.Uniform1i(aLocation, aValue);
}

void
LayerProgram::SetLayerOpacity(GLfloat aValue)
{
  sglWrapper.Uniform1f(mLayerOpacityLocation, aValue);
}

void
LayerProgram::PushRenderTargetOffset(GLfloat aValueX, GLfloat aValueY)
{
  GLvec2 vector;
  vector.mX = aValueX;
  vector.mY = aValueY;
  mRenderTargetOffsetStack.AppendElement(vector);
}

void
LayerProgram::PopRenderTargetOffset()
{
  NS_ASSERTION(mRenderTargetOffsetStack.Length(), "Unbalanced push/pops");
  mRenderTargetOffsetStack.RemoveElementAt(mRenderTargetOffsetStack.Length() - 1);
}

void
LayerProgram::Apply()
{
  if (!mRenderTargetOffsetStack.Length()) {
    sglWrapper.Uniform4f(mRenderTargetOffsetLocation, 0, 0, 0, 0);
  } else {
    GLvec2 vector =
      mRenderTargetOffsetStack[mRenderTargetOffsetStack.Length() - 1];
    sglWrapper.Uniform4f(mRenderTargetOffsetLocation, vector.mX, vector.mY, 0, 0);
  }
}

void
RGBLayerProgram::UpdateLocations()
{
  LayerProgram::UpdateLocations();

  mLayerTextureLocation = sglWrapper.GetUniformLocation(mProgram, "uLayerTexture");
}

void
YCbCrLayerProgram::UpdateLocations()
{
  LayerProgram::UpdateLocations();

  mYTextureLocation = sglWrapper.GetUniformLocation(mProgram, "uYTexture");
  mCbTextureLocation = sglWrapper.GetUniformLocation(mProgram, "uCbTexture");
  mCrTextureLocation = sglWrapper.GetUniformLocation(mProgram, "uCrTexture");
}

} /* layers */
} /* mozilla */
