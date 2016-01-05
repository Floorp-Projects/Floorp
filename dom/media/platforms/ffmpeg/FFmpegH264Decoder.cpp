/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TaskQueue.h"

#include "nsThreadUtils.h"
#include "nsAutoPtr.h"
#include "ImageContainer.h"

#include "MediaInfo.h"

#include "FFmpegH264Decoder.h"
#include "FFmpegLog.h"
#include "mozilla/PodOperations.h"

typedef mozilla::layers::Image Image;
typedef mozilla::layers::PlanarYCbCrImage PlanarYCbCrImage;

namespace mozilla
{

FFmpegH264Decoder<LIBAV_VER>::PtsCorrectionContext::PtsCorrectionContext()
  : mNumFaultyPts(0)
  , mNumFaultyDts(0)
  , mLastPts(INT64_MIN)
  , mLastDts(INT64_MIN)
{
}

int64_t
FFmpegH264Decoder<LIBAV_VER>::PtsCorrectionContext::GuessCorrectPts(int64_t aPts, int64_t aDts)
{
  int64_t pts = AV_NOPTS_VALUE;

  if (aDts != int64_t(AV_NOPTS_VALUE)) {
    mNumFaultyDts += aDts <= mLastDts;
    mLastDts = aDts;
  }
  if (aPts != int64_t(AV_NOPTS_VALUE)) {
    mNumFaultyPts += aPts <= mLastPts;
    mLastPts = aPts;
  }
  if ((mNumFaultyPts <= mNumFaultyDts || aDts == int64_t(AV_NOPTS_VALUE)) &&
      aPts != int64_t(AV_NOPTS_VALUE)) {
    pts = aPts;
  } else {
    pts = aDts;
  }
  return pts;
}

void
FFmpegH264Decoder<LIBAV_VER>::PtsCorrectionContext::Reset()
{
  mNumFaultyPts = 0;
  mNumFaultyDts = 0;
  mLastPts = INT64_MIN;
  mLastDts = INT64_MIN;
}

FFmpegH264Decoder<LIBAV_VER>::FFmpegH264Decoder(
  FlushableTaskQueue* aTaskQueue, MediaDataDecoderCallback* aCallback,
  const VideoInfo& aConfig,
  ImageContainer* aImageContainer)
  : FFmpegDataDecoder(aTaskQueue, aCallback, GetCodecId(aConfig.mMimeType))
  , mImageContainer(aImageContainer)
  , mDisplay(aConfig.mDisplay)
  , mImage(aConfig.mImage)
{
  MOZ_COUNT_CTOR(FFmpegH264Decoder);
  // Use a new MediaByteBuffer as the object will be modified during initialization.
  mExtraData = new MediaByteBuffer;
  mExtraData->AppendElements(*aConfig.mExtraData);
}

RefPtr<MediaDataDecoder::InitPromise>
FFmpegH264Decoder<LIBAV_VER>::Init()
{
  if (NS_FAILED(InitDecoder())) {
    return InitPromise::CreateAndReject(DecoderFailureReason::INIT_ERROR, __func__);
  }

  mCodecContext->width = mImage.width;
  mCodecContext->height = mImage.height;

  return InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
}

FFmpegH264Decoder<LIBAV_VER>::DecodeResult
FFmpegH264Decoder<LIBAV_VER>::DoDecodeFrame(MediaRawData* aSample)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

  uint8_t* inputData = const_cast<uint8_t*>(aSample->Data());
  size_t inputSize = aSample->Size();

#if LIBAVCODEC_VERSION_MAJOR >= 54
  if (inputSize && mCodecParser && (mCodecID == AV_CODEC_ID_VP8
#if LIBAVCODEC_VERSION_MAJOR >= 55
      || mCodecID == AV_CODEC_ID_VP9
#endif
      )) {
    bool gotFrame = false;
    while (inputSize) {
      uint8_t* data;
      int size;
      int len = av_parser_parse2(mCodecParser, mCodecContext, &data, &size,
                                 inputData, inputSize,
                                 aSample->mTime, aSample->mTimecode,
                                 aSample->mOffset);
      if (size_t(len) > inputSize) {
        mCallback->Error();
        return DecodeResult::DECODE_ERROR;
      }
      inputData += len;
      inputSize -= len;
      if (size) {
        switch (DoDecodeFrame(aSample, data, size)) {
          case DecodeResult::DECODE_ERROR:
            return DecodeResult::DECODE_ERROR;
          case DecodeResult::DECODE_FRAME:
            gotFrame = true;
            break;
          default:
            break;
        }
      }
    }
    return gotFrame ? DecodeResult::DECODE_FRAME : DecodeResult::DECODE_NO_FRAME;
  }
#endif
  return DoDecodeFrame(aSample, inputData, inputSize);
}

FFmpegH264Decoder<LIBAV_VER>::DecodeResult
FFmpegH264Decoder<LIBAV_VER>::DoDecodeFrame(MediaRawData* aSample,
                                            uint8_t* aData, int aSize)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

  AVPacket packet;
  av_init_packet(&packet);

  packet.data = aData;
  packet.size = aSize;
  packet.dts = aSample->mTimecode;
  packet.pts = aSample->mTime;
  packet.flags = aSample->mKeyframe ? AV_PKT_FLAG_KEY : 0;
  packet.pos = aSample->mOffset;

  // LibAV provides no API to retrieve the decoded sample's duration.
  // (FFmpeg >= 1.0 provides av_frame_get_pkt_duration)
  // As such we instead use a map using the dts as key that we will retrieve
  // later.
  // The map will have a typical size of 16 entry.
  mDurationMap.Insert(aSample->mTimecode, aSample->mDuration);

  if (!PrepareFrame()) {
    NS_WARNING("FFmpeg h264 decoder failed to allocate frame.");
    mCallback->Error();
    return DecodeResult::DECODE_ERROR;
  }

  // Required with old version of FFmpeg/LibAV
  mFrame->reordered_opaque = AV_NOPTS_VALUE;

  int decoded;
  int bytesConsumed =
    avcodec_decode_video2(mCodecContext, mFrame, &decoded, &packet);

  FFMPEG_LOG("DoDecodeFrame:decode_video: rv=%d decoded=%d "
             "(Input: pts(%lld) dts(%lld) Output: pts(%lld) "
             "opaque(%lld) pkt_pts(%lld) pkt_dts(%lld))",
             bytesConsumed, decoded, packet.pts, packet.dts, mFrame->pts,
             mFrame->reordered_opaque, mFrame->pkt_pts, mFrame->pkt_dts);

  if (bytesConsumed < 0) {
    NS_WARNING("FFmpeg video decoder error.");
    mCallback->Error();
    return DecodeResult::DECODE_ERROR;
  }

  // If we've decoded a frame then we need to output it
  if (decoded) {
    int64_t pts = mPtsContext.GuessCorrectPts(mFrame->pkt_pts, mFrame->pkt_dts);
    FFMPEG_LOG("Got one frame output with pts=%lld opaque=%lld",
               pts, mCodecContext->reordered_opaque);
    // Retrieve duration from dts.
    // We use the first entry found matching this dts (this is done to
    // handle damaged file with multiple frames with the same dts)

    int64_t duration;
    if (!mDurationMap.Find(mFrame->pkt_dts, duration)) {
      NS_WARNING("Unable to retrieve duration from map");
      duration = aSample->mDuration;
      // dts are probably incorrectly reported ; so clear the map as we're
      // unlikely to find them in the future anyway. This also guards
      // against the map becoming extremely big.
      mDurationMap.Clear();
    }

    VideoInfo info;
    info.mDisplay = mDisplay;

    VideoData::YCbCrBuffer b;
    b.mPlanes[0].mData = mFrame->data[0];
    b.mPlanes[0].mStride = mFrame->linesize[0];
    b.mPlanes[0].mHeight = mFrame->height;
    b.mPlanes[0].mWidth = mFrame->width;
    b.mPlanes[0].mOffset = b.mPlanes[0].mSkip = 0;

    b.mPlanes[1].mData = mFrame->data[1];
    b.mPlanes[1].mStride = mFrame->linesize[1];
    b.mPlanes[1].mHeight = (mFrame->height + 1) >> 1;
    b.mPlanes[1].mWidth = (mFrame->width + 1) >> 1;
    b.mPlanes[1].mOffset = b.mPlanes[1].mSkip = 0;

    b.mPlanes[2].mData = mFrame->data[2];
    b.mPlanes[2].mStride = mFrame->linesize[2];
    b.mPlanes[2].mHeight = (mFrame->height + 1) >> 1;
    b.mPlanes[2].mWidth = (mFrame->width + 1) >> 1;
    b.mPlanes[2].mOffset = b.mPlanes[2].mSkip = 0;

    RefPtr<VideoData> v = VideoData::Create(info,
                                              mImageContainer,
                                              aSample->mOffset,
                                              pts,
                                              duration,
                                              b,
                                              !!mFrame->key_frame,
                                              -1,
                                              mImage);
    if (!v) {
      NS_WARNING("image allocation error.");
      mCallback->Error();
      return DecodeResult::DECODE_ERROR;
    }
    mCallback->Output(v);
    return DecodeResult::DECODE_FRAME;
  }
  return DecodeResult::DECODE_NO_FRAME;
}

void
FFmpegH264Decoder<LIBAV_VER>::DecodeFrame(MediaRawData* aSample)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

  if (DoDecodeFrame(aSample) != DecodeResult::DECODE_ERROR &&
      mTaskQueue->IsEmpty()) {
    mCallback->InputExhausted();
  }
}

nsresult
FFmpegH264Decoder<LIBAV_VER>::Input(MediaRawData* aSample)
{
  nsCOMPtr<nsIRunnable> runnable(
    NS_NewRunnableMethodWithArg<RefPtr<MediaRawData>>(
      this, &FFmpegH264Decoder<LIBAV_VER>::DecodeFrame,
      RefPtr<MediaRawData>(aSample)));
  mTaskQueue->Dispatch(runnable.forget());

  return NS_OK;
}

void
FFmpegH264Decoder<LIBAV_VER>::ProcessDrain()
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  RefPtr<MediaRawData> empty(new MediaRawData());
  while (DoDecodeFrame(empty) == DecodeResult::DECODE_FRAME) {
  }
  mCallback->DrainComplete();
}

void
FFmpegH264Decoder<LIBAV_VER>::ProcessFlush()
{
  mPtsContext.Reset();
  mDurationMap.Clear();
  FFmpegDataDecoder::ProcessFlush();
}

FFmpegH264Decoder<LIBAV_VER>::~FFmpegH264Decoder()
{
  MOZ_COUNT_DTOR(FFmpegH264Decoder);
}

AVCodecID
FFmpegH264Decoder<LIBAV_VER>::GetCodecId(const nsACString& aMimeType)
{
  if (aMimeType.EqualsLiteral("video/avc") || aMimeType.EqualsLiteral("video/mp4")) {
    return AV_CODEC_ID_H264;
  }

  if (aMimeType.EqualsLiteral("video/x-vnd.on2.vp6")) {
    return AV_CODEC_ID_VP6F;
  }

#if LIBAVCODEC_VERSION_MAJOR >= 54
  if (aMimeType.EqualsLiteral("video/webm; codecs=vp8")) {
    return AV_CODEC_ID_VP8;
  }
#endif

#if LIBAVCODEC_VERSION_MAJOR >= 55
  if (aMimeType.EqualsLiteral("video/webm; codecs=vp9")) {
    return AV_CODEC_ID_VP9;
  }
#endif

  return AV_CODEC_ID_NONE;
}

} // namespace mozilla
