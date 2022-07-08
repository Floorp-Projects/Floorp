/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _include_gfx_ipc_CanvasRenderThread_h__
#define _include_gfx_ipc_CanvasRenderThread_h__

#include "mozilla/DataMutex.h"
#include "mozilla/layers/SynchronousTask.h"
#include "mozilla/StaticPtr.h"
#include "nsISupportsImpl.h"
#include "nsThread.h"

namespace mozilla::gfx {

class CanvasRenderThread final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_DELETE_ON_MAIN_THREAD(
      CanvasRenderThread)

 public:
  /// Can be called from any thread.
  static CanvasRenderThread* Get();

  /// Can only be called from the main thread.
  static void Start();

  /// Can only be called from the main thread.
  static void ShutDown();

  /// Can be called from any thread.
  static bool IsInCanvasRenderThread();

  /// Can be called from any thread.
  static already_AddRefed<nsIThread> GetCanvasRenderThread();

 private:
  explicit CanvasRenderThread(RefPtr<nsIThread> aThread);
  ~CanvasRenderThread();

  void ShutDownTask(layers::SynchronousTask* aTask);
  void PostRunnable(already_AddRefed<nsIRunnable> aRunnable);

  RefPtr<nsIThread> const mThread;
};

}  // namespace mozilla::gfx

#endif  // _include_gfx_ipc_CanvasRenderThread_h__
