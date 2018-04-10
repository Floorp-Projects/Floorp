/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZUpdater_h
#define mozilla_layers_APZUpdater_h

#include <deque>
#include <unordered_map>

#include "base/platform_thread.h"   // for PlatformThreadId
#include "LayersTypes.h"
#include "mozilla/layers/APZTestData.h"
#include "mozilla/StaticMutex.h"
#include "nsThreadUtils.h"
#include "Units.h"

namespace mozilla {

namespace wr {
struct WrWindowId;
} // namespace wr

namespace layers {

class APZCTreeManager;
class FocusTarget;
class Layer;
class WebRenderScrollData;

/**
 * This interface is used to send updates or otherwise mutate APZ internal
 * state. These functions is usually called from the compositor thread in
 * response to IPC messages. The method implementations internally redispatch
 * themselves to the updater thread in the case where the compositor thread
 * is not the updater thread.
 */
class APZUpdater {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(APZUpdater)

public:
  explicit APZUpdater(const RefPtr<APZCTreeManager>& aApz);

  bool HasTreeManager(const RefPtr<APZCTreeManager>& aApz);
  void SetWebRenderWindowId(const wr::WindowId& aWindowId);

  /**
   * This function is invoked from rust on the scene builder thread when it
   * is created. It effectively tells the APZUpdater "the current thread is
   * the updater thread for this window id" and allows APZUpdater to remember
   * which thread it is.
   */
  static void SetUpdaterThread(const wr::WrWindowId& aWindowId);
  static void ProcessPendingTasks(const wr::WrWindowId& aWindowId);

  void ClearTree();
  void UpdateFocusState(LayersId aRootLayerTreeId,
                        LayersId aOriginatingLayersId,
                        const FocusTarget& aFocusTarget);
  void UpdateHitTestingTree(LayersId aRootLayerTreeId,
                            Layer* aRoot,
                            bool aIsFirstPaint,
                            LayersId aOriginatingLayersId,
                            uint32_t aPaintSequenceNumber);
  void UpdateHitTestingTree(LayersId aRootLayerTreeId,
                            const WebRenderScrollData& aScrollData,
                            bool aIsFirstPaint,
                            LayersId aOriginatingLayersId,
                            uint32_t aPaintSequenceNumber);

  void NotifyLayerTreeAdopted(LayersId aLayersId,
                              const RefPtr<APZUpdater>& aOldUpdater);
  void NotifyLayerTreeRemoved(LayersId aLayersId);

  bool GetAPZTestData(LayersId aLayersId, APZTestData* aOutData);

  void SetTestAsyncScrollOffset(LayersId aLayersId,
                                const FrameMetrics::ViewID& aScrollId,
                                const CSSPoint& aOffset);
  void SetTestAsyncZoom(LayersId aLayersId,
                        const FrameMetrics::ViewID& aScrollId,
                        const LayerToParentLayerScale& aZoom);

  /**
   * This can be used to assert that the current thread is the
   * updater thread (which samples the async transform).
   * This does nothing if thread assertions are disabled.
   */
  void AssertOnUpdaterThread();

  /**
   * Runs the given task on the APZ "updater thread" for this APZUpdater. If
   * this function is called from the updater thread itself then the task is
   * run immediately without getting queued.
   */
  void RunOnUpdaterThread(already_AddRefed<Runnable> aTask);

  /**
   * Returns true if currently on the APZUpdater's "updater thread".
   */
  bool IsUpdaterThread();

  /**
   * Dispatches the given task to the APZ "controller thread", but does it *from*
   * the updater thread. That is, if the thread on which this function is called
   * is not the updater thread, the task is first dispatched to the updater thread.
   * When the updater thread runs it (or if this is called directly on the updater
   * thread), that is when the task gets dispatched to the controller thread.
   * The controller thread then actually runs the task.
   */
  void RunOnControllerThread(already_AddRefed<Runnable> aTask);

protected:
  virtual ~APZUpdater();

  bool UsingWebRenderUpdaterThread() const;
  static already_AddRefed<APZUpdater> GetUpdater(const wr::WrWindowId& aWindowId);

private:
  RefPtr<APZCTreeManager> mApz;

  // Used to manage the mapping from a WR window id to APZUpdater. These are only
  // used if WebRender is enabled. Both sWindowIdMap and mWindowId should only
  // be used while holding the sWindowIdLock.
  static StaticMutex sWindowIdLock;
  static std::unordered_map<uint64_t, APZUpdater*> sWindowIdMap;
  Maybe<wr::WrWindowId> mWindowId;

  // If WebRender and async scene building are enabled, this holds the thread id
  // of the scene builder thread (which is the updater thread) for the
  // compositor associated with this APZUpdater instance. It may be populated
  // even if async scene building is not enabled, but in that case we don't
  // care about the contents.
  // This is written to once during init and never cleared, and so reading it
  // from multiple threads during normal operation (after initialization)
  // without locking should be fine.
  Maybe<PlatformThreadId> mUpdaterThreadId;
#ifdef DEBUG
  // This flag is used to ensure that we don't ever try to do updater-thread
  // stuff before the updater thread has been properly initialized.
  mutable bool mUpdaterThreadQueried;
#endif

  // Lock used to protect mUpdaterQueue
  Mutex mQueueLock;
  // Holds a FIFO queue of tasks to be run on the updater thread,
  // when the updater thread is a WebRender thread, since it won't have a
  // message loop we can dispatch to.
  std::deque<RefPtr<Runnable>> mUpdaterQueue;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_APZUpdater_h
