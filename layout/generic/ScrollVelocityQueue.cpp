/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollVelocityQueue.h"

#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/StaticPrefs_layout.h"
#include "nsPresContext.h"
#include "nsRefreshDriver.h"

namespace mozilla {
namespace layout {

void ScrollVelocityQueue::Sample(const nsPoint& aScrollPosition) {
  float flingSensitivity =
      StaticPrefs::layout_css_scroll_snap_prediction_sensitivity();
  int maxVelocity =
      StaticPrefs::layout_css_scroll_snap_prediction_max_velocity();
  maxVelocity = nsPresContext::CSSPixelsToAppUnits(maxVelocity);
  int maxOffset = maxVelocity * flingSensitivity;
  TimeStamp currentRefreshTime =
      mPresContext->RefreshDriver()->MostRecentRefresh();
  if (mSampleTime.IsNull()) {
    mAccumulator = nsPoint();
  } else {
    uint32_t durationMs = (currentRefreshTime - mSampleTime).ToMilliseconds();
    if (durationMs > StaticPrefs::apz_velocity_relevance_time_ms()) {
      mAccumulator = nsPoint();
      mQueue.Clear();
    } else if (durationMs == 0) {
      mAccumulator += aScrollPosition - mLastPosition;
    } else {
      nsPoint velocity = mAccumulator * 1000 / durationMs;
      velocity.Clamp(maxVelocity);
      mQueue.AppendElement(std::make_pair(durationMs, velocity));
      mAccumulator = aScrollPosition - mLastPosition;
    }
  }
  mAccumulator.Clamp(maxOffset);
  mSampleTime = currentRefreshTime;
  mLastPosition = aScrollPosition;
  TrimQueue();
}

void ScrollVelocityQueue::TrimQueue() {
  if (mSampleTime.IsNull()) {
    // There are no samples, nothing to do here.
    return;
  }

  TimeStamp currentRefreshTime =
      mPresContext->RefreshDriver()->MostRecentRefresh();
  uint32_t timeDelta = (currentRefreshTime - mSampleTime).ToMilliseconds();
  for (int i = mQueue.Length() - 1; i >= 0; i--) {
    timeDelta += mQueue[i].first;
    if (timeDelta >= StaticPrefs::apz_velocity_relevance_time_ms()) {
      // The rest of the samples have expired and should be dropped
      for (; i >= 0; i--) {
        mQueue.RemoveElementAt(0);
      }
    }
  }
}

void ScrollVelocityQueue::Reset() {
  mAccumulator = nsPoint();
  mSampleTime = TimeStamp();
  mQueue.Clear();
}

/**
  Calculate the velocity of the scroll frame, in appunits / second.
*/
nsPoint ScrollVelocityQueue::GetVelocity() {
  TrimQueue();
  if (mQueue.Length() == 0) {
    // If getting the scroll velocity before any scrolling has occurred,
    // the velocity must be (0, 0)
    return nsPoint();
  }
  nsPoint velocity;
  for (int i = mQueue.Length() - 1; i >= 0; i--) {
    velocity += mQueue[i].second;
  }
  return velocity / mQueue.Length();
  ;
}

}  // namespace layout
}  // namespace mozilla
