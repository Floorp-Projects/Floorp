/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebAudioUtils.h"
#include "AudioNodeStream.h"

namespace mozilla {

namespace dom {

struct ConvertTimeToTickHelper
{
  AudioNodeStream* mSourceStream;
  AudioNodeStream* mDestinationStream;

  static int64_t Convert(double aTime, void* aClosure)
  {
    ConvertTimeToTickHelper* This = static_cast<ConvertTimeToTickHelper*> (aClosure);
    MOZ_ASSERT(This->mSourceStream->SampleRate() == This->mDestinationStream->SampleRate());
    return WebAudioUtils::ConvertDestinationStreamTimeToSourceStreamTime(
        aTime, This->mSourceStream, This->mDestinationStream);
  }
};

TrackTicks
WebAudioUtils::ConvertDestinationStreamTimeToSourceStreamTime(double aTime,
                                                              AudioNodeStream* aSource,
                                                              MediaStream* aDestination)
{
  StreamTime streamTime = std::max<MediaTime>(0, SecondsToMediaTime(aTime));
  GraphTime graphTime = aDestination->StreamTimeToGraphTime(streamTime);
  StreamTime thisStreamTime = aSource->GraphTimeToStreamTimeOptimistic(graphTime);
  TrackTicks ticks = TimeToTicksRoundUp(aSource->SampleRate(), thisStreamTime);
  return ticks;
}

double
WebAudioUtils::StreamPositionToDestinationTime(TrackTicks aSourcePosition,
                                               AudioNodeStream* aSource,
                                               AudioNodeStream* aDestination)
{
  MOZ_ASSERT(aSource->SampleRate() == aDestination->SampleRate());
  StreamTime sourceTime = TicksToTimeRoundDown(aSource->SampleRate(), aSourcePosition);
  GraphTime graphTime = aSource->StreamTimeToGraphTime(sourceTime);
  StreamTime destinationTime = aDestination->GraphTimeToStreamTimeOptimistic(graphTime);
  return MediaTimeToSeconds(destinationTime);
}

void
WebAudioUtils::ConvertAudioParamToTicks(AudioParamTimeline& aParam,
                                        AudioNodeStream* aSource,
                                        AudioNodeStream* aDest)
{
  MOZ_ASSERT(!aSource || aSource->SampleRate() == aDest->SampleRate());
  ConvertTimeToTickHelper ctth;
  ctth.mSourceStream = aSource;
  ctth.mDestinationStream = aDest;
  aParam.ConvertEventTimesToTicks(ConvertTimeToTickHelper::Convert, &ctth, aDest->SampleRate());
}

}
}
