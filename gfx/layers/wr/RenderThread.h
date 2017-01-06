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

class RendererEvent
{
public:
  virtual ~RendererEvent() {}
  virtual void Run(RenderThread& aRenderThread, gfx::WindowId aWindow);
};

class RenderThread final
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(RenderThread)

public:
  static RenderThread* Get();

  static void Start();

  static void ShutDown();

  static MessageLoop* Loop();

  static bool IsInRenderThread();

  void AddRenderer(gfx::WindowId aWindowId, UniquePtr<RendererOGL> aRenderer);

  void RemoveRenderer(gfx::WindowId aWindowId);

  // Automatically forwarded to the render thread.
  void NewFrameReady(gfx::WindowId aWindowId);

  // Automatically forwarded to the render thread.
  void NewScrollFrameReady(gfx::WindowId aWindowId, bool aCompositeNeeded);

  // Automatically forwarded to the render thread.
  void PipelineSizeChanged(gfx::WindowId aWindowId, uint64_t aPipelineId, float aWidth, float aHeight);

  // Automatically forwarded to the render thread.
  void RunEvent(gfx::WindowId aWindowId, UniquePtr<RendererEvent> aCallBack);

private:
  explicit RenderThread(base::Thread* aThread);

  ~RenderThread();

  void UpdateAndRender(gfx::WindowId aWindowId);

  base::Thread* const mThread;

  std::map<gfx::WindowId, UniquePtr<RendererOGL>> mRenderers;
};

} // namespace
} // namespace

#endif
