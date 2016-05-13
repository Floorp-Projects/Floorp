/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MozPromise.h"
#include "MediaDecoderReaderWrapper.h"

namespace mozilla {

extern LazyLogModule gMediaDecoderLog;

#undef LOG
#define LOG(...) \
  MOZ_LOG(gMediaDecoderLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

// StartTimeRendezvous is a helper class that quarantines the first sample
// until it gets a sample from both channels, such that we can be guaranteed
// to know the start time by the time On{Audio,Video}Decoded is called on MDSM.
class StartTimeRendezvous {
  typedef MediaDecoderReader::MediaDataPromise MediaDataPromise;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(StartTimeRendezvous);

public:
  StartTimeRendezvous(AbstractThread* aOwnerThread,
                      bool aHasAudio,
                      bool aHasVideo,
                      bool aForceZeroStartTime)
    : mOwnerThread(aOwnerThread)
  {
    if (aForceZeroStartTime) {
      mAudioStartTime.emplace(0);
      mVideoStartTime.emplace(0);
      return;
    }
    if (!aHasAudio) {
      mAudioStartTime.emplace(INT64_MAX);
    }
    if (!aHasVideo) {
      mVideoStartTime.emplace(INT64_MAX);
    }
  }

  void Destroy()
  {
    mAudioStartTime = Some(mAudioStartTime.refOr(INT64_MAX));
    mVideoStartTime = Some(mVideoStartTime.refOr(INT64_MAX));
    mHaveStartTimePromise.RejectIfExists(false, __func__);
  }

  RefPtr<HaveStartTimePromise> AwaitStartTime()
  {
    if (HaveStartTime()) {
      return HaveStartTimePromise::CreateAndResolve(true, __func__);
    }
    return mHaveStartTimePromise.Ensure(__func__);
  }

  template<MediaData::Type SampleType>
  RefPtr<MediaDataPromise>
  ProcessFirstSample(MediaData* aData)
  {
    typedef typename MediaDataPromise::Private PromisePrivate;
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());

    MaybeSetChannelStartTime<SampleType>(aData->mTime);

    RefPtr<PromisePrivate> p = new PromisePrivate(__func__);
    RefPtr<MediaData> data = aData;
    RefPtr<StartTimeRendezvous> self = this;
    AwaitStartTime()->Then(
      mOwnerThread, __func__,
      [p, data, self] () {
        MOZ_ASSERT(self->mOwnerThread->IsCurrentThreadIn());
        p->Resolve(data, __func__);
      },
      [p] () {
        p->Reject(MediaDecoderReader::CANCELED, __func__);
      });

    return p.forget();
  }

  template<MediaData::Type SampleType>
  void FirstSampleRejected(MediaDecoderReader::NotDecodedReason aReason)
  {
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
    if (aReason == MediaDecoderReader::DECODE_ERROR) {
      mHaveStartTimePromise.RejectIfExists(false, __func__);
    } else if (aReason == MediaDecoderReader::END_OF_STREAM) {
      LOG("StartTimeRendezvous=%p SampleType(%d) Has no samples.",
           this, SampleType);
      MaybeSetChannelStartTime<SampleType>(INT64_MAX);
    }
  }

  bool HaveStartTime() const
  {
    return mAudioStartTime.isSome() && mVideoStartTime.isSome();
  }

  int64_t StartTime() const
  {
    int64_t time = std::min(mAudioStartTime.ref(), mVideoStartTime.ref());
    return time == INT64_MAX ? 0 : time;
  }

private:
  ~StartTimeRendezvous() {}

  template<MediaData::Type SampleType>
  void MaybeSetChannelStartTime(int64_t aStartTime)
  {
    if (ChannelStartTime(SampleType).isSome()) {
      // If we're initialized with aForceZeroStartTime=true, the channel start
      // times are already set.
      return;
    }

    LOG("StartTimeRendezvous=%p Setting SampleType(%d) start time to %lld",
        this, SampleType, aStartTime);

    ChannelStartTime(SampleType).emplace(aStartTime);
    if (HaveStartTime()) {
      mHaveStartTimePromise.ResolveIfExists(true, __func__);
    }
  }

  Maybe<int64_t>& ChannelStartTime(MediaData::Type aType)
  {
    return aType == MediaData::AUDIO_DATA ? mAudioStartTime : mVideoStartTime;
  }

  MozPromiseHolder<HaveStartTimePromise> mHaveStartTimePromise;
  RefPtr<AbstractThread> mOwnerThread;
  Maybe<int64_t> mAudioStartTime;
  Maybe<int64_t> mVideoStartTime;
};

MediaDecoderReaderWrapper::MediaDecoderReaderWrapper(bool aIsRealTime,
                                                     AbstractThread* aOwnerThread,
                                                     MediaDecoderReader* aReader)
  : mForceZeroStartTime(aIsRealTime || aReader->ForceZeroStartTime())
  , mOwnerThread(aOwnerThread)
  , mReader(aReader)
  , mAudioCallbackID("AudioCallbackID")
  , mVideoCallbackID("VideoCallbackID")
{}

MediaDecoderReaderWrapper::~MediaDecoderReaderWrapper()
{}

media::TimeUnit
MediaDecoderReaderWrapper::StartTime() const
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mShutdown);
  return media::TimeUnit::FromMicroseconds(mStartTimeRendezvous->StartTime());
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

RefPtr<HaveStartTimePromise>
MediaDecoderReaderWrapper::AwaitStartTime()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mShutdown);
  return mStartTimeRendezvous->AwaitStartTime();
}

void
MediaDecoderReaderWrapper::CancelAudioCallback(CallbackID aID)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(aID == mAudioCallbackID);
  ++mAudioCallbackID;
  mRequestAudioDataCB = nullptr;
}

void
MediaDecoderReaderWrapper::CancelVideoCallback(CallbackID aID)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(aID == mVideoCallbackID);
  ++mVideoCallbackID;
  mRequestVideoDataCB = nullptr;
}

void
MediaDecoderReaderWrapper::RequestAudioData()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mShutdown);
  MOZ_ASSERT(mRequestAudioDataCB, "Request audio data without callback!");

  auto p = InvokeAsync(mReader->OwnerThread(), mReader.get(), __func__,
                       &MediaDecoderReader::RequestAudioData);

  if (!mStartTimeRendezvous->HaveStartTime()) {
    p = p->Then(mOwnerThread, __func__, mStartTimeRendezvous.get(),
                &StartTimeRendezvous::ProcessFirstSample<MediaData::AUDIO_DATA>,
                &StartTimeRendezvous::FirstSampleRejected<MediaData::AUDIO_DATA>)
         ->CompletionPromise();
  }

  RefPtr<MediaDecoderReaderWrapper> self = this;
  mAudioDataRequest.Begin(p->Then(mOwnerThread, __func__,
    [self] (MediaData* aAudioSample) {
      MOZ_ASSERT(self->mRequestAudioDataCB);
      self->mAudioDataRequest.Complete();
      self->OnSampleDecoded(self->mRequestAudioDataCB.get(), aAudioSample, TimeStamp());
    },
    [self] (MediaDecoderReader::NotDecodedReason aReason) {
      MOZ_ASSERT(self->mRequestAudioDataCB);
      self->mAudioDataRequest.Complete();
      self->OnNotDecoded(self->mRequestAudioDataCB.get(), aReason);
    }));
}

void
MediaDecoderReaderWrapper::RequestVideoData(bool aSkipToNextKeyframe,
                                            media::TimeUnit aTimeThreshold)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mShutdown);
  MOZ_ASSERT(mRequestVideoDataCB, "Request video data without callback!");

  // Time the video decode and send this value back to callbacks who accept
  // a TimeStamp as its second parameter.
  TimeStamp videoDecodeStartTime = TimeStamp::Now();

  if (aTimeThreshold.ToMicroseconds() > 0 &&
      mStartTimeRendezvous->HaveStartTime()) {
    aTimeThreshold += StartTime();
  }

  auto p = InvokeAsync(mReader->OwnerThread(), mReader.get(), __func__,
                       &MediaDecoderReader::RequestVideoData,
                       aSkipToNextKeyframe, aTimeThreshold.ToMicroseconds());

  if (!mStartTimeRendezvous->HaveStartTime()) {
    p = p->Then(mOwnerThread, __func__, mStartTimeRendezvous.get(),
                &StartTimeRendezvous::ProcessFirstSample<MediaData::VIDEO_DATA>,
                &StartTimeRendezvous::FirstSampleRejected<MediaData::VIDEO_DATA>)
         ->CompletionPromise();
  }

  RefPtr<MediaDecoderReaderWrapper> self = this;
  mVideoDataRequest.Begin(p->Then(mOwnerThread, __func__,
    [self, videoDecodeStartTime] (MediaData* aVideoSample) {
      MOZ_ASSERT(self->mRequestVideoDataCB);
      self->mVideoDataRequest.Complete();
      self->OnSampleDecoded(self->mRequestVideoDataCB.get(), aVideoSample, videoDecodeStartTime);
    },
    [self] (MediaDecoderReader::NotDecodedReason aReason) {
      MOZ_ASSERT(self->mRequestVideoDataCB);
      self->mVideoDataRequest.Complete();
      self->OnNotDecoded(self->mRequestVideoDataCB.get(), aReason);
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

RefPtr<MediaDecoderReader::SeekPromise>
MediaDecoderReaderWrapper::Seek(SeekTarget aTarget, media::TimeUnit aEndTime)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  aTarget.SetTime(aTarget.GetTime() + StartTime());
  return InvokeAsync(mReader->OwnerThread(), mReader.get(), __func__,
                     &MediaDecoderReader::Seek, aTarget,
                     aEndTime.ToMicroseconds());
}

RefPtr<MediaDecoderReaderWrapper::WaitForDataPromise>
MediaDecoderReaderWrapper::WaitForData(MediaData::Type aType)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  return InvokeAsync(mReader->OwnerThread(), mReader.get(), __func__,
                     &MediaDecoderReader::WaitForData, aType);
}

RefPtr<MediaDecoderReaderWrapper::BufferedUpdatePromise>
MediaDecoderReaderWrapper::UpdateBufferedWithPromise()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  return InvokeAsync(mReader->OwnerThread(), mReader.get(), __func__,
                     &MediaDecoderReader::UpdateBufferedWithPromise);
}

void
MediaDecoderReaderWrapper::ReleaseMediaResources()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod(mReader, &MediaDecoderReader::ReleaseMediaResources);
  mReader->OwnerThread()->Dispatch(r.forget());
}

void
MediaDecoderReaderWrapper::SetIdle()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod(mReader, &MediaDecoderReader::SetIdle);
  mReader->OwnerThread()->Dispatch(r.forget());
}

void
MediaDecoderReaderWrapper::ResetDecode(TargetQueues aQueues)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());

  mAudioDataRequest.DisconnectIfExists();
  mVideoDataRequest.DisconnectIfExists();

  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod<TargetQueues>(mReader,
                                    &MediaDecoderReader::ResetDecode,
                                    aQueues);
  mReader->OwnerThread()->Dispatch(r.forget());
}

RefPtr<ShutdownPromise>
MediaDecoderReaderWrapper::Shutdown()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mRequestAudioDataCB);
  MOZ_ASSERT(!mRequestVideoDataCB);
  MOZ_ASSERT(!mAudioDataRequest.Exists());
  MOZ_ASSERT(!mVideoDataRequest.Exists());

  mShutdown = true;
  if (mStartTimeRendezvous) {
    mStartTimeRendezvous->Destroy();
    mStartTimeRendezvous = nullptr;
  }
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
  // Set up the start time rendezvous if it doesn't already exist (which is
  // generally the case, unless we're coming out of dormant mode).
  if (!mStartTimeRendezvous) {
    mStartTimeRendezvous = new StartTimeRendezvous(
      mOwnerThread, aMetadata->mInfo.HasAudio(),
      aMetadata->mInfo.HasVideo(), mForceZeroStartTime);

    RefPtr<MediaDecoderReaderWrapper> self = this;
    mStartTimeRendezvous->AwaitStartTime()->Then(
      mOwnerThread, __func__,
      [self] ()  {
        NS_ENSURE_TRUE_VOID(!self->mShutdown);
        self->mReader->DispatchSetStartTime(self->StartTime().ToMicroseconds());
      },
      [] () {
        NS_WARNING("Setting start time on reader failed");
      });
  }
}

void
MediaDecoderReaderWrapper::OnSampleDecoded(CallbackBase* aCallback,
                                           MediaData* aSample,
                                           TimeStamp aDecodeStartTime)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mShutdown);

  aSample->AdjustForStartTime(StartTime().ToMicroseconds());
  aCallback->OnResolved(aSample, aDecodeStartTime);
}

void
MediaDecoderReaderWrapper::OnNotDecoded(CallbackBase* aCallback,
                                        MediaDecoderReader::NotDecodedReason aReason)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  MOZ_ASSERT(!mShutdown);

  aCallback->OnRejected(aReason);
}

} // namespace mozilla
