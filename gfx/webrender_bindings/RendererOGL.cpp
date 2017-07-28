/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RendererOGL.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/webrender/RenderBufferTextureHost.h"
#include "mozilla/webrender/RenderTextureHostOGL.h"
#include "mozilla/widget/CompositorWidget.h"

namespace mozilla {
namespace wr {

wr::WrExternalImage LockExternalImage(void* aObj, wr::WrExternalImageId aId, uint8_t aChannelIndex)
{
  RendererOGL* renderer = reinterpret_cast<RendererOGL*>(aObj);
  RenderTextureHost* texture = renderer->GetRenderTexture(aId);

  if (texture->AsBufferTextureHost()) {
    RenderBufferTextureHost* bufferTexture = texture->AsBufferTextureHost();
    MOZ_ASSERT(bufferTexture);
    bufferTexture->Lock();
    RenderBufferTextureHost::RenderBufferData data =
        bufferTexture->GetBufferDataForRender(aChannelIndex);

    return RawDataToWrExternalImage(data.mData, data.mBufferSize);
  } else {
    // texture handle case
    RenderTextureHostOGL* textureOGL = texture->AsTextureHostOGL();
    MOZ_ASSERT(textureOGL);

    textureOGL->SetGLContext(renderer->mGL);
    textureOGL->Lock();
    gfx::IntSize size = textureOGL->GetSize(aChannelIndex);

    return NativeTextureToWrExternalImage(textureOGL->GetGLHandle(aChannelIndex),
                                          0, 0,
                                          size.width, size.height);
  }
}

void UnlockExternalImage(void* aObj, wr::WrExternalImageId aId, uint8_t aChannelIndex)
{
  RendererOGL* renderer = reinterpret_cast<RendererOGL*>(aObj);
  RenderTextureHost* texture = renderer->GetRenderTexture(aId);
  MOZ_ASSERT(texture);
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
{
  MOZ_ASSERT(mThread);
  MOZ_ASSERT(mGL);
  MOZ_ASSERT(mWidget);
  MOZ_ASSERT(mRenderer);
  MOZ_ASSERT(mBridge);
  MOZ_COUNT_CTOR(RendererOGL);
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
  if (!mGL->MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current, can't draw.";
    // XXX This could cause oom in webrender since pending_texture_updates is not handled.
    // It needs to be addressed.
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
  wr_renderer_render(mRenderer, size.width, size.height);

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
RendererOGL::SetProfilerEnabled(bool aEnabled)
{
  wr_renderer_set_profiler_enabled(mRenderer, aEnabled);
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

} // namespace wr
} // namespace mozilla
