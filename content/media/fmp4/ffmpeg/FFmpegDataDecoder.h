/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegDataDecoder_h__
#define __FFmpegDataDecoder_h__

#include "FFmpegDecoderModule.h"
#include "FFmpegRuntimeLinker.h"
#include "FFmpegCompat.h"
#include "mozilla/Vector.h"

namespace mozilla
{

class FFmpegDataDecoder : public MediaDataDecoder
{
public:
  FFmpegDataDecoder(MediaTaskQueue* aTaskQueue, AVCodecID aCodecID);
  virtual ~FFmpegDataDecoder();

  static bool Link();

  virtual nsresult Init() MOZ_OVERRIDE;
  virtual nsresult Input(mp4_demuxer::MP4Sample* aSample) = 0;
  virtual nsresult Flush() MOZ_OVERRIDE;
  virtual nsresult Drain() = 0;
  virtual nsresult Shutdown() MOZ_OVERRIDE;

protected:
  MediaTaskQueue* mTaskQueue;
  AVCodecContext mCodecContext;
  Vector<uint8_t> mExtraData;

private:
  static bool sFFmpegInitDone;

  AVCodecID mCodecID;
};

} // namespace mozilla

#endif // __FFmpegDataDecoder_h__
