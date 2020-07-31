/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderCompositorOGL.h"

#include "GLContext.h"
#include "GLContextEGL.h"
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

  mIsEGL = aGL->GetContextType() == mozilla::gl::GLContextType::EGL;
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

  if (mIsEGL) {
    // sets 0 if buffer_age is not supported
    mBufferAge = gl::GLContextEGL::Cast(gl())->GetBufferAge();
  }

  return true;
}

RenderedFrameId RenderCompositorOGL::EndFrame(
    const nsTArray<DeviceIntRect>& aDirtyRects) {
  RenderedFrameId frameId = GetNextRenderFrameId();
  if (UsePartialPresent() && aDirtyRects.Length() > 0) {
    gfx::IntRegion bufferInvalid;
    for (const DeviceIntRect& rect : aDirtyRects) {
      const auto width = std::min(rect.size.width, GetBufferSize().width);
      const auto height = std::min(rect.size.height, GetBufferSize().height);
      const auto left =
          std::max(0, std::min(rect.origin.x, GetBufferSize().width));
      const auto bottom =
          std::max(0, std::min(rect.origin.y + height, GetBufferSize().height));
      bufferInvalid.OrWith(
          gfx::IntRect(left, (GetBufferSize().height - bottom), width, height));
    }
    gl()->SetDamage(bufferInvalid);
  }
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

uint32_t RenderCompositorOGL::GetMaxPartialPresentRects() {
  return mIsEGL ? gfx::gfxVars::WebRenderMaxPartialPresentRects() : 0;
}

bool RenderCompositorOGL::RequestFullRender() {
  return mIsEGL && (mBufferAge != 2);
}

bool RenderCompositorOGL::UsePartialPresent() {
  return mIsEGL && gfx::gfxVars::WebRenderMaxPartialPresentRects() > 0;
}

bool RenderCompositorOGL::ShouldDrawPreviousPartialPresentRegions() {
  return mIsEGL && gl::GLContextEGL::Cast(gl())->HasBufferAge();
}

}  // namespace wr
}  // namespace mozilla
