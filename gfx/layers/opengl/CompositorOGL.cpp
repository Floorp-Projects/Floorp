/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorOGL.h"
#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t, uint8_t
#include <stdlib.h>                     // for free, malloc
#include "GLContextProvider.h"          // for GLContextProvider
#include "GLContext.h"                  // for GLContext
#include "GLUploadHelpers.h"
#include "Layers.h"                     // for WriteSnapshotToDumpFile
#include "LayerScope.h"                 // for LayerScope
#include "gfx2DGlue.h"                  // for ThebesFilter
#include "gfxCrashReporterUtils.h"      // for ScopedGfxFeatureReporter
#include "GraphicsFilter.h"             // for GraphicsFilter
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxPrefs.h"                   // for gfxPrefs
#include "gfxRect.h"                    // for gfxRect
#include "gfxUtils.h"                   // for NextPowerOfTwo, gfxUtils, etc
#include "mozilla/ArrayUtils.h"         // for ArrayLength
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4, Matrix
#include "mozilla/layers/LayerManagerComposite.h"  // for LayerComposite, etc
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
#include "ScopedGLHelpers.h"
#include "GLReadTexImageHelper.h"
#include "TiledLayerBuffer.h"           // for TiledLayerComposer
#include "HeapCopyOfStackArray.h"

#if MOZ_WIDGET_ANDROID
#include "TexturePoolOGL.h"
#endif

#ifdef XP_MACOSX
#include "nsCocoaFeatures.h"
#endif

#include "GeckoProfiler.h"

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
#include "libdisplay/GonkDisplay.h"     // for GonkDisplay
#include <ui/Fence.h>
#endif

namespace mozilla {

using namespace std;
using namespace gfx;

namespace layers {

using namespace mozilla::gl;

static void
BindMaskForProgram(ShaderProgramOGL* aProgram, TextureSourceOGL* aSourceMask,
                   GLenum aTexUnit, const gfx::Matrix4x4& aTransform)
{
  MOZ_ASSERT(LOCAL_GL_TEXTURE0 <= aTexUnit && aTexUnit <= LOCAL_GL_TEXTURE31);
  aSourceMask->BindTexture(aTexUnit, gfx::Filter::LINEAR);
  aProgram->SetMaskTextureUnit(aTexUnit - LOCAL_GL_TEXTURE0);
  aProgram->SetMaskLayerTransform(aTransform);
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
  SetBackend(LayersBackend::LAYERS_OPENGL);
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

  // Allow to create offscreen GL context for main Layer Manager
  if (!context && PR_GetEnv("MOZ_LAYERS_PREFER_OFFSCREEN")) {
    SurfaceCaps caps = SurfaceCaps::ForRGB();
    caps.preserve = false;
    caps.bpp16 = gfxPlatform::GetPlatform()->GetOffscreenFormat() == gfxImageFormat::RGB16_565;
    context = GLContextProvider::CreateOffscreen(gfxIntSize(mSurfaceSize.width,
                                                            mSurfaceSize.height), caps);
  }

  if (!context)
    context = gl::GLContextProvider::CreateForWindow(mWidget);

  if (!context) {
    NS_WARNING("Failed to create CompositorOGL context");
  }

  return context.forget();
}

void
CompositorOGL::Destroy()
{
  if (mTexturePool) {
    mTexturePool->Clear();
    mTexturePool = nullptr;
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

  for (std::map<ShaderConfigOGL, ShaderProgramOGL *>::iterator iter = mPrograms.begin();
       iter != mPrograms.end();
       iter++) {
    delete iter->second;
  }
  mPrograms.clear();

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

  // On the main thread the Widget will be destroyed soon and calling MakeCurrent
  // after that could cause a crash (at least with GLX, see bug 1059793), unless
  // context is marked as destroyed.
  // There may be some textures still alive that will try to call MakeCurrent on
  // the context so let's make sure it is marked destroyed now.
  mGLContext->MarkDestroyed();

  mGLContext = nullptr;
}

bool
CompositorOGL::Initialize()
{
  bool force = gfxPrefs::LayersAccelerationForceEnabled();

  ScopedGfxFeatureReporter reporter("GL Layers", force);

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

  // initialise a common shader to check that we can actually compile a shader
  RefPtr<EffectSolidColor> effect = new EffectSolidColor(Color(0, 0, 0, 0));
  ShaderConfigOGL config = GetShaderConfigFor(effect);
  if (!GetShaderProgramFor(config)) {
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

    if (!mGLContext->IsGLES()) {
      // No TEXTURE_RECTANGLE_ARB available on ES2
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

  // 4 quads, with the number of the quad (vertexID) encoded in w.
  GLfloat vertices[] = {
    0.0f, 0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    1.0f, 1.0f, 0.0f, 0.0f,

    0.0f, 0.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 0.0f, 1.0f,
    0.0f, 1.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 0.0f, 1.0f,
    0.0f, 1.0f, 0.0f, 1.0f,
    1.0f, 1.0f, 0.0f, 1.0f,

    0.0f, 0.0f, 0.0f, 2.0f,
    1.0f, 0.0f, 0.0f, 2.0f,
    0.0f, 1.0f, 0.0f, 2.0f,
    1.0f, 0.0f, 0.0f, 2.0f,
    0.0f, 1.0f, 0.0f, 2.0f,
    1.0f, 1.0f, 0.0f, 2.0f,

    0.0f, 0.0f, 0.0f, 3.0f,
    1.0f, 0.0f, 0.0f, 3.0f,
    0.0f, 1.0f, 0.0f, 3.0f,
    1.0f, 0.0f, 0.0f, 3.0f,
    0.0f, 1.0f, 0.0f, 3.0f,
    1.0f, 1.0f, 0.0f, 3.0f,
  };
  HeapCopyOfStackArray<GLfloat> verticesOnHeap(vertices);
  mGLContext->fBufferData(LOCAL_GL_ARRAY_BUFFER,
                          verticesOnHeap.ByteLength(),
                          verticesOnHeap.Data(),
                          LOCAL_GL_STATIC_DRAW);
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

  reporter.SetSuccessful();
  return true;
}

static GLfloat
WrapTexCoord(GLfloat v)
{
    // fmodf gives negative results for negative numbers;
    // that is, fmodf(0.75, 1.0) == 0.75, but
    // fmodf(-0.75, 1.0) == -0.75.  For the negative case,
    // the result we need is 0.25, so we add 1.0f.
    if (v < 0.0f) {
        return 1.0f + fmodf(v, 1.0f);
    }

    return fmodf(v, 1.0f);
}

static void
SetRects(int n,
         Rect* aLayerRects,
         Rect* aTextureRects,
         GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1,
         GLfloat tx0, GLfloat ty0, GLfloat tx1, GLfloat ty1,
         bool flip_y /* = false */)
{
  if (flip_y) {
    std::swap(ty0, ty1);
  }
  aLayerRects[n] = Rect(x0, y0, x1 - x0, y1 - y0);
  aTextureRects[n] = Rect(tx0, ty0, tx1 - tx0, ty1 - ty0);
}

#ifdef DEBUG
static inline bool
FuzzyEqual(float a, float b)
{
  return fabs(a - b) < 0.0001f;
}
#endif

static int
DecomposeIntoNoRepeatRects(const Rect& aRect,
                           const Rect& aTexCoordRect,
                           Rect* aLayerRects,
                           Rect* aTextureRects)
{
  Rect texCoordRect = aTexCoordRect;

  // If the texture should be flipped, it will have negative height. Detect that
  // here and compensate for it. We will flip each rect as we emit it.
  bool flipped = false;
  if (texCoordRect.height < 0) {
    flipped = true;
    texCoordRect.y += texCoordRect.height;
    texCoordRect.height = -texCoordRect.height;
  }

  // Wrap the texture coordinates so they are within [0,1] and cap width/height
  // at 1. We rely on this below.
  texCoordRect = Rect(Point(WrapTexCoord(texCoordRect.x),
                            WrapTexCoord(texCoordRect.y)),
                      Size(std::min(texCoordRect.width, 1.0f),
                           std::min(texCoordRect.height, 1.0f)));

  NS_ASSERTION(texCoordRect.x >= 0.0f && texCoordRect.x <= 1.0f &&
               texCoordRect.y >= 0.0f && texCoordRect.y <= 1.0f &&
               texCoordRect.width >= 0.0f && texCoordRect.width <= 1.0f &&
               texCoordRect.height >= 0.0f && texCoordRect.height <= 1.0f &&
               texCoordRect.XMost() >= 0.0f && texCoordRect.XMost() <= 2.0f &&
               texCoordRect.YMost() >= 0.0f && texCoordRect.YMost() <= 2.0f,
               "We just wrapped the texture coordinates, didn't we?");

  // Get the top left and bottom right points of the rectangle. Note that
  // tl.x/tl.y are within [0,1] but br.x/br.y are within [0,2].
  Point tl = texCoordRect.TopLeft();
  Point br = texCoordRect.BottomRight();

  NS_ASSERTION(tl.x >= 0.0f && tl.x <= 1.0f &&
               tl.y >= 0.0f && tl.y <= 1.0f &&
               br.x >= tl.x && br.x <= 2.0f &&
               br.y >= tl.y && br.y <= 2.0f &&
               br.x - tl.x <= 1.0f &&
               br.y - tl.y <= 1.0f,
               "Somehow generated invalid texture coordinates");

  // Then check if we wrap in either the x or y axis.
  bool xwrap = br.x > 1.0f;
  bool ywrap = br.y > 1.0f;

  // If xwrap is false, the texture will be sampled from tl.x .. br.x.
  // If xwrap is true, then it will be split into tl.x .. 1.0, and
  // 0.0 .. WrapTexCoord(br.x). Same for the Y axis. The destination
  // rectangle is also split appropriately, according to the calculated
  // xmid/ymid values.
  if (!xwrap && !ywrap) {
    SetRects(0, aLayerRects, aTextureRects,
             aRect.x, aRect.y, aRect.XMost(), aRect.YMost(),
             tl.x, tl.y, br.x, br.y,
             flipped);
    return 1;
  }

  // If we are dealing with wrapping br.x and br.y are greater than 1.0 so
  // wrap them here as well.
  br = Point(xwrap ? WrapTexCoord(br.x) : br.x,
             ywrap ? WrapTexCoord(br.y) : br.y);

  // If we wrap around along the x axis, we will draw first from
  // tl.x .. 1.0 and then from 0.0 .. br.x (which we just wrapped above).
  // The same applies for the Y axis. The midpoints we calculate here are
  // only valid if we actually wrap around.
  GLfloat xmid = aRect.x + (1.0f - tl.x) / texCoordRect.width * aRect.width;
  GLfloat ymid = aRect.y + (1.0f - tl.y) / texCoordRect.height * aRect.height;

  NS_ASSERTION(!xwrap ||
               (xmid > aRect.x &&
                xmid < aRect.XMost() &&
                FuzzyEqual((xmid - aRect.x) + (aRect.XMost() - xmid), aRect.width)),
               "xmid should be within [x,XMost()] and the wrapped rect should have the same width");
  NS_ASSERTION(!ywrap ||
               (ymid > aRect.y &&
                ymid < aRect.YMost() &&
                FuzzyEqual((ymid - aRect.y) + (aRect.YMost() - ymid), aRect.height)),
               "ymid should be within [y,YMost()] and the wrapped rect should have the same height");

  if (!xwrap && ywrap) {
    SetRects(0, aLayerRects, aTextureRects,
             aRect.x, aRect.y, aRect.XMost(), ymid,
             tl.x, tl.y, br.x, 1.0f,
             flipped);
    SetRects(1, aLayerRects, aTextureRects,
             aRect.x, ymid, aRect.XMost(), aRect.YMost(),
             tl.x, 0.0f, br.x, br.y,
             flipped);
    return 2;
  }

  if (xwrap && !ywrap) {
    SetRects(0, aLayerRects, aTextureRects,
             aRect.x, aRect.y, xmid, aRect.YMost(),
             tl.x, tl.y, 1.0f, br.y,
             flipped);
    SetRects(1, aLayerRects, aTextureRects,
             xmid, aRect.y, aRect.XMost(), aRect.YMost(),
             0.0f, tl.y, br.x, br.y,
             flipped);
    return 2;
  }

  SetRects(0, aLayerRects, aTextureRects,
           aRect.x, aRect.y, xmid, ymid,
           tl.x, tl.y, 1.0f, 1.0f,
           flipped);
  SetRects(1, aLayerRects, aTextureRects,
           xmid, aRect.y, aRect.XMost(), ymid,
           0.0f, tl.y, br.x, 1.0f,
           flipped);
  SetRects(2, aLayerRects, aTextureRects,
           aRect.x, ymid, xmid, aRect.YMost(),
           tl.x, 0.0f, 1.0f, br.y,
           flipped);
  SetRects(3, aLayerRects, aTextureRects,
           xmid, ymid, aRect.XMost(), aRect.YMost(),
           0.0f, 0.0f, br.x, br.y,
           flipped);
  return 4;
}

// |aRect| is the rectangle we want to draw to. We will draw it with
// up to 4 draw commands if necessary to avoid wrapping.
// |aTexCoordRect| is the rectangle from the texture that we want to
// draw using the given program.
// |aTexture| is the texture we are drawing. Its actual size can be
// larger than the rectangle given by |texCoordRect|.
void
CompositorOGL::BindAndDrawQuadWithTextureRect(ShaderProgramOGL *aProg,
                                              const Rect& aRect,
                                              const Rect& aTexCoordRect,
                                              TextureSource *aTexture)
{
  Rect layerRects[4];
  Rect textureRects[4];
  int rects = DecomposeIntoNoRepeatRects(aRect,
                                         aTexCoordRect,
                                         layerRects,
                                         textureRects);
  BindAndDrawQuads(aProg, rects, layerRects, textureRects);
}

void
CompositorOGL::PrepareViewport(const gfx::IntSize& aSize,
                               const Matrix& aWorldTransform)
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
  Matrix viewMatrix;
  if (mGLContext->IsOffscreen()) {
    // In case of rendering via GL Offscreen context, disable Y-Flipping
    viewMatrix.Translate(-1.0, -1.0);
    viewMatrix.Scale(2.0f / float(aSize.width), 2.0f / float(aSize.height));
  } else {
    viewMatrix.Translate(-1.0, 1.0);
    viewMatrix.Scale(2.0f / float(aSize.width), 2.0f / float(aSize.height));
    viewMatrix.Scale(1.0f, -1.0f);
  }

  viewMatrix = aWorldTransform * viewMatrix;

  Matrix4x4 matrix3d = Matrix4x4::From2D(viewMatrix);
  matrix3d._33 = 0.0f;

  mProjMatrix = matrix3d;
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
CompositorOGL::GetCurrentRenderTarget() const
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
CompositorOGL::ClearRect(const gfx::Rect& aRect)
{
  // Map aRect to OGL coordinates, origin:bottom-left
  GLint y = mHeight - (aRect.y + aRect.height);

  ScopedGLState scopedScissorTestState(mGLContext, LOCAL_GL_SCISSOR_TEST, true);
  ScopedScissorRect autoScissorRect(mGLContext, aRect.x, y, aRect.width, aRect.height);
  mGLContext->fClearColor(0.0, 0.0, 0.0, 0.0);
  mGLContext->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT);
}

void
CompositorOGL::BeginFrame(const nsIntRegion& aInvalidRegion,
                          const Rect *aClipRectIn,
                          const gfx::Matrix& aTransform,
                          const Rect& aRenderBounds,
                          Rect *aClipRectOut,
                          Rect *aRenderBoundsOut)
{
  PROFILER_LABEL("CompositorOGL", "BeginFrame",
    js::ProfileEntry::Category::GRAPHICS);

  MOZ_ASSERT(!mFrameInProgress, "frame still in progress (should have called EndFrame or AbortFrame");

  mFrameInProgress = true;
  gfx::Rect rect;
  if (mUseExternalSurfaceSize) {
    rect = gfx::Rect(0, 0, mSurfaceSize.width, mSurfaceSize.height);
  } else {
    rect = gfx::Rect(aRenderBounds.x, aRenderBounds.y, aRenderBounds.width, aRenderBounds.height);
    // If render bounds is not updated explicitly, try to infer it from widget
    if (rect.width == 0 || rect.height == 0) {
      // FIXME/bug XXXXXX this races with rotation changes on the main
      // thread, and undoes all the care we take with layers txns being
      // sent atomically with rotation changes
      nsIntRect intRect;
      mWidget->GetClientBounds(intRect);
      rect = gfx::Rect(0, 0, intRect.width, intRect.height);
    }
  }

  rect = aTransform.TransformBounds(rect);
  if (aRenderBoundsOut) {
    *aRenderBoundsOut = rect;
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

#if MOZ_WIDGET_ANDROID
  TexturePoolOGL::Fill(gl());
#endif

  // Make sure the render offset is respected. We ignore this when we have a
  // target to stop tests failing - this is only used by the Android browser
  // UI for its dynamic toolbar.
  IntPoint origin;
  if (!mTarget) {
    origin = -TruncatedToInt(mRenderOffset.ToUnknownPoint());
  }

  mCurrentRenderTarget =
    CompositingRenderTargetOGL::RenderTargetForWindow(this,
                                                      origin,
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

  mGLContext->fEnable(LOCAL_GL_SCISSOR_TEST);

  if (aClipRectOut && !aClipRectIn) {
    aClipRectOut->SetRect(0, 0, width, height);
  }

  // If the Android compositor is being used, this clear will be done in
  // DrawWindowUnderlay. Make sure the bits used here match up with those used
  // in mobile/android/base/gfx/LayerRenderer.java
#ifndef MOZ_WIDGET_ANDROID
  mGLContext->fClearColor(0.0, 0.0, 0.0, 0.0);
  mGLContext->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT);
#endif
}

void
CompositorOGL::CreateFBOWithTexture(const IntRect& aRect, bool aCopyFromSource,
                                    GLuint aSourceFrameBuffer,
                                    GLuint *aFBO, GLuint *aTexture)
{
  // we're about to create a framebuffer backed by textures to use as an intermediate
  // surface. What to do if its size (as given by aRect) would exceed the
  // maximum texture size supported by the GL? The present code chooses the compromise
  // of just clamping the framebuffer's size to the max supported size.
  // This gives us a lower resolution rendering of the intermediate surface (children layers).
  // See bug 827170 for a discussion.
  IntRect clampedRect = aRect;
  int32_t maxTexSize = GetMaxTextureSize();
  clampedRect.width = std::min(clampedRect.width, maxTexSize);
  clampedRect.height = std::min(clampedRect.height, maxTexSize);

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
        = gl()->IsGLES() ? (format == LOCAL_GL_RGBA)
                          : true;

    if (isFormatCompatibleWithRGBA) {
      mGLContext->fCopyTexImage2D(mFBOTextureTarget,
                                  0,
                                  LOCAL_GL_RGBA,
                                  clampedRect.x, FlipY(clampedRect.y + clampedRect.height),
                                  clampedRect.width, clampedRect.height,
                                  0);
    } else {
      // Curses, incompatible formats.  Take a slow path.

      // RGBA
      size_t bufferSize = clampedRect.width * clampedRect.height * 4;
      nsAutoArrayPtr<uint8_t> buf(new uint8_t[bufferSize]);

      mGLContext->fReadPixels(clampedRect.x, clampedRect.y,
                              clampedRect.width, clampedRect.height,
                              LOCAL_GL_RGBA,
                              LOCAL_GL_UNSIGNED_BYTE,
                              buf);
      mGLContext->fTexImage2D(mFBOTextureTarget,
                              0,
                              LOCAL_GL_RGBA,
                              clampedRect.width, clampedRect.height,
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
                            clampedRect.width, clampedRect.height,
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

ShaderConfigOGL
CompositorOGL::GetShaderConfigFor(Effect *aEffect,
                                  MaskType aMask,
                                  gfx::CompositionOp aOp,
                                  bool aColorMatrix) const
{
  ShaderConfigOGL config;

  switch(aEffect->mType) {
  case EffectTypes::SOLID_COLOR:
    config.SetRenderColor(true);
    break;
  case EffectTypes::YCBCR:
    config.SetYCbCr(true);
    break;
  case EffectTypes::COMPONENT_ALPHA:
  {
    config.SetComponentAlpha(true);
    EffectComponentAlpha* effectComponentAlpha =
      static_cast<EffectComponentAlpha*>(aEffect);
    gfx::SurfaceFormat format = effectComponentAlpha->mOnWhite->GetFormat();
    config.SetRBSwap(format == gfx::SurfaceFormat::B8G8R8A8 ||
                     format == gfx::SurfaceFormat::B8G8R8X8);
    break;
  }
  case EffectTypes::RENDER_TARGET:
    config.SetTextureTarget(mFBOTextureTarget);
    break;
  default:
  {
    MOZ_ASSERT(aEffect->mType == EffectTypes::RGB);
    TexturedEffect* texturedEffect =
        static_cast<TexturedEffect*>(aEffect);
    TextureSourceOGL* source = texturedEffect->mTexture->AsSourceOGL();
    MOZ_ASSERT_IF(source->GetTextureTarget() == LOCAL_GL_TEXTURE_EXTERNAL,
                  source->GetFormat() == gfx::SurfaceFormat::R8G8B8A8);
    MOZ_ASSERT_IF(source->GetTextureTarget() == LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                  source->GetFormat() == gfx::SurfaceFormat::R8G8B8A8 ||
                  source->GetFormat() == gfx::SurfaceFormat::R8G8B8X8 ||
                  source->GetFormat() == gfx::SurfaceFormat::R5G6B5);
    config = ShaderConfigFromTargetAndFormat(source->GetTextureTarget(),
                                             source->GetFormat());
    if (aOp == gfx::CompositionOp::OP_MULTIPLY &&
        !texturedEffect->mPremultiplied) {
      // We can do these blend modes just using glBlendFunc but we need the data
      // to be premultiplied first.
      config.SetPremultiply(true);
    }
    break;
  }
  }
  config.SetColorMatrix(aColorMatrix);
  config.SetMask2D(aMask == MaskType::Mask2d);
  config.SetMask3D(aMask == MaskType::Mask3d);
  return config;
}

ShaderProgramOGL*
CompositorOGL::GetShaderProgramFor(const ShaderConfigOGL &aConfig)
{
  std::map<ShaderConfigOGL, ShaderProgramOGL *>::iterator iter = mPrograms.find(aConfig);
  if (iter != mPrograms.end())
    return iter->second;

  ProgramProfileOGL profile = ProgramProfileOGL::GetProfileFor(aConfig);
  ShaderProgramOGL *shader = new ShaderProgramOGL(gl(), profile);
  if (!shader->Initialize()) {
    delete shader;
    return nullptr;
  }

  mPrograms[aConfig] = shader;
  return shader;
}

static bool SetBlendMode(GLContext* aGL, gfx::CompositionOp aBlendMode, bool aIsPremultiplied = true)
{
  if (aBlendMode == gfx::CompositionOp::OP_OVER && aIsPremultiplied) {
    return false;
  }

  GLenum srcBlend;
  GLenum dstBlend;
  GLenum srcAlphaBlend = LOCAL_GL_ONE;
  GLenum dstAlphaBlend = LOCAL_GL_ONE;

  switch (aBlendMode) {
    case gfx::CompositionOp::OP_OVER:
      MOZ_ASSERT(!aIsPremultiplied);
      srcBlend = LOCAL_GL_SRC_ALPHA;
      dstBlend = LOCAL_GL_ONE_MINUS_SRC_ALPHA;
      break;
    case gfx::CompositionOp::OP_SCREEN:
      srcBlend = aIsPremultiplied ? LOCAL_GL_ONE : LOCAL_GL_SRC_ALPHA;
      dstBlend = LOCAL_GL_ONE_MINUS_SRC_COLOR;
      break;
    case gfx::CompositionOp::OP_MULTIPLY:
      // If the source data was un-premultiplied we should have already
      // asked the fragment shader to fix that.
      srcBlend = LOCAL_GL_DST_COLOR;
      dstBlend = LOCAL_GL_ONE_MINUS_SRC_ALPHA;
      break;
    case gfx::CompositionOp::OP_SOURCE:
      srcBlend = aIsPremultiplied ? LOCAL_GL_ONE : LOCAL_GL_SRC_ALPHA;
      dstBlend = LOCAL_GL_ZERO;
      srcAlphaBlend = LOCAL_GL_ONE;
      dstAlphaBlend = LOCAL_GL_ZERO;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported blend mode!");
      return false;
  }

  aGL->fBlendFuncSeparate(srcBlend, dstBlend,
                          srcAlphaBlend, dstAlphaBlend);
  return true;
}

void
CompositorOGL::DrawQuad(const Rect& aRect,
                        const Rect& aClipRect,
                        const EffectChain &aEffectChain,
                        Float aOpacity,
                        const gfx::Matrix4x4 &aTransform)
{
  PROFILER_LABEL("CompositorOGL", "DrawQuad",
    js::ProfileEntry::Category::GRAPHICS);

  MOZ_ASSERT(mFrameInProgress, "frame not started");

  IntRect intClipRect;
  aClipRect.ToIntRect(&intClipRect);
  if (!mTarget) {
    intClipRect.MoveBy(mRenderOffset.x, mRenderOffset.y);
  }

  gl()->fScissor(intClipRect.x, FlipY(intClipRect.y + intClipRect.height),
                 intClipRect.width, intClipRect.height);

  LayerScope::SendEffectChain(mGLContext, aEffectChain,
                              aRect.width, aRect.height);

  MaskType maskType;
  EffectMask* effectMask;
  TextureSourceOGL* sourceMask = nullptr;
  gfx::Matrix4x4 maskQuadTransform;
  if (aEffectChain.mSecondaryEffects[EffectTypes::MASK]) {
    effectMask = static_cast<EffectMask*>(aEffectChain.mSecondaryEffects[EffectTypes::MASK].get());
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
                 ? MaskType::Mask3d
                 : MaskType::Mask2d;
  } else {
    maskType = MaskType::MaskNone;
  }

  {
    // XXX: This doesn't handle 3D transforms. It also doesn't handled rotated
    //      quads. Fix me.
    const Rect destRect = aTransform.TransformBounds(aRect);
    mPixelsFilled += destRect.width * destRect.height;
  }

  // Determine the color if this is a color shader and fold the opacity into
  // the color since color shaders don't have an opacity uniform.
  Color color;
  if (aEffectChain.mPrimaryEffect->mType == EffectTypes::SOLID_COLOR) {
    EffectSolidColor* effectSolidColor =
      static_cast<EffectSolidColor*>(aEffectChain.mPrimaryEffect.get());
    color = effectSolidColor->mColor;

    Float opacity = aOpacity * color.a;
    color.r *= opacity;
    color.g *= opacity;
    color.b *= opacity;
    color.a = opacity;

    // We can fold opacity into the color, so no need to consider it further.
    aOpacity = 1.f;
  }

  gfx::CompositionOp blendMode = gfx::CompositionOp::OP_OVER;
  if (aEffectChain.mSecondaryEffects[EffectTypes::BLEND_MODE]) {
    EffectBlendMode *blendEffect =
      static_cast<EffectBlendMode*>(aEffectChain.mSecondaryEffects[EffectTypes::BLEND_MODE].get());
    blendMode = blendEffect->mBlendMode;
  }

  bool colorMatrix = aEffectChain.mSecondaryEffects[EffectTypes::COLOR_MATRIX];
  ShaderConfigOGL config = GetShaderConfigFor(aEffectChain.mPrimaryEffect, maskType, blendMode, colorMatrix);
  config.SetOpacity(aOpacity != 1.f);
  ShaderProgramOGL *program = GetShaderProgramFor(config);
  program->Activate();
  program->SetProjectionMatrix(mProjMatrix);
  program->SetLayerTransform(aTransform);

  if (colorMatrix) {
      EffectColorMatrix* effectColorMatrix =
        static_cast<EffectColorMatrix*>(aEffectChain.mSecondaryEffects[EffectTypes::COLOR_MATRIX].get());
      program->SetColorMatrix(effectColorMatrix->mColorMatrix);
  }

  IntPoint offset = mCurrentRenderTarget->GetOrigin();
  program->SetRenderOffset(offset.x, offset.y);
  if (aOpacity != 1.f)
    program->SetLayerOpacity(aOpacity);
  if (config.mFeatures & ENABLE_TEXTURE_RECT) {
    TexturedEffect* texturedEffect =
        static_cast<TexturedEffect*>(aEffectChain.mPrimaryEffect.get());
    TextureSourceOGL* source = texturedEffect->mTexture->AsSourceOGL();
    // This is used by IOSurface that use 0,0...w,h coordinate rather then 0,0..1,1.
    program->SetTexCoordMultiplier(source->GetSize().width, source->GetSize().height);
  }

  bool didSetBlendMode = false;

  switch (aEffectChain.mPrimaryEffect->mType) {
    case EffectTypes::SOLID_COLOR: {
      program->SetRenderColor(color);

      if (maskType != MaskType::MaskNone) {
        BindMaskForProgram(program, sourceMask, LOCAL_GL_TEXTURE0, maskQuadTransform);
      }

      didSetBlendMode = SetBlendMode(gl(), blendMode);

      BindAndDrawQuad(program, aRect);
    }
    break;

  case EffectTypes::RGB: {
      TexturedEffect* texturedEffect =
          static_cast<TexturedEffect*>(aEffectChain.mPrimaryEffect.get());
      TextureSource *source = texturedEffect->mTexture;

      didSetBlendMode = SetBlendMode(gl(), blendMode, texturedEffect->mPremultiplied);

      gfx::Filter filter = texturedEffect->mFilter;
      Matrix4x4 textureTransform = source->AsSourceOGL()->GetTextureTransform();

#ifdef MOZ_WIDGET_ANDROID
      gfx::Matrix textureTransform2D;
      if (filter != gfx::Filter::POINT &&
          aTransform.Is2DIntegerTranslation() &&
          textureTransform.Is2D(&textureTransform2D) &&
          textureTransform2D.HasOnlyIntegerTranslation()) {
        // On Android we encounter small resampling errors in what should be
        // pixel-aligned compositing operations. This works around them. This
        // code should not be needed!
        filter = gfx::Filter::POINT;
      }
#endif
      source->AsSourceOGL()->BindTexture(LOCAL_GL_TEXTURE0, filter);

      program->SetTextureUnit(0);
      program->SetTextureTransform(textureTransform);

      if (maskType != MaskType::MaskNone) {
        BindMaskForProgram(program, sourceMask, LOCAL_GL_TEXTURE1, maskQuadTransform);
      }

      BindAndDrawQuadWithTextureRect(program, aRect, texturedEffect->mTextureCoords, source);
    }
    break;
  case EffectTypes::YCBCR: {
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

      sourceY->BindTexture(LOCAL_GL_TEXTURE0, effectYCbCr->mFilter);
      sourceCb->BindTexture(LOCAL_GL_TEXTURE1, effectYCbCr->mFilter);
      sourceCr->BindTexture(LOCAL_GL_TEXTURE2, effectYCbCr->mFilter);

      program->SetYCbCrTextureUnits(Y, Cb, Cr);
      program->SetTextureTransform(Matrix4x4());

      if (maskType != MaskType::MaskNone) {
        BindMaskForProgram(program, sourceMask, LOCAL_GL_TEXTURE3, maskQuadTransform);
      }
      didSetBlendMode = SetBlendMode(gl(), blendMode);
      BindAndDrawQuadWithTextureRect(program,
                                     aRect,
                                     effectYCbCr->mTextureCoords,
                                     sourceYCbCr->GetSubSource(Y));
    }
    break;
  case EffectTypes::RENDER_TARGET: {
      EffectRenderTarget* effectRenderTarget =
        static_cast<EffectRenderTarget*>(aEffectChain.mPrimaryEffect.get());
      RefPtr<CompositingRenderTargetOGL> surface
        = static_cast<CompositingRenderTargetOGL*>(effectRenderTarget->mRenderTarget.get());

      surface->BindTexture(LOCAL_GL_TEXTURE0, mFBOTextureTarget);

      // Drawing is always flipped, but when copying between surfaces we want to avoid
      // this, so apply a flip here to cancel the other one out.
      Matrix transform;
      transform.Translate(0.0, 1.0);
      transform.Scale(1.0f, -1.0f);
      program->SetTextureTransform(Matrix4x4::From2D(transform));
      program->SetTextureUnit(0);

      if (maskType != MaskType::MaskNone) {
        sourceMask->BindTexture(LOCAL_GL_TEXTURE1, gfx::Filter::LINEAR);
        program->SetMaskTextureUnit(1);
        program->SetMaskLayerTransform(maskQuadTransform);
      }

      if (config.mFeatures & ENABLE_TEXTURE_RECT) {
        // 2DRect case, get the multiplier right for a sampler2DRect
        program->SetTexCoordMultiplier(aRect.width, aRect.height);
      }

      // Drawing is always flipped, but when copying between surfaces we want to avoid
      // this. Pass true for the flip parameter to introduce a second flip
      // that cancels the other one out.
      didSetBlendMode = SetBlendMode(gl(), blendMode);
      BindAndDrawQuad(program, aRect);
    }
    break;
  case EffectTypes::COMPONENT_ALPHA: {
      MOZ_ASSERT(gfxPrefs::ComponentAlphaEnabled());
      MOZ_ASSERT(blendMode == gfx::CompositionOp::OP_OVER, "Can't support blend modes with component alpha!");
      EffectComponentAlpha* effectComponentAlpha =
        static_cast<EffectComponentAlpha*>(aEffectChain.mPrimaryEffect.get());
      TextureSourceOGL* sourceOnWhite = effectComponentAlpha->mOnWhite->AsSourceOGL();
      TextureSourceOGL* sourceOnBlack = effectComponentAlpha->mOnBlack->AsSourceOGL();

      if (!sourceOnBlack->IsValid() ||
          !sourceOnWhite->IsValid()) {
        NS_WARNING("Invalid layer texture for component alpha");
        return;
      }

      sourceOnBlack->BindTexture(LOCAL_GL_TEXTURE0, effectComponentAlpha->mFilter);
      sourceOnWhite->BindTexture(LOCAL_GL_TEXTURE1, effectComponentAlpha->mFilter);

      program->SetBlackTextureUnit(0);
      program->SetWhiteTextureUnit(1);
      program->SetTextureTransform(Matrix4x4());

      if (maskType != MaskType::MaskNone) {
        BindMaskForProgram(program, sourceMask, LOCAL_GL_TEXTURE2, maskQuadTransform);
      }
      // Pass 1.
      gl()->fBlendFuncSeparate(LOCAL_GL_ZERO, LOCAL_GL_ONE_MINUS_SRC_COLOR,
                               LOCAL_GL_ONE, LOCAL_GL_ONE);
      program->SetTexturePass2(false);
      BindAndDrawQuadWithTextureRect(program,
                                     aRect,
                                     effectComponentAlpha->mTextureCoords,
                                     effectComponentAlpha->mOnBlack);

      // Pass 2.
      gl()->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE,
                               LOCAL_GL_ONE, LOCAL_GL_ONE);

#ifdef XP_MACOSX
      if (gl()->WorkAroundDriverBugs() &&
          gl()->Vendor() == GLVendor::NVIDIA &&
          !nsCocoaFeatures::OnMavericksOrLater()) {
        // Bug 987497: With some GPUs the nvidia driver on 10.8 and below
        // won't pick up the TexturePass2 uniform change below if we don't do
        // something to force it. Re-activating the shader seems to be one way
        // of achieving that.
        program->Activate();
      }
#endif

      program->SetTexturePass2(true);
      BindAndDrawQuadWithTextureRect(program,
                                     aRect,
                                     effectComponentAlpha->mTextureCoords,
                                     effectComponentAlpha->mOnBlack);

      mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                     LOCAL_GL_ONE, LOCAL_GL_ONE);
    }
    break;
  default:
    MOZ_ASSERT(false, "Unhandled effect type");
    break;
  }

  if (didSetBlendMode) {
    gl()->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                             LOCAL_GL_ONE, LOCAL_GL_ONE);
  }

  // in case rendering has used some other GL context
  MakeCurrent();
}

void
CompositorOGL::EndFrame()
{
  PROFILER_LABEL("CompositorOGL", "EndFrame",
    js::ProfileEntry::Category::GRAPHICS);

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
    CopyToTarget(target, nsIntPoint(), mCurrentRenderTarget->GetTransform());

    WriteSnapshotToDumpFile(this, target);
  }
#endif

  mFrameInProgress = false;

  if (mTarget) {
    CopyToTarget(mTarget, mTargetBounds.TopLeft(), mCurrentRenderTarget->GetTransform());
    mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
    mCurrentRenderTarget = nullptr;
    return;
  }

  mCurrentRenderTarget = nullptr;

  if (mTexturePool) {
    mTexturePool->EndFrame();
  }

  mGLContext->SwapBuffers();
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);

  // Unbind all textures
  mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);
  mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, 0);
  if (!mGLContext->IsGLES()) {
    mGLContext->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, 0);
  }

  mGLContext->fActiveTexture(LOCAL_GL_TEXTURE1);
  mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, 0);
  if (!mGLContext->IsGLES()) {
    mGLContext->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, 0);
  }

  mGLContext->fActiveTexture(LOCAL_GL_TEXTURE2);
  mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, 0);
  if (!mGLContext->IsGLES()) {
    mGLContext->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, 0);
  }
}

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
void
CompositorOGL::SetFBAcquireFence(Layer* aLayer)
{
  // OpenGL does not provide ReleaseFence for rendering.
  // Instead use FBAcquireFence as layer buffer's ReleaseFence
  // to prevent flickering and tearing.
  // FBAcquireFence is FramebufferSurface's AcquireFence.
  // AcquireFence will be signaled when a buffer's content is available.
  // See Bug 974152.

  if (!aLayer) {
    return;
  }

  const nsIntRegion& visibleRegion = aLayer->GetEffectiveVisibleRegion();
  if (visibleRegion.IsEmpty()) {
      return;
  }

  // Set FBAcquireFence on ContainerLayer's childs
  ContainerLayer* container = aLayer->AsContainerLayer();
  if (container) {
    for (Layer* child = container->GetFirstChild(); child; child = child->GetNextSibling()) {
      SetFBAcquireFence(child);
    }
    return;
  }

  // Set FBAcquireFence as tiles' ReleaseFence on TiledLayerComposer.
  TiledLayerComposer* composer = nullptr;
  LayerComposite* shadow = aLayer->AsLayerComposite();
  if (shadow) {
    composer = shadow->GetTiledLayerComposer();
    if (composer) {
      composer->SetReleaseFence(new android::Fence(GetGonkDisplay()->GetPrevFBAcquireFd()));
      return;
    }
  }

  // Set FBAcquireFence as layer buffer's ReleaseFence
  LayerRenderState state = aLayer->GetRenderState();
  if (!state.mTexture) {
    return;
  }
  TextureHostOGL* texture = state.mTexture->AsHostOGL();
  if (!texture) {
    return;
  }
  texture->SetReleaseFence(new android::Fence(GetGonkDisplay()->GetPrevFBAcquireFd()));
}
#else
void
CompositorOGL::SetFBAcquireFence(Layer* aLayer)
{
}
#endif

void
CompositorOGL::EndFrameForExternalComposition(const gfx::Matrix& aTransform)
{
  // This lets us reftest and screenshot content rendered externally
  if (mTarget) {
    MakeCurrent();
    CopyToTarget(mTarget, mTargetBounds.TopLeft(), aTransform);
    mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
  }
  if (mTexturePool) {
    mTexturePool->EndFrame();
  }
}

void
CompositorOGL::AbortFrame()
{
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
  mFrameInProgress = false;
  mCurrentRenderTarget = nullptr;

  if (mTexturePool) {
    mTexturePool->EndFrame();
  }
}

void
CompositorOGL::SetDestinationSurfaceSize(const gfx::IntSize& aSize)
{
  mSurfaceSize.width = aSize.width;
  mSurfaceSize.height = aSize.height;
}

void
CompositorOGL::CopyToTarget(DrawTarget* aTarget, const nsIntPoint& aTopLeft, const gfx::Matrix& aTransform)
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

  if (!mGLContext->IsGLES()) {
    // GLES2 promises that binding to any custom FBO will attach
    // to GL_COLOR_ATTACHMENT0 attachment point.
    mGLContext->fReadBuffer(LOCAL_GL_BACK);
  }

  RefPtr<DataSourceSurface> source =
        Factory::CreateDataSourceSurface(rect.Size(), gfx::SurfaceFormat::B8G8R8A8);
  if (NS_WARN_IF(!source)) {
    return;
  }

  ReadPixelsIntoDataSurface(mGLContext, source);

  // Map from GL space to Cairo space and reverse the world transform.
  Matrix glToCairoTransform = aTransform;
  glToCairoTransform.Invert();
  glToCairoTransform.Scale(1.0, -1.0);
  glToCairoTransform.Translate(0.0, -height);

  glToCairoTransform.PostTranslate(-aTopLeft.x, -aTopLeft.y);

  Matrix oldMatrix = aTarget->GetTransform();
  aTarget->SetTransform(glToCairoTransform);
  Rect floatRect = Rect(rect.x, rect.y, rect.width, rect.height);
  aTarget->DrawSurface(source, floatRect, floatRect, DrawSurfaceOptions(), DrawOptions(1.0f, CompositionOp::OP_SOURCE));
  aTarget->SetTransform(oldMatrix);
  aTarget->Flush();
}

void
CompositorOGL::Pause()
{
#ifdef MOZ_WIDGET_ANDROID
  if (!gl() || gl()->IsDestroyed())
    return;

  // ReleaseSurface internally calls MakeCurrent.
  gl()->ReleaseSurface();
#endif
}

bool
CompositorOGL::Resume()
{
#ifdef MOZ_WIDGET_ANDROID
  if (!gl() || gl()->IsDestroyed())
    return false;

  // RenewSurface internally calls MakeCurrent.
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
CompositorOGL::BindAndDrawQuads(ShaderProgramOGL *aProg,
                                int aQuads,
                                const Rect* aLayerRects,
                                const Rect* aTextureRects)
{
  NS_ASSERTION(aProg->HasInitialized(), "Shader program not correctly initialized");

  const GLuint coordAttribIndex = 0;

  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mQuadVBO);
  mGLContext->fVertexAttribPointer(coordAttribIndex, 4,
                                   LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                                   (GLvoid*) 0);
  mGLContext->fEnableVertexAttribArray(coordAttribIndex);

  aProg->SetLayerRects(aLayerRects);
  if (aProg->GetTextureCount() > 0) {
    aProg->SetTextureRects(aTextureRects);
  }

  // We are using GL_TRIANGLES here because the Mac Intel drivers fail to properly
  // process uniform arrays with GL_TRIANGLE_STRIP. Go figure.
  mGLContext->fDrawArrays(LOCAL_GL_TRIANGLES, 0, 6 * aQuads);
}

GLuint
CompositorOGL::GetTemporaryTexture(GLenum aTarget, GLenum aUnit)
{
  if (!mTexturePool) {
#ifdef MOZ_WIDGET_GONK
    mTexturePool = new PerFrameTexturePoolOGL(gl());
#else
    mTexturePool = new PerUnitTexturePoolOGL(gl());
#endif
  }
  return mTexturePool->GetTexture(aTarget, aUnit);
}

GLuint
PerUnitTexturePoolOGL::GetTexture(GLenum aTarget, GLenum aTextureUnit)
{
  if (mTextureTarget == 0) {
    mTextureTarget = aTarget;
  }
  MOZ_ASSERT(mTextureTarget == aTarget);

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
    if (!mGL->MakeCurrent()) {
      return 0;
    }
    mGL->fGenTextures(1, &mTextures[index]);
    mGL->fBindTexture(aTarget, mTextures[index]);
    mGL->fTexParameteri(aTarget, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
    mGL->fTexParameteri(aTarget, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
  }
  return mTextures[index];
}

void
PerUnitTexturePoolOGL::DestroyTextures()
{
  if (mGL && mGL->MakeCurrent()) {
    if (mTextures.Length() > 0) {
      mGL->fDeleteTextures(mTextures.Length(), &mTextures[0]);
    }
  }
  mTextures.SetLength(0);
}

void
PerFrameTexturePoolOGL::DestroyTextures()
{
  if (!mGL->MakeCurrent()) {
    return;
  }

  if (mUnusedTextures.Length() > 0) {
    mGL->fDeleteTextures(mUnusedTextures.Length(), &mUnusedTextures[0]);
    mUnusedTextures.Clear();
  }

  if (mCreatedTextures.Length() > 0) {
    mGL->fDeleteTextures(mCreatedTextures.Length(), &mCreatedTextures[0]);
    mCreatedTextures.Clear();
  }
}

GLuint
PerFrameTexturePoolOGL::GetTexture(GLenum aTarget, GLenum)
{
  if (mTextureTarget == 0) {
    mTextureTarget = aTarget;
  }

  // The pool should always use the same texture target because it is illegal
  // to change the target of an already exisiting gl texture.
  // If we need to use several targets, a pool with several sub-pools (one per
  // target) will have to be implemented.
  // At the moment this pool is only used with tiling on b2g so we always need
  // the same target.
  MOZ_ASSERT(mTextureTarget == aTarget);

  GLuint texture = 0;

  if (!mUnusedTextures.IsEmpty()) {
    // Try to reuse one from the unused pile first
    texture = mUnusedTextures[0];
    mUnusedTextures.RemoveElementAt(0);
  } else if (mGL->MakeCurrent()) {
    // There isn't one to reuse, create one.
    mGL->fGenTextures(1, &texture);
    mGL->fBindTexture(aTarget, texture);
    mGL->fTexParameteri(aTarget, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
    mGL->fTexParameteri(aTarget, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
  }

  if (texture) {
    mCreatedTextures.AppendElement(texture);
  }

  return texture;
}

void
PerFrameTexturePoolOGL::EndFrame()
{
  if (!mGL->MakeCurrent()) {
    // this means the context got destroyed underneith us somehow, and the driver
    // already has destroyed the textures.
    mCreatedTextures.Clear();
    mUnusedTextures.Clear();
    return;
  }

  // Some platforms have issues unlocking Gralloc buffers even when they're
  // rebound.
  if (gfxPrefs::OverzealousGrallocUnlocking()) {
    mUnusedTextures.AppendElements(mCreatedTextures);
    mCreatedTextures.Clear();
  }

  // Delete unused textures
  for (size_t i = 0; i < mUnusedTextures.Length(); i++) {
    GLuint texture = mUnusedTextures[i];
    mGL->fDeleteTextures(1, &texture);
  }
  mUnusedTextures.Clear();

  // Move all created textures into the unused pile
  mUnusedTextures.AppendElements(mCreatedTextures);
  mCreatedTextures.Clear();
}

} /* layers */
} /* mozilla */
