/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderCompositorEGL.h"

#include "GLContext.h"
#include "GLContextEGL.h"
#include "GLContextProvider.h"
#include "GLLibraryEGL.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/BuildConstants.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/widget/CompositorWidget.h"

#ifdef MOZ_WAYLAND
#  include "mozilla/WidgetUtilsGtk.h"
#  include "mozilla/widget/GtkCompositorWidget.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/java/GeckoSurfaceTextureWrappers.h"
#  include "mozilla/layers/AndroidHardwareBuffer.h"
#  include "mozilla/widget/AndroidCompositorWidget.h"
#  include <android/native_window.h>
#  include <android/native_window_jni.h>
#endif

namespace mozilla::wr {

/* static */
UniquePtr<RenderCompositor> RenderCompositorEGL::Create(
    RefPtr<widget::CompositorWidget> aWidget, nsACString& aError) {
#ifdef MOZ_WAYLAND
  if (!mozilla::widget::GdkIsWaylandDisplay()) {
    return nullptr;
  }
#endif
  if (!RenderThread::Get()->SharedGL()) {
    gfxCriticalNote << "Failed to get shared GL context";
    return nullptr;
  }
  return MakeUnique<RenderCompositorEGL>(aWidget);
}

EGLSurface RenderCompositorEGL::CreateEGLSurface() {
  EGLSurface surface = EGL_NO_SURFACE;
  surface = gl::GLContextEGL::CreateEGLSurfaceForCompositorWidget(
      mWidget, gl::GLContextEGL::Cast(gl())->mConfig);
  if (surface == EGL_NO_SURFACE) {
    gfxCriticalNote << "Failed to create EGLSurface";
  }
  return surface;
}

RenderCompositorEGL::RenderCompositorEGL(
    RefPtr<widget::CompositorWidget> aWidget)
    : RenderCompositor(std::move(aWidget)), mEGLSurface(EGL_NO_SURFACE) {}

RenderCompositorEGL::~RenderCompositorEGL() {
#ifdef MOZ_WIDGET_ANDROID
  java::GeckoSurfaceTexture::DestroyUnused((int64_t)gl());
#endif
  DestroyEGLSurface();
}

bool RenderCompositorEGL::BeginFrame() {
#ifdef MOZ_WAYLAND
  if (mEGLSurface == EGL_NO_SURFACE) {
    gfxCriticalNote
        << "We don't have EGLSurface to draw into. Called too early?";
    return false;
  }
  if (mWidget->AsX11()) {
    mWidget->AsX11()->SetEGLNativeWindowSize(GetBufferSize());
  }
#endif
  if (!MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current, can't draw.";
    return false;
  }

#ifdef MOZ_WIDGET_ANDROID
  java::GeckoSurfaceTexture::DestroyUnused((int64_t)gl());
  gl()->MakeCurrent();  // DestroyUnused can change the current context!
#endif

  return true;
}

RenderedFrameId RenderCompositorEGL::EndFrame(
    const nsTArray<DeviceIntRect>& aDirtyRects) {
#ifdef MOZ_WIDGET_ANDROID
  const auto& gle = gl::GLContextEGL::Cast(gl());
  const auto& egl = gle->mEgl;

  EGLSync sync = nullptr;
  if (layers::AndroidHardwareBufferApi::Get()) {
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

  RenderedFrameId frameId = GetNextRenderFrameId();
  if (mEGLSurface != EGL_NO_SURFACE && aDirtyRects.Length() > 0) {
    gfx::IntRegion bufferInvalid;
    const auto bufferSize = GetBufferSize();
    for (const DeviceIntRect& rect : aDirtyRects) {
      const auto left = std::max(0, std::min(bufferSize.width, rect.origin.x));
      const auto top = std::max(0, std::min(bufferSize.height, rect.origin.y));

      const auto right = std::min(bufferSize.width,
                                  std::max(0, rect.origin.x + rect.size.width));
      const auto bottom = std::min(
          bufferSize.height, std::max(0, rect.origin.y + rect.size.height));

      const auto width = right - left;
      const auto height = bottom - top;

      bufferInvalid.OrWith(
          gfx::IntRect(left, (GetBufferSize().height - bottom), width, height));
    }
    gl()->SetDamage(bufferInvalid);
  }
  gl()->SwapBuffers();
  return frameId;
}

void RenderCompositorEGL::Pause() { DestroyEGLSurface(); }

bool RenderCompositorEGL::Resume() {
  if (kIsAndroid) {
    // Destroy EGLSurface if it exists.
    DestroyEGLSurface();

#ifdef MOZ_WIDGET_ANDROID
    // Query the new surface size as this may have changed. We cannot use
    // mWidget->GetClientSize() due to a race condition between
    // nsWindow::Resize() being called and the frame being rendered after the
    // surface is resized.
    EGLNativeWindowType window = mWidget->AsAndroid()->GetEGLNativeWindow();
    JNIEnv* const env = jni::GetEnvForThread();
    ANativeWindow* const nativeWindow =
        ANativeWindow_fromSurface(env, reinterpret_cast<jobject>(window));
    const int32_t width = ANativeWindow_getWidth(nativeWindow);
    const int32_t height = ANativeWindow_getHeight(nativeWindow);

    GLint maxTextureSize = 0;
    gl()->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, (GLint*)&maxTextureSize);

    // When window size is too big, hardware buffer allocation could fail.
    if (maxTextureSize < width || maxTextureSize < height) {
      gfxCriticalNote << "Too big ANativeWindow size(" << width << ", "
                      << height << ") MaxTextureSize " << maxTextureSize;
      return false;
    }

    mEGLSurface = CreateEGLSurface();
    if (mEGLSurface == EGL_NO_SURFACE) {
      RenderThread::Get()->HandleWebRenderError(WebRenderError::NEW_SURFACE);
      return false;
    }
    gl::GLContextEGL::Cast(gl())->SetEGLSurfaceOverride(mEGLSurface);

    mEGLSurfaceSize = LayoutDeviceIntSize(width, height);
    ANativeWindow_release(nativeWindow);
#endif  // MOZ_WIDGET_ANDROID
  } else if (kIsWayland) {
    // Destroy EGLSurface if it exists and create a new one. We will set the
    // swap interval after MakeCurrent() has been called.
    DestroyEGLSurface();
    mEGLSurface = CreateEGLSurface();
    if (mEGLSurface != EGL_NO_SURFACE) {
      // We have a new EGL surface, which on wayland needs to be configured for
      // non-blocking buffer swaps. We need MakeCurrent() to set our current EGL
      // context before we call eglSwapInterval, which is why we do it here
      // rather than where the surface was created.
      const auto& gle = gl::GLContextEGL::Cast(gl());
      const auto& egl = gle->mEgl;
      MakeCurrent();
      // Make eglSwapBuffers() non-blocking on wayland.
      egl->fSwapInterval(0);
    } else {
      RenderThread::Get()->HandleWebRenderError(WebRenderError::NEW_SURFACE);
      return false;
    }
  }
  return true;
}

bool RenderCompositorEGL::IsPaused() { return mEGLSurface == EGL_NO_SURFACE; }

gl::GLContext* RenderCompositorEGL::gl() const {
  return RenderThread::Get()->SharedGL();
}

bool RenderCompositorEGL::MakeCurrent() {
  gl::GLContextEGL::Cast(gl())->SetEGLSurfaceOverride(mEGLSurface);
  return gl()->MakeCurrent();
}

void RenderCompositorEGL::DestroyEGLSurface() {
  const auto& gle = gl::GLContextEGL::Cast(gl());
  const auto& egl = gle->mEgl;

  // Release EGLSurface of back buffer before calling ResizeBuffers().
  if (mEGLSurface) {
    gle->SetEGLSurfaceOverride(EGL_NO_SURFACE);
    egl->fDestroySurface(mEGLSurface);
    mEGLSurface = nullptr;
  }
}

ipc::FileDescriptor RenderCompositorEGL::GetAndResetReleaseFence() {
#ifdef MOZ_WIDGET_ANDROID
  MOZ_ASSERT(!layers::AndroidHardwareBufferApi::Get() ||
             mReleaseFenceFd.IsValid());
  return std::move(mReleaseFenceFd);
#else
  return ipc::FileDescriptor();
#endif
}

LayoutDeviceIntSize RenderCompositorEGL::GetBufferSize() {
#ifdef MOZ_WIDGET_ANDROID
  return mEGLSurfaceSize;
#else
  return mWidget->GetClientSize();
#endif
}

bool RenderCompositorEGL::UsePartialPresent() {
  return gfx::gfxVars::WebRenderMaxPartialPresentRects() > 0;
}

bool RenderCompositorEGL::RequestFullRender() { return false; }

uint32_t RenderCompositorEGL::GetMaxPartialPresentRects() {
  return gfx::gfxVars::WebRenderMaxPartialPresentRects();
}

bool RenderCompositorEGL::ShouldDrawPreviousPartialPresentRegions() {
  return true;
}

size_t RenderCompositorEGL::GetBufferAge() const {
  if (!StaticPrefs::
          gfx_webrender_allow_partial_present_buffer_age_AtStartup()) {
    return 0;
  }
  return gl()->GetBufferAge();
}

void RenderCompositorEGL::SetBufferDamageRegion(const wr::DeviceIntRect* aRects,
                                                size_t aNumRects) {
  const auto& gle = gl::GLContextEGL::Cast(gl());
  const auto& egl = gle->mEgl;
  if (gle->HasKhrPartialUpdate() &&
      StaticPrefs::gfx_webrender_allow_partial_present_buffer_age_AtStartup()) {
    std::vector<EGLint> rects;
    rects.reserve(4 * aNumRects);
    const auto bufferSize = GetBufferSize();
    for (size_t i = 0; i < aNumRects; i++) {
      const auto left =
          std::max(0, std::min(bufferSize.width, aRects[i].origin.x));
      const auto top =
          std::max(0, std::min(bufferSize.height, aRects[i].origin.y));

      const auto right =
          std::min(bufferSize.width,
                   std::max(0, aRects[i].origin.x + aRects[i].size.width));
      const auto bottom =
          std::min(bufferSize.height,
                   std::max(0, aRects[i].origin.y + aRects[i].size.height));

      const auto width = right - left;
      const auto height = bottom - top;

      rects.push_back(left);
      rects.push_back(bufferSize.height - bottom);
      rects.push_back(width);
      rects.push_back(height);
    }
    const auto ret =
        egl->fSetDamageRegion(mEGLSurface, rects.data(), rects.size() / 4);
    if (ret == LOCAL_EGL_FALSE) {
      const auto err = egl->mLib->fGetError();
      gfxCriticalError() << "Error in eglSetDamageRegion: " << gfx::hexa(err);
    }
  }
}

}  // namespace mozilla::wr
