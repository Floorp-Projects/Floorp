/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BUFFER_DECODER_H_
#define BUFFER_DECODER_H_

#include "AbstractMediaDecoder.h"
#include "MediaTaskQueue.h"
#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"

namespace mozilla {

/**
 * This class provides a decoder object which decodes a media file that lives in
 * a memory buffer.
 */
class BufferDecoder final : public AbstractMediaDecoder
{
public:
  // This class holds a weak pointer to MediaResource.  It's the responsibility
  // of the caller to manage the memory of the MediaResource object.
  explicit BufferDecoder(MediaResource* aResource);

  NS_DECL_THREADSAFE_ISUPPORTS

  // This has to be called before decoding begins
  void BeginDecoding(MediaTaskQueue* aTaskQueueIdentity);

  virtual ReentrantMonitor& GetReentrantMonitor() final override;

  virtual bool IsShutdown() const final override;

  virtual bool OnStateMachineTaskQueue() const final override;

  virtual bool OnDecodeTaskQueue() const final override;

  virtual MediaResource* GetResource() const final override;

  virtual void NotifyBytesConsumed(int64_t aBytes, int64_t aOffset) final override;

  virtual void NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded,
                                   uint32_t aDropped) final override;

  virtual int64_t GetMediaDuration() final override;

  virtual void SetMediaDuration(int64_t aDuration) final override;

  virtual void UpdateEstimatedMediaDuration(int64_t aDuration) final override;

  virtual void SetMediaSeekable(bool aMediaSeekable) final override;

  virtual VideoFrameContainer* GetVideoFrameContainer() final override;
  virtual layers::ImageContainer* GetImageContainer() final override;

  virtual bool IsTransportSeekable() final override;

  virtual bool IsMediaSeekable() final override;

  virtual void MetadataLoaded(nsAutoPtr<MediaInfo> aInfo,
                              nsAutoPtr<MetadataTags> aTags,
                              MediaDecoderEventVisibility aEventVisibility) final override;
  virtual void QueueMetadata(int64_t aTime, nsAutoPtr<MediaInfo> aInfo, nsAutoPtr<MetadataTags> aTags) final override;
  virtual void FirstFrameLoaded(nsAutoPtr<MediaInfo> aInfo,
                                MediaDecoderEventVisibility aEventVisibility) final override;

  virtual void RemoveMediaTracks() final override;

  virtual void SetMediaEndTime(int64_t aTime) final override;

  virtual void OnReadMetadataCompleted() final override;

  virtual MediaDecoderOwner* GetOwner() final override;

  virtual void NotifyWaitingForResourcesStatusChanged() final override;

  virtual void NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset) final override;

private:
  virtual ~BufferDecoder();

  // This monitor object is not really used to synchronize access to anything.
  // It's just there in order for us to be able to override
  // GetReentrantMonitor correctly.
  ReentrantMonitor mReentrantMonitor;
  nsRefPtr<MediaTaskQueue> mTaskQueueIdentity;
  nsRefPtr<MediaResource> mResource;
};

} // namespace mozilla

#endif /* BUFFER_DECODER_H_ */
