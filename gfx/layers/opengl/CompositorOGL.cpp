/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorOGL.h"
#include <stddef.h>             // for size_t
#include <stdint.h>             // for uint32_t, uint8_t
#include <stdlib.h>             // for free, malloc
#include "GLContextProvider.h"  // for GLContextProvider
#include "GLContext.h"          // for GLContext
#include "GLUploadHelpers.h"
#include "gfxCrashReporterUtils.h"  // for ScopedGfxFeatureReporter
#include "gfxEnv.h"                 // for gfxEnv
#include "gfxPlatform.h"            // for gfxPlatform
#include "gfxRect.h"                // for gfxRect
#include "gfxUtils.h"               // for gfxUtils, etc
#include "mozilla/ArrayUtils.h"     // for ArrayLength
#include "mozilla/Preferences.h"    // for Preferences
#include "mozilla/ProfilerLabels.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/StaticPrefs_nglayout.h"
#include "mozilla/gfx/BasePoint.h"  // for BasePoint
#include "mozilla/gfx/Matrix.h"     // for Matrix4x4, Matrix
#include "mozilla/gfx/Triangle.h"   // for Triangle
#include "mozilla/gfx/gfxVars.h"    // for gfxVars
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/NativeLayer.h"
#include "mozilla/layers/CompositingRenderTargetOGL.h"
#include "mozilla/layers/Effects.h"      // for EffectChain, TexturedEffect, etc
#include "mozilla/layers/TextureHost.h"  // for TextureSource, etc
#include "mozilla/layers/TextureHostOGL.h"  // for TextureSourceOGL, etc
#include "mozilla/layers/PTextureParent.h"  // for OtherPid() on PTextureParent
#include "mozilla/mozalloc.h"               // for operator delete, etc
#include "nsAppRunner.h"
#include "nsAString.h"
#include "nsClassHashtable.h"
#include "nsIConsoleService.h"      // for nsIConsoleService, etc
#include "nsIWidget.h"              // for nsIWidget
#include "nsLiteralString.h"        // for NS_LITERAL_STRING
#include "nsMathUtils.h"            // for NS_roundf
#include "nsRect.h"                 // for mozilla::gfx::IntRect
#include "nsServiceManagerUtils.h"  // for do_GetService
#include "nsString.h"               // for nsString, nsAutoCString, etc
#include "OGLShaderProgram.h"       // for ShaderProgramOGL, etc
#include "ScopedGLHelpers.h"
#include "GLReadTexImageHelper.h"
#include "HeapCopyOfStackArray.h"
#include "GLBlitHelper.h"
#include "mozilla/gfx/Swizzle.h"
#ifdef MOZ_WIDGET_GTK
#  include "mozilla/widget/GtkCompositorWidget.h"
#endif
#if MOZ_WIDGET_ANDROID
#  include "GLContextEGL.h"
#  include "GLLibraryEGL.h"
#  include "mozilla/java/GeckoSurfaceTextureWrappers.h"
#  include "mozilla/layers/AndroidHardwareBuffer.h"
#endif

namespace mozilla {

using namespace gfx;

namespace layers {

using namespace mozilla::gl;

static const GLuint kCoordinateAttributeIndex = 0;
static const GLuint kTexCoordinateAttributeIndex = 1;

class DummyTextureSourceOGL : public TextureSource, public TextureSourceOGL {
 public:
  const char* Name() const override { return "DummyTextureSourceOGL"; }

  TextureSourceOGL* AsSourceOGL() override { return this; }

  void BindTexture(GLenum activetex,
                   gfx::SamplingFilter aSamplingFilter) override {}

  bool IsValid() const override { return false; }

  gfx::IntSize GetSize() const override { return gfx::IntSize(); }

  gfx::SurfaceFormat GetFormat() const override {
    return gfx::SurfaceFormat::B8G8R8A8;
  }

  GLenum GetWrapMode() const override { return LOCAL_GL_CLAMP_TO_EDGE; }

 protected:
};

class AsyncReadbackBufferOGL final : public AsyncReadbackBuffer {
 public:
  AsyncReadbackBufferOGL(GLContext* aGL, const IntSize& aSize);

  bool MapAndCopyInto(DataSourceSurface* aSurface,
                      const IntSize& aReadSize) const override;

  void Bind() const {
    mGL->fBindBuffer(LOCAL_GL_PIXEL_PACK_BUFFER, mBufferHandle);
    mGL->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, 1);
  }

 protected:
  virtual ~AsyncReadbackBufferOGL();

 private:
  GLContext* mGL;
  GLuint mBufferHandle;
};

AsyncReadbackBufferOGL::AsyncReadbackBufferOGL(GLContext* aGL,
                                               const IntSize& aSize)
    : AsyncReadbackBuffer(aSize), mGL(aGL), mBufferHandle(0) {
  size_t bufferByteCount = mSize.width * mSize.height * 4;
  mGL->fGenBuffers(1, &mBufferHandle);

  ScopedPackState scopedPackState(mGL);
  Bind();
  mGL->fBufferData(LOCAL_GL_PIXEL_PACK_BUFFER, bufferByteCount, nullptr,
                   LOCAL_GL_STREAM_READ);
}

AsyncReadbackBufferOGL::~AsyncReadbackBufferOGL() {
  if (mGL && mGL->MakeCurrent()) {
    mGL->fDeleteBuffers(1, &mBufferHandle);
  }
}

bool AsyncReadbackBufferOGL::MapAndCopyInto(DataSourceSurface* aSurface,
                                            const IntSize& aReadSize) const {
  MOZ_RELEASE_ASSERT(aReadSize <= aSurface->GetSize());

  if (!mGL || !mGL->MakeCurrent()) {
    return false;
  }

  ScopedPackState scopedPackState(mGL);
  Bind();

  const uint8_t* srcData = nullptr;
  if (mGL->IsSupported(GLFeature::map_buffer_range)) {
    srcData = static_cast<uint8_t*>(mGL->fMapBufferRange(
        LOCAL_GL_PIXEL_PACK_BUFFER, 0, aReadSize.height * aReadSize.width * 4,
        LOCAL_GL_MAP_READ_BIT));
  } else {
    srcData = static_cast<uint8_t*>(
        mGL->fMapBuffer(LOCAL_GL_PIXEL_PACK_BUFFER, LOCAL_GL_READ_ONLY));
  }

  if (!srcData) {
    return false;
  }

  int32_t srcStride = mSize.width * 4;  // Bind() sets an alignment of 1
  DataSourceSurface::ScopedMap map(aSurface, DataSourceSurface::WRITE);
  uint8_t* destData = map.GetData();
  int32_t destStride = map.GetStride();
  SurfaceFormat destFormat = aSurface->GetFormat();
  for (int32_t destRow = 0; destRow < aReadSize.height; destRow++) {
    // Turn srcData upside down during the copy.
    int32_t srcRow = aReadSize.height - 1 - destRow;
    const uint8_t* src = &srcData[srcRow * srcStride];
    uint8_t* dest = &destData[destRow * destStride];
    SwizzleData(src, srcStride, SurfaceFormat::R8G8B8A8, dest, destStride,
                destFormat, IntSize(aReadSize.width, 1));
  }

  mGL->fUnmapBuffer(LOCAL_GL_PIXEL_PACK_BUFFER);

  return true;
}

PerUnitTexturePoolOGL::PerUnitTexturePoolOGL(gl::GLContext* aGL)
    : mTextureTarget(0),  // zero is never a valid texture target
      mGL(aGL) {}

PerUnitTexturePoolOGL::~PerUnitTexturePoolOGL() { DestroyTextures(); }

CompositorOGL::CompositorOGL(widget::CompositorWidget* aWidget,
                             int aSurfaceWidth, int aSurfaceHeight,
                             bool aUseExternalSurfaceSize)
    : Compositor(aWidget),
      mWidgetSize(-1, -1),
      mSurfaceSize(aSurfaceWidth, aSurfaceHeight),
      mFBOTextureTarget(0),
      mWindowRenderTarget(nullptr),
      mQuadVBO(0),
      mTriangleVBO(0),
      mPreviousFrameDoneSync(nullptr),
      mThisFrameDoneSync(nullptr),
      mHasBGRA(0),
      mUseExternalSurfaceSize(aUseExternalSurfaceSize),
      mFrameInProgress(false),
      mDestroyed(false),
      mViewportSize(0, 0) {
  if (aWidget->GetNativeLayerRoot()) {
    // We can only render into native layers, our GLContext won't have a usable
    // default framebuffer.
    mCanRenderToDefaultFramebuffer = false;
  }
  MOZ_COUNT_CTOR(CompositorOGL);
}

CompositorOGL::~CompositorOGL() { MOZ_COUNT_DTOR(CompositorOGL); }

already_AddRefed<mozilla::gl::GLContext> CompositorOGL::CreateContext() {
  RefPtr<GLContext> context;

  // Used by mock widget to create an offscreen context
  nsIWidget* widget = mWidget->RealWidget();
  void* widgetOpenGLContext =
      widget ? widget->GetNativeData(NS_NATIVE_OPENGL_CONTEXT) : nullptr;
  if (widgetOpenGLContext) {
    GLContext* alreadyRefed = reinterpret_cast<GLContext*>(widgetOpenGLContext);
    return already_AddRefed<GLContext>(alreadyRefed);
  }

#ifdef XP_WIN
  if (gfxEnv::MOZ_LAYERS_PREFER_EGL()) {
    printf_stderr("Trying GL layers...\n");
    context = gl::GLContextProviderEGL::CreateForCompositorWidget(
        mWidget, /* aHardwareWebRender */ false, /* aForceAccelerated */ false);
  }
#endif

  // Allow to create offscreen GL context for main Layer Manager
  if (!context && gfxEnv::MOZ_LAYERS_PREFER_OFFSCREEN()) {
    nsCString discardFailureId;
    context = GLContextProvider::CreateHeadless(
        {CreateContextFlags::REQUIRE_COMPAT_PROFILE}, &discardFailureId);
    if (!context->CreateOffscreenDefaultFb(mSurfaceSize)) {
      context = nullptr;
    }
  }

  if (!context) {
    context = gl::GLContextProvider::CreateForCompositorWidget(
        mWidget,
        /* aHardwareWebRender */ false,
        gfxVars::RequiresAcceleratedGLContextForCompositorOGL());
  }

  if (!context) {
    NS_WARNING("Failed to create CompositorOGL context");
  }

  return context.forget();
}

void CompositorOGL::Destroy() {
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

void CompositorOGL::CleanupResources() {
  if (!mGLContext) return;

  if (mSurfacePoolHandle) {
    mSurfacePoolHandle->Pool()->DestroyGLResourcesForContext(mGLContext);
    mSurfacePoolHandle = nullptr;
  }

  RefPtr<GLContext> ctx = mGLContext->GetSharedContext();
  if (!ctx) {
    ctx = mGLContext;
  }

  if (!ctx->MakeCurrent()) {
    // Leak resources!
    mQuadVBO = 0;
    mTriangleVBO = 0;
    mPreviousFrameDoneSync = nullptr;
    mThisFrameDoneSync = nullptr;
    mProgramsHolder = nullptr;
    mGLContext = nullptr;
    mNativeLayersReferenceRT = nullptr;
    mFullWindowRenderTarget = nullptr;
    return;
  }

  mProgramsHolder = nullptr;
  mNativeLayersReferenceRT = nullptr;
  mFullWindowRenderTarget = nullptr;

#ifdef MOZ_WIDGET_GTK
  // TextureSources might hold RefPtr<gl::GLContext>.
  // All of them needs to be released to destroy GLContext.
  // GLContextGLX has to be destroyed before related gtk window is destroyed.
  for (auto textureSource : mRegisteredTextureSources) {
    textureSource->DeallocateDeviceData();
  }
  mRegisteredTextureSources.clear();
#endif

  ctx->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

  if (mQuadVBO) {
    ctx->fDeleteBuffers(1, &mQuadVBO);
    mQuadVBO = 0;
  }

  if (mTriangleVBO) {
    ctx->fDeleteBuffers(1, &mTriangleVBO);
    mTriangleVBO = 0;
  }

  mGLContext->MakeCurrent();

  if (mPreviousFrameDoneSync) {
    mGLContext->fDeleteSync(mPreviousFrameDoneSync);
    mPreviousFrameDoneSync = nullptr;
  }

  if (mThisFrameDoneSync) {
    mGLContext->fDeleteSync(mThisFrameDoneSync);
    mThisFrameDoneSync = nullptr;
  }

  if (mOwnsGLContext) {
    // On the main thread the Widget will be destroyed soon and calling
    // MakeCurrent after that could cause a crash (at least with GLX, see bug
    // 1059793), unless context is marked as destroyed. There may be some
    // textures still alive that will try to call MakeCurrent on the context so
    // let's make sure it is marked destroyed now.
    mGLContext->MarkDestroyed();
  }

  mGLContext = nullptr;
}

bool CompositorOGL::Initialize(GLContext* aGLContext,
                               RefPtr<ShaderProgramOGLsHolder> aProgramsHolder,
                               nsCString* const out_failureReason) {
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(!mGLContext);

  mGLContext = aGLContext;
  mProgramsHolder = aProgramsHolder;
  mOwnsGLContext = false;

  return Initialize(out_failureReason);
}

bool CompositorOGL::Initialize(nsCString* const out_failureReason) {
  ScopedGfxFeatureReporter reporter("GL Layers");

  // Do not allow double initialization
  MOZ_ASSERT(mGLContext == nullptr || !mOwnsGLContext,
             "Don't reinitialize CompositorOGL");

  if (!mGLContext) {
    MOZ_ASSERT(mOwnsGLContext);
    mGLContext = CreateContext();
  }

#ifdef MOZ_WIDGET_ANDROID
  if (!mGLContext) {
    *out_failureReason = "FEATURE_FAILURE_OPENGL_NO_ANDROID_CONTEXT";
    MOZ_CRASH("We need a context on Android");
  }
#endif

  if (!mGLContext) {
    *out_failureReason = "FEATURE_FAILURE_OPENGL_CREATE_CONTEXT";
    return false;
  }

  if (!mProgramsHolder) {
    mProgramsHolder = new ShaderProgramOGLsHolder(mGLContext);
  }

  MakeCurrent();

  mHasBGRA = mGLContext->IsExtensionSupported(
                 gl::GLContext::EXT_texture_format_BGRA8888) ||
             mGLContext->IsExtensionSupported(gl::GLContext::EXT_bgra);

  mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                 LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA);
  mGLContext->fEnable(LOCAL_GL_BLEND);

  // initialise a common shader to check that we can actually compile a shader
  RefPtr<DummyTextureSourceOGL> source = new DummyTextureSourceOGL;
  RefPtr<EffectRGB> effect =
      new EffectRGB(source, /* aPremultiplied */ true, SamplingFilter::GOOD);
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

    GLenum textureTargets[] = {LOCAL_GL_TEXTURE_2D, LOCAL_GL_NONE};

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
      if (!target) continue;

      mGLContext->fGenTextures(1, &testTexture);
      mGLContext->fBindTexture(target, testTexture);
      mGLContext->fTexParameteri(target, LOCAL_GL_TEXTURE_MIN_FILTER,
                                 LOCAL_GL_NEAREST);
      mGLContext->fTexParameteri(target, LOCAL_GL_TEXTURE_MAG_FILTER,
                                 LOCAL_GL_NEAREST);
      mGLContext->fTexImage2D(
          target, 0, LOCAL_GL_RGBA, 5, 3, /* sufficiently NPOT */
          0, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, nullptr);

      // unbind this texture, in preparation for binding it to the FBO
      mGLContext->fBindTexture(target, 0);

      mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, testFBO);
      mGLContext->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                        LOCAL_GL_COLOR_ATTACHMENT0, target,
                                        testTexture, 0);

      if (mGLContext->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER) ==
          LOCAL_GL_FRAMEBUFFER_COMPLETE) {
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
      /* Unable to find a texture target that works with FBOs and NPOT textures
       */
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
    if (!mGLContext->IsExtensionSupported(
            gl::GLContext::ARB_texture_rectangle)) {
      *out_failureReason = "FEATURE_FAILURE_OPENGL_ARB_EXT";
      return false;
    }
  }

  // Create a VBO for triangle vertices.
  mGLContext->fGenBuffers(1, &mTriangleVBO);

  /* Create a simple quad VBO */
  mGLContext->fGenBuffers(1, &mQuadVBO);

  // 4 quads, with the number of the quad (vertexID) encoded in w.
  GLfloat vertices[] = {
      0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f,

      0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
      1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f,

      0.0f, 0.0f, 0.0f, 2.0f, 1.0f, 0.0f, 0.0f, 2.0f, 0.0f, 1.0f, 0.0f, 2.0f,
      1.0f, 0.0f, 0.0f, 2.0f, 0.0f, 1.0f, 0.0f, 2.0f, 1.0f, 1.0f, 0.0f, 2.0f,

      0.0f, 0.0f, 0.0f, 3.0f, 1.0f, 0.0f, 0.0f, 3.0f, 0.0f, 1.0f, 0.0f, 3.0f,
      1.0f, 0.0f, 0.0f, 3.0f, 0.0f, 1.0f, 0.0f, 3.0f, 1.0f, 1.0f, 0.0f, 3.0f,
  };
  HeapCopyOfStackArray<GLfloat> verticesOnHeap(vertices);

  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mQuadVBO);
  mGLContext->fBufferData(LOCAL_GL_ARRAY_BUFFER, verticesOnHeap.ByteLength(),
                          verticesOnHeap.Data(), LOCAL_GL_STATIC_DRAW);
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);

  nsCOMPtr<nsIConsoleService> console(
      do_GetService(NS_CONSOLESERVICE_CONTRACTID));

  if (console) {
    nsString msg;
    msg += nsLiteralString(
        u"OpenGL compositor Initialized Succesfully.\nVersion: ");
    msg += NS_ConvertUTF8toUTF16(nsDependentCString(
        (const char*)mGLContext->fGetString(LOCAL_GL_VERSION)));
    msg += u"\nVendor: "_ns;
    msg += NS_ConvertUTF8toUTF16(nsDependentCString(
        (const char*)mGLContext->fGetString(LOCAL_GL_VENDOR)));
    msg += u"\nRenderer: "_ns;
    msg += NS_ConvertUTF8toUTF16(nsDependentCString(
        (const char*)mGLContext->fGetString(LOCAL_GL_RENDERER)));
    msg += u"\nFBO Texture Target: "_ns;
    if (mFBOTextureTarget == LOCAL_GL_TEXTURE_2D)
      msg += u"TEXTURE_2D"_ns;
    else
      msg += u"TEXTURE_RECTANGLE"_ns;
    console->LogStringMessage(msg.get());
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
static IntSize CalculatePOTSize(const IntSize& aSize, GLContext* gl) {
  if (CanUploadNonPowerOfTwo(gl)) return aSize;

  return IntSize(RoundUpPow2(aSize.width), RoundUpPow2(aSize.height));
}

gfx::Rect CompositorOGL::GetTextureCoordinates(gfx::Rect textureRect,
                                               TextureSource* aTexture) {
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
      textureRect.Scale(xScale, yScale);
    }
  }

  return textureRect;
}

void CompositorOGL::PrepareViewport(CompositingRenderTargetOGL* aRenderTarget) {
  MOZ_ASSERT(aRenderTarget);
  // Logical surface size.
  const gfx::IntSize& size = aRenderTarget->GetSize();
  // Physical surface size.
  const gfx::IntSize& phySize = aRenderTarget->GetPhysicalSize();

  // Set the viewport correctly.
  mGLContext->fViewport(mSurfaceOrigin.x, mSurfaceOrigin.y, phySize.width,
                        phySize.height);

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
    viewMatrix.PreTranslate(-1.0, 1.0);
    viewMatrix.PreScale(2.0f / float(size.width), 2.0f / float(size.height));
    viewMatrix.PreScale(1.0f, -1.0f);

    MOZ_ASSERT(mCurrentRenderTarget, "No destination");

    Matrix4x4 matrix3d = Matrix4x4::From2D(viewMatrix);
    matrix3d._33 = 0.0f;
    mProjMatrix = matrix3d;
    mGLContext->fDepthRange(0.0f, 1.0f);
  } else {
    bool depthEnable;
    float zNear, zFar;
    aRenderTarget->GetProjection(mProjMatrix, depthEnable, zNear, zFar);
    mGLContext->fDepthRange(zNear, zFar);
  }
}

already_AddRefed<CompositingRenderTarget> CompositorOGL::CreateRenderTarget(
    const IntRect& aRect, SurfaceInitMode aInit) {
  MOZ_ASSERT(!aRect.IsZeroArea(),
             "Trying to create a render target of invalid size");

  if (aRect.IsZeroArea()) {
    return nullptr;
  }

  if (!gl()) {
    // CompositingRenderTargetOGL does not work without a gl context.
    return nullptr;
  }

  GLuint tex = 0;
  GLuint fbo = 0;
  IntRect rect = aRect;
  IntSize fboSize;
  CreateFBOWithTexture(rect, false, 0, &fbo, &tex, &fboSize);
  return CompositingRenderTargetOGL::CreateForNewFBOAndTakeOwnership(
      this, tex, fbo, aRect, aRect.TopLeft(), aRect.Size(), mFBOTextureTarget,
      aInit);
}

void CompositorOGL::SetRenderTarget(CompositingRenderTarget* aSurface) {
  MOZ_ASSERT(aSurface);
  CompositingRenderTargetOGL* surface =
      static_cast<CompositingRenderTargetOGL*>(aSurface);
  if (mCurrentRenderTarget != surface) {
    mCurrentRenderTarget = surface;
    surface->BindRenderTarget();
  }

  PrepareViewport(mCurrentRenderTarget);
}

already_AddRefed<CompositingRenderTarget>
CompositorOGL::GetCurrentRenderTarget() const {
  return do_AddRef(mCurrentRenderTarget);
}

already_AddRefed<CompositingRenderTarget> CompositorOGL::GetWindowRenderTarget()
    const {
  return do_AddRef(mWindowRenderTarget);
}

already_AddRefed<AsyncReadbackBuffer> CompositorOGL::CreateAsyncReadbackBuffer(
    const IntSize& aSize) {
  return MakeAndAddRef<AsyncReadbackBufferOGL>(mGLContext, aSize);
}

bool CompositorOGL::ReadbackRenderTarget(CompositingRenderTarget* aSource,
                                         AsyncReadbackBuffer* aDest) {
  IntSize size = aSource->GetSize();
  MOZ_RELEASE_ASSERT(aDest->GetSize() == size);

  RefPtr<CompositingRenderTarget> previousTarget = GetCurrentRenderTarget();
  if (previousTarget != aSource) {
    SetRenderTarget(aSource);
  }

  ScopedPackState scopedPackState(mGLContext);
  static_cast<AsyncReadbackBufferOGL*>(aDest)->Bind();

  mGLContext->fReadPixels(0, 0, size.width, size.height, LOCAL_GL_RGBA,
                          LOCAL_GL_UNSIGNED_BYTE, 0);

  if (previousTarget != aSource) {
    SetRenderTarget(previousTarget);
  }
  return true;
}

bool CompositorOGL::BlitRenderTarget(CompositingRenderTarget* aSource,
                                     const gfx::IntSize& aSourceSize,
                                     const gfx::IntSize& aDestSize) {
  if (!mGLContext->IsSupported(GLFeature::framebuffer_blit)) {
    return false;
  }
  CompositingRenderTargetOGL* source =
      static_cast<CompositingRenderTargetOGL*>(aSource);
  GLuint srcFBO = source->GetFBO();
  GLuint destFBO = mCurrentRenderTarget->GetFBO();
  mGLContext->BlitHelper()->BlitFramebufferToFramebuffer(
      srcFBO, destFBO, IntRect(IntPoint(), aSourceSize),
      IntRect(IntPoint(), aDestSize), LOCAL_GL_LINEAR);
  return true;
}

static GLenum GetFrameBufferInternalFormat(
    GLContext* gl, GLuint aFrameBuffer,
    mozilla::widget::CompositorWidget* aWidget) {
  if (aFrameBuffer == 0) {  // default framebuffer
    return aWidget->GetGLFrameBufferFormat();
  }
  return LOCAL_GL_RGBA;
}

Maybe<IntRect> CompositorOGL::BeginFrameForWindow(
    const nsIntRegion& aInvalidRegion, const Maybe<IntRect>& aClipRect,
    const IntRect& aRenderBounds, const nsIntRegion& aOpaqueRegion) {
  MOZ_RELEASE_ASSERT(!mTarget, "mTarget not cleared properly");
  return BeginFrame(aInvalidRegion, aClipRect, aRenderBounds, aOpaqueRegion);
}

Maybe<IntRect> CompositorOGL::BeginFrame(const nsIntRegion& aInvalidRegion,
                                         const Maybe<IntRect>& aClipRect,
                                         const IntRect& aRenderBounds,
                                         const nsIntRegion& aOpaqueRegion) {
  AUTO_PROFILER_LABEL("CompositorOGL::BeginFrame", GRAPHICS);

  MOZ_ASSERT(!mFrameInProgress,
             "frame still in progress (should have called EndFrame");

  IntRect rect;
  if (mUseExternalSurfaceSize) {
    rect = IntRect(IntPoint(), mSurfaceSize);
  } else {
    rect = aRenderBounds;
  }

  // We can't draw anything to something with no area
  // so just return
  if (rect.IsZeroArea()) {
    return Nothing();
  }

  // If the widget size changed, we have to force a MakeCurrent
  // to make sure that GL sees the updated widget size.
  if (mWidgetSize.ToUnknownSize() != rect.Size()) {
    MakeCurrent(ForceMakeCurrent);

    mWidgetSize = LayoutDeviceIntSize::FromUnknownSize(rect.Size());
#ifdef MOZ_WIDGET_GTK
    if (mWidget && mWidget->AsGTK()) {
      if (!mWidget->AsGTK()->SetEGLNativeWindowSize(mWidgetSize)) {
        // We don't have correct window size to paint into.
        return Nothing();
      }
    }
#endif
  } else {
    MakeCurrent();
  }

#ifdef MOZ_WIDGET_ANDROID
  java::GeckoSurfaceTexture::DestroyUnused((int64_t)mGLContext.get());
  mGLContext->MakeCurrent();  // DestroyUnused can change the current context!
#endif

  // Default blend function implements "OVER"
  mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                 LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA);
  mGLContext->fEnable(LOCAL_GL_BLEND);

  RefPtr<CompositingRenderTarget> rt;
  if (mCanRenderToDefaultFramebuffer) {
    rt = CompositingRenderTargetOGL::CreateForWindow(this, rect.Size());
  } else if (mTarget) {
    rt = CreateRenderTarget(rect, INIT_MODE_CLEAR);
  } else {
    MOZ_CRASH("Unexpected call");
  }

  if (!rt) {
    return Nothing();
  }

  // We're about to actually draw a frame.
  mFrameInProgress = true;

  SetRenderTarget(rt);
  mWindowRenderTarget = mCurrentRenderTarget;

  for (auto iter = aInvalidRegion.RectIter(); !iter.Done(); iter.Next()) {
    const IntRect& r = iter.Get();
    mCurrentFrameInvalidRegion.OrWith(
        IntRect(r.X(), FlipY(r.YMost()), r.Width(), r.Height()));
  }
  // Check to see if there is any transparent dirty region that would require
  // clearing. If not, just invalidate the framebuffer if supported.
  // TODO: Currently we initialize the clear region to the widget bounds as
  // SwapBuffers will update the entire framebuffer. On platforms that support
  // damage regions, we could initialize this to mCurrentFrameInvalidRegion.
  IntRegion regionToClear(rect);
  regionToClear.SubOut(aOpaqueRegion);
  GLbitfield clearBits = LOCAL_GL_DEPTH_BUFFER_BIT;
  if (regionToClear.IsEmpty() &&
      mGLContext->IsSupported(GLFeature::invalidate_framebuffer)) {
    GLenum attachments[] = {LOCAL_GL_COLOR};
    mGLContext->fInvalidateFramebuffer(
        LOCAL_GL_FRAMEBUFFER, MOZ_ARRAY_LENGTH(attachments), attachments);
  } else {
    clearBits |= LOCAL_GL_COLOR_BUFFER_BIT;
  }

#if defined(MOZ_WIDGET_ANDROID)
  if ((mSurfaceOrigin.x > 0) || (mSurfaceOrigin.y > 0)) {
    mGLContext->fClearColor(
        StaticPrefs::gfx_compositor_override_clear_color_r(),
        StaticPrefs::gfx_compositor_override_clear_color_g(),
        StaticPrefs::gfx_compositor_override_clear_color_b(),
        StaticPrefs::gfx_compositor_override_clear_color_a());
  } else {
    mGLContext->fClearColor(mClearColor.r, mClearColor.g, mClearColor.b,
                            mClearColor.a);
  }
#else
  mGLContext->fClearColor(mClearColor.r, mClearColor.g, mClearColor.b,
                          mClearColor.a);
#endif  // defined(MOZ_WIDGET_ANDROID)
  mGLContext->fClear(clearBits);

  return Some(rect);
}

void CompositorOGL::CreateFBOWithTexture(const gfx::IntRect& aRect,
                                         bool aCopyFromSource,
                                         GLuint aSourceFrameBuffer,
                                         GLuint* aFBO, GLuint* aTexture,
                                         gfx::IntSize* aAllocSize) {
  *aTexture =
      CreateTexture(aRect, aCopyFromSource, aSourceFrameBuffer, aAllocSize);
  mGLContext->fGenFramebuffers(1, aFBO);
}

// Should be called after calls to fReadPixels or fCopyTexImage2D, and other
// GL read calls.
static void WorkAroundAppleIntelHD3000GraphicsGLDriverBug(GLContext* aGL) {
#ifdef XP_MACOSX
  if (aGL->WorkAroundDriverBugs() &&
      aGL->Renderer() == GLRenderer::IntelHD3000) {
    // Work around a bug in the Apple Intel HD Graphics 3000 driver (bug
    // 1586627, filed with Apple as FB7379358). This bug has been found present
    // on 10.9.3 and on 10.13.6, so it likely affects all shipped versions of
    // this driver. (macOS 10.14 does not support this GPU.)
    // The bug manifests as follows: Reading from a framebuffer puts that
    // framebuffer into a state such that deleting that framebuffer can break
    // other framebuffers in certain cases. More specifically, if you have two
    // framebuffers A and B, the following sequence of events breaks subsequent
    // drawing to B:
    //  1. A becomes "most recently read-from framebuffer".
    //  2. B is drawn to.
    //  3. A is deleted, and other GL state (such as GL_SCISSOR enabled state)
    //     is touched.
    //  4. B is drawn to again.
    // Now all draws to framebuffer B, including the draw from step 4, will
    // render at the wrong position and upside down.
    //
    // When AfterGLReadCall() is called, the currently bound framebuffer is the
    // framebuffer that has been read from most recently. So in the presence of
    // this bug, deleting this framebuffer has now become dangerous. We work
    // around the bug by creating a new short-lived framebuffer, making that new
    // framebuffer the most recently read-from framebuffer (using
    // glCopyTexImage2D), and then deleting it under controlled circumstances.
    // This deletion is not affected by the bug because our deletion call is not
    // interleaved with draw calls to another framebuffer and a touching of the
    // GL scissor enabled state.

    ScopedTexture texForReading(aGL);
    {
      // Initialize a 1x1 texture.
      ScopedBindTexture autoBindTexForReading(aGL, texForReading);
      aGL->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_RGBA, 1, 1, 0,
                       LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, nullptr);
      aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER,
                          LOCAL_GL_LINEAR);
      aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER,
                          LOCAL_GL_LINEAR);
    }
    // Make a framebuffer around the texture.
    ScopedFramebufferForTexture autoFBForReading(aGL, texForReading);
    if (autoFBForReading.IsComplete()) {
      // "Read" from the framebuffer, by initializing a new texture using
      // glCopyTexImage2D. This flips the bad bit on autoFBForReading.FB().
      ScopedBindFramebuffer autoFB(aGL, autoFBForReading.FB());
      ScopedTexture texReadingDest(aGL);
      ScopedBindTexture autoBindTexReadingDest(aGL, texReadingDest);
      aGL->fCopyTexImage2D(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_RGBA, 0, 0, 1, 1,
                           0);
    }
    // When autoFBForReading goes out of scope, the "poisoned" framebuffer is
    // deleted, and the bad state seems to go away along with it.
  }
#endif
}

GLuint CompositorOGL::CreateTexture(const IntRect& aRect, bool aCopyFromSource,
                                    GLuint aSourceFrameBuffer,
                                    IntSize* aAllocSize) {
  // we're about to create a framebuffer backed by textures to use as an
  // intermediate surface. What to do if its size (as given by aRect) would
  // exceed the maximum texture size supported by the GL? The present code
  // chooses the compromise of just clamping the framebuffer's size to the max
  // supported size. This gives us a lower resolution rendering of the
  // intermediate surface (children layers). See bug 827170 for a discussion.
  IntRect clampedRect = aRect;
  int32_t maxTexSize = GetMaxTextureSize();
  clampedRect.SetWidth(std::min(clampedRect.Width(), maxTexSize));
  clampedRect.SetHeight(std::min(clampedRect.Height(), maxTexSize));

  auto clampedRectWidth = clampedRect.Width();
  auto clampedRectHeight = clampedRect.Height();

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

    bool isFormatCompatibleWithRGBA =
        gl()->IsGLES() ? (format == LOCAL_GL_RGBA) : true;

    if (isFormatCompatibleWithRGBA) {
      mGLContext->fCopyTexImage2D(mFBOTextureTarget, 0, LOCAL_GL_RGBA,
                                  clampedRect.X(), FlipY(clampedRect.YMost()),
                                  clampedRectWidth, clampedRectHeight, 0);
      WorkAroundAppleIntelHD3000GraphicsGLDriverBug(mGLContext);
    } else {
      // Curses, incompatible formats.  Take a slow path.

      // RGBA
      size_t bufferSize = clampedRectWidth * clampedRectHeight * 4;
      auto buf = MakeUnique<uint8_t[]>(bufferSize);

      mGLContext->fReadPixels(clampedRect.X(), clampedRect.Y(),
                              clampedRectWidth, clampedRectHeight,
                              LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, buf.get());
      WorkAroundAppleIntelHD3000GraphicsGLDriverBug(mGLContext);
      mGLContext->fTexImage2D(mFBOTextureTarget, 0, LOCAL_GL_RGBA,
                              clampedRectWidth, clampedRectHeight, 0,
                              LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, buf.get());
    }

    GLenum error = mGLContext->fGetError();
    if (error != LOCAL_GL_NO_ERROR) {
      nsAutoCString msg;
      msg.AppendPrintf(
          "Texture initialization failed! -- error 0x%x, Source %d, Source "
          "format %d,  RGBA Compat %d",
          error, aSourceFrameBuffer, format, isFormatCompatibleWithRGBA);
      NS_ERROR(msg.get());
    }
  } else {
    mGLContext->fTexImage2D(mFBOTextureTarget, 0, LOCAL_GL_RGBA,
                            clampedRectWidth, clampedRectHeight, 0,
                            LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, nullptr);
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

  if (aAllocSize) {
    aAllocSize->width = clampedRectWidth;
    aAllocSize->height = clampedRectHeight;
  }

  return tex;
}

ShaderConfigOGL CompositorOGL::GetShaderConfigFor(Effect* aEffect,
                                                  bool aDEAAEnabled) const {
  ShaderConfigOGL config;

  switch (aEffect->mType) {
    case EffectTypes::YCBCR: {
      config.SetYCbCr(true);
      EffectYCbCr* effectYCbCr = static_cast<EffectYCbCr*>(aEffect);
      config.SetColorMultiplier(
          RescalingFactorForColorDepth(effectYCbCr->mColorDepth));
      config.SetTextureTarget(
          effectYCbCr->mTexture->AsSourceOGL()->GetTextureTarget());
      break;
    }
    case EffectTypes::NV12:
      config.SetNV12(true);
      if (gl()->IsExtensionSupported(gl::GLContext::ARB_texture_rectangle)) {
        config.SetTextureTarget(LOCAL_GL_TEXTURE_RECTANGLE_ARB);
      } else {
        config.SetTextureTarget(LOCAL_GL_TEXTURE_2D);
      }
      break;
    default: {
      MOZ_ASSERT(aEffect->mType == EffectTypes::RGB);
      TexturedEffect* texturedEffect = static_cast<TexturedEffect*>(aEffect);
      TextureSourceOGL* source = texturedEffect->mTexture->AsSourceOGL();
      MOZ_ASSERT_IF(
          source->GetTextureTarget() == LOCAL_GL_TEXTURE_EXTERNAL,
          source->GetFormat() == gfx::SurfaceFormat::R8G8B8A8 ||
              source->GetFormat() == gfx::SurfaceFormat::R8G8B8X8 ||
              source->GetFormat() == gfx::SurfaceFormat::B8G8R8A8 ||
              source->GetFormat() == gfx::SurfaceFormat::B8G8R8X8 ||
              source->GetFormat() == gfx::SurfaceFormat::R5G6B5_UINT16);
      MOZ_ASSERT_IF(
          source->GetTextureTarget() == LOCAL_GL_TEXTURE_RECTANGLE_ARB,
          source->GetFormat() == gfx::SurfaceFormat::R8G8B8A8 ||
              source->GetFormat() == gfx::SurfaceFormat::R8G8B8X8 ||
              source->GetFormat() == gfx::SurfaceFormat::R5G6B5_UINT16 ||
              source->GetFormat() == gfx::SurfaceFormat::YUV422);
      config = ShaderConfigFromTargetAndFormat(source->GetTextureTarget(),
                                               source->GetFormat());
      if (!texturedEffect->mPremultiplied) {
        config.SetNoPremultipliedAlpha();
      }
      break;
    }
  }
  config.SetDEAA(aDEAAEnabled);
  return config;
}

ShaderProgramOGL* CompositorOGL::GetShaderProgramFor(
    const ShaderConfigOGL& aConfig) {
  ShaderProgramOGL* shader = mProgramsHolder->GetShaderProgramFor(aConfig);
  return shader;
}

void CompositorOGL::ResetProgram() { mProgramsHolder->ResetCurrentProgram(); }

static bool SetBlendMode(GLContext* aGL, gfx::CompositionOp aBlendMode,
                         bool aIsPremultiplied = true) {
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

  aGL->fBlendFuncSeparate(srcBlend, dstBlend, srcAlphaBlend, dstAlphaBlend);
  return true;
}

gfx::Point3D CompositorOGL::GetLineCoefficients(const gfx::Point& aPoint1,
                                                const gfx::Point& aPoint2) {
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
  coeffecients.z =
      aPoint1.x.value * aPoint2.y.value - aPoint2.x.value * aPoint1.y.value;

  coeffecients *= 1.0f / sqrtf(coeffecients.x * coeffecients.x +
                               coeffecients.y * coeffecients.y);

  // Offset outwards by 0.5 pixel as the edge is considered to be 1 pixel
  // wide and included within the interior of the polygon
  coeffecients.z += 0.5f;

  return coeffecients;
}

void CompositorOGL::DrawQuad(const Rect& aRect, const IntRect& aClipRect,
                             const EffectChain& aEffectChain, Float aOpacity,
                             const gfx::Matrix4x4& aTransform,
                             const gfx::Rect& aVisibleRect) {
  AUTO_PROFILER_LABEL("CompositorOGL::DrawQuad", GRAPHICS);

  DrawGeometry(aRect, aRect, aClipRect, aEffectChain, aOpacity, aTransform,
               aVisibleRect);
}

template <typename Geometry>
void CompositorOGL::DrawGeometry(const Geometry& aGeometry,
                                 const gfx::Rect& aRect,
                                 const gfx::IntRect& aClipRect,
                                 const EffectChain& aEffectChain,
                                 gfx::Float aOpacity,
                                 const gfx::Matrix4x4& aTransform,
                                 const gfx::Rect& aVisibleRect) {
  MOZ_ASSERT(mFrameInProgress, "frame not started");
  MOZ_ASSERT(mCurrentRenderTarget, "No destination");

  MakeCurrent();

  // Convert aClipRect into render target space, and intersect it with the
  // render target's clip.
  IntRect clipRect = aClipRect + mCurrentRenderTarget->GetClipSpaceOrigin();
  if (Maybe<IntRect> rtClip = mCurrentRenderTarget->GetClipRect()) {
    clipRect = clipRect.Intersect(*rtClip);
  }

  Rect destRect = aTransform.TransformAndClipBounds(
      aRect, Rect(mCurrentRenderTarget->GetRect().Intersect(clipRect)));
  if (destRect.IsEmpty()) {
    return;
  }

  // Move clipRect into device space.
  IntPoint offset = mCurrentRenderTarget->GetOrigin();
  clipRect -= offset;

  if (!mTarget && mCurrentRenderTarget->IsWindow()) {
    clipRect.MoveBy(mSurfaceOrigin.x, -mSurfaceOrigin.y);
  }

  ScopedGLState scopedScissorTestState(mGLContext, LOCAL_GL_SCISSOR_TEST, true);
  ScopedScissorRect autoScissorRect(mGLContext, clipRect.X(),
                                    FlipY(clipRect.Y() + clipRect.Height()),
                                    clipRect.Width(), clipRect.Height());

  // Only apply DEAA to quads that have been transformed such that aliasing
  // could be visible
  bool bEnableAA = StaticPrefs::layers_deaa_enabled() &&
                   !aTransform.Is2DIntegerTranslation();

  ShaderConfigOGL config =
      GetShaderConfigFor(aEffectChain.mPrimaryEffect, bEnableAA);

  config.SetOpacity(aOpacity != 1.f);
  ApplyPrimitiveConfig(config, aGeometry);

  ShaderProgramOGL* program = mProgramsHolder->ActivateProgram(config);
  if (!program) {
    return;
  }
  program->SetProjectionMatrix(mProjMatrix);
  program->SetLayerTransform(aTransform);
  program->SetRenderOffset(offset.x, offset.y);

  if (aOpacity != 1.f) program->SetLayerOpacity(aOpacity);

  if (config.mFeatures & ENABLE_TEXTURE_RECT) {
    TextureSourceOGL* source = nullptr;
    TexturedEffect* texturedEffect =
        static_cast<TexturedEffect*>(aEffectChain.mPrimaryEffect.get());
    source = texturedEffect->mTexture->AsSourceOGL();
    // This is used by IOSurface that use 0,0...w,h coordinate rather then
    // 0,0..1,1.
    program->SetTexCoordMultiplier(source->GetSize().width,
                                   source->GetSize().height);
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
    size_t pointCount =
        flatTransform.TransformAndClipRect(aVisibleRect, viewportClip, points);
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
        winding = (points[wp1].x - points[wp].x).value *
                      (points[wp1].y + points[wp].y).value +
                  (points[wp2].x - points[wp1].x).value *
                      (points[wp2].y + points[wp1].y).value +
                  (points[wp].x - points[wp2].x).value *
                      (points[wp].y + points[wp2].y).value;
        wp++;
      }
      bool frontFacing = winding >= 0.0f;

      // Calculate the line coefficients used by the DEAA shader to determine
      // the sub-pixel coverage of the edge pixels
      for (size_t i = 0; i < pointCount; i++) {
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
    case EffectTypes::RGB: {
      TexturedEffect* texturedEffect =
          static_cast<TexturedEffect*>(aEffectChain.mPrimaryEffect.get());
      TextureSource* source = texturedEffect->mTexture;

      didSetBlendMode = SetBlendMode(gl(), gfx::CompositionOp::OP_OVER,
                                     texturedEffect->mPremultiplied);

      gfx::SamplingFilter samplingFilter = texturedEffect->mSamplingFilter;

      source->AsSourceOGL()->BindTexture(LOCAL_GL_TEXTURE0, samplingFilter);

      program->SetTextureUnit(0);

      Matrix4x4 textureTransform = source->AsSourceOGL()->GetTextureTransform();
      program->SetTextureTransform(textureTransform);

      BindAndDrawGeometryWithTextureRect(
          program, aGeometry, texturedEffect->mTextureCoords, source);
      source->AsSourceOGL()->MaybeFenceTexture();
    } break;
    case EffectTypes::YCBCR: {
      EffectYCbCr* effectYCbCr =
          static_cast<EffectYCbCr*>(aEffectChain.mPrimaryEffect.get());
      TextureSource* sourceYCbCr = effectYCbCr->mTexture;
      const int Y = 0, Cb = 1, Cr = 2;
      TextureSourceOGL* sourceY = sourceYCbCr->GetSubSource(Y)->AsSourceOGL();
      TextureSourceOGL* sourceCb = sourceYCbCr->GetSubSource(Cb)->AsSourceOGL();
      TextureSourceOGL* sourceCr = sourceYCbCr->GetSubSource(Cr)->AsSourceOGL();

      if (!sourceY || !sourceCb || !sourceCr) {
        NS_WARNING("Invalid layer texture.");
        return;
      }

      sourceY->BindTexture(LOCAL_GL_TEXTURE0, effectYCbCr->mSamplingFilter);
      sourceCb->BindTexture(LOCAL_GL_TEXTURE1, effectYCbCr->mSamplingFilter);
      sourceCr->BindTexture(LOCAL_GL_TEXTURE2, effectYCbCr->mSamplingFilter);

      if (config.mFeatures & ENABLE_TEXTURE_RECT) {
        // This is used by IOSurface that use 0,0...w,h coordinate rather then
        // 0,0..1,1.
        program->SetCbCrTexCoordMultiplier(sourceCb->GetSize().width,
                                           sourceCb->GetSize().height);
      }

      program->SetYCbCrTextureUnits(Y, Cb, Cr);
      program->SetTextureTransform(Matrix4x4());
      program->SetYUVColorSpace(effectYCbCr->mYUVColorSpace);

      BindAndDrawGeometryWithTextureRect(program, aGeometry,
                                         effectYCbCr->mTextureCoords,
                                         sourceYCbCr->GetSubSource(Y));
      sourceY->MaybeFenceTexture();
      sourceCb->MaybeFenceTexture();
      sourceCr->MaybeFenceTexture();
    } break;
    case EffectTypes::NV12: {
      EffectNV12* effectNV12 =
          static_cast<EffectNV12*>(aEffectChain.mPrimaryEffect.get());
      TextureSource* sourceNV12 = effectNV12->mTexture;
      const int Y = 0, CbCr = 1;
      TextureSourceOGL* sourceY = sourceNV12->GetSubSource(Y)->AsSourceOGL();
      TextureSourceOGL* sourceCbCr =
          sourceNV12->GetSubSource(CbCr)->AsSourceOGL();

      if (!sourceY || !sourceCbCr) {
        NS_WARNING("Invalid layer texture.");
        return;
      }

      sourceY->BindTexture(LOCAL_GL_TEXTURE0, effectNV12->mSamplingFilter);
      sourceCbCr->BindTexture(LOCAL_GL_TEXTURE1, effectNV12->mSamplingFilter);

      if (config.mFeatures & ENABLE_TEXTURE_RECT) {
        // This is used by IOSurface that use 0,0...w,h coordinate rather then
        // 0,0..1,1.
        program->SetCbCrTexCoordMultiplier(sourceCbCr->GetSize().width,
                                           sourceCbCr->GetSize().height);
      }

      program->SetNV12TextureUnits(Y, CbCr);
      program->SetTextureTransform(Matrix4x4());
      program->SetYUVColorSpace(effectNV12->mYUVColorSpace);

      BindAndDrawGeometryWithTextureRect(program, aGeometry,
                                         effectNV12->mTextureCoords,
                                         sourceNV12->GetSubSource(Y));
      sourceY->MaybeFenceTexture();
      sourceCbCr->MaybeFenceTexture();
    } break;
    default:
      MOZ_ASSERT(false, "Unhandled effect type");
      break;
  }

  if (didSetBlendMode) {
    gl()->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                             LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA);
  }

  // in case rendering has used some other GL context
  MakeCurrent();
}

void CompositorOGL::BindAndDrawGeometry(ShaderProgramOGL* aProgram,
                                        const gfx::Rect& aRect) {
  BindAndDrawQuad(aProgram, aRect);
}

void CompositorOGL::BindAndDrawGeometry(
    ShaderProgramOGL* aProgram,
    const nsTArray<gfx::TexturedTriangle>& aTriangles) {
  NS_ASSERTION(aProgram->HasInitialized(),
               "Shader program not correctly initialized");

  const nsTArray<TexturedVertex> vertices =
      TexturedTrianglesToVertexArray(aTriangles);

  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mTriangleVBO);
  mGLContext->fBufferData(LOCAL_GL_ARRAY_BUFFER,
                          vertices.Length() * sizeof(TexturedVertex),
                          vertices.Elements(), LOCAL_GL_STREAM_DRAW);

  const GLsizei stride = 4 * sizeof(GLfloat);
  InitializeVAO(kCoordinateAttributeIndex, 2, stride, 0);
  InitializeVAO(kTexCoordinateAttributeIndex, 2, stride, 2 * sizeof(GLfloat));

  mGLContext->fDrawArrays(LOCAL_GL_TRIANGLES, 0, vertices.Length());

  mGLContext->fDisableVertexAttribArray(kCoordinateAttributeIndex);
  mGLContext->fDisableVertexAttribArray(kTexCoordinateAttributeIndex);
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
}

// |aRect| is the rectangle we want to draw to. We will draw it with
// up to 4 draw commands if necessary to avoid wrapping.
// |aTexCoordRect| is the rectangle from the texture that we want to
// draw using the given program.
// |aTexture| is the texture we are drawing. Its actual size can be
// larger than the rectangle given by |texCoordRect|.
void CompositorOGL::BindAndDrawGeometryWithTextureRect(
    ShaderProgramOGL* aProg, const Rect& aRect, const Rect& aTexCoordRect,
    TextureSource* aTexture) {
  Rect scaledTexCoordRect = GetTextureCoordinates(aTexCoordRect, aTexture);
  Rect layerRects[4];
  Rect textureRects[4];
  size_t rects = DecomposeIntoNoRepeatRects(aRect, scaledTexCoordRect,
                                            &layerRects, &textureRects);

  BindAndDrawQuads(aProg, rects, layerRects, textureRects);
}

void CompositorOGL::BindAndDrawGeometryWithTextureRect(
    ShaderProgramOGL* aProg, const nsTArray<gfx::TexturedTriangle>& aTriangles,
    const gfx::Rect& aTexCoordRect, TextureSource* aTexture) {
  BindAndDrawGeometry(aProg, aTriangles);
}

void CompositorOGL::BindAndDrawQuads(ShaderProgramOGL* aProg, int aQuads,
                                     const Rect* aLayerRects,
                                     const Rect* aTextureRects) {
  NS_ASSERTION(aProg->HasInitialized(),
               "Shader program not correctly initialized");

  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mQuadVBO);
  InitializeVAO(kCoordinateAttributeIndex, 4, 0, 0);

  aProg->SetLayerRects(aLayerRects);
  if (aProg->GetTextureCount() > 0) {
    aProg->SetTextureRects(aTextureRects);
  }

  // We are using GL_TRIANGLES here because the Mac Intel drivers fail to
  // properly process uniform arrays with GL_TRIANGLE_STRIP. Go figure.
  mGLContext->fDrawArrays(LOCAL_GL_TRIANGLES, 0, 6 * aQuads);
  mGLContext->fDisableVertexAttribArray(kCoordinateAttributeIndex);
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
}

void CompositorOGL::InitializeVAO(const GLuint aAttrib, const GLint aComponents,
                                  const GLsizei aStride, const size_t aOffset) {
  mGLContext->fVertexAttribPointer(aAttrib, aComponents, LOCAL_GL_FLOAT,
                                   LOCAL_GL_FALSE, aStride,
                                   reinterpret_cast<GLvoid*>(aOffset));
  mGLContext->fEnableVertexAttribArray(aAttrib);
}

#ifdef MOZ_DUMP_PAINTING
template <typename T>
void WriteSnapshotToDumpFile_internal(T* aObj, DataSourceSurface* aSurf) {
  if (gfxUtils::sDumpPaintFile != stderr) {
    gfxUtils::DumpAsDataURI(aSurf, gfxUtils::sDumpPaintFile);
  } else {
    nsCString uri = gfxUtils::GetAsDataURI(aSurf);
    nsPrintfCString string(R"(array["%s-%)" PRIu64 R"("]="%s";\n)",
                           aObj->Name(), uint64_t(aObj), uri.BeginReading());
    fprintf_stderr(gfxUtils::sDumpPaintFile, "%s", string.get());
  }
}

void WriteSnapshotToDumpFile(Compositor* aCompositor, DrawTarget* aTarget) {
  RefPtr<SourceSurface> surf = aTarget->Snapshot();
  RefPtr<DataSourceSurface> dSurf = surf->GetDataSurface();
  WriteSnapshotToDumpFile_internal(aCompositor, dSurf);
}
#endif

void CompositorOGL::EndFrame() {
  AUTO_PROFILER_LABEL("CompositorOGL::EndFrame", GRAPHICS);

#ifdef MOZ_DUMP_PAINTING
  if (gfxEnv::MOZ_DISABLE_FORCE_PRESENT()) {
    LayoutDeviceIntSize size;
    if (mUseExternalSurfaceSize) {
      size = LayoutDeviceIntSize(mSurfaceSize.width, mSurfaceSize.height);
    } else {
      size = mWidget->GetClientSize();
    }
    RefPtr<DrawTarget> target =
        gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
            IntSize(size.width, size.height), SurfaceFormat::B8G8R8A8);
    if (target) {
      CopyToTarget(target, nsIntPoint(), Matrix());
      WriteSnapshotToDumpFile(this, target);
    }
  }
#endif

  mFrameInProgress = false;
  mShouldInvalidateWindow = false;

  if (mTarget) {
    CopyToTarget(mTarget, mTargetBounds.TopLeft(), Matrix());
    mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
    mTarget = nullptr;
    mWindowRenderTarget = nullptr;
    mCurrentRenderTarget = nullptr;
    Compositor::EndFrame();
    return;
  }

  mWindowRenderTarget = nullptr;
  mCurrentRenderTarget = nullptr;

  if (mTexturePool) {
    mTexturePool->EndFrame();
  }

  InsertFrameDoneSync();

  mGLContext->SetDamage(mCurrentFrameInvalidRegion);
  mGLContext->SwapBuffers();
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);

  // Unbind all textures
  for (GLuint i = 0; i <= 4; i++) {
    mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0 + i);
    mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, 0);
    if (!mGLContext->IsGLES()) {
      mGLContext->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, 0);
    }
  }

  mCurrentFrameInvalidRegion.SetEmpty();

  Compositor::EndFrame();
}

void CompositorOGL::InsertFrameDoneSync() {
#ifdef XP_MACOSX
  // Only do this on macOS.
  // On other platforms, SwapBuffers automatically applies back-pressure.
  if (mThisFrameDoneSync) {
    mGLContext->fDeleteSync(mThisFrameDoneSync);
  }
  mThisFrameDoneSync =
      mGLContext->fFenceSync(LOCAL_GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
#elif defined(MOZ_WIDGET_ANDROID)
  const auto& gle = gl::GLContextEGL::Cast(mGLContext);
  const auto& egl = gle->mEgl;

  EGLSync sync = nullptr;
  if (AndroidHardwareBufferApi::Get()) {
    sync = egl->fCreateSync(LOCAL_EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr);
  }
  if (sync) {
    int fenceFd = egl->fDupNativeFenceFDANDROID(sync);
    if (fenceFd >= 0) {
      mReleaseFenceFd = ipc::FileDescriptor(UniqueFileHandle(fenceFd));
    }
    egl->fDestroySync(sync);
    sync = nullptr;
  }
#endif
}

ipc::FileDescriptor CompositorOGL::GetReleaseFence() { return mReleaseFenceFd; }

bool CompositorOGL::NeedToRecreateFullWindowRenderTarget() const {
  if (!ShouldRecordFrames()) {
    return false;
  }
  if (!mFullWindowRenderTarget) {
    return true;
  }
  IntSize windowSize = mWidget->GetClientSize().ToUnknownSize();
  return mFullWindowRenderTarget->GetSize() != windowSize;
}

void CompositorOGL::SetDestinationSurfaceSize(const IntSize& aSize) {
  mSurfaceSize.width = aSize.width;
  mSurfaceSize.height = aSize.height;
}

void CompositorOGL::CopyToTarget(DrawTarget* aTarget,
                                 const nsIntPoint& aTopLeft,
                                 const gfx::Matrix& aTransform) {
  MOZ_ASSERT(aTarget);
  IntRect rect;
  if (mUseExternalSurfaceSize) {
    rect = IntRect(0, 0, mSurfaceSize.width, mSurfaceSize.height);
  } else {
    rect = IntRect(0, 0, mWidgetSize.width, mWidgetSize.height);
  }
  GLint width = rect.Width();
  GLint height = rect.Height();

  if ((int64_t(width) * int64_t(height) * int64_t(4)) > INT32_MAX) {
    NS_ERROR("Widget size too big - integer overflow!");
    return;
  }

  RefPtr<DataSourceSurface> source = Factory::CreateDataSourceSurface(
      rect.Size(), gfx::SurfaceFormat::B8G8R8A8);
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
  Rect floatRect = Rect(rect.X(), rect.Y(), width, height);
  aTarget->DrawSurface(source, floatRect, floatRect, DrawSurfaceOptions(),
                       DrawOptions(1.0f, CompositionOp::OP_SOURCE));
  aTarget->SetTransform(oldMatrix);
  aTarget->Flush();
}

void CompositorOGL::Pause() {
#ifdef MOZ_WIDGET_ANDROID
  if (!gl() || gl()->IsDestroyed()) return;
  // ReleaseSurface internally calls MakeCurrent
  gl()->ReleaseSurface();
#elif defined(MOZ_WIDGET_GTK)
  // ReleaseSurface internally calls MakeCurrent
  gl()->ReleaseSurface();
#endif
}

bool CompositorOGL::Resume() {
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_UIKIT) || \
    defined(MOZ_WIDGET_GTK)
  if (!gl() || gl()->IsDestroyed()) return false;

  // RenewSurface internally calls MakeCurrent.
  return gl()->RenewSurface(GetWidget());
#else
  return true;
#endif
}

already_AddRefed<DataTextureSource> CompositorOGL::CreateDataTextureSource(
    TextureFlags aFlags) {
  if (!gl()) {
    return nullptr;
  }

  return MakeAndAddRef<TextureImageTextureSourceOGL>(this, aFlags);
}

int32_t CompositorOGL::GetMaxTextureSize() const {
  MOZ_ASSERT(mGLContext);
  GLint texSize = 0;
  mGLContext->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, &texSize);
  MOZ_ASSERT(texSize != 0);
  return texSize;
}

void CompositorOGL::MakeCurrent(MakeCurrentFlags aFlags) {
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }
  mGLContext->MakeCurrent(aFlags & ForceMakeCurrent);
}

GLuint CompositorOGL::GetTemporaryTexture(GLenum aTarget, GLenum aUnit) {
  if (!mTexturePool) {
    mTexturePool = new PerUnitTexturePoolOGL(gl());
  }
  return mTexturePool->GetTexture(aTarget, aUnit);
}

GLuint PerUnitTexturePoolOGL::GetTexture(GLenum aTarget, GLenum aTextureUnit) {
  if (mTextureTarget == 0) {
    mTextureTarget = aTarget;
  }
  MOZ_ASSERT(mTextureTarget == aTarget);

  size_t index = aTextureUnit - LOCAL_GL_TEXTURE0;
  // lazily grow the array of temporary textures
  if (mTextures.Length() <= index) {
    size_t prevLength = mTextures.Length();
    mTextures.SetLength(index + 1);
    for (unsigned int i = prevLength; i <= index; ++i) {
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
    mGL->fTexParameteri(aTarget, LOCAL_GL_TEXTURE_WRAP_S,
                        LOCAL_GL_CLAMP_TO_EDGE);
    mGL->fTexParameteri(aTarget, LOCAL_GL_TEXTURE_WRAP_T,
                        LOCAL_GL_CLAMP_TO_EDGE);
  }
  return mTextures[index];
}

void PerUnitTexturePoolOGL::DestroyTextures() {
  if (mGL && mGL->MakeCurrent()) {
    if (mTextures.Length() > 0) {
      mGL->fDeleteTextures(mTextures.Length(), &mTextures[0]);
    }
  }
  mTextures.SetLength(0);
}

void CompositorOGL::RegisterTextureSource(TextureSource* aTextureSource) {
#ifdef MOZ_WIDGET_GTK
  if (mDestroyed) {
    return;
  }
  mRegisteredTextureSources.insert(aTextureSource);
#endif
}

void CompositorOGL::UnregisterTextureSource(TextureSource* aTextureSource) {
#ifdef MOZ_WIDGET_GTK
  if (mDestroyed) {
    return;
  }
  mRegisteredTextureSources.erase(aTextureSource);
#endif
}

}  // namespace layers
}  // namespace mozilla
