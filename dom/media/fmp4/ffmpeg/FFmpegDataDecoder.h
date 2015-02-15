/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegDataDecoder_h__
#define __FFmpegDataDecoder_h__

#include "PlatformDecoderModule.h"
#include "FFmpegLibs.h"
#include "mozilla/StaticMutex.h"
#include "mp4_demuxer/mp4_demuxer.h"

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
  FFmpegDataDecoder(FlushableMediaTaskQueue* aTaskQueue, AVCodecID aCodecID);
  virtual ~FFmpegDataDecoder();

  static bool Link();

  virtual nsresult Init() MOZ_OVERRIDE;
  virtual nsresult Input(mp4_demuxer::MP4Sample* aSample) MOZ_OVERRIDE = 0;
  virtual nsresult Flush() MOZ_OVERRIDE;
  virtual nsresult Drain() MOZ_OVERRIDE = 0;
  virtual nsresult Shutdown() MOZ_OVERRIDE;

protected:
  AVFrame*        PrepareFrame();

  FlushableMediaTaskQueue* mTaskQueue;
  AVCodecContext* mCodecContext;
  AVFrame*        mFrame;
  nsRefPtr<mp4_demuxer::ByteBuffer> mExtraData;

private:
  static bool sFFmpegInitDone;
  static StaticMutex sMonitor;

  AVCodecID mCodecID;
};

} // namespace mozilla

#endif // __FFmpegDataDecoder_h__
