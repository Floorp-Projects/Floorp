/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ReaderProxy_h_
#define ReaderProxy_h_

#include "mozilla/AbstractThread.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"

#include "MediaEventSource.h"
#include "MediaFormatReader.h"

namespace mozilla {

/**
 * A wrapper around MediaFormatReader to offset the timestamps of Audio/Video
 * samples by the start time to ensure MDSM can always assume zero start time.
 * It also adjusts the seek target passed to Seek() to ensure correct seek time
 * is passed to the underlying reader.
 */
class ReaderProxy {
  using MetadataPromise = MediaFormatReader::MetadataPromise;
  using AudioDataPromise = MediaFormatReader::AudioDataPromise;
  using VideoDataPromise = MediaFormatReader::VideoDataPromise;
  using SeekPromise = MediaFormatReader::SeekPromise;
  using WaitForDataPromise = MediaFormatReader::WaitForDataPromise;
  using TrackSet = MediaFormatReader::TrackSet;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ReaderProxy);

 public:
  ReaderProxy(AbstractThread* aOwnerThread, MediaFormatReader* aReader);

  media::TimeUnit StartTime() const;
  RefPtr<MetadataPromise> ReadMetadata();

  RefPtr<AudioDataPromise> RequestAudioData();

  RefPtr<VideoDataPromise> RequestVideoData(
      const media::TimeUnit& aTimeThreshold, bool aRequestNextVideoKeyFrame);

  RefPtr<WaitForDataPromise> WaitForData(MediaData::Type aType);

  RefPtr<SeekPromise> Seek(const SeekTarget& aTarget);
  RefPtr<ShutdownPromise> Shutdown();

  void ReleaseResources();
  void ResetDecode(TrackSet aTracks);

  nsresult Init() { return mReader->Init(); }
  bool UseBufferingHeuristics() const {
    return mReader->UseBufferingHeuristics();
  }

  bool VideoIsHardwareAccelerated() const {
    return mReader->VideoIsHardwareAccelerated();
  }
  TimedMetadataEventSource& TimedMetadataEvent() {
    return mReader->TimedMetadataEvent();
  }
  MediaEventSource<void>& OnMediaNotSeekable() {
    return mReader->OnMediaNotSeekable();
  }
  MediaEventProducer<VideoInfo, AudioInfo>& OnTrackInfoUpdatedEvent() {
    return mReader->OnTrackInfoUpdatedEvent();
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

  void UpdateMediaEngineId(uint64_t aMediaEngineId);

 private:
  ~ReaderProxy();
  RefPtr<MetadataPromise> OnMetadataRead(MetadataHolder&& aMetadata);
  RefPtr<MetadataPromise> OnMetadataNotRead(const MediaResult& aError);
  void UpdateDuration();
  RefPtr<SeekPromise> SeekInternal(const SeekTarget& aTarget);

  RefPtr<ReaderProxy::AudioDataPromise> OnAudioDataRequestCompleted(
      RefPtr<AudioData> aAudio);
  RefPtr<ReaderProxy::AudioDataPromise> OnAudioDataRequestFailed(
      const MediaResult& aError);

  const RefPtr<AbstractThread> mOwnerThread;
  const RefPtr<MediaFormatReader> mReader;

  bool mShutdown = false;
  Maybe<media::TimeUnit> mStartTime;

  // State-watching manager.
  WatchManager<ReaderProxy> mWatchManager;

  // Duration, mirrored from the state machine task queue.
  Mirror<media::NullableTimeUnit> mDuration;
};

}  // namespace mozilla

#endif  // ReaderProxy_h_
