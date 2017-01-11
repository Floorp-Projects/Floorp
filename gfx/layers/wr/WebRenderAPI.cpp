/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderAPI.h"
#include "mozilla/layers/RendererOGL.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/widget/CompositorWidget.h"
#include "mozilla/widget/CompositorWidget.h"
#include "mozilla/layers/SynchronousTask.h"

namespace mozilla {
namespace layers {

class NewRenderer : public RendererEvent
{
public:
  NewRenderer(WrAPI** aApi, CompositorBridgeParentBase* aBridge,
              RefPtr<widget::CompositorWidget>&& aWidget,
              SynchronousTask* aTask,
              bool aEnableProfiler)
  : mWrApi(aApi)
  , mBridge(aBridge)
  , mCompositorWidget(Move(aWidget))
  , mTask(aTask)
  , mEnableProfiler(aEnableProfiler)
  {
    MOZ_COUNT_CTOR(NewRenderer);
  }

  ~NewRenderer()
  {
    MOZ_COUNT_DTOR(NewRenderer);
  }

  virtual void Run(RenderThread& aRenderThread, gfx::WindowId aWindowId) override
  {
    AutoCompleteTask complete(mTask);

    RefPtr<gl::GLContext> gl = gl::GLContextProvider::CreateForCompositorWidget(mCompositorWidget, true);
    if (!gl || !gl->MakeCurrent()) {
      return;
    }

    wr_gl_init(&*gl);

    WrRenderer* wrRenderer = nullptr;
    wr_window_new(aWindowId.mHandle, this->mEnableProfiler, mWrApi, &wrRenderer);
    MOZ_ASSERT(wrRenderer);

    RefPtr<RenderThread> thread = &aRenderThread;
    auto renderer = MakeUnique<RendererOGL>(Move(thread),
                                            Move(gl),
                                            Move(mCompositorWidget),
                                            aWindowId,
                                            wrRenderer,
                                            mBridge);

    aRenderThread.AddRenderer(aWindowId, Move(renderer));
  }

  WrAPI** mWrApi;
  CompositorBridgeParentBase* mBridge;
  RefPtr<widget::CompositorWidget> mCompositorWidget;
  SynchronousTask* mTask;
  bool mEnableProfiler;
};

class RemoveRenderer : public RendererEvent
{
public:
  explicit RemoveRenderer(SynchronousTask* aTask)
  : mTask(aTask)
  {
    MOZ_COUNT_CTOR(RemoveRenderer);
  }

  ~RemoveRenderer()
  {
    MOZ_COUNT_DTOR(RemoveRenderer);
  }

  virtual void Run(RenderThread& aRenderThread, gfx::WindowId aWindowId) override
  {
    aRenderThread.RemoveRenderer(aWindowId);
    AutoCompleteTask complete(mTask);
  }

  SynchronousTask* mTask;
};


//static
already_AddRefed<WebRenderAPI>
WebRenderAPI::Create(bool aEnableProfiler,
                     CompositorBridgeParentBase* aBridge,
                     RefPtr<widget::CompositorWidget>&& aWidget)
{
  MOZ_ASSERT(aBridge);
  MOZ_ASSERT(aWidget);

  static uint64_t sNextId = 1;
  gfx::WindowId id(sNextId++);

  WrAPI* wrApi = nullptr;

  // Dispatch a synchronous task because the WrApi object needs to be created
  // on the render thread. If need be we could delay waiting on this task until
  // the next time we need to access the WrApi object.
  SynchronousTask task("Create Renderer");
  auto event = MakeUnique<NewRenderer>(&wrApi, aBridge, Move(aWidget), &task, aEnableProfiler);
  RenderThread::Get()->RunEvent(id, Move(event));

  task.Wait();

  if (!wrApi) {
    return nullptr;
  }

  return RefPtr<WebRenderAPI>(new WebRenderAPI(wrApi, id)).forget();
}

WebRenderAPI::~WebRenderAPI()
{

#ifdef MOZ_ENABLE_WEBRENDER
  // Need to wrap this in an ifdef otherwise VC++ emits a warning (treated as error)
  // in the non-webrender targets.
  // We should be able to remove this #ifdef if/when we remove the
  // MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE annotations in webrender.h
  wr_api_delete(mWrApi);
#endif

  SynchronousTask task("Destroy WebRenderAPI");
  auto event = MakeUnique<RemoveRenderer>(&task);
  // TODO use the WebRender API instead of scheduling on this message loop directly.
  // this needs PR #688
  RenderThread::Get()->RunEvent(mId, Move(event));
  task.Wait();
}

} // namespace
} // namespace
