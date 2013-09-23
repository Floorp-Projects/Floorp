/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(RtspOmxReader_h_)
#define RtspOmxReader_h_

#include "MediaResource.h"
#include "MediaDecoderReader.h"
#include "MediaOmxReader.h"

namespace mozilla {

namespace dom {
  class TimeRanges;
}

class AbstractMediaDecoder;
class RtspMediaResource;

/* RtspOmxReader is a subclass of MediaOmxReader.
 * The major reason that RtspOmxReader inherit from MediaOmxReader is the
 * same video/audio decoding logic we can reuse.
 */
class RtspOmxReader : public MediaOmxReader
{
protected:
  // Provide a Rtsp extractor.
  nsresult InitOmxDecoder() MOZ_FINAL MOZ_OVERRIDE;

public:
  RtspOmxReader(AbstractMediaDecoder* aDecoder)
    : MediaOmxReader(aDecoder) {
    MOZ_COUNT_CTOR(RtspOmxReader);
    NS_ASSERTION(mDecoder, "RtspOmxReader mDecoder is null.");
    NS_ASSERTION(mDecoder->GetResource(),
                 "RtspOmxReader mDecoder->GetResource() is null.");
    mRtspResource = mDecoder->GetResource()->GetRtspPointer();
    MOZ_ASSERT(mRtspResource);
  }

  virtual ~RtspOmxReader() MOZ_OVERRIDE {
    MOZ_COUNT_DTOR(RtspOmxReader);
  }

  // Implement a time-based seek instead of byte-based..
  virtual nsresult Seek(int64_t aTime, int64_t aStartTime, int64_t aEndTime,
                        int64_t aCurrentTime) MOZ_FINAL MOZ_OVERRIDE;

  // Override GetBuffered() to do nothing for below reasons:
  // 1. Because the Rtsp stream is a/v separated. The buffered data in a/v
  // tracks are not consistent with time stamp.
  // For example: audio buffer: 1~2s, video buffer: 1.5~2.5s
  // 2. Since the Rtsp is a realtime streaming, the buffer we made for
  // RtspMediaResource is quite small. The small buffer implies the time ranges
  // we returned are not useful for the MediaDecodeStateMachine. Unlike the
  // ChannelMediaResource, it has a "cache" that can store the whole streaming
  // data so the |GetBuffered| function can retrieve useful time ranges.
  virtual nsresult GetBuffered(mozilla::dom::TimeRanges* aBuffered,
                               int64_t aStartTime) MOZ_FINAL MOZ_OVERRIDE {
    return NS_OK;
  }

private:
  // A pointer to RtspMediaResource for calling the Rtsp specific function.
  // The lifetime of mRtspResource is controlled by MediaDecoder. MediaDecoder
  // holds the MediaDecoderStateMachine and RtspMediaResource.
  // And MediaDecoderStateMachine holds this RtspOmxReader.
  RtspMediaResource* mRtspResource;
};

} // namespace mozilla

#endif
