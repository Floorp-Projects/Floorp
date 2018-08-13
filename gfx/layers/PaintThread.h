/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_PAINTTHREAD_H
#define MOZILLA_LAYERS_PAINTTHREAD_H

#include "base/platform_thread.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/layers/TextureClient.h"
#include "RotatedBuffer.h"
#include "nsThreadUtils.h"

class nsIThreadPool;

namespace mozilla {
namespace gfx {
class DrawTarget;
class DrawTargetCapture;
};

namespace layers {

// A paint task contains a description of a rasterization work to be done
// on the paint thread or paint worker pool.
//
// More specifically it contains:
// 1. A capture command list of drawing commands
// 2. A destination draw target to replay the draw commands upon
// 3. A list of dependent texture clients that must be kept alive for the
//    task's duration, and then destroyed on the main thread
class PaintTask {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PaintTask)
public:
  PaintTask() {}

  void DropTextureClients();

  RefPtr<gfx::DrawTarget> mTarget;
  RefPtr<gfx::DrawTargetCapture> mCapture;
  AutoTArray<RefPtr<TextureClient>, 4> mClients;

protected:
  virtual ~PaintTask() {}
};

class CompositorBridgeChild;

class PaintThread final
{
  friend void DestroyPaintThread(UniquePtr<PaintThread>&& aPaintThread);

public:
  static void Start();
  static void Shutdown();
  static PaintThread* Get();

  // Helper for asserts.
  static bool IsOnPaintThread();
  bool IsOnPaintWorkerThread();

  // This allows external users to run code on the paint thread.
  void Dispatch(RefPtr<Runnable>& aRunnable);

  // This allows the paint thread to dynamically toggle between a paint worker
  // thread pool used with tiling, and a single paint thread used with rotated
  // buffer.
  void UpdateRenderMode();

  // Must be called on the main thread. Queues an async paint
  // task to be completed on the paint thread.
  void QueuePaintTask(PaintTask* aTask);

  // Must be called on the main thread. Signifies that the current
  // layer tree transaction has been finished and any async paints
  // for it have been queued on the paint thread. This MUST be called
  // at the end of a layer transaction as it will be used to do an optional
  // texture sync and then unblock the main thread if it is waiting to paint
  // a new frame.
  void QueueEndLayerTransaction(SyncObjectClient* aSyncObject);

  // Sync Runnables need threads to be ref counted,
  // But this thread lives through the whole process.
  // We're only temporarily using sync runnables so
  // Override release/addref but don't do anything.
  void Release();
  void AddRef();

  static int32_t CalculatePaintWorkerCount();

private:
  PaintThread();

  bool Init();
  void ShutdownOnPaintThread();
  void InitOnPaintThread();
  void InitPaintWorkers();

  void AsyncPaintTask(CompositorBridgeChild* aBridge,
                      PaintTask* aTask);
  void AsyncEndLayerTransaction(CompositorBridgeChild* aBridge);

  static StaticAutoPtr<PaintThread> sSingleton;
  static StaticRefPtr<nsIThread> sThread;
  static PlatformThreadId sThreadId;

  RefPtr<nsIThreadPool> mPaintWorkers;
};

} // namespace layers
} // namespace mozilla

#endif
