/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderThread.h"
#include "nsThreadUtils.h"
#include "mozilla/layers/RendererOGL.h"
#include "mozilla/widget/CompositorWidget.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace layers {

static StaticRefPtr<RenderThread> sRenderThread;

RenderThread::RenderThread(base::Thread* aThread)
: mThread(aThread)
{

}

RenderThread::~RenderThread()
{
  delete mThread;
}

// statuc
RenderThread*
RenderThread::Get()
{
  return sRenderThread;
}

// static
void
RenderThread::Start()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sRenderThread);

  base::Thread* thread = new base::Thread("Renderer");

  base::Thread::Options options;
  // TODO(nical): The compositor thread has a bunch of specific options, see
  // which ones make sense here.
  if (!thread->StartWithOptions(options)) {
    delete thread;
    return;
  }

  sRenderThread = new RenderThread(thread);
}

// static
void
RenderThread::ShutDown()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sRenderThread);

  // TODO(nical): sync with the render thread

  sRenderThread = nullptr;
}

// static
MessageLoop*
RenderThread::Loop()
{
  return sRenderThread ? sRenderThread->mThread->message_loop() : nullptr;
}

// static
bool
RenderThread::IsInRenderThread()
{
  return sRenderThread && sRenderThread->mThread->thread_id() == PlatformThread::CurrentId();
}

void
RenderThread::AddRenderer(gfx::WindowId aWindowId, UniquePtr<RendererOGL> aRenderer)
{
  mRenderers[aWindowId] = Move(aRenderer);
}

void
RenderThread::RemoveRenderer(gfx::WindowId aWindowId)
{
  mRenderers.erase(aWindowId);
}


void
RenderThread::NewFrameReady(gfx::WindowId aWindowId)
{
  if (!IsInRenderThread()) {
    Loop()->PostTask(NewRunnableMethod<gfx::WindowId>(
      this, &RenderThread::NewFrameReady, aWindowId
    ));
    return;
  }

  UpdateAndRender(aWindowId);
}

void
RenderThread::NewScrollFrameReady(gfx::WindowId aWindowId, bool aCompositeNeeded)
{
  if (!IsInRenderThread()) {
    Loop()->PostTask(NewRunnableMethod<gfx::WindowId, bool>(
      this, &RenderThread::NewScrollFrameReady, aWindowId, aCompositeNeeded
    ));
    return;
  }

  UpdateAndRender(aWindowId);
}

void
RenderThread::PipelineSizeChanged(gfx::WindowId aWindowId, uint64_t aPipelineId, float aWidth, float aHeight)
{
  if (!IsInRenderThread()) {
    Loop()->PostTask(NewRunnableMethod<gfx::WindowId, uint64_t, float, float>(
      this, &RenderThread::PipelineSizeChanged,
      aWindowId, aPipelineId, aWidth, aHeight
    ));
    return;
  }

  UpdateAndRender(aWindowId);
}

void
RenderThread::RunEvent(gfx::WindowId aWindowId, UniquePtr<RendererEvent> aEvent)
{
  if (!IsInRenderThread()) {
    Loop()->PostTask(NewRunnableMethod<gfx::WindowId, UniquePtr<RendererEvent>&&>(
      this, &RenderThread::RunEvent,
      aWindowId, Move(aEvent)
    ));
    return;
  }

  aEvent->Run(*this, aWindowId);
  aEvent = nullptr;
}


void
RenderThread::UpdateAndRender(gfx::WindowId aWindowId)
{
  auto it = mRenderers.find(aWindowId);
  MOZ_ASSERT(it != mRenderers.end());
  if (it == mRenderers.end()) {
    return;
  }

  auto& renderer = it->second;

  // TODO: WebRender has the notion of epoch and gecko has transaction ids.
  // They mostly mean the same thing but I'm not sure they are produced the same
  // way. We need to merge the two or have a way to associate transaction ids with
  // epochs to wire everything up properly.
  auto transactionId = 0;

  renderer->Update();
  renderer->Render(transactionId);
}

} // namespace
} // namespace

extern "C" {

void wr_notifier_new_frame_ready(uint64_t aWindowId)
{
  mozilla::layers::RenderThread::Get()->NewFrameReady(mozilla::gfx::WindowId(aWindowId));
}

void wr_notifier_new_scroll_frame_ready(uint64_t aWindowId, bool aCompositeNeeded)
{
  mozilla::layers::RenderThread::Get()->NewScrollFrameReady(mozilla::gfx::WindowId(aWindowId),
                                                            aCompositeNeeded);
}

void wr_notifier_pipeline_size_changed(uint64_t aWindowId,
                                       uint64_t aPipelineId,
                                       float aWidth,
                                       float aHeight)
{
  mozilla::layers::RenderThread::Get()->PipelineSizeChanged(mozilla::gfx::WindowId(aWindowId),
                                                            aPipelineId, aWidth, aHeight);
}

void wr_notifier_external_event(uint64_t aWindowId, size_t aRawEvent)
{
  mozilla::UniquePtr<mozilla::layers::RendererEvent> evt(
    reinterpret_cast<mozilla::layers::RendererEvent*>(aRawEvent));
  mozilla::layers::RenderThread::Get()->RunEvent(mozilla::gfx::WindowId(aWindowId),
                                                 mozilla::Move(evt));
}

} // extern C
