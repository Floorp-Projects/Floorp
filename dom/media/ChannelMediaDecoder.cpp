/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChannelMediaDecoder.h"
#include "DecoderTraits.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "BaseMediaResource.h"
#include "MediaShutdownManager.h"

namespace mozilla {

extern LazyLogModule gMediaDecoderLog;
#define LOG(x, ...)                                                            \
  DDMOZ_LOG(gMediaDecoderLog, LogLevel::Debug, x, ##__VA_ARGS__)

ChannelMediaDecoder::ResourceCallback::ResourceCallback(
  AbstractThread* aMainThread)
  : mAbstractMainThread(aMainThread)
{
  MOZ_ASSERT(aMainThread);
  DecoderDoctorLogger::LogConstructionAndBase(
    "ChannelMediaDecoder::ResourceCallback",
    this,
    static_cast<const MediaResourceCallback*>(this));
}

ChannelMediaDecoder::ResourceCallback::~ResourceCallback()
{
  DecoderDoctorLogger::LogDestruction("ChannelMediaDecoder::ResourceCallback",
                                      this);
}

void
ChannelMediaDecoder::ResourceCallback::Connect(ChannelMediaDecoder* aDecoder)
{
  MOZ_ASSERT(NS_IsMainThread());
  mDecoder = aDecoder;
  DecoderDoctorLogger::LinkParentAndChild(
    "ChannelMediaDecoder::ResourceCallback", this, "decoder", mDecoder);
  mTimer = NS_NewTimer(mAbstractMainThread->AsEventTarget());
}

void
ChannelMediaDecoder::ResourceCallback::Disconnect()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoder) {
    DecoderDoctorLogger::UnlinkParentAndChild(
      "ChannelMediaDecoder::ResourceCallback", this, mDecoder);
    mDecoder = nullptr;
    mTimer->Cancel();
    mTimer = nullptr;
  }
}

AbstractThread*
ChannelMediaDecoder::ResourceCallback::AbstractMainThread() const
{
  return mAbstractMainThread;
}

MediaDecoderOwner*
ChannelMediaDecoder::ResourceCallback::GetMediaOwner() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mDecoder ? mDecoder->GetOwner() : nullptr;
}

void
ChannelMediaDecoder::ResourceCallback::NotifyNetworkError(
  const MediaResult& aError)
{
  MOZ_ASSERT(NS_IsMainThread());
  DDLOGEX2("ChannelMediaDecoder::ResourceCallback",
           this,
           DDLogCategory::Log,
           "network_error",
           aError);
  if (mDecoder) {
    mDecoder->NetworkError(aError);
  }
}

/* static */ void
ChannelMediaDecoder::ResourceCallback::TimerCallback(nsITimer* aTimer,
                                                     void* aClosure)
{
  MOZ_ASSERT(NS_IsMainThread());
  ResourceCallback* thiz = static_cast<ResourceCallback*>(aClosure);
  MOZ_ASSERT(thiz->mDecoder);
  thiz->mDecoder->NotifyReaderDataArrived();
  thiz->mTimerArmed = false;
}

void
ChannelMediaDecoder::ResourceCallback::NotifyDataArrived()
{
  MOZ_ASSERT(NS_IsMainThread());
  DDLOGEX2("ChannelMediaDecoder::ResourceCallback",
           this,
           DDLogCategory::Log,
           "data_arrived",
           true);

  if (!mDecoder) {
    return;
  }

  mDecoder->DownloadProgressed();

  if (mTimerArmed) {
    return;
  }
  // In situations where these notifications come from stochastic network
  // activity, we can save significant computation by throttling the
  // calls to MediaDecoder::NotifyDataArrived() which will update the buffer
  // ranges of the reader.
  mTimerArmed = true;
  mTimer->InitWithNamedFuncCallback(
    TimerCallback, this, sDelay, nsITimer::TYPE_ONE_SHOT,
    "ChannelMediaDecoder::ResourceCallback::TimerCallback");
}

void
ChannelMediaDecoder::ResourceCallback::NotifyDataEnded(nsresult aStatus)
{
  DDLOGEX2("ChannelMediaDecoder::ResourceCallback",
           this,
           DDLogCategory::Log,
           "data_ended",
           aStatus);
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoder) {
    mDecoder->NotifyDownloadEnded(aStatus);
  }
}

void
ChannelMediaDecoder::ResourceCallback::NotifyPrincipalChanged()
{
  MOZ_ASSERT(NS_IsMainThread());
  DDLOGEX2("ChannelMediaDecoder::ResourceCallback",
           this,
           DDLogCategory::Log,
           "principal_changed",
           true);
  if (mDecoder) {
    mDecoder->NotifyPrincipalChanged();
  }
}

void
ChannelMediaDecoder::ResourceCallback::NotifySuspendedStatusChanged(
  bool aSuspendedByCache)
{
  MOZ_ASSERT(NS_IsMainThread());
  DDLOGEX2("ChannelMediaDecoder::ResourceCallback",
           this,
           DDLogCategory::Log,
           "suspended_status_changed",
           aSuspendedByCache);
  MediaDecoderOwner* owner = GetMediaOwner();
  if (owner) {
    AbstractThread::AutoEnter context(owner->AbstractMainThread());
    owner->NotifySuspendedByCache(aSuspendedByCache);
  }
}

ChannelMediaDecoder::ChannelMediaDecoder(MediaDecoderInit& aInit)
  : MediaDecoder(aInit)
  , mResourceCallback(new ResourceCallback(aInit.mOwner->AbstractMainThread()))
{
  mResourceCallback->Connect(this);
}

/* static */
already_AddRefed<ChannelMediaDecoder>
ChannelMediaDecoder::Create(MediaDecoderInit& aInit,
                            DecoderDoctorDiagnostics* aDiagnostics)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<ChannelMediaDecoder> decoder;

  const MediaContainerType& type = aInit.mContainerType;
  if (DecoderTraits::IsSupportedType(type)) {
    decoder = new ChannelMediaDecoder(aInit);
    return decoder.forget();
  }

  if (DecoderTraits::IsHttpLiveStreamingType(type)) {
    // We don't have an HLS decoder.
    Telemetry::Accumulate(Telemetry::MEDIA_HLS_DECODER_SUCCESS, false);
  }

  return nullptr;
}

bool
ChannelMediaDecoder::CanClone()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mResource && mResource->CanClone();
}

already_AddRefed<ChannelMediaDecoder>
ChannelMediaDecoder::Clone(MediaDecoderInit& aInit)
{
  if (!mResource || !DecoderTraits::IsSupportedType(aInit.mContainerType)) {
    return nullptr;
  }
  RefPtr<ChannelMediaDecoder> decoder = new ChannelMediaDecoder(aInit);
  if (!decoder) {
    return nullptr;
  }
  nsresult rv = decoder->Load(mResource);
  if (NS_FAILED(rv)) {
    decoder->Shutdown();
    return nullptr;
  }
  return decoder.forget();
}

MediaDecoderStateMachine* ChannelMediaDecoder::CreateStateMachine()
{
  MOZ_ASSERT(NS_IsMainThread());
  MediaFormatReaderInit init;
  init.mVideoFrameContainer = GetVideoFrameContainer();
  init.mKnowsCompositor = GetCompositor();
  init.mCrashHelper = GetOwner()->CreateGMPCrashHelper();
  init.mFrameStats = mFrameStats;
  init.mResource = mResource;
  init.mMediaDecoderOwnerID = mOwner;
  mReader = DecoderTraits::CreateReader(ContainerType(), init);
  return new MediaDecoderStateMachine(this, mReader);
}

void
ChannelMediaDecoder::Shutdown()
{
  mResourceCallback->Disconnect();
  MediaDecoder::Shutdown();

  // Force any outstanding seek and byterange requests to complete
  // to prevent shutdown from deadlocking.
  if (mResource) {
    mResource->Close();
  }
}

nsresult
ChannelMediaDecoder::Load(nsIChannel* aChannel,
                          bool aIsPrivateBrowsing,
                          nsIStreamListener** aStreamListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mResource);
  MOZ_ASSERT(aStreamListener);

  mResource =
    BaseMediaResource::Create(mResourceCallback, aChannel, aIsPrivateBrowsing);
  if (!mResource) {
    return NS_ERROR_FAILURE;
  }
  DDLINKCHILD("resource", mResource.get());

  nsresult rv = MediaShutdownManager::Instance().Register(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mResource->Open(aStreamListener);
  NS_ENSURE_SUCCESS(rv, rv);

  SetStateMachine(CreateStateMachine());
  NS_ENSURE_TRUE(GetStateMachine(), NS_ERROR_FAILURE);

  return InitializeStateMachine();
}

nsresult
ChannelMediaDecoder::Load(BaseMediaResource* aOriginal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mResource);

  mResource = aOriginal->CloneData(mResourceCallback);
  if (!mResource) {
    return NS_ERROR_FAILURE;
  }
  DDLINKCHILD("resource", mResource.get());

  nsresult rv = MediaShutdownManager::Instance().Register(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  SetStateMachine(CreateStateMachine());
  NS_ENSURE_TRUE(GetStateMachine(), NS_ERROR_FAILURE);

  return InitializeStateMachine();
}

void
ChannelMediaDecoder::NotifyDownloadEnded(nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  AbstractThread::AutoEnter context(AbstractMainThread());

  LOG("NotifyDownloadEnded, status=%" PRIx32, static_cast<uint32_t>(aStatus));

  MediaDecoderOwner* owner = GetOwner();
  if (NS_SUCCEEDED(aStatus) || aStatus == NS_BASE_STREAM_CLOSED) {
    nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction("ChannelMediaDecoder::UpdatePlaybackRate", [
        rate = ComputePlaybackRate(),
        res = RefPtr<BaseMediaResource>(mResource)
      ]() { UpdatePlaybackRate(rate, res); });
    nsresult rv = GetStateMachine()->OwnerThread()->Dispatch(r.forget());
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    owner->DownloadSuspended();
    // NotifySuspendedStatusChanged will tell the element that download
    // has been suspended "by the cache", which is true since we never
    // download anything. The element can then transition to HAVE_ENOUGH_DATA.
    owner->NotifySuspendedByCache(true);
  } else if (aStatus == NS_BINDING_ABORTED) {
    // Download has been cancelled by user.
    owner->LoadAborted();
  } else {
    NetworkError(MediaResult(aStatus, "Download aborted"));
  }
}

bool
ChannelMediaDecoder::CanPlayThroughImpl()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mCanPlayThrough;
}

bool
ChannelMediaDecoder::IsLiveStream()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mResource->IsLiveStream();
}

void
ChannelMediaDecoder::OnPlaybackEvent(MediaPlaybackEvent&& aEvent)
{
  MOZ_ASSERT(NS_IsMainThread());
  switch (aEvent.mType) {
    case MediaPlaybackEvent::PlaybackStarted:
      mPlaybackPosition = aEvent.mData.as<int64_t>();
      mPlaybackStatistics.Start();
      break;
    case MediaPlaybackEvent::PlaybackProgressed: {
      int64_t newPos = aEvent.mData.as<int64_t>();
      mPlaybackStatistics.AddBytes(newPos - mPlaybackPosition);
      mPlaybackPosition = newPos;
      break;
    }
    case MediaPlaybackEvent::PlaybackStopped: {
      int64_t newPos = aEvent.mData.as<int64_t>();
      mPlaybackStatistics.AddBytes(newPos - mPlaybackPosition);
      mPlaybackPosition = newPos;
      mPlaybackStatistics.Stop();
      break;
    }
    default:
      break;
  }
  MediaDecoder::OnPlaybackEvent(Move(aEvent));
}

void
ChannelMediaDecoder::DurationChanged()
{
  MOZ_ASSERT(NS_IsMainThread());
  AbstractThread::AutoEnter context(AbstractMainThread());
  MediaDecoder::DurationChanged();
  // Duration has changed so we should recompute playback rate
  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableFunction("ChannelMediaDecoder::UpdatePlaybackRate", [
      rate = ComputePlaybackRate(),
      res = RefPtr<BaseMediaResource>(mResource)
    ]() { UpdatePlaybackRate(rate, res); });
  nsresult rv = GetStateMachine()->OwnerThread()->Dispatch(r.forget());
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
}

void
ChannelMediaDecoder::DownloadProgressed()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  AbstractThread::AutoEnter context(AbstractMainThread());
  GetOwner()->DownloadProgressed();

  using StatsPromise = MozPromise<MediaStatistics, bool, true>;
  InvokeAsync(GetStateMachine()->OwnerThread(),
              __func__,
              [
                rate = ComputePlaybackRate(),
                res = RefPtr<BaseMediaResource>(mResource),
                pos = mPlaybackPosition
              ]() {
                UpdatePlaybackRate(rate, res);
                MediaStatistics stats = GetStatistics(rate, res, pos);
                return StatsPromise::CreateAndResolve(stats, __func__);
              })
    ->Then(
      mAbstractMainThread,
      __func__,
      [ =, self = RefPtr<ChannelMediaDecoder>(this) ](MediaStatistics aStats) {
        if (IsShutdown()) {
          return;
        }
        mCanPlayThrough = aStats.CanPlayThrough();
        GetStateMachine()->DispatchCanPlayThrough(mCanPlayThrough);
        mResource->ThrottleReadahead(ShouldThrottleDownload(aStats));
      },
      []() { MOZ_ASSERT_UNREACHABLE("Promise not resolved"); });
}

ChannelMediaDecoder::PlaybackRateInfo
ChannelMediaDecoder::ComputePlaybackRate()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mResource);

  int64_t length = mResource->GetLength();
  if (mozilla::IsFinite<double>(mDuration) && mDuration > 0 && length >= 0) {
    return { uint32_t(length / mDuration), true };
  }

  bool reliable = false;
  uint32_t rate = mPlaybackStatistics.GetRate(&reliable);
  return { rate, reliable };
}

/* static */ void
ChannelMediaDecoder::UpdatePlaybackRate(const PlaybackRateInfo& aInfo,
                                        BaseMediaResource* aResource)
{
  MOZ_ASSERT(!NS_IsMainThread());

  uint32_t rate = aInfo.mRate;

  if (aInfo.mReliable) {
    // Avoid passing a zero rate
    rate = std::max(rate, 1u);
  } else {
    // Set a minimum rate of 10,000 bytes per second ... sometimes we just
    // don't have good data
    rate = std::max(rate, 10000u);
  }

  aResource->SetPlaybackRate(rate);
}

/* static */ MediaStatistics
ChannelMediaDecoder::GetStatistics(const PlaybackRateInfo& aInfo,
                                   BaseMediaResource* aRes,
                                   int64_t aPlaybackPosition)
{
  MOZ_ASSERT(!NS_IsMainThread());

  MediaStatistics result;
  result.mDownloadRate = aRes->GetDownloadRate(&result.mDownloadRateReliable);
  result.mDownloadPosition = aRes->GetCachedDataEnd(aPlaybackPosition);
  result.mTotalBytes = aRes->GetLength();
  result.mPlaybackRate = aInfo.mRate;
  result.mPlaybackRateReliable = aInfo.mReliable;
  result.mPlaybackPosition = aPlaybackPosition;
  return result;
}

bool
ChannelMediaDecoder::ShouldThrottleDownload(const MediaStatistics& aStats)
{
  // We throttle the download if either the throttle override pref is set
  // (so that we can always throttle in Firefox on mobile) or if the download
  // is fast enough that there's no concern about playback being interrupted.
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(GetStateMachine(), false);

  int64_t length = aStats.mTotalBytes;
  if (length > 0 &&
      length <= int64_t(MediaPrefs::MediaMemoryCacheMaxSize()) * 1024) {
    // Don't throttle the download of small resources. This is to speed
    // up seeking, as seeks into unbuffered ranges would require starting
    // up a new HTTP transaction, which adds latency.
    return false;
  }

  if (Preferences::GetBool("media.throttle-regardless-of-download-rate",
                           false)) {
    return true;
  }

  if (!aStats.mDownloadRateReliable || !aStats.mPlaybackRateReliable) {
    return false;
  }
  uint32_t factor =
    std::max(2u, Preferences::GetUint("media.throttle-factor", 2));
  return aStats.mDownloadRate > factor * aStats.mPlaybackRate;
}

void
ChannelMediaDecoder::AddSizeOfResources(ResourceSizes* aSizes)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mResource) {
    aSizes->mByteSize += mResource->SizeOfIncludingThis(aSizes->mMallocSizeOf);
  }
}

already_AddRefed<nsIPrincipal>
ChannelMediaDecoder::GetCurrentPrincipal()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mResource ? mResource->GetCurrentPrincipal() : nullptr;
}

bool
ChannelMediaDecoder::IsTransportSeekable()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mResource->IsTransportSeekable();
}

void
ChannelMediaDecoder::SetLoadInBackground(bool aLoadInBackground)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mResource) {
    mResource->SetLoadInBackground(aLoadInBackground);
  }
}

void
ChannelMediaDecoder::Suspend()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mResource) {
    mResource->Suspend(true);
  }
}

void
ChannelMediaDecoder::Resume()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mResource) {
    mResource->Resume();
  }
}

void
ChannelMediaDecoder::PinForSeek()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mResource || mPinnedForSeek) {
    return;
  }
  mPinnedForSeek = true;
  mResource->Pin();
}

void
ChannelMediaDecoder::UnpinForSeek()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  if (!mResource || !mPinnedForSeek) {
    return;
  }
  mPinnedForSeek = false;
  mResource->Unpin();
}

void
ChannelMediaDecoder::MetadataLoaded(
  UniquePtr<MediaInfo> aInfo,
  UniquePtr<MetadataTags> aTags,
  MediaDecoderEventVisibility aEventVisibility)
{
  MediaDecoder::MetadataLoaded(Move(aInfo), Move(aTags), aEventVisibility);
  // Set mode to PLAYBACK after reading metadata.
  mResource->SetReadMode(MediaCacheStream::MODE_PLAYBACK);
}

nsCString
ChannelMediaDecoder::GetDebugInfo()
{
  auto&& str = MediaDecoder::GetDebugInfo();
  if (mResource) {
    AppendStringIfNotEmpty(str, mResource->GetDebugInfo());
  }
  return str;
}

} // namespace mozilla

// avoid redefined macro in unified build
#undef LOG
