/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CheckerboardEvent.h"
#include "mozilla/Logging.h"

#include <algorithm>  // for std::sort

static mozilla::LazyLogModule sApzCheckLog("apz.checkerboard");

namespace mozilla {
namespace layers {

// Relatively arbitrary limit to prevent a perma-checkerboard event from
// eating up gobs of memory. Ideally we shouldn't have perma-checkerboarding
// but better to guard against it.
#define LOG_LENGTH_LIMIT (50 * 1024)

const char* CheckerboardEvent::sDescriptions[] = {
    "page",
    "painted displayport",
    "requested displayport",
    "viewport",
};

const char* CheckerboardEvent::sColors[] = {
    "brown",
    "lightgreen",
    "yellow",
    "red",
};

CheckerboardEvent::CheckerboardEvent(bool aRecordTrace)
    : mRecordTrace(aRecordTrace),
      mOriginTime(TimeStamp::Now()),
      mCheckerboardingActive(false),
      mLastSampleTime(mOriginTime),
      mFrameCount(0),
      mTotalPixelMs(0),
      mPeakPixels(0),
      mRendertraceLock("Rendertrace") {}

uint32_t CheckerboardEvent::GetSeverity() {
  // Scale the total into a 32-bit value
  return (uint32_t)sqrt((double)mTotalPixelMs);
}

uint32_t CheckerboardEvent::GetPeak() { return mPeakPixels; }

TimeDuration CheckerboardEvent::GetDuration() { return mEndTime - mStartTime; }

std::string CheckerboardEvent::GetLog() {
  MonitorAutoLock lock(mRendertraceLock);
  return mRendertraceInfo.str();
}

bool CheckerboardEvent::IsRecordingTrace() { return mRecordTrace; }

void CheckerboardEvent::UpdateRendertraceProperty(
    RendertraceProperty aProperty, const CSSRect& aRect,
    const std::string& aExtraInfo) {
  if (!mRecordTrace) {
    return;
  }
  MonitorAutoLock lock(mRendertraceLock);
  if (!mCheckerboardingActive) {
    mBufferedProperties[aProperty].Update(aProperty, aRect, aExtraInfo, lock);
  } else {
    LogInfo(aProperty, TimeStamp::Now(), aRect, aExtraInfo, lock);
  }
}

void CheckerboardEvent::LogInfo(RendertraceProperty aProperty,
                                const TimeStamp& aTimestamp,
                                const CSSRect& aRect,
                                const std::string& aExtraInfo,
                                const MonitorAutoLock& aProofOfLock) {
  MOZ_ASSERT(mRecordTrace);
  if (mRendertraceInfo.tellp() >= LOG_LENGTH_LIMIT) {
    // The log is already long enough, don't put more things into it. We'll
    // append a truncation message when this event ends.
    return;
  }
  // The log is consumed by the page at about:checkerboard. The format is not
  // formally specced, but an informal description can be found at
  // https://searchfox.org/mozilla-central/rev/d866b96d74ec2a63f09ee418f048d23f4fd379a2/toolkit/components/aboutcheckerboard/content/aboutCheckerboard.js#86
  mRendertraceInfo << "RENDERTRACE "
                   << (aTimestamp - mOriginTime).ToMilliseconds() << " rect "
                   << sColors[aProperty] << " " << aRect.X() << " " << aRect.Y()
                   << " " << aRect.Width() << " " << aRect.Height() << " "
                   << "// " << sDescriptions[aProperty] << aExtraInfo << '\n';
}

bool CheckerboardEvent::RecordFrameInfo(uint32_t aCssPixelsCheckerboarded) {
  TimeStamp sampleTime = TimeStamp::Now();
  bool eventEnding = false;
  if (aCssPixelsCheckerboarded > 0) {
    if (!mCheckerboardingActive) {
      StartEvent();
    }
    MOZ_ASSERT(mCheckerboardingActive);
    MOZ_ASSERT(sampleTime >= mLastSampleTime);
    mTotalPixelMs +=
        (uint64_t)((sampleTime - mLastSampleTime).ToMilliseconds() *
                   aCssPixelsCheckerboarded);
    if (aCssPixelsCheckerboarded > mPeakPixels) {
      mPeakPixels = aCssPixelsCheckerboarded;
    }
    mFrameCount++;
  } else {
    if (mCheckerboardingActive) {
      StopEvent();
      eventEnding = true;
    }
    MOZ_ASSERT(!mCheckerboardingActive);
  }
  mLastSampleTime = sampleTime;
  return eventEnding;
}

void CheckerboardEvent::StartEvent() {
  MOZ_LOG(sApzCheckLog, LogLevel::Debug, ("Starting checkerboard event"));
  MOZ_ASSERT(!mCheckerboardingActive);
  mCheckerboardingActive = true;
  mStartTime = TimeStamp::Now();

  if (!mRecordTrace) {
    return;
  }
  MonitorAutoLock lock(mRendertraceLock);
  std::vector<PropertyValue> history;
  for (PropertyBuffer& bufferedProperty : mBufferedProperties) {
    bufferedProperty.Flush(history, lock);
  }
  std::sort(history.begin(), history.end());
  for (const PropertyValue& p : history) {
    LogInfo(p.mProperty, p.mTimeStamp, p.mRect, p.mExtraInfo, lock);
  }
  mRendertraceInfo << " -- checkerboarding starts below --\n";
}

void CheckerboardEvent::StopEvent() {
  MOZ_LOG(sApzCheckLog, LogLevel::Debug, ("Stopping checkerboard event"));
  mCheckerboardingActive = false;
  mEndTime = TimeStamp::Now();

  if (!mRecordTrace) {
    return;
  }
  MonitorAutoLock lock(mRendertraceLock);
  if (mRendertraceInfo.tellp() >= LOG_LENGTH_LIMIT) {
    mRendertraceInfo << "[logging aborted due to length limitations]\n";
  }
  mRendertraceInfo << "Checkerboarded for " << mFrameCount << " frames ("
                   << (mEndTime - mStartTime).ToMilliseconds() << " ms), "
                   << mPeakPixels << " peak, " << GetSeverity() << " severity."
                   << '\n';
}

bool CheckerboardEvent::PropertyValue::operator<(
    const PropertyValue& aOther) const {
  if (mTimeStamp < aOther.mTimeStamp) {
    return true;
  } else if (mTimeStamp > aOther.mTimeStamp) {
    return false;
  }
  return mProperty < aOther.mProperty;
}

CheckerboardEvent::PropertyBuffer::PropertyBuffer() : mIndex(0) {}

void CheckerboardEvent::PropertyBuffer::Update(
    RendertraceProperty aProperty, const CSSRect& aRect,
    const std::string& aExtraInfo, const MonitorAutoLock& aProofOfLock) {
  mValues[mIndex] = {aProperty, TimeStamp::Now(), aRect, aExtraInfo};
  mIndex = (mIndex + 1) % BUFFER_SIZE;
}

void CheckerboardEvent::PropertyBuffer::Flush(
    std::vector<PropertyValue>& aOut, const MonitorAutoLock& aProofOfLock) {
  for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
    uint32_t ix = (mIndex + i) % BUFFER_SIZE;
    if (!mValues[ix].mTimeStamp.IsNull()) {
      aOut.push_back(mValues[ix]);
      mValues[ix].mTimeStamp = TimeStamp();
    }
  }
}

}  // namespace layers
}  // namespace mozilla
