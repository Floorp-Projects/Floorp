/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebAudioUtils.h"
#include "AudioNodeStream.h"
#include "AudioParamTimeline.h"
#include "blink/HRTFDatabaseLoader.h"
#include "speex/speex_resampler.h"

namespace mozilla {

namespace dom {

// 32 is the minimum required by the spec and matches what is used by blink.
// The limit protects against large memory allocations.
const uint32_t WebAudioUtils::MaxChannelCount = 32;

struct ConvertTimeToTickHelper
{
  AudioNodeStream* mSourceStream;
  AudioNodeStream* mDestinationStream;

  static int64_t Convert(double aTime, void* aClosure)
  {
    ConvertTimeToTickHelper* This = static_cast<ConvertTimeToTickHelper*> (aClosure);
    MOZ_ASSERT(This->mSourceStream->SampleRate() == This->mDestinationStream->SampleRate());
    return This->mSourceStream->
      TicksFromDestinationTime(This->mDestinationStream, aTime);
  }
};

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

void
WebAudioUtils::Shutdown()
{
  WebCore::HRTFDatabaseLoader::shutdown();
}

int
WebAudioUtils::SpeexResamplerProcess(SpeexResamplerState* aResampler,
                                     uint32_t aChannel,
                                     const float* aIn, uint32_t* aInLen,
                                     float* aOut, uint32_t* aOutLen)
{
#ifdef MOZ_SAMPLE_TYPE_S16
  nsAutoTArray<AudioDataValue, WEBAUDIO_BLOCK_SIZE*4> tmp1;
  nsAutoTArray<AudioDataValue, WEBAUDIO_BLOCK_SIZE*4> tmp2;
  tmp1.SetLength(*aInLen);
  tmp2.SetLength(*aOutLen);
  ConvertAudioSamples(aIn, tmp1.Elements(), *aInLen);
  int result = speex_resampler_process_int(aResampler, aChannel, tmp1.Elements(), aInLen, tmp2.Elements(), aOutLen);
  ConvertAudioSamples(tmp2.Elements(), aOut, *aOutLen);
  return result;
#else
  return speex_resampler_process_float(aResampler, aChannel, aIn, aInLen, aOut, aOutLen);
#endif
}

int
WebAudioUtils::SpeexResamplerProcess(SpeexResamplerState* aResampler,
                                     uint32_t aChannel,
                                     const int16_t* aIn, uint32_t* aInLen,
                                     float* aOut, uint32_t* aOutLen)
{
  nsAutoTArray<AudioDataValue, WEBAUDIO_BLOCK_SIZE*4> tmp;
#ifdef MOZ_SAMPLE_TYPE_S16
  tmp.SetLength(*aOutLen);
  int result = speex_resampler_process_int(aResampler, aChannel, aIn, aInLen, tmp.Elements(), aOutLen);
  ConvertAudioSamples(tmp.Elements(), aOut, *aOutLen);
  return result;
#else
  tmp.SetLength(*aInLen);
  ConvertAudioSamples(aIn, tmp.Elements(), *aInLen);
  int result = speex_resampler_process_float(aResampler, aChannel, tmp.Elements(), aInLen, aOut, aOutLen);
  return result;
#endif
}

int
WebAudioUtils::SpeexResamplerProcess(SpeexResamplerState* aResampler,
                                     uint32_t aChannel,
                                     const int16_t* aIn, uint32_t* aInLen,
                                     int16_t* aOut, uint32_t* aOutLen)
{
#ifdef MOZ_SAMPLE_TYPE_S16
  return speex_resampler_process_int(aResampler, aChannel, aIn, aInLen, aOut, aOutLen);
#else
  nsAutoTArray<AudioDataValue, WEBAUDIO_BLOCK_SIZE*4> tmp1;
  nsAutoTArray<AudioDataValue, WEBAUDIO_BLOCK_SIZE*4> tmp2;
  tmp1.SetLength(*aInLen);
  tmp2.SetLength(*aOutLen);
  ConvertAudioSamples(aIn, tmp1.Elements(), *aInLen);
  int result = speex_resampler_process_float(aResampler, aChannel, tmp1.Elements(), aInLen, tmp2.Elements(), aOutLen);
  ConvertAudioSamples(tmp2.Elements(), aOut, *aOutLen);
  return result;
#endif
}

}
}
