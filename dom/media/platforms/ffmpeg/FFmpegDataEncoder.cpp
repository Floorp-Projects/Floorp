/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegDataEncoder.h"
#include "PlatformEncoderModule.h"

#include <utility>

#include "FFmpegLog.h"
#include "libavutil/error.h"
#include "mozilla/StaticMutex.h"

#include "FFmpegUtils.h"

namespace mozilla {

// TODO: Remove this function and simply use `avcodec_find_encoder` once
// libopenh264 is supported.
static AVCodec* FindEncoderWithPreference(const FFmpegLibWrapper* aLib,
                                          AVCodecID aCodecId) {
  MOZ_ASSERT(aLib);

  AVCodec* codec = nullptr;

  // Prioritize libx264 for now since it's the only h264 codec we tested.
  if (aCodecId == AV_CODEC_ID_H264) {
    codec = aLib->avcodec_find_encoder_by_name("libx264");
    if (codec) {
      FFMPEGV_LOG("Prefer libx264 for h264 codec");
      return codec;
    }
    FFMPEGV_LOG("Fallback to other h264 library. Fingers crossed");
  }

  return aLib->avcodec_find_encoder(aCodecId);
}

template <>
AVCodecID GetFFmpegEncoderCodecId<LIBAV_VER>(CodecType aCodec) {
#if LIBAVCODEC_VERSION_MAJOR >= 58
  if (aCodec == CodecType::VP8) {
    return AV_CODEC_ID_VP8;
  }

  if (aCodec == CodecType::VP9) {
    return AV_CODEC_ID_VP9;
  }

#  if !defined(USING_MOZFFVPX)
  if (aCodec == CodecType::H264) {
    return AV_CODEC_ID_H264;
  }
#  endif

  if (aCodec == CodecType::AV1) {
    return AV_CODEC_ID_AV1;
  }

  if (aCodec == CodecType::Opus) {
    return AV_CODEC_ID_OPUS;
  }

  if (aCodec == CodecType::Vorbis) {
    return AV_CODEC_ID_VORBIS;
  }
#endif
  return AV_CODEC_ID_NONE;
}

StaticMutex FFmpegDataEncoder<LIBAV_VER>::sMutex;

FFmpegDataEncoder<LIBAV_VER>::FFmpegDataEncoder(
    const FFmpegLibWrapper* aLib, AVCodecID aCodecID,
    const RefPtr<TaskQueue>& aTaskQueue, const EncoderConfig& aConfig)
    : mLib(aLib),
      mCodecID(aCodecID),
      mTaskQueue(aTaskQueue),
      mConfig(aConfig),
      mCodecName(EmptyCString()),
      mCodecContext(nullptr),
      mFrame(nullptr),
      mVideoCodec(IsVideoCodec(aCodecID)) {
  MOZ_ASSERT(mLib);
  MOZ_ASSERT(mTaskQueue);
#if LIBAVCODEC_VERSION_MAJOR < 58
  MOZ_CRASH("FFmpegDataEncoder needs ffmpeg 58 at least.");
#endif
};

RefPtr<MediaDataEncoder::InitPromise> FFmpegDataEncoder<LIBAV_VER>::Init() {
  FFMPEG_LOG("Init");
  return InvokeAsync(mTaskQueue, this, __func__,
                     &FFmpegDataEncoder::ProcessInit);
}

RefPtr<MediaDataEncoder::EncodePromise> FFmpegDataEncoder<LIBAV_VER>::Encode(
    const MediaData* aSample) {
  MOZ_ASSERT(aSample != nullptr);

  FFMPEG_LOG("Encode");
  return InvokeAsync(mTaskQueue, __func__,
                     [self = RefPtr<FFmpegDataEncoder<LIBAV_VER>>(this),
                      sample = RefPtr<const MediaData>(aSample)]() {
                       return self->ProcessEncode(sample);
                     });
}

RefPtr<MediaDataEncoder::ReconfigurationPromise>
FFmpegDataEncoder<LIBAV_VER>::Reconfigure(
    const RefPtr<const EncoderConfigurationChangeList>& aConfigurationChanges) {
  return InvokeAsync<const RefPtr<const EncoderConfigurationChangeList>>(
      mTaskQueue, this, __func__,
      &FFmpegDataEncoder<LIBAV_VER>::ProcessReconfigure, aConfigurationChanges);
}

RefPtr<MediaDataEncoder::EncodePromise> FFmpegDataEncoder<LIBAV_VER>::Drain() {
  FFMPEG_LOG("Drain");
  return InvokeAsync(mTaskQueue, this, __func__,
                     &FFmpegDataEncoder::ProcessDrain);
}

RefPtr<ShutdownPromise> FFmpegDataEncoder<LIBAV_VER>::Shutdown() {
  FFMPEG_LOG("Shutdown");
  return InvokeAsync(mTaskQueue, this, __func__,
                     &FFmpegDataEncoder::ProcessShutdown);
}

RefPtr<GenericPromise> FFmpegDataEncoder<LIBAV_VER>::SetBitrate(
    uint32_t aBitrate) {
  FFMPEG_LOG("SetBitrate");
  return GenericPromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
}

RefPtr<MediaDataEncoder::InitPromise>
FFmpegDataEncoder<LIBAV_VER>::ProcessInit() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEG_LOG("ProcessInit");
  nsresult rv = InitSpecific();
  return NS_FAILED(rv) ? InitPromise::CreateAndReject(rv, __func__)
                       : InitPromise::CreateAndResolve(true, __func__);
}

RefPtr<MediaDataEncoder::EncodePromise>
FFmpegDataEncoder<LIBAV_VER>::ProcessEncode(RefPtr<const MediaData> aSample) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEG_LOG("ProcessEncode");

#if LIBAVCODEC_VERSION_MAJOR < 58
  // TODO(Bug 1868253): implement encode with avcodec_encode_video2().
  MOZ_CRASH("FFmpegDataEncoder needs ffmpeg 58 at least.");
  return EncodePromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
#else

  auto rv = EncodeInputWithModernAPIs(std::move(aSample));
  if (rv.isErr()) {
    return EncodePromise::CreateAndReject(rv.inspectErr(), __func__);
  }

  return EncodePromise::CreateAndResolve(rv.unwrap(), __func__);
#endif
}

RefPtr<MediaDataEncoder::ReconfigurationPromise>
FFmpegDataEncoder<LIBAV_VER>::ProcessReconfigure(
    const RefPtr<const EncoderConfigurationChangeList>& aConfigurationChanges) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEG_LOG("ProcessReconfigure");

  // Tracked in bug 1869583 -- for now this encoder always reports it cannot be
  // reconfigured on the fly
  return MediaDataEncoder::ReconfigurationPromise::CreateAndReject(
      NS_ERROR_NOT_IMPLEMENTED, __func__);
}

RefPtr<MediaDataEncoder::EncodePromise>
FFmpegDataEncoder<LIBAV_VER>::ProcessDrain() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEG_LOG("ProcessDrain");

#if LIBAVCODEC_VERSION_MAJOR < 58
  MOZ_CRASH("FFmpegDataEncoder needs ffmpeg 58 at least.");
  return EncodePromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
#else
  auto rv = DrainWithModernAPIs();
  if (rv.isErr()) {
    return EncodePromise::CreateAndReject(rv.inspectErr(), __func__);
  }
  return EncodePromise::CreateAndResolve(rv.unwrap(), __func__);
#endif
}

RefPtr<ShutdownPromise> FFmpegDataEncoder<LIBAV_VER>::ProcessShutdown() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEG_LOG("ProcessShutdown");

  ShutdownInternal();

  // Don't shut mTaskQueue down since it's owned by others.
  return ShutdownPromise::CreateAndResolve(true, __func__);
}

AVCodec* FFmpegDataEncoder<LIBAV_VER>::InitCommon() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEG_LOG("FFmpegDataEncoder::InitCommon");

  AVCodec* codec = FindEncoderWithPreference(mLib, mCodecID);
  if (!codec) {
    FFMPEG_LOG("failed to find ffmpeg encoder for codec id %d", mCodecID);
    return nullptr;
  }
  FFMPEG_LOG("found codec: %s", codec->name);
  mCodecName = codec->name;

  ForceEnablingFFmpegDebugLogs();

  MOZ_ASSERT(!mCodecContext);
  if (!(mCodecContext = mLib->avcodec_alloc_context3(codec))) {
    FFMPEG_LOG("failed to allocate ffmpeg context for codec %s", codec->name);
    return nullptr;
  }

  return codec;
}

MediaResult FFmpegDataEncoder<LIBAV_VER>::FinishInitCommon(AVCodec* aCodec) {
  mCodecContext->bit_rate = static_cast<FFmpegBitRate>(mConfig.mBitrate);
#if LIBAVCODEC_VERSION_MAJOR >= 60
  mCodecContext->flags |= AV_CODEC_FLAG_FRAME_DURATION;
#endif

  AVDictionary* options = nullptr;
  if (int ret = OpenCodecContext(aCodec, &options); ret < 0) {
    FFMPEG_LOG("failed to open %s avcodec: %s", aCodec->name,
                MakeErrorString(mLib, ret).get());
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("avcodec_open2 error"));
  }
  mLib->av_dict_free(&options);

  return MediaResult(NS_OK);
}

void FFmpegDataEncoder<LIBAV_VER>::ShutdownInternal() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEG_LOG("ShutdownInternal");

  DestroyFrame();

  if (mCodecContext) {
    CloseCodecContext();
    mLib->av_freep(&mCodecContext);
    mCodecContext = nullptr;
  }
}

int FFmpegDataEncoder<LIBAV_VER>::OpenCodecContext(const AVCodec* aCodec,
                                                   AVDictionary** aOptions) {
  MOZ_ASSERT(mCodecContext);

  StaticMutexAutoLock mon(sMutex);
  return mLib->avcodec_open2(mCodecContext, aCodec, aOptions);
}

void FFmpegDataEncoder<LIBAV_VER>::CloseCodecContext() {
  MOZ_ASSERT(mCodecContext);

  StaticMutexAutoLock mon(sMutex);
  mLib->avcodec_close(mCodecContext);
}

bool FFmpegDataEncoder<LIBAV_VER>::PrepareFrame() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  // TODO: Merge the duplicate part with FFmpegDataDecoder's PrepareFrame.
#if LIBAVCODEC_VERSION_MAJOR >= 55
  if (mFrame) {
    mLib->av_frame_unref(mFrame);
  } else {
    mFrame = mLib->av_frame_alloc();
  }
#elif LIBAVCODEC_VERSION_MAJOR == 54
  if (mFrame) {
    mLib->avcodec_get_frame_defaults(mFrame);
  } else {
    mFrame = mLib->avcodec_alloc_frame();
  }
#else
  mLib->av_freep(&mFrame);
  mFrame = mLib->avcodec_alloc_frame();
#endif
  return !!mFrame;
}

void FFmpegDataEncoder<LIBAV_VER>::DestroyFrame() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  if (mFrame) {
#if LIBAVCODEC_VERSION_MAJOR >= 55
    mLib->av_frame_unref(mFrame);
    mLib->av_frame_free(&mFrame);
#elif LIBAVCODEC_VERSION_MAJOR == 54
    mLib->avcodec_free_frame(&mFrame);
#else
    mLib->av_freep(&mFrame);
#endif
    mFrame = nullptr;
  }
}

// avcodec_send_frame and avcodec_receive_packet were introduced in version 58.
#if LIBAVCODEC_VERSION_MAJOR >= 58
Result<MediaDataEncoder::EncodedData, nsresult>
FFmpegDataEncoder<LIBAV_VER>::EncodeWithModernAPIs() {
  // Initialize AVPacket.
  AVPacket* pkt = mLib->av_packet_alloc();

  if (!pkt) {
    FFMPEG_LOG("failed to allocate packet");
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }

  auto freePacket = MakeScopeExit([this, &pkt] { mLib->av_packet_free(&pkt); });

  // Send frame and receive packets.
  if (int ret = mLib->avcodec_send_frame(mCodecContext, mFrame); ret < 0) {
    // In theory, avcodec_send_frame could sent -EAGAIN to signal its internal
    // buffers is full. In practice this can't happen as we only feed one frame
    // at a time, and we immediately call avcodec_receive_packet right after.
    // TODO: Create a NS_ERROR_DOM_MEDIA_ENCODE_ERR in ErrorList.py?
    FFMPEG_LOG("avcodec_send_frame error: %s",
                MakeErrorString(mLib, ret).get());
    return Err(NS_ERROR_DOM_MEDIA_FATAL_ERR);
  }

  EncodedData output;
  while (true) {
    int ret = mLib->avcodec_receive_packet(mCodecContext, pkt);
    if (ret == AVERROR(EAGAIN)) {
      // The encoder is asking for more inputs.
      FFMPEG_LOG("encoder is asking for more input!");
      break;
    }

    if (ret < 0) {
      // AVERROR_EOF is returned when the encoder has been fully flushed, but it
      // shouldn't happen here.
      FFMPEG_LOG("avcodec_receive_packet error: %s",
                  MakeErrorString(mLib, ret).get());
      return Err(NS_ERROR_DOM_MEDIA_FATAL_ERR);
    }

    RefPtr<MediaRawData> d = ToMediaRawData(pkt);
    mLib->av_packet_unref(pkt);
    if (!d) {
      FFMPEGV_LOG("failed to create a MediaRawData from the AVPacket");
      return Result<MediaDataEncoder::EncodedData, nsresult>(
          NS_ERROR_DOM_MEDIA_FATAL_ERR);
    }
    output.AppendElement(std::move(d));
  }

  FFMPEG_LOG("Got %zu encoded data", output.Length());
  return std::move(output);
}

Result<MediaDataEncoder::EncodedData, nsresult>
FFmpegDataEncoder<LIBAV_VER>::DrainWithModernAPIs() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  MOZ_ASSERT(mCodecContext);

  // TODO: Create a Result<EncodedData, nsresult> EncodeWithModernAPIs(AVFrame
  // *aFrame) to merge the duplicate code below with EncodeWithModernAPIs above.

  // Initialize AVPacket.
  AVPacket* pkt = mLib->av_packet_alloc();
  if (!pkt) {
    FFMPEG_LOG("failed to allocate packet");
    return Err(NS_ERROR_DOM_MEDIA_FATAL_ERR);
  }
  auto freePacket = MakeScopeExit([this, &pkt] { mLib->av_packet_free(&pkt); });

  // Enter draining mode by sending NULL to the avcodec_send_frame(). Note that
  // this can leave the encoder in a permanent EOF state after draining. As a
  // result, the encoder is unable to continue encoding. A new
  // AVCodecContext/encoder creation is required if users need to encode after
  // draining.
  //
  // TODO: Use `avcodec_flush_buffers` to drain the pending packets if
  // AV_CODEC_CAP_ENCODER_FLUSH is set in mCodecContext->codec->capabilities.
  if (int ret = mLib->avcodec_send_frame(mCodecContext, nullptr); ret < 0) {
    if (ret == AVERROR_EOF) {
      // The encoder has been flushed. Drain can be called multiple time.
      FFMPEG_LOG("encoder has been flushed!");
      return EncodedData();
    }

    FFMPEG_LOG("avcodec_send_frame error: %s",
                MakeErrorString(mLib, ret).get());
    return Err(NS_ERROR_DOM_MEDIA_FATAL_ERR);
  }

  EncodedData output;
  while (true) {
    int ret = mLib->avcodec_receive_packet(mCodecContext, pkt);
    if (ret == AVERROR_EOF) {
      FFMPEG_LOG("encoder has no more output packet!");
      break;
    }

    if (ret < 0) {
      // avcodec_receive_packet should not result in a -EAGAIN once it's in
      // draining mode.
      FFMPEG_LOG("avcodec_receive_packet error: %s",
                  MakeErrorString(mLib, ret).get());
      return Err(NS_ERROR_DOM_MEDIA_FATAL_ERR);
    }

    RefPtr<MediaRawData> d = ToMediaRawData(pkt);
    mLib->av_packet_unref(pkt);
    if (!d) {
      FFMPEG_LOG("failed to create a MediaRawData from the AVPacket");
      return Err(NS_ERROR_DOM_MEDIA_FATAL_ERR);
    }
    output.AppendElement(std::move(d));
  }

  FFMPEG_LOG("Encoding successful, %zu packets", output.Length());

  // TODO: Evaluate a better solution (Bug 1869466)
  // TODO: Only re-create AVCodecContext when avcodec_flush_buffers is
  // unavailable.
  ShutdownInternal();
  nsresult r = InitSpecific();
  return NS_FAILED(r) ? Result<MediaDataEncoder::EncodedData, nsresult>(
                            NS_ERROR_DOM_MEDIA_FATAL_ERR)
                      : Result<MediaDataEncoder::EncodedData, nsresult>(
                            std::move(output));
}
#endif  // LIBAVCODEC_VERSION_MAJOR >= 58

RefPtr<MediaRawData> FFmpegDataEncoder<LIBAV_VER>::ToMediaRawDataCommon(
    AVPacket* aPacket) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  MOZ_ASSERT(aPacket);

  // Copy frame data from AVPacket.
  auto data = MakeRefPtr<MediaRawData>();
  UniquePtr<MediaRawDataWriter> writer(data->CreateWriter());
  if (!writer->Append(aPacket->data, static_cast<size_t>(aPacket->size))) {
    FFMPEG_LOG("fail to allocate MediaRawData buffer");
    return nullptr;  // OOM
  }

  data->mKeyframe = (aPacket->flags & AV_PKT_FLAG_KEY) != 0;
  // TODO(bug 1869560): The unit of pts, dts, and duration is time_base, which
  // is recommended to be the reciprocal of the frame rate, but we set it to
  // microsecond for now.
  data->mTime = media::TimeUnit::FromMicroseconds(aPacket->pts);
#if LIBAVCODEC_VERSION_MAJOR >= 60
  data->mDuration = media::TimeUnit::FromMicroseconds(aPacket->duration);
#else
  int64_t duration;
  if (mDurationMap.Find(aPacket->pts, duration)) {
    data->mDuration = media::TimeUnit::FromMicroseconds(duration);
  } else {
    data->mDuration = media::TimeUnit::FromMicroseconds(aPacket->duration);
  }
#endif
  data->mTimecode = media::TimeUnit::FromMicroseconds(aPacket->dts);

  if (auto r = GetExtraData(aPacket); r.isOk()) {
    data->mExtraData = r.unwrap();
  }

  return data;
}
void FFmpegDataEncoder<LIBAV_VER>::ForceEnablingFFmpegDebugLogs() {
#if DEBUG
  if (!getenv("MOZ_AV_LOG_LEVEL") &&
      MOZ_LOG_TEST(sFFmpegVideoLog, LogLevel::Debug)) {
    mLib->av_log_set_level(AV_LOG_DEBUG);
  }
#endif  // DEBUG
}

}  // namespace mozilla
