/* -*- mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaSourceDecoder.h"

#include "mozilla/Logging.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "MediaDecoderStateMachine.h"
#include "MediaSource.h"
#include "MediaSourceReader.h"
#include "MediaSourceResource.h"
#include "MediaSourceUtils.h"
#include "SourceBufferDecoder.h"
#include "VideoUtils.h"

extern PRLogModuleInfo* GetMediaSourceLog();

#define MSE_DEBUG(arg, ...) MOZ_LOG(GetMediaSourceLog(), mozilla::LogLevel::Debug, ("MediaSourceDecoder(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define MSE_DEBUGV(arg, ...) MOZ_LOG(GetMediaSourceLog(), mozilla::LogLevel::Verbose, ("MediaSourceDecoder(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

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
  MOZ_ASSERT(!GetStateMachine());
  SetStateMachine(CreateStateMachine());
  if (!GetStateMachine()) {
    NS_WARNING("Failed to create state machine!");
    return NS_ERROR_FAILURE;
  }

  nsresult rv = GetStateMachine()->Init(nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  SetStateMachineParameters();
  return ScheduleStateMachine();
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
    media::TimeIntervals buffered = mReader->GetBuffered();
    if (buffered.Length()) {
      seekable += media::TimeInterval(buffered.GetStart(), buffered.GetEnd());
    }
  } else {
    seekable += media::TimeInterval(media::TimeUnit::FromSeconds(0),
                                    media::TimeUnit::FromSeconds(duration));
  }
  MSE_DEBUG("ranges=%s", DumpTimeRanges(seekable).get());
  return seekable;
}

void
MediaSourceDecoder::Shutdown()
{
  MSE_DEBUG("Shutdown");
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
  MOZ_ASSERT(!mMediaSource && !GetStateMachine() && NS_IsMainThread());
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
MediaSourceDecoder::Ended(bool aEnded)
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  static_cast<MediaSourceResource*>(GetResource())->SetEnded(aEnded);
  mReader->Ended(aEnded);
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
  DurationChangedRunnable(MediaSourceDecoder* aDecoder,
                          double aOldDuration,
                          double aNewDuration)
    : mDecoder(aDecoder)
    , mOldDuration(aOldDuration)
    , mNewDuration(aNewDuration)
  { }

  NS_IMETHOD Run() override final {
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
MediaSourceDecoder::SetInitialDuration(int64_t aDuration)
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
  SetMediaSourceDuration(duration, MSRangeRemovalAction::SKIP);
}

void
MediaSourceDecoder::SetMediaSourceDuration(double aDuration, MSRangeRemovalAction aAction)
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  double oldDuration = mMediaSourceDuration;
  if (aDuration >= 0) {
    int64_t checkedDuration;
    if (NS_FAILED(SecondsToUsecs(aDuration, checkedDuration))) {
      // INT64_MAX is used as infinity by the state machine.
      // We want a very bigger number, but not infinity.
      checkedDuration = INT64_MAX - 1;
    }
    GetStateMachine()->SetDuration(checkedDuration);
    mMediaSourceDuration = aDuration;
  } else {
    GetStateMachine()->SetDuration(INT64_MAX);
    mMediaSourceDuration = PositiveInfinity<double>();
  }
  if (mReader) {
    mReader->SetMediaSourceDuration(mMediaSourceDuration);
  }
  ScheduleDurationChange(oldDuration, aDuration, aAction);
}

void
MediaSourceDecoder::ScheduleDurationChange(double aOldDuration,
                                           double aNewDuration,
                                           MSRangeRemovalAction aAction)
{
  if (aAction == MSRangeRemovalAction::SKIP) {
    if (NS_IsMainThread()) {
      MediaDecoder::DurationChanged();
    } else {
      nsCOMPtr<nsIRunnable> task =
        NS_NewRunnableMethod(this, &MediaDecoder::DurationChanged);
      NS_DispatchToMainThread(task);
    }
  } else {
    if (NS_IsMainThread()) {
      DurationChanged(aOldDuration, aNewDuration);
    } else {
      nsCOMPtr<nsIRunnable> task =
        new DurationChangedRunnable(this, aOldDuration, aNewDuration);
      NS_DispatchToMainThread(task);
    }
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

void
MediaSourceDecoder::GetMozDebugReaderData(nsAString& aString)
{
  mReader->GetMozDebugReaderData(aString);
}

#ifdef MOZ_EME
nsresult
MediaSourceDecoder::SetCDMProxy(CDMProxy* aProxy)
{
  nsresult rv = MediaDecoder::SetCDMProxy(aProxy);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mReader->SetCDMProxy(aProxy);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aProxy) {
    // The sub readers can't decrypt EME content until they have a CDMProxy,
    // and the CDMProxy knows the capabilities of the CDM. The MediaSourceReader
    // remains in "waiting for resources" state until then. We need to kick the
    // reader out of waiting if the CDM gets added with known capabilities.
    CDMCaps::AutoLock caps(aProxy->Capabilites());
    if (!caps.AreCapsKnown()) {
      nsCOMPtr<nsIRunnable> task(
        NS_NewRunnableMethod(this, &MediaDecoder::NotifyWaitingForResourcesStatusChanged));
      caps.CallOnMainThreadWhenCapsAvailable(task);
    }
  }
  return NS_OK;
}
#endif

bool
MediaSourceDecoder::IsActiveReader(MediaDecoderReader* aReader)
{
  return mReader->IsActiveReader(aReader);
}

double
MediaSourceDecoder::GetDuration()
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  return mMediaSourceDuration;
}

already_AddRefed<SourceBufferDecoder>
MediaSourceDecoder::SelectDecoder(int64_t aTarget,
                                  int64_t aTolerance,
                                  const nsTArray<nsRefPtr<SourceBufferDecoder>>& aTrackDecoders)
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

  media::TimeUnit target{media::TimeUnit::FromMicroseconds(aTarget)};
  media::TimeUnit tolerance{media::TimeUnit::FromMicroseconds(aTolerance + aTarget)};

  // aTolerance gives a slight bias toward the start of a range only.
  // Consider decoders in order of newest to oldest, as a newer decoder
  // providing a given buffered range is expected to replace an older one.
  for (int32_t i = aTrackDecoders.Length() - 1; i >= 0; --i) {
    nsRefPtr<SourceBufferDecoder> newDecoder = aTrackDecoders[i];

    media::TimeIntervals ranges = newDecoder->GetBuffered();
    for (uint32_t j = 0; j < ranges.Length(); j++) {
      if (target < ranges.End(j) && tolerance >= ranges.Start(j)) {
        return newDecoder.forget();
      }
    }
    MSE_DEBUGV("SelectDecoder(%lld fuzz:%lld) newDecoder=%p (%d/%d) target not in ranges=%s",
               aTarget, aTolerance, newDecoder.get(), i+1,
               aTrackDecoders.Length(), DumpTimeRanges(ranges).get());
  }

  return nullptr;
}

#undef MSE_DEBUG
#undef MSE_DEBUGV

} // namespace mozilla
