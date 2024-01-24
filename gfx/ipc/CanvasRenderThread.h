/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _include_gfx_ipc_CanvasRenderThread_h__
#define _include_gfx_ipc_CanvasRenderThread_h__

#include "mozilla/AlreadyAddRefed.h"
#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"

class nsIRunnable;
class nsIThread;
class nsIThreadPool;

namespace mozilla {
class TaskQueue;

namespace gfx {

/**
 * This class represents the virtual thread for canvas rendering. Depending on
 * platform requirements and user configuration, canvas rendering may happen on
 * the Compositor thread, Render thread or the CanvasRender thread.
 */
class CanvasRenderThread final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_DELETE_ON_MAIN_THREAD(
      CanvasRenderThread)

 public:
  /// Can only be called from the main thread, expected to be called at most
  /// once during a process' lifetime. Must be called after the Compositor and
  /// Render threads are initialized.
  static void Start();

  /// Can only be called from the main thread. Must be called before the
  /// Compositor and Render threads are shutdown.
  static void Shutdown();

  /// Can be called from any thread.
  static bool IsInCanvasRenderThread();

  /// Can be called from any thread.
  static bool IsInCanvasWorkerThread();

  /// Can be called from any thread.
  static bool IsInCanvasRenderOrWorkerThread();

  /// Can be called from any thread, may return nullptr late in shutdown.
  static already_AddRefed<nsIThread> GetCanvasRenderThread();

  static already_AddRefed<TaskQueue> CreateWorkerTaskQueue();

  static void Dispatch(already_AddRefed<nsIRunnable> aRunnable);

 private:
  CanvasRenderThread(nsCOMPtr<nsIThread>&& aThread,
                     nsCOMPtr<nsIThreadPool>&& aWorkers, bool aCreatedThread);
  ~CanvasRenderThread();

  nsCOMPtr<nsIThread> const mThread;

  nsCOMPtr<nsIThreadPool> const mWorkers;

  // True if mThread points to CanvasRender thread, false if mThread points to
  // Compositor/Render thread.
  bool mCreatedThread;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // _include_gfx_ipc_CanvasRenderThread_h__
