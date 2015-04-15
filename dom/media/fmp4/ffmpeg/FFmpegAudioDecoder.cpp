/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTaskQueue.h"
#include "FFmpegRuntimeLinker.h"

#include "FFmpegAudioDecoder.h"

#define MAX_CHANNELS 16

namespace mozilla
{

FFmpegAudioDecoder<LIBAV_VER>::FFmpegAudioDecoder(
  FlushableMediaTaskQueue* aTaskQueue, MediaDataDecoderCallback* aCallback,
  const AudioInfo& aConfig)
  : FFmpegDataDecoder(aTaskQueue, GetCodecId(aConfig.mMimeType))
  , mCallback(aCallback)
{
  MOZ_COUNT_CTOR(FFmpegAudioDecoder);
  // Use a new MediaByteBuffer as the object will be modified during initialization.
  mExtraData = new MediaByteBuffer;
  mExtraData->AppendElements(*aConfig.mCodecSpecificConfig);
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
FFmpegAudioDecoder<LIBAV_VER>::DecodePacket(MediaRawData* aSample)
{
  AVPacket packet;
  av_init_packet(&packet);

  packet.data = const_cast<uint8_t*>(aSample->mData);
  packet.size = aSample->mSize;

  if (!PrepareFrame()) {
    NS_WARNING("FFmpeg audio decoder failed to allocate frame.");
    mCallback->Error();
    return;
  }

  int64_t samplePosition = aSample->mOffset;
  Microseconds pts = aSample->mTime;

  while (packet.size > 0) {
    int decoded;
    int bytesConsumed =
      avcodec_decode_audio4(mCodecContext, mFrame, &decoded, &packet);

    if (bytesConsumed < 0) {
      NS_WARNING("FFmpeg audio decoder error.");
      mCallback->Error();
      return;
    }

    if (decoded) {
      uint32_t numChannels = mCodecContext->channels;
      uint32_t samplingRate = mCodecContext->sample_rate;

      nsAutoArrayPtr<AudioDataValue> audio(
        CopyAndPackAudio(mFrame, numChannels, mFrame->nb_samples));

      CheckedInt<Microseconds> duration =
        FramesToUsecs(mFrame->nb_samples, samplingRate);
      if (!duration.isValid()) {
        NS_WARNING("Invalid count of accumulated audio samples");
        mCallback->Error();
        return;
      }

      nsRefPtr<AudioData> data = new AudioData(samplePosition,
                                               pts,
                                               duration.value(),
                                               mFrame->nb_samples,
                                               audio.forget(),
                                               numChannels,
                                               samplingRate);
      mCallback->Output(data);
      pts += duration.value();
    }
    packet.data += bytesConsumed;
    packet.size -= bytesConsumed;
    samplePosition += bytesConsumed;
  }

  if (mTaskQueue->IsEmpty()) {
    mCallback->InputExhausted();
  }
}

nsresult
FFmpegAudioDecoder<LIBAV_VER>::Input(MediaRawData* aSample)
{
  mTaskQueue->Dispatch(NS_NewRunnableMethodWithArg<nsRefPtr<MediaRawData> >(
    this, &FFmpegAudioDecoder::DecodePacket, nsRefPtr<MediaRawData>(aSample)));

  return NS_OK;
}

nsresult
FFmpegAudioDecoder<LIBAV_VER>::Drain()
{
  mTaskQueue->AwaitIdle();
  mCallback->DrainComplete();
  return Flush();
}

AVCodecID
FFmpegAudioDecoder<LIBAV_VER>::GetCodecId(const nsACString& aMimeType)
{
  if (aMimeType.EqualsLiteral("audio/mpeg")) {
    return AV_CODEC_ID_MP3;
  }

  if (aMimeType.EqualsLiteral("audio/mp4a-latm")) {
    return AV_CODEC_ID_AAC;
  }

  return AV_CODEC_ID_NONE;
}

FFmpegAudioDecoder<LIBAV_VER>::~FFmpegAudioDecoder()
{
  MOZ_COUNT_DTOR(FFmpegAudioDecoder);
}

} // namespace mozilla
