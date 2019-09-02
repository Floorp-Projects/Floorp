/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderCompositorOGL.h"

#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/widget/CompositorWidget.h"

namespace mozilla {
namespace wr {

/* static */
UniquePtr<RenderCompositor> RenderCompositorOGL::Create(
    RefPtr<widget::CompositorWidget>&& aWidget) {
  RefPtr<gl::GLContext> gl;
  gl = gl::GLContextProvider::CreateForCompositorWidget(
      aWidget, /* aWebRender */ true, /* aForceAccelerated */ true);
  if (!gl || !gl->MakeCurrent()) {
    gfxCriticalNote << "Failed GL context creation for WebRender: "
                    << gfx::hexa(gl.get());
    return nullptr;
  }
  return MakeUnique<RenderCompositorOGL>(std::move(gl), std::move(aWidget));
}

RenderCompositorOGL::RenderCompositorOGL(
    RefPtr<gl::GLContext>&& aGL, RefPtr<widget::CompositorWidget>&& aWidget)
    : RenderCompositor(std::move(aWidget)),
      mGL(aGL),
      mPreviousFrameDoneSync(nullptr),
      mThisFrameDoneSync(nullptr) {
  MOZ_ASSERT(mGL);
}

RenderCompositorOGL::~RenderCompositorOGL() {
  if (!mGL->MakeCurrent()) {
    gfxCriticalNote
        << "Failed to make render context current during destroying.";
    // Leak resources!
    mPreviousFrameDoneSync = nullptr;
    mThisFrameDoneSync = nullptr;
    return;
  }

  if (mPreviousFrameDoneSync) {
    mGL->fDeleteSync(mPreviousFrameDoneSync);
  }
  if (mThisFrameDoneSync) {
    mGL->fDeleteSync(mThisFrameDoneSync);
  }
}

bool RenderCompositorOGL::BeginFrame(layers::NativeLayer* aNativeLayer) {
  if (!mGL->MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current, can't draw.";
    return false;
  }

  if (aNativeLayer) {
    aNativeLayer->SetSurfaceIsFlipped(true);
    aNativeLayer->SetGLContext(mGL);
    Maybe<GLuint> fbo = aNativeLayer->NextSurfaceAsFramebuffer(true);
    if (!fbo) {
      return false;
    }
    mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, *fbo);
    mCurrentNativeLayer = aNativeLayer;
  } else {
    mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mGL->GetDefaultFramebuffer());
  }

  return true;
}

void RenderCompositorOGL::EndFrame() {
  InsertFrameDoneSync();
  mGL->SwapBuffers();

  if (mCurrentNativeLayer) {
    mCurrentNativeLayer->NotifySurfaceReady();
    mCurrentNativeLayer = nullptr;
  }
}

void RenderCompositorOGL::InsertFrameDoneSync() {
#ifdef XP_MACOSX
  // Only do this on macOS.
  // On other platforms, SwapBuffers automatically applies back-pressure.
  if (StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
    if (mThisFrameDoneSync) {
      mGL->fDeleteSync(mThisFrameDoneSync);
    }
    mThisFrameDoneSync =
        mGL->fFenceSync(LOCAL_GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  }
#endif
}

bool RenderCompositorOGL::WaitForGPU() {
  if (mPreviousFrameDoneSync) {
    AUTO_PROFILER_LABEL("Waiting for GPU to finish previous frame", GRAPHICS);
    mGL->fClientWaitSync(mPreviousFrameDoneSync,
                         LOCAL_GL_SYNC_FLUSH_COMMANDS_BIT,
                         LOCAL_GL_TIMEOUT_IGNORED);
    mGL->fDeleteSync(mPreviousFrameDoneSync);
  }
  mPreviousFrameDoneSync = mThisFrameDoneSync;
  mThisFrameDoneSync = nullptr;

  return true;
}

void RenderCompositorOGL::Pause() {}

bool RenderCompositorOGL::Resume() { return true; }

LayoutDeviceIntSize RenderCompositorOGL::GetBufferSize() {
  return mWidget->GetClientSize();
}

}  // namespace wr
}  // namespace mozilla
