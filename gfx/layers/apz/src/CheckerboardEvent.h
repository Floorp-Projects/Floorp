/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CheckerboardEvent_h
#define mozilla_layers_CheckerboardEvent_h

#include "mozilla/Monitor.h"
#include "mozilla/TimeStamp.h"
#include <sstream>
#include "Units.h"

namespace mozilla {
namespace layers {

/**
 * This class records information relevant to one "checkerboard event", which is
 * a contiguous set of frames where a given APZC was checkerboarding. The intent
 * of this class is to record enough information that it can provide actionable
 * steps to reduce the occurrence of checkerboarding. Furthermore, it records
 * information about the severity of the checkerboarding so as to allow
 * prioritizing the debugging of some checkerboarding events over others.
 */
class CheckerboardEvent {
public:
  enum RendertraceProperty {
    Page,
    PaintedCriticalDisplayPort,
    PaintedDisplayPort,
    RequestedDisplayPort,
    UserVisible,

    // sentinel final value
    MAX_RendertraceProperty
  };

  static const char* sDescriptions[MAX_RendertraceProperty];
  static const char* sColors[MAX_RendertraceProperty];

public:
  CheckerboardEvent();

  /**
   * Gets the "severity" of the checkerboard event. This doesn't have units,
   * it's just useful for comparing two checkerboard events to see which one
   * is worse, for some implementation-specific definition of "worse".
   */
  uint64_t GetSeverity();

  /**
   * Gets the raw log of the checkerboard event. This can be called any time,
   * although it really only makes sense to pull once the event is done, after
   * RecordFrameInfo returns true.
   */
  std::string GetLog();

  /**
   * Provide a new value for one of the rects that is tracked for
   * checkerboard events.
   */
  void UpdateRendertraceProperty(RendertraceProperty aProperty,
                                 const CSSRect& aRect,
                                 const std::string& aExtraInfo = std::string());

  /**
   * Provide the number of CSS pixels that are checkerboarded in a composite
   * at the current time.
   * @return true if the checkerboard event has completed. The caller should
   * stop updating this object once this happens.
   */
  bool RecordFrameInfo(uint32_t aCssPixelsCheckerboarded);

private:
  /**
   * Helper method to do stuff when checkeboarding starts.
   */
  void StartEvent();
  /**
   * Helper method to do stuff when checkerboarding stops.
   */
  void StopEvent();

  /**
   * Helper method to log a rendertrace property and its value to the
   * rendertrace info buffer (mRendertraceInfo).
   */
  void LogInfo(RendertraceProperty aProperty,
               const TimeStamp& aTimestamp,
               const CSSRect& aRect,
               const std::string& aExtraInfo,
               const MonitorAutoLock& aProofOfLock);

  /**
   * Helper struct that holds a single rendertrace property value.
   */
  struct PropertyValue
  {
    RendertraceProperty mProperty;
    TimeStamp mTimeStamp;
    CSSRect mRect;
    std::string mExtraInfo;

    bool operator<(const PropertyValue& aOther) const;
  };

  /**
   * A circular buffer that stores the most recent BUFFER_SIZE values of a
   * given property.
   */
  class PropertyBuffer
  {
  public:
    PropertyBuffer();
    /**
     * Add a new value to the buffer, overwriting the oldest one if needed.
     */
    void Update(RendertraceProperty aProperty, const CSSRect& aRect,
                const std::string& aExtraInfo,
                const MonitorAutoLock& aProofOfLock);
    /**
     * Dump the recorded values, oldest to newest, to the given vector, and
     * remove them from this buffer.
     */
    void Flush(std::vector<PropertyValue>& aOut,
               const MonitorAutoLock& aProofOfLock);

  private:
    static const uint32_t BUFFER_SIZE = 5;

    /**
     * The index of the oldest value in the buffer. This is the next index
     * that will be written to.
     */
    uint32_t mIndex;
    PropertyValue mValues[BUFFER_SIZE];
  };

private:
  /**
   * A base time so that the other timestamps can be turned into durations.
   */
  const TimeStamp mOriginTime;
  /**
   * Whether or not a checkerboard event is currently occurring.
   */
  bool mCheckerboardingActive;

  /**
   * The start time of the checkerboard event.
   */
  TimeStamp mStartTime;
  /**
   * The end time of the checkerboard event.
   */
  TimeStamp mEndTime;
  /**
   * The sample time of the last frame recorded.
   */
  TimeStamp mLastSampleTime;
  /**
   * The number of contiguous frames with checkerboard.
   */
  uint32_t mFrameCount;
  /**
   * The total number of pixel-milliseconds of checkerboarding visible to
   * the user during the checkerboarding event.
   */
  uint64_t mTotalPixelMs;
  /**
   * The largest number of pixels of checkerboarding visible to the user
   * during any one frame, during this checkerboarding event.
   */
  uint32_t mPeakPixels;

  /**
   * Monitor that needs to be acquired before touching mBufferedProperties
   * or mRendertraceInfo.
   */
  mutable Monitor mRendertraceLock;
  /**
   * A circular buffer to store some properties. This is used before the
   * checkerboarding actually starts, so that we have some data on what
   * was happening before the checkerboarding started.
   */
  PropertyBuffer mBufferedProperties[MAX_RendertraceProperty];
  /**
   * The rendertrace info buffer that gives us info on what was happening
   * during the checkerboard event.
   */
  std::ostringstream mRendertraceInfo;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_CheckerboardEvent_h
