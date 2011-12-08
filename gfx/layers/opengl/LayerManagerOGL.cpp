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

#include "mozilla/layers/PLayers.h"

/* This must occur *after* layers/PLayers.h to avoid typedefs conflicts. */
#include "mozilla/Util.h"

#include "LayerManagerOGL.h"
#include "ThebesLayerOGL.h"
#include "ContainerLayerOGL.h"
#include "ImageLayerOGL.h"
#include "ColorLayerOGL.h"
#include "CanvasLayerOGL.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Preferences.h"

#include "LayerManagerOGLShaders.h"

#include "gfxContext.h"
#include "nsIWidget.h"

#include "GLContext.h"
#include "GLContextProvider.h"

#include "nsIServiceManager.h"
#include "nsIConsoleService.h"

#include "gfxCrashReporterUtils.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gl;

#ifdef CHECK_CURRENT_PROGRAM
int LayerManagerOGLProgram::sCurrentProgramKey = 0;
#endif

/**
 * LayerManagerOGL
 */
LayerManagerOGL::LayerManagerOGL(nsIWidget *aWidget)
  : mWidget(aWidget)
  , mWidgetSize(-1, -1)
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

    mDestroyed = true;
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

already_AddRefed<mozilla::gl::GLContext>
LayerManagerOGL::CreateContext()
{
  nsRefPtr<GLContext> context;

#ifdef XP_WIN
  if (PR_GetEnv("MOZ_LAYERS_PREFER_EGL")) {
    printf_stderr("Trying GL layers...\n");
    context = gl::GLContextProviderEGL::CreateForWindow(mWidget);
  }
#endif

  if (!context)
    context = gl::GLContextProvider::CreateForWindow(mWidget);

  if (!context) {
    NS_WARNING("Failed to create LayerManagerOGL context");
  }
  return context.forget();
}

bool
LayerManagerOGL::Initialize(nsRefPtr<GLContext> aContext)
{
  ScopedGfxFeatureReporter reporter("GL Layers");

  // Do not allow double intiailization
  NS_ABORT_IF_FALSE(mGLContext == nsnull, "Don't reiniailize layer managers");

  if (!aContext)
    return false;

  mGLContext = aContext;
  mGLContext->SetFlipped(true);

  MakeCurrent();

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
      return false;                                                            \
    }                                                                             \
    mPrograms.AppendElement(p);                                                   \
  } while (0)


  // NOTE: Order matters here, and should be in the same order as the
  // ProgramType enum!
#ifdef DEBUG
  GLint programIndex = 0;
#endif

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
  SHADER_PROGRAM(ComponentAlphaPass1ProgramType, ComponentAlphaTextureLayerProgram,
                 sLayerVS, sComponentPass1FS);
  SHADER_PROGRAM(ComponentAlphaPass2ProgramType, ComponentAlphaTextureLayerProgram,
                 sLayerVS, sComponentPass2FS);
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

  for (PRUint32 i = 0; i < ArrayLength(textureTargets); i++) {
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
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);

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

  Preferences::AddBoolVarCache(&sDrawFPS, "layers.acceleration.draw-fps");

  reporter.SetSuccessful();
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
#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("[----- BeginTransaction"));
  Log();
#endif

  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }

  mTarget = aTarget;
}

bool
LayerManagerOGL::EndEmptyTransaction()
{
  if (!mRoot)
    return false;

  EndTransaction(nsnull, nsnull);
  return true;
}

void
LayerManagerOGL::EndTransaction(DrawThebesLayerCallback aCallback,
                                void* aCallbackData,
                                EndTransactionFlags aFlags)
{
#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("  ----- (beginning paint)"));
  Log();
#endif

  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }

  if (mRoot && !(aFlags & END_NO_IMMEDIATE_REDRAW)) {
    // The results of our drawing always go directly into a pixel buffer,
    // so we don't need to pass any global transform here.
    mRoot->ComputeEffectiveTransforms(gfx3DMatrix());

    mThebesLayerCallback = aCallback;
    mThebesLayerCallbackData = aCallbackData;

    Render();

    mThebesLayerCallback = nsnull;
    mThebesLayerCallbackData = nsnull;
  }

  mTarget = NULL;

#ifdef MOZ_LAYERS_HAVE_LOG
  Log();
  MOZ_LAYERS_LOG(("]----- EndTransaction"));
#endif
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

bool LayerManagerOGL::sDrawFPS = false;

/* This function tries to stick to portable C89 as much as possible
 * so that it can be easily copied into other applications */
void
LayerManagerOGL::FPSState::DrawFPS(GLContext* context, CopyProgram* copyprog)
{
  fcount++;

  int rate = 30;
  if (fcount >= rate) {
    TimeStamp now = TimeStamp::Now();
    TimeDuration duration = now - last;
    last = now;
    fps = rate / duration.ToSeconds() + .5;
    fcount = 0;
  }

  GLint viewport[4];
  context->fGetIntegerv(LOCAL_GL_VIEWPORT, viewport);

  static GLuint texture;
  if (!initialized) {
    // Bind the number of textures we need, in this case one.
    context->fGenTextures(1, &texture);
    context->fBindTexture(LOCAL_GL_TEXTURE_2D, texture);
    context->fTexParameteri(LOCAL_GL_TEXTURE_2D,LOCAL_GL_TEXTURE_MIN_FILTER,LOCAL_GL_NEAREST);
    context->fTexParameteri(LOCAL_GL_TEXTURE_2D,LOCAL_GL_TEXTURE_MAG_FILTER,LOCAL_GL_NEAREST);

    unsigned char text[] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0, 255, 255, 255,   0, 255, 255,   0,   0, 255, 255, 255,   0, 255, 255, 255,   0, 255,   0, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0,
      0, 255,   0, 255,   0,   0, 255,   0,   0,   0,   0, 255,   0,   0,   0, 255,   0, 255,   0, 255,   0, 255,   0,   0,   0, 255,   0,   0,   0,   0,   0, 255,   0, 255,   0, 255,   0, 255,   0, 255,   0,
      0, 255,   0, 255,   0,   0, 255,   0,   0, 255, 255, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0,   0,   0, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0,
      0, 255,   0, 255,   0,   0, 255,   0,   0, 255,   0,   0,   0,   0,   0, 255,   0,   0,   0, 255,   0,   0,   0, 255,   0, 255,   0, 255,   0,   0,   0, 255,   0, 255,   0, 255,   0,   0,   0, 255,   0,
      0, 255, 255, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0,   0,   0, 255,   0, 255, 255, 255,   0, 255, 255, 255,   0,   0,   0, 255,   0, 255, 255, 255,   0,   0,   0, 255,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    };

    // convert from 8 bit to 32 bit so that don't have to write the text above out in 32 bit format
    // we rely on int being 32 bits
    unsigned int* buf = (unsigned int*)malloc(64 * 8 * 4);
    for (int i = 0; i < 7; i++) {
      for (int j = 0; j < 41; j++) {
        unsigned int purple = 0xfff000ff;
        unsigned int white  = 0xffffffff;
        buf[i * 64 + j] = (text[i * 41 + j] == 0) ? purple : white;
      }
    }
    context->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_RGBA, 64, 8, 0, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, buf);
    free(buf);
    initialized = true;
  }

  struct Vertex2D {
    float x,y;
  };
  const Vertex2D vertices[] = {
    { -1.0f, 1.0f - 42.f / viewport[3] },
    { -1.0f, 1.0f},
    { -1.0f + 22.f / viewport[2], 1.0f - 42.f / viewport[3] },
    { -1.0f + 22.f / viewport[2], 1.0f },

    {  -1.0f + 22.f / viewport[2], 1.0f - 42.f / viewport[3] },
    {  -1.0f + 22.f / viewport[2], 1.0f },
    {  -1.0f + 44.f / viewport[2], 1.0f - 42.f / viewport[3] },
    {  -1.0f + 44.f / viewport[2], 1.0f },

    { -1.0f + 44.f / viewport[2], 1.0f - 42.f / viewport[3] },
    { -1.0f + 44.f / viewport[2], 1.0f },
    { -1.0f + 66.f / viewport[2], 1.0f - 42.f / viewport[3] },
    { -1.0f + 66.f / viewport[2], 1.0f }
  };

  int v1   = fps % 10;
  int v10  = (fps % 100) / 10;
  int v100 = (fps % 1000) / 100;

  // Feel free to comment these texture coordinates out and use one
  // of the ones below instead, or play around with your own values.
  const GLfloat texCoords[] = {
    (v100 * 4.f) / 64, 7.f / 8,
    (v100 * 4.f) / 64, 0.0f,
    (v100 * 4.f + 4) / 64, 7.f / 8,
    (v100 * 4.f + 4) / 64, 0.0f,

    (v10 * 4.f) / 64, 7.f / 8,
    (v10 * 4.f) / 64, 0.0f,
    (v10 * 4.f + 4) / 64, 7.f / 8,
    (v10 * 4.f + 4) / 64, 0.0f,

    (v1 * 4.f) / 64, 7.f / 8,
    (v1 * 4.f) / 64, 0.0f,
    (v1 * 4.f + 4) / 64, 7.f / 8,
    (v1 * 4.f + 4) / 64, 0.0f,
  };

  // Turn necessary features on
  context->fEnable(LOCAL_GL_BLEND);
  context->fBlendFunc(LOCAL_GL_ONE, LOCAL_GL_SRC_COLOR);

  context->fActiveTexture(LOCAL_GL_TEXTURE0);
  context->fBindTexture(LOCAL_GL_TEXTURE_2D, texture);

  copyprog->Activate();
  copyprog->SetTextureUnit(0);

  // we're going to use client-side vertex arrays for this.
  context->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);

  // "COPY"
  context->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ZERO,
                              LOCAL_GL_ONE, LOCAL_GL_ZERO);

  // enable our vertex attribs; we'll call glVertexPointer below
  // to fill with the correct data.
  GLint vcattr = copyprog->AttribLocation(CopyProgram::VertexCoordAttrib);
  GLint tcattr = copyprog->AttribLocation(CopyProgram::TexCoordAttrib);

  context->fEnableVertexAttribArray(vcattr);
  context->fEnableVertexAttribArray(tcattr);

  context->fVertexAttribPointer(vcattr,
                                2, LOCAL_GL_FLOAT,
                                LOCAL_GL_FALSE,
                                0, vertices);

  context->fVertexAttribPointer(tcattr,
                                2, LOCAL_GL_FLOAT,
                                LOCAL_GL_FALSE,
                                0, texCoords);

  context->fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 12);
}

// |aTexCoordRect| is the rectangle from the texture that we want to
// draw using the given program.  The program already has a necessary
// offset and scale, so the geometry that needs to be drawn is a unit
// square from 0,0 to 1,1.
//
// |aTexSize| is the actual size of the texture, as it can be larger
// than the rectangle given by |aTexCoordRect|.
void 
LayerManagerOGL::BindAndDrawQuadWithTextureRect(LayerProgram *aProg,
                                                const nsIntRect& aTexCoordRect,
                                                const nsIntSize& aTexSize,
                                                GLenum aWrapMode)
{
  GLuint vertAttribIndex =
    aProg->AttribLocation(LayerProgram::VertexAttrib);
  GLuint texCoordAttribIndex =
    aProg->AttribLocation(LayerProgram::TexCoordAttrib);
  NS_ASSERTION(texCoordAttribIndex != GLuint(-1), "no texture coords?");

  // clear any bound VBO so that glVertexAttribPointer() goes back to
  // "pointer mode"
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);

  // Given what we know about these textures and coordinates, we can
  // compute fmod(t, 1.0f) to get the same texture coordinate out.  If
  // the texCoordRect dimension is < 0 or > width/height, then we have
  // wraparound that we need to deal with by drawing multiple quads,
  // because we can't rely on full non-power-of-two texture support
  // (which is required for the REPEAT wrap mode).

  GLContext::RectTriangles rects;

  if (aWrapMode == LOCAL_GL_REPEAT) {
    rects.addRect(/* dest rectangle */
                  0.0f, 0.0f, 1.0f, 1.0f,
                  /* tex coords */
                  aTexCoordRect.x / GLfloat(aTexSize.width),
                  aTexCoordRect.y / GLfloat(aTexSize.height),
                  aTexCoordRect.XMost() / GLfloat(aTexSize.width),
                  aTexCoordRect.YMost() / GLfloat(aTexSize.height));
  } else {
    GLContext::DecomposeIntoNoRepeatTriangles(aTexCoordRect, aTexSize, rects);
  }

  mGLContext->fVertexAttribPointer(vertAttribIndex, 2,
                                   LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                                   rects.vertexPointer());

  mGLContext->fVertexAttribPointer(texCoordAttribIndex, 2,
                                   LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                                   rects.texCoordPointer());

  {
    mGLContext->fEnableVertexAttribArray(texCoordAttribIndex);
    {
      mGLContext->fEnableVertexAttribArray(vertAttribIndex);

      mGLContext->fDrawArrays(LOCAL_GL_TRIANGLES, 0, rects.elements());

      mGLContext->fDisableVertexAttribArray(vertAttribIndex);
    }
    mGLContext->fDisableVertexAttribArray(texCoordAttribIndex);
  }
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
  WorldTransformRect(rect);

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
    MakeCurrent(true);

    mWidgetSize.width = width;
    mWidgetSize.height = height;
  } else {
    MakeCurrent();
  }

  SetupBackBuffer(width, height);
  SetupPipeline(width, height, ApplyWorldTransform);

  // Default blend function implements "OVER"
  mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                 LOCAL_GL_ONE, LOCAL_GL_ONE);
  mGLContext->fEnable(LOCAL_GL_BLEND);

  const nsIntRect *clipRect = mRoot->GetClipRect();

  if (clipRect) {
    nsIntRect r = *clipRect;
    WorldTransformRect(r);
    mGLContext->fScissor(r.x, r.y, r.width, r.height);
  } else {
    mGLContext->fScissor(0, 0, width, height);
  }

  mGLContext->fEnable(LOCAL_GL_SCISSOR_TEST);

  mGLContext->fClearColor(0.0, 0.0, 0.0, 0.0);
  mGLContext->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT);

  // Render our layers.
  RootLayer()->RenderLayer(mGLContext->IsDoubleBuffered() ? 0 : mBackBufferFBO,
                           nsIntPoint(0, 0));
                           
  mWidget->DrawOver(this, rect);

  if (mTarget) {
    CopyToTarget();
    mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
    return;
  }

  if (sDrawFPS) {
    mFPS.DrawFPS(mGLContext, GetCopy2DProgram());
  }

  if (mGLContext->IsDoubleBuffered()) {
    mGLContext->SwapBuffers();
    mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
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
    nsIntRect cRect = *r; r = &cRect;
    WorldTransformRect(cRect);
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

    // Use flipped texture coordinates since our
    // projection matrix also has a flip and we
    // need to cancel that out.
    float coords[] = { left, bottom,
                       right, bottom,
                       left, top,
                       right, top };

    mGLContext->fVertexAttribPointer(vcattr,
                                     2, LOCAL_GL_FLOAT,
                                     LOCAL_GL_FALSE,
                                     0, vertices);

    mGLContext->fVertexAttribPointer(tcattr,
                                     2, LOCAL_GL_FLOAT,
                                     LOCAL_GL_FALSE,
                                     0, coords);

    mGLContext->fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);
  }

  mGLContext->fDisableVertexAttribArray(vcattr);
  mGLContext->fDisableVertexAttribArray(tcattr);

  mGLContext->fFlush();
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
}

void
LayerManagerOGL::SetWorldTransform(const gfxMatrix& aMatrix)
{
  NS_ASSERTION(aMatrix.PreservesAxisAlignedRectangles(),
               "SetWorldTransform only accepts matrices that satisfy PreservesAxisAlignedRectangles");
  NS_ASSERTION(!aMatrix.HasNonIntegerScale(),
               "SetWorldTransform only accepts matrices with integer scale");

  mWorldMatrix = aMatrix;
}

gfxMatrix&
LayerManagerOGL::GetWorldTransform(void)
{
  return mWorldMatrix;
}

void
LayerManagerOGL::WorldTransformRect(nsIntRect& aRect)
{
  gfxRect grect(aRect.x, aRect.y, aRect.width, aRect.height);
  grect = mWorldMatrix.TransformBounds(grect);
  aRect.SetRect(grect.X(), grect.Y(), grect.Width(), grect.Height());
}

void
LayerManagerOGL::SetupPipeline(int aWidth, int aHeight, WorldTransforPolicy aTransformPolicy)
{
  // Set the viewport correctly. 
  mGLContext->fViewport(0, 0, aWidth, aHeight);

  // We flip the view matrix around so that everything is right-side up; we're
  // drawing directly into the window's back buffer, so this keeps things
  // looking correct.
  //
  // XXX: We keep track of whether the window size changed, so we could skip
  // this update if it hadn't changed since the last call. We will need to
  // track changes to aTransformPolicy and mWorldMatrix for this to work
  // though.

  // Matrix to transform (0, 0, aWidth, aHeight) to viewport space (-1.0, 1.0,
  // 2, 2) and flip the contents.
  gfxMatrix viewMatrix; 
  viewMatrix.Translate(-gfxPoint(1.0, -1.0));
  viewMatrix.Scale(2.0f / float(aWidth), 2.0f / float(aHeight));
  viewMatrix.Scale(1.0f, -1.0f);

  if (aTransformPolicy == ApplyWorldTransform) {
    viewMatrix = mWorldMatrix * viewMatrix;
  }

  gfx3DMatrix matrix3d = gfx3DMatrix::From2D(viewMatrix);
  matrix3d._33 = 0.0f;

  SetLayerProgramProjectionMatrix(matrix3d);
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

  GLenum result = mGLContext->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
  if (result != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
    nsCAutoString msg;
    msg.Append("Framebuffer not complete -- error 0x");
    msg.AppendInt(result, 16);
    NS_RUNTIMEABORT(msg.get());
  }

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

  mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER,
                               mGLContext->IsDoubleBuffered() ? 0 : mBackBufferFBO);

#ifndef USE_GLES2
  // GLES2 promises that binding to any custom FBO will attach
  // to GL_COLOR_ATTACHMENT0 attachment point.
  if (mGLContext->IsDoubleBuffered()) {
    mGLContext->fReadBuffer(LOCAL_GL_BACK);
  }
  else {
    mGLContext->fReadBuffer(LOCAL_GL_COLOR_ATTACHMENT0);
  }
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

  mTarget->SetOperator(gfxContext::OPERATOR_SOURCE);
  mTarget->Scale(1.0, -1.0);
  mTarget->Translate(-gfxPoint(0.0, height));
  mTarget->SetSource(imageSurface);
  mTarget->Paint();
}

LayerManagerOGL::ProgramType LayerManagerOGL::sLayerProgramTypes[] = {
  gl::RGBALayerProgramType,
  gl::BGRALayerProgramType,
  gl::RGBXLayerProgramType,
  gl::BGRXLayerProgramType,
  gl::RGBARectLayerProgramType,
  gl::ColorLayerProgramType,
  gl::YCbCrLayerProgramType,
  gl::ComponentAlphaPass1ProgramType,
  gl::ComponentAlphaPass2ProgramType
};

#define FOR_EACH_LAYER_PROGRAM(vname)                       \
  for (size_t lpindex = 0;                                  \
       lpindex < ArrayLength(sLayerProgramTypes);           \
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
LayerManagerOGL::CreateFBOWithTexture(const nsIntRect& aRect, InitMode aInit,
                                      GLuint *aFBO, GLuint *aTexture)
{
  GLuint tex, fbo;

  mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);
  mGLContext->fGenTextures(1, &tex);
  mGLContext->fBindTexture(mFBOTextureTarget, tex);
  if (aInit == InitModeCopy) {
    mGLContext->fCopyTexImage2D(mFBOTextureTarget,
                                0,
                                LOCAL_GL_RGBA,
                                aRect.x, aRect.y,
                                aRect.width, aRect.height,
                                0);
  } else {
    mGLContext->fTexImage2D(mFBOTextureTarget,
                            0,
                            LOCAL_GL_RGBA,
                            aRect.width, aRect.height,
                            0,
                            LOCAL_GL_RGBA,
                            LOCAL_GL_UNSIGNED_BYTE,
                            NULL);
  }
  mGLContext->fTexParameteri(mFBOTextureTarget, LOCAL_GL_TEXTURE_MIN_FILTER,
                             LOCAL_GL_LINEAR);
  mGLContext->fTexParameteri(mFBOTextureTarget, LOCAL_GL_TEXTURE_MAG_FILTER,
                             LOCAL_GL_LINEAR);
  mGLContext->fTexParameteri(mFBOTextureTarget, LOCAL_GL_TEXTURE_WRAP_S, 
                             LOCAL_GL_CLAMP_TO_EDGE);
  mGLContext->fTexParameteri(mFBOTextureTarget, LOCAL_GL_TEXTURE_WRAP_T, 
                             LOCAL_GL_CLAMP_TO_EDGE);
  mGLContext->fBindTexture(mFBOTextureTarget, 0);

  mGLContext->fGenFramebuffers(1, &fbo);
  mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, fbo);
  mGLContext->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                    LOCAL_GL_COLOR_ATTACHMENT0,
                                    mFBOTextureTarget,
                                    tex,
                                    0);

  // Making this call to fCheckFramebufferStatus prevents a crash on
  // PowerVR. See bug 695246.
  GLenum result = mGLContext->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
  if (result != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
    nsCAutoString msg;
    msg.Append("Framebuffer not complete -- error 0x");
    msg.AppendInt(result, 16);
    NS_RUNTIMEABORT(msg.get());
  }

  SetupPipeline(aRect.width, aRect.height, DontApplyWorldTransform);
  mGLContext->fScissor(0, 0, aRect.width, aRect.height);

  if (aInit == InitModeClear) {
    mGLContext->fClearColor(0.0, 0.0, 0.0, 0.0);
    mGLContext->fClear(LOCAL_GL_COLOR_BUFFER_BIT);
  }

  *aFBO = fbo;
  *aTexture = tex;
}

already_AddRefed<ShadowThebesLayer>
LayerManagerOGL::CreateShadowThebesLayer()
{
  if (LayerManagerOGL::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nsnull;
  }
  return nsRefPtr<ShadowThebesLayerOGL>(new ShadowThebesLayerOGL(this)).forget();
}

already_AddRefed<ShadowContainerLayer>
LayerManagerOGL::CreateShadowContainerLayer()
{
  if (LayerManagerOGL::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nsnull;
  }
  return nsRefPtr<ShadowContainerLayerOGL>(new ShadowContainerLayerOGL(this)).forget();
}

already_AddRefed<ShadowImageLayer>
LayerManagerOGL::CreateShadowImageLayer()
{
  if (LayerManagerOGL::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nsnull;
  }
  return nsRefPtr<ShadowImageLayerOGL>(new ShadowImageLayerOGL(this)).forget();
}

already_AddRefed<ShadowColorLayer>
LayerManagerOGL::CreateShadowColorLayer()
{
  if (LayerManagerOGL::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nsnull;
  }
  return nsRefPtr<ShadowColorLayerOGL>(new ShadowColorLayerOGL(this)).forget();
}

already_AddRefed<ShadowCanvasLayer>
LayerManagerOGL::CreateShadowCanvasLayer()
{
  if (LayerManagerOGL::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nsnull;
  }
  return nsRefPtr<ShadowCanvasLayerOGL>(new ShadowCanvasLayerOGL(this)).forget();
}


} /* layers */
} /* mozilla */
