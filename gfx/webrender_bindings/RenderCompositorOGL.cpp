/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderCompositorOGL.h"

#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/widget/CompositorWidget.h"

namespace mozilla {
namespace wr {

/* static */
UniquePtr<RenderCompositor> RenderCompositorOGL::Create(
    RefPtr<widget::CompositorWidget>&& aWidget) {
  RefPtr<gl::GLContext> gl = RenderThread::Get()->SharedGL();
  if (!gl) {
    gl = gl::GLContextProvider::CreateForCompositorWidget(
        aWidget, /* aWebRender */ true, /* aForceAccelerated */ true);
    RenderThread::MaybeEnableGLDebugMessage(gl);
  }
  if (!gl || !gl->MakeCurrent()) {
    gfxCriticalNote << "Failed GL context creation for WebRender: "
                    << gfx::hexa(gl.get());
    return nullptr;
  }
  return MakeUnique<RenderCompositorOGL>(std::move(gl), std::move(aWidget));
}

RenderCompositorOGL::RenderCompositorOGL(
    RefPtr<gl::GLContext>&& aGL, RefPtr<widget::CompositorWidget>&& aWidget)
    : RenderCompositor(std::move(aWidget)), mGL(aGL) {
  MOZ_ASSERT(mGL);
}

RenderCompositorOGL::~RenderCompositorOGL() {
  if (!mGL->MakeCurrent()) {
    gfxCriticalNote
        << "Failed to make render context current during destroying.";
    // Leak resources!
    return;
  }
}

bool RenderCompositorOGL::BeginFrame() {
  if (!mGL->MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current, can't draw.";
    return false;
  }

  mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mGL->GetDefaultFramebuffer());
  return true;
}

RenderedFrameId RenderCompositorOGL::EndFrame(
    const nsTArray<DeviceIntRect>& aDirtyRects) {
  RenderedFrameId frameId = GetNextRenderFrameId();
  mGL->SwapBuffers();
  return frameId;
}

void RenderCompositorOGL::Pause() {}

bool RenderCompositorOGL::Resume() { return true; }

LayoutDeviceIntSize RenderCompositorOGL::GetBufferSize() {
  return mWidget->GetClientSize();
}

CompositorCapabilities RenderCompositorOGL::GetCompositorCapabilities() {
  CompositorCapabilities caps;

  caps.virtual_surface_size = 0;

  return caps;
}

}  // namespace wr
}  // namespace mozilla
