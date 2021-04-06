/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_OMTASampler_h
#define mozilla_layers_OMTASampler_h

#include <unordered_map>
#include <queue>

#include "base/platform_thread.h"           // for PlatformThreadId
#include "mozilla/layers/OMTAController.h"  // for OMTAController
#include "mozilla/Maybe.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/webrender/WebRenderTypes.h"  // For WrWindowId, WrEpoch, etc.

namespace mozilla {

class TimeStamp;

namespace wr {
struct Transaction;
class TransactionWrapper;
}  // namespace wr

namespace layers {
class Animation;
class CompositorAnimationStorage;
class OMTAValue;
struct CompositorAnimationIdsForEpoch;
struct LayersId;
struct WrAnimations;

/**
 * This interface exposes OMTA methods related to "sampling" (i.e. calculating
 * animating values) and "". All sampling methods should be called on the
 * sampler thread, all some of them should be called on the compositor thread.
 */
class OMTASampler final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OMTASampler)

 public:
  OMTASampler(const RefPtr<CompositorAnimationStorage>& aAnimStorage,
              LayersId aRootLayersId);

  // Whoever creates this sampler is responsible for calling Destroy() on it
  // before releasing the owning refptr.
  void Destroy();

  void SetWebRenderWindowId(const wr::WrWindowId& aWindowId);

  /**
   * This function is invoked from rust on the render backend thread when it
   * is created. It effectively tells the OMTASampler "the current thread is
   * the sampler thread for this window id" and allows OMTASampler to remember
   * which thread it is.
   */
  static void SetSamplerThread(const wr::WrWindowId& aWindowId);

  static void Sample(const wr::WrWindowId& aWindowId, wr::Transaction* aTxn);

  /**
   * Sample all animations, called on the sampler thread.
   */
  void Sample(wr::TransactionWrapper& aTxn);

  /**
   * These funtions get called on the the compositor thread.
   */
  void SetSampleTime(const TimeStamp& aSampleTime);
  void ResetPreviousSampleTime();
  void SetAnimations(uint64_t aId, const LayersId& aLayersId,
                     const nsTArray<layers::Animation>& aAnimations);
  bool HasAnimations() const;

  /**
   * Clear AnimatedValues and Animations data, called on the compositor
   * thread.
   */
  void ClearActiveAnimations(
      std::unordered_map<uint64_t, wr::Epoch>& aActiveAnimations);
  void RemoveEpochDataPriorTo(
      std::queue<CompositorAnimationIdsForEpoch>& aCompositorAnimationsToDelete,
      std::unordered_map<uint64_t, wr::Epoch>& aActiveAnimations,
      const wr::Epoch& aRenderedEpoch);

  // Those two methods are for testing called on the compositor thread.
  OMTAValue GetOMTAValue(const uint64_t& aId) const;
  /**
   * There are two possibilities when this function gets called, either 1) in
   * testing refesh driver mode or 2) in normal refresh driver mode. In the case
   * of 2) |aTestingSampleTime| should be Nothing() so that we can use
   * |mPreviousSampleTime| and |mSampleTime| for sampling animations.
   */
  void SampleForTesting(const Maybe<TimeStamp>& aTestingSampleTime);

  /**
   * Returns true if currently on the "sampler thread".
   */
  bool IsSamplerThread() const;

  void EnterTestMode() { mIsInTestMode = true; }
  void LeaveTestMode() { mIsInTestMode = false; }

 protected:
  ~OMTASampler() = default;

  static already_AddRefed<OMTASampler> GetSampler(
      const wr::WrWindowId& aWindowId);

 private:
  WrAnimations SampleAnimations(const TimeStamp& aPreviousSampleTime,
                                const TimeStamp& aSampleTime);

  RefPtr<OMTAController> mController;
  // Can only be accessed or modified while holding mStorageLock.
  RefPtr<CompositorAnimationStorage> mAnimStorage;
  mutable Mutex mStorageLock;

  // Used to manage the mapping from a WR window id to OMTASampler. These are
  // only used if WebRender is enabled. Both sWindowIdMap and mWindowId should
  // only be used while holding the sWindowIdLock. Note that we use a
  // StaticAutoPtr wrapper on sWindowIdMap to avoid a static initializer for the
  // unordered_map. This also avoids the initializer/memory allocation in cases
  // where we're not using WebRender.
  static StaticMutex sWindowIdLock;
  static StaticAutoPtr<std::unordered_map<uint64_t, RefPtr<OMTASampler>>>
      sWindowIdMap;
  Maybe<wr::WrWindowId> mWindowId;

  // Lock used to protected mSamplerThreadId
  mutable Mutex mThreadIdLock;
  // If WebRender is enabled, this holds the thread id of the render backend
  // thread (which is the sampler thread) for the compositor associated with
  // this OMTASampler instance.
  Maybe<PlatformThreadId> mSamplerThreadId;

  Mutex mSampleTimeLock;
  // Can only be accessed or modified while holding mSampleTimeLock.
  TimeStamp mSampleTime;
  // Same as |mSampleTime|, can only be accessed or modified while holding
  // mSampleTimeLock.
  // We basically use this time stamp instead of |mSampleTime| to make
  // animations more in sync with other animations on the main thread.
  TimeStamp mPreviousSampleTime;
  bool mIsInTestMode;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_OMTASampler_h
