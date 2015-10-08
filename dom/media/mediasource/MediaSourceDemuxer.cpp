/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <limits>
#include <stdint.h>

#include "MediaSourceDemuxer.h"
#include "MediaSourceUtils.h"
#include "SourceBufferList.h"
#include "nsPrintfCString.h"

namespace mozilla {

typedef TrackInfo::TrackType TrackType;
using media::TimeUnit;
using media::TimeIntervals;

// Gap allowed between frames. Due to inaccuracies in determining buffer end
// frames (Bug 1065207). This value is based on the end of frame
// default value used in Blink, kDefaultBufferDurationInMs.
#define EOS_FUZZ_US 125000

MediaSourceDemuxer::MediaSourceDemuxer()
  : mTaskQueue(new TaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK),
                             /* aSupportsTailDispatch = */ true))
  , mMonitor("MediaSourceDemuxer")
{
  MOZ_ASSERT(NS_IsMainThread());
}

nsRefPtr<MediaSourceDemuxer::InitPromise>
MediaSourceDemuxer::Init()
{
  return InvokeAsync(GetTaskQueue(), this, __func__,
                     &MediaSourceDemuxer::AttemptInit);
}

nsRefPtr<MediaSourceDemuxer::InitPromise>
MediaSourceDemuxer::AttemptInit()
{
  MOZ_ASSERT(OnTaskQueue());

  if (ScanSourceBuffersForContent()) {
    return InitPromise::CreateAndResolve(NS_OK, __func__);
  }

  nsRefPtr<InitPromise> p = mInitPromise.Ensure(__func__);

  return p;
}

void MediaSourceDemuxer::NotifyDataArrived()
{
  nsRefPtr<MediaSourceDemuxer> self = this;
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableFunction([self] () {
      if (self->mInitPromise.IsEmpty()) {
        return;
      }
      if (self->ScanSourceBuffersForContent()) {
        self->mInitPromise.ResolveIfExists(NS_OK, __func__);
      }
    });
  GetTaskQueue()->Dispatch(task.forget());
}

bool
MediaSourceDemuxer::ScanSourceBuffersForContent()
{
  MOZ_ASSERT(OnTaskQueue());

  if (mSourceBuffers.IsEmpty()) {
    return false;
  }

  MonitorAutoLock mon(mMonitor);

  bool haveEmptySourceBuffer = false;
  for (const auto& sourceBuffer : mSourceBuffers) {
    MediaInfo info = sourceBuffer->GetMetadata();
    if (!info.HasAudio() && !info.HasVideo()) {
      haveEmptySourceBuffer = true;
    }
    if (info.HasAudio() && !mAudioTrack) {
      mInfo.mAudio = info.mAudio;
      mAudioTrack = sourceBuffer;
    }
    if (info.HasVideo() && !mVideoTrack) {
      mInfo.mVideo = info.mVideo;
      mVideoTrack = sourceBuffer;
    }
    if (info.IsEncrypted() && !mInfo.IsEncrypted()) {
      mInfo.mCrypto = info.mCrypto;
    }
  }
  if (mInfo.HasAudio() && mInfo.HasVideo()) {
    // We have both audio and video. We can ignore non-ready source buffer.
    return true;
  }
  return !haveEmptySourceBuffer;
}

bool
MediaSourceDemuxer::HasTrackType(TrackType aType) const
{
  MonitorAutoLock mon(mMonitor);

  switch (aType) {
    case TrackType::kAudioTrack:
      return mInfo.HasAudio();
    case TrackType::kVideoTrack:
      return mInfo.HasVideo();
    default:
      return false;
  }
}

uint32_t
MediaSourceDemuxer::GetNumberTracks(TrackType aType) const
{
  return HasTrackType(aType) ? 1u : 0;
}

already_AddRefed<MediaTrackDemuxer>
MediaSourceDemuxer::GetTrackDemuxer(TrackType aType, uint32_t aTrackNumber)
{
  nsRefPtr<TrackBuffersManager> manager = GetManager(aType);
  if (!manager) {
    MOZ_CRASH("TODO: sourcebuffer was deleted from under us");
    return nullptr;
  }
  nsRefPtr<MediaSourceTrackDemuxer> e =
    new MediaSourceTrackDemuxer(this, aType, manager);
  mDemuxers.AppendElement(e);
  return e.forget();
}

bool
MediaSourceDemuxer::IsSeekable() const
{
  return true;
}

UniquePtr<EncryptionInfo>
MediaSourceDemuxer::GetCrypto()
{
  MonitorAutoLock mon(mMonitor);
  auto crypto = MakeUnique<EncryptionInfo>();
  *crypto = mInfo.mCrypto;
  return crypto;
}

void
MediaSourceDemuxer::AttachSourceBuffer(TrackBuffersManager* aSourceBuffer)
{
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArg<TrackBuffersManager*>(
      this, &MediaSourceDemuxer::DoAttachSourceBuffer,
      aSourceBuffer);
  GetTaskQueue()->Dispatch(task.forget());
}

void
MediaSourceDemuxer::DoAttachSourceBuffer(mozilla::TrackBuffersManager* aSourceBuffer)
{
  MOZ_ASSERT(OnTaskQueue());
  mSourceBuffers.AppendElement(aSourceBuffer);
  ScanSourceBuffersForContent();
}

void
MediaSourceDemuxer::DetachSourceBuffer(TrackBuffersManager* aSourceBuffer)
{
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArg<TrackBuffersManager*>(
      this, &MediaSourceDemuxer::DoDetachSourceBuffer,
      aSourceBuffer);
  GetTaskQueue()->Dispatch(task.forget());
}

void
MediaSourceDemuxer::DoDetachSourceBuffer(TrackBuffersManager* aSourceBuffer)
{
  MOZ_ASSERT(OnTaskQueue());
  for (uint32_t i = 0; i < mSourceBuffers.Length(); i++) {
    if (mSourceBuffers[i].get() == aSourceBuffer) {
      mSourceBuffers.RemoveElementAt(i);
    }
  }
  if (aSourceBuffer == mAudioTrack) {
    mAudioTrack = nullptr;
  }
  if (aSourceBuffer == mVideoTrack) {
    mVideoTrack = nullptr;
  }
  ScanSourceBuffersForContent();
}

TrackInfo*
MediaSourceDemuxer::GetTrackInfo(TrackType aTrack)
{
  MonitorAutoLock mon(mMonitor);
  switch (aTrack) {
    case TrackType::kAudioTrack:
      return &mInfo.mAudio;
    case TrackType::kVideoTrack:
      return &mInfo.mVideo;
    default:
      return nullptr;
  }
}

TrackBuffersManager*
MediaSourceDemuxer::GetManager(TrackType aTrack)
{
  MonitorAutoLock mon(mMonitor);
  switch (aTrack) {
    case TrackType::kAudioTrack:
      return mAudioTrack;
    case TrackType::kVideoTrack:
      return mVideoTrack;
    default:
      return nullptr;
  }
}

MediaSourceDemuxer::~MediaSourceDemuxer()
{
  mInitPromise.RejectIfExists(DemuxerFailureReason::SHUTDOWN, __func__);
  mTaskQueue->BeginShutdown();
  mTaskQueue = nullptr;
}

void
MediaSourceDemuxer::GetMozDebugReaderData(nsAString& aString)
{
  MonitorAutoLock mon(mMonitor);
  nsAutoCString result;
  result += nsPrintfCString("Dumping data for demuxer %p:\n", this);
  if (mAudioTrack) {
    result += nsPrintfCString("\tDumping Audio Track Buffer(%s): - mLastAudioTime: %f\n"
                              "\t\tNumSamples:%u Size:%u NextGetSampleIndex:%u NextInsertionIndex:%d\n",
                              mAudioTrack->mAudioTracks.mInfo->mMimeType.get(),
                              mAudioTrack->mAudioTracks.mNextSampleTime.ToSeconds(),
                              mAudioTrack->mAudioTracks.mBuffers[0].Length(),
                              mAudioTrack->mAudioTracks.mSizeBuffer,
                              mAudioTrack->mAudioTracks.mNextGetSampleIndex.valueOr(-1),
                              mAudioTrack->mAudioTracks.mNextInsertionIndex.valueOr(-1));

    result += nsPrintfCString("\t\tBuffered: ranges=%s\n",
                              DumpTimeRanges(mAudioTrack->SafeBuffered(TrackInfo::kAudioTrack)).get());
  }
  if (mVideoTrack) {
    result += nsPrintfCString("\tDumping Video Track Buffer(%s) - mLastVideoTime: %f\n"
                              "\t\tNumSamples:%u Size:%u NextGetSampleIndex:%u NextInsertionIndex:%d\n",
                              mVideoTrack->mVideoTracks.mInfo->mMimeType.get(),
                              mVideoTrack->mVideoTracks.mNextSampleTime.ToSeconds(),
                              mVideoTrack->mVideoTracks.mBuffers[0].Length(),
                              mVideoTrack->mVideoTracks.mSizeBuffer,
                              mVideoTrack->mVideoTracks.mNextGetSampleIndex.valueOr(-1),
                              mVideoTrack->mVideoTracks.mNextInsertionIndex.valueOr(-1));

    result += nsPrintfCString("\t\tBuffered: ranges=%s\n",
                              DumpTimeRanges(mVideoTrack->SafeBuffered(TrackInfo::kVideoTrack)).get());
  }
  aString += NS_ConvertUTF8toUTF16(result);
}

MediaSourceTrackDemuxer::MediaSourceTrackDemuxer(MediaSourceDemuxer* aParent,
                                                 TrackInfo::TrackType aType,
                                                 TrackBuffersManager* aManager)
  : mParent(aParent)
  , mManager(aManager)
  , mType(aType)
  , mMonitor("MediaSourceTrackDemuxer")
{
}

UniquePtr<TrackInfo>
MediaSourceTrackDemuxer::GetInfo() const
{
  return mParent->GetTrackInfo(mType)->Clone();
}

nsRefPtr<MediaSourceTrackDemuxer::SeekPromise>
MediaSourceTrackDemuxer::Seek(media::TimeUnit aTime)
{
  MOZ_ASSERT(mParent, "Called after BreackCycle()");
  return InvokeAsync(mParent->GetTaskQueue(), this, __func__,
                     &MediaSourceTrackDemuxer::DoSeek, aTime);
}

nsRefPtr<MediaSourceTrackDemuxer::SamplesPromise>
MediaSourceTrackDemuxer::GetSamples(int32_t aNumSamples)
{
  MOZ_ASSERT(mParent, "Called after BreackCycle()");
  return InvokeAsync(mParent->GetTaskQueue(), this, __func__,
                     &MediaSourceTrackDemuxer::DoGetSamples, aNumSamples);
}

void
MediaSourceTrackDemuxer::Reset()
{
  MOZ_ASSERT(mParent, "Called after BreackCycle()");
  nsRefPtr<MediaSourceTrackDemuxer> self = this;
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableFunction([self] () {
      self->mManager->Seek(self->mType, TimeUnit(), TimeUnit());
      {
        MonitorAutoLock mon(self->mMonitor);
        self->mNextRandomAccessPoint =
          self->mManager->GetNextRandomAccessPoint(self->mType);
      }
    });
  mParent->GetTaskQueue()->Dispatch(task.forget());
}

nsresult
MediaSourceTrackDemuxer::GetNextRandomAccessPoint(media::TimeUnit* aTime)
{
  MonitorAutoLock mon(mMonitor);
  *aTime = mNextRandomAccessPoint;
  return NS_OK;
}

nsRefPtr<MediaSourceTrackDemuxer::SkipAccessPointPromise>
MediaSourceTrackDemuxer::SkipToNextRandomAccessPoint(media::TimeUnit aTimeThreshold)
{
  return InvokeAsync(mParent->GetTaskQueue(), this, __func__,
                     &MediaSourceTrackDemuxer::DoSkipToNextRandomAccessPoint,
                     aTimeThreshold);
}

media::TimeIntervals
MediaSourceTrackDemuxer::GetBuffered()
{
  return mManager->Buffered();
}

void
MediaSourceTrackDemuxer::BreakCycles()
{
  nsRefPtr<MediaSourceTrackDemuxer> self = this;
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableFunction([self]() {
      self->mParent = nullptr;
      self->mManager = nullptr;
    } );
  mParent->GetTaskQueue()->Dispatch(task.forget());
}

nsRefPtr<MediaSourceTrackDemuxer::SeekPromise>
MediaSourceTrackDemuxer::DoSeek(media::TimeUnit aTime)
{
  TimeIntervals buffered = mManager->Buffered(mType);
  buffered.SetFuzz(TimeUnit::FromMicroseconds(EOS_FUZZ_US));

  if (!buffered.Contains(aTime)) {
    // We don't have the data to seek to.
    return SeekPromise::CreateAndReject(DemuxerFailureReason::WAITING_FOR_DATA,
                                        __func__);
  }
  TimeUnit seekTime =
    mManager->Seek(mType, aTime, TimeUnit::FromMicroseconds(EOS_FUZZ_US));
  {
    MonitorAutoLock mon(mMonitor);
    mNextRandomAccessPoint = mManager->GetNextRandomAccessPoint(mType);
  }
  return SeekPromise::CreateAndResolve(seekTime, __func__);
}

nsRefPtr<MediaSourceTrackDemuxer::SamplesPromise>
MediaSourceTrackDemuxer::DoGetSamples(int32_t aNumSamples)
{
  bool error;
  nsRefPtr<MediaRawData> sample = mManager->GetSample(mType,
                                                      TimeUnit::FromMicroseconds(EOS_FUZZ_US),
                                                      error);
  if (!sample) {
    if (error) {
      return SamplesPromise::CreateAndReject(DemuxerFailureReason::DEMUXER_ERROR, __func__);
    }
    return SamplesPromise::CreateAndReject(
      mManager->IsEnded() ? DemuxerFailureReason::END_OF_STREAM :
                            DemuxerFailureReason::WAITING_FOR_DATA, __func__);
  }
  nsRefPtr<SamplesHolder> samples = new SamplesHolder;
  samples->mSamples.AppendElement(sample);
  if (mNextRandomAccessPoint.ToMicroseconds() <= sample->mTime) {
    MonitorAutoLock mon(mMonitor);
    mNextRandomAccessPoint = mManager->GetNextRandomAccessPoint(mType);
  }
  return SamplesPromise::CreateAndResolve(samples, __func__);
}

nsRefPtr<MediaSourceTrackDemuxer::SkipAccessPointPromise>
MediaSourceTrackDemuxer::DoSkipToNextRandomAccessPoint(media::TimeUnit aTimeThreadshold)
{
  bool found;
  uint32_t parsed =
    mManager->SkipToNextRandomAccessPoint(mType, aTimeThreadshold, found);
  if (found) {
    return SkipAccessPointPromise::CreateAndResolve(parsed, __func__);
  }
  SkipFailureHolder holder(
    mManager->IsEnded() ? DemuxerFailureReason::END_OF_STREAM :
                          DemuxerFailureReason::WAITING_FOR_DATA, parsed);
  return SkipAccessPointPromise::CreateAndReject(holder, __func__);
}

} // namespace mozilla
