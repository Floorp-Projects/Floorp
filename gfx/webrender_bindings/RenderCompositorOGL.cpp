/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderCompositorOGL.h"

#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/SyncObject.h"
#include "mozilla/widget/CompositorWidget.h"

#ifdef XP_WIN
#include "GLContextEGL.h"
#endif

namespace mozilla {
namespace wr {

/* static */ UniquePtr<RenderCompositor>
RenderCompositorOGL::Create(RefPtr<widget::CompositorWidget>&& aWidget)
{
  RefPtr<gl::GLContext> gl;
  if (gfx::gfxVars::UseWebRenderANGLE()) {
    gl = gl::GLContextProviderEGL::CreateForCompositorWidget(aWidget, true);
    if (!gl || !gl->IsANGLE()) {
      gfxCriticalNote << "Failed ANGLE GL context creation for WebRender: " << gfx::hexa(gl.get());
      return nullptr;
    }
  }
  if (!gl) {
    gl = gl::GLContextProvider::CreateForCompositorWidget(aWidget, true);
  }
  if (!gl || !gl->MakeCurrent()) {
    gfxCriticalNote << "Failed GL context creation for WebRender: " << gfx::hexa(gl.get());
    return nullptr;
  }
  return MakeUnique<RenderCompositorOGL>(Move(gl), Move(aWidget));
}

RenderCompositorOGL::RenderCompositorOGL(RefPtr<gl::GLContext>&& aGL,
                                             RefPtr<widget::CompositorWidget>&& aWidget)
  : RenderCompositor(Move(aWidget))
  , mGL(aGL)
{
  MOZ_ASSERT(mGL);

#ifdef XP_WIN
  if (mGL->IsANGLE()) {
    gl::GLLibraryEGL* egl = &gl::sEGLLibrary;

    // Fetch the D3D11 device.
    EGLDeviceEXT eglDevice = nullptr;
    egl->fQueryDisplayAttribEXT(egl->Display(), LOCAL_EGL_DEVICE_EXT, (EGLAttrib*)&eglDevice);
    MOZ_ASSERT(eglDevice);
    ID3D11Device* device = nullptr;
    egl->fQueryDeviceAttribEXT(eglDevice, LOCAL_EGL_D3D11_DEVICE_ANGLE, (EGLAttrib*)&device);
    MOZ_ASSERT(device);

    mSyncObject = layers::SyncObjectHost::CreateSyncObjectHost(device);
    if (mSyncObject) {
      if (!mSyncObject->Init()) {
        // Some errors occur. Clear the mSyncObject here.
        // Then, there will be no texture synchronization.
        mSyncObject = nullptr;
      }
    }
  }
#endif
}

RenderCompositorOGL::~RenderCompositorOGL()
{
}

bool
RenderCompositorOGL::Destroy()
{
  return true;
}

bool
RenderCompositorOGL::BeginFrame()
{
  if (!mGL->MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current, can't draw.";
    return false;
  }

  if (mSyncObject) {
    // XXX: if the synchronization is failed, we should handle the device reset.
    mSyncObject->Synchronize();
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

bool
RenderCompositorOGL::UseANGLE() const
{
  return mGL->IsANGLE();
}

LayoutDeviceIntSize
RenderCompositorOGL::GetClientSize()
{
  return mWidget->GetClientSize();
}


} // namespace wr
} // namespace mozilla
