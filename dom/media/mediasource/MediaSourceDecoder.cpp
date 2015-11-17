/* -*- mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaSourceDecoder.h"

#include "mozilla/Logging.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/Preferences.h"
#include "MediaDecoderStateMachine.h"
#include "MediaSource.h"
#include "MediaSourceResource.h"
#include "MediaSourceUtils.h"
#include "VideoUtils.h"
#include "MediaFormatReader.h"
#include "MediaSourceDemuxer.h"
#include "SourceBufferList.h"

extern mozilla::LogModule* GetMediaSourceLog();

#define MSE_DEBUG(arg, ...) MOZ_LOG(GetMediaSourceLog(), mozilla::LogLevel::Debug, ("MediaSourceDecoder(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define MSE_DEBUGV(arg, ...) MOZ_LOG(GetMediaSourceLog(), mozilla::LogLevel::Verbose, ("MediaSourceDecoder(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

using namespace mozilla::media;

namespace mozilla {

MediaSourceDecoder::MediaSourceDecoder(dom::HTMLMediaElement* aElement)
  : MediaDecoder(aElement)
  , mMediaSource(nullptr)
  , mEnded(false)
{
  SetExplicitDuration(UnspecifiedNaN<double>());
}

MediaDecoder*
MediaSourceDecoder::Clone(MediaDecoderOwner* aOwner)
{
  // TODO: Sort out cloning.
  return nullptr;
}

MediaDecoderStateMachine*
MediaSourceDecoder::CreateStateMachine()
{
  MOZ_ASSERT(NS_IsMainThread());
  mDemuxer = new MediaSourceDemuxer();
  RefPtr<MediaFormatReader> reader =
    new MediaFormatReader(this, mDemuxer, GetVideoFrameContainer());
  return new MediaDecoderStateMachine(this, reader);
}

nsresult
MediaSourceDecoder::Load(nsIStreamListener**)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!GetStateMachine());
  SetStateMachine(CreateStateMachine());
  if (!GetStateMachine()) {
    NS_WARNING("Failed to create state machine!");
    return NS_ERROR_FAILURE;
  }

  nsresult rv = GetStateMachine()->Init();
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
    if (buffered.Length()) {
      seekable +=
        media::TimeInterval(media::TimeUnit::FromSeconds(0), buffered.GetEnd());
    }
  } else {
    seekable += media::TimeInterval(media::TimeUnit::FromSeconds(0),
                                    media::TimeUnit::FromSeconds(duration));
  }
  MSE_DEBUG("ranges=%s", DumpTimeRanges(seekable).get());
  return seekable;
}

media::TimeIntervals
MediaSourceDecoder::GetBuffered()
{
  MOZ_ASSERT(NS_IsMainThread());

  dom::SourceBufferList* sourceBuffers = mMediaSource->ActiveSourceBuffers();
  media::TimeUnit highestEndTime;
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

  buffered +=
    media::TimeInterval(media::TimeUnit::FromMicroseconds(0), highestEndTime);

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

/*static*/
already_AddRefed<MediaResource>
MediaSourceDecoder::CreateResource(nsIPrincipal* aPrincipal)
{
  return RefPtr<MediaResource>(new MediaSourceResource(aPrincipal)).forget();
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
  mEnded = true;
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
  SetMediaSourceDuration(duration, MSRangeRemovalAction::SKIP);
}

void
MediaSourceDecoder::SetMediaSourceDuration(double aDuration, MSRangeRemovalAction aAction)
{
  MOZ_ASSERT(NS_IsMainThread());
  double oldDuration = ExplicitDuration();
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

  if (mMediaSource && aAction != MSRangeRemovalAction::SKIP) {
    mMediaSource->DurationChange(oldDuration, aDuration);
  }
}

double
MediaSourceDecoder::GetMediaSourceDuration()
{
  MOZ_ASSERT(NS_IsMainThread());
  return ExplicitDuration();
}

void
MediaSourceDecoder::GetMozDebugReaderData(nsAString& aString)
{
  mDemuxer->GetMozDebugReaderData(aString);
}

double
MediaSourceDecoder::GetDuration()
{
  MOZ_ASSERT(NS_IsMainThread());
  return ExplicitDuration();
}

#undef MSE_DEBUG
#undef MSE_DEBUGV

} // namespace mozilla
