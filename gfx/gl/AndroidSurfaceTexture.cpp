/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_ANDROID

#  include "AndroidSurfaceTexture.h"

#  include "GeneratedJNINatives.h"

#  include "AndroidNativeWindow.h"
#  include "GLContextEGL.h"
#  include "GLBlitHelper.h"
#  include "GLImages.h"

using namespace mozilla;

namespace mozilla {
namespace gl {

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

class SharedGL final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedGL);

  explicit SharedGL(AndroidNativeWindow& window) {
    MutexAutoLock lock(sMutex);

    if (!sContext) {
      MOZ_ASSERT(sInstanceCount == 0);
      sContext = CreateContext();
      if (!sContext) {
        return;
      }
    }

    InitSurface(window);
    ++sInstanceCount;
  }

  void Blit(const AndroidSurfaceTextureHandle& sourceTextureHandle,
            const gfx::IntSize& imageSize) {
    MutexAutoLock lock(sMutex);
    MOZ_ASSERT(sContext);

    // Setting overide also makes conext and surface current.
    sContext->SetEGLSurfaceOverride(mTargetSurface);
    RefPtr<layers::SurfaceTextureImage> img = new layers::SurfaceTextureImage(
        sourceTextureHandle, imageSize, false, OriginPos::TopLeft);
    sContext->BlitHelper()->BlitImage(img, imageSize, OriginPos::BottomLeft);
    sContext->SwapBuffers();
    // This method is called through binder IPC and could run on any thread in
    // the pool. Release the context and surface from this thread after use so
    // they can be bound to another thread later.
    UnmakeCurrent(sContext);
  }

 private:
  ~SharedGL() {
    MutexAutoLock lock(sMutex);

    if (mTargetSurface != EGL_NO_SURFACE) {
      GLLibraryEGL::Get()->fDestroySurface(EGL_DISPLAY(), mTargetSurface);
    }

    // Destroy shared GL context when no one uses it.
    if (--sInstanceCount == 0) {
      sContext = nullptr;
    }
  }

  static already_AddRefed<GLContextEGL> CreateContext() {
    sMutex.AssertCurrentThreadOwns();
    MOZ_ASSERT(!sContext);

    auto* egl = gl::GLLibraryEGL::Get();
    EGLDisplay eglDisplay = egl->fGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLConfig eglConfig;
    CreateConfig(&eglConfig, /* bpp */ 24, /* depth buffer? */ false);
    EGLint attributes[] = {LOCAL_EGL_CONTEXT_CLIENT_VERSION, 2, LOCAL_EGL_NONE};
    EGLContext eglContext =
        egl->fCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, attributes);
    RefPtr<GLContextEGL> gl = new GLContextEGL(
        CreateContextFlags::NONE, SurfaceCaps::Any(),
        /* offscreen? */ false, eglConfig, EGL_NO_SURFACE, eglContext);
    if (!gl->Init()) {
      NS_WARNING("Fail to create GL context for native blitter.");
      return nullptr;
    }

    // Yield the current state made in constructor.
    UnmakeCurrent(gl);
    return gl.forget();
  }

  void InitSurface(AndroidNativeWindow& window) {
    sMutex.AssertCurrentThreadOwns();
    MOZ_ASSERT(sContext);

    mTargetSurface = gl::GLLibraryEGL::Get()->fCreateWindowSurface(
        sContext->GetEGLDisplay(), sContext->mConfig, window.NativeWindow(), 0);
  }

  static bool UnmakeCurrent(RefPtr<GLContextEGL>& gl) {
    sMutex.AssertCurrentThreadOwns();
    MOZ_ASSERT(gl);

    if (!gl->IsCurrent()) {
      return true;
    }

    return gl::GLLibraryEGL::Get()->fMakeCurrent(
        EGL_DISPLAY(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  }

  static Mutex sMutex;
  static RefPtr<GLContextEGL> sContext;
  static size_t sInstanceCount;

  EGLSurface mTargetSurface;
};

Mutex SharedGL::sMutex("SharedGLContext::sMutex");
RefPtr<GLContextEGL> SharedGL::sContext(nullptr);
size_t SharedGL::sInstanceCount = 0;

class GLBlitterSupport final
    : public java::GeckoSurfaceTexture::NativeGLBlitHelper::Natives<
          GLBlitterSupport> {
 public:
  using Base =
      java::GeckoSurfaceTexture::NativeGLBlitHelper::Natives<GLBlitterSupport>;
  using Base::AttachNative;
  using Base::DisposeNative;
  using Base::GetNative;

  static java::GeckoSurfaceTexture::NativeGLBlitHelper::LocalRef Create(
      jint sourceTextureHandle, jni::Object::Param targetSurface, jint width,
      jint height) {
    AndroidNativeWindow win(java::GeckoSurface::Ref::From(targetSurface));
    auto helper = java::GeckoSurfaceTexture::NativeGLBlitHelper::New();
    RefPtr<SharedGL> gl = new SharedGL(win);
    GLBlitterSupport::AttachNative(
        helper, MakeUnique<GLBlitterSupport>(std::move(gl), sourceTextureHandle,
                                             width, height));
    return helper;
  }

  GLBlitterSupport(RefPtr<SharedGL>&& gl, jint sourceTextureHandle, jint width,
                   jint height)
      : mGl(gl),
        mSourceTextureHandle(sourceTextureHandle),
        mSize(width, height) {}

  void Blit() { mGl->Blit(mSourceTextureHandle, mSize); }

 private:
  const RefPtr<SharedGL> mGl;
  const AndroidSurfaceTextureHandle mSourceTextureHandle;
  const gfx::IntSize mSize;
};

void AndroidSurfaceTexture::Init() { GLBlitterSupport::Init(); }

}  // namespace gl
}  // namespace mozilla
#endif  // MOZ_WIDGET_ANDROID
