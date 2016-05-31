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

namespace mozilla
{

template <int V>
class FFmpegDataDecoder : public MediaDataDecoder
{
};

template <>
class FFmpegDataDecoder<LIBAV_VER> : public MediaDataDecoder
{
public:
  FFmpegDataDecoder(FFmpegLibWrapper* aLib, TaskQueue* aTaskQueue,
                    MediaDataDecoderCallback* aCallback,
                    AVCodecID aCodecID);
  virtual ~FFmpegDataDecoder();

  static bool Link();

  RefPtr<InitPromise> Init() override = 0;
  nsresult Input(MediaRawData* aSample) override;
  nsresult Flush() override;
  nsresult Drain() override;
  nsresult Shutdown() override;

  static AVCodec* FindAVCodec(FFmpegLibWrapper* aLib, AVCodecID aCodec);

protected:
  enum DecodeResult {
    DECODE_FRAME,
    DECODE_NO_FRAME,
    DECODE_ERROR,
    FATAL_ERROR
  };

  // Flush and Drain operation, always run
  virtual void ProcessFlush();
  virtual void ProcessShutdown();
  virtual void InitCodecContext() {}
  AVFrame*        PrepareFrame();
  nsresult        InitDecoder();

  FFmpegLibWrapper* mLib;
  MediaDataDecoderCallback* mCallback;

  AVCodecContext* mCodecContext;
  AVFrame*        mFrame;
  RefPtr<MediaByteBuffer> mExtraData;
  AVCodecID mCodecID;

private:
  void ProcessDecode(MediaRawData* aSample);
  virtual DecodeResult DoDecode(MediaRawData* aSample) = 0;
  virtual void ProcessDrain() = 0;

  static StaticMutex sMonitor;
  const RefPtr<TaskQueue> mTaskQueue;
  // Set/cleared on reader thread calling Flush() to indicate that output is
  // not required and so input samples on mTaskQueue need not be processed.
  Atomic<bool> mIsFlushing;
};

} // namespace mozilla

#endif // __FFmpegDataDecoder_h__
