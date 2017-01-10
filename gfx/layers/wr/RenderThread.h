/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=99 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_RENDERTHREAD_H
#define MOZILLA_LAYERS_RENDERTHREAD_H

#include "base/basictypes.h"            // for DISALLOW_EVIL_CONSTRUCTORS
#include "base/platform_thread.h"       // for PlatformThreadId
#include "base/thread.h"                // for Thread
#include "base/message_loop.h"
#include "nsISupportsImpl.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"
#include "mozilla/gfx/webrender.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/layers/WebRenderTypes.h"

namespace mozilla {
namespace layers {

class RendererOGL;
class RenderThread;

/// Base class for an event that can bes cheduled to run on the render thread.
///
/// The event can be passed through the same channels as regular WebRender messages
/// to preserve ordering.
class RendererEvent
{
public:
  virtual ~RendererEvent() {}
  virtual void Run(RenderThread& aRenderThread, gfx::WindowId aWindow) = 0;
};

/// The render thread is where WebRender issues all of its GPU work, and as much
/// as possible this thread should only serve this purpose.
///
/// The render thread owns the different RendererOGLs (one per window) and implements
/// the RenderNotifier api exposed by the WebRender bindings.
///
/// We should generally avoid posting tasks to the render thhread's event loop directly
/// and instead use the RendererEvent mechanism which avoids races between the events
/// and WebRender's own messages.
///
/// The GL context(s) should be created and used on this thread only.
/// XXX - I've tried to organize code so that we can potentially avoid making
/// this a singleton since this bad habbit has a tendency to bite us later, but
/// I haven't gotten all the way there either in order to focus on the more
/// important pieces first. So we are a bit in-between (this is totally a singleton
/// but in some places we pretend it's not). Hopefully we can evolve this in a way
/// that keeps the door open to removing the singleton bits.
class RenderThread final
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(RenderThread)

public:
  /// Can be called from any thread.
  static RenderThread* Get();

  static void Start();

  static void ShutDown();

  /// Can be called from any thread.
  /// In most cases it is best to use RendererEvents through WebRender's api instead
  /// of scheduling directly to this message loop (so as to preserve the ordering
  /// of the messages).
  static MessageLoop* Loop();

  /// Can be called from any thread.
  static bool IsInRenderThread();

  /// Can only be called from the render thread.
  void AddRenderer(gfx::WindowId aWindowId, UniquePtr<RendererOGL> aRenderer);

  /// Can only be called from the render thread.
  void RemoveRenderer(gfx::WindowId aWindowId);

  // RenderNotifier implementation

  /// Automatically forwarded to the render thread.
  void NewFrameReady(gfx::WindowId aWindowId);

  /// Automatically forwarded to the render thread.
  void NewScrollFrameReady(gfx::WindowId aWindowId, bool aCompositeNeeded);

  /// Automatically forwarded to the render thread.
  void PipelineSizeChanged(gfx::WindowId aWindowId, uint64_t aPipelineId, float aWidth, float aHeight);

  /// Automatically forwarded to the render thread.
  void RunEvent(gfx::WindowId aWindowId, UniquePtr<RendererEvent> aCallBack);

private:
  explicit RenderThread(base::Thread* aThread);

  ~RenderThread();

  /// Can only be called from the render thread.
  void UpdateAndRender(gfx::WindowId aWindowId);

  base::Thread* const mThread;

  std::map<gfx::WindowId, UniquePtr<RendererOGL>> mRenderers;
};

} // namespace
} // namespace

#endif
