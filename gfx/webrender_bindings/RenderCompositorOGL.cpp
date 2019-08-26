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

#ifdef XP_MACOSX
#  include "GLContextCGL.h"
#  include "mozilla/layers/NativeLayerCA.h"
#endif

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

#ifdef XP_MACOSX
class SurfaceRegistryWrapperAroundGLContextCGL
    : public layers::IOSurfaceRegistry {
 public:
  explicit SurfaceRegistryWrapperAroundGLContextCGL(gl::GLContextCGL* aContext)
      : mContext(aContext) {}
  void RegisterSurface(CFTypeRefPtr<IOSurfaceRef> aSurface) override {
    mContext->RegisterIOSurface(aSurface.get());
  }
  void UnregisterSurface(CFTypeRefPtr<IOSurfaceRef> aSurface) override {
    mContext->UnregisterIOSurface(aSurface.get());
  }
  RefPtr<gl::GLContextCGL> mContext;
};
#endif

bool RenderCompositorOGL::BeginFrame(layers::NativeLayer* aNativeLayer) {
  if (!mGL->MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current, can't draw.";
    return false;
  }

  if (aNativeLayer) {
#ifdef XP_MACOSX
    layers::NativeLayerCA* nativeLayer = aNativeLayer->AsNativeLayerCA();
    MOZ_RELEASE_ASSERT(nativeLayer, "Unexpected native layer type");
    nativeLayer->SetSurfaceIsFlipped(true);
    auto glContextCGL = gl::GLContextCGL::Cast(mGL);
    MOZ_RELEASE_ASSERT(glContextCGL, "Unexpected GLContext type");
    RefPtr<layers::IOSurfaceRegistry> currentRegistry =
        nativeLayer->GetSurfaceRegistry();
    if (!currentRegistry) {
      nativeLayer->SetSurfaceRegistry(
          MakeAndAddRef<SurfaceRegistryWrapperAroundGLContextCGL>(
              glContextCGL));
    } else {
      MOZ_RELEASE_ASSERT(static_cast<SurfaceRegistryWrapperAroundGLContextCGL*>(
                             currentRegistry.get())
                             ->mContext == glContextCGL);
    }
    CFTypeRefPtr<IOSurfaceRef> surf = nativeLayer->NextSurface();
    if (!surf) {
      return false;
    }
    glContextCGL->UseRegisteredIOSurfaceForDefaultFramebuffer(surf.get());
    mCurrentNativeLayer = aNativeLayer;
#else
    MOZ_CRASH("Unexpected native layer on this platform");
#endif
  }

  mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mGL->GetDefaultFramebuffer());
  return true;
}

void RenderCompositorOGL::EndFrame() {
  InsertFrameDoneSync();
  mGL->SwapBuffers();

  if (mCurrentNativeLayer) {
#ifdef XP_MACOSX
    layers::NativeLayerCA* nativeLayer = mCurrentNativeLayer->AsNativeLayerCA();
    MOZ_RELEASE_ASSERT(nativeLayer, "Unexpected native layer type");
    nativeLayer->NotifySurfaceReady();
    mCurrentNativeLayer = nullptr;
#else
    MOZ_CRASH("Unexpected native layer on this platform");
#endif
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
