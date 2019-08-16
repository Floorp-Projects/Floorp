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
#include "Layers.h"                 // for WriteSnapshotToDumpFile
#include "LayerScope.h"             // for LayerScope
#include "gfxCrashReporterUtils.h"  // for ScopedGfxFeatureReporter
#include "gfxEnv.h"                 // for gfxEnv
#include "gfxPlatform.h"            // for gfxPlatform
#include "gfxRect.h"                // for gfxRect
#include "gfxUtils.h"               // for gfxUtils, etc
#include "mozilla/ArrayUtils.h"     // for ArrayLength
#include "mozilla/Preferences.h"    // for Preferences
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/gfx/BasePoint.h"  // for BasePoint
#include "mozilla/gfx/Matrix.h"     // for Matrix4x4, Matrix
#include "mozilla/gfx/Triangle.h"   // for Triangle
#include "mozilla/gfx/gfxVars.h"    // for gfxVars
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/LayerManagerComposite.h"  // for LayerComposite, etc
#include "mozilla/layers/CompositingRenderTargetOGL.h"
#include "mozilla/layers/Effects.h"      // for EffectChain, TexturedEffect, etc
#include "mozilla/layers/TextureHost.h"  // for TextureSource, etc
#include "mozilla/layers/TextureHostOGL.h"  // for TextureSourceOGL, etc
#include "mozilla/layers/PTextureParent.h"  // for OtherPid() on PTextureParent
#ifdef XP_DARWIN
#  include "mozilla/layers/TextureSync.h"  // for TextureSync::etc.
#endif
#include "mozilla/mozalloc.h"  // for operator delete, etc
#include "nsAppRunner.h"
#include "nsAString.h"
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
#include "GLBlitTextureImageHelper.h"
#include "HeapCopyOfStackArray.h"
#include "GLBlitHelper.h"
#include "mozilla/gfx/Swizzle.h"

#if MOZ_WIDGET_ANDROID
#  include "GeneratedJNIWrappers.h"
#endif

#include "GeckoProfiler.h"

namespace mozilla {

using namespace std;
using namespace gfx;

namespace layers {

using namespace mozilla::gl;

static const GLuint kCoordinateAttributeIndex = 0;
static const GLuint kTexCoordinateAttributeIndex = 1;

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

static void BindMaskForProgram(ShaderProgramOGL* aProgram,
                               TextureSourceOGL* aSourceMask, GLenum aTexUnit,
                               const gfx::Matrix4x4& aTransform) {
  MOZ_ASSERT(LOCAL_GL_TEXTURE0 <= aTexUnit && aTexUnit <= LOCAL_GL_TEXTURE31);
  aSourceMask->BindTexture(aTexUnit, gfx::SamplingFilter::LINEAR);
  aProgram->SetMaskTextureUnit(aTexUnit - LOCAL_GL_TEXTURE0);
  aProgram->SetMaskLayerTransform(aTransform);
}

void CompositorOGL::BindBackdrop(ShaderProgramOGL* aProgram, GLuint aBackdrop,
                                 GLenum aTexUnit) {
  MOZ_ASSERT(aBackdrop);

  mGLContext->fActiveTexture(aTexUnit);
  mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, aBackdrop);
  mGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER,
                             LOCAL_GL_LINEAR);
  mGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER,
                             LOCAL_GL_LINEAR);
  aProgram->SetBackdropTextureUnit(aTexUnit - LOCAL_GL_TEXTURE0);
}

CompositorOGL::CompositorOGL(CompositorBridgeParent* aParent,
                             widget::CompositorWidget* aWidget,
                             int aSurfaceWidth, int aSurfaceHeight,
                             bool aUseExternalSurfaceSize)
    : Compositor(aWidget, aParent),
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
      mViewportSize(0, 0),
      mCurrentProgram(nullptr) {
#ifdef XP_DARWIN
  TextureSync::RegisterTextureSourceProvider(this);
#endif
  MOZ_COUNT_CTOR(CompositorOGL);
}

CompositorOGL::~CompositorOGL() {
#ifdef XP_DARWIN
  TextureSync::UnregisterTextureSourceProvider(this);
#endif
  MOZ_COUNT_DTOR(CompositorOGL);
  Destroy();
}

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
  if (gfxEnv::LayersPreferEGL()) {
    printf_stderr("Trying GL layers...\n");
    context = gl::GLContextProviderEGL::CreateForCompositorWidget(
        mWidget, /* aWebRender */ false, /* aForceAccelerated */ false);
  }
#endif

  // Allow to create offscreen GL context for main Layer Manager
  if (!context && gfxEnv::LayersPreferOffscreen()) {
    SurfaceCaps caps = SurfaceCaps::ForRGB();
    caps.preserve = false;
    caps.bpp16 = gfxVars::OffscreenFormat() == SurfaceFormat::R5G6B5_UINT16;

    nsCString discardFailureId;
    context = GLContextProvider::CreateOffscreen(
        mSurfaceSize, caps, CreateContextFlags::REQUIRE_COMPAT_PROFILE,
        &discardFailureId);
  }

  if (!context) {
    context = gl::GLContextProvider::CreateForCompositorWidget(
        mWidget,
        /* aWebRender */ false,
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

#ifdef XP_DARWIN
  mMaybeUnlockBeforeNextComposition.Clear();
#endif

  if (!mDestroyed) {
    mDestroyed = true;
    CleanupResources();
  }
}

void CompositorOGL::CleanupResources() {
  if (!mGLContext) return;

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
    mGLContext = nullptr;
    mPrograms.clear();
    return;
  }

  for (std::map<ShaderConfigOGL, ShaderProgramOGL*>::iterator iter =
           mPrograms.begin();
       iter != mPrograms.end(); iter++) {
    delete iter->second;
  }
  mPrograms.clear();

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

  mBlitTextureImageHelper = nullptr;

  // On the main thread the Widget will be destroyed soon and calling
  // MakeCurrent after that could cause a crash (at least with GLX, see bug
  // 1059793), unless context is marked as destroyed. There may be some textures
  // still alive that will try to call MakeCurrent on the context so let's make
  // sure it is marked destroyed now.
  mGLContext->MarkDestroyed();

  mGLContext = nullptr;
}

bool CompositorOGL::Initialize(nsCString* const out_failureReason) {
  ScopedGfxFeatureReporter reporter("GL Layers");

  // Do not allow double initialization
  MOZ_ASSERT(mGLContext == nullptr, "Don't reinitialize CompositorOGL");

  mGLContext = CreateContext();

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

  MakeCurrent();

  mHasBGRA = mGLContext->IsExtensionSupported(
                 gl::GLContext::EXT_texture_format_BGRA8888) ||
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
    msg += NS_LITERAL_STRING(
        "OpenGL compositor Initialized Succesfully.\nVersion: ");
    msg += NS_ConvertUTF8toUTF16(nsDependentCString(
        (const char*)mGLContext->fGetString(LOCAL_GL_VERSION)));
    msg += NS_LITERAL_STRING("\nVendor: ");
    msg += NS_ConvertUTF8toUTF16(nsDependentCString(
        (const char*)mGLContext->fGetString(LOCAL_GL_VENDOR)));
    msg += NS_LITERAL_STRING("\nRenderer: ");
    msg += NS_ConvertUTF8toUTF16(nsDependentCString(
        (const char*)mGLContext->fGetString(LOCAL_GL_RENDERER)));
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
  const gfx::IntSize& size = aRenderTarget->mInitParams.mSize;
  // Physical surface size.
  const gfx::IntSize& phySize = aRenderTarget->mInitParams.mPhySize;

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
  IntSize FBOSize;
  CreateFBOWithTexture(rect, false, 0, &fbo, &tex, &FBOSize);
  RefPtr<CompositingRenderTargetOGL> surface =
      new CompositingRenderTargetOGL(this, aRect.TopLeft(), tex, fbo);
  surface->Initialize(aRect.Size(), FBOSize, mFBOTextureTarget, aInit);
  return surface.forget();
}

already_AddRefed<CompositingRenderTarget>
CompositorOGL::CreateRenderTargetFromSource(
    const IntRect& aRect, const CompositingRenderTarget* aSource,
    const IntPoint& aSourcePoint) {
  MOZ_ASSERT(!aRect.IsZeroArea(),
             "Trying to create a render target of invalid size");

  if (aRect.IsZeroArea()) {
    return nullptr;
  }

  if (!gl()) {
    return nullptr;
  }

  GLuint tex = 0;
  GLuint fbo = 0;
  const CompositingRenderTargetOGL* sourceSurface =
      static_cast<const CompositingRenderTargetOGL*>(aSource);
  IntRect sourceRect(aSourcePoint, aRect.Size());
  if (aSource) {
    CreateFBOWithTexture(sourceRect, true, sourceSurface->GetFBO(), &fbo, &tex);
  } else {
    CreateFBOWithTexture(sourceRect, true, 0, &fbo, &tex);
  }

  RefPtr<CompositingRenderTargetOGL> surface =
      new CompositingRenderTargetOGL(this, aRect.TopLeft(), tex, fbo);
  surface->Initialize(aRect.Size(), sourceRect.Size(), mFBOTextureTarget,
                      INIT_MODE_NONE);
  return surface.forget();
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
      srcFBO, destFBO, aSourceSize, aDestSize, LOCAL_GL_LINEAR);
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

void CompositorOGL::ClearRect(const gfx::Rect& aRect) {
  // Map aRect to OGL coordinates, origin:bottom-left
  GLint y = mViewportSize.height - aRect.YMost();

  ScopedGLState scopedScissorTestState(mGLContext, LOCAL_GL_SCISSOR_TEST, true);
  ScopedScissorRect autoScissorRect(mGLContext, aRect.X(), y, aRect.Width(),
                                    aRect.Height());
  mGLContext->fClearColor(0.0, 0.0, 0.0, 0.0);
  mGLContext->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT);
}

void CompositorOGL::BeginFrame(const nsIntRegion& aInvalidRegion,
                               const IntRect* aClipRectIn,
                               const IntRect& aRenderBounds,
                               const nsIntRegion& aOpaqueRegion,
                               IntRect* aClipRectOut,
                               IntRect* aRenderBoundsOut) {
  AUTO_PROFILER_LABEL("CompositorOGL::BeginFrame", GRAPHICS);

  MOZ_ASSERT(!mFrameInProgress,
             "frame still in progress (should have called EndFrame");

  gfx::IntRect rect;
  if (mUseExternalSurfaceSize) {
    rect = gfx::IntRect(0, 0, mSurfaceSize.width, mSurfaceSize.height);
  } else {
    rect = gfx::IntRect(aRenderBounds.X(), aRenderBounds.Y(),
                        aRenderBounds.Width(), aRenderBounds.Height());
  }

  if (aRenderBoundsOut) {
    *aRenderBoundsOut = rect;
  }

  auto width = rect.Width();
  auto height = rect.Height();

  // We can't draw anything to something with no area
  // so just return
  if (width == 0 || height == 0) return;

  // We're about to actually draw a frame.
  mFrameInProgress = true;

  // If the widget size changed, we have to force a MakeCurrent
  // to make sure that GL sees the updated widget size.
  if (mWidgetSize.width != width || mWidgetSize.height != height) {
    MakeCurrent(ForceMakeCurrent);

    mWidgetSize.width = width;
    mWidgetSize.height = height;
  } else {
    MakeCurrent();
  }

  mPixelsPerFrame = width * height;
  mPixelsFilled = 0;

#ifdef MOZ_WIDGET_ANDROID
  java::GeckoSurfaceTexture::DestroyUnused((int64_t)mGLContext.get());
  mGLContext->MakeCurrent();  // DestroyUnused can change the current context!
#endif

  // Default blend function implements "OVER"
  mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                 LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA);
  mGLContext->fEnable(LOCAL_GL_BLEND);

  RefPtr<CompositingRenderTargetOGL> rt =
      CompositingRenderTargetOGL::RenderTargetForWindow(this,
                                                        IntSize(width, height));
  SetRenderTarget(rt);
  mWindowRenderTarget = mCurrentRenderTarget;

  if (aClipRectOut && !aClipRectIn) {
    aClipRectOut->SetRect(0, 0, width, height);
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
  mGLContext->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT);
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
    } else {
      // Curses, incompatible formats.  Take a slow path.

      // RGBA
      size_t bufferSize = clampedRectWidth * clampedRectHeight * 4;
      auto buf = MakeUnique<uint8_t[]>(bufferSize);

      mGLContext->fReadPixels(clampedRect.X(), clampedRect.Y(),
                              clampedRectWidth, clampedRectHeight,
                              LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, buf.get());
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
                                                  TextureSourceOGL* aSourceMask,
                                                  gfx::CompositionOp aOp,
                                                  bool aColorMatrix,
                                                  bool aDEAAEnabled) const {
  ShaderConfigOGL config;

  switch (aEffect->mType) {
    case EffectTypes::SOLID_COLOR:
      config.SetRenderColor(true);
      break;
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
      config.SetTextureTarget(LOCAL_GL_TEXTURE_RECTANGLE_ARB);
      break;
    case EffectTypes::COMPONENT_ALPHA: {
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
  config.SetColorMatrix(aColorMatrix);
  config.SetMask(!!aSourceMask);
  if (aSourceMask) {
    config.SetMaskTextureTarget(aSourceMask->GetTextureTarget());
  }
  config.SetDEAA(aDEAAEnabled);
  config.SetCompositionOp(aOp);
  return config;
}

ShaderProgramOGL* CompositorOGL::GetShaderProgramFor(
    const ShaderConfigOGL& aConfig) {
  std::map<ShaderConfigOGL, ShaderProgramOGL*>::iterator iter =
      mPrograms.find(aConfig);
  if (iter != mPrograms.end()) return iter->second;

  ProgramProfileOGL profile = ProgramProfileOGL::GetProfileFor(aConfig);
  ShaderProgramOGL* shader = new ShaderProgramOGL(gl(), profile);
  if (!shader->Initialize()) {
    gfxCriticalError() << "Shader compilation failure, cfg:"
                       << " features: " << gfx::hexa(aConfig.mFeatures)
                       << " multiplier: " << aConfig.mMultiplier
                       << " op: " << aConfig.mCompositionOp;
    delete shader;
    return nullptr;
  }

  mPrograms[aConfig] = shader;
  return shader;
}

void CompositorOGL::ActivateProgram(ShaderProgramOGL* aProg) {
  if (mCurrentProgram != aProg) {
    gl()->fUseProgram(aProg->GetProgram());
    mCurrentProgram = aProg;
  }
}

void CompositorOGL::ResetProgram() { mCurrentProgram = nullptr; }

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
  coeffecients.z = aPoint1.x * aPoint2.y - aPoint2.x * aPoint1.y;

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

void CompositorOGL::DrawTriangles(
    const nsTArray<gfx::TexturedTriangle>& aTriangles, const gfx::Rect& aRect,
    const gfx::IntRect& aClipRect, const EffectChain& aEffectChain,
    gfx::Float aOpacity, const gfx::Matrix4x4& aTransform,
    const gfx::Rect& aVisibleRect) {
  AUTO_PROFILER_LABEL("CompositorOGL::DrawTriangles", GRAPHICS);

  DrawGeometry(aTriangles, aRect, aClipRect, aEffectChain, aOpacity, aTransform,
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

  IntPoint offset = mCurrentRenderTarget->GetOrigin();
  IntSize size = mCurrentRenderTarget->GetSize();

  Rect renderBound(0, 0, size.width, size.height);
  renderBound.IntersectRect(renderBound, Rect(aClipRect));
  renderBound.MoveBy(offset);

  Rect destRect = aTransform.TransformAndClipBounds(aRect, renderBound);

  // XXX: This doesn't handle 3D transforms. It also doesn't handled rotated
  //      quads. Fix me.
  mPixelsFilled += destRect.Width() * destRect.Height();

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

  EffectMask* effectMask;
  Rect maskBounds;
  if (aEffectChain.mSecondaryEffects[EffectTypes::MASK]) {
    effectMask = static_cast<EffectMask*>(
        aEffectChain.mSecondaryEffects[EffectTypes::MASK].get());

    // We're assuming that the gl backend won't cheat and use NPOT
    // textures when glContext says it can't (which seems to happen
    // on a mac when you force POT textures)
    IntSize maskSize = CalculatePOTSize(effectMask->mSize, mGLContext);

    const gfx::Matrix4x4& maskTransform = effectMask->mMaskTransform;
    NS_ASSERTION(maskTransform.Is2D(),
                 "How did we end up with a 3D transform here?!");
    maskBounds = Rect(Point(), Size(maskSize));
    maskBounds = maskTransform.As2D().TransformBounds(maskBounds);

    clipRect = clipRect.Intersect(RoundedOut(maskBounds) - offset);
  }

  // aClipRect is in destination coordinate space (after all
  // transforms and offsets have been applied) so if our
  // drawing is going to be shifted by mRenderOffset then we need
  // to shift the clip rect by the same amount.
  if (!mTarget && mCurrentRenderTarget->IsWindow()) {
    clipRect.MoveBy(mRenderOffset.x + mSurfaceOrigin.x,
                    mRenderOffset.y - mSurfaceOrigin.y);
  }

  ScopedGLState scopedScissorTestState(mGLContext, LOCAL_GL_SCISSOR_TEST, true);
  ScopedScissorRect autoScissorRect(mGLContext, clipRect.X(),
                                    FlipY(clipRect.Y() + clipRect.Height()),
                                    clipRect.Width(), clipRect.Height());

  MaskType maskType;
  TextureSourceOGL* sourceMask = nullptr;
  gfx::Matrix4x4 maskQuadTransform;
  if (aEffectChain.mSecondaryEffects[EffectTypes::MASK]) {
    sourceMask = effectMask->mMaskTexture->AsSourceOGL();

    // NS_ASSERTION(textureMask->IsAlpha(),
    //              "OpenGL mask layers must be backed by alpha surfaces");

    maskQuadTransform._11 = 1.0f / maskBounds.Width();
    maskQuadTransform._22 = 1.0f / maskBounds.Height();
    maskQuadTransform._41 = float(-maskBounds.X()) / maskBounds.Width();
    maskQuadTransform._42 = float(-maskBounds.Y()) / maskBounds.Height();

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
    EffectBlendMode* blendEffect = static_cast<EffectBlendMode*>(
        aEffectChain.mSecondaryEffects[EffectTypes::BLEND_MODE].get());
    blendMode = blendEffect->mBlendMode;
  }

  // Only apply DEAA to quads that have been transformed such that aliasing
  // could be visible
  bool bEnableAA = StaticPrefs::layers_deaa_enabled() &&
                   !aTransform.Is2DIntegerTranslation();

  bool colorMatrix = aEffectChain.mSecondaryEffects[EffectTypes::COLOR_MATRIX];
  ShaderConfigOGL config =
      GetShaderConfigFor(aEffectChain.mPrimaryEffect, sourceMask, blendMode,
                         colorMatrix, bEnableAA);

  config.SetOpacity(aOpacity != 1.f);
  ApplyPrimitiveConfig(config, aGeometry);

  ShaderProgramOGL* program = GetShaderProgramFor(config);
  MOZ_DIAGNOSTIC_ASSERT(program);
  if (!program) {
    return;
  }
  ActivateProgram(program);
  program->SetProjectionMatrix(mProjMatrix);
  program->SetLayerTransform(aTransform);
  LayerScope::SetLayerTransform(aTransform);

  if (colorMatrix) {
    EffectColorMatrix* effectColorMatrix = static_cast<EffectColorMatrix*>(
        aEffectChain.mSecondaryEffects[EffectTypes::COLOR_MATRIX].get());
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
      gfx::IntRect rect = ComputeBackdropCopyRect(aRect, aClipRect, aTransform,
                                                  &backdropTransform);
      mixBlendBackdrop =
          CreateTexture(rect, true, mCurrentRenderTarget->GetFBO());
      createdMixBlendBackdropTexture = true;
    }
    program->SetBackdropTransform(backdropTransform);
  }

  program->SetRenderOffset(offset.x, offset.y);
  LayerScope::SetRenderOffset(offset.x, offset.y);

  if (aOpacity != 1.f) program->SetLayerOpacity(aOpacity);

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
    // This is used by IOSurface that use 0,0...w,h coordinate rather then
    // 0,0..1,1.
    program->SetTexCoordMultiplier(source->GetSize().width,
                                   source->GetSize().height);
  }

  if (sourceMask && config.mFeatures & ENABLE_MASK_TEXTURE_RECT) {
    program->SetMaskCoordMultiplier(sourceMask->GetSize().width,
                                    sourceMask->GetSize().height);
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
        winding =
            (points[wp1].x - points[wp].x) * (points[wp1].y + points[wp].y) +
            (points[wp2].x - points[wp1].x) * (points[wp2].y + points[wp1].y) +
            (points[wp].x - points[wp2].x) * (points[wp].y + points[wp2].y);
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
    case EffectTypes::SOLID_COLOR: {
      program->SetRenderColor(color);

      if (maskType != MaskType::MaskNone) {
        BindMaskForProgram(program, sourceMask, LOCAL_GL_TEXTURE0,
                           maskQuadTransform);
      }
      if (mixBlendBackdrop) {
        BindBackdrop(program, mixBlendBackdrop, LOCAL_GL_TEXTURE1);
      }

      didSetBlendMode = SetBlendMode(gl(), blendMode);

      BindAndDrawGeometry(program, aGeometry);
    } break;

    case EffectTypes::RGB: {
      TexturedEffect* texturedEffect =
          static_cast<TexturedEffect*>(aEffectChain.mPrimaryEffect.get());
      TextureSource* source = texturedEffect->mTexture;

      didSetBlendMode =
          SetBlendMode(gl(), blendMode, texturedEffect->mPremultiplied);

      gfx::SamplingFilter samplingFilter = texturedEffect->mSamplingFilter;

      source->AsSourceOGL()->BindTexture(LOCAL_GL_TEXTURE0, samplingFilter);

      program->SetTextureUnit(0);

      Matrix4x4 textureTransform = source->AsSourceOGL()->GetTextureTransform();
      program->SetTextureTransform(textureTransform);

      if (maskType != MaskType::MaskNone) {
        BindMaskForProgram(program, sourceMask, LOCAL_GL_TEXTURE1,
                           maskQuadTransform);
      }
      if (mixBlendBackdrop) {
        BindBackdrop(program, mixBlendBackdrop, LOCAL_GL_TEXTURE2);
      }

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

      if (maskType != MaskType::MaskNone) {
        BindMaskForProgram(program, sourceMask, LOCAL_GL_TEXTURE3,
                           maskQuadTransform);
      }
      if (mixBlendBackdrop) {
        BindBackdrop(program, mixBlendBackdrop, LOCAL_GL_TEXTURE4);
      }
      didSetBlendMode = SetBlendMode(gl(), blendMode);
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

      if (maskType != MaskType::MaskNone) {
        BindMaskForProgram(program, sourceMask, LOCAL_GL_TEXTURE2,
                           maskQuadTransform);
      }
      if (mixBlendBackdrop) {
        BindBackdrop(program, mixBlendBackdrop, LOCAL_GL_TEXTURE3);
      }
      didSetBlendMode = SetBlendMode(gl(), blendMode);
      BindAndDrawGeometryWithTextureRect(program, aGeometry,
                                         effectNV12->mTextureCoords,
                                         sourceNV12->GetSubSource(Y));
      sourceY->MaybeFenceTexture();
      sourceCbCr->MaybeFenceTexture();
    } break;
    case EffectTypes::RENDER_TARGET: {
      EffectRenderTarget* effectRenderTarget =
          static_cast<EffectRenderTarget*>(aEffectChain.mPrimaryEffect.get());
      RefPtr<CompositingRenderTargetOGL> surface =
          static_cast<CompositingRenderTargetOGL*>(
              effectRenderTarget->mRenderTarget.get());

      surface->BindTexture(LOCAL_GL_TEXTURE0, mFBOTextureTarget);

      // Drawing is always flipped, but when copying between surfaces we want to
      // avoid this, so apply a flip here to cancel the other one out.
      Matrix transform;
      transform.PreTranslate(0.0, 1.0);
      transform.PreScale(1.0f, -1.0f);
      program->SetTextureTransform(Matrix4x4::From2D(transform));
      program->SetTextureUnit(0);

      if (maskType != MaskType::MaskNone) {
        BindMaskForProgram(program, sourceMask, LOCAL_GL_TEXTURE1,
                           maskQuadTransform);
      }
      if (mixBlendBackdrop) {
        BindBackdrop(program, mixBlendBackdrop, LOCAL_GL_TEXTURE2);
      }

      if (config.mFeatures & ENABLE_TEXTURE_RECT) {
        // 2DRect case, get the multiplier right for a sampler2DRect
        program->SetTexCoordMultiplier(surface->GetSize().width,
                                       surface->GetSize().height);
      }

      // Drawing is always flipped, but when copying between surfaces we want to
      // avoid this. Pass true for the flip parameter to introduce a second flip
      // that cancels the other one out.
      didSetBlendMode = SetBlendMode(gl(), blendMode);
      BindAndDrawGeometry(program, aGeometry);
    } break;
    case EffectTypes::COMPONENT_ALPHA: {
      MOZ_ASSERT(LayerManager::LayersComponentAlphaEnabled());
      MOZ_ASSERT(blendMode == gfx::CompositionOp::OP_OVER,
                 "Can't support blend modes with component alpha!");
      EffectComponentAlpha* effectComponentAlpha =
          static_cast<EffectComponentAlpha*>(aEffectChain.mPrimaryEffect.get());
      TextureSourceOGL* sourceOnWhite =
          effectComponentAlpha->mOnWhite->AsSourceOGL();
      TextureSourceOGL* sourceOnBlack =
          effectComponentAlpha->mOnBlack->AsSourceOGL();

      if (!sourceOnBlack->IsValid() || !sourceOnWhite->IsValid()) {
        NS_WARNING("Invalid layer texture for component alpha");
        return;
      }

      sourceOnBlack->BindTexture(LOCAL_GL_TEXTURE0,
                                 effectComponentAlpha->mSamplingFilter);
      sourceOnWhite->BindTexture(LOCAL_GL_TEXTURE1,
                                 effectComponentAlpha->mSamplingFilter);

      program->SetBlackTextureUnit(0);
      program->SetWhiteTextureUnit(1);
      program->SetTextureTransform(Matrix4x4());

      if (maskType != MaskType::MaskNone) {
        BindMaskForProgram(program, sourceMask, LOCAL_GL_TEXTURE2,
                           maskQuadTransform);
      }
      // Pass 1.
      gl()->fBlendFuncSeparate(LOCAL_GL_ZERO, LOCAL_GL_ONE_MINUS_SRC_COLOR,
                               LOCAL_GL_ONE, LOCAL_GL_ONE);
      program->SetTexturePass2(false);
      BindAndDrawGeometryWithTextureRect(program, aGeometry,
                                         effectComponentAlpha->mTextureCoords,
                                         effectComponentAlpha->mOnBlack);

      // Pass 2.
      gl()->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE, LOCAL_GL_ONE,
                               LOCAL_GL_ONE);
      program->SetTexturePass2(true);
      BindAndDrawGeometryWithTextureRect(program, aGeometry,
                                         effectComponentAlpha->mTextureCoords,
                                         effectComponentAlpha->mOnBlack);

      mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                     LOCAL_GL_ONE,
                                     LOCAL_GL_ONE_MINUS_SRC_ALPHA);

      sourceOnBlack->MaybeFenceTexture();
      sourceOnWhite->MaybeFenceTexture();
    } break;
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

  LayerScope::DrawEnd(mGLContext, aEffectChain, aRect.Width(), aRect.Height());
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
  LayerScope::SetDrawRects(aQuads, aLayerRects, aTextureRects);
}

void CompositorOGL::InitializeVAO(const GLuint aAttrib, const GLint aComponents,
                                  const GLsizei aStride, const size_t aOffset) {
  mGLContext->fVertexAttribPointer(aAttrib, aComponents, LOCAL_GL_FLOAT,
                                   LOCAL_GL_FALSE, aStride,
                                   reinterpret_cast<GLvoid*>(aOffset));
  mGLContext->fEnableVertexAttribArray(aAttrib);
}

void CompositorOGL::EndFrame() {
  AUTO_PROFILER_LABEL("CompositorOGL::EndFrame", GRAPHICS);

  MOZ_ASSERT(mCurrentRenderTarget == mWindowRenderTarget,
             "Rendering target not properly restored");

#ifdef MOZ_DUMP_PAINTING
  if (gfxEnv::DumpCompositorTextures()) {
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

  if (mTarget) {
    CopyToTarget(mTarget, mTargetBounds.TopLeft(), Matrix());
    mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
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

  Compositor::EndFrame();
}

void CompositorOGL::InsertFrameDoneSync() {
#ifdef XP_MACOSX
  // Only do this on macOS.
  // On other platforms, SwapBuffers automatically applies back-pressure.
  if (StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
    if (mThisFrameDoneSync) {
      mGLContext->fDeleteSync(mThisFrameDoneSync);
    }
    mThisFrameDoneSync =
        mGLContext->fFenceSync(LOCAL_GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  }
#endif
}

void CompositorOGL::WaitForGPU() {
  if (mPreviousFrameDoneSync) {
    AUTO_PROFILER_LABEL("Waiting for GPU to finish previous frame", GRAPHICS);
    mGLContext->fClientWaitSync(mPreviousFrameDoneSync,
                                LOCAL_GL_SYNC_FLUSH_COMMANDS_BIT,
                                LOCAL_GL_TIMEOUT_IGNORED);
    mGLContext->fDeleteSync(mPreviousFrameDoneSync);
  }
  mPreviousFrameDoneSync = mThisFrameDoneSync;
  mThisFrameDoneSync = nullptr;
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

  if (!mGLContext->IsGLES()) {
    // GLES2 promises that binding to any custom FBO will attach
    // to GL_COLOR_ATTACHMENT0 attachment point.
    mGLContext->fReadBuffer(LOCAL_GL_BACK);
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
  gl()->MakeCurrent();
  java::GeckoSurfaceTexture::DestroyUnused((int64_t)mGLContext.get());
  java::GeckoSurfaceTexture::DetachAllFromGLContext((int64_t)mGLContext.get());
  // ReleaseSurface internally calls MakeCurrent
  gl()->ReleaseSurface();
#endif
}

bool CompositorOGL::Resume() {
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_UIKIT)
  if (!gl() || gl()->IsDestroyed()) return false;

  // RenewSurface internally calls MakeCurrent.
  return gl()->RenewSurface(GetWidget());
#endif
  return true;
}

already_AddRefed<DataTextureSource> CompositorOGL::CreateDataTextureSource(
    TextureFlags aFlags) {
  if (!gl()) {
    return nullptr;
  }

  return MakeAndAddRef<TextureImageTextureSourceOGL>(this, aFlags);
}

already_AddRefed<DataTextureSource>
CompositorOGL::CreateDataTextureSourceAroundYCbCr(TextureHost* aTexture) {
  if (!gl()) {
    return nullptr;
  }

  BufferTextureHost* bufferTexture = aTexture->AsBufferTextureHost();
  MOZ_ASSERT(bufferTexture);

  if (!bufferTexture) {
    return nullptr;
  }

  uint8_t* buf = bufferTexture->GetBuffer();
  const BufferDescriptor& buffDesc = bufferTexture->GetBufferDescriptor();
  const YCbCrDescriptor& desc = buffDesc.get_YCbCrDescriptor();

  RefPtr<gfx::DataSourceSurface> tempY =
      gfx::Factory::CreateWrappingDataSourceSurface(
          ImageDataSerializer::GetYChannel(buf, desc), desc.yStride(),
          desc.ySize(), SurfaceFormatForColorDepth(desc.colorDepth()));
  if (!tempY) {
    return nullptr;
  }
  RefPtr<gfx::DataSourceSurface> tempCb =
      gfx::Factory::CreateWrappingDataSourceSurface(
          ImageDataSerializer::GetCbChannel(buf, desc), desc.cbCrStride(),
          desc.cbCrSize(), SurfaceFormatForColorDepth(desc.colorDepth()));
  if (!tempCb) {
    return nullptr;
  }
  RefPtr<gfx::DataSourceSurface> tempCr =
      gfx::Factory::CreateWrappingDataSourceSurface(
          ImageDataSerializer::GetCrChannel(buf, desc), desc.cbCrStride(),
          desc.cbCrSize(), SurfaceFormatForColorDepth(desc.colorDepth()));
  if (!tempCr) {
    return nullptr;
  }

  RefPtr<DirectMapTextureSource> srcY = new DirectMapTextureSource(this, tempY);
  RefPtr<DirectMapTextureSource> srcU =
      new DirectMapTextureSource(this, tempCb);
  RefPtr<DirectMapTextureSource> srcV =
      new DirectMapTextureSource(this, tempCr);

  srcY->SetNextSibling(srcU);
  srcU->SetNextSibling(srcV);

  return srcY.forget();
}

#ifdef XP_DARWIN
void CompositorOGL::MaybeUnlockBeforeNextComposition(
    TextureHost* aTextureHost) {
  auto bufferTexture = aTextureHost->AsBufferTextureHost();
  if (bufferTexture) {
    mMaybeUnlockBeforeNextComposition.AppendElement(bufferTexture);
  }
}

void CompositorOGL::TryUnlockTextures() {
  nsClassHashtable<nsUint32HashKey, nsTArray<uint64_t>>
      texturesIdsToUnlockByPid;
  for (auto& texture : mMaybeUnlockBeforeNextComposition) {
    if (texture->IsDirectMap() && texture->CanUnlock()) {
      texture->ReadUnlock();
      auto actor = texture->GetIPDLActor();
      if (actor) {
        base::ProcessId pid = actor->OtherPid();
        nsTArray<uint64_t>* textureIds =
            texturesIdsToUnlockByPid.LookupOrAdd(pid);
        textureIds->AppendElement(TextureHost::GetTextureSerial(actor));
      }
    }
  }
  mMaybeUnlockBeforeNextComposition.Clear();
  for (auto it = texturesIdsToUnlockByPid.ConstIter(); !it.Done(); it.Next()) {
    TextureSync::SetTexturesUnlocked(it.Key(), *it.UserData());
  }
}
#endif

already_AddRefed<DataTextureSource>
CompositorOGL::CreateDataTextureSourceAround(gfx::DataSourceSurface* aSurface) {
  if (!gl()) {
    return nullptr;
  }

  return MakeAndAddRef<DirectMapTextureSource>(this, aSurface);
}

bool CompositorOGL::SupportsPartialTextureUpdate() {
  return CanUploadSubTextures(mGLContext);
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

GLBlitTextureImageHelper* CompositorOGL::BlitTextureImageHelper() {
  if (!mBlitTextureImageHelper) {
    mBlitTextureImageHelper = MakeUnique<GLBlitTextureImageHelper>(this);
  }

  return mBlitTextureImageHelper.get();
}

GLuint CompositorOGL::GetTemporaryTexture(GLenum aTarget, GLenum aUnit) {
  if (!mTexturePool) {
    mTexturePool = new PerUnitTexturePoolOGL(gl());
  }
  return mTexturePool->GetTexture(aTarget, aUnit);
}

bool CompositorOGL::SupportsTextureDirectMapping() {
  if (!StaticPrefs::gfx_allow_texture_direct_mapping_AtStartup()) {
    return false;
  }

  if (mGLContext) {
    mGLContext->MakeCurrent();
    return mGLContext->IsExtensionSupported(
               gl::GLContext::APPLE_client_storage) &&
           mGLContext->IsExtensionSupported(gl::GLContext::APPLE_texture_range);
  }

  return false;
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

bool CompositorOGL::SupportsLayerGeometry() const {
  return StaticPrefs::layers_geometry_opengl_enabled();
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
