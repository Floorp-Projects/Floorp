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

#define GECKO_FRAME_TYPE 0x00093CC0

typedef mozilla::layers::Image Image;
typedef mozilla::layers::PlanarYCbCrImage PlanarYCbCrImage;

namespace mozilla
{

FFmpegH264Decoder<LIBAV_VER>::FFmpegH264Decoder(
  FlushableTaskQueue* aTaskQueue, MediaDataDecoderCallback* aCallback,
  const VideoInfo& aConfig,
  ImageContainer* aImageContainer)
  : FFmpegDataDecoder(aTaskQueue, aCallback, GetCodecId(aConfig.mMimeType))
  , mImageContainer(aImageContainer)
  , mPictureWidth(aConfig.mImage.width)
  , mPictureHeight(aConfig.mImage.height)
  , mDisplayWidth(aConfig.mDisplay.width)
  , mDisplayHeight(aConfig.mDisplay.height)
{
  MOZ_COUNT_CTOR(FFmpegH264Decoder);
  // Use a new MediaByteBuffer as the object will be modified during initialization.
  mExtraData = new MediaByteBuffer;
  mExtraData->AppendElements(*aConfig.mExtraData);
}

nsRefPtr<MediaDataDecoder::InitPromise>
FFmpegH264Decoder<LIBAV_VER>::Init()
{
  if (NS_FAILED(InitDecoder())) {
    return InitPromise::CreateAndReject(DecoderFailureReason::INIT_ERROR, __func__);
  }

  mCodecContext->get_buffer = AllocateBufferCb;
  mCodecContext->release_buffer = ReleaseBufferCb;
  mCodecContext->width = mPictureWidth;
  mCodecContext->height = mPictureHeight;

  return InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
}

int64_t
FFmpegH264Decoder<LIBAV_VER>::GetPts(const AVPacket& packet)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
#if LIBAVCODEC_VERSION_MAJOR == 53
  if (mFrame->pkt_pts == 0) {
    return mFrame->pkt_dts;
  } else {
    return mFrame->pkt_pts;
  }
#else
  return mFrame->pkt_pts;
#endif
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
    int64_t pts = GetPts(packet);
    FFMPEG_LOG("Got one frame output with pts=%lld opaque=%lld",
               pts, mCodecContext->reordered_opaque);

    VideoInfo info;
    info.mDisplay = nsIntSize(mDisplayWidth, mDisplayHeight);

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

    nsRefPtr<VideoData> v = VideoData::Create(info,
                                              mImageContainer,
                                              aSample->mOffset,
                                              pts,
                                              aSample->mDuration,
                                              b,
                                              aSample->mKeyframe,
                                              -1,
                                              gfx::IntRect(0, 0, mCodecContext->width, mCodecContext->height));
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

/* static */ int
FFmpegH264Decoder<LIBAV_VER>::AllocateBufferCb(AVCodecContext* aCodecContext,
                                               AVFrame* aFrame)
{
  MOZ_ASSERT(aCodecContext->codec_type == AVMEDIA_TYPE_VIDEO);

  FFmpegH264Decoder* self =
    static_cast<FFmpegH264Decoder*>(aCodecContext->opaque);

  switch (aCodecContext->pix_fmt) {
  case PIX_FMT_YUV420P:
    return self->AllocateYUV420PVideoBuffer(aCodecContext, aFrame);
  default:
    return avcodec_default_get_buffer(aCodecContext, aFrame);
  }
}

/* static */ void
FFmpegH264Decoder<LIBAV_VER>::ReleaseBufferCb(AVCodecContext* aCodecContext,
                                              AVFrame* aFrame)
{
  switch (aCodecContext->pix_fmt) {
    case PIX_FMT_YUV420P: {
      Image* image = static_cast<Image*>(aFrame->opaque);
      if (image) {
        image->Release();
      }
      for (uint32_t i = 0; i < AV_NUM_DATA_POINTERS; i++) {
        aFrame->data[i] = nullptr;
      }
      break;
    }
    default:
      avcodec_default_release_buffer(aCodecContext, aFrame);
      break;
  }
}

int
FFmpegH264Decoder<LIBAV_VER>::AllocateYUV420PVideoBuffer(
  AVCodecContext* aCodecContext, AVFrame* aFrame)
{
  bool needAlign = aCodecContext->codec->capabilities & CODEC_CAP_DR1;
  bool needEdge = !(aCodecContext->flags & CODEC_FLAG_EMU_EDGE);
  int edgeWidth = needEdge ? avcodec_get_edge_width() : 0;

  int decodeWidth = aCodecContext->width + edgeWidth * 2;
  int decodeHeight = aCodecContext->height + edgeWidth * 2;

  if (needAlign) {
    // Align width and height to account for CODEC_CAP_DR1.
    // Make sure the decodeWidth is a multiple of 64, so a UV plane stride will be
    // a multiple of 32. FFmpeg uses SSE3 accelerated code to copy a frame line by
    // line.
    // VP9 decoder uses MOVAPS/VEX.256 which requires 32-bytes aligned memory.
    decodeWidth = (decodeWidth + 63) & ~63;
    decodeHeight = (decodeHeight + 63) & ~63;
  }

  PodZero(&aFrame->data[0], AV_NUM_DATA_POINTERS);
  PodZero(&aFrame->linesize[0], AV_NUM_DATA_POINTERS);

  int pitch = decodeWidth;
  int chroma_pitch  = (pitch + 1) / 2;
  int chroma_height = (decodeHeight +1) / 2;

  // Get strides for each plane.
  aFrame->linesize[0] = pitch;
  aFrame->linesize[1] = aFrame->linesize[2] = chroma_pitch;

  size_t allocSize = pitch * decodeHeight + (chroma_pitch * chroma_height) * 2;

  nsRefPtr<Image> image =
    mImageContainer->CreateImage(ImageFormat::PLANAR_YCBCR);
  PlanarYCbCrImage* ycbcr = static_cast<PlanarYCbCrImage*>(image.get());
  uint8_t* buffer = ycbcr->AllocateAndGetNewBuffer(allocSize + 64);
  // FFmpeg requires a 16/32 bytes-aligned buffer, align it on 64 to be safe
  buffer = reinterpret_cast<uint8_t*>((reinterpret_cast<uintptr_t>(buffer) + 63) & ~63);

  if (!buffer) {
    NS_WARNING("Failed to allocate buffer for FFmpeg video decoding");
    return -1;
  }

  int offsets[3] = {
    0,
    pitch * decodeHeight,
    pitch * decodeHeight + chroma_pitch * chroma_height };

  // Add a horizontal bar |edgeWidth| pixels high at the
  // top of the frame, plus |edgeWidth| pixels from the left of the frame.
  int planesEdgeWidth[3] = {
    edgeWidth * aFrame->linesize[0] + edgeWidth,
    edgeWidth / 2 * aFrame->linesize[1] + edgeWidth / 2,
    edgeWidth / 2 * aFrame->linesize[2] + edgeWidth / 2 };

  for (uint32_t i = 0; i < 3; i++) {
    aFrame->data[i] = buffer + offsets[i] + planesEdgeWidth[i];
  }

  // Unused, but needs to be non-zero to keep ffmpeg happy.
  aFrame->type = GECKO_FRAME_TYPE;

  aFrame->extended_data = aFrame->data;
  aFrame->width = aCodecContext->width;
  aFrame->height = aCodecContext->height;

  aFrame->opaque = static_cast<void*>(image.forget().take());

  return 0;
}

nsresult
FFmpegH264Decoder<LIBAV_VER>::Input(MediaRawData* aSample)
{
  nsCOMPtr<nsIRunnable> runnable(
    NS_NewRunnableMethodWithArg<nsRefPtr<MediaRawData>>(
      this, &FFmpegH264Decoder<LIBAV_VER>::DecodeFrame,
      nsRefPtr<MediaRawData>(aSample)));
  mTaskQueue->Dispatch(runnable.forget());

  return NS_OK;
}

void
FFmpegH264Decoder<LIBAV_VER>::ProcessDrain()
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  nsRefPtr<MediaRawData> empty(new MediaRawData());
  while (DoDecodeFrame(empty) == DecodeResult::DECODE_FRAME) {
  }
  mCallback->DrainComplete();
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
