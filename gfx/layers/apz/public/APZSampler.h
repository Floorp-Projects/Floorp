/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZSampler_h
#define mozilla_layers_APZSampler_h

#include <unordered_map>

#include "base/platform_thread.h" // for PlatformThreadId
#include "mozilla/layers/AsyncCompositionManager.h" // for AsyncTransform
#include "mozilla/StaticMutex.h"
#include "nsTArray.h"
#include "Units.h"

namespace mozilla {

class TimeStamp;

namespace wr {
class TransactionBuilder;
struct WrTransformProperty;
struct WrWindowId;
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

  void SetWebRenderWindowId(const wr::WindowId& aWindowId);

  /**
   * This function is invoked from rust on the render backend thread when it
   * is created. It effectively tells the APZSampler "the current thread is
   * the sampler thread for this window id" and allows APZSampler to remember
   * which thread it is.
   */
  static void SetSamplerThread(const wr::WrWindowId& aWindowId);

  bool PushStateToWR(wr::TransactionBuilder& aTxn,
                     const TimeStamp& aSampleTime);

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
  void AssertOnSamplerThread() const;

  /**
   * Returns true if currently on the APZSampler's "sampler thread".
   */
  bool IsSamplerThread() const;

protected:
  virtual ~APZSampler();

  bool UsingWebRenderSamplerThread() const;
  static already_AddRefed<APZSampler> GetSampler(const wr::WrWindowId& aWindowId);

private:
  RefPtr<APZCTreeManager> mApz;

  // Used to manage the mapping from a WR window id to APZSampler. These are only
  // used if WebRender is enabled. Both sWindowIdMap and mWindowId should only
  // be used while holding the sWindowIdLock.
  static StaticMutex sWindowIdLock;
  static std::unordered_map<uint64_t, APZSampler*> sWindowIdMap;
  Maybe<wr::WrWindowId> mWindowId;

  // If WebRender is enabled, this holds the thread id of the render backend
  // thread (which is the sampler thread) for the compositor associated with
  // this APZSampler instance.
  // This is written to once during init and never cleared, and so reading it
  // from multiple threads during normal operation (after initialization)
  // without locking should be fine.
  Maybe<PlatformThreadId> mSamplerThreadId;
#ifdef DEBUG
  // This flag is used to ensure that we don't ever try to do sampler-thread
  // stuff before the updater thread has been properly initialized.
  mutable bool mSamplerThreadQueried;
#endif

};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_APZSampler_h
