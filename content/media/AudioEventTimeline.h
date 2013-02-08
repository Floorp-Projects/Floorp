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
private:
  struct Event {
    enum Type MOZ_ENUM_TYPE(uint32_t) {
      SetValue,
      LinearRamp,
      ExponentialRamp,
      SetTarget,
      SetValueCurve
    };

    Event(Type aType, double aTime, float aValue, double aTimeConstant = 0.0,
          float aDuration = 0.0, float* aCurve = nullptr, uint32_t aCurveLength = 0)
      : mType(aType)
      , mTimeConstant(aTimeConstant)
      , mDuration(aDuration)
    {
      if (aType == Event::SetValueCurve) {
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

    Type mType;
    union {
      float mValue;
      uint32_t mCurveLength;
    };
    union {
      double mTime;
      float* mCurve;
    };
    double mTimeConstant;
    double mDuration;

  private:
    static bool IsValid(double value)
    {
      return MOZ_DOUBLE_IS_FINITE(value);
    }
  };

public:
  explicit AudioEventTimeline(float aDefaultValue)
    : mValue(aDefaultValue)
  {
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
    InsertEvent(Event(Event::SetValue, aStartTime, aValue), aRv);
  }

  void LinearRampToValueAtTime(float aValue, double aEndTime, ErrorResult& aRv)
  {
    InsertEvent(Event(Event::LinearRamp, aEndTime, aValue), aRv);
  }

  void ExponentialRampToValueAtTime(float aValue, double aEndTime, ErrorResult& aRv)
  {
    InsertEvent(Event(Event::ExponentialRamp, aEndTime, aValue), aRv);
  }

  void SetTargetAtTime(float aTarget, double aStartTime, double aTimeConstant, ErrorResult& aRv)
  {
    InsertEvent(Event(Event::SetTarget, aStartTime, aTarget, aTimeConstant), aRv);
  }

  void SetValueCurveAtTime(const float* aValues, uint32_t aValuesLength, double aStartTime, double aDuration, ErrorResult& aRv)
  {
    // TODO: implement
    // Note that we will need to copy the buffer here.
    // InsertEvent(Event(Event::SetValueCurve, aStartTime, 0.0f, 0.0f, aDuration, aValues, aValuesLength), aRv);
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

  // This method computes the AudioParam value at a given time based on the event timeline
  float GetValueAtTime(double aTime) const
  {
    const Event* previous = nullptr;
    const Event* next = nullptr;

    bool bailOut = false;
    for (unsigned i = 0; !bailOut && i < mEvents.Length(); ++i) {
      switch (mEvents[i].mType) {
      case Event::SetValue:
      case Event::SetTarget:
      case Event::LinearRamp:
      case Event::ExponentialRamp:
        if (aTime == mEvents[i].mTime) {
          // Find the last event with the same time
          do {
            ++i;
          } while (i < mEvents.Length() &&
                   aTime == mEvents[i].mTime);
          return mEvents[i - 1].mValue;
        }
        previous = next;
        next = &mEvents[i];
        if (aTime < mEvents[i].mTime) {
          bailOut = true;
        }
        break;
      case Event::SetValueCurve:
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
      switch (next->mType) {
      case Event::SetValue:
      case Event::SetTarget:
        // The requested time is before the first event
        return mValue;
      case Event::LinearRamp:
        // Use t=0 as T0 and v=defaultValue as V0
        return LinearInterpolate(0.0, mValue, next->mTime, next->mValue, aTime);
      case Event::ExponentialRamp:
        // Use t=0 as T0 and v=defaultValue as V0
        return ExponentialInterpolate(0.0, mValue, next->mTime, next->mValue, aTime);
      case Event::SetValueCurve:
        // TODO: implement
        return 0.0f;
      }
      MOZ_ASSERT(false, "unreached");
    }

    // SetTarget nodes can be handled no matter what their next node is (if they have one)
    if (previous->mType == Event::SetTarget) {
      // Follow the curve, without regard to the next node
      return ExponentialApproach(previous->mTime, mValue, previous->mValue,
                                 previous->mTimeConstant, aTime);
    }

    // If the requested time is after all of the existing events
    if (!next) {
      switch (previous->mType) {
      case Event::SetValue:
      case Event::LinearRamp:
      case Event::ExponentialRamp:
        // The value will be constant after the last event
        return previous->mValue;
      case Event::SetValueCurve:
        // TODO: implement
        return 0.0f;
      case Event::SetTarget:
        MOZ_ASSERT(false, "unreached");
      }
      MOZ_ASSERT(false, "unreached");
    }

    // Finally, handle the case where we have both a previous and a next event

    // First, handle the case where our range ends up in a ramp event
    switch (next->mType) {
    case Event::LinearRamp:
      return LinearInterpolate(previous->mTime, previous->mValue, next->mTime, next->mValue, aTime);
    case Event::ExponentialRamp:
      return ExponentialInterpolate(previous->mTime, previous->mValue, next->mTime, next->mValue, aTime);
    case Event::SetValue:
    case Event::SetTarget:
    case Event::SetValueCurve:
      break;
    }

    // Now handle all other cases
    switch (previous->mType) {
    case Event::SetValue:
    case Event::LinearRamp:
    case Event::ExponentialRamp:
      // If the next event type is neither linear or exponential ramp, the
      // value is constant.
      return previous->mValue;
    case Event::SetValueCurve:
      // TODO: implement
      return 0.0f;
    case Event::SetTarget:
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

private:
  void InsertEvent(const Event& aEvent, ErrorResult& aRv)
  {
    if (!aEvent.IsValid()) {
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return;
    }

    // Make sure that non-curve events don't fall within the duration of a
    // curve event.
    for (unsigned i = 0; i < mEvents.Length(); ++i) {
      if (mEvents[i].mType == Event::SetValueCurve &&
          mEvents[i].mTime <= aEvent.mTime &&
          (mEvents[i].mTime + mEvents[i].mDuration) >= aEvent.mTime) {
        aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
        return;
      }
    }

    // Make sure that curve events don't fall in a range which includes other
    // events.
    if (aEvent.mType == Event::SetValueCurve) {
      for (unsigned i = 0; i < mEvents.Length(); ++i) {
        if (mEvents[i].mTime >= aEvent.mTime &&
            mEvents[i].mTime <= (aEvent.mTime + aEvent.mDuration)) {
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
  nsTArray<Event> mEvents;
  float mValue;
};

}
}

#endif

