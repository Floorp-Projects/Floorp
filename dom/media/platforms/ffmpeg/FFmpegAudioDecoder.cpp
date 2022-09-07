/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegAudioDecoder.h"
#include "FFmpegLog.h"
#include "TimeUnits.h"
#include "VideoUtils.h"
#include "BufferReader.h"
#include "mozilla/StaticPrefs_media.h"

namespace mozilla {

FFmpegAudioDecoder<LIBAV_VER>::FFmpegAudioDecoder(FFmpegLibWrapper* aLib,
                                                  const AudioInfo& aConfig)
    : FFmpegDataDecoder(aLib, GetCodecId(aConfig.mMimeType)) {
  MOZ_COUNT_CTOR(FFmpegAudioDecoder);

  if (mCodecID == AV_CODEC_ID_AAC &&
      aConfig.mCodecSpecificConfig.is<AacCodecSpecificData>()) {
    const AacCodecSpecificData& aacCodecSpecificData =
        aConfig.mCodecSpecificConfig.as<AacCodecSpecificData>();
    mExtraData = new MediaByteBuffer;
    // Ffmpeg expects the DecoderConfigDescriptor blob.
    mExtraData->AppendElements(
        *aacCodecSpecificData.mDecoderConfigDescriptorBinaryBlob);
    return;
  }

  if (mCodecID == AV_CODEC_ID_MP3) {
    MOZ_DIAGNOSTIC_ASSERT(
        aConfig.mCodecSpecificConfig.is<Mp3CodecSpecificData>());
    // Gracefully handle bad data. If don't hit the preceding assert once this
    // has been shipped for awhile, we can remove it and make the following code
    // non-conditional.
    if (aConfig.mCodecSpecificConfig.is<Mp3CodecSpecificData>()) {
      const Mp3CodecSpecificData& mp3CodecSpecificData =
          aConfig.mCodecSpecificConfig.as<Mp3CodecSpecificData>();
      mEncoderDelay = mp3CodecSpecificData.mEncoderDelayFrames;
      mEncoderPadding = mp3CodecSpecificData.mEncoderPaddingFrames;
      FFMPEG_LOG("FFmpegAudioDecoder, found encoder delay (%" PRIu32
                 ") and padding values (%" PRIu32 ") in extra data",
                 mEncoderDelay, mEncoderPadding);
      return;
    }
  }

  if (mCodecID == AV_CODEC_ID_FLAC) {
    MOZ_DIAGNOSTIC_ASSERT(
        aConfig.mCodecSpecificConfig.is<FlacCodecSpecificData>());
    // Gracefully handle bad data. If don't hit the preceding assert once this
    // has been shipped for awhile, we can remove it and make the following code
    // non-conditional.
    if (aConfig.mCodecSpecificConfig.is<FlacCodecSpecificData>()) {
      const FlacCodecSpecificData& flacCodecSpecificData =
          aConfig.mCodecSpecificConfig.as<FlacCodecSpecificData>();
      if (flacCodecSpecificData.mStreamInfoBinaryBlob->IsEmpty()) {
        // Flac files without headers will be missing stream info. In this case
        // we don't want to feed ffmpeg empty extra data as it will fail, just
        // early return.
        return;
      }
      // Use a new MediaByteBuffer as the object will be modified during
      // initialization.
      mExtraData = new MediaByteBuffer;
      mExtraData->AppendElements(*flacCodecSpecificData.mStreamInfoBinaryBlob);
      return;
    }
  }

  // Gracefully handle failure to cover all codec specific cases above. Once
  // we're confident there is no fall through from these cases above, we should
  // remove this code.
  RefPtr<MediaByteBuffer> audioCodecSpecificBinaryBlob =
      GetAudioCodecSpecificBlob(aConfig.mCodecSpecificConfig);
  if (audioCodecSpecificBinaryBlob && audioCodecSpecificBinaryBlob->Length()) {
    // Use a new MediaByteBuffer as the object will be modified during
    // initialization.
    mExtraData = new MediaByteBuffer;
    mExtraData->AppendElements(*audioCodecSpecificBinaryBlob);
  }
}

RefPtr<MediaDataDecoder::InitPromise> FFmpegAudioDecoder<LIBAV_VER>::Init() {
  MediaResult rv = InitDecoder();

  return NS_SUCCEEDED(rv)
             ? InitPromise::CreateAndResolve(TrackInfo::kAudioTrack, __func__)
             : InitPromise::CreateAndReject(rv, __func__);
}

void FFmpegAudioDecoder<LIBAV_VER>::InitCodecContext() {
  MOZ_ASSERT(mCodecContext);
  // We do not want to set this value to 0 as FFmpeg by default will
  // use the number of cores, which with our mozlibavutil get_cpu_count
  // isn't implemented.
  mCodecContext->thread_count = 1;
  // FFmpeg takes this as a suggestion for what format to use for audio samples.
  // LibAV 0.8 produces rubbish float interleaved samples, request 16 bits
  // audio.
#ifdef MOZ_SAMPLE_TYPE_S16
  mCodecContext->request_sample_fmt = AV_SAMPLE_FMT_S16;
#else
  mCodecContext->request_sample_fmt =
      (mLib->mVersion == 53) ? AV_SAMPLE_FMT_S16 : AV_SAMPLE_FMT_FLT;
#endif
}

static AlignedAudioBuffer CopyAndPackAudio(AVFrame* aFrame,
                                           uint32_t aNumChannels,
                                           uint32_t aNumAFrames) {
  AlignedAudioBuffer audio(aNumChannels * aNumAFrames);
  if (!audio) {
    return audio;
  }

#ifdef MOZ_SAMPLE_TYPE_S16
  if (aFrame->format == AV_SAMPLE_FMT_FLT) {
    // Audio data already packed. Need to convert from 32 bits Float to S16
    AudioDataValue* tmp = audio.get();
    float* data = reinterpret_cast<float**>(aFrame->data)[0];
    for (uint32_t frame = 0; frame < aNumAFrames; frame++) {
      for (uint32_t channel = 0; channel < aNumChannels; channel++) {
        *tmp++ = FloatToAudioSample<int16_t>(*data++);
      }
    }
  } else if (aFrame->format == AV_SAMPLE_FMT_FLTP) {
    // Planar audio data. Convert it from 32 bits float to S16
    // and pack it into something we can understand.
    AudioDataValue* tmp = audio.get();
    float** data = reinterpret_cast<float**>(aFrame->data);
    for (uint32_t frame = 0; frame < aNumAFrames; frame++) {
      for (uint32_t channel = 0; channel < aNumChannels; channel++) {
        *tmp++ = FloatToAudioSample<int16_t>(data[channel][frame]);
      }
    }
  } else if (aFrame->format == AV_SAMPLE_FMT_S16) {
    // Audio data already packed. No need to do anything other than copy it
    // into a buffer we own.
    memcpy(audio.get(), aFrame->data[0],
           aNumChannels * aNumAFrames * sizeof(AudioDataValue));
  } else if (aFrame->format == AV_SAMPLE_FMT_S16P) {
    // Planar audio data. Pack it into something we can understand.
    AudioDataValue* tmp = audio.get();
    AudioDataValue** data = reinterpret_cast<AudioDataValue**>(aFrame->data);
    for (uint32_t frame = 0; frame < aNumAFrames; frame++) {
      for (uint32_t channel = 0; channel < aNumChannels; channel++) {
        *tmp++ = data[channel][frame];
      }
    }
  } else if (aFrame->format == AV_SAMPLE_FMT_S32) {
    // Audio data already packed. Need to convert from S32 to S16
    AudioDataValue* tmp = audio.get();
    int32_t* data = reinterpret_cast<int32_t**>(aFrame->data)[0];
    for (uint32_t frame = 0; frame < aNumAFrames; frame++) {
      for (uint32_t channel = 0; channel < aNumChannels; channel++) {
        *tmp++ = *data++ / (1U << 16);
      }
    }
  } else if (aFrame->format == AV_SAMPLE_FMT_S32P) {
    // Planar audio data. Convert it from S32 to S16
    // and pack it into something we can understand.
    AudioDataValue* tmp = audio.get();
    int32_t** data = reinterpret_cast<int32_t**>(aFrame->data);
    for (uint32_t frame = 0; frame < aNumAFrames; frame++) {
      for (uint32_t channel = 0; channel < aNumChannels; channel++) {
        *tmp++ = data[channel][frame] / (1U << 16);
      }
    }
  }
#else
  if (aFrame->format == AV_SAMPLE_FMT_FLT) {
    // Audio data already packed. No need to do anything other than copy it
    // into a buffer we own.
    memcpy(audio.get(), aFrame->data[0],
           aNumChannels * aNumAFrames * sizeof(AudioDataValue));
  } else if (aFrame->format == AV_SAMPLE_FMT_FLTP) {
    // Planar audio data. Pack it into something we can understand.
    AudioDataValue* tmp = audio.get();
    AudioDataValue** data = reinterpret_cast<AudioDataValue**>(aFrame->data);
    for (uint32_t frame = 0; frame < aNumAFrames; frame++) {
      for (uint32_t channel = 0; channel < aNumChannels; channel++) {
        *tmp++ = data[channel][frame];
      }
    }
  } else if (aFrame->format == AV_SAMPLE_FMT_S16) {
    // Audio data already packed. Need to convert from S16 to 32 bits Float
    AudioDataValue* tmp = audio.get();
    int16_t* data = reinterpret_cast<int16_t**>(aFrame->data)[0];
    for (uint32_t frame = 0; frame < aNumAFrames; frame++) {
      for (uint32_t channel = 0; channel < aNumChannels; channel++) {
        *tmp++ = AudioSampleToFloat(*data++);
      }
    }
  } else if (aFrame->format == AV_SAMPLE_FMT_S16P) {
    // Planar audio data. Convert it from S16 to 32 bits float
    // and pack it into something we can understand.
    AudioDataValue* tmp = audio.get();
    int16_t** data = reinterpret_cast<int16_t**>(aFrame->data);
    for (uint32_t frame = 0; frame < aNumAFrames; frame++) {
      for (uint32_t channel = 0; channel < aNumChannels; channel++) {
        *tmp++ = AudioSampleToFloat(data[channel][frame]);
      }
    }
  } else if (aFrame->format == AV_SAMPLE_FMT_S32) {
    // Audio data already packed. Need to convert from S16 to 32 bits Float
    AudioDataValue* tmp = audio.get();
    int32_t* data = reinterpret_cast<int32_t**>(aFrame->data)[0];
    for (uint32_t frame = 0; frame < aNumAFrames; frame++) {
      for (uint32_t channel = 0; channel < aNumChannels; channel++) {
        *tmp++ = AudioSampleToFloat(*data++);
      }
    }
  } else if (aFrame->format == AV_SAMPLE_FMT_S32P) {
    // Planar audio data. Convert it from S32 to 32 bits float
    // and pack it into something we can understand.
    AudioDataValue* tmp = audio.get();
    int32_t** data = reinterpret_cast<int32_t**>(aFrame->data);
    for (uint32_t frame = 0; frame < aNumAFrames; frame++) {
      for (uint32_t channel = 0; channel < aNumChannels; channel++) {
        *tmp++ = AudioSampleToFloat(data[channel][frame]);
      }
    }
  }
#endif

  return audio;
}

typedef AudioConfig::ChannelLayout ChannelLayout;

MediaResult FFmpegAudioDecoder<LIBAV_VER>::DoDecode(MediaRawData* aSample,
                                                    uint8_t* aData, int aSize,
                                                    bool* aGotFrame,
                                                    DecodedData& aResults) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  PROCESS_DECODE_LOG(aSample);
  AVPacket packet;
  mLib->av_init_packet(&packet);

  packet.data = const_cast<uint8_t*>(aData);
  packet.size = aSize;

  if (aGotFrame) {
    *aGotFrame = false;
  }

  if (!PrepareFrame()) {
    return MediaResult(
        NS_ERROR_OUT_OF_MEMORY,
        RESULT_DETAIL("FFmpeg audio decoder failed to allocate frame"));
  }

  int64_t samplePosition = aSample->mOffset;
  media::TimeUnit pts = aSample->mTime;

  while (packet.size > 0) {
    int decoded = false;
    int bytesConsumed = -1;
#if LIBAVCODEC_VERSION_MAJOR < 59
    bytesConsumed =
        mLib->avcodec_decode_audio4(mCodecContext, mFrame, &decoded, &packet);
    if (bytesConsumed < 0) {
      NS_WARNING("FFmpeg audio decoder error.");
      return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                         RESULT_DETAIL("FFmpeg audio error:%d", bytesConsumed));
    }
#else
#  define AVRESULT_OK 0
    int ret = mLib->avcodec_send_packet(mCodecContext, &packet);
    switch (ret) {
      case AVRESULT_OK:
        bytesConsumed = packet.size;
        break;
      case AVERROR(EAGAIN):
        break;
      case AVERROR_EOF:
        FFMPEG_LOG("  End of stream.");
        return MediaResult(NS_ERROR_DOM_MEDIA_END_OF_STREAM,
                           RESULT_DETAIL("End of stream"));
      default:
        NS_WARNING("FFmpeg audio decoder error.");
        return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                           RESULT_DETAIL("FFmpeg audio error"));
    }

    ret = mLib->avcodec_receive_frame(mCodecContext, mFrame);
    switch (ret) {
      case AVRESULT_OK:
        decoded = true;
        break;
      case AVERROR(EAGAIN):
        break;
      case AVERROR_EOF: {
        FFMPEG_LOG("  End of stream.");
        return MediaResult(NS_ERROR_DOM_MEDIA_END_OF_STREAM,
                           RESULT_DETAIL("End of stream"));
      }
    }
#endif

    if (decoded) {
      if (mFrame->format != AV_SAMPLE_FMT_FLT &&
          mFrame->format != AV_SAMPLE_FMT_FLTP &&
          mFrame->format != AV_SAMPLE_FMT_S16 &&
          mFrame->format != AV_SAMPLE_FMT_S16P &&
          mFrame->format != AV_SAMPLE_FMT_S32 &&
          mFrame->format != AV_SAMPLE_FMT_S32P) {
        return MediaResult(
            NS_ERROR_DOM_MEDIA_DECODE_ERR,
            RESULT_DETAIL(
                "FFmpeg audio decoder outputs unsupported audio format"));
      }
      uint32_t numChannels = mCodecContext->channels;
      uint32_t samplingRate = mCodecContext->sample_rate;

      AlignedAudioBuffer audio =
          CopyAndPackAudio(mFrame, numChannels, mFrame->nb_samples);
      if (!audio) {
        return MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__);
      }

      DebugOnly<bool> trimmed = false;
      if (mEncoderDelay) {
        trimmed = true;
        uint32_t toPop = std::min((uint32_t)mFrame->nb_samples, mEncoderDelay);
        audio.PopFront(toPop * numChannels);
        mFrame->nb_samples -= toPop;
        mEncoderDelay -= toPop;
      }

      if (aSample->mEOS && mEncoderPadding) {
        trimmed = true;
        uint32_t toTrim =
            std::min((uint32_t)mFrame->nb_samples, mEncoderPadding);
        mEncoderPadding -= toTrim;
        audio.PopBack(toTrim * numChannels);
        mFrame->nb_samples = audio.Length() / numChannels;
      }

      media::TimeUnit duration =
          FramesToTimeUnit(mFrame->nb_samples, samplingRate);
      if (!duration.IsValid()) {
        return MediaResult(NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
                           RESULT_DETAIL("Invalid sample duration"));
      }

      media::TimeUnit newpts = pts + duration;
      if (!newpts.IsValid()) {
        return MediaResult(
            NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
            RESULT_DETAIL("Invalid count of accumulated audio samples"));
      }

      RefPtr<AudioData> data =
          new AudioData(samplePosition, pts, std::move(audio), numChannels,
                        samplingRate, mCodecContext->channel_layout);
      MOZ_ASSERT(duration == data->mDuration || trimmed, "must be equal");
      aResults.AppendElement(std::move(data));

      pts = newpts;

      if (aGotFrame) {
        *aGotFrame = true;
      }
    }
    // The packet wasn't sent to ffmpeg, another attempt will happen next
    // iteration.
    if (bytesConsumed != -1) {
      packet.data += bytesConsumed;
      packet.size -= bytesConsumed;
      samplePosition += bytesConsumed;
    }
  }
  return NS_OK;
}

AVCodecID FFmpegAudioDecoder<LIBAV_VER>::GetCodecId(
    const nsACString& aMimeType) {
  if (aMimeType.EqualsLiteral("audio/mpeg")) {
#ifdef FFVPX_VERSION
    if (!StaticPrefs::media_ffvpx_mp3_enabled()) {
      return AV_CODEC_ID_NONE;
    }
#endif
    return AV_CODEC_ID_MP3;
  } else if (aMimeType.EqualsLiteral("audio/flac")) {
    return AV_CODEC_ID_FLAC;
  } else if (aMimeType.EqualsLiteral("audio/mp4a-latm")) {
    return AV_CODEC_ID_AAC;
  }

  return AV_CODEC_ID_NONE;
}

FFmpegAudioDecoder<LIBAV_VER>::~FFmpegAudioDecoder() {
  MOZ_COUNT_DTOR(FFmpegAudioDecoder);
}

}  // namespace mozilla
