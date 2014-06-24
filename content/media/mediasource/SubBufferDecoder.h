/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SUBBUFFERDECODER_H_
#define MOZILLA_SUBBUFFERDECODER_H_

#include "BufferDecoder.h"
#include "SourceBufferResource.h"

namespace mozilla {

class MediaSourceDecoder;

class SubBufferDecoder : public BufferDecoder
{
public:
  // This class holds a weak pointer to MediaResource.  It's the responsibility
  // of the caller to manage the memory of the MediaResource object.
  SubBufferDecoder(MediaResource* aResource, MediaSourceDecoder* aParentDecoder)
    : BufferDecoder(aResource), mParentDecoder(aParentDecoder), mReader(nullptr)
    , mMediaDuration(-1), mMediaStartTime(0)
  {
  }

  void SetReader(MediaDecoderReader* aReader)
  {
    MOZ_ASSERT(!mReader);
    mReader = aReader;
  }

  MediaDecoderReader* GetReader()
  {
    return mReader;
  }

  virtual ReentrantMonitor& GetReentrantMonitor() MOZ_OVERRIDE;
  virtual bool OnStateMachineThread() const MOZ_OVERRIDE;
  virtual bool OnDecodeThread() const MOZ_OVERRIDE;
  virtual SourceBufferResource* GetResource() const MOZ_OVERRIDE;
  virtual void NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded) MOZ_OVERRIDE;
  virtual void SetMediaDuration(int64_t aDuration) MOZ_OVERRIDE;
  virtual void UpdateEstimatedMediaDuration(int64_t aDuration) MOZ_OVERRIDE;
  virtual void SetMediaSeekable(bool aMediaSeekable) MOZ_OVERRIDE;
  virtual layers::ImageContainer* GetImageContainer() MOZ_OVERRIDE;
  virtual MediaDecoderOwner* GetOwner() MOZ_OVERRIDE;

  void NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset)
  {
    mReader->NotifyDataArrived(aBuffer, aLength, aOffset);

    // XXX: Params make no sense to parent decoder as it relates to a
    // specific SubBufferDecoder's data stream.  Pass bogus values here to
    // force parent decoder's state machine to recompute end time for
    // infinite length media.
    mParentDecoder->NotifyDataArrived(nullptr, 0, 0);
  }

  nsresult GetBuffered(dom::TimeRanges* aBuffered)
  {
    // XXX: Need mStartTime (from StateMachine) instead of passing 0.
    return mReader->GetBuffered(aBuffered, 0);
  }

  // Given a time convert it into an approximate byte offset from the
  // cached data. Returns -1 if no such value is computable.
  int64_t ConvertToByteOffset(double aTime);

  int64_t GetMediaDuration() MOZ_OVERRIDE
  {
    return mMediaDuration;
  }

  int64_t GetMediaStartTime()
  {
    return mMediaStartTime;
  }

  void SetMediaStartTime(int64_t aMediaStartTime)
  {
    mMediaStartTime = aMediaStartTime;
  }

private:
  MediaSourceDecoder* mParentDecoder;
  nsRefPtr<MediaDecoderReader> mReader;
  int64_t mMediaDuration;
  int64_t mMediaStartTime;
};

} // namespace mozilla

#endif /* MOZILLA_SUBBUFFERDECODER_H_ */
