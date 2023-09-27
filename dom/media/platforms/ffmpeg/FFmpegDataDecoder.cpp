/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include "libavutil/dict.h"
#include "libavcodec/avcodec.h"
#ifdef __GNUC__
#  include <unistd.h>
#endif

#include "FFmpegDataDecoder.h"
#include "FFmpegLog.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/TaskQueue.h"
#include "prsystem.h"
#include "VideoUtils.h"
#include "FFmpegLibs.h"

namespace mozilla {

StaticMutex FFmpegDataDecoder<LIBAV_VER>::sMutex;

static bool IsVideoCodec(AVCodecID aCodecID) {
  switch (aCodecID) {
    case AV_CODEC_ID_H264:
#if LIBAVCODEC_VERSION_MAJOR >= 54
    case AV_CODEC_ID_VP8:
#endif
#if LIBAVCODEC_VERSION_MAJOR >= 55
    case AV_CODEC_ID_VP9:
#endif
#if LIBAVCODEC_VERSION_MAJOR >= 59
    case AV_CODEC_ID_AV1:
#endif
      return true;
    default:
      return false;
  }
}

FFmpegDataDecoder<LIBAV_VER>::FFmpegDataDecoder(FFmpegLibWrapper* aLib,
                                                AVCodecID aCodecID)
    : mLib(aLib),
      mCodecContext(nullptr),
      mCodecParser(nullptr),
      mFrame(nullptr),
      mExtraData(nullptr),
      mCodecID(aCodecID),
      mVideoCodec(IsVideoCodec(aCodecID)),
      mTaskQueue(TaskQueue::Create(
          GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER),
          "FFmpegDataDecoder")),
      mLastInputDts(media::TimeUnit::FromNegativeInfinity()) {
  MOZ_ASSERT(aLib);
  MOZ_COUNT_CTOR(FFmpegDataDecoder);
}

FFmpegDataDecoder<LIBAV_VER>::~FFmpegDataDecoder() {
  MOZ_COUNT_DTOR(FFmpegDataDecoder);
  if (mCodecParser) {
    mLib->av_parser_close(mCodecParser);
    mCodecParser = nullptr;
  }
}

MediaResult FFmpegDataDecoder<LIBAV_VER>::AllocateExtraData() {
  if (mExtraData) {
    mCodecContext->extradata_size = mExtraData->Length();
    // FFmpeg may use SIMD instructions to access the data which reads the
    // data in 32 bytes block. Must ensure we have enough data to read.
    uint32_t padding_size =
#if LIBAVCODEC_VERSION_MAJOR >= 58
        AV_INPUT_BUFFER_PADDING_SIZE;
#else
        FF_INPUT_BUFFER_PADDING_SIZE;
#endif
    mCodecContext->extradata = static_cast<uint8_t*>(
        mLib->av_malloc(mExtraData->Length() + padding_size));
    if (!mCodecContext->extradata) {
      return MediaResult(NS_ERROR_OUT_OF_MEMORY,
                         RESULT_DETAIL("Couldn't init ffmpeg extradata"));
    }
    memcpy(mCodecContext->extradata, mExtraData->Elements(),
           mExtraData->Length());
  } else {
    mCodecContext->extradata_size = 0;
  }

  return NS_OK;
}

// Note: This doesn't run on the ffmpeg TaskQueue, it runs on some other media
// taskqueue
MediaResult FFmpegDataDecoder<LIBAV_VER>::InitDecoder(AVDictionary** aOptions) {
  FFMPEG_LOG("Initialising FFmpeg decoder");

  AVCodec* codec = FindAVCodec(mLib, mCodecID);
  if (!codec) {
    FFMPEG_LOG("  couldn't find ffmpeg decoder for codec id %d", mCodecID);
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("unable to find codec"));
  }
  // openh264 has broken decoding of some h264 videos so
  // don't use it unless explicitly allowed for now.
  if (!strcmp(codec->name, "libopenh264") &&
      !StaticPrefs::media_ffmpeg_allow_openh264()) {
    FFMPEG_LOG("  unable to find codec (openh264 disabled by pref)");
    return MediaResult(
        NS_ERROR_DOM_MEDIA_FATAL_ERR,
        RESULT_DETAIL("unable to find codec (openh264 disabled by pref)"));
  }
  FFMPEG_LOG("  codec %s : %s", codec->name, codec->long_name);

  StaticMutexAutoLock mon(sMutex);

  if (!(mCodecContext = mLib->avcodec_alloc_context3(codec))) {
    FFMPEG_LOG("  couldn't allocate ffmpeg context for codec %s", codec->name);
    return MediaResult(NS_ERROR_OUT_OF_MEMORY,
                       RESULT_DETAIL("Couldn't init ffmpeg context"));
  }

  if (NeedParser()) {
    MOZ_ASSERT(mCodecParser == nullptr);
    mCodecParser = mLib->av_parser_init(mCodecID);
    if (mCodecParser) {
      mCodecParser->flags |= ParserFlags();
    }
  }
  mCodecContext->opaque = this;

  InitCodecContext();
  MediaResult ret = AllocateExtraData();
  if (NS_FAILED(ret)) {
    FFMPEG_LOG("  couldn't allocate ffmpeg extra data for codec %s",
               codec->name);
    mLib->av_freep(&mCodecContext);
    return ret;
  }

#if LIBAVCODEC_VERSION_MAJOR < 57
  if (codec->capabilities & CODEC_CAP_DR1) {
    mCodecContext->flags |= CODEC_FLAG_EMU_EDGE;
  }
#endif

  if (mLib->avcodec_open2(mCodecContext, codec, aOptions) < 0) {
    if (mCodecContext->extradata) {
      mLib->av_freep(&mCodecContext->extradata);
    }
    mLib->av_freep(&mCodecContext);
    FFMPEG_LOG("  Couldn't open avcodec");
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("Couldn't open avcodec"));
  }

  FFMPEG_LOG("  FFmpeg decoder init successful.");
  return NS_OK;
}

RefPtr<ShutdownPromise> FFmpegDataDecoder<LIBAV_VER>::Shutdown() {
  RefPtr<FFmpegDataDecoder<LIBAV_VER>> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self]() {
    self->ProcessShutdown();
    return self->mTaskQueue->BeginShutdown();
  });
}

RefPtr<MediaDataDecoder::DecodePromise> FFmpegDataDecoder<LIBAV_VER>::Decode(
    MediaRawData* aSample) {
  return InvokeAsync<MediaRawData*>(mTaskQueue, this, __func__,
                                    &FFmpegDataDecoder::ProcessDecode, aSample);
}

RefPtr<MediaDataDecoder::DecodePromise>
FFmpegDataDecoder<LIBAV_VER>::ProcessDecode(MediaRawData* aSample) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  PROCESS_DECODE_LOG(aSample);
  bool gotFrame = false;
  DecodedData results;
  MediaResult rv = DoDecode(aSample, &gotFrame, results);
  if (NS_FAILED(rv)) {
    return DecodePromise::CreateAndReject(rv, __func__);
  }
  return DecodePromise::CreateAndResolve(std::move(results), __func__);
}

MediaResult FFmpegDataDecoder<LIBAV_VER>::DoDecode(
    MediaRawData* aSample, bool* aGotFrame,
    MediaDataDecoder::DecodedData& aResults) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  uint8_t* inputData = const_cast<uint8_t*>(aSample->Data());
  size_t inputSize = aSample->Size();

  mLastInputDts = aSample->mTimecode;

  if (inputData && mCodecParser) {  // inputData is null when draining.
    if (aGotFrame) {
      *aGotFrame = false;
    }
    while (inputSize) {
      uint8_t* data = inputData;
      int size = inputSize;
      int len = mLib->av_parser_parse2(
          mCodecParser, mCodecContext, &data, &size, inputData, inputSize,
          aSample->mTime.ToMicroseconds(), aSample->mTimecode.ToMicroseconds(),
          aSample->mOffset);
      if (size_t(len) > inputSize) {
        return NS_ERROR_DOM_MEDIA_DECODE_ERR;
      }
      if (size) {
        bool gotFrame = false;
        MediaResult rv = DoDecode(aSample, data, size, &gotFrame, aResults);
        if (NS_FAILED(rv)) {
          return rv;
        }
        if (gotFrame && aGotFrame) {
          *aGotFrame = true;
        }
      }
      inputData += len;
      inputSize -= len;
    }
    return NS_OK;
  }
  return DoDecode(aSample, inputData, inputSize, aGotFrame, aResults);
}

RefPtr<MediaDataDecoder::FlushPromise> FFmpegDataDecoder<LIBAV_VER>::Flush() {
  return InvokeAsync(mTaskQueue, this, __func__,
                     &FFmpegDataDecoder<LIBAV_VER>::ProcessFlush);
}

RefPtr<MediaDataDecoder::DecodePromise> FFmpegDataDecoder<LIBAV_VER>::Drain() {
  return InvokeAsync(mTaskQueue, this, __func__,
                     &FFmpegDataDecoder<LIBAV_VER>::ProcessDrain);
}

RefPtr<MediaDataDecoder::DecodePromise>
FFmpegDataDecoder<LIBAV_VER>::ProcessDrain() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  FFMPEG_LOG("FFmpegDataDecoder: draining buffers");
  RefPtr<MediaRawData> empty(new MediaRawData());
  empty->mTimecode = mLastInputDts;
  bool gotFrame = false;
  DecodedData results;
  // When draining the FFmpeg decoder will return either a single frame at a
  // time until gotFrame is set to false; or return a block of frames with
  // NS_ERROR_DOM_MEDIA_END_OF_STREAM
  while (NS_SUCCEEDED(DoDecode(empty, &gotFrame, results)) && gotFrame) {
  }
  return DecodePromise::CreateAndResolve(std::move(results), __func__);
}

RefPtr<MediaDataDecoder::FlushPromise>
FFmpegDataDecoder<LIBAV_VER>::ProcessFlush() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  if (mCodecContext) {
    FFMPEG_LOG("FFmpegDataDecoder: flushing buffers");
    mLib->avcodec_flush_buffers(mCodecContext);
  }
  if (mCodecParser) {
    FFMPEG_LOG("FFmpegDataDecoder: reinitializing parser");
    mLib->av_parser_close(mCodecParser);
    mCodecParser = mLib->av_parser_init(mCodecID);
  }
  return FlushPromise::CreateAndResolve(true, __func__);
}

void FFmpegDataDecoder<LIBAV_VER>::ProcessShutdown() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  StaticMutexAutoLock mon(sMutex);

  if (mCodecContext) {
    FFMPEG_LOG("FFmpegDataDecoder: shutdown");
    if (mCodecContext->extradata) {
      mLib->av_freep(&mCodecContext->extradata);
    }
    mLib->avcodec_close(mCodecContext);
    mLib->av_freep(&mCodecContext);
#if LIBAVCODEC_VERSION_MAJOR >= 55
    mLib->av_frame_free(&mFrame);
#elif LIBAVCODEC_VERSION_MAJOR == 54
    mLib->avcodec_free_frame(&mFrame);
#else
    mLib->av_freep(&mFrame);
#endif
  }
}

AVFrame* FFmpegDataDecoder<LIBAV_VER>::PrepareFrame() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
#if LIBAVCODEC_VERSION_MAJOR >= 55
  if (mFrame) {
    mLib->av_frame_unref(mFrame);
  } else {
    mFrame = mLib->av_frame_alloc();
  }
#elif LIBAVCODEC_VERSION_MAJOR == 54
  if (mFrame) {
    mLib->avcodec_get_frame_defaults(mFrame);
  } else {
    mFrame = mLib->avcodec_alloc_frame();
  }
#else
  mLib->av_freep(&mFrame);
  mFrame = mLib->avcodec_alloc_frame();
#endif
  return mFrame;
}

/* static */ AVCodec* FFmpegDataDecoder<LIBAV_VER>::FindAVCodec(
    FFmpegLibWrapper* aLib, AVCodecID aCodec) {
  return aLib->avcodec_find_decoder(aCodec);
}

#ifdef MOZ_WIDGET_GTK
/* static */ AVCodec* FFmpegDataDecoder<LIBAV_VER>::FindHardwareAVCodec(
    FFmpegLibWrapper* aLib, AVCodecID aCodec) {
  void* opaque = nullptr;
  while (AVCodec* codec = aLib->av_codec_iterate(&opaque)) {
    if (codec->id == aCodec && aLib->av_codec_is_decoder(codec) &&
        aLib->avcodec_get_hw_config(codec, 0)) {
      return codec;
    }
  }
  return nullptr;
}
#endif

}  // namespace mozilla
