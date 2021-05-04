/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioEventTimeline_h_
#define AudioEventTimeline_h_

#include <algorithm>
#include "mozilla/Assertions.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/PodOperations.h"
#include "mozilla/ErrorResult.h"

#include "MainThreadUtils.h"
#include "nsTArray.h"
#include "math.h"
#include "WebAudioUtils.h"

// XXX Avoid including this here by moving function bodies to the cpp file
#include "js/GCAPI.h"

namespace mozilla {

class AudioNodeTrack;

namespace dom {

struct AudioTimelineEvent final {
  enum Type : uint32_t {
    SetValue,
    SetValueAtTime,
    LinearRamp,
    ExponentialRamp,
    SetTarget,
    SetValueCurve,
    Track,
    Cancel
  };

  AudioTimelineEvent(Type aType, double aTime, float aValue,
                     double aTimeConstant = 0.0, double aDuration = 0.0,
                     const float* aCurve = nullptr, uint32_t aCurveLength = 0);
  explicit AudioTimelineEvent(AudioNodeTrack* aTrack);
  AudioTimelineEvent(const AudioTimelineEvent& rhs);
  ~AudioTimelineEvent();

  template <class TimeType>
  TimeType Time() const;

  void SetTimeInTicks(int64_t aTimeInTicks) {
    mTimeInTicks = aTimeInTicks;
#ifdef DEBUG
    mTimeIsInTicks = true;
#endif
  }

  void SetCurveParams(const float* aCurve, uint32_t aCurveLength) {
    mCurveLength = aCurveLength;
    if (aCurveLength) {
      mCurve = new float[aCurveLength];
      PodCopy(mCurve, aCurve, aCurveLength);
    } else {
      mCurve = nullptr;
    }
  }

  Type mType;
  union {
    float mValue;
    uint32_t mCurveLength;
  };
  // mCurve contains a buffer of SetValueCurve samples.  We sample the
  // values in the buffer depending on how far along we are in time.
  // If we're at time T and the event has started as time T0 and has a
  // duration of D, we sample the buffer at floor(mCurveLength*(T-T0)/D)
  // if T<T0+D, and just take the last sample in the buffer otherwise.
  float* mCurve;
  RefPtr<AudioNodeTrack> mTrack;
  double mTimeConstant;
  double mDuration;
#ifdef DEBUG
  bool mTimeIsInTicks;
#endif

 private:
  // This member is accessed using the `Time` method, for safety.
  //
  // The time for an event can either be in absolute value or in ticks.
  // Initially the time of the event is always in absolute value.
  // In order to convert it to ticks, call SetTimeInTicks.  Once this
  // method has been called for an event, the time cannot be converted
  // back to absolute value.
  union {
    double mTime;
    int64_t mTimeInTicks;
  };
};

template <>
inline double AudioTimelineEvent::Time<double>() const {
  MOZ_ASSERT(!mTimeIsInTicks);
  return mTime;
}

template <>
inline int64_t AudioTimelineEvent::Time<int64_t>() const {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(mTimeIsInTicks);
  return mTimeInTicks;
}

class AudioEventTimeline {
 public:
  explicit AudioEventTimeline(float aDefaultValue)
      : mValue(aDefaultValue),
        mComputedValue(aDefaultValue),
        mLastComputedValue(aDefaultValue) {}

  bool ValidateEvent(const AudioTimelineEvent& aEvent, ErrorResult& aRv) const {
    MOZ_ASSERT(NS_IsMainThread());

    auto TimeOf = [](const AudioTimelineEvent& aEvent) -> double {
      return aEvent.Time<double>();
    };

    // Validate the event itself
    if (!WebAudioUtils::IsTimeValid(TimeOf(aEvent))) {
      aRv.ThrowRangeError<MSG_INVALID_AUDIOPARAM_METHOD_START_TIME_ERROR>();
      return false;
    }
    if (!WebAudioUtils::IsTimeValid(aEvent.mTimeConstant)) {
      aRv.ThrowRangeError(
          "The exponential constant passed to setTargetAtTime must be "
          "non-negative.");
      return false;
    }

    if (aEvent.mType == AudioTimelineEvent::SetValueCurve) {
      if (!aEvent.mCurve || aEvent.mCurveLength < 2) {
        aRv.ThrowInvalidStateError("Curve length must be at least 2");
        return false;
      }
      if (aEvent.mDuration <= 0) {
        aRv.ThrowRangeError(
            "The curve duration for setValueCurveAtTime must be strictly "
            "positive.");
        return false;
      }
    }

    MOZ_ASSERT(IsValid(aEvent.mValue) && IsValid(aEvent.mDuration));

    // Make sure that new events don't fall within the duration of a
    // curve event.
    for (unsigned i = 0; i < mEvents.Length(); ++i) {
      if (mEvents[i].mType == AudioTimelineEvent::SetValueCurve &&
          TimeOf(mEvents[i]) <= TimeOf(aEvent) &&
          TimeOf(mEvents[i]) + mEvents[i].mDuration > TimeOf(aEvent)) {
        aRv.ThrowNotSupportedError("Can't add events during a curve event");
        return false;
      }
    }

    // Make sure that new curve events don't fall in a range which includes
    // other events.
    if (aEvent.mType == AudioTimelineEvent::SetValueCurve) {
      for (unsigned i = 0; i < mEvents.Length(); ++i) {
        if (TimeOf(aEvent) < TimeOf(mEvents[i]) &&
            TimeOf(aEvent) + aEvent.mDuration > TimeOf(mEvents[i])) {
          aRv.ThrowNotSupportedError(
              "Can't add curve events that overlap other events");
          return false;
        }
      }
    }

    // Make sure that invalid values are not used for exponential curves
    if (aEvent.mType == AudioTimelineEvent::ExponentialRamp) {
      if (aEvent.mValue <= 0.f) {
        aRv.ThrowRangeError(
            "The value passed to exponentialRampToValueAtTime must be "
            "positive.");
        return false;
      }
      const AudioTimelineEvent* previousEvent =
          GetPreviousEvent(TimeOf(aEvent));
      if (previousEvent) {
        if (previousEvent->mValue <= 0.f) {
          // XXXbz I see no mention of SyntaxError in the Web Audio API spec
          aRv.ThrowSyntaxError("Previous event value must be positive");
          return false;
        }
      } else {
        if (mValue <= 0.f) {
          // XXXbz I see no mention of SyntaxError in the Web Audio API spec
          aRv.ThrowSyntaxError("Our value must be positive");
          return false;
        }
      }
    }
    return true;
  }

  template <typename TimeType>
  void InsertEvent(const AudioTimelineEvent& aEvent) {
    for (unsigned i = 0; i < mEvents.Length(); ++i) {
      if (aEvent.Time<TimeType>() == mEvents[i].Time<TimeType>()) {
        // If two events happen at the same time, have them in chronological
        // order of insertion.
        do {
          ++i;
        } while (i < mEvents.Length() &&
                 aEvent.Time<TimeType>() == mEvents[i].Time<TimeType>());
        mEvents.InsertElementAt(i, aEvent);
        return;
      }
      // Otherwise, place the event right after the latest existing event
      if (aEvent.Time<TimeType>() < mEvents[i].Time<TimeType>()) {
        mEvents.InsertElementAt(i, aEvent);
        return;
      }
    }

    // If we couldn't find a place for the event, just append it to the list
    mEvents.AppendElement(aEvent);
  }

  bool HasSimpleValue() const { return mEvents.IsEmpty(); }

  float GetValue() const {
    // This method should only be called if HasSimpleValue() returns true
    MOZ_ASSERT(HasSimpleValue());
    return mValue;
  }

  void SetValue(float aValue) {
    // Silently don't change anything if there are any events
    if (mEvents.IsEmpty()) {
      mLastComputedValue = mComputedValue = mValue = aValue;
    }
  }

  void SetValueAtTime(float aValue, double aStartTime, ErrorResult& aRv) {
    AudioTimelineEvent event(AudioTimelineEvent::SetValueAtTime, aStartTime,
                             aValue);

    if (ValidateEvent(event, aRv)) {
      InsertEvent<double>(event);
    }
  }

  void LinearRampToValueAtTime(float aValue, double aEndTime,
                               ErrorResult& aRv) {
    AudioTimelineEvent event(AudioTimelineEvent::LinearRamp, aEndTime, aValue);

    if (ValidateEvent(event, aRv)) {
      InsertEvent<double>(event);
    }
  }

  void ExponentialRampToValueAtTime(float aValue, double aEndTime,
                                    ErrorResult& aRv) {
    AudioTimelineEvent event(AudioTimelineEvent::ExponentialRamp, aEndTime,
                             aValue);

    if (ValidateEvent(event, aRv)) {
      InsertEvent<double>(event);
    }
  }

  void SetTargetAtTime(float aTarget, double aStartTime, double aTimeConstant,
                       ErrorResult& aRv) {
    AudioTimelineEvent event(AudioTimelineEvent::SetTarget, aStartTime, aTarget,
                             aTimeConstant);

    if (ValidateEvent(event, aRv)) {
      InsertEvent<double>(event);
    }
  }

  void SetValueCurveAtTime(const float* aValues, uint32_t aValuesLength,
                           double aStartTime, double aDuration,
                           ErrorResult& aRv) {
    AudioTimelineEvent event(AudioTimelineEvent::SetValueCurve, aStartTime,
                             0.0f, 0.0f, aDuration, aValues, aValuesLength);
    if (ValidateEvent(event, aRv)) {
      InsertEvent<double>(event);
    }
  }

  template <typename TimeType>
  void CancelScheduledValues(TimeType aStartTime) {
    for (unsigned i = 0; i < mEvents.Length(); ++i) {
      if (mEvents[i].Time<TimeType>() >= aStartTime) {
#ifdef DEBUG
        // Sanity check: the array should be sorted, so all of the following
        // events should have a time greater than aStartTime too.
        for (unsigned j = i + 1; j < mEvents.Length(); ++j) {
          MOZ_ASSERT(mEvents[j].Time<TimeType>() >= aStartTime);
        }
#endif
        mEvents.TruncateLength(i);
        break;
      }
    }
  }

  void CancelAllEvents() { mEvents.Clear(); }

  static bool TimesEqual(int64_t aLhs, int64_t aRhs) { return aLhs == aRhs; }

  // Since we are going to accumulate error by adding 0.01 multiple time in a
  // loop, we want to fuzz the equality check in GetValueAtTime.
  static bool TimesEqual(double aLhs, double aRhs) {
    const float kEpsilon = 0.0000000001f;
    return fabs(aLhs - aRhs) < kEpsilon;
  }

  template <class TimeType>
  float GetValueAtTime(TimeType aTime) {
    float result;
    GetValuesAtTimeHelper(aTime, &result, 1);
    return result;
  }

  template <class TimeType>
  void GetValuesAtTime(TimeType aTime, float* aBuffer, const size_t aSize) {
    MOZ_ASSERT(aBuffer);
    GetValuesAtTimeHelper(aTime, aBuffer, aSize);
  }

  // Return the number of events scheduled
  uint32_t GetEventCount() const { return mEvents.Length(); }

  template <class TimeType>
  void CleanupEventsOlderThan(TimeType aTime) {
    while (mEvents.Length() > 1 && aTime > mEvents[1].Time<TimeType>()) {
      if (mEvents[1].mType == AudioTimelineEvent::SetTarget) {
        mLastComputedValue = GetValuesAtTimeHelperInternal(
            mEvents[1].Time<TimeType>(), &mEvents[0], nullptr);
      }

      MOZ_ASSERT(!mEvents[0].mTrack,
                 "AudioParam tracks should never be destroyed on the real-time "
                 "thread.");
      JS::AutoSuppressGCAnalysis suppress;
      mEvents.RemoveElementAt(0);
    }
  }

 private:
  template <class TimeType>
  void GetValuesAtTimeHelper(TimeType aTime, float* aBuffer,
                             const size_t aSize);

  template <class TimeType>
  float GetValueAtTimeOfEvent(const AudioTimelineEvent* aNext);

  template <class TimeType>
  float GetValuesAtTimeHelperInternal(TimeType aTime,
                                      const AudioTimelineEvent* aPrevious,
                                      const AudioTimelineEvent* aNext);

  const AudioTimelineEvent* GetPreviousEvent(double aTime) const;

  static bool IsValid(double value) { return mozilla::IsFinite(value); }

  // This is a sorted array of the events in the timeline.  Queries of this
  // data structure should probably be more frequent than modifications to it,
  // and that is the reason why we're using a simple array as the data
  // structure. We can optimize this in the future if the performance of the
  // array ends up being a bottleneck.
  nsTArray<AudioTimelineEvent> mEvents;
  float mValue;
  // This is the value of this AudioParam we computed at the last tick.
  float mComputedValue;
  // This is the value of this AudioParam at the last tick of the previous
  // event.
  float mLastComputedValue;
};

}  // namespace dom
}  // namespace mozilla

#endif
