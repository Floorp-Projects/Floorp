/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioParamTimeline_h_
#define AudioParamTimeline_h_

#include "AudioEventTimeline.h"
#include "AudioNodeTrack.h"
#include "AudioSegment.h"

namespace mozilla::dom {

struct AudioParamEvent final : public AudioTimelineEvent {
  AudioParamEvent(Type aType, double aTime, float aValue,
                  double aTimeConstant = 0.0)
      : AudioTimelineEvent(aType, aTime, aValue, aTimeConstant) {}
  AudioParamEvent(Type aType, const nsTArray<float>& aValues, double aStartTime,
                  double aDuration)
      : AudioTimelineEvent(aType, aValues, aStartTime, aDuration) {}
  explicit AudioParamEvent(AudioNodeTrack* aTrack)
      : AudioTimelineEvent(Track, 0.0, 0.f), mTrack(aTrack) {}

  RefPtr<AudioNodeTrack> mTrack;
};

// This helper class is used to represent the part of the AudioParam
// class that gets sent to AudioNodeEngine instances.  In addition to
// AudioEventTimeline methods, it holds a pointer to an optional
// AudioNodeTrack which represents the AudioNode inputs to the AudioParam.
// This AudioNodeTrack is managed by the AudioParam subclass on the main
// thread, and can only be obtained from the AudioNodeEngine instances
// consuming this class.
class AudioParamTimeline : public AudioEventTimeline {
  typedef AudioEventTimeline BaseClass;

 public:
  explicit AudioParamTimeline(float aDefaultValue) : BaseClass(aDefaultValue) {}

  AudioNodeTrack* Track() const { return mTrack; }

  bool HasSimpleValue() const {
    return BaseClass::HasSimpleValue() &&
           (!mTrack || mTrack->LastChunks()[0].IsNull());
  }

  template <class TimeType>
  float GetValueAtTime(TimeType aTime);

  // Prefer this method over GetValueAtTime() only if HasSimpleValue() is
  // known false.
  float GetComplexValueAtTime(int64_t aTime);
  float GetComplexValueAtTime(double aTime) = delete;

  template <typename TimeType>
  void InsertEvent(const AudioParamEvent& aEvent) {
    if (aEvent.mType == AudioTimelineEvent::Cancel) {
      CancelScheduledValues(aEvent.Time<TimeType>());
      return;
    }
    if (aEvent.mType == AudioTimelineEvent::Track) {
      mTrack = aEvent.mTrack;
      return;
    }
    if (aEvent.mType == AudioTimelineEvent::SetValue) {
      AudioEventTimeline::SetValue(aEvent.NominalValue());
      return;
    }
    AudioEventTimeline::InsertEvent<TimeType>(aEvent);
  }

  // Get the values of the AudioParam at time aTime + (0 to aSize).
  // aBuffer must have the correct aSize.
  // aSize here is an offset to aTime if we try to get the value in ticks,
  // otherwise it should always be zero.  aSize is meant to be used when
  // getting the value of an a-rate AudioParam for each tick inside an
  // AudioNodeEngine implementation.
  void GetValuesAtTime(int64_t aTime, float* aBuffer, const size_t aSize);
  void GetValuesAtTime(double aTime, float* aBuffer,
                       const size_t aSize) = delete;

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
    return mTrack ? mTrack->SizeOfIncludingThis(aMallocSizeOf) : 0;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  float AudioNodeInputValue(size_t aCounter) const;

 protected:
  // This is created lazily when needed.
  RefPtr<AudioNodeTrack> mTrack;
};

template <>
inline float AudioParamTimeline::GetValueAtTime(double aTime) {
  // Getting an AudioParam value on an AudioNode does not consider input from
  // other AudioNodes, which is managed only on the graph thread.
  return BaseClass::GetValueAtTime(aTime);
}

template <>
inline float AudioParamTimeline::GetValueAtTime(int64_t aTime) {
  // Use GetValuesAtTime() for a-rate parameters.
  MOZ_ASSERT(aTime % WEBAUDIO_BLOCK_SIZE == 0);
  if (HasSimpleValue()) {
    return GetValue();
  }
  return GetComplexValueAtTime(aTime);
}

inline float AudioParamTimeline::GetComplexValueAtTime(int64_t aTime) {
  MOZ_ASSERT(aTime % WEBAUDIO_BLOCK_SIZE == 0);

  // Mix the value of the AudioParam itself with that of the AudioNode inputs.
  return BaseClass::GetValueAtTime(aTime) +
         (mTrack ? AudioNodeInputValue(0) : 0.0f);
}

inline void AudioParamTimeline::GetValuesAtTime(int64_t aTime, float* aBuffer,
                                                const size_t aSize) {
  MOZ_ASSERT(aBuffer);
  MOZ_ASSERT(aSize <= WEBAUDIO_BLOCK_SIZE);
  MOZ_ASSERT(aSize == 1 || !HasSimpleValue());

  // Mix the value of the AudioParam itself with that of the AudioNode inputs.
  BaseClass::GetValuesAtTime(aTime, aBuffer, aSize);
  if (mTrack) {
    uint32_t blockOffset = aTime % WEBAUDIO_BLOCK_SIZE;
    MOZ_ASSERT(blockOffset + aSize <= WEBAUDIO_BLOCK_SIZE);
    for (size_t i = 0; i < aSize; ++i) {
      aBuffer[i] += AudioNodeInputValue(blockOffset + i);
    }
  }
}

}  // namespace mozilla::dom

#endif
