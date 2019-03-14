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
#include "mozilla/widget/CompositorWidget.h"
#include "mozilla/widget/GtkCompositorWidget.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

namespace mozilla {
namespace wr {

/* static */
UniquePtr<RenderCompositor> RenderCompositorEGL::Create(
    RefPtr<widget::CompositorWidget> aWidget) {
  if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
    return nullptr;
  }

  RefPtr<gl::GLContext> gl;
  gl = CreateGLContext();
  if (!gl) {
    return nullptr;
  }
  return MakeUnique<RenderCompositorEGL>(gl, aWidget);
}

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
    RefPtr<gl::GLContext> aGL, RefPtr<widget::CompositorWidget> aWidget)
    : RenderCompositor(std::move(aWidget)),
      mGL(aGL),
      mEGLSurface(EGL_NO_SURFACE) {
  MOZ_ASSERT(mGL);
}

RenderCompositorEGL::~RenderCompositorEGL() { DestroyEGLSurface(); }

bool RenderCompositorEGL::BeginFrame() {
  if (mWidget->AsX11() &&
      mWidget->AsX11()->WaylandRequestsUpdatingEGLSurface()) {
    // Destroy EGLSurface if it exists.
    DestroyEGLSurface();
    mEGLSurface = CreateEGLSurface();
    gl::GLContextEGL::Cast(gl())->SetEGLSurfaceOverride(mEGLSurface);
  }

  if (!mGL->MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current, can't draw.";
    return false;
  }

  return true;
}

void RenderCompositorEGL::EndFrame() {
  if (mEGLSurface != EGL_NO_SURFACE) {
    mGL->SwapBuffers();
  }
}

void RenderCompositorEGL::WaitForGPU() {}

void RenderCompositorEGL::Pause() {}

bool RenderCompositorEGL::Resume() { return true; }

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
