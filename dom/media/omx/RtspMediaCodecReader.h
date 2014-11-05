/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(RtspMediaCodecReader_h_)
#define RtspMediaCodecReader_h_

#include "MediaCodecReader.h"

namespace mozilla {

namespace dom {
  class TimeRanges;
}

class AbstractMediaDecoder;
class RtspMediaResource;

/* RtspMediaCodecReader is a subclass of MediaCodecReader.
 * The major reason that RtspMediaCodecReader inherit from MediaCodecReader is
 * the same video/audio decoding logic we can reuse.
 */
class RtspMediaCodecReader MOZ_FINAL : public MediaCodecReader
{
protected:
  // Provide a Rtsp extractor.
  virtual bool CreateExtractor() MOZ_OVERRIDE;
  // Ensure the play command is sent to Rtsp server.
  void EnsureActive();

public:
  RtspMediaCodecReader(AbstractMediaDecoder* aDecoder);

  virtual ~RtspMediaCodecReader();

  // Implement a time-based seek instead of byte-based.
  virtual nsresult Seek(int64_t aTime, int64_t aStartTime, int64_t aEndTime,
                        int64_t aCurrentTime) MOZ_OVERRIDE;

  // Override GetBuffered() to do nothing for below reasons:
  // 1. Because the Rtsp stream is a/v separated. The buffered data in a/v
  // tracks are not consistent with time stamp.
  // For example: audio buffer: 1~2s, video buffer: 1.5~2.5s
  // 2. Since the Rtsp is a realtime streaming, the buffer we made for
  // RtspMediaResource is quite small. The small buffer implies the time ranges
  // we returned are not useful for the MediaDecodeStateMachine. Unlike the
  // ChannelMediaResource, it has a "cache" that can store the whole streaming
  // data so the |GetBuffered| function can retrieve useful time ranges.
  virtual nsresult GetBuffered(dom::TimeRanges* aBuffered,
                               int64_t aStartTime) MOZ_OVERRIDE {
    return NS_OK;
  }

  virtual void SetIdle() MOZ_OVERRIDE;

  // Disptach a DecodeVideoFrameTask to decode video data.
  virtual void RequestVideoData(bool aSkipToNextKeyframe,
                                int64_t aTimeThreshold) MOZ_OVERRIDE;

  // Disptach a DecodeAudioDataTask to decode audio data.
  virtual void RequestAudioData() MOZ_OVERRIDE;

  virtual nsresult ReadMetadata(MediaInfo* aInfo,
                                MetadataTags** aTags) MOZ_OVERRIDE;

private:
  // A pointer to RtspMediaResource for calling the Rtsp specific function.
  // The lifetime of mRtspResource is controlled by MediaDecoder. MediaDecoder
  // holds the MediaDecoderStateMachine and RtspMediaResource.
  // And MediaDecoderStateMachine holds this RtspMediaCodecReader.
  RtspMediaResource* mRtspResource;
};

} // namespace mozilla

#endif
