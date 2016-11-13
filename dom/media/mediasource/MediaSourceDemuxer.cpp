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
#include "OpusDecoder.h"

namespace mozilla {

typedef TrackInfo::TrackType TrackType;
using media::TimeUnit;
using media::TimeIntervals;

MediaSourceDemuxer::MediaSourceDemuxer()
  : mTaskQueue(new AutoTaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK),
                                 /* aSupportsTailDispatch = */ false))
  , mMonitor("MediaSourceDemuxer")
{
  MOZ_ASSERT(NS_IsMainThread());
}

// Due to inaccuracies in determining buffer end
// frames (Bug 1065207). This value is based on videos seen in the wild.
const TimeUnit MediaSourceDemuxer::EOS_FUZZ = media::TimeUnit::FromMicroseconds(500000);

RefPtr<MediaSourceDemuxer::InitPromise>
MediaSourceDemuxer::Init()
{
  RefPtr<MediaSourceDemuxer> self = this;
  return InvokeAsync(GetTaskQueue(), __func__,
    [self](){
      if (self->ScanSourceBuffersForContent()) {
        return InitPromise::CreateAndResolve(NS_OK, __func__);
      }

      RefPtr<InitPromise> p = self->mInitPromise.Ensure(__func__);

      return p;
    });
}

void
MediaSourceDemuxer::AddSizeOfResources(MediaSourceDecoder::ResourceSizes* aSizes)
{
  MOZ_ASSERT(NS_IsMainThread());

  // NB: The track buffers must only be accessed on the TaskQueue.
  RefPtr<MediaSourceDemuxer> self = this;
  RefPtr<MediaSourceDecoder::ResourceSizes> sizes = aSizes;
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableFunction([self, sizes] () {
      for (TrackBuffersManager* manager : self->mSourceBuffers) {
        manager->AddSizeOfResources(sizes);
      }
    });

  GetTaskQueue()->Dispatch(task.forget());
}

void MediaSourceDemuxer::NotifyDataArrived()
{
  RefPtr<MediaSourceDemuxer> self = this;
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
  RefPtr<TrackBuffersManager> manager = GetManager(aType);
  if (!manager) {
    return nullptr;
  }
  RefPtr<MediaSourceTrackDemuxer> e =
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
    NewRunnableMethod<TrackBuffersManager*>(
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
    NewRunnableMethod<TrackBuffersManager*>(
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
  mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
}

void
MediaSourceDemuxer::GetMozDebugReaderData(nsAString& aString)
{
  MonitorAutoLock mon(mMonitor);
  nsAutoCString result;
  result += nsPrintfCString("Dumping data for demuxer %p:\n", this);
  if (mAudioTrack) {
    result += nsPrintfCString("\tDumping Audio Track Buffer(%s): - mLastAudioTime: %f\n"
                              "\t\tNumSamples:%u Size:%u Evictable:%u NextGetSampleIndex:%u NextInsertionIndex:%d\n",
                              mAudioTrack->mAudioTracks.mInfo->mMimeType.get(),
                              mAudioTrack->mAudioTracks.mNextSampleTime.ToSeconds(),
                              mAudioTrack->mAudioTracks.mBuffers[0].Length(),
                              mAudioTrack->mAudioTracks.mSizeBuffer,
                              mAudioTrack->Evictable(TrackInfo::kAudioTrack),
                              mAudioTrack->mAudioTracks.mNextGetSampleIndex.valueOr(-1),
                              mAudioTrack->mAudioTracks.mNextInsertionIndex.valueOr(-1));

    result += nsPrintfCString("\t\tBuffered: ranges=%s\n",
                              DumpTimeRanges(mAudioTrack->SafeBuffered(TrackInfo::kAudioTrack)).get());
  }
  if (mVideoTrack) {
    result += nsPrintfCString("\tDumping Video Track Buffer(%s) - mLastVideoTime: %f\n"
                              "\t\tNumSamples:%u Size:%u Evictable:%u NextGetSampleIndex:%u NextInsertionIndex:%d\n",
                              mVideoTrack->mVideoTracks.mInfo->mMimeType.get(),
                              mVideoTrack->mVideoTracks.mNextSampleTime.ToSeconds(),
                              mVideoTrack->mVideoTracks.mBuffers[0].Length(),
                              mVideoTrack->mVideoTracks.mSizeBuffer,
                              mVideoTrack->Evictable(TrackInfo::kVideoTrack),
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
  , mReset(true)
  , mPreRoll(
      TimeUnit::FromMicroseconds(
        OpusDataDecoder::IsOpus(mParent->GetTrackInfo(mType)->mMimeType)
          ? 80000 : 0))
{
}

UniquePtr<TrackInfo>
MediaSourceTrackDemuxer::GetInfo() const
{
  return mParent->GetTrackInfo(mType)->Clone();
}

RefPtr<MediaSourceTrackDemuxer::SeekPromise>
MediaSourceTrackDemuxer::Seek(media::TimeUnit aTime)
{
  MOZ_ASSERT(mParent, "Called after BreackCycle()");
  return InvokeAsync(mParent->GetTaskQueue(), this, __func__,
                     &MediaSourceTrackDemuxer::DoSeek, aTime);
}

RefPtr<MediaSourceTrackDemuxer::SamplesPromise>
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
  RefPtr<MediaSourceTrackDemuxer> self = this;
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableFunction([self] () {
      self->mNextSample.reset();
      self->mReset = true;
      self->mManager->Seek(self->mType, TimeUnit(), TimeUnit());
      {
        MonitorAutoLock mon(self->mMonitor);
        self->mNextRandomAccessPoint =
          self->mManager->GetNextRandomAccessPoint(self->mType,
                                                   MediaSourceDemuxer::EOS_FUZZ);
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

RefPtr<MediaSourceTrackDemuxer::SkipAccessPointPromise>
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
  RefPtr<MediaSourceTrackDemuxer> self = this;
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableFunction([self]() {
      self->mParent = nullptr;
      self->mManager = nullptr;
    } );
  mParent->GetTaskQueue()->Dispatch(task.forget());
}

RefPtr<MediaSourceTrackDemuxer::SeekPromise>
MediaSourceTrackDemuxer::DoSeek(media::TimeUnit aTime)
{
  TimeIntervals buffered = mManager->Buffered(mType);
  // Fuzz factor represents a +/- threshold. So when seeking it allows the gap
  // to be twice as big as the fuzz value. We only want to allow EOS_FUZZ gap.
  buffered.SetFuzz(MediaSourceDemuxer::EOS_FUZZ / 2);
  TimeUnit seekTime = std::max(aTime - mPreRoll, TimeUnit::FromMicroseconds(0));

  if (mManager->IsEnded() && seekTime >= buffered.GetEnd()) {
    // We're attempting to seek past the end time. Cap seekTime so that we seek
    // to the last sample instead.
    seekTime =
      std::max(mManager->HighestStartTime(mType) - mPreRoll,
               TimeUnit::FromMicroseconds(0));
  }
  if (!buffered.ContainsWithStrictEnd(seekTime)) {
    if (!buffered.ContainsWithStrictEnd(aTime)) {
      // We don't have the data to seek to.
      return SeekPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA,
                                          __func__);
    }
    // Theoretically we should reject the promise with WAITING_FOR_DATA,
    // however, to avoid unwanted regressions we assume that if at this time
    // we don't have the wanted data it won't come later.
    // Instead of using the pre-rolled time, use the earliest time available in
    // the interval.
    TimeIntervals::IndexType index = buffered.Find(aTime);
    MOZ_ASSERT(index != TimeIntervals::NoIndex);
    seekTime = buffered[index].mStart;
  }
  seekTime = mManager->Seek(mType, seekTime, MediaSourceDemuxer::EOS_FUZZ);
  MediaResult result = NS_OK;
  RefPtr<MediaRawData> sample =
    mManager->GetSample(mType,
                        media::TimeUnit(),
                        result);
  MOZ_ASSERT(NS_SUCCEEDED(result) && sample);
  mNextSample = Some(sample);
  mReset = false;
  {
    MonitorAutoLock mon(mMonitor);
    mNextRandomAccessPoint =
      mManager->GetNextRandomAccessPoint(mType, MediaSourceDemuxer::EOS_FUZZ);
  }
  return SeekPromise::CreateAndResolve(seekTime, __func__);
}

RefPtr<MediaSourceTrackDemuxer::SamplesPromise>
MediaSourceTrackDemuxer::DoGetSamples(int32_t aNumSamples)
{
  if (mReset) {
    // If a seek (or reset) was recently performed, we ensure that the data
    // we are about to retrieve is still available.
    TimeIntervals buffered = mManager->Buffered(mType);
    buffered.SetFuzz(MediaSourceDemuxer::EOS_FUZZ / 2);

    if (!buffered.Length() && mManager->IsEnded()) {
      return SamplesPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_END_OF_STREAM,
                                             __func__);
    }
    if (!buffered.ContainsWithStrictEnd(TimeUnit::FromMicroseconds(0))) {
      return SamplesPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA,
                                             __func__);
    }
    mReset = false;
  }
  RefPtr<MediaRawData> sample;
  if (mNextSample) {
    sample = mNextSample.ref();
    mNextSample.reset();
  } else {
    MediaResult result = NS_OK;
    sample = mManager->GetSample(mType, MediaSourceDemuxer::EOS_FUZZ, result);
    if (!sample) {
      if (result == NS_ERROR_DOM_MEDIA_END_OF_STREAM ||
          result == NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA) {
        return SamplesPromise::CreateAndReject(
          (result == NS_ERROR_DOM_MEDIA_END_OF_STREAM && mManager->IsEnded())
          ? NS_ERROR_DOM_MEDIA_END_OF_STREAM
          : NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA, __func__);
      }
      return SamplesPromise::CreateAndReject(result, __func__);
    }
  }
  RefPtr<SamplesHolder> samples = new SamplesHolder;
  samples->mSamples.AppendElement(sample);
  if (mNextRandomAccessPoint.ToMicroseconds() <= sample->mTime) {
    MonitorAutoLock mon(mMonitor);
    mNextRandomAccessPoint =
      mManager->GetNextRandomAccessPoint(mType, MediaSourceDemuxer::EOS_FUZZ);
  }
  return SamplesPromise::CreateAndResolve(samples, __func__);
}

RefPtr<MediaSourceTrackDemuxer::SkipAccessPointPromise>
MediaSourceTrackDemuxer::DoSkipToNextRandomAccessPoint(media::TimeUnit aTimeThreadshold)
{
  uint32_t parsed = 0;
  // Ensure that the data we are about to skip to is still available.
  TimeIntervals buffered = mManager->Buffered(mType);
  buffered.SetFuzz(MediaSourceDemuxer::EOS_FUZZ / 2);
  if (buffered.ContainsWithStrictEnd(aTimeThreadshold)) {
    bool found;
    parsed = mManager->SkipToNextRandomAccessPoint(mType,
                                                   aTimeThreadshold,
                                                   MediaSourceDemuxer::EOS_FUZZ,
                                                   found);
    if (found) {
      return SkipAccessPointPromise::CreateAndResolve(parsed, __func__);
    }
  }
  SkipFailureHolder holder(
    mManager->IsEnded() ? NS_ERROR_DOM_MEDIA_END_OF_STREAM :
                          NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA, parsed);
  return SkipAccessPointPromise::CreateAndReject(holder, __func__);
}

} // namespace mozilla
