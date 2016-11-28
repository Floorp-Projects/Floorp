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

typedef MozPromise<bool, bool, /* isExclusive = */ false> HaveStartTimePromise;

typedef Variant<MediaData*, MediaResult> AudioCallbackData;
typedef Variant<Tuple<MediaData*, TimeStamp>, MediaResult> VideoCallbackData;
typedef Variant<MediaData::Type, WaitForDataRejectValue> WaitCallbackData;

/**
 * A wrapper around MediaDecoderReader to offset the timestamps of Audio/Video
 * samples by the start time to ensure MDSM can always assume zero start time.
 * It also adjusts the seek target passed to Seek() to ensure correct seek time
 * is passed to the underlying reader.
 */
class MediaDecoderReaderWrapper {
  typedef MediaDecoderReader::MetadataPromise MetadataPromise;
  typedef MediaDecoderReader::MediaDataPromise MediaDataPromise;
  typedef MediaDecoderReader::SeekPromise SeekPromise;
  typedef MediaDecoderReader::WaitForDataPromise WaitForDataPromise;
  typedef MediaDecoderReader::BufferedUpdatePromise BufferedUpdatePromise;
  typedef MediaDecoderReader::TrackSet TrackSet;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDecoderReaderWrapper);

private:
  MediaCallbackExc<AudioCallbackData> mAudioCallback;
  MediaCallbackExc<VideoCallbackData> mVideoCallback;
  MediaCallbackExc<WaitCallbackData> mAudioWaitCallback;
  MediaCallbackExc<WaitCallbackData> mVideoWaitCallback;

public:
  MediaDecoderReaderWrapper(AbstractThread* aOwnerThread,
                            MediaDecoderReader* aReader);

  media::TimeUnit StartTime() const;
  RefPtr<MetadataPromise> ReadMetadata();

  decltype(mAudioCallback)& AudioCallback() { return mAudioCallback; }
  decltype(mVideoCallback)& VideoCallback() { return mVideoCallback; }
  decltype(mAudioWaitCallback)& AudioWaitCallback() { return mAudioWaitCallback; }
  decltype(mVideoWaitCallback)& VideoWaitCallback() { return mVideoWaitCallback; }

  // NOTE: please set callbacks before requesting audio/video data!
  void RequestAudioData();
  void RequestVideoData(bool aSkipToNextKeyframe, media::TimeUnit aTimeThreshold);

  // NOTE: please set callbacks before invoking WaitForData()!
  void WaitForData(MediaData::Type aType);

  bool IsRequestingAudioData() const;
  bool IsRequestingVideoData() const;
  bool IsWaitingAudioData() const;
  bool IsWaitingVideoData() const;

  RefPtr<SeekPromise> Seek(SeekTarget aTarget, media::TimeUnit aEndTime);
  RefPtr<BufferedUpdatePromise> UpdateBufferedWithPromise();
  RefPtr<ShutdownPromise> Shutdown();

  void ReleaseResources();
  void ResetDecode(TrackSet aTracks);

  nsresult Init() { return mReader->Init(); }
  bool IsWaitForDataSupported() const { return mReader->IsWaitForDataSupported(); }
  bool IsAsync() const { return mReader->IsAsync(); }
  bool UseBufferingHeuristics() const { return mReader->UseBufferingHeuristics(); }
  bool ForceZeroStartTime() const { return mReader->ForceZeroStartTime(); }

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

private:
  ~MediaDecoderReaderWrapper();

  void OnMetadataRead(MetadataHolder* aMetadata);
  void OnMetadataNotRead() {}
  MediaCallbackExc<WaitCallbackData>& WaitCallbackRef(MediaData::Type aType);
  MozPromiseRequestHolder<WaitForDataPromise>& WaitRequestRef(MediaData::Type aType);

  const RefPtr<AbstractThread> mOwnerThread;
  const RefPtr<MediaDecoderReader> mReader;

  bool mShutdown = false;
  Maybe<media::TimeUnit> mStartTime;

  MozPromiseRequestHolder<MediaDataPromise> mAudioDataRequest;
  MozPromiseRequestHolder<MediaDataPromise> mVideoDataRequest;
  MozPromiseRequestHolder<WaitForDataPromise> mAudioWaitRequest;
  MozPromiseRequestHolder<WaitForDataPromise> mVideoWaitRequest;
};

} // namespace mozilla

#endif // MediaDecoderReaderWrapper_h_
