/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MozPromise.h"
#include "MediaDecoderReaderWrapper.h"

namespace mozilla {

MediaDecoderReaderWrapper::MediaDecoderReaderWrapper(AbstractThread* aOwnerThread,
                                                     MediaDecoderReader* aReader)
  : mOwnerThread(aOwnerThread)
  , mReader(aReader)
{}

MediaDecoderReaderWrapper::~MediaDecoderReaderWrapper()
{}

media::TimeUnit
MediaDecoderReaderWrapper::StartTime() const
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mShutdown);
  return mStartTime.ref();
}

RefPtr<MediaDecoderReaderWrapper::MetadataPromise>
MediaDecoderReaderWrapper::ReadMetadata()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mShutdown);
  return InvokeAsync(mReader->OwnerThread(), mReader.get(), __func__,
                     &MediaDecoderReader::AsyncReadMetadata)
         ->Then(mOwnerThread, __func__, this,
                &MediaDecoderReaderWrapper::OnMetadataRead,
                &MediaDecoderReaderWrapper::OnMetadataNotRead)
         ->CompletionPromise();
}

void
MediaDecoderReaderWrapper::RequestAudioData()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mShutdown);

  auto p = InvokeAsync(mReader->OwnerThread(), mReader.get(), __func__,
                       &MediaDecoderReader::RequestAudioData);

  RefPtr<MediaDecoderReaderWrapper> self = this;
  mAudioDataRequest.Begin(p->Then(mOwnerThread, __func__,
    [self] (MediaData* aAudioSample) {
      self->mAudioDataRequest.Complete();
      aAudioSample->AdjustForStartTime(self->StartTime().ToMicroseconds());
      self->mAudioCallback.Notify(AsVariant(aAudioSample));
    },
    [self] (const MediaResult& aError) {
      self->mAudioDataRequest.Complete();
      self->mAudioCallback.Notify(AsVariant(aError));
    }));
}

void
MediaDecoderReaderWrapper::RequestVideoData(bool aSkipToNextKeyframe,
                                            media::TimeUnit aTimeThreshold)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mShutdown);

  // Time the video decode and send this value back to callbacks who accept
  // a TimeStamp as its second parameter.
  TimeStamp videoDecodeStartTime = TimeStamp::Now();

  if (aTimeThreshold.ToMicroseconds() > 0) {
    aTimeThreshold += StartTime();
  }

  auto p = InvokeAsync(mReader->OwnerThread(), mReader.get(), __func__,
                       &MediaDecoderReader::RequestVideoData,
                       aSkipToNextKeyframe, aTimeThreshold.ToMicroseconds());

  RefPtr<MediaDecoderReaderWrapper> self = this;
  mVideoDataRequest.Begin(p->Then(mOwnerThread, __func__,
    [self, videoDecodeStartTime] (MediaData* aVideoSample) {
      self->mVideoDataRequest.Complete();
      aVideoSample->AdjustForStartTime(self->StartTime().ToMicroseconds());
      self->mVideoCallback.Notify(AsVariant(MakeTuple(aVideoSample, videoDecodeStartTime)));
    },
    [self] (const MediaResult& aError) {
      self->mVideoDataRequest.Complete();
      self->mVideoCallback.Notify(AsVariant(aError));
    }));
}

bool
MediaDecoderReaderWrapper::IsRequestingAudioData() const
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  return mAudioDataRequest.Exists();
}

bool
MediaDecoderReaderWrapper::IsRequestingVideoData() const
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  return mVideoDataRequest.Exists();
}

bool
MediaDecoderReaderWrapper::IsWaitingAudioData() const
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  return mAudioWaitRequest.Exists();
}

bool
MediaDecoderReaderWrapper::IsWaitingVideoData() const
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  return mVideoWaitRequest.Exists();
}

RefPtr<MediaDecoderReader::SeekPromise>
MediaDecoderReaderWrapper::Seek(const SeekTarget& aTarget,
                                const media::TimeUnit& aEndTime)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  SeekTarget adjustedTarget = aTarget;
  adjustedTarget.SetTime(adjustedTarget.GetTime() + StartTime());
  return InvokeAsync<SeekTarget&&, int64_t>(
           mReader->OwnerThread(), mReader.get(), __func__,
           &MediaDecoderReader::Seek,
           Move(adjustedTarget), aEndTime.ToMicroseconds());
}

void
MediaDecoderReaderWrapper::WaitForData(MediaData::Type aType)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());

  auto p = InvokeAsync(mReader->OwnerThread(), mReader.get(), __func__,
                       &MediaDecoderReader::WaitForData, aType);

  RefPtr<MediaDecoderReaderWrapper> self = this;
  WaitRequestRef(aType).Begin(p->Then(mOwnerThread, __func__,
    [self] (MediaData::Type aType) {
      self->WaitRequestRef(aType).Complete();
      self->WaitCallbackRef(aType).Notify(AsVariant(aType));
    },
    [self, aType] (WaitForDataRejectValue aRejection) {
      self->WaitRequestRef(aType).Complete();
      self->WaitCallbackRef(aType).Notify(AsVariant(aRejection));
    }));
}

MediaCallbackExc<WaitCallbackData>&
MediaDecoderReaderWrapper::WaitCallbackRef(MediaData::Type aType)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  return aType == MediaData::AUDIO_DATA ? mAudioWaitCallback : mVideoWaitCallback;
}

MozPromiseRequestHolder<MediaDecoderReader::WaitForDataPromise>&
MediaDecoderReaderWrapper::WaitRequestRef(MediaData::Type aType)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  return aType == MediaData::AUDIO_DATA ? mAudioWaitRequest : mVideoWaitRequest;
}

RefPtr<MediaDecoderReaderWrapper::BufferedUpdatePromise>
MediaDecoderReaderWrapper::UpdateBufferedWithPromise()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  return InvokeAsync(mReader->OwnerThread(), mReader.get(), __func__,
                     &MediaDecoderReader::UpdateBufferedWithPromise);
}

void
MediaDecoderReaderWrapper::ReleaseResources()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod(mReader, &MediaDecoderReader::ReleaseResources);
  mReader->OwnerThread()->Dispatch(r.forget());
}

void
MediaDecoderReaderWrapper::ResetDecode(TrackSet aTracks)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());

  if (aTracks.contains(TrackInfo::kAudioTrack)) {
    mAudioDataRequest.DisconnectIfExists();
    mAudioWaitRequest.DisconnectIfExists();
  }

  if (aTracks.contains(TrackInfo::kVideoTrack)) {
    mVideoDataRequest.DisconnectIfExists();
    mVideoWaitRequest.DisconnectIfExists();
  }

  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod<TrackSet>(mReader,
                                &MediaDecoderReader::ResetDecode,
                                aTracks);
  mReader->OwnerThread()->Dispatch(r.forget());
}

RefPtr<ShutdownPromise>
MediaDecoderReaderWrapper::Shutdown()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mAudioDataRequest.Exists());
  MOZ_ASSERT(!mVideoDataRequest.Exists());

  mShutdown = true;
  return InvokeAsync(mReader->OwnerThread(), mReader.get(), __func__,
                     &MediaDecoderReader::Shutdown);
}

void
MediaDecoderReaderWrapper::OnMetadataRead(MetadataHolder* aMetadata)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  if (mShutdown) {
    return;
  }

  if (mStartTime.isNothing()) {
    mStartTime.emplace(aMetadata->mInfo.mStartTime);
    // Note: MFR should be able to setup its start time by itself without going
    // through here. MediaDecoderReader::DispatchSetStartTime() will be removed
    // once we remove all the legacy readers' code in the following bugs.
    mReader->DispatchSetStartTime(StartTime().ToMicroseconds());
  }
}

void
MediaDecoderReaderWrapper::SetVideoBlankDecode(bool aIsBlankDecode)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod<bool>(mReader, &MediaDecoderReader::SetVideoBlankDecode,
                            aIsBlankDecode);
  mReader->OwnerThread()->Dispatch(r.forget());
}

} // namespace mozilla
