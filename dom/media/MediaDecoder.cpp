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
#include "mozilla/dom/AudioTrack.h"
#include "mozilla/dom/AudioTrackList.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/VideoTrack.h"
#include "mozilla/dom/VideoTrackList.h"
#include "nsPrintfCString.h"
#include "mozilla/Telemetry.h"
#include "GMPCrashHelper.h"
#include "Layers.h"
#include "mozilla/layers/ShadowLayers.h"

#ifdef MOZ_ANDROID_OMX
#include "AndroidBridge.h"
#endif

using namespace mozilla::dom;
using namespace mozilla::layers;
using namespace mozilla::media;

namespace mozilla {

// The amount of instability we tollerate in calls to
// MediaDecoder::UpdateEstimatedMediaDuration(); changes of duration
// less than this are ignored, as they're assumed to be the result of
// instability in the duration estimation.
static const uint64_t ESTIMATED_DURATION_FUZZ_FACTOR_USECS = USECS_PER_S / 2;

// avoid redefined macro in unified build
#undef DECODER_LOG
#undef DUMP_LOG

LazyLogModule gMediaDecoderLog("MediaDecoder");
#define DECODER_LOG(x, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Debug, ("Decoder=%p " x, this, ##__VA_ARGS__))

#define DUMP_LOG(x, ...) \
  NS_DebugBreak(NS_DEBUG_WARNING, nsPrintfCString("Decoder=%p " x, this, ##__VA_ARGS__).get(), nullptr, nullptr, -1)

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

  static MediaMemoryTracker* UniqueInstance() {
    if (!sUniqueInstance) {
      sUniqueInstance = new MediaMemoryTracker();
      sUniqueInstance->InitMemoryReporter();
    }
    return sUniqueInstance;
  }

  typedef nsTArray<MediaDecoder*> DecodersArray;
  static DecodersArray& Decoders() {
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

#if defined(PR_LOGGING)
LazyLogModule gMediaTimerLog("MediaTimer");
LazyLogModule gMediaSampleLog("MediaSample");
#endif

void
MediaDecoder::InitStatics()
{
  MOZ_ASSERT(NS_IsMainThread());
}

NS_IMPL_ISUPPORTS(MediaMemoryTracker, nsIMemoryReporter)

NS_IMPL_ISUPPORTS0(MediaDecoder)


void
MediaDecoder::ResourceCallback::Connect(MediaDecoder* aDecoder)
{
  MOZ_ASSERT(NS_IsMainThread());
  mDecoder = aDecoder;
  mTimer = do_CreateInstance("@mozilla.org/timer;1");
}

void
MediaDecoder::ResourceCallback::Disconnect()
{
  MOZ_ASSERT(NS_IsMainThread());
  mDecoder = nullptr;
  mTimer->Cancel();
  mTimer = nullptr;
}

MediaDecoderOwner*
MediaDecoder::ResourceCallback::GetMediaOwner() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mDecoder ? mDecoder->mOwner : nullptr;
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

void
MediaDecoder::ResourceCallback::NotifyDecodeError()
{
  RefPtr<ResourceCallback> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([=] () {
    if (self->mDecoder) {
      self->mDecoder->DecodeError(NS_ERROR_DOM_MEDIA_FATAL_ERR);
    }
  });
  AbstractThread::MainThread()->Dispatch(r.forget());
}

/* static */ void
MediaDecoder::ResourceCallback::TimerCallback(nsITimer* aTimer, void* aClosure)
{
  MOZ_ASSERT(NS_IsMainThread());
  ResourceCallback* thiz = static_cast<ResourceCallback*>(aClosure);
  MOZ_ASSERT(thiz->mDecoder);
  thiz->mDecoder->NotifyDataArrived();
  thiz->mTimerArmed = false;
}

void
MediaDecoder::ResourceCallback::NotifyDataArrived()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mDecoder || mTimerArmed) {
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
MediaDecoder::ResourceCallback::NotifyBytesDownloaded()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoder) {
    mDecoder->NotifyBytesDownloaded();
  }
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
      HTMLMediaElement* element = self->GetMediaOwner()->GetMediaElement();
      if (element) {
        element->DownloadSuspended();
      }
      // NotifySuspendedStatusChanged will tell the element that download
      // has been suspended "by the cache", which is true since we never
      // download anything. The element can then transition to HAVE_ENOUGH_DATA.
      self->mDecoder->NotifySuspendedStatusChanged();
    }
  });
  AbstractThread::MainThread()->Dispatch(r.forget());
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
  AbstractThread::MainThread()->Dispatch(r.forget());
}

void
MediaDecoder::NotifyOwnerActivityChanged(bool aIsVisible)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown());
  SetElementVisibility(aIsVisible);

  MediaDecoderOwner* owner = GetOwner();
  NS_ENSURE_TRUE_VOID(owner);

  dom::HTMLMediaElement* element = owner->GetMediaElement();
  NS_ENSURE_TRUE_VOID(element);

  RefPtr<LayerManager> layerManager =
    nsContentUtils::LayerManagerForDocument(element->OwnerDoc());
  NS_ENSURE_TRUE_VOID(layerManager);

  RefPtr<KnowsCompositor> knowsCompositor = layerManager->AsShadowForwarder();
  mCompositorUpdatedEvent.Notify(knowsCompositor);
}

void
MediaDecoder::Pause()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown());
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
  MOZ_ASSERT(!IsShutdown());
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
  name(AbstractThread::MainThread(), val, "MediaDecoder::" #name " (Mirror)")
#define INIT_CANONICAL(name, val) \
  name(AbstractThread::MainThread(), val, "MediaDecoder::" #name " (Canonical)")

MediaDecoder::MediaDecoder(MediaDecoderOwner* aOwner)
  : mWatchManager(this, AbstractThread::MainThread())
  , mLogicalPosition(0.0)
  , mDuration(std::numeric_limits<double>::quiet_NaN())
  , mResourceCallback(new ResourceCallback())
  , mCDMProxyPromise(mCDMProxyPromiseHolder.Ensure(__func__))
  , mIgnoreProgressData(false)
  , mInfiniteStream(false)
  , mOwner(aOwner)
  , mFrameStats(new FrameStatistics())
  , mVideoFrameContainer(aOwner->GetVideoFrameContainer())
  , mPlaybackStatistics(new MediaChannelStatistics())
  , mPinnedForSeek(false)
  , mMinimizePreroll(false)
  , mMediaTracksConstructed(false)
  , mFiredMetadataLoaded(false)
  , mElementVisible(!aOwner->IsHidden())
  , mForcedHidden(false)
  , INIT_MIRROR(mStateMachineIsShutdown, true)
  , INIT_MIRROR(mBuffered, TimeIntervals())
  , INIT_MIRROR(mNextFrameStatus, MediaDecoderOwner::NEXT_FRAME_UNINITIALIZED)
  , INIT_MIRROR(mCurrentPosition, 0)
  , INIT_MIRROR(mStateMachineDuration, NullableTimeUnit())
  , INIT_MIRROR(mPlaybackPosition, 0)
  , INIT_MIRROR(mIsAudioDataAudible, false)
  , INIT_CANONICAL(mVolume, 0.0)
  , INIT_CANONICAL(mPreservesPitch, true)
  , INIT_CANONICAL(mEstimatedDuration, NullableTimeUnit())
  , INIT_CANONICAL(mExplicitDuration, Maybe<double>())
  , INIT_CANONICAL(mPlayState, PLAY_STATE_LOADING)
  , INIT_CANONICAL(mNextState, PLAY_STATE_PAUSED)
  , INIT_CANONICAL(mLogicallySeeking, false)
  , INIT_CANONICAL(mSameOriginMedia, false)
  , INIT_CANONICAL(mMediaPrincipalHandle, PRINCIPAL_HANDLE_NONE)
  , INIT_CANONICAL(mPlaybackBytesPerSecond, 0.0)
  , INIT_CANONICAL(mPlaybackRateReliable, true)
  , INIT_CANONICAL(mDecoderPosition, 0)
  , INIT_CANONICAL(mIsVisible, !aOwner->IsHidden())
  , mTelemetryReported(false)
{
  MOZ_COUNT_CTOR(MediaDecoder);
  MOZ_ASSERT(NS_IsMainThread());
  MediaMemoryTracker::AddMediaDecoder(this);

  mAudioChannel = AudioChannelService::GetDefaultAudioChannel();
  mResourceCallback->Connect(this);

  //
  // Initialize watchers.
  //

  // mDuration
  mWatchManager.Watch(mStateMachineDuration, &MediaDecoder::DurationChanged);

  // mStateMachineIsShutdown
  mWatchManager.Watch(mStateMachineIsShutdown, &MediaDecoder::ShutdownBitChanged);

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

  mWatchManager.Watch(mIsAudioDataAudible, &MediaDecoder::NotifyAudibleStateChanged);

  MediaShutdownManager::Instance().Register(this);
}

#undef INIT_MIRROR
#undef INIT_CANONICAL

void
MediaDecoder::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown());

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
      ->Then(AbstractThread::MainThread(), __func__, this,
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
    AbstractThread::MainThread()->Dispatch(r.forget());
  }

  // Force any outstanding seek and byterange requests to complete
  // to prevent shutdown from deadlocking.
  if (mResource) {
    mResource->Close();
  }

  ChangeState(PLAY_STATE_SHUTDOWN);
  mOwner = nullptr;
}

MediaDecoder::~MediaDecoder()
{
  MOZ_ASSERT(NS_IsMainThread());
  MediaMemoryTracker::RemoveMediaDecoder(this);
  UnpinForSeek();
  MOZ_COUNT_DTOR(MediaDecoder);
}

void
MediaDecoder::OnPlaybackEvent(MediaEventType aEvent)
{
  switch (aEvent) {
    case MediaEventType::PlaybackStarted:
      mPlaybackStatistics->Start();
      break;
    case MediaEventType::PlaybackStopped:
      mPlaybackStatistics->Stop();
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
      mOwner->DispatchAsyncEvent(NS_LITERAL_STRING("mozentervideosuspend"));
      break;
    case MediaEventType::ExitVideoSuspend:
      mOwner->DispatchAsyncEvent(NS_LITERAL_STRING("mozexitvideosuspend"));
      break;
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
  MOZ_ASSERT(!IsShutdown());
  HTMLMediaElement* element = mOwner->GetMediaElement();
  if (!element) {
    return;
  }
  nsIDocument* doc = element->OwnerDoc();
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

  nsresult rv = OpenResource(aStreamListener);
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
  if (mMinimizePreroll) {
    mDecoderStateMachine->DispatchMinimizePrerollUntilPlaybackStarts();
  }
  if (mPlaybackRate != 1 && mPlaybackRate != 0) {
    mDecoderStateMachine->DispatchSetPlaybackRate(mPlaybackRate);
  }
  mTimedMetadataListener = mDecoderStateMachine->TimedMetadataEvent().Connect(
    AbstractThread::MainThread(), this, &MediaDecoder::OnMetadataUpdate);
  mMetadataLoadedListener = mDecoderStateMachine->MetadataLoadedEvent().Connect(
    AbstractThread::MainThread(), this, &MediaDecoder::MetadataLoaded);
  mFirstFrameLoadedListener = mDecoderStateMachine->FirstFrameLoadedEvent().Connect(
    AbstractThread::MainThread(), this, &MediaDecoder::FirstFrameLoaded);

  mOnPlaybackEvent = mDecoderStateMachine->OnPlaybackEvent().Connect(
    AbstractThread::MainThread(), this, &MediaDecoder::OnPlaybackEvent);
  mOnPlaybackErrorEvent = mDecoderStateMachine->OnPlaybackErrorEvent().Connect(
    AbstractThread::MainThread(), this, &MediaDecoder::OnPlaybackErrorEvent);
  mOnDecoderDoctorEvent = mDecoderStateMachine->OnDecoderDoctorEvent().Connect(
    AbstractThread::MainThread(), this, &MediaDecoder::OnDecoderDoctorEvent);
  mOnMediaNotSeekable = mDecoderStateMachine->OnMediaNotSeekable().Connect(
    AbstractThread::MainThread(), this, &MediaDecoder::OnMediaNotSeekable);
}

void
MediaDecoder::SetMinimizePrerollUntilPlaybackStarts()
{
  MOZ_ASSERT(NS_IsMainThread());
  DECODER_LOG("SetMinimizePrerollUntilPlaybackStarts()");
  mMinimizePreroll = true;

  // This needs to be called before we init the state machine, otherwise it will
  // have no effect.
  MOZ_DIAGNOSTIC_ASSERT(!mDecoderStateMachine);
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
MediaDecoder::Seek(double aTime, SeekTarget::Type aSeekType, dom::Promise* aPromise /*=nullptr*/)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown());

  MOZ_ASSERT(aTime >= 0.0, "Cannot seek to a negative value.");

  int64_t timeUsecs = TimeUnit::FromSeconds(aTime).ToMicroseconds();

  mLogicalPosition = aTime;

  mLogicallySeeking = true;
  SeekTarget target = SeekTarget(timeUsecs, aSeekType);
  CallSeek(target, aPromise);

  if (mPlayState == PLAY_STATE_ENDED) {
    PinForSeek();
    ChangeState(mOwner->GetPaused() ? PLAY_STATE_PAUSED : PLAY_STATE_PLAYING);
  }
  return NS_OK;
}

void
MediaDecoder::AsyncResolveSeekDOMPromiseIfExists()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mSeekDOMPromise) {
    RefPtr<dom::Promise> promise = mSeekDOMPromise;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([=] () {
      promise->MaybeResolveWithUndefined();
    });
    AbstractThread::MainThread()->Dispatch(r.forget());
    mSeekDOMPromise = nullptr;
  }
}

void
MediaDecoder::AsyncRejectSeekDOMPromiseIfExists()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mSeekDOMPromise) {
    RefPtr<dom::Promise> promise = mSeekDOMPromise;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([=] () {
      promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    });
    AbstractThread::MainThread()->Dispatch(r.forget());
    mSeekDOMPromise = nullptr;
  }
}

void
MediaDecoder::DiscardOngoingSeekIfExists()
{
  MOZ_ASSERT(NS_IsMainThread());
  mSeekRequest.DisconnectIfExists();
  AsyncRejectSeekDOMPromiseIfExists();
}

void
MediaDecoder::CallSeek(const SeekTarget& aTarget, dom::Promise* aPromise)
{
  MOZ_ASSERT(NS_IsMainThread());
  DiscardOngoingSeekIfExists();

  mSeekDOMPromise = aPromise;
  mSeekRequest.Begin(
    mDecoderStateMachine->InvokeSeek(aTarget)
    ->Then(AbstractThread::MainThread(), __func__, this,
           &MediaDecoder::OnSeekResolved, &MediaDecoder::OnSeekRejected));
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
  RemoveMediaTracks();
  MetadataLoaded(nsAutoPtr<MediaInfo>(new MediaInfo(*aMetadata.mInfo)),
                 Move(aMetadata.mTags),
                 MediaDecoderEventVisibility::Observable);
  FirstFrameLoaded(Move(aMetadata.mInfo),
                   MediaDecoderEventVisibility::Observable);
}

void
MediaDecoder::MetadataLoaded(nsAutoPtr<MediaInfo> aInfo,
                             nsAutoPtr<MetadataTags> aTags,
                             MediaDecoderEventVisibility aEventVisibility)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown());

  DECODER_LOG("MetadataLoaded, channels=%u rate=%u hasAudio=%d hasVideo=%d",
              aInfo->mAudio.mChannels, aInfo->mAudio.mRate,
              aInfo->HasAudio(), aInfo->HasVideo());

  mMediaSeekable = aInfo->mMediaSeekable;
  mMediaSeekableOnlyInBufferedRanges = aInfo->mMediaSeekableOnlyInBufferedRanges;
  mInfo = aInfo.forget();
  ConstructMediaTracks();

  // Make sure the element and the frame (if any) are told about
  // our new size.
  if (aEventVisibility != MediaDecoderEventVisibility::Suppressed) {
    mFiredMetadataLoaded = true;
    mOwner->MetadataLoaded(mInfo, nsAutoPtr<const MetadataTags>(aTags.forget()));
  }
  // Invalidate() will end up calling mOwner->UpdateMediaSize with the last
  // dimensions retrieved from the video frame container. The video frame
  // container contains more up to date dimensions than aInfo.
  // So we call Invalidate() after calling mOwner->MetadataLoaded to ensure
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
  if (mInfo->HasAudio() && !mInfo->mAudio.GetAsAudioInfo()->mMimeType.IsEmpty()) {
    codecs.AppendElement(mInfo->mAudio.GetAsAudioInfo()->mMimeType);
  }
  if (mInfo->HasVideo() && !mInfo->mVideo.GetAsVideoInfo()->mMimeType.IsEmpty()) {
    codecs.AppendElement(mInfo->mVideo.GetAsVideoInfo()->mMimeType);
  }
  if (codecs.IsEmpty()) {
    if (mResource->GetContentType().IsEmpty()) {
      NS_WARNING("Somehow the resource's content type is empty");
      return;
    }
    codecs.AppendElement(nsPrintfCString("resource; %s", mResource->GetContentType().get()));
  }
  for (const nsCString& codec : codecs) {
    DECODER_LOG("Telemetry MEDIA_CODEC_USED= '%s'", codec.get());
    Telemetry::Accumulate(Telemetry::ID::MEDIA_CODEC_USED, codec);
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
  MOZ_ASSERT(!IsShutdown());

  DECODER_LOG("FirstFrameLoaded, channels=%u rate=%u hasAudio=%d hasVideo=%d mPlayState=%s",
              aInfo->mAudio.mChannels, aInfo->mAudio.mRate,
              aInfo->HasAudio(), aInfo->HasVideo(), PlayStateStr());

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

  // mOwner->FirstFrameLoaded() might call us back. Put it at the bottom of
  // this function to avoid unexpected shutdown from reentrant calls.
  if (aEventVisibility != MediaDecoderEventVisibility::Suppressed) {
    mOwner->FirstFrameLoaded();
  }
}

void
MediaDecoder::NetworkError()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown());
  mOwner->NetworkError();
}

void
MediaDecoder::DecodeError(const MediaResult& aError)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown());
  mOwner->DecodeError(aError);
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
  MOZ_ASSERT(!IsShutdown());
  return mOwner->HasError();
}

class MediaElementGMPCrashHelper : public GMPCrashHelper
{
public:
  explicit MediaElementGMPCrashHelper(HTMLMediaElement* aElement)
    : mElement(aElement)
  {
    MOZ_ASSERT(NS_IsMainThread()); // WeakPtr isn't thread safe.
  }
  already_AddRefed<nsPIDOMWindowInner> GetPluginCrashedEventTarget() override
  {
    MOZ_ASSERT(NS_IsMainThread()); // WeakPtr isn't thread safe.
    if (!mElement) {
      return nullptr;
    }
    return do_AddRef(mElement->OwnerDoc()->GetInnerWindow());
  }
private:
  WeakPtr<HTMLMediaElement> mElement;
};

already_AddRefed<GMPCrashHelper>
MediaDecoder::GetCrashHelper()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mOwner->GetMediaElement() ?
    MakeAndAddRef<MediaElementGMPCrashHelper>(mOwner->GetMediaElement()) : nullptr;
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
  MOZ_ASSERT(!IsShutdown());

  if (mLogicallySeeking || mPlayState == PLAY_STATE_LOADING) {
    return;
  }

  ChangeState(PLAY_STATE_ENDED);
  InvalidateWithFlags(VideoFrameContainer::INVALIDATE_FORCE);
  mOwner->PlaybackEnded();

  // This must be called after |mOwner->PlaybackEnded()| call above, in order
  // to fire the required durationchange.
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
  result.mDownloadRate = mResource->GetDownloadRate(&result.mDownloadRateReliable);
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
  if (!IsNaN(mDuration) && !mozilla::IsInfinite<double>(mDuration) && length >= 0) {
    mPlaybackRateReliable = true;
    mPlaybackBytesPerSecond = length / mDuration;
    return;
  }

  bool reliable = false;
  mPlaybackBytesPerSecond = mPlaybackStatistics->GetRateAtLastStop(&reliable);
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
  MOZ_ASSERT(!IsShutdown());
  if (mResource) {
    bool suspended = mResource->IsSuspendedByCache();
    mOwner->NotifySuspendedByCache(suspended);
  }
}

void
MediaDecoder::NotifyBytesDownloaded()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown());
  UpdatePlaybackRate();
  mOwner->DownloadProgressed();
}

void
MediaDecoder::NotifyDownloadEnded(nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown());

  DECODER_LOG("NotifyDownloadEnded, status=%x", aStatus);

  if (aStatus == NS_BINDING_ABORTED) {
    // Download has been cancelled by user.
    mOwner->LoadAborted();
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
  MOZ_ASSERT(!IsShutdown());
  nsCOMPtr<nsIPrincipal> newPrincipal = GetCurrentPrincipal();
  mMediaPrincipalHandle = MakePrincipalHandle(newPrincipal);
  mOwner->NotifyDecoderPrincipalChanged();
}

void
MediaDecoder::NotifyBytesConsumed(int64_t aBytes, int64_t aOffset)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown());

  if (mIgnoreProgressData) {
    return;
  }

  MOZ_ASSERT(mDecoderStateMachine);
  if (aOffset >= mDecoderPosition) {
    mPlaybackStatistics->AddBytes(aBytes);
  }
  mDecoderPosition = aOffset + aBytes;
}

void
MediaDecoder::OnSeekResolved(SeekResolveValue aVal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown());
  mSeekRequest.Complete();

  bool fireEnded = false;
  {
    // An additional seek was requested while the current seek was
    // in operation.
    UnpinForSeek();
    fireEnded = aVal.mAtEnd;
    if (aVal.mAtEnd) {
      ChangeState(PLAY_STATE_ENDED);
    }
    mLogicallySeeking = false;
  }

  // Ensure logical position is updated after seek.
  UpdateLogicalPositionInternal();

  mOwner->SeekCompleted();
  AsyncResolveSeekDOMPromiseIfExists();
  if (fireEnded) {
    mOwner->PlaybackEnded();
  }
}

void
MediaDecoder::OnSeekRejected()
{
  MOZ_ASSERT(NS_IsMainThread());
  mSeekRequest.Complete();
  mLogicallySeeking = false;
  AsyncRejectSeekDOMPromiseIfExists();
}

void
MediaDecoder::SeekingStarted()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown());
  mOwner->SeekStarted();
}

void
MediaDecoder::ChangeState(PlayState aState)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown(), "SHUTDOWN is the final state.");

  if (mNextState == aState) {
    mNextState = PLAY_STATE_PAUSED;
  }

  DECODER_LOG("ChangeState %s => %s", PlayStateStr(), ToPlayStateStr(aState));
  mPlayState = aState;

  if (mPlayState == PLAY_STATE_PLAYING) {
    ConstructMediaTracks();
  } else if (IsEnded()) {
    RemoveMediaTracks();
  }
}

void
MediaDecoder::UpdateLogicalPositionInternal()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown());

  double currentPosition = static_cast<double>(CurrentPosition()) / static_cast<double>(USECS_PER_S);
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
  MOZ_ASSERT(!IsShutdown());

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

  DECODER_LOG("Duration changed to %f", mDuration);

  // Duration has changed so we should recompute playback rate
  UpdatePlaybackRate();

  // See https://www.w3.org/Bugs/Public/show_bug.cgi?id=28822 for a discussion
  // of whether we should fire durationchange on explicit infinity.
  if (mFiredMetadataLoaded &&
      (!mozilla::IsInfinite<double>(mDuration) || mExplicitDuration.Ref().isSome())) {
    mOwner->DispatchAsyncEvent(NS_LITERAL_STRING("durationchange"));
  }

  if (CurrentPosition() > TimeUnit::FromSeconds(mDuration).ToMicroseconds()) {
    Seek(mDuration, SeekTarget::Accurate);
  }
}

void
MediaDecoder::SetElementVisibility(bool aIsVisible)
{
  MOZ_ASSERT(NS_IsMainThread());
  mElementVisible = aIsVisible;
  mIsVisible = !mForcedHidden && mElementVisible;
}

void
MediaDecoder::SetForcedHidden(bool aForcedHidden)
{
  MOZ_ASSERT(NS_IsMainThread());
  mForcedHidden = aForcedHidden;
  SetElementVisibility(mElementVisible);
}

void
MediaDecoder::UpdateEstimatedMediaDuration(int64_t aDuration)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mPlayState <= PLAY_STATE_LOADING) {
    return;
  }

  // The duration is only changed if its significantly different than the
  // the current estimate, as the incoming duration is an estimate and so
  // often is unstable as more data is read and the estimate is updated.
  // Can result in a durationchangeevent. aDuration is in microseconds.
  if (mEstimatedDuration.Ref().isSome() &&
      mozilla::Abs(mEstimatedDuration.Ref().ref().ToMicroseconds() - aDuration) < ESTIMATED_DURATION_FUZZ_FACTOR_USECS) {
    return;
  }

  mEstimatedDuration = Some(TimeUnit::FromMicroseconds(aDuration));
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
      media::TimeInterval(media::TimeUnit::FromMicroseconds(0),
                          IsInfinite() ?
                            media::TimeUnit::FromInfinity() :
                            media::TimeUnit::FromSeconds(GetDuration())));
  }
}

void
MediaDecoder::SetFragmentEndTime(double aTime)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoderStateMachine) {
    mDecoderStateMachine->DispatchSetFragmentEndTime(static_cast<int64_t>(aTime * USECS_PER_S));
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


  if (oldRate == 0 && !mOwner->GetPaused()) {
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
MediaDecoder::ConnectMirrors(MediaDecoderStateMachine* aObject)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aObject);
  mStateMachineDuration.Connect(aObject->CanonicalDuration());
  mBuffered.Connect(aObject->CanonicalBuffered());
  mStateMachineIsShutdown.Connect(aObject->CanonicalIsShutdown());
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
  mStateMachineIsShutdown.DisconnectIfConnected();
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
  } else {
    DisconnectMirrors();
  }
}

ImageContainer*
MediaDecoder::GetImageContainer()
{
  return mVideoFrameContainer ? mVideoFrameContainer->GetImageContainer() : nullptr;
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
MediaDecoder::GetBuffered() {
  MOZ_ASSERT(NS_IsMainThread());
  return mBuffered.Ref();
}

size_t
MediaDecoder::SizeOfVideoQueue() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoderStateMachine) {
    return mDecoderStateMachine->SizeOfVideoQueue();
  }
  return 0;
}

size_t
MediaDecoder::SizeOfAudioQueue() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoderStateMachine) {
    return mDecoderStateMachine->SizeOfAudioQueue();
  }
  return 0;
}

void MediaDecoder::AddSizeOfResources(ResourceSizes* aSizes) {
  MOZ_ASSERT(NS_IsMainThread());
  if (GetResource()) {
    aSizes->mByteSize += GetResource()->SizeOfIncludingThis(aSizes->mMallocSizeOf);
  }
}

void
MediaDecoder::NotifyDataArrived() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown());
  mDataArrivedEvent.Notify();
}

// Provide access to the state machine object
MediaDecoderStateMachine*
MediaDecoder::GetStateMachine() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mDecoderStateMachine;
}

void
MediaDecoder::FireTimeUpdate()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown());
  mOwner->FireTimeUpdate(true);
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
  return AndroidBridge::Bridge() &&
         AndroidBridge::Bridge()->GetAPIVersion() < 16 &&
         Preferences::GetBool("media.plugins.enabled");
}
#endif

NS_IMETHODIMP
MediaMemoryTracker::CollectReports(nsIHandleReportCallback* aHandleReport,
                                   nsISupports* aData, bool aAnonymize)
{
  int64_t video = 0, audio = 0;

  // NB: When resourceSizes' ref count goes to 0 the promise will report the
  //     resources memory and finish the asynchronous memory report.
  RefPtr<MediaDecoder::ResourceSizes> resourceSizes =
      new MediaDecoder::ResourceSizes(MediaMemoryTracker::MallocSizeOf);

  nsCOMPtr<nsIHandleReportCallback> handleReport = aHandleReport;
  nsCOMPtr<nsISupports> data = aData;

  resourceSizes->Promise()->Then(
      AbstractThread::MainThread(), __func__,
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
  // mOwner is valid until shutdown.
  return mOwner;
}

void
MediaDecoder::ConstructMediaTracks()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown());

  if (mMediaTracksConstructed || !mInfo) {
    return;
  }

  HTMLMediaElement* element = mOwner->GetMediaElement();
  if (!element) {
    return;
  }

  mMediaTracksConstructed = true;

  AudioTrackList* audioList = element->AudioTracks();
  if (audioList && mInfo->HasAudio()) {
    const TrackInfo& info = mInfo->mAudio;
    RefPtr<AudioTrack> track = MediaTrackList::CreateAudioTrack(
    info.mId, info.mKind, info.mLabel, info.mLanguage, info.mEnabled);

    audioList->AddTrack(track);
  }

  VideoTrackList* videoList = element->VideoTracks();
  if (videoList && mInfo->HasVideo()) {
    const TrackInfo& info = mInfo->mVideo;
    RefPtr<VideoTrack> track = MediaTrackList::CreateVideoTrack(
    info.mId, info.mKind, info.mLabel, info.mLanguage);

    videoList->AddTrack(track);
    track->SetEnabledInternal(info.mEnabled, MediaTrack::FIRE_NO_EVENTS);
  }
}

void
MediaDecoder::RemoveMediaTracks()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown());

  HTMLMediaElement* element = mOwner->GetMediaElement();
  if (!element) {
    return;
  }

  AudioTrackList* audioList = element->AudioTracks();
  if (audioList) {
    audioList->RemoveTracks();
  }

  VideoTrackList* videoList = element->VideoTracks();
  if (videoList) {
    videoList->RemoveTracks();
  }

  mMediaTracksConstructed = false;
}

MediaDecoderOwner::NextFrameStatus
MediaDecoder::NextFrameBufferedStatus()
{
  MOZ_ASSERT(NS_IsMainThread());
  // Next frame hasn't been decoded yet.
  // Use the buffered range to consider if we have the next frame available.
  media::TimeUnit currentPosition =
    media::TimeUnit::FromMicroseconds(CurrentPosition());
  media::TimeInterval interval(currentPosition,
                               currentPosition + media::TimeUnit::FromMicroseconds(DEFAULT_NEXT_FRAME_AVAILABLE_BUFFERED));
  return GetBuffered().Contains(interval)
    ? MediaDecoderOwner::NEXT_FRAME_AVAILABLE
    : MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE;
}

void
MediaDecoder::DumpDebugInfo()
{
  MOZ_ASSERT(!IsShutdown());
  DUMP_LOG("metadata: channels=%u rate=%u hasAudio=%d hasVideo=%d, "
           "state: mPlayState=%s mdsm=%p",
           mInfo ? mInfo->mAudio.mChannels : 0, mInfo ? mInfo->mAudio.mRate : 0,
           mInfo ? mInfo->HasAudio() : 0, mInfo ? mInfo->HasVideo() : 0,
           PlayStateStr(), GetStateMachine());

  nsString str;
  GetMozDebugReaderData(str);
  if (!str.IsEmpty()) {
    DUMP_LOG("reader data:\n%s", NS_ConvertUTF16toUTF8(str).get());
  }

  if (GetStateMachine()) {
    GetStateMachine()->DumpDebugInfo();
  }
}

void
MediaDecoder::NotifyAudibleStateChanged()
{
  MOZ_ASSERT(!IsShutdown());
  mOwner->SetAudibleState(mIsAudioDataAudible);
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
#undef DECODER_LOG
