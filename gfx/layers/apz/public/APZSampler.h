/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZSampler_h
#define mozilla_layers_APZSampler_h

#include "mozilla/layers/AsyncCompositionManager.h" // for AsyncTransform
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
class LayerMetricsWrapper;
struct ScrollbarData;

/**
 * This interface exposes APZ methods related to "sampling" (i.e. reading the
 * async transforms produced by APZ). These methods should all be called on
 * the sampler thread.
 */
class APZSampler {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(APZSampler)

public:
  explicit APZSampler(const RefPtr<APZCTreeManager>& aApz);

  bool PushStateToWR(wr::TransactionBuilder& aTxn,
                     const TimeStamp& aSampleTime,
                     nsTArray<wr::WrTransformProperty>& aTransformArray);

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
      const ScrollbarData& aThumbData,
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
