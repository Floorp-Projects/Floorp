/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VPXDecoder.h"
#include "TimeUnits.h"
#include "gfx2DGlue.h"
#include "mozilla/PodOperations.h"
#include "mozilla/SyncRunnable.h"
#include "nsError.h"
#include "prsystem.h"

#include <algorithm>

#undef LOG
#define LOG(arg, ...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, ("VPXDecoder(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {

using namespace gfx;
using namespace layers;

static int MimeTypeToCodec(const nsACString& aMimeType)
{
  if (aMimeType.EqualsLiteral("video/webm; codecs=vp8")) {
    return VPXDecoder::Codec::VP8;
  } else if (aMimeType.EqualsLiteral("video/webm; codecs=vp9")) {
    return VPXDecoder::Codec::VP9;
  } else if (aMimeType.EqualsLiteral("video/vp9")) {
    return VPXDecoder::Codec::VP9;
  }
  return -1;
}

static nsresult
InitContext(vpx_codec_ctx_t* aCtx,
            const VideoInfo& aInfo,
            const int aCodec)
{
  int decode_threads = 2;

  vpx_codec_iface_t* dx = nullptr;
  if (aCodec == VPXDecoder::Codec::VP8) {
    dx = vpx_codec_vp8_dx();
  }
  else if (aCodec == VPXDecoder::Codec::VP9) {
    dx = vpx_codec_vp9_dx();
    if (aInfo.mDisplay.width >= 2048) {
      decode_threads = 8;
    }
    else if (aInfo.mDisplay.width >= 1024) {
      decode_threads = 4;
    }
  }
  decode_threads = std::min(decode_threads, PR_GetNumberOfProcessors());

  vpx_codec_dec_cfg_t config;
  config.threads = decode_threads;
  config.w = config.h = 0; // set after decode

  if (!dx || vpx_codec_dec_init(aCtx, dx, &config, 0)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

VPXDecoder::VPXDecoder(const CreateDecoderParams& aParams)
  : mImageContainer(aParams.mImageContainer)
  , mTaskQueue(aParams.mTaskQueue)
  , mInfo(aParams.VideoConfig())
  , mCodec(MimeTypeToCodec(aParams.VideoConfig().mMimeType))
{
  MOZ_COUNT_CTOR(VPXDecoder);
  PodZero(&mVPX);
  PodZero(&mVPXAlpha);
}

VPXDecoder::~VPXDecoder()
{
  MOZ_COUNT_DTOR(VPXDecoder);
}

RefPtr<ShutdownPromise>
VPXDecoder::Shutdown()
{
  RefPtr<VPXDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self, this]() {
    vpx_codec_destroy(&mVPX);
    vpx_codec_destroy(&mVPXAlpha);
    return ShutdownPromise::CreateAndResolve(true, __func__);
  });
}

RefPtr<MediaDataDecoder::InitPromise>
VPXDecoder::Init()
{
  if (NS_FAILED(InitContext(&mVPX, mInfo, mCodec))) {
    return VPXDecoder::InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                                    __func__);
  }
  if (mInfo.HasAlpha()) {
    if (NS_FAILED(InitContext(&mVPXAlpha, mInfo, mCodec))) {
      return VPXDecoder::InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                                      __func__);
    }
  }
  return VPXDecoder::InitPromise::CreateAndResolve(TrackInfo::kVideoTrack,
                                                   __func__);
}

RefPtr<MediaDataDecoder::FlushPromise>
VPXDecoder::Flush()
{
  return InvokeAsync(mTaskQueue, __func__, []() {
    return FlushPromise::CreateAndResolve(true, __func__);
  });
}

RefPtr<MediaDataDecoder::DecodePromise>
VPXDecoder::ProcessDecode(MediaRawData* aSample)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

#if defined(DEBUG)
  vpx_codec_stream_info_t si;
  PodZero(&si);
  si.sz = sizeof(si);
  if (mCodec == Codec::VP8) {
    vpx_codec_peek_stream_info(vpx_codec_vp8_dx(), aSample->Data(), aSample->Size(), &si);
  } else if (mCodec == Codec::VP9) {
    vpx_codec_peek_stream_info(vpx_codec_vp9_dx(), aSample->Data(), aSample->Size(), &si);
  }
  NS_ASSERTION(bool(si.is_kf) == aSample->mKeyframe,
               "VPX Decode Keyframe error sample->mKeyframe and si.si_kf out of sync");
#endif

  if (vpx_codec_err_t r = vpx_codec_decode(&mVPX, aSample->Data(), aSample->Size(), nullptr, 0)) {
    LOG("VPX Decode error: %s", vpx_codec_err_to_string(r));
    return DecodePromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                  RESULT_DETAIL("VPX error: %s", vpx_codec_err_to_string(r))),
      __func__);
  }

  vpx_codec_iter_t iter = nullptr;
  vpx_image_t *img;
  vpx_image_t *img_alpha = nullptr;
  bool alpha_decoded = false;
  DecodedData results;

  while ((img = vpx_codec_get_frame(&mVPX, &iter))) {
    NS_ASSERTION(img->fmt == VPX_IMG_FMT_I420 ||
                 img->fmt == VPX_IMG_FMT_I444,
                 "WebM image format not I420 or I444");
    NS_ASSERTION(!alpha_decoded,
                 "Multiple frames per packet that contains alpha");

    if (aSample->AlphaSize() > 0) {
      if (!alpha_decoded){
        MediaResult rv = DecodeAlpha(&img_alpha, aSample);
        if (NS_FAILED(rv)) {
          return DecodePromise::CreateAndReject(rv, __func__);
        }
        alpha_decoded = true;
      }
    }
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

    if (img->fmt == VPX_IMG_FMT_I420) {
      b.mPlanes[1].mHeight = (img->d_h + 1) >> img->y_chroma_shift;
      b.mPlanes[1].mWidth = (img->d_w + 1) >> img->x_chroma_shift;

      b.mPlanes[2].mHeight = (img->d_h + 1) >> img->y_chroma_shift;
      b.mPlanes[2].mWidth = (img->d_w + 1) >> img->x_chroma_shift;
    } else if (img->fmt == VPX_IMG_FMT_I444) {
      b.mPlanes[1].mHeight = img->d_h;
      b.mPlanes[1].mWidth = img->d_w;

      b.mPlanes[2].mHeight = img->d_h;
      b.mPlanes[2].mWidth = img->d_w;
    } else {
      LOG("VPX Unknown image format");
      return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                    RESULT_DETAIL("VPX Unknown image format")),
        __func__);
    }

    RefPtr<VideoData> v;
    if (!img_alpha) {
      v = VideoData::CreateAndCopyData(mInfo,
                                       mImageContainer,
                                       aSample->mOffset,
                                       aSample->mTime.ToMicroseconds(),
                                       aSample->mDuration,
                                       b,
                                       aSample->mKeyframe,
                                       aSample->mTimecode.ToMicroseconds(),
                                       mInfo.ScaledImageRect(img->d_w,
                                                             img->d_h));
    } else {
      VideoData::YCbCrBuffer::Plane alpha_plane;
      alpha_plane.mData = img_alpha->planes[0];
      alpha_plane.mStride = img_alpha->stride[0];
      alpha_plane.mHeight = img_alpha->d_h;
      alpha_plane.mWidth = img_alpha->d_w;
      alpha_plane.mOffset = alpha_plane.mSkip = 0;
      v = VideoData::CreateAndCopyData(mInfo,
                                       mImageContainer,
                                       aSample->mOffset,
                                       aSample->mTime.ToMicroseconds(),
                                       aSample->mDuration,
                                       b,
                                       alpha_plane,
                                       aSample->mKeyframe,
                                       aSample->mTimecode.ToMicroseconds(),
                                       mInfo.ScaledImageRect(img->d_w,
                                                             img->d_h));

    }

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
VPXDecoder::Decode(MediaRawData* aSample)
{
  return InvokeAsync<MediaRawData*>(mTaskQueue, this, __func__,
                                    &VPXDecoder::ProcessDecode, aSample);
}

RefPtr<MediaDataDecoder::DecodePromise>
VPXDecoder::Drain()
{
  return InvokeAsync(mTaskQueue, __func__, [] {
    return DecodePromise::CreateAndResolve(DecodedData(), __func__);
  });
}

MediaResult
VPXDecoder::DecodeAlpha(vpx_image_t** aImgAlpha, const MediaRawData* aSample)
{
  vpx_codec_err_t r = vpx_codec_decode(&mVPXAlpha,
                                       aSample->AlphaData(),
                                       aSample->AlphaSize(),
                                       nullptr,
                                       0);
  if (r) {
    LOG("VPX decode alpha error: %s", vpx_codec_err_to_string(r));
    return MediaResult(
      NS_ERROR_DOM_MEDIA_DECODE_ERR,
      RESULT_DETAIL("VPX decode alpha error: %s", vpx_codec_err_to_string(r)));
  }

  vpx_codec_iter_t iter = nullptr;

  *aImgAlpha = vpx_codec_get_frame(&mVPXAlpha, &iter);
  NS_ASSERTION((*aImgAlpha)->fmt == VPX_IMG_FMT_I420 ||
               (*aImgAlpha)->fmt == VPX_IMG_FMT_I444,
               "WebM image format not I420 or I444");

  return NS_OK;
}

/* static */
bool
VPXDecoder::IsVPX(const nsACString& aMimeType, uint8_t aCodecMask)
{
  return ((aCodecMask & VPXDecoder::VP8)
          && aMimeType.EqualsLiteral("video/webm; codecs=vp8"))
         || ((aCodecMask & VPXDecoder::VP9)
             && aMimeType.EqualsLiteral("video/webm; codecs=vp9"))
         || ((aCodecMask & VPXDecoder::VP9)
             && aMimeType.EqualsLiteral("video/vp9"));
}

/* static */
bool
VPXDecoder::IsVP8(const nsACString& aMimeType)
{
  return IsVPX(aMimeType, VPXDecoder::VP8);
}

/* static */
bool
VPXDecoder::IsVP9(const nsACString& aMimeType)
{
  return IsVPX(aMimeType, VPXDecoder::VP9);
}

} // namespace mozilla
#undef LOG
