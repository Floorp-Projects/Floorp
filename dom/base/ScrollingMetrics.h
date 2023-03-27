/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScrollingMetrics_h
#define mozilla_dom_ScrollingMetrics_h

#include "Units.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {

/**
 * ScrollingMetrics records user-intiated scrolling interactions. These are
 * aggregrated along with other user interactions (e.g. typing, view time), in
 * the history metadata.
 */
class ScrollingMetrics {
 public:
  using ScrollingMetricsPromise =
      MozPromise<std::tuple<uint32_t, uint32_t>, bool, true>;

  static RefPtr<ScrollingMetricsPromise> CollectScrollingMetrics() {
    return GetSingleton()->CollectScrollingMetricsInternal();
  }

  // Return the tuple of <scrollingTimeInMilliseconds,
  // ScrollingDistanceInPixels>
  static std::tuple<uint32_t, uint32_t> CollectLocalScrollingMetrics() {
    return GetSingleton()->CollectLocalScrollingMetricsInternal();
  }

  // Report a new user-initiated scrolling interaction
  static void OnScrollingInteraction(CSSCoord distanceScrolled);

  static void OnScrollingInteractionEnded();

 private:
  static ScrollingMetrics* GetSingleton();
  static StaticAutoPtr<ScrollingMetrics> sSingleton;
  RefPtr<ScrollingMetricsPromise> CollectScrollingMetricsInternal();
  std::tuple<uint32_t, uint32_t> CollectLocalScrollingMetricsInternal();
};

}  // namespace mozilla

#endif /* mozilla_dom_ScrollingMetrics_h */
