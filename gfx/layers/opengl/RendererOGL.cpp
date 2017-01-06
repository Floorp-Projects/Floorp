/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RendererOGL.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/WebRenderBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"

namespace mozilla {
namespace layers {

// static
RendererOGL*
RendererOGL::Create(already_AddRefed<RenderThread> aThread,
                    already_AddRefed<widget::CompositorWidget> aWidget,
                    WrRenderer* aWrRenderer,
                    uint64_t aPipelineId,
                    WebRenderBridgeParent* aBridge)
{
  RefPtr<widget::CompositorWidget> widget = aWidget;
  RefPtr<RenderThread> thread = aThread;

  MOZ_ASSERT(widget);
  MOZ_ASSERT(thread);
  MOZ_ASSERT(aWrRenderer);
  MOZ_ASSERT(RenderThread::IsInRenderThread());

  if (!widget || !thread) {
    return nullptr;
  }

  RefPtr<gl::GLContext> gl = gl::GLContextProvider::CreateForCompositorWidget(widget, true);
  if (!gl) {
    return nullptr;
  }

  if (!gl->MakeCurrent()) {
    return nullptr;
  }

  return new RendererOGL(thread.forget(),
                         gl.forget(),
                         widget.forget(),
                         aPipelineId,
                         aWrRenderer,
                         aBridge);
}

RendererOGL::RendererOGL(already_AddRefed<RenderThread> aThread,
                         already_AddRefed<gl::GLContext> aGL,
                         already_AddRefed<widget::CompositorWidget> aWidget,
                         uint64_t aPipelineId,
                         WrRenderer* aWrRenderer,
                         WebRenderBridgeParent* aBridge)
: mThread(aThread)
, mGL(aGL)
, mWidget(aWidget)
, mWrRenderer(aWrRenderer)
, mBridge(aBridge)
, mPipelineId(aPipelineId)
{
}

RendererOGL::~RendererOGL()
{

}

void
RendererOGL::Update()
{
  wr_renderer_update(mWrRenderer);
}

bool
RendererOGL::Render(uint64_t aTransactionId)
{
  TimeStamp start = TimeStamp::Now();

  if (!mGL->MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current, can't draw.";
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
    return false;
  }
  // XXX set clear color if MOZ_WIDGET_ANDROID is defined.
  // XXX pass the actual render bounds instead of an empty rect.
  mWidget->DrawWindowUnderlay(&widgetContext, LayoutDeviceIntRect());

  auto size = mWidget->GetClientSize();
  wr_renderer_render(mWrRenderer, size.width, size.height);

  mGL->SwapBuffers();
  mWidget->DrawWindowOverlay(&widgetContext, LayoutDeviceIntRect());
  mWidget->PostRender(&widgetContext);

  // TODO: Flush pending actions such as texture deletions/unlocks.

  TimeStamp end = TimeStamp::Now();

  CompositorThreadHolder::Loop()->PostTask(NewRunnableMethod<uint64_t, TimeStamp, TimeStamp>(
    mBridge,
    &WebRenderBridgeParent::DidComposite,
    aTransactionId, start, end
  ));

  return true;
}

}
}
