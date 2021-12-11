/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollingMetrics.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/StaticPrefs_browser.h"

namespace mozilla {

static TimeStamp gScrollingStartTime;
static TimeStamp gScrollingEndTime;
static uint32_t gScrollDistanceCSSPixels = 0;
static dom::InteractionData gScrollingInteraction = {};

void ScrollingMetrics::OnScrollingInteractionEnded() {
  // We are only interested in content process scrolling
  if (XRE_IsParentProcess()) {
    return;
  }

  if (!gScrollingStartTime.IsNull() && !gScrollingEndTime.IsNull()) {
    gScrollingInteraction.mInteractionCount++;
    gScrollingInteraction.mInteractionTimeInMilliseconds +=
        static_cast<uint32_t>(
            (gScrollingEndTime - gScrollingStartTime).ToMilliseconds());

    gScrollingInteraction.mScrollingDistanceInPixels +=
        gScrollDistanceCSSPixels;
  }

  gScrollDistanceCSSPixels = 0;
  gScrollingStartTime = TimeStamp();
  gScrollingEndTime = TimeStamp();
}

void ScrollingMetrics::OnScrollingInteraction(CSSCoord distanceScrolled) {
  // We are only interested in content process scrolling
  if (XRE_IsParentProcess()) {
    return;
  }

  TimeStamp now = TimeStamp::Now();
  if (gScrollingEndTime.IsNull()) {
    gScrollingEndTime = now;
  }

  TimeDuration delay = now - gScrollingEndTime;

  // Has it been too long since the last scroll input event to consider it part
  // of the same interaction?
  if (delay >
      TimeDuration::FromMilliseconds(
          StaticPrefs::browser_places_interactions_scrolling_timeout_ms())) {
    OnScrollingInteractionEnded();
  }

  if (gScrollingStartTime.IsNull()) {
    gScrollingStartTime = now;
  }
  gScrollingEndTime = now;
  gScrollDistanceCSSPixels += static_cast<uint32_t>(distanceScrolled);
}

StaticAutoPtr<ScrollingMetrics> ScrollingMetrics::sSingleton;

ScrollingMetrics* ScrollingMetrics::GetSingleton() {
  if (!sSingleton) {
    sSingleton = new ScrollingMetrics;
  }

  return sSingleton.get();
}

struct ScrollingMetricsCollector {
  void AppendScrollingMetrics(const Tuple<uint32_t, uint32_t>& aMetrics,
                              dom::ContentParent* aParent) {
    mTimeScrolledMS += Get<0>(aMetrics);
    mDistanceScrolledPixels += Get<1>(aMetrics);
  }

  ~ScrollingMetricsCollector() {
    mPromiseHolder.Resolve(MakeTuple(mTimeScrolledMS, mDistanceScrolledPixels),
                           __func__);
  }

  uint32_t mTimeScrolledMS = 0;
  uint32_t mDistanceScrolledPixels = 0;
  MozPromiseHolder<ScrollingMetrics::ScrollingMetricsPromise> mPromiseHolder;
};

auto ScrollingMetrics::CollectScrollingMetricsInternal()
    -> RefPtr<ScrollingMetrics::ScrollingMetricsPromise> {
  std::shared_ptr<ScrollingMetricsCollector> collector =
      std::make_shared<ScrollingMetricsCollector>();

  nsTArray<dom::ContentParent*> contentParents;
  dom::ContentParent::GetAll(contentParents);
  for (dom::ContentParent* parent : contentParents) {
    RefPtr<dom::ContentParent> parentRef = parent;
    parent->SendCollectScrollingMetrics(
        [collector,
         parentRef](const mozilla::Tuple<uint32_t, uint32_t>& aMetrics) {
          collector->AppendScrollingMetrics(aMetrics, parentRef.get());
        },
        [](mozilla::ipc::ResponseRejectReason) {});
  }

  return collector->mPromiseHolder.Ensure(__func__);
}

Tuple<uint32_t, uint32_t>
ScrollingMetrics::CollectLocalScrollingMetricsInternal() {
  OnScrollingInteractionEnded();

  Tuple<uint32_t, uint32_t> metrics =
      MakeTuple(gScrollingInteraction.mInteractionTimeInMilliseconds,
                gScrollingInteraction.mScrollingDistanceInPixels);
  gScrollingInteraction = {};
  return metrics;
}

}  // namespace mozilla
