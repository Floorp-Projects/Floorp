/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZUpdater_h
#define mozilla_layers_APZUpdater_h

#include <deque>
#include <unordered_map>

#include "base/platform_thread.h"  // for PlatformThreadId
#include "LayersTypes.h"
#include "APZTypes.h"
#include "mozilla/layers/APZTestData.h"
#include "mozilla/layers/WebRenderScrollData.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "nsThreadUtils.h"
#include "Units.h"

namespace mozilla {

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
  APZUpdater(const RefPtr<APZCTreeManager>& aApz, bool aIsUsingWebRender);

  bool HasTreeManager(const RefPtr<APZCTreeManager>& aApz);
  void SetWebRenderWindowId(const wr::WindowId& aWindowId);

  /**
   * This function is invoked from rust on the scene builder thread when it
   * is created. It effectively tells the APZUpdater "the current thread is
   * the updater thread for this window id" and allows APZUpdater to remember
   * which thread it is.
   */
  static void SetUpdaterThread(const wr::WrWindowId& aWindowId);
  static void PrepareForSceneSwap(const wr::WrWindowId& aWindowId);
  static void CompleteSceneSwap(const wr::WrWindowId& aWindowId,
                                const wr::WrPipelineInfo& aInfo);
  static void ProcessPendingTasks(const wr::WrWindowId& aWindowId);

  void ClearTree(LayersId aRootLayersId);
  void UpdateFocusState(LayersId aRootLayerTreeId,
                        WRRootId aOriginatingWrRootId,
                        const FocusTarget& aFocusTarget);
  void UpdateHitTestingTree(LayersId aRootLayerTreeId, Layer* aRoot,
                            bool aIsFirstPaint, LayersId aOriginatingLayersId,
                            uint32_t aPaintSequenceNumber);
  /**
   * This should be called (in the WR-enabled case) when the compositor receives
   * a new WebRenderScrollData for a layers id. The |aScrollData| parameter is
   * the scroll data for |aOriginatingLayersId| and |aEpoch| is the
   * corresponding epoch for the transaction that transferred the scroll data.
   * This function will store the new scroll data and update the focus state and
   * hit-testing tree.
   */
  void UpdateScrollDataAndTreeState(WRRootId aRootLayerTreeId,
                                    WRRootId aOriginatingWrRootId,
                                    const wr::Epoch& aEpoch,
                                    WebRenderScrollData&& aScrollData);
  /**
   * This is called in the WR-enabled case when we get an empty transaction that
   * has some scroll offset updates (from paint-skipped scrolling on the content
   * side). This function will update the stored scroll offsets and the
   * hit-testing tree.
   */
  void UpdateScrollOffsets(WRRootId aRootLayerTreeId,
                           WRRootId aOriginatingWrRootId,
                           ScrollUpdatesMap&& aUpdates,
                           uint32_t aPaintSequenceNumber);

  void NotifyLayerTreeAdopted(WRRootId aWrRootId,
                              const RefPtr<APZUpdater>& aOldUpdater);
  void NotifyLayerTreeRemoved(WRRootId aWrRootId);

  bool GetAPZTestData(WRRootId aWrRootId, APZTestData* aOutData);

  void SetTestAsyncScrollOffset(WRRootId aWrRootId,
                                const ScrollableLayerGuid::ViewID& aScrollId,
                                const CSSPoint& aOffset);
  void SetTestAsyncZoom(WRRootId aWrRootId,
                        const ScrollableLayerGuid::ViewID& aScrollId,
                        const LayerToParentLayerScale& aZoom);

  // This can only be called on the updater thread.
  const WebRenderScrollData* GetScrollData(WRRootId aWrRootId) const;

  /**
   * This can be used to assert that the current thread is the
   * updater thread (which samples the async transform).
   * This does nothing if thread assertions are disabled.
   */
  void AssertOnUpdaterThread() const;

  /**
   * Runs the given task on the APZ "updater thread" for this APZUpdater. If
   * this function is called from the updater thread itself then the task is
   * run immediately without getting queued.
   *
   * Conceptually each (layers tree, render root) tuple has a separate task
   * queue. (In the case where WebRender is disabled, the render root is
   * always the default render root). This makes it so that even if one
   * (layers tree, render root) is blocked waiting for a scene build in
   * WebRender, other tasks can still be processed. However, there may be
   * tasks that are tied to multiple render roots within a given layers tree,
   * and which would therefore block on all the associated (layers tree,
   * render root) queues. The aSelector argument allows expressing the set of
   * render roots the task is tied to so that this ordering dependency can be
   * respected.
   */
  void RunOnUpdaterThread(UpdaterQueueSelector aSelector,
                          already_AddRefed<Runnable> aTask);

  /**
   * Returns true if currently on the APZUpdater's "updater thread".
   */
  bool IsUpdaterThread() const;

  /**
   * Dispatches the given task to the APZ "controller thread", but does it
   * *from* the updater thread. That is, if the thread on which this function is
   * called is not the updater thread, the task is first dispatched to the
   * updater thread. When the updater thread runs it (or if this is called
   * directly on the updater thread), that is when the task gets dispatched to
   * the controller thread. The controller thread then actually runs the task.
   *
   * See the RunOnUpdaterThread method for details on the aSelector argument.
   */
  void RunOnControllerThread(UpdaterQueueSelector aSelector,
                             already_AddRefed<Runnable> aTask);

 protected:
  virtual ~APZUpdater();

  bool UsingWebRenderUpdaterThread() const;
  static already_AddRefed<APZUpdater> GetUpdater(
      const wr::WrWindowId& aWindowId);

  void ProcessQueue();

 private:
  RefPtr<APZCTreeManager> mApz;
  bool mDestroyed;
  bool mIsUsingWebRender;

  // Map from WRRoot id to WebRenderScrollData. This can only be touched on
  // the updater thread.
  std::unordered_map<WRRootId, WebRenderScrollData, WRRootId::HashFn>
      mScrollData;

  // Stores epoch state for a particular WRRoot id. This structure is only
  // accessed on the updater thread.
  struct EpochState {
    // The epoch for the most recent scroll data sent from the content side.
    wr::Epoch mRequired;
    // The epoch for the most recent scene built and swapped in on the WR side.
    Maybe<wr::Epoch> mBuilt;
    // True if and only if the WRRoot id is the root WRRoot id for the
    // compositor
    bool mIsRoot;

    EpochState();

    // Whether or not the state for this WRRoot id is such that it blocks
    // processing of tasks. This happens if the root tree or a "visible"
    // render root has scroll data for an epoch newer than what has been
    // built. A "visible" render root is one that is attached to the full
    // layer tree (i.e. there is a chain of reflayer items from the root layer
    // tree to the relevant layer subtree) on a WR document whose scene has
    // been built. This is not always the case; for instance a content process
    // may send the compositor layers for a document before the chrome has
    // attached the remote iframe to the root UI document. Since WR only
    // builds pipelines for "visible" render roots, |mBuilt| being populated
    // means that the render root is "visible".
    bool IsBlocked() const;
  };

  // Map from WRRoot id to epoch state.
  // This data structure can only be touched on the updater thread.
  std::unordered_map<WRRootId, EpochState, WRRootId::HashFn> mEpochData;

  // Used to manage the mapping from a WR window id to APZUpdater. These are
  // only used if WebRender is enabled. Both sWindowIdMap and mWindowId should
  // only be used while holding the sWindowIdLock. Note that we use a
  // StaticAutoPtr wrapper on sWindowIdMap to avoid a static initializer for the
  // unordered_map. This also avoids the initializer/memory allocation in cases
  // where we're not using WebRender.
  static StaticMutex sWindowIdLock;
  static StaticAutoPtr<std::unordered_map<uint64_t, APZUpdater*>> sWindowIdMap;
  Maybe<wr::WrWindowId> mWindowId;

  // Lock used to protected mUpdaterThreadId;
  mutable Mutex mThreadIdLock;
  // If WebRender and async scene building are enabled, this holds the thread id
  // of the scene builder thread (which is the updater thread) for the
  // compositor associated with this APZUpdater instance. It may be populated
  // even if async scene building is not enabled, but in that case we don't
  // care about the contents.
  Maybe<PlatformThreadId> mUpdaterThreadId;

  // Helper struct that pairs each queued runnable with the layers id and render
  // roots that it is associated with. This allows us to easily implement the
  // conceptual separation of mUpdaterQueue into independent queues per (layers
  // id, render root) pair. Note that when the UpdaterQueueSelector has multiple
  // render roots, the task blocks on *all* of the queues for the (layers
  // id, render root) pairs.
  struct QueuedTask {
    UpdaterQueueSelector mSelector;
    RefPtr<Runnable> mRunnable;
  };

  // Lock used to protect mUpdaterQueue
  Mutex mQueueLock;
  // Holds a queue of tasks to be run on the updater thread, when the updater
  // thread is a WebRender thread, since it is conceptually separated into
  // multiple ones, one per (layers id, render root). Tasks for a given
  // conceptual queue will always run in FIFO order, and tasks that are tied
  // to multiple queues (by virtue of having multiple render roots in their
  // UpdaterQueueSelector) can cause one queue to be blocked on another in
  // order to preserve FIFO ordering. In the common case, though, where there
  // is exactly one render root per task, there is no guaranteed ordering for
  // tasks with different render roots.
  std::deque<QueuedTask> mUpdaterQueue;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_APZUpdater_h
