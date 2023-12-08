/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegVideoEncoder.h"

#include "FFmpegLog.h"
#include "FFmpegRuntimeLinker.h"
#include "ImageContainer.h"
#include "VPXDecoder.h"
#include "libavutil/error.h"
#include "libavutil/pixfmt.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/PodOperations.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/ImageUtils.h"
#include "nsPrintfCString.h"

// The ffmpeg namespace is introduced to avoid the PixelFormat's name conflicts
// with MediaDataEncoder::PixelFormat in MediaDataEncoder class scope.
namespace ffmpeg {

#if LIBAVCODEC_VERSION_MAJOR >= 57
using FFmpegBitRate = int64_t;
constexpr size_t FFmpegErrorMaxStringSize = AV_ERROR_MAX_STRING_SIZE;
#else
using FFmpegBitRate = int;
constexpr size_t FFmpegErrorMaxStringSize = 64;
#endif

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

static nsCString MakeErrorString(const FFmpegLibWrapper* aLib, int aErrNum) {
  MOZ_ASSERT(aLib);

  char errStr[ffmpeg::FFmpegErrorMaxStringSize];
  aLib->av_strerror(aErrNum, errStr, ffmpeg::FFmpegErrorMaxStringSize);
  return nsCString(errStr);
}

template <>
AVCodecID GetFFmpegEncoderCodecId<LIBAV_VER>(const nsACString& aMimeType) {
#if LIBAVCODEC_VERSION_MAJOR >= 58
  if (VPXDecoder::IsVP8(aMimeType)) {
    return AV_CODEC_ID_VP8;
  }

  if (VPXDecoder::IsVP9(aMimeType)) {
    return AV_CODEC_ID_VP9;
  }
#endif
  return AV_CODEC_ID_NONE;
}

template <typename ConfigType>
StaticMutex FFmpegVideoEncoder<LIBAV_VER, ConfigType>::sMutex;

template <typename ConfigType>
FFmpegVideoEncoder<LIBAV_VER, ConfigType>::FFmpegVideoEncoder(
    const FFmpegLibWrapper* aLib, AVCodecID aCodecID,
    RefPtr<TaskQueue> aTaskQueue, const ConfigType& aConfig)
    : mLib(aLib),
      mCodecID(aCodecID),
      mTaskQueue(aTaskQueue),
      mConfig(aConfig),
      mCodecContext(nullptr),
      mFrame(nullptr) {
  MOZ_ASSERT(mLib);
  MOZ_ASSERT(mTaskQueue);
};

template <typename ConfigType>
RefPtr<MediaDataEncoder::InitPromise>
FFmpegVideoEncoder<LIBAV_VER, ConfigType>::Init() {
  FFMPEGV_LOG("Init");

#if LIBAVCODEC_VERSION_MAJOR < 58
  FFMPEGV_LOG("FFmpegVideoEncoder needs ffmpeg 58 at least.");
  return InitPromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
#else
  return InvokeAsync(mTaskQueue, this, __func__,
                     &FFmpegVideoEncoder::ProcessInit);
#endif
}

template <typename ConfigType>
RefPtr<MediaDataEncoder::EncodePromise>
FFmpegVideoEncoder<LIBAV_VER, ConfigType>::Encode(const MediaData* aSample) {
  MOZ_ASSERT(aSample != nullptr);

  FFMPEGV_LOG("Encode");

#if LIBAVCODEC_VERSION_MAJOR < 58
  FFMPEGV_LOG("FFmpegVideoEncoder needs ffmpeg 58 at least.");
  return EncodePromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
#else
  return InvokeAsync(
      mTaskQueue, __func__,
      [self = RefPtr<FFmpegVideoEncoder<LIBAV_VER, ConfigType> >(this),
       sample = RefPtr<const MediaData>(aSample)]() {
        return self->ProcessEncode(std::move(sample));
      });
#endif
}

template <typename ConfigType>
RefPtr<MediaDataEncoder::EncodePromise>
FFmpegVideoEncoder<LIBAV_VER, ConfigType>::Drain() {
  FFMPEGV_LOG("Drain");

#if LIBAVCODEC_VERSION_MAJOR < 58
  FFMPEGV_LOG("FFmpegVideoEncoder needs ffmpeg 58 at least.");
  return EncodePromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
#else
  return InvokeAsync(mTaskQueue, this, __func__,
                     &FFmpegVideoEncoder::ProcessDrain);
#endif
}

template <typename ConfigType>
RefPtr<ShutdownPromise> FFmpegVideoEncoder<LIBAV_VER, ConfigType>::Shutdown() {
  FFMPEGV_LOG("Shutdown");

#if LIBAVCODEC_VERSION_MAJOR < 58
  FFMPEGV_LOG("FFmpegVideoEncoder needs ffmpeg 58 at least.");
  return mTaskQueue->BeginShutdown();
#else
  RefPtr<FFmpegVideoEncoder<LIBAV_VER, ConfigType> > self = this;
  return InvokeAsync(mTaskQueue, __func__, [self]() {
    self->ProcessShutdown();
    return self->mTaskQueue->BeginShutdown();
  });
#endif
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

  // Set up AVCodecContext.
  mCodecContext->pix_fmt = fmt;
  mCodecContext->bit_rate =
      static_cast<ffmpeg::FFmpegBitRate>(mConfig.mBitsPerSec);
  mCodecContext->width = static_cast<int>(mConfig.mSize.width);
  mCodecContext->height = static_cast<int>(mConfig.mSize.height);
  mCodecContext->time_base =
      AVRational{.num = 1, .den = static_cast<int>(mConfig.mFramerate)};
#if LIBAVCODEC_VERSION_MAJOR >= 57
  mCodecContext->framerate =
      AVRational{.num = static_cast<int>(mConfig.mFramerate), .den = 1};
#endif
  mCodecContext->gop_size = static_cast<int>(mConfig.mKeyframeInterval);
  // TODO: keyint_min, max_b_frame?
  // TODO: VPX specific settings
  // - if mConfig.mDenoising is set: av_opt_set_int(mCodecContext->priv_data,
  // "noise_sensitivity", x, 0), where the x is from 0(disabled) to 6.
  // - if mConfig.mAdaptiveQp is set: av_opt_set_int(mCodecContext->priv_data,
  // "aq_mode", x, 0), where x is from 0 to 3: 0 - Disabled, 1 - Variance
  // AQ(default), 2 - Complexity AQ, 3 - Cycle AQ.
  // - if min and max rates are known (VBR?),
  // av_opt_set(mCodecContext->priv_data, "minrate", x, 0) and
  // av_opt_set(mCodecContext->priv_data, "maxrate", y, 0)
  // - For real time usage:
  // av_opt_set(mCodecContext->priv_data, "deadline", "realtime", 0)
  // av_opt_set(mCodecContext->priv_data, "lag-in-frames", "0", 0)
  // av_opt_set(mCodecContext->priv_data, "target_bitrate", x, 0)
  // av_opt_set(mCodecContext->priv_data, "rc_buf_sz", x, 0)
  // TODO: H264 specific settings.
  // - for AVCC format: set mCodecContext->extradata and
  // mCodecContext->extradata_size.
  // - for AnnexB format: set mCodecContext->flags |=
  // AV_CODEC_FLAG_GLOBAL_HEADER for start code.
  // TODO: AV1 specific settings.

  AVDictionary* options = nullptr;
  if (int ret = OpenCodecContext(codec, &options); ret < 0) {
    FFMPEGV_LOG("failed to open %s avcodec: %s", codec->name,
                MakeErrorString(mLib, ret).get());
    return InitPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Unable to open avcodec")),
        __func__);
  }
  mLib->av_dict_free(&options);

  FFMPEGV_LOG("%s has been initialized with format: %s, bitrate: %" PRIi64
              ", width: %d, height: %d, time_base: %d/%d",
              codec->name, ffmpeg::GetPixelFormatString(mCodecContext->pix_fmt),
              static_cast<int64_t>(mCodecContext->bit_rate),
              mCodecContext->width, mCodecContext->height,
              mCodecContext->time_base.num, mCodecContext->time_base.den);

  return InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
}

template <typename ConfigType>
RefPtr<MediaDataEncoder::EncodePromise>
FFmpegVideoEncoder<LIBAV_VER, ConfigType>::ProcessEncode(
    RefPtr<const MediaData> aSample) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEGV_LOG("ProcessEncode");

#if LIBAVCODEC_VERSION_MAJOR < 58
  // TODO(Bug 1868253): implement encode with avcodec_encode_video2().
  FFMPEGV_LOG("FFmpegVideoEncoder needs ffmpeg 58 at least.");
  return EncodePromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
#else
  RefPtr<const VideoData> sample(aSample->As<const VideoData>());
  MOZ_ASSERT(sample);

  return EncodeWithModernAPIs(sample);
#endif
}

template <typename ConfigType>
RefPtr<MediaDataEncoder::EncodePromise>
FFmpegVideoEncoder<LIBAV_VER, ConfigType>::ProcessDrain() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEGV_LOG("ProcessDrain");

#if LIBAVCODEC_VERSION_MAJOR < 58
  FFMPEGV_LOG("FFmpegVideoEncoder needs ffmpeg 58 at least.");
  return EncodePromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
#else
  return DrainWithModernAPIs();
#endif
}

template <typename ConfigType>
void FFmpegVideoEncoder<LIBAV_VER, ConfigType>::ProcessShutdown() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEGV_LOG("ProcessShutdown");

  DestroyFrame();

  if (mCodecContext) {
    CloseCodecContext();
    mLib->av_freep(&mCodecContext);
    mCodecContext = nullptr;
  }
}

template <typename ConfigType>
int FFmpegVideoEncoder<LIBAV_VER, ConfigType>::OpenCodecContext(
    const AVCodec* aCodec, AVDictionary** aOptions) {
  MOZ_ASSERT(mCodecContext);

  StaticMutexAutoLock mon(sMutex);
  return mLib->avcodec_open2(mCodecContext, aCodec, aOptions);
}

template <typename ConfigType>
void FFmpegVideoEncoder<LIBAV_VER, ConfigType>::CloseCodecContext() {
  MOZ_ASSERT(mCodecContext);

  StaticMutexAutoLock mon(sMutex);
  mLib->avcodec_close(mCodecContext);
}

template <typename ConfigType>
bool FFmpegVideoEncoder<LIBAV_VER, ConfigType>::PrepareFrame() {
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

template <typename ConfigType>
void FFmpegVideoEncoder<LIBAV_VER, ConfigType>::DestroyFrame() {
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
// TODO: Bug 1868907 - Avoid copy in FFmpegVideoEncoder::Encode.
static bool CopyPlane(uint8_t* aDst, const uint8_t* aSrc,
                      const gfx::IntSize& aSize, int32_t aStride) {
  int32_t height = aSize.height;
  int32_t width = aSize.width;

  MOZ_RELEASE_ASSERT(width <= aStride);

  CheckedInt<size_t> size(height);
  size *= aStride;

  if (!size.isValid()) {
    return false;
  }

  PodCopy(aDst, aSrc, size.value());
  return true;
}

template <typename ConfigType>
RefPtr<MediaDataEncoder::EncodePromise>
FFmpegVideoEncoder<LIBAV_VER, ConfigType>::EncodeWithModernAPIs(
    RefPtr<const VideoData> aSample) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  MOZ_ASSERT(mCodecContext);
  MOZ_ASSERT(aSample);

  // Validate input.
  if (!aSample->mImage) {
    FFMPEGV_LOG("No image");
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_ILLEGAL_INPUT,
                    RESULT_DETAIL("No image in sample")),
        __func__);
  } else if (aSample->mImage->GetSize().IsEmpty()) {
    FFMPEGV_LOG("image width or height is invalid");
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_ILLEGAL_INPUT,
                    RESULT_DETAIL("Invalid image size")),
        __func__);
  }

  // Check image format.
  const dom::ImageUtils imageUtils(aSample->mImage);
  ffmpeg::FFmpegPixelFormat imgFmt =
      ffmpeg::ToSupportedFFmpegPixelFormat(imageUtils.GetFormat());
  if (imgFmt == ffmpeg::FFMPEG_PIX_FMT_NONE) {
    FFMPEGV_LOG(
        "image type %s is unsupported",
        dom::ImageBitmapFormatValues::GetString(imageUtils.GetFormat()).data());
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR,
                    RESULT_DETAIL("Unsupport image type")),
        __func__);
  }

  // Allocate AVFrame.
  if (!PrepareFrame()) {
    FFMPEGV_LOG("failed to allocate frame");
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY,
                    RESULT_DETAIL("Unable to allocate frame")),
        __func__);
  }

  // Set AVFrame properties for its internal data allocation.
  // TODO: Need to convert image data into mConfig.mSourcePixelFormat format if
  // their formats mismatch.
  mFrame->format = imgFmt;
  mFrame->width = static_cast<int>(aSample->mImage->GetSize().width);
  mFrame->height = static_cast<int>(aSample->mImage->GetSize().height);

  // Allocate AVFrame data.
  if (int ret = mLib->av_frame_get_buffer(mFrame, 0); ret < 0) {
    FFMPEGV_LOG("failed to allocate frame data: %s",
                MakeErrorString(mLib, ret).get());
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY,
                    RESULT_DETAIL("Unable to allocate frame data")),
        __func__);
  }

  // Make sure AVFrame is writable.
  if (int ret = mLib->av_frame_make_writable(mFrame); ret < 0) {
    FFMPEGV_LOG("failed to make frame writable: %s",
                MakeErrorString(mLib, ret).get());
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_NOT_AVAILABLE,
                    RESULT_DETAIL("Unable to make frame writable")),
        __func__);
  }

  // Fill AVFrame data.
  if (aSample->mImage->AsPlanarYCbCrImage()) {
    const layers::PlanarYCbCrData* data =
        aSample->mImage->AsPlanarYCbCrImage()->GetData();
    if (!data) {
      FFMPEGV_LOG("No data in YCbCr image");
      return EncodePromise::CreateAndReject(
          MediaResult(NS_ERROR_ILLEGAL_INPUT,
                      RESULT_DETAIL("Unable to get image data")),
          __func__);
    }

    uint8_t* yPlane = mFrame->data[0];
    uint8_t* uPlane = mFrame->data[1];
    uint8_t* vPlane = mFrame->data[2];
    // TODO: Handle alpha channel when AV_PIX_FMT_YUVA* is supported.

    // TODO: Check mFrame->linesize?

    // TODO: Evaluate if we need to take data->mYSkip, data->mCbSkip, and
    // data->mCrSkip into account when cloning the data.
    if (!CopyPlane(yPlane, data->mYChannel, data->YDataSize(),
                   data->mYStride) ||
        !CopyPlane(uPlane, data->mCbChannel, data->CbCrDataSize(),
                   data->mCbCrStride) ||
        !CopyPlane(vPlane, data->mCrChannel, data->CbCrDataSize(),
                   data->mCbCrStride)) {
      FFMPEGV_LOG("Input YCbCr image is too big");
      return EncodePromise::CreateAndReject(
          MediaResult(NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
                      RESULT_DETAIL("Input image is too big")),
          __func__);
    }
  } else if (aSample->mImage->AsNVImage()) {
    const layers::PlanarYCbCrData* data =
        aSample->mImage->AsNVImage()->GetData();
    if (!data) {
      FFMPEGV_LOG("No data in NV image");
      return EncodePromise::CreateAndReject(
          MediaResult(NS_ERROR_ILLEGAL_INPUT,
                      RESULT_DETAIL("Input image is too big")),
          __func__);
    }

    uint8_t* yPlane = mFrame->data[0];
    uint8_t* uvPlane = mFrame->data[1];

    // TODO: Evaluate if we need to take data->mYSkip, data->mCbSkip, and
    // data->mCrSkip into account when cloning the data.
    if (!CopyPlane(yPlane, data->mYChannel, data->YDataSize(),
                   data->mYStride) ||
        !CopyPlane(uvPlane, data->mCbChannel, data->CbCrDataSize(),
                   data->mCbCrStride)) {
      FFMPEGV_LOG("Input NV image is too big");
      return EncodePromise::CreateAndReject(
          MediaResult(NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
                      RESULT_DETAIL("Input image is too big")),
          __func__);
    }
  } else {
    RefPtr<gfx::SourceSurface> surface = aSample->mImage->GetAsSourceSurface();
    if (!surface) {
      FFMPEGV_LOG("failed to get source surface of the image");
      return EncodePromise::CreateAndReject(
          MediaResult(NS_ERROR_NOT_AVAILABLE,
                      RESULT_DETAIL("Unable to get source surface ")),
          __func__);
    }

    RefPtr<gfx::DataSourceSurface> dataSurface = surface->GetDataSurface();
    if (!dataSurface) {
      FFMPEGV_LOG("failed to get data source surface of the image");
      return EncodePromise::CreateAndReject(
          MediaResult(NS_ERROR_NOT_AVAILABLE,
                      RESULT_DETAIL("Unable to get data source surface ")),
          __func__);
    }

    const gfx::SurfaceFormat format = dataSurface->GetFormat();
    if (format != gfx::SurfaceFormat::R8G8B8A8 &&
        format != gfx::SurfaceFormat::R8G8B8X8 &&
        format != gfx::SurfaceFormat::B8G8R8A8 &&
        format != gfx::SurfaceFormat::B8G8R8X8 &&
        format != gfx::SurfaceFormat::R8G8B8 &&
        format != gfx::SurfaceFormat::B8G8R8) {
      FFMPEGV_LOG("surface format is unsupported");
      return EncodePromise::CreateAndReject(
          MediaResult(NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR,
                      RESULT_DETAIL("Unsupported surface format")),
          __func__);
    }

    gfx::DataSourceSurface::ScopedMap map(dataSurface,
                                          gfx::DataSourceSurface::READ);
    if (!map.IsMapped()) {
      return EncodePromise::CreateAndReject(
          MediaResult(NS_ERROR_NOT_AVAILABLE,
                      RESULT_DETAIL("Unable to read image data")),
          __func__);
    }

    // TODO: Gecko prefers BGRA. Does format always match
    // mConfig.mSourcePixelFormat? gfx::SwizzleData if they mismatch.
    uint8_t* buf = mFrame->data[0];
    if (!CopyPlane(buf, map.GetData(), dataSurface->GetSize(),
                   map.GetStride())) {
      FFMPEGV_LOG("Input RGB-type image is too big");
      return EncodePromise::CreateAndReject(
          MediaResult(NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
                      RESULT_DETAIL("Input RGB-type image is too big")),
          __func__);
    }
  }

  FFMPEGV_LOG(
      "Fill AVFrame with %s image data",
      dom::ImageBitmapFormatValues::GetString(imageUtils.GetFormat()).data());

  // Set presentation timestamp of the AVFrame.
  // The unit of pts is AVCodecContext's time_base, which is the reciprocal of
  // the frame rate.
  mFrame->pts = aSample->mTime.ToTicksAtRate(mConfig.mFramerate);

  // Initialize AVPacket.
  AVPacket* pkt = mLib->av_packet_alloc();
  if (!pkt) {
    FFMPEGV_LOG("failed to allocate packet");
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY,
                    RESULT_DETAIL("Unable to allocate packet")),
        __func__);
  }

  // Send frame and receive packets.

  if (int ret = mLib->avcodec_send_frame(mCodecContext, mFrame); ret < 0) {
    // In theory, avcodec_send_frame could sent -EAGAIN to signal its internal
    // buffers is full. In practice this can't happen as we only feed one frame
    // at a time, and we immediately call avcodec_receive_packet right after.
    // TODO: Create a NS_ERROR_DOM_MEDIA_ENCODE_ERR in ErrorList.py?
    FFMPEGV_LOG("avcodec_send_frame error: %s",
                MakeErrorString(mLib, ret).get());
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("avcodec_send_frame error")),
        __func__);
  }

  EncodedData output;
  while (true) {
    int ret = mLib->avcodec_receive_packet(mCodecContext, pkt);
    if (ret == AVERROR(EAGAIN)) {
      // The encoder is asking for more inputs.
      FFMPEGV_LOG("encoder is asking for more input!");
      break;
    }

    if (ret < 0) {
      // AVERROR_EOF is returned when the encoder has been fully flushed, but it
      // shouldn't happen here.
      FFMPEGV_LOG("avcodec_receive_packet error: %s",
                  MakeErrorString(mLib, ret).get());
      return EncodePromise::CreateAndReject(
          MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                      RESULT_DETAIL("avcodec_receive_packet error")),
          __func__);
    }

    RefPtr<MediaRawData> d = ToMediaRawData(pkt);
    mLib->av_packet_unref(pkt);
    if (!d) {
      FFMPEGV_LOG("failed to create a MediaRawData from the AVPacket");
      return EncodePromise::CreateAndReject(
          MediaResult(
              NS_ERROR_OUT_OF_MEMORY,
              RESULT_DETAIL("Unable to get MediaRawData from AVPacket")),
          __func__);
    }
    output.AppendElement(std::move(d));
  }

  FFMPEGV_LOG("get %zu encoded data", output.Length());
  return EncodePromise::CreateAndResolve(std::move(output), __func__);
}

template <typename ConfigType>
RefPtr<MediaDataEncoder::EncodePromise>
FFmpegVideoEncoder<LIBAV_VER, ConfigType>::DrainWithModernAPIs() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  MOZ_ASSERT(mCodecContext);

  // TODO: Create a Result<EncodedData, nsresult> EncodeWithModernAPIs(AVFrame
  // *aFrame) to merge the duplicate code below with EncodeWithModernAPIs above.

  // Initialize AVPacket.
  AVPacket* pkt = mLib->av_packet_alloc();
  if (!pkt) {
    FFMPEGV_LOG("failed to allocate packet");
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY,
                    RESULT_DETAIL("Unable to allocate packet")),
        __func__);
  }

  // TODO: Bug 1868906 - Allow continuing encoding after Drain().
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
      FFMPEGV_LOG("encoder has been flushed!");
      return EncodePromise::CreateAndResolve(EncodedData(), __func__);
    }

    FFMPEGV_LOG("avcodec_send_frame error: %s",
                MakeErrorString(mLib, ret).get());
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("avcodec_send_frame error")),
        __func__);
  }

  EncodedData output;
  while (true) {
    int ret = mLib->avcodec_receive_packet(mCodecContext, pkt);
    if (ret == AVERROR_EOF) {
      FFMPEGV_LOG("encoder has no more output packet!");
      break;
    }

    if (ret < 0) {
      // avcodec_receive_packet should not result in a -EAGAIN once it's in
      // draining mode.
      FFMPEGV_LOG("avcodec_receive_packet error: %s",
                  MakeErrorString(mLib, ret).get());
      return EncodePromise::CreateAndReject(
          MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                      RESULT_DETAIL("avcodec_receive_packet error")),
          __func__);
    }

    RefPtr<MediaRawData> d = ToMediaRawData(pkt);
    mLib->av_packet_unref(pkt);
    if (!d) {
      FFMPEGV_LOG("failed to create a MediaRawData from the AVPacket");
      return EncodePromise::CreateAndReject(
          MediaResult(
              NS_ERROR_OUT_OF_MEMORY,
              RESULT_DETAIL("Unable to get MediaRawData from AVPacket")),
          __func__);
    }
    output.AppendElement(std::move(d));
  }

  FFMPEGV_LOG("get %zu encoded data", output.Length());
  return EncodePromise::CreateAndResolve(std::move(output), __func__);
}
#endif

template <typename ConfigType>
RefPtr<MediaRawData> FFmpegVideoEncoder<LIBAV_VER, ConfigType>::ToMediaRawData(
    AVPacket* aPacket) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  MOZ_ASSERT(aPacket);

  // TODO: Do we need to check AV_PKT_FLAG_CORRUPT?

  // Copy frame data from AVPacket.
  auto data = MakeRefPtr<MediaRawData>();
  UniquePtr<MediaRawDataWriter> writer(data->CreateWriter());
  if (!writer->Append(aPacket->data, static_cast<size_t>(aPacket->size))) {
    FFMPEGV_LOG("fail to allocate MediaRawData buffer");
    return nullptr;  // OOM
  }

  data->mKeyframe = (aPacket->flags & AV_PKT_FLAG_KEY) != 0;
  // The unit of pts, dts, and duration is AVCodecContext's time_base, which is
  // the reciprocal of the frame rate.
  data->mTime =
      media::TimeUnit(aPacket->pts, static_cast<int64_t>(mConfig.mFramerate));
  data->mDuration = media::TimeUnit(aPacket->duration,
                                    static_cast<int64_t>(mConfig.mFramerate));
  data->mTimecode =
      media::TimeUnit(aPacket->dts, static_cast<int64_t>(mConfig.mFramerate));
  return data;
}

template class FFmpegVideoEncoder<LIBAV_VER, MediaDataEncoder::VP8Config>;
template class FFmpegVideoEncoder<LIBAV_VER, MediaDataEncoder::VP9Config>;

}  // namespace mozilla
