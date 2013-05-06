/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioEventTimeline_h_
#define AudioEventTimeline_h_

#include "mozilla/Assertions.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/TypedEnum.h"

#include "nsTArray.h"
#include "math.h"

namespace mozilla {

namespace dom {

// This is an internal helper class and should not be used outside of this header.
struct AudioTimelineEvent {
  enum Type MOZ_ENUM_TYPE(uint32_t) {
    SetValue,
    LinearRamp,
    ExponentialRamp,
    SetTarget,
    SetValueCurve
  };

  AudioTimelineEvent(Type aType, double aTime, float aValue, double aTimeConstant = 0.0,
                     float aDuration = 0.0, float* aCurve = nullptr, uint32_t aCurveLength = 0)
    : mType(aType)
    , mTimeConstant(aTimeConstant)
    , mDuration(aDuration)
#ifdef DEBUG
    , mTimeIsInTicks(false)
#endif
  {
    if (aType == AudioTimelineEvent::SetValueCurve) {
      mCurve = aCurve;
      mCurveLength = aCurveLength;
    } else {
      mValue = aValue;
      mTime = aTime;
    }
  }

  bool IsValid() const
  {
    return IsValid(mTime) &&
           IsValid(mValue) &&
           IsValid(mTimeConstant) &&
           IsValid(mDuration);
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

  Type mType;
  union {
    float mValue;
    uint32_t mCurveLength;
  };
  union {
    // The time for an event can either be in absolute value or in ticks.
    // Initially the time of the event is always in absolute value.
    // In order to convert it to ticks, call SetTimeInTicks.  Once this
    // method has been called for an event, the time cannot be converted
    // back to absolute value.
    union {
      double mTime;
      int64_t mTimeInTicks;
    };
    float* mCurve;
  };
  double mTimeConstant;
  double mDuration;
#ifdef DEBUG
  bool mTimeIsInTicks;
#endif

private:
  static bool IsValid(double value)
  {
    return MOZ_DOUBLE_IS_FINITE(value);
  }
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
    : mValue(aDefaultValue)
  {
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
      mValue = aValue;
    }
  }

  float ComputedValue() const
  {
    // TODO: implement
    return 0;
  }

  void SetValueAtTime(float aValue, double aStartTime, ErrorResult& aRv)
  {
    InsertEvent(AudioTimelineEvent(AudioTimelineEvent::SetValue, aStartTime, aValue), aRv);
  }

  void LinearRampToValueAtTime(float aValue, double aEndTime, ErrorResult& aRv)
  {
    InsertEvent(AudioTimelineEvent(AudioTimelineEvent::LinearRamp, aEndTime, aValue), aRv);
  }

  void ExponentialRampToValueAtTime(float aValue, double aEndTime, ErrorResult& aRv)
  {
    InsertEvent(AudioTimelineEvent(AudioTimelineEvent::ExponentialRamp, aEndTime, aValue), aRv);
  }

  void SetTargetAtTime(float aTarget, double aStartTime, double aTimeConstant, ErrorResult& aRv)
  {
    InsertEvent(AudioTimelineEvent(AudioTimelineEvent::SetTarget, aStartTime, aTarget, aTimeConstant), aRv);
  }

  void SetValueCurveAtTime(const float* aValues, uint32_t aValuesLength, double aStartTime, double aDuration, ErrorResult& aRv)
  {
    // TODO: implement
    // Note that we will need to copy the buffer here.
    // InsertEvent(AudioTimelineEvent(AudioTimelineEvent::SetValueCurve, aStartTime, 0.0f, 0.0f, aDuration, aValues, aValuesLength), aRv);
  }

  void CancelScheduledValues(double aStartTime)
  {
    for (unsigned i = 0; i < mEvents.Length(); ++i) {
      if (mEvents[i].mTime >= aStartTime) {
#ifdef DEBUG
        // Sanity check: the array should be sorted, so all of the following
        // events should have a time greater than aStartTime too.
        for (unsigned j = i + 1; j < mEvents.Length(); ++j) {
          MOZ_ASSERT(mEvents[j].mTime >= aStartTime);
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

  // This method computes the AudioParam value at a given time based on the event timeline
  template<class TimeType>
  float GetValueAtTime(TimeType aTime) const
  {
    const AudioTimelineEvent* previous = nullptr;
    const AudioTimelineEvent* next = nullptr;

    bool bailOut = false;
    for (unsigned i = 0; !bailOut && i < mEvents.Length(); ++i) {
      switch (mEvents[i].mType) {
      case AudioTimelineEvent::SetValue:
      case AudioTimelineEvent::SetTarget:
      case AudioTimelineEvent::LinearRamp:
      case AudioTimelineEvent::ExponentialRamp:
        if (aTime == mEvents[i].template Time<TimeType>()) {
          // Find the last event with the same time
          do {
            ++i;
          } while (i < mEvents.Length() &&
                   aTime == mEvents[i].template Time<TimeType>());

          // SetTarget nodes can be handled no matter what their next node is (if they have one)
          if (mEvents[i - 1].mType == AudioTimelineEvent::SetTarget) {
            // Follow the curve, without regard to the next node
            return ExponentialApproach(mEvents[i - 1].template Time<TimeType>(),
                                       mValue, mEvents[i - 1].mValue,
                                       mEvents[i - 1].mTimeConstant, aTime);
          }

          // For other event types
          return mEvents[i - 1].mValue;
        }
        previous = next;
        next = &mEvents[i];
        if (aTime < mEvents[i].template Time<TimeType>()) {
          bailOut = true;
        }
        break;
      case AudioTimelineEvent::SetValueCurve:
        // TODO: implement
        break;
      default:
        MOZ_ASSERT(false, "unreached");
      }
    }
    // Handle the case where the time is past all of the events
    if (!bailOut) {
      previous = next;
      next = nullptr;
    }

    // Just return the default value if we did not find anything
    if (!previous && !next) {
      return mValue;
    }

    // If the requested time is before all of the existing events
    if (!previous) {
      return mValue;
    }

    // SetTarget nodes can be handled no matter what their next node is (if they have one)
    if (previous->mType == AudioTimelineEvent::SetTarget) {
      // Follow the curve, without regard to the next node
      return ExponentialApproach(previous->template Time<TimeType>(), mValue, previous->mValue,
                                 previous->mTimeConstant, aTime);
    }

    // If the requested time is after all of the existing events
    if (!next) {
      switch (previous->mType) {
      case AudioTimelineEvent::SetValue:
      case AudioTimelineEvent::LinearRamp:
      case AudioTimelineEvent::ExponentialRamp:
        // The value will be constant after the last event
        return previous->mValue;
      case AudioTimelineEvent::SetValueCurve:
        // TODO: implement
        return 0.0f;
      case AudioTimelineEvent::SetTarget:
        MOZ_ASSERT(false, "unreached");
      }
      MOZ_ASSERT(false, "unreached");
    }

    // Finally, handle the case where we have both a previous and a next event

    // First, handle the case where our range ends up in a ramp event
    switch (next->mType) {
    case AudioTimelineEvent::LinearRamp:
      return LinearInterpolate(previous->template Time<TimeType>(), previous->mValue, next->template Time<TimeType>(), next->mValue, aTime);
    case AudioTimelineEvent::ExponentialRamp:
      return ExponentialInterpolate(previous->template Time<TimeType>(), previous->mValue, next->template Time<TimeType>(), next->mValue, aTime);
    case AudioTimelineEvent::SetValue:
    case AudioTimelineEvent::SetTarget:
    case AudioTimelineEvent::SetValueCurve:
      break;
    }

    // Now handle all other cases
    switch (previous->mType) {
    case AudioTimelineEvent::SetValue:
    case AudioTimelineEvent::LinearRamp:
    case AudioTimelineEvent::ExponentialRamp:
      // If the next event type is neither linear or exponential ramp, the
      // value is constant.
      return previous->mValue;
    case AudioTimelineEvent::SetValueCurve:
      // TODO: implement
      return 0.0f;
    case AudioTimelineEvent::SetTarget:
      MOZ_ASSERT(false, "unreached");
    }

    MOZ_ASSERT(false, "unreached");
    return 0.0f;
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

  void ConvertEventTimesToTicks(int64_t (*aConvertor)(double aTime, void* aClosure), void* aClosure,
                                int32_t aSampleRate)
  {
    for (unsigned i = 0; i < mEvents.Length(); ++i) {
      mEvents[i].SetTimeInTicks(aConvertor(mEvents[i].template Time<double>(), aClosure));
      mEvents[i].mTimeConstant *= aSampleRate;
      mEvents[i].mDuration *= aSampleRate;
    }
  }

private:
  const AudioTimelineEvent* GetPreviousEvent(double aTime) const
  {
    const AudioTimelineEvent* previous = nullptr;
    const AudioTimelineEvent* next = nullptr;

    bool bailOut = false;
    for (unsigned i = 0; !bailOut && i < mEvents.Length(); ++i) {
      switch (mEvents[i].mType) {
      case AudioTimelineEvent::SetValue:
      case AudioTimelineEvent::SetTarget:
      case AudioTimelineEvent::LinearRamp:
      case AudioTimelineEvent::ExponentialRamp:
        if (aTime == mEvents[i].mTime) {
          // Find the last event with the same time
          do {
            ++i;
          } while (i < mEvents.Length() &&
                   aTime == mEvents[i].mTime);
          return &mEvents[i - 1];
        }
        previous = next;
        next = &mEvents[i];
        if (aTime < mEvents[i].mTime) {
          bailOut = true;
        }
        break;
      case AudioTimelineEvent::SetValueCurve:
        // TODO: implement
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

  void InsertEvent(const AudioTimelineEvent& aEvent, ErrorResult& aRv)
  {
    if (!aEvent.IsValid()) {
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return;
    }

    // Make sure that non-curve events don't fall within the duration of a
    // curve event.
    for (unsigned i = 0; i < mEvents.Length(); ++i) {
      if (mEvents[i].mType == AudioTimelineEvent::SetValueCurve &&
          mEvents[i].mTime <= aEvent.mTime &&
          (mEvents[i].mTime + mEvents[i].mDuration) >= aEvent.mTime) {
        aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
        return;
      }
    }

    // Make sure that curve events don't fall in a range which includes other
    // events.
    if (aEvent.mType == AudioTimelineEvent::SetValueCurve) {
      for (unsigned i = 0; i < mEvents.Length(); ++i) {
        if (mEvents[i].mTime >= aEvent.mTime &&
            mEvents[i].mTime <= (aEvent.mTime + aEvent.mDuration)) {
          aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
          return;
        }
      }
    }

    // Make sure that invalid values are not used for exponential curves
    if (aEvent.mType == AudioTimelineEvent::ExponentialRamp) {
      if (aEvent.mValue <= 0.f) {
        aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
        return;
      }
      const AudioTimelineEvent* previousEvent = GetPreviousEvent(aEvent.mTime);
      if (previousEvent) {
        if (previousEvent->mValue <= 0.f) {
          aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
          return;
        }
      } else {
        if (mValue <= 0.f) {
          aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
          return;
        }
      }
    }

    for (unsigned i = 0; i < mEvents.Length(); ++i) {
      if (aEvent.mTime == mEvents[i].mTime) {
        if (aEvent.mType == mEvents[i].mType) {
          // If times and types are equal, replace the event
          mEvents.ReplaceElementAt(i, aEvent);
        } else {
          // Otherwise, place the element after the last event of another type
          do {
            ++i;
          } while (i < mEvents.Length() &&
                   aEvent.mType != mEvents[i].mType &&
                   aEvent.mTime == mEvents[i].mTime);
          mEvents.InsertElementAt(i, aEvent);
        }
        return;
      }
      // Otherwise, place the event right after the latest existing event
      if (aEvent.mTime < mEvents[i].mTime) {
        mEvents.InsertElementAt(i, aEvent);
        return;
      }
    }

    // If we couldn't find a place for the event, just append it to the list
    mEvents.AppendElement(aEvent);
  }

private:
  // This is a sorted array of the events in the timeline.  Queries of this
  // data structure should probably be more frequent than modifications to it,
  // and that is the reason why we're using a simple array as the data structure.
  // We can optimize this in the future if the performance of the array ends up
  // being a bottleneck.
  nsTArray<AudioTimelineEvent> mEvents;
  float mValue;
};

}
}

#endif

