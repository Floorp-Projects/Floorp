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

#include "GLContext.h"
#include "GLContextProvider.h"

#include "nsIServiceManager.h"
#include "nsIConsoleService.h"

static const GLint VERTEX_ATTRIB_LOCATION = 0;

namespace mozilla {
namespace layers {

using namespace mozilla::gl;

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
  mGLContext->MakeCurrent();
  delete mRGBLayerProgram;
  delete mYCbCrLayerProgram;
}

PRBool
LayerManagerOGL::Initialize()
{
  mGLContext = sGLContextProvider.CreateForWindow(mWidget);

  if (!mGLContext) {
    return PR_FALSE;
  }

  MakeCurrent();

  mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA, LOCAL_GL_ONE, LOCAL_GL_ONE);
  mGLContext->fEnable(LOCAL_GL_BLEND);
  mGLContext->fEnable(LOCAL_GL_TEXTURE_2D);
  mGLContext->fEnable(LOCAL_GL_SCISSOR_TEST);

  mVertexShader = mGLContext->fCreateShader(LOCAL_GL_VERTEX_SHADER);
  mRGBShader = mGLContext->fCreateShader(LOCAL_GL_FRAGMENT_SHADER);
  mYUVShader = mGLContext->fCreateShader(LOCAL_GL_FRAGMENT_SHADER);

  mGLContext->fShaderSource(mVertexShader, 1, (const GLchar**)&sVertexShader, NULL);
  mGLContext->fShaderSource(mRGBShader, 1, (const GLchar**)&sRGBLayerPS, NULL);
  mGLContext->fShaderSource(mYUVShader, 1, (const GLchar**)&sYUVLayerPS, NULL);

  mGLContext->fCompileShader(mVertexShader);
  mGLContext->fCompileShader(mRGBShader);
  mGLContext->fCompileShader(mYUVShader);

  GLint status;
  mGLContext->fGetShaderiv(mVertexShader, LOCAL_GL_COMPILE_STATUS, &status);
  if (!status) {
    return false;
  }

  mGLContext->fGetShaderiv(mRGBShader, LOCAL_GL_COMPILE_STATUS, &status);
  if (!status) {
    return false;
  }

  mGLContext->fGetShaderiv(mYUVShader, LOCAL_GL_COMPILE_STATUS, &status);

  if (!status) {
    return false;
  }

  /**
   * We'll test the ability here to bind NPOT textures to a framebuffer, if
   * this fails we'll try EXT_texture_rectangle.
   */
  mGLContext->fGenFramebuffers(1, &mFrameBuffer);

  GLenum textureTargets[] = { LOCAL_GL_TEXTURE_2D,
                              LOCAL_GL_TEXTURE_RECTANGLE_EXT };
  mFBOTextureTarget = 0;

  for (int i = 0; i < NS_ARRAY_LENGTH(textureTargets); i++) {
    mGLContext->fGenTextures(1, &mBackBuffer);
    mGLContext->fBindTexture(textureTargets[i], mBackBuffer);
    mGLContext->fTexParameteri(textureTargets[i],
                               LOCAL_GL_TEXTURE_MIN_FILTER,
                               LOCAL_GL_NEAREST);
    mGLContext->fTexParameteri(textureTargets[i],
                               LOCAL_GL_TEXTURE_MAG_FILTER,
                               LOCAL_GL_NEAREST);
    mGLContext->fTexImage2D(textureTargets[i],
                            0,
                            LOCAL_GL_RGBA,
                            200,
                            100,
                            0,
                            LOCAL_GL_RGBA,
                            LOCAL_GL_UNSIGNED_BYTE,
                            NULL);

    mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mFrameBuffer);
    mGLContext->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                      LOCAL_GL_COLOR_ATTACHMENT0,
                                      textureTargets[i],
                                      mBackBuffer,
                                      0);

    if (mGLContext->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER) ==
        LOCAL_GL_FRAMEBUFFER_COMPLETE) {
          mFBOTextureTarget = textureTargets[i];
          break;
    }

    mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);
    mGLContext->fBindTexture(textureTargets[i], 0);
    /**
     * We need to delete this texture since we can't bind a texture multiple
     * time to different textures.
     */
    mGLContext->fDeleteTextures(1, &mBackBuffer);
  }
  if (mFBOTextureTarget == 0) {
    /* Unable to find a texture target that works with FBOs and NPOT textures */
    return false;
  }

  mRGBLayerProgram = new RGBLayerProgram();
  if (!mRGBLayerProgram->Initialize(mVertexShader, mRGBShader, mGLContext)) {
    return false;
  }
  mYCbCrLayerProgram = new YCbCrLayerProgram();
  if (!mYCbCrLayerProgram->Initialize(mVertexShader, mYUVShader, mGLContext)) {
    return false;
  }

  mRGBLayerProgram->UpdateLocations();
  mYCbCrLayerProgram->UpdateLocations();

  mGLContext->fGenBuffers(1, &mVBO);
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mVBO);
  mGLContext->fEnableClientState(LOCAL_GL_VERTEX_ARRAY);
  mGLContext->fEnableVertexAttribArray(VERTEX_ATTRIB_LOCATION);

  GLfloat vertices[] = { 0, 0, 1.0f, 0, 0, 1.0f, 1.0f, 1.0f };
  mGLContext->fBufferData(LOCAL_GL_ARRAY_BUFFER, sizeof(vertices), vertices, LOCAL_GL_STATIC_DRAW);

  mRGBLayerProgram->Activate();
  mGLContext->fVertexAttribPointer(VERTEX_ATTRIB_LOCATION,
                        2,
                        LOCAL_GL_FLOAT,
                        LOCAL_GL_FALSE,
                        0,
                        0);
  mYCbCrLayerProgram->Activate();
  mGLContext->fVertexAttribPointer(VERTEX_ATTRIB_LOCATION,
                        2,
                        LOCAL_GL_FLOAT,
                        LOCAL_GL_FALSE,
                        0,
                        0);

  mRGBLayerProgram->SetLayerTexture(0);

  mYCbCrLayerProgram->SetYTexture(0);
  mYCbCrLayerProgram->SetCbTexture(1);
  mYCbCrLayerProgram->SetCrTexture(2);

  nsCOMPtr<nsIConsoleService> 
    console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));

  if (console) {
    nsString msg;
    msg +=
      NS_LITERAL_STRING("OpenGL LayerManager Initialized Succesfully.\nVersion: ");
    msg += NS_ConvertUTF8toUTF16(
      nsDependentCString((const char*)mGLContext->fGetString(LOCAL_GL_VERSION)));
    msg += NS_LITERAL_STRING("\nVendor: ");
    msg += NS_ConvertUTF8toUTF16(
      nsDependentCString((const char*)mGLContext->fGetString(LOCAL_GL_VENDOR)));
    msg += NS_LITERAL_STRING("\nRenderer: ");
    msg += NS_ConvertUTF8toUTF16(
      nsDependentCString((const char*)mGLContext->fGetString(LOCAL_GL_RENDERER)));
    console->LogStringMessage(msg.get());
  }

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
    mGLContext->fEnable(LOCAL_GL_SCISSOR_TEST);
  } else {
    mGLContext->fDisable(LOCAL_GL_SCISSOR_TEST);
  }
}

void
LayerManagerOGL::MakeCurrent()
{
    mGLContext->MakeCurrent();
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

  mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mFrameBuffer);
  mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA, LOCAL_GL_ONE, LOCAL_GL_ONE);
  mGLContext->fClearColor(0.0, 0.0, 0.0, 0.0);
  mGLContext->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT);

  SetupPipeline();
  SetClippingEnabled(PR_FALSE);

  if (mRootLayer) {
    const nsIntRect *clipRect = mRootLayer->GetLayer()->GetClipRect();
    if (clipRect) {
      mGLContext->fScissor(clipRect->x, clipRect->y, clipRect->width, clipRect->height);
    } else {
      mGLContext->fScissor(0, 0, width, height);
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
    mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);
    mGLContext->fUseProgram(0);
    if (mFBOTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE_EXT) {
      mGLContext->fEnable(LOCAL_GL_TEXTURE_RECTANGLE_EXT);
    }
    mGLContext->fDisableVertexAttribArray(VERTEX_ATTRIB_LOCATION);
    mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
    mGLContext->fEnableClientState(LOCAL_GL_VERTEX_ARRAY);
    mGLContext->fEnableClientState(LOCAL_GL_TEXTURE_COORD_ARRAY);
    mGLContext->fBindTexture(mFBOTextureTarget, mBackBuffer);

    const nsIntRect *r;
    for (nsIntRegionRectIterator iter(mClippingRegion);
         (r = iter.Next()) != nsnull;) {
      mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ZERO, LOCAL_GL_ONE, LOCAL_GL_ZERO);
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

      float coords[8];
      if (mFBOTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE_EXT) {
        /* These are in non-normalized texture coords */
        coords[0] = (GLfloat)r->x; coords[1] = (GLfloat)r->y;
        coords[2] = (GLfloat)r->XMost(); coords[3] = (GLfloat)r->y;
        coords[4] = (GLfloat)r->x; coords[5] = (GLfloat)r->YMost();
        coords[6] = (GLfloat)r->XMost(); coords[7] = (GLfloat)r->YMost();
      } else {
        coords[0] = left; coords[1] = top;
        coords[2] = right; coords[3] = top;
        coords[4] = left; coords[5] = bottom;
        coords[6] = right; coords[7] = bottom;
      }

      mGLContext->fVertexPointer(2, LOCAL_GL_FLOAT, 0, vertices);
      mGLContext->fTexCoordPointer(2, LOCAL_GL_FLOAT, 0, coords);
      mGLContext->fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);
    }
    mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mVBO);
    mGLContext->fEnableVertexAttribArray(VERTEX_ATTRIB_LOCATION);
    mGLContext->fDisableClientState(LOCAL_GL_TEXTURE_COORD_ARRAY);
    if (mFBOTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE_EXT) {
      mGLContext->fDisable(LOCAL_GL_TEXTURE_RECTANGLE_EXT);
    }
  }

  mGLContext->fFinish();
}

void
LayerManagerOGL::SetupPipeline()
{
  nsIntRect rect;
  mWidget->GetBounds(rect);

  mGLContext->fViewport(0, 0, rect.width, rect.height);

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

  /**
   * Setup the texture used as the backbuffer. We use a texture as our
   * backbuffer since we can rely on both GLES and OGL 2.1 to support this
   * method.
   */
  mGLContext->fBindTexture(mFBOTextureTarget, mBackBuffer);
  mGLContext->fTexEnvf(LOCAL_GL_TEXTURE_ENV, LOCAL_GL_TEXTURE_ENV_MODE, LOCAL_GL_MODULATE);
  mGLContext->fTexParameteri(mFBOTextureTarget, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_NEAREST);
  mGLContext->fTexParameteri(mFBOTextureTarget, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_NEAREST);
  mGLContext->fTexImage2D(mFBOTextureTarget,
                        0,
                        LOCAL_GL_RGBA,
                        width,
                        height,
                        0,
                        LOCAL_GL_RGBA,
                        LOCAL_GL_UNSIGNED_BYTE,
                        NULL);

  /* Bind our framebuffer to make our content render into our backbuffer. */
  mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mFrameBuffer);
  mGLContext->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                   LOCAL_GL_COLOR_ATTACHMENT0,
                                   mFBOTextureTarget,
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

  mGLContext->fReadBuffer(LOCAL_GL_COLOR_ATTACHMENT0);

  if (imageSurface->Stride() != width * 4) {
    char *tmpData = new char[width * height * 4];

    mGLContext->fReadPixels(0,
                          0,
                          width,
                          height,
                          LOCAL_GL_BGRA,
                          LOCAL_GL_UNSIGNED_BYTE,
                          tmpData);
    mGLContext->fFinish();

    for (int y = 0; y < height; y++) {
      memcpy(imageSurface->Data() + imageSurface->Stride() * y,
             tmpData + width * 4 * y,
             width * 4);
    }
    delete [] tmpData;
  } else {
    mGLContext->fReadPixels(0,
                          0,
                          width,
                          height,
                          LOCAL_GL_BGRA,
                          LOCAL_GL_UNSIGNED_BYTE,
                          imageSurface->Data());
    mGLContext->fFinish();
  }                          

  mTarget->SetOperator(gfxContext::OPERATOR_OVER);
  mTarget->SetSource(imageSurface);
  mTarget->Paint();
}

LayerOGL::LayerOGL(LayerManagerOGL *aManager)
  : mOGLManager(aManager)
  , mNextSibling(NULL)
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
  : mGLContext(NULL)
  , mProgram(0)
{
}

LayerProgram::~LayerProgram()
{
  mGLContext->fDeleteProgram(mProgram);
}

PRBool
LayerProgram::Initialize(GLuint aVertexShader,
                         GLuint aFragmentShader,
                         mozilla::gl::GLContext *aContext)
{
  mGLContext = aContext;

  mProgram = mGLContext->fCreateProgram();
  mGLContext->fAttachShader(mProgram, aVertexShader);
  mGLContext->fAttachShader(mProgram, aFragmentShader);

  mGLContext->fBindAttribLocation(mProgram, VERTEX_ATTRIB_LOCATION, "aVertex");
  
  mGLContext->fLinkProgram(mProgram);

  GLint status;
  mGLContext->fGetProgramiv(mProgram, LOCAL_GL_LINK_STATUS, &status);

  if (!status) {
    return false;
  }
  return true;
}

void
LayerProgram::Activate()
{
  mGLContext->fUseProgram(mProgram);
}

void
LayerProgram::UpdateLocations()
{
  mMatrixProjLocation = mGLContext->fGetUniformLocation(mProgram, "uMatrixProj");
  mLayerQuadTransformLocation =
    mGLContext->fGetUniformLocation(mProgram, "uLayerQuadTransform");
  mLayerTransformLocation = mGLContext->fGetUniformLocation(mProgram, "uLayerTransform");
  mRenderTargetOffsetLocation =
    mGLContext->fGetUniformLocation(mProgram, "uRenderTargetOffset");
  mLayerOpacityLocation = mGLContext->fGetUniformLocation(mProgram, "uLayerOpacity");
}

void
LayerProgram::SetMatrixUniform(GLint aLocation, const GLfloat *aValue)
{
  mGLContext->fUniformMatrix4fv(aLocation, 1, false, aValue);
}

void
LayerProgram::SetInt(GLint aLocation, GLint aValue)
{
  mGLContext->fUniform1i(aLocation, aValue);
}

void
LayerProgram::SetLayerOpacity(GLfloat aValue)
{
  mGLContext->fUniform1f(mLayerOpacityLocation, aValue);
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
    mGLContext->fUniform4f(mRenderTargetOffsetLocation, 0, 0, 0, 0);
  } else {
    GLvec2 vector =
      mRenderTargetOffsetStack[mRenderTargetOffsetStack.Length() - 1];
    mGLContext->fUniform4f(mRenderTargetOffsetLocation, vector.mX, vector.mY, 0, 0);
  }
}

void
RGBLayerProgram::UpdateLocations()
{
  LayerProgram::UpdateLocations();

  mLayerTextureLocation = mGLContext->fGetUniformLocation(mProgram, "uLayerTexture");
}

void
YCbCrLayerProgram::UpdateLocations()
{
  LayerProgram::UpdateLocations();

  mYTextureLocation = mGLContext->fGetUniformLocation(mProgram, "uYTexture");
  mCbTextureLocation = mGLContext->fGetUniformLocation(mProgram, "uCbTexture");
  mCrTextureLocation = mGLContext->fGetUniformLocation(mProgram, "uCrTexture");
}

} /* layers */
} /* mozilla */
