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
                &MediaDecoderReaderWrapper::OnMetadataNotRead);
}

RefPtr<MediaDecoderReaderWrapper::MediaDataPromise>
MediaDecoderReaderWrapper::RequestAudioData()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mShutdown);

  int64_t startTime = StartTime().ToMicroseconds();
  return InvokeAsync(mReader->OwnerThread(), mReader.get(),
                     __func__, &MediaDecoderReader::RequestAudioData)
    ->Then(mOwnerThread, __func__,
           [startTime] (MediaData* aAudio) {
             aAudio->AdjustForStartTime(startTime);
           },
           [] (const MediaResult& aError) {});
}

RefPtr<MediaDecoderReaderWrapper::MediaDataPromise>
MediaDecoderReaderWrapper::RequestVideoData(bool aSkipToNextKeyframe,
                                            media::TimeUnit aTimeThreshold)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mShutdown);

  if (aTimeThreshold.ToMicroseconds() > 0) {
    aTimeThreshold += StartTime();
  }

  int64_t startTime = StartTime().ToMicroseconds();
  return InvokeAsync(mReader->OwnerThread(), mReader.get(), __func__,
                     &MediaDecoderReader::RequestVideoData,
                     aSkipToNextKeyframe, aTimeThreshold.ToMicroseconds())
    ->Then(mOwnerThread, __func__,
           [startTime] (MediaData* aVideo) {
             aVideo->AdjustForStartTime(startTime);
           },
           [] (const MediaResult& aError) {});
}

RefPtr<MediaDecoderReader::SeekPromise>
MediaDecoderReaderWrapper::Seek(const SeekTarget& aTarget)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  SeekTarget adjustedTarget = aTarget;
  adjustedTarget.SetTime(adjustedTarget.GetTime() + StartTime());
  return InvokeAsync<SeekTarget&&>(
           mReader->OwnerThread(), mReader.get(), __func__,
           &MediaDecoderReader::Seek,
           Move(adjustedTarget));
}

RefPtr<MediaDecoderReaderWrapper::WaitForDataPromise>
MediaDecoderReaderWrapper::WaitForData(MediaData::Type aType)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  return InvokeAsync(mReader->OwnerThread(), mReader.get(), __func__,
                     &MediaDecoderReader::WaitForData, aType);
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
