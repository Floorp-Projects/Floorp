/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZSampler.h"

#include "APZCTreeManager.h"
#include "AsyncPanZoomController.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/LayerMetricsWrapper.h"
#include "mozilla/layers/SynchronousTask.h"
#include "TreeTraversal.h"
#include "mozilla/webrender/WebRenderAPI.h"

namespace mozilla {
namespace layers {

StaticMutex APZSampler::sWindowIdLock;
std::unordered_map<uint64_t, APZSampler*> APZSampler::sWindowIdMap;


APZSampler::APZSampler(const RefPtr<APZCTreeManager>& aApz)
  : mApz(aApz)
#ifdef DEBUG
  , mSamplerThreadQueried(false)
#endif
  , mSampleTimeLock("APZSampler::mSampleTimeLock")
{
  MOZ_ASSERT(aApz);
  mApz->SetSampler(this);
}

APZSampler::~APZSampler()
{
  mApz->SetSampler(nullptr);

  StaticMutexAutoLock lock(sWindowIdLock);
  if (mWindowId) {
    sWindowIdMap.erase(wr::AsUint64(*mWindowId));
  }
}

void
APZSampler::SetWebRenderWindowId(const wr::WindowId& aWindowId)
{
  StaticMutexAutoLock lock(sWindowIdLock);
  MOZ_ASSERT(!mWindowId);
  mWindowId = Some(aWindowId);
  sWindowIdMap[wr::AsUint64(aWindowId)] = this;
}

/*static*/ void
APZSampler::SetSamplerThread(const wr::WrWindowId& aWindowId)
{
  if (RefPtr<APZSampler> sampler = GetSampler(aWindowId)) {
    // Ensure nobody tried to use the updater thread before this point.
    MOZ_ASSERT(!sampler->mSamplerThreadQueried);
    sampler->mSamplerThreadId = Some(PlatformThread::CurrentId());
  }
}

void
APZSampler::SetSampleTime(const TimeStamp& aSampleTime)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MutexAutoLock lock(mSampleTimeLock);
  mSampleTime = aSampleTime;
}

bool
APZSampler::PushStateToWR(wr::TransactionWrapper& aTxn)
{
  // This function will be removed eventually since we'll have WR pull
  // the transforms from APZ instead.
  AssertOnSamplerThread();
  TimeStamp sampleTime;
  { // scope lock
    MutexAutoLock lock(mSampleTimeLock);

    // If mSampleTime is null we're in a startup phase where the
    // WebRenderBridgeParent hasn't yet provided us with a sample time.
    // If we're that early there probably aren't any APZ animations happening
    // anyway, so using Timestamp::Now() should be fine.
    sampleTime = mSampleTime.IsNull() ? TimeStamp::Now() : mSampleTime;
  }
  return mApz->PushStateToWR(aTxn, sampleTime);
}

bool
APZSampler::SampleAnimations(const LayerMetricsWrapper& aLayer,
                             const TimeStamp& aSampleTime)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  AssertOnSamplerThread();

  // TODO: eventually we can drop the aLayer argument and just walk the APZ
  // tree directly in mApz.

  bool activeAnimations = false;

  ForEachNodePostOrder<ForwardIterator>(aLayer,
      [&activeAnimations, &aSampleTime](LayerMetricsWrapper aLayerMetrics)
      {
        if (AsyncPanZoomController* apzc = aLayerMetrics.GetApzc()) {
          apzc->ReportCheckerboard(aSampleTime);
          activeAnimations |= apzc->AdvanceAnimations(aSampleTime);
        }
      }
  );

  return activeAnimations;
}

LayerToParentLayerMatrix4x4
APZSampler::ComputeTransformForScrollThumb(const LayerToParentLayerMatrix4x4& aCurrentTransform,
                                           const LayerMetricsWrapper& aContent,
                                           const ScrollbarData& aThumbData,
                                           bool aScrollbarIsDescendant,
                                           AsyncTransformComponentMatrix* aOutClipTransform)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  AssertOnSamplerThread();

  return mApz->ComputeTransformForScrollThumb(aCurrentTransform,
                                              aContent.GetTransform(),
                                              aContent.GetApzc(),
                                              aContent.Metrics(),
                                              aThumbData,
                                              aScrollbarIsDescendant,
                                              aOutClipTransform);
}

ParentLayerPoint
APZSampler::GetCurrentAsyncScrollOffset(const LayerMetricsWrapper& aLayer)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  AssertOnSamplerThread();

  MOZ_ASSERT(aLayer.GetApzc());
  return aLayer.GetApzc()->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForCompositing);
}

AsyncTransform
APZSampler::GetCurrentAsyncTransform(const LayerMetricsWrapper& aLayer)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  AssertOnSamplerThread();

  MOZ_ASSERT(aLayer.GetApzc());
  return aLayer.GetApzc()->GetCurrentAsyncTransform(AsyncPanZoomController::eForCompositing);
}

AsyncTransformComponentMatrix
APZSampler::GetOverscrollTransform(const LayerMetricsWrapper& aLayer)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  AssertOnSamplerThread();

  MOZ_ASSERT(aLayer.GetApzc());
  return aLayer.GetApzc()->GetOverscrollTransform(AsyncPanZoomController::eForCompositing);
}

AsyncTransformComponentMatrix
APZSampler::GetCurrentAsyncTransformWithOverscroll(const LayerMetricsWrapper& aLayer)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  AssertOnSamplerThread();

  MOZ_ASSERT(aLayer.GetApzc());
  return aLayer.GetApzc()->GetCurrentAsyncTransformWithOverscroll(AsyncPanZoomController::eForCompositing);
}

void
APZSampler::MarkAsyncTransformAppliedToContent(const LayerMetricsWrapper& aLayer)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  AssertOnSamplerThread();

  MOZ_ASSERT(aLayer.GetApzc());
  aLayer.GetApzc()->MarkAsyncTransformAppliedToContent();
}

bool
APZSampler::HasUnusedAsyncTransform(const LayerMetricsWrapper& aLayer)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  AssertOnSamplerThread();

  AsyncPanZoomController* apzc = aLayer.GetApzc();
  return apzc
      && !apzc->GetAsyncTransformAppliedToContent()
      && !AsyncTransformComponentMatrix(apzc->GetCurrentAsyncTransform(AsyncPanZoomController::eForCompositing)).IsIdentity();
}

void
APZSampler::AssertOnSamplerThread() const
{
  if (APZThreadUtils::GetThreadAssertionsEnabled()) {
    MOZ_ASSERT(IsSamplerThread());
  }
}

bool
APZSampler::IsSamplerThread() const
{
  if (UsingWebRenderSamplerThread()) {
    return PlatformThread::CurrentId() == *mSamplerThreadId;
  }
  return CompositorThreadHolder::IsInCompositorThread();
}

bool
APZSampler::UsingWebRenderSamplerThread() const
{
  // If mSamplerThreadId is not set at the point that this is called, then
  // that means that either (a) WebRender is not enabled for the compositor
  // to which this APZSampler is attached or (b) we are attempting to do
  // something sampler-related before WebRender is up and running. In case
  // (a) falling back to the compositor thread is correct, and in case (b)
  // we should stop doing the sampler-related thing so early. We catch this
  // case by setting the mSamplerThreadQueried flag and asserting on WR
  // initialization.
#ifdef DEBUG
  mSamplerThreadQueried = true;
#endif
  return mSamplerThreadId.isSome();
}

/*static*/ already_AddRefed<APZSampler>
APZSampler::GetSampler(const wr::WrWindowId& aWindowId)
{
  RefPtr<APZSampler> sampler;
  StaticMutexAutoLock lock(sWindowIdLock);
  auto it = sWindowIdMap.find(wr::AsUint64(aWindowId));
  if (it != sWindowIdMap.end()) {
    sampler = it->second;
  }
  return sampler.forget();
}

} // namespace layers
} // namespace mozilla

void
apz_register_sampler(mozilla::wr::WrWindowId aWindowId)
{
}

void
apz_sample_transforms(mozilla::wr::WrWindowId aWindowId,
                      mozilla::wr::Transaction *aTransaction)
{
}

void
apz_deregister_sampler(mozilla::wr::WrWindowId aWindowId)
{
}
