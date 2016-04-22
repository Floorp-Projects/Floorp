/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaDecoderReaderWrapper_h_
#define MediaDecoderReaderWrapper_h_

#include "mozilla/AbstractThread.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"

#include "MediaDecoderReader.h"

namespace mozilla {

class StartTimeRendezvous;

typedef MozPromise<bool, bool, /* isExclusive = */ false> HaveStartTimePromise;

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
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDecoderReaderWrapper);

public:
  MediaDecoderReaderWrapper(bool aIsRealTime,
                            AbstractThread* aOwnerThread,
                            MediaDecoderReader* aReader);

  media::TimeUnit StartTime() const;
  RefPtr<MetadataPromise> ReadMetadata();
  RefPtr<HaveStartTimePromise> AwaitStartTime();
  RefPtr<MediaDataPromise> RequestAudioData();
  RefPtr<MediaDataPromise> RequestVideoData(bool aSkipToNextKeyframe,
                                            media::TimeUnit aTimeThreshold);
  RefPtr<SeekPromise> Seek(SeekTarget aTarget, media::TimeUnit aEndTime);
  RefPtr<WaitForDataPromise> WaitForData(MediaData::Type aType);
  RefPtr<BufferedUpdatePromise> UpdateBufferedWithPromise();
  RefPtr<ShutdownPromise> Shutdown();

  void ReleaseMediaResources();
  void SetIdle();
  void ResetDecode();

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

#ifdef MOZ_EME
  void SetCDMProxy(CDMProxy* aProxy) { mReader->SetCDMProxy(aProxy); }
#endif

private:
  ~MediaDecoderReaderWrapper();

  void OnMetadataRead(MetadataHolder* aMetadata);
  void OnMetadataNotRead() {}
  void OnSampleDecoded(MediaData* aSample);
  void OnNotDecoded() {}

  const bool mForceZeroStartTime;
  const RefPtr<AbstractThread> mOwnerThread;
  const RefPtr<MediaDecoderReader> mReader;

  bool mShutdown = false;
  RefPtr<StartTimeRendezvous> mStartTimeRendezvous;
};

} // namespace mozilla

#endif // MediaDecoderReaderWrapper_h_
