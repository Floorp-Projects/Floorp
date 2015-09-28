/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebAudioUtils.h"
#include "AudioNodeStream.h"
#include "blink/HRTFDatabaseLoader.h"

namespace mozilla {

namespace dom {

void WebAudioUtils::ConvertAudioTimelineEventToTicks(AudioTimelineEvent& aEvent,
                                                     AudioNodeStream* aSource,
                                                     AudioNodeStream* aDest)
{
  MOZ_ASSERT(!aSource || aSource->SampleRate() == aDest->SampleRate());
  aEvent.SetTimeInTicks(
      aSource->TicksFromDestinationTime(aDest, aEvent.Time<double>()));
  aEvent.mTimeConstant *= aSource->SampleRate();
  aEvent.mDuration *= aSource->SampleRate();
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

} // namespace dom
} // namespace mozilla
