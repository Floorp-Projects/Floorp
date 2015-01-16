/* -*- mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaSourceDecoder.h"

#include "prlog.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/TimeRanges.h"
#include "MediaDecoderStateMachine.h"
#include "MediaSource.h"
#include "MediaSourceReader.h"
#include "MediaSourceResource.h"
#include "MediaSourceUtils.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* GetMediaSourceLog();
extern PRLogModuleInfo* GetMediaSourceAPILog();

#define MSE_DEBUG(...) PR_LOG(GetMediaSourceLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#define MSE_DEBUGV(...) PR_LOG(GetMediaSourceLog(), PR_LOG_DEBUG+1, (__VA_ARGS__))
#define MSE_API(...) PR_LOG(GetMediaSourceAPILog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define MSE_DEBUG(...)
#define MSE_DEBUGV(...)
#define MSE_API(...)
#endif

namespace mozilla {

class SourceBufferDecoder;

MediaSourceDecoder::MediaSourceDecoder(dom::HTMLMediaElement* aElement)
  : mMediaSource(nullptr)
  , mMediaSourceDuration(UnspecifiedNaN<double>())
{
  Init(aElement);
}

MediaDecoder*
MediaSourceDecoder::Clone()
{
  // TODO: Sort out cloning.
  return nullptr;
}

MediaDecoderStateMachine*
MediaSourceDecoder::CreateStateMachine()
{
  mReader = new MediaSourceReader(this);
  return new MediaDecoderStateMachine(this, mReader);
}

nsresult
MediaSourceDecoder::Load(nsIStreamListener**, MediaDecoder*)
{
  MOZ_ASSERT(!mDecoderStateMachine);
  mDecoderStateMachine = CreateStateMachine();
  if (!mDecoderStateMachine) {
    NS_WARNING("Failed to create state machine!");
    return NS_ERROR_FAILURE;
  }

  nsresult rv = mDecoderStateMachine->Init(nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  SetStateMachineParameters();
  return ScheduleStateMachineThread();
}

nsresult
MediaSourceDecoder::GetSeekable(dom::TimeRanges* aSeekable)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mMediaSource) {
    return NS_ERROR_FAILURE;
  }

  double duration = mMediaSource->Duration();
  if (IsNaN(duration)) {
    // Return empty range.
  } else if (duration > 0 && mozilla::IsInfinite(duration)) {
    nsRefPtr<dom::TimeRanges> bufferedRanges = new dom::TimeRanges();
    mReader->GetBuffered(bufferedRanges);
    aSeekable->Add(bufferedRanges->GetStartTime(), bufferedRanges->GetEndTime());
  } else {
    aSeekable->Add(0, duration);
  }
  MSE_DEBUG("MediaSourceDecoder(%p)::GetSeekable ranges=%s", this, DumpTimeRanges(aSeekable).get());
  return NS_OK;
}

void
MediaSourceDecoder::Shutdown()
{
  MSE_DEBUG("MediaSourceDecoder(%p)::Shutdown", this);
  // Detach first so that TrackBuffers are unused on the main thread when
  // shut down on the decode task queue.
  if (mMediaSource) {
    mMediaSource->Detach();
  }

  MediaDecoder::Shutdown();
  // Kick WaitForData out of its slumber.
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  mon.NotifyAll();
}

/*static*/
already_AddRefed<MediaResource>
MediaSourceDecoder::CreateResource(nsIPrincipal* aPrincipal)
{
  return nsRefPtr<MediaResource>(new MediaSourceResource(aPrincipal)).forget();
}

void
MediaSourceDecoder::AttachMediaSource(dom::MediaSource* aMediaSource)
{
  MOZ_ASSERT(!mMediaSource && !mDecoderStateMachine && NS_IsMainThread());
  mMediaSource = aMediaSource;
}

void
MediaSourceDecoder::DetachMediaSource()
{
  MOZ_ASSERT(mMediaSource && NS_IsMainThread());
  mMediaSource = nullptr;
}

already_AddRefed<SourceBufferDecoder>
MediaSourceDecoder::CreateSubDecoder(const nsACString& aType, int64_t aTimestampOffset)
{
  MOZ_ASSERT(mReader);
  return mReader->CreateSubDecoder(aType, aTimestampOffset);
}

void
MediaSourceDecoder::AddTrackBuffer(TrackBuffer* aTrackBuffer)
{
  MOZ_ASSERT(mReader);
  mReader->AddTrackBuffer(aTrackBuffer);
}

void
MediaSourceDecoder::RemoveTrackBuffer(TrackBuffer* aTrackBuffer)
{
  MOZ_ASSERT(mReader);
  mReader->RemoveTrackBuffer(aTrackBuffer);
}

void
MediaSourceDecoder::OnTrackBufferConfigured(TrackBuffer* aTrackBuffer, const MediaInfo& aInfo)
{
  MOZ_ASSERT(mReader);
  mReader->OnTrackBufferConfigured(aTrackBuffer, aInfo);
}

void
MediaSourceDecoder::Ended()
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  mReader->Ended();
  mon.NotifyAll();
}

bool
MediaSourceDecoder::IsExpectingMoreData()
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  return !mReader->IsEnded();
}

class DurationChangedRunnable : public nsRunnable {
public:
  explicit DurationChangedRunnable(MediaSourceDecoder* aDecoder,
                                   double aOldDuration,
                                   double aNewDuration)
    : mDecoder(aDecoder)
    , mOldDuration(aOldDuration)
    , mNewDuration(aNewDuration)
  { }

  NS_IMETHOD Run() MOZ_OVERRIDE MOZ_FINAL {
    mDecoder->DurationChanged(mOldDuration, mNewDuration);
    return NS_OK;
  }

private:
  RefPtr<MediaSourceDecoder> mDecoder;
  double mOldDuration;
  double mNewDuration;
};

void
MediaSourceDecoder::DurationChanged(double aOldDuration, double aNewDuration)
{
  MOZ_ASSERT(NS_IsMainThread());
  // Run the MediaSource duration changed algorithm
  if (mMediaSource) {
    mMediaSource->DurationChange(aOldDuration, aNewDuration);
  }
  // Run the MediaElement duration changed algorithm
  MediaDecoder::DurationChanged();
}

void
MediaSourceDecoder::SetDecodedDuration(int64_t aDuration)
{
  // Only use the decoded duration if one wasn't already
  // set.
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  if (!mMediaSource || !IsNaN(mMediaSourceDuration)) {
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
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  double oldDuration = mMediaSourceDuration;
  if (aDuration >= 0) {
    mDecoderStateMachine->SetDuration(aDuration * USECS_PER_S);
    mMediaSourceDuration = aDuration;
  } else {
    mDecoderStateMachine->SetDuration(INT64_MAX);
    mMediaSourceDuration = PositiveInfinity<double>();
  }
  if (NS_IsMainThread()) {
    DurationChanged(oldDuration, mMediaSourceDuration);
  } else {
    nsRefPtr<nsIRunnable> task =
      new DurationChangedRunnable(this, oldDuration, mMediaSourceDuration);
    NS_DispatchToMainThread(task);
  }
}

double
MediaSourceDecoder::GetMediaSourceDuration()
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  return mMediaSourceDuration;
}

void
MediaSourceDecoder::NotifyTimeRangesChanged()
{
  MOZ_ASSERT(mReader);
  mReader->NotifyTimeRangesChanged();
}

void
MediaSourceDecoder::PrepareReaderInitialization()
{
  MOZ_ASSERT(mReader);
  mReader->PrepareInitialization();
}

#ifdef MOZ_EME
nsresult
MediaSourceDecoder::SetCDMProxy(CDMProxy* aProxy)
{
  nsresult rv = MediaDecoder::SetCDMProxy(aProxy);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mReader->SetCDMProxy(aProxy);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
#endif

bool
MediaSourceDecoder::IsActiveReader(MediaDecoderReader* aReader)
{
  return mReader->IsActiveReader(aReader);
}

} // namespace mozilla
