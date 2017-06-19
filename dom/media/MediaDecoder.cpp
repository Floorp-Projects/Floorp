/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDecoder.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/MathAlgorithms.h"
#include <limits>
#include "nsIObserver.h"
#include "nsTArray.h"
#include "VideoUtils.h"
#include "MediaDecoderStateMachine.h"
#include "ImageContainer.h"
#include "MediaResource.h"
#include "VideoFrameContainer.h"
#include "nsError.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"
#include "nsIMemoryReporter.h"
#include "nsComponentManagerUtils.h"
#include <algorithm>
#include "MediaShutdownManager.h"
#include "AudioChannelService.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/dom/AudioTrack.h"
#include "mozilla/dom/AudioTrackList.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/VideoTrack.h"
#include "mozilla/dom/VideoTrackList.h"
#include "nsPrintfCString.h"
#include "mozilla/Telemetry.h"
#include "Layers.h"
#include "mozilla/layers/ShadowLayers.h"

#ifdef MOZ_ANDROID_OMX
#include "AndroidBridge.h"
#endif

using namespace mozilla::dom;
using namespace mozilla::layers;
using namespace mozilla::media;

namespace mozilla {

// avoid redefined macro in unified build
#undef LOG
#undef DUMP

LazyLogModule gMediaDecoderLog("MediaDecoder");
#define LOG(x, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Debug, ("Decoder=%p " x, this, ##__VA_ARGS__))

#define DUMP(x, ...) \
  printf_stderr("%s\n", nsPrintfCString("Decoder=%p " x, this, ##__VA_ARGS__).get())

#define NS_DispatchToMainThread(...) CompileError_UseAbstractMainThreadInstead

static const char*
ToPlayStateStr(MediaDecoder::PlayState aState)
{
  switch (aState) {
    case MediaDecoder::PLAY_STATE_START:    return "START";
    case MediaDecoder::PLAY_STATE_LOADING:  return "LOADING";
    case MediaDecoder::PLAY_STATE_PAUSED:   return "PAUSED";
    case MediaDecoder::PLAY_STATE_PLAYING:  return "PLAYING";
    case MediaDecoder::PLAY_STATE_ENDED:    return "ENDED";
    case MediaDecoder::PLAY_STATE_SHUTDOWN: return "SHUTDOWN";
    default: MOZ_ASSERT_UNREACHABLE("Invalid playState.");
  }
  return "UNKNOWN";
}

class MediaMemoryTracker : public nsIMemoryReporter
{
  virtual ~MediaMemoryTracker();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf);

  MediaMemoryTracker();
  void InitMemoryReporter();

  static StaticRefPtr<MediaMemoryTracker> sUniqueInstance;

  static MediaMemoryTracker* UniqueInstance()
  {
    if (!sUniqueInstance) {
      sUniqueInstance = new MediaMemoryTracker();
      sUniqueInstance->InitMemoryReporter();
    }
    return sUniqueInstance;
  }

  typedef nsTArray<MediaDecoder*> DecodersArray;
  static DecodersArray& Decoders()
  {
    return UniqueInstance()->mDecoders;
  }

  DecodersArray mDecoders;

public:
  static void AddMediaDecoder(MediaDecoder* aDecoder)
  {
    Decoders().AppendElement(aDecoder);
  }

  static void RemoveMediaDecoder(MediaDecoder* aDecoder)
  {
    DecodersArray& decoders = Decoders();
    decoders.RemoveElement(aDecoder);
    if (decoders.IsEmpty()) {
      sUniqueInstance = nullptr;
    }
  }
};

StaticRefPtr<MediaMemoryTracker> MediaMemoryTracker::sUniqueInstance;

LazyLogModule gMediaTimerLog("MediaTimer");

constexpr TimeUnit MediaDecoder::DEFAULT_NEXT_FRAME_AVAILABLE_BUFFERED;

void
MediaDecoder::InitStatics()
{
  MOZ_ASSERT(NS_IsMainThread());
}

NS_IMPL_ISUPPORTS(MediaMemoryTracker, nsIMemoryReporter)

NS_IMPL_ISUPPORTS0(MediaDecoder)

MediaDecoder::ResourceCallback::ResourceCallback(AbstractThread* aMainThread)
  : mAbstractMainThread(aMainThread)
{
  MOZ_ASSERT(aMainThread);
}

void
MediaDecoder::ResourceCallback::Connect(MediaDecoder* aDecoder)
{
  MOZ_ASSERT(NS_IsMainThread());
  mDecoder = aDecoder;
  mTimer = do_CreateInstance("@mozilla.org/timer;1");
  mTimer->SetTarget(mAbstractMainThread->AsEventTarget());
}

void
MediaDecoder::ResourceCallback::Disconnect()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoder) {
    mDecoder = nullptr;
    mTimer->Cancel();
    mTimer = nullptr;
  }
}

MediaDecoderOwner*
MediaDecoder::ResourceCallback::GetMediaOwner() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mDecoder ? mDecoder->GetOwner() : nullptr;
}

void
MediaDecoder::ResourceCallback::SetInfinite(bool aInfinite)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoder) {
    mDecoder->SetInfinite(aInfinite);
  }
}

void
MediaDecoder::ResourceCallback::NotifyNetworkError()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoder) {
    mDecoder->NetworkError();
  }
}

/* static */ void
MediaDecoder::ResourceCallback::TimerCallback(nsITimer* aTimer, void* aClosure)
{
  MOZ_ASSERT(NS_IsMainThread());
  ResourceCallback* thiz = static_cast<ResourceCallback*>(aClosure);
  MOZ_ASSERT(thiz->mDecoder);
  thiz->mDecoder->NotifyDataArrivedInternal();
  thiz->mTimerArmed = false;
}

void
MediaDecoder::ResourceCallback::NotifyDataArrived()
{
  MOZ_ASSERT(NS_IsMainThread());
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
  mTimer->InitWithFuncCallback(
    TimerCallback, this, sDelay, nsITimer::TYPE_ONE_SHOT);
}

void
MediaDecoder::ResourceCallback::NotifyDataEnded(nsresult aStatus)
{
  RefPtr<ResourceCallback> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([=] () {
    if (!self->mDecoder) {
      return;
    }
    self->mDecoder->NotifyDownloadEnded(aStatus);
    if (NS_SUCCEEDED(aStatus)) {
      MediaDecoderOwner* owner = self->GetMediaOwner();
      MOZ_ASSERT(owner);
      owner->DownloadSuspended();

      // NotifySuspendedStatusChanged will tell the element that download
      // has been suspended "by the cache", which is true since we never
      // download anything. The element can then transition to HAVE_ENOUGH_DATA.
      self->mDecoder->NotifySuspendedStatusChanged();
    }
  });
  mAbstractMainThread->Dispatch(r.forget());
}

void
MediaDecoder::ResourceCallback::NotifyPrincipalChanged()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoder) {
    mDecoder->NotifyPrincipalChanged();
  }
}

void
MediaDecoder::ResourceCallback::NotifySuspendedStatusChanged()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoder) {
    mDecoder->NotifySuspendedStatusChanged();
  }
}

void
MediaDecoder::ResourceCallback::NotifyBytesConsumed(int64_t aBytes,
                                                    int64_t aOffset)
{
  RefPtr<ResourceCallback> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([=] () {
    if (self->mDecoder) {
      self->mDecoder->NotifyBytesConsumed(aBytes, aOffset);
    }
  });
  mAbstractMainThread->Dispatch(r.forget());
}

void
MediaDecoder::NotifyOwnerActivityChanged(bool aIsDocumentVisible,
                                         Visibility aElementVisibility,
                                         bool aIsElementInTree)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  SetElementVisibility(aIsDocumentVisible, aElementVisibility, aIsElementInTree);

  NotifyCompositor();
}

void
MediaDecoder::Pause()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  if (mPlayState == PLAY_STATE_LOADING || IsEnded()) {
    mNextState = PLAY_STATE_PAUSED;
    return;
  }
  ChangeState(PLAY_STATE_PAUSED);
}

void
MediaDecoder::SetVolume(double aVolume)
{
  MOZ_ASSERT(NS_IsMainThread());
  mVolume = aVolume;
}

void
MediaDecoder::AddOutputStream(ProcessedMediaStream* aStream,
                              bool aFinishWhenEnded)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDecoderStateMachine, "Must be called after Load().");
  mDecoderStateMachine->AddOutputStream(aStream, aFinishWhenEnded);
}

void
MediaDecoder::RemoveOutputStream(MediaStream* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDecoderStateMachine, "Must be called after Load().");
  mDecoderStateMachine->RemoveOutputStream(aStream);
}

double
MediaDecoder::GetDuration()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mDuration;
}

AbstractCanonical<media::NullableTimeUnit>*
MediaDecoder::CanonicalDurationOrNull()
{
  MOZ_ASSERT(mDecoderStateMachine);
  return mDecoderStateMachine->CanonicalDuration();
}

void
MediaDecoder::SetInfinite(bool aInfinite)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  mInfiniteStream = aInfinite;
  DurationChanged();
}

bool
MediaDecoder::IsInfinite() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mInfiniteStream;
}

#define INIT_MIRROR(name, val) \
  name(mOwner->AbstractMainThread(), val, "MediaDecoder::" #name " (Mirror)")
#define INIT_CANONICAL(name, val) \
  name(mOwner->AbstractMainThread(), val, "MediaDecoder::" #name " (Canonical)")

MediaDecoder::MediaDecoder(MediaDecoderInit& aInit)
  : mWatchManager(this, aInit.mOwner->AbstractMainThread())
  , mLogicalPosition(0.0)
  , mDuration(std::numeric_limits<double>::quiet_NaN())
  , mResourceCallback(new ResourceCallback(aInit.mOwner->AbstractMainThread()))
  , mCDMProxyPromise(mCDMProxyPromiseHolder.Ensure(__func__))
  , mIgnoreProgressData(false)
  , mInfiniteStream(false)
  , mOwner(aInit.mOwner)
  , mAbstractMainThread(aInit.mOwner->AbstractMainThread())
  , mFrameStats(new FrameStatistics())
  , mVideoFrameContainer(aInit.mOwner->GetVideoFrameContainer())
  , mPinnedForSeek(false)
  , mAudioChannel(aInit.mAudioChannel)
  , mMinimizePreroll(aInit.mMinimizePreroll)
  , mFiredMetadataLoaded(false)
  , mIsDocumentVisible(false)
  , mElementVisibility(Visibility::UNTRACKED)
  , mIsElementInTree(false)
  , mForcedHidden(false)
  , mHasSuspendTaint(aInit.mHasSuspendTaint)
  , mPlaybackRate(aInit.mPlaybackRate)
  , INIT_MIRROR(mBuffered, TimeIntervals())
  , INIT_MIRROR(mNextFrameStatus, MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE)
  , INIT_MIRROR(mCurrentPosition, TimeUnit::Zero())
  , INIT_MIRROR(mStateMachineDuration, NullableTimeUnit())
  , INIT_MIRROR(mPlaybackPosition, 0)
  , INIT_MIRROR(mIsAudioDataAudible, false)
  , INIT_CANONICAL(mVolume, aInit.mVolume)
  , INIT_CANONICAL(mPreservesPitch, aInit.mPreservesPitch)
  , INIT_CANONICAL(mLooping, aInit.mLooping)
  , INIT_CANONICAL(mExplicitDuration, Maybe<double>())
  , INIT_CANONICAL(mPlayState, PLAY_STATE_LOADING)
  , INIT_CANONICAL(mNextState, PLAY_STATE_PAUSED)
  , INIT_CANONICAL(mLogicallySeeking, false)
  , INIT_CANONICAL(mSameOriginMedia, false)
  , INIT_CANONICAL(mMediaPrincipalHandle, PRINCIPAL_HANDLE_NONE)
  , INIT_CANONICAL(mPlaybackBytesPerSecond, 0.0)
  , INIT_CANONICAL(mPlaybackRateReliable, true)
  , INIT_CANONICAL(mDecoderPosition, 0)
  , mTelemetryReported(false)
  , mIsMediaElement(!!mOwner->GetMediaElement())
  , mElement(mOwner->GetMediaElement())
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mAbstractMainThread);
  MediaMemoryTracker::AddMediaDecoder(this);

  mResourceCallback->Connect(this);

  //
  // Initialize watchers.
  //

  // mDuration
  mWatchManager.Watch(mStateMachineDuration, &MediaDecoder::DurationChanged);

  // readyState
  mWatchManager.Watch(mPlayState, &MediaDecoder::UpdateReadyState);
  mWatchManager.Watch(mNextFrameStatus, &MediaDecoder::UpdateReadyState);
  // ReadyState computation depends on MediaDecoder::CanPlayThrough, which
  // depends on the download rate.
  mWatchManager.Watch(mBuffered, &MediaDecoder::UpdateReadyState);

  // mLogicalPosition
  mWatchManager.Watch(mCurrentPosition, &MediaDecoder::UpdateLogicalPosition);
  mWatchManager.Watch(mPlayState, &MediaDecoder::UpdateLogicalPosition);
  mWatchManager.Watch(mLogicallySeeking, &MediaDecoder::UpdateLogicalPosition);

  // mIgnoreProgressData
  mWatchManager.Watch(mLogicallySeeking, &MediaDecoder::SeekingChanged);

  mWatchManager.Watch(mIsAudioDataAudible,
                      &MediaDecoder::NotifyAudibleStateChanged);

  MediaShutdownManager::InitStatics();
}

#undef INIT_MIRROR
#undef INIT_CANONICAL

void
MediaDecoder::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  // Unwatch all watch targets to prevent further notifications.
  mWatchManager.Shutdown();

  mResourceCallback->Disconnect();

  mCDMProxyPromiseHolder.RejectIfExists(true, __func__);

  DiscardOngoingSeekIfExists();

  // This changes the decoder state to SHUTDOWN and does other things
  // necessary to unblock the state machine thread if it's blocked, so
  // the asynchronous shutdown in nsDestroyStateMachine won't deadlock.
  if (mDecoderStateMachine) {
    mTimedMetadataListener.Disconnect();
    mMetadataLoadedListener.Disconnect();
    mFirstFrameLoadedListener.Disconnect();
    mOnPlaybackEvent.Disconnect();
    mOnPlaybackErrorEvent.Disconnect();
    mOnDecoderDoctorEvent.Disconnect();
    mOnMediaNotSeekable.Disconnect();

    mDecoderStateMachine->BeginShutdown()
      ->Then(mAbstractMainThread, __func__, this,
             &MediaDecoder::FinishShutdown,
             &MediaDecoder::FinishShutdown);
  } else {
    // Ensure we always unregister asynchronously in order not to disrupt
    // the hashtable iterating in MediaShutdownManager::Shutdown().
    RefPtr<MediaDecoder> self = this;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([self] () {
      self->mVideoFrameContainer = nullptr;
      MediaShutdownManager::Instance().Unregister(self);
    });
    mAbstractMainThread->Dispatch(r.forget());
  }

  // Force any outstanding seek and byterange requests to complete
  // to prevent shutdown from deadlocking.
  if (mResource) {
    mResource->Close();
  }

  // Ask the owner to remove its audio/video tracks.
  GetOwner()->RemoveMediaTracks();

  ChangeState(PLAY_STATE_SHUTDOWN);
  mOwner = nullptr;
}

void
MediaDecoder::NotifyXPCOMShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (auto owner = GetOwner()) {
    owner->NotifyXPCOMShutdown();
  }
  MOZ_DIAGNOSTIC_ASSERT(IsShutdown());

  // Don't cause grief to release builds by ensuring Shutdown()
  // is always called during shutdown phase.
  if (!IsShutdown()) {
    Shutdown();
  }
}

MediaDecoder::~MediaDecoder()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(IsShutdown());
  mResourceCallback->Disconnect();
  MediaMemoryTracker::RemoveMediaDecoder(this);
  UnpinForSeek();
}

void
MediaDecoder::OnPlaybackEvent(MediaEventType aEvent)
{
  switch (aEvent) {
    case MediaEventType::PlaybackStarted:
      mPlaybackStatistics.Start();
      break;
    case MediaEventType::PlaybackStopped:
      mPlaybackStatistics.Stop();
      ComputePlaybackRate();
      break;
    case MediaEventType::PlaybackEnded:
      PlaybackEnded();
      break;
    case MediaEventType::SeekStarted:
      SeekingStarted();
      break;
    case MediaEventType::Invalidate:
      Invalidate();
      break;
    case MediaEventType::EnterVideoSuspend:
      GetOwner()->DispatchAsyncEvent(NS_LITERAL_STRING("mozentervideosuspend"));
      break;
    case MediaEventType::ExitVideoSuspend:
      GetOwner()->DispatchAsyncEvent(NS_LITERAL_STRING("mozexitvideosuspend"));
      break;
    case MediaEventType::StartVideoSuspendTimer:
      GetOwner()->DispatchAsyncEvent(NS_LITERAL_STRING("mozstartvideosuspendtimer"));
      break;
    case MediaEventType::CancelVideoSuspendTimer:
      GetOwner()->DispatchAsyncEvent(NS_LITERAL_STRING("mozcancelvideosuspendtimer"));
      break;
    case MediaEventType::VideoOnlySeekBegin:
      GetOwner()->DispatchAsyncEvent(NS_LITERAL_STRING("mozvideoonlyseekbegin"));
      break;
    case MediaEventType::VideoOnlySeekCompleted:
      GetOwner()->DispatchAsyncEvent(NS_LITERAL_STRING("mozvideoonlyseekcompleted"));
  }
}

void
MediaDecoder::OnPlaybackErrorEvent(const MediaResult& aError)
{
  DecodeError(aError);
}

void
MediaDecoder::OnDecoderDoctorEvent(DecoderDoctorEvent aEvent)
{
  MOZ_ASSERT(NS_IsMainThread());
  // OnDecoderDoctorEvent is disconnected at shutdown time.
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  nsIDocument* doc = GetOwner()->GetDocument();
  if (!doc) {
    return;
  }
  DecoderDoctorDiagnostics diags;
  diags.StoreEvent(doc, aEvent, __func__);
}

void
MediaDecoder::FinishShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  mDecoderStateMachine->BreakCycles();
  SetStateMachine(nullptr);
  mVideoFrameContainer = nullptr;
  MediaShutdownManager::Instance().Unregister(this);
}

MediaResourceCallback*
MediaDecoder::GetResourceCallback() const
{
  return mResourceCallback;
}

nsresult
MediaDecoder::OpenResource(nsIStreamListener** aStreamListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (aStreamListener) {
    *aStreamListener = nullptr;
  }
  return mResource->Open(aStreamListener);
}

nsresult
MediaDecoder::Load(nsIStreamListener** aStreamListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mResource, "Can't load without a MediaResource");

  nsresult rv = MediaShutdownManager::Instance().Register(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = OpenResource(aStreamListener);
  NS_ENSURE_SUCCESS(rv, rv);

  SetStateMachine(CreateStateMachine());
  NS_ENSURE_TRUE(GetStateMachine(), NS_ERROR_FAILURE);

  return InitializeStateMachine();
}

nsresult
MediaDecoder::InitializeStateMachine()
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ASSERTION(mDecoderStateMachine, "Cannot initialize null state machine!");

  nsresult rv = mDecoderStateMachine->Init(this);
  NS_ENSURE_SUCCESS(rv, rv);

  // If some parameters got set before the state machine got created,
  // set them now
  SetStateMachineParameters();

  return NS_OK;
}

void
MediaDecoder::SetStateMachineParameters()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mPlaybackRate != 1 && mPlaybackRate != 0) {
    mDecoderStateMachine->DispatchSetPlaybackRate(mPlaybackRate);
  }
  mTimedMetadataListener = mDecoderStateMachine->TimedMetadataEvent().Connect(
    mAbstractMainThread, this, &MediaDecoder::OnMetadataUpdate);
  mMetadataLoadedListener = mDecoderStateMachine->MetadataLoadedEvent().Connect(
    mAbstractMainThread, this, &MediaDecoder::MetadataLoaded);
  mFirstFrameLoadedListener =
    mDecoderStateMachine->FirstFrameLoadedEvent().Connect(
      mAbstractMainThread, this, &MediaDecoder::FirstFrameLoaded);

  mOnPlaybackEvent = mDecoderStateMachine->OnPlaybackEvent().Connect(
    mAbstractMainThread, this, &MediaDecoder::OnPlaybackEvent);
  mOnPlaybackErrorEvent = mDecoderStateMachine->OnPlaybackErrorEvent().Connect(
    mAbstractMainThread, this, &MediaDecoder::OnPlaybackErrorEvent);
  mOnDecoderDoctorEvent = mDecoderStateMachine->OnDecoderDoctorEvent().Connect(
    mAbstractMainThread, this, &MediaDecoder::OnDecoderDoctorEvent);
  mOnMediaNotSeekable = mDecoderStateMachine->OnMediaNotSeekable().Connect(
    mAbstractMainThread, this, &MediaDecoder::OnMediaNotSeekable);
}

nsresult
MediaDecoder::Play()
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ASSERTION(mDecoderStateMachine != nullptr, "Should have state machine.");
  if (mPlaybackRate == 0) {
    return NS_OK;
  }

  if (IsEnded()) {
    return Seek(0, SeekTarget::PrevSyncPoint);
  } else if (mPlayState == PLAY_STATE_LOADING) {
    mNextState = PLAY_STATE_PLAYING;
    return NS_OK;
  }

  ChangeState(PLAY_STATE_PLAYING);
  return NS_OK;
}

nsresult
MediaDecoder::Seek(double aTime, SeekTarget::Type aSeekType)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  MOZ_ASSERT(aTime >= 0.0, "Cannot seek to a negative value.");

  int64_t timeUsecs = TimeUnit::FromSeconds(aTime).ToMicroseconds();

  mLogicalPosition = aTime;

  mLogicallySeeking = true;
  SeekTarget target = SeekTarget(timeUsecs, aSeekType);
  CallSeek(target);

  if (mPlayState == PLAY_STATE_ENDED) {
    PinForSeek();
    ChangeState(GetOwner()->GetPaused() ? PLAY_STATE_PAUSED : PLAY_STATE_PLAYING);
  }
  return NS_OK;
}

void
MediaDecoder::DiscardOngoingSeekIfExists()
{
  MOZ_ASSERT(NS_IsMainThread());
  mSeekRequest.DisconnectIfExists();
  GetOwner()->AsyncRejectSeekDOMPromiseIfExists();
}

void
MediaDecoder::CallSeek(const SeekTarget& aTarget)
{
  MOZ_ASSERT(NS_IsMainThread());
  DiscardOngoingSeekIfExists();

  mDecoderStateMachine->InvokeSeek(aTarget)
  ->Then(mAbstractMainThread, __func__, this,
         &MediaDecoder::OnSeekResolved, &MediaDecoder::OnSeekRejected)
  ->Track(mSeekRequest);
}

double
MediaDecoder::GetCurrentTime()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mLogicalPosition;
}

already_AddRefed<nsIPrincipal>
MediaDecoder::GetCurrentPrincipal()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mResource ? mResource->GetCurrentPrincipal() : nullptr;
}

void
MediaDecoder::OnMetadataUpdate(TimedMetadata&& aMetadata)
{
  MOZ_ASSERT(NS_IsMainThread());
  GetOwner()->RemoveMediaTracks();
  MetadataLoaded(MakeUnique<MediaInfo>(*aMetadata.mInfo),
                 UniquePtr<MetadataTags>(aMetadata.mTags.forget()),
                 MediaDecoderEventVisibility::Observable);
  FirstFrameLoaded(Move(aMetadata.mInfo),
                   MediaDecoderEventVisibility::Observable);
}

void
MediaDecoder::MetadataLoaded(UniquePtr<MediaInfo> aInfo,
                             UniquePtr<MetadataTags> aTags,
                             MediaDecoderEventVisibility aEventVisibility)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  LOG("MetadataLoaded, channels=%u rate=%u hasAudio=%d hasVideo=%d",
      aInfo->mAudio.mChannels, aInfo->mAudio.mRate,
      aInfo->HasAudio(), aInfo->HasVideo());

  mMediaSeekable = aInfo->mMediaSeekable;
  mMediaSeekableOnlyInBufferedRanges = aInfo->mMediaSeekableOnlyInBufferedRanges;
  mInfo = aInfo.release();
  GetOwner()->ConstructMediaTracks(mInfo);

  // Make sure the element and the frame (if any) are told about
  // our new size.
  if (aEventVisibility != MediaDecoderEventVisibility::Suppressed) {
    mFiredMetadataLoaded = true;
    GetOwner()->MetadataLoaded(
      mInfo, nsAutoPtr<const MetadataTags>(aTags.release()));
  }
  // Invalidate() will end up calling GetOwner()->UpdateMediaSize with the last
  // dimensions retrieved from the video frame container. The video frame
  // container contains more up to date dimensions than aInfo.
  // So we call Invalidate() after calling GetOwner()->MetadataLoaded to ensure
  // the media element has the latest dimensions.
  Invalidate();

  EnsureTelemetryReported();
}

void
MediaDecoder::EnsureTelemetryReported()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mTelemetryReported || !mInfo) {
    // Note: sometimes we get multiple MetadataLoaded calls (for example
    // for chained ogg). So we ensure we don't report duplicate results for
    // these resources.
    return;
  }

  nsTArray<nsCString> codecs;
  if (mInfo->HasAudio()
      && !mInfo->mAudio.GetAsAudioInfo()->mMimeType.IsEmpty()) {
    codecs.AppendElement(mInfo->mAudio.GetAsAudioInfo()->mMimeType);
  }
  if (mInfo->HasVideo()
      && !mInfo->mVideo.GetAsVideoInfo()->mMimeType.IsEmpty()) {
    codecs.AppendElement(mInfo->mVideo.GetAsVideoInfo()->mMimeType);
  }
  if (codecs.IsEmpty()) {
    codecs.AppendElement(nsPrintfCString(
      "resource; %s", mResource->GetContentType().OriginalString().Data()));
  }
  for (const nsCString& codec : codecs) {
    LOG("Telemetry MEDIA_CODEC_USED= '%s'", codec.get());
    Telemetry::Accumulate(Telemetry::HistogramID::MEDIA_CODEC_USED, codec);
  }

  mTelemetryReported = true;
}

const char*
MediaDecoder::PlayStateStr()
{
  MOZ_ASSERT(NS_IsMainThread());
  return ToPlayStateStr(mPlayState);
}

void
MediaDecoder::FirstFrameLoaded(nsAutoPtr<MediaInfo> aInfo,
                               MediaDecoderEventVisibility aEventVisibility)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  LOG("FirstFrameLoaded, channels=%u rate=%u hasAudio=%d hasVideo=%d "
      "mPlayState=%s",
      aInfo->mAudio.mChannels, aInfo->mAudio.mRate, aInfo->HasAudio(),
      aInfo->HasVideo(), PlayStateStr());

  mInfo = aInfo.forget();

  Invalidate();

  // This can run cache callbacks.
  mResource->EnsureCacheUpToDate();

  // The element can run javascript via events
  // before reaching here, so only change the
  // state if we're still set to the original
  // loading state.
  if (mPlayState == PLAY_STATE_LOADING) {
    ChangeState(mNextState);
  }

  // Run NotifySuspendedStatusChanged now to give us a chance to notice
  // that autoplay should run.
  NotifySuspendedStatusChanged();

  // GetOwner()->FirstFrameLoaded() might call us back. Put it at the bottom of
  // this function to avoid unexpected shutdown from reentrant calls.
  if (aEventVisibility != MediaDecoderEventVisibility::Suppressed) {
    GetOwner()->FirstFrameLoaded();
  }
}

void
MediaDecoder::NetworkError()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  GetOwner()->NetworkError();
}

void
MediaDecoder::DecodeError(const MediaResult& aError)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  GetOwner()->DecodeError(aError);
}

void
MediaDecoder::UpdateSameOriginStatus(bool aSameOrigin)
{
  MOZ_ASSERT(NS_IsMainThread());
  mSameOriginMedia = aSameOrigin;
}

bool
MediaDecoder::IsSeeking() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mLogicallySeeking;
}

bool
MediaDecoder::OwnerHasError() const
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  return GetOwner()->HasError();
}

already_AddRefed<GMPCrashHelper>
MediaDecoder::GetCrashHelper()
{
  MOZ_ASSERT(NS_IsMainThread());
  return GetOwner()->CreateGMPCrashHelper();
}

bool
MediaDecoder::IsEnded() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mPlayState == PLAY_STATE_ENDED;
}

bool
MediaDecoder::IsShutdown() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mPlayState == PLAY_STATE_SHUTDOWN;
}

void
MediaDecoder::PlaybackEnded()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  if (mLogicallySeeking || mPlayState == PLAY_STATE_LOADING ||
      mPlayState == PLAY_STATE_ENDED) {
    LOG("MediaDecoder::PlaybackEnded bailed out, "
        "mLogicallySeeking=%d mPlayState=%s",
        mLogicallySeeking.Ref(), ToPlayStateStr(mPlayState));
    return;
  }

  LOG("MediaDecoder::PlaybackEnded");

  ChangeState(PLAY_STATE_ENDED);
  InvalidateWithFlags(VideoFrameContainer::INVALIDATE_FORCE);
  GetOwner()->PlaybackEnded();

  // This must be called after |GetOwner()->PlaybackEnded()| call above, in
  // order to fire the required durationchange.
  if (IsInfinite()) {
    SetInfinite(false);
  }
}

MediaStatistics
MediaDecoder::GetStatistics()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mResource);

  MediaStatistics result;
  result.mDownloadRate =
    mResource->GetDownloadRate(&result.mDownloadRateReliable);
  result.mDownloadPosition = mResource->GetCachedDataEnd(mDecoderPosition);
  result.mTotalBytes = mResource->GetLength();
  result.mPlaybackRate = mPlaybackBytesPerSecond;
  result.mPlaybackRateReliable = mPlaybackRateReliable;
  result.mDecoderPosition = mDecoderPosition;
  result.mPlaybackPosition = mPlaybackPosition;
  return result;
}

void
MediaDecoder::ComputePlaybackRate()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mResource);

  int64_t length = mResource->GetLength();
  if (mozilla::IsFinite<double>(mDuration)
      && mDuration > 0
      && length >= 0) {
    mPlaybackRateReliable = true;
    mPlaybackBytesPerSecond = length / mDuration;
    return;
  }

  bool reliable = false;
  mPlaybackBytesPerSecond = mPlaybackStatistics.GetRateAtLastStop(&reliable);
  mPlaybackRateReliable = reliable;
}

void
MediaDecoder::UpdatePlaybackRate()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mResource);

  ComputePlaybackRate();
  uint32_t rate = mPlaybackBytesPerSecond;

  if (mPlaybackRateReliable) {
    // Avoid passing a zero rate
    rate = std::max(rate, 1u);
  } else {
    // Set a minimum rate of 10,000 bytes per second ... sometimes we just
    // don't have good data
    rate = std::max(rate, 10000u);
  }

  mResource->SetPlaybackRate(rate);
}

void
MediaDecoder::NotifySuspendedStatusChanged()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  if (mResource) {
    bool suspended = mResource->IsSuspendedByCache();
    GetOwner()->NotifySuspendedByCache(suspended);
  }
}

bool
MediaDecoder::ShouldThrottleDownload()
{
  // We throttle the download if either the throttle override pref is set
  // (so that we can always throttle in Firefox on mobile) or if the download
  // is fast enough that there's no concern about playback being interrupted.
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(mDecoderStateMachine, false);

  if (Preferences::GetBool("media.throttle-regardless-of-download-rate",
                           false)) {
    return true;
  }

  MediaStatistics stats = GetStatistics();
  if (!stats.mDownloadRateReliable || !stats.mPlaybackRateReliable) {
    return false;
  }
  uint32_t factor =
    std::max(2u, Preferences::GetUint("media.throttle-factor", 2));
  return stats.mDownloadRate > factor * stats.mPlaybackRate;
}

void
MediaDecoder::DownloadProgressed()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  UpdatePlaybackRate();
  GetOwner()->DownloadProgressed();
  mResource->ThrottleReadahead(ShouldThrottleDownload());
}

void
MediaDecoder::NotifyDownloadEnded(nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  LOG("NotifyDownloadEnded, status=%" PRIx32, static_cast<uint32_t>(aStatus));

  if (aStatus == NS_BINDING_ABORTED) {
    // Download has been cancelled by user.
    GetOwner()->LoadAborted();
    return;
  }

  UpdatePlaybackRate();

  if (NS_SUCCEEDED(aStatus)) {
    // A final progress event will be fired by the MediaResource calling
    // DownloadSuspended on the element.
    // Also NotifySuspendedStatusChanged() will be called to update readyState
    // if download ended with success.
  } else if (aStatus != NS_BASE_STREAM_CLOSED) {
    NetworkError();
  }
}

void
MediaDecoder::NotifyPrincipalChanged()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  nsCOMPtr<nsIPrincipal> newPrincipal = GetCurrentPrincipal();
  mMediaPrincipalHandle = MakePrincipalHandle(newPrincipal);
  GetOwner()->NotifyDecoderPrincipalChanged();
}

void
MediaDecoder::NotifyBytesConsumed(int64_t aBytes, int64_t aOffset)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  if (mIgnoreProgressData) {
    return;
  }

  MOZ_ASSERT(mDecoderStateMachine);
  if (aOffset >= mDecoderPosition) {
    mPlaybackStatistics.AddBytes(aBytes);
  }
  mDecoderPosition = aOffset + aBytes;
}

void
MediaDecoder::OnSeekResolved()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  mSeekRequest.Complete();

  {
    // An additional seek was requested while the current seek was
    // in operation.
    UnpinForSeek();
    mLogicallySeeking = false;
  }

  // Ensure logical position is updated after seek.
  UpdateLogicalPositionInternal();

  GetOwner()->SeekCompleted();
  GetOwner()->AsyncResolveSeekDOMPromiseIfExists();
}

void
MediaDecoder::OnSeekRejected()
{
  MOZ_ASSERT(NS_IsMainThread());
  mSeekRequest.Complete();
  mLogicallySeeking = false;
  GetOwner()->AsyncRejectSeekDOMPromiseIfExists();
}

void
MediaDecoder::SeekingStarted()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  GetOwner()->SeekStarted();
}

void
MediaDecoder::ChangeState(PlayState aState)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown(), "SHUTDOWN is the final state.");

  if (mNextState == aState) {
    mNextState = PLAY_STATE_PAUSED;
  }

  LOG("ChangeState %s => %s", PlayStateStr(), ToPlayStateStr(aState));
  mPlayState = aState;

  if (mPlayState == PLAY_STATE_PLAYING) {
    GetOwner()->ConstructMediaTracks(mInfo);
  } else if (IsEnded()) {
    GetOwner()->RemoveMediaTracks();
  }
}

void
MediaDecoder::UpdateLogicalPositionInternal()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  double currentPosition = CurrentPosition().ToSeconds();
  if (mPlayState == PLAY_STATE_ENDED) {
    currentPosition = std::max(currentPosition, mDuration);
  }
  bool logicalPositionChanged = mLogicalPosition != currentPosition;
  mLogicalPosition = currentPosition;

  // Invalidate the frame so any video data is displayed.
  // Do this before the timeupdate event so that if that
  // event runs JavaScript that queries the media size, the
  // frame has reflowed and the size updated beforehand.
  Invalidate();

  if (logicalPositionChanged) {
    FireTimeUpdate();
  }
}

void
MediaDecoder::DurationChanged()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  double oldDuration = mDuration;
  if (IsInfinite()) {
    mDuration = std::numeric_limits<double>::infinity();
  } else if (mExplicitDuration.Ref().isSome()) {
    mDuration = mExplicitDuration.Ref().ref();
  } else if (mStateMachineDuration.Ref().isSome()) {
    mDuration = mStateMachineDuration.Ref().ref().ToSeconds();
  }

  if (mDuration == oldDuration || IsNaN(mDuration)) {
    return;
  }

  LOG("Duration changed to %f", mDuration);

  // Duration has changed so we should recompute playback rate
  UpdatePlaybackRate();

  // See https://www.w3.org/Bugs/Public/show_bug.cgi?id=28822 for a discussion
  // of whether we should fire durationchange on explicit infinity.
  if (mFiredMetadataLoaded
      && (!mozilla::IsInfinite<double>(mDuration)
          || mExplicitDuration.Ref().isSome())) {
    GetOwner()->DispatchAsyncEvent(NS_LITERAL_STRING("durationchange"));
  }

  if (CurrentPosition() > TimeUnit::FromSeconds(mDuration)) {
    Seek(mDuration, SeekTarget::Accurate);
  }
}

void
MediaDecoder::NotifyCompositor()
{
  MediaDecoderOwner* owner = GetOwner();
  NS_ENSURE_TRUE_VOID(owner);

  nsIDocument* ownerDoc = owner->GetDocument();
  NS_ENSURE_TRUE_VOID(ownerDoc);

  RefPtr<LayerManager> layerManager =
    nsContentUtils::LayerManagerForDocument(ownerDoc);
  if (layerManager) {
    RefPtr<KnowsCompositor> knowsCompositor = layerManager->AsShadowForwarder();
    mCompositorUpdatedEvent.Notify(knowsCompositor);
  }
}

void
MediaDecoder::SetElementVisibility(bool aIsDocumentVisible,
                                   Visibility aElementVisibility,
                                   bool aIsElementInTree)
{
  MOZ_ASSERT(NS_IsMainThread());
  mIsDocumentVisible = aIsDocumentVisible;
  mElementVisibility = aElementVisibility;
  mIsElementInTree = aIsElementInTree;
  UpdateVideoDecodeMode();
}

void
MediaDecoder::SetForcedHidden(bool aForcedHidden)
{
  MOZ_ASSERT(NS_IsMainThread());
  mForcedHidden = aForcedHidden;
  UpdateVideoDecodeMode();
}

void
MediaDecoder::SetSuspendTaint(bool aTainted)
{
  MOZ_ASSERT(NS_IsMainThread());
  mHasSuspendTaint = aTainted;
  UpdateVideoDecodeMode();
}

void
MediaDecoder::UpdateVideoDecodeMode()
{
  // The MDSM may yet be set.
  if (!mDecoderStateMachine) {
    return;
  }

  // If an element is in-tree with UNTRACKED visibility, the visibility is
  // incomplete and don't update the video decode mode.
  if (mIsElementInTree && mElementVisibility == Visibility::UNTRACKED) {
    return;
  }

  // If mHasSuspendTaint is set, never suspend the video decoder.
  if (mHasSuspendTaint) {
    mDecoderStateMachine->SetVideoDecodeMode(VideoDecodeMode::Normal);
    return;
  }

  // Don't suspend elements that is not in tree.
  if (!mIsElementInTree) {
    mDecoderStateMachine->SetVideoDecodeMode(VideoDecodeMode::Normal);
    return;
  }

  // If mForcedHidden is set, suspend the video decoder anyway.
  if (mForcedHidden) {
    mDecoderStateMachine->SetVideoDecodeMode(VideoDecodeMode::Suspend);
    return;
  }

  // Otherwise, depends on the owner's visibility state.
  // A element is visible only if its document is visible and the element
  // itself is visible.
  if (mIsDocumentVisible &&
      mElementVisibility == Visibility::APPROXIMATELY_VISIBLE) {
    mDecoderStateMachine->SetVideoDecodeMode(VideoDecodeMode::Normal);
  } else {
    mDecoderStateMachine->SetVideoDecodeMode(VideoDecodeMode::Suspend);
  }
}

bool
MediaDecoder::HasSuspendTaint() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mHasSuspendTaint;
}

bool
MediaDecoder::IsTransportSeekable()
{
  MOZ_ASSERT(NS_IsMainThread());
  return GetResource()->IsTransportSeekable();
}

bool
MediaDecoder::IsMediaSeekable()
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(GetStateMachine(), false);
  return mMediaSeekable;
}

media::TimeIntervals
MediaDecoder::GetSeekable()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (IsNaN(GetDuration())) {
    // We do not have a duration yet, we can't determine the seekable range.
    return TimeIntervals();
  }

  // We can seek in buffered range if the media is seekable. Also, we can seek
  // in unbuffered ranges if the transport level is seekable (local file or the
  // server supports range requests, etc.) or in cue-less WebMs
  if (mMediaSeekableOnlyInBufferedRanges) {
    return GetBuffered();
  } else if (!IsMediaSeekable()) {
    return media::TimeIntervals();
  } else if (!IsTransportSeekable()) {
    return GetBuffered();
  } else {
    return media::TimeIntervals(
      media::TimeInterval(TimeUnit::Zero(),
                          IsInfinite()
                          ? TimeUnit::FromInfinity()
                          : TimeUnit::FromSeconds(GetDuration())));
  }
}

void
MediaDecoder::SetFragmentEndTime(double aTime)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoderStateMachine) {
    mDecoderStateMachine->DispatchSetFragmentEndTime(
      TimeUnit::FromSeconds(aTime));
  }
}

void
MediaDecoder::Suspend()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mResource) {
    mResource->Suspend(true);
  }
}

void
MediaDecoder::Resume()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mResource) {
    mResource->Resume();
  }
}

void
MediaDecoder::SetLoadInBackground(bool aLoadInBackground)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mResource) {
    mResource->SetLoadInBackground(aLoadInBackground);
  }
}

void
MediaDecoder::SetPlaybackRate(double aPlaybackRate)
{
  MOZ_ASSERT(NS_IsMainThread());

  double oldRate = mPlaybackRate;
  mPlaybackRate = aPlaybackRate;
  if (aPlaybackRate == 0) {
    Pause();
    return;
  }


  if (oldRate == 0 && !GetOwner()->GetPaused()) {
    // PlaybackRate is no longer null.
    // Restart the playback if the media was playing.
    Play();
  }

  if (mDecoderStateMachine) {
    mDecoderStateMachine->DispatchSetPlaybackRate(aPlaybackRate);
  }
}

void
MediaDecoder::SetPreservesPitch(bool aPreservesPitch)
{
  MOZ_ASSERT(NS_IsMainThread());
  mPreservesPitch = aPreservesPitch;
}

void
MediaDecoder::SetLooping(bool aLooping)
{
  MOZ_ASSERT(NS_IsMainThread());
  mLooping = aLooping;
}

void
MediaDecoder::ConnectMirrors(MediaDecoderStateMachine* aObject)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aObject);
  mStateMachineDuration.Connect(aObject->CanonicalDuration());
  mBuffered.Connect(aObject->CanonicalBuffered());
  mNextFrameStatus.Connect(aObject->CanonicalNextFrameStatus());
  mCurrentPosition.Connect(aObject->CanonicalCurrentPosition());
  mPlaybackPosition.Connect(aObject->CanonicalPlaybackOffset());
  mIsAudioDataAudible.Connect(aObject->CanonicalIsAudioDataAudible());
}

void
MediaDecoder::DisconnectMirrors()
{
  MOZ_ASSERT(NS_IsMainThread());
  mStateMachineDuration.DisconnectIfConnected();
  mBuffered.DisconnectIfConnected();
  mNextFrameStatus.DisconnectIfConnected();
  mCurrentPosition.DisconnectIfConnected();
  mPlaybackPosition.DisconnectIfConnected();
  mIsAudioDataAudible.DisconnectIfConnected();
}

void
MediaDecoder::SetStateMachine(MediaDecoderStateMachine* aStateMachine)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT_IF(aStateMachine, !mDecoderStateMachine);
  mDecoderStateMachine = aStateMachine;
  if (aStateMachine) {
    ConnectMirrors(aStateMachine);
    UpdateVideoDecodeMode();
  } else {
    DisconnectMirrors();
  }
}

ImageContainer*
MediaDecoder::GetImageContainer()
{
  return mVideoFrameContainer ? mVideoFrameContainer->GetImageContainer()
                              : nullptr;
}

void
MediaDecoder::InvalidateWithFlags(uint32_t aFlags)
{
  if (mVideoFrameContainer) {
    mVideoFrameContainer->InvalidateWithFlags(aFlags);
  }
}

void
MediaDecoder::Invalidate()
{
  if (mVideoFrameContainer) {
    mVideoFrameContainer->Invalidate();
  }
}

// Constructs the time ranges representing what segments of the media
// are buffered and playable.
media::TimeIntervals
MediaDecoder::GetBuffered()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mBuffered.Ref();
}

size_t
MediaDecoder::SizeOfVideoQueue()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoderStateMachine) {
    return mDecoderStateMachine->SizeOfVideoQueue();
  }
  return 0;
}

size_t
MediaDecoder::SizeOfAudioQueue()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoderStateMachine) {
    return mDecoderStateMachine->SizeOfAudioQueue();
  }
  return 0;
}

void MediaDecoder::AddSizeOfResources(ResourceSizes* aSizes)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (GetResource()) {
    aSizes->mByteSize +=
      GetResource()->SizeOfIncludingThis(aSizes->mMallocSizeOf);
  }
}

void
MediaDecoder::NotifyDataArrivedInternal()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  mDataArrivedEvent.Notify();
}

void
MediaDecoder::NotifyDataArrived()
{
  NotifyDataArrivedInternal();
  DownloadProgressed();
}

// Provide access to the state machine object
MediaDecoderStateMachine*
MediaDecoder::GetStateMachine() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mDecoderStateMachine;
}

void
MediaDecoder::FireTimeUpdate()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  GetOwner()->FireTimeUpdate(true);
}

void
MediaDecoder::PinForSeek()
{
  MOZ_ASSERT(NS_IsMainThread());
  MediaResource* resource = GetResource();
  if (!resource || mPinnedForSeek) {
    return;
  }
  mPinnedForSeek = true;
  resource->Pin();
}

void
MediaDecoder::UnpinForSeek()
{
  MOZ_ASSERT(NS_IsMainThread());
  MediaResource* resource = GetResource();
  if (!resource || !mPinnedForSeek) {
    return;
  }
  mPinnedForSeek = false;
  resource->Unpin();
}

bool
MediaDecoder::CanPlayThrough()
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(mDecoderStateMachine, false);
  return GetStatistics().CanPlayThrough();
}

RefPtr<MediaDecoder::CDMProxyPromise>
MediaDecoder::RequestCDMProxy() const
{
  return mCDMProxyPromise;
}

void
MediaDecoder::SetCDMProxy(CDMProxy* aProxy)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aProxy);

  mCDMProxyPromiseHolder.ResolveIfExists(aProxy, __func__);
}

bool
MediaDecoder::IsOpusEnabled()
{
  return Preferences::GetBool("media.opus.enabled");
}

bool
MediaDecoder::IsOggEnabled()
{
  return Preferences::GetBool("media.ogg.enabled");
}

bool
MediaDecoder::IsWaveEnabled()
{
  return Preferences::GetBool("media.wave.enabled");
}

bool
MediaDecoder::IsWebMEnabled()
{
  return Preferences::GetBool("media.webm.enabled");
}

#ifdef MOZ_ANDROID_OMX
bool
MediaDecoder::IsAndroidMediaPluginEnabled()
{
  return jni::GetAPIVersion() < 16
         && Preferences::GetBool("media.plugins.enabled");
}
#endif

NS_IMETHODIMP
MediaMemoryTracker::CollectReports(nsIHandleReportCallback* aHandleReport,
                                   nsISupports* aData, bool aAnonymize)
{
  // NB: When resourceSizes' ref count goes to 0 the promise will report the
  //     resources memory and finish the asynchronous memory report.
  RefPtr<MediaDecoder::ResourceSizes> resourceSizes =
      new MediaDecoder::ResourceSizes(MediaMemoryTracker::MallocSizeOf);

  nsCOMPtr<nsIHandleReportCallback> handleReport = aHandleReport;
  nsCOMPtr<nsISupports> data = aData;

  resourceSizes->Promise()->Then(
      // Don't use SystemGroup::AbstractMainThreadFor() for
      // handleReport->Callback() will run scripts.
      AbstractThread::MainThread(),
      __func__,
      [handleReport, data] (size_t size) {
        handleReport->Callback(
            EmptyCString(), NS_LITERAL_CSTRING("explicit/media/resources"),
            KIND_HEAP, UNITS_BYTES, size,
            NS_LITERAL_CSTRING("Memory used by media resources including "
                               "streaming buffers, caches, etc."),
            data);

        nsCOMPtr<nsIMemoryReporterManager> imgr =
          do_GetService("@mozilla.org/memory-reporter-manager;1");

        if (imgr) {
          imgr->EndReport();
        }
      },
      [] (size_t) { /* unused reject function */ });

  int64_t video = 0;
  int64_t audio = 0;
  DecodersArray& decoders = Decoders();
  for (size_t i = 0; i < decoders.Length(); ++i) {
    MediaDecoder* decoder = decoders[i];
    video += decoder->SizeOfVideoQueue();
    audio += decoder->SizeOfAudioQueue();
    decoder->AddSizeOfResources(resourceSizes);
  }

  MOZ_COLLECT_REPORT(
    "explicit/media/decoded/video", KIND_HEAP, UNITS_BYTES, video,
    "Memory used by decoded video frames.");

  MOZ_COLLECT_REPORT(
    "explicit/media/decoded/audio", KIND_HEAP, UNITS_BYTES, audio,
    "Memory used by decoded audio chunks.");

  return NS_OK;
}

MediaDecoderOwner*
MediaDecoder::GetOwner() const
{
  MOZ_ASSERT(NS_IsMainThread());
  // Check object lifetime when mOwner points to a media element.
  MOZ_DIAGNOSTIC_ASSERT(!mOwner || !mIsMediaElement || mElement);
  // mOwner is valid until shutdown.
  return mOwner;
}

MediaDecoderOwner::NextFrameStatus
MediaDecoder::NextFrameBufferedStatus()
{
  MOZ_ASSERT(NS_IsMainThread());
  // Next frame hasn't been decoded yet.
  // Use the buffered range to consider if we have the next frame available.
  auto currentPosition = CurrentPosition();
  media::TimeInterval interval(
    currentPosition,
    currentPosition + DEFAULT_NEXT_FRAME_AVAILABLE_BUFFERED);
  return GetBuffered().Contains(interval)
         ? MediaDecoderOwner::NEXT_FRAME_AVAILABLE
         : MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE;
}

nsCString
MediaDecoder::GetDebugInfo()
{
  return nsPrintfCString(
    "MediaDecoder State: channels=%u rate=%u hasAudio=%d hasVideo=%d "
    "mPlayState=%s mdsm=%p",
    mInfo ? mInfo->mAudio.mChannels : 0, mInfo ? mInfo->mAudio.mRate : 0,
    mInfo ? mInfo->HasAudio() : 0, mInfo ? mInfo->HasVideo() : 0,
    PlayStateStr(), GetStateMachine());
}

void
MediaDecoder::DumpDebugInfo()
{
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  nsCString str = GetDebugInfo();

  nsAutoCString readerStr;
  GetMozDebugReaderData(readerStr);
  if (!readerStr.IsEmpty()) {
    str += "\nreader data:\n";
    str += readerStr;
  }

  if (!GetStateMachine()) {
    DUMP("%s", str.get());
    return;
  }

  RefPtr<MediaDecoder> self = this;
  GetStateMachine()->RequestDebugInfo()->Then(
    SystemGroup::AbstractMainThreadFor(TaskCategory::Other), __func__,
    [this, self, str] (const nsACString& aString) {
      DUMP("%s", str.get());
      DUMP("%s", aString.Data());
    },
    [this, self, str] () {
      DUMP("%s", str.get());
    });
}

RefPtr<MediaDecoder::DebugInfoPromise>
MediaDecoder::RequestDebugInfo()
{
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  auto str = GetDebugInfo();
  if (!GetStateMachine()) {
    return DebugInfoPromise::CreateAndResolve(str, __func__);
  }

  return GetStateMachine()->RequestDebugInfo()->Then(
    SystemGroup::AbstractMainThreadFor(TaskCategory::Other), __func__,
    [str] (const nsACString& aString) {
      nsCString result = str + nsCString("\n") + aString;
      return DebugInfoPromise::CreateAndResolve(result, __func__);
    },
    [str] () {
      return DebugInfoPromise::CreateAndResolve(str, __func__);
    });
}

void
MediaDecoder::NotifyAudibleStateChanged()
{
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  GetOwner()->SetAudibleState(mIsAudioDataAudible);
}

MediaMemoryTracker::MediaMemoryTracker()
{
}

void
MediaMemoryTracker::InitMemoryReporter()
{
  RegisterWeakAsyncMemoryReporter(this);
}

MediaMemoryTracker::~MediaMemoryTracker()
{
  UnregisterWeakMemoryReporter(this);
}

} // namespace mozilla

// avoid redefined macro in unified build
#undef DUMP
#undef LOG
#undef NS_DispatchToMainThread
