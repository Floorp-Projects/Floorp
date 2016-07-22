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
#include "gfxCrashReporterUtils.h"      // for ScopedGfxFeatureReporter
#include "gfxEnv.h"                     // for gfxEnv
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxPrefs.h"                   // for gfxPrefs
#include "gfxRect.h"                    // for gfxRect
#include "gfxUtils.h"                   // for gfxUtils, etc
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
#include "nsAppRunner.h"
#include "nsAString.h"
#include "nsIConsoleService.h"          // for nsIConsoleService, etc
#include "nsIWidget.h"                  // for nsIWidget
#include "nsLiteralString.h"            // for NS_LITERAL_STRING
#include "nsMathUtils.h"                // for NS_roundf
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsServiceManagerUtils.h"      // for do_GetService
#include "nsString.h"                   // for nsString, nsAutoCString, etc
#include "ScopedGLHelpers.h"
#include "GLReadTexImageHelper.h"
#include "GLBlitTextureImageHelper.h"
#include "HeapCopyOfStackArray.h"

#if MOZ_WIDGET_ANDROID
#include "TexturePoolOGL.h"
#endif

#include "GeckoProfiler.h"

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
  aSourceMask->BindTexture(aTexUnit, gfx::SamplingFilter::LINEAR);
  aProgram->SetMaskTextureUnit(aTexUnit - LOCAL_GL_TEXTURE0);
  aProgram->SetMaskLayerTransform(aTransform);
}

void
CompositorOGL::BindBackdrop(ShaderProgramOGL* aProgram, GLuint aBackdrop, GLenum aTexUnit)
{
  MOZ_ASSERT(aBackdrop);

  mGLContext->fActiveTexture(aTexUnit);
  mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, aBackdrop);
  mGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  mGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  aProgram->SetBackdropTextureUnit(aTexUnit - LOCAL_GL_TEXTURE0);
}

CompositorOGL::CompositorOGL(CompositorBridgeParent* aParent,
                             widget::CompositorWidget* aWidget,
                             int aSurfaceWidth, int aSurfaceHeight,
                             bool aUseExternalSurfaceSize)
  : Compositor(aWidget, aParent)
  , mWidgetSize(-1, -1)
  , mSurfaceSize(aSurfaceWidth, aSurfaceHeight)
  , mHasBGRA(0)
  , mUseExternalSurfaceSize(aUseExternalSurfaceSize)
  , mFrameInProgress(false)
  , mDestroyed(false)
  , mViewportSize(0, 0)
  , mCurrentProgram(nullptr)
{
  MOZ_COUNT_CTOR(CompositorOGL);
}

CompositorOGL::~CompositorOGL()
{
  MOZ_COUNT_DTOR(CompositorOGL);
  Destroy();
}

already_AddRefed<mozilla::gl::GLContext>
CompositorOGL::CreateContext()
{
  RefPtr<GLContext> context;

  // Used by mock widget to create an offscreen context
  void* widgetOpenGLContext = mWidget->RealWidget()->GetNativeData(NS_NATIVE_OPENGL_CONTEXT);
  if (widgetOpenGLContext) {
    GLContext* alreadyRefed = reinterpret_cast<GLContext*>(widgetOpenGLContext);
    return already_AddRefed<GLContext>(alreadyRefed);
  }

#ifdef XP_WIN
  if (gfxEnv::LayersPreferEGL()) {
    printf_stderr("Trying GL layers...\n");
    context = gl::GLContextProviderEGL::CreateForWindow(mWidget->RealWidget(), false);
  }
#endif

  // Allow to create offscreen GL context for main Layer Manager
  if (!context && gfxEnv::LayersPreferOffscreen()) {
    SurfaceCaps caps = SurfaceCaps::ForRGB();
    caps.preserve = false;
    caps.bpp16 = gfxPlatform::GetPlatform()->GetOffscreenFormat() == SurfaceFormat::R5G6B5_UINT16;

    nsCString discardFailureId;
    context = GLContextProvider::CreateOffscreen(mSurfaceSize,
                                                 caps, CreateContextFlags::REQUIRE_COMPAT_PROFILE,
                                                 &discardFailureId);
  }

  if (!context) {
    context = gl::GLContextProvider::CreateForWindow(mWidget->RealWidget(),
                gfxPlatform::GetPlatform()->RequiresAcceleratedGLContextForCompositorOGL());
  }

  if (!context) {
    NS_WARNING("Failed to create CompositorOGL context");
  }

#ifdef MOZ_WIDGET_GONK
  mWidget->RealWidget()->SetNativeData(
    NS_NATIVE_OPENGL_CONTEXT, reinterpret_cast<uintptr_t>(context.get()));
#endif

  return context.forget();
}

void
CompositorOGL::Destroy()
{
  Compositor::Destroy();

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

#ifdef MOZ_WIDGET_GONK
  mWidget->RealWidget()->SetNativeData(
    NS_NATIVE_OPENGL_CONTEXT, reinterpret_cast<uintptr_t>(nullptr));
#endif

  RefPtr<GLContext> ctx = mGLContext->GetSharedContext();
  if (!ctx) {
    ctx = mGLContext;
  }

  if (!ctx->MakeCurrent()) {
    // Leak resources!
    mQuadVBO = 0;
    mGLContext = nullptr;
    mPrograms.clear();
    return;
  }

  for (std::map<ShaderConfigOGL, ShaderProgramOGL *>::iterator iter = mPrograms.begin();
       iter != mPrograms.end();
       iter++) {
    delete iter->second;
  }
  mPrograms.clear();

  ctx->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

  if (mQuadVBO) {
    ctx->fDeleteBuffers(1, &mQuadVBO);
    mQuadVBO = 0;
  }

  DestroyVR(ctx);

  mGLContext->MakeCurrent();

  mBlitTextureImageHelper = nullptr;

  mContextStateTracker.DestroyOGL(mGLContext);

  // On the main thread the Widget will be destroyed soon and calling MakeCurrent
  // after that could cause a crash (at least with GLX, see bug 1059793), unless
  // context is marked as destroyed.
  // There may be some textures still alive that will try to call MakeCurrent on
  // the context so let's make sure it is marked destroyed now.
  mGLContext->MarkDestroyed();

  mGLContext = nullptr;
}

bool
CompositorOGL::Initialize(nsCString* const out_failureReason)
{
  ScopedGfxFeatureReporter reporter("GL Layers");

  // Do not allow double initialization
  MOZ_ASSERT(mGLContext == nullptr, "Don't reinitialize CompositorOGL");

  mGLContext = CreateContext();

#ifdef MOZ_WIDGET_ANDROID
  if (!mGLContext){
    *out_failureReason = "FEATURE_FAILURE_OPENGL_NO_ANDROID_CONTEXT";
    NS_RUNTIMEABORT("We need a context on Android");
  }
#endif

  if (!mGLContext){
    *out_failureReason = "FEATURE_FAILURE_OPENGL_CREATE_CONTEXT";
    return false;
  }

  MakeCurrent();

  mHasBGRA =
    mGLContext->IsExtensionSupported(gl::GLContext::EXT_texture_format_BGRA8888) ||
    mGLContext->IsExtensionSupported(gl::GLContext::EXT_bgra);

  mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                 LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA);
  mGLContext->fEnable(LOCAL_GL_BLEND);

  // initialise a common shader to check that we can actually compile a shader
  RefPtr<EffectSolidColor> effect = new EffectSolidColor(Color(0, 0, 0, 0));
  ShaderConfigOGL config = GetShaderConfigFor(effect);
  if (!GetShaderProgramFor(config)) {
    *out_failureReason = "FEATURE_FAILURE_OPENGL_COMPILE_SHADER";
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
      *out_failureReason = "FEATURE_FAILURE_OPENGL_NO_TEXTURE_TARGET";
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
    if (!mGLContext->IsExtensionSupported(gl::GLContext::ARB_texture_rectangle)){
      *out_failureReason = "FEATURE_FAILURE_OPENGL_ARB_EXT";
      return false;
    }
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

  mVR.mInitialized = false;
  if (gfxPrefs::VREnabled()) {
    if (!InitializeVR()) {
      NS_WARNING("Failed to initialize VR in CompositorOGL");
    }
  }

  reporter.SetSuccessful();

  return true;
}

/*
 * Returns a size that is equal to, or larger than and closest to,
 * aSize where both width and height are powers of two.
 * If the OpenGL setup is capable of using non-POT textures,
 * then it will just return aSize.
 */
static IntSize
CalculatePOTSize(const IntSize& aSize, GLContext* gl)
{
  if (CanUploadNonPowerOfTwo(gl))
    return aSize;

  return IntSize(RoundUpPow2(aSize.width), RoundUpPow2(aSize.height));
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
  Rect scaledTexCoordRect = aTexCoordRect;

  // If the OpenGL setup does not support non-power-of-two textures then the
  // texture's width and height will have been increased to the next
  // power-of-two (unless already a power of two). In that case we must scale
  // the texture coordinates to account for that.
  if (!CanUploadNonPowerOfTwo(mGLContext)) {
    const IntSize& textureSize = aTexture->GetSize();
    const IntSize potSize = CalculatePOTSize(textureSize, mGLContext);
    if (potSize != textureSize) {
      const float xScale = (float)textureSize.width / (float)potSize.width;
      const float yScale = (float)textureSize.height / (float)potSize.height;
      scaledTexCoordRect.Scale(xScale, yScale);
    }
  }

  Rect layerRects[4];
  Rect textureRects[4];
  size_t rects = DecomposeIntoNoRepeatRects(aRect,
                                            scaledTexCoordRect,
                                            &layerRects,
                                            &textureRects);

  BindAndDrawQuads(aProg, rects, layerRects, textureRects);
}

void
CompositorOGL::PrepareViewport(CompositingRenderTargetOGL* aRenderTarget)
{
  MOZ_ASSERT(aRenderTarget);
  const gfx::IntSize& size = aRenderTarget->mInitParams.mSize;

  // Set the viewport correctly.
  mGLContext->fViewport(0, 0, size.width, size.height);

  mViewportSize = size;

  if (!aRenderTarget->HasComplexProjection()) {
    // We flip the view matrix around so that everything is right-side up; we're
    // drawing directly into the window's back buffer, so this keeps things
    // looking correct.
    // XXX: We keep track of whether the window size changed, so we could skip
    // this update if it hadn't changed since the last call.

    // Matrix to transform (0, 0, aWidth, aHeight) to viewport space (-1.0, 1.0,
    // 2, 2) and flip the contents.
    Matrix viewMatrix;
    if (mGLContext->IsOffscreen() && !gIsGtest) {
      // In case of rendering via GL Offscreen context, disable Y-Flipping
      viewMatrix.PreTranslate(-1.0, -1.0);
      viewMatrix.PreScale(2.0f / float(size.width), 2.0f / float(size.height));
    } else {
      viewMatrix.PreTranslate(-1.0, 1.0);
      viewMatrix.PreScale(2.0f / float(size.width), 2.0f / float(size.height));
      viewMatrix.PreScale(1.0f, -1.0f);
    }

    MOZ_ASSERT(mCurrentRenderTarget, "No destination");
    // If we're drawing directly to the window then we want to offset
    // drawing by the render offset.
    if (!mTarget && mCurrentRenderTarget->IsWindow()) {
      viewMatrix.PreTranslate(mRenderOffset.x, mRenderOffset.y);
    }

    Matrix4x4 matrix3d = Matrix4x4::From2D(viewMatrix);
    matrix3d._33 = 0.0f;
    mProjMatrix = matrix3d;
    mGLContext->fDepthRange(0.0f, 1.0f);
  } else {
    // XXX take into account mRenderOffset
    bool depthEnable;
    float zNear, zFar;
    aRenderTarget->GetProjection(mProjMatrix, depthEnable, zNear, zFar);
    mGLContext->fDepthRange(zNear, zFar);
  }
}

already_AddRefed<CompositingRenderTarget>
CompositorOGL::CreateRenderTarget(const IntRect &aRect, SurfaceInitMode aInit)
{
  MOZ_ASSERT(aRect.width != 0 && aRect.height != 0, "Trying to create a render target of invalid size");

  if (aRect.width * aRect.height == 0) {
    return nullptr;
  }

  if (!gl()) {
    // CompositingRenderTargetOGL does not work without a gl context.
    return nullptr;
  }

  GLuint tex = 0;
  GLuint fbo = 0;
  CreateFBOWithTexture(aRect, false, 0, &fbo, &tex);
  RefPtr<CompositingRenderTargetOGL> surface
    = new CompositingRenderTargetOGL(this, aRect.TopLeft(), tex, fbo);
  surface->Initialize(aRect.Size(), mFBOTextureTarget, aInit);
  return surface.forget();
}

already_AddRefed<CompositingRenderTarget>
CompositorOGL::CreateRenderTargetFromSource(const IntRect &aRect,
                                            const CompositingRenderTarget *aSource,
                                            const IntPoint &aSourcePoint)
{
  MOZ_ASSERT(aRect.width != 0 && aRect.height != 0, "Trying to create a render target of invalid size");

  if (aRect.width * aRect.height == 0) {
    return nullptr;
  }

  if (!gl()) {
    return nullptr;
  }

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
    mCurrentRenderTarget = surface;
    if (mCurrentRenderTarget) {
      mContextStateTracker.PopOGLSection(gl(), "Frame");
    }
    mContextStateTracker.PushOGLSection(gl(), "Frame");
    surface->BindRenderTarget();
  }

  PrepareViewport(mCurrentRenderTarget);
}

CompositingRenderTarget*
CompositorOGL::GetCurrentRenderTarget() const
{
  return mCurrentRenderTarget;
}

static GLenum
GetFrameBufferInternalFormat(GLContext* gl,
                             GLuint aFrameBuffer,
                             mozilla::widget::CompositorWidget* aWidget)
{
  if (aFrameBuffer == 0) { // default framebuffer
    return aWidget->GetGLFrameBufferFormat();
  }
  return LOCAL_GL_RGBA;
}

void
CompositorOGL::ClearRect(const gfx::Rect& aRect)
{
  // Map aRect to OGL coordinates, origin:bottom-left
  GLint y = mViewportSize.height - (aRect.y + aRect.height);

  ScopedGLState scopedScissorTestState(mGLContext, LOCAL_GL_SCISSOR_TEST, true);
  ScopedScissorRect autoScissorRect(mGLContext, aRect.x, y, aRect.width, aRect.height);
  mGLContext->fClearColor(0.0, 0.0, 0.0, 0.0);
  mGLContext->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT);
}

void
CompositorOGL::BeginFrame(const nsIntRegion& aInvalidRegion,
                          const IntRect *aClipRectIn,
                          const IntRect& aRenderBounds,
                          const nsIntRegion& aOpaqueRegion,
                          IntRect *aClipRectOut,
                          IntRect *aRenderBoundsOut)
{
  PROFILER_LABEL("CompositorOGL", "BeginFrame",
    js::ProfileEntry::Category::GRAPHICS);

  MOZ_ASSERT(!mFrameInProgress, "frame still in progress (should have called EndFrame");

  gfx::IntRect rect;
  if (mUseExternalSurfaceSize) {
    rect = gfx::IntRect(0, 0, mSurfaceSize.width, mSurfaceSize.height);
  } else {
    rect = gfx::IntRect(aRenderBounds.x, aRenderBounds.y, aRenderBounds.width, aRenderBounds.height);
  }

  if (aRenderBoundsOut) {
    *aRenderBoundsOut = rect;
  }

  GLint width = rect.width;
  GLint height = rect.height;

  // We can't draw anything to something with no area
  // so just return
  if (width == 0 || height == 0)
    return;

  // We're about to actually draw a frame.
  mFrameInProgress = true;

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

  // Default blend function implements "OVER"
  mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                 LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA);
  mGLContext->fEnable(LOCAL_GL_BLEND);

  // Make sure SCISSOR is enabled before setting the render target, since the RT
  // assumes scissor is enabled while it does clears.
  mGLContext->fEnable(LOCAL_GL_SCISSOR_TEST);

  // Prefer the native windowing system's provided window size for the viewport.
  IntSize viewportSize =
    mGLContext->GetTargetSize().valueOr(mWidgetSize.ToUnknownSize());
  if (viewportSize != mWidgetSize.ToUnknownSize()) {
    mGLContext->fScissor(0, 0, viewportSize.width, viewportSize.height);
  }

  RefPtr<CompositingRenderTargetOGL> rt =
    CompositingRenderTargetOGL::RenderTargetForWindow(this, viewportSize);
  SetRenderTarget(rt);

#ifdef DEBUG
  mWindowRenderTarget = mCurrentRenderTarget;
#endif

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
CompositorOGL::CreateFBOWithTexture(const gfx::IntRect& aRect, bool aCopyFromSource,
                                    GLuint aSourceFrameBuffer,
                                    GLuint *aFBO, GLuint *aTexture)
{
  *aTexture = CreateTexture(aRect, aCopyFromSource, aSourceFrameBuffer);
  mGLContext->fGenFramebuffers(1, aFBO);
}

GLuint
CompositorOGL::CreateTexture(const IntRect& aRect, bool aCopyFromSource, GLuint aSourceFrameBuffer)
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

  GLuint tex;

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
      auto buf = MakeUnique<uint8_t[]>(bufferSize);

      mGLContext->fReadPixels(clampedRect.x, clampedRect.y,
                              clampedRect.width, clampedRect.height,
                              LOCAL_GL_RGBA,
                              LOCAL_GL_UNSIGNED_BYTE,
                              buf.get());
      mGLContext->fTexImage2D(mFBOTextureTarget,
                              0,
                              LOCAL_GL_RGBA,
                              clampedRect.width, clampedRect.height,
                              0,
                              LOCAL_GL_RGBA,
                              LOCAL_GL_UNSIGNED_BYTE,
                              buf.get());
    }

    GLenum error = mGLContext->fGetError();
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

  return tex;
}

ShaderConfigOGL
CompositorOGL::GetShaderConfigFor(Effect *aEffect,
                                  MaskType aMask,
                                  gfx::CompositionOp aOp,
                                  bool aColorMatrix,
                                  bool aDEAAEnabled) const
{
  ShaderConfigOGL config;

  switch(aEffect->mType) {
  case EffectTypes::SOLID_COLOR:
    config.SetRenderColor(true);
    break;
  case EffectTypes::YCBCR:
    config.SetYCbCr(true);
    break;
  case EffectTypes::NV12:
    config.SetNV12(true);
    config.SetTextureTarget(LOCAL_GL_TEXTURE_RECTANGLE_ARB);
    break;
  case EffectTypes::COMPONENT_ALPHA:
  {
    config.SetComponentAlpha(true);
    EffectComponentAlpha* effectComponentAlpha =
      static_cast<EffectComponentAlpha*>(aEffect);
    gfx::SurfaceFormat format = effectComponentAlpha->mOnWhite->GetFormat();
    config.SetRBSwap(format == gfx::SurfaceFormat::B8G8R8A8 ||
                     format == gfx::SurfaceFormat::B8G8R8X8);
    TextureSourceOGL* source = effectComponentAlpha->mOnWhite->AsSourceOGL();
    config.SetTextureTarget(source->GetTextureTarget());
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
                  source->GetFormat() == gfx::SurfaceFormat::R8G8B8A8 ||
                  source->GetFormat() == gfx::SurfaceFormat::R8G8B8X8);
    MOZ_ASSERT_IF(source->GetTextureTarget() == LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                  source->GetFormat() == gfx::SurfaceFormat::R8G8B8A8 ||
                  source->GetFormat() == gfx::SurfaceFormat::R8G8B8X8 ||
                  source->GetFormat() == gfx::SurfaceFormat::R5G6B5_UINT16 ||
                  source->GetFormat() == gfx::SurfaceFormat::YUV422 );
    config = ShaderConfigFromTargetAndFormat(source->GetTextureTarget(),
                                             source->GetFormat());
    if (!texturedEffect->mPremultiplied) {
      config.SetNoPremultipliedAlpha();
    }
    break;
  }
  }
  config.SetColorMatrix(aColorMatrix);
  config.SetMask(aMask == MaskType::Mask);
  config.SetDEAA(aDEAAEnabled);
  config.SetCompositionOp(aOp);
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

void
CompositorOGL::ActivateProgram(ShaderProgramOGL* aProg)
{
  if (mCurrentProgram != aProg) {
    gl()->fUseProgram(aProg->GetProgram());
    mCurrentProgram = aProg;
  }
}

void
CompositorOGL::ResetProgram()
{
  mCurrentProgram = nullptr;
}

static bool SetBlendMode(GLContext* aGL, gfx::CompositionOp aBlendMode, bool aIsPremultiplied = true)
{
  if (BlendOpIsMixBlendMode(aBlendMode)) {
    // Mix-blend modes require an extra step (or more) that cannot be expressed
    // in the fixed-function blending capabilities of opengl. We handle them
    // separately in shaders, and the shaders assume we will use our default
    // blend function for compositing (premultiplied OP_OVER).
    return false;
  }
  if (aBlendMode == gfx::CompositionOp::OP_OVER && aIsPremultiplied) {
    return false;
  }

  GLenum srcBlend;
  GLenum dstBlend;
  GLenum srcAlphaBlend = LOCAL_GL_ONE;
  GLenum dstAlphaBlend = LOCAL_GL_ONE_MINUS_SRC_ALPHA;

  switch (aBlendMode) {
    case gfx::CompositionOp::OP_OVER:
      MOZ_ASSERT(!aIsPremultiplied);
      srcBlend = LOCAL_GL_SRC_ALPHA;
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

gfx::Point3D
CompositorOGL::GetLineCoefficients(const gfx::Point& aPoint1,
                                   const gfx::Point& aPoint2)
{
  // Return standard coefficients for a line between aPoint1 and aPoint2
  // for standard line equation:
  //
  // Ax + By + C = 0
  //
  // A = (p1.y – p2.y)
  // B = (p2.x – p1.x)
  // C = (p1.x * p2.y) – (p2.x * p1.y)

  gfx::Point3D coeffecients;
  coeffecients.x = aPoint1.y - aPoint2.y;
  coeffecients.y = aPoint2.x - aPoint1.x;
  coeffecients.z = aPoint1.x * aPoint2.y - aPoint2.x * aPoint1.y;

  coeffecients *= 1.0f / sqrtf(coeffecients.x * coeffecients.x +
                               coeffecients.y * coeffecients.y);

  // Offset outwards by 0.5 pixel as the edge is considered to be 1 pixel
  // wide and included within the interior of the polygon
  coeffecients.z += 0.5f;

  return coeffecients;
}

void
CompositorOGL::DrawQuad(const Rect& aRect,
                        const IntRect& aClipRect,
                        const EffectChain &aEffectChain,
                        Float aOpacity,
                        const gfx::Matrix4x4& aTransform,
                        const gfx::Rect& aVisibleRect)
{
  PROFILER_LABEL("CompositorOGL", "DrawQuad",
    js::ProfileEntry::Category::GRAPHICS);

  MOZ_ASSERT(mFrameInProgress, "frame not started");
  MOZ_ASSERT(mCurrentRenderTarget, "No destination");

  if (aEffectChain.mPrimaryEffect->mType == EffectTypes::VR_DISTORTION) {
    DrawVRDistortion(aRect, aClipRect, aEffectChain, aOpacity, aTransform);
    return;
  }

  MakeCurrent();

  IntPoint offset = mCurrentRenderTarget->GetOrigin();
  IntSize size = mCurrentRenderTarget->GetSize();

  Rect renderBound(0, 0, size.width, size.height);
  renderBound.IntersectRect(renderBound, Rect(aClipRect));
  renderBound.MoveBy(offset);

  Rect destRect = aTransform.TransformAndClipBounds(aRect, renderBound);

  // XXX: This doesn't handle 3D transforms. It also doesn't handled rotated
  //      quads. Fix me.
  mPixelsFilled += destRect.width * destRect.height;

  // Do a simple culling if this rect is out of target buffer.
  // Inflate a small size to avoid some numerical imprecision issue.
  destRect.Inflate(1, 1);
  destRect.MoveBy(-offset);
  renderBound = Rect(0, 0, size.width, size.height);
  if (!renderBound.Intersects(destRect)) {
    return;
  }

  LayerScope::DrawBegin();

  IntRect clipRect = aClipRect;
  // aClipRect is in destination coordinate space (after all
  // transforms and offsets have been applied) so if our
  // drawing is going to be shifted by mRenderOffset then we need
  // to shift the clip rect by the same amount.
  if (!mTarget && mCurrentRenderTarget->IsWindow()) {
    clipRect.MoveBy(mRenderOffset.x, mRenderOffset.y);
  }

  gl()->fScissor(clipRect.x, FlipY(clipRect.y + clipRect.height),
                 clipRect.width, clipRect.height);

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

    maskType = MaskType::Mask;
  } else {
    maskType = MaskType::MaskNone;
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

  bool createdMixBlendBackdropTexture = false;
  GLuint mixBlendBackdrop = 0;
  gfx::CompositionOp blendMode = gfx::CompositionOp::OP_OVER;

  if (aEffectChain.mSecondaryEffects[EffectTypes::BLEND_MODE]) {
    EffectBlendMode *blendEffect =
      static_cast<EffectBlendMode*>(aEffectChain.mSecondaryEffects[EffectTypes::BLEND_MODE].get());
    blendMode = blendEffect->mBlendMode;
  }

  // Only apply DEAA to quads that have been transformed such that aliasing
  // could be visible
  bool bEnableAA = gfxPrefs::LayersDEAAEnabled() &&
                   !aTransform.Is2DIntegerTranslation();

  bool colorMatrix = aEffectChain.mSecondaryEffects[EffectTypes::COLOR_MATRIX];
  ShaderConfigOGL config = GetShaderConfigFor(aEffectChain.mPrimaryEffect,
                                              maskType, blendMode, colorMatrix,
                                              bEnableAA);
  config.SetOpacity(aOpacity != 1.f);
  ShaderProgramOGL *program = GetShaderProgramFor(config);
  ActivateProgram(program);
  program->SetProjectionMatrix(mProjMatrix);
  program->SetLayerTransform(aTransform);
  LayerScope::SetLayerTransform(aTransform);
  if (colorMatrix) {
      EffectColorMatrix* effectColorMatrix =
        static_cast<EffectColorMatrix*>(aEffectChain.mSecondaryEffects[EffectTypes::COLOR_MATRIX].get());
      program->SetColorMatrix(effectColorMatrix->mColorMatrix);
  }

  if (BlendOpIsMixBlendMode(blendMode)) {
    gfx::Matrix4x4 backdropTransform;

    if (gl()->IsExtensionSupported(GLContext::NV_texture_barrier)) {
      // The NV_texture_barrier extension lets us read directly from the
      // backbuffer. Let's do that.
      // We need to tell OpenGL about this, so that it can make sure everything
      // on the GPU is happening in the right order.
      gl()->fTextureBarrier();
      mixBlendBackdrop = mCurrentRenderTarget->GetTextureHandle();
    } else {
      gfx::IntRect rect = ComputeBackdropCopyRect(aRect, aClipRect, aTransform, &backdropTransform);
      mixBlendBackdrop = CreateTexture(rect, true, mCurrentRenderTarget->GetFBO());
      createdMixBlendBackdropTexture = true;
    }
    program->SetBackdropTransform(backdropTransform);
  }

  program->SetRenderOffset(offset.x, offset.y);
  LayerScope::SetRenderOffset(offset.x, offset.y);

  if (aOpacity != 1.f)
    program->SetLayerOpacity(aOpacity);
  if (config.mFeatures & ENABLE_TEXTURE_RECT) {
    TextureSourceOGL* source = nullptr;
    if (aEffectChain.mPrimaryEffect->mType == EffectTypes::COMPONENT_ALPHA) {
      EffectComponentAlpha* effectComponentAlpha =
        static_cast<EffectComponentAlpha*>(aEffectChain.mPrimaryEffect.get());
      source = effectComponentAlpha->mOnWhite->AsSourceOGL();
    } else {
      TexturedEffect* texturedEffect =
        static_cast<TexturedEffect*>(aEffectChain.mPrimaryEffect.get());
      source = texturedEffect->mTexture->AsSourceOGL();
    }
    // This is used by IOSurface that use 0,0...w,h coordinate rather then 0,0..1,1.
    program->SetTexCoordMultiplier(source->GetSize().width, source->GetSize().height);
  }

  // XXX kip - These calculations could be performed once per layer rather than
  //           for every tile.  This might belong in Compositor.cpp once DEAA
  //           is implemented for DirectX.
  if (bEnableAA) {
    // Calculate the transformed vertices of aVisibleRect in screen space
    // pixels, mirroring the calculations in the vertex shader
    Matrix4x4 flatTransform = aTransform;
    flatTransform.PostTranslate(-offset.x, -offset.y, 0.0f);
    flatTransform *= mProjMatrix;

    Rect viewportClip = Rect(-1.0f, -1.0f, 2.0f, 2.0f);
    size_t edgeCount = 0;
    Point3D coefficients[4];

    Point points[Matrix4x4::kTransformAndClipRectMaxVerts];
    size_t pointCount = flatTransform.TransformAndClipRect(aVisibleRect, viewportClip, points);
    for (size_t i = 0; i < pointCount; i++) {
      points[i] = Point((points[i].x * 0.5f + 0.5f) * mViewportSize.width,
                        (points[i].y * 0.5f + 0.5f) * mViewportSize.height);
    }
    if (pointCount > 2) {
      // Use shoelace formula on a triangle in the clipped quad to determine if
      // winding order is reversed.  Iterate through the triangles until one is
      // found with a non-zero area.
      float winding = 0.0f;
      size_t wp = 0;
      while (winding == 0.0f && wp < pointCount) {
        int wp1 = (wp + 1) % pointCount;
        int wp2 = (wp + 2) % pointCount;
        winding = (points[wp1].x - points[wp].x) * (points[wp1].y + points[wp].y) +
                  (points[wp2].x - points[wp1].x) * (points[wp2].y + points[wp1].y) +
                  (points[wp].x - points[wp2].x) * (points[wp].y + points[wp2].y);
        wp++;
      }
      bool frontFacing = winding >= 0.0f;

      // Calculate the line coefficients used by the DEAA shader to determine the
      // sub-pixel coverage of the edge pixels
      for (size_t i=0; i<pointCount; i++) {
        const Point& p1 = points[i];
        const Point& p2 = points[(i + 1) % pointCount];
        // Create a DEAA edge for any non-straight lines, to a maximum of 4
        if (p1.x != p2.x && p1.y != p2.y && edgeCount < 4) {
          if (frontFacing) {
            coefficients[edgeCount++] = GetLineCoefficients(p2, p1);
          } else {
            coefficients[edgeCount++] = GetLineCoefficients(p1, p2);
          }
        }
      }
    }

    // The coefficients that are not needed must not cull any fragments.
    // We fill these unused coefficients with a clipping plane that has no
    // effect.
    for (size_t i = edgeCount; i < 4; i++) {
      coefficients[i] = Point3D(0.0f, 1.0f, mViewportSize.height);
    }

    // Set uniforms required by DEAA shader
    Matrix4x4 transformInverted = aTransform;
    transformInverted.Invert();
    program->SetLayerTransformInverse(transformInverted);
    program->SetDEAAEdges(coefficients);
    program->SetVisibleCenter(aVisibleRect.Center());
    program->SetViewportSize(mViewportSize);
  }

  bool didSetBlendMode = false;

  switch (aEffectChain.mPrimaryEffect->mType) {
    case EffectTypes::SOLID_COLOR: {
      program->SetRenderColor(color);

      if (maskType != MaskType::MaskNone) {
        BindMaskForProgram(program, sourceMask, LOCAL_GL_TEXTURE0, maskQuadTransform);
      }
      if (mixBlendBackdrop) {
        BindBackdrop(program, mixBlendBackdrop, LOCAL_GL_TEXTURE1);
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

      gfx::SamplingFilter samplingFilter = texturedEffect->mSamplingFilter;

      source->AsSourceOGL()->BindTexture(LOCAL_GL_TEXTURE0, samplingFilter);

      program->SetTextureUnit(0);

      Matrix4x4 textureTransform = source->AsSourceOGL()->GetTextureTransform();
      program->SetTextureTransform(textureTransform);

      if (maskType != MaskType::MaskNone) {
        BindMaskForProgram(program, sourceMask, LOCAL_GL_TEXTURE1, maskQuadTransform);
      }
      if (mixBlendBackdrop) {
        BindBackdrop(program, mixBlendBackdrop, LOCAL_GL_TEXTURE2);
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

      if (!sourceY || !sourceCb || !sourceCr) {
        NS_WARNING("Invalid layer texture.");
        return;
      }

      sourceY->BindTexture(LOCAL_GL_TEXTURE0, effectYCbCr->mSamplingFilter);
      sourceCb->BindTexture(LOCAL_GL_TEXTURE1, effectYCbCr->mSamplingFilter);
      sourceCr->BindTexture(LOCAL_GL_TEXTURE2, effectYCbCr->mSamplingFilter);

      program->SetYCbCrTextureUnits(Y, Cb, Cr);
      program->SetTextureTransform(Matrix4x4());

      if (maskType != MaskType::MaskNone) {
        BindMaskForProgram(program, sourceMask, LOCAL_GL_TEXTURE3, maskQuadTransform);
      }
      if (mixBlendBackdrop) {
        BindBackdrop(program, mixBlendBackdrop, LOCAL_GL_TEXTURE4);
      }
      didSetBlendMode = SetBlendMode(gl(), blendMode);
      BindAndDrawQuadWithTextureRect(program,
                                     aRect,
                                     effectYCbCr->mTextureCoords,
                                     sourceYCbCr->GetSubSource(Y));
    }
    break;
  case EffectTypes::NV12: {
      EffectNV12* effectNV12 =
        static_cast<EffectNV12*>(aEffectChain.mPrimaryEffect.get());
      TextureSource* sourceNV12 = effectNV12->mTexture;
      const int Y = 0, CbCr = 1;
      TextureSourceOGL* sourceY =  sourceNV12->GetSubSource(Y)->AsSourceOGL();
      TextureSourceOGL* sourceCbCr = sourceNV12->GetSubSource(CbCr)->AsSourceOGL();

      if (!sourceY || !sourceCbCr) {
        NS_WARNING("Invalid layer texture.");
        return;
      }

      sourceY->BindTexture(LOCAL_GL_TEXTURE0, effectNV12->mSamplingFilter);
      sourceCbCr->BindTexture(LOCAL_GL_TEXTURE1, effectNV12->mSamplingFilter);

      if (config.mFeatures & ENABLE_TEXTURE_RECT) {
        // This is used by IOSurface that use 0,0...w,h coordinate rather then 0,0..1,1.
        program->SetCbCrTexCoordMultiplier(sourceCbCr->GetSize().width, sourceCbCr->GetSize().height);
      }

      program->SetNV12TextureUnits(Y, CbCr);
      program->SetTextureTransform(Matrix4x4());

      if (maskType != MaskType::MaskNone) {
        BindMaskForProgram(program, sourceMask, LOCAL_GL_TEXTURE2, maskQuadTransform);
      }
      if (mixBlendBackdrop) {
        BindBackdrop(program, mixBlendBackdrop, LOCAL_GL_TEXTURE3);
      }
      didSetBlendMode = SetBlendMode(gl(), blendMode);
      BindAndDrawQuadWithTextureRect(program,
                                     aRect,
                                     effectNV12->mTextureCoords,
                                     sourceNV12->GetSubSource(Y));
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
      transform.PreTranslate(0.0, 1.0);
      transform.PreScale(1.0f, -1.0f);
      program->SetTextureTransform(Matrix4x4::From2D(transform));
      program->SetTextureUnit(0);

      if (maskType != MaskType::MaskNone) {
        BindMaskForProgram(program, sourceMask, LOCAL_GL_TEXTURE1, maskQuadTransform);
      }
      if (mixBlendBackdrop) {
        BindBackdrop(program, mixBlendBackdrop, LOCAL_GL_TEXTURE2);
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

      sourceOnBlack->BindTexture(LOCAL_GL_TEXTURE0, effectComponentAlpha->mSamplingFilter);
      sourceOnWhite->BindTexture(LOCAL_GL_TEXTURE1, effectComponentAlpha->mSamplingFilter);

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
      program->SetTexturePass2(true);
      BindAndDrawQuadWithTextureRect(program,
                                     aRect,
                                     effectComponentAlpha->mTextureCoords,
                                     effectComponentAlpha->mOnBlack);

      mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                     LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA);
    }
    break;
  default:
    MOZ_ASSERT(false, "Unhandled effect type");
    break;
  }

  if (didSetBlendMode) {
    gl()->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                             LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA);
  }
  if (createdMixBlendBackdropTexture) {
    gl()->fDeleteTextures(1, &mixBlendBackdrop);
  }

  // in case rendering has used some other GL context
  MakeCurrent();
  LayerScope::DrawEnd(mGLContext, aEffectChain, aRect.width, aRect.height);
}

void
CompositorOGL::EndFrame()
{
  PROFILER_LABEL("CompositorOGL", "EndFrame",
    js::ProfileEntry::Category::GRAPHICS);

  MOZ_ASSERT(mCurrentRenderTarget == mWindowRenderTarget, "Rendering target not properly restored");

  Compositor::EndFrame();

#ifdef MOZ_DUMP_PAINTING
  if (gfxEnv::DumpCompositorTextures()) {
    LayoutDeviceIntSize size;
    if (mUseExternalSurfaceSize) {
      size = LayoutDeviceIntSize(mSurfaceSize.width, mSurfaceSize.height);
    } else {
      size = mWidget->GetClientSize();
    }
    RefPtr<DrawTarget> target = gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(IntSize(size.width, size.height), SurfaceFormat::B8G8R8A8);
    if (target) {
      CopyToTarget(target, nsIntPoint(), Matrix());
      WriteSnapshotToDumpFile(this, target);
    }
  }
#endif

  mContextStateTracker.PopOGLSection(gl(), "Frame");

  mFrameInProgress = false;

  if (mTarget) {
    CopyToTarget(mTarget, mTargetBounds.TopLeft(), Matrix());
    mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
    mCurrentRenderTarget = nullptr;
    return;
  }

  if (mTexturePool) {
    mTexturePool->EndFrame();
  }

  // If our window size changed during composition, we should discard the frame.
  // We don't need to worry about rescheduling a composite, as widget
  // implementations handle this in their expose event listeners.
  // See bug 1184534. TODO: implement this for single-buffered targets?
  IntSize targetSize = mGLContext->GetTargetSize().valueOr(mViewportSize);
  if (!(mCurrentRenderTarget->IsWindow() && targetSize != mViewportSize)) {
    mGLContext->SwapBuffers();
  }

  mCurrentRenderTarget = nullptr;

  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);

  // Unbind all textures
  for (GLuint i = 0; i <= 4; i++) {
    mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0 + i);
    mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, 0);
    if (!mGLContext->IsGLES()) {
      mGLContext->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, 0);
    }
  }
}

void
CompositorOGL::EndFrameForExternalComposition(const gfx::Matrix& aTransform)
{
  MOZ_ASSERT(!mTarget);
  if (mTexturePool) {
    mTexturePool->EndFrame();
  }
}

void
CompositorOGL::SetDestinationSurfaceSize(const IntSize& aSize)
{
  mSurfaceSize.width = aSize.width;
  mSurfaceSize.height = aSize.height;
}

void
CompositorOGL::CopyToTarget(DrawTarget* aTarget, const nsIntPoint& aTopLeft, const gfx::Matrix& aTransform)
{
  MOZ_ASSERT(aTarget);
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
  glToCairoTransform.PreScale(1.0, -1.0);
  glToCairoTransform.PreTranslate(0.0, -height);

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
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_UIKIT)
  if (!gl() || gl()->IsDestroyed())
    return false;

  // RenewSurface internally calls MakeCurrent.
  return gl()->RenewSurface(GetWidget()->RealWidget());
#endif
  return true;
}

already_AddRefed<DataTextureSource>
CompositorOGL::CreateDataTextureSource(TextureFlags aFlags)
{
  return MakeAndAddRef<TextureImageTextureSourceOGL>(this, aFlags);
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
  LayerScope::SetDrawRects(aQuads, aLayerRects, aTextureRects);
}

GLBlitTextureImageHelper*
CompositorOGL::BlitTextureImageHelper()
{
    if (!mBlitTextureImageHelper) {
        mBlitTextureImageHelper = MakeUnique<GLBlitTextureImageHelper>(this);
    }

    return mBlitTextureImageHelper.get();
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

} // namespace layers
} // namespace mozilla
