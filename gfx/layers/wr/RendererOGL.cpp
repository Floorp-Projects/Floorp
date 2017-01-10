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
#include "mozilla/widget/CompositorWidget.h"

namespace mozilla {
namespace layers {

// static
UniquePtr<RendererOGL>
RendererOGL::Create(already_AddRefed<RenderThread> aThread,
                    already_AddRefed<widget::CompositorWidget> aWidget,
                    WrRenderer* aWrRenderer,
                    gfx::WindowId aWindowId,
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

  wr_gl_init(&*gl);

  return UniquePtr<RendererOGL>(new RendererOGL(thread.forget(),
                                                gl.forget(),
                                                widget.forget(),
                                                aWindowId,
                                                aWrRenderer,
                                                aBridge));
}

RendererOGL::RendererOGL(already_AddRefed<RenderThread> aThread,
                         already_AddRefed<gl::GLContext> aGL,
                         already_AddRefed<widget::CompositorWidget> aWidget,
                         gfx::WindowId aWindowId,
                         WrRenderer* aWrRenderer,
                         WebRenderBridgeParent* aBridge)
: mThread(aThread)
, mGL(aGL)
, mWidget(aWidget)
, mWrRenderer(aWrRenderer)
, mBridge(aBridge)
, mWindowId(aWindowId)
{
  MOZ_ASSERT(mThread);
  MOZ_ASSERT(mGL);
  MOZ_ASSERT(mWidget);
  MOZ_ASSERT(mWrRenderer);
  MOZ_ASSERT(mBridge);
  MOZ_COUNT_CTOR(RendererOGL);
}

RendererOGL::~RendererOGL()
{
  MOZ_COUNT_DTOR(RendererOGL);
#ifdef MOZ_ENABLE_WEBRENDER
  // Need to wrap this in an ifdef otherwise VC++ emits a warning (treated as error)
  // in the non-webrender targets.
  // We should be able to remove this #ifdef if/when we remove the
  // MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE annotations in webrender.h
  wr_renderer_delete(mWrRenderer);
#endif
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
