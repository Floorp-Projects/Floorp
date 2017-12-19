/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MozPromise.h"
#include "MediaFormatReader.h"
#include "ReaderProxy.h"
#include "TimeUnits.h"

namespace mozilla {

ReaderProxy::ReaderProxy(AbstractThread* aOwnerThread,
                         MediaFormatReader* aReader)
  : mOwnerThread(aOwnerThread)
  , mReader(aReader)
  , mWatchManager(this, aReader->OwnerThread())
  , mDuration(aReader->OwnerThread(),
              media::NullableTimeUnit(),
              "ReaderProxy::mDuration (Mirror)")
  , mSeamlessLoopingBlocked(false)
  , mSeamlessLoopingEnabled(false)
{
  // Must support either heuristic buffering or WaitForData().
  MOZ_ASSERT(mReader->UseBufferingHeuristics() ||
             mReader->IsWaitForDataSupported());
}

ReaderProxy::~ReaderProxy()
{}

media::TimeUnit
ReaderProxy::StartTime() const
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  return mStartTime.ref();
}

RefPtr<ReaderProxy::MetadataPromise>
ReaderProxy::ReadMetadata()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mShutdown);
  return InvokeAsync(mReader->OwnerThread(),
                     mReader.get(),
                     __func__,
                     &MediaFormatReader::AsyncReadMetadata)
    ->Then(mOwnerThread,
           __func__,
           this,
           &ReaderProxy::OnMetadataRead,
           &ReaderProxy::OnMetadataNotRead);
}

RefPtr<ReaderProxy::AudioDataPromise>
ReaderProxy::OnAudioDataRequestCompleted(RefPtr<AudioData> aAudio)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());

  // Subtract the start time and add the looping-offset time.
  int64_t offset =
    StartTime().ToMicroseconds() - mLoopingOffset.ToMicroseconds();
  aAudio->AdjustForStartTime(offset);
  if (aAudio->mTime.IsValid()) {
    mLastAudioEndTime = aAudio->mTime;
    return AudioDataPromise::CreateAndResolve(aAudio.forget(), __func__);
  }
  return AudioDataPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
                                           __func__);
}

RefPtr<ReaderProxy::AudioDataPromise>
ReaderProxy::OnAudioDataRequestFailed(const MediaResult& aError)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());

  if (mSeamlessLoopingBlocked || !mSeamlessLoopingEnabled ||
      aError.Code() != NS_ERROR_DOM_MEDIA_END_OF_STREAM) {
    return AudioDataPromise::CreateAndReject(aError, __func__);
  }

  // The data time in the audio queue is assumed to be increased linearly,
  // so we need to add the last ending time as the offset to correct the
  // audio data time in the next round when seamless looping is enabled.
  mLoopingOffset = mLastAudioEndTime;

  // Save the duration of the audio track if it hasn't been set.
  if (!mAudioDuration.IsValid()) {
    mAudioDuration = mLastAudioEndTime;
  }

  // For seamless looping, the demuxer is sought to the beginning and then
  // keep requesting decoded data in advance, upon receiving EOS.
  // The MDSM will not be aware of the EOS and keep receiving decoded data
  // as usual while looping is on.
  RefPtr<ReaderProxy> self = this;
  RefPtr<MediaFormatReader> reader = mReader;
  ResetDecode(TrackInfo::kAudioTrack);
  return SeekInternal(SeekTarget(media::TimeUnit::Zero(), SeekTarget::Accurate))
    ->Then(mReader->OwnerThread(),
           __func__,
           [reader]() { return reader->RequestAudioData(); },
           [](const SeekRejectValue& aReject) {
             return AudioDataPromise::CreateAndReject(aReject.mError, __func__);
           })
    ->Then(mOwnerThread,
           __func__,
           [self](RefPtr<AudioData> aAudio) {
             return self->OnAudioDataRequestCompleted(aAudio.forget());
           },
           [](const MediaResult& aError) {
             return AudioDataPromise::CreateAndReject(aError, __func__);
           });
}

RefPtr<ReaderProxy::AudioDataPromise>
ReaderProxy::RequestAudioData()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mShutdown);

  mSeamlessLoopingBlocked = false;
  return InvokeAsync(mReader->OwnerThread(),
                     mReader.get(),
                     __func__,
                     &MediaFormatReader::RequestAudioData)
    ->Then(mOwnerThread,
           __func__,
           this,
           &ReaderProxy::OnAudioDataRequestCompleted,
           &ReaderProxy::OnAudioDataRequestFailed);
}

RefPtr<ReaderProxy::VideoDataPromise>
ReaderProxy::RequestVideoData(const media::TimeUnit& aTimeThreshold)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mShutdown);

  mSeamlessLoopingBlocked = false;
  const auto threshold = aTimeThreshold > media::TimeUnit::Zero()
                         ? aTimeThreshold + StartTime()
                         : aTimeThreshold;

  int64_t startTime = StartTime().ToMicroseconds();
  return InvokeAsync(mReader->OwnerThread(),
                     mReader.get(),
                     __func__,
                     &MediaFormatReader::RequestVideoData,
                     threshold)
    ->Then(mOwnerThread,
           __func__,
           [startTime](RefPtr<VideoData> aVideo) {
             aVideo->AdjustForStartTime(startTime);
             return aVideo->mTime.IsValid()
                      ? VideoDataPromise::CreateAndResolve(aVideo.forget(),
                                                           __func__)
                      : VideoDataPromise::CreateAndReject(
                          NS_ERROR_DOM_MEDIA_OVERFLOW_ERR, __func__);
           },
           [](const MediaResult& aError) {
             return VideoDataPromise::CreateAndReject(aError, __func__);
           });
}

RefPtr<ReaderProxy::SeekPromise>
ReaderProxy::Seek(const SeekTarget& aTarget)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  mSeamlessLoopingBlocked = true;
  // Reset the members for seamless looping if the seek is triggered outside.
  mLoopingOffset = media::TimeUnit::Zero();
  mLastAudioEndTime = media::TimeUnit::Zero();
  mAudioDuration = media::TimeUnit::Invalid();
  return SeekInternal(aTarget);
}

RefPtr<ReaderProxy::SeekPromise>
ReaderProxy::SeekInternal(const SeekTarget& aTarget)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  SeekTarget adjustedTarget = aTarget;
  adjustedTarget.SetTime(adjustedTarget.GetTime() + StartTime());
  return InvokeAsync(mReader->OwnerThread(),
                     mReader.get(),
                     __func__,
                     &MediaFormatReader::Seek,
                     Move(adjustedTarget));
}

RefPtr<ReaderProxy::WaitForDataPromise>
ReaderProxy::WaitForData(MediaData::Type aType)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(mReader->IsWaitForDataSupported());
  return InvokeAsync(mReader->OwnerThread(),
                     mReader.get(),
                     __func__,
                     &MediaFormatReader::WaitForData,
                     aType);
}

void
ReaderProxy::ReleaseResources()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod("MediaFormatReader::ReleaseResources",
                      mReader,
                      &MediaFormatReader::ReleaseResources);
  nsresult rv = mReader->OwnerThread()->Dispatch(r.forget());
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
}

void
ReaderProxy::ResetDecode(TrackSet aTracks)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod<TrackSet>("MediaFormatReader::ResetDecode",
                                mReader,
                                &MediaFormatReader::ResetDecode,
                                aTracks);
  nsresult rv = mReader->OwnerThread()->Dispatch(r.forget());
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
}

RefPtr<ShutdownPromise>
ReaderProxy::Shutdown()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  mShutdown = true;
  RefPtr<ReaderProxy> self = this;
  return InvokeAsync(mReader->OwnerThread(), __func__, [self]() {
    self->mDuration.DisconnectIfConnected();
    self->mWatchManager.Shutdown();
    return self->mReader->Shutdown();
  });
}

RefPtr<ReaderProxy::MetadataPromise>
ReaderProxy::OnMetadataRead(MetadataHolder&& aMetadata)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  if (mShutdown) {
    return MetadataPromise::CreateAndReject(
      NS_ERROR_DOM_MEDIA_ABORT_ERR, __func__);
  }

  if (mStartTime.isNothing()) {
    mStartTime.emplace(aMetadata.mInfo->mStartTime);
  }
  return MetadataPromise::CreateAndResolve(Move(aMetadata), __func__);
}

RefPtr<ReaderProxy::MetadataPromise>
ReaderProxy::OnMetadataNotRead(const MediaResult& aError)
{
  return MetadataPromise::CreateAndReject(aError, __func__);
}

void
ReaderProxy::SetVideoBlankDecode(bool aIsBlankDecode)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod<bool>("MediaFormatReader::SetVideoNullDecode",
                            mReader,
                            &MediaFormatReader::SetVideoNullDecode,
                            aIsBlankDecode);
  nsresult rv = mReader->OwnerThread()->Dispatch(r.forget());
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
}

void
ReaderProxy::UpdateDuration()
{
  MOZ_ASSERT(mReader->OwnerThread()->IsCurrentThreadIn());
  mReader->UpdateDuration(mDuration.Ref().ref());
}

void
ReaderProxy::SetCanonicalDuration(
  AbstractCanonical<media::NullableTimeUnit>* aCanonical)
{
  using DurationT = AbstractCanonical<media::NullableTimeUnit>;
  RefPtr<ReaderProxy> self = this;
  RefPtr<DurationT> canonical = aCanonical;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
    "ReaderProxy::SetCanonicalDuration", [this, self, canonical]() {
      mDuration.Connect(canonical);
      mWatchManager.Watch(mDuration, &ReaderProxy::UpdateDuration);
    });
  nsresult rv = mReader->OwnerThread()->Dispatch(r.forget());
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
}

void
ReaderProxy::SetSeamlessLoopingEnabled(bool aEnabled)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  mSeamlessLoopingEnabled = aEnabled;
}

void
ReaderProxy::AdjustByLooping(media::TimeUnit& aTime)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mShutdown);
  MOZ_ASSERT(!mSeamlessLoopingEnabled || !mSeamlessLoopingBlocked);
  if (mAudioDuration.IsValid() && mAudioDuration.IsPositive()) {
    aTime = aTime % mAudioDuration.ToMicroseconds();
  }
}

} // namespace mozilla
