/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegAudioDecoder.h"
#include "FFmpegUtils.h"
#include "AudioSampleFormat.h"
#include "FFmpegLog.h"
#include "TimeUnits.h"
#include "VideoUtils.h"
#include "BufferReader.h"
#include "libavutil/dict.h"
#include "libavutil/samplefmt.h"
#if defined(FFVPX_VERSION)
#  include "libavutil/channel_layout.h"
#endif
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/Telemetry.h"

namespace mozilla {

using TimeUnit = media::TimeUnit;

FFmpegAudioDecoder<LIBAV_VER>::FFmpegAudioDecoder(
    FFmpegLibWrapper* aLib, const CreateDecoderParams& aDecoderParams)
    : FFmpegDataDecoder(aLib, GetCodecId(aDecoderParams.AudioConfig().mMimeType,
                                         aDecoderParams.AudioConfig())),
      mAudioInfo(aDecoderParams.AudioConfig()) {
  MOZ_COUNT_CTOR(FFmpegAudioDecoder);

  if (mCodecID == AV_CODEC_ID_AAC &&
      mAudioInfo.mCodecSpecificConfig.is<AacCodecSpecificData>()) {
    const AacCodecSpecificData& aacCodecSpecificData =
        mAudioInfo.mCodecSpecificConfig.as<AacCodecSpecificData>();
    mExtraData = new MediaByteBuffer;
    // Ffmpeg expects the DecoderConfigDescriptor blob.
    mExtraData->AppendElements(
        *aacCodecSpecificData.mDecoderConfigDescriptorBinaryBlob);
    FFMPEG_LOG("FFmpegAudioDecoder ctor (aac)");
    return;
  }

  if (mCodecID == AV_CODEC_ID_MP3) {
    // Nothing to do
    return;
  }

  if (mCodecID == AV_CODEC_ID_FLAC) {
    MOZ_DIAGNOSTIC_ASSERT(
        mAudioInfo.mCodecSpecificConfig.is<FlacCodecSpecificData>());
    // Gracefully handle bad data. If don't hit the preceding assert once this
    // has been shipped for awhile, we can remove it and make the following code
    // non-conditional.
    if (mAudioInfo.mCodecSpecificConfig.is<FlacCodecSpecificData>()) {
      const FlacCodecSpecificData& flacCodecSpecificData =
          mAudioInfo.mCodecSpecificConfig.as<FlacCodecSpecificData>();
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

  // Vorbis and Opus are handled by this case.
  RefPtr<MediaByteBuffer> audioCodecSpecificBinaryBlob =
      GetAudioCodecSpecificBlob(mAudioInfo.mCodecSpecificConfig);
  if (audioCodecSpecificBinaryBlob && audioCodecSpecificBinaryBlob->Length()) {
    // Use a new MediaByteBuffer as the object will be modified during
    // initialization.
    mExtraData = new MediaByteBuffer;
    mExtraData->AppendElements(*audioCodecSpecificBinaryBlob);
  }

  if (mCodecID == AV_CODEC_ID_OPUS) {
    mDefaultPlaybackDeviceMono = aDecoderParams.mOptions.contains(
        CreateDecoderParams::Option::DefaultPlaybackDeviceMono);
  }
}

RefPtr<MediaDataDecoder::InitPromise> FFmpegAudioDecoder<LIBAV_VER>::Init() {
  AVDictionary* options = nullptr;
  if (mCodecID == AV_CODEC_ID_OPUS) {
    // Opus has a special feature for stereo coding where it represent wide
    // stereo channels by 180-degree out of phase. This improves quality, but
    // needs to be disabled when the output is downmixed to mono. Playback
    // number of channels are set in AudioSink, using the same method
    // `DecideAudioPlaybackChannels()`, and triggers downmix if needed.
    if (mDefaultPlaybackDeviceMono ||
        DecideAudioPlaybackChannels(mAudioInfo) == 1) {
      mLib->av_dict_set(&options, "apply_phase_inv", "false", 0);
    }
  }

  MediaResult rv = InitDecoder(&options);

  mLib->av_dict_free(&options);

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
  mCodecContext->request_sample_fmt =
      (mLib->mVersion == 53) ? AV_SAMPLE_FMT_S16 : AV_SAMPLE_FMT_FLT;
#ifdef FFVPX_VERSION
  // AudioInfo's layout first 32-bits are bit-per-bit compatible with
  // WAVEFORMATEXTENSIBLE and FFmpeg's AVChannel enum. We can cast here.
  mCodecContext->ch_layout.nb_channels =
      AssertedCast<int>(mAudioInfo.mChannels);
  if (mAudioInfo.mChannelMap != AudioConfig::ChannelLayout::UNKNOWN_MAP) {
    mLib->av_channel_layout_from_mask(
        &mCodecContext->ch_layout,
        AssertedCast<uint64_t>(mAudioInfo.mChannelMap));
  } else {
    mLib->av_channel_layout_default(&mCodecContext->ch_layout,
                                    AssertedCast<int>(mAudioInfo.mChannels));
  }
  mCodecContext->sample_rate = AssertedCast<int>(mAudioInfo.mRate);
#endif
}

static AlignedAudioBuffer CopyAndPackAudio(AVFrame* aFrame,
                                           uint32_t aNumChannels,
                                           uint32_t aNumAFrames) {
  AlignedAudioBuffer audio(aNumChannels * aNumAFrames);
  if (!audio) {
    return audio;
  }

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
        *tmp++ = ConvertAudioSample<float>(*data++);
      }
    }
  } else if (aFrame->format == AV_SAMPLE_FMT_S16P) {
    // Planar audio data. Convert it from S16 to 32 bits float
    // and pack it into something we can understand.
    AudioDataValue* tmp = audio.get();
    int16_t** data = reinterpret_cast<int16_t**>(aFrame->data);
    for (uint32_t frame = 0; frame < aNumAFrames; frame++) {
      for (uint32_t channel = 0; channel < aNumChannels; channel++) {
        *tmp++ = ConvertAudioSample<float>(data[channel][frame]);
      }
    }
  } else if (aFrame->format == AV_SAMPLE_FMT_S32) {
    // Audio data already packed. Need to convert from S16 to 32 bits Float
    AudioDataValue* tmp = audio.get();
    int32_t* data = reinterpret_cast<int32_t**>(aFrame->data)[0];
    for (uint32_t frame = 0; frame < aNumAFrames; frame++) {
      for (uint32_t channel = 0; channel < aNumChannels; channel++) {
        *tmp++ = ConvertAudioSample<float>(*data++);
      }
    }
  } else if (aFrame->format == AV_SAMPLE_FMT_S32P) {
    // Planar audio data. Convert it from S32 to 32 bits float
    // and pack it into something we can understand.
    AudioDataValue* tmp = audio.get();
    int32_t** data = reinterpret_cast<int32_t**>(aFrame->data);
    for (uint32_t frame = 0; frame < aNumAFrames; frame++) {
      for (uint32_t channel = 0; channel < aNumChannels; channel++) {
        *tmp++ = ConvertAudioSample<float>(data[channel][frame]);
      }
    }
  } else if (aFrame->format == AV_SAMPLE_FMT_U8) {
    // Interleaved audio data. Convert it from u8 to the expected sample-format
    AudioDataValue* tmp = audio.get();
    uint8_t* data = reinterpret_cast<uint8_t**>(aFrame->data)[0];
    for (uint32_t frame = 0; frame < aNumAFrames; frame++) {
      for (uint32_t channel = 0; channel < aNumChannels; channel++) {
        *tmp++ = ConvertAudioSample<float>(*data++);
      }
    }
  } else if (aFrame->format == AV_SAMPLE_FMT_U8P) {
    // Planar audio data. Convert it from u8 to the expected sample-format
    // and pack it into something we can understand.
    AudioDataValue* tmp = audio.get();
    uint8_t** data = reinterpret_cast<uint8_t**>(aFrame->data);
    for (uint32_t frame = 0; frame < aNumAFrames; frame++) {
      for (uint32_t channel = 0; channel < aNumChannels; channel++) {
        *tmp++ = ConvertAudioSample<float>(data[channel][frame]);
      }
    }
  }

  return audio;
}

using ChannelLayout = AudioConfig::ChannelLayout;

MediaResult FFmpegAudioDecoder<LIBAV_VER>::PostProcessOutput(
    bool aDecoded, MediaRawData* aSample, DecodedData& aResults,
    bool* aGotFrame, int32_t aSubmitted) {
  media::TimeUnit pts = aSample->mTime;

  if (mFrame->format != AV_SAMPLE_FMT_FLT &&
      mFrame->format != AV_SAMPLE_FMT_FLTP &&
      mFrame->format != AV_SAMPLE_FMT_S16 &&
      mFrame->format != AV_SAMPLE_FMT_S16P &&
      mFrame->format != AV_SAMPLE_FMT_S32 &&
      mFrame->format != AV_SAMPLE_FMT_S32P &&
      mFrame->format != AV_SAMPLE_FMT_U8 &&
      mFrame->format != AV_SAMPLE_FMT_U8P) {
    return MediaResult(
        NS_ERROR_DOM_MEDIA_DECODE_ERR,
        RESULT_DETAIL("FFmpeg audio decoder outputs unsupported audio format"));
  }

  if (aSubmitted < 0) {
    FFMPEG_LOG("Got %d more frame from packet", mFrame->nb_samples);
  }

  FFMPEG_LOG("FFmpegAudioDecoder decoded: [%s,%s] (Duration: %s) [%s]",
             aSample->mTime.ToString().get(),
             aSample->GetEndTime().ToString().get(),
             aSample->mDuration.ToString().get(),
             mLib->av_get_sample_fmt_name(mFrame->format));

  uint32_t numChannels = ChannelCount(mCodecContext);
  uint32_t samplingRate = mCodecContext->sample_rate;
  if (!numChannels) {
    numChannels = mAudioInfo.mChannels;
  }
  if (!samplingRate) {
    samplingRate = mAudioInfo.mRate;
  }
  AlignedAudioBuffer audio =
      CopyAndPackAudio(mFrame, numChannels, mFrame->nb_samples);
  if (!audio) {
    FFMPEG_LOG("CopyAndPackAudio error (OOM)");
    return MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__);
  }

  media::TimeUnit duration = TimeUnit(mFrame->nb_samples, samplingRate);
  if (!duration.IsValid()) {
    FFMPEG_LOG("Duration isn't valid (%d + %d)", mFrame->nb_samples,
               samplingRate);
    return MediaResult(NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
                       RESULT_DETAIL("Invalid sample duration"));
  }

  media::TimeUnit newpts = pts + duration;
  if (!newpts.IsValid()) {
    FFMPEG_LOG("New pts isn't valid (%lf + %lf)", pts.ToSeconds(),
               duration.ToSeconds());
    return MediaResult(
        NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
        RESULT_DETAIL("Invalid count of accumulated audio samples"));
  }

  RefPtr<AudioData> data =
      new AudioData(aSample->mOffset, pts, std::move(audio), numChannels,
                    samplingRate, mAudioInfo.mChannelMap);
  MOZ_ASSERT(duration == data->mDuration, "must be equal");
  aResults.AppendElement(std::move(data));

  pts = newpts;

  if (aGotFrame) {
    *aGotFrame = true;
  }
  return NS_OK;
}

#if LIBAVCODEC_VERSION_MAJOR < 59
MediaResult FFmpegAudioDecoder<LIBAV_VER>::DecodeUsingFFmpeg(
    AVPacket* aPacket, bool& aDecoded, MediaRawData* aSample,
    DecodedData& aResults, bool* aGotFrame) {
  int decoded = 0;
  int rv =
      mLib->avcodec_decode_audio4(mCodecContext, mFrame, &decoded, aPacket);
  aDecoded = decoded == 1;
  if (rv < 0) {
    NS_WARNING("FFmpeg audio decoder error.");
    return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                       RESULT_DETAIL("FFmpeg audio error"));
  }
  PostProcessOutput(decoded, aSample, aResults, aGotFrame, 0);
  return NS_OK;
}
#else
#  define AVRESULT_OK 0

MediaResult FFmpegAudioDecoder<LIBAV_VER>::DecodeUsingFFmpeg(
    AVPacket* aPacket, bool& aDecoded, MediaRawData* aSample,
    DecodedData& aResults, bool* aGotFrame) {
  // This in increment whenever avcodec_send_packet succeeds, and decremented
  // whenever avcodec_receive_frame succeeds. Because it is possible to have
  // multiple AVFrames from a single AVPacket, this number can be negative.
  // This is used to ensure that pts and duration are correctly set on the
  // resulting audio buffers.
  int32_t submitted = 0;
  int ret = mLib->avcodec_send_packet(mCodecContext, aPacket);
  switch (ret) {
    case AVRESULT_OK:
      submitted++;
      break;
    case AVERROR(EAGAIN):
      FFMPEG_LOG("  av_codec_send_packet: EAGAIN.");
      MOZ_ASSERT(false, "EAGAIN");
      break;
    case AVERROR_EOF:
      FFMPEG_LOG("  End of stream.");
      return MediaResult(NS_ERROR_DOM_MEDIA_END_OF_STREAM,
                         RESULT_DETAIL("End of stream"));
    default:
      NS_WARNING("FFmpeg audio decoder error (avcodec_send_packet).");
      return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                         RESULT_DETAIL("FFmpeg audio error"));
  }

  MediaResult rv;

  while (ret == 0) {
    aDecoded = false;
    ret = mLib->avcodec_receive_frame(mCodecContext, mFrame);
    switch (ret) {
      case AVRESULT_OK:
        aDecoded = true;
        submitted--;
        if (submitted < 0) {
          FFMPEG_LOG("Multiple AVFrame from a single AVPacket");
        }
        break;
      case AVERROR(EAGAIN): {
        // Quirk of the vorbis decoder -- the first packet doesn't return audio.
        if (submitted == 1 && mCodecID == AV_CODEC_ID_VORBIS) {
          AlignedAudioBuffer buf;
          aResults.AppendElement(
              new AudioData(0, TimeUnit::Zero(), std::move(buf),
                            mAudioInfo.mChannels, mAudioInfo.mRate));
        }
        FFMPEG_LOG("  EAGAIN (packets submitted: %" PRIu32 ").", submitted);
        rv = NS_OK;
        break;
      }
      case AVERROR_EOF: {
        FFMPEG_LOG("  End of stream.");
        rv = MediaResult(NS_ERROR_DOM_MEDIA_END_OF_STREAM,
                         RESULT_DETAIL("End of stream"));
        break;
      }
      default:
        FFMPEG_LOG("  avcodec_receive_packet error.");
        NS_WARNING("FFmpeg audio decoder error (avcodec_receive_packet).");
        rv = MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                         RESULT_DETAIL("FFmpeg audio error"));
    }
    if (aDecoded) {
      PostProcessOutput(aDecoded, aSample, aResults, aGotFrame, submitted);
    }
  }

  return NS_OK;
}
#endif

MediaResult FFmpegAudioDecoder<LIBAV_VER>::DoDecode(MediaRawData* aSample,
                                                    uint8_t* aData, int aSize,
                                                    bool* aGotFrame,
                                                    DecodedData& aResults) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  PROCESS_DECODE_LOG(aSample);
  AVPacket* packet;
#if LIBAVCODEC_VERSION_MAJOR >= 61
  packet = mLib->av_packet_alloc();
  auto freePacket = MakeScopeExit([&] { mLib->av_packet_free(&packet); });
#else
  AVPacket packet_mem;
  packet = &packet_mem;
  mLib->av_init_packet(packet);
#endif

  FFMPEG_LOG("FFmpegAudioDecoder::DoDecode: %d bytes, [%s,%s] (Duration: %s)",
             aSize, aSample->mTime.ToString().get(),
             aSample->GetEndTime().ToString().get(),
             aSample->mDuration.ToString().get());

  packet->data = const_cast<uint8_t*>(aData);
  packet->size = aSize;

  if (aGotFrame) {
    *aGotFrame = false;
  }

  if (!PrepareFrame()) {
    FFMPEG_LOG("FFmpegAudioDecoder: OOM in PrepareFrame");
    return MediaResult(
        NS_ERROR_OUT_OF_MEMORY,
        RESULT_DETAIL("FFmpeg audio decoder failed to allocate frame"));
  }

  bool decoded = false;
  auto rv = DecodeUsingFFmpeg(packet, decoded, aSample, aResults, aGotFrame);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

AVCodecID FFmpegAudioDecoder<LIBAV_VER>::GetCodecId(const nsACString& aMimeType,
                                                    const AudioInfo& aInfo) {
  if (aMimeType.EqualsLiteral("audio/mp4a-latm")) {
    return AV_CODEC_ID_AAC;
  }
#ifdef FFVPX_VERSION
  if (aMimeType.EqualsLiteral("audio/mpeg")) {
    return AV_CODEC_ID_MP3;
  }
  if (aMimeType.EqualsLiteral("audio/flac")) {
    return AV_CODEC_ID_FLAC;
  }
  if (aMimeType.EqualsLiteral("audio/vorbis")) {
    return AV_CODEC_ID_VORBIS;
  }
  if (aMimeType.EqualsLiteral("audio/opus")) {
    return AV_CODEC_ID_OPUS;
  }
  if (aMimeType.Find("wav") != kNotFound) {
    if (aMimeType.EqualsLiteral("audio/x-wav") ||
        aMimeType.EqualsLiteral("audio/wave; codecs=1") ||
        aMimeType.EqualsLiteral("audio/wave; codecs=65534")) {
      // find the pcm format
      switch (aInfo.mBitDepth) {
        case 8:
          return AV_CODEC_ID_PCM_U8;
        case 16:
          return AV_CODEC_ID_PCM_S16LE;
        case 24:
          return AV_CODEC_ID_PCM_S24LE;
        case 32:
          return AV_CODEC_ID_PCM_S32LE;
        case 0:
          // ::Init will find and use the right type here, this is just
          // returning something that means that this media type can be decoded.
          // This happens when attempting to find what decoder to use for a
          // media type, without actually having looked at the actual
          // bytestream. This decoder can decode all usual PCM bytestream
          // anyway.
          return AV_CODEC_ID_PCM_S16LE;
        default:
          return AV_CODEC_ID_NONE;
      };
    }
    if (aMimeType.EqualsLiteral("audio/wave; codecs=3")) {
      return AV_CODEC_ID_PCM_F32LE;
    }
    // A-law
    if (aMimeType.EqualsLiteral("audio/wave; codecs=6")) {
      return AV_CODEC_ID_PCM_ALAW;
    }
    // Mu-law
    if (aMimeType.EqualsLiteral("audio/wave; codecs=7")) {
      return AV_CODEC_ID_PCM_MULAW;
    }
  }
#endif

  return AV_CODEC_ID_NONE;
}

nsCString FFmpegAudioDecoder<LIBAV_VER>::GetCodecName() const {
#if LIBAVCODEC_VERSION_MAJOR > 53
  return nsCString(mLib->avcodec_descriptor_get(mCodecID)->name);
#else
  return "unknown"_ns;
#endif
}

FFmpegAudioDecoder<LIBAV_VER>::~FFmpegAudioDecoder() {
  MOZ_COUNT_DTOR(FFmpegAudioDecoder);
}

}  // namespace mozilla
