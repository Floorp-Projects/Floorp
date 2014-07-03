/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegAACDecoder_h__
#define __FFmpegAACDecoder_h__

#include "FFmpegDataDecoder.h"
#include "mp4_demuxer/DecoderData.h"

namespace mozilla
{

template <int V> class FFmpegAACDecoder
{
};

template <>
class FFmpegAACDecoder<LIBAV_VER> : public FFmpegDataDecoder<LIBAV_VER>
{
public:
  FFmpegAACDecoder(MediaTaskQueue* aTaskQueue,
                   MediaDataDecoderCallback* aCallback,
                   const mp4_demuxer::AudioDecoderConfig& aConfig);
  virtual ~FFmpegAACDecoder();

  virtual nsresult Init() MOZ_OVERRIDE;
  virtual nsresult Input(mp4_demuxer::MP4Sample* aSample) MOZ_OVERRIDE;
  virtual nsresult Drain() MOZ_OVERRIDE;

private:
  void DecodePacket(mp4_demuxer::MP4Sample* aSample);

  MediaDataDecoderCallback* mCallback;
};

} // namespace mozilla

#endif // __FFmpegAACDecoder_h__
