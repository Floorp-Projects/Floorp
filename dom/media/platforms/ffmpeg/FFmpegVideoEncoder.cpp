/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegVideoEncoder.h"

#include "FFmpegLog.h"
#include "FFmpegRuntimeLinker.h"
#include "VPXDecoder.h"
#include "libavutil/pixfmt.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "nsPrintfCString.h"

// The ffmpeg namespace is introduced to avoid the PixelFormat's name conflicts
// with MediaDataEncoder::PixelFormat in MediaDataEncoder class scope.
namespace ffmpeg {

#if LIBAVCODEC_VERSION_MAJOR < 54
using FFmpegPixelFormat = enum PixelFormat;
const FFmpegPixelFormat FFMPEG_PIX_FMT_NONE = FFmpegPixelFormat::PIX_FMT_NONE;
const FFmpegPixelFormat FFMPEG_PIX_FMT_RGBA = FFmpegPixelFormat::PIX_FMT_RGBA;
const FFmpegPixelFormat FFMPEG_PIX_FMT_BGRA = FFmpegPixelFormat::PIX_FMT_BGRA;
const FFmpegPixelFormat FFMPEG_PIX_FMT_RGB24 = FFmpegPixelFormat::PIX_FMT_RGB24;
const FFmpegPixelFormat FFMPEG_PIX_FMT_BGR24 = FFmpegPixelFormat::PIX_FMT_BGR24;
const FFmpegPixelFormat FFMPEG_PIX_FMT_YUV444P =
    FFmpegPixelFormat::PIX_FMT_YUV444P;
const FFmpegPixelFormat FFMPEG_PIX_FMT_YUV422P =
    FFmpegPixelFormat::PIX_FMT_YUV422P;
const FFmpegPixelFormat FFMPEG_PIX_FMT_YUV420P =
    FFmpegPixelFormat::PIX_FMT_YUV420P;
const FFmpegPixelFormat FFMPEG_PIX_FMT_NV12 = FFmpegPixelFormat::PIX_FMT_NV12;
const FFmpegPixelFormat FFMPEG_PIX_FMT_NV21 = FFmpegPixelFormat::PIX_FMT_NV21;
#else
using FFmpegPixelFormat = enum AVPixelFormat;
const FFmpegPixelFormat FFMPEG_PIX_FMT_NONE =
    FFmpegPixelFormat::AV_PIX_FMT_NONE;
const FFmpegPixelFormat FFMPEG_PIX_FMT_RGBA =
    FFmpegPixelFormat::AV_PIX_FMT_RGBA;
const FFmpegPixelFormat FFMPEG_PIX_FMT_BGRA =
    FFmpegPixelFormat::AV_PIX_FMT_BGRA;
const FFmpegPixelFormat FFMPEG_PIX_FMT_RGB24 =
    FFmpegPixelFormat::AV_PIX_FMT_RGB24;
const FFmpegPixelFormat FFMPEG_PIX_FMT_BGR24 =
    FFmpegPixelFormat::AV_PIX_FMT_BGR24;
const FFmpegPixelFormat FFMPEG_PIX_FMT_YUV444P =
    FFmpegPixelFormat::AV_PIX_FMT_YUV444P;
const FFmpegPixelFormat FFMPEG_PIX_FMT_YUV422P =
    FFmpegPixelFormat::AV_PIX_FMT_YUV422P;
const FFmpegPixelFormat FFMPEG_PIX_FMT_YUV420P =
    FFmpegPixelFormat::AV_PIX_FMT_YUV420P;
const FFmpegPixelFormat FFMPEG_PIX_FMT_NV12 =
    FFmpegPixelFormat::AV_PIX_FMT_NV12;
const FFmpegPixelFormat FFMPEG_PIX_FMT_NV21 =
    FFmpegPixelFormat::AV_PIX_FMT_NV21;
#endif

// TODO: WebCodecs' I420A should map to MediaDataEncoder::PixelFormat and then
// to AV_PIX_FMT_YUVA420P here.
static FFmpegPixelFormat ToSupportedFFmpegPixelFormat(
    const mozilla::dom::ImageBitmapFormat& aFormat) {
  switch (aFormat) {
    case mozilla::dom::ImageBitmapFormat::RGBA32:
      return FFMPEG_PIX_FMT_RGBA;
    case mozilla::dom::ImageBitmapFormat::BGRA32:
      return FFMPEG_PIX_FMT_BGRA;
    case mozilla::dom::ImageBitmapFormat::RGB24:
      return FFMPEG_PIX_FMT_RGB24;
    case mozilla::dom::ImageBitmapFormat::BGR24:
      return FFMPEG_PIX_FMT_BGR24;
    case mozilla::dom::ImageBitmapFormat::YUV444P:
      return FFMPEG_PIX_FMT_YUV444P;
    case mozilla::dom::ImageBitmapFormat::YUV422P:
      return FFMPEG_PIX_FMT_YUV422P;
    case mozilla::dom::ImageBitmapFormat::YUV420P:
      return FFMPEG_PIX_FMT_YUV420P;
    case mozilla::dom::ImageBitmapFormat::YUV420SP_NV12:
      return FFMPEG_PIX_FMT_NV12;
    case mozilla::dom::ImageBitmapFormat::YUV420SP_NV21:
      return FFMPEG_PIX_FMT_NV21;
    case mozilla::dom::ImageBitmapFormat::GRAY8:
      // Although GRAY8 can be map to a AVPixelFormat, it's unsupported for now.
    case mozilla::dom::ImageBitmapFormat::HSV:
    case mozilla::dom::ImageBitmapFormat::Lab:
    case mozilla::dom::ImageBitmapFormat::DEPTH:
    case mozilla::dom::ImageBitmapFormat::EndGuard_:
      break;
  }
  return FFMPEG_PIX_FMT_NONE;
}

static const char* GetPixelFormatString(FFmpegPixelFormat aFormat) {
  switch (aFormat) {
    case FFMPEG_PIX_FMT_NONE:
      return "none";
    case FFMPEG_PIX_FMT_RGBA:
      return "packed RGBA 8:8:8:8 (32bpp, RGBARGBA...)";
    case FFMPEG_PIX_FMT_BGRA:
      return "packed BGRA 8:8:8:8 (32bpp, BGRABGRA...)";
    case FFMPEG_PIX_FMT_RGB24:
      return "packed RGB 8:8:8 (24bpp, RGBRGB...)";
    case FFMPEG_PIX_FMT_BGR24:
      return "packed RGB 8:8:8 (24bpp, BGRBGR...)";
    case FFMPEG_PIX_FMT_YUV444P:
      return "planar YUV 4:4:4 (24bpp, 1 Cr & Cb sample per 1x1 Y samples)";
    case FFMPEG_PIX_FMT_YUV422P:
      return "planar YUV 4:2:2 (16bpp, 1 Cr & Cb sample per 2x1 Y samples)";
    case FFMPEG_PIX_FMT_YUV420P:
      return "planar YUV 4:2:0 (12bpp, 1 Cr & Cb sample per 2x2 Y samples)";
    case FFMPEG_PIX_FMT_NV12:
      return "planar YUV 4:2:0 (12bpp, 1 interleaved UV components per 1x1 Y "
             "samples)";
    case FFMPEG_PIX_FMT_NV21:
      return "planar YUV 4:2:0 (12bpp, 1 interleaved VU components per 1x1 Y "
             "samples)";
    default:
      break;
  }
  MOZ_ASSERT_UNREACHABLE("Unsupported pixel format");
  return "unsupported";
}

};  // namespace ffmpeg

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

  ffmpeg::FFmpegPixelFormat fmt =
      ffmpeg::ToSupportedFFmpegPixelFormat(mConfig.mSourcePixelFormat);
  if (fmt == ffmpeg::FFMPEG_PIX_FMT_NONE) {
    FFMPEGV_LOG(
        "%s is unsupported format",
        dom::ImageBitmapFormatValues::GetString(mConfig.mSourcePixelFormat)
            .data());
    return InitPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR,
                    RESULT_DETAIL("Pixel format is not supported")),
        __func__);
  }

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
