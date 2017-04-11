/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_composite_Diagnostics_h
#define mozilla_gfx_layers_composite_Diagnostics_h

#include "FPSCounter.h"
#include "gfxPrefs.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#include <deque>
#include <string>
#include <utility>

namespace mozilla {
namespace layers {

class PaintTiming;

class TimedMetric
{
  typedef std::pair<float, TimeStamp> Entry;

public:
  void Add(float aValue) {
    if (mHistory.size() > kMaxHistory) {
      mHistory.pop_front();
    }
    mHistory.push_back(Entry(aValue, TimeStamp::Now()));
  }

  float Average() const;
  bool Empty() const {
    return mHistory.empty();
  }

private:
  static const size_t kMaxHistory = 60;

  std::deque<Entry> mHistory;
};

// These statistics are collected by layers backends, preferably by the GPU
struct GPUStats
{
  GPUStats()
   : mInvalidPixels(0),
     mScreenPixels(0),
     mPixelsFilled(0)
  {}

  uint32_t mInvalidPixels;
  uint32_t mScreenPixels;
  uint32_t mPixelsFilled;
  Maybe<float> mDrawTime;
};

// Collects various diagnostics about layers performance.
class Diagnostics
{
public:
  Diagnostics();

  void RecordPaintTimes(const PaintTiming& aPaintTimes);
  void RecordUpdateTime(float aValue) {
    mUpdateMs.Add(aValue);
  }
  void RecordPrepareTime(float aValue) {
    mPrepareMs.Add(aValue);
  }
  void RecordCompositeTime(float aValue) {
    mCompositeMs.Add(aValue);
  }
  void AddTxnFrame() {
    mTransactionFps.AddFrame(TimeStamp::Now());
  }

  std::string GetFrameOverlayString(const GPUStats& aStats);

  class Record {
  public:
    Record() {
      if (gfxPrefs::LayersDrawFPS()) {
        mStart = TimeStamp::Now();
      }
    }
    bool Recording() const {
      return !mStart.IsNull();
    }
    float Duration() const {
      return (TimeStamp::Now() - mStart).ToMilliseconds();
    }

  private:
    TimeStamp mStart;
  };

private:
  FPSCounter mCompositeFps;
  FPSCounter mTransactionFps;
  TimedMetric mDlbMs;
  TimedMetric mFlbMs;
  TimedMetric mRasterMs;
  TimedMetric mSerializeMs;
  TimedMetric mSendMs;
  TimedMetric mUpdateMs;
  TimedMetric mPrepareMs;
  TimedMetric mCompositeMs;
  TimedMetric mGPUDrawMs;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_composite_Diagnostics_h
