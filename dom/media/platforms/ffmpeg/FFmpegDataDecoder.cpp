/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TaskQueue.h"

#include <string.h>
#include <unistd.h>

#include "FFmpegLibs.h"
#include "FFmpegLog.h"
#include "FFmpegDataDecoder.h"
#include "prsystem.h"
#include "FFmpegRuntimeLinker.h"

namespace mozilla
{

bool FFmpegDataDecoder<LIBAV_VER>::sFFmpegInitDone = false;
StaticMutex FFmpegDataDecoder<LIBAV_VER>::sMonitor;

FFmpegDataDecoder<LIBAV_VER>::FFmpegDataDecoder(FlushableTaskQueue* aTaskQueue,
                                                MediaDataDecoderCallback* aCallback,
                                                AVCodecID aCodecID)
  : mTaskQueue(aTaskQueue)
  , mCallback(aCallback)
  , mCodecContext(nullptr)
  , mFrame(NULL)
  , mExtraData(nullptr)
  , mCodecID(aCodecID)
  , mMonitor("FFMpegaDataDecoder")
  , mIsFlushing(false)
  , mCodecParser(nullptr)
{
  MOZ_COUNT_CTOR(FFmpegDataDecoder);
}

FFmpegDataDecoder<LIBAV_VER>::~FFmpegDataDecoder()
{
  MOZ_COUNT_DTOR(FFmpegDataDecoder);
  if (mCodecParser) {
    av_parser_close(mCodecParser);
    mCodecParser = nullptr;
  }
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
    if (*aFormats == PIX_FMT_YUV420P || *aFormats == PIX_FMT_YUVJ420P) {
      FFMPEG_LOG("Requesting pixel format YUV420P.");
      return PIX_FMT_YUV420P;
    }
  }

  NS_WARNING("FFmpeg does not share any supported pixel formats.");
  return PIX_FMT_NONE;
}

nsresult
FFmpegDataDecoder<LIBAV_VER>::InitDecoder()
{
  FFMPEG_LOG("Initialising FFmpeg decoder.");

  AVCodec* codec = FindAVCodec(mCodecID);
  if (!codec) {
    NS_WARNING("Couldn't find ffmpeg decoder");
    return NS_ERROR_FAILURE;
  }

  StaticMutexAutoLock mon(sMonitor);

  if (!(mCodecContext = avcodec_alloc_context3(codec))) {
    NS_WARNING("Couldn't init ffmpeg context");
    return NS_ERROR_FAILURE;
  }

  mCodecContext->opaque = this;

  // FFmpeg takes this as a suggestion for what format to use for audio samples.
  uint32_t major, minor;
  FFmpegRuntimeLinker::GetVersion(major, minor);
  // LibAV 0.8 produces rubbish float interleaved samples, request 16 bits audio.
  mCodecContext->request_sample_fmt = major == 53 ?
    AV_SAMPLE_FMT_S16 : AV_SAMPLE_FMT_FLT;

  // FFmpeg will call back to this to negotiate a video pixel format.
  mCodecContext->get_format = ChoosePixelFormat;

  mCodecContext->thread_count = PR_GetNumberOfProcessors();
  mCodecContext->thread_type = FF_THREAD_SLICE | FF_THREAD_FRAME;
  mCodecContext->thread_safe_callbacks = false;

  if (mExtraData) {
    mCodecContext->extradata_size = mExtraData->Length();
    // FFmpeg may use SIMD instructions to access the data which reads the
    // data in 32 bytes block. Must ensure we have enough data to read.
    mExtraData->AppendElements(FF_INPUT_BUFFER_PADDING_SIZE);
    mCodecContext->extradata = mExtraData->Elements();
  } else {
    mCodecContext->extradata_size = 0;
  }

  if (codec->capabilities & CODEC_CAP_DR1) {
    mCodecContext->flags |= CODEC_FLAG_EMU_EDGE;
  }

  if (avcodec_open2(mCodecContext, codec, nullptr) < 0) {
    NS_WARNING("Couldn't initialise ffmpeg decoder");
    avcodec_close(mCodecContext);
    av_freep(&mCodecContext);
    return NS_ERROR_FAILURE;
  }

  if (mCodecContext->codec_type == AVMEDIA_TYPE_AUDIO &&
      mCodecContext->sample_fmt != AV_SAMPLE_FMT_FLT &&
      mCodecContext->sample_fmt != AV_SAMPLE_FMT_FLTP &&
      mCodecContext->sample_fmt != AV_SAMPLE_FMT_S16 &&
      mCodecContext->sample_fmt != AV_SAMPLE_FMT_S16P) {
    NS_WARNING("FFmpeg audio decoder outputs unsupported audio format.");
    return NS_ERROR_FAILURE;
  }

  mCodecParser = av_parser_init(mCodecID);
  if (mCodecParser) {
    mCodecParser->flags |= PARSER_FLAG_COMPLETE_FRAMES;
  }

  FFMPEG_LOG("FFmpeg init successful.");
  return NS_OK;
}

nsresult
FFmpegDataDecoder<LIBAV_VER>::Shutdown()
{
  if (mTaskQueue) {
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableMethod(this, &FFmpegDataDecoder<LIBAV_VER>::ProcessShutdown);
    mTaskQueue->Dispatch(runnable.forget());
  } else {
    ProcessShutdown();
  }
  return NS_OK;
}

nsresult
FFmpegDataDecoder<LIBAV_VER>::Flush()
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  mIsFlushing = true;
  mTaskQueue->Flush();
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &FFmpegDataDecoder<LIBAV_VER>::ProcessFlush);
  MonitorAutoLock mon(mMonitor);
  mTaskQueue->Dispatch(runnable.forget());
  while (mIsFlushing) {
    mon.Wait();
  }
  return NS_OK;
}

nsresult
FFmpegDataDecoder<LIBAV_VER>::Drain()
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &FFmpegDataDecoder<LIBAV_VER>::ProcessDrain);
  mTaskQueue->Dispatch(runnable.forget());
  return NS_OK;
}

void
FFmpegDataDecoder<LIBAV_VER>::ProcessFlush()
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  if (mCodecContext) {
    avcodec_flush_buffers(mCodecContext);
  }
  MonitorAutoLock mon(mMonitor);
  mIsFlushing = false;
  mon.NotifyAll();
}

void
FFmpegDataDecoder<LIBAV_VER>::ProcessShutdown()
{
  StaticMutexAutoLock mon(sMonitor);

  if (sFFmpegInitDone && mCodecContext) {
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
}

AVFrame*
FFmpegDataDecoder<LIBAV_VER>::PrepareFrame()
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
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

/* static */ AVCodec*
FFmpegDataDecoder<LIBAV_VER>::FindAVCodec(AVCodecID aCodec)
{
  StaticMutexAutoLock mon(sMonitor);
  if (!sFFmpegInitDone) {
    avcodec_register_all();
#ifdef DEBUG
    av_log_set_level(AV_LOG_DEBUG);
#endif
    sFFmpegInitDone = true;
  }
  return avcodec_find_decoder(aCodec);
}
  
} // namespace mozilla
