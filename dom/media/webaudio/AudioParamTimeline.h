/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioParamTimeline_h_
#define AudioParamTimeline_h_

#include "AudioEventTimeline.h"
#include "mozilla/ErrorResult.h"
#include "nsAutoPtr.h"
#include "MediaStreamGraph.h"
#include "AudioSegment.h"

namespace mozilla {

namespace dom {

// This helper class is used to represent the part of the AudioParam
// class that gets sent to AudioNodeEngine instances.  In addition to
// AudioEventTimeline methods, it holds a pointer to an optional
// MediaStream which represents the AudioNode inputs to the AudioParam.
// This MediaStream is managed by the AudioParam subclass on the main
// thread, and can only be obtained from the AudioNodeEngine instances
// consuming this class.
class AudioParamTimeline : public AudioEventTimeline<ErrorResult>
{
  typedef AudioEventTimeline<ErrorResult> BaseClass;

public:
  explicit AudioParamTimeline(float aDefaultValue)
    : BaseClass(aDefaultValue)
  {
  }

  MediaStream* Stream() const
  {
    return mStream;
  }

  bool HasSimpleValue() const
  {
    return BaseClass::HasSimpleValue() && !mStream;
  }

  // Get the value of the AudioParam at time aTime + aCounter.
  // aCounter here is an offset to aTime if we try to get the value in ticks,
  // otherwise it should always be zero.  aCounter is meant to be used when
  // getting the value of an a-rate AudioParam for each tick inside an
  // AudioNodeEngine implementation.
  template<class TimeType>
  float GetValueAtTime(TimeType aTime, size_t aCounter = 0);

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    return mStream ? mStream->SizeOfIncludingThis(aMallocSizeOf) : 0;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }


private:
  float AudioNodeInputValue(size_t aCounter) const;

protected:
  // This is created lazily when needed.
  nsRefPtr<MediaStream> mStream;
};

template<> inline float
AudioParamTimeline::GetValueAtTime(double aTime, size_t aCounter)
{
  MOZ_ASSERT(!aCounter);

  // Getting an AudioParam value on an AudioNode does not consider input from
  // other AudioNodes, which is managed only on the graph thread.
  return BaseClass::GetValueAtTime(aTime);
}


template<> inline float
AudioParamTimeline::GetValueAtTime(int64_t aTime, size_t aCounter)
{
  MOZ_ASSERT(aCounter < WEBAUDIO_BLOCK_SIZE);
  MOZ_ASSERT(!aCounter || !HasSimpleValue());

  // Mix the value of the AudioParam itself with that of the AudioNode inputs.
  return BaseClass::GetValueAtTime(static_cast<int64_t>(aTime + aCounter)) +
    (mStream ? AudioNodeInputValue(aCounter) : 0.0f);
}

}
}

#endif

