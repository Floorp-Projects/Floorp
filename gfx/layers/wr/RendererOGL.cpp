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
#include "mozilla/widget/CompositorWidget.h"
#include "base/task.h"

namespace mozilla {
namespace layers {

RendererOGL::RendererOGL(RefPtr<RenderThread>&& aThread,
                         RefPtr<gl::GLContext>&& aGL,
                         RefPtr<widget::CompositorWidget>&& aWidget,
                         gfx::WindowId aWindowId,
                         WrRenderer* aWrRenderer,
                         CompositorBridgeParentBase* aBridge)
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

static void
NotifyDidRender(CompositorBridgeParentBase* aBridge, uint64_t aTransactionId, TimeStamp aStart, TimeStamp aEnd)
{

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

  CompositorThreadHolder::Loop()->PostTask(NewRunnableFunction(
    &NotifyDidRender,
    mBridge, aTransactionId, start, end
  ));

  return true;
}

}
}
