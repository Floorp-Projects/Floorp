/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AOMDecoder.h"
#include "MediaResult.h"
#include "TimeUnits.h"
#include "aom/aomdx.h"
#include "gfx2DGlue.h"
#include "mozilla/PodOperations.h"
#include "mozilla/SyncRunnable.h"
#include "nsError.h"
#include "prsystem.h"

#include <algorithm>

#undef LOG
#define LOG(arg, ...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, ("AOMDecoder(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define LOG_RESULT(code, message, ...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, ("AOMDecoder::%s: %s (code %d) " message, __func__, aom_codec_err_to_string(code), (int)code, ##__VA_ARGS__))

namespace mozilla {

using namespace gfx;
using namespace layers;


static MediaResult
InitContext(aom_codec_ctx_t* aCtx,
            const VideoInfo& aInfo)
{
  aom_codec_iface_t* dx = aom_codec_av1_dx();
  if (!dx) {
    return MediaResult(NS_ERROR_FAILURE,
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
    LOG_RESULT(res, "Codec initialization failed!");
    return MediaResult(NS_ERROR_FAILURE,
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
  return InvokeAsync(mTaskQueue, __func__, [self, this]() {
    auto res = aom_codec_destroy(&mCodec);
    if (res != AOM_CODEC_OK) {
      LOG_RESULT(res, "aom_codec_destroy");
    }
    return ShutdownPromise::CreateAndResolve(true, __func__);
  });
}

RefPtr<MediaDataDecoder::InitPromise>
AOMDecoder::Init()
{
  if (NS_FAILED(InitContext(&mCodec, mInfo))) {
    return AOMDecoder::InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR,
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
  DecodedData results;

  while ((img = aom_codec_get_frame(&mCodec, &iter))) {
    NS_ASSERTION(img->fmt == AOM_IMG_FMT_I420 ||
                 img->fmt == AOM_IMG_FMT_I444,
                 "WebM image format not I420 or I444");

    // Chroma shifts are rounded down as per the decoding examples in the SDK
    VideoData::YCbCrBuffer b;
    b.mPlanes[0].mData = img->planes[0];
    b.mPlanes[0].mStride = img->stride[0];
    b.mPlanes[0].mHeight = img->d_h;
    b.mPlanes[0].mWidth = img->d_w;
    b.mPlanes[0].mOffset = b.mPlanes[0].mSkip = 0;

    b.mPlanes[1].mData = img->planes[1];
    b.mPlanes[1].mStride = img->stride[1];
    b.mPlanes[1].mOffset = b.mPlanes[1].mSkip = 0;

    b.mPlanes[2].mData = img->planes[2];
    b.mPlanes[2].mStride = img->stride[2];
    b.mPlanes[2].mOffset = b.mPlanes[2].mSkip = 0;

    if (img->fmt == AOM_IMG_FMT_I420) {
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
    results.AppendElement(Move(v));
  }
  return DecodePromise::CreateAndResolve(Move(results), __func__);
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
  return aMimeType.EqualsLiteral("video/webm; codecs=av1")
         || aMimeType.EqualsLiteral("video/av1");
}

/* static */
bool
AOMDecoder::IsSupportedCodec(const nsAString& aCodecType)
{
  // While AV1 is under development, we describe support
  // for a specific aom commit hash so sites can check
  // compatibility.
  auto version = NS_ConvertASCIItoUTF16("av1.experimental.");
  version.AppendLiteral("4d668d7feb1f8abd809d1bca0418570a7f142a36");
  return aCodecType.EqualsLiteral("av1") ||
         aCodecType.Equals(version);
}

/* static */
bool
AOMDecoder::IsKeyframe(Span<const uint8_t> aBuffer) {
  aom_codec_stream_info_t info;
  PodZero(&info);
  info.sz = sizeof(info);

  auto res = aom_codec_peek_stream_info(aom_codec_av1_dx(),
                                        aBuffer.Elements(),
                                        aBuffer.Length(),
                                        &info);
  if (res != AOM_CODEC_OK) {
    LOG_RESULT(res, "couldn't get keyframe flag with aom_codec_peek_stream_info");
    return false;
  }

  return bool(info.is_kf);
}

/* static */
nsIntSize
AOMDecoder::GetFrameSize(Span<const uint8_t> aBuffer) {
  aom_codec_stream_info_t info;
  PodZero(&info);
  info.sz = sizeof(info);

  auto res = aom_codec_peek_stream_info(aom_codec_av1_dx(),
                                        aBuffer.Elements(),
                                        aBuffer.Length(),
                                        &info);
  if (res != AOM_CODEC_OK) {
    LOG_RESULT(res, "couldn't get frame size with aom_codec_peek_stream_info");
  }

  return nsIntSize(info.w, info.h);
}

} // namespace mozilla
#undef LOG
