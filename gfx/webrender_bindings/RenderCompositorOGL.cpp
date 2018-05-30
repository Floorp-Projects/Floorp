/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderCompositorOGL.h"

#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/widget/CompositorWidget.h"

namespace mozilla {
namespace wr {

/* static */ UniquePtr<RenderCompositor>
RenderCompositorOGL::Create(RefPtr<widget::CompositorWidget>&& aWidget)
{
  RefPtr<gl::GLContext> gl;
  gl = gl::GLContextProvider::CreateForCompositorWidget(aWidget, true);
  if (!gl || !gl->MakeCurrent()) {
    gfxCriticalNote << "Failed GL context creation for WebRender: " << gfx::hexa(gl.get());
    return nullptr;
  }
  return MakeUnique<RenderCompositorOGL>(std::move(gl), std::move(aWidget));
}

RenderCompositorOGL::RenderCompositorOGL(RefPtr<gl::GLContext>&& aGL,
                                         RefPtr<widget::CompositorWidget>&& aWidget)
  : RenderCompositor(std::move(aWidget))
  , mGL(aGL)
{
  MOZ_ASSERT(mGL);
}

RenderCompositorOGL::~RenderCompositorOGL()
{
}

bool
RenderCompositorOGL::BeginFrame()
{
  if (!mGL->MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current, can't draw.";
    return false;
  }
  return true;
}

void
RenderCompositorOGL::EndFrame()
{
  mGL->SwapBuffers();
}

void
RenderCompositorOGL::Pause()
{
#ifdef MOZ_WIDGET_ANDROID
  if (!mGL || mGL->IsDestroyed()) {
    return;
  }
  // ReleaseSurface internally calls MakeCurrent.
  mGL->ReleaseSurface();
#endif
}

bool
RenderCompositorOGL::Resume()
{
#ifdef MOZ_WIDGET_ANDROID
  if (!mGL || mGL->IsDestroyed()) {
    return false;
  }
  // RenewSurface internally calls MakeCurrent.
  return mGL->RenewSurface(mWidget);
#else
  return true;
#endif
}

LayoutDeviceIntSize
RenderCompositorOGL::GetBufferSize()
{
  return mWidget->GetClientSize();
}


} // namespace wr
} // namespace mozilla
