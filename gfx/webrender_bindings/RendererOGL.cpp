/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RendererOGL.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/webrender/RenderBufferTextureHost.h"
#include "mozilla/webrender/RenderTextureHostOGL.h"
#include "mozilla/widget/CompositorWidget.h"

namespace mozilla {
namespace wr {

wr::WrExternalImage LockExternalImage(void* aObj, wr::WrExternalImageId aId, uint8_t aChannelIndex)
{
  RendererOGL* renderer = reinterpret_cast<RendererOGL*>(aObj);
  RenderTextureHost* texture = renderer->GetRenderTexture(aId);
  MOZ_ASSERT(texture);
  if (!texture) {
    gfxCriticalNote << "Failed to lock ExternalImage for extId:" << AsUint64(aId);
    return InvalidToWrExternalImage();
  }
  return texture->Lock(aChannelIndex, renderer->mGL);
}

void UnlockExternalImage(void* aObj, wr::WrExternalImageId aId, uint8_t aChannelIndex)
{
  RendererOGL* renderer = reinterpret_cast<RendererOGL*>(aObj);
  RenderTextureHost* texture = renderer->GetRenderTexture(aId);
  MOZ_ASSERT(texture);
  if (!texture) {
    return;
  }
  texture->Unlock();
}

RendererOGL::RendererOGL(RefPtr<RenderThread>&& aThread,
                         RefPtr<gl::GLContext>&& aGL,
                         RefPtr<widget::CompositorWidget>&& aWidget,
                         wr::WindowId aWindowId,
                         wr::Renderer* aRenderer,
                         layers::CompositorBridgeParentBase* aBridge)
  : mThread(aThread)
  , mGL(aGL)
  , mWidget(aWidget)
  , mRenderer(aRenderer)
  , mBridge(aBridge)
  , mWindowId(aWindowId)
  , mDebugFlags({ 0 })
{
  MOZ_ASSERT(mThread);
  MOZ_ASSERT(mGL);
  MOZ_ASSERT(mWidget);
  MOZ_ASSERT(mRenderer);
  MOZ_ASSERT(mBridge);
  MOZ_COUNT_CTOR(RendererOGL);

#ifdef XP_WIN
  if (aGL->IsANGLE()) {
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

RendererOGL::~RendererOGL()
{
  MOZ_COUNT_DTOR(RendererOGL);
  if (!mGL->MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current during destroying.";
    // Leak resources!
    return;
  }
  wr_renderer_delete(mRenderer);
}

wr::WrExternalImageHandler
RendererOGL::GetExternalImageHandler()
{
  return wr::WrExternalImageHandler {
    this,
    LockExternalImage,
    UnlockExternalImage,
  };
}

void
RendererOGL::Update()
{
  wr_renderer_update(mRenderer);
}

bool
RendererOGL::Render()
{
  uint32_t flags = gfx::gfxVars::WebRenderDebugFlags();

  if (mDebugFlags.mBits != flags) {
    mDebugFlags.mBits = flags;
    wr_renderer_set_debug_flags(mRenderer, mDebugFlags);
  }

  if (!mGL->MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current, can't draw.";
    NotifyWebRenderError(WebRenderError::MAKE_CURRENT);
    return false;
  }

  mozilla::widget::WidgetRenderingContext widgetContext;

#if defined(XP_MACOSX)
  widgetContext.mGL = mGL;
// TODO: we don't have a notion of compositor here.
//#elif defined(MOZ_WIDGET_ANDROID)
//  widgetContext.mCompositor = mCompositor;
#endif

  if (!mWidget->PreRender(&widgetContext)) {
    // XXX This could cause oom in webrender since pending_texture_updates is not handled.
    // It needs to be addressed.
    return false;
  }
  // XXX set clear color if MOZ_WIDGET_ANDROID is defined.

  auto size = mWidget->GetClientSize();

  if (mSyncObject) {
    // XXX: if the synchronization is failed, we should handle the device reset.
    mSyncObject->Synchronize();
  }

  if (!wr_renderer_render(mRenderer, size.width, size.height)) {
    NotifyWebRenderError(WebRenderError::RENDER);
  }

  mGL->SwapBuffers();
  mWidget->PostRender(&widgetContext);

#if defined(ENABLE_FRAME_LATENCY_LOG)
  if (mFrameStartTime) {
    uint32_t latencyMs = round((TimeStamp::Now() - mFrameStartTime).ToMilliseconds());
    printf_stderr("generate frame latencyMs latencyMs %d\n", latencyMs);
  }
  // Clear frame start time
  mFrameStartTime = TimeStamp();
#endif

  // TODO: Flush pending actions such as texture deletions/unlocks and
  //       textureHosts recycling.

  return true;
}

void
RendererOGL::Pause()
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
RendererOGL::Resume()
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

void
RendererOGL::SetFrameStartTime(const TimeStamp& aTime)
{
  if (mFrameStartTime) {
    // frame start time is already set. This could happen when multiple
    // generate frame requests are merged by webrender.
    return;
  }
  mFrameStartTime = aTime;
}

wr::WrRenderedEpochs*
RendererOGL::FlushRenderedEpochs()
{
  return wr_renderer_flush_rendered_epochs(mRenderer);
}

RenderTextureHost*
RendererOGL::GetRenderTexture(wr::WrExternalImageId aExternalImageId)
{
  return mThread->GetRenderTexture(aExternalImageId);
}

static void
DoNotifyWebRenderError(layers::CompositorBridgeParentBase* aBridge, WebRenderError aError)
{
  aBridge->NotifyWebRenderError(aError);
}

void
RendererOGL::NotifyWebRenderError(WebRenderError aError)
{
  layers::CompositorThreadHolder::Loop()->PostTask(NewRunnableFunction(
    "DoNotifyWebRenderErrorRunnable",
    &DoNotifyWebRenderError,
    mBridge,
    aError
  ));
}

} // namespace wr
} // namespace mozilla
