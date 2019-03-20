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

namespace mozilla {
namespace wr {

/* static */
UniquePtr<RenderCompositor> RenderCompositorEGL::Create(
    RefPtr<widget::CompositorWidget> aWidget) {
#ifdef MOZ_WIDGET_ANDROID
  if (!RenderThread::Get()->SharedGL()) {
    gfxCriticalNote << "Failed to get shared GL context";
    return nullptr;
  }
  return MakeUnique<RenderCompositorEGL>(aWidget);
#elif defined(MOZ_WAYLAND)
  // Disabled SharedGL() usage on wayland. Its usage caused flicker when
  // multiple windows were opened. See Bug 1535893.
  if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
    return nullptr;
  }
  RefPtr<gl::GLContext> gl;
  gl = CreateGLContext();
  if (!gl) {
    return nullptr;
  }
  return MakeUnique<RenderCompositorEGL>(gl, aWidget);
#endif
}

#ifdef MOZ_WAYLAND
/* static */ already_AddRefed<gl::GLContext>
RenderCompositorEGL::CreateGLContext() {
  // Create GLContext with dummy EGLSurface.
  RefPtr<gl::GLContext> gl =
      gl::GLContextProviderEGL::CreateForCompositorWidget(
          nullptr, /* aWebRender */ true, /* aForceAccelerated */ true);
  if (!gl || !gl->MakeCurrent()) {
    gfxCriticalNote << "Failed GL context creation for WebRender: "
                    << gfx::hexa(gl.get());
    return nullptr;
  }

  return gl.forget();
}
#endif

EGLSurface RenderCompositorEGL::CreateEGLSurface() {
  EGLSurface surface = EGL_NO_SURFACE;
  surface = gl::GLContextEGL::CreateEGLSurfaceForCompositorWidget(
      mWidget, gl::GLContextEGL::Cast(gl())->mConfig);
  if (surface == EGL_NO_SURFACE) {
    gfxCriticalNote << "Failed to create EGLSurface";
  }
  return surface;
}

#ifdef MOZ_WIDGET_ANDROID
RenderCompositorEGL::RenderCompositorEGL(
    RefPtr<widget::CompositorWidget> aWidget)
    : RenderCompositor(std::move(aWidget)), mEGLSurface(EGL_NO_SURFACE) {}
#elif defined(MOZ_WAYLAND)
RenderCompositorEGL::RenderCompositorEGL(
    RefPtr<gl::GLContext> aGL, RefPtr<widget::CompositorWidget> aWidget)
    : RenderCompositor(std::move(aWidget)),
      mGL(aGL),
      mEGLSurface(EGL_NO_SURFACE) {
  MOZ_ASSERT(mGL);
}
#endif

RenderCompositorEGL::~RenderCompositorEGL() { DestroyEGLSurface(); }

bool RenderCompositorEGL::BeginFrame() {
#ifdef MOZ_WAYLAND
  if (mWidget->AsX11() &&
      mWidget->AsX11()->WaylandRequestsUpdatingEGLSurface()) {
    // Destroy EGLSurface if it exists.
    DestroyEGLSurface();
    mEGLSurface = CreateEGLSurface();
    gl::GLContextEGL::Cast(gl())->SetEGLSurfaceOverride(mEGLSurface);
  }
#endif
  if (!gl()->MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current, can't draw.";
    return false;
  }

  return true;
}

void RenderCompositorEGL::EndFrame() {
  if (mEGLSurface != EGL_NO_SURFACE) {
    gl()->SwapBuffers();
  }
}

void RenderCompositorEGL::WaitForGPU() {}

void RenderCompositorEGL::Pause() {
#ifdef MOZ_WIDGET_ANDROID
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
#ifdef MOZ_WIDGET_ANDROID
  return RenderThread::Get()->SharedGL();
#elif defined(MOZ_WAYLAND)
  MOZ_ASSERT(mGL);
  return mGL;
#endif
}

bool RenderCompositorEGL::MakeCurrent() {
  gl::GLContextEGL::Cast(gl())->SetEGLSurfaceOverride(mEGLSurface);
  return gl()->MakeCurrent();
}

void RenderCompositorEGL::DestroyEGLSurface() {
  auto* egl = gl::GLLibraryEGL::Get();

  // Release EGLSurface of back buffer before calling ResizeBuffers().
  if (mEGLSurface) {
    gl::GLContextEGL::Cast(gl())->SetEGLSurfaceOverride(EGL_NO_SURFACE);
    egl->fDestroySurface(egl->Display(), mEGLSurface);
    mEGLSurface = nullptr;
  }
}

LayoutDeviceIntSize RenderCompositorEGL::GetBufferSize() {
  return mWidget->GetClientSize();
}

}  // namespace wr
}  // namespace mozilla
