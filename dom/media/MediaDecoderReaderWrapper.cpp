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
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(StartTimeRendezvous);
  typedef MediaDecoderReader::AudioDataPromise AudioDataPromise;
  typedef MediaDecoderReader::VideoDataPromise VideoDataPromise;

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

  template<typename PromiseType>
  struct PromiseSampleType {
    typedef typename PromiseType::ResolveValueType::element_type Type;
  };

  template<typename PromiseType, MediaData::Type SampleType>
  RefPtr<PromiseType>
  ProcessFirstSample(typename PromiseSampleType<PromiseType>::Type* aData)
  {
    typedef typename PromiseSampleType<PromiseType>::Type DataType;
    typedef typename PromiseType::Private PromisePrivate;
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());

    MaybeSetChannelStartTime<SampleType>(aData->mTime);

    RefPtr<PromisePrivate> p = new PromisePrivate(__func__);
    RefPtr<DataType> data = aData;
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
{}

MediaDecoderReaderWrapper::~MediaDecoderReaderWrapper()
{}

media::TimeUnit
MediaDecoderReaderWrapper::StartTime() const
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  return media::TimeUnit::FromMicroseconds(mStartTimeRendezvous->StartTime());
}

RefPtr<MediaDecoderReaderWrapper::MetadataPromise>
MediaDecoderReaderWrapper::ReadMetadata()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
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
  return mStartTimeRendezvous->AwaitStartTime();
}

RefPtr<MediaDecoderReaderWrapper::AudioDataPromise>
MediaDecoderReaderWrapper::RequestAudioData()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());

  auto p = InvokeAsync(mReader->OwnerThread(), mReader.get(), __func__,
                       &MediaDecoderReader::RequestAudioData);

  if (!mStartTimeRendezvous->HaveStartTime()) {
    p = p->Then(mOwnerThread, __func__, mStartTimeRendezvous.get(),
                &StartTimeRendezvous::ProcessFirstSample<AudioDataPromise, MediaData::AUDIO_DATA>,
                &StartTimeRendezvous::FirstSampleRejected<MediaData::AUDIO_DATA>)
         ->CompletionPromise();
  }

  return p->Then(mOwnerThread, __func__, this,
                 &MediaDecoderReaderWrapper::OnSampleDecoded,
                 &MediaDecoderReaderWrapper::OnNotDecoded)
          ->CompletionPromise();
}

RefPtr<MediaDecoderReaderWrapper::VideoDataPromise>
MediaDecoderReaderWrapper::RequestVideoData(bool aSkipToNextKeyframe,
                                            media::TimeUnit aTimeThreshold)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());

  if (aTimeThreshold.ToMicroseconds() > 0 &&
      mStartTimeRendezvous->HaveStartTime()) {
    aTimeThreshold += StartTime();
  }

  auto p = InvokeAsync(mReader->OwnerThread(), mReader.get(), __func__,
                       &MediaDecoderReader::RequestVideoData,
                       aSkipToNextKeyframe, aTimeThreshold.ToMicroseconds());

  if (!mStartTimeRendezvous->HaveStartTime()) {
    p = p->Then(mOwnerThread, __func__, mStartTimeRendezvous.get(),
                &StartTimeRendezvous::ProcessFirstSample<VideoDataPromise, MediaData::VIDEO_DATA>,
                &StartTimeRendezvous::FirstSampleRejected<MediaData::VIDEO_DATA>)
         ->CompletionPromise();
  }

  return p->Then(mOwnerThread, __func__, this,
                 &MediaDecoderReaderWrapper::OnSampleDecoded,
                 &MediaDecoderReaderWrapper::OnNotDecoded)
          ->CompletionPromise();
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

void
MediaDecoderReaderWrapper::Shutdown()
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  mShutdown = true;
  if (mStartTimeRendezvous) {
    mStartTimeRendezvous->Destroy();
  }
}

void
MediaDecoderReaderWrapper::OnMetadataRead(MetadataHolder* aMetadata)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
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
MediaDecoderReaderWrapper::OnSampleDecoded(MediaData* aSample)
{
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  aSample->AdjustForStartTime(StartTime().ToMicroseconds());
}

} // namespace mozilla
