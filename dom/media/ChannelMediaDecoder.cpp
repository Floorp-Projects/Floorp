/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChannelMediaDecoder.h"
#include "ChannelMediaResource.h"
#include "DecoderTraits.h"
#include "ExternalEngineStateMachine.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "BaseMediaResource.h"
#include "MediaShutdownManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_media.h"
#include "VideoUtils.h"

namespace mozilla {

extern LazyLogModule gMediaDecoderLog;
#define LOG(x, ...) \
  DDMOZ_LOG(gMediaDecoderLog, LogLevel::Debug, x, ##__VA_ARGS__)

ChannelMediaDecoder::ResourceCallback::ResourceCallback(
    AbstractThread* aMainThread)
    : mAbstractMainThread(aMainThread) {
  MOZ_ASSERT(aMainThread);
  DecoderDoctorLogger::LogConstructionAndBase(
      "ChannelMediaDecoder::ResourceCallback", this,
      static_cast<const MediaResourceCallback*>(this));
}

ChannelMediaDecoder::ResourceCallback::~ResourceCallback() {
  DecoderDoctorLogger::LogDestruction("ChannelMediaDecoder::ResourceCallback",
                                      this);
}

void ChannelMediaDecoder::ResourceCallback::Connect(
    ChannelMediaDecoder* aDecoder) {
  MOZ_ASSERT(NS_IsMainThread());
  mDecoder = aDecoder;
  DecoderDoctorLogger::LinkParentAndChild(
      "ChannelMediaDecoder::ResourceCallback", this, "decoder", mDecoder);
  mTimer = NS_NewTimer(mAbstractMainThread->AsEventTarget());
}

void ChannelMediaDecoder::ResourceCallback::Disconnect() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoder) {
    DecoderDoctorLogger::UnlinkParentAndChild(
        "ChannelMediaDecoder::ResourceCallback", this, mDecoder);
    mDecoder = nullptr;
    mTimer->Cancel();
    mTimer = nullptr;
  }
}

AbstractThread* ChannelMediaDecoder::ResourceCallback::AbstractMainThread()
    const {
  return mAbstractMainThread;
}

MediaDecoderOwner* ChannelMediaDecoder::ResourceCallback::GetMediaOwner()
    const {
  MOZ_ASSERT(NS_IsMainThread());
  return mDecoder ? mDecoder->GetOwner() : nullptr;
}

void ChannelMediaDecoder::ResourceCallback::NotifyNetworkError(
    const MediaResult& aError) {
  MOZ_ASSERT(NS_IsMainThread());
  DDLOGEX2("ChannelMediaDecoder::ResourceCallback", this, DDLogCategory::Log,
           "network_error", aError);
  if (mDecoder) {
    mDecoder->NetworkError(aError);
  }
}

/* static */
void ChannelMediaDecoder::ResourceCallback::TimerCallback(nsITimer* aTimer,
                                                          void* aClosure) {
  MOZ_ASSERT(NS_IsMainThread());
  ResourceCallback* thiz = static_cast<ResourceCallback*>(aClosure);
  MOZ_ASSERT(thiz->mDecoder);
  thiz->mDecoder->NotifyReaderDataArrived();
  thiz->mTimerArmed = false;
}

void ChannelMediaDecoder::ResourceCallback::NotifyDataArrived() {
  MOZ_ASSERT(NS_IsMainThread());
  DDLOGEX2("ChannelMediaDecoder::ResourceCallback", this, DDLogCategory::Log,
           "data_arrived", true);

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

void ChannelMediaDecoder::ResourceCallback::NotifyDataEnded(nsresult aStatus) {
  DDLOGEX2("ChannelMediaDecoder::ResourceCallback", this, DDLogCategory::Log,
           "data_ended", aStatus);
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoder) {
    mDecoder->NotifyDownloadEnded(aStatus);
  }
}

void ChannelMediaDecoder::ResourceCallback::NotifyPrincipalChanged() {
  MOZ_ASSERT(NS_IsMainThread());
  DDLOGEX2("ChannelMediaDecoder::ResourceCallback", this, DDLogCategory::Log,
           "principal_changed", true);
  if (mDecoder) {
    mDecoder->NotifyPrincipalChanged();
  }
}

void ChannelMediaDecoder::NotifyPrincipalChanged() {
  MOZ_ASSERT(NS_IsMainThread());
  MediaDecoder::NotifyPrincipalChanged();
  if (!mInitialChannelPrincipalKnown) {
    // We'll receive one notification when the channel's initial principal
    // is known, after all HTTP redirects have resolved. This isn't really a
    // principal change, so return here to avoid the mSameOriginMedia check
    // below.
    mInitialChannelPrincipalKnown = true;
    return;
  }
  if (!mSameOriginMedia) {
    // Block mid-flight redirects to non CORS same origin destinations.
    // See bugs 1441153, 1443942.
    LOG("ChannnelMediaDecoder prohibited cross origin redirect blocked.");
    NetworkError(MediaResult(NS_ERROR_DOM_BAD_URI,
                             "Prohibited cross origin redirect blocked"));
  }
}

void ChannelMediaDecoder::ResourceCallback::NotifySuspendedStatusChanged(
    bool aSuspendedByCache) {
  MOZ_ASSERT(NS_IsMainThread());
  DDLOGEX2("ChannelMediaDecoder::ResourceCallback", this, DDLogCategory::Log,
           "suspended_status_changed", aSuspendedByCache);
  MediaDecoderOwner* owner = GetMediaOwner();
  if (owner) {
    owner->NotifySuspendedByCache(aSuspendedByCache);
  }
}

ChannelMediaDecoder::ChannelMediaDecoder(MediaDecoderInit& aInit)
    : MediaDecoder(aInit),
      mResourceCallback(
          new ResourceCallback(aInit.mOwner->AbstractMainThread())) {
  mResourceCallback->Connect(this);
}

/* static */
already_AddRefed<ChannelMediaDecoder> ChannelMediaDecoder::Create(
    MediaDecoderInit& aInit, DecoderDoctorDiagnostics* aDiagnostics) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<ChannelMediaDecoder> decoder;
  if (DecoderTraits::CanHandleContainerType(aInit.mContainerType,
                                            aDiagnostics) != CANPLAY_NO) {
    decoder = new ChannelMediaDecoder(aInit);
    return decoder.forget();
  }

  return nullptr;
}

bool ChannelMediaDecoder::CanClone() {
  MOZ_ASSERT(NS_IsMainThread());
  return mResource && mResource->CanClone();
}

already_AddRefed<ChannelMediaDecoder> ChannelMediaDecoder::Clone(
    MediaDecoderInit& aInit) {
  if (!mResource || DecoderTraits::CanHandleContainerType(
                        aInit.mContainerType, nullptr) == CANPLAY_NO) {
    return nullptr;
  }
  RefPtr<ChannelMediaDecoder> decoder = new ChannelMediaDecoder(aInit);
  nsresult rv = decoder->Load(mResource);
  if (NS_FAILED(rv)) {
    decoder->Shutdown();
    return nullptr;
  }
  return decoder.forget();
}

MediaDecoderStateMachineBase* ChannelMediaDecoder::CreateStateMachine(
    bool aDisableExternalEngine) {
  MOZ_ASSERT(NS_IsMainThread());
  MediaFormatReaderInit init;
  init.mVideoFrameContainer = GetVideoFrameContainer();
  init.mKnowsCompositor = GetCompositor();
  init.mCrashHelper = GetOwner()->CreateGMPCrashHelper();
  init.mFrameStats = mFrameStats;
  init.mResource = mResource;
  init.mMediaDecoderOwnerID = mOwner;
  mReader = DecoderTraits::CreateReader(ContainerType(), init);

#ifdef MOZ_WMF_MEDIA_ENGINE
  // TODO : Only for testing development for now. In the future this should be
  // used for encrypted content only.
  if (StaticPrefs::media_wmf_media_engine_enabled() &&
      StaticPrefs::media_wmf_media_engine_channel_decoder_enabled() &&
      !aDisableExternalEngine) {
    return new ExternalEngineStateMachine(this, mReader);
  }
#endif
  return new MediaDecoderStateMachine(this, mReader);
}

void ChannelMediaDecoder::Shutdown() {
  mResourceCallback->Disconnect();
  MediaDecoder::Shutdown();

  if (mResource) {
    // Force any outstanding seek and byterange requests to complete
    // to prevent shutdown from deadlocking.
    mResourceClosePromise = mResource->Close();
  }
}

void ChannelMediaDecoder::ShutdownInternal() {
  if (!mResourceClosePromise) {
    MediaShutdownManager::Instance().Unregister(this);
    return;
  }

  mResourceClosePromise->Then(
      AbstractMainThread(), __func__,
      [self = RefPtr<ChannelMediaDecoder>(this)] {
        MediaShutdownManager::Instance().Unregister(self);
      });
}

nsresult ChannelMediaDecoder::Load(nsIChannel* aChannel,
                                   bool aIsPrivateBrowsing,
                                   nsIStreamListener** aStreamListener) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mResource);
  MOZ_ASSERT(aStreamListener);

  mResource = BaseMediaResource::Create(mResourceCallback, aChannel,
                                        aIsPrivateBrowsing);
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
  return CreateAndInitStateMachine(mResource->IsLiveStream());
}

nsresult ChannelMediaDecoder::Load(BaseMediaResource* aOriginal) {
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
  return CreateAndInitStateMachine(mResource->IsLiveStream());
}

void ChannelMediaDecoder::NotifyDownloadEnded(nsresult aStatus) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  LOG("NotifyDownloadEnded, status=%" PRIx32, static_cast<uint32_t>(aStatus));

  if (NS_SUCCEEDED(aStatus)) {
    // Download ends successfully. This is a stream with a finite length.
    GetStateMachine()->DispatchIsLiveStream(false);
  }

  MediaDecoderOwner* owner = GetOwner();
  if (NS_SUCCEEDED(aStatus) || aStatus == NS_BASE_STREAM_CLOSED) {
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
        "ChannelMediaDecoder::UpdatePlaybackRate",
        [stats = mPlaybackStatistics,
         res = RefPtr<BaseMediaResource>(mResource), duration = mDuration]() {
          auto rate = ComputePlaybackRate(stats, res, duration);
          UpdatePlaybackRate(rate, res);
        });
    nsresult rv = GetStateMachine()->OwnerThread()->Dispatch(r.forget());
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
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

bool ChannelMediaDecoder::CanPlayThroughImpl() {
  MOZ_ASSERT(NS_IsMainThread());
  return mCanPlayThrough;
}

void ChannelMediaDecoder::OnPlaybackEvent(MediaPlaybackEvent&& aEvent) {
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
  MediaDecoder::OnPlaybackEvent(std::move(aEvent));
}

void ChannelMediaDecoder::DurationChanged() {
  MOZ_ASSERT(NS_IsMainThread());
  MediaDecoder::DurationChanged();
  // Duration has changed so we should recompute playback rate
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "ChannelMediaDecoder::UpdatePlaybackRate",
      [stats = mPlaybackStatistics, res = RefPtr<BaseMediaResource>(mResource),
       duration = mDuration]() {
        auto rate = ComputePlaybackRate(stats, res, duration);
        UpdatePlaybackRate(rate, res);
      });
  nsresult rv = GetStateMachine()->OwnerThread()->Dispatch(r.forget());
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  Unused << rv;
}

void ChannelMediaDecoder::DownloadProgressed() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  GetOwner()->DownloadProgressed();

  using StatsPromise = MozPromise<MediaStatistics, bool, true>;
  InvokeAsync(GetStateMachine()->OwnerThread(), __func__,
              [playbackStats = mPlaybackStatistics,
               res = RefPtr<BaseMediaResource>(mResource), duration = mDuration,
               pos = mPlaybackPosition]() {
                auto rate = ComputePlaybackRate(playbackStats, res, duration);
                UpdatePlaybackRate(rate, res);
                MediaStatistics stats = GetStatistics(rate, res, pos);
                return StatsPromise::CreateAndResolve(stats, __func__);
              })
      ->Then(
          mAbstractMainThread, __func__,
          [=,
           self = RefPtr<ChannelMediaDecoder>(this)](MediaStatistics aStats) {
            if (IsShutdown()) {
              return;
            }
            mCanPlayThrough = aStats.CanPlayThrough();
            GetStateMachine()->DispatchCanPlayThrough(mCanPlayThrough);
            mResource->ThrottleReadahead(ShouldThrottleDownload(aStats));
            // Update readyState since mCanPlayThrough might have changed.
            GetOwner()->UpdateReadyState();
          },
          []() { MOZ_ASSERT_UNREACHABLE("Promise not resolved"); });
}

/* static */ ChannelMediaDecoder::PlaybackRateInfo
ChannelMediaDecoder::ComputePlaybackRate(const MediaChannelStatistics& aStats,
                                         BaseMediaResource* aResource,
                                         double aDuration) {
  MOZ_ASSERT(!NS_IsMainThread());

  int64_t length = aResource->GetLength();
  if (mozilla::IsFinite<double>(aDuration) && aDuration > 0 && length >= 0 &&
      length / aDuration < UINT32_MAX) {
    return {uint32_t(length / aDuration), true};
  }

  bool reliable = false;
  uint32_t rate = aStats.GetRate(&reliable);
  return {rate, reliable};
}

/* static */
void ChannelMediaDecoder::UpdatePlaybackRate(const PlaybackRateInfo& aInfo,
                                             BaseMediaResource* aResource) {
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

/* static */
MediaStatistics ChannelMediaDecoder::GetStatistics(
    const PlaybackRateInfo& aInfo, BaseMediaResource* aRes,
    int64_t aPlaybackPosition) {
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

bool ChannelMediaDecoder::ShouldThrottleDownload(
    const MediaStatistics& aStats) {
  // We throttle the download if either the throttle override pref is set
  // (so that we always throttle at the readahead limit on mobile if using
  // a cellular network) or if the download is fast enough that there's no
  // concern about playback being interrupted.
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(GetStateMachine(), false);

  int64_t length = aStats.mTotalBytes;
  if (length > 0 &&
      length <= int64_t(StaticPrefs::media_memory_cache_max_size()) * 1024) {
    // Don't throttle the download of small resources. This is to speed
    // up seeking, as seeks into unbuffered ranges would require starting
    // up a new HTTP transaction, which adds latency.
    return false;
  }

  if (OnCellularConnection() &&
      Preferences::GetBool(
          "media.throttle-cellular-regardless-of-download-rate", false)) {
    return true;
  }

  if (!aStats.mDownloadRateReliable || !aStats.mPlaybackRateReliable) {
    return false;
  }
  uint32_t factor =
      std::max(2u, Preferences::GetUint("media.throttle-factor", 2));
  return aStats.mDownloadRate > factor * aStats.mPlaybackRate;
}

void ChannelMediaDecoder::AddSizeOfResources(ResourceSizes* aSizes) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mResource) {
    aSizes->mByteSize += mResource->SizeOfIncludingThis(aSizes->mMallocSizeOf);
  }
}

already_AddRefed<nsIPrincipal> ChannelMediaDecoder::GetCurrentPrincipal() {
  MOZ_ASSERT(NS_IsMainThread());
  return mResource ? mResource->GetCurrentPrincipal() : nullptr;
}

bool ChannelMediaDecoder::HadCrossOriginRedirects() {
  MOZ_ASSERT(NS_IsMainThread());
  return mResource ? mResource->HadCrossOriginRedirects() : false;
}

bool ChannelMediaDecoder::IsTransportSeekable() {
  MOZ_ASSERT(NS_IsMainThread());
  return mResource->IsTransportSeekable();
}

void ChannelMediaDecoder::SetLoadInBackground(bool aLoadInBackground) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mResource) {
    mResource->SetLoadInBackground(aLoadInBackground);
  }
}

void ChannelMediaDecoder::Suspend() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mResource) {
    mResource->Suspend(true);
  }
  MediaDecoder::Suspend();
}

void ChannelMediaDecoder::Resume() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mResource) {
    mResource->Resume();
  }
  MediaDecoder::Resume();
}

void ChannelMediaDecoder::MetadataLoaded(
    UniquePtr<MediaInfo> aInfo, UniquePtr<MetadataTags> aTags,
    MediaDecoderEventVisibility aEventVisibility) {
  MediaDecoder::MetadataLoaded(std::move(aInfo), std::move(aTags),
                               aEventVisibility);
  // Set mode to PLAYBACK after reading metadata.
  mResource->SetReadMode(MediaCacheStream::MODE_PLAYBACK);
}

void ChannelMediaDecoder::GetDebugInfo(dom::MediaDecoderDebugInfo& aInfo) {
  MediaDecoder::GetDebugInfo(aInfo);
  if (mResource) {
    mResource->GetDebugInfo(aInfo.mResource);
  }
}

}  // namespace mozilla

// avoid redefined macro in unified build
#undef LOG
