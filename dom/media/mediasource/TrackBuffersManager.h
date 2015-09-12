/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_TRACKBUFFERSMANAGER_H_
#define MOZILLA_TRACKBUFFERSMANAGER_H_

#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/Monitor.h"
#include "mozilla/Pair.h"
#include "mozilla/dom/SourceBufferBinding.h"

#include "SourceBufferContentManager.h"
#include "MediaDataDemuxer.h"
#include "MediaSourceDecoder.h"
#include "nsProxyRelease.h"
#include "nsTArray.h"

namespace mozilla {

class ContainerParser;
class MediaByteBuffer;
class MediaRawData;
class MediaSourceDemuxer;
class SourceBufferResource;

namespace dom {
  class SourceBufferAttributes;
}

class TrackBuffersManager : public SourceBufferContentManager {
public:
  typedef MozPromise<bool, nsresult, /* IsExclusive = */ true> CodedFrameProcessingPromise;
  typedef TrackInfo::TrackType TrackType;
  typedef MediaData::Type MediaType;
  typedef nsTArray<nsRefPtr<MediaRawData>> TrackBuffer;

  TrackBuffersManager(dom::SourceBufferAttributes* aAttributes,
                      MediaSourceDecoder* aParentDecoder,
                      const nsACString& aType);

  bool AppendData(MediaByteBuffer* aData,
                  media::TimeUnit aTimestampOffset) override;

  nsRefPtr<AppendPromise> BufferAppend() override;

  void AbortAppendData() override;

  void ResetParserState() override;

  nsRefPtr<RangeRemovalPromise> RangeRemoval(media::TimeUnit aStart,
                                             media::TimeUnit aEnd) override;

  EvictDataResult
  EvictData(media::TimeUnit aPlaybackTime,
            uint32_t aThreshold,
            media::TimeUnit* aBufferStartTime) override;

  void EvictBefore(media::TimeUnit aTime) override;

  media::TimeIntervals Buffered() override;

  int64_t GetSize() override;

  void Ended() override;

  void Detach() override;

  AppendState GetAppendState() override
  {
    return mAppendState;
  }

  void SetGroupStartTimestamp(const media::TimeUnit& aGroupStartTimestamp) override;
  void RestartGroupStartTimestamp() override;
  media::TimeUnit GroupEndTimestamp() override;

  // Interface for MediaSourceDemuxer
  MediaInfo GetMetadata();
  const TrackBuffer& GetTrackBuffer(TrackInfo::TrackType aTrack);
  const media::TimeIntervals& Buffered(TrackInfo::TrackType);
  media::TimeIntervals SafeBuffered(TrackInfo::TrackType) const;
  bool IsEnded() const
  {
    return mEnded;
  }
  media::TimeUnit Seek(TrackInfo::TrackType aTrack,
                       const media::TimeUnit& aTime);
  uint32_t SkipToNextRandomAccessPoint(TrackInfo::TrackType aTrack,
                                       const media::TimeUnit& aTimeThreadshold,
                                       bool& aFound);
  already_AddRefed<MediaRawData> GetSample(TrackInfo::TrackType aTrack,
                                           const media::TimeUnit& aFuzz,
                                           bool& aError);
  media::TimeUnit GetNextRandomAccessPoint(TrackInfo::TrackType aTrack);

#if defined(DEBUG)
  void Dump(const char* aPath) override;
#endif

private:
  // for MediaSourceDemuxer::GetMozDebugReaderData
  friend class MediaSourceDemuxer;
  virtual ~TrackBuffersManager();
  // All following functions run on the taskqueue.
  nsRefPtr<AppendPromise> InitSegmentParserLoop();
  void ScheduleSegmentParserLoop();
  void SegmentParserLoop();
  void AppendIncomingBuffers();
  void InitializationSegmentReceived();
  void ShutdownDemuxers();
  void CreateDemuxerforMIMEType();
  void ResetDemuxingState();
  void NeedMoreData();
  void RejectAppend(nsresult aRejectValue, const char* aName);
  // Will return a promise that will be resolved once all frames of the current
  // media segment have been processed.
  nsRefPtr<CodedFrameProcessingPromise> CodedFrameProcessing();
  void CompleteCodedFrameProcessing();
  // Called by ResetParserState. Complete parsing the input buffer for the
  // current media segment.
  void FinishCodedFrameProcessing();
  void CompleteResetParserState();
  nsRefPtr<RangeRemovalPromise>
    CodedFrameRemovalWithPromise(media::TimeInterval aInterval);
  bool CodedFrameRemoval(media::TimeInterval aInterval);
  void SetAppendState(AppendState aAppendState);

  bool HasVideo() const
  {
    return mVideoTracks.mNumTracks > 0;
  }
  bool HasAudio() const
  {
    return mAudioTracks.mNumTracks > 0;
  }

  typedef Pair<nsRefPtr<MediaByteBuffer>, media::TimeUnit> IncomingBuffer;
  void AppendIncomingBuffer(IncomingBuffer aData);
  nsTArray<IncomingBuffer> mIncomingBuffers;

  // The input buffer as per http://w3c.github.io/media-source/index.html#sourcebuffer-input-buffer
  nsRefPtr<MediaByteBuffer> mInputBuffer;
  // The current append state as per https://w3c.github.io/media-source/#sourcebuffer-append-state
  // Accessed on both the main thread and the task queue.
  Atomic<AppendState> mAppendState;
  // Buffer full flag as per https://w3c.github.io/media-source/#sourcebuffer-buffer-full-flag.
  // Accessed on both the main thread and the task queue.
  // TODO: Unused for now.
  Atomic<bool> mBufferFull;
  bool mFirstInitializationSegmentReceived;
  // Set to true once a new segment is started.
  bool mNewMediaSegmentStarted;
  bool mActiveTrack;
  Maybe<media::TimeUnit> mGroupStartTimestamp;
  media::TimeUnit mGroupEndTimestamp;
  nsCString mType;

  // ContainerParser objects and methods.
  // Those are used to parse the incoming input buffer.

  // Recreate the ContainerParser and if aReuseInitData is true then
  // feed it with the previous init segment found.
  void RecreateParser(bool aReuseInitData);
  nsAutoPtr<ContainerParser> mParser;

  // Demuxer objects and methods.
  void AppendDataToCurrentInputBuffer(MediaByteBuffer* aData);
  nsRefPtr<MediaByteBuffer> mInitData;
  // Temporary input buffer to handle partial media segment header.
  // We store the current input buffer content into it should we need to
  // reinitialize the demuxer once we have some samples and a discontinuity is
  // detected.
  nsRefPtr<MediaByteBuffer> mPendingInputBuffer;
  nsRefPtr<SourceBufferResource> mCurrentInputBuffer;
  nsRefPtr<MediaDataDemuxer> mInputDemuxer;
  // Length already processed in current media segment.
  uint32_t mProcessedInput;
  Maybe<media::TimeUnit> mLastParsedEndTime;

  void OnDemuxerInitDone(nsresult);
  void OnDemuxerInitFailed(DemuxerFailureReason aFailure);
  void OnDemuxerResetDone(nsresult);
  MozPromiseRequestHolder<MediaDataDemuxer::InitPromise> mDemuxerInitRequest;
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

  void DoEvictData(const media::TimeUnit& aPlaybackTime, uint32_t aThreshold);

  struct TrackData {
    TrackData()
      : mNumTracks(0)
      , mNeedRandomAccessPoint(true)
      , mSizeBuffer(0)
    {}
    uint32_t mNumTracks;
    // Definition of variables:
    // https://w3c.github.io/media-source/#track-buffers
    // Last decode timestamp variable that stores the decode timestamp of the
    // last coded frame appended in the current coded frame group.
    // The variable is initially unset to indicate that no coded frames have
    // been appended yet.
    Maybe<media::TimeUnit> mLastDecodeTimestamp;
    // Last frame duration variable that stores the coded frame duration of the
    // last coded frame appended in the current coded frame group.
    // The variable is initially unset to indicate that no coded frames have
    // been appended yet.
    Maybe<media::TimeUnit> mLastFrameDuration;
    // Highest end timestamp variable that stores the highest coded frame end
    // timestamp across all coded frames in the current coded frame group that
    // were appended to this track buffer.
    // The variable is initially unset to indicate that no coded frames have
    // been appended yet.
    Maybe<media::TimeUnit> mHighestEndTimestamp;
    // Longest frame duration seen in a coded frame group.
    Maybe<media::TimeUnit> mLongestFrameDuration;
    // Need random access point flag variable that keeps track of whether the
    // track buffer is waiting for a random access point coded frame.
    // The variable is initially set to true to indicate that random access
    // point coded frame is needed before anything can be added to the track
    // buffer.
    bool mNeedRandomAccessPoint;
    nsRefPtr<MediaTrackDemuxer> mDemuxer;
    MozPromiseRequestHolder<MediaTrackDemuxer::SamplesPromise> mDemuxRequest;
    // Highest end timestamp of the last media segment demuxed.
    media::TimeUnit mLastParsedEndTime;

    // If set, position where the next contiguous frame will be inserted.
    // If a discontinuity is detected, it will be unset and recalculated upon
    // the next insertion.
    Maybe<size_t> mNextInsertionIndex;
    // Samples just demuxed, but not yet parsed.
    TrackBuffer mQueuedSamples;
    // We only manage a single track of each type at this time.
    nsTArray<TrackBuffer> mBuffers;
    // Track buffer ranges variable that represents the presentation time ranges
    // occupied by the coded frames currently stored in the track buffer.
    media::TimeIntervals mBufferedRanges;
    // Byte size of all samples contained in this track buffer.
    uint32_t mSizeBuffer;
    // TrackInfo of the first metadata received.
    nsRefPtr<SharedTrackInfo> mInfo;
    // TrackInfo of the last metadata parsed (updated with each init segment.
    nsRefPtr<SharedTrackInfo> mLastInfo;

    // If set, position of the next sample to be retrieved by GetSample().
    Maybe<uint32_t> mNextGetSampleIndex;
    // Approximation of the next sample's decode timestamp.
    media::TimeUnit mNextSampleTimecode;
    // Approximation of the next sample's presentation timestamp.
    media::TimeUnit mNextSampleTime;

    void ResetAppendState()
    {
      mLastDecodeTimestamp.reset();
      mLastFrameDuration.reset();
      mHighestEndTimestamp.reset();
      mNeedRandomAccessPoint = true;

      mLongestFrameDuration.reset();
      mNextInsertionIndex.reset();
    }
  };

  void CheckSequenceDiscontinuity();
  void ProcessFrames(TrackBuffer& aSamples, TrackData& aTrackData);
  bool CheckNextInsertionIndex(TrackData& aTrackData,
                               const media::TimeUnit& aSampleTime);
  void InsertFrames(TrackBuffer& aSamples,
                    const media::TimeIntervals& aIntervals,
                    TrackData& aTrackData);
  void RemoveFrames(const media::TimeIntervals& aIntervals,
                    TrackData& aTrackData,
                    uint32_t aStartIndex);
  void UpdateBufferedRanges();
  void RejectProcessing(nsresult aRejectValue, const char* aName);
  void ResolveProcessing(bool aResolveValue, const char* aName);
  MozPromiseRequestHolder<CodedFrameProcessingPromise> mProcessingRequest;
  MozPromiseHolder<CodedFrameProcessingPromise> mProcessingPromise;

  MozPromiseHolder<AppendPromise> mAppendPromise;
  // Set to true while SegmentParserLoop is running. This is used for diagnostic
  // purposes only. We can't rely on mAppendPromise to be empty as it is only
  // cleared in a follow up task.
  bool mAppendRunning;

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
  RefPtr<TaskQueue> mTaskQueue;

  media::TimeInterval mAppendWindow;
  media::TimeUnit mTimestampOffset;
  media::TimeUnit mLastTimestampOffset;
  void RestoreCachedVariables();

  // Strong references to external objects.
  nsRefPtr<dom::SourceBufferAttributes> mSourceBufferAttributes;
  nsMainThreadPtrHandle<MediaSourceDecoder> mParentDecoder;

  // MediaSource duration mirrored from MediaDecoder on the main thread..
  Mirror<Maybe<double>> mMediaSourceDuration;

  // Set to true if abort was called.
  Atomic<bool> mAbort;
  // Set to true if mediasource state changed to ended.
  Atomic<bool> mEnded;

  // Global size of this source buffer content.
  Atomic<int64_t> mSizeSourceBuffer;
  uint32_t mEvictionThreshold;
  Atomic<bool> mEvictionOccurred;

  // Monitor to protect following objects accessed across multipple threads.
  mutable Monitor mMonitor;
  // Stable audio and video track time ranges.
  media::TimeIntervals mVideoBufferedRanges;
  media::TimeIntervals mAudioBufferedRanges;
  media::TimeUnit mOfficialGroupEndTimestamp;
  // MediaInfo of the first init segment read.
  MediaInfo mInfo;
};

} // namespace mozilla

#endif /* MOZILLA_TRACKBUFFERSMANAGER_H_ */
