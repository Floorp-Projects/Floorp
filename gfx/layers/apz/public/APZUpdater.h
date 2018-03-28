/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZUpdater_h
#define mozilla_layers_APZUpdater_h

#include "LayersTypes.h"
#include "mozilla/layers/APZTestData.h"
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
  explicit APZUpdater(const RefPtr<APZCTreeManager>& aApz);

  bool HasTreeManager(const RefPtr<APZCTreeManager>& aApz);

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

private:
  RefPtr<APZCTreeManager> mApz;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_APZUpdater_h
