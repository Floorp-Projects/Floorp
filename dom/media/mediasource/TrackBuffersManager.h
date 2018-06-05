/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_TRACKBUFFERSMANAGER_H_
#define MOZILLA_TRACKBUFFERSMANAGER_H_

#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/NotNull.h"
#include "AutoTaskQueue.h"

#include "MediaContainerType.h"
#include "MediaData.h"
#include "MediaDataDemuxer.h"
#include "MediaResult.h"
#include "MediaSourceDecoder.h"
#include "SourceBufferTask.h"
#include "TimeUnits.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"

namespace mozilla {

class AbstractThread;
class ContainerParser;
class MediaByteBuffer;
class MediaRawData;
class MediaSourceDemuxer;
class SourceBufferResource;

class SourceBufferTaskQueue
{
public:
  SourceBufferTaskQueue() { }

  ~SourceBufferTaskQueue()
  {
    MOZ_ASSERT(mQueue.IsEmpty(), "All tasks must have been processed");
  }

  void Push(SourceBufferTask* aTask)
  {
    mQueue.AppendElement(aTask);
  }

  already_AddRefed<SourceBufferTask> Pop()
  {
    if (!mQueue.Length()) {
      return nullptr;
    }
    RefPtr<SourceBufferTask> task = std::move(mQueue[0]);
    mQueue.RemoveElementAt(0);
    return task.forget();
  }

  nsTArray<RefPtr<SourceBufferTask>>::size_type Length() const
  {
    return mQueue.Length();
  }
private:
  nsTArray<RefPtr<SourceBufferTask>> mQueue;
};

DDLoggedTypeDeclName(TrackBuffersManager);

class TrackBuffersManager : public DecoderDoctorLifeLogger<TrackBuffersManager>
{
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
                      const MediaContainerType& aType);

  // Queue a task to add data to the end of the input buffer and run the MSE
  // Buffer Append Algorithm
  // 3.5.5 Buffer Append Algorithm.
  // http://w3c.github.io/media-source/index.html#sourcebuffer-buffer-append
  RefPtr<AppendPromise> AppendData(already_AddRefed<MediaByteBuffer> aData,
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

  // Queue a task to run ChangeType
  void ChangeType(const MediaContainerType& aType);

  // Returns the buffered range currently managed.
  // This may be called on any thread.
  // Buffered must conform to http://w3c.github.io/media-source/index.html#widl-SourceBuffer-buffered
  media::TimeIntervals Buffered() const;
  media::TimeUnit HighestStartTime() const;
  media::TimeUnit HighestEndTime() const;

  // Return the size of the data managed by this SourceBufferContentManager.
  int64_t GetSize() const;

  // Indicate that the MediaSource parent object got into "ended" state.
  void Ended();

  // The parent SourceBuffer is about to be destroyed.
  void Detach();

  int64_t EvictionThreshold() const;

  // Interface for MediaSourceDemuxer
  MediaInfo GetMetadata() const;
  const TrackBuffer& GetTrackBuffer(TrackInfo::TrackType aTrack) const;
  const media::TimeIntervals& Buffered(TrackInfo::TrackType) const;
  const media::TimeUnit& HighestStartTime(TrackInfo::TrackType) const;
  media::TimeIntervals SafeBuffered(TrackInfo::TrackType) const;
  bool IsEnded() const
  {
    return mEnded;
  }
  uint32_t Evictable(TrackInfo::TrackType aTrack) const;
  media::TimeUnit Seek(TrackInfo::TrackType aTrack,
                       const media::TimeUnit& aTime,
                       const media::TimeUnit& aFuzz);
  uint32_t SkipToNextRandomAccessPoint(TrackInfo::TrackType aTrack,
                                       const media::TimeUnit& aTimeThreadshold,
                                       const media::TimeUnit& aFuzz,
                                       bool& aFound);

  already_AddRefed<MediaRawData> GetSample(TrackInfo::TrackType aTrack,
                                           const media::TimeUnit& aFuzz,
                                           MediaResult& aResult);
  int32_t FindCurrentPosition(TrackInfo::TrackType aTrack,
                              const media::TimeUnit& aFuzz) const;
  media::TimeUnit GetNextRandomAccessPoint(TrackInfo::TrackType aTrack,
                                           const media::TimeUnit& aFuzz);

  void AddSizeOfResources(MediaSourceDecoder::ResourceSizes* aSizes) const;
  void AssertHasNextSampleIndex(TrackInfo::TrackType aTrack) const
  {
    const auto& trackData = GetTracksData(aTrack);
    MOZ_DIAGNOSTIC_ASSERT(trackData.mNextGetSampleIndex.isSome(),
                          "next sample index must be set");
  }

private:
  typedef MozPromise<bool, MediaResult, /* IsExclusive = */ true> CodedFrameProcessingPromise;

  // for MediaSourceDemuxer::GetMozDebugReaderData
  friend class MediaSourceDemuxer;
  ~TrackBuffersManager();
  // All following functions run on the taskqueue.
  RefPtr<AppendPromise> DoAppendData(already_AddRefed<MediaByteBuffer> aData,
                                     const SourceBufferAttributes& aAttributes);
  void ScheduleSegmentParserLoop();
  void SegmentParserLoop();
  void InitializationSegmentReceived();
  void ShutdownDemuxers();
  void CreateDemuxerforMIMEType();
  void ResetDemuxingState();
  void NeedMoreData();
  void RejectAppend(const MediaResult& aRejectValue, const char* aName);
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
  bool mChangeTypeReceived;
  // Set to true once a new segment is started.
  bool mNewMediaSegmentStarted;
  bool mActiveTrack;
  MediaContainerType mType;

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
  uint64_t mProcessedInput;
  Maybe<media::TimeUnit> mLastParsedEndTime;

  void OnDemuxerInitDone(const MediaResult& aResult);
  void OnDemuxerInitFailed(const MediaResult& aFailure);
  void OnDemuxerResetDone(const MediaResult& aResult);
  MozPromiseRequestHolder<MediaDataDemuxer::InitPromise> mDemuxerInitRequest;

  void OnDemuxFailed(TrackType aTrack, const MediaResult& aError);
  void DoDemuxVideo();
  void OnVideoDemuxCompleted(RefPtr<MediaTrackDemuxer::SamplesHolder> aSamples);
  void OnVideoDemuxFailed(const MediaResult& aError)
  {
    mVideoTracks.mDemuxRequest.Complete();
    OnDemuxFailed(TrackType::kVideoTrack, aError);
  }
  void DoDemuxAudio();
  void OnAudioDemuxCompleted(RefPtr<MediaTrackDemuxer::SamplesHolder> aSamples);
  void OnAudioDemuxFailed(const MediaResult& aError)
  {
    mAudioTracks.mDemuxRequest.Complete();
    OnDemuxFailed(TrackType::kAudioTrack, aError);
  }

  // Dispatches an "encrypted" event is any sample in array has initData
  // present.
  void MaybeDispatchEncryptedEvent(
    const nsTArray<RefPtr<MediaRawData>>& aSamples);

  void DoEvictData(const media::TimeUnit& aPlaybackTime, int64_t aSizeToEvict);

  struct TrackData
  {
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
    // Highest presentation timestamp in track buffer.
    // Protected by global monitor, except when reading on the task queue as it
    // is only written there.
    media::TimeUnit mHighestStartTimestamp;
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
    Maybe<uint32_t> mNextInsertionIndex;
    // Samples just demuxed, but not yet parsed.
    TrackBuffer mQueuedSamples;
    const TrackBuffer& GetTrackBuffer() const
    {
      MOZ_RELEASE_ASSERT(mBuffers.Length(),
                         "TrackBuffer must have been created");
      return mBuffers.LastElement();
    }
    TrackBuffer& GetTrackBuffer()
    {
      MOZ_RELEASE_ASSERT(mBuffers.Length(),
                         "TrackBuffer must have been created");
      return mBuffers.LastElement();
    }
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
    RefPtr<TrackInfoSharedPtr> mInfo;
    // TrackInfo of the last metadata parsed (updated with each init segment.
    RefPtr<TrackInfoSharedPtr> mLastInfo;

    // If set, position of the next sample to be retrieved by GetSample().
    // If the position is equal to the TrackBuffer's length, it indicates that
    // we've reached EOS.
    Maybe<uint32_t> mNextGetSampleIndex;
    // Approximation of the next sample's decode timestamp.
    media::TimeUnit mNextSampleTimecode;
    // Approximation of the next sample's presentation timestamp.
    media::TimeUnit mNextSampleTime;

    struct EvictionIndex
    {
      EvictionIndex() { Reset(); }
      void Reset()
      {
        mEvictable = 0;
        mLastIndex = 0;
      }
      uint32_t mEvictable;
      uint32_t mLastIndex;
    };
    // Size of data that can be safely evicted during the next eviction
    // cycle.
    // We consider as evictable all frames up to the last keyframe prior to
    // mNextGetSampleIndex. If mNextGetSampleIndex isn't set, then we assume
    // that we can't yet evict data.
    // Protected by global monitor, except when reading on the task queue as it
    // is only written there.
    EvictionIndex mEvictionIndex;

    void ResetAppendState()
    {
      mLastDecodeTimestamp.reset();
      mLastFrameDuration.reset();
      mHighestEndTimestamp.reset();
      mNeedRandomAccessPoint = true;
      mNextInsertionIndex.reset();
    }

    void Reset()
    {
      ResetAppendState();
      mEvictionIndex.Reset();
      for (auto& buffer : mBuffers) {
        buffer.Clear();
      }
      mSizeBuffer = 0;
      mNextGetSampleIndex.reset();
      mBufferedRanges.Clear();
      mSanitizedBufferedRanges.Clear();
    }

    void AddSizeOfResources(MediaSourceDecoder::ResourceSizes* aSizes) const;
  };

  void CheckSequenceDiscontinuity(const media::TimeUnit& aPresentationTime);
  void ProcessFrames(TrackBuffer& aSamples, TrackData& aTrackData);
  media::TimeInterval PresentationInterval(const TrackBuffer& aSamples) const;
  bool CheckNextInsertionIndex(TrackData& aTrackData,
                               const media::TimeUnit& aSampleTime);
  void InsertFrames(TrackBuffer& aSamples,
                    const media::TimeIntervals& aIntervals,
                    TrackData& aTrackData);
  void UpdateHighestTimestamp(TrackData& aTrackData,
                              const media::TimeUnit& aHighestTime);
  // Remove all frames and their dependencies contained in aIntervals.
  // Return the index at which frames were first removed or 0 if no frames
  // removed.
  uint32_t RemoveFrames(const media::TimeIntervals& aIntervals,
                        TrackData& aTrackData,
                        uint32_t aStartIndex);
  // Recalculate track's evictable amount.
  void ResetEvictionIndex(TrackData& aTrackData);
  void UpdateEvictionIndex(TrackData& aTrackData, uint32_t aCurrentIndex);
  // Find index of sample. Return a negative value if not found.
  uint32_t FindSampleIndex(const TrackBuffer& aTrackBuffer,
                           const media::TimeInterval& aInterval);
  const MediaRawData* GetSample(TrackInfo::TrackType aTrack,
                                uint32_t aIndex,
                                const media::TimeUnit& aExpectedDts,
                                const media::TimeUnit& aExpectedPts,
                                const media::TimeUnit& aFuzz);
  void UpdateBufferedRanges();
  void RejectProcessing(const MediaResult& aRejectValue, const char* aName);
  void ResolveProcessing(bool aResolveValue, const char* aName);
  MozPromiseRequestHolder<CodedFrameProcessingPromise> mProcessingRequest;
  MozPromiseHolder<CodedFrameProcessingPromise> mProcessingPromise;

  // Trackbuffers definition.
  nsTArray<const TrackData*> GetTracksList() const;
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
  const TrackData& GetTracksData(TrackType aTrack) const
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
  RefPtr<AutoTaskQueue> GetTaskQueueSafe() const
  {
    MutexAutoLock mut(mMutex);
    return mTaskQueue;
  }
  NotNull<AbstractThread*> TaskQueueFromTaskQueue() const
  {
#ifdef DEBUG
    RefPtr<AutoTaskQueue> taskQueue = GetTaskQueueSafe();
    MOZ_ASSERT(taskQueue && taskQueue->IsCurrentThreadIn());
#endif
    return WrapNotNull(mTaskQueue.get());
  }
  bool OnTaskQueue() const
  {
    auto taskQueue = TaskQueueFromTaskQueue();
    return taskQueue->IsCurrentThreadIn();
  }
  void ResetTaskQueue()
  {
    MutexAutoLock mut(mMutex);
    mTaskQueue = nullptr;
  }

  // SourceBuffer Queues and running context.
  SourceBufferTaskQueue mQueue;
  void QueueTask(SourceBufferTask* aTask);
  void ProcessTasks();
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

  const RefPtr<AbstractThread> mAbstractMainThread;

  // Return public highest end time across all aTracks.
  // Monitor must be held.
  media::TimeUnit HighestEndTime(nsTArray<const media::TimeIntervals*>& aTracks) const;

  // Set to true if mediasource state changed to ended.
  Atomic<bool> mEnded;

  // Global size of this source buffer content.
  Atomic<int64_t> mSizeSourceBuffer;
  const int64_t mVideoEvictionThreshold;
  const int64_t mAudioEvictionThreshold;
  enum class EvictionState
  {
    NO_EVICTION_NEEDED,
    EVICTION_NEEDED,
    EVICTION_COMPLETED,
  };
  Atomic<EvictionState> mEvictionState;

  // Monitor to protect following objects accessed across multiple threads.
  mutable Mutex mMutex;
  // mTaskQueue is only ever written after construction on the task queue.
  // As such, it can be accessed while on task queue without the need for the
  // mutex.
  RefPtr<AutoTaskQueue> mTaskQueue;
  // Stable audio and video track time ranges.
  media::TimeIntervals mVideoBufferedRanges;
  media::TimeIntervals mAudioBufferedRanges;
  // MediaInfo of the first init segment read.
  MediaInfo mInfo;
};

} // namespace mozilla

#endif /* MOZILLA_TRACKBUFFERSMANAGER_H_ */
