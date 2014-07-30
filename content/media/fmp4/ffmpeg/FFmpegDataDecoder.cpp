/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include <unistd.h>

#include "MediaTaskQueue.h"
#include "mp4_demuxer/mp4_demuxer.h"
#include "FFmpegLibs.h"
#include "FFmpegLog.h"
#include "FFmpegDataDecoder.h"
#include "prsystem.h"

namespace mozilla
{

bool FFmpegDataDecoder<LIBAV_VER>::sFFmpegInitDone = false;

FFmpegDataDecoder<LIBAV_VER>::FFmpegDataDecoder(MediaTaskQueue* aTaskQueue,
                                                AVCodecID aCodecID)
  : mTaskQueue(aTaskQueue)
  , mCodecContext(nullptr)
  , mFrame(NULL)
  , mCodecID(aCodecID)
{
  MOZ_COUNT_CTOR(FFmpegDataDecoder);
}

FFmpegDataDecoder<LIBAV_VER>::~FFmpegDataDecoder()
{
  MOZ_COUNT_DTOR(FFmpegDataDecoder);
}

/**
 * FFmpeg calls back to this function with a list of pixel formats it supports.
 * We choose a pixel format that we support and return it.
 * For now, we just look for YUV420P as it is the only non-HW accelerated format
 * supported by FFmpeg's H264 decoder.
 */
static PixelFormat
ChoosePixelFormat(AVCodecContext* aCodecContext, const PixelFormat* aFormats)
{
  FFMPEG_LOG("Choosing FFmpeg pixel format for video decoding.");
  for (; *aFormats > -1; aFormats++) {
    if (*aFormats == PIX_FMT_YUV420P) {
      FFMPEG_LOG("Requesting pixel format YUV420P.");
      return PIX_FMT_YUV420P;
    }
  }

  NS_WARNING("FFmpeg does not share any supported pixel formats.");
  return PIX_FMT_NONE;
}

nsresult
FFmpegDataDecoder<LIBAV_VER>::Init()
{
  FFMPEG_LOG("Initialising FFmpeg decoder.");

  if (!sFFmpegInitDone) {
    av_register_all();
#ifdef DEBUG
    av_log_set_level(AV_LOG_DEBUG);
#endif
    sFFmpegInitDone = true;
  }

  AVCodec* codec = avcodec_find_decoder(mCodecID);
  if (!codec) {
    NS_WARNING("Couldn't find ffmpeg decoder");
    return NS_ERROR_FAILURE;
  }

  if (!(mCodecContext = avcodec_alloc_context3(codec))) {
    NS_WARNING("Couldn't init ffmpeg context");
    return NS_ERROR_FAILURE;
  }

  mCodecContext->opaque = this;

  // FFmpeg takes this as a suggestion for what format to use for audio samples.
  mCodecContext->request_sample_fmt = AV_SAMPLE_FMT_FLT;

  // FFmpeg will call back to this to negotiate a video pixel format.
  mCodecContext->get_format = ChoosePixelFormat;

  mCodecContext->thread_count = PR_GetNumberOfProcessors();
  mCodecContext->thread_type = FF_THREAD_SLICE | FF_THREAD_FRAME;
  mCodecContext->thread_safe_callbacks = false;

  mCodecContext->extradata_size = mExtraData.length();
  for (int i = 0; i < FF_INPUT_BUFFER_PADDING_SIZE; i++) {
    mExtraData.append(0);
  }
  mCodecContext->extradata = mExtraData.begin();

  if (codec->capabilities & CODEC_CAP_DR1) {
    mCodecContext->flags |= CODEC_FLAG_EMU_EDGE;
  }

  if (avcodec_open2(mCodecContext, codec, nullptr) < 0) {
    NS_WARNING("Couldn't initialise ffmpeg decoder");
    return NS_ERROR_FAILURE;
  }

  if (mCodecContext->codec_type == AVMEDIA_TYPE_AUDIO &&
      mCodecContext->sample_fmt != AV_SAMPLE_FMT_FLT &&
      mCodecContext->sample_fmt != AV_SAMPLE_FMT_FLTP) {
    NS_WARNING("FFmpeg AAC decoder outputs unsupported audio format.");
    return NS_ERROR_FAILURE;
  }

  FFMPEG_LOG("FFmpeg init successful.");
  return NS_OK;
}

nsresult
FFmpegDataDecoder<LIBAV_VER>::Flush()
{
  mTaskQueue->Flush();
  avcodec_flush_buffers(mCodecContext);
  return NS_OK;
}

nsresult
FFmpegDataDecoder<LIBAV_VER>::Shutdown()
{
  if (sFFmpegInitDone) {
    avcodec_close(mCodecContext);
    av_freep(&mCodecContext);
#if LIBAVCODEC_VERSION_MAJOR >= 55
    av_frame_free(&mFrame);
#elif LIBAVCODEC_VERSION_MAJOR == 54
    avcodec_free_frame(&mFrame);
#else
    delete mFrame;
    mFrame = nullptr;
#endif
  }
  return NS_OK;
}

AVFrame*
FFmpegDataDecoder<LIBAV_VER>::PrepareFrame()
{
#if LIBAVCODEC_VERSION_MAJOR >= 55
  if (mFrame) {
    av_frame_unref(mFrame);
  } else {
    mFrame = av_frame_alloc();
  }
#elif LIBAVCODEC_VERSION_MAJOR == 54
  if (mFrame) {
    avcodec_get_frame_defaults(mFrame);
  } else {
    mFrame = avcodec_alloc_frame();
  }
#else
  delete mFrame;
  mFrame = new AVFrame;
  avcodec_get_frame_defaults(mFrame);
#endif
  return mFrame;
}

} // namespace mozilla
