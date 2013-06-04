/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TextureHostOGL.h"
#include "CompositorOGL.h"
#include "mozilla/layers/ImageHost.h"
#include "mozilla/layers/ContentHost.h"
#include "mozilla/layers/CompositingRenderTargetOGL.h"
#include "mozilla/Preferences.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/PLayer.h"
#include "mozilla/layers/Effects.h"
#include "nsIWidget.h"
#include "FPSCounter.h"

#include "gfxUtils.h"

#include "GLContextProvider.h"

#include "nsIServiceManager.h"
#include "nsIConsoleService.h"

#include "gfxCrashReporterUtils.h"

#include "nsMathUtils.h"

#include "GeckoProfiler.h"
#include <algorithm>

#if MOZ_ANDROID_OMTC
#include "TexturePoolOGL.h"
#endif


namespace mozilla {

using namespace gfx;

namespace layers {

using namespace mozilla::gl;

static inline IntSize ns2gfxSize(const nsIntSize& s) {
  return IntSize(s.width, s.height);
}


void
FPSState::DrawFPS(TimeStamp aNow,
                  GLContext* context, ShaderProgramOGL* copyprog)
{
  int fps = int(mCompositionFps.AddFrameAndGetFps(aNow));
  int txnFps = int(mTransactionFps.GetFpsAt(aNow));

  GLint viewport[4];
  context->fGetIntegerv(LOCAL_GL_VIEWPORT, viewport);

  if (!mTexture) {
    // Bind the number of textures we need, in this case one.
    context->fGenTextures(1, &mTexture);
    context->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
    context->fTexParameteri(LOCAL_GL_TEXTURE_2D,LOCAL_GL_TEXTURE_MIN_FILTER,LOCAL_GL_NEAREST);
    context->fTexParameteri(LOCAL_GL_TEXTURE_2D,LOCAL_GL_TEXTURE_MAG_FILTER,LOCAL_GL_NEAREST);

    uint32_t text[] = {
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
  }

  struct Vertex2D {
    float x,y;
  };
  float oneOverVP2 = 1.0 / viewport[2];
  float oneOverVP3 = 1.0 / viewport[3];
  const Vertex2D vertices[] = {
    { -1.0f, 1.0f - 42.f * oneOverVP3 },
    { -1.0f, 1.0f},
    { -1.0f + 22.f * oneOverVP2, 1.0f - 42.f * oneOverVP3 },
    { -1.0f + 22.f * oneOverVP2, 1.0f },

    {  -1.0f + 22.f * oneOverVP2, 1.0f - 42.f * oneOverVP3 },
    {  -1.0f + 22.f * oneOverVP2, 1.0f },
    {  -1.0f + 44.f * oneOverVP2, 1.0f - 42.f * oneOverVP3 },
    {  -1.0f + 44.f * oneOverVP2, 1.0f },

    { -1.0f + 44.f * oneOverVP2, 1.0f - 42.f * oneOverVP3 },
    { -1.0f + 44.f * oneOverVP2, 1.0f },
    { -1.0f + 66.f * oneOverVP2, 1.0f - 42.f * oneOverVP3 },
    { -1.0f + 66.f * oneOverVP2, 1.0f }
  };

  const Vertex2D vertices2[] = {
    { -1.0f + 80.f * oneOverVP2, 1.0f - 42.f * oneOverVP3 },
    { -1.0f + 80.f * oneOverVP2, 1.0f },
    { -1.0f + 102.f * oneOverVP2, 1.0f - 42.f * oneOverVP3 },
    { -1.0f + 102.f * oneOverVP2, 1.0f },

    { -1.0f + 102.f * oneOverVP2, 1.0f - 42.f * oneOverVP3 },
    { -1.0f + 102.f * oneOverVP2, 1.0f },
    { -1.0f + 124.f * oneOverVP2, 1.0f - 42.f * oneOverVP3 },
    { -1.0f + 124.f * oneOverVP2, 1.0f },

    { -1.0f + 124.f * oneOverVP2, 1.0f - 42.f * oneOverVP3 },
    { -1.0f + 124.f * oneOverVP2, 1.0f },
    { -1.0f + 146.f * oneOverVP2, 1.0f - 42.f * oneOverVP3 },
    { -1.0f + 146.f * oneOverVP2, 1.0f },
  };

  int v1   = fps % 10;
  int v10  = (fps % 100) / 10;
  int v100 = (fps % 1000) / 100;

  int txn1 = txnFps % 10;
  int txn10  = (txnFps % 100) / 10;
  int txn100 = (txnFps % 1000) / 100;

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

  const GLfloat texCoords2[] = {
    (txn100 * 4.f) / 64, 7.f / 8,
    (txn100 * 4.f) / 64, 0.0f,
    (txn100 * 4.f + 4) / 64, 7.f / 8,
    (txn100 * 4.f + 4) / 64, 0.0f,

    (txn10 * 4.f) / 64, 7.f / 8,
    (txn10 * 4.f) / 64, 0.0f,
    (txn10 * 4.f + 4) / 64, 7.f / 8,
    (txn10 * 4.f + 4) / 64, 0.0f,

    (txn1 * 4.f) / 64, 7.f / 8,
    (txn1 * 4.f) / 64, 0.0f,
    (txn1 * 4.f + 4) / 64, 7.f / 8,
    (txn1 * 4.f + 4) / 64, 0.0f,
  };

  // Turn necessary features on
  context->fEnable(LOCAL_GL_BLEND);
  context->fBlendFunc(LOCAL_GL_ONE, LOCAL_GL_SRC_COLOR);

  context->fActiveTexture(LOCAL_GL_TEXTURE0);
  context->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

  copyprog->Activate();
  copyprog->SetTextureUnit(0);

  // we're going to use client-side vertex arrays for this.
  context->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);

  // "COPY"
  context->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ZERO,
                              LOCAL_GL_ONE, LOCAL_GL_ZERO);

  // enable our vertex attribs; we'll call glVertexPointer below
  // to fill with the correct data.
  GLint vcattr = copyprog->AttribLocation(ShaderProgramOGL::VertexCoordAttrib);
  GLint tcattr = copyprog->AttribLocation(ShaderProgramOGL::TexCoordAttrib);

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

  context->fVertexAttribPointer(vcattr,
                                2, LOCAL_GL_FLOAT,
                                LOCAL_GL_FALSE,
                                0, vertices2);

  context->fVertexAttribPointer(tcattr,
                                2, LOCAL_GL_FLOAT,
                                LOCAL_GL_FALSE,
                                0, texCoords2);

  context->fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 12);
}

#ifdef CHECK_CURRENT_PROGRAM
int ShaderProgramOGL::sCurrentProgramKey = 0;
#endif

CompositorOGL::CompositorOGL(nsIWidget *aWidget, int aSurfaceWidth,
                             int aSurfaceHeight, bool aUseExternalSurfaceSize)
  : mWidget(aWidget)
  , mWidgetSize(-1, -1)
  , mSurfaceSize(aSurfaceWidth, aSurfaceHeight)
  , mHasBGRA(0)
  , mUseExternalSurfaceSize(aUseExternalSurfaceSize)
  , mFrameInProgress(false)
  , mDestroyed(false)
  , mTextures({0, 0, 0})
{
  MOZ_COUNT_CTOR(CompositorOGL);
  sBackend = LAYERS_OPENGL;
}

CompositorOGL::~CompositorOGL()
{
  MOZ_COUNT_DTOR(CompositorOGL);
  Destroy();
}

already_AddRefed<mozilla::gl::GLContext>
CompositorOGL::CreateContext()
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
    NS_WARNING("Failed to create CompositorOGL context");
  }
  return context.forget();
}

void
CompositorOGL::AddPrograms(ShaderProgramType aType)
{
  for (PRUint32 maskType = MaskNone; maskType < NumMaskTypes; ++maskType) {
    if (ProgramProfileOGL::ProgramExists(aType, static_cast<MaskType>(maskType))) {
      mPrograms[aType].mVariations[maskType] = new ShaderProgramOGL(this->gl(),
        ProgramProfileOGL::GetProfileFor(aType, static_cast<MaskType>(maskType)));
    } else {
      mPrograms[aType].mVariations[maskType] = nullptr;
    }
  }
}

GLuint
CompositorOGL::GetTemporaryTexture(GLenum aTextureUnit)
{
  if (!mTextures[aTextureUnit - LOCAL_GL_TEXTURE0]) {
    gl()->MakeCurrent();
    gl()->fGenTextures(1, &mTextures[aTextureUnit - LOCAL_GL_TEXTURE0]);
  }
  return mTextures[aTextureUnit - LOCAL_GL_TEXTURE0];
}

void
CompositorOGL::Destroy()
{
  if (gl()) {
    gl()->MakeCurrent();
    gl()->fDeleteTextures(3, mTextures);
    mTextures[0] = 0;
    mTextures[1] = 0;
    mTextures[2] = 0;
  } else {
    MOZ_ASSERT(!mTextures[0] && !mTextures[1] && !mTextures[2]);
  }

  if (!mDestroyed) {
    mDestroyed = true;
    CleanupResources();
  }
}

void
CompositorOGL::CleanupResources()
{
  if (!mGLContext)
    return;

  nsRefPtr<GLContext> ctx = mGLContext->GetSharedContext();
  if (!ctx) {
    ctx = mGLContext;
  }

  mPrograms.Clear();

  ctx->MakeCurrent();

  ctx->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

  if (mQuadVBO) {
    ctx->fDeleteBuffers(1, &mQuadVBO);
    mQuadVBO = 0;
  }

  mGLContext = nullptr;
}

// Impl of a a helper-runnable's "Run" method, used in Initialize()
NS_IMETHODIMP
CompositorOGL::ReadDrawFPSPref::Run()
{
  // NOTE: This must match the code in Initialize()'s NS_IsMainThread check.
  Preferences::AddBoolVarCache(&sDrawFPS, "layers.acceleration.draw-fps");
  Preferences::AddBoolVarCache(&sFrameCounter, "layers.acceleration.frame-counter");
  return NS_OK;
}

bool
CompositorOGL::Initialize()
{
  ScopedGfxFeatureReporter reporter("GL Layers", true);

  // Do not allow double initialization
  NS_ABORT_IF_FALSE(mGLContext == nullptr, "Don't reinitialize CompositorOGL");

  mGLContext = CreateContext();

#ifdef MOZ_WIDGET_ANDROID
  if (!mGLContext)
    NS_RUNTIMEABORT("We need a context on Android");
#endif

  if (!mGLContext)
    return false;

  mGLContext->SetFlipped(true);

  MakeCurrent();

  mHasBGRA =
    mGLContext->IsExtensionSupported(gl::GLContext::EXT_texture_format_BGRA8888) ||
    mGLContext->IsExtensionSupported(gl::GLContext::EXT_bgra);

  mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                 LOCAL_GL_ONE, LOCAL_GL_ONE);
  mGLContext->fEnable(LOCAL_GL_BLEND);

  mPrograms.AppendElements(NumProgramTypes);
  for (int type = 0; type < NumProgramTypes; ++type) {
    AddPrograms(static_cast<ShaderProgramType>(type));
  }

  // initialise a common shader to check that we can actually compile a shader
  if (!mPrograms[gl::RGBALayerProgramType].mVariations[MaskNone]->Initialize()) {
    return false;
  }

  if (mGLContext->WorkAroundDriverBugs()) {
    /**
    * We'll test the ability here to bind NPOT textures to a framebuffer, if
    * this fails we'll try ARB_texture_rectangle.
    */

    GLenum textureTargets[] = {
      LOCAL_GL_TEXTURE_2D,
      LOCAL_GL_NONE
    };

    if (mGLContext->IsGLES2()) {
        textureTargets[1] = LOCAL_GL_TEXTURE_RECTANGLE_ARB;
    }

    mFBOTextureTarget = LOCAL_GL_NONE;

    GLuint testFBO = 0;
    mGLContext->fGenFramebuffers(1, &testFBO);
    GLuint testTexture = 0;

    for (PRUint32 i = 0; i < ArrayLength(textureTargets); i++) {
      GLenum target = textureTargets[i];
      if (!target)
          continue;

      mGLContext->fGenTextures(1, &testTexture);
      mGLContext->fBindTexture(target, testTexture);
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

      mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, testFBO);
      mGLContext->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                        LOCAL_GL_COLOR_ATTACHMENT0,
                                        target,
                                        testTexture,
                                        0);

      if (mGLContext->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER) ==
          LOCAL_GL_FRAMEBUFFER_COMPLETE)
      {
        mFBOTextureTarget = target;
        mGLContext->fDeleteTextures(1, &testTexture);
        break;
      }

      mGLContext->fDeleteTextures(1, &testTexture);
    }

    if (testFBO) {
      mGLContext->fDeleteFramebuffers(1, &testFBO);
    }

    if (mFBOTextureTarget == LOCAL_GL_NONE) {
      /* Unable to find a texture target that works with FBOs and NPOT textures */
      return false;
    }
  } else {
    // not trying to work around driver bugs, so TEXTURE_2D should just work
    mFBOTextureTarget = LOCAL_GL_TEXTURE_2D;
  }

  // back to default framebuffer, to avoid confusion
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

  if (NS_IsMainThread()) {
    // NOTE: This must match the code in ReadDrawFPSPref::Run().
    Preferences::AddBoolVarCache(&sDrawFPS, "layers.acceleration.draw-fps");
    Preferences::AddBoolVarCache(&sFrameCounter, "layers.acceleration.frame-counter");
  } else {
    // We have to dispatch an event to the main thread to read the pref.
    NS_DispatchToMainThread(new ReadDrawFPSPref());
  }

  reporter.SetSuccessful();
  return true;
}

// |aTexCoordRect| is the rectangle from the texture that we want to
// draw using the given program.  The program already has a necessary
// offset and scale, so the geometry that needs to be drawn is a unit
// square from 0,0 to 1,1.
//
// |aTexSize| is the actual size of the texture, as it can be larger
// than the rectangle given by |aTexCoordRect|.
void
CompositorOGL::BindAndDrawQuadWithTextureRect(ShaderProgramOGL *aProg,
                                              const Rect& aTexCoordRect,
                                              TextureSource *aTexture)
{
  NS_ASSERTION(aProg->HasInitialized(), "Shader program not correctly initialized");
  GLuint vertAttribIndex =
    aProg->AttribLocation(ShaderProgramOGL::VertexCoordAttrib);
  GLuint texCoordAttribIndex =
    aProg->AttribLocation(ShaderProgramOGL::TexCoordAttrib);
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

  GLenum wrapMode = aTexture->AsSourceOGL()->GetWrapMode();

  IntSize realTexSize = aTexture->GetSize();
  if (!mGLContext->CanUploadNonPowerOfTwo()) {
    realTexSize = IntSize(NextPowerOfTwo(realTexSize.width),
                          NextPowerOfTwo(realTexSize.height));
  }

  // We need to convert back to actual texels here to get proper behaviour with
  // our GL helper functions. Should fix this sometime.
  // I want to vomit.
  IntRect texCoordRect = IntRect(NS_roundf(aTexCoordRect.x * aTexture->GetSize().width),
                                 NS_roundf(aTexCoordRect.y * aTexture->GetSize().height),
                                 NS_roundf(aTexCoordRect.width * aTexture->GetSize().width),
                                 NS_roundf(aTexCoordRect.height * aTexture->GetSize().height));

  // This is fairly disgusting - if the texture should be flipped it will have a
  // negative height, in which case we un-invert the texture coords and pass the
  // flipped 'flag' to the functions below. We can't just use the inverted coords
  // because our GL funtions use an explicit flag.
  bool flipped = false;
  if (texCoordRect.height < 0) {
    flipped = true;
    texCoordRect.y = texCoordRect.YMost();
    texCoordRect.height = -texCoordRect.height;
  }

  if (wrapMode == LOCAL_GL_REPEAT) {
    rects.addRect(/* dest rectangle */
                  0.0f, 0.0f, 1.0f, 1.0f,
                  /* tex coords */
                  texCoordRect.x / GLfloat(realTexSize.width),
                  texCoordRect.y / GLfloat(realTexSize.height),
                  texCoordRect.XMost() / GLfloat(realTexSize.width),
                  texCoordRect.YMost() / GLfloat(realTexSize.height),
                  flipped);
  } else {
    nsIntRect tcRect(texCoordRect.x, texCoordRect.y,
                     texCoordRect.width, texCoordRect.height);
    GLContext::DecomposeIntoNoRepeatTriangles(tcRect,
                                              nsIntSize(realTexSize.width, realTexSize.height),
                                              rects, flipped);
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
CompositorOGL::PrepareViewport(const gfx::IntSize& aSize,
                               const gfxMatrix& aWorldTransform)
{
  // Set the viewport correctly.
  mGLContext->fViewport(0, 0, aSize.width, aSize.height);

  // We flip the view matrix around so that everything is right-side up; we're
  // drawing directly into the window's back buffer, so this keeps things
  // looking correct.
  // XXX: We keep track of whether the window size changed, so we could skip
  // this update if it hadn't changed since the last call. We will need to
  // track changes to aTransformPolicy and aWorldTransform for this to work
  // though.

  // Matrix to transform (0, 0, aWidth, aHeight) to viewport space (-1.0, 1.0,
  // 2, 2) and flip the contents.
  gfxMatrix viewMatrix;
  viewMatrix.Translate(-gfxPoint(1.0, -1.0));
  viewMatrix.Scale(2.0f / float(aSize.width), 2.0f / float(aSize.height));
  viewMatrix.Scale(1.0f, -1.0f);
  if (!mTarget) {
    viewMatrix.Translate(gfxPoint(mRenderOffset.x, mRenderOffset.y));
  }

  viewMatrix = aWorldTransform * viewMatrix;

  gfx3DMatrix matrix3d = gfx3DMatrix::From2D(viewMatrix);
  matrix3d._33 = 0.0f;

  SetLayerProgramProjectionMatrix(matrix3d);
}

void
CompositorOGL::SetLayerProgramProjectionMatrix(const gfx3DMatrix& aMatrix)
{
  for (unsigned int i = 0; i < mPrograms.Length(); ++i) {
    for (PRUint32 mask = MaskNone; mask < NumMaskTypes; ++mask) {
      if (mPrograms[i].mVariations[mask]) {
        mPrograms[i].mVariations[mask]->CheckAndSetProjectionMatrix(aMatrix);
      }
    }
  }
}

TemporaryRef<CompositingRenderTarget>
CompositorOGL::CreateRenderTarget(const IntRect &aRect, SurfaceInitMode aInit)
{
  GLuint tex = 0;
  GLuint fbo = 0;
  CreateFBOWithTexture(aRect, aInit, 0, &fbo, &tex);
  RefPtr<CompositingRenderTargetOGL> surface
    = new CompositingRenderTargetOGL(this, tex, fbo);
  surface->Initialize(IntSize(aRect.width, aRect.height), mFBOTextureTarget, aInit);
  return surface.forget();
}

TemporaryRef<CompositingRenderTarget>
CompositorOGL::CreateRenderTargetFromSource(const IntRect &aRect,
                                            const CompositingRenderTarget *aSource)
{
  GLuint tex = 0;
  GLuint fbo = 0;
  const CompositingRenderTargetOGL* sourceSurface
    = static_cast<const CompositingRenderTargetOGL*>(aSource);
  if (aSource) {
    CreateFBOWithTexture(aRect, INIT_MODE_COPY, sourceSurface->GetFBO(),
                         &fbo, &tex);
  } else {
    CreateFBOWithTexture(aRect, INIT_MODE_COPY, 0,
                         &fbo, &tex);
  }

  RefPtr<CompositingRenderTargetOGL> surface
    = new CompositingRenderTargetOGL(this, tex, fbo);
  surface->Initialize(IntSize(aRect.width, aRect.height),
                      mFBOTextureTarget,
                      INIT_MODE_COPY);
  return surface.forget();
}

void
CompositorOGL::SetRenderTarget(CompositingRenderTarget *aSurface)
{
  MOZ_ASSERT(aSurface);
  CompositingRenderTargetOGL* surface
    = static_cast<CompositingRenderTargetOGL*>(aSurface);
  if (mCurrentRenderTarget != surface) {
    surface->BindRenderTarget();
    mCurrentRenderTarget = surface;
  }
}

CompositingRenderTarget*
CompositorOGL::GetCurrentRenderTarget()
{
  return mCurrentRenderTarget;
}

static GLenum
GetFrameBufferInternalFormat(GLContext* gl,
                             GLuint aFrameBuffer,
                             nsIWidget* aWidget)
{
  if (aFrameBuffer == 0) { // default framebuffer
    return aWidget->GetGLFrameBufferFormat();
  }
  return LOCAL_GL_RGBA;
}

bool CompositorOGL::sDrawFPS = false;
bool CompositorOGL::sFrameCounter = false;

static uint16_t sFrameCount = 0;
void
FPSState::DrawFrameCounter(GLContext* context)
{
  profiler_set_frame_number(sFrameCount);

  context->fEnable(LOCAL_GL_SCISSOR_TEST);

  uint16_t frameNumber = sFrameCount;
  for (size_t i = 0; i < 16; i++) {
    context->fScissor(3*i, 0, 3, 3);

    // We should do this using a single draw call
    // instead of 16 glClear()
    if ((frameNumber >> i) & 0x1) {
      context->fClearColor(0.0, 0.0, 0.0, 0.0);
    } else {
      context->fClearColor(1.0, 1.0, 1.0, 0.0);
    }
    context->fClear(LOCAL_GL_COLOR_BUFFER_BIT);
  }
  // We intentionally overflow at 2^16.
  sFrameCount++;

  context->fDisable(LOCAL_GL_SCISSOR_TEST);
}

/*
 * Returns a size that is larger than and closest to aSize where both
 * width and height are powers of two.
 * If the OpenGL setup is capable of using non-POT textures, then it
 * will just return aSize.
 */
static IntSize
CalculatePOTSize(const IntSize& aSize, GLContext* gl)
{
  if (gl->CanUploadNonPowerOfTwo())
    return aSize;

  return IntSize(NextPowerOfTwo(aSize.width), NextPowerOfTwo(aSize.height));
}

void
CompositorOGL::BeginFrame(const Rect *aClipRectIn, const gfxMatrix& aTransform,
                          const Rect& aRenderBounds, Rect *aClipRectOut,
                          Rect *aRenderBoundsOut)
{
  MOZ_ASSERT(!mFrameInProgress, "frame still in progress (should have called EndFrame or AbortFrame");

  mFrameInProgress = true;
  gfxRect rect;
  if (mUseExternalSurfaceSize) {
    rect = gfxRect(0, 0, mSurfaceSize.width, mSurfaceSize.height);
  } else {
    rect = gfxRect(aRenderBounds.x, aRenderBounds.y, aRenderBounds.width, aRenderBounds.height);
    // If render bounds is not updated explicitly, try to infer it from widget
    if (rect.width == 0 || rect.height == 0) {
      // FIXME/bug XXXXXX this races with rotation changes on the main
      // thread, and undoes all the care we take with layers txns being
      // sent atomically with rotation changes
      nsIntRect intRect;
      mWidget->GetClientBounds(intRect);
      rect = gfxRect(0, 0, intRect.width, intRect.height);
    }
  }

  rect = aTransform.TransformBounds(rect);
  if (aRenderBoundsOut) {
    *aRenderBoundsOut = Rect(rect.x, rect.y, rect.width, rect.height);
  }

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
    MakeCurrent(ForceMakeCurrent);

    mWidgetSize.width = width;
    mWidgetSize.height = height;
  } else {
    MakeCurrent();
  }

#if MOZ_ANDROID_OMTC
  TexturePoolOGL::Fill(gl());
#endif

  mCurrentRenderTarget = CompositingRenderTargetOGL::RenderTargetForWindow(this,
                            IntSize(width, height),
                            aTransform);
  mCurrentRenderTarget->BindRenderTarget();
#ifdef DEBUG
  mWindowRenderTarget = mCurrentRenderTarget;
#endif

  // Default blend function implements "OVER"
  mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                 LOCAL_GL_ONE, LOCAL_GL_ONE);
  mGLContext->fEnable(LOCAL_GL_BLEND);

  if (!aClipRectIn) {
    mGLContext->fScissor(0, 0, width, height);
    if (aClipRectOut) {
      aClipRectOut->SetRect(0, 0, width, height);
    }
  } else {
    mGLContext->fScissor(aClipRectIn->x, aClipRectIn->y, aClipRectIn->width, aClipRectIn->height);
  }

  mGLContext->fEnable(LOCAL_GL_SCISSOR_TEST);

  // If the Android compositor is being used, this clear will be done in
  // DrawWindowUnderlay. Make sure the bits used here match up with those used
  // in mobile/android/base/gfx/LayerRenderer.java
#ifndef MOZ_ANDROID_OMTC
  mGLContext->fClearColor(0.0, 0.0, 0.0, 0.0);
  mGLContext->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT);
#endif
}

void
CompositorOGL::CreateFBOWithTexture(const IntRect& aRect, SurfaceInitMode aInit,
                                    GLuint aSourceFrameBuffer,
                                    GLuint *aFBO, GLuint *aTexture)
{
  GLuint tex, fbo;

  mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);
  mGLContext->fGenTextures(1, &tex);
  mGLContext->fBindTexture(mFBOTextureTarget, tex);

  if (aInit == INIT_MODE_COPY) {
    GLuint curFBO = mCurrentRenderTarget->GetFBO();
    if (curFBO != aSourceFrameBuffer) {
      mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, aSourceFrameBuffer);
    }

    // We're going to create an RGBA temporary fbo.  But to
    // CopyTexImage() from the current framebuffer, the framebuffer's
    // format has to be compatible with the new texture's.  So we
    // check the format of the framebuffer here and take a slow path
    // if it's incompatible.
    GLenum format =
      GetFrameBufferInternalFormat(gl(), aSourceFrameBuffer, mWidget);

    bool isFormatCompatibleWithRGBA
        = gl()->IsGLES2() ? (format == LOCAL_GL_RGBA)
                          : true;

    if (isFormatCompatibleWithRGBA) {
      mGLContext->fCopyTexImage2D(mFBOTextureTarget,
                                  0,
                                  LOCAL_GL_RGBA,
                                  aRect.x, aRect.y,
                                  aRect.width, aRect.height,
                                  0);
    } else {
      // Curses, incompatible formats.  Take a slow path.

      // RGBA
      size_t bufferSize = aRect.width * aRect.height * 4;
      nsAutoArrayPtr<uint8_t> buf(new uint8_t[bufferSize]);

      mGLContext->fReadPixels(aRect.x, aRect.y,
                              aRect.width, aRect.height,
                              LOCAL_GL_RGBA,
                              LOCAL_GL_UNSIGNED_BYTE,
                              buf);
      mGLContext->fTexImage2D(mFBOTextureTarget,
                              0,
                              LOCAL_GL_RGBA,
                              aRect.width, aRect.height,
                              0,
                              LOCAL_GL_RGBA,
                              LOCAL_GL_UNSIGNED_BYTE,
                              buf);
    }
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

  *aFBO = fbo;
  *aTexture = tex;
}

gl::ShaderProgramType
CompositorOGL::GetProgramTypeForEffect(Effect *aEffect) const
{
  switch(aEffect->mType) {
  case EFFECT_SOLID_COLOR:
    return gl::ColorLayerProgramType;
  case EFFECT_RGBA:
  case EFFECT_RGBX:
  case EFFECT_BGRA:
  case EFFECT_BGRX:
  {
    TexturedEffect* texturedEffect =
        static_cast<TexturedEffect*>(aEffect);
    TextureSourceOGL* source = texturedEffect->mTexture->AsSourceOGL();
    return source->GetShaderProgram();
  }
  case EFFECT_YCBCR:
    return gl::YCbCrLayerProgramType;
  case EFFECT_RENDER_TARGET:
    return GetFBOLayerProgramType();
  default:
    return gl::RGBALayerProgramType;
  }
}

void
CompositorOGL::DrawQuad(const Rect& aRect, const Rect& aClipRect,
                        const EffectChain &aEffectChain,
                        Float aOpacity, const gfx::Matrix4x4 &aTransform,
                        const Point& aOffset)
{
  MOZ_ASSERT(mFrameInProgress, "frame not started");

  IntRect intClipRect;
  aClipRect.ToIntRect(&intClipRect);
  mGLContext->PushScissorRect(nsIntRect(intClipRect.x, intClipRect.y,
                                        intClipRect.width, intClipRect.height));

  MaskType maskType;
  EffectMask* effectMask;
  TextureSourceOGL* sourceMask = nullptr;
  gfx::Matrix4x4 maskQuadTransform;
  if (aEffectChain.mSecondaryEffects[EFFECT_MASK]) {
    effectMask = static_cast<EffectMask*>(aEffectChain.mSecondaryEffects[EFFECT_MASK].get());
    sourceMask = effectMask->mMaskTexture->AsSourceOGL();

    // NS_ASSERTION(textureMask->IsAlpha(),
    //              "OpenGL mask layers must be backed by alpha surfaces");

    // We're assuming that the gl backend won't cheat and use NPOT
    // textures when glContext says it can't (which seems to happen
    // on a mac when you force POT textures)
    IntSize maskSize = CalculatePOTSize(effectMask->mSize, mGLContext);

    const gfx::Matrix4x4& maskTransform = effectMask->mMaskTransform;
    NS_ASSERTION(maskTransform.Is2D(), "How did we end up with a 3D transform here?!");
    Rect bounds = Rect(Point(), Size(maskSize));
    bounds = maskTransform.As2D().TransformBounds(bounds);

    maskQuadTransform._11 = 1.0f/bounds.width;
    maskQuadTransform._22 = 1.0f/bounds.height;
    maskQuadTransform._41 = float(-bounds.x)/bounds.width;
    maskQuadTransform._42 = float(-bounds.y)/bounds.height;

    maskType = effectMask->mIs3D
                 ? Mask3d
                 : Mask2d;
  } else {
    maskType = MaskNone;
  }

  gl::ShaderProgramType programType = GetProgramTypeForEffect(aEffectChain.mPrimaryEffect);
  ShaderProgramOGL *program = GetProgram(programType, maskType);
  program->Activate();
  if (programType == gl::RGBARectLayerProgramType) {
    TexturedEffect* texturedEffect =
        static_cast<TexturedEffect*>(aEffectChain.mPrimaryEffect.get());
    TextureSourceOGL* source = texturedEffect->mTexture->AsSourceOGL();
    // This is used by IOSurface that use 0,0...w,h coordinate rather then 0,0..1,1.
    program->SetTexCoordMultiplier(source->GetSize().width, source->GetSize().height);
  }
  program->SetLayerQuadRect(aRect);
  program->SetLayerTransform(aTransform);
  program->SetRenderOffset(aOffset.x, aOffset.y);

  switch (aEffectChain.mPrimaryEffect->mType) {
    case EFFECT_SOLID_COLOR: {
      EffectSolidColor* effectSolidColor =
        static_cast<EffectSolidColor*>(aEffectChain.mPrimaryEffect.get());

      Color color = effectSolidColor->mColor;
      /* Multiply color by the layer opacity, as the shader
       * ignores layer opacity and expects a final color to
       * write to the color buffer.  This saves a needless
       * multiply in the fragment shader.
       */
      Float opacity = aOpacity * color.a;
      color.r *= opacity;
      color.g *= opacity;
      color.b *= opacity;
      color.a = opacity;

      program->SetRenderColor(color);

      if (maskType != MaskNone) {
        sourceMask->BindTexture(LOCAL_GL_TEXTURE0);
        program->SetMaskTextureUnit(0);
        program->SetMaskLayerTransform(maskQuadTransform);
      }

      BindAndDrawQuad(program);
    }
    break;

  case EFFECT_BGRA:
  case EFFECT_BGRX:
  case EFFECT_RGBA:
  case EFFECT_RGBX: {
      TexturedEffect* texturedEffect =
          static_cast<TexturedEffect*>(aEffectChain.mPrimaryEffect.get());
      Rect textureCoords;
      TextureSource *source = texturedEffect->mTexture;

      if (!texturedEffect->mPremultiplied) {
        mGLContext->fBlendFuncSeparate(LOCAL_GL_SRC_ALPHA, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                       LOCAL_GL_ONE, LOCAL_GL_ONE);
      }

      source->AsSourceOGL()->BindTexture(LOCAL_GL_TEXTURE0);
      if (programType == gl::RGBALayerExternalProgramType) {
        program->SetTextureTransform(source->AsSourceOGL()->GetTextureTransform());
      }

      mGLContext->ApplyFilterToBoundTexture(source->AsSourceOGL()->GetTextureTarget(),
                                            ThebesFilter(texturedEffect->mFilter));

      program->SetTextureUnit(0);
      program->SetLayerOpacity(aOpacity);

      if (maskType != MaskNone) {
        mGLContext->fActiveTexture(LOCAL_GL_TEXTURE1);
        sourceMask->BindTexture(LOCAL_GL_TEXTURE1);
        program->SetMaskTextureUnit(1);
        program->SetMaskLayerTransform(maskQuadTransform);
      }

      BindAndDrawQuadWithTextureRect(program, texturedEffect->mTextureCoords, source);

      if (!texturedEffect->mPremultiplied) {
        mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                       LOCAL_GL_ONE, LOCAL_GL_ONE);
      }
    }
    break;
  case EFFECT_YCBCR: {
      EffectYCbCr* effectYCbCr =
        static_cast<EffectYCbCr*>(aEffectChain.mPrimaryEffect.get());
      TextureSource* sourceYCbCr = effectYCbCr->mTexture;
      const int Y = 0, Cb = 1, Cr = 2;
      TextureSourceOGL* sourceY =  sourceYCbCr->GetSubSource(Y)->AsSourceOGL();
      TextureSourceOGL* sourceCb = sourceYCbCr->GetSubSource(Cb)->AsSourceOGL();
      TextureSourceOGL* sourceCr = sourceYCbCr->GetSubSource(Cr)->AsSourceOGL();

      if (!sourceY && !sourceCb && !sourceCr) {
        NS_WARNING("Invalid layer texture.");
        return;
      }

      gfxPattern::GraphicsFilter filter = ThebesFilter(effectYCbCr->mFilter);

      sourceY->BindTexture(LOCAL_GL_TEXTURE0);
      mGLContext->ApplyFilterToBoundTexture(filter);
      sourceCb->BindTexture(LOCAL_GL_TEXTURE1);
      mGLContext->ApplyFilterToBoundTexture(filter);
      sourceCr->BindTexture(LOCAL_GL_TEXTURE2);
      mGLContext->ApplyFilterToBoundTexture(filter);

      program->SetYCbCrTextureUnits(Y, Cb, Cr);
      program->SetLayerOpacity(aOpacity);

      if (maskType != MaskNone) {
        sourceMask->BindTexture(LOCAL_GL_TEXTURE3);
        program->SetMaskTextureUnit(3);
        program->SetMaskLayerTransform(maskQuadTransform);
      }
      BindAndDrawQuadWithTextureRect(program, effectYCbCr->mTextureCoords, sourceYCbCr->GetSubSource(Y));
    }
    break;
  case EFFECT_RENDER_TARGET: {
      EffectRenderTarget* effectRenderTarget =
        static_cast<EffectRenderTarget*>(aEffectChain.mPrimaryEffect.get());
      RefPtr<CompositingRenderTargetOGL> surface
        = static_cast<CompositingRenderTargetOGL*>(effectRenderTarget->mRenderTarget.get());

      ShaderProgramOGL *program = GetProgram(GetFBOLayerProgramType(), maskType);

      surface->BindTexture(LOCAL_GL_TEXTURE0, mFBOTextureTarget);

      program->Activate();
      program->SetTextureUnit(0);
      program->SetLayerOpacity(aOpacity);

      if (maskType != MaskNone) {
        sourceMask->BindTexture(LOCAL_GL_TEXTURE1);
        program->SetMaskTextureUnit(1);
        program->SetMaskLayerTransform(maskQuadTransform);
      }

      if (program->GetTexCoordMultiplierUniformLocation() != -1) {
        // 2DRect case, get the multiplier right for a sampler2DRect
        program->SetTexCoordMultiplier(aRect.width, aRect.height);
      }

      // Drawing is always flipped, but when copying between surfaces we want to avoid
      // this. Pass true for the flip parameter to introduce a second flip
      // that cancels the other one out.
      BindAndDrawQuad(program, true);
    }
    break;
  case EFFECT_COMPONENT_ALPHA: {
      EffectComponentAlpha* effectComponentAlpha =
        static_cast<EffectComponentAlpha*>(aEffectChain.mPrimaryEffect.get());
      TextureSourceOGL* sourceOnWhite = effectComponentAlpha->mOnWhite->AsSourceOGL();
      TextureSourceOGL* sourceOnBlack = effectComponentAlpha->mOnBlack->AsSourceOGL();

      if (!sourceOnBlack->IsValid() ||
          !sourceOnWhite->IsValid()) {
        NS_WARNING("Invalid layer texture for component alpha");
        return;
      }

      for (PRInt32 pass = 1; pass <=2; ++pass) {
        ShaderProgramOGL* program;
        if (pass == 1) {
          program = GetProgram(gl::ComponentAlphaPass1ProgramType, maskType);
          gl()->fBlendFuncSeparate(LOCAL_GL_ZERO, LOCAL_GL_ONE_MINUS_SRC_COLOR,
                                   LOCAL_GL_ONE, LOCAL_GL_ONE);
        } else {
          program = GetProgram(gl::ComponentAlphaPass2ProgramType, maskType);
          gl()->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE,
                                   LOCAL_GL_ONE, LOCAL_GL_ONE);
        }

        sourceOnBlack->BindTexture(LOCAL_GL_TEXTURE0);
        sourceOnWhite->BindTexture(LOCAL_GL_TEXTURE1);

        program->Activate();
        program->SetBlackTextureUnit(0);
        program->SetWhiteTextureUnit(1);
        program->SetLayerOpacity(aOpacity);
        program->SetLayerTransform(aTransform);
        program->SetRenderOffset(aOffset.x, aOffset.y);
        program->SetLayerQuadRect(aRect);
        if (maskType != MaskNone) {
          sourceMask->BindTexture(LOCAL_GL_TEXTURE2);
          program->SetMaskTextureUnit(2);
          program->SetMaskLayerTransform(maskQuadTransform);
        }

        BindAndDrawQuadWithTextureRect(program, effectComponentAlpha->mTextureCoords, effectComponentAlpha->mOnBlack);

        mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                       LOCAL_GL_ONE, LOCAL_GL_ONE);
      }
    }
    break;
  default:
    MOZ_ASSERT(false, "Unhandled effect type");
    break;
  }

  mGLContext->PopScissorRect();
  mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);
  // in case rendering has used some other GL context
  MakeCurrent();
}

void
CompositorOGL::EndFrame()
{
  MOZ_ASSERT(mCurrentRenderTarget == mWindowRenderTarget, "Rendering target not properly restored");

#ifdef MOZ_DUMP_PAINTING
  if (gfxUtils::sDumpPainting) {
    nsIntRect rect;
    if (mUseExternalSurfaceSize) {
      rect = nsIntRect(0, 0, mSurfaceSize.width, mSurfaceSize.height);
    } else {
      mWidget->GetBounds(rect);
    }
    nsRefPtr<gfxASurface> surf = gfxPlatform::GetPlatform()->CreateOffscreenSurface(rect.Size(), gfxASurface::CONTENT_COLOR_ALPHA);
    nsRefPtr<gfxContext> ctx = new gfxContext(surf);
    CopyToTarget(ctx, mCurrentRenderTarget->GetTransform());

    WriteSnapshotToDumpFile(this, surf);
  }
#endif

  mFrameInProgress = false;

  if (mTarget) {
    CopyToTarget(mTarget, mCurrentRenderTarget->GetTransform());
    mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
    mCurrentRenderTarget = nullptr;
    return;
  }

  mCurrentRenderTarget = nullptr;

  if (sDrawFPS && !mFPS) {
    mFPS = new FPSState();
  } else if (!sDrawFPS && mFPS) {
    mFPS = nullptr;
  }

  if (mFPS) {
    mFPS->DrawFPS(TimeStamp::Now(), mGLContext, GetProgram(Copy2DProgramType));
  } else if (sFrameCounter) {
    FPSState::DrawFrameCounter(mGLContext);
  }

  mGLContext->SwapBuffers();
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
}

void
CompositorOGL::EndFrameForExternalComposition(const gfxMatrix& aTransform)
{
  if (sDrawFPS) {
    if (!mFPS) {
      mFPS = new FPSState();
    }
    double fps = mFPS->mCompositionFps.AddFrameAndGetFps(TimeStamp::Now());
    printf_stderr("HWComposer: FPS is %g\n", fps);
  }

  // This lets us reftest and screenshot content rendered externally
  if (mTarget) {
    MakeCurrent();
    CopyToTarget(mTarget, aTransform);
    mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
  }
}

void
CompositorOGL::AbortFrame()
{
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
  mFrameInProgress = false;
  mCurrentRenderTarget = nullptr;
}

void
CompositorOGL::SetDestinationSurfaceSize(const gfx::IntSize& aSize)
{
  mSurfaceSize.width = aSize.width;
  mSurfaceSize.height = aSize.height;
}

void
CompositorOGL::CopyToTarget(gfxContext *aTarget, const gfxMatrix& aTransform)
{
  nsIntRect rect;
  if (mUseExternalSurfaceSize) {
    rect = nsIntRect(0, 0, mSurfaceSize.width, mSurfaceSize.height);
  } else {
    rect = nsIntRect(0, 0, mWidgetSize.width, mWidgetSize.height);
  }
  GLint width = rect.width;
  GLint height = rect.height;

  if ((PRInt64(width) * PRInt64(height) * PRInt64(4)) > PR_INT32_MAX) {
    NS_ERROR("Widget size too big - integer overflow!");
    return;
  }

  nsRefPtr<gfxImageSurface> imageSurface =
    new gfxImageSurface(gfxIntSize(width, height),
                        gfxASurface::ImageFormatARGB32);

  mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

  if (!mGLContext->IsGLES2()) {
    // GLES2 promises that binding to any custom FBO will attach
    // to GL_COLOR_ATTACHMENT0 attachment point.
    mGLContext->fReadBuffer(LOCAL_GL_BACK);
  }

  NS_ASSERTION(imageSurface->Stride() == width * 4,
               "Image Surfaces being created with weird stride!");

  mGLContext->ReadPixelsIntoImageSurface(imageSurface);

  // Map from GL space to Cairo space and reverse the world transform.
  gfxMatrix glToCairoTransform = aTransform;
  glToCairoTransform.Invert();
  glToCairoTransform.Scale(1.0, -1.0);
  glToCairoTransform.Translate(-gfxPoint(0.0, height));

  gfxContextAutoSaveRestore restore(aTarget);
  aTarget->SetOperator(gfxContext::OPERATOR_SOURCE);
  aTarget->SetMatrix(glToCairoTransform);
  aTarget->SetSource(imageSurface);
  aTarget->Paint();
}

double
CompositorOGL::AddFrameAndGetFps(const TimeStamp& timestamp)
{
  if (sDrawFPS) {
    if (!mFPS) {
      mFPS = new FPSState();
    }
    double fps = mFPS->mCompositionFps.AddFrameAndGetFps(timestamp);

    return fps;
  }

  return 0.;
}

void
CompositorOGL::NotifyLayersTransaction()
{
  if (mFPS) {
    mFPS->NotifyShadowTreeTransaction();
  }
}

void
CompositorOGL::Pause()
{
#ifdef MOZ_WIDGET_ANDROID
  gl()->ReleaseSurface();
#endif
}

bool
CompositorOGL::Resume()
{
#ifdef MOZ_WIDGET_ANDROID
  return gl()->RenewSurface();
#endif
  return true;
}


} /* layers */
} /* mozilla */
