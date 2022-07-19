/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidSurfaceTexture.h"

#include "GLContextEGL.h"
#include "GLBlitHelper.h"
#include "GLImages.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/LayersSurfaces.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/java/GeckoSurfaceTextureNatives.h"
#  include "AndroidNativeWindow.h"
#endif  // MOZ_WIDGET_ANDROID

namespace mozilla {
namespace gl {

class AndroidSharedBlitGL final {
 public:
  explicit AndroidSharedBlitGL(const EGLNativeWindowType window) {
    StaticMutexAutoLock lock(sMutex);

    if (!sContext) {
      MOZ_ASSERT(sInstanceCount == 0);
      sContext = CreateContext();
      if (!sContext) {
        return;
      }
    }

    const auto& egl = *(sContext->mEgl);
    mTargetSurface = egl.fCreateWindowSurface(sContext->mConfig, window, 0);

    ++sInstanceCount;
  }

  ~AndroidSharedBlitGL() {
    StaticMutexAutoLock lock(sMutex);

    if (mTargetSurface != EGL_NO_SURFACE) {
      const auto& egl = *(sContext->mEgl);
      egl.fDestroySurface(mTargetSurface);
    }

    // Destroy shared GL context when no one uses it.
    if (--sInstanceCount == 0) {
      sContext = nullptr;
    }
  }

#ifdef MOZ_WIDGET_ANDROID
  void Blit(const java::GeckoSurfaceTexture::Ref& surfaceTexture,
            const gfx::IntSize& imageSize) {
    StaticMutexAutoLock lock(sMutex);
    MOZ_ASSERT(sContext);

    // Setting overide also makes conext and surface current.
    sContext->SetEGLSurfaceOverride(mTargetSurface);
    DebugOnly<bool> rv = sContext->BlitHelper()->Blit(surfaceTexture, imageSize,
                                                      OriginPos::TopLeft);
    MOZ_ASSERT(rv);
    sContext->SwapBuffers();
    // This method is called through binder IPC and could run on any thread in
    // the pool. Release the context and surface from this thread after use so
    // they can be bound to another thread later.
    UnmakeCurrent(sContext);
  }
#endif

 private:
  static already_AddRefed<GLContextEGL> CreateContextImpl(bool aUseGles) {
    sMutex.AssertCurrentThreadOwns();
    MOZ_ASSERT(!sContext);

    nsCString ignored;
    const auto egl = gl::DefaultEglDisplay(&ignored);
    EGLConfig eglConfig;
    CreateConfig(*egl, &eglConfig, /* bpp */ 24, /* depth buffer? */ false,
                 aUseGles);
    auto gl = GLContextEGL::CreateGLContext(egl, {}, eglConfig, EGL_NO_SURFACE,
                                            true, &ignored);
    if (!gl) {
      NS_WARNING("Fail to create GL context for native blitter.");
      return nullptr;
    }
    gl->mOwningThreadId = Nothing();

    // Yield the current state made in constructor.
    UnmakeCurrent(gl);
    return gl.forget();
  }

  static already_AddRefed<GLContextEGL> CreateContext() {
    RefPtr<GLContextEGL> gl;
#if !defined(MOZ_WIDGET_ANDROID)
    gl = CreateContextImpl(/* aUseGles */ false);
#endif  // !defined(MOZ_WIDGET_ANDROID)

    if (!gl) {
      gl = CreateContextImpl(/* aUseGles */ true);
    }
    return gl.forget();
  }

  static bool UnmakeCurrent(GLContextEGL* const gl) {
    sMutex.AssertCurrentThreadOwns();
    MOZ_ASSERT(gl);

    if (!gl->IsCurrent()) {
      return true;
    }
    const auto& egl = *(gl->mEgl);
    return egl.fMakeCurrent(EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  }

  static StaticMutex sMutex MOZ_UNANNOTATED;
  static StaticRefPtr<GLContextEGL> sContext;
  static size_t sInstanceCount;

  EGLSurface mTargetSurface;
};

StaticMutex AndroidSharedBlitGL::sMutex;
StaticRefPtr<GLContextEGL> AndroidSharedBlitGL::sContext;
size_t AndroidSharedBlitGL::sInstanceCount = 0;

// -
#ifdef MOZ_WIDGET_ANDROID

void AndroidSurfaceTexture::GetTransformMatrix(
    java::sdk::SurfaceTexture::Param surfaceTexture,
    gfx::Matrix4x4* outMatrix) {
  JNIEnv* const env = jni::GetEnvForThread();

  auto jarray = jni::FloatArray::LocalRef::Adopt(env, env->NewFloatArray(16));
  surfaceTexture->GetTransformMatrix(jarray);

  jfloat* array = env->GetFloatArrayElements(jarray.Get(), nullptr);

  memcpy(&(outMatrix->_11), array, sizeof(float) * 16);

  env->ReleaseFloatArrayElements(jarray.Get(), array, 0);
}

class GLBlitterSupport final
    : public java::GeckoSurfaceTexture::NativeGLBlitHelper::Natives<
          GLBlitterSupport> {
 public:
  using Base =
      java::GeckoSurfaceTexture::NativeGLBlitHelper::Natives<GLBlitterSupport>;
  using Base::AttachNative;
  using Base::DisposeNative;
  using Base::GetNative;

  static java::GeckoSurfaceTexture::NativeGLBlitHelper::LocalRef NativeCreate(
      jlong sourceTextureHandle, jni::Object::Param targetSurface, jint width,
      jint height) {
    AndroidNativeWindow win(java::GeckoSurface::Ref::From(targetSurface));
    auto helper = java::GeckoSurfaceTexture::NativeGLBlitHelper::New();
    const auto& eglWindow = win.NativeWindow();
    GLBlitterSupport::AttachNative(
        helper,
        MakeUnique<GLBlitterSupport>(MakeUnique<AndroidSharedBlitGL>(eglWindow),
                                     sourceTextureHandle, width, height));
    return helper;
  }

  GLBlitterSupport(UniquePtr<AndroidSharedBlitGL>&& gl,
                   jlong sourceTextureHandle, jint width, jint height)
      : mGl(std::move(gl)),
        mSourceTextureHandle(sourceTextureHandle),
        mSize(width, height) {}

  void Blit() {
    auto surfaceTexture =
        java::GeckoSurfaceTexture::Lookup(mSourceTextureHandle);
    mGl->Blit(surfaceTexture, mSize);
  }

 private:
  const UniquePtr<AndroidSharedBlitGL> mGl;
  const AndroidSurfaceTextureHandle mSourceTextureHandle;
  const gfx::IntSize mSize;
};

void AndroidSurfaceTexture::Init() { GLBlitterSupport::Init(); }

#endif  // MOZ_WIDGET_ANDROID

}  // namespace gl
}  // namespace mozilla
