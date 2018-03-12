/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZSampler_h
#define mozilla_layers_APZSampler_h

#include "mozilla/layers/APZTestData.h"
#include "mozilla/Maybe.h"
#include "nsTArray.h"

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

protected:
  virtual ~APZSampler();

private:
  RefPtr<APZCTreeManager> mApz;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_APZSampler_h
