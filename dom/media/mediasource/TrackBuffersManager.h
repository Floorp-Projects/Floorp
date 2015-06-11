/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_TRACKBUFFERSMANAGER_H_
#define MOZILLA_TRACKBUFFERSMANAGER_H_

#include "SourceBufferContentManager.h"
#include "MediaSourceDecoder.h"
#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/Monitor.h"
#include "mozilla/Pair.h"
#include "nsProxyRelease.h"
#include "nsTArray.h"

namespace mozilla {

class ContainerParser;
class MediaLargeByteBuffer;
class MediaRawData;
class SourceBuffer;
class SourceBufferResource;

using media::TimeUnit;
using media::TimeInterval;
using media::TimeIntervals;
using dom::SourceBufferAppendMode;

class TrackBuffersManager : public SourceBufferContentManager {
public:
  typedef MediaPromise<bool, nsresult, /* IsExclusive = */ true> CodedFrameProcessingPromise;
  typedef TrackInfo::TrackType TrackType;
  typedef MediaData::Type MediaType;
  typedef nsTArray<nsRefPtr<MediaRawData>> TrackBuffer;

  TrackBuffersManager(dom::SourceBuffer* aParent, MediaSourceDecoder* aParentDecoder, const nsACString& aType);

  bool AppendData(MediaLargeByteBuffer* aData, TimeUnit aTimestampOffset) override;

  nsRefPtr<AppendPromise> BufferAppend() override;

  void AbortAppendData() override;

  void ResetParserState() override;

  nsRefPtr<RangeRemovalPromise> RangeRemoval(TimeUnit aStart, TimeUnit aEnd) override;

  EvictDataResult
  EvictData(TimeUnit aPlaybackTime, uint32_t aThreshold, TimeUnit* aBufferStartTime) override;

  void EvictBefore(TimeUnit aTime) override;

  TimeIntervals Buffered() override;

  int64_t GetSize() override;

  void Ended() override;

  void Detach() override;

  AppendState GetAppendState() override
  {
    return mAppendState;
  }

  void SetGroupStartTimestamp(const TimeUnit& aGroupStartTimestamp) override;
  void RestartGroupStartTimestamp() override;

  // Interface for MediaSourceDemuxer
  MediaInfo GetMetadata();
  const TrackBuffer& GetTrackBuffer(TrackInfo::TrackType aTrack);
  const TimeIntervals& Buffered(TrackInfo::TrackType);
  bool IsEnded() const
  {
    return mEnded;
  }

#if defined(DEBUG)
  void Dump(const char* aPath) override;
#endif

private:
  virtual ~TrackBuffersManager();
  void InitSegmentParserLoop();
  void ScheduleSegmentParserLoop();
  void SegmentParserLoop();
  void AppendIncomingBuffers();
  void InitializationSegmentReceived();
  void CreateDemuxerforMIMEType();
  void NeedMoreData();
  void RejectAppend(nsresult aRejectValue, const char* aName);
  // Will return a promise that will be resolved once all frames of the current
  // media segment have been processed.
  nsRefPtr<CodedFrameProcessingPromise> CodedFrameProcessing();
  // Called by ResetParserState. Complete parsing the input buffer for the
  // current media segment
  void FinishCodedFrameProcessing();
  void CompleteCodedFrameProcessing();
  void CompleteResetParserState();
  void CodedFrameRemoval(TimeInterval aInterval);
  void SetAppendState(AppendState aAppendState);

  bool HasVideo() const
  {
    return mVideoTracks.mNumTracks > 0;
  }
  bool HasAudio() const
  {
    return mAudioTracks.mNumTracks > 0;
  }

  // The input buffer as per http://w3c.github.io/media-source/index.html#sourcebuffer-input-buffer
  nsRefPtr<MediaLargeByteBuffer> mInputBuffer;
  // The current append state as per https://w3c.github.io/media-source/#sourcebuffer-append-state
  // Accessed on both the main thread and the task queue.
  Atomic<AppendState> mAppendState;
  // Buffer full flag as per https://w3c.github.io/media-source/#sourcebuffer-buffer-full-flag.
  // Accessed on both the main thread and the task queue.
  // TODO: Unused for now.
  Atomic<bool> mBufferFull;
  bool mFirstInitializationSegmentReceived;
  bool mActiveTrack;
  Maybe<TimeUnit> mGroupStartTimestamp;
  TimeUnit mGroupEndTimestamp;
  nsCString mType;

  // ContainerParser objects and methods.
  // Those are used to parse the incoming input buffer.

  // Recreate the ContainerParser and only feed it with the previous init
  // segment found.
  void RecreateParser();
  nsAutoPtr<ContainerParser> mParser;

  // Demuxer objects and methods.
  nsRefPtr<SourceBufferResource> mCurrentInputBuffer;
  nsRefPtr<MediaDataDemuxer> mInputDemuxer;
  void OnDemuxerInitDone(nsresult);
  void OnDemuxerInitFailed(DemuxerFailureReason aFailure);
  MediaPromiseRequestHolder<MediaDataDemuxer::InitPromise> mDemuxerInitRequest;
  bool mEncrypted;

  void OnDemuxFailed(TrackType aTrack, DemuxerFailureReason aFailure);
  void DoDemuxVideo();
  void OnVideoDemuxCompleted(nsRefPtr<MediaTrackDemuxer::SamplesHolder> aSamples);
  void OnVideoDemuxFailed(DemuxerFailureReason aFailure)
  {
    mVideoTracks.mDemuxRequest.Complete();
    OnDemuxFailed(TrackType::kVideoTrack, aFailure);
  }
  void DoDemuxAudio();
  void OnAudioDemuxCompleted(nsRefPtr<MediaTrackDemuxer::SamplesHolder> aSamples);
  void OnAudioDemuxFailed(DemuxerFailureReason aFailure)
  {
    mAudioTracks.mDemuxRequest.Complete();
    OnDemuxFailed(TrackType::kAudioTrack, aFailure);
  }

  void DoEvictData(const TimeUnit& aPlaybackTime, uint32_t aThreshold);

  struct TrackData {
    TrackData()
      : mNumTracks(0)
      , mNeedRandomAccessPoint(true)
      , mSizeBuffer(0)
    {}
    uint32_t mNumTracks;
    Maybe<TimeUnit> mLastDecodeTimestamp;
    Maybe<TimeUnit> mLastFrameDuration;
    Maybe<TimeUnit> mHighestEndTimestamp;
    // Longest frame duration seen in a coded frame group.
    Maybe<TimeUnit> mLongestFrameDuration;
    bool mNeedRandomAccessPoint;
    nsRefPtr<MediaTrackDemuxer> mDemuxer;
    TrackBuffer mQueuedSamples;
    MediaPromiseRequestHolder<MediaTrackDemuxer::SamplesPromise> mDemuxRequest;
    UniquePtr<TrackInfo> mInfo;
    // We only manage a single track of each type at this time.
    nsTArray<TrackBuffer> mBuffers;
    TimeIntervals mBufferedRanges;
    uint32_t mSizeBuffer;
  };
  bool ProcessFrame(MediaRawData* aSample, TrackData& aTrackData);
  MediaPromiseRequestHolder<CodedFrameProcessingPromise> mProcessingRequest;
  MediaPromiseHolder<CodedFrameProcessingPromise> mProcessingPromise;

  // SourceBuffer media promise (resolved on the main thread)
  MediaPromiseHolder<AppendPromise> mAppendPromise;
  MediaPromiseHolder<RangeRemovalPromise> mRangeRemovalPromise;

  // Trackbuffers definition.
  nsTArray<TrackData*> GetTracksList();
  TrackData& GetTracksData(TrackType aTrack)
  {
    switch(aTrack) {
      case TrackType::kVideoTrack:
        return mVideoTracks;
      case TrackType::kAudioTrack:
      default:
        return mAudioTracks;
    }
  }
  TrackData mVideoTracks;
  TrackData mAudioTracks;

  // TaskQueue methods and objects.
  AbstractThread* GetTaskQueue() {
    return mTaskQueue;
  }
  bool OnTaskQueue()
  {
    return !GetTaskQueue() || GetTaskQueue()->IsCurrentThreadIn();
  }
  RefPtr<MediaTaskQueue> mTaskQueue;

  TimeUnit mTimestampOffset;
  TimeUnit mLastTimestampOffset;
  void RestoreCachedVariables();

  // Strong references to external objects.
  nsMainThreadPtrHandle<dom::SourceBuffer> mParent;
  nsMainThreadPtrHandle<MediaSourceDecoder> mParentDecoder;

  // Set to true if abort is called.
  Atomic<bool> mAbort;
  // Set to true if mediasource state changed to ended.
  Atomic<bool> mEnded;

  // Global size of this source buffer content.
  Atomic<int64_t> mSizeSourceBuffer;

  // Monitor to protect following objects accessed across multipple threads.
  mutable Monitor mMonitor;
  // Set by the main thread, but only when all our tasks are completes
  // (e.g. when SourceBuffer.updating is false). So the monitor isn't
  // technically required for mIncomingBuffer.
  typedef Pair<nsRefPtr<MediaLargeByteBuffer>, TimeUnit> IncomingBuffer;
  nsTArray<IncomingBuffer> mIncomingBuffers;
  TimeIntervals mVideoBufferedRanges;
  TimeIntervals mAudioBufferedRanges;
  MediaInfo mInfo;
};

} // namespace mozilla
#endif /* MOZILLA_TRACKBUFFERSMANAGER_H_ */
