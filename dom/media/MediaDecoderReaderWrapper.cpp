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
{
  // Must support either heuristic buffering or WaitForData().
  MOZ_ASSERT(mReader->UseBufferingHeuristics() ||
             mReader->IsWaitForDataSupported());
}

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

RefPtr<MediaDecoderReaderWrapper::AudioDataPromise>
MediaDecoderReaderWrapper::RequestAudioData()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mShutdown);

  int64_t startTime = StartTime().ToMicroseconds();
  return InvokeAsync(mReader->OwnerThread(), mReader.get(),
                     __func__, &MediaDecoderReader::RequestAudioData)
    ->Then(mOwnerThread, __func__,
           [startTime] (RefPtr<AudioData> aAudio) {
             aAudio->AdjustForStartTime(startTime);
             return AudioDataPromise::CreateAndResolve(aAudio.forget(), __func__);
           },
           [] (const MediaResult& aError) {
             return AudioDataPromise::CreateAndReject(aError, __func__);
           });
}

RefPtr<MediaDecoderReaderWrapper::VideoDataPromise>
MediaDecoderReaderWrapper::RequestVideoData(const media::TimeUnit& aTimeThreshold)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mShutdown);

  const auto threshold = aTimeThreshold > media::TimeUnit::Zero()
                         ? aTimeThreshold + StartTime()
                         : aTimeThreshold;

  int64_t startTime = StartTime().ToMicroseconds();
  return InvokeAsync(
    mReader->OwnerThread(), mReader.get(), __func__,
    &MediaDecoderReader::RequestVideoData, threshold)
  ->Then(mOwnerThread, __func__,
         [startTime] (RefPtr<VideoData> aVideo) {
           aVideo->AdjustForStartTime(startTime);
           return VideoDataPromise::CreateAndResolve(aVideo.forget(), __func__);
         },
         [] (const MediaResult& aError) {
           return VideoDataPromise::CreateAndReject(aError, __func__);
         });
}

RefPtr<MediaDecoderReader::SeekPromise>
MediaDecoderReaderWrapper::Seek(const SeekTarget& aTarget)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  SeekTarget adjustedTarget = aTarget;
  adjustedTarget.SetTime(adjustedTarget.GetTime() + StartTime());
  return InvokeAsync(
           mReader->OwnerThread(), mReader.get(), __func__,
           &MediaDecoderReader::Seek,
           Move(adjustedTarget));
}

RefPtr<MediaDecoderReaderWrapper::WaitForDataPromise>
MediaDecoderReaderWrapper::WaitForData(MediaData::Type aType)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(mReader->IsWaitForDataSupported());
  return InvokeAsync(mReader->OwnerThread(), mReader.get(), __func__,
                     &MediaDecoderReader::WaitForData, aType);
}

void
MediaDecoderReaderWrapper::ReleaseResources()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod("MediaDecoderReader::ReleaseResources",
                      mReader,
                      &MediaDecoderReader::ReleaseResources);
  mReader->OwnerThread()->Dispatch(r.forget());
}

void
MediaDecoderReaderWrapper::ResetDecode(TrackSet aTracks)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod<TrackSet>("MediaDecoderReader::ResetDecode",
                                mReader,
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

RefPtr<MediaDecoderReaderWrapper::MetadataPromise>
MediaDecoderReaderWrapper::OnMetadataRead(MetadataHolder&& aMetadata)
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

RefPtr<MediaDecoderReaderWrapper::MetadataPromise>
MediaDecoderReaderWrapper::OnMetadataNotRead(const MediaResult& aError)
{
  return MetadataPromise::CreateAndReject(aError, __func__);
}

void
MediaDecoderReaderWrapper::SetVideoBlankDecode(bool aIsBlankDecode)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod<bool>("MediaDecoderReader::SetVideoNullDecode",
                            mReader,
                            &MediaDecoderReader::SetVideoNullDecode,
                            aIsBlankDecode);
  mReader->OwnerThread()->Dispatch(r.forget());
}

} // namespace mozilla
