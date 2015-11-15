/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VPXDecoder.h"
#include "gfx2DGlue.h"
#include "nsError.h"
#include "TimeUnits.h"
#include "mozilla/PodOperations.h"
#include "prsystem.h"

#include <algorithm>

#undef LOG
extern mozilla::LogModule* GetPDMLog();
#define LOG(arg, ...) MOZ_LOG(GetPDMLog(), mozilla::LogLevel::Debug, ("VPXDecoder(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {

using namespace gfx;
using namespace layers;

VPXDecoder::VPXDecoder(const VideoInfo& aConfig,
                       ImageContainer* aImageContainer,
                       FlushableTaskQueue* aTaskQueue,
                       MediaDataDecoderCallback* aCallback)
  : mImageContainer(aImageContainer)
  , mTaskQueue(aTaskQueue)
  , mCallback(aCallback)
  , mInfo(aConfig)
{
  MOZ_COUNT_CTOR(VPXDecoder);
  if (aConfig.mMimeType.EqualsLiteral("video/webm; codecs=vp8")) {
    mCodec = Codec::VP8;
  } else if (aConfig.mMimeType.EqualsLiteral("video/webm; codecs=vp9")) {
    mCodec = Codec::VP9;
  } else {
    mCodec = -1;
  }
  PodZero(&mVPX);
}

VPXDecoder::~VPXDecoder()
{
  MOZ_COUNT_DTOR(VPXDecoder);
}

nsresult
VPXDecoder::Shutdown()
{
  vpx_codec_destroy(&mVPX);
  return NS_OK;
}

RefPtr<MediaDataDecoder::InitPromise>
VPXDecoder::Init()
{
  int decode_threads = 2;

  vpx_codec_iface_t* dx = nullptr;
  if (mCodec == Codec::VP8) {
    dx = vpx_codec_vp8_dx();
  } else if (mCodec == Codec::VP9) {
    dx = vpx_codec_vp9_dx();
    if (mInfo.mDisplay.width >= 2048) {
      decode_threads = 8;
    } else if (mInfo.mDisplay.width >= 1024) {
      decode_threads = 4;
    }
  }
  decode_threads = std::min(decode_threads, PR_GetNumberOfProcessors());

  vpx_codec_dec_cfg_t config;
  config.threads = decode_threads;
  config.w = config.h = 0; // set after decode

  if (!dx || vpx_codec_dec_init(&mVPX, dx, &config, 0)) {
    return InitPromise::CreateAndReject(DecoderFailureReason::INIT_ERROR, __func__);
  }
  return InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
}

nsresult
VPXDecoder::Flush()
{
  mTaskQueue->Flush();
  return NS_OK;
}

int
VPXDecoder::DoDecodeFrame(MediaRawData* aSample)
{
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
    return -1;
  }

  vpx_codec_iter_t  iter = nullptr;
  vpx_image_t      *img;

  while ((img = vpx_codec_get_frame(&mVPX, &iter))) {
    NS_ASSERTION(img->fmt == VPX_IMG_FMT_I420, "WebM image format not I420");

    // Chroma shifts are rounded down as per the decoding examples in the SDK
    VideoData::YCbCrBuffer b;
    b.mPlanes[0].mData = img->planes[0];
    b.mPlanes[0].mStride = img->stride[0];
    b.mPlanes[0].mHeight = img->d_h;
    b.mPlanes[0].mWidth = img->d_w;
    b.mPlanes[0].mOffset = b.mPlanes[0].mSkip = 0;

    b.mPlanes[1].mData = img->planes[1];
    b.mPlanes[1].mStride = img->stride[1];
    b.mPlanes[1].mHeight = (img->d_h + 1) >> img->y_chroma_shift;
    b.mPlanes[1].mWidth = (img->d_w + 1) >> img->x_chroma_shift;
    b.mPlanes[1].mOffset = b.mPlanes[1].mSkip = 0;

    b.mPlanes[2].mData = img->planes[2];
    b.mPlanes[2].mStride = img->stride[2];
    b.mPlanes[2].mHeight = (img->d_h + 1) >> img->y_chroma_shift;
    b.mPlanes[2].mWidth = (img->d_w + 1) >> img->x_chroma_shift;
    b.mPlanes[2].mOffset = b.mPlanes[2].mSkip = 0;

    VideoInfo info;
    info.mDisplay = mInfo.mDisplay;
    RefPtr<VideoData> v = VideoData::Create(info,
                                              mImageContainer,
                                              aSample->mOffset,
                                              aSample->mTime,
                                              aSample->mDuration,
                                              b,
                                              aSample->mKeyframe,
                                              aSample->mTimecode,
                                              mInfo.mImage);

    if (!v) {
      LOG("Image allocation error source %ldx%ld display %ldx%ld picture %ldx%ld",
          img->d_w, img->d_h, mInfo.mDisplay.width, mInfo.mDisplay.height,
          mInfo.mImage.width, mInfo.mImage.height);
      return -1;
    }
    mCallback->Output(v);
  }
  return 0;
}

void
VPXDecoder::DecodeFrame(MediaRawData* aSample)
{
  if (DoDecodeFrame(aSample) == -1) {
    mCallback->Error();
  } else if (mTaskQueue->IsEmpty()) {
    mCallback->InputExhausted();
  }
}

nsresult
VPXDecoder::Input(MediaRawData* aSample)
{
  nsCOMPtr<nsIRunnable> runnable(
    NS_NewRunnableMethodWithArg<RefPtr<MediaRawData>>(
      this, &VPXDecoder::DecodeFrame,
      RefPtr<MediaRawData>(aSample)));
  mTaskQueue->Dispatch(runnable.forget());

  return NS_OK;
}

void
VPXDecoder::DoDrain()
{
  mCallback->DrainComplete();
}

nsresult
VPXDecoder::Drain()
{
  nsCOMPtr<nsIRunnable> runnable(
    NS_NewRunnableMethod(this, &VPXDecoder::DoDrain));
  mTaskQueue->Dispatch(runnable.forget());

  return NS_OK;
}

/* static */
bool
VPXDecoder::IsVPX(const nsACString& aMimeType)
{
  return aMimeType.EqualsLiteral("video/webm; codecs=vp8") ||
    aMimeType.EqualsLiteral("video/webm; codecs=vp9");
}

} // namespace mozilla
#undef LOG
