/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegH264Decoder_h__
#define __FFmpegH264Decoder_h__

#include "nsTPriorityQueue.h"

#include "FFmpegDataDecoder.h"

namespace mozilla
{

class FFmpegH264Decoder : public FFmpegDataDecoder
{
  typedef mozilla::layers::Image Image;
  typedef mozilla::layers::ImageContainer ImageContainer;

public:
  FFmpegH264Decoder(MediaTaskQueue* aTaskQueue,
                    MediaDataDecoderCallback* aCallback,
                    const mp4_demuxer::VideoDecoderConfig &aConfig,
                    ImageContainer* aImageContainer);
  virtual ~FFmpegH264Decoder();

  virtual nsresult Init() MOZ_OVERRIDE;
  virtual nsresult Input(mp4_demuxer::MP4Sample* aSample) MOZ_OVERRIDE;
  virtual nsresult Drain() MOZ_OVERRIDE;
  virtual nsresult Flush() MOZ_OVERRIDE;

private:
  void DecodeFrame(mp4_demuxer::MP4Sample* aSample);
  void OutputDelayedFrames();

  /**
   * This method allocates a buffer for FFmpeg's decoder, wrapped in an Image.
   * Currently it only supports Planar YUV420, which appears to be the only
   * non-hardware accelerated image format that FFmpeg's H264 decoder is
   * capable of outputting.
   */
  int AllocateYUV420PVideoBuffer(AVCodecContext* aCodecContext,
                                 AVFrame* aFrame);

  static int AllocateBufferCb(AVCodecContext* aCodecContext, AVFrame* aFrame);

  mp4_demuxer::VideoDecoderConfig mConfig;
  MediaDataDecoderCallback* mCallback;
  nsRefPtr<ImageContainer> mImageContainer;

  /**
   * Pass Image back from the allocator to our DoDecode method.
   * We *should* return the image back through ffmpeg wrapped in an AVFrame like
   * we're meant to. However, if avcodec_decode_video2 fails, it returns a
   * completely different frame from the one holding our image and it will be
   * leaked.
   * This could be handled in the future by wrapping our Image in a reference
   * counted AVBuffer and letting ffmpeg hold the nsAutoPtr<Image>, but
   * currently we have to support older versions of ffmpeg which lack
   * refcounting.
   */
  nsRefPtr<Image> mCurrentImage;

  struct VideoDataComparator
  {
    bool LessThan(VideoData* const &a, VideoData* const &b) const
    {
      return a->mTime < b->mTime;
    }
  };

  /**
   * FFmpeg returns frames in DTS order, so we need to keep a heap of the
   * previous MAX_B_FRAMES + 1 frames (all B frames in a GOP + one P frame)
   * ordered by PTS to make sure we present video frames in the intended order.
   */
  nsTPriorityQueue<VideoData*, VideoDataComparator> mDelayedFrames;
};

} // namespace mozilla

#endif // __FFmpegH264Decoder_h__
