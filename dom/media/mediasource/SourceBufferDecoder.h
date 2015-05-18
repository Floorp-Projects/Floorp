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
#ifdef MOZ_EME
#include "mozilla/CDMProxy.h"
#endif
#include "mozilla/ReentrantMonitor.h"

namespace mozilla {

class MediaResource;
class MediaDecoderReader;

class SourceBufferDecoder final : public AbstractMediaDecoder
{
public:
  // This class holds a weak pointer to MediaResource.  It's the responsibility
  // of the caller to manage the memory of the MediaResource object.
  SourceBufferDecoder(MediaResource* aResource, AbstractMediaDecoder* aParentDecoder,
                      int64_t aTimestampOffset /* microseconds */);

  NS_DECL_THREADSAFE_ISUPPORTS

  virtual bool IsMediaSeekable() final override;
  virtual bool IsShutdown() const final override;
  virtual bool IsTransportSeekable() final override;
  virtual bool OnDecodeTaskQueue() const final override;
  virtual bool OnStateMachineTaskQueue() const final override;
  virtual int64_t GetMediaDuration() final override;
  virtual layers::ImageContainer* GetImageContainer() final override;
  virtual MediaDecoderOwner* GetOwner() final override;
  virtual SourceBufferResource* GetResource() const final override;
  virtual ReentrantMonitor& GetReentrantMonitor() final override;
  virtual VideoFrameContainer* GetVideoFrameContainer() final override;
  virtual void MetadataLoaded(nsAutoPtr<MediaInfo> aInfo,
                              nsAutoPtr<MetadataTags> aTags,
                              MediaDecoderEventVisibility aEventVisibility) final override;
  virtual void FirstFrameLoaded(nsAutoPtr<MediaInfo> aInfo,
                                MediaDecoderEventVisibility aEventVisibility) final override;
  virtual void NotifyBytesConsumed(int64_t aBytes, int64_t aOffset) final override;
  virtual void NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset) final override;
  virtual void NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded, uint32_t aDropped) final override;
  virtual void NotifyWaitingForResourcesStatusChanged() final override;
  virtual void OnReadMetadataCompleted() final override;
  virtual void QueueMetadata(int64_t aTime, nsAutoPtr<MediaInfo> aInfo, nsAutoPtr<MetadataTags> aTags) final override;
  virtual void RemoveMediaTracks() final override;
  virtual void SetMediaDuration(int64_t aDuration) final override;
  virtual void SetMediaEndTime(int64_t aTime) final override;
  virtual void SetMediaSeekable(bool aMediaSeekable) final override;
  virtual void UpdateEstimatedMediaDuration(int64_t aDuration) final override;
  virtual bool HasInitializationData() final override;

  // SourceBufferResource specific interface below.
  int64_t GetTimestampOffset() const { return mTimestampOffset; }
  void SetTimestampOffset(int64_t aOffset)  { mTimestampOffset = aOffset; }

  // Warning: this mirrors GetBuffered in MediaDecoder, but this class's base is
  // AbstractMediaDecoder, which does not supply this interface.
  media::TimeIntervals GetBuffered();

  void SetReader(MediaDecoderReader* aReader)
  {
    MOZ_ASSERT(!mReader);
    mReader = aReader;
  }

  MediaDecoderReader* GetReader() const
  {
    return mReader;
  }

  void SetTaskQueue(MediaTaskQueue* aTaskQueue)
  {
    MOZ_ASSERT((!mTaskQueue && aTaskQueue) || (mTaskQueue && !aTaskQueue));
    mTaskQueue = aTaskQueue;
  }

  void BreakCycles()
  {
    if (mReader) {
      mReader->BreakCycles();
      mReader = nullptr;
    }
    mTaskQueue = nullptr;
#ifdef MOZ_EME
    mCDMProxy = nullptr;
#endif
  }

#ifdef MOZ_EME
  virtual nsresult SetCDMProxy(CDMProxy* aProxy) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
    mCDMProxy = aProxy;
    return NS_OK;
  }

  virtual CDMProxy* GetCDMProxy() override
  {
    MOZ_ASSERT(OnDecodeTaskQueue() || NS_IsMainThread());
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
    return mCDMProxy;
  }
#endif

  // Given a time convert it into an approximate byte offset from the
  // cached data. Returns -1 if no such value is computable.
  int64_t ConvertToByteOffset(double aTime);

  // All durations are in usecs.

  // We can't at this stage, accurately remove coded frames.
  // Trim is a work around that hides data located after a given time by
  // preventing playback beyond the trim point.
  // No data is actually removed.
  // aDuration is were data will be trimmed from.
  void Trim(int64_t aDuration);
  bool WasTrimmed()
  {
    return mTrimmedOffset >= 0;
  }

  // returns the real duration of the resource, including trimmed data.
  void SetRealMediaDuration(int64_t aDuration);
  int64_t GetRealMediaDuration()
  {
    return mRealMediaDuration;
  }

private:
  virtual ~SourceBufferDecoder();

  // Our TrackBuffer's task queue, this is only non-null during initialization.
  RefPtr<MediaTaskQueue> mTaskQueue;

  nsRefPtr<MediaResource> mResource;

  AbstractMediaDecoder* mParentDecoder;
  nsRefPtr<MediaDecoderReader> mReader;
  // in microseconds
  int64_t mTimestampOffset;
  // mMediaDuration contains the apparent buffer duration, excluding trimmed data.
  int64_t mMediaDuration;
  // mRealMediaDuration contains the real buffer duration, including trimmed data.
  int64_t mRealMediaDuration;
  // in seconds
  double mTrimmedOffset;

#ifdef MOZ_EME
  nsRefPtr<CDMProxy> mCDMProxy;
#endif
};

} // namespace mozilla

#endif /* MOZILLA_SOURCEBUFFERDECODER_H_ */
