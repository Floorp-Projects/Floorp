/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AOMDecoder.h"
#include "MediaResult.h"
#include "TimeUnits.h"
#include "aom/aomdx.h"
#include "aom/aom_image.h"
#include "gfx2DGlue.h"
#include "mozilla/PodOperations.h"
#include "mozilla/SyncRunnable.h"
#include "nsError.h"
#include "prsystem.h"
#include "ImageContainer.h"

#include <algorithm>

#undef LOG
#define LOG(arg, ...)                                                          \
  DDMOZ_LOG(                                                                   \
    sPDMLog, mozilla::LogLevel::Debug, "::%s: " arg, __func__, ##__VA_ARGS__)
#define LOG_RESULT(code, message, ...)                                         \
  DDMOZ_LOG(sPDMLog,                                                           \
            mozilla::LogLevel::Debug,                                          \
            "::%s: %s (code %d) " message,                                     \
            __func__,                                                          \
            aom_codec_err_to_string(code),                                     \
            (int)code,                                                         \
            ##__VA_ARGS__)
#define LOGEX_RESULT(_this, code, message, ...)                                \
  DDMOZ_LOGEX(_this,                                                           \
              sPDMLog,                                                         \
              mozilla::LogLevel::Debug,                                        \
              "::%s: %s (code %d) " message,                                   \
              __func__,                                                        \
              aom_codec_err_to_string(code),                                   \
              (int)code,                                                       \
              ##__VA_ARGS__)
#define LOG_STATIC_RESULT(code, message, ...)                                  \
  MOZ_LOG(sPDMLog,                                                             \
          mozilla::LogLevel::Debug,                                            \
          ("AOMDecoder::%s: %s (code %d) " message,                            \
           __func__,                                                           \
           aom_codec_err_to_string(code),                                      \
           (int)code,                                                          \
           ##__VA_ARGS__))

namespace mozilla {

using namespace gfx;
using namespace layers;

static MediaResult
InitContext(AOMDecoder& aAOMDecoder,
            aom_codec_ctx_t* aCtx,
            const VideoInfo& aInfo)
{
  aom_codec_iface_t* dx = aom_codec_av1_dx();
  if (!dx) {
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("Couldn't get AV1 decoder interface."));
  }

  int decode_threads = 2;
  if (aInfo.mDisplay.width >= 2048) {
    decode_threads = 8;
  }
  else if (aInfo.mDisplay.width >= 1024) {
    decode_threads = 4;
  }
  decode_threads = std::min(decode_threads, PR_GetNumberOfProcessors());

  aom_codec_dec_cfg_t config;
  PodZero(&config);
  config.threads = decode_threads;
  config.w = config.h = 0; // set after decode

  aom_codec_flags_t flags = 0;

  auto res = aom_codec_dec_init(aCtx, dx, &config, flags);
  if (res != AOM_CODEC_OK) {
    LOGEX_RESULT(
      &aAOMDecoder, res, "Codec initialization failed, res=%d", int(res));
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("AOM error initializing AV1 decoder: %s",
                                     aom_codec_err_to_string(res)));
  }
  return NS_OK;
}

AOMDecoder::AOMDecoder(const CreateDecoderParams& aParams)
  : mImageContainer(aParams.mImageContainer)
  , mTaskQueue(aParams.mTaskQueue)
  , mInfo(aParams.VideoConfig())
{
  PodZero(&mCodec);
}

AOMDecoder::~AOMDecoder()
{
}

RefPtr<ShutdownPromise>
AOMDecoder::Shutdown()
{
  RefPtr<AOMDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self]() {
    auto res = aom_codec_destroy(&self->mCodec);
    if (res != AOM_CODEC_OK) {
      LOGEX_RESULT(self.get(), res, "aom_codec_destroy");
    }
    return ShutdownPromise::CreateAndResolve(true, __func__);
  });
}

RefPtr<MediaDataDecoder::InitPromise>
AOMDecoder::Init()
{
  MediaResult rv = InitContext(*this, &mCodec, mInfo);
  if (NS_FAILED(rv)) {
    return AOMDecoder::InitPromise::CreateAndReject(rv,
                                                    __func__);
  }
  return AOMDecoder::InitPromise::CreateAndResolve(TrackInfo::kVideoTrack,
                                                   __func__);
}

RefPtr<MediaDataDecoder::FlushPromise>
AOMDecoder::Flush()
{
  return InvokeAsync(mTaskQueue, __func__, []() {
    return FlushPromise::CreateAndResolve(true, __func__);
  });
}

// Ported from third_party/aom/tools_common.c.
static aom_codec_err_t
highbd_img_downshift(aom_image_t *dst, aom_image_t *src, int down_shift) {
  int plane;
  if (dst->d_w != src->d_w || dst->d_h != src->d_h)
    return AOM_CODEC_INVALID_PARAM;
  if (dst->x_chroma_shift != src->x_chroma_shift)
    return AOM_CODEC_INVALID_PARAM;
  if (dst->y_chroma_shift != src->y_chroma_shift)
    return AOM_CODEC_INVALID_PARAM;
  if (dst->fmt != (src->fmt & ~AOM_IMG_FMT_HIGHBITDEPTH))
    return AOM_CODEC_INVALID_PARAM;
  if (down_shift < 0)
      return AOM_CODEC_INVALID_PARAM;
  switch (dst->fmt) {
    case AOM_IMG_FMT_I420:
    case AOM_IMG_FMT_I422:
    case AOM_IMG_FMT_I444:
    case AOM_IMG_FMT_I440:
      break;
    default:
      return AOM_CODEC_INVALID_PARAM;
  }
  switch (src->fmt) {
    case AOM_IMG_FMT_I42016:
    case AOM_IMG_FMT_I42216:
    case AOM_IMG_FMT_I44416:
    case AOM_IMG_FMT_I44016:
      break;
    default:
      // We don't support anything that's not 16 bit
      return AOM_CODEC_UNSUP_BITSTREAM;
  }
  for (plane = 0; plane < 3; plane++) {
    int w = src->d_w;
    int h = src->d_h;
    int x, y;
    if (plane) {
      w = (w + src->x_chroma_shift) >> src->x_chroma_shift;
      h = (h + src->y_chroma_shift) >> src->y_chroma_shift;
    }
    for (y = 0; y < h; y++) {
      uint16_t *p_src =
          (uint16_t *)(src->planes[plane] + y * src->stride[plane]);
      uint8_t *p_dst =
          dst->planes[plane] + y * dst->stride[plane];
      for (x = 0; x < w; x++) *p_dst++ = (*p_src++ >> down_shift) & 0xFF;
    }
  }
  return AOM_CODEC_OK;
}

// UniquePtr dtor wrapper for aom_image_t.
struct AomImageFree {
  void operator()(aom_image_t* img) { aom_img_free(img); }
};

RefPtr<MediaDataDecoder::DecodePromise>
AOMDecoder::ProcessDecode(MediaRawData* aSample)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

#if defined(DEBUG)
  NS_ASSERTION(IsKeyframe(*aSample) == aSample->mKeyframe,
               "AOM Decode Keyframe error sample->mKeyframe and si.si_kf out of sync");
#endif

  if (aom_codec_err_t r = aom_codec_decode(&mCodec, aSample->Data(), aSample->Size(), nullptr, 0)) {
    LOG_RESULT(r, "Decode error!");
    return DecodePromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                  RESULT_DETAIL("AOM error decoding AV1 sample: %s",
                                aom_codec_err_to_string(r))),
      __func__);
  }

  aom_codec_iter_t iter = nullptr;
  aom_image_t *img;
  UniquePtr<aom_image_t, AomImageFree> img8;
  DecodedData results;

  while ((img = aom_codec_get_frame(&mCodec, &iter))) {
    // Track whether the underlying buffer is 8 or 16 bits per channel.
    bool highbd = bool(img->fmt & AOM_IMG_FMT_HIGHBITDEPTH);
    if (highbd) {
      // Downsample images with more than 8 bits per channel.
      aom_img_fmt_t fmt8 = static_cast<aom_img_fmt_t>(img->fmt ^ AOM_IMG_FMT_HIGHBITDEPTH);
      img8.reset(aom_img_alloc(NULL, fmt8, img->d_w, img->d_h, 16));
      if (img8 == nullptr) {
        LOG("Couldn't allocate bitdepth reduction target!");
        return DecodePromise::CreateAndReject(
          MediaResult(NS_ERROR_OUT_OF_MEMORY,
                      RESULT_DETAIL("Couldn't allocate conversion buffer for AV1 frame")),
                      __func__);
      }
      if (aom_codec_err_t r = highbd_img_downshift(img8.get(), img, img->bit_depth - 8)) {
        LOG_RESULT(r, "Image downconversion failed");
        return DecodePromise::CreateAndReject(
          MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                      RESULT_DETAIL("Error converting AV1 frame to 8 bits: %s",
                                    aom_codec_err_to_string(r))),
          __func__);
      }
      // img normally points to storage owned by mCodec, so it is not freed.
      // To copy out the contents of img8 we can overwrite img with an alias.
      // Since img is assigned at the start of the while loop and img8 is held
      // outside that loop, the alias won't outlive the storage it points to.
      img = img8.get();
      highbd = false;
    }

    NS_ASSERTION(img->fmt == AOM_IMG_FMT_I420 ||
                 img->fmt == AOM_IMG_FMT_I42016 ||
                 img->fmt == AOM_IMG_FMT_I444 ||
                 img->fmt == AOM_IMG_FMT_I44416,
                 "AV1 image format not I420 or I444");

    // Chroma shifts are rounded down as per the decoding examples in the SDK
    VideoData::YCbCrBuffer b;
    b.mPlanes[0].mData = img->planes[0];
    b.mPlanes[0].mStride = img->stride[0];
    b.mPlanes[0].mHeight = img->d_h;
    b.mPlanes[0].mWidth = img->d_w;
    b.mPlanes[0].mOffset = 0;
    b.mPlanes[0].mSkip = highbd ? 1 : 0;

    b.mPlanes[1].mData = img->planes[1];
    b.mPlanes[1].mStride = img->stride[1];
    b.mPlanes[1].mOffset = 0;
    b.mPlanes[1].mSkip = highbd ? 1 : 0;

    b.mPlanes[2].mData = img->planes[2];
    b.mPlanes[2].mStride = img->stride[2];
    b.mPlanes[2].mOffset = 0;
    b.mPlanes[2].mSkip = highbd ? 1 : 0;

    if (img->fmt == AOM_IMG_FMT_I420 ||
        img->fmt == AOM_IMG_FMT_I42016) {
      b.mPlanes[1].mHeight = (img->d_h + 1) >> img->y_chroma_shift;
      b.mPlanes[1].mWidth = (img->d_w + 1) >> img->x_chroma_shift;

      b.mPlanes[2].mHeight = (img->d_h + 1) >> img->y_chroma_shift;
      b.mPlanes[2].mWidth = (img->d_w + 1) >> img->x_chroma_shift;
    } else if (img->fmt == AOM_IMG_FMT_I444) {
      b.mPlanes[1].mHeight = img->d_h;
      b.mPlanes[1].mWidth = img->d_w;

      b.mPlanes[2].mHeight = img->d_h;
      b.mPlanes[2].mWidth = img->d_w;
    } else {
      LOG("AOM Unknown image format");
      return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                    RESULT_DETAIL("AOM Unknown image format")),
        __func__);
    }

    RefPtr<VideoData> v;
    v = VideoData::CreateAndCopyData(mInfo,
                                     mImageContainer,
                                     aSample->mOffset,
                                     aSample->mTime,
                                     aSample->mDuration,
                                     b,
                                     aSample->mKeyframe,
                                     aSample->mTimecode,
                                     mInfo.ScaledImageRect(img->d_w,
                                                           img->d_h));

    if (!v) {
      LOG(
        "Image allocation error source %ux%u display %ux%u picture %ux%u",
        img->d_w, img->d_h, mInfo.mDisplay.width, mInfo.mDisplay.height,
        mInfo.mImage.width, mInfo.mImage.height);
      return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__), __func__);
    }
    results.AppendElement(std::move(v));
  }
  return DecodePromise::CreateAndResolve(std::move(results), __func__);
}

RefPtr<MediaDataDecoder::DecodePromise>
AOMDecoder::Decode(MediaRawData* aSample)
{
  return InvokeAsync<MediaRawData*>(mTaskQueue, this, __func__,
                                    &AOMDecoder::ProcessDecode, aSample);
}

RefPtr<MediaDataDecoder::DecodePromise>
AOMDecoder::Drain()
{
  return InvokeAsync(mTaskQueue, __func__, [] {
    return DecodePromise::CreateAndResolve(DecodedData(), __func__);
  });
}


/* static */
bool
AOMDecoder::IsAV1(const nsACString& aMimeType)
{
  return aMimeType.EqualsLiteral("video/av1");
}

/* static */
bool
AOMDecoder::IsSupportedCodec(const nsAString& aCodecType)
{
  // While AV1 is under development, we describe support
  // for a specific aom commit hash so sites can check
  // compatibility.
  auto version = NS_ConvertASCIItoUTF16("av1.experimental.");
  version.AppendLiteral("d14c5bb4f336ef1842046089849dee4a301fbbf0");
  return aCodecType.EqualsLiteral("av1") ||
         aCodecType.Equals(version);
}

/* static */
bool
AOMDecoder::IsKeyframe(Span<const uint8_t> aBuffer) {
  aom_codec_stream_info_t info;
  PodZero(&info);

  auto res = aom_codec_peek_stream_info(aom_codec_av1_dx(),
                                        aBuffer.Elements(),
                                        aBuffer.Length(),
                                        &info);
  if (res != AOM_CODEC_OK) {
    LOG_STATIC_RESULT(
      res, "couldn't get keyframe flag with aom_codec_peek_stream_info");
    return false;
  }

  return bool(info.is_kf);
}

/* static */
gfx::IntSize
AOMDecoder::GetFrameSize(Span<const uint8_t> aBuffer)
{
  aom_codec_stream_info_t info;
  PodZero(&info);

  auto res = aom_codec_peek_stream_info(aom_codec_av1_dx(),
                                        aBuffer.Elements(),
                                        aBuffer.Length(),
                                        &info);
  if (res != AOM_CODEC_OK) {
    LOG_STATIC_RESULT(
      res, "couldn't get frame size with aom_codec_peek_stream_info");
  }

  return gfx::IntSize(info.w, info.h);
}

} // namespace mozilla
#undef LOG
