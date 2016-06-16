/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioEventTimeline.h"

#include "mozilla/ErrorResult.h"

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

namespace mozilla {
namespace dom {

template <class ErrorResult> bool
AudioEventTimeline::ValidateEvent(AudioTimelineEvent& aEvent,
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
        !(aEvent.mType == AudioTimelineEvent::SetValueCurve &&
          aEvent.template Time<double>() == mEvents[i].template Time<double>()) &&
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
      // In case we have two curve at the same time
      if (mEvents[i].mType == AudioTimelineEvent::SetValueCurve &&
          mEvents[i].template Time<double>() == aEvent.template Time<double>()) {
        continue;
      }
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
template bool
AudioEventTimeline::ValidateEvent(AudioTimelineEvent& aEvent,
                                  ErrorResult& aRv);

// This method computes the AudioParam value at a given time based on the event timeline
template<class TimeType> void
AudioEventTimeline::GetValuesAtTimeHelper(TimeType aTime, float* aBuffer,
                                          const size_t aSize)
{
  MOZ_ASSERT(aBuffer);
  MOZ_ASSERT(aSize);

  size_t eventIndex = 0;
  const AudioTimelineEvent* previous = nullptr;
  const AudioTimelineEvent* next = nullptr;
  bool bailOut = false;

  // Let's remove old events except the last one: we need it to calculate some curves.
  while (mEvents.Length() > 1 &&
         aTime > mEvents[1].template Time<TimeType>()) {
    mEvents.RemoveElementAt(0);
  }

  for (size_t bufferIndex = 0; bufferIndex < aSize; ++bufferIndex, ++aTime) {
    for (; !bailOut && eventIndex < mEvents.Length(); ++eventIndex) {

#ifdef DEBUG
      const AudioTimelineEvent* current = &mEvents[eventIndex];
      MOZ_ASSERT(current->mType == AudioTimelineEvent::SetValueAtTime ||
                 current->mType == AudioTimelineEvent::SetTarget ||
                 current->mType == AudioTimelineEvent::LinearRamp ||
                 current->mType == AudioTimelineEvent::ExponentialRamp ||
                 current->mType == AudioTimelineEvent::SetValueCurve);
#endif

      if (TimesEqual(aTime, mEvents[eventIndex].template Time<TimeType>())) {
        mLastComputedValue = mComputedValue;
        // Find the last event with the same time
        while (eventIndex < mEvents.Length() - 1 &&
               TimesEqual(aTime, mEvents[eventIndex + 1].template Time<TimeType>())) {
          ++eventIndex;
        }
        break;
      }

      previous = next;
      next = &mEvents[eventIndex];
      if (aTime < mEvents[eventIndex].template Time<TimeType>()) {
        bailOut = true;
      }
    }

    if (!bailOut && eventIndex < mEvents.Length()) {
      // The time matches one of the events exactly.
      MOZ_ASSERT(TimesEqual(aTime, mEvents[eventIndex].template Time<TimeType>()));

      // SetTarget nodes can be handled no matter what their next node is (if they have one)
      if (mEvents[eventIndex].mType == AudioTimelineEvent::SetTarget) {
        // Follow the curve, without regard to the next event, starting at
        // the last value of the last event.
        aBuffer[bufferIndex] = ExponentialApproach(mEvents[eventIndex].template Time<TimeType>(),
                                                mLastComputedValue, mEvents[eventIndex].mValue,
                                                mEvents[eventIndex].mTimeConstant, aTime);
        continue;
      }

      // SetValueCurve events can be handled no matter what their event node is (if they have one)
      if (mEvents[eventIndex].mType == AudioTimelineEvent::SetValueCurve) {
        aBuffer[bufferIndex] = ExtractValueFromCurve(mEvents[eventIndex].template Time<TimeType>(),
                                                  mEvents[eventIndex].mCurve,
                                                  mEvents[eventIndex].mCurveLength,
                                                  mEvents[eventIndex].mDuration, aTime);
        continue;
      }

      // For other event types
      aBuffer[bufferIndex] = mEvents[eventIndex].mValue;
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
template void
AudioEventTimeline::GetValuesAtTimeHelper(double aTime, float* aBuffer,
                                          const size_t aSize);
template void
AudioEventTimeline::GetValuesAtTimeHelper(int64_t aTime, float* aBuffer,
                                          const size_t aSize);

template<class TimeType> float
AudioEventTimeline::GetValuesAtTimeHelperInternal(TimeType aTime,
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
        MOZ_FALLTHROUGH_ASSERT("AudioTimelineEvent::SetTarget");
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
    MOZ_FALLTHROUGH_ASSERT("AudioTimelineEvent::SetTarget");
  case AudioTimelineEvent::SetValue:
  case AudioTimelineEvent::Cancel:
  case AudioTimelineEvent::Stream:
    MOZ_ASSERT(false, "Should have been handled earlier.");
  }

  MOZ_ASSERT(false, "unreached");
  return 0.0f;
}
template float
AudioEventTimeline::GetValuesAtTimeHelperInternal(double aTime,
                                    const AudioTimelineEvent* aPrevious,
                                    const AudioTimelineEvent* aNext);
template float
AudioEventTimeline::GetValuesAtTimeHelperInternal(int64_t aTime,
                                    const AudioTimelineEvent* aPrevious,
                                    const AudioTimelineEvent* aNext);

const AudioTimelineEvent*
AudioEventTimeline::GetPreviousEvent(double aTime) const
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

} // namespace dom
} // namespace mozilla

