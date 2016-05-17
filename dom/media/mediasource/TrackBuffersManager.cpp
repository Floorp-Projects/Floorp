/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TrackBuffersManager.h"
#include "ContainerParser.h"
#include "MediaSourceDemuxer.h"
#include "MediaSourceUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/StateMirroring.h"
#include "SourceBufferResource.h"
#include "SourceBuffer.h"
#include "WebMDemuxer.h"
#include "SourceBufferTask.h"

#ifdef MOZ_FMP4
#include "MP4Demuxer.h"
#endif

#include <limits>

extern mozilla::LogModule* GetMediaSourceLog();

#define MSE_DEBUG(arg, ...) MOZ_LOG(GetMediaSourceLog(), mozilla::LogLevel::Debug, ("TrackBuffersManager(%p:%s)::%s: " arg, this, mType.get(), __func__, ##__VA_ARGS__))
#define MSE_DEBUGV(arg, ...) MOZ_LOG(GetMediaSourceLog(), mozilla::LogLevel::Verbose, ("TrackBuffersManager(%p:%s)::%s: " arg, this, mType.get(), __func__, ##__VA_ARGS__))

mozilla::LogModule* GetMediaSourceSamplesLog()
{
  static mozilla::LazyLogModule sLogModule("MediaSourceSamples");
  return sLogModule;
}
#define SAMPLE_DEBUG(arg, ...) MOZ_LOG(GetMediaSourceSamplesLog(), mozilla::LogLevel::Debug, ("TrackBuffersManager(%p:%s)::%s: " arg, this, mType.get(), __func__, ##__VA_ARGS__))

namespace mozilla {

using dom::SourceBufferAppendMode;
using media::TimeUnit;
using media::TimeInterval;
using media::TimeIntervals;
typedef SourceBufferTask::AppendBufferResult AppendBufferResult;

static const char*
AppendStateToStr(SourceBufferAttributes::AppendState aState)
{
  switch (aState) {
    case SourceBufferAttributes::AppendState::WAITING_FOR_SEGMENT:
      return "WAITING_FOR_SEGMENT";
    case SourceBufferAttributes::AppendState::PARSING_INIT_SEGMENT:
      return "PARSING_INIT_SEGMENT";
    case SourceBufferAttributes::AppendState::PARSING_MEDIA_SEGMENT:
      return "PARSING_MEDIA_SEGMENT";
    default:
      return "IMPOSSIBLE";
  }
}

static Atomic<uint32_t> sStreamSourceID(0u);

#ifdef MOZ_EME
class DispatchKeyNeededEvent : public Runnable {
public:
  DispatchKeyNeededEvent(AbstractMediaDecoder* aDecoder,
                         nsTArray<uint8_t>& aInitData,
                         const nsString& aInitDataType)
    : mDecoder(aDecoder)
    , mInitData(aInitData)
    , mInitDataType(aInitDataType)
  {
  }
  NS_IMETHOD Run() {
    // Note: Null check the owner, as the decoder could have been shutdown
    // since this event was dispatched.
    MediaDecoderOwner* owner = mDecoder->GetOwner();
    if (owner) {
      owner->DispatchEncrypted(mInitData, mInitDataType);
    }
    mDecoder = nullptr;
    return NS_OK;
  }
private:
  RefPtr<AbstractMediaDecoder> mDecoder;
  nsTArray<uint8_t> mInitData;
  nsString mInitDataType;
};
#endif // MOZ_EME

TrackBuffersManager::TrackBuffersManager(MediaSourceDecoder* aParentDecoder,
                                         const nsACString& aType)
  : mInputBuffer(new MediaByteBuffer)
  , mBufferFull(false)
  , mFirstInitializationSegmentReceived(false)
  , mNewMediaSegmentStarted(false)
  , mActiveTrack(false)
  , mType(aType)
  , mParser(ContainerParser::CreateForMIMEType(aType))
  , mProcessedInput(0)
  , mTaskQueue(aParentDecoder->GetDemuxer()->GetTaskQueue())
  , mParentDecoder(new nsMainThreadPtrHolder<MediaSourceDecoder>(aParentDecoder, false /* strict */))
  , mEnded(false)
  , mDetached(false)
  , mVideoEvictionThreshold(Preferences::GetUint("media.mediasource.eviction_threshold.video",
                                                 100 * 1024 * 1024))
  , mAudioEvictionThreshold(Preferences::GetUint("media.mediasource.eviction_threshold.audio",
                                                 30 * 1024 * 1024))
  , mEvictionOccurred(false)
  , mMonitor("TrackBuffersManager")
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be instanciated on the main thread");
}

TrackBuffersManager::~TrackBuffersManager()
{
  CancelAllTasks();
  ShutdownDemuxers();
}

RefPtr<TrackBuffersManager::AppendPromise>
TrackBuffersManager::AppendData(MediaByteBuffer* aData,
                                const SourceBufferAttributes& aAttributes)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("Appending %lld bytes", aData->Length());

  mEnded = false;

  RefPtr<MediaByteBuffer> buffer = aData;

  return InvokeAsync(GetTaskQueue(), this,
                     __func__, &TrackBuffersManager::DoAppendData,
                     buffer, aAttributes);
}

RefPtr<TrackBuffersManager::AppendPromise>
TrackBuffersManager::DoAppendData(RefPtr<MediaByteBuffer> aData,
                                  SourceBufferAttributes aAttributes)
{
  RefPtr<AppendBufferTask> task = new AppendBufferTask(aData, aAttributes);
  RefPtr<AppendPromise> p = task->mPromise.Ensure(__func__);
  QueueTask(task);

  return p;
}

void
TrackBuffersManager::QueueTask(SourceBufferTask* aTask)
{
  if (!OnTaskQueue()) {
    GetTaskQueue()->Dispatch(NewRunnableMethod<RefPtr<SourceBufferTask>>(
      this, &TrackBuffersManager::QueueTask, aTask));
    return;
  }
  MOZ_ASSERT(OnTaskQueue());
  mQueue.Push(aTask);
  ProcessTasks();
}

void
TrackBuffersManager::ProcessTasks()
{
  MOZ_ASSERT(OnTaskQueue());
  typedef SourceBufferTask::Type Type;

  if (mDetached) {
    return;
  }

  if (mCurrentTask) {
    // Already have a task pending. ProcessTask will be scheduled once the
    // current task complete.
    return;
  }
  RefPtr<SourceBufferTask> task = mQueue.Pop();
  if (!task) {
    // nothing to do.
    return;
  }
  switch (task->GetType()) {
    case Type::AppendBuffer:
      mCurrentTask = task;
      if (!mInputBuffer) {
        mInputBuffer = task->As<AppendBufferTask>()->mBuffer;
      } else if (!mInputBuffer->AppendElements(*task->As<AppendBufferTask>()->mBuffer, fallible)) {
        RejectAppend(NS_ERROR_OUT_OF_MEMORY, __func__);
        return;
      }
      mSourceBufferAttributes =
        MakeUnique<SourceBufferAttributes>(task->As<AppendBufferTask>()->mAttributes);
      mAppendWindow =
        TimeInterval(TimeUnit::FromSeconds(mSourceBufferAttributes->GetAppendWindowStart()),
                     TimeUnit::FromSeconds(mSourceBufferAttributes->GetAppendWindowEnd()));
      ScheduleSegmentParserLoop();
      break;
    case Type::RangeRemoval:
    {
      bool rv = CodedFrameRemoval(task->As<RangeRemovalTask>()->mRange);
      task->As<RangeRemovalTask>()->mPromise.Resolve(rv, __func__);
      break;
    }
    case Type::EvictData:
      DoEvictData(task->As<EvictDataTask>()->mPlaybackTime,
                  task->As<EvictDataTask>()->mSizeToEvict);
      break;
    case Type::Abort:
      // not handled yet, and probably never.
      break;
    case Type::Reset:
      CompleteResetParserState();
      break;
    default:
      NS_WARNING("Invalid Task");
  }
  GetTaskQueue()->Dispatch(NewRunnableMethod(this, &TrackBuffersManager::ProcessTasks));
}

// A PromiseHolder will assert upon destruction if it has a pending promise
// that hasn't been completed. It is possible that a task didn't get processed
// due to the owning SourceBuffer having shutdown.
// We resolve/reject all pending promises and remove all pending tasks from the
// queue.
void
TrackBuffersManager::CancelAllTasks()
{
  typedef SourceBufferTask::Type Type;
  MOZ_DIAGNOSTIC_ASSERT(mDetached);

  if (mCurrentTask) {
    mQueue.Push(mCurrentTask);
    mCurrentTask = nullptr;
  }

  RefPtr<SourceBufferTask> task;
  while ((task = mQueue.Pop())) {
    switch (task->GetType()) {
      case Type::AppendBuffer:
        task->As<AppendBufferTask>()->mPromise.RejectIfExists(NS_ERROR_ABORT, __func__);
        break;
      case Type::RangeRemoval:
        task->As<RangeRemovalTask>()->mPromise.ResolveIfExists(false, __func__);
        break;
      case Type::EvictData:
        break;
      case Type::Abort:
        // not handled yet, and probably never.
        break;
      case Type::Reset:
        break;
      default:
        NS_WARNING("Invalid Task");
    }
  }
}

// The MSE spec requires that we abort the current SegmentParserLoop
// which is then followed by a call to ResetParserState.
// However due to our asynchronous design this causes inherent difficulties.
// As the spec behaviour is non deterministic anyway, we instead process all
// pending frames found in the input buffer.
void
TrackBuffersManager::AbortAppendData()
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("");

  QueueTask(new AbortTask());
}

void
TrackBuffersManager::ResetParserState(SourceBufferAttributes& aAttributes)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("");

  // Spec states:
  // 1. If the append state equals PARSING_MEDIA_SEGMENT and the input buffer contains some complete coded frames, then run the coded frame processing algorithm until all of these complete coded frames have been processed.
  // However, we will wait until all coded frames have been processed regardless
  // of the value of append state.
  QueueTask(new ResetTask());

  // ResetParserState has some synchronous steps that much be performed now.
  // The remaining steps will be performed once the ResetTask gets executed.

  // 6. If the mode attribute equals "sequence", then set the group start timestamp to the group end timestamp
  if (aAttributes.GetAppendMode() == SourceBufferAppendMode::Sequence) {
    aAttributes.SetGroupStartTimestamp(aAttributes.GetGroupEndTimestamp());
  }
  // 8. Set append state to WAITING_FOR_SEGMENT.
  aAttributes.SetAppendState(AppendState::WAITING_FOR_SEGMENT);
}

RefPtr<TrackBuffersManager::RangeRemovalPromise>
TrackBuffersManager::RangeRemoval(TimeUnit aStart, TimeUnit aEnd)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("From %.2f to %.2f", aStart.ToSeconds(), aEnd.ToSeconds());

  mEnded = false;

  return InvokeAsync(GetTaskQueue(), this, __func__,
                     &TrackBuffersManager::CodedFrameRemovalWithPromise,
                     TimeInterval(aStart, aEnd));
}

TrackBuffersManager::EvictDataResult
TrackBuffersManager::EvictData(const TimeUnit& aPlaybackTime, int64_t aSize)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aSize > EvictionThreshold()) {
    // We're adding more data than we can hold.
    return EvictDataResult::BUFFER_FULL;
  }
  const int64_t toEvict = GetSize() + aSize - EvictionThreshold();

  MSE_DEBUG("buffered=%lldkb, eviction threshold=%ukb, evict=%lldkb",
            GetSize() / 1024, EvictionThreshold() / 1024, toEvict / 1024);

  if (toEvict <= 0) {
    return EvictDataResult::NO_DATA_EVICTED;
  }
  if (toEvict <= 512*1024) {
    // Don't bother evicting less than 512KB.
    return EvictDataResult::CANT_EVICT;
  }

  if (mBufferFull && mEvictionOccurred) {
    return EvictDataResult::BUFFER_FULL;
  }

  MSE_DEBUG("Reaching our size limit, schedule eviction of %lld bytes", toEvict);

  QueueTask(new EvictDataTask(aPlaybackTime, toEvict));

  return EvictDataResult::NO_DATA_EVICTED;
}

TimeIntervals
TrackBuffersManager::Buffered()
{
  MSE_DEBUG("");
  MonitorAutoLock mon(mMonitor);
  // http://w3c.github.io/media-source/index.html#widl-SourceBuffer-buffered
  // 2. Let highest end time be the largest track buffer ranges end time across all the track buffers managed by this SourceBuffer object.
  TimeUnit highestEndTime;

  nsTArray<TimeIntervals*> tracks;
  if (HasVideo()) {
    tracks.AppendElement(&mVideoBufferedRanges);
  }
  if (HasAudio()) {
    tracks.AppendElement(&mAudioBufferedRanges);
  }
  for (auto trackRanges : tracks) {
    highestEndTime = std::max(trackRanges->GetEnd(), highestEndTime);
  }

  // 3. Let intersection ranges equal a TimeRange object containing a single range from 0 to highest end time.
  TimeIntervals intersection{TimeInterval(TimeUnit::FromSeconds(0), highestEndTime)};

  // 4. For each track buffer managed by this SourceBuffer, run the following steps:
  //   1. Let track ranges equal the track buffer ranges for the current track buffer.
  for (auto trackRanges : tracks) {
    // 2. If readyState is "ended", then set the end time on the last range in track ranges to highest end time.
    if (mEnded) {
      trackRanges->Add(TimeInterval(trackRanges->GetEnd(), highestEndTime));
    }
    // 3. Let new intersection ranges equal the intersection between the intersection ranges and the track ranges.
    intersection.Intersection(*trackRanges);
  }
  return intersection;
}

int64_t
TrackBuffersManager::GetSize() const
{
  return mSizeSourceBuffer;
}

void
TrackBuffersManager::Ended()
{
  mEnded = true;
}

void
TrackBuffersManager::Detach()
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("");
  mDetached = true;
}

void
TrackBuffersManager::CompleteResetParserState()
{
  MOZ_ASSERT(OnTaskQueue());
  MSE_DEBUG("");

  // We shouldn't change mInputDemuxer while a demuxer init/reset request is
  // being processed. See bug 1239983.
  MOZ_DIAGNOSTIC_ASSERT(!mDemuxerInitRequest.Exists(), "Previous AppendBuffer didn't complete");

  for (auto& track : GetTracksList()) {
    // 2. Unset the last decode timestamp on all track buffers.
    // 3. Unset the last frame duration on all track buffers.
    // 4. Unset the highest end timestamp on all track buffers.
    // 5. Set the need random access point flag on all track buffers to true.
    track->ResetAppendState();

    // if we have been aborted, we may have pending frames that we are going
    // to discard now.
    track->mQueuedSamples.Clear();
  }

  // 7. Remove all bytes from the input buffer.
  mInputBuffer = nullptr;
  if (mCurrentInputBuffer) {
    mCurrentInputBuffer->EvictAll();
    // The demuxer will be recreated during the next run of SegmentParserLoop.
    // As such we don't need to notify it that data has been removed.
    mCurrentInputBuffer = new SourceBufferResource(mType);
  }

  // We could be left with a demuxer in an unusable state. It needs to be
  // recreated. We store in the InputBuffer an init segment which will be parsed
  // during the next Segment Parser Loop and a new demuxer will be created and
  // initialized.
  if (mFirstInitializationSegmentReceived) {
    MOZ_ASSERT(mInitData && mInitData->Length(), "we must have an init segment");
    // The aim here is really to destroy our current demuxer.
    CreateDemuxerforMIMEType();
    // Recreate our input buffer. We can't directly assign the initData buffer
    // to mInputBuffer as it will get modified in the Segment Parser Loop.
    mInputBuffer = new MediaByteBuffer;
    mInputBuffer->AppendElements(*mInitData);
  }
  RecreateParser(true);
}

int64_t
TrackBuffersManager::EvictionThreshold() const
{
  if (HasVideo()) {
    return mVideoEvictionThreshold;
  }
  return mAudioEvictionThreshold;
}

void
TrackBuffersManager::DoEvictData(const TimeUnit& aPlaybackTime,
                                 int64_t aSizeToEvict)
{
  MOZ_ASSERT(OnTaskQueue());

  // Video is what takes the most space, only evict there if we have video.
  const auto& track = HasVideo() ? mVideoTracks : mAudioTracks;
  const auto& buffer = track.mBuffers.LastElement();
  // Remove any data we've already played, or before the next sample to be
  // demuxed whichever is lowest.
  TimeUnit lowerLimit = std::min(track.mNextSampleTime, aPlaybackTime);
  uint32_t lastKeyFrameIndex = 0;
  int64_t toEvict = aSizeToEvict;
  int64_t partialEvict = 0;
  for (uint32_t i = 0; i < buffer.Length(); i++) {
    const auto& frame = buffer[i];
    if (frame->mKeyframe) {
      lastKeyFrameIndex = i;
      toEvict -= partialEvict;
      if (toEvict < 0) {
        break;
      }
      partialEvict = 0;
    }
    if (frame->mTime >= lowerLimit.ToMicroseconds()) {
      break;
    }
    partialEvict += frame->ComputedSizeOfIncludingThis();
  }

  const int64_t finalSize = mSizeSourceBuffer - aSizeToEvict;

  if (lastKeyFrameIndex > 0) {
    MSE_DEBUG("Step1. Evicting %lld bytes prior currentTime",
              aSizeToEvict - toEvict);
    CodedFrameRemoval(
      TimeInterval(TimeUnit::FromMicroseconds(0),
                   TimeUnit::FromMicroseconds(buffer[lastKeyFrameIndex]->mTime - 1)));
  }

  if (mSizeSourceBuffer <= finalSize) {
    return;
  }

  toEvict = mSizeSourceBuffer - finalSize;

  // Still some to remove. Remove data starting from the end, up to 30s ahead
  // of the later of the playback time or the next sample to be demuxed.
  // 30s is a value chosen as it appears to work with YouTube.
  TimeUnit upperLimit =
    std::max(aPlaybackTime, track.mNextSampleTime) + TimeUnit::FromSeconds(30);
  uint32_t evictedFramesStartIndex = buffer.Length();
  for (int32_t i = buffer.Length() - 1; i >= 0; i--) {
    const auto& frame = buffer[i];
    if (frame->mTime <= upperLimit.ToMicroseconds() || toEvict < 0) {
      // We've reached a frame that shouldn't be evicted -> Evict after it -> i+1.
      // Or the previous loop reached the eviction threshold -> Evict from it -> i+1.
      evictedFramesStartIndex = i + 1;
      break;
    }
    toEvict -= frame->ComputedSizeOfIncludingThis();
  }
  if (evictedFramesStartIndex < buffer.Length()) {
    MSE_DEBUG("Step2. Evicting %lld bytes from trailing data",
              mSizeSourceBuffer - finalSize - toEvict);
    CodedFrameRemoval(
      TimeInterval(TimeUnit::FromMicroseconds(buffer[evictedFramesStartIndex]->mTime),
                   TimeUnit::FromInfinity()));
  }
}

RefPtr<TrackBuffersManager::RangeRemovalPromise>
TrackBuffersManager::CodedFrameRemovalWithPromise(TimeInterval aInterval)
{
  MOZ_ASSERT(OnTaskQueue());

  RefPtr<RangeRemovalTask> task = new RangeRemovalTask(aInterval);
  RefPtr<RangeRemovalPromise> p = task->mPromise.Ensure(__func__);
  QueueTask(task);

  return p;
}

bool
TrackBuffersManager::CodedFrameRemoval(TimeInterval aInterval)
{
  MOZ_ASSERT(OnTaskQueue());
  MSE_DEBUG("From %.2fs to %.2f",
            aInterval.mStart.ToSeconds(), aInterval.mEnd.ToSeconds());

#if DEBUG
  if (HasVideo()) {
    MSE_DEBUG("before video ranges=%s",
              DumpTimeRanges(mVideoTracks.mBufferedRanges).get());
  }
  if (HasAudio()) {
    MSE_DEBUG("before audio ranges=%s",
              DumpTimeRanges(mAudioTracks.mBufferedRanges).get());
  }
#endif

  // 1. Let start be the starting presentation timestamp for the removal range.
  TimeUnit start = aInterval.mStart;
  // 2. Let end be the end presentation timestamp for the removal range.
  TimeUnit end = aInterval.mEnd;

  bool dataRemoved = false;

  // 3. For each track buffer in this source buffer, run the following steps:
  for (auto track : GetTracksList()) {
    MSE_DEBUGV("Processing %s track", track->mInfo->mMimeType.get());
    // 1. Let remove end timestamp be the current value of duration
    // See bug: https://www.w3.org/Bugs/Public/show_bug.cgi?id=28727
    // At worse we will remove all frames until the end, unless a key frame is
    // found between the current interval's end and the trackbuffer's end.
    TimeUnit removeEndTimestamp = track->mBufferedRanges.GetEnd();

    if (start > removeEndTimestamp) {
      // Nothing to remove.
      continue;
    }

    // 2. If this track buffer has a random access point timestamp that is greater than or equal to end,
    // then update remove end timestamp to that random access point timestamp.
    if (end < track->mBufferedRanges.GetEnd()) {
      for (auto& frame : track->mBuffers.LastElement()) {
        if (frame->mKeyframe && frame->mTime >= end.ToMicroseconds()) {
          removeEndTimestamp = TimeUnit::FromMicroseconds(frame->mTime);
          break;
        }
      }
    }

    // 3. Remove all media data, from this track buffer, that contain starting
    // timestamps greater than or equal to start and less than the remove end timestamp.
    // 4. Remove decoding dependencies of the coded frames removed in the previous step:
    // Remove all coded frames between the coded frames removed in the previous step and the next random access point after those removed frames.
    TimeIntervals removedInterval{TimeInterval(start, removeEndTimestamp)};
    RemoveFrames(removedInterval, *track, 0);

    // 5. If this object is in activeSourceBuffers, the current playback position
    // is greater than or equal to start and less than the remove end timestamp,
    // and HTMLMediaElement.readyState is greater than HAVE_METADATA, then set the
    // HTMLMediaElement.readyState attribute to HAVE_METADATA and stall playback.
    // This will be done by the MDSM during playback.
    // TODO properly, so it works even if paused.
  }

  UpdateBufferedRanges();

  // Update our reported total size.
  mSizeSourceBuffer = mVideoTracks.mSizeBuffer + mAudioTracks.mSizeBuffer;

  // 4. If buffer full flag equals true and this object is ready to accept more bytes, then set the buffer full flag to false.
  if (mBufferFull && mSizeSourceBuffer < EvictionThreshold()) {
    mBufferFull = false;
  }
  mEvictionOccurred = true;

  return dataRemoved;
}

void
TrackBuffersManager::UpdateBufferedRanges()
{
  MonitorAutoLock mon(mMonitor);

  mVideoBufferedRanges = mVideoTracks.mSanitizedBufferedRanges;
  mAudioBufferedRanges = mAudioTracks.mSanitizedBufferedRanges;

#if DEBUG
  if (HasVideo()) {
    MSE_DEBUG("after video ranges=%s",
              DumpTimeRanges(mVideoTracks.mBufferedRanges).get());
  }
  if (HasAudio()) {
    MSE_DEBUG("after audio ranges=%s",
              DumpTimeRanges(mAudioTracks.mBufferedRanges).get());
  }
#endif
}

void
TrackBuffersManager::SegmentParserLoop()
{
  MOZ_ASSERT(OnTaskQueue());

  while (true) {
    // 1. If the input buffer is empty, then jump to the need more data step below.
    if (!mInputBuffer || mInputBuffer->IsEmpty()) {
      NeedMoreData();
      return;
    }
    // 2. If the input buffer contains bytes that violate the SourceBuffer
    // byte stream format specification, then run the append error algorithm with
    // the decode error parameter set to true and abort this algorithm.
    // TODO

    // 3. Remove any bytes that the byte stream format specifications say must be
    // ignored from the start of the input buffer.
    // We do not remove bytes from our input buffer. Instead we enforce that
    // our ContainerParser is able to skip over all data that is supposed to be
    // ignored.

    // 4. If the append state equals WAITING_FOR_SEGMENT, then run the following
    // steps:
    if (mSourceBufferAttributes->GetAppendState() == AppendState::WAITING_FOR_SEGMENT) {
      if (mParser->IsInitSegmentPresent(mInputBuffer)) {
        SetAppendState(AppendState::PARSING_INIT_SEGMENT);
        if (mFirstInitializationSegmentReceived) {
          // This is a new initialization segment. Obsolete the old one.
          RecreateParser(false);
        }
        continue;
      }
      if (mParser->IsMediaSegmentPresent(mInputBuffer)) {
        SetAppendState(AppendState::PARSING_MEDIA_SEGMENT);
        mNewMediaSegmentStarted = true;
        continue;
      }
      // We have neither an init segment nor a media segment, this is either
      // invalid data or not enough data to detect a segment type.
      MSE_DEBUG("Found invalid or incomplete data.");
      NeedMoreData();
      return;
    }

    int64_t start, end;
    bool newData = mParser->ParseStartAndEndTimestamps(mInputBuffer, start, end);
    mProcessedInput += mInputBuffer->Length();

    // 5. If the append state equals PARSING_INIT_SEGMENT, then run the
    // following steps:
    if (mSourceBufferAttributes->GetAppendState() == AppendState::PARSING_INIT_SEGMENT) {
      if (mParser->InitSegmentRange().IsEmpty()) {
        mInputBuffer = nullptr;
        NeedMoreData();
        return;
      }
      InitializationSegmentReceived();
      return;
    }
    if (mSourceBufferAttributes->GetAppendState() == AppendState::PARSING_MEDIA_SEGMENT) {
      // 1. If the first initialization segment received flag is false, then run the append error algorithm with the decode error parameter set to true and abort this algorithm.
      if (!mFirstInitializationSegmentReceived) {
        RejectAppend(NS_ERROR_FAILURE, __func__);
        return;
      }

      // We can't feed some demuxers (WebMDemuxer) with data that do not have
      // monotonizally increasing timestamps. So we check if we have a
      // discontinuity from the previous segment parsed.
      // If so, recreate a new demuxer to ensure that the demuxer is only fed
      // monotonically increasing data.
      if (mNewMediaSegmentStarted) {
        if (newData && mLastParsedEndTime.isSome() &&
            start < mLastParsedEndTime.ref().ToMicroseconds()) {
          MSE_DEBUG("Re-creating demuxer");
          ResetDemuxingState();
          return;
        }
        if (newData || !mParser->MediaSegmentRange().IsEmpty()) {
          if (mPendingInputBuffer) {
            // We now have a complete media segment header. We can resume parsing
            // the data.
            AppendDataToCurrentInputBuffer(mPendingInputBuffer);
            mPendingInputBuffer = nullptr;
          }
          mNewMediaSegmentStarted = false;
        } else {
          // We don't have any data to demux yet, stash aside the data.
          // This also handles the case:
          // 2. If the input buffer does not contain a complete media segment header yet, then jump to the need more data step below.
          if (!mPendingInputBuffer) {
            mPendingInputBuffer = mInputBuffer;
          } else {
            mPendingInputBuffer->AppendElements(*mInputBuffer);
          }
          mInputBuffer = nullptr;
          NeedMoreData();
          return;
        }
      }

      // 3. If the input buffer contains one or more complete coded frames, then run the coded frame processing algorithm.
      RefPtr<TrackBuffersManager> self = this;
      mProcessingRequest.Begin(CodedFrameProcessing()
          ->Then(GetTaskQueue(), __func__,
                 [self] (bool aNeedMoreData) {
                   self->mProcessingRequest.Complete();
                   if (aNeedMoreData) {
                     self->NeedMoreData();
                   } else {
                     self->ScheduleSegmentParserLoop();
                   }
                 },
                 [self] (nsresult aRejectValue) {
                   self->mProcessingRequest.Complete();
                   self->RejectAppend(aRejectValue, __func__);
                 }));
      return;
    }
  }
}

void
TrackBuffersManager::NeedMoreData()
{
  MSE_DEBUG("");
  if (mDetached) {
    // We've been detached.
    return;
  }
  MOZ_DIAGNOSTIC_ASSERT(mCurrentTask && mCurrentTask->GetType() == SourceBufferTask::Type::AppendBuffer);
  MOZ_DIAGNOSTIC_ASSERT(mSourceBufferAttributes);

  mCurrentTask->As<AppendBufferTask>()->mPromise.Resolve(
    SourceBufferTask::AppendBufferResult(mActiveTrack,
                                         *mSourceBufferAttributes),
                                         __func__);
  mSourceBufferAttributes = nullptr;
  mCurrentTask = nullptr;
  ProcessTasks();
}

void
TrackBuffersManager::RejectAppend(nsresult aRejectValue, const char* aName)
{
  MSE_DEBUG("rv=%d", aRejectValue);
  if (mDetached) {
    // We've been detached.
    return;
  }
  MOZ_DIAGNOSTIC_ASSERT(mCurrentTask && mCurrentTask->GetType() == SourceBufferTask::Type::AppendBuffer);

  mCurrentTask->As<AppendBufferTask>()->mPromise.Reject(aRejectValue, __func__);
  mSourceBufferAttributes = nullptr;
  mCurrentTask = nullptr;
  ProcessTasks();
}

void
TrackBuffersManager::ScheduleSegmentParserLoop()
{
  if (mDetached) {
    return;
  }
  GetTaskQueue()->Dispatch(NewRunnableMethod(this, &TrackBuffersManager::SegmentParserLoop));
}

void
TrackBuffersManager::ShutdownDemuxers()
{
  if (mVideoTracks.mDemuxer) {
    mVideoTracks.mDemuxer->BreakCycles();
    mVideoTracks.mDemuxer = nullptr;
  }
  if (mAudioTracks.mDemuxer) {
    mAudioTracks.mDemuxer->BreakCycles();
    mAudioTracks.mDemuxer = nullptr;
  }
  // We shouldn't change mInputDemuxer while a demuxer init/reset request is
  // being processed. See bug 1239983.
  MOZ_DIAGNOSTIC_ASSERT(!mDemuxerInitRequest.Exists());
  mInputDemuxer = nullptr;
  mLastParsedEndTime.reset();
}

void
TrackBuffersManager::CreateDemuxerforMIMEType()
{
  ShutdownDemuxers();

  if (mType.LowerCaseEqualsLiteral("video/webm") ||
      mType.LowerCaseEqualsLiteral("audio/webm")) {
    mInputDemuxer = new WebMDemuxer(mCurrentInputBuffer, true /* IsMediaSource*/ );
    return;
  }

#ifdef MOZ_FMP4
  if (mType.LowerCaseEqualsLiteral("video/mp4") ||
      mType.LowerCaseEqualsLiteral("audio/mp4")) {
    mInputDemuxer = new MP4Demuxer(mCurrentInputBuffer);
    return;
  }
#endif
  NS_WARNING("Not supported (yet)");
  return;
}

// We reset the demuxer by creating a new one and initializing it.
void
TrackBuffersManager::ResetDemuxingState()
{
  MOZ_ASSERT(mParser && mParser->HasInitData());
  RecreateParser(true);
  mCurrentInputBuffer = new SourceBufferResource(mType);
  // The demuxer isn't initialized yet ; we don't want to notify it
  // that data has been appended yet ; so we simply append the init segment
  // to the resource.
  mCurrentInputBuffer->AppendData(mParser->InitData());
  CreateDemuxerforMIMEType();
  if (!mInputDemuxer) {
    RejectAppend(NS_ERROR_FAILURE, __func__);
    return;
  }
  mDemuxerInitRequest.Begin(mInputDemuxer->Init()
                      ->Then(GetTaskQueue(), __func__,
                             this,
                             &TrackBuffersManager::OnDemuxerResetDone,
                             &TrackBuffersManager::OnDemuxerInitFailed));
}

void
TrackBuffersManager::OnDemuxerResetDone(nsresult)
{
  MOZ_ASSERT(OnTaskQueue());
  mDemuxerInitRequest.Complete();
  // mInputDemuxer shouldn't have been destroyed while a demuxer init/reset
  // request was being processed. See bug 1239983.
  MOZ_DIAGNOSTIC_ASSERT(mInputDemuxer);

  // Recreate track demuxers.
  uint32_t numVideos = mInputDemuxer->GetNumberTracks(TrackInfo::kVideoTrack);
  if (numVideos) {
    // We currently only handle the first video track.
    mVideoTracks.mDemuxer =
      mInputDemuxer->GetTrackDemuxer(TrackInfo::kVideoTrack, 0);
    MOZ_ASSERT(mVideoTracks.mDemuxer);
  }

  uint32_t numAudios = mInputDemuxer->GetNumberTracks(TrackInfo::kAudioTrack);
  if (numAudios) {
    // We currently only handle the first audio track.
    mAudioTracks.mDemuxer =
      mInputDemuxer->GetTrackDemuxer(TrackInfo::kAudioTrack, 0);
    MOZ_ASSERT(mAudioTracks.mDemuxer);
  }

  if (mPendingInputBuffer) {
    // We had a partial media segment header stashed aside.
    // Reparse its content so we can continue parsing the current input buffer.
    int64_t start, end;
    mParser->ParseStartAndEndTimestamps(mPendingInputBuffer, start, end);
    mProcessedInput += mPendingInputBuffer->Length();
  }

  SegmentParserLoop();
}

void
TrackBuffersManager::AppendDataToCurrentInputBuffer(MediaByteBuffer* aData)
{
  MOZ_ASSERT(mCurrentInputBuffer);
  mCurrentInputBuffer->AppendData(aData);
  mInputDemuxer->NotifyDataArrived();
}

void
TrackBuffersManager::InitializationSegmentReceived()
{
  MOZ_ASSERT(mParser->HasCompleteInitData());
  mCurrentInputBuffer = new SourceBufferResource(mType);
  // The demuxer isn't initialized yet ; we don't want to notify it
  // that data has been appended yet ; so we simply append the init segment
  // to the resource.
  mCurrentInputBuffer->AppendData(mParser->InitData());
  uint32_t length =
    mParser->InitSegmentRange().mEnd - (mProcessedInput - mInputBuffer->Length());
  if (mInputBuffer->Length() == length) {
    mInputBuffer = nullptr;
  } else {
    mInputBuffer->RemoveElementsAt(0, length);
  }
  CreateDemuxerforMIMEType();
  if (!mInputDemuxer) {
    NS_WARNING("TODO type not supported");
    RejectAppend(NS_ERROR_DOM_NOT_SUPPORTED_ERR, __func__);
    return;
  }
  mDemuxerInitRequest.Begin(mInputDemuxer->Init()
                      ->Then(GetTaskQueue(), __func__,
                             this,
                             &TrackBuffersManager::OnDemuxerInitDone,
                             &TrackBuffersManager::OnDemuxerInitFailed));
}

void
TrackBuffersManager::OnDemuxerInitDone(nsresult)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_DIAGNOSTIC_ASSERT(mInputDemuxer, "mInputDemuxer has been destroyed");

  mDemuxerInitRequest.Complete();

  MediaInfo info;

  uint32_t numVideos = mInputDemuxer->GetNumberTracks(TrackInfo::kVideoTrack);
  if (numVideos) {
    // We currently only handle the first video track.
    mVideoTracks.mDemuxer =
      mInputDemuxer->GetTrackDemuxer(TrackInfo::kVideoTrack, 0);
    MOZ_ASSERT(mVideoTracks.mDemuxer);
    info.mVideo = *mVideoTracks.mDemuxer->GetInfo()->GetAsVideoInfo();
    info.mVideo.mTrackId = 2;
  }

  uint32_t numAudios = mInputDemuxer->GetNumberTracks(TrackInfo::kAudioTrack);
  if (numAudios) {
    // We currently only handle the first audio track.
    mAudioTracks.mDemuxer =
      mInputDemuxer->GetTrackDemuxer(TrackInfo::kAudioTrack, 0);
    MOZ_ASSERT(mAudioTracks.mDemuxer);
    info.mAudio = *mAudioTracks.mDemuxer->GetInfo()->GetAsAudioInfo();
    info.mAudio.mTrackId = 1;
  }

  int64_t videoDuration = numVideos ? info.mVideo.mDuration : 0;
  int64_t audioDuration = numAudios ? info.mAudio.mDuration : 0;

  int64_t duration = std::max(videoDuration, audioDuration);
  // 1. Update the duration attribute if it currently equals NaN.
  // Those steps are performed by the MediaSourceDecoder::SetInitialDuration
  AbstractThread::MainThread()->Dispatch(NewRunnableMethod<int64_t>
                                         (mParentDecoder,
                                          &MediaSourceDecoder::SetInitialDuration,
                                          duration ? duration : -1));

  // 2. If the initialization segment has no audio, video, or text tracks, then
  // run the append error algorithm with the decode error parameter set to true
  // and abort these steps.
  if (!numVideos && !numAudios) {
    RejectAppend(NS_ERROR_FAILURE, __func__);
    return;
  }

  // 3. If the first initialization segment received flag is true, then run the following steps:
  if (mFirstInitializationSegmentReceived) {
    if (numVideos != mVideoTracks.mNumTracks ||
        numAudios != mAudioTracks.mNumTracks ||
        (numVideos && info.mVideo.mMimeType != mVideoTracks.mInfo->mMimeType) ||
        (numAudios && info.mAudio.mMimeType != mAudioTracks.mInfo->mMimeType)) {
      RejectAppend(NS_ERROR_FAILURE, __func__);
      return;
    }
    // 1. If more than one track for a single type are present (ie 2 audio tracks),
    // then the Track IDs match the ones in the first initialization segment.
    // TODO
    // 2. Add the appropriate track descriptions from this initialization
    // segment to each of the track buffers.
    // TODO
    // 3. Set the need random access point flag on all track buffers to true.
    mVideoTracks.mNeedRandomAccessPoint = true;
    mAudioTracks.mNeedRandomAccessPoint = true;
  }

  // 4. Let active track flag equal false.
  bool activeTrack = false;

  // Increase our stream id.
  uint32_t streamID = sStreamSourceID++;

  // 5. If the first initialization segment received flag is false, then run the following steps:
  if (!mFirstInitializationSegmentReceived) {
    mAudioTracks.mNumTracks = numAudios;
    // TODO:
    // 1. If the initialization segment contains tracks with codecs the user agent
    // does not support, then run the append error algorithm with the decode
    // error parameter set to true and abort these steps.

    // 2. For each audio track in the initialization segment, run following steps:
    // for (uint32_t i = 0; i < numAudios; i++) {
    if (numAudios) {
      // 1. Let audio byte stream track ID be the Track ID for the current track being processed.
      // 2. Let audio language be a BCP 47 language tag for the language specified in the initialization segment for this track or an empty string if no language info is present.
      // 3. If audio language equals an empty string or the 'und' BCP 47 value, then run the default track language algorithm with byteStreamTrackID set to audio byte stream track ID and type set to "audio" and assign the value returned by the algorithm to audio language.
      // 4. Let audio label be a label specified in the initialization segment for this track or an empty string if no label info is present.
      // 5. If audio label equals an empty string, then run the default track label algorithm with byteStreamTrackID set to audio byte stream track ID and type set to "audio" and assign the value returned by the algorithm to audio label.
      // 6. Let audio kinds be an array of kind strings specified in the initialization segment for this track or an empty array if no kind information is provided.
      // 7. If audio kinds equals an empty array, then run the default track kinds algorithm with byteStreamTrackID set to audio byte stream track ID and type set to "audio" and assign the value returned by the algorithm to audio kinds.
      // 8. For each value in audio kinds, run the following steps:
      //   1. Let current audio kind equal the value from audio kinds for this iteration of the loop.
      //   2. Let new audio track be a new AudioTrack object.
      //   3. Generate a unique ID and assign it to the id property on new audio track.
      //   4. Assign audio language to the language property on new audio track.
      //   5. Assign audio label to the label property on new audio track.
      //   6. Assign current audio kind to the kind property on new audio track.
      //   7. If audioTracks.length equals 0, then run the following steps:
      //     1. Set the enabled property on new audio track to true.
      //     2. Set active track flag to true.
      activeTrack = true;
      //   8. Add new audio track to the audioTracks attribute on this SourceBuffer object.
      //   9. Queue a task to fire a trusted event named addtrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the AudioTrackList object referenced by the audioTracks attribute on this SourceBuffer object.
      //   10. Add new audio track to the audioTracks attribute on the HTMLMediaElement.
      //   11. Queue a task to fire a trusted event named addtrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the AudioTrackList object referenced by the audioTracks attribute on the HTMLMediaElement.
      mAudioTracks.mBuffers.AppendElement(TrackBuffer());
      // 10. Add the track description for this track to the track buffer.
      mAudioTracks.mInfo = new SharedTrackInfo(info.mAudio, streamID);
      mAudioTracks.mLastInfo = mAudioTracks.mInfo;
    }

    mVideoTracks.mNumTracks = numVideos;
    // 3. For each video track in the initialization segment, run following steps:
    // for (uint32_t i = 0; i < numVideos; i++) {
    if (numVideos) {
      // 1. Let video byte stream track ID be the Track ID for the current track being processed.
      // 2. Let video language be a BCP 47 language tag for the language specified in the initialization segment for this track or an empty string if no language info is present.
      // 3. If video language equals an empty string or the 'und' BCP 47 value, then run the default track language algorithm with byteStreamTrackID set to video byte stream track ID and type set to "video" and assign the value returned by the algorithm to video language.
      // 4. Let video label be a label specified in the initialization segment for this track or an empty string if no label info is present.
      // 5. If video label equals an empty string, then run the default track label algorithm with byteStreamTrackID set to video byte stream track ID and type set to "video" and assign the value returned by the algorithm to video label.
      // 6. Let video kinds be an array of kind strings specified in the initialization segment for this track or an empty array if no kind information is provided.
      // 7. If video kinds equals an empty array, then run the default track kinds algorithm with byteStreamTrackID set to video byte stream track ID and type set to "video" and assign the value returned by the algorithm to video kinds.
      // 8. For each value in video kinds, run the following steps:
      //   1. Let current video kind equal the value from video kinds for this iteration of the loop.
      //   2. Let new video track be a new VideoTrack object.
      //   3. Generate a unique ID and assign it to the id property on new video track.
      //   4. Assign video language to the language property on new video track.
      //   5. Assign video label to the label property on new video track.
      //   6. Assign current video kind to the kind property on new video track.
      //   7. If videoTracks.length equals 0, then run the following steps:
      //     1. Set the selected property on new video track to true.
      //     2. Set active track flag to true.
      activeTrack = true;
      //   8. Add new video track to the videoTracks attribute on this SourceBuffer object.
      //   9. Queue a task to fire a trusted event named addtrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the VideoTrackList object referenced by the videoTracks attribute on this SourceBuffer object.
      //   10. Add new video track to the videoTracks attribute on the HTMLMediaElement.
      //   11. Queue a task to fire a trusted event named addtrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the VideoTrackList object referenced by the videoTracks attribute on the HTMLMediaElement.
      mVideoTracks.mBuffers.AppendElement(TrackBuffer());
      // 10. Add the track description for this track to the track buffer.
      mVideoTracks.mInfo = new SharedTrackInfo(info.mVideo, streamID);
      mVideoTracks.mLastInfo = mVideoTracks.mInfo;
    }
    // 4. For each text track in the initialization segment, run following steps:
    // 5. If active track flag equals true, then run the following steps:
    // This is handled by SourceBuffer once the promise is resolved.
    if (activeTrack) {
      mActiveTrack = true;
    }

    // 6. Set first initialization segment received flag to true.
    mFirstInitializationSegmentReceived = true;
  } else {
    mAudioTracks.mLastInfo = new SharedTrackInfo(info.mAudio, streamID);
    mVideoTracks.mLastInfo = new SharedTrackInfo(info.mVideo, streamID);
  }

  UniquePtr<EncryptionInfo> crypto = mInputDemuxer->GetCrypto();
  if (crypto && crypto->IsEncrypted()) {
#ifdef MOZ_EME
    // Try and dispatch 'encrypted'. Won't go if ready state still HAVE_NOTHING.
    for (uint32_t i = 0; i < crypto->mInitDatas.Length(); i++) {
      NS_DispatchToMainThread(
        new DispatchKeyNeededEvent(mParentDecoder, crypto->mInitDatas[i].mInitData, NS_LITERAL_STRING("cenc")));
    }
#endif // MOZ_EME
    info.mCrypto = *crypto;
    // We clear our crypto init data array, so the MediaFormatReader will
    // not emit an encrypted event for the same init data again.
    info.mCrypto.mInitDatas.Clear();
    mEncrypted = true;
  }

  {
    MonitorAutoLock mon(mMonitor);
    mInfo = info;
  }

  // We now have a valid init data ; we can store it for later use.
  mInitData = mParser->InitData();

  // 3. Remove the initialization segment bytes from the beginning of the input buffer.
  // This step has already been done in InitializationSegmentReceived when we
  // transferred the content into mCurrentInputBuffer.
  mCurrentInputBuffer->EvictAll();
  mInputDemuxer->NotifyDataRemoved();
  RecreateParser(true);

  // 4. Set append state to WAITING_FOR_SEGMENT.
  SetAppendState(AppendState::WAITING_FOR_SEGMENT);
  // 5. Jump to the loop top step above.
  ScheduleSegmentParserLoop();
}

void
TrackBuffersManager::OnDemuxerInitFailed(DemuxerFailureReason aFailure)
{
  MOZ_ASSERT(aFailure != DemuxerFailureReason::WAITING_FOR_DATA);
  mDemuxerInitRequest.Complete();

  RejectAppend(NS_ERROR_FAILURE, __func__);
}

RefPtr<TrackBuffersManager::CodedFrameProcessingPromise>
TrackBuffersManager::CodedFrameProcessing()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mProcessingPromise.IsEmpty());

  MediaByteRange mediaRange = mParser->MediaSegmentRange();
  if (mediaRange.IsEmpty()) {
    AppendDataToCurrentInputBuffer(mInputBuffer);
    mInputBuffer = nullptr;
  } else {
    MOZ_ASSERT(mProcessedInput >= mInputBuffer->Length());
    if (int64_t(mProcessedInput - mInputBuffer->Length()) > mediaRange.mEnd) {
      // Something is not quite right with the data appended. Refuse it.
      // This would typically happen if the previous media segment was partial
      // yet a new complete media segment was added.
      return CodedFrameProcessingPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
    }
    // The mediaRange is offset by the init segment position previously added.
    uint32_t length =
      mediaRange.mEnd - (mProcessedInput - mInputBuffer->Length());
    if (!length) {
      // We've completed our earlier media segment and no new data is to be
      // processed. This happens with some containers that can't detect that a
      // media segment is ending until a new one starts.
      RefPtr<CodedFrameProcessingPromise> p = mProcessingPromise.Ensure(__func__);
      CompleteCodedFrameProcessing();
      return p;
    }
    RefPtr<MediaByteBuffer> segment = new MediaByteBuffer;
    if (!segment->AppendElements(mInputBuffer->Elements(), length, fallible)) {
      return CodedFrameProcessingPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY, __func__);
    }
    AppendDataToCurrentInputBuffer(segment);
    mInputBuffer->RemoveElementsAt(0, length);
  }

  RefPtr<CodedFrameProcessingPromise> p = mProcessingPromise.Ensure(__func__);

  DoDemuxVideo();

  return p;
}

void
TrackBuffersManager::OnDemuxFailed(TrackType aTrack,
                                   DemuxerFailureReason aFailure)
{
  MOZ_ASSERT(OnTaskQueue());
  MSE_DEBUG("Failed to demux %s, failure:%d",
            aTrack == TrackType::kVideoTrack ? "video" : "audio", aFailure);
  switch (aFailure) {
    case DemuxerFailureReason::END_OF_STREAM:
    case DemuxerFailureReason::WAITING_FOR_DATA:
      if (aTrack == TrackType::kVideoTrack) {
        DoDemuxAudio();
      } else {
        CompleteCodedFrameProcessing();
      }
      break;
    case DemuxerFailureReason::DEMUXER_ERROR:
      RejectProcessing(NS_ERROR_FAILURE, __func__);
      break;
    case DemuxerFailureReason::CANCELED:
    case DemuxerFailureReason::SHUTDOWN:
      RejectProcessing(NS_ERROR_ABORT, __func__);
      break;
    default:
      MOZ_ASSERT(false);
      break;
  }
}

void
TrackBuffersManager::DoDemuxVideo()
{
  MOZ_ASSERT(OnTaskQueue());
  if (!HasVideo()) {
    DoDemuxAudio();
    return;
  }
  mVideoTracks.mDemuxRequest.Begin(mVideoTracks.mDemuxer->GetSamples(-1)
                             ->Then(GetTaskQueue(), __func__, this,
                                    &TrackBuffersManager::OnVideoDemuxCompleted,
                                    &TrackBuffersManager::OnVideoDemuxFailed));
}

void
TrackBuffersManager::OnVideoDemuxCompleted(RefPtr<MediaTrackDemuxer::SamplesHolder> aSamples)
{
  MOZ_ASSERT(OnTaskQueue());
  MSE_DEBUG("%d video samples demuxed", aSamples->mSamples.Length());
  mVideoTracks.mDemuxRequest.Complete();
  mVideoTracks.mQueuedSamples.AppendElements(aSamples->mSamples);
  DoDemuxAudio();
}

void
TrackBuffersManager::DoDemuxAudio()
{
  MOZ_ASSERT(OnTaskQueue());
  if (!HasAudio()) {
    CompleteCodedFrameProcessing();
    return;
  }
  mAudioTracks.mDemuxRequest.Begin(mAudioTracks.mDemuxer->GetSamples(-1)
                             ->Then(GetTaskQueue(), __func__, this,
                                    &TrackBuffersManager::OnAudioDemuxCompleted,
                                    &TrackBuffersManager::OnAudioDemuxFailed));
}

void
TrackBuffersManager::OnAudioDemuxCompleted(RefPtr<MediaTrackDemuxer::SamplesHolder> aSamples)
{
  MOZ_ASSERT(OnTaskQueue());
  MSE_DEBUG("%d audio samples demuxed", aSamples->mSamples.Length());
  mAudioTracks.mDemuxRequest.Complete();
  mAudioTracks.mQueuedSamples.AppendElements(aSamples->mSamples);
  CompleteCodedFrameProcessing();
}

void
TrackBuffersManager::CompleteCodedFrameProcessing()
{
  MOZ_ASSERT(OnTaskQueue());

  // 1. For each coded frame in the media segment run the following steps:
  // Coded Frame Processing steps 1.1 to 1.21.
  ProcessFrames(mVideoTracks.mQueuedSamples, mVideoTracks);
  mVideoTracks.mQueuedSamples.Clear();

#if defined(DEBUG)
  if (HasVideo()) {
    const auto& track = mVideoTracks.mBuffers.LastElement();
    MOZ_ASSERT(track.IsEmpty() || track[0]->mKeyframe);
    for (uint32_t i = 1; i < track.Length(); i++) {
      MOZ_ASSERT((track[i-1]->mTrackInfo->GetID() == track[i]->mTrackInfo->GetID() && track[i-1]->mTimecode <= track[i]->mTimecode) ||
                 track[i]->mKeyframe);
    }
  }
#endif

  ProcessFrames(mAudioTracks.mQueuedSamples, mAudioTracks);
  mAudioTracks.mQueuedSamples.Clear();

#if defined(DEBUG)
  if (HasAudio()) {
    const auto& track = mAudioTracks.mBuffers.LastElement();
    MOZ_ASSERT(track.IsEmpty() || track[0]->mKeyframe);
    for (uint32_t i = 1; i < track.Length(); i++) {
      MOZ_ASSERT((track[i-1]->mTrackInfo->GetID() == track[i]->mTrackInfo->GetID() && track[i-1]->mTimecode <= track[i]->mTimecode) ||
                 track[i]->mKeyframe);
    }
  }
#endif

  UpdateBufferedRanges();

  // Update our reported total size.
  mSizeSourceBuffer = mVideoTracks.mSizeBuffer + mAudioTracks.mSizeBuffer;

  // Return to step 6.4 of Segment Parser Loop algorithm
  // 4. If this SourceBuffer is full and cannot accept more media data, then set the buffer full flag to true.
  if (mSizeSourceBuffer >= EvictionThreshold()) {
    mBufferFull = true;
    mEvictionOccurred = false;
  }

  // 5. If the input buffer does not contain a complete media segment, then jump to the need more data step below.
  if (mParser->MediaSegmentRange().IsEmpty()) {
    ResolveProcessing(true, __func__);
    return;
  }

  mLastParsedEndTime = Some(std::max(mAudioTracks.mLastParsedEndTime,
                                     mVideoTracks.mLastParsedEndTime));

  // 6. Remove the media segment bytes from the beginning of the input buffer.
  // Clear our demuxer from any already processed data.
  // As we have handled a complete media segment, it is safe to evict all data
  // from the resource.
  mCurrentInputBuffer->EvictAll();
  mInputDemuxer->NotifyDataRemoved();
  RecreateParser(true);

  // 7. Set append state to WAITING_FOR_SEGMENT.
  SetAppendState(AppendState::WAITING_FOR_SEGMENT);

  // 8. Jump to the loop top step above.
  ResolveProcessing(false, __func__);
}

void
TrackBuffersManager::RejectProcessing(nsresult aRejectValue, const char* aName)
{
  mProcessingPromise.RejectIfExists(aRejectValue, __func__);
}

void
TrackBuffersManager::ResolveProcessing(bool aResolveValue, const char* aName)
{
  mProcessingPromise.ResolveIfExists(aResolveValue, __func__);
}

void
TrackBuffersManager::CheckSequenceDiscontinuity(const TimeUnit& aPresentationTime)
{
  if (mSourceBufferAttributes->GetAppendMode() == SourceBufferAppendMode::Sequence &&
      mSourceBufferAttributes->HaveGroupStartTimestamp()) {
    mSourceBufferAttributes->SetTimestampOffset(
      mSourceBufferAttributes->GetGroupStartTimestamp() - aPresentationTime);
    mSourceBufferAttributes->SetGroupEndTimestamp(
      mSourceBufferAttributes->GetGroupStartTimestamp());
    mVideoTracks.mNeedRandomAccessPoint = true;
    mAudioTracks.mNeedRandomAccessPoint = true;
    mSourceBufferAttributes->ResetGroupStartTimestamp();
  }
}

void
TrackBuffersManager::ProcessFrames(TrackBuffer& aSamples, TrackData& aTrackData)
{
  if (!aSamples.Length()) {
    return;
  }

  // 1. If generate timestamps flag equals true
  // Let presentation timestamp equal 0.
  // Otherwise
  // Let presentation timestamp be a double precision floating point representation of the coded frame's presentation timestamp in seconds.
  TimeUnit presentationTimestamp = mSourceBufferAttributes->mGenerateTimestamps
    ? TimeUnit() : TimeUnit::FromMicroseconds(aSamples[0]->mTime);

  // 3. If mode equals "sequence" and group start timestamp is set, then run the following steps:
  CheckSequenceDiscontinuity(presentationTimestamp);

  // 5. Let track buffer equal the track buffer that the coded frame will be added to.
  auto& trackBuffer = aTrackData;

  // Some videos do not exactly start at 0, but instead a small negative value.
  // To avoid evicting the starting frame of those videos, we allow a leeway
  // of +- mLongestFrameDuration on the append window start.
  // We only apply the leeway with the default append window start of 0
  // otherwise do as per spec.
  TimeInterval targetWindow = mAppendWindow.mStart != TimeUnit::FromSeconds(0)
    ? mAppendWindow
    : TimeInterval(mAppendWindow.mStart, mAppendWindow.mEnd,
                   trackBuffer.mLastFrameDuration.isSome()
                     ? trackBuffer.mLongestFrameDuration
                     : TimeUnit::FromMicroseconds(aSamples[0]->mDuration));

  TimeIntervals samplesRange;
  uint32_t sizeNewSamples = 0;
  TrackBuffer samples; // array that will contain the frames to be added
                       // to our track buffer.

  // We assume that no frames are contiguous within a media segment and as such
  // don't need to check for discontinuity except for the first frame and should
  // a frame be ignored due to the target window.
  bool needDiscontinuityCheck = true;

  if (aSamples.Length()) {
    aTrackData.mLastParsedEndTime = TimeUnit();
  }

  for (auto& sample : aSamples) {
    SAMPLE_DEBUG("Processing %s frame(pts:%lld end:%lld, dts:%lld, duration:%lld, "
               "kf:%d)",
               aTrackData.mInfo->mMimeType.get(),
               sample->mTime,
               sample->GetEndTime(),
               sample->mTimecode,
               sample->mDuration,
               sample->mKeyframe);

    const TimeUnit sampleEndTime =
      TimeUnit::FromMicroseconds(sample->GetEndTime());
    if (sampleEndTime > aTrackData.mLastParsedEndTime) {
      aTrackData.mLastParsedEndTime = sampleEndTime;
    }

    // We perform step 10 right away as we can't do anything should a keyframe
    // be needed until we have one.

    // 10. If the need random access point flag on track buffer equals true, then run the following steps:
    if (trackBuffer.mNeedRandomAccessPoint) {
      // 1. If the coded frame is not a random access point, then drop the coded frame and jump to the top of the loop to start processing the next coded frame.
      if (!sample->mKeyframe) {
        continue;
      }
      // 2. Set the need random access point flag on track buffer to false.
      trackBuffer.mNeedRandomAccessPoint = false;
    }

    // We perform step 1,2 and 4 at once:
    // 1. If generate timestamps flag equals true:
    //   Let presentation timestamp equal 0.
    //   Let decode timestamp equal 0.
    // Otherwise:
    //   Let presentation timestamp be a double precision floating point representation of the coded frame's presentation timestamp in seconds.
    //   Let decode timestamp be a double precision floating point representation of the coded frame's decode timestamp in seconds.

    // 2. Let frame duration be a double precision floating point representation of the coded frame's duration in seconds.
    // Step 3 is performed earlier or when a discontinuity has been detected.
    // 4. If timestampOffset is not 0, then run the following steps:

    TimeUnit sampleTime = TimeUnit::FromMicroseconds(sample->mTime);
    TimeUnit sampleTimecode = TimeUnit::FromMicroseconds(sample->mTimecode);
    TimeUnit sampleDuration = TimeUnit::FromMicroseconds(sample->mDuration);
    TimeUnit timestampOffset = mSourceBufferAttributes->GetTimestampOffset();

    TimeInterval sampleInterval =
      mSourceBufferAttributes->mGenerateTimestamps
        ? TimeInterval(timestampOffset, timestampOffset + sampleDuration)
        : TimeInterval(timestampOffset + sampleTime,
                       timestampOffset + sampleTime + sampleDuration);
    TimeUnit decodeTimestamp =
      mSourceBufferAttributes->mGenerateTimestamps
        ? timestampOffset
        : timestampOffset + sampleTimecode;

    // 6. If last decode timestamp for track buffer is set and decode timestamp is less than last decode timestamp:
    // OR
    // If last decode timestamp for track buffer is set and the difference between decode timestamp and last decode timestamp is greater than 2 times last frame duration:

    if (needDiscontinuityCheck && trackBuffer.mLastDecodeTimestamp.isSome() &&
        (decodeTimestamp < trackBuffer.mLastDecodeTimestamp.ref() ||
         (decodeTimestamp - trackBuffer.mLastDecodeTimestamp.ref()
          > 2 * trackBuffer.mLongestFrameDuration))) {
      MSE_DEBUG("Discontinuity detected.");
      SourceBufferAppendMode appendMode = mSourceBufferAttributes->GetAppendMode();

      // 1a. If mode equals "segments":
      if (appendMode == SourceBufferAppendMode::Segments) {
        // Set group end timestamp to presentation timestamp.
        mSourceBufferAttributes->SetGroupEndTimestamp(sampleInterval.mStart);
      }
      // 1b. If mode equals "sequence":
      if (appendMode == SourceBufferAppendMode::Sequence) {
        // Set group start timestamp equal to the group end timestamp.
        mSourceBufferAttributes->SetGroupStartTimestamp(
          mSourceBufferAttributes->GetGroupEndTimestamp());
      }
      for (auto& track : GetTracksList()) {
        // 2. Unset the last decode timestamp on all track buffers.
        // 3. Unset the last frame duration on all track buffers.
        // 4. Unset the highest end timestamp on all track buffers.
        // 5. Set the need random access point flag on all track buffers to true.
        track->ResetAppendState();
      }
      // 6. Jump to the Loop Top step above to restart processing of the current coded frame.
      // Rather that restarting the process for the frame, we run the first
      // steps again instead.
      // 3. If mode equals "sequence" and group start timestamp is set, then run the following steps:
      TimeUnit presentationTimestamp = mSourceBufferAttributes->mGenerateTimestamps
        ? TimeUnit() : sampleTime;
      CheckSequenceDiscontinuity(presentationTimestamp);

      if (!sample->mKeyframe) {
        continue;
      }
      if (appendMode == SourceBufferAppendMode::Sequence) {
        // mSourceBufferAttributes->GetTimestampOffset() was modified during CheckSequenceDiscontinuity.
        // We need to update our variables.
        timestampOffset = mSourceBufferAttributes->GetTimestampOffset();
        sampleInterval =
          mSourceBufferAttributes->mGenerateTimestamps
            ? TimeInterval(timestampOffset, timestampOffset + sampleDuration)
            : TimeInterval(timestampOffset + sampleTime,
                           timestampOffset + sampleTime + sampleDuration);
        decodeTimestamp =
          mSourceBufferAttributes->mGenerateTimestamps
            ? timestampOffset
            : timestampOffset + sampleTimecode;
      }
      trackBuffer.mNeedRandomAccessPoint = false;
      needDiscontinuityCheck = false;
    }

    // 7. Let frame end timestamp equal the sum of presentation timestamp and frame duration.
    // This is sampleInterval.mEnd

    // 8. If presentation timestamp is less than appendWindowStart, then set the need random access point flag to true, drop the coded frame, and jump to the top of the loop to start processing the next coded frame.
    // 9. If frame end timestamp is greater than appendWindowEnd, then set the need random access point flag to true, drop the coded frame, and jump to the top of the loop to start processing the next coded frame.
    if (!targetWindow.ContainsWithStrictEnd(sampleInterval)) {
      if (samples.Length()) {
        // We are creating a discontinuity in the samples.
        // Insert the samples processed so far.
        InsertFrames(samples, samplesRange, trackBuffer);
        samples.Clear();
        samplesRange = TimeIntervals();
        trackBuffer.mSizeBuffer += sizeNewSamples;
        sizeNewSamples = 0;
      }
      trackBuffer.mNeedRandomAccessPoint = true;
      needDiscontinuityCheck = true;
      continue;
    }

    samplesRange += sampleInterval;
    sizeNewSamples += sample->ComputedSizeOfIncludingThis();
    sample->mTime = sampleInterval.mStart.ToMicroseconds();
    sample->mTimecode = decodeTimestamp.ToMicroseconds();
    sample->mTrackInfo = trackBuffer.mLastInfo;
    samples.AppendElement(sample);

    // Steps 11,12,13,14, 15 and 16 will be done in one block in InsertFrames.

    trackBuffer.mLongestFrameDuration =
      trackBuffer.mLastFrameDuration.isSome()
      ? sample->mKeyframe
        ? sampleDuration
        : std::max(sampleDuration, trackBuffer.mLongestFrameDuration)
      : sampleDuration;

    // 17. Set last decode timestamp for track buffer to decode timestamp.
    trackBuffer.mLastDecodeTimestamp = Some(decodeTimestamp);
    // 18. Set last frame duration for track buffer to frame duration.
    trackBuffer.mLastFrameDuration = Some(sampleDuration);

    // 19. If highest end timestamp for track buffer is unset or frame end timestamp is greater than highest end timestamp, then set highest end timestamp for track buffer to frame end timestamp.
    if (trackBuffer.mHighestEndTimestamp.isNothing() ||
        sampleInterval.mEnd > trackBuffer.mHighestEndTimestamp.ref()) {
      trackBuffer.mHighestEndTimestamp = Some(sampleInterval.mEnd);
    }
    // 20. If frame end timestamp is greater than group end timestamp, then set group end timestamp equal to frame end timestamp.
    if (sampleInterval.mEnd > mSourceBufferAttributes->GetGroupEndTimestamp()) {
      mSourceBufferAttributes->SetGroupEndTimestamp(sampleInterval.mEnd);
    }
    // 21. If generate timestamps flag equals true, then set timestampOffset equal to frame end timestamp.
    if (mSourceBufferAttributes->mGenerateTimestamps) {
      mSourceBufferAttributes->SetTimestampOffset(sampleInterval.mEnd);
    }
  }

  if (samples.Length()) {
    InsertFrames(samples, samplesRange, trackBuffer);
    trackBuffer.mSizeBuffer += sizeNewSamples;
  }
}

bool
TrackBuffersManager::CheckNextInsertionIndex(TrackData& aTrackData,
                                             const TimeUnit& aSampleTime)
{
  if (aTrackData.mNextInsertionIndex.isSome()) {
    return true;
  }

  TrackBuffer& data = aTrackData.mBuffers.LastElement();

  if (data.IsEmpty() || aSampleTime < aTrackData.mBufferedRanges.GetStart()) {
    aTrackData.mNextInsertionIndex = Some(size_t(0));
    return true;
  }

  // Find which discontinuity we should insert the frame before.
  TimeInterval target;
  for (const auto& interval : aTrackData.mBufferedRanges) {
    if (aSampleTime < interval.mStart) {
      target = interval;
      break;
    }
  }
  if (target.IsEmpty()) {
    // No target found, it will be added at the end of the track buffer.
    aTrackData.mNextInsertionIndex = Some(data.Length());
    return true;
  }
  // We now need to find the first frame of the searched interval.
  // We will insert our new frames right before.
  for (uint32_t i = 0; i < data.Length(); i++) {
    const RefPtr<MediaRawData>& sample = data[i];
    if (sample->mTime >= target.mStart.ToMicroseconds() ||
        sample->GetEndTime() > target.mStart.ToMicroseconds()) {
      aTrackData.mNextInsertionIndex = Some(size_t(i));
      return true;
    }
  }
  NS_ASSERTION(false, "Insertion Index Not Found");
  return false;
}

void
TrackBuffersManager::InsertFrames(TrackBuffer& aSamples,
                                  const TimeIntervals& aIntervals,
                                  TrackData& aTrackData)
{
  // 5. Let track buffer equal the track buffer that the coded frame will be added to.
  auto& trackBuffer = aTrackData;

  MSE_DEBUGV("Processing %d %s frames(start:%lld end:%lld)",
             aSamples.Length(),
             aTrackData.mInfo->mMimeType.get(),
             aIntervals.GetStart().ToMicroseconds(),
             aIntervals.GetEnd().ToMicroseconds());

  // TODO: Handle splicing of audio (and text) frames.
  // 11. Let spliced audio frame be an unset variable for holding audio splice information
  // 12. Let spliced timed text frame be an unset variable for holding timed text splice information

  // 13. If last decode timestamp for track buffer is unset and presentation timestamp falls within the presentation interval of a coded frame in track buffer,then run the following steps:
  // For now we only handle replacing existing frames with the new ones. So we
  // skip this step.

  // 14. Remove existing coded frames in track buffer:
  //   a) If highest end timestamp for track buffer is not set:
  //      Remove all coded frames from track buffer that have a presentation timestamp greater than or equal to presentation timestamp and less than frame end timestamp.
  //   b) If highest end timestamp for track buffer is set and less than or equal to presentation timestamp:
  //      Remove all coded frames from track buffer that have a presentation timestamp greater than or equal to highest end timestamp and less than frame end timestamp

  // There is an ambiguity on how to remove frames, which was lodged with:
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=28710, implementing as per
  // bug description.

  // 15. Remove decoding dependencies of the coded frames removed in the previous step:
  // Remove all coded frames between the coded frames removed in the previous step and the next random access point after those removed frames.

  TimeIntervals intersection = trackBuffer.mBufferedRanges;
  intersection.Intersection(aIntervals);

  if (intersection.Length()) {
    RemoveFrames(aIntervals, trackBuffer, trackBuffer.mNextInsertionIndex.refOr(0));
  }

  // 16. Add the coded frame with the presentation timestamp, decode timestamp, and frame duration to the track buffer.
  if (!CheckNextInsertionIndex(aTrackData,
                               TimeUnit::FromMicroseconds(aSamples[0]->mTime))) {
    RejectProcessing(NS_ERROR_FAILURE, __func__);
    return;
  }

  // Adjust our demuxing index if necessary.
  if (trackBuffer.mNextGetSampleIndex.isSome()) {
    if (trackBuffer.mNextInsertionIndex.ref() == trackBuffer.mNextGetSampleIndex.ref() &&
        aIntervals.GetEnd() >= trackBuffer.mNextSampleTime) {
      MSE_DEBUG("Next sample to be played got overwritten");
      trackBuffer.mNextGetSampleIndex.reset();
    } else if (trackBuffer.mNextInsertionIndex.ref() <= trackBuffer.mNextGetSampleIndex.ref()) {
      trackBuffer.mNextGetSampleIndex.ref() += aSamples.Length();
    }
  }

  TrackBuffer& data = trackBuffer.mBuffers.LastElement();
  data.InsertElementsAt(trackBuffer.mNextInsertionIndex.ref(), aSamples);
  trackBuffer.mNextInsertionIndex.ref() += aSamples.Length();

  // Update our buffered range with new sample interval.
  trackBuffer.mBufferedRanges += aIntervals;
  // We allow a fuzz factor in our interval of half a frame length,
  // as fuzz is +/- value, giving an effective leeway of a full frame
  // length.
  TimeIntervals range(aIntervals);
  range.SetFuzz(trackBuffer.mLongestFrameDuration / 2);
  trackBuffer.mSanitizedBufferedRanges += range;
}

void
TrackBuffersManager::RemoveFrames(const TimeIntervals& aIntervals,
                                  TrackData& aTrackData,
                                  uint32_t aStartIndex)
{
  TrackBuffer& data = aTrackData.mBuffers.LastElement();
  Maybe<uint32_t> firstRemovedIndex;
  uint32_t lastRemovedIndex = 0;

  // We loop from aStartIndex to avoid removing frames that we inserted earlier
  // and part of the current coded frame group. This is allows to handle step
  // 14 of the coded frame processing algorithm without having to check the value
  // of highest end timestamp:
  // "Remove existing coded frames in track buffer:
  //  If highest end timestamp for track buffer is not set:
  //   Remove all coded frames from track buffer that have a presentation timestamp greater than or equal to presentation timestamp and less than frame end timestamp.
  //  If highest end timestamp for track buffer is set and less than or equal to presentation timestamp:
  //   Remove all coded frames from track buffer that have a presentation timestamp greater than or equal to highest end timestamp and less than frame end timestamp"
  for (uint32_t i = aStartIndex; i < data.Length(); i++) {
    MediaRawData* sample = data[i].get();
    TimeInterval sampleInterval =
      TimeInterval(TimeUnit::FromMicroseconds(sample->mTime),
                   TimeUnit::FromMicroseconds(sample->GetEndTime()));
    if (aIntervals.Contains(sampleInterval)) {
      if (firstRemovedIndex.isNothing()) {
        firstRemovedIndex = Some(i);
      }
      lastRemovedIndex = i;
    }
  }

  if (firstRemovedIndex.isNothing()) {
    return;
  }

  // Remove decoding dependencies of the coded frames removed in the previous step:
  // Remove all coded frames between the coded frames removed in the previous step and the next random access point after those removed frames.
  for (uint32_t i = lastRemovedIndex + 1; i < data.Length(); i++) {
    MediaRawData* sample = data[i].get();
    if (sample->mKeyframe) {
      break;
    }
    lastRemovedIndex = i;
  }

  int64_t maxSampleDuration = 0;
  TimeIntervals removedIntervals;
  for (uint32_t i = firstRemovedIndex.ref(); i <= lastRemovedIndex; i++) {
    MediaRawData* sample = data[i].get();
    TimeInterval sampleInterval =
      TimeInterval(TimeUnit::FromMicroseconds(sample->mTime),
                   TimeUnit::FromMicroseconds(sample->GetEndTime()));
    removedIntervals += sampleInterval;
    if (sample->mDuration > maxSampleDuration) {
      maxSampleDuration = sample->mDuration;
    }
    aTrackData.mSizeBuffer -= sample->ComputedSizeOfIncludingThis();
  }

  MSE_DEBUG("Removing frames from:%u (frames:%u) ([%f, %f))",
            firstRemovedIndex.ref(),
            lastRemovedIndex - firstRemovedIndex.ref() + 1,
            removedIntervals.GetStart().ToSeconds(),
            removedIntervals.GetEnd().ToSeconds());

  if (aTrackData.mNextGetSampleIndex.isSome()) {
    if (aTrackData.mNextGetSampleIndex.ref() >= firstRemovedIndex.ref() &&
        aTrackData.mNextGetSampleIndex.ref() <= lastRemovedIndex) {
      MSE_DEBUG("Next sample to be played got evicted");
      aTrackData.mNextGetSampleIndex.reset();
    } else if (aTrackData.mNextGetSampleIndex.ref() > lastRemovedIndex) {
      aTrackData.mNextGetSampleIndex.ref() -=
        lastRemovedIndex - firstRemovedIndex.ref() + 1;
    }
  }

  if (aTrackData.mNextInsertionIndex.isSome()) {
    if (aTrackData.mNextInsertionIndex.ref() > firstRemovedIndex.ref() &&
        aTrackData.mNextInsertionIndex.ref() <= lastRemovedIndex + 1) {
      aTrackData.ResetAppendState();
      MSE_DEBUG("NextInsertionIndex got reset.");
    } else if (aTrackData.mNextInsertionIndex.ref() > lastRemovedIndex + 1) {
      aTrackData.mNextInsertionIndex.ref() -=
        lastRemovedIndex - firstRemovedIndex.ref() + 1;
    }
  }

  // Update our buffered range to exclude the range just removed.
  aTrackData.mBufferedRanges -= removedIntervals;

  // Recalculate sanitized buffered ranges.
  aTrackData.mSanitizedBufferedRanges = aTrackData.mBufferedRanges;
  aTrackData.mSanitizedBufferedRanges.SetFuzz(TimeUnit::FromMicroseconds(maxSampleDuration/2));

  data.RemoveElementsAt(firstRemovedIndex.ref(),
                        lastRemovedIndex - firstRemovedIndex.ref() + 1);
}

void
TrackBuffersManager::RecreateParser(bool aReuseInitData)
{
  MOZ_ASSERT(OnTaskQueue());
  // Recreate our parser for only the data remaining. This is required
  // as it has parsed the entire InputBuffer provided.
  // Once the old TrackBuffer/MediaSource implementation is removed
  // we can optimize this part. TODO
  mParser = ContainerParser::CreateForMIMEType(mType);
  if (aReuseInitData && mInitData) {
    int64_t start, end;
    mParser->ParseStartAndEndTimestamps(mInitData, start, end);
    mProcessedInput = mInitData->Length();
  } else {
    mProcessedInput = 0;
  }
}

nsTArray<TrackBuffersManager::TrackData*>
TrackBuffersManager::GetTracksList()
{
  MOZ_ASSERT(OnTaskQueue());
  nsTArray<TrackData*> tracks;
  if (HasVideo()) {
    tracks.AppendElement(&mVideoTracks);
  }
  if (HasAudio()) {
    tracks.AppendElement(&mAudioTracks);
  }
  return tracks;
}

void
TrackBuffersManager::SetAppendState(SourceBufferAttributes::AppendState aAppendState)
{
  MSE_DEBUG("AppendState changed from %s to %s",
            AppendStateToStr(mSourceBufferAttributes->GetAppendState()), AppendStateToStr(aAppendState));
  mSourceBufferAttributes->SetAppendState(aAppendState);
}

MediaInfo
TrackBuffersManager::GetMetadata()
{
  MonitorAutoLock mon(mMonitor);
  return mInfo;
}

const TimeIntervals&
TrackBuffersManager::Buffered(TrackInfo::TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());
  return GetTracksData(aTrack).mBufferedRanges;
}

TimeIntervals
TrackBuffersManager::SafeBuffered(TrackInfo::TrackType aTrack) const
{
  MonitorAutoLock mon(mMonitor);
  return aTrack == TrackInfo::kVideoTrack
    ? mVideoBufferedRanges
    : mAudioBufferedRanges;
}

const TrackBuffersManager::TrackBuffer&
TrackBuffersManager::GetTrackBuffer(TrackInfo::TrackType aTrack)
{
  MOZ_ASSERT(OnTaskQueue());
  return GetTracksData(aTrack).mBuffers.LastElement();
}

uint32_t TrackBuffersManager::FindSampleIndex(const TrackBuffer& aTrackBuffer,
                                              const TimeInterval& aInterval)
{
  TimeUnit target = aInterval.mStart - aInterval.mFuzz;

  for (uint32_t i = 0; i < aTrackBuffer.Length(); i++) {
    const RefPtr<MediaRawData>& sample = aTrackBuffer[i];
    if (sample->mTime >= target.ToMicroseconds() ||
        sample->GetEndTime() > target.ToMicroseconds()) {
      return i;
    }
  }
  NS_ASSERTION(false, "FindSampleIndex called with invalid arguments");

  return 0;
}

TimeUnit
TrackBuffersManager::Seek(TrackInfo::TrackType aTrack,
                          const TimeUnit& aTime,
                          const TimeUnit& aFuzz)
{
  MOZ_ASSERT(OnTaskQueue());
  auto& trackBuffer = GetTracksData(aTrack);
  const TrackBuffersManager::TrackBuffer& track = GetTrackBuffer(aTrack);

  if (!track.Length()) {
    // This a reset. It will be followed by another valid seek.
    trackBuffer.mNextGetSampleIndex = Some(uint32_t(0));
    trackBuffer.mNextSampleTimecode = TimeUnit();
    trackBuffer.mNextSampleTime = TimeUnit();
    return TimeUnit();
  }

  uint32_t i = 0;

  if (aTime != TimeUnit()) {
    // Determine the interval of samples we're attempting to seek to.
    TimeIntervals buffered = trackBuffer.mBufferedRanges;
    TimeIntervals::IndexType index = buffered.Find(aTime);
    buffered.SetFuzz(aFuzz);
    index = buffered.Find(aTime);
    MOZ_ASSERT(index != TimeIntervals::NoIndex);

    TimeInterval target = buffered[index];
    i = FindSampleIndex(track, target);
  }

  Maybe<TimeUnit> lastKeyFrameTime;
  TimeUnit lastKeyFrameTimecode;
  uint32_t lastKeyFrameIndex = 0;
  for (; i < track.Length(); i++) {
    const RefPtr<MediaRawData>& sample = track[i];
    TimeUnit sampleTime = TimeUnit::FromMicroseconds(sample->mTime);
    if (sampleTime > aTime && lastKeyFrameTime.isSome()) {
      break;
    }
    if (sample->mKeyframe) {
      lastKeyFrameTimecode = TimeUnit::FromMicroseconds(sample->mTimecode);
      lastKeyFrameTime = Some(sampleTime);
      lastKeyFrameIndex = i;
    }
    if (sampleTime == aTime ||
        (sampleTime > aTime && lastKeyFrameTime.isSome())) {
      break;
    }
  }
  MSE_DEBUG("Keyframe %s found at %lld",
            lastKeyFrameTime.isSome() ? "" : "not",
            lastKeyFrameTime.refOr(TimeUnit()).ToMicroseconds());

  trackBuffer.mNextGetSampleIndex = Some(lastKeyFrameIndex);
  trackBuffer.mNextSampleTimecode = lastKeyFrameTimecode;
  trackBuffer.mNextSampleTime = lastKeyFrameTime.refOr(TimeUnit());

  return lastKeyFrameTime.refOr(TimeUnit());
}

uint32_t
TrackBuffersManager::SkipToNextRandomAccessPoint(TrackInfo::TrackType aTrack,
                                                 const TimeUnit& aTimeThreadshold,
                                                 bool& aFound)
{
  MOZ_ASSERT(OnTaskQueue());
  uint32_t parsed = 0;
  auto& trackData = GetTracksData(aTrack);
  const TrackBuffer& track = GetTrackBuffer(aTrack);
  aFound = false;

  uint32_t nextSampleIndex = trackData.mNextGetSampleIndex.valueOr(0);
  for (uint32_t i = nextSampleIndex; i < track.Length(); i++) {
    const RefPtr<MediaRawData>& sample = track[i];
    if (sample->mKeyframe &&
        sample->mTime >= aTimeThreadshold.ToMicroseconds()) {
      trackData.mNextSampleTimecode =
        TimeUnit::FromMicroseconds(sample->mTimecode);
      trackData.mNextSampleTime =
        TimeUnit::FromMicroseconds(sample->mTime);
      trackData.mNextGetSampleIndex = Some(i);
      aFound = true;
      break;
    }
    parsed++;
  }

  return parsed;
}

const MediaRawData*
TrackBuffersManager::GetSample(TrackInfo::TrackType aTrack,
                               size_t aIndex,
                               const TimeUnit& aExpectedDts,
                               const TimeUnit& aExpectedPts,
                               const TimeUnit& aFuzz)
{
  const TrackBuffer& track = GetTrackBuffer(aTrack);

  if (aIndex >= track.Length()) {
    // reached the end.
    return nullptr;
  }

  const RefPtr<MediaRawData>& sample = track[aIndex];
  if (!aIndex || sample->mTimecode <= (aExpectedDts + aFuzz).ToMicroseconds() ||
      sample->mTime <= (aExpectedPts + aFuzz).ToMicroseconds()) {
    return sample;
  }

  // Gap is too big. End of Stream or Waiting for Data.
  // TODO, check that we have continuous data based on the sanitized buffered
  // range instead.
  return nullptr;
}

already_AddRefed<MediaRawData>
TrackBuffersManager::GetSample(TrackInfo::TrackType aTrack,
                               const TimeUnit& aFuzz,
                               bool& aError)
{
  MOZ_ASSERT(OnTaskQueue());
  auto& trackData = GetTracksData(aTrack);
  const TrackBuffer& track = GetTrackBuffer(aTrack);

  aError = false;

  if (!track.Length()) {
    return nullptr;
  }
  if (trackData.mNextGetSampleIndex.isNothing() &&
      trackData.mNextSampleTimecode == TimeUnit()) {
    // First demux, get first sample.
    trackData.mNextGetSampleIndex = Some(0u);
  }

  if (trackData.mNextGetSampleIndex.isSome()) {
    const MediaRawData* sample =
      GetSample(aTrack,
                trackData.mNextGetSampleIndex.ref(),
                trackData.mNextSampleTimecode,
                trackData.mNextSampleTime,
                aFuzz);
    if (!sample) {
      return nullptr;
    }

    RefPtr<MediaRawData> p = sample->Clone();
    if (!p) {
      aError = true;
      return nullptr;
    }
    trackData.mNextGetSampleIndex.ref()++;
    // Estimate decode timestamp of the next sample.
    trackData.mNextSampleTimecode =
      TimeUnit::FromMicroseconds(sample->mTimecode + sample->mDuration);
    trackData.mNextSampleTime =
      TimeUnit::FromMicroseconds(sample->GetEndTime());
    return p.forget();
  }

  // Our previous index has been overwritten, attempt to find the new one.
  for (uint32_t i = 0; i < track.Length(); i++) {
    const RefPtr<MediaRawData>& sample = track[i];
    TimeInterval sampleInterval{
      TimeUnit::FromMicroseconds(sample->mTimecode),
      TimeUnit::FromMicroseconds(sample->mTimecode + sample->mDuration),
      aFuzz};

    if (sampleInterval.ContainsWithStrictEnd(trackData.mNextSampleTimecode)) {
      RefPtr<MediaRawData> p = sample->Clone();
      if (!p) {
        // OOM
        aError = true;
        return nullptr;
      }
      trackData.mNextGetSampleIndex = Some(i+1);
      trackData.mNextSampleTimecode = sampleInterval.mEnd;
      trackData.mNextSampleTime =
        TimeUnit::FromMicroseconds(sample->GetEndTime());
      return p.forget();
    }
  }

  // We couldn't find our sample by decode timestamp. Attempt to find it using
  // presentation timestamp. There will likely be small jerkiness.
    for (uint32_t i = 0; i < track.Length(); i++) {
    const RefPtr<MediaRawData>& sample = track[i];
    TimeInterval sampleInterval{
      TimeUnit::FromMicroseconds(sample->mTime),
      TimeUnit::FromMicroseconds(sample->GetEndTime()),
      aFuzz};

    if (sampleInterval.ContainsWithStrictEnd(trackData.mNextSampleTimecode)) {
      RefPtr<MediaRawData> p = sample->Clone();
      if (!p) {
        // OOM
        aError = true;
        return nullptr;
      }
      trackData.mNextGetSampleIndex = Some(i+1);
      // Estimate decode timestamp of the next sample.
      trackData.mNextSampleTimecode = sampleInterval.mEnd;
      trackData.mNextSampleTime =
        TimeUnit::FromMicroseconds(sample->GetEndTime());
      return p.forget();
    }
  }

  MSE_DEBUG("Couldn't find sample (pts:%lld dts:%lld)",
             trackData.mNextSampleTime.ToMicroseconds(),
            trackData.mNextSampleTimecode.ToMicroseconds());
  return nullptr;
}

TimeUnit
TrackBuffersManager::GetNextRandomAccessPoint(TrackInfo::TrackType aTrack,
                                              const TimeUnit& aFuzz)
{
  auto& trackData = GetTracksData(aTrack);
  MOZ_ASSERT(trackData.mNextGetSampleIndex.isSome());
  const TrackBuffersManager::TrackBuffer& track = GetTrackBuffer(aTrack);

  uint32_t i = trackData.mNextGetSampleIndex.ref();
  TimeUnit nextSampleTimecode = trackData.mNextSampleTimecode;
  TimeUnit nextSampleTime = trackData.mNextSampleTime;

  for (; i < track.Length(); i++) {
    const MediaRawData* sample =
      GetSample(aTrack, i, nextSampleTimecode, nextSampleTime, aFuzz);
    if (!sample) {
      break;
    }
    if (sample->mKeyframe) {
      return TimeUnit::FromMicroseconds(sample->mTime);
    }
    nextSampleTimecode =
      TimeUnit::FromMicroseconds(sample->mTimecode + sample->mDuration);
    nextSampleTime = TimeUnit::FromMicroseconds(sample->GetEndTime());
  }
  return TimeUnit::FromInfinity();
}

void
TrackBuffersManager::TrackData::AddSizeOfResources(MediaSourceDecoder::ResourceSizes* aSizes)
{
  for (TrackBuffer& buffer : mBuffers) {
    for (MediaRawData* data : buffer) {
      aSizes->mByteSize += data->SizeOfIncludingThis(aSizes->mMallocSizeOf);
    }
  }
}

void
TrackBuffersManager::AddSizeOfResources(MediaSourceDecoder::ResourceSizes* aSizes)
{
  MOZ_ASSERT(OnTaskQueue());
  mVideoTracks.AddSizeOfResources(aSizes);
  mAudioTracks.AddSizeOfResources(aSizes);
}

} // namespace mozilla
#undef MSE_DEBUG
#undef MSE_DEBUGV
#undef SAMPLE_DEBUG
