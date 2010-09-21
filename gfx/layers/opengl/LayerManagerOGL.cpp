/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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

#include "LayerManagerOGL.h"
#include "ThebesLayerOGL.h"
#include "ContainerLayerOGL.h"
#include "ImageLayerOGL.h"
#include "ColorLayerOGL.h"
#include "CanvasLayerOGL.h"

#include "LayerManagerOGLShaders.h"

#include "gfxContext.h"
#include "nsIWidget.h"

#include "GLContext.h"
#include "GLContextProvider.h"

#include "nsIServiceManager.h"
#include "nsIConsoleService.h"

#include "nsIGfxInfo.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gl;

int LayerManagerOGLProgram::sCurrentProgramKey = 0;

/**
 * LayerManagerOGL
 */
LayerManagerOGL::LayerManagerOGL(nsIWidget *aWidget)
  : mWidget(aWidget)
  , mBackBufferFBO(0)
  , mBackBufferTexture(0)
  , mBackBufferSize(-1, -1)
  , mHasBGRA(0)
{
}

LayerManagerOGL::~LayerManagerOGL()
{
  Destroy();
}

void
LayerManagerOGL::Destroy()
{
  if (!mDestroyed) {
    if (mRoot) {
      RootLayer()->Destroy();
    }
    mRoot = nsnull;

    // Make a copy, since SetLayerManager will cause mImageContainers
    // to get mutated.
    nsTArray<ImageContainer*> imageContainers(mImageContainers);
    for (PRUint32 i = 0; i < imageContainers.Length(); ++i) {
      ImageContainer *c = imageContainers[i];
      c->SetLayerManager(nsnull);
    }

    CleanupResources();

    mDestroyed = PR_TRUE;
  }
}

void
LayerManagerOGL::CleanupResources()
{
  if (!mGLContext)
    return;

  nsRefPtr<GLContext> ctx = mGLContext->GetSharedContext();
  if (!ctx) {
    ctx = mGLContext;
  }

  ctx->MakeCurrent();

  for (unsigned int i = 0; i < mPrograms.Length(); ++i)
    delete mPrograms[i];
  mPrograms.Clear();

  ctx->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

  if (mBackBufferFBO) {
    ctx->fDeleteFramebuffers(1, &mBackBufferFBO);
    mBackBufferFBO = 0;
  }

  if (mBackBufferTexture) {
    ctx->fDeleteTextures(1, &mBackBufferTexture);
    mBackBufferTexture = 0;
  }

  if (mQuadVBO) {
    ctx->fDeleteBuffers(1, &mQuadVBO);
    mQuadVBO = 0;
  }

  mGLContext = nsnull;
}

PRBool
LayerManagerOGL::Initialize(GLContext *aExistingContext)
{
  if (aExistingContext) {
    mGLContext = aExistingContext;
  } else {
    if (mGLContext)
      CleanupResources();

    nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
    if (gfxInfo) {
      PRInt32 status;
      if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_OPENGL_LAYERS, &status))) {
        if (status != nsIGfxInfo::FEATURE_STATUS_UNKNOWN &&
            status != nsIGfxInfo::FEATURE_AVAILABLE) {
          NS_WARNING("OpenGL-accelerated layers are not supported on this system.");
          return PR_FALSE;
        }
      }
    }

    mGLContext = nsnull;

#ifdef XP_WIN
    if (PR_GetEnv("MOZ_LAYERS_PREFER_EGL")) {
      printf_stderr("Trying GL layers...\n");
      mGLContext = gl::GLContextProviderEGL::CreateForWindow(mWidget);
    }
#endif

    if (!mGLContext)
      mGLContext = gl::GLContextProvider::CreateForWindow(mWidget);

    if (!mGLContext) {
      NS_WARNING("Failed to create LayerManagerOGL context");
      return PR_FALSE;
    }
  }

  MakeCurrent();

  DEBUG_GL_ERROR_CHECK(mGLContext);

  mHasBGRA =
    mGLContext->IsExtensionSupported(gl::GLContext::EXT_texture_format_BGRA8888) ||
    mGLContext->IsExtensionSupported(gl::GLContext::EXT_bgra);

  mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                 LOCAL_GL_ONE, LOCAL_GL_ONE);
  mGLContext->fEnable(LOCAL_GL_BLEND);

  // We unfortunately can't do generic initialization here, since the
  // concrete type actually matters.  This macro generates the
  // initialization using a concrete type and index.
#define SHADER_PROGRAM(penum, ptype, vsstr, fsstr) do {                           \
    NS_ASSERTION(programIndex++ == penum, "out of order shader initialization!"); \
    ptype *p = new ptype(mGLContext);                                             \
    if (!p->Initialize(vsstr, fsstr)) {                                           \
      delete p;                                                                   \
      return PR_FALSE;                                                            \
    }                                                                             \
    mPrograms.AppendElement(p);                                                   \
  } while (0)


  // NOTE: Order matters here, and should be in the same order as the
  // ProgramType enum!
  GLint programIndex = 0;

  /* Layer programs */
  SHADER_PROGRAM(RGBALayerProgramType, ColorTextureLayerProgram,
                 sLayerVS, sRGBATextureLayerFS);
  SHADER_PROGRAM(BGRALayerProgramType, ColorTextureLayerProgram,
                 sLayerVS, sBGRATextureLayerFS);
  SHADER_PROGRAM(RGBXLayerProgramType, ColorTextureLayerProgram,
                 sLayerVS, sRGBXTextureLayerFS);
  SHADER_PROGRAM(BGRXLayerProgramType, ColorTextureLayerProgram,
                 sLayerVS, sBGRXTextureLayerFS);
  SHADER_PROGRAM(RGBARectLayerProgramType, ColorTextureLayerProgram,
                 sLayerVS, sRGBARectTextureLayerFS);
  SHADER_PROGRAM(ColorLayerProgramType, SolidColorLayerProgram,
                 sLayerVS, sSolidColorLayerFS);
  SHADER_PROGRAM(YCbCrLayerProgramType, YCbCrTextureLayerProgram,
                 sLayerVS, sYCbCrTextureLayerFS);
  /* Copy programs (used for final framebuffer blit) */
  SHADER_PROGRAM(Copy2DProgramType, CopyProgram,
                 sCopyVS, sCopy2DFS);
  SHADER_PROGRAM(Copy2DRectProgramType, CopyProgram,
                 sCopyVS, sCopy2DRectFS);

#undef SHADER_PROGRAM

  NS_ASSERTION(programIndex == NumProgramTypes,
               "not all programs were initialized!");

  /**
   * We'll test the ability here to bind NPOT textures to a framebuffer, if
   * this fails we'll try ARB_texture_rectangle.
   */
  mGLContext->fGenFramebuffers(1, &mBackBufferFBO);

  GLenum textureTargets[] = {
    LOCAL_GL_TEXTURE_2D,
#ifndef USE_GLES2
    LOCAL_GL_TEXTURE_RECTANGLE_ARB
#endif
  };

  mFBOTextureTarget = LOCAL_GL_NONE;

  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(textureTargets); i++) {
    GLenum target = textureTargets[i];
    mGLContext->fGenTextures(1, &mBackBufferTexture);
    mGLContext->fBindTexture(target, mBackBufferTexture);
    mGLContext->fTexParameteri(target,
                               LOCAL_GL_TEXTURE_MIN_FILTER,
                               LOCAL_GL_NEAREST);
    mGLContext->fTexParameteri(target,
                               LOCAL_GL_TEXTURE_MAG_FILTER,
                               LOCAL_GL_NEAREST);
    mGLContext->fTexImage2D(target,
                            0,
                            LOCAL_GL_RGBA,
                            5, 3, /* sufficiently NPOT */
                            0,
                            LOCAL_GL_RGBA,
                            LOCAL_GL_UNSIGNED_BYTE,
                            NULL);

    // unbind this texture, in preparation for binding it to the FBO
    mGLContext->fBindTexture(target, 0);

    mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mBackBufferFBO);
    mGLContext->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                      LOCAL_GL_COLOR_ATTACHMENT0,
                                      target,
                                      mBackBufferTexture,
                                      0);

    if (mGLContext->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER) ==
        LOCAL_GL_FRAMEBUFFER_COMPLETE)
    {
      mFBOTextureTarget = target;
      break;
    }

    // We weren't succesful with this texture, so we don't need it
    // any more.
    mGLContext->fDeleteTextures(1, &mBackBufferTexture);
  }

  if (mFBOTextureTarget == LOCAL_GL_NONE) {
    /* Unable to find a texture target that works with FBOs and NPOT textures */
    return false;
  }

  mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

  if (mFBOTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE_ARB) {
    /* If we're using TEXTURE_RECTANGLE, then we must have the ARB
     * extension -- the EXT variant does not provide support for
     * texture rectangle access inside GLSL (sampler2DRect,
     * texture2DRect).
     */
    if (!mGLContext->IsExtensionSupported(gl::GLContext::ARB_texture_rectangle))
      return false;
  }

  // back to default framebuffer, to avoid confusion
  mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

  DEBUG_GL_ERROR_CHECK(mGLContext);

  /* Create a simple quad VBO */

  mGLContext->fGenBuffers(1, &mQuadVBO);
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mQuadVBO);

  GLfloat vertices[] = {
    /* First quad vertices */
    0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
    /* Then quad texcoords */
    0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
    /* Then flipped quad texcoords */
    0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
  };
  mGLContext->fBufferData(LOCAL_GL_ARRAY_BUFFER, sizeof(vertices), vertices, LOCAL_GL_STATIC_DRAW);

  DEBUG_GL_ERROR_CHECK(mGLContext);

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
    msg += NS_LITERAL_STRING("\nFBO Texture Target: ");
    if (mFBOTextureTarget == LOCAL_GL_TEXTURE_2D)
      msg += NS_LITERAL_STRING("TEXTURE_2D");
    else
      msg += NS_LITERAL_STRING("TEXTURE_RECTANGLE");
    console->LogStringMessage(msg.get());
  }

  DEBUG_GL_ERROR_CHECK(mGLContext);

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
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }

  mTarget = aTarget;
}

void
LayerManagerOGL::EndTransaction(DrawThebesLayerCallback aCallback,
                                void* aCallbackData)
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }

  mThebesLayerCallback = aCallback;
  mThebesLayerCallbackData = aCallbackData;

  Render();

  mThebesLayerCallback = nsnull;
  mThebesLayerCallbackData = nsnull;

  mTarget = NULL;
}

already_AddRefed<ThebesLayer>
LayerManagerOGL::CreateThebesLayer()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nsnull;
  }

  nsRefPtr<ThebesLayer> layer = new ThebesLayerOGL(this);
  return layer.forget();
}

already_AddRefed<ContainerLayer>
LayerManagerOGL::CreateContainerLayer()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nsnull;
  }

  nsRefPtr<ContainerLayer> layer = new ContainerLayerOGL(this);
  return layer.forget();
}

already_AddRefed<ImageContainer>
LayerManagerOGL::CreateImageContainer()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nsnull;
  }

  nsRefPtr<ImageContainer> container = new ImageContainerOGL(this);
  RememberImageContainer(container);
  return container.forget();
}

already_AddRefed<ImageLayer>
LayerManagerOGL::CreateImageLayer()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nsnull;
  }

  nsRefPtr<ImageLayer> layer = new ImageLayerOGL(this);
  return layer.forget();
}

already_AddRefed<ColorLayer>
LayerManagerOGL::CreateColorLayer()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nsnull;
  }

  nsRefPtr<ColorLayer> layer = new ColorLayerOGL(this);
  return layer.forget();
}

already_AddRefed<CanvasLayer>
LayerManagerOGL::CreateCanvasLayer()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nsnull;
  }

  nsRefPtr<CanvasLayer> layer = new CanvasLayerOGL(this);
  return layer.forget();
}

void
LayerManagerOGL::ForgetImageContainer(ImageContainer *aContainer)
{
  NS_ASSERTION(aContainer->Manager() == this,
               "ForgetImageContainer called on non-owned container!");

  if (!mImageContainers.RemoveElement(aContainer)) {
    NS_WARNING("ForgetImageContainer couldn't find container it was supposed to forget!");
    return;
  }
}

void
LayerManagerOGL::RememberImageContainer(ImageContainer *aContainer)
{
  NS_ASSERTION(aContainer->Manager() == this,
               "RememberImageContainer called on non-owned container!");
  mImageContainers.AppendElement(aContainer);
}

LayerOGL*
LayerManagerOGL::RootLayer() const
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nsnull;
  }

  return static_cast<LayerOGL*>(mRoot->ImplData());
}

void
LayerManagerOGL::Render()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }

  nsIntRect rect;
  mWidget->GetClientBounds(rect);

  GLint width = rect.width;
  GLint height = rect.height;

  // We can't draw anything to something with no area
  // so just return
  if (width == 0 || height == 0)
    return;

  // If the widget size changed, we have to force a MakeCurrent
  // to make sure that GL sees the updated widget size.
  if (mWidgetSize.width != width ||
      mWidgetSize.height != height)
  {
    MakeCurrent(PR_TRUE);

    mWidgetSize.width = width;
    mWidgetSize.height = height;
  } else {
    MakeCurrent();
  }

  DEBUG_GL_ERROR_CHECK(mGLContext);

  SetupBackBuffer(width, height);
  SetupPipeline(width, height);

  // Default blend function implements "OVER"
  mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                 LOCAL_GL_ONE, LOCAL_GL_ONE);
  mGLContext->fEnable(LOCAL_GL_BLEND);

  DEBUG_GL_ERROR_CHECK(mGLContext);

  const nsIntRect *clipRect = mRoot->GetClipRect();

  if (clipRect) {
    nsIntRect r = *clipRect;
    if (!mGLContext->IsDoubleBuffered())
      mGLContext->FixWindowCoordinateRect(r, mWidgetSize.height);
    mGLContext->fScissor(r.x, r.y, r.width, r.height);
  } else {
    mGLContext->fScissor(0, 0, width, height);
  }

  mGLContext->fEnable(LOCAL_GL_SCISSOR_TEST);

  DEBUG_GL_ERROR_CHECK(mGLContext);

  mGLContext->fClearColor(0.0, 0.0, 0.0, 0.0);
  mGLContext->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT);

  // Render our layers.
  RootLayer()->RenderLayer(mGLContext->IsDoubleBuffered() ? 0 : mBackBufferFBO,
                           nsIntPoint(0, 0));

  DEBUG_GL_ERROR_CHECK(mGLContext);

  if (mTarget) {
    CopyToTarget();
    return;
  }

  if (mGLContext->IsDoubleBuffered()) {
    mGLContext->SwapBuffers();
    return;
  }

  mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

  mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);

  CopyProgram *copyprog = GetCopy2DProgram();

  if (mFBOTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE_ARB) {
    copyprog = GetCopy2DRectProgram();
  }

  mGLContext->fBindTexture(mFBOTextureTarget, mBackBufferTexture);

  copyprog->Activate();
  copyprog->SetTextureUnit(0);

  if (copyprog->GetTexCoordMultiplierUniformLocation() != -1) {
    float f[] = { float(width), float(height) };
    copyprog->SetUniform(copyprog->GetTexCoordMultiplierUniformLocation(),
                         2, f);
  }

  DEBUG_GL_ERROR_CHECK(mGLContext);

  // we're going to use client-side vertex arrays for this.
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);

  // "COPY"
  mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ZERO,
                                 LOCAL_GL_ONE, LOCAL_GL_ZERO);

  // enable our vertex attribs; we'll call glVertexPointer below
  // to fill with the correct data.
  GLint vcattr = copyprog->AttribLocation(CopyProgram::VertexCoordAttrib);
  GLint tcattr = copyprog->AttribLocation(CopyProgram::TexCoordAttrib);

  mGLContext->fEnableVertexAttribArray(vcattr);
  mGLContext->fEnableVertexAttribArray(tcattr);

  const nsIntRect *r;
  nsIntRegionRectIterator iter(mClippingRegion);

  while ((r = iter.Next()) != nsnull) {
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

    float coords[] = { left, top,
                       right, top,
                       left, bottom,
                       right, bottom };

    mGLContext->fVertexAttribPointer(vcattr,
                                     2, LOCAL_GL_FLOAT,
                                     LOCAL_GL_FALSE,
                                     0, vertices);

    mGLContext->fVertexAttribPointer(tcattr,
                                     2, LOCAL_GL_FLOAT,
                                     LOCAL_GL_FALSE,
                                     0, coords);

    mGLContext->fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);
    DEBUG_GL_ERROR_CHECK(mGLContext);
  }

  mGLContext->fDisableVertexAttribArray(vcattr);
  mGLContext->fDisableVertexAttribArray(tcattr);

  DEBUG_GL_ERROR_CHECK(mGLContext);

  mGLContext->fFlush();

  DEBUG_GL_ERROR_CHECK(mGLContext);
}

void
LayerManagerOGL::SetupPipeline(int aWidth, int aHeight)
{
  // Set the viewport correctly.  Note that his viewport is used
  // throughout the GL layers rendering pipeline, even when we're
  // rendering to a FBO with different dimensions than the window.
  // This means that we can set the viewMatrix once on every program
  // (below).  When we render to a FBO (as in ContainerLayerOGL), we
  // have to pass a correct child offset so that the coordinate system
  // is translated appropriately to start at the origin of the FBO
  // (or, put another way, so that the FBO looks to be at the right
  // spot in the parent).
  //
  // Note: this effectively means that we can't really draw to a FBO
  // that is bigger than the window dimensions.  This is fine for now,
  // but might be a problem if we ever start doing GL drawing to
  // retained layer FBOs that happen to retain more than is visible.
  //
  // When we're not double buffering, we use a FBO as our backbuffer.
  // We use a normal view transform in that case, meaning that our FBO
  // and all other FBOs look upside down.  We then do a Y-flip when
  // we draw it into the window.
  mGLContext->fViewport(0, 0, aWidth, aHeight);

  // Matrix to transform to viewport space ( <-1.0, 1.0> topleft, 
  // <1.0, -1.0> bottomright).
  //
  // When we are double buffering, we change the view matrix around so
  // that everything is right-side up; we're drawing directly into
  // the window's back buffer, so this keeps things looking correct.
  //
  // XXX we could potentially always use the double-buffering view
  // matrix and just change our single-buffer draw code.
  //
  // XXX we keep track of whether the window size changed, so we can
  // skip this update if it hadn't since the last call.
  gfx3DMatrix viewMatrix;
  if (mGLContext->IsDoubleBuffered()) {
    /* If it's double buffered, we don't have a frontbuffer FBO,
     * so put in a Y-flip in this transform.
     */
    viewMatrix._11 = 2.0f / float(aWidth);
    viewMatrix._22 = -2.0f / float(aHeight);
    viewMatrix._41 = -1.0f;
    viewMatrix._42 = 1.0f;
  } else {
    viewMatrix._11 = 2.0f / float(aWidth);
    viewMatrix._22 = 2.0f / float(aHeight);
    viewMatrix._41 = -1.0f;
    viewMatrix._42 = -1.0f;
  }

  SetLayerProgramProjectionMatrix(viewMatrix);
}

void
LayerManagerOGL::SetupBackBuffer(int aWidth, int aHeight)
{
  if (mGLContext->IsDoubleBuffered()) {
    mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);
    return;
  }

  // Do we have a FBO of the right size already?
  if (mBackBufferSize.width == aWidth &&
      mBackBufferSize.height == aHeight)
  {
    mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mBackBufferFBO);
    return;
  }

  // we already have a FBO, but we need to resize its texture.
  mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);
  mGLContext->fBindTexture(mFBOTextureTarget, mBackBufferTexture);
  mGLContext->fTexImage2D(mFBOTextureTarget,
                          0,
                          LOCAL_GL_RGBA,
                          aWidth, aHeight,
                          0,
                          LOCAL_GL_RGBA,
                          LOCAL_GL_UNSIGNED_BYTE,
                          NULL);
  mGLContext->fBindTexture(mFBOTextureTarget, 0);

  mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mBackBufferFBO);
  mGLContext->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                    LOCAL_GL_COLOR_ATTACHMENT0,
                                    mFBOTextureTarget,
                                    mBackBufferTexture,
                                    0);

  NS_ASSERTION(mGLContext->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER) ==
               LOCAL_GL_FRAMEBUFFER_COMPLETE, "Error setting up framebuffer.");

  mBackBufferSize.width = aWidth;
  mBackBufferSize.height = aHeight;
}

void
LayerManagerOGL::CopyToTarget()
{
  nsIntRect rect;
  mWidget->GetBounds(rect);
  GLint width = rect.width;
  GLint height = rect.height;

  if ((PRInt64(width) * PRInt64(height) * PRInt64(4)) > PR_INT32_MAX) {
    NS_ERROR("Widget size too big - integer overflow!");
    return;
  }

  nsRefPtr<gfxImageSurface> imageSurface =
    new gfxImageSurface(gfxIntSize(width, height),
                        gfxASurface::ImageFormatARGB32);

#ifdef USE_GLES2
  // GLES2 promises that binding to any custom FBO will attach 
  // to GL_COLOR_ATTACHMENT0 attachment point.
  mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER,
                               mGLContext->IsDoubleBuffered() ? 0 : mBackBufferFBO);
#else
  mGLContext->fReadBuffer(LOCAL_GL_COLOR_ATTACHMENT0);
#endif

  GLenum format = LOCAL_GL_RGBA;
  if (mHasBGRA)
    format = LOCAL_GL_BGRA;

  NS_ASSERTION(imageSurface->Stride() == width * 4,
               "Image Surfaces being created with weird stride!");

  PRUint32 currentPackAlignment = 0;
  mGLContext->fGetIntegerv(LOCAL_GL_PACK_ALIGNMENT, (GLint*)&currentPackAlignment);
  if (currentPackAlignment != 4) {
    mGLContext->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, 4);
  }

  mGLContext->fReadPixels(0, 0,
                          width, height,
                          format,
                          LOCAL_GL_UNSIGNED_BYTE,
                          imageSurface->Data());

  if (currentPackAlignment != 4) {
    mGLContext->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, currentPackAlignment);
  }

  if (!mHasBGRA) {
    // need to swap B and R bytes
    for (int j = 0; j < height; ++j) {
      PRUint32 *row = (PRUint32*) (imageSurface->Data() + imageSurface->Stride() * j);
      for (int i = 0; i < width; ++i) {
        *row = (*row & 0xff00ff00) | ((*row & 0xff) << 16) | ((*row & 0xff0000) >> 16);
        row++;
      }
    }
  }

  mTarget->SetOperator(gfxContext::OPERATOR_OVER);
  mTarget->SetSource(imageSurface);
  mTarget->Paint();
}

LayerManagerOGL::ProgramType LayerManagerOGL::sLayerProgramTypes[] = {
  LayerManagerOGL::RGBALayerProgramType,
  LayerManagerOGL::BGRALayerProgramType,
  LayerManagerOGL::RGBXLayerProgramType,
  LayerManagerOGL::BGRXLayerProgramType,
  LayerManagerOGL::RGBARectLayerProgramType,
  LayerManagerOGL::ColorLayerProgramType,
  LayerManagerOGL::YCbCrLayerProgramType
};

#define FOR_EACH_LAYER_PROGRAM(vname)                       \
  for (size_t lpindex = 0;                                  \
       lpindex < NS_ARRAY_LENGTH(sLayerProgramTypes);       \
       ++lpindex)                                           \
  {                                                         \
    LayerProgram *vname = static_cast<LayerProgram*>        \
      (mPrograms[sLayerProgramTypes[lpindex]]);             \
    do

#define FOR_EACH_LAYER_PROGRAM_END              \
    while (0);                                  \
  }                                             \

void
LayerManagerOGL::SetLayerProgramProjectionMatrix(const gfx3DMatrix& aMatrix)
{
  FOR_EACH_LAYER_PROGRAM(lp) {
    lp->Activate();
    lp->SetProjectionMatrix(aMatrix);
  } FOR_EACH_LAYER_PROGRAM_END
}

void
LayerManagerOGL::CreateFBOWithTexture(int aWidth, int aHeight,
                                      GLuint *aFBO, GLuint *aTexture)
{
  GLuint tex, fbo;

  mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);
  mGLContext->fGenTextures(1, &tex);
  mGLContext->fBindTexture(mFBOTextureTarget, tex);
  mGLContext->fTexImage2D(mFBOTextureTarget,
                          0,
                          LOCAL_GL_RGBA,
                          aWidth, aHeight,
                          0,
                          LOCAL_GL_RGBA,
                          LOCAL_GL_UNSIGNED_BYTE,
                          NULL);
  mGLContext->fTexParameteri(mFBOTextureTarget, LOCAL_GL_TEXTURE_MIN_FILTER,
                             LOCAL_GL_LINEAR);
  mGLContext->fTexParameteri(mFBOTextureTarget, LOCAL_GL_TEXTURE_MAG_FILTER,
                             LOCAL_GL_LINEAR);
  mGLContext->fBindTexture(mFBOTextureTarget, 0);

  mGLContext->fGenFramebuffers(1, &fbo);
  mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, fbo);
  mGLContext->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                    LOCAL_GL_COLOR_ATTACHMENT0,
                                    mFBOTextureTarget,
                                    tex,
                                    0);

  NS_ASSERTION(mGLContext->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER) ==
               LOCAL_GL_FRAMEBUFFER_COMPLETE, "Error setting up framebuffer.");

  *aFBO = fbo;
  *aTexture = tex;

  DEBUG_GL_ERROR_CHECK(gl());
}

void LayerOGL::ApplyFilter(gfxPattern::GraphicsFilter aFilter)
{
  if (aFilter == gfxPattern::FILTER_NEAREST) {
    gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_NEAREST);
    gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_NEAREST);
  } else {
    if (aFilter != gfxPattern::FILTER_GOOD) {
      NS_WARNING("Unsupported filter type!");
    }
    gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
    gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  }
}

} /* layers */
} /* mozilla */
