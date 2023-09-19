/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegDataDecoder_h__
#define __FFmpegDataDecoder_h__

#include "FFmpegLibWrapper.h"
#include "PlatformDecoderModule.h"
#include "mozilla/StaticMutex.h"

// This must be the last header included
#include "FFmpegLibs.h"

namespace mozilla {

template <int V>
class FFmpegDataDecoder : public MediaDataDecoder {};

template <>
class FFmpegDataDecoder<LIBAV_VER>;
DDLoggedTypeNameAndBase(FFmpegDataDecoder<LIBAV_VER>, MediaDataDecoder);

template <>
class FFmpegDataDecoder<LIBAV_VER>
    : public MediaDataDecoder,
      public DecoderDoctorLifeLogger<FFmpegDataDecoder<LIBAV_VER>> {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FFmpegDataDecoder, final);

  FFmpegDataDecoder(FFmpegLibWrapper* aLib, AVCodecID aCodecID);

  static bool Link();

  RefPtr<InitPromise> Init() override = 0;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;

  static AVCodec* FindAVCodec(FFmpegLibWrapper* aLib, AVCodecID aCodec);
#ifdef MOZ_WIDGET_GTK
  static AVCodec* FindHardwareAVCodec(FFmpegLibWrapper* aLib, AVCodecID aCodec);
#endif

 protected:
  // Flush and Drain operation, always run
  virtual RefPtr<FlushPromise> ProcessFlush();
  virtual void ProcessShutdown();
  virtual void InitCodecContext() MOZ_REQUIRES(sMutex) {}
  AVFrame* PrepareFrame();
  MediaResult InitDecoder(AVDictionary** aOptions);
  MediaResult AllocateExtraData();
  MediaResult DoDecode(MediaRawData* aSample, bool* aGotFrame,
                       DecodedData& aResults);

  FFmpegLibWrapper* mLib;  // set in constructor

  // mCodecContext is accessed on taskqueue only, no locking needed
  AVCodecContext* mCodecContext;
  AVCodecParserContext* mCodecParser;
  AVFrame* mFrame;
  RefPtr<MediaByteBuffer> mExtraData;
  AVCodecID mCodecID;  // set in constructor
  bool mVideoCodec;

 protected:
  virtual ~FFmpegDataDecoder();

  static StaticMutex sMutex;  // used to provide critical-section locking
                              // for calls into ffmpeg
  const RefPtr<TaskQueue> mTaskQueue;  // set in constructor

 private:
  RefPtr<DecodePromise> ProcessDecode(MediaRawData* aSample);
  RefPtr<DecodePromise> ProcessDrain();
  virtual MediaResult DoDecode(MediaRawData* aSample, uint8_t* aData, int aSize,
                               bool* aGotFrame,
                               MediaDataDecoder::DecodedData& aOutResults) = 0;
  virtual bool NeedParser() const { return false; }
  virtual int ParserFlags() const { return PARSER_FLAG_COMPLETE_FRAMES; }

  MozPromiseHolder<DecodePromise> mPromise;
  media::TimeUnit mLastInputDts;  // used on Taskqueue
};

}  // namespace mozilla

#endif  // __FFmpegDataDecoder_h__
