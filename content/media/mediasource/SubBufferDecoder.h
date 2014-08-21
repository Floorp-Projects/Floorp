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

class MediaResource;
class MediaSourceDecoder;
class MediaDecoderReader;

namespace dom {

class TimeRanges;

} // namespace dom

class SubBufferDecoder : public BufferDecoder
{
public:
  // This class holds a weak pointer to MediaResource.  It's the responsibility
  // of the caller to manage the memory of the MediaResource object.
  SubBufferDecoder(MediaResource* aResource, AbstractMediaDecoder* aParentDecoder)
    : BufferDecoder(aResource), mParentDecoder(aParentDecoder), mReader(nullptr)
    , mMediaDuration(-1), mDiscarded(false)
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

  // Warning: these mirror calls from MediaDecoder, but this class's base is
  // AbstractMediaDecoder, which does not supply this interface.
  void NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset);
  nsresult GetBuffered(dom::TimeRanges* aBuffered);

  // Given a time convert it into an approximate byte offset from the
  // cached data. Returns -1 if no such value is computable.
  int64_t ConvertToByteOffset(double aTime);

  int64_t GetMediaDuration() MOZ_OVERRIDE
  {
    return mMediaDuration;
  }

  bool IsDiscarded()
  {
    return mDiscarded;
  }

  void SetDiscarded()
  {
    GetResource()->Ended();
    mDiscarded = true;
  }

  // Returns true if the data buffered by this decoder contains the given time.
  bool ContainsTime(double aTime);

private:
  AbstractMediaDecoder* mParentDecoder;
  nsRefPtr<MediaDecoderReader> mReader;
  int64_t mMediaDuration;
  bool mDiscarded;
};

} // namespace mozilla

#endif /* MOZILLA_SUBBUFFERDECODER_H_ */
