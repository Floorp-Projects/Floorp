/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include <unistd.h>

#include "MediaTaskQueue.h"
#include "mp4_demuxer/mp4_demuxer.h"
#include "FFmpegRuntimeLinker.h"

#include "FFmpegDataDecoder.h"

namespace mozilla
{

bool FFmpegDataDecoder::sFFmpegInitDone = false;

FFmpegDataDecoder::FFmpegDataDecoder(MediaTaskQueue* aTaskQueue,
                                     AVCodecID aCodecID)
  : mTaskQueue(aTaskQueue), mCodecID(aCodecID)
{
  MOZ_COUNT_CTOR(FFmpegDataDecoder);
}

FFmpegDataDecoder::~FFmpegDataDecoder() {
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
FFmpegDataDecoder::Init()
{
  FFMPEG_LOG("Initialising FFmpeg decoder.");

  if (!FFmpegRuntimeLinker::Link()) {
    NS_WARNING("Failed to link FFmpeg shared libraries.");
    return NS_ERROR_FAILURE;
  }

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

  if (avcodec_get_context_defaults3(&mCodecContext, codec) < 0) {
    NS_WARNING("Couldn't init ffmpeg context");
    return NS_ERROR_FAILURE;
  }

  mCodecContext.opaque = this;

  // FFmpeg takes this as a suggestion for what format to use for audio samples.
  mCodecContext.request_sample_fmt = AV_SAMPLE_FMT_FLT;

  // FFmpeg will call back to this to negotiate a video pixel format.
  mCodecContext.get_format = ChoosePixelFormat;

  AVDictionary* opts = nullptr;
  if (avcodec_open2(&mCodecContext, codec, &opts) < 0) {
    NS_WARNING("Couldn't initialise ffmpeg decoder");
    return NS_ERROR_FAILURE;
  }

  if (mCodecContext.codec_type == AVMEDIA_TYPE_AUDIO &&
      mCodecContext.sample_fmt != AV_SAMPLE_FMT_FLT &&
      mCodecContext.sample_fmt != AV_SAMPLE_FMT_FLTP) {
    NS_WARNING("FFmpeg AAC decoder outputs unsupported audio format.");
    return NS_ERROR_FAILURE;
  }

  FFMPEG_LOG("FFmpeg init successful.");
  return NS_OK;
}

nsresult
FFmpegDataDecoder::Flush()
{
  mTaskQueue->Flush();
  avcodec_flush_buffers(&mCodecContext);
  return NS_OK;
}

nsresult
FFmpegDataDecoder::Shutdown()
{
  if (sFFmpegInitDone) {
    avcodec_close(&mCodecContext);
  }
  return NS_OK;
}

} // namespace mozilla
