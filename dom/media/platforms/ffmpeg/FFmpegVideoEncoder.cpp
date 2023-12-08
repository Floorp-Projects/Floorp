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

template <typename ConfigType>
FFmpegVideoEncoder<LIBAV_VER, ConfigType>::FFmpegVideoEncoder(
    const FFmpegLibWrapper* aLib, AVCodecID aCodecID,
    RefPtr<TaskQueue> aTaskQueue, const ConfigType& aConfig)
    : mLib(aLib),
      mCodecID(aCodecID),
      mTaskQueue(aTaskQueue),
      mConfig(aConfig),
      mCodecContext(nullptr) {
  MOZ_ASSERT(mLib);
  MOZ_ASSERT(mTaskQueue);
};

template <typename ConfigType>
RefPtr<MediaDataEncoder::InitPromise>
FFmpegVideoEncoder<LIBAV_VER, ConfigType>::Init() {
  FFMPEGV_LOG("Init");
  return InvokeAsync(mTaskQueue, this, __func__,
                     &FFmpegVideoEncoder::ProcessInit);
}

template <typename ConfigType>
RefPtr<MediaDataEncoder::EncodePromise>
FFmpegVideoEncoder<LIBAV_VER, ConfigType>::Encode(const MediaData* aSample) {
  FFMPEGV_LOG("Encode");
  return EncodePromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
}

template <typename ConfigType>
RefPtr<MediaDataEncoder::EncodePromise>
FFmpegVideoEncoder<LIBAV_VER, ConfigType>::Drain() {
  FFMPEGV_LOG("Drain");
  return EncodePromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
}

template <typename ConfigType>
RefPtr<ShutdownPromise> FFmpegVideoEncoder<LIBAV_VER, ConfigType>::Shutdown() {
  FFMPEGV_LOG("Shutdown");
  RefPtr<FFmpegVideoEncoder<LIBAV_VER, ConfigType>> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self]() {
    self->ProcessShutdown();
    return self->mTaskQueue->BeginShutdown();
  });
}

template <typename ConfigType>
RefPtr<GenericPromise> FFmpegVideoEncoder<LIBAV_VER, ConfigType>::SetBitrate(
    Rate aBitsPerSec) {
  FFMPEGV_LOG("SetBitrate");
  return GenericPromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
}

template <typename ConfigType>
nsCString FFmpegVideoEncoder<LIBAV_VER, ConfigType>::GetDescriptionName()
    const {
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

template <typename ConfigType>
RefPtr<MediaDataEncoder::InitPromise>
FFmpegVideoEncoder<LIBAV_VER, ConfigType>::ProcessInit() {
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

template <typename ConfigType>
void FFmpegVideoEncoder<LIBAV_VER, ConfigType>::ProcessShutdown() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEGV_LOG("ProcessShutdown");

  if (mCodecContext) {
    mLib->av_freep(&mCodecContext);
    mCodecContext = nullptr;
  }
}

template class FFmpegVideoEncoder<LIBAV_VER, MediaDataEncoder::VP8Config>;
template class FFmpegVideoEncoder<LIBAV_VER, MediaDataEncoder::VP9Config>;

}  // namespace mozilla
