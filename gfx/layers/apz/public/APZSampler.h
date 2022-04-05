/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZSampler_h
#define mozilla_layers_APZSampler_h

#include <unordered_map>

#include "apz/src/APZCTreeManager.h"
#include "base/platform_thread.h"  // for PlatformThreadId
#include "mozilla/layers/APZUtils.h"
#include "mozilla/layers/SampleTime.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "Units.h"
#include "VsyncSource.h"

namespace mozilla {

class TimeStamp;

namespace wr {
struct Transaction;
class TransactionWrapper;
struct WrWindowId;
}  // namespace wr

namespace layers {

struct ScrollbarData;

/**
 * This interface exposes APZ methods related to "sampling" (i.e. reading the
 * async transforms produced by APZ). These methods should all be called on
 * the sampler thread.
 */
class APZSampler {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(APZSampler)

 public:
  APZSampler(const RefPtr<APZCTreeManager>& aApz, bool aIsUsingWebRender);

  // Whoever creates this sampler is responsible for calling Destroy() on it
  // before releasing the owning refptr.
  void Destroy();

  void SetWebRenderWindowId(const wr::WindowId& aWindowId);

  /**
   * This function is invoked from rust on the render backend thread when it
   * is created. It effectively tells the APZSampler "the current thread is
   * the sampler thread for this window id" and allows APZSampler to remember
   * which thread it is.
   */
  static void SetSamplerThread(const wr::WrWindowId& aWindowId);
  static void SampleForWebRender(const wr::WrWindowId& aWindowId,
                                 const uint64_t* aGeneratedFrameId,
                                 wr::Transaction* aTransaction);

  void SetSampleTime(const SampleTime& aSampleTime);
  void SampleForWebRender(const Maybe<VsyncId>& aGeneratedFrameId,
                          wr::TransactionWrapper& aTxn);

  /**
   * Similar to above GetCurrentAsyncTransform, but get the current transform
   * with LayersId and ViewID.
   * NOTE: This function should NOT be called on the compositor thread.
   */
  AsyncTransform GetCurrentAsyncTransform(
      const LayersId& aLayersId, const ScrollableLayerGuid::ViewID& aScrollId,
      AsyncTransformComponents aComponents,
      const MutexAutoLock& aProofOfMapLock) const;

  /**
   * Returns the composition bounds of the APZC corresponding to the pair of
   * |aLayersId| and |aScrollId|.
   */
  ParentLayerRect GetCompositionBounds(
      const LayersId& aLayersId, const ScrollableLayerGuid::ViewID& aScrollId,
      const MutexAutoLock& aProofOfMapLock) const;

  struct ScrollOffsetAndRange {
    CSSPoint mOffset;
    CSSRect mRange;
  };
  /**
   * Returns the scroll offset and scroll range of the APZC corresponding to the
   * pair of |aLayersId| and |aScrollId|
   *
   * Note: This is called from OMTA Sampler thread, or Compositor thread for
   * testing.
   */
  Maybe<ScrollOffsetAndRange> GetCurrentScrollOffsetAndRange(
      const LayersId& aLayersId, const ScrollableLayerGuid::ViewID& aScrollId,
      const MutexAutoLock& aProofOfMapLock) const;

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

  template <typename Callback>
  void CallWithMapLock(Callback& aCallback) {
    mApz->CallWithMapLock(aCallback);
  }

 protected:
  virtual ~APZSampler();

  static already_AddRefed<APZSampler> GetSampler(
      const wr::WrWindowId& aWindowId);

 private:
  RefPtr<APZCTreeManager> mApz;
  bool mIsUsingWebRender;

  // Used to manage the mapping from a WR window id to APZSampler. These are
  // only used if WebRender is enabled. Both sWindowIdMap and mWindowId should
  // only be used while holding the sWindowIdLock. Note that we use a
  // StaticAutoPtr wrapper on sWindowIdMap to avoid a static initializer for the
  // unordered_map. This also avoids the initializer/memory allocation in cases
  // where we're not using WebRender.
  static StaticMutex sWindowIdLock MOZ_UNANNOTATED;
  static StaticAutoPtr<std::unordered_map<uint64_t, RefPtr<APZSampler>>>
      sWindowIdMap;
  Maybe<wr::WrWindowId> mWindowId;

  // Lock used to protected mSamplerThreadId
  mutable Mutex mThreadIdLock MOZ_UNANNOTATED;
  // If WebRender is enabled, this holds the thread id of the render backend
  // thread (which is the sampler thread) for the compositor associated with
  // this APZSampler instance.
  Maybe<PlatformThreadId> mSamplerThreadId;

  Mutex mSampleTimeLock MOZ_UNANNOTATED;
  // Can only be accessed or modified while holding mSampleTimeLock.
  SampleTime mSampleTime;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_APZSampler_h
