/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CanvasThread_h
#define mozilla_layers_CanvasThread_h

#include "mozilla/layers/CompositorThread.h"
#include "mozilla/DataMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/UniquePtr.h"
#include "nsISupports.h"
#include "nsIThread.h"
#include "nsIThreadPool.h"

namespace mozilla {
namespace layers {

/**
 * The CanvasThreadHolder is used to manage the lifetime of the canvas
 * thread (IPC) and workers and also provides methods for using them.
 */
class CanvasThreadHolder final {
  // We only AddRef/Release on the Compositor thread.
  NS_INLINE_DECL_REFCOUNTING(CanvasThreadHolder)

 public:
  /**
   * Ensures that the canvas thread and workers are created.
   * This must be called on the compositor thread.
   * @return a CanvasThreadHolder
   */
  static already_AddRefed<CanvasThreadHolder> EnsureCanvasThread();

  /**
   * Used to release CanvasThreadHolder references, which must be released on
   * the compositor thread.
   * @param aCanvasThreadHolder the reference to release
   */
  static void ReleaseOnCompositorThread(
      already_AddRefed<CanvasThreadHolder> aCanvasThreadHolder);

  /**
   * @return true if in the canvas thread
   */
  static bool IsInCanvasThread();

  /**
   * @return true if in a canvas worker thread
   */
  static bool IsInCanvasWorker();

  /**
   * @return true if in the canvas thread or worker
   */
  static bool IsInCanvasThreadOrWorker();

  /**
   * Static method to dispatch a runnable to the canvas thread. If there is no
   * canvas thread this will just release aRunnable.
   * @param aRunnable the runnable to dispatch
   */
  static void MaybeDispatchToCanvasThread(
      already_AddRefed<nsIRunnable> aRunnable);

  /**
   * Dispatch a runnable to the canvas thread.
   * @param aRunnable the runnable to dispatch
   */
  void DispatchToCanvasThread(already_AddRefed<nsIRunnable> aRunnable);

  /**
   * Create a TaskQueue for the canvas worker threads. This must be shutdown
   * before the reference to CanvasThreadHolder is dropped.
   * @return a TaskQueue that dispatches to the canvas worker threads
   */
  already_AddRefed<TaskQueue> CreateWorkerTaskQueue();

 private:
  static void StaticRelease(
      already_AddRefed<CanvasThreadHolder> aCanvasThreadHolder);

  static StaticDataMutex<StaticRefPtr<CanvasThreadHolder>> sCanvasThreadHolder;

  CanvasThreadHolder(already_AddRefed<nsIThread> aCanvasThread,
                     already_AddRefed<nsIThreadPool> aCanvasWorkers);

  ~CanvasThreadHolder();

  nsCOMPtr<nsIThread> mCanvasThread;
  RefPtr<nsIThreadPool> mCanvasWorkers;

  // Hold a reference to prevent the compositor thread ending.
  RefPtr<CompositorThreadHolder> mCompositorThreadKeepAlive;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_CanvasThread_h
