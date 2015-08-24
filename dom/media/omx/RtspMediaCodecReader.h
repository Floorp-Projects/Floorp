/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(RtspMediaCodecReader_h_)
#define RtspMediaCodecReader_h_

#include "MediaCodecReader.h"

namespace mozilla {

class AbstractMediaDecoder;
class RtspMediaResource;

/* RtspMediaCodecReader is a subclass of MediaCodecReader.
 * The major reason that RtspMediaCodecReader inherit from MediaCodecReader is
 * the same video/audio decoding logic we can reuse.
 */
class RtspMediaCodecReader final : public MediaCodecReader
{
protected:
  // Provide a Rtsp extractor.
  virtual bool CreateExtractor() override;
  // Ensure the play command is sent to Rtsp server.
  void EnsureActive();

public:
  RtspMediaCodecReader(AbstractMediaDecoder* aDecoder);

  virtual ~RtspMediaCodecReader();

  // Implement a time-based seek instead of byte-based.
  virtual nsRefPtr<SeekPromise>
  Seek(int64_t aTime, int64_t aEndTime) override;

  // Override GetBuffered() to do nothing for below reasons:
  // 1. Because the Rtsp stream is a/v separated. The buffered data in a/v
  // tracks are not consistent with time stamp.
  // For example: audio buffer: 1~2s, video buffer: 1.5~2.5s
  // 2. Since the Rtsp is a realtime streaming, the buffer we made for
  // RtspMediaResource is quite small. The small buffer implies the time ranges
  // we returned are not useful for the MediaDecodeStateMachine. Unlike the
  // ChannelMediaResource, it has a "cache" that can store the whole streaming
  // data so the |GetBuffered| function can retrieve useful time ranges.
  virtual media::TimeIntervals GetBuffered() override {
    return media::TimeIntervals::Invalid();
  }

  virtual void SetIdle() override;

  // Disptach a DecodeVideoFrameTask to decode video data.
  virtual nsRefPtr<VideoDataPromise>
  RequestVideoData(bool aSkipToNextKeyframe,
                   int64_t aTimeThreshold) override;

  // Disptach a DecodeAudioDataTask to decode audio data.
  virtual nsRefPtr<AudioDataPromise> RequestAudioData() override;

  virtual nsRefPtr<MediaDecoderReader::MetadataPromise> AsyncReadMetadata()
    override;

  virtual void HandleResourceAllocated() override;

private:
  // A pointer to RtspMediaResource for calling the Rtsp specific function.
  // The lifetime of mRtspResource is controlled by MediaDecoder. MediaDecoder
  // holds the MediaDecoderStateMachine and RtspMediaResource.
  // And MediaDecoderStateMachine holds this RtspMediaCodecReader.
  RtspMediaResource* mRtspResource;
};

} // namespace mozilla

#endif
