/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaDecoderReaderWrapper_h_
#define MediaDecoderReaderWrapper_h_

#include "mozilla/AbstractThread.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Variant.h"
#include "nsISupportsImpl.h"

#include "MediaDecoderReader.h"
#include "MediaEventSource.h"

namespace mozilla {

/**
 * A wrapper around MediaDecoderReader to offset the timestamps of Audio/Video
 * samples by the start time to ensure MDSM can always assume zero start time.
 * It also adjusts the seek target passed to Seek() to ensure correct seek time
 * is passed to the underlying reader.
 */
class MediaDecoderReaderWrapper {
  typedef MediaDecoderReader::MetadataPromise MetadataPromise;
  typedef MediaDecoderReader::AudioDataPromise AudioDataPromise;
  typedef MediaDecoderReader::VideoDataPromise VideoDataPromise;
  typedef MediaDecoderReader::SeekPromise SeekPromise;
  typedef MediaDecoderReader::WaitForDataPromise WaitForDataPromise;
  typedef MediaDecoderReader::TrackSet TrackSet;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDecoderReaderWrapper);

public:
  MediaDecoderReaderWrapper(AbstractThread* aOwnerThread,
                            MediaDecoderReader* aReader);

  media::TimeUnit StartTime() const;
  RefPtr<MetadataPromise> ReadMetadata();

  RefPtr<AudioDataPromise> RequestAudioData();

  RefPtr<VideoDataPromise>
  RequestVideoData(const media::TimeUnit& aTimeThreshold);

  RefPtr<WaitForDataPromise> WaitForData(MediaData::Type aType);

  RefPtr<SeekPromise> Seek(const SeekTarget& aTarget);
  RefPtr<ShutdownPromise> Shutdown();

  void ReleaseResources();
  void ResetDecode(TrackSet aTracks);

  nsresult Init() { return mReader->Init(); }
  bool UseBufferingHeuristics() const { return mReader->UseBufferingHeuristics(); }

  bool VideoIsHardwareAccelerated() const {
    return mReader->VideoIsHardwareAccelerated();
  }
  TimedMetadataEventSource& TimedMetadataEvent() {
    return mReader->TimedMetadataEvent();
  }
  MediaEventSource<void>& OnMediaNotSeekable() {
    return mReader->OnMediaNotSeekable();
  }
  size_t SizeOfVideoQueueInBytes() const {
    return mReader->SizeOfVideoQueueInBytes();
  }
  size_t SizeOfAudioQueueInBytes() const {
    return mReader->SizeOfAudioQueueInBytes();
  }
  size_t SizeOfAudioQueueInFrames() const {
    return mReader->SizeOfAudioQueueInFrames();
  }
  size_t SizeOfVideoQueueInFrames() const {
    return mReader->SizeOfVideoQueueInFrames();
  }
  void ReadUpdatedMetadata(MediaInfo* aInfo) {
    mReader->ReadUpdatedMetadata(aInfo);
  }
  AbstractCanonical<media::TimeIntervals>* CanonicalBuffered() {
    return mReader->CanonicalBuffered();
  }

  void SetCDMProxy(CDMProxy* aProxy) { mReader->SetCDMProxy(aProxy); }

  void SetVideoBlankDecode(bool aIsBlankDecode);

  void SetCanonicalDuration(
    AbstractCanonical<media::NullableTimeUnit>* aCanonical);

private:
  ~MediaDecoderReaderWrapper();
  RefPtr<MetadataPromise> OnMetadataRead(MetadataHolder&& aMetadata);
  RefPtr<MetadataPromise> OnMetadataNotRead(const MediaResult& aError);
  void UpdateDuration();

  const RefPtr<AbstractThread> mOwnerThread;
  const RefPtr<MediaDecoderReader> mReader;

  bool mShutdown = false;
  Maybe<media::TimeUnit> mStartTime;

  // State-watching manager.
  WatchManager<MediaDecoderReaderWrapper> mWatchManager;

  // Duration, mirrored from the state machine task queue.
  Mirror<media::NullableTimeUnit> mDuration;
};

} // namespace mozilla

#endif // MediaDecoderReaderWrapper_h_
