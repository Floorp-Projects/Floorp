/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <limits>
#include <stdint.h>

#include "MediaSourceDemuxer.h"
#include "SourceBufferList.h"

namespace mozilla {

typedef TrackInfo::TrackType TrackType;
using media::TimeUnit;
using media::TimeIntervals;

MediaSourceDemuxer::MediaSourceDemuxer()
  : mTaskQueue(new MediaTaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK),
                                  /* aSupportsTailDispatch = */ true))
  , mMonitor("MediaSourceDemuxer")
{
  MOZ_ASSERT(NS_IsMainThread());
}

nsRefPtr<MediaSourceDemuxer::InitPromise>
MediaSourceDemuxer::Init()
{
  nsRefPtr<InitPromise> p = mInitPromise.Ensure(__func__);

  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableMethod(this, &MediaSourceDemuxer::AttemptInit);
  GetTaskQueue()->Dispatch(task.forget());

  return p;
}

void
MediaSourceDemuxer::AttemptInit()
{
  MOZ_ASSERT(OnTaskQueue());
  MonitorAutoLock mon(mMonitor);
  ScanSourceBuffersForContent();

  if (mInfo.HasAudio() || mInfo.HasVideo()) {
    mInitPromise.Resolve(NS_OK, __func__);
  } else {
    mInitPromise.Reject(DemuxerFailureReason::WAITING_FOR_DATA, __func__);
  }
}

void
MediaSourceDemuxer::ScanSourceBuffersForContent()
{
  mMonitor.AssertCurrentThreadOwns();

  for (const auto& sourceBuffer : mSourceBuffers) {
    MediaInfo info = sourceBuffer->GetMetadata();
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
MediaSourceDemuxer::NotifyTimeRangesChanged()
{
  MOZ_ASSERT(OnTaskQueue());
  for (uint32_t i = 0; i < mDemuxers.Length(); i++) {
    mDemuxers[i]->NotifyTimeRangesChanged();
  }
}

void
MediaSourceDemuxer::AttachSourceBuffer(TrackBuffersManager* aSourceBuffer)
{
  MonitorAutoLock mon(mMonitor);
  mSourceBuffers.AppendElement(aSourceBuffer);
  ScanSourceBuffersForContent();
}

void
MediaSourceDemuxer::DetachSourceBuffer(TrackBuffersManager* aSourceBuffer)
{
  MonitorAutoLock mon(mMonitor);
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
  mTaskQueue->BeginShutdown();
  mTaskQueue = nullptr;
}

MediaSourceTrackDemuxer::MediaSourceTrackDemuxer(MediaSourceDemuxer* aParent,
                                                 TrackInfo::TrackType aType,
                                                 TrackBuffersManager* aManager)
  : mParent(aParent)
  , mManager(aManager)
  , mType(aType)
  , mNextSampleIndex(0)
  , mDetached(false)
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
  nsRefPtr<SeekPromise> p = mSeekPromise.Ensure(__func__);

  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArg<TimeUnit>(
      this, &MediaSourceTrackDemuxer::DoSeek, aTime);
  mParent->GetTaskQueue()->Dispatch(task.forget());

  return p;
}

nsRefPtr<MediaSourceTrackDemuxer::SamplesPromise>
MediaSourceTrackDemuxer::GetSamples(int32_t aNumSamples)
{
  nsRefPtr<SamplesPromise> p = mSamplePromise.Ensure(__func__);

  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArg<int32_t>(
      this, &MediaSourceTrackDemuxer::DoGetSamples, aNumSamples);
  mParent->GetTaskQueue()->Dispatch(task.forget());

  return p;
}

void
MediaSourceTrackDemuxer::Reset()
{
  mSamplePromise.RejectIfExists(DemuxerFailureReason::CANCELED, __func__);
  mSkipPromise.RejectIfExists(SkipFailureHolder(DemuxerFailureReason::CANCELED, 0), __func__);
  mSeekPromise.RejectIfExists(DemuxerFailureReason::CANCELED, __func__);
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableMethod(this, &MediaSourceTrackDemuxer::DoReset);
  mParent->GetTaskQueue()->Dispatch(task.forget());
}

nsresult
MediaSourceTrackDemuxer::GetNextRandomAccessPoint(media::TimeUnit* aTime)
{
  *aTime = mNextRandomAccessPoint;
  return NS_OK;
}

nsRefPtr<MediaSourceTrackDemuxer::SkipAccessPointPromise>
MediaSourceTrackDemuxer::SkipToNextRandomAccessPoint(media::TimeUnit aTimeThreshold)
{
  nsRefPtr<SkipAccessPointPromise> p = mSkipPromise.Ensure(__func__);

  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArg<TimeUnit>(
      this, &MediaSourceTrackDemuxer::DoSkipToNextRandomAccessPoint, aTimeThreshold);
  mParent->GetTaskQueue()->Dispatch(task.forget());

  return p;
}

int64_t
MediaSourceTrackDemuxer::GetEvictionOffset(media::TimeUnit aTime)
{
  // Unused.
  return 0;
}

media::TimeIntervals
MediaSourceTrackDemuxer::GetBuffered()
{
  return mManager->Buffered();
}

void
MediaSourceTrackDemuxer::BreakCycles()
{
  mDetached = true;
  mParent = nullptr;
  mManager = nullptr;
}

void
MediaSourceTrackDemuxer::DoSeek(const media::TimeUnit& aTime)
{
  MOZ_ASSERT(mParent->OnTaskQueue());
  if (mSeekPromise.IsEmpty()) {
    // Got reset.
    return;
  }
  const TrackBuffersManager::TrackBuffer& track = mManager->GetTrackBuffer(mType);
  TimeUnit lastKeyFrameTime;
  uint32_t lastKeyFrameIndex = 0;
  for (uint32_t i = 0; i < track.Length(); i++) {
    const nsRefPtr<MediaRawData>& sample = track[i];
    if (sample->mKeyframe) {
      lastKeyFrameTime = TimeUnit::FromMicroseconds(sample->mTime);
      lastKeyFrameIndex = i;
    }
    if (sample->mTime >= aTime.ToMicroseconds()) {
      break;
    }
  }
  mNextSampleIndex = lastKeyFrameIndex;
  mNextSampleTime = lastKeyFrameTime;
  mNextRandomAccessPoint = GetNextRandomAccessPoint();
  mSeekPromise.ResolveIfExists(mNextSampleTime, __func__);
}

void
MediaSourceTrackDemuxer::DoGetSamples(int32_t aNumSamples)
{
  MOZ_ASSERT(mParent->OnTaskQueue());
  if (mSamplePromise.IsEmpty()) {
    // Got reset.
    return;
  }

  DemuxerFailureReason failure;
  nsRefPtr<MediaRawData> sample = GetSample(failure);
  if (sample) {
    nsRefPtr<SamplesHolder> samples = new SamplesHolder;
    samples->mSamples.AppendElement(sample);
    mNextRandomAccessPoint = GetNextRandomAccessPoint();
    mSamplePromise.ResolveIfExists(samples, __func__);
  } else {
    mSamplePromise.RejectIfExists(failure, __func__);
  }
}

void
MediaSourceTrackDemuxer::DoSkipToNextRandomAccessPoint(const media::TimeUnit& aTimeThreadshold)
{
  MOZ_ASSERT(mParent->OnTaskQueue());
  if (mSkipPromise.IsEmpty()) {
    // Got reset.
    return;
  }
  bool found = false;
  int32_t parsed = 0;
  const TrackBuffersManager::TrackBuffer& track =
    mManager->GetTrackBuffer(mType);
  for (uint32_t i = mNextSampleIndex; i < track.Length(); i++) {
    const nsRefPtr<MediaRawData>& sample = track[i];
    if (sample->mKeyframe && sample->mTime >= aTimeThreadshold.ToMicroseconds()) {
      mNextSampleTime = TimeUnit::FromMicroseconds(sample->GetEndTime());
      found = true;
      break;
    }
    parsed++;
  }

  if (found) {
    mSkipPromise.ResolveIfExists(parsed, __func__);
  } else {
    SkipFailureHolder holder(
      mManager->IsEnded() ? DemuxerFailureReason::END_OF_STREAM :
                            DemuxerFailureReason::WAITING_FOR_DATA, parsed);
    mSkipPromise.RejectIfExists(holder, __func__);
  }
}

already_AddRefed<MediaRawData>
MediaSourceTrackDemuxer::GetSample(DemuxerFailureReason& aFailure)
{
  MOZ_ASSERT(mParent->OnTaskQueue());
  const TrackBuffersManager::TrackBuffer& track =
    mManager->GetTrackBuffer(mType);
  const TimeIntervals& ranges = mManager->Buffered(mType);
  if (mNextSampleTime >= ranges.GetEnd()) {
    if (mManager->IsEnded()) {
      aFailure = DemuxerFailureReason::END_OF_STREAM;
    } else {
      aFailure = DemuxerFailureReason::WAITING_FOR_DATA;
    }
    return nullptr;
  }
  if (mNextSampleTime == TimeUnit()) {
    // First demux, get first sample time.
    mNextSampleTime = ranges.GetStart();
  }
  if (!ranges.Contains(mNextSampleTime)) {
    aFailure = DemuxerFailureReason::WAITING_FOR_DATA;
    return nullptr;
  }

  if (mNextSampleIndex) {
    const nsRefPtr<MediaRawData>& sample = track[mNextSampleIndex];
    nsRefPtr<MediaRawData> p = sample->Clone();
    if (!p) {
      aFailure = DemuxerFailureReason::DEMUXER_ERROR;
      return nullptr;
    }
    mNextSampleTime = TimeUnit::FromMicroseconds(sample->GetEndTime());
    mNextSampleIndex++;
    return p.forget();
  }
  for (uint32_t i = 0; i < track.Length(); i++) {
    const nsRefPtr<MediaRawData>& sample = track[i];
    if (sample->mTime >= mNextSampleTime.ToMicroseconds()) {
      nsRefPtr<MediaRawData> p = sample->Clone();
      mNextSampleTime = TimeUnit::FromMicroseconds(sample->GetEndTime());
      mNextSampleIndex = i+1;
      if (!p) {
        // OOM
        break;
      }
      return p.forget();
    }
  }
  aFailure = DemuxerFailureReason::DEMUXER_ERROR;
  return nullptr;
}

TimeUnit
MediaSourceTrackDemuxer::GetNextRandomAccessPoint()
{
  MOZ_ASSERT(mParent->OnTaskQueue());
  const TrackBuffersManager::TrackBuffer& track = mManager->GetTrackBuffer(mType);
  for (uint32_t i = mNextSampleIndex; i < track.Length(); i++) {
    const nsRefPtr<MediaRawData>& sample = track[i];
    if (sample->mKeyframe) {
      return TimeUnit::FromMicroseconds(sample->mTime);
    }
  }
  return media::TimeUnit::FromInfinity();
}

void
MediaSourceTrackDemuxer::NotifyTimeRangesChanged()
{
  if (mDetached) {
    return;
  }
  MOZ_ASSERT(mParent->OnTaskQueue());
  mNextSampleIndex = 0;
}

void
MediaSourceTrackDemuxer::DoReset()
{
  if (mDetached) {
    return;
  }
  MOZ_ASSERT(mParent->OnTaskQueue());
  mNextSampleTime = TimeUnit();
  mNextRandomAccessPoint = TimeUnit();
  mNextSampleIndex = 0;
}

} // namespace mozilla
