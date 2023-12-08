/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegVideoEncoder.h"

#include "FFmpegLog.h"
#include "FFmpegRuntimeLinker.h"
#include "VPXDecoder.h"
#include "nsPrintfCString.h"

namespace mozilla {

template <>
AVCodecID GetFFmpegEncoderCodecId<LIBAV_VER>(const nsACString& aMimeType) {
#if LIBAVCODEC_VERSION_MAJOR >= 54
  if (VPXDecoder::IsVP8(aMimeType)) {
    return AV_CODEC_ID_VP8;
  }
#endif

#if LIBAVCODEC_VERSION_MAJOR >= 55
  if (VPXDecoder::IsVP9(aMimeType)) {
    return AV_CODEC_ID_VP9;
  }
#endif
  return AV_CODEC_ID_NONE;
}

FFmpegVideoEncoder<LIBAV_VER>::FFmpegVideoEncoder(const FFmpegLibWrapper* aLib,
                                                  AVCodecID aCodecID,
                                                  RefPtr<TaskQueue> aTaskQueue)
    : mLib(aLib), mCodecID(aCodecID), mTaskQueue(aTaskQueue), mCodecContext(nullptr) {
  MOZ_ASSERT(mLib);
  MOZ_ASSERT(mTaskQueue);
};

RefPtr<MediaDataEncoder::InitPromise> FFmpegVideoEncoder<LIBAV_VER>::Init() {
  FFMPEGV_LOG("Init");
  return InvokeAsync(mTaskQueue, this, __func__,
                     &FFmpegVideoEncoder::ProcessInit);
}

RefPtr<MediaDataEncoder::EncodePromise> FFmpegVideoEncoder<LIBAV_VER>::Encode(
    const MediaData* aSample) {
  FFMPEGV_LOG("Encode");
  return EncodePromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
}

RefPtr<MediaDataEncoder::EncodePromise> FFmpegVideoEncoder<LIBAV_VER>::Drain() {
  FFMPEGV_LOG("Drain");
  return EncodePromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
}

RefPtr<ShutdownPromise> FFmpegVideoEncoder<LIBAV_VER>::Shutdown() {
  FFMPEGV_LOG("Shutdown");
  RefPtr<FFmpegVideoEncoder<LIBAV_VER>> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self]() {
    self->ProcessShutdown();
    return self->mTaskQueue->BeginShutdown();
  });
}

RefPtr<GenericPromise> FFmpegVideoEncoder<LIBAV_VER>::SetBitrate(
    Rate aBitsPerSec) {
  FFMPEGV_LOG("SetBitrate");
  return GenericPromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
}

nsCString FFmpegVideoEncoder<LIBAV_VER>::GetDescriptionName() const {
#ifdef USING_MOZFFVPX
  return "ffvpx video encoder"_ns;
#else
  const char* lib =
#  if defined(MOZ_FFMPEG)
      FFmpegRuntimeLinker::LinkStatusLibraryName();
#  else
      "no library: ffmpeg disabled during build";
#  endif
  return nsPrintfCString("ffmpeg video encoder (%s)", lib);
#endif
}

RefPtr<MediaDataEncoder::InitPromise>
FFmpegVideoEncoder<LIBAV_VER>::ProcessInit() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEGV_LOG("ProcessInit");

  AVCodec* codec = mLib->avcodec_find_encoder(mCodecID);
  if (!codec) {
    FFMPEGV_LOG("failed to find ffmpeg encoder for codec id %d", mCodecID);
    return InitPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Unable to find codec")),
        __func__);
  }
  FFMPEGV_LOG("find codec: %s", codec->name);

  MOZ_ASSERT(!mCodecContext);
  if (!(mCodecContext = mLib->avcodec_alloc_context3(codec))) {
    FFMPEGV_LOG("failed to allocate ffmpeg context for codec %s", codec->name);
    return InitPromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY,
                    RESULT_DETAIL("Failed to initialize ffmpeg context")),
        __func__);
  }

  // TODO: setting mCodecContext.

  FFMPEGV_LOG("%s has been initialized", codec->name);
  return InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
}

void FFmpegVideoEncoder<LIBAV_VER>::ProcessShutdown() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEGV_LOG("ProcessShutdown");

  if (mCodecContext) {
    mLib->av_freep(&mCodecContext);
    mCodecContext = nullptr;
  }
}

}  // namespace mozilla
