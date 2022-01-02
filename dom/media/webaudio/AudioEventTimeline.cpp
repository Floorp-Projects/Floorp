/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioEventTimeline.h"
#include "AudioNodeTrack.h"

#include "mozilla/ErrorResult.h"

static float LinearInterpolate(double t0, float v0, double t1, float v1,
                               double t) {
  return v0 + (v1 - v0) * ((t - t0) / (t1 - t0));
}

static float ExponentialInterpolate(double t0, float v0, double t1, float v1,
                                    double t) {
  return v0 * powf(v1 / v0, (t - t0) / (t1 - t0));
}

static float ExponentialApproach(double t0, double v0, float v1,
                                 double timeConstant, double t) {
  if (!mozilla::dom::WebAudioUtils::FuzzyEqual(timeConstant, 0.0)) {
    return v1 + (v0 - v1) * expf(-(t - t0) / timeConstant);
  } else {
    return v1;
  }
}

static float ExtractValueFromCurve(double startTime, float* aCurve,
                                   uint32_t aCurveLength, double duration,
                                   double t) {
  if (t >= startTime + duration) {
    // After the duration, return the last curve value
    return aCurve[aCurveLength - 1];
  }
  double ratio = std::max((t - startTime) / duration, 0.0);
  if (ratio >= 1.0) {
    return aCurve[aCurveLength - 1];
  }
  uint32_t current = uint32_t(floor((aCurveLength - 1) * ratio));
  uint32_t next = current + 1;
  double step = duration / double(aCurveLength - 1);
  if (next < aCurveLength) {
    double t0 = current * step;
    double t1 = next * step;
    return LinearInterpolate(t0, aCurve[current], t1, aCurve[next],
                             t - startTime);
  } else {
    return aCurve[current];
  }
}

namespace mozilla::dom {

AudioTimelineEvent::AudioTimelineEvent(Type aType, double aTime, float aValue,
                                       double aTimeConstant, double aDuration,
                                       const float* aCurve,
                                       uint32_t aCurveLength)
    : mType(aType),
      mCurve(nullptr),
      mTimeConstant(aTimeConstant),
      mDuration(aDuration)
#ifdef DEBUG
      ,
      mTimeIsInTicks(false)
#endif
{
  mTime = aTime;
  if (aType == AudioTimelineEvent::SetValueCurve) {
    SetCurveParams(aCurve, aCurveLength);
  } else {
    mValue = aValue;
  }
}

AudioTimelineEvent::AudioTimelineEvent(AudioNodeTrack* aTrack)
    : mType(Track),
      mCurve(nullptr),
      mTrack(aTrack),
      mTimeConstant(0.0),
      mDuration(0.0)
#ifdef DEBUG
      ,
      mTimeIsInTicks(false)
#endif
      ,
      mTime(0.0) {
}

AudioTimelineEvent::AudioTimelineEvent(const AudioTimelineEvent& rhs) {
  PodCopy(this, &rhs, 1);

  if (rhs.mType == AudioTimelineEvent::SetValueCurve) {
    SetCurveParams(rhs.mCurve, rhs.mCurveLength);
  } else if (rhs.mType == AudioTimelineEvent::Track) {
    new (&mTrack) decltype(mTrack)(rhs.mTrack);
  }
}

AudioTimelineEvent::~AudioTimelineEvent() {
  if (mType == AudioTimelineEvent::SetValueCurve) {
    delete[] mCurve;
  }
}

// This method computes the AudioParam value at a given time based on the event
// timeline
template <class TimeType>
void AudioEventTimeline::GetValuesAtTimeHelper(TimeType aTime, float* aBuffer,
                                               const size_t aSize) {
  MOZ_ASSERT(aBuffer);
  MOZ_ASSERT(aSize);

  auto TimeOf = [](const AudioTimelineEvent& aEvent) -> TimeType {
    return aEvent.Time<TimeType>();
  };

  size_t eventIndex = 0;
  const AudioTimelineEvent* previous = nullptr;

  // Let's remove old events except the last one: we need it to calculate some
  // curves.
  CleanupEventsOlderThan(aTime);

  for (size_t bufferIndex = 0; bufferIndex < aSize; ++bufferIndex, ++aTime) {
    bool timeMatchesEventIndex = false;
    const AudioTimelineEvent* next;
    for (;; ++eventIndex) {
      if (eventIndex >= mEvents.Length()) {
        next = nullptr;
        break;
      }

      next = &mEvents[eventIndex];
      if (aTime < TimeOf(*next)) {
        break;
      }

#ifdef DEBUG
      MOZ_ASSERT(next->mType == AudioTimelineEvent::SetValueAtTime ||
                 next->mType == AudioTimelineEvent::SetTarget ||
                 next->mType == AudioTimelineEvent::LinearRamp ||
                 next->mType == AudioTimelineEvent::ExponentialRamp ||
                 next->mType == AudioTimelineEvent::SetValueCurve);
#endif

      if (TimesEqual(aTime, TimeOf(*next))) {
        mLastComputedValue = mComputedValue;
        // Find the last event with the same time
        while (eventIndex < mEvents.Length() - 1 &&
               TimesEqual(aTime, TimeOf(mEvents[eventIndex + 1]))) {
          mLastComputedValue =
              GetValueAtTimeOfEvent<TimeType>(&mEvents[eventIndex]);
          ++eventIndex;
        }

        timeMatchesEventIndex = true;
        break;
      }

      previous = next;
    }

    if (timeMatchesEventIndex) {
      // The time matches one of the events exactly.
      MOZ_ASSERT(TimesEqual(aTime, TimeOf(mEvents[eventIndex])));
      mComputedValue = GetValueAtTimeOfEvent<TimeType>(&mEvents[eventIndex]);
    } else {
      mComputedValue = GetValuesAtTimeHelperInternal(aTime, previous, next);
    }

    aBuffer[bufferIndex] = mComputedValue;
  }
}
template void AudioEventTimeline::GetValuesAtTimeHelper(double aTime,
                                                        float* aBuffer,
                                                        const size_t aSize);
template void AudioEventTimeline::GetValuesAtTimeHelper(int64_t aTime,
                                                        float* aBuffer,
                                                        const size_t aSize);

template <class TimeType>
float AudioEventTimeline::GetValueAtTimeOfEvent(
    const AudioTimelineEvent* aNext) {
  TimeType time = aNext->Time<TimeType>();
  switch (aNext->mType) {
    case AudioTimelineEvent::SetTarget:
      // SetTarget nodes can be handled no matter what their next node is
      // (if they have one).
      // Follow the curve, without regard to the next event, starting at
      // the last value of the last event.
      return ExponentialApproach(time, mLastComputedValue, aNext->mValue,
                                 aNext->mTimeConstant, time);
      break;
    case AudioTimelineEvent::SetValueCurve:
      // SetValueCurve events can be handled no matter what their event
      // node is (if they have one)
      return ExtractValueFromCurve(time, aNext->mCurve, aNext->mCurveLength,
                                   aNext->mDuration, time);
      break;
    default:
      // For other event types
      return aNext->mValue;
  }
}

template <class TimeType>
float AudioEventTimeline::GetValuesAtTimeHelperInternal(
    TimeType aTime, const AudioTimelineEvent* aPrevious,
    const AudioTimelineEvent* aNext) {
  // If the requested time is before all of the existing events
  if (!aPrevious) {
    return mValue;
  }

  // If this event is a curve event, this returns the end time of the curve.
  // Otherwise, this returns the time of the event.
  auto TimeOf = [](const AudioTimelineEvent* aEvent) -> TimeType {
    if (aEvent->mType == AudioTimelineEvent::SetValueCurve) {
      return aEvent->Time<TimeType>() + aEvent->mDuration;
    }
    return aEvent->Time<TimeType>();
  };

  // Value for an event. For a ValueCurve event, this is the value of the last
  // element of the curve.
  auto ValueOf = [](const AudioTimelineEvent* aEvent) -> float {
    if (aEvent->mType == AudioTimelineEvent::SetValueCurve) {
      return aEvent->mCurve[aEvent->mCurveLength - 1];
    }
    return aEvent->mValue;
  };

  // SetTarget nodes can be handled no matter what their next node is (if
  // they have one)
  if (aPrevious->mType == AudioTimelineEvent::SetTarget) {
    return ExponentialApproach(TimeOf(aPrevious), mLastComputedValue,
                               ValueOf(aPrevious), aPrevious->mTimeConstant,
                               aTime);
  }

  // SetValueCurve events can be handled no matter what their next node is
  // (if they have one), when aTime is in the curve region.
  if (aPrevious->mType == AudioTimelineEvent::SetValueCurve &&
      aTime <= aPrevious->Time<TimeType>() + aPrevious->mDuration) {
    return ExtractValueFromCurve(aPrevious->Time<TimeType>(), aPrevious->mCurve,
                                 aPrevious->mCurveLength, aPrevious->mDuration,
                                 aTime);
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
        return ExtractValueFromCurve(aPrevious->Time<TimeType>(),
                                     aPrevious->mCurve, aPrevious->mCurveLength,
                                     aPrevious->mDuration, aTime);
      case AudioTimelineEvent::SetTarget:
        MOZ_FALLTHROUGH_ASSERT("AudioTimelineEvent::SetTarget");
      case AudioTimelineEvent::SetValue:
      case AudioTimelineEvent::Cancel:
      case AudioTimelineEvent::Track:
        MOZ_ASSERT(false, "Should have been handled earlier.");
    }
    MOZ_ASSERT(false, "unreached");
  }

  // Finally, handle the case where we have both a previous and a next event

  // First, handle the case where our range ends up in a ramp event
  switch (aNext->mType) {
    case AudioTimelineEvent::LinearRamp:
      return LinearInterpolate(TimeOf(aPrevious), ValueOf(aPrevious),
                               TimeOf(aNext), ValueOf(aNext), aTime);

    case AudioTimelineEvent::ExponentialRamp:
      return ExponentialInterpolate(TimeOf(aPrevious), ValueOf(aPrevious),
                                    TimeOf(aNext), ValueOf(aNext), aTime);

    case AudioTimelineEvent::SetValueAtTime:
    case AudioTimelineEvent::SetTarget:
    case AudioTimelineEvent::SetValueCurve:
      break;
    case AudioTimelineEvent::SetValue:
    case AudioTimelineEvent::Cancel:
    case AudioTimelineEvent::Track:
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
      return ExtractValueFromCurve(aPrevious->Time<TimeType>(),
                                   aPrevious->mCurve, aPrevious->mCurveLength,
                                   aPrevious->mDuration, aTime);
    case AudioTimelineEvent::SetTarget:
      MOZ_FALLTHROUGH_ASSERT("AudioTimelineEvent::SetTarget");
    case AudioTimelineEvent::SetValue:
    case AudioTimelineEvent::Cancel:
    case AudioTimelineEvent::Track:
      MOZ_ASSERT(false, "Should have been handled earlier.");
  }

  MOZ_ASSERT(false, "unreached");
  return 0.0f;
}
template float AudioEventTimeline::GetValuesAtTimeHelperInternal(
    double aTime, const AudioTimelineEvent* aPrevious,
    const AudioTimelineEvent* aNext);
template float AudioEventTimeline::GetValuesAtTimeHelperInternal(
    int64_t aTime, const AudioTimelineEvent* aPrevious,
    const AudioTimelineEvent* aNext);

const AudioTimelineEvent* AudioEventTimeline::GetPreviousEvent(
    double aTime) const {
  const AudioTimelineEvent* previous = nullptr;
  const AudioTimelineEvent* next = nullptr;

  auto TimeOf = [](const AudioTimelineEvent& aEvent) -> double {
    return aEvent.Time<double>();
  };

  bool bailOut = false;
  for (unsigned i = 0; !bailOut && i < mEvents.Length(); ++i) {
    switch (mEvents[i].mType) {
      case AudioTimelineEvent::SetValueAtTime:
      case AudioTimelineEvent::SetTarget:
      case AudioTimelineEvent::LinearRamp:
      case AudioTimelineEvent::ExponentialRamp:
      case AudioTimelineEvent::SetValueCurve:
        if (aTime == TimeOf(mEvents[i])) {
          // Find the last event with the same time
          do {
            ++i;
          } while (i < mEvents.Length() && aTime == TimeOf(mEvents[i]));
          return &mEvents[i - 1];
        }
        previous = next;
        next = &mEvents[i];
        if (aTime < TimeOf(mEvents[i])) {
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

}  // namespace mozilla::dom
