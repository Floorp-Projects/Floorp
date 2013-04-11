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
    TrackRate sampleRate = IdealAudioRate();
    StreamTime streamTime;

    ConvertTimeToTickHelper* This = static_cast<ConvertTimeToTickHelper*> (aClosure);
    if (This->mSourceStream) {
      TrackTicks tick = This->mDestinationStream->GetCurrentPosition();
      StreamTime destinationStreamTime = TicksToTimeRoundDown(sampleRate, tick);
      GraphTime graphTime = This->mDestinationStream->StreamTimeToGraphTime(destinationStreamTime);
      streamTime = This->mSourceStream->GraphTimeToStreamTime(graphTime);
    } else {
      streamTime = This->mDestinationStream->GetCurrentPosition();
    }
    return TimeToTicksRoundDown(sampleRate, streamTime + SecondsToMediaTime(aTime));
  }
};

void
WebAudioUtils::ConvertAudioParamToTicks(AudioParamTimeline& aParam,
                                        AudioNodeStream* aSource,
                                        AudioNodeStream* aDest)
{
  ConvertTimeToTickHelper ctth;
  ctth.mSourceStream = aSource;
  ctth.mDestinationStream = aDest;
  aParam.ConvertEventTimesToTicks(ConvertTimeToTickHelper::Convert, &ctth);
}

}
}
