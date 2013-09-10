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
  // This class holds a weak pointer to MediaResouce.  It's the responsibility
  // of the caller to manage the memory of the MediaResource object.
  explicit BufferDecoder(MediaResource* aResource);
  virtual ~BufferDecoder();

  NS_DECL_THREADSAFE_ISUPPORTS

  // This has to be called before decoding begins
  void BeginDecoding(nsIThread* aDecodeThread);

  ReentrantMonitor& GetReentrantMonitor() MOZ_OVERRIDE;

  bool IsShutdown() const MOZ_FINAL MOZ_OVERRIDE;

  bool OnStateMachineThread() const MOZ_OVERRIDE;

  bool OnDecodeThread() const MOZ_OVERRIDE;

  MediaResource* GetResource() const MOZ_FINAL MOZ_OVERRIDE;

  void NotifyBytesConsumed(int64_t aBytes) MOZ_FINAL MOZ_OVERRIDE;

  void NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded) MOZ_FINAL MOZ_OVERRIDE;

  int64_t GetEndMediaTime() const MOZ_FINAL MOZ_OVERRIDE;

  int64_t GetMediaDuration() MOZ_FINAL MOZ_OVERRIDE;

  void SetMediaDuration(int64_t aDuration) MOZ_OVERRIDE;

  void UpdateEstimatedMediaDuration(int64_t aDuration) MOZ_OVERRIDE;

  void SetMediaSeekable(bool aMediaSeekable) MOZ_OVERRIDE;

  void SetTransportSeekable(bool aTransportSeekable) MOZ_OVERRIDE;

  VideoFrameContainer* GetVideoFrameContainer() MOZ_FINAL MOZ_OVERRIDE;
  layers::ImageContainer* GetImageContainer() MOZ_OVERRIDE;

  bool IsTransportSeekable() MOZ_FINAL MOZ_OVERRIDE;

  bool IsMediaSeekable() MOZ_FINAL MOZ_OVERRIDE;

  void MetadataLoaded(int aChannels, int aRate, bool aHasAudio, bool aHasVideo, MetadataTags* aTags) MOZ_FINAL MOZ_OVERRIDE;
  void QueueMetadata(int64_t aTime, int aChannels, int aRate, bool aHasAudio, bool aHasVideo, MetadataTags* aTags) MOZ_FINAL MOZ_OVERRIDE;

  void SetMediaEndTime(int64_t aTime) MOZ_FINAL MOZ_OVERRIDE;

  void UpdatePlaybackPosition(int64_t aTime) MOZ_FINAL MOZ_OVERRIDE;

  void OnReadMetadataCompleted() MOZ_FINAL MOZ_OVERRIDE;

  MediaDecoderOwner* GetOwner() MOZ_FINAL MOZ_OVERRIDE;

private:
  // This monitor object is not really used to synchronize access to anything.
  // It's just there in order for us to be able to override
  // GetReentrantMonitor correctly.
  ReentrantMonitor mReentrantMonitor;
  nsCOMPtr<nsIThread> mDecodeThread;
  nsRefPtr<MediaResource> mResource;
};

} // namespace mozilla

#endif /* BUFFER_DECODER_H_ */
