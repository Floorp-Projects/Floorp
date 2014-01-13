/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorOGL.h"
#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t, uint8_t
#include <stdlib.h>                     // for free, malloc
#include "FPSCounter.h"                 // for FPSState, FPSCounter
#include "GLContextProvider.h"          // for GLContextProvider
#include "GLContext.h"                  // for GLContext
#include "GLUploadHelpers.h"
#include "Layers.h"                     // for WriteSnapshotToDumpFile
#include "LayerScope.h"                 // for LayerScope
#include "gfx2DGlue.h"                  // for ThebesFilter
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "gfxASurface.h"                // for gfxASurface, etc
#include "gfxCrashReporterUtils.h"      // for ScopedGfxFeatureReporter
#include "gfxImageSurface.h"            // for gfxImageSurface
#include "gfxMatrix.h"                  // for gfxMatrix
#include "GraphicsFilter.h"             // for GraphicsFilter
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxRect.h"                    // for gfxRect
#include "gfxUtils.h"                   // for NextPowerOfTwo, gfxUtils, etc
#include "mozilla/ArrayUtils.h"         // for ArrayLength
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4, Matrix
#include "mozilla/layers/CompositingRenderTargetOGL.h"
#include "mozilla/layers/Effects.h"     // for EffectChain, TexturedEffect, etc
#include "mozilla/layers/TextureHost.h"  // for TextureSource, etc
#include "mozilla/layers/TextureHostOGL.h"  // for TextureSourceOGL, etc
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsAString.h"
#include "nsIConsoleService.h"          // for nsIConsoleService, etc
#include "nsIWidget.h"                  // for nsIWidget
#include "nsLiteralString.h"            // for NS_LITERAL_STRING
#include "nsMathUtils.h"                // for NS_roundf
#include "nsRect.h"                     // for nsIntRect
#include "nsServiceManagerUtils.h"      // for do_GetService
#include "nsString.h"                   // for nsString, nsAutoCString, etc
#include "DecomposeIntoNoRepeatTriangles.h"
#include "ScopedGLHelpers.h"
#include "GLReadTexImageHelper.h"

#if MOZ_ANDROID_OMTC
#include "TexturePoolOGL.h"
#endif
#include "GeckoProfiler.h"

#define BUFFER_OFFSET(i) ((char *)nullptr + (i))

namespace mozilla {

using namespace gfx;

namespace layers {

using namespace mozilla::gl;

static inline IntSize ns2gfxSize(const nsIntSize& s) {
  return IntSize(s.width, s.height);
}

static void
BindMaskForProgram(ShaderProgramOGL* aProgram, TextureSourceOGL* aSourceMask,
                   GLenum aTexUnit, const gfx::Matrix4x4& aTransform)
{
  MOZ_ASSERT(LOCAL_GL_TEXTURE0 <= aTexUnit && aTexUnit <= LOCAL_GL_TEXTURE31);
  aSourceMask->BindTexture(aTexUnit);
  aProgram->SetMaskTextureUnit(aTexUnit - LOCAL_GL_TEXTURE0);
  aProgram->SetMaskLayerTransform(aTransform);
}

// Draw the given quads with the already selected shader. Texture coordinates
// are supplied if the shader requires them.
static void
DrawQuads(GLContext *aGLContext,
          VBOArena &aVBOs,
          ShaderProgramOGL *aProg,
          RectTriangles &aRects)
{
  NS_ASSERTION(aProg->HasInitialized(), "Shader program not correctly initialized");
  GLuint vertAttribIndex =
    aProg->AttribLocation(ShaderProgramOGL::VertexCoordAttrib);
  GLuint texCoordAttribIndex =
    aProg->AttribLocation(ShaderProgramOGL::TexCoordAttrib);
  bool texCoords = (texCoordAttribIndex != GLuint(-1));

  GLsizei elements = aRects.elements();
  GLsizei bytes = elements * 2 * sizeof(GLfloat);

  GLsizei total = bytes;
  if (texCoords) {
    total *= 2;
  }

  aGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER,
                          aVBOs.Allocate(aGLContext));
  aGLContext->fBufferData(LOCAL_GL_ARRAY_BUFFER,
                          total,
                          nullptr,
                          LOCAL_GL_STREAM_DRAW);

  aGLContext->fBufferSubData(LOCAL_GL_ARRAY_BUFFER,
                             0,
                             bytes,
                             aRects.vertexPointer());
  aGLContext->fEnableVertexAttribArray(vertAttribIndex);
  aGLContext->fVertexAttribPointer(vertAttribIndex,
                                   2, LOCAL_GL_FLOAT,
                                   LOCAL_GL_FALSE,
                                   0, BUFFER_OFFSET(0));

  if (texCoords) {
    aGLContext->fBufferSubData(LOCAL_GL_ARRAY_BUFFER,
                               bytes,
                               bytes,
                               aRects.texCoordPointer());
    aGLContext->fEnableVertexAttribArray(texCoordAttribIndex);
    aGLContext->fVertexAttribPointer(texCoordAttribIndex,
                                     2, LOCAL_GL_FLOAT,
                                     LOCAL_GL_FALSE,
                                     0, BUFFER_OFFSET(bytes));
  }

  aGLContext->fDrawArrays(LOCAL_GL_TRIANGLES, 0, elements);

  aGLContext->fDisableVertexAttribArray(vertAttribIndex);
  if (texCoords) {
    aGLContext->fDisableVertexAttribArray(texCoordAttribIndex);
  }

  aGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
}

// Size of the builtin font.
static const float FontHeight = 7.f;
static const float FontWidth = 5.f;
static const float FontStride = 4.f;

// Scale the font when drawing it to the viewport for better readability.
static const float FontScaleX = 2.f;
static const float FontScaleY = 3.f;

// Size of the font texture (POT).
static const size_t FontTextureWidth = 64;
static const size_t FontTextureHeight = 8;

static void
AddDigits(RectTriangles &aRects,
          const gfx::IntSize aViewportSize,
          unsigned int aOffset,
          unsigned int aValue)
{
  unsigned int divisor = 100;
  for (size_t n = 0; n < 3; ++n) {
    gfxRect d(aOffset * FontWidth, 0.f, FontWidth, FontHeight);
    d.Scale(FontScaleX / aViewportSize.width, FontScaleY / aViewportSize.height);

    unsigned int digit = aValue % (divisor * 10) / divisor;
    gfxRect t(digit * FontStride, 0.f, FontWidth, FontHeight);
    t.Scale(1.f / FontTextureWidth, 1.f / FontTextureHeight);

    aRects.addRect(d.x, d.y, d.x + d.width, d.y + d.height,
                   t.x, t.y, t.x + t.width, t.y + t.height,
                   false);
    divisor /= 10;
    ++aOffset;
  }
}

void
FPSState::DrawFPS(TimeStamp aNow,
                  unsigned int aFillRatio,
                  GLContext* aContext,
                  ShaderProgramOGL* aProgram)
{
  if (!mTexture) {
    // Bind the number of textures we need, in this case one.
    aContext->fGenTextures(1, &mTexture);
    aContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
    aContext->fTexParameteri(LOCAL_GL_TEXTURE_2D,LOCAL_GL_TEXTURE_MIN_FILTER,LOCAL_GL_NEAREST);
    aContext->fTexParameteri(LOCAL_GL_TEXTURE_2D,LOCAL_GL_TEXTURE_MAG_FILTER,LOCAL_GL_NEAREST);

    const char *text =
      "                                         "
      " XXX XX  XXX XXX X X XXX XXX XXX XXX XXX "
      " X X  X    X   X X X X   X     X X X X X "
      " X X  X  XXX XXX XXX XXX XXX   X XXX XXX "
      " X X  X  X     X   X   X X X   X X X   X "
      " XXX XXX XXX XXX   X XXX XXX   X XXX   X "
      "                                         ";

    // Convert the text encoding above to RGBA.
    uint32_t* buf = (uint32_t *) malloc(64 * 8 * sizeof(uint32_t));
    for (int i = 0; i < 7; i++) {
      for (int j = 0; j < 41; j++) {
        uint32_t purple = 0xfff000ff;
        uint32_t white  = 0xffffffff;
        buf[i * 64 + j] = (text[i * 41 + j] == ' ') ? purple : white;
      }
    }
    aContext->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_RGBA, 64, 8, 0, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, buf);
    free(buf);
  }

  mVBOs.Reset();

  GLint viewport[4];
  aContext->fGetIntegerv(LOCAL_GL_VIEWPORT, viewport);
  gfx::IntSize viewportSize(viewport[2], viewport[3]);

  unsigned int fps = unsigned(mCompositionFps.AddFrameAndGetFps(aNow));
  unsigned int txnFps = unsigned(mTransactionFps.GetFpsAt(aNow));

  RectTriangles rects;
  AddDigits(rects, viewportSize, 0, fps);
  AddDigits(rects, viewportSize, 4, txnFps);
  AddDigits(rects, viewportSize, 8, aFillRatio);

  // Turn necessary features on
  aContext->fEnable(LOCAL_GL_BLEND);
  aContext->fBlendFunc(LOCAL_GL_ONE, LOCAL_GL_SRC_COLOR);

  aContext->fActiveTexture(LOCAL_GL_TEXTURE0);
  aContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

  aProgram->Activate();
  aProgram->SetTextureUnit(0);
  aProgram->SetLayerQuadRect(gfx::Rect(0.f, 0.f, viewport[2], viewport[3]));
  aProgram->SetLayerOpacity(1.f);
  aProgram->SetTextureTransform(gfx3DMatrix());
  aProgram->SetLayerTransform(gfx3DMatrix());
  aProgram->SetRenderOffset(0, 0);

  aContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ZERO,
                               LOCAL_GL_ONE, LOCAL_GL_ZERO);

  DrawQuads(aContext, mVBOs, aProgram, rects);
}

CompositorOGL::CompositorOGL(nsIWidget *aWidget, int aSurfaceWidth,
                             int aSurfaceHeight, bool aUseExternalSurfaceSize)
  : mWidget(aWidget)
  , mWidgetSize(-1, -1)
  , mSurfaceSize(aSurfaceWidth, aSurfaceHeight)
  , mHasBGRA(0)
  , mUseExternalSurfaceSize(aUseExternalSurfaceSize)
  , mFrameInProgress(false)
  , mDestroyed(false)
  , mHeight(0)
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
  for (uint32_t maskType = MaskNone; maskType < NumMaskTypes; ++maskType) {
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
  size_t index = aTextureUnit - LOCAL_GL_TEXTURE0;
  // lazily grow the array of temporary textures
  if (mTextures.Length() <= index) {
    size_t prevLength = mTextures.Length();
    mTextures.SetLength(index + 1);
    for(unsigned int i = prevLength; i <= index; ++i) {
      mTextures[i] = 0;
    }
  }
  // lazily initialize the temporary textures
  if (!mTextures[index]) {
    if (!gl()->MakeCurrent()) {
      return 0;
    }
    gl()->fGenTextures(1, &mTextures[index]);
  }
  return mTextures[index];
}

void
CompositorOGL::Destroy()
{
  if (gl() && gl()->MakeCurrent()) {
    if (mTextures.Length() > 0) {
      gl()->fDeleteTextures(mTextures.Length(), &mTextures[0]);
    }
    mVBOs.Flush(gl());
    if (mFPS) {
      if (mFPS->mTexture > 0)
        gl()->fDeleteTextures(1, &mFPS->mTexture);
      mFPS->mVBOs.Flush(gl());
    }
  }
  mTextures.SetLength(0);
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

  if (!ctx->MakeCurrent()) {
    mQuadVBO = 0;
    mGLContext = nullptr;
    return;
  }

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
  if (!mPrograms[RGBALayerProgramType].mVariations[MaskNone]->Initialize()) {
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

    for (uint32_t i = 0; i < ArrayLength(textureTargets); i++) {
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
                              nullptr);

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
      NS_LITERAL_STRING("OpenGL compositor Initialized Succesfully.\nVersion: ");
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
  } else {
    // We have to dispatch an event to the main thread to read the pref.
    NS_DispatchToMainThread(new ReadDrawFPSPref());
  }

  reporter.SetSuccessful();
  return true;
}

// |aTextureTransform| is the texture transform that will be set on
// aProg, possibly multiplied with another texture transform of our
// own.
// |aTexCoordRect| is the rectangle from the texture that we want to
// draw using the given program.  The program already has a necessary
// offset and scale, so the geometry that needs to be drawn is a unit
// square from 0,0 to 1,1.
//
// |aTexture| is the texture we are drawing. Its actual size can be
// larger than the rectangle given by |aTexCoordRect|.
void
CompositorOGL::BindAndDrawQuadWithTextureRect(ShaderProgramOGL *aProg,
                                              const gfx3DMatrix& aTextureTransform,
                                              const Rect& aTexCoordRect,
                                              TextureSource *aTexture)
{
  // Given what we know about these textures and coordinates, we can
  // compute fmod(t, 1.0f) to get the same texture coordinate out.  If
  // the texCoordRect dimension is < 0 or > width/height, then we have
  // wraparound that we need to deal with by drawing multiple quads,
  // because we can't rely on full non-power-of-two texture support
  // (which is required for the REPEAT wrap mode).

  RectTriangles rects;

  GLenum wrapMode = aTexture->AsSourceOGL()->GetWrapMode();

  IntSize realTexSize = aTexture->GetSize();
  if (!CanUploadNonPowerOfTwo(mGLContext)) {
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
    DecomposeIntoNoRepeatTriangles(tcRect,
                                   nsIntSize(realTexSize.width, realTexSize.height),
                                   rects, flipped);
  }

  gfx3DMatrix textureTransform;
  if (rects.IsSimpleQuad(textureTransform)) {
    aProg->SetTextureTransform(aTextureTransform * textureTransform);
    BindAndDrawQuad(aProg, false);
  } else {
    aProg->SetTextureTransform(aTextureTransform);
    DrawQuads(mGLContext, mVBOs, aProg, rects);
  }
}

void
CompositorOGL::PrepareViewport(const gfx::IntSize& aSize,
                               const gfxMatrix& aWorldTransform)
{
  // Set the viewport correctly.
  mGLContext->fViewport(0, 0, aSize.width, aSize.height);

  mHeight = aSize.height;

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
    for (uint32_t mask = MaskNone; mask < NumMaskTypes; ++mask) {
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
  CreateFBOWithTexture(aRect, false, 0, &fbo, &tex);
  RefPtr<CompositingRenderTargetOGL> surface
    = new CompositingRenderTargetOGL(this, aRect.TopLeft(), tex, fbo);
  surface->Initialize(aRect.Size(), mFBOTextureTarget, aInit);
  return surface.forget();
}

TemporaryRef<CompositingRenderTarget>
CompositorOGL::CreateRenderTargetFromSource(const IntRect &aRect,
                                            const CompositingRenderTarget *aSource,
                                            const IntPoint &aSourcePoint)
{
  GLuint tex = 0;
  GLuint fbo = 0;
  const CompositingRenderTargetOGL* sourceSurface
    = static_cast<const CompositingRenderTargetOGL*>(aSource);
  IntRect sourceRect(aSourcePoint, aRect.Size());
  if (aSource) {
    CreateFBOWithTexture(sourceRect, true, sourceSurface->GetFBO(),
                         &fbo, &tex);
  } else {
    CreateFBOWithTexture(sourceRect, true, 0,
                         &fbo, &tex);
  }

  RefPtr<CompositingRenderTargetOGL> surface
    = new CompositingRenderTargetOGL(this, aRect.TopLeft(), tex, fbo);
  surface->Initialize(aRect.Size(),
                      mFBOTextureTarget,
                      INIT_MODE_NONE);
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

/*
 * Returns a size that is larger than and closest to aSize where both
 * width and height are powers of two.
 * If the OpenGL setup is capable of using non-POT textures, then it
 * will just return aSize.
 */
static IntSize
CalculatePOTSize(const IntSize& aSize, GLContext* gl)
{
  if (CanUploadNonPowerOfTwo(gl))
    return aSize;

  return IntSize(NextPowerOfTwo(aSize.width), NextPowerOfTwo(aSize.height));
}

void
CompositorOGL::BeginFrame(const nsIntRegion& aInvalidRegion,
                          const Rect *aClipRectIn,
                          const gfxMatrix& aTransform,
                          const Rect& aRenderBounds,
                          Rect *aClipRectOut,
                          Rect *aRenderBoundsOut)
{
  PROFILER_LABEL("CompositorOGL", "BeginFrame");
  MOZ_ASSERT(!mFrameInProgress, "frame still in progress (should have called EndFrame or AbortFrame");

  LayerScope::BeginFrame(mGLContext, PR_Now());

  mVBOs.Reset();

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

  mPixelsPerFrame = width * height;
  mPixelsFilled = 0;

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

  if (aClipRectOut && !aClipRectIn) {
    aClipRectOut->SetRect(0, 0, width, height);
  }

  // If the Android compositor is being used, this clear will be done in
  // DrawWindowUnderlay. Make sure the bits used here match up with those used
  // in mobile/android/base/gfx/LayerRenderer.java
#ifndef MOZ_ANDROID_OMTC
  mGLContext->fClearColor(0.0, 0.0, 0.0, 0.0);
  mGLContext->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT);
#endif
}

void
CompositorOGL::CreateFBOWithTexture(const IntRect& aRect, bool aCopyFromSource,
                                    GLuint aSourceFrameBuffer,
                                    GLuint *aFBO, GLuint *aTexture)
{
  GLuint tex, fbo;

  mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);
  mGLContext->fGenTextures(1, &tex);
  mGLContext->fBindTexture(mFBOTextureTarget, tex);

  if (aCopyFromSource) {
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
    GLenum error = mGLContext->GetAndClearError();
    if (error != LOCAL_GL_NO_ERROR) {
      nsAutoCString msg;
      msg.AppendPrintf("Texture initialization failed! -- error 0x%x, Source %d, Source format %d,  RGBA Compat %d",
                       error, aSourceFrameBuffer, format, isFormatCompatibleWithRGBA);
      NS_ERROR(msg.get());
    }
  } else {
    mGLContext->fTexImage2D(mFBOTextureTarget,
                            0,
                            LOCAL_GL_RGBA,
                            aRect.width, aRect.height,
                            0,
                            LOCAL_GL_RGBA,
                            LOCAL_GL_UNSIGNED_BYTE,
                            nullptr);
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

ShaderProgramType
CompositorOGL::GetProgramTypeForEffect(Effect *aEffect) const
{
  switch(aEffect->mType) {
  case EFFECT_SOLID_COLOR:
    return ColorLayerProgramType;
  case EFFECT_RGBA:
  case EFFECT_RGBX:
  case EFFECT_BGRA:
  case EFFECT_BGRX:
  {
    TexturedEffect* texturedEffect =
        static_cast<TexturedEffect*>(aEffect);
    TextureSourceOGL* source = texturedEffect->mTexture->AsSourceOGL();

    return ShaderProgramFromTargetAndFormat(source->GetTextureTarget(),
                                            source->GetFormat());
  }
  case EFFECT_YCBCR:
    return YCbCrLayerProgramType;
  case EFFECT_RENDER_TARGET:
    return GetFBOLayerProgramType();
  default:
    return RGBALayerProgramType;
  }
}

struct MOZ_STACK_CLASS AutoBindTexture
  : public ScopedGLWrapper<AutoBindTexture>
{
  friend struct ScopedGLWrapper<AutoBindTexture>;

protected:
  GLenum mTexUnit;
  GLuint mOldTexId;

public:
  explicit AutoBindTexture(GLContext* aGL)
    : ScopedGLWrapper<AutoBindTexture>(aGL)
    , mTexUnit(0)
    , mOldTexId(GLuint(-1))
  { }

  AutoBindTexture(GLContext* aGL, TextureSourceOGL* aTexture,
                  GLenum aTexUnit = LOCAL_GL_TEXTURE0)
    : ScopedGLWrapper<AutoBindTexture>(aGL)
    , mTexUnit(0)
    , mOldTexId(GLuint(-1))
  {
    MOZ_ASSERT(aTexture);
    MOZ_ASSERT(mOldTexId == GLuint(-1));
    mTexUnit = aTexUnit;

    ScopedBindTextureUnit autoBindTexUnit(mGL, aTexUnit);

    mGL->GetUIntegerv(LOCAL_GL_TEXTURE_BINDING_2D, &mOldTexId);
    aTexture->BindTexture(mTexUnit);
  }

protected:
  void UnwrapImpl()
  {
    if (mOldTexId == GLuint(-1))
      return;

    ScopedBindTextureUnit autoBindTexUnit(mGL, mTexUnit);
    mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mOldTexId);
  }
};

struct MOZ_STACK_CLASS AutoSaveTexture
  : public ScopedGLWrapper<AutoSaveTexture>
{
  friend struct ScopedGLWrapper<AutoSaveTexture>;

protected:
  GLenum mTexUnit;
  GLuint mOldTexId;

public:
  AutoSaveTexture(GLContext* aGL, GLenum aTexUnit = LOCAL_GL_TEXTURE0)
    : ScopedGLWrapper<AutoSaveTexture>(aGL)
    , mTexUnit(aTexUnit)
    , mOldTexId(GLuint(-1))
  {
    ScopedBindTextureUnit savedTexUnit(mGL, mTexUnit);
    mGL->GetUIntegerv(LOCAL_GL_TEXTURE_BINDING_2D, &mOldTexId);
  }

protected:
  void UnwrapImpl()
  {
    ScopedBindTextureUnit savedTexUnit(mGL, mTexUnit);
    mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mOldTexId);
  }
};


void
CompositorOGL::DrawLines(const std::vector<gfx::Point>& aLines, const gfx::Rect& aClipRect,
                         const gfx::Color& aColor,
                         gfx::Float aOpacity, const gfx::Matrix4x4 &aTransform)
{
  mGLContext->fLineWidth(2.0);

  EffectChain effects;
  effects.mPrimaryEffect = new EffectSolidColor(aColor);

  for (int32_t i = 0; i < (int32_t)aLines.size() - 1; i++) {
    const gfx::Point& p1 = aLines[i];
    const gfx::Point& p2 = aLines[i+1];
    DrawQuadInternal(Rect(p1.x, p2.y, p2.x - p1.x, p1.y - p2.y),
                     aClipRect, effects, aOpacity, aTransform,
                     LOCAL_GL_LINE_STRIP);
  }
}

/**
 * Applies aFilter to the texture currently bound to aTarget.
 */
void ApplyFilterToBoundTexture(GLContext* aGL,
                               GraphicsFilter aFilter,
                               GLuint aTarget = LOCAL_GL_TEXTURE_2D)
{
  GLenum filter =
    (aFilter == GraphicsFilter::FILTER_NEAREST ? LOCAL_GL_NEAREST : LOCAL_GL_LINEAR);

    aGL->fTexParameteri(aTarget, LOCAL_GL_TEXTURE_MIN_FILTER, filter);
    aGL->fTexParameteri(aTarget, LOCAL_GL_TEXTURE_MAG_FILTER, filter);
}

void
CompositorOGL::DrawQuadInternal(const Rect& aRect,
                                const Rect& aClipRect,
                                const EffectChain &aEffectChain,
                                Float aOpacity,
                                const gfx::Matrix4x4 &aTransform,
                                GLuint aDrawMode)
{
  PROFILER_LABEL("CompositorOGL", "DrawQuad");
  MOZ_ASSERT(mFrameInProgress, "frame not started");

  Rect clipRect = aClipRect;
  if (!mTarget) {
    clipRect.MoveBy(mRenderOffset.x, mRenderOffset.y);
  }
  IntRect intClipRect;
  clipRect.ToIntRect(&intClipRect);

  ScopedGLState scopedScissorTestState(mGLContext, LOCAL_GL_SCISSOR_TEST, true);
  ScopedScissorRect autoScissor(mGLContext,
                                intClipRect.x,
                                FlipY(intClipRect.y + intClipRect.height),
                                intClipRect.width,
                                intClipRect.height);

  LayerScope::SendEffectChain(mGLContext, aEffectChain,
                              aRect.width, aRect.height);

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

  mPixelsFilled += aRect.width * aRect.height;

  ShaderProgramType programType = GetProgramTypeForEffect(aEffectChain.mPrimaryEffect);
  ShaderProgramOGL *program = GetProgram(programType, maskType);
  program->Activate();
  if (programType == RGBARectLayerProgramType ||
      programType == RGBXRectLayerProgramType) {
    TexturedEffect* texturedEffect =
        static_cast<TexturedEffect*>(aEffectChain.mPrimaryEffect.get());
    TextureSourceOGL* source = texturedEffect->mTexture->AsSourceOGL();
    // This is used by IOSurface that use 0,0...w,h coordinate rather then 0,0..1,1.
    program->SetTexCoordMultiplier(source->GetSize().width, source->GetSize().height);
  }
  program->SetLayerQuadRect(aRect);
  program->SetLayerTransform(aTransform);
  IntPoint offset = mCurrentRenderTarget->GetOrigin();
  program->SetRenderOffset(offset.x, offset.y);

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

      AutoSaveTexture bindMask(mGLContext, LOCAL_GL_TEXTURE0);
      if (maskType != MaskNone) {
        BindMaskForProgram(program, sourceMask, LOCAL_GL_TEXTURE0, maskQuadTransform);
      }

      BindAndDrawQuad(program, false, aDrawMode);
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

      AutoBindTexture bindSource(mGLContext, source->AsSourceOGL(), LOCAL_GL_TEXTURE0);

      GraphicsFilter filter = ThebesFilter(texturedEffect->mFilter);
      gfx3DMatrix textureTransform = source->AsSourceOGL()->GetTextureTransform();

#ifdef MOZ_WIDGET_ANDROID
      gfxMatrix textureTransform2D;
      if (filter != GraphicsFilter::FILTER_NEAREST &&
          aTransform.Is2DIntegerTranslation() &&
          textureTransform.Is2D(&textureTransform2D) &&
          textureTransform2D.HasOnlyIntegerTranslation()) {
        // On Android we encounter small resampling errors in what should be
        // pixel-aligned compositing operations. This works around them. This
        // code should not be needed!
        filter = GraphicsFilter::FILTER_NEAREST;
      }
#endif
      ApplyFilterToBoundTexture(mGLContext, filter,
                                source->AsSourceOGL()->GetTextureTarget());

      program->SetTextureUnit(0);
      program->SetLayerOpacity(aOpacity);

      AutoSaveTexture bindMask(mGLContext, LOCAL_GL_TEXTURE1);
      if (maskType != MaskNone) {
        BindMaskForProgram(program, sourceMask, LOCAL_GL_TEXTURE1, maskQuadTransform);
      }

      BindAndDrawQuadWithTextureRect(program, textureTransform,
                                     texturedEffect->mTextureCoords, source);

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

      GraphicsFilter filter = ThebesFilter(effectYCbCr->mFilter);

      AutoBindTexture bindY(mGLContext, sourceY, LOCAL_GL_TEXTURE0);
      ApplyFilterToBoundTexture(mGLContext, filter);
      AutoBindTexture bindCb(mGLContext, sourceCb, LOCAL_GL_TEXTURE1);
      ApplyFilterToBoundTexture(mGLContext, filter);
      AutoBindTexture bindCr(mGLContext, sourceCr, LOCAL_GL_TEXTURE2);
      ApplyFilterToBoundTexture(mGLContext, filter);

      program->SetYCbCrTextureUnits(Y, Cb, Cr);
      program->SetLayerOpacity(aOpacity);

      AutoSaveTexture bindMask(mGLContext, LOCAL_GL_TEXTURE3);
      if (maskType != MaskNone) {
        BindMaskForProgram(program, sourceMask, LOCAL_GL_TEXTURE3, maskQuadTransform);
      }
      BindAndDrawQuadWithTextureRect(program,
                                     gfx3DMatrix(),
                                     effectYCbCr->mTextureCoords,
                                     sourceYCbCr->GetSubSource(Y));
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
      program->SetTextureTransform(gfx3DMatrix());

      AutoSaveTexture bindMask(mGLContext, LOCAL_GL_TEXTURE1);
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
      MOZ_ASSERT(gfxPlatform::ComponentAlphaEnabled());
      EffectComponentAlpha* effectComponentAlpha =
        static_cast<EffectComponentAlpha*>(aEffectChain.mPrimaryEffect.get());
      TextureSourceOGL* sourceOnWhite = effectComponentAlpha->mOnWhite->AsSourceOGL();
      TextureSourceOGL* sourceOnBlack = effectComponentAlpha->mOnBlack->AsSourceOGL();

      if (!sourceOnBlack->IsValid() ||
          !sourceOnWhite->IsValid()) {
        NS_WARNING("Invalid layer texture for component alpha");
        return;
      }

      for (int32_t pass = 1; pass <=2; ++pass) {
        ShaderProgramOGL* program;
        if (pass == 1) {
          ShaderProgramType type = gl()->GetPreferredARGB32Format() == LOCAL_GL_BGRA ?
                                   ComponentAlphaPass1RGBProgramType :
                                   ComponentAlphaPass1ProgramType;
          program = GetProgram(type, maskType);
          gl()->fBlendFuncSeparate(LOCAL_GL_ZERO, LOCAL_GL_ONE_MINUS_SRC_COLOR,
                                   LOCAL_GL_ONE, LOCAL_GL_ONE);
        } else {
          ShaderProgramType type = gl()->GetPreferredARGB32Format() == LOCAL_GL_BGRA ?
                                   ComponentAlphaPass2RGBProgramType :
                                   ComponentAlphaPass2ProgramType;
          program = GetProgram(type, maskType);
          gl()->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE,
                                   LOCAL_GL_ONE, LOCAL_GL_ONE);
        }

        AutoBindTexture bindSourceOnBlack(mGLContext, sourceOnBlack, LOCAL_GL_TEXTURE0);
        AutoBindTexture bindSourceOnWhite(mGLContext, sourceOnWhite, LOCAL_GL_TEXTURE1);

        program->Activate();
        program->SetBlackTextureUnit(0);
        program->SetWhiteTextureUnit(1);
        program->SetLayerOpacity(aOpacity);
        program->SetLayerTransform(aTransform);
        program->SetRenderOffset(offset.x, offset.y);
        program->SetLayerQuadRect(aRect);
        AutoSaveTexture bindMask(mGLContext, LOCAL_GL_TEXTURE2);
        if (maskType != MaskNone) {
          BindMaskForProgram(program, sourceMask, LOCAL_GL_TEXTURE2, maskQuadTransform);
        }

        BindAndDrawQuadWithTextureRect(program,
                                       gfx3DMatrix(),
                                       effectComponentAlpha->mTextureCoords,
                                       effectComponentAlpha->mOnBlack);

        mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                       LOCAL_GL_ONE, LOCAL_GL_ONE);
      }
    }
    break;
  default:
    MOZ_ASSERT(false, "Unhandled effect type");
    break;
  }

  mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);
  // in case rendering has used some other GL context
  MakeCurrent();
}

void
CompositorOGL::EndFrame()
{
  PROFILER_LABEL("CompositorOGL", "EndFrame");
  MOZ_ASSERT(mCurrentRenderTarget == mWindowRenderTarget, "Rendering target not properly restored");

#ifdef MOZ_DUMP_PAINTING
  if (gfxUtils::sDumpPainting) {
    nsIntRect rect;
    if (mUseExternalSurfaceSize) {
      rect = nsIntRect(0, 0, mSurfaceSize.width, mSurfaceSize.height);
    } else {
      mWidget->GetBounds(rect);
    }
    RefPtr<DrawTarget> target = gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(IntSize(rect.width, rect.height), SurfaceFormat::B8G8R8A8);
    CopyToTarget(target, mCurrentRenderTarget->GetTransform());

    WriteSnapshotToDumpFile(this, target);
  }
#endif

  mFrameInProgress = false;

  LayerScope::EndFrame(mGLContext);

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
    float fillRatio = 0;
    if (mPixelsFilled > 0 && mPixelsPerFrame > 0) {
      fillRatio = 100.0f * float(mPixelsFilled) / float(mPixelsPerFrame);
      if (fillRatio > 999.0f)
        fillRatio = 999.0f;
    }
    mFPS->DrawFPS(TimeStamp::Now(), unsigned(fillRatio), mGLContext, GetProgram(RGBXLayerProgramType));
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
CompositorOGL::CopyToTarget(DrawTarget *aTarget, const gfxMatrix& aTransform)
{
  IntRect rect;
  if (mUseExternalSurfaceSize) {
    rect = IntRect(0, 0, mSurfaceSize.width, mSurfaceSize.height);
  } else {
    rect = IntRect(0, 0, mWidgetSize.width, mWidgetSize.height);
  }
  GLint width = rect.width;
  GLint height = rect.height;

  if ((int64_t(width) * int64_t(height) * int64_t(4)) > INT32_MAX) {
    NS_ERROR("Widget size too big - integer overflow!");
    return;
  }

  mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

  if (!mGLContext->IsGLES2()) {
    // GLES2 promises that binding to any custom FBO will attach
    // to GL_COLOR_ATTACHMENT0 attachment point.
    mGLContext->fReadBuffer(LOCAL_GL_BACK);
  }

  RefPtr<DataSourceSurface> source =
        Factory::CreateDataSourceSurface(rect.Size(), gfx::SurfaceFormat::B8G8R8A8);
  // XXX we should do this properly one day without using the gfxImageSurface
  nsRefPtr<gfxImageSurface> surf =
    new gfxImageSurface(source->GetData(),
                        gfxIntSize(width, height),
                        source->Stride(),
                        gfxImageFormatARGB32);
  ReadPixelsIntoImageSurface(mGLContext, surf);
  source->MarkDirty();

  // Map from GL space to Cairo space and reverse the world transform.
  Matrix glToCairoTransform = ToMatrix(aTransform);
  glToCairoTransform.Invert();
  glToCairoTransform.Scale(1.0, -1.0);
  glToCairoTransform.Translate(0.0, -height);

  Matrix oldMatrix = aTarget->GetTransform();
  aTarget->SetTransform(glToCairoTransform);
  Rect floatRect = Rect(rect.x, rect.y, rect.width, rect.height);
  aTarget->DrawSurface(source, floatRect, floatRect, DrawSurfaceOptions(), DrawOptions(1.0f, CompositionOp::OP_SOURCE));
  aTarget->SetTransform(oldMatrix);
  aTarget->Flush();
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

TemporaryRef<DataTextureSource>
CompositorOGL::CreateDataTextureSource(TextureFlags aFlags)
{
  RefPtr<DataTextureSource> result =
    new TextureImageTextureSourceOGL(mGLContext, aFlags);
  return result;
}

bool
CompositorOGL::SupportsPartialTextureUpdate()
{
  return CanUploadSubTextures(mGLContext);
}

int32_t
CompositorOGL::GetMaxTextureSize() const
{
  MOZ_ASSERT(mGLContext);
  GLint texSize = 0;
  mGLContext->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE,
                            &texSize);
  MOZ_ASSERT(texSize != 0);
  return texSize;
}

void
CompositorOGL::MakeCurrent(MakeCurrentFlags aFlags) {
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }
  mGLContext->MakeCurrent(aFlags & ForceMakeCurrent);
}

void
CompositorOGL::BindQuadVBO() {
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mQuadVBO);
}

void
CompositorOGL::QuadVBOVerticesAttrib(GLuint aAttribIndex) {
  mGLContext->fVertexAttribPointer(aAttribIndex, 2,
                                    LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                                    (GLvoid*) QuadVBOVertexOffset());
}

void
CompositorOGL::QuadVBOTexCoordsAttrib(GLuint aAttribIndex) {
  mGLContext->fVertexAttribPointer(aAttribIndex, 2,
                                    LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                                    (GLvoid*) QuadVBOTexCoordOffset());
}

void
CompositorOGL::QuadVBOFlippedTexCoordsAttrib(GLuint aAttribIndex) {
  mGLContext->fVertexAttribPointer(aAttribIndex, 2,
                                    LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                                    (GLvoid*) QuadVBOFlippedTexCoordOffset());
}

void
CompositorOGL::BindAndDrawQuad(GLuint aVertAttribIndex,
                               GLuint aTexCoordAttribIndex,
                               bool aFlipped,
                               GLuint aDrawMode)
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
  if (aDrawMode == LOCAL_GL_LINE_STRIP) {
    mGLContext->fDrawArrays(aDrawMode, 1, 2);
  } else {
    mGLContext->fDrawArrays(aDrawMode, 0, 4);
  }
  mGLContext->fDisableVertexAttribArray(aVertAttribIndex);

  if (aTexCoordAttribIndex != GLuint(-1)) {
    mGLContext->fDisableVertexAttribArray(aTexCoordAttribIndex);
  }
}

void
CompositorOGL::BindAndDrawQuad(ShaderProgramOGL *aProg,
                               bool aFlipped,
                               GLuint aDrawMode)
{
  NS_ASSERTION(aProg->HasInitialized(), "Shader program not correctly initialized");
  BindAndDrawQuad(aProg->AttribLocation(ShaderProgramOGL::VertexCoordAttrib),
                  aProg->AttribLocation(ShaderProgramOGL::TexCoordAttrib),
                  aFlipped, aDrawMode);
}

} /* layers */
} /* mozilla */
