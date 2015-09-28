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

#include "MainThreadUtils.h"
#include "nsTArray.h"
#include "math.h"
#include "WebAudioUtils.h"

namespace mozilla {

class MediaStream;

namespace dom {

struct AudioTimelineEvent final
{
  enum Type : uint32_t
  {
    SetValue,
    SetValueAtTime,
    LinearRamp,
    ExponentialRamp,
    SetTarget,
    SetValueCurve,
    Stream,
    Cancel
  };

  AudioTimelineEvent(Type aType, double aTime, float aValue, double aTimeConstant = 0.0,
                     float aDuration = 0.0, const float* aCurve = nullptr,
                     uint32_t aCurveLength = 0)
    : mType(aType)
    , mTimeConstant(aTimeConstant)
    , mDuration(aDuration)
#ifdef DEBUG
    , mTimeIsInTicks(false)
#endif
  {
    mTime = aTime;
    if (aType == AudioTimelineEvent::SetValueCurve) {
      SetCurveParams(aCurve, aCurveLength);
    } else {
      mValue = aValue;
    }
  }

  explicit AudioTimelineEvent(MediaStream* aStream)
    : mType(Stream)
    , mStream(aStream)
#ifdef DEBUG
    , mTimeIsInTicks(false)
#endif
  {
  }

  AudioTimelineEvent(const AudioTimelineEvent& rhs)
  {
    PodCopy(this, &rhs, 1);

    if (rhs.mType == AudioTimelineEvent::SetValueCurve) {
      SetCurveParams(rhs.mCurve, rhs.mCurveLength);
    } else if (rhs.mType == AudioTimelineEvent::Stream) {
      new (&mStream) decltype(mStream)(rhs.mStream);
    }
  }

  ~AudioTimelineEvent()
  {
    if (mType == AudioTimelineEvent::SetValueCurve) {
      delete[] mCurve;
    }
  }

  template <class TimeType>
  TimeType Time() const;

  void SetTimeInTicks(int64_t aTimeInTicks)
  {
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
  nsRefPtr<MediaStream> mStream;
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
inline double AudioTimelineEvent::Time<double>() const
{
  MOZ_ASSERT(!mTimeIsInTicks);
  return mTime;
}

template <>
inline int64_t AudioTimelineEvent::Time<int64_t>() const
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(mTimeIsInTicks);
  return mTimeInTicks;
}

/**
 * This class will be instantiated with different template arguments for testing and
 * production code.
 *
 * ErrorResult is a type which satisfies the following:
 *  - Implements a Throw() method taking an nsresult argument, representing an error code.
 */
template <class ErrorResult>
class AudioEventTimeline
{
public:
  explicit AudioEventTimeline(float aDefaultValue)
    : mValue(aDefaultValue),
      mComputedValue(aDefaultValue),
      mLastComputedValue(aDefaultValue)
  { }

  bool ValidateEvent(AudioTimelineEvent& aEvent,
                     ErrorResult& aRv)
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Validate the event itself
    if (!WebAudioUtils::IsTimeValid(aEvent.template Time<double>()) ||
        !WebAudioUtils::IsTimeValid(aEvent.mTimeConstant)) {
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return false;
    }

    if (aEvent.mType == AudioTimelineEvent::SetValueCurve) {
      if (!aEvent.mCurve || !aEvent.mCurveLength) {
        aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
        return false;
      }
      for (uint32_t i = 0; i < aEvent.mCurveLength; ++i) {
        if (!IsValid(aEvent.mCurve[i])) {
          aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
          return false;
        }
      }
    }

    if (aEvent.mType == AudioTimelineEvent::SetTarget &&
        WebAudioUtils::FuzzyEqual(aEvent.mTimeConstant, 0.0)) {
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return false;
    }

    bool timeAndValueValid = IsValid(aEvent.mValue) &&
                             IsValid(aEvent.mDuration);
    if (!timeAndValueValid) {
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return false;
    }

    // Make sure that non-curve events don't fall within the duration of a
    // curve event.
    for (unsigned i = 0; i < mEvents.Length(); ++i) {
      if (mEvents[i].mType == AudioTimelineEvent::SetValueCurve &&
          mEvents[i].template Time<double>() <= aEvent.template Time<double>() &&
          (mEvents[i].template Time<double>() + mEvents[i].mDuration) >= aEvent.template Time<double>()) {
        aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
        return false;
      }
    }

    // Make sure that curve events don't fall in a range which includes other
    // events.
    if (aEvent.mType == AudioTimelineEvent::SetValueCurve) {
      for (unsigned i = 0; i < mEvents.Length(); ++i) {
        if (mEvents[i].template Time<double>() > aEvent.template Time<double>() &&
            mEvents[i].template Time<double>() < (aEvent.template Time<double>() + aEvent.mDuration)) {
          aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
          return false;
        }
      }
    }

    // Make sure that invalid values are not used for exponential curves
    if (aEvent.mType == AudioTimelineEvent::ExponentialRamp) {
      if (aEvent.mValue <= 0.f) {
        aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
        return false;
      }
      const AudioTimelineEvent* previousEvent = GetPreviousEvent(aEvent.template Time<double>());
      if (previousEvent) {
        if (previousEvent->mValue <= 0.f) {
          aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
          return false;
        }
      } else {
        if (mValue <= 0.f) {
          aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
          return false;
        }
      }
    }
    return true;
  }

  template<typename TimeType>
  void InsertEvent(const AudioTimelineEvent& aEvent)
  {
    for (unsigned i = 0; i < mEvents.Length(); ++i) {
      if (aEvent.template Time<TimeType>() == mEvents[i].template Time<TimeType>()) {
        if (aEvent.mType == mEvents[i].mType) {
          // If times and types are equal, replace the event
          mEvents.ReplaceElementAt(i, aEvent);
        } else {
          // Otherwise, place the element after the last event of another type
          do {
            ++i;
          } while (i < mEvents.Length() &&
                   aEvent.mType != mEvents[i].mType &&
                   aEvent.template Time<TimeType>() == mEvents[i].template Time<TimeType>());
          mEvents.InsertElementAt(i, aEvent);
        }
        return;
      }
      // Otherwise, place the event right after the latest existing event
      if (aEvent.template Time<TimeType>() < mEvents[i].template Time<TimeType>()) {
        mEvents.InsertElementAt(i, aEvent);
        return;
      }
    }

    // If we couldn't find a place for the event, just append it to the list
    mEvents.AppendElement(aEvent);
  }

  bool HasSimpleValue() const
  {
    return mEvents.IsEmpty();
  }

  float GetValue() const
  {
    // This method should only be called if HasSimpleValue() returns true
    MOZ_ASSERT(HasSimpleValue());
    return mValue;
  }

  float Value() const
  {
    // TODO: Return the current value based on the timeline of the AudioContext
    return mValue;
  }

  void SetValue(float aValue)
  {
    // Silently don't change anything if there are any events
    if (mEvents.IsEmpty()) {
      mLastComputedValue = mComputedValue = mValue = aValue;
    }
  }

  void SetValueAtTime(float aValue, double aStartTime, ErrorResult& aRv)
  {
    AudioTimelineEvent event(AudioTimelineEvent::SetValueAtTime, aStartTime, aValue);

    if (ValidateEvent(event, aRv)) {
      InsertEvent<double>(event);
    }
  }

  void LinearRampToValueAtTime(float aValue, double aEndTime, ErrorResult& aRv)
  {
    AudioTimelineEvent event(AudioTimelineEvent::LinearRamp, aEndTime, aValue);

    if (ValidateEvent(event, aRv)) {
      InsertEvent<double>(event);
    }
  }

  void ExponentialRampToValueAtTime(float aValue, double aEndTime, ErrorResult& aRv)
  {
    AudioTimelineEvent event(AudioTimelineEvent::ExponentialRamp, aEndTime, aValue);

    if (ValidateEvent(event, aRv)) {
      InsertEvent<double>(event);
    }
  }

  void SetTargetAtTime(float aTarget, double aStartTime, double aTimeConstant, ErrorResult& aRv)
  {
    AudioTimelineEvent event(AudioTimelineEvent::SetTarget, aStartTime, aTarget, aTimeConstant);

    if (ValidateEvent(event, aRv)) {
      InsertEvent<double>(event);
    }
  }

  void SetValueCurveAtTime(const float* aValues, uint32_t aValuesLength, double aStartTime, double aDuration, ErrorResult& aRv)
  {
    AudioTimelineEvent event(AudioTimelineEvent::SetValueCurve, aStartTime, 0.0f, 0.0f, aDuration, aValues, aValuesLength);
    if (ValidateEvent(event, aRv)) {
      InsertEvent<double>(event);
    }
  }

  template<typename TimeType>
  void CancelScheduledValues(TimeType aStartTime)
  {
    for (unsigned i = 0; i < mEvents.Length(); ++i) {
      if (mEvents[i].template Time<TimeType>() >= aStartTime) {
#ifdef DEBUG
        // Sanity check: the array should be sorted, so all of the following
        // events should have a time greater than aStartTime too.
        for (unsigned j = i + 1; j < mEvents.Length(); ++j) {
          MOZ_ASSERT(mEvents[j].template Time<TimeType>() >= aStartTime);
        }
#endif
        mEvents.TruncateLength(i);
        break;
      }
    }
  }

  void CancelAllEvents()
  {
    mEvents.Clear();
  }

  static bool TimesEqual(int64_t aLhs, int64_t aRhs)
  {
    return aLhs == aRhs;
  }

  // Since we are going to accumulate error by adding 0.01 multiple time in a
  // loop, we want to fuzz the equality check in GetValueAtTime.
  static bool TimesEqual(double aLhs, double aRhs)
  {
    const float kEpsilon = 0.0000000001f;
    return fabs(aLhs - aRhs) < kEpsilon;
  }

  template<class TimeType>
  float GetValueAtTime(TimeType aTime)
  {
    GetValuesAtTimeHelper(aTime, &mComputedValue, 1);
    return mComputedValue;
  }

  template<class TimeType>
  void GetValuesAtTime(TimeType aTime, float* aBuffer, const size_t aSize)
  {
    MOZ_ASSERT(aBuffer);
    GetValuesAtTimeHelper(aTime, aBuffer, aSize);
    mComputedValue = aBuffer[aSize - 1];
  }

  // This method computes the AudioParam value at a given time based on the event timeline
  template<class TimeType>
  void GetValuesAtTimeHelper(TimeType aTime, float* aBuffer, const size_t aSize)
  {
    MOZ_ASSERT(aBuffer);
    MOZ_ASSERT(aSize);

    size_t lastEventId = 0;
    const AudioTimelineEvent* previous = nullptr;
    const AudioTimelineEvent* next = nullptr;
    bool bailOut = false;

    // Let's remove old events except the last one: we need it to calculate some curves.
    while (mEvents.Length() > 1 &&
           aTime > mEvents[1].template Time<TimeType>()) {
      mEvents.RemoveElementAt(0);
    }

    for (size_t bufferIndex = 0; bufferIndex < aSize; ++bufferIndex, ++aTime) {
      for (; !bailOut && lastEventId < mEvents.Length(); ++lastEventId) {

#ifdef DEBUG
        const AudioTimelineEvent* current = &mEvents[lastEventId];
        MOZ_ASSERT(current->mType == AudioTimelineEvent::SetValueAtTime ||
                   current->mType == AudioTimelineEvent::SetTarget ||
                   current->mType == AudioTimelineEvent::LinearRamp ||
                   current->mType == AudioTimelineEvent::ExponentialRamp ||
                   current->mType == AudioTimelineEvent::SetValueCurve);
#endif

        if (TimesEqual(aTime, mEvents[lastEventId].template Time<TimeType>())) {
          mLastComputedValue = mComputedValue;
          // Find the last event with the same time
          while (lastEventId < mEvents.Length() - 1 &&
                 TimesEqual(aTime, mEvents[lastEventId + 1].template Time<TimeType>())) {
            ++lastEventId;
          }
          break;
        }

        previous = next;
        next = &mEvents[lastEventId];
        if (aTime < mEvents[lastEventId].template Time<TimeType>()) {
          bailOut = true;
        }
      }

      // The time was found in the list of events.
      if (!bailOut && lastEventId < mEvents.Length()) {
        MOZ_ASSERT(TimesEqual(aTime, mEvents[lastEventId].template Time<TimeType>()));

        // SetTarget nodes can be handled no matter what their next node is (if they have one)
        if (mEvents[lastEventId].mType == AudioTimelineEvent::SetTarget) {
          // Follow the curve, without regard to the next event, starting at
          // the last value of the last event.
          aBuffer[bufferIndex] = ExponentialApproach(mEvents[lastEventId].template Time<TimeType>(),
                                                  mLastComputedValue, mEvents[lastEventId].mValue,
                                                  mEvents[lastEventId].mTimeConstant, aTime);
          continue;
        }

        // SetValueCurve events can be handled no matter what their event node is (if they have one)
        if (mEvents[lastEventId].mType == AudioTimelineEvent::SetValueCurve) {
          aBuffer[bufferIndex] = ExtractValueFromCurve(mEvents[lastEventId].template Time<TimeType>(),
                                                    mEvents[lastEventId].mCurve,
                                                    mEvents[lastEventId].mCurveLength,
                                                    mEvents[lastEventId].mDuration, aTime);
          continue;
        }

        // For other event types
        aBuffer[bufferIndex] = mEvents[lastEventId].mValue;
        continue;
      }

      // Handle the case where the time is past all of the events
      if (!bailOut) {
        aBuffer[bufferIndex] = GetValuesAtTimeHelperInternal(aTime, next, nullptr);
      } else {
        aBuffer[bufferIndex] = GetValuesAtTimeHelperInternal(aTime, previous, next);
      }
    }
  }

  // Return the number of events scheduled
  uint32_t GetEventCount() const
  {
    return mEvents.Length();
  }

  static float LinearInterpolate(double t0, float v0, double t1, float v1, double t)
  {
    return v0 + (v1 - v0) * ((t - t0) / (t1 - t0));
  }

  static float ExponentialInterpolate(double t0, float v0, double t1, float v1, double t)
  {
    return v0 * powf(v1 / v0, (t - t0) / (t1 - t0));
  }

  static float ExponentialApproach(double t0, double v0, float v1, double timeConstant, double t)
  {
    return v1 + (v0 - v1) * expf(-(t - t0) / timeConstant);
  }

  static float ExtractValueFromCurve(double startTime, float* aCurve, uint32_t aCurveLength, double duration, double t)
  {
    if (t >= startTime + duration) {
      // After the duration, return the last curve value
      return aCurve[aCurveLength - 1];
    }
    double ratio = std::max((t - startTime) / duration, 0.0);
    if (ratio >= 1.0) {
      return aCurve[aCurveLength - 1];
    }
    return aCurve[uint32_t(aCurveLength * ratio)];
  }

  template<class TimeType>
  float GetValuesAtTimeHelperInternal(TimeType aTime,
                                      const AudioTimelineEvent* aPrevious,
                                      const AudioTimelineEvent* aNext)
  {
    // If the requested time is before all of the existing events
    if (!aPrevious) {
       return mValue;
    }

    // SetTarget nodes can be handled no matter what their next node is (if
    // they have one)
    if (aPrevious->mType == AudioTimelineEvent::SetTarget) {
      return ExponentialApproach(aPrevious->template Time<TimeType>(),
                                 mLastComputedValue, aPrevious->mValue,
                                 aPrevious->mTimeConstant, aTime);
    }

    // SetValueCurve events can be handled no mattar what their next node is
    // (if they have one)
    if (aPrevious->mType == AudioTimelineEvent::SetValueCurve) {
      return ExtractValueFromCurve(aPrevious->template Time<TimeType>(),
                                   aPrevious->mCurve, aPrevious->mCurveLength,
                                   aPrevious->mDuration, aTime);
    }

    // If the requested time is after all of the existing events
    if (!aNext) {
      switch (aPrevious->mType) {
        case AudioTimelineEvent::SetValueAtTime:
        case AudioTimelineEvent::LinearRamp:
        case AudioTimelineEvent::ExponentialRamp:
          // The value will be constant after the last event
          return aPrevious->mValue;
        case AudioTimelineEvent::SetValueCurve:
          return ExtractValueFromCurve(aPrevious->template Time<TimeType>(),
                                       aPrevious->mCurve, aPrevious->mCurveLength,
                                       aPrevious->mDuration, aTime);
        case AudioTimelineEvent::SetTarget:
          MOZ_ASSERT(false, "unreached");
        case AudioTimelineEvent::SetValue:
        case AudioTimelineEvent::Cancel:
        case AudioTimelineEvent::Stream:
          MOZ_ASSERT(false, "Should have been handled earlier.");
      }
      MOZ_ASSERT(false, "unreached");
    }

    // Finally, handle the case where we have both a previous and a next event

    // First, handle the case where our range ends up in a ramp event
    switch (aNext->mType) {
    case AudioTimelineEvent::LinearRamp:
      return LinearInterpolate(aPrevious->template Time<TimeType>(),
                               aPrevious->mValue,
                               aNext->template Time<TimeType>(),
                               aNext->mValue, aTime);

    case AudioTimelineEvent::ExponentialRamp:
      return ExponentialInterpolate(aPrevious->template Time<TimeType>(),
                                    aPrevious->mValue,
                                    aNext->template Time<TimeType>(),
                                    aNext->mValue, aTime);

    case AudioTimelineEvent::SetValueAtTime:
    case AudioTimelineEvent::SetTarget:
    case AudioTimelineEvent::SetValueCurve:
      break;
    case AudioTimelineEvent::SetValue:
    case AudioTimelineEvent::Cancel:
    case AudioTimelineEvent::Stream:
      MOZ_ASSERT(false, "Should have been handled earlier.");
    }

    // Now handle all other cases
    switch (aPrevious->mType) {
    case AudioTimelineEvent::SetValueAtTime:
    case AudioTimelineEvent::LinearRamp:
    case AudioTimelineEvent::ExponentialRamp:
      // If the next event type is neither linear or exponential ramp, the
      // value is constant.
      return aPrevious->mValue;
    case AudioTimelineEvent::SetValueCurve:
      return ExtractValueFromCurve(aPrevious->template Time<TimeType>(),
                                   aPrevious->mCurve, aPrevious->mCurveLength,
                                   aPrevious->mDuration, aTime);
    case AudioTimelineEvent::SetTarget:
      MOZ_ASSERT(false, "unreached");
    case AudioTimelineEvent::SetValue:
    case AudioTimelineEvent::Cancel:
    case AudioTimelineEvent::Stream:
      MOZ_ASSERT(false, "Should have been handled earlier.");
    }

    MOZ_ASSERT(false, "unreached");
    return 0.0f;
  }

  const AudioTimelineEvent* GetPreviousEvent(double aTime) const
  {
    const AudioTimelineEvent* previous = nullptr;
    const AudioTimelineEvent* next = nullptr;

    bool bailOut = false;
    for (unsigned i = 0; !bailOut && i < mEvents.Length(); ++i) {
      switch (mEvents[i].mType) {
      case AudioTimelineEvent::SetValueAtTime:
      case AudioTimelineEvent::SetTarget:
      case AudioTimelineEvent::LinearRamp:
      case AudioTimelineEvent::ExponentialRamp:
      case AudioTimelineEvent::SetValueCurve:
        if (aTime == mEvents[i].template Time<double>()) {
          // Find the last event with the same time
          do {
            ++i;
          } while (i < mEvents.Length() &&
                   aTime == mEvents[i].template Time<double>());
          return &mEvents[i - 1];
        }
        previous = next;
        next = &mEvents[i];
        if (aTime < mEvents[i].template Time<double>()) {
          bailOut = true;
        }
        break;
      default:
        MOZ_ASSERT(false, "unreached");
      }
    }
    // Handle the case where the time is past all of the events
    if (!bailOut) {
      previous = next;
    }

    return previous;
  }
private:
  static bool IsValid(double value)
  {
    return mozilla::IsFinite(value);
  }

  // This is a sorted array of the events in the timeline.  Queries of this
  // data structure should probably be more frequent than modifications to it,
  // and that is the reason why we're using a simple array as the data structure.
  // We can optimize this in the future if the performance of the array ends up
  // being a bottleneck.
  nsTArray<AudioTimelineEvent> mEvents;
  float mValue;
  // This is the value of this AudioParam we computed at the last call.
  float mComputedValue;
  // This is the value of this AudioParam at the last tick of the previous event.
  float mLastComputedValue;
};

} // namespace dom
} // namespace mozilla

#endif
