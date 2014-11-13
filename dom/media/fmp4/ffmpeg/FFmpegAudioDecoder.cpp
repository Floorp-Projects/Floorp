/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTaskQueue.h"
#include "FFmpegRuntimeLinker.h"

#include "FFmpegAudioDecoder.h"
#include "mp4_demuxer/Adts.h"

#define MAX_CHANNELS 16

typedef mp4_demuxer::MP4Sample MP4Sample;

namespace mozilla
{

FFmpegAudioDecoder<LIBAV_VER>::FFmpegAudioDecoder(
  MediaTaskQueue* aTaskQueue, MediaDataDecoderCallback* aCallback,
  const mp4_demuxer::AudioDecoderConfig& aConfig)
  : FFmpegDataDecoder(aTaskQueue, GetCodecId(aConfig.mime_type))
  , mCallback(aCallback)
  , mConfig(aConfig)
{
  MOZ_COUNT_CTOR(FFmpegAudioDecoder);
  mExtraData.append(aConfig.audio_specific_config.begin(),
                    aConfig.audio_specific_config.length());
}

nsresult
FFmpegAudioDecoder<LIBAV_VER>::Init()
{
  nsresult rv = FFmpegDataDecoder::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

static AudioDataValue*
CopyAndPackAudio(AVFrame* aFrame, uint32_t aNumChannels, uint32_t aNumAFrames)
{
  MOZ_ASSERT(aNumChannels <= MAX_CHANNELS);

  nsAutoArrayPtr<AudioDataValue> audio(
    new AudioDataValue[aNumChannels * aNumAFrames]);

  if (aFrame->format == AV_SAMPLE_FMT_FLT) {
    // Audio data already packed. No need to do anything other than copy it
    // into a buffer we own.
    memcpy(audio, aFrame->data[0],
           aNumChannels * aNumAFrames * sizeof(AudioDataValue));
  } else if (aFrame->format == AV_SAMPLE_FMT_FLTP) {
    // Planar audio data. Pack it into something we can understand.
    AudioDataValue* tmp = audio;
    AudioDataValue** data = reinterpret_cast<AudioDataValue**>(aFrame->data);
    for (uint32_t frame = 0; frame < aNumAFrames; frame++) {
      for (uint32_t channel = 0; channel < aNumChannels; channel++) {
        *tmp++ = data[channel][frame];
      }
    }
  } else if (aFrame->format == AV_SAMPLE_FMT_S16) {
    // Audio data already packed. Need to convert from S16 to 32 bits Float
    AudioDataValue* tmp = audio;
    int16_t* data = reinterpret_cast<int16_t**>(aFrame->data)[0];
    for (uint32_t frame = 0; frame < aNumAFrames; frame++) {
      for (uint32_t channel = 0; channel < aNumChannels; channel++) {
        *tmp++ = AudioSampleToFloat(*data++);
      }
    }
  } else if (aFrame->format == AV_SAMPLE_FMT_S16P) {
    // Planar audio data. Convert it from S16 to 32 bits float
    // and pack it into something we can understand.
    AudioDataValue* tmp = audio;
    int16_t** data = reinterpret_cast<int16_t**>(aFrame->data);
    for (uint32_t frame = 0; frame < aNumAFrames; frame++) {
      for (uint32_t channel = 0; channel < aNumChannels; channel++) {
        *tmp++ = AudioSampleToFloat(data[channel][frame]);
      }
    }
  }

  return audio.forget();
}

void
FFmpegAudioDecoder<LIBAV_VER>::DecodePacket(MP4Sample* aSample)
{
  AVPacket packet;
  av_init_packet(&packet);

  aSample->Pad(FF_INPUT_BUFFER_PADDING_SIZE);
  packet.data = aSample->data;
  packet.size = aSample->size;
  packet.pos = aSample->byte_offset;

  if (!PrepareFrame()) {
    NS_WARNING("FFmpeg audio decoder failed to allocate frame.");
    mCallback->Error();
    return;
  }

  int decoded;
  int bytesConsumed =
    avcodec_decode_audio4(mCodecContext, mFrame, &decoded, &packet);

  if (bytesConsumed < 0 || !decoded) {
    NS_WARNING("FFmpeg audio decoder error.");
    mCallback->Error();
    return;
  }

  NS_ASSERTION(bytesConsumed == (int)aSample->size,
               "Only one audio packet should be received at a time.");

  uint32_t numChannels = mCodecContext->channels;
  uint32_t samplingRate = mCodecContext->sample_rate;

  nsAutoArrayPtr<AudioDataValue> audio(
    CopyAndPackAudio(mFrame, numChannels, mFrame->nb_samples));

  nsAutoPtr<AudioData> data(
    new AudioData(packet.pos, aSample->composition_timestamp, aSample->duration,
                  mFrame->nb_samples, audio.forget(), numChannels, samplingRate));

  mCallback->Output(data.forget());

  if (mTaskQueue->IsEmpty()) {
    mCallback->InputExhausted();
  }
}

nsresult
FFmpegAudioDecoder<LIBAV_VER>::Input(MP4Sample* aSample)
{
  mTaskQueue->Dispatch(NS_NewRunnableMethodWithArg<nsAutoPtr<MP4Sample> >(
    this, &FFmpegAudioDecoder::DecodePacket, nsAutoPtr<MP4Sample>(aSample)));

  return NS_OK;
}

nsresult
FFmpegAudioDecoder<LIBAV_VER>::Drain()
{
  mCallback->DrainComplete();
  return NS_OK;
}

AVCodecID
FFmpegAudioDecoder<LIBAV_VER>::GetCodecId(const char* aMimeType)
{
  if (!strcmp(aMimeType, "audio/mpeg")) {
    return AV_CODEC_ID_MP3;
  }

  if (!strcmp(aMimeType, "audio/mp4a-latm")) {
    return AV_CODEC_ID_AAC;
  }

  return AV_CODEC_ID_NONE;
}

FFmpegAudioDecoder<LIBAV_VER>::~FFmpegAudioDecoder()
{
  MOZ_COUNT_DTOR(FFmpegAudioDecoder);
}

} // namespace mozilla
