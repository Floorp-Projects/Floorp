/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SOURCEBUFFERDECODER_H_
#define MOZILLA_SOURCEBUFFERDECODER_H_

#include "AbstractMediaDecoder.h"
#include "MediaDecoderReader.h"
#include "SourceBufferResource.h"
#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"

namespace mozilla {

class MediaResource;
class MediaDecoderReader;

namespace dom {

class TimeRanges;

} // namespace dom

class SourceBufferDecoder : public AbstractMediaDecoder
{
public:
  // This class holds a weak pointer to MediaResource.  It's the responsibility
  // of the caller to manage the memory of the MediaResource object.
  SourceBufferDecoder(MediaResource* aResource, AbstractMediaDecoder* aParentDecoder);

  NS_DECL_THREADSAFE_ISUPPORTS

  virtual bool IsMediaSeekable() MOZ_FINAL MOZ_OVERRIDE;
  virtual bool IsShutdown() const MOZ_FINAL MOZ_OVERRIDE;
  virtual bool IsTransportSeekable() MOZ_FINAL MOZ_OVERRIDE;
  virtual bool OnDecodeThread() const MOZ_FINAL MOZ_OVERRIDE;
  virtual bool OnStateMachineThread() const MOZ_FINAL MOZ_OVERRIDE;
  virtual int64_t GetMediaDuration() MOZ_FINAL MOZ_OVERRIDE;
  virtual layers::ImageContainer* GetImageContainer() MOZ_FINAL MOZ_OVERRIDE;
  virtual MediaDecoderOwner* GetOwner() MOZ_FINAL MOZ_OVERRIDE;
  virtual SourceBufferResource* GetResource() const MOZ_FINAL MOZ_OVERRIDE;
  virtual ReentrantMonitor& GetReentrantMonitor() MOZ_FINAL MOZ_OVERRIDE;
  virtual VideoFrameContainer* GetVideoFrameContainer() MOZ_FINAL MOZ_OVERRIDE;
  virtual void MetadataLoaded(MediaInfo* aInfo, MetadataTags* aTags) MOZ_FINAL MOZ_OVERRIDE;
  virtual void NotifyBytesConsumed(int64_t aBytes, int64_t aOffset) MOZ_FINAL MOZ_OVERRIDE;
  virtual void NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset) MOZ_FINAL MOZ_OVERRIDE;
  virtual void NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded) MOZ_FINAL MOZ_OVERRIDE;
  virtual void NotifyWaitingForResourcesStatusChanged() MOZ_FINAL MOZ_OVERRIDE;
  virtual void OnReadMetadataCompleted() MOZ_FINAL MOZ_OVERRIDE;
  virtual void QueueMetadata(int64_t aTime, MediaInfo* aInfo, MetadataTags* aTags) MOZ_FINAL MOZ_OVERRIDE;
  virtual void RemoveMediaTracks() MOZ_FINAL MOZ_OVERRIDE;
  virtual void SetMediaDuration(int64_t aDuration) MOZ_FINAL MOZ_OVERRIDE;
  virtual void SetMediaEndTime(int64_t aTime) MOZ_FINAL MOZ_OVERRIDE;
  virtual void SetMediaSeekable(bool aMediaSeekable) MOZ_FINAL MOZ_OVERRIDE;
  virtual void UpdateEstimatedMediaDuration(int64_t aDuration) MOZ_FINAL MOZ_OVERRIDE;
  virtual void UpdatePlaybackPosition(int64_t aTime) MOZ_FINAL MOZ_OVERRIDE;

  // SourceBufferResource specific interface below.

  // Warning: this mirrors GetBuffered in MediaDecoder, but this class's base is
  // AbstractMediaDecoder, which does not supply this interface.
  nsresult GetBuffered(dom::TimeRanges* aBuffered);

  void SetReader(MediaDecoderReader* aReader)
  {
    MOZ_ASSERT(!mReader);
    mReader = aReader;
  }

  MediaDecoderReader* GetReader()
  {
    return mReader;
  }

  void SetTaskQueue(MediaTaskQueue* aTaskQueue)
  {
    MOZ_ASSERT((!mTaskQueue && aTaskQueue) || (mTaskQueue && !aTaskQueue));
    mTaskQueue = aTaskQueue;
  }

  // Given a time convert it into an approximate byte offset from the
  // cached data. Returns -1 if no such value is computable.
  int64_t ConvertToByteOffset(double aTime);

  // Returns true if the data buffered by this decoder contains the given time.
  bool ContainsTime(double aTime);

private:
  virtual ~SourceBufferDecoder();

  // Our TrackBuffer's task queue, this is only non-null during initialization.
  RefPtr<MediaTaskQueue> mTaskQueue;

  nsRefPtr<MediaResource> mResource;

  AbstractMediaDecoder* mParentDecoder;
  nsRefPtr<MediaDecoderReader> mReader;
  int64_t mMediaDuration;
};

} // namespace mozilla

#endif /* MOZILLA_SOURCEBUFFERDECODER_H_ */
