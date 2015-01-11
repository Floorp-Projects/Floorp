/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASOURCEREADER_H_
#define MOZILLA_MEDIASOURCEREADER_H_

#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsString.h"
#include "nsTArray.h"
#include "MediaDecoderReader.h"

namespace mozilla {

class MediaSourceDecoder;
class SourceBufferDecoder;
class TrackBuffer;

namespace dom {

class MediaSource;

} // namespace dom

class MediaSourceReader : public MediaDecoderReader
{
public:
  explicit MediaSourceReader(MediaSourceDecoder* aDecoder);

  nsresult Init(MediaDecoderReader* aCloneDonor) MOZ_OVERRIDE
  {
    // Although we technically don't implement anything here, we return NS_OK
    // so that when the state machine initializes and calls this function
    // we don't return an error code back to the media element.
    return NS_OK;
  }

  // Indicates the point in time at which the reader should consider
  // registered TrackBuffers essential for initialization.
  void PrepareInitialization();

  bool IsWaitingMediaResources() MOZ_OVERRIDE;

  nsRefPtr<AudioDataPromise> RequestAudioData() MOZ_OVERRIDE;
  nsRefPtr<VideoDataPromise>
  RequestVideoData(bool aSkipToNextKeyframe, int64_t aTimeThreshold) MOZ_OVERRIDE;

  virtual size_t SizeOfVideoQueueInFrames() MOZ_OVERRIDE;
  virtual size_t SizeOfAudioQueueInFrames() MOZ_OVERRIDE;

  void OnAudioDecoded(AudioData* aSample);
  void OnAudioNotDecoded(NotDecodedReason aReason);
  void OnVideoDecoded(VideoData* aSample);
  void OnVideoNotDecoded(NotDecodedReason aReason);

  void OnSeekCompleted(int64_t aTime);
  void OnSeekFailed(nsresult aResult);

  virtual bool IsWaitForDataSupported() MOZ_OVERRIDE { return true; }
  virtual nsRefPtr<WaitForDataPromise> WaitForData(MediaData::Type aType) MOZ_OVERRIDE;
  void MaybeNotifyHaveData();

  bool HasVideo() MOZ_OVERRIDE
  {
    return mInfo.HasVideo();
  }

  bool HasAudio() MOZ_OVERRIDE
  {
    return mInfo.HasAudio();
  }

  void NotifyTimeRangesChanged();

  // We can't compute a proper start time since we won't necessarily
  // have the first frame of the resource available. This does the same
  // as chrome/blink and assumes that we always start at t=0.
  virtual int64_t ComputeStartTime(const VideoData* aVideo, const AudioData* aAudio) MOZ_OVERRIDE { return 0; }

  // Buffering heuristics don't make sense for MSE, because the arrival of data
  // is at least partly controlled by javascript, and javascript does not expect
  // us to sit on unplayed data just because it may not be enough to play
  // through.
  bool UseBufferingHeuristics() MOZ_OVERRIDE { return false; }

  bool IsMediaSeekable() MOZ_OVERRIDE { return true; }

  nsresult ReadMetadata(MediaInfo* aInfo, MetadataTags** aTags) MOZ_OVERRIDE;
  void ReadUpdatedMetadata(MediaInfo* aInfo) MOZ_OVERRIDE;
  nsRefPtr<SeekPromise>
  Seek(int64_t aTime, int64_t aStartTime, int64_t aEndTime,
       int64_t aCurrentTime) MOZ_OVERRIDE;

  // Acquires the decoder monitor, and is thus callable on any thread.
  nsresult GetBuffered(dom::TimeRanges* aBuffered) MOZ_OVERRIDE;

  already_AddRefed<SourceBufferDecoder> CreateSubDecoder(const nsACString& aType,
                                                         int64_t aTimestampOffset /* microseconds */);

  void AddTrackBuffer(TrackBuffer* aTrackBuffer);
  void RemoveTrackBuffer(TrackBuffer* aTrackBuffer);
  void OnTrackBufferConfigured(TrackBuffer* aTrackBuffer, const MediaInfo& aInfo);

  nsRefPtr<ShutdownPromise> Shutdown() MOZ_OVERRIDE;

  virtual void BreakCycles() MOZ_OVERRIDE;

  bool IsShutdown()
  {
    ReentrantMonitorAutoEnter decoderMon(mDecoder->GetReentrantMonitor());
    return mDecoder->IsShutdown();
  }

  // Return true if all of the active tracks contain data for the specified time.
  bool TrackBuffersContainTime(int64_t aTime);

  // Mark the reader to indicate that EndOfStream has been called on our MediaSource
  void Ended();

  // Return true if the Ended method has been called
  bool IsEnded();

#ifdef MOZ_EME
  nsresult SetCDMProxy(CDMProxy* aProxy);
#endif

private:
  // Switch the current audio/video reader to the reader that
  // contains aTarget (or up to aError after target). Both
  // aTarget and aError are in microseconds.
  bool SwitchAudioReader(int64_t aTarget, int64_t aError = 0);
  bool SwitchVideoReader(int64_t aTarget, int64_t aError = 0);

  // Return a reader from the set available in aTrackDecoders that has data
  // available in the range requested by aTarget.
  already_AddRefed<MediaDecoderReader> SelectReader(int64_t aTarget,
                                                    int64_t aError,
                                                    const nsTArray<nsRefPtr<SourceBufferDecoder>>& aTrackDecoders);
  bool HaveData(int64_t aTarget, MediaData::Type aType);

  void AttemptSeek();
  void FinalizeSeek();

  nsRefPtr<MediaDecoderReader> mAudioReader;
  nsRefPtr<MediaDecoderReader> mVideoReader;

  nsTArray<nsRefPtr<TrackBuffer>> mTrackBuffers;
  nsTArray<nsRefPtr<TrackBuffer>> mShutdownTrackBuffers;
  nsTArray<nsRefPtr<TrackBuffer>> mEssentialTrackBuffers;
  nsRefPtr<TrackBuffer> mAudioTrack;
  nsRefPtr<TrackBuffer> mVideoTrack;

  MediaPromiseHolder<AudioDataPromise> mAudioPromise;
  MediaPromiseHolder<VideoDataPromise> mVideoPromise;

  MediaPromiseHolder<WaitForDataPromise> mAudioWaitPromise;
  MediaPromiseHolder<WaitForDataPromise> mVideoWaitPromise;
  MediaPromiseHolder<WaitForDataPromise>& WaitPromise(MediaData::Type aType)
  {
    return aType == MediaData::AUDIO_DATA ? mAudioWaitPromise : mVideoWaitPromise;
  }

#ifdef MOZ_EME
  nsRefPtr<CDMProxy> mCDMProxy;
#endif

  // These are read and written on the decode task queue threads.
  int64_t mLastAudioTime;
  int64_t mLastVideoTime;

  // Temporary seek information while we wait for the data
  // to be added to the track buffer.
  MediaPromiseHolder<SeekPromise> mSeekPromise;
  int64_t mPendingSeekTime;
  int64_t mPendingStartTime;
  int64_t mPendingEndTime;
  int64_t mPendingCurrentTime;
  bool mWaitingForSeekData;

  // Number of outstanding OnSeekCompleted notifications
  // we're expecting to get from child decoders, and the
  // result we're going to forward onto our callback.
  uint32_t mPendingSeeks;
  nsresult mSeekResult;

  int64_t mTimeThreshold;
  bool mDropAudioBeforeThreshold;
  bool mDropVideoBeforeThreshold;

  bool mEnded;

  // For a seek to complete we need to send a sample with
  // the mDiscontinuity field set to true once we have the
  // first decoded sample. These flags are set during seeking
  // so we can detect when we have the first decoded sample
  // after a seek.
  bool mAudioIsSeeking;
  bool mVideoIsSeeking;

  bool mHasEssentialTrackBuffers;

  void ContinueShutdown();
  MediaPromiseHolder<ShutdownPromise> mMediaSourceShutdownPromise;
#ifdef MOZ_FMP4
  nsRefPtr<SharedDecoderManager> mSharedDecoderManager;
#endif
};

} // namespace mozilla

#endif /* MOZILLA_MEDIASOURCEREADER_H_ */
