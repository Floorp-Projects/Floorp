/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegDataDecoder_h__
#define __FFmpegDataDecoder_h__

#include "PlatformDecoderModule.h"
#include "FFmpegLibWrapper.h"
#include "mozilla/StaticMutex.h"
#include "FFmpegLibs.h"

namespace mozilla {

template <int V>
class FFmpegDataDecoder : public MediaDataDecoder
{
};

template<>
class FFmpegDataDecoder<LIBAV_VER>;
DDLoggedTypeNameAndBase(FFmpegDataDecoder<LIBAV_VER>, MediaDataDecoder);

template<>
class FFmpegDataDecoder<LIBAV_VER>
  : public MediaDataDecoder
  , public DecoderDoctorLifeLogger<FFmpegDataDecoder<LIBAV_VER>>
{
public:
  FFmpegDataDecoder(FFmpegLibWrapper* aLib, TaskQueue* aTaskQueue,
                    AVCodecID aCodecID);
  virtual ~FFmpegDataDecoder();

  static bool Link();

  RefPtr<InitPromise> Init() override = 0;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;

  static AVCodec* FindAVCodec(FFmpegLibWrapper* aLib, AVCodecID aCodec);

protected:
  // Flush and Drain operation, always run
  virtual RefPtr<FlushPromise> ProcessFlush();
  virtual void ProcessShutdown();
  virtual void InitCodecContext() { }
  AVFrame* PrepareFrame();
  MediaResult InitDecoder();
  MediaResult DoDecode(MediaRawData* aSample,
                       bool* aGotFrame,
                       DecodedData& aOutResults);

  FFmpegLibWrapper* mLib;

  AVCodecContext* mCodecContext;
  AVCodecParserContext* mCodecParser;
  AVFrame* mFrame;
  RefPtr<MediaByteBuffer> mExtraData;
  AVCodecID mCodecID;

private:
  RefPtr<DecodePromise> ProcessDecode(MediaRawData* aSample);
  RefPtr<DecodePromise> ProcessDrain();
  virtual MediaResult DoDecode(MediaRawData* aSample,
                               uint8_t* aData,
                               int aSize,
                               bool* aGotFrame,
                               MediaDataDecoder::DecodedData& aOutResults) = 0;
  virtual bool NeedParser() const { return false; }
  virtual int ParserFlags() const { return PARSER_FLAG_COMPLETE_FRAMES; }

  static StaticMutex sMonitor;
  const RefPtr<TaskQueue> mTaskQueue;
  MozPromiseHolder<DecodePromise> mPromise;
  media::TimeUnit mLastInputDts;
};

} // namespace mozilla

#endif // __FFmpegDataDecoder_h__
