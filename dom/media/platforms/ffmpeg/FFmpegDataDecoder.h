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

template <>
class FFmpegDataDecoder<LIBAV_VER> : public MediaDataDecoder
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
  AVFrame*        PrepareFrame();
  nsresult        InitDecoder();

  FFmpegLibWrapper* mLib;

  AVCodecContext* mCodecContext;
  AVFrame*        mFrame;
  RefPtr<MediaByteBuffer> mExtraData;
  AVCodecID mCodecID;

private:
  virtual RefPtr<DecodePromise> ProcessDecode(MediaRawData* aSample) = 0;
  virtual RefPtr<DecodePromise> ProcessDrain() = 0;

  static StaticMutex sMonitor;
  const RefPtr<TaskQueue> mTaskQueue;
  MozPromiseHolder<DecodePromise> mPromise;
};

} // namespace mozilla

#endif // __FFmpegDataDecoder_h__
