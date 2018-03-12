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

  void ClearTree();
  void UpdateFocusState(uint64_t aRootLayerTreeId,
                        uint64_t aOriginatingLayersId,
                        const FocusTarget& aFocusTarget);
  void UpdateHitTestingTree(uint64_t aRootLayerTreeId,
                            Layer* aRoot,
                            bool aIsFirstPaint,
                            uint64_t aOriginatingLayersId,
                            uint32_t aPaintSequenceNumber);
  void UpdateHitTestingTree(uint64_t aRootLayerTreeId,
                            const WebRenderScrollData& aScrollData,
                            bool aIsFirstPaint,
                            uint64_t aOriginatingLayersId,
                            uint32_t aPaintSequenceNumber);

  void NotifyLayerTreeAdopted(uint64_t aLayersId,
                              const RefPtr<APZSampler>& aOldSampler);
  void NotifyLayerTreeRemoved(uint64_t aLayersId);

  bool PushStateToWR(wr::TransactionBuilder& aTxn,
                     const TimeStamp& aSampleTime,
                     nsTArray<wr::WrTransformProperty>& aTransformArray);

  bool GetAPZTestData(uint64_t aLayersId, APZTestData* aOutData);

  void SetTestAsyncScrollOffset(uint64_t aLayersId,
                                const FrameMetrics::ViewID& aScrollId,
                                const CSSPoint& aOffset);
  void SetTestAsyncZoom(uint64_t aLayersId,
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

  AsyncTransform GetCurrentAsyncTransform(const LayerMetricsWrapper& aLayer);
  AsyncTransformComponentMatrix GetOverscrollTransform(const LayerMetricsWrapper& aLayer);

  void MarkAsyncTransformAppliedToContent(const LayerMetricsWrapper& aLayer);

protected:
  virtual ~APZSampler();

private:
  RefPtr<APZCTreeManager> mApz;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_APZSampler_h
