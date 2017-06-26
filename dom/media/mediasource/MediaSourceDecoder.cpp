/* -*- mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaSourceDecoder.h"

#include "mozilla/Logging.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "MediaDecoderStateMachine.h"
#include "MediaShutdownManager.h"
#include "MediaSource.h"
#include "MediaSourceResource.h"
#include "MediaSourceUtils.h"
#include "VideoUtils.h"
#include "MediaSourceDemuxer.h"
#include "SourceBufferList.h"
#include <algorithm>

extern mozilla::LogModule* GetMediaSourceLog();

#define MSE_DEBUG(arg, ...) MOZ_LOG(GetMediaSourceLog(), mozilla::LogLevel::Debug, ("MediaSourceDecoder(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define MSE_DEBUGV(arg, ...) MOZ_LOG(GetMediaSourceLog(), mozilla::LogLevel::Verbose, ("MediaSourceDecoder(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

using namespace mozilla::media;

namespace mozilla {

MediaSourceDecoder::MediaSourceDecoder(MediaDecoderInit& aInit)
  : MediaDecoder(aInit)
  , mMediaSource(nullptr)
  , mEnded(false)
{
  mExplicitDuration.Set(Some(UnspecifiedNaN<double>()));
}

MediaDecoderStateMachine*
MediaSourceDecoder::CreateStateMachine()
{
  MOZ_ASSERT(NS_IsMainThread());
  mDemuxer = new MediaSourceDemuxer(AbstractMainThread());
  mReader = new MediaFormatReader(this, mDemuxer, GetVideoFrameContainer());
  return new MediaDecoderStateMachine(this, mReader);
}

nsresult
MediaSourceDecoder::Load(nsIPrincipal* aPrincipal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!GetStateMachine());

  mResource = new MediaSourceResource(aPrincipal);

  nsresult rv = MediaShutdownManager::Instance().Register(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  SetStateMachine(CreateStateMachine());
  if (!GetStateMachine()) {
    NS_WARNING("Failed to create state machine!");
    return NS_ERROR_FAILURE;
  }

  rv = GetStateMachine()->Init(this);
  NS_ENSURE_SUCCESS(rv, rv);

  SetStateMachineParameters();
  return NS_OK;
}

media::TimeIntervals
MediaSourceDecoder::GetSeekable()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mMediaSource) {
    NS_WARNING("MediaSource element isn't attached");
    return media::TimeIntervals::Invalid();
  }

  media::TimeIntervals seekable;
  double duration = mMediaSource->Duration();
  if (IsNaN(duration)) {
    // Return empty range.
  } else if (duration > 0 && mozilla::IsInfinite(duration)) {
    media::TimeIntervals buffered = GetBuffered();

    // 1. If live seekable range is not empty:
    if (mMediaSource->HasLiveSeekableRange()) {
      // 1. Let union ranges be the union of live seekable range and the
      // HTMLMediaElement.buffered attribute.
      media::TimeIntervals unionRanges =
        buffered + mMediaSource->LiveSeekableRange();
      // 2. Return a single range with a start time equal to the earliest start
      // time in union ranges and an end time equal to the highest end time in
      // union ranges and abort these steps.
      seekable +=
        media::TimeInterval(unionRanges.GetStart(), unionRanges.GetEnd());
      return seekable;
    }

    if (buffered.Length()) {
      seekable += media::TimeInterval(TimeUnit::Zero(), buffered.GetEnd());
    }
  } else {
    seekable += media::TimeInterval(TimeUnit::Zero(),
                                    TimeUnit::FromSeconds(duration));
  }
  MSE_DEBUG("ranges=%s", DumpTimeRanges(seekable).get());
  return seekable;
}

media::TimeIntervals
MediaSourceDecoder::GetBuffered()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mMediaSource) {
    NS_WARNING("MediaSource element isn't attached");
    return media::TimeIntervals::Invalid();
  }
  dom::SourceBufferList* sourceBuffers = mMediaSource->ActiveSourceBuffers();
  if (!sourceBuffers) {
    // Media source object is shutting down.
    return TimeIntervals();
  }
  TimeUnit highestEndTime;
  nsTArray<media::TimeIntervals> activeRanges;
  media::TimeIntervals buffered;

  for (uint32_t i = 0; i < sourceBuffers->Length(); i++) {
    bool found;
    dom::SourceBuffer* sb = sourceBuffers->IndexedGetter(i, found);
    MOZ_ASSERT(found);

    activeRanges.AppendElement(sb->GetTimeIntervals());
    highestEndTime =
      std::max(highestEndTime, activeRanges.LastElement().GetEnd());
  }

  buffered += media::TimeInterval(TimeUnit::Zero(), highestEndTime);

  for (auto& range : activeRanges) {
    if (mEnded && range.Length()) {
      // Set the end time on the last range to highestEndTime by adding a
      // new range spanning the current end time to highestEndTime, which
      // Normalize() will then merge with the old last range.
      range +=
        media::TimeInterval(range.GetEnd(), highestEndTime);
    }
    buffered.Intersection(range);
  }

  MSE_DEBUG("ranges=%s", DumpTimeRanges(buffered).get());
  return buffered;
}

void
MediaSourceDecoder::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("Shutdown");
  // Detach first so that TrackBuffers are unused on the main thread when
  // shut down on the decode task queue.
  if (mMediaSource) {
    mMediaSource->Detach();
  }
  mDemuxer = nullptr;

  MediaDecoder::Shutdown();
}

void
MediaSourceDecoder::AttachMediaSource(dom::MediaSource* aMediaSource)
{
  MOZ_ASSERT(!mMediaSource && !GetStateMachine() && NS_IsMainThread());
  mMediaSource = aMediaSource;
}

void
MediaSourceDecoder::DetachMediaSource()
{
  MOZ_ASSERT(mMediaSource && NS_IsMainThread());
  mMediaSource = nullptr;
}

void
MediaSourceDecoder::Ended(bool aEnded)
{
  MOZ_ASSERT(NS_IsMainThread());
  static_cast<MediaSourceResource*>(GetResource())->SetEnded(aEnded);
  if (aEnded) {
    // We want the MediaSourceReader to refresh its buffered range as it may
    // have been modified (end lined up).
    NotifyDataArrived();
  }
  mEnded = aEnded;
}

void
MediaSourceDecoder::AddSizeOfResources(ResourceSizes* aSizes)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (GetDemuxer()) {
    GetDemuxer()->AddSizeOfResources(aSizes);
  }
}

void
MediaSourceDecoder::SetInitialDuration(int64_t aDuration)
{
  MOZ_ASSERT(NS_IsMainThread());
  // Only use the decoded duration if one wasn't already
  // set.
  if (!mMediaSource || !IsNaN(ExplicitDuration())) {
    return;
  }
  double duration = aDuration;
  // A duration of -1 is +Infinity.
  if (aDuration >= 0) {
    duration /= USECS_PER_S;
  }
  SetMediaSourceDuration(duration);
}

void
MediaSourceDecoder::SetMediaSourceDuration(double aDuration)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown());
  if (aDuration >= 0) {
    int64_t checkedDuration;
    if (NS_FAILED(SecondsToUsecs(aDuration, checkedDuration))) {
      // INT64_MAX is used as infinity by the state machine.
      // We want a very bigger number, but not infinity.
      checkedDuration = INT64_MAX - 1;
    }
    SetExplicitDuration(aDuration);
  } else {
    SetExplicitDuration(PositiveInfinity<double>());
  }
}

void
MediaSourceDecoder::GetMozDebugReaderData(nsACString& aString)
{
  if (mReader && mDemuxer) {
    mReader->GetMozDebugReaderData(aString);
    mDemuxer->GetMozDebugReaderData(aString);
  }
}

double
MediaSourceDecoder::GetDuration()
{
  MOZ_ASSERT(NS_IsMainThread());
  return ExplicitDuration();
}

MediaDecoderOwner::NextFrameStatus
MediaSourceDecoder::NextFrameBufferedStatus()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mMediaSource
      || mMediaSource->ReadyState() == dom::MediaSourceReadyState::Closed) {
    return MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE;
  }

  // Next frame hasn't been decoded yet.
  // Use the buffered range to consider if we have the next frame available.
  auto currentPosition = CurrentPosition();
  TimeIntervals buffered = GetBuffered();
  buffered.SetFuzz(MediaSourceDemuxer::EOS_FUZZ / 2);
  TimeInterval interval(
    currentPosition,
    currentPosition + DEFAULT_NEXT_FRAME_AVAILABLE_BUFFERED);
  return buffered.ContainsStrict(ClampIntervalToEnd(interval))
         ? MediaDecoderOwner::NEXT_FRAME_AVAILABLE
         : MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE;
}

bool
MediaSourceDecoder::CanPlayThrough()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NextFrameBufferedStatus() == MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE) {
    return false;
  }

  if (IsNaN(mMediaSource->Duration())) {
    // Don't have any data yet.
    return false;
  }
  TimeUnit duration = TimeUnit::FromSeconds(mMediaSource->Duration());
  auto currentPosition = CurrentPosition();
  if (duration.IsInfinite()) {
    // We can't make an informed decision and just assume that it's a live
    // stream
    return true;
  } else if (duration <= currentPosition) {
    return true;
  }
  // If we have data up to the mediasource's duration or 30s ahead, we can
  // assume that we can play without interruption.
  TimeIntervals buffered = GetBuffered();
  buffered.SetFuzz(MediaSourceDemuxer::EOS_FUZZ / 2);
  TimeUnit timeAhead =
    std::min(duration, currentPosition + TimeUnit::FromSeconds(30));
  TimeInterval interval(currentPosition, timeAhead);
  return buffered.ContainsStrict(ClampIntervalToEnd(interval));
}

void
MediaSourceDecoder::NotifyWaitingForKey()
{
  mWaitingForKeyEvent.Notify();
}

MediaEventSource<void>*
MediaSourceDecoder::WaitingForKeyEvent()
{
  return &mWaitingForKeyEvent;
}

TimeInterval
MediaSourceDecoder::ClampIntervalToEnd(const TimeInterval& aInterval)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mEnded) {
    return aInterval;
  }
  TimeUnit duration = TimeUnit::FromSeconds(GetDuration());
  if (duration < aInterval.mStart) {
    return aInterval;
  }
  return TimeInterval(aInterval.mStart,
                      std::min(aInterval.mEnd, duration),
                      aInterval.mFuzz);
}

void
MediaSourceDecoder::NotifyInitDataArrived()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mDemuxer) {
    mDemuxer->NotifyInitDataArrived();
  }
}

#undef MSE_DEBUG
#undef MSE_DEBUGV

} // namespace mozilla
