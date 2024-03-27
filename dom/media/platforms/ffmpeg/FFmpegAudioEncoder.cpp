/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegAudioEncoder.h"

#include "FFmpegRuntimeLinker.h"
#include "FFmpegLog.h"
#include "FFmpegUtils.h"

#include "AudioSegment.h"

namespace mozilla {

FFmpegAudioEncoder<LIBAV_VER>::FFmpegAudioEncoder(
    const FFmpegLibWrapper* aLib, AVCodecID aCodecID,
    const RefPtr<TaskQueue>& aTaskQueue, const EncoderConfig& aConfig)
    : FFmpegDataEncoder(aLib, aCodecID, aTaskQueue, aConfig)
{}

nsCString FFmpegAudioEncoder<LIBAV_VER>::GetDescriptionName() const {
#ifdef USING_MOZFFVPX
  return "ffvpx audio encoder"_ns;
#else
  const char* lib =
#  if defined(MOZ_FFMPEG)
      FFmpegRuntimeLinker::LinkStatusLibraryName();
#  else
      "no library: ffmpeg disabled during build";
#  endif
  return nsPrintfCString("ffmpeg audio encoder (%s)", lib);
#endif
}

nsresult FFmpegAudioEncoder<LIBAV_VER>::InitSpecific() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEG_LOG("FFmpegAudioEncoder::InitInternal");

  // Initialize the common members of the encoder instance
  AVCodec* codec = FFmpegDataEncoder<LIBAV_VER>::InitCommon();
  if (!codec) {
    FFMPEG_LOG("FFmpegDataEncoder::InitCommon failed");
    return NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR;
  }

  // And now the audio-specific part
  mCodecContext->sample_rate = AssertedCast<int>(mConfig.mSampleRate);
  mCodecContext->channels = AssertedCast<int>(mConfig.mNumberOfChannels);

#if LIBAVCODEC_VERSION_MAJOR >= 60
  // Gecko's ordering intentionnally matches ffmepg's ordering
  mLib->av_channel_layout_default(&mCodecContext->ch_layout,
                                  AssertedCast<int>(mCodecContext->channels));
#endif

  switch (mConfig.mCodec) {
    case CodecType::Opus:
      // When using libopus, ffmpeg supports interleaved float and s16 input.
      mCodecContext->sample_fmt = AV_SAMPLE_FMT_FLT;
      break;
    case CodecType::Vorbis:
      // When using libvorbis, ffmpeg only supports planar f32 input.
      mCodecContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Not supported");
  }

  if (mConfig.mCodec == CodecType::Opus) {
    // Default is VBR
    if (mConfig.mBitrateMode == BitrateMode::Constant) {
      mLib->av_opt_set(mCodecContext->priv_data, "vbr", "off", 0);
    }
  }
  // Override the time base: always the sample-rate in audio
  mCodecContext->time_base =
      AVRational{.num = 1, .den = mCodecContext->sample_rate};

  // Apply codec specific settings.
  nsAutoCString codecSpecificLog;
  if (mConfig.mCodecSpecific) {
    // TODO
  }

  MediaResult rv = FinishInitCommon(codec);
  if (NS_FAILED(rv)) {
    FFMPEG_LOG("FFmpeg encode initialization failure.");
    return rv.Code();
  }

  return NS_OK;
}

// avcodec_send_frame and avcodec_receive_packet were introduced in version 58.
#if LIBAVCODEC_VERSION_MAJOR >= 58

Result<MediaDataEncoder::EncodedData, nsresult>
FFmpegAudioEncoder<LIBAV_VER>::EncodeOnePacket(Span<float> aSamples,
                                               uint32_t aChannels,
                                               uint32_t aRate,
                                               media::TimeUnit aPts) {
  // Allocate AVFrame.
  if (!PrepareFrame()) {
    FFMPEG_LOG("failed to allocate frame");
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }

  uint32_t frameCount = aFrames.Length() / aChannels;

  // This method assumes that the audio has been packetized appropriately --
  // packets smaller than the packet size are allowed when draining.
  MOZ_ASSERT(AssertedCast<int>(frameCount) <= mCodecContext->frame_size);

  mFrame->channels = AssertedCast<int>(aChannels);

#  if LIBAVCODEC_VERSION_MAJOR >= 60
  int rv = mLib->av_channel_layout_copy(&mFrame->ch_layout,
                                        &mCodecContext->ch_layout);
  if (rv < 0) {
    FFMPEG_LOG("channel layout copy error: %s",
                MakeErrorString(mLib, rv).get());
    return Err(NS_ERROR_DOM_MEDIA_FATAL_ERR);
  }
#  endif

  mFrame->sample_rate = AssertedCast<int>(aRate);
  // Not a mistake, nb_samples is per channel in ffmpeg
  mFrame->nb_samples = AssertedCast<int>(frameCount);
  // Audio is converted below if needed
  mFrame->format = mCodecContext->sample_fmt;
  // Set presentation timestamp and duration of the AVFrame.
#  if LIBAVCODEC_VERSION_MAJOR >= 59
  mFrame->time_base = AVRational{.num = 1, .den = static_cast<int>(aRate)};
#  endif
  mFrame->pts = aPts.ToTicksAtRate(aRate);
  mFrame->pkt_duration = frameCount;
#  if LIBAVCODEC_VERSION_MAJOR >= 60
  mFrame->duration = frameCount;
#  else
  // Save duration in the time_base unit.
  mDurationMap.Insert(mFrame->pts, mFrame->pkt_duration);
#  endif

  if (int ret = mLib->av_frame_get_buffer(mFrame, 16); ret < 0) {
    FFMPEG_LOG("failed to allocate frame data: %s",
                MakeErrorString(mLib, ret).get());
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }

  // Make sure AVFrame is writable.
  if (int ret = mLib->av_frame_make_writable(mFrame); ret < 0) {
    FFMPEG_LOG("failed to make frame writable: %s",
                MakeErrorString(mLib, ret).get());
    return Err( NS_ERROR_DOM_MEDIA_FATAL_ERR);
  }

  // The input is always in f32 interleaved for now
  if (mCodecContext->sample_fmt == AV_SAMPLE_FMT_FLT) {
    PodCopy(reinterpret_cast<float*>(mFrame->data[0]), aSamples.data(),
            aSamples.Length());
  } else {
    MOZ_ASSERT(mCodecContext->sample_fmt == AV_SAMPLE_FMT_FLTP);
    for (uint32_t i = 0; i < aChannels; i++) {
      DeinterleaveAndConvertBuffer(aSamples.data(), mFrame->nb_samples,
                                   aChannels, mFrame->data);
    }
  }

  // Now send the AVFrame to ffmpeg for encoding, same code for audio and video.
  return FFmpegDataEncoder<LIBAV_VER>::EncodeWithModernAPIs();
}

Result<MediaDataEncoder::EncodedData, nsresult> FFmpegAudioEncoder<
    LIBAV_VER>::EncodeInputWithModernAPIs(RefPtr<const MediaData> aSample) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  MOZ_ASSERT(mCodecContext);
  MOZ_ASSERT(aSample);

  RefPtr<const AudioData> sample(aSample->As<AudioData>());

  FFMPEG_LOG("Encoding %" PRIu32 " frames of audio at pts: %s",
              sample->Frames(), sample->mTime.ToString().get());

  // TODO: Validate input -- error out of sample rate or channel count is not
  // what's expected

  if (sample->mRate != mConfig.mSampleRate ||
      sample->mChannels != mConfig.mNumberOfChannels) {
    FFMPEG_LOG(
        "Rate or sample-rate mismatch at the input of the audio encoder, "
        "erroring out");
    return Result<MediaDataEncoder::EncodedData, nsresult>(
        NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);
  }

  // ffmpeg expects exactly sized input audio packets most of the time.
  // Packetization is performed if needed, and audio packets of the correct size
  // are fed to ffmpeg, with timestamps extrapolated the timestamp found on
  // the input MediaData.

  if (!mPacketizer) {
    media::TimeUnit basePts = media::TimeUnit::Zero(mConfig.mSampleRate);
    basePts += sample->mTime;
    mPacketizer.emplace(mCodecContext->frame_size, sample->mChannels,
                        basePts.ToTicksAtRate(mConfig.mSampleRate),
                        mConfig.mSampleRate);
  }

  if (!mFirstPacketPts.IsValid()) {
    mFirstPacketPts = sample->mTime;
  }

  EncodedData output;
  MediaResult rv = NS_OK;

  mPacketizer->Input(sample->Data().data(), sample->Frames());

  // Dequeue and encode each packet
  while (mPacketizer->PacketsAvailable() && rv.Code() == NS_OK) {
    mTempBuffer.SetLength(mCodecContext->frame_size *
                          mConfig.mNumberOfChannels);
    media::TimeUnit pts = mPacketizer->Output(mTempBuffer.Elements());
    auto audio = Span(mTempBuffer.Elements(), mTempBuffer.Length());
    FFMPEG_LOG("Encoding %" PRIu32 " frames, pts: %s",
                mPacketizer->PacketSize(), pts.ToString().get());
    auto encodeResult = EncodeOnePacket(audio, pts);
    if (encodeResult.isOk()) {
      output.AppendElements(std::move(encodeResult.unwrap()));
    } else {
      return encodeResult;
    }
    pts += media::TimeUnit(mPacketizer->PacketSize(), mConfig.mSampleRate);
  }
  return Result<MediaDataEncoder::EncodedData, nsresult>(std::move(output));
}

Result<MediaDataEncoder::EncodedData, nsresult>
FFmpegAudioEncoder<LIBAV_VER>::DrainWithModernAPIs() {
  // If there's no packetizer, or it's empty, we can proceed immediately.
  if (!mPacketizer || mPacketizer->FramesAvailable() == 0) {
    return FFmpegDataEncoder<LIBAV_VER>::DrainWithModernAPIs();
  }
  EncodedData output;
  MediaResult rv = NS_OK;
  // Dequeue and encode each packet
  mTempBuffer.SetLength(mCodecContext->frame_size *
                        mPacketizer->ChannelCount());
  uint32_t written;
  media::TimeUnit pts = mPacketizer->Drain(mTempBuffer.Elements(), written);
  auto audio =
      Span(mTempBuffer.Elements(), written * mPacketizer->ChannelCount());
  auto encodeResult = EncodeOnePacket(audio, mPacketizer->ChannelCount(),
                                      mConfig.mSampleRate, pts);
  if (encodeResult.isOk()) {
    auto array = encodeResult.unwrap();
    output.AppendElements(std::move(array));
  } else {
    return encodeResult;
  }
  // Now, drain the encoder
  auto drainResult = FFmpegDataEncoder<LIBAV_VER>::DrainWithModernAPIs();
  if (drainResult.isOk()) {
    auto array = drainResult.unwrap();
    output.AppendElements(std::move(array));
  } else {
    return drainResult;
  }
  return Result<MediaDataEncoder::EncodedData, nsresult>(std::move(output));
}
#endif  // if LIBAVCODEC_VERSION_MAJOR >= 58

RefPtr<MediaRawData> FFmpegAudioEncoder<LIBAV_VER>::ToMediaRawData(
    AVPacket* aPacket) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  MOZ_ASSERT(aPacket);

  RefPtr<MediaRawData> data = ToMediaRawDataCommon(aPacket);

  data->mTime = media::TimeUnit(aPacket->pts, mConfig.mSampleRate);
  data->mTimecode = data->mTime;
  data->mDuration =
      media::TimeUnit(mCodecContext->frame_size, mConfig.mSampleRate);

  // Handle encoder delay
  // Tracked in https://github.com/w3c/webcodecs/issues/626 because not quite
  // specced yet.
  if (mFirstPacketPts > data->mTime) {
    data->mOriginalPresentationWindow =
        Some(media::TimeInterval{data->mTime, data->GetEndTime()});
    // Duration is likely to be ajusted when the above spec issue is fixed. For
    // now, leave it as-is
    //  data->mDuration -= (mFirstPacketPts - data->mTime);
    // if (data->mDuration.IsNegative()) {
    //   data->mDuration = media::TimeUnit::Zero();
    // }
    data->mTime = mFirstPacketPts;
  }

  if (auto r = GetExtraData(aPacket); r.isOk()) {
    data->mExtraData = r.unwrap();
  }

  if (data->mExtraData) {
    FFMPEG_LOG(
        "FFmpegAudioEncoder out: [%s,%s] (%zu bytes, extradata %zu bytes)",
        data->mTime.ToString().get(), data->mDuration.ToString().get(),
        data->Size(), data->mExtraData->Length());
  } else {
    FFMPEG_LOG("FFmpegAudioEncoder out: [%s,%s] (%zu bytes)",
                data->mTime.ToString().get(), data->mDuration.ToString().get(),
                data->Size());
  }

  return data;
}

Result<already_AddRefed<MediaByteBuffer>, nsresult>
FFmpegAudioEncoder<LIBAV_VER>::GetExtraData(AVPacket* aPacket) {
  MOZ_ASSERT(aPacket);

  // Create extra data.
  auto extraData = MakeRefPtr<MediaByteBuffer>();
  extraData->SetLength(mCodecContext->extradata_size);
  MOZ_ASSERT(extraData);
  return extraData.forget();
}

}  // namespace mozilla
