/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BUFFER_DECODER_H_
#define BUFFER_DECODER_H_

#include "AbstractMediaDecoder.h"
#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"

namespace mozilla {

/**
 * This class provides a decoder object which decodes a media file that lives in
 * a memory buffer.
 */
class BufferDecoder : public AbstractMediaDecoder
{
public:
  // This class holds a weak pointer to MediaResource.  It's the responsibility
  // of the caller to manage the memory of the MediaResource object.
  explicit BufferDecoder(MediaResource* aResource);

  NS_DECL_THREADSAFE_ISUPPORTS

  // This has to be called before decoding begins
  void BeginDecoding(nsIThread* aDecodeThread);

  virtual ReentrantMonitor& GetReentrantMonitor() MOZ_OVERRIDE;

  virtual bool IsShutdown() const MOZ_OVERRIDE;

  virtual bool OnStateMachineThread() const MOZ_OVERRIDE;

  virtual bool OnDecodeThread() const MOZ_OVERRIDE;

  virtual MediaResource* GetResource() const MOZ_OVERRIDE;

  virtual void NotifyBytesConsumed(int64_t aBytes, int64_t aOffset) MOZ_OVERRIDE;

  virtual void NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded) MOZ_OVERRIDE;

  virtual int64_t GetEndMediaTime() const MOZ_OVERRIDE;

  virtual int64_t GetMediaDuration() MOZ_OVERRIDE;

  virtual void SetMediaDuration(int64_t aDuration) MOZ_OVERRIDE;

  virtual void UpdateEstimatedMediaDuration(int64_t aDuration) MOZ_OVERRIDE;

  virtual void SetMediaSeekable(bool aMediaSeekable) MOZ_OVERRIDE;

  virtual VideoFrameContainer* GetVideoFrameContainer() MOZ_OVERRIDE;
  virtual layers::ImageContainer* GetImageContainer() MOZ_OVERRIDE;

  virtual bool IsTransportSeekable() MOZ_OVERRIDE;

  virtual bool IsMediaSeekable() MOZ_OVERRIDE;

  virtual void MetadataLoaded(MediaInfo* aInfo, MetadataTags* aTags) MOZ_OVERRIDE;
  virtual void QueueMetadata(int64_t aTime, MediaInfo* aInfo, MetadataTags* aTags) MOZ_OVERRIDE;

  virtual void RemoveMediaTracks() MOZ_OVERRIDE;

  virtual void SetMediaEndTime(int64_t aTime) MOZ_OVERRIDE;

  virtual void UpdatePlaybackPosition(int64_t aTime) MOZ_OVERRIDE;

  virtual void OnReadMetadataCompleted() MOZ_OVERRIDE;

  virtual MediaDecoderOwner* GetOwner() MOZ_OVERRIDE;

  virtual void NotifyWaitingForResourcesStatusChanged() MOZ_OVERRIDE;

  virtual void NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset) MOZ_OVERRIDE;

protected:
  virtual ~BufferDecoder();

  // This monitor object is not really used to synchronize access to anything.
  // It's just there in order for us to be able to override
  // GetReentrantMonitor correctly.
  ReentrantMonitor mReentrantMonitor;
  nsCOMPtr<nsIThread> mDecodeThread;
  nsRefPtr<MediaResource> mResource;
};

} // namespace mozilla

#endif /* BUFFER_DECODER_H_ */
