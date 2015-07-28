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

#include <algorithm>

#undef LOG
#define LOG(arg, ...) MOZ_LOG(gMediaDecoderLog, mozilla::LogLevel::Debug, ("VPXDecoder(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {

using namespace gfx;
using namespace layers;

extern PRLogModuleInfo* gMediaDecoderLog;

VPXDecoder::VPXDecoder(const VideoInfo& aConfig,
                       ImageContainer* aImageContainer,
                       FlushableTaskQueue* aTaskQueue,
                       MediaDataDecoderCallback* aCallback)
  : mImageContainer(aImageContainer)
  , mTaskQueue(aTaskQueue)
  , mCallback(aCallback)
  , mIter(nullptr)
  , mDisplayWidth(aConfig.mDisplay.width)
  , mDisplayHeight(aConfig.mDisplay.height)
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

nsresult
VPXDecoder::Init()
{
  vpx_codec_iface_t* dx = nullptr;
  if (mCodec == Codec::VP8) {
    dx = vpx_codec_vp8_dx();
  } else if (mCodec == Codec::VP9) {
    dx = vpx_codec_vp9_dx();
  }
  if (!dx || vpx_codec_dec_init(&mVPX, dx, nullptr, 0)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
VPXDecoder::Flush()
{
  mTaskQueue->Flush();
  mIter = nullptr;
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
    vpx_codec_peek_stream_info(vpx_codec_vp8_dx(), aSample->mData, aSample->mSize, &si);
  } else if (mCodec == Codec::VP9) {
    vpx_codec_peek_stream_info(vpx_codec_vp9_dx(), aSample->mData, aSample->mSize, &si);
  }
  NS_ASSERTION(bool(si.is_kf) == aSample->mKeyframe,
               "VPX Decode Keyframe error sample->mKeyframe and si.si_kf out of sync");
#endif

  if (vpx_codec_err_t r = vpx_codec_decode(&mVPX, aSample->mData, aSample->mSize, nullptr, 0)) {
    LOG("VPX Decode error: %s", vpx_codec_err_to_string(r));
    return -1;
  }

  vpx_image_t      *img;

  if ((img = vpx_codec_get_frame(&mVPX, &mIter))) {
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

    IntRect picture = IntRect(0, 0, img->d_w, img->d_h);
    VideoInfo info;
    info.mDisplay = nsIntSize(mDisplayWidth, mDisplayHeight);
    nsRefPtr<VideoData> v = VideoData::Create(info,
                                              mImageContainer,
                                              aSample->mOffset,
                                              aSample->mTime,
                                              aSample->mDuration,
                                              b,
                                              aSample->mKeyframe,
                                              aSample->mTimecode,
                                              picture);

    if (!v) {
      LOG("Image allocation error source %ldx%ld display %ldx%ld picture %ldx%ld",
          img->d_w, img->d_h, mDisplayWidth, mDisplayHeight,
          picture.width, picture.height);
      return -1;
    }
    mCallback->Output(v);
    return 1;
  }
  mIter = nullptr;
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
    NS_NewRunnableMethodWithArg<nsRefPtr<MediaRawData>>(
      this, &VPXDecoder::DecodeFrame,
      nsRefPtr<MediaRawData>(aSample)));
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
