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
#include "AutoTaskQueue.h"
#include "mozilla/dom/SourceBufferBinding.h"

#include "MediaData.h"
#include "MediaDataDemuxer.h"
#include "MediaSourceDecoder.h"
#include "SourceBufferTask.h"
#include "TimeUnits.h"
#include "nsProxyRelease.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {

class ContainerParser;
class MediaByteBuffer;
class MediaRawData;
class MediaSourceDemuxer;
class SourceBufferResource;

class SourceBufferTaskQueue {
public:
  SourceBufferTaskQueue()
  : mMonitor("SourceBufferTaskQueue")
  {}

  void Push(SourceBufferTask* aTask)
  {
    MonitorAutoLock mon(mMonitor);
    mQueue.AppendElement(aTask);
  }

  already_AddRefed<SourceBufferTask> Pop()
  {
    MonitorAutoLock mon(mMonitor);
    if (!mQueue.Length()) {
      return nullptr;
    }
    RefPtr<SourceBufferTask> task = Move(mQueue[0]);
    mQueue.RemoveElementAt(0);
    return task.forget();
  }

  nsTArray<SourceBufferTask>::size_type Length() const
  {
    MonitorAutoLock mon(mMonitor);
    return mQueue.Length();
  }

private:
  mutable Monitor mMonitor;
  nsTArray<RefPtr<SourceBufferTask>> mQueue;
};

class TrackBuffersManager {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TrackBuffersManager);

  enum class EvictDataResult : int8_t
  {
    NO_DATA_EVICTED,
    CANT_EVICT,
    BUFFER_FULL,
  };

  typedef TrackInfo::TrackType TrackType;
  typedef MediaData::Type MediaType;
  typedef nsTArray<RefPtr<MediaRawData>> TrackBuffer;
  typedef SourceBufferTask::AppendPromise AppendPromise;
  typedef SourceBufferTask::RangeRemovalPromise RangeRemovalPromise;

  // Interface for SourceBuffer
  TrackBuffersManager(MediaSourceDecoder* aParentDecoder,
                      const nsACString& aType);

  // Queue a task to add data to the end of the input buffer and run the MSE
  // Buffer Append Algorithm
  // 3.5.5 Buffer Append Algorithm.
  // http://w3c.github.io/media-source/index.html#sourcebuffer-buffer-append
  RefPtr<AppendPromise> AppendData(MediaByteBuffer* aData,
                                   const SourceBufferAttributes& aAttributes);

  // Queue a task to abort any pending AppendData.
  // Does nothing at this stage.
  void AbortAppendData();

  // Queue a task to run MSE Reset Parser State Algorithm.
  // 3.5.2 Reset Parser State
  void ResetParserState(SourceBufferAttributes& aAttributes);

  // Queue a task to run the MSE range removal algorithm.
  // http://w3c.github.io/media-source/#sourcebuffer-coded-frame-removal
  RefPtr<RangeRemovalPromise> RangeRemoval(media::TimeUnit aStart,
                                             media::TimeUnit aEnd);

  // Schedule data eviction if necessary as the next call to AppendData will
  // add aSize bytes.
  // Eviction is done in two steps, first remove data up to aPlaybackTime
  // and if still more space is needed remove from the end.
  EvictDataResult EvictData(const media::TimeUnit& aPlaybackTime, int64_t aSize);

  // Returns the buffered range currently managed.
  // This may be called on any thread.
  // Buffered must conform to http://w3c.github.io/media-source/index.html#widl-SourceBuffer-buffered
  media::TimeIntervals Buffered();

  // Return the size of the data managed by this SourceBufferContentManager.
  int64_t GetSize() const;

  // Indicate that the MediaSource parent object got into "ended" state.
  void Ended();

  // The parent SourceBuffer is about to be destroyed.
  void Detach();

  int64_t EvictionThreshold() const;

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
                       const media::TimeUnit& aTime,
                       const media::TimeUnit& aFuzz);
  uint32_t SkipToNextRandomAccessPoint(TrackInfo::TrackType aTrack,
                                       const media::TimeUnit& aTimeThreadshold,
                                       bool& aFound);
  already_AddRefed<MediaRawData> GetSample(TrackInfo::TrackType aTrack,
                                           const media::TimeUnit& aFuzz,
                                           bool& aError);
  media::TimeUnit GetNextRandomAccessPoint(TrackInfo::TrackType aTrack,
                                           const media::TimeUnit& aFuzz);

  void AddSizeOfResources(MediaSourceDecoder::ResourceSizes* aSizes);

private:
  typedef MozPromise<bool, nsresult, /* IsExclusive = */ true> CodedFrameProcessingPromise;

  // for MediaSourceDemuxer::GetMozDebugReaderData
  friend class MediaSourceDemuxer;
  virtual ~TrackBuffersManager();
  // All following functions run on the taskqueue.
  RefPtr<AppendPromise> DoAppendData(RefPtr<MediaByteBuffer> aData,
                                     SourceBufferAttributes aAttributes);
  void ScheduleSegmentParserLoop();
  void SegmentParserLoop();
  void InitializationSegmentReceived();
  void ShutdownDemuxers();
  void CreateDemuxerforMIMEType();
  void ResetDemuxingState();
  void NeedMoreData();
  void RejectAppend(nsresult aRejectValue, const char* aName);
  // Will return a promise that will be resolved once all frames of the current
  // media segment have been processed.
  RefPtr<CodedFrameProcessingPromise> CodedFrameProcessing();
  void CompleteCodedFrameProcessing();
  // Called by ResetParserState.
  void CompleteResetParserState();
  RefPtr<RangeRemovalPromise>
    CodedFrameRemovalWithPromise(media::TimeInterval aInterval);
  bool CodedFrameRemoval(media::TimeInterval aInterval);
  void SetAppendState(SourceBufferAttributes::AppendState aAppendState);

  bool HasVideo() const
  {
    return mVideoTracks.mNumTracks > 0;
  }
  bool HasAudio() const
  {
    return mAudioTracks.mNumTracks > 0;
  }

  // The input buffer as per http://w3c.github.io/media-source/index.html#sourcebuffer-input-buffer
  RefPtr<MediaByteBuffer> mInputBuffer;
  // Buffer full flag as per https://w3c.github.io/media-source/#sourcebuffer-buffer-full-flag.
  // Accessed on both the main thread and the task queue.
  Atomic<bool> mBufferFull;
  bool mFirstInitializationSegmentReceived;
  // Set to true once a new segment is started.
  bool mNewMediaSegmentStarted;
  bool mActiveTrack;
  nsCString mType;

  // ContainerParser objects and methods.
  // Those are used to parse the incoming input buffer.

  // Recreate the ContainerParser and if aReuseInitData is true then
  // feed it with the previous init segment found.
  void RecreateParser(bool aReuseInitData);
  nsAutoPtr<ContainerParser> mParser;

  // Demuxer objects and methods.
  void AppendDataToCurrentInputBuffer(MediaByteBuffer* aData);
  RefPtr<MediaByteBuffer> mInitData;
  // Temporary input buffer to handle partial media segment header.
  // We store the current input buffer content into it should we need to
  // reinitialize the demuxer once we have some samples and a discontinuity is
  // detected.
  RefPtr<MediaByteBuffer> mPendingInputBuffer;
  RefPtr<SourceBufferResource> mCurrentInputBuffer;
  RefPtr<MediaDataDemuxer> mInputDemuxer;
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
  void OnVideoDemuxCompleted(RefPtr<MediaTrackDemuxer::SamplesHolder> aSamples);
  void OnVideoDemuxFailed(DemuxerFailureReason aFailure)
  {
    mVideoTracks.mDemuxRequest.Complete();
    OnDemuxFailed(TrackType::kVideoTrack, aFailure);
  }
  void DoDemuxAudio();
  void OnAudioDemuxCompleted(RefPtr<MediaTrackDemuxer::SamplesHolder> aSamples);
  void OnAudioDemuxFailed(DemuxerFailureReason aFailure)
  {
    mAudioTracks.mDemuxRequest.Complete();
    OnDemuxFailed(TrackType::kAudioTrack, aFailure);
  }

  void DoEvictData(const media::TimeUnit& aPlaybackTime, int64_t aSizeToEvict);

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
    // Longest frame duration seen since last random access point.
    // Only ever accessed when mLastDecodeTimestamp and mLastFrameDuration are
    // set.
    media::TimeUnit mLongestFrameDuration;
    // Need random access point flag variable that keeps track of whether the
    // track buffer is waiting for a random access point coded frame.
    // The variable is initially set to true to indicate that random access
    // point coded frame is needed before anything can be added to the track
    // buffer.
    bool mNeedRandomAccessPoint;
    RefPtr<MediaTrackDemuxer> mDemuxer;
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
    // Sanitized mBufferedRanges with a fuzz of half a sample's duration applied
    // This buffered ranges is the basis of what is exposed to the JS.
    media::TimeIntervals mSanitizedBufferedRanges;
    // Byte size of all samples contained in this track buffer.
    uint32_t mSizeBuffer;
    // TrackInfo of the first metadata received.
    RefPtr<SharedTrackInfo> mInfo;
    // TrackInfo of the last metadata parsed (updated with each init segment.
    RefPtr<SharedTrackInfo> mLastInfo;

    // If set, position of the next sample to be retrieved by GetSample().
    // If the position is equal to the TrackBuffer's length, it indicates that
    // we've reached EOS.
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

      mNextInsertionIndex.reset();
    }

    void AddSizeOfResources(MediaSourceDecoder::ResourceSizes* aSizes);
  };

  void CheckSequenceDiscontinuity(const media::TimeUnit& aPresentationTime);
  void ProcessFrames(TrackBuffer& aSamples, TrackData& aTrackData);
  bool CheckNextInsertionIndex(TrackData& aTrackData,
                               const media::TimeUnit& aSampleTime);
  void InsertFrames(TrackBuffer& aSamples,
                    const media::TimeIntervals& aIntervals,
                    TrackData& aTrackData);
  void RemoveFrames(const media::TimeIntervals& aIntervals,
                    TrackData& aTrackData,
                    uint32_t aStartIndex);
  // Find index of sample. Return a negative value if not found.
  uint32_t FindSampleIndex(const TrackBuffer& aTrackBuffer,
                           const media::TimeInterval& aInterval);
  const MediaRawData* GetSample(TrackInfo::TrackType aTrack,
                                size_t aIndex,
                                const media::TimeUnit& aExpectedDts,
                                const media::TimeUnit& aExpectedPts,
                                const media::TimeUnit& aFuzz);
  void UpdateBufferedRanges();
  void RejectProcessing(nsresult aRejectValue, const char* aName);
  void ResolveProcessing(bool aResolveValue, const char* aName);
  MozPromiseRequestHolder<CodedFrameProcessingPromise> mProcessingRequest;
  MozPromiseHolder<CodedFrameProcessingPromise> mProcessingPromise;

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
  RefPtr<AutoTaskQueue> mTaskQueue;

  // SourceBuffer Queues and running context.
  SourceBufferTaskQueue mQueue;
  void QueueTask(SourceBufferTask* aTask);
  void ProcessTasks();
  void CancelAllTasks();
  // Set if the TrackBuffersManager is currently processing a task.
  // At this stage, this task is always a AppendBufferTask.
  RefPtr<SourceBufferTask> mCurrentTask;
  // Current SourceBuffer state for ongoing task.
  // Its content is returned to the SourceBuffer once the AppendBufferTask has
  // completed.
  UniquePtr<SourceBufferAttributes> mSourceBufferAttributes;
  // The current sourcebuffer append window. It's content is equivalent to
  // mSourceBufferAttributes.mAppendWindowStart/End
  media::TimeInterval mAppendWindow;

  // Strong references to external objects.
  nsMainThreadPtrHandle<MediaSourceDecoder> mParentDecoder;

  // Set to true if mediasource state changed to ended.
  Atomic<bool> mEnded;

  // Global size of this source buffer content.
  Atomic<int64_t> mSizeSourceBuffer;
  const int64_t mVideoEvictionThreshold;
  const int64_t mAudioEvictionThreshold;
  Atomic<bool> mEvictionOccurred;

  // Monitor to protect following objects accessed across multipple threads.
  mutable Monitor mMonitor;
  // Stable audio and video track time ranges.
  media::TimeIntervals mVideoBufferedRanges;
  media::TimeIntervals mAudioBufferedRanges;
  // MediaInfo of the first init segment read.
  MediaInfo mInfo;
};

} // namespace mozilla

#endif /* MOZILLA_TRACKBUFFERSMANAGER_H_ */
