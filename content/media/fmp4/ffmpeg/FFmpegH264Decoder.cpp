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

void
FFmpegH264Decoder<LIBAV_VER>::DecodeFrame(mp4_demuxer::MP4Sample* aSample)
{
  AVPacket packet;
  av_init_packet(&packet);

  aSample->Pad(FF_INPUT_BUFFER_PADDING_SIZE);
  packet.data = aSample->data;
  packet.size = aSample->size;
  packet.pts = aSample->composition_timestamp;
  packet.flags = aSample->is_sync_point ? AV_PKT_FLAG_KEY : 0;
  packet.pos = aSample->byte_offset;

  nsAutoPtr<AVFrame> frame(avcodec_alloc_frame());
  avcodec_get_frame_defaults(frame);

  int decoded;
  int bytesConsumed =
    avcodec_decode_video2(mCodecContext, frame, &decoded, &packet);

  if (bytesConsumed < 0) {
    NS_WARNING("FFmpeg video decoder error.");
    mCallback->Error();
    return;
  }

  // If we've decoded a frame then we need to output it
  if (decoded) {
    nsAutoPtr<VideoData> data;

    VideoInfo info;
    info.mDisplay = nsIntSize(mCodecContext->width, mCodecContext->height);
    info.mStereoMode = StereoMode::MONO;
    info.mHasVideo = true;

    data = VideoData::CreateFromImage(
      info, mImageContainer, aSample->byte_offset, frame->pkt_pts,
      aSample->duration, reinterpret_cast<Image*>(frame->opaque),
      aSample->is_sync_point, -1,
      gfx::IntRect(0, 0, mCodecContext->width, mCodecContext->height));

    mCallback->Output(data.forget());
  }

  if (mTaskQueue->IsEmpty()) {
    mCallback->InputExhausted();
  }
}

static void
PlanarYCbCrDataFromAVFrame(mozilla::layers::PlanarYCbCrData& aData,
                           AVFrame* aFrame)
{
  aData.mPicX = aData.mPicY = 0;
  aData.mPicSize = mozilla::gfx::IntSize(aFrame->width, aFrame->height);
  aData.mStereoMode = StereoMode::MONO;

  aData.mYChannel = aFrame->data[0];
  aData.mYStride = aFrame->linesize[0];
  aData.mYSize = aData.mPicSize;
  aData.mYSkip = 0;

  aData.mCbChannel = aFrame->data[1];
  aData.mCrChannel = aFrame->data[2];
  aData.mCbCrStride = aFrame->linesize[1];
  aData.mCbSkip = aData.mCrSkip = 0;
  aData.mCbCrSize =
    mozilla::gfx::IntSize((aFrame->width + 1) / 2, (aFrame->height + 1) / 2);
}

/* static */ int
FFmpegH264Decoder<LIBAV_VER>::AllocateBufferCb(AVCodecContext* aCodecContext,
                                               AVFrame* aFrame)
{
  MOZ_ASSERT(aCodecContext->codec_type == AVMEDIA_TYPE_VIDEO);

  FFmpegH264Decoder* self =
    reinterpret_cast<FFmpegH264Decoder*>(aCodecContext->opaque);

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
  Image* image = reinterpret_cast<Image*>(aFrame->opaque);
  avcodec_default_release_buffer(aCodecContext, aFrame);
  if (image) {
    image->Release();
  }
}

int
FFmpegH264Decoder<LIBAV_VER>::AllocateYUV420PVideoBuffer(
  AVCodecContext* aCodecContext, AVFrame* aFrame)
{
  // Older versions of ffmpeg require that edges be allocated* around* the
  // actual image.
  int edgeWidth = avcodec_get_edge_width();
  int decodeWidth = aCodecContext->width + edgeWidth * 2;
  int decodeHeight = aCodecContext->height + edgeWidth * 2;

  // Align width and height to possibly speed up decode.
  int stride_align[AV_NUM_DATA_POINTERS];
  avcodec_align_dimensions2(aCodecContext, &decodeWidth, &decodeHeight,
                            stride_align);

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
  PlanarYCbCrImage* ycbcr = reinterpret_cast<PlanarYCbCrImage*>(image.get());
  uint8_t* buffer = ycbcr->AllocateAndGetNewBuffer(allocSize);

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

  mozilla::layers::PlanarYCbCrData data;
  PlanarYCbCrDataFromAVFrame(data, aFrame);
  ycbcr->SetDataNoCopy(data);

  aFrame->opaque = reinterpret_cast<void*>(image.forget().take());

  return 0;
}

nsresult
FFmpegH264Decoder<LIBAV_VER>::Input(mp4_demuxer::MP4Sample* aSample)
{
  mTaskQueue->Dispatch(
    NS_NewRunnableMethodWithArg<nsAutoPtr<mp4_demuxer::MP4Sample> >(
      this, &FFmpegH264Decoder<LIBAV_VER>::DecodeFrame,
      nsAutoPtr<mp4_demuxer::MP4Sample>(aSample)));

  return NS_OK;
}

void
FFmpegH264Decoder<LIBAV_VER>::NotifyDrain()
{
  mCallback->DrainComplete();
}

nsresult
FFmpegH264Decoder<LIBAV_VER>::Drain()
{
  // The maximum number of frames that can be waiting to be decoded is
  // max_b_frames + 1: One P frame and max_b_frames B frames.
  for (int32_t i = 0; i <= mCodecContext->max_b_frames; i++) {
    // An empty frame tells FFmpeg to decode the next delayed frame it has in
    // its queue, if it has any.
    nsAutoPtr<MP4Sample> empty(new MP4Sample());

    nsresult rv = Input(empty.forget());
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mTaskQueue->Dispatch(
    NS_NewRunnableMethod(this, &FFmpegH264Decoder<LIBAV_VER>::NotifyDrain));

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
