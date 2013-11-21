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
  {
  }

  void SetReader(MediaDecoderReader* aReader)
  {
    MOZ_ASSERT(!mReader);
    mReader = aReader;
  }

  virtual ReentrantMonitor& GetReentrantMonitor() MOZ_OVERRIDE;
  virtual bool OnStateMachineThread() const MOZ_OVERRIDE;
  virtual bool OnDecodeThread() const MOZ_OVERRIDE;
  virtual SourceBufferResource* GetResource() const MOZ_OVERRIDE;
  virtual void SetMediaDuration(int64_t aDuration) MOZ_OVERRIDE;
  virtual void UpdateEstimatedMediaDuration(int64_t aDuration) MOZ_OVERRIDE;
  virtual void SetMediaSeekable(bool aMediaSeekable) MOZ_OVERRIDE;
  virtual void SetTransportSeekable(bool aTransportSeekable) MOZ_OVERRIDE;
  virtual layers::ImageContainer* GetImageContainer() MOZ_OVERRIDE;

  void NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset)
  {
    mReader->NotifyDataArrived(aBuffer, aLength, aOffset);

    // XXX: aOffset makes no sense here, need view of "data timeline".
    mParentDecoder->NotifyDataArrived(aBuffer, aLength, aOffset);
  }

  nsresult GetBuffered(dom::TimeRanges* aBuffered)
  {
    // XXX: Need mStartTime (from StateMachine) instead of passing 0.
    return mReader->GetBuffered(aBuffered, 0);
  }

private:
  MediaSourceDecoder* mParentDecoder;
  nsAutoPtr<MediaDecoderReader> mReader;
};

} // namespace mozilla

#endif /* MOZILLA_SUBBUFFERDECODER_H_ */
