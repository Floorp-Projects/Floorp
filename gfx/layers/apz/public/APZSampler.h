/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZSampler_h
#define mozilla_layers_APZSampler_h

#include "LayersTypes.h"
#include "mozilla/layers/APZTestData.h"
#include "mozilla/layers/AsyncCompositionManager.h" // for AsyncTransform
#include "mozilla/Maybe.h"
#include "nsTArray.h"
#include "Units.h"

namespace mozilla {

class TimeStamp;

namespace wr {
class TransactionBuilder;
struct WrTransformProperty;
} // namespace wr

namespace layers {

class APZCTreeManager;
class FocusTarget;
class Layer;
class LayerMetricsWrapper;
struct ScrollThumbData;
class WebRenderScrollData;

/**
 * This interface is used to interact with the APZ code from the compositor
 * thread. It internally redispatches the functions to the sampler thread
 * in the case where the two threads are not the same.
 */
class APZSampler {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(APZSampler)

public:
  explicit APZSampler(const RefPtr<APZCTreeManager>& aApz);

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
                              const RefPtr<APZSampler>& aOldSampler);
  void NotifyLayerTreeRemoved(LayersId aLayersId);

  bool PushStateToWR(wr::TransactionBuilder& aTxn,
                     const TimeStamp& aSampleTime,
                     nsTArray<wr::WrTransformProperty>& aTransformArray);

  bool GetAPZTestData(LayersId aLayersId, APZTestData* aOutData);

  void SetTestAsyncScrollOffset(LayersId aLayersId,
                                const FrameMetrics::ViewID& aScrollId,
                                const CSSPoint& aOffset);
  void SetTestAsyncZoom(LayersId aLayersId,
                        const FrameMetrics::ViewID& aScrollId,
                        const LayerToParentLayerScale& aZoom);

  bool SampleAnimations(const LayerMetricsWrapper& aLayer,
                        const TimeStamp& aSampleTime);

  /**
   * Compute the updated shadow transform for a scroll thumb layer that
   * reflects async scrolling of the associated scroll frame.
   *
   * Refer to APZCTreeManager::ComputeTransformForScrollThumb for the
   * description of parameters. The only difference is that this function takes
   * |aContent| instead of |aApzc| and |aMetrics|; aContent is the
   * LayerMetricsWrapper corresponding to the scroll frame that is scrolled by
   * the scroll thumb, and so the APZC and metrics can be obtained from
   * |aContent|.
   */
  LayerToParentLayerMatrix4x4 ComputeTransformForScrollThumb(
      const LayerToParentLayerMatrix4x4& aCurrentTransform,
      const LayerMetricsWrapper& aContent,
      const ScrollThumbData& aThumbData,
      bool aScrollbarIsDescendant,
      AsyncTransformComponentMatrix* aOutClipTransform);

  ParentLayerPoint GetCurrentAsyncScrollOffset(const LayerMetricsWrapper& aLayer);
  AsyncTransform GetCurrentAsyncTransform(const LayerMetricsWrapper& aLayer);
  AsyncTransformComponentMatrix GetOverscrollTransform(const LayerMetricsWrapper& aLayer);
  AsyncTransformComponentMatrix GetCurrentAsyncTransformWithOverscroll(const LayerMetricsWrapper& aLayer);

  void MarkAsyncTransformAppliedToContent(const LayerMetricsWrapper& aLayer);
  bool HasUnusedAsyncTransform(const LayerMetricsWrapper& aLayer);

  /**
   * This can be used to assert that the current thread is the
   * sampler thread (which samples the async transform).
   * This does nothing if thread assertions are disabled.
   */
  void AssertOnSamplerThread();

  /**
   * Runs the given task on the APZ "sampler thread" for this APZSampler. If
   * this function is called from the sampler thread itself then the task is
   * run immediately without getting queued.
   */
  void RunOnSamplerThread(already_AddRefed<Runnable> aTask);

  /**
   * Returns true if currently on the APZSampler's "sampler thread".
   */
  bool IsSamplerThread();

protected:
  virtual ~APZSampler();

private:
  RefPtr<APZCTreeManager> mApz;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_APZSampler_h
