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

  class TimeUnion {
   public:
    // double 0.0 is bit-identical to int64_t 0.
    TimeUnion()
        : mSeconds()
#if DEBUG
          ,
          mIsInSeconds(true),
          mIsInTicks(true)
#endif
    {
    }
    explicit TimeUnion(double aTime)
        : mSeconds(aTime)
#if DEBUG
          ,
          mIsInSeconds(true),
          mIsInTicks(false)
#endif
    {
    }
    explicit TimeUnion(int64_t aTime)
        : mTicks(aTime)
#if DEBUG
          ,
          mIsInSeconds(false),
          mIsInTicks(true)
#endif
    {
    }

    double operator=(double aTime) {
#if DEBUG
      mIsInSeconds = true;
      mIsInTicks = true;
#endif
      return mSeconds = aTime;
    }
    int64_t operator=(int64_t aTime) {
#if DEBUG
      mIsInSeconds = true;
      mIsInTicks = true;
#endif
      return mTicks = aTime;
    }

    template <class TimeType>
    TimeType Get() const;

   private:
    union {
      double mSeconds;
      int64_t mTicks;
    };
#ifdef DEBUG
    bool mIsInSeconds;
    bool mIsInTicks;
#endif
  };

  AudioTimelineEvent(Type aType, double aTime, float aValue,
                     double aTimeConstant = 0.0, double aDuration = 0.0,
                     const float* aCurve = nullptr, uint32_t aCurveLength = 0);
  explicit AudioTimelineEvent(AudioNodeTrack* aTrack);
  AudioTimelineEvent(const AudioTimelineEvent& rhs);
  ~AudioTimelineEvent();

  template <class TimeType>
  TimeType Time() const {
    return mTime.Get<TimeType>();
  }
  // If this event is a curve event, this returns the end time of the curve.
  // Otherwise, this returns the time of the event.
  template <class TimeType>
  double EndTime() const;
  // Value for an event, or for a ValueCurve event, this is the value of the
  // last element of the curve.
  float EndValue() const;

  void SetTimeInTicks(int64_t aTimeInTicks) { mTime = aTimeInTicks; }

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

 private:
  // This member is accessed using the `Time` method.
  //
  // The time for an event can either be in seconds or in ticks.
  // Initially the time of the event is always in seconds.
  // In order to convert it to ticks, call SetTimeInTicks.  Once this
  // method has been called for an event, the time cannot be converted
  // back to seconds.
  TimeUnion mTime;
};

template <>
inline double AudioTimelineEvent::TimeUnion::Get<double>() const {
  MOZ_ASSERT(mIsInSeconds);
  return mSeconds;
}
template <>
inline int64_t AudioTimelineEvent::TimeUnion::Get<int64_t>() const {
  MOZ_ASSERT(mIsInTicks);
  return mTicks;
}

class AudioEventTimeline {
 public:
  explicit AudioEventTimeline(float aDefaultValue)
      : mDefaultValue(aDefaultValue),
        mSetTargetStartValue(aDefaultValue),
        mSimpleValue(Some(aDefaultValue)) {}

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
        if (mDefaultValue <= 0.f) {
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
    mSimpleValue.reset();
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

  bool HasSimpleValue() const { return mSimpleValue.isSome(); }

  float GetValue() const {
    // This method should only be called if HasSimpleValue() returns true
    MOZ_ASSERT(HasSimpleValue());
    return mSimpleValue.value();
  }

  void SetValue(float aValue) {
    // FIXME: bug 1308435
    // A spec change means this should instead behave like setValueAtTime().

    // Silently don't change anything if there are any events
    if (mEvents.IsEmpty()) {
      mSetTargetStartValue = mDefaultValue = aValue;
      mSimpleValue = Some(aValue);
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
    if (mEvents.IsEmpty()) {
      mSimpleValue = Some(mDefaultValue);
    }
  }

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

  void GetValuesAtTime(int64_t aTime, float* aBuffer, const size_t aSize) {
    MOZ_ASSERT(aBuffer);
    GetValuesAtTimeHelper(aTime, aBuffer, aSize);
  }
  void GetValuesAtTime(double aTime, float* aBuffer,
                       const size_t aSize) = delete;

  // Return the number of events scheduled
  uint32_t GetEventCount() const { return mEvents.Length(); }

  template <class TimeType>
  void CleanupEventsOlderThan(TimeType aTime);

 private:
  template <class TimeType>
  void GetValuesAtTimeHelper(TimeType aTime, float* aBuffer,
                             const size_t aSize);

  template <class TimeType>
  float GetValueAtTimeOfEvent(const AudioTimelineEvent* aEvent,
                              const AudioTimelineEvent* aPrevious);

  template <class TimeType>
  float GetValuesAtTimeHelperInternal(TimeType aTime,
                                      const AudioTimelineEvent* aPrevious,
                                      const AudioTimelineEvent* aNext);

  const AudioTimelineEvent* GetPreviousEvent(double aTime) const;

  static bool IsValid(double value) { return std::isfinite(value); }

  template <class TimeType>
  float ComputeSetTargetStartValue(const AudioTimelineEvent* aPreviousEvent,
                                   TimeType aTime);

  // This is a sorted array of the events in the timeline.  Queries of this
  // data structure should probably be more frequent than modifications to it,
  // and that is the reason why we're using a simple array as the data
  // structure. We can optimize this in the future if the performance of the
  // array ends up being a bottleneck.
  nsTArray<AudioTimelineEvent> mEvents;
  float mDefaultValue;
  // This is the value of this AudioParam at the end of the previous
  // event for SetTarget curves.
  float mSetTargetStartValue;
  AudioTimelineEvent::TimeUnion mSetTargetStartTime;
  Maybe<float> mSimpleValue;
};

}  // namespace dom
}  // namespace mozilla

#endif
