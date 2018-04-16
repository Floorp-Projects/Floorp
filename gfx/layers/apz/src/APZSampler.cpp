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

namespace mozilla {
namespace layers {

StaticMutex APZSampler::sWindowIdLock;
std::unordered_map<uint64_t, APZSampler*> APZSampler::sWindowIdMap;


APZSampler::APZSampler(const RefPtr<APZCTreeManager>& aApz)
  : mApz(aApz)
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

bool
APZSampler::PushStateToWR(wr::TransactionBuilder& aTxn,
                          const TimeStamp& aSampleTime)
{
  // This function will be removed eventually since we'll have WR pull
  // the transforms from APZ instead.
  return mApz->PushStateToWR(aTxn, aSampleTime);
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
  return CompositorThreadHolder::IsInCompositorThread();
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
