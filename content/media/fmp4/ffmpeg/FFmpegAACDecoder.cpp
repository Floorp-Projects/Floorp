/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTaskQueue.h"
#include "FFmpegRuntimeLinker.h"

#include "FFmpegAACDecoder.h"

#define MAX_CHANNELS 16

typedef mp4_demuxer::MP4Sample MP4Sample;

namespace mozilla
{

FFmpegAACDecoder<LIBAV_VER>::FFmpegAACDecoder(
  MediaTaskQueue* aTaskQueue, MediaDataDecoderCallback* aCallback,
  const mp4_demuxer::AudioDecoderConfig& aConfig)
  : FFmpegDataDecoder(aTaskQueue, AV_CODEC_ID_AAC), mCallback(aCallback)
{
  MOZ_COUNT_CTOR(FFmpegAACDecoder);
}

nsresult
FFmpegAACDecoder<LIBAV_VER>::Init()
{
  nsresult rv = FFmpegDataDecoder::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

static AudioDataValue*
CopyAndPackAudio(AVFrame* aFrame, uint32_t aNumChannels, uint32_t aNumSamples)
{
  // These are the only two valid AAC packet sizes.
  NS_ASSERTION(aNumSamples == 960 || aNumSamples == 1024,
               "Should have exactly one AAC audio packet.");
  MOZ_ASSERT(aNumChannels <= MAX_CHANNELS);

  nsAutoArrayPtr<AudioDataValue> audio(
    new AudioDataValue[aNumChannels * aNumSamples]);

  AudioDataValue** data = reinterpret_cast<AudioDataValue**>(aFrame->data);

  if (aFrame->format == AV_SAMPLE_FMT_FLT) {
    // Audio data already packed. No need to do anything other than copy it
    // into a buffer we own.
    memcpy(audio, data[0], aNumChannels * aNumSamples * sizeof(AudioDataValue));
  } else if (aFrame->format == AV_SAMPLE_FMT_FLTP) {
    // Planar audio data. Pack it into something we can understand.
    for (uint32_t channel = 0; channel < aNumChannels; channel++) {
      for (uint32_t sample = 0; sample < aNumSamples; sample++) {
        audio[sample * aNumChannels + channel] = data[channel][sample];
      }
    }
  }

  return audio.forget();
}

void
FFmpegAACDecoder<LIBAV_VER>::DecodePacket(MP4Sample* aSample)
{
  nsAutoPtr<AVFrame> frame(avcodec_alloc_frame());
  avcodec_get_frame_defaults(frame);

  AVPacket packet;
  av_init_packet(&packet);

  aSample->Pad(FF_INPUT_BUFFER_PADDING_SIZE);
  packet.data = aSample->data;
  packet.size = aSample->size;
  packet.pos = aSample->byte_offset;

  int decoded;
  int bytesConsumed =
    avcodec_decode_audio4(mCodecContext, frame.get(), &decoded, &packet);

  if (bytesConsumed < 0 || !decoded) {
    NS_WARNING("FFmpeg audio decoder error.");
    mCallback->Error();
    return;
  }

  NS_ASSERTION(bytesConsumed == (int)aSample->size,
               "Only one audio packet should be received at a time.");

  uint32_t numChannels = mCodecContext->channels;

  nsAutoArrayPtr<AudioDataValue> audio(
    CopyAndPackAudio(frame.get(), numChannels, frame->nb_samples));

  nsAutoPtr<AudioData> data(
    new AudioData(packet.pos, aSample->composition_timestamp, aSample->duration,
                  frame->nb_samples, audio.forget(), numChannels));

  mCallback->Output(data.forget());

  if (mTaskQueue->IsEmpty()) {
    mCallback->InputExhausted();
  }
}

nsresult
FFmpegAACDecoder<LIBAV_VER>::Input(MP4Sample* aSample)
{
  mTaskQueue->Dispatch(NS_NewRunnableMethodWithArg<nsAutoPtr<MP4Sample> >(
    this, &FFmpegAACDecoder::DecodePacket, nsAutoPtr<MP4Sample>(aSample)));

  return NS_OK;
}

nsresult
FFmpegAACDecoder<LIBAV_VER>::Drain()
{
  mCallback->DrainComplete();
  return NS_OK;
}

FFmpegAACDecoder<LIBAV_VER>::~FFmpegAACDecoder()
{
  MOZ_COUNT_DTOR(FFmpegAACDecoder);
}

} // namespace mozilla
