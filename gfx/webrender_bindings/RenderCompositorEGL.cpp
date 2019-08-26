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
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/widget/CompositorWidget.h"

#ifdef MOZ_WAYLAND
#  include "mozilla/widget/GtkCompositorWidget.h"
#  include <gdk/gdk.h>
#  include <gdk/gdkx.h>
#endif

#ifdef MOZ_WIDGET_ANDROID
#  include "GeneratedJNIWrappers.h"
#endif

namespace mozilla {
namespace wr {

/* static */
UniquePtr<RenderCompositor> RenderCompositorEGL::Create(
    RefPtr<widget::CompositorWidget> aWidget) {
#ifdef MOZ_WAYLAND
  if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
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
  java::GeckoSurfaceTexture::DetachAllFromGLContext((int64_t)gl());
#endif
  DestroyEGLSurface();
}

bool RenderCompositorEGL::BeginFrame(layers::NativeLayer* aNativeLayer) {
  MOZ_RELEASE_ASSERT(!aNativeLayer, "Unexpected native layer on this platform");
#ifdef MOZ_WAYLAND
  bool newSurface =
      mWidget->AsX11() && mWidget->AsX11()->WaylandRequestsUpdatingEGLSurface();
  if (newSurface) {
    // Destroy EGLSurface if it exists and create a new one. We will set the
    // swap interval after MakeCurrent() has been called.
    DestroyEGLSurface();
    mEGLSurface = CreateEGLSurface();
    if (mEGLSurface == EGL_NO_SURFACE) {
      RenderThread::Get()->HandleWebRenderError(WebRenderError::NEW_SURFACE);
    }
  }
  if (mEGLSurface == EGL_NO_SURFACE) {
    gfxCriticalNote
        << "We don't have EGLSurface to draw into. Called too early?";
    return false;
  }
#endif
  if (!MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current, can't draw.";
    return false;
  }

#ifdef MOZ_WAYLAND
  if (newSurface) {
    // We have a new EGL surface, which on wayland needs to be configured for
    // non-blocking buffer swaps. We need MakeCurrent() to set our current EGL
    // context before we call eglSwapInterval, which is why we do it here rather
    // than where the surface was created.
    const auto& gle = gl::GLContextEGL::Cast(gl());
    const auto& egl = gle->mEgl;
    // Make eglSwapBuffers() non-blocking on wayland.
    egl->fSwapInterval(egl->Display(), 0);
  }
#endif

#ifdef MOZ_WIDGET_ANDROID
  java::GeckoSurfaceTexture::DestroyUnused((int64_t)gl());
  gl()->MakeCurrent();  // DestroyUnused can change the current context!
#endif

  return true;
}

void RenderCompositorEGL::EndFrame() {
  if (mEGLSurface != EGL_NO_SURFACE) {
    gl()->SwapBuffers();
  }
}

void RenderCompositorEGL::Pause() {
#ifdef MOZ_WIDGET_ANDROID
  java::GeckoSurfaceTexture::DestroyUnused((int64_t)gl());
  java::GeckoSurfaceTexture::DetachAllFromGLContext((int64_t)gl());
  DestroyEGLSurface();
#endif
}

bool RenderCompositorEGL::Resume() {
#ifdef MOZ_WIDGET_ANDROID
  // Destroy EGLSurface if it exists.
  DestroyEGLSurface();
  mEGLSurface = CreateEGLSurface();
  gl::GLContextEGL::Cast(gl())->SetEGLSurfaceOverride(mEGLSurface);
#endif
  return true;
}

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
    egl->fDestroySurface(egl->Display(), mEGLSurface);
    mEGLSurface = nullptr;
  }
}

LayoutDeviceIntSize RenderCompositorEGL::GetBufferSize() {
  return mWidget->GetClientSize();
}

}  // namespace wr
}  // namespace mozilla
