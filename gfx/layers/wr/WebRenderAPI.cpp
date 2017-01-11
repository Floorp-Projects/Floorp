/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderAPI.h"
#include "mozilla/layers/RendererOGL.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/widget/CompositorWidget.h"

namespace mozilla {
namespace layers {

class NewRenderer : public RendererEvent
{
public:
  NewRenderer(WrRenderer* aWrRenderer, CompositorBridgeParentBase* aBridge,
              RefPtr<widget::CompositorWidget>&& aWidget)
  : mWrRenderer(aWrRenderer)
  , mBridge(aBridge)
  , mCompositorWidget(Move(aWidget))
  {
    MOZ_COUNT_CTOR(NewRenderer);
  }

  ~NewRenderer()
  {
    MOZ_COUNT_DTOR(NewRenderer);
  }

  virtual void Run(RenderThread& aRenderThread, gfx::WindowId aWindowId) override
  {
    RefPtr<RenderThread> thread = &aRenderThread;
    auto renderer = RendererOGL::Create(thread.forget(),
                                        mCompositorWidget.forget(),
                                        mWrRenderer,
                                        aWindowId,
                                        mBridge);
    if (renderer) {
      aRenderThread.AddRenderer(aWindowId, Move(renderer));
    } else {
      // TODO: should notify the compositor thread rather than crash.
      MOZ_CRASH("Failed to initialize the Renderer.");
    }
  }

  WrRenderer* mWrRenderer;
  CompositorBridgeParentBase* mBridge;
  RefPtr<widget::CompositorWidget> mCompositorWidget;
};

class RemoveRenderer : public RendererEvent
{
public:
  RemoveRenderer() { MOZ_COUNT_CTOR(RemoveRenderer); }

  ~RemoveRenderer() { MOZ_COUNT_DTOR(RemoveRenderer); }

  virtual void Run(RenderThread& aRenderThread, gfx::WindowId aWindowId) override
  {
    aRenderThread.RemoveRenderer(aWindowId);
  }
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

  WrAPI* api = nullptr;
  WrRenderer* renderer = nullptr;

  wr_window_new(id.mHandle, aEnableProfiler, &api, &renderer);
  MOZ_ASSERT(renderer);
  MOZ_ASSERT(api);

  auto event = MakeUnique<NewRenderer>(renderer, aBridge, Move(aWidget));

  // TODO use the WebRender API instead of scheduling on this message loop directly.
  // this needs PR #688
  RenderThread::Get()->RunEvent(id, Move(event));

  RefPtr<WebRenderAPI> ptr(new WebRenderAPI(api, id));
  return ptr.forget();
}

WebRenderAPI::~WebRenderAPI()
{
  auto event = MakeUnique<RemoveRenderer>();

  // TODO use the WebRender API instead of scheduling on this message loop directly.
  // this needs PR #688
  RenderThread::Get()->RunEvent(mId, Move(event));

#ifdef MOZ_ENABLE_WEBRENDER
  // Need to wrap this in an ifdef otherwise VC++ emits a warning (treated as error)
  // in the non-webrender targets.
  // We should be able to remove this #ifdef if/when we remove the
  // MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE annotations in webrender.h
  wr_api_delete(mApi);
#endif
}

} // namespace
} // namespace
