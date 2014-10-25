/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTaskQueue.h"
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"
#include "ImageContainer.h"

#include "mp4_demuxer/mp4_demuxer.h"

#include "FFmpegH264Decoder.h"

#define GECKO_FRAME_TYPE 0x00093CC0

typedef mozilla::layers::Image Image;
typedef mozilla::layers::PlanarYCbCrImage PlanarYCbCrImage;

typedef mp4_demuxer::MP4Sample MP4Sample;

namespace mozilla
{

FFmpegH264Decoder<LIBAV_VER>::FFmpegH264Decoder(
  MediaTaskQueue* aTaskQueue, MediaDataDecoderCallback* aCallback,
  const mp4_demuxer::VideoDecoderConfig& aConfig,
  ImageContainer* aImageContainer)
  : FFmpegDataDecoder(aTaskQueue, AV_CODEC_ID_H264)
  , mCallback(aCallback)
  , mImageContainer(aImageContainer)
{
  MOZ_COUNT_CTOR(FFmpegH264Decoder);
  mExtraData.append(aConfig.extra_data.begin(), aConfig.extra_data.length());
}

nsresult
FFmpegH264Decoder<LIBAV_VER>::Init()
{
  nsresult rv = FFmpegDataDecoder::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  mCodecContext->get_buffer = AllocateBufferCb;
  mCodecContext->release_buffer = ReleaseBufferCb;

  return NS_OK;
}

FFmpegH264Decoder<LIBAV_VER>::DecodeResult
FFmpegH264Decoder<LIBAV_VER>::DoDecodeFrame(mp4_demuxer::MP4Sample* aSample)
{
  AVPacket packet;
  av_init_packet(&packet);

  aSample->Pad(FF_INPUT_BUFFER_PADDING_SIZE);
  packet.data = aSample->data;
  packet.size = aSample->size;
  packet.dts = aSample->decode_timestamp;
  packet.pts = aSample->composition_timestamp;
  packet.flags = aSample->is_sync_point ? AV_PKT_FLAG_KEY : 0;
  packet.pos = aSample->byte_offset;

  if (!PrepareFrame()) {
    NS_WARNING("FFmpeg h264 decoder failed to allocate frame.");
    mCallback->Error();
    return DecodeResult::DECODE_ERROR;
  }

  int decoded;
  int bytesConsumed =
    avcodec_decode_video2(mCodecContext, mFrame, &decoded, &packet);

  if (bytesConsumed < 0) {
    NS_WARNING("FFmpeg video decoder error.");
    mCallback->Error();
    return DecodeResult::DECODE_ERROR;
  }

  // If we've decoded a frame then we need to output it
  if (decoded) {
    VideoInfo info;
    info.mDisplay = nsIntSize(mCodecContext->width, mCodecContext->height);
    info.mStereoMode = StereoMode::MONO;
    info.mHasVideo = true;

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

    VideoData *v = VideoData::Create(info,
                                     mImageContainer,
                                     aSample->byte_offset,
                                     mFrame->pkt_pts,
                                     aSample->duration,
                                     b,
                                     aSample->is_sync_point,
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
FFmpegH264Decoder<LIBAV_VER>::DecodeFrame(mp4_demuxer::MP4Sample* aSample)
{
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
  int edgeWidth =  needAlign ? avcodec_get_edge_width() : 0;
  int decodeWidth = aCodecContext->width + edgeWidth * 2;
  int decodeHeight = aCodecContext->height + edgeWidth * 2;

  if (needAlign) {
    // Align width and height to account for CODEC_FLAG_EMU_EDGE.
    int stride_align[AV_NUM_DATA_POINTERS];
    avcodec_align_dimensions2(aCodecContext, &decodeWidth, &decodeHeight,
                              stride_align);
  }

  // Get strides for each plane.
  av_image_fill_linesizes(aFrame->linesize, aCodecContext->pix_fmt,
                          decodeWidth);

  // Let FFmpeg set up its YUV plane pointers and tell us how much memory we
  // need.
  // Note that we're passing |nullptr| here as the base address as we haven't
  // allocated our image yet. We will adjust |aFrame->data| below.
  size_t allocSize =
    av_image_fill_pointers(aFrame->data, aCodecContext->pix_fmt, decodeHeight,
                           nullptr /* base address */, aFrame->linesize);

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

  // Now that we've allocated our image, we can add its address to the offsets
  // set by |av_image_fill_pointers| above. We also have to add |edgeWidth|
  // pixels of padding here.
  for (uint32_t i = 0; i < AV_NUM_DATA_POINTERS; i++) {
    // The C planes are half the resolution of the Y plane, so we need to halve
    // the edge width here.
    uint32_t planeEdgeWidth = edgeWidth / (i ? 2 : 1);

    // Add buffer offset, plus a horizontal bar |edgeWidth| pixels high at the
    // top of the frame, plus |edgeWidth| pixels from the left of the frame.
    aFrame->data[i] += reinterpret_cast<ptrdiff_t>(
      buffer + planeEdgeWidth * aFrame->linesize[i] + planeEdgeWidth);
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
FFmpegH264Decoder<LIBAV_VER>::Input(mp4_demuxer::MP4Sample* aSample)
{
  mTaskQueue->Dispatch(
    NS_NewRunnableMethodWithArg<nsAutoPtr<mp4_demuxer::MP4Sample>>(
      this, &FFmpegH264Decoder<LIBAV_VER>::DecodeFrame,
      nsAutoPtr<mp4_demuxer::MP4Sample>(aSample)));

  return NS_OK;
}

void
FFmpegH264Decoder<LIBAV_VER>::DoDrain()
{
  nsAutoPtr<MP4Sample> empty(new MP4Sample());
  while (DoDecodeFrame(empty) == DecodeResult::DECODE_FRAME) {
  }
  mCallback->DrainComplete();
}

nsresult
FFmpegH264Decoder<LIBAV_VER>::Drain()
{
  mTaskQueue->Dispatch(
    NS_NewRunnableMethod(this, &FFmpegH264Decoder<LIBAV_VER>::DoDrain));

  return NS_OK;
}

nsresult
FFmpegH264Decoder<LIBAV_VER>::Flush()
{
  nsresult rv = FFmpegDataDecoder::Flush();
  // Even if the above fails we may as well clear our frame queue.
  return rv;
}

FFmpegH264Decoder<LIBAV_VER>::~FFmpegH264Decoder()
{
  MOZ_COUNT_DTOR(FFmpegH264Decoder);
}

} // namespace mozilla
