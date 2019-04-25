/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDecoder.h"

#include "DOMMediaStream.h"
#include "ImageContainer.h"
#include "Layers.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "MediaResource.h"
#include "MediaShutdownManager.h"
#include "VideoFrameContainer.h"
#include "VideoUtils.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "Visibility.h"
#include "mozilla/Unused.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "nsPrintfCString.h"
#include "nsTArray.h"
#include <algorithm>
#include <limits>

using namespace mozilla::dom;
using namespace mozilla::layers;
using namespace mozilla::media;

namespace mozilla {

// GetCurrentTime is defined in winbase.h as zero argument macro forwarding to
// GetTickCount() and conflicts with MediaDecoder::GetCurrentTime
// implementation.
#ifdef GetCurrentTime
#  undef GetCurrentTime
#endif

// avoid redefined macro in unified build
#undef LOG
#undef DUMP

LazyLogModule gMediaDecoderLog("MediaDecoder");
#define LOG(x, ...) \
  DDMOZ_LOG(gMediaDecoderLog, LogLevel::Debug, x, ##__VA_ARGS__)

#define DUMP(x, ...) printf_stderr(x "\n", ##__VA_ARGS__)

#define NS_DispatchToMainThread(...) CompileError_UseAbstractMainThreadInstead

static const char* ToPlayStateStr(MediaDecoder::PlayState aState) {
  switch (aState) {
    case MediaDecoder::PLAY_STATE_START:
      return "START";
    case MediaDecoder::PLAY_STATE_LOADING:
      return "LOADING";
    case MediaDecoder::PLAY_STATE_PAUSED:
      return "PAUSED";
    case MediaDecoder::PLAY_STATE_PLAYING:
      return "PLAYING";
    case MediaDecoder::PLAY_STATE_ENDED:
      return "ENDED";
    case MediaDecoder::PLAY_STATE_SHUTDOWN:
      return "SHUTDOWN";
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid playState.");
  }
  return "UNKNOWN";
}

class MediaMemoryTracker : public nsIMemoryReporter {
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
  static DecodersArray& Decoders() { return UniqueInstance()->mDecoders; }

  DecodersArray mDecoders;

 public:
  static void AddMediaDecoder(MediaDecoder* aDecoder) {
    Decoders().AppendElement(aDecoder);
  }

  static void RemoveMediaDecoder(MediaDecoder* aDecoder) {
    DecodersArray& decoders = Decoders();
    decoders.RemoveElement(aDecoder);
    if (decoders.IsEmpty()) {
      sUniqueInstance = nullptr;
    }
  }

  static RefPtr<MediaMemoryPromise> GetSizes() {
    MOZ_ASSERT(NS_IsMainThread());
    DecodersArray& decoders = Decoders();

    // if we don't have any decoder, we can bail
    if (decoders.IsEmpty()) {
      // and release the instance that was created by calling Decoders()
      sUniqueInstance = nullptr;
      return MediaMemoryPromise::CreateAndResolve(MediaMemoryInfo(), __func__);
    }

    RefPtr<MediaDecoder::ResourceSizes> resourceSizes =
        new MediaDecoder::ResourceSizes(MediaMemoryTracker::MallocSizeOf);

    size_t videoSize = 0;
    size_t audioSize = 0;

    for (auto&& decoder : decoders) {
      videoSize += decoder->SizeOfVideoQueue();
      audioSize += decoder->SizeOfAudioQueue();
      decoder->AddSizeOfResources(resourceSizes);
    }

    return resourceSizes->Promise()->Then(
        SystemGroup::AbstractMainThreadFor(TaskCategory::Performance), __func__,
        [videoSize, audioSize](size_t resourceSize) {
          return MediaMemoryPromise::CreateAndResolve(
              MediaMemoryInfo(videoSize, audioSize, resourceSize), __func__);
        },
        [](size_t) {
          return MediaMemoryPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                     __func__);
        });
  }
};

// When media is looping back to the head position, the spec [1] mentions that
// MediaElement should dispatch `seeking` first, `timeupdate`, and `seeked` in
// the end. This guard should be created before we fire `timeupdate` so that it
// can ensure the event order.
// [1]
// https://html.spec.whatwg.org/multipage/media.html#playing-the-media-resource:attr-media-loop-2
// https://html.spec.whatwg.org/multipage/media.html#seeking:dom-media-seek
class MOZ_RAII SeekEventsGuard {
 public:
  explicit SeekEventsGuard(MediaDecoderOwner* aOwner, bool aIsLoopingBack)
      : mOwner(aOwner), mIsLoopingBack(aIsLoopingBack) {
    MOZ_ASSERT(mOwner);
    if (mIsLoopingBack) {
      mOwner->SeekStarted();
    }
  }

  ~SeekEventsGuard() {
    MOZ_ASSERT(mOwner);
    if (mIsLoopingBack) {
      mOwner->SeekCompleted();
    }
  }

 private:
  MediaDecoderOwner* mOwner;
  bool mIsLoopingBack;
};

StaticRefPtr<MediaMemoryTracker> MediaMemoryTracker::sUniqueInstance;

RefPtr<MediaMemoryPromise> GetMediaMemorySizes() {
  return MediaMemoryTracker::GetSizes();
}

LazyLogModule gMediaTimerLog("MediaTimer");

constexpr TimeUnit MediaDecoder::DEFAULT_NEXT_FRAME_AVAILABLE_BUFFERED;

void MediaDecoder::InitStatics() {
  MOZ_ASSERT(NS_IsMainThread());
  // Eagerly init gMediaDecoderLog to work around bug 1415441.
  MOZ_LOG(gMediaDecoderLog, LogLevel::Info, ("MediaDecoder::InitStatics"));
}

NS_IMPL_ISUPPORTS(MediaMemoryTracker, nsIMemoryReporter)

void MediaDecoder::NotifyOwnerActivityChanged(bool aIsDocumentVisible,
                                              Visibility aElementVisibility,
                                              bool aIsElementInTree) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  AbstractThread::AutoEnter context(AbstractMainThread());
  SetElementVisibility(aIsDocumentVisible, aElementVisibility,
                       aIsElementInTree);

  NotifyCompositor();
}

void MediaDecoder::Pause() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  AbstractThread::AutoEnter context(AbstractMainThread());
  if (mPlayState == PLAY_STATE_LOADING || IsEnded()) {
    mNextState = PLAY_STATE_PAUSED;
    return;
  }
  ChangeState(PLAY_STATE_PAUSED);
}

void MediaDecoder::SetVolume(double aVolume) {
  MOZ_ASSERT(NS_IsMainThread());
  AbstractThread::AutoEnter context(AbstractMainThread());
  mVolume = aVolume;
}

RefPtr<GenericPromise> MediaDecoder::SetSink(AudioDeviceInfo* aSink) {
  MOZ_ASSERT(NS_IsMainThread());
  AbstractThread::AutoEnter context(AbstractMainThread());
  return GetStateMachine()->InvokeSetSink(aSink);
}

void MediaDecoder::SetOutputStreamCORSMode(CORSMode aCORSMode) {
  MOZ_ASSERT(NS_IsMainThread());
  AbstractThread::AutoEnter context(AbstractMainThread());
  mDecoderStateMachine->SetOutputStreamCORSMode(aCORSMode);
}

void MediaDecoder::AddOutputStream(DOMMediaStream* aStream) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDecoderStateMachine, "Must be called after Load().");
  AbstractThread::AutoEnter context(AbstractMainThread());
  mDecoderStateMachine->EnsureOutputStreamManager(
      aStream->GetInputStream()->Graph());
  if (mInfo) {
    mDecoderStateMachine->EnsureOutputStreamManagerHasTracks(*mInfo);
  }
  mDecoderStateMachine->AddOutputStream(aStream);
}

void MediaDecoder::RemoveOutputStream(DOMMediaStream* aStream) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDecoderStateMachine, "Must be called after Load().");
  AbstractThread::AutoEnter context(AbstractMainThread());
  mDecoderStateMachine->RemoveOutputStream(aStream);
}

void MediaDecoder::SetNextOutputStreamTrackID(TrackID aNextTrackID) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDecoderStateMachine, "Must be called after Load().");
  AbstractThread::AutoEnter context(AbstractMainThread());
  mDecoderStateMachine->SetNextOutputStreamTrackID(aNextTrackID);
}

TrackID MediaDecoder::GetNextOutputStreamTrackID() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDecoderStateMachine, "Must be called after Load().");
  AbstractThread::AutoEnter context(AbstractMainThread());
  return mDecoderStateMachine->GetNextOutputStreamTrackID();
}

double MediaDecoder::GetDuration() {
  MOZ_ASSERT(NS_IsMainThread());
  AbstractThread::AutoEnter context(AbstractMainThread());
  return mDuration;
}

bool MediaDecoder::IsInfinite() const {
  MOZ_ASSERT(NS_IsMainThread());
  AbstractThread::AutoEnter context(AbstractMainThread());
  return mozilla::IsInfinite<double>(mDuration);
}

#define INIT_MIRROR(name, val) \
  name(mOwner->AbstractMainThread(), val, "MediaDecoder::" #name " (Mirror)")
#define INIT_CANONICAL(name, val) \
  name(mOwner->AbstractMainThread(), val, "MediaDecoder::" #name " (Canonical)")

MediaDecoder::MediaDecoder(MediaDecoderInit& aInit)
    : mWatchManager(this, aInit.mOwner->AbstractMainThread()),
      mLogicalPosition(0.0),
      mDuration(std::numeric_limits<double>::quiet_NaN()),
      mOwner(aInit.mOwner),
      mAbstractMainThread(aInit.mOwner->AbstractMainThread()),
      mFrameStats(new FrameStatistics()),
      mVideoFrameContainer(aInit.mOwner->GetVideoFrameContainer()),
      mMinimizePreroll(aInit.mMinimizePreroll),
      mFiredMetadataLoaded(false),
      mIsDocumentVisible(false),
      mElementVisibility(Visibility::Untracked),
      mIsElementInTree(false),
      mForcedHidden(false),
      mHasSuspendTaint(aInit.mHasSuspendTaint),
      mIsCloningVisually(false),
      mPlaybackRate(aInit.mPlaybackRate),
      mLogicallySeeking(false, "MediaDecoder::mLogicallySeeking"),
      INIT_MIRROR(mBuffered, TimeIntervals()),
      INIT_MIRROR(mCurrentPosition, TimeUnit::Zero()),
      INIT_MIRROR(mStateMachineDuration, NullableTimeUnit()),
      INIT_MIRROR(mIsAudioDataAudible, false),
      INIT_CANONICAL(mVolume, aInit.mVolume),
      INIT_CANONICAL(mPreservesPitch, aInit.mPreservesPitch),
      INIT_CANONICAL(mLooping, aInit.mLooping),
      INIT_CANONICAL(mPlayState, PLAY_STATE_LOADING),
      INIT_CANONICAL(mSameOriginMedia, false),
      mVideoDecodingOberver(
          new BackgroundVideoDecodingPermissionObserver(this)),
      mIsBackgroundVideoDecodingAllowed(false),
      mTelemetryReported(false),
      mContainerType(aInit.mContainerType) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mAbstractMainThread);
  MediaMemoryTracker::AddMediaDecoder(this);

  //
  // Initialize watchers.
  //

  // mDuration
  mWatchManager.Watch(mStateMachineDuration, &MediaDecoder::DurationChanged);

  // readyState
  mWatchManager.Watch(mPlayState, &MediaDecoder::UpdateReadyState);
  // ReadyState computation depends on MediaDecoder::CanPlayThrough, which
  // depends on the download rate.
  mWatchManager.Watch(mBuffered, &MediaDecoder::UpdateReadyState);

  // mLogicalPosition
  mWatchManager.Watch(mCurrentPosition, &MediaDecoder::UpdateLogicalPosition);
  mWatchManager.Watch(mPlayState, &MediaDecoder::UpdateLogicalPosition);
  mWatchManager.Watch(mLogicallySeeking, &MediaDecoder::UpdateLogicalPosition);

  mWatchManager.Watch(mIsAudioDataAudible,
                      &MediaDecoder::NotifyAudibleStateChanged);

  mVideoDecodingOberver->RegisterEvent();
}

#undef INIT_MIRROR
#undef INIT_CANONICAL

void MediaDecoder::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  AbstractThread::AutoEnter context(AbstractMainThread());

  // Unwatch all watch targets to prevent further notifications.
  mWatchManager.Shutdown();

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
    mOnEncrypted.Disconnect();
    mOnWaitingForKey.Disconnect();
    mOnDecodeWarning.Disconnect();
    mOnNextFrameStatus.Disconnect();

    mDecoderStateMachine->BeginShutdown()->Then(
        mAbstractMainThread, __func__, this, &MediaDecoder::FinishShutdown,
        &MediaDecoder::FinishShutdown);
  } else {
    // Ensure we always unregister asynchronously in order not to disrupt
    // the hashtable iterating in MediaShutdownManager::Shutdown().
    RefPtr<MediaDecoder> self = this;
    nsCOMPtr<nsIRunnable> r =
        NS_NewRunnableFunction("MediaDecoder::Shutdown", [self]() {
          self->mVideoFrameContainer = nullptr;
          MediaShutdownManager::Instance().Unregister(self);
        });
    mAbstractMainThread->Dispatch(r.forget());
  }

  // Ask the owner to remove its audio/video tracks.
  GetOwner()->RemoveMediaTracks();

  ChangeState(PLAY_STATE_SHUTDOWN);
  mVideoDecodingOberver->UnregisterEvent();
  mVideoDecodingOberver = nullptr;
  mOwner = nullptr;
}

void MediaDecoder::NotifyXPCOMShutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  // NotifyXPCOMShutdown will clear its reference to mDecoder. So we must ensure
  // that this MediaDecoder stays alive until completion.
  RefPtr<MediaDecoder> kungFuDeathGrip = this;
  if (auto owner = GetOwner()) {
    owner->NotifyXPCOMShutdown();
  } else if (!IsShutdown()) {
    Shutdown();
  }
  MOZ_DIAGNOSTIC_ASSERT(IsShutdown());
}

MediaDecoder::~MediaDecoder() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(IsShutdown());
  MediaMemoryTracker::RemoveMediaDecoder(this);
}

void MediaDecoder::OnPlaybackEvent(MediaPlaybackEvent&& aEvent) {
  switch (aEvent.mType) {
    case MediaPlaybackEvent::PlaybackEnded:
      PlaybackEnded();
      break;
    case MediaPlaybackEvent::SeekStarted:
      SeekingStarted();
      break;
    case MediaPlaybackEvent::Invalidate:
      Invalidate();
      break;
    case MediaPlaybackEvent::EnterVideoSuspend:
      GetOwner()->DispatchAsyncEvent(NS_LITERAL_STRING("mozentervideosuspend"));
      mIsVideoDecodingSuspended = true;
      break;
    case MediaPlaybackEvent::ExitVideoSuspend:
      GetOwner()->DispatchAsyncEvent(NS_LITERAL_STRING("mozexitvideosuspend"));
      mIsVideoDecodingSuspended = false;
      break;
    case MediaPlaybackEvent::StartVideoSuspendTimer:
      GetOwner()->DispatchAsyncEvent(
          NS_LITERAL_STRING("mozstartvideosuspendtimer"));
      break;
    case MediaPlaybackEvent::CancelVideoSuspendTimer:
      GetOwner()->DispatchAsyncEvent(
          NS_LITERAL_STRING("mozcancelvideosuspendtimer"));
      break;
    case MediaPlaybackEvent::VideoOnlySeekBegin:
      GetOwner()->DispatchAsyncEvent(
          NS_LITERAL_STRING("mozvideoonlyseekbegin"));
      break;
    case MediaPlaybackEvent::VideoOnlySeekCompleted:
      GetOwner()->DispatchAsyncEvent(
          NS_LITERAL_STRING("mozvideoonlyseekcompleted"));
      break;
    default:
      break;
  }
}

bool MediaDecoder::IsVideoDecodingSuspended() const {
  return mIsVideoDecodingSuspended;
}

void MediaDecoder::OnPlaybackErrorEvent(const MediaResult& aError) {
  DecodeError(aError);
}

void MediaDecoder::OnDecoderDoctorEvent(DecoderDoctorEvent aEvent) {
  MOZ_ASSERT(NS_IsMainThread());
  // OnDecoderDoctorEvent is disconnected at shutdown time.
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  Document* doc = GetOwner()->GetDocument();
  if (!doc) {
    return;
  }
  DecoderDoctorDiagnostics diags;
  diags.StoreEvent(doc, aEvent, __func__);
}

static const char* NextFrameStatusToStr(
    MediaDecoderOwner::NextFrameStatus aStatus) {
  switch (aStatus) {
    case MediaDecoderOwner::NEXT_FRAME_AVAILABLE:
      return "NEXT_FRAME_AVAILABLE";
    case MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE:
      return "NEXT_FRAME_UNAVAILABLE";
    case MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_BUFFERING:
      return "NEXT_FRAME_UNAVAILABLE_BUFFERING";
    case MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_SEEKING:
      return "NEXT_FRAME_UNAVAILABLE_SEEKING";
    case MediaDecoderOwner::NEXT_FRAME_UNINITIALIZED:
      return "NEXT_FRAME_UNINITIALIZED";
  }
  return "UNKNOWN";
}

void MediaDecoder::OnNextFrameStatus(
    MediaDecoderOwner::NextFrameStatus aStatus) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  if (mNextFrameStatus != aStatus) {
    LOG("Changed mNextFrameStatus to %s", NextFrameStatusToStr(aStatus));
    mNextFrameStatus = aStatus;
    UpdateReadyState();
  }
}

void MediaDecoder::FinishShutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  SetStateMachine(nullptr);
  mVideoFrameContainer = nullptr;
  MediaShutdownManager::Instance().Unregister(this);
}

nsresult MediaDecoder::InitializeStateMachine() {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ASSERTION(mDecoderStateMachine, "Cannot initialize null state machine!");
  AbstractThread::AutoEnter context(AbstractMainThread());

  nsresult rv = mDecoderStateMachine->Init(this);
  NS_ENSURE_SUCCESS(rv, rv);

  // If some parameters got set before the state machine got created,
  // set them now
  SetStateMachineParameters();

  return NS_OK;
}

void MediaDecoder::SetStateMachineParameters() {
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
  mOnNextFrameStatus = mDecoderStateMachine->OnNextFrameStatus().Connect(
      mAbstractMainThread, this, &MediaDecoder::OnNextFrameStatus);

  mOnEncrypted = mReader->OnEncrypted().Connect(
      mAbstractMainThread, GetOwner(), &MediaDecoderOwner::DispatchEncrypted);
  mOnWaitingForKey = mReader->OnWaitingForKey().Connect(
      mAbstractMainThread, GetOwner(), &MediaDecoderOwner::NotifyWaitingForKey);
  mOnDecodeWarning = mReader->OnDecodeWarning().Connect(
      mAbstractMainThread, GetOwner(), &MediaDecoderOwner::DecodeWarning);
}

void MediaDecoder::Play() {
  MOZ_ASSERT(NS_IsMainThread());

  AbstractThread::AutoEnter context(AbstractMainThread());
  NS_ASSERTION(mDecoderStateMachine != nullptr, "Should have state machine.");
  if (mPlaybackRate == 0) {
    return;
  }

  if (IsEnded()) {
    Seek(0, SeekTarget::PrevSyncPoint);
    return;
  } else if (mPlayState == PLAY_STATE_LOADING) {
    mNextState = PLAY_STATE_PLAYING;
    return;
  }

  ChangeState(PLAY_STATE_PLAYING);
}

void MediaDecoder::Seek(double aTime, SeekTarget::Type aSeekType) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  AbstractThread::AutoEnter context(AbstractMainThread());
  MOZ_ASSERT(aTime >= 0.0, "Cannot seek to a negative value.");

  auto time = TimeUnit::FromSeconds(aTime);

  mLogicalPosition = aTime;

  mLogicallySeeking = true;
  SeekTarget target = SeekTarget(time, aSeekType);
  CallSeek(target);

  if (mPlayState == PLAY_STATE_ENDED) {
    ChangeState(GetOwner()->GetPaused() ? PLAY_STATE_PAUSED
                                        : PLAY_STATE_PLAYING);
  }
}

void MediaDecoder::DiscardOngoingSeekIfExists() {
  MOZ_ASSERT(NS_IsMainThread());
  AbstractThread::AutoEnter context(AbstractMainThread());
  mSeekRequest.DisconnectIfExists();
  GetOwner()->AsyncRejectSeekDOMPromiseIfExists();
}

void MediaDecoder::CallSeek(const SeekTarget& aTarget) {
  MOZ_ASSERT(NS_IsMainThread());
  AbstractThread::AutoEnter context(AbstractMainThread());
  DiscardOngoingSeekIfExists();

  mDecoderStateMachine->InvokeSeek(aTarget)
      ->Then(mAbstractMainThread, __func__, this, &MediaDecoder::OnSeekResolved,
             &MediaDecoder::OnSeekRejected)
      ->Track(mSeekRequest);
}

double MediaDecoder::GetCurrentTime() {
  MOZ_ASSERT(NS_IsMainThread());
  AbstractThread::AutoEnter context(AbstractMainThread());
  return mLogicalPosition;
}

void MediaDecoder::OnMetadataUpdate(TimedMetadata&& aMetadata) {
  MOZ_ASSERT(NS_IsMainThread());
  AbstractThread::AutoEnter context(AbstractMainThread());
  GetOwner()->RemoveMediaTracks();
  MetadataLoaded(MakeUnique<MediaInfo>(*aMetadata.mInfo),
                 UniquePtr<MetadataTags>(std::move(aMetadata.mTags)),
                 MediaDecoderEventVisibility::Observable);
  FirstFrameLoaded(std::move(aMetadata.mInfo),
                   MediaDecoderEventVisibility::Observable);
}

void MediaDecoder::MetadataLoaded(
    UniquePtr<MediaInfo> aInfo, UniquePtr<MetadataTags> aTags,
    MediaDecoderEventVisibility aEventVisibility) {
  MOZ_ASSERT(NS_IsMainThread());
  AbstractThread::AutoEnter context(AbstractMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  LOG("MetadataLoaded, channels=%u rate=%u hasAudio=%d hasVideo=%d",
      aInfo->mAudio.mChannels, aInfo->mAudio.mRate, aInfo->HasAudio(),
      aInfo->HasVideo());

  mMediaSeekable = aInfo->mMediaSeekable;
  mMediaSeekableOnlyInBufferedRanges =
      aInfo->mMediaSeekableOnlyInBufferedRanges;
  mInfo = aInfo.release();
  GetOwner()->ConstructMediaTracks(mInfo);
  mDecoderStateMachine->EnsureOutputStreamManagerHasTracks(*mInfo);

  // Make sure the element and the frame (if any) are told about
  // our new size.
  if (aEventVisibility != MediaDecoderEventVisibility::Suppressed) {
    mFiredMetadataLoaded = true;
    GetOwner()->MetadataLoaded(mInfo, std::move(aTags));
  }
  // Invalidate() will end up calling GetOwner()->UpdateMediaSize with the last
  // dimensions retrieved from the video frame container. The video frame
  // container contains more up to date dimensions than aInfo.
  // So we call Invalidate() after calling GetOwner()->MetadataLoaded to ensure
  // the media element has the latest dimensions.
  Invalidate();

  EnsureTelemetryReported();
}

void MediaDecoder::EnsureTelemetryReported() {
  MOZ_ASSERT(NS_IsMainThread());
  AbstractThread::AutoEnter context(AbstractMainThread());

  if (mTelemetryReported || !mInfo) {
    // Note: sometimes we get multiple MetadataLoaded calls (for example
    // for chained ogg). So we ensure we don't report duplicate results for
    // these resources.
    return;
  }

  nsTArray<nsCString> codecs;
  if (mInfo->HasAudio() &&
      !mInfo->mAudio.GetAsAudioInfo()->mMimeType.IsEmpty()) {
    codecs.AppendElement(mInfo->mAudio.GetAsAudioInfo()->mMimeType);
  }
  if (mInfo->HasVideo() &&
      !mInfo->mVideo.GetAsVideoInfo()->mMimeType.IsEmpty()) {
    codecs.AppendElement(mInfo->mVideo.GetAsVideoInfo()->mMimeType);
  }
  if (codecs.IsEmpty()) {
    codecs.AppendElement(nsPrintfCString(
        "resource; %s", ContainerType().OriginalString().Data()));
  }
  for (const nsCString& codec : codecs) {
    LOG("Telemetry MEDIA_CODEC_USED= '%s'", codec.get());
    Telemetry::Accumulate(Telemetry::HistogramID::MEDIA_CODEC_USED, codec);
  }

  mTelemetryReported = true;
}

const char* MediaDecoder::PlayStateStr() {
  MOZ_ASSERT(NS_IsMainThread());
  AbstractThread::AutoEnter context(AbstractMainThread());
  return ToPlayStateStr(mPlayState);
}

void MediaDecoder::FirstFrameLoaded(
    nsAutoPtr<MediaInfo> aInfo, MediaDecoderEventVisibility aEventVisibility) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  AbstractThread::AutoEnter context(AbstractMainThread());

  LOG("FirstFrameLoaded, channels=%u rate=%u hasAudio=%d hasVideo=%d "
      "mPlayState=%s transportSeekable=%d",
      aInfo->mAudio.mChannels, aInfo->mAudio.mRate, aInfo->HasAudio(),
      aInfo->HasVideo(), PlayStateStr(), IsTransportSeekable());

  mInfo = aInfo.forget();

  Invalidate();

  // The element can run javascript via events
  // before reaching here, so only change the
  // state if we're still set to the original
  // loading state.
  if (mPlayState == PLAY_STATE_LOADING) {
    ChangeState(mNextState);
  }

  // GetOwner()->FirstFrameLoaded() might call us back. Put it at the bottom of
  // this function to avoid unexpected shutdown from reentrant calls.
  if (aEventVisibility != MediaDecoderEventVisibility::Suppressed) {
    GetOwner()->FirstFrameLoaded();
  }
}

void MediaDecoder::NetworkError(const MediaResult& aError) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  GetOwner()->NetworkError(aError);
}

void MediaDecoder::DecodeError(const MediaResult& aError) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  GetOwner()->DecodeError(aError);
}

void MediaDecoder::UpdateSameOriginStatus(bool aSameOrigin) {
  MOZ_ASSERT(NS_IsMainThread());
  AbstractThread::AutoEnter context(AbstractMainThread());
  nsCOMPtr<nsIPrincipal> principal = GetCurrentPrincipal();
  mDecoderStateMachine->SetOutputStreamPrincipal(principal);
  mSameOriginMedia = aSameOrigin;
}

bool MediaDecoder::IsSeeking() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mLogicallySeeking;
}

bool MediaDecoder::OwnerHasError() const {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  return GetOwner()->HasError();
}

bool MediaDecoder::IsEnded() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mPlayState == PLAY_STATE_ENDED;
}

bool MediaDecoder::IsShutdown() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mPlayState == PLAY_STATE_SHUTDOWN;
}

void MediaDecoder::PlaybackEnded() {
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
}

void MediaDecoder::NotifyPrincipalChanged() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  AbstractThread::AutoEnter context(AbstractMainThread());
  GetOwner()->NotifyDecoderPrincipalChanged();
}

void MediaDecoder::OnSeekResolved() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  AbstractThread::AutoEnter context(AbstractMainThread());
  mLogicallySeeking = false;

  // Ensure logical position is updated after seek.
  UpdateLogicalPositionInternal();
  mSeekRequest.Complete();

  GetOwner()->SeekCompleted();
  GetOwner()->AsyncResolveSeekDOMPromiseIfExists();
}

void MediaDecoder::OnSeekRejected() {
  MOZ_ASSERT(NS_IsMainThread());
  mSeekRequest.Complete();
  mLogicallySeeking = false;
  GetOwner()->AsyncRejectSeekDOMPromiseIfExists();
}

void MediaDecoder::SeekingStarted() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  GetOwner()->SeekStarted();
}

void MediaDecoder::ChangeState(PlayState aState) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown(), "SHUTDOWN is the final state.");
  AbstractThread::AutoEnter context(AbstractMainThread());

  if (mNextState == aState) {
    mNextState = PLAY_STATE_PAUSED;
  }

  if (mPlayState != aState) {
    DDLOG(DDLogCategory::Property, "play_state", ToPlayStateStr(aState));
  }
  mPlayState = aState;

  if (mPlayState == PLAY_STATE_PLAYING) {
    GetOwner()->ConstructMediaTracks(mInfo);
  } else if (IsEnded()) {
    GetOwner()->RemoveMediaTracks();
  }
}

bool MediaDecoder::IsLoopingBack(double aPrevPos, double aCurPos) const {
  // If current position is early than previous position and we didn't do seek,
  // that means we looped back to the start position.
  return mLooping && !mSeekRequest.Exists() && aCurPos < aPrevPos;
}

void MediaDecoder::UpdateLogicalPositionInternal() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  double currentPosition = CurrentPosition().ToSeconds();
  if (mPlayState == PLAY_STATE_ENDED) {
    currentPosition = std::max(currentPosition, mDuration);
  }
  bool logicalPositionChanged = mLogicalPosition != currentPosition;
  SeekEventsGuard guard(GetOwner(),
                        IsLoopingBack(mLogicalPosition, currentPosition));
  mLogicalPosition = currentPosition;
  DDLOG(DDLogCategory::Property, "currentTime", mLogicalPosition);

  // Invalidate the frame so any video data is displayed.
  // Do this before the timeupdate event so that if that
  // event runs JavaScript that queries the media size, the
  // frame has reflowed and the size updated beforehand.
  Invalidate();

  if (logicalPositionChanged) {
    FireTimeUpdate();
  }
}

void MediaDecoder::DurationChanged() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  AbstractThread::AutoEnter context(AbstractMainThread());

  double oldDuration = mDuration;

  // Use the explicit duration if we have one.
  // Otherwise use the duration mirrored from MDSM.
  if (mExplicitDuration.isSome()) {
    mDuration = mExplicitDuration.ref();
  } else if (mStateMachineDuration.Ref().isSome()) {
    mDuration = mStateMachineDuration.Ref().ref().ToSeconds();
  }

  if (mDuration == oldDuration || IsNaN(mDuration)) {
    return;
  }

  LOG("Duration changed to %f", mDuration);

  // See https://www.w3.org/Bugs/Public/show_bug.cgi?id=28822 for a discussion
  // of whether we should fire durationchange on explicit infinity.
  if (mFiredMetadataLoaded &&
      (!mozilla::IsInfinite<double>(mDuration) || mExplicitDuration.isSome())) {
    GetOwner()->DispatchAsyncEvent(NS_LITERAL_STRING("durationchange"));
  }

  if (CurrentPosition() > TimeUnit::FromSeconds(mDuration)) {
    Seek(mDuration, SeekTarget::Accurate);
  }
}

already_AddRefed<KnowsCompositor> MediaDecoder::GetCompositor() {
  MediaDecoderOwner* owner = GetOwner();
  Document* ownerDoc = owner ? owner->GetDocument() : nullptr;
  RefPtr<LayerManager> layerManager =
      ownerDoc ? nsContentUtils::LayerManagerForDocument(ownerDoc) : nullptr;
  RefPtr<KnowsCompositor> knows =
      layerManager ? layerManager->AsKnowsCompositor() : nullptr;
  return knows ? knows->GetForMedia().forget() : nullptr;
}

void MediaDecoder::NotifyCompositor() {
  RefPtr<KnowsCompositor> knowsCompositor = GetCompositor();
  if (knowsCompositor) {
    nsCOMPtr<nsIRunnable> r =
        NewRunnableMethod<already_AddRefed<KnowsCompositor>&&>(
            "MediaFormatReader::UpdateCompositor", mReader,
            &MediaFormatReader::UpdateCompositor, knowsCompositor.forget());
    Unused << mReader->OwnerThread()->Dispatch(r.forget());
  }
}

void MediaDecoder::SetElementVisibility(bool aIsDocumentVisible,
                                        Visibility aElementVisibility,
                                        bool aIsElementInTree) {
  MOZ_ASSERT(NS_IsMainThread());
  mIsDocumentVisible = aIsDocumentVisible;
  mElementVisibility = aElementVisibility;
  mIsElementInTree = aIsElementInTree;
  UpdateVideoDecodeMode();
}

void MediaDecoder::SetForcedHidden(bool aForcedHidden) {
  MOZ_ASSERT(NS_IsMainThread());
  mForcedHidden = aForcedHidden;
  UpdateVideoDecodeMode();
}

void MediaDecoder::SetSuspendTaint(bool aTainted) {
  MOZ_ASSERT(NS_IsMainThread());
  mHasSuspendTaint = aTainted;
  UpdateVideoDecodeMode();
}

void MediaDecoder::UpdateVideoDecodeMode() {
  MOZ_ASSERT(NS_IsMainThread());
  AbstractThread::AutoEnter context(mAbstractMainThread);

  // The MDSM may yet be set.
  if (!mDecoderStateMachine) {
    LOG("UpdateVideoDecodeMode(), early return because we don't have MDSM.");
    return;
  }

  // Seeking is required when leaving suspend mode.
  if (!mMediaSeekable) {
    LOG("UpdateVideoDecodeMode(), set Normal because the media is not "
        "seekable");
    mDecoderStateMachine->SetVideoDecodeMode(VideoDecodeMode::Normal);
    return;
  }

  // If mHasSuspendTaint is set, never suspend the video decoder.
  if (mHasSuspendTaint) {
    LOG("UpdateVideoDecodeMode(), set Normal because the element has been "
        "tainted.");
    mDecoderStateMachine->SetVideoDecodeMode(VideoDecodeMode::Normal);
    return;
  }

  // If mIsCloningVisually is set, never suspend the video decoder.
  if (mIsCloningVisually) {
    LOG("UpdateVideoDecodeMode(), set Normal because the element is cloning "
        "itself visually to another video container.");
    mDecoderStateMachine->SetVideoDecodeMode(VideoDecodeMode::Normal);
    return;
  }

  // Don't suspend elements that is not in tree.
  if (!mIsElementInTree) {
    LOG("UpdateVideoDecodeMode(), set Normal because the element is not in "
        "tree.");
    mDecoderStateMachine->SetVideoDecodeMode(VideoDecodeMode::Normal);
    return;
  }

  // If mForcedHidden is set, suspend the video decoder anyway.
  if (mForcedHidden) {
    LOG("UpdateVideoDecodeMode(), set Suspend because the element is forced to "
        "be suspended.");
    mDecoderStateMachine->SetVideoDecodeMode(VideoDecodeMode::Suspend);
    return;
  }

  // Resume decoding in the advance, even the element is in the background.
  if (mIsBackgroundVideoDecodingAllowed) {
    LOG("UpdateVideoDecodeMode(), set Normal because the tab is in background "
        "and hovered.");
    mDecoderStateMachine->SetVideoDecodeMode(VideoDecodeMode::Normal);
    return;
  }

  // If the element is in-tree with UNTRACKED visibility, that means the element
  // is not close enough to the viewport so we have not start to update its
  // visibility. In this case, it's equals to invisible.
  if (mIsElementInTree && mElementVisibility == Visibility::Untracked) {
    LOG("UpdateVideoDecodeMode(), set Suspend because element hasn't be "
        "updated visibility state.");
    mDecoderStateMachine->SetVideoDecodeMode(VideoDecodeMode::Suspend);
    return;
  }

  // Otherwise, depends on the owner's visibility state.
  // A element is visible only if its document is visible and the element
  // itself is visible.
  if (mIsDocumentVisible &&
      mElementVisibility == Visibility::ApproximatelyVisible) {
    LOG("UpdateVideoDecodeMode(), set Normal because the element visible.");
    mDecoderStateMachine->SetVideoDecodeMode(VideoDecodeMode::Normal);
  } else {
    LOG("UpdateVideoDecodeMode(), set Suspend because the element is not "
        "visible.");
    mDecoderStateMachine->SetVideoDecodeMode(VideoDecodeMode::Suspend);
  }
}

void MediaDecoder::SetIsBackgroundVideoDecodingAllowed(bool aAllowed) {
  mIsBackgroundVideoDecodingAllowed = aAllowed;
  UpdateVideoDecodeMode();
}

bool MediaDecoder::HasSuspendTaint() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mHasSuspendTaint;
}

void MediaDecoder::SetCloningVisually(bool aIsCloningVisually) {
  if (mIsCloningVisually != aIsCloningVisually) {
    mIsCloningVisually = aIsCloningVisually;
    UpdateVideoDecodeMode();
  }
}

bool MediaDecoder::IsMediaSeekable() {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(GetStateMachine(), false);
  return mMediaSeekable;
}

media::TimeIntervals MediaDecoder::GetSeekable() {
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
    return media::TimeIntervals(media::TimeInterval(
        TimeUnit::Zero(), IsInfinite() ? TimeUnit::FromInfinity()
                                       : TimeUnit::FromSeconds(GetDuration())));
  }
}

void MediaDecoder::SetFragmentEndTime(double aTime) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoderStateMachine) {
    mDecoderStateMachine->DispatchSetFragmentEndTime(
        TimeUnit::FromSeconds(aTime));
  }
}

void MediaDecoder::SetPlaybackRate(double aPlaybackRate) {
  MOZ_ASSERT(NS_IsMainThread());
  AbstractThread::AutoEnter context(AbstractMainThread());

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

void MediaDecoder::SetPreservesPitch(bool aPreservesPitch) {
  MOZ_ASSERT(NS_IsMainThread());
  AbstractThread::AutoEnter context(AbstractMainThread());
  mPreservesPitch = aPreservesPitch;
}

void MediaDecoder::SetLooping(bool aLooping) {
  MOZ_ASSERT(NS_IsMainThread());
  AbstractThread::AutoEnter context(AbstractMainThread());
  mLooping = aLooping;
}

void MediaDecoder::ConnectMirrors(MediaDecoderStateMachine* aObject) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aObject);
  mStateMachineDuration.Connect(aObject->CanonicalDuration());
  mBuffered.Connect(aObject->CanonicalBuffered());
  mCurrentPosition.Connect(aObject->CanonicalCurrentPosition());
  mIsAudioDataAudible.Connect(aObject->CanonicalIsAudioDataAudible());
}

void MediaDecoder::DisconnectMirrors() {
  MOZ_ASSERT(NS_IsMainThread());
  mStateMachineDuration.DisconnectIfConnected();
  mBuffered.DisconnectIfConnected();
  mCurrentPosition.DisconnectIfConnected();
  mIsAudioDataAudible.DisconnectIfConnected();
}

void MediaDecoder::SetStateMachine(MediaDecoderStateMachine* aStateMachine) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT_IF(aStateMachine, !mDecoderStateMachine);
  if (aStateMachine) {
    mDecoderStateMachine = aStateMachine;
    DDLINKCHILD("decoder state machine", mDecoderStateMachine.get());
    ConnectMirrors(aStateMachine);
    UpdateVideoDecodeMode();
  } else if (mDecoderStateMachine) {
    DDUNLINKCHILD(mDecoderStateMachine.get());
    mDecoderStateMachine = nullptr;
    DisconnectMirrors();
  }
}

ImageContainer* MediaDecoder::GetImageContainer() {
  return mVideoFrameContainer ? mVideoFrameContainer->GetImageContainer()
                              : nullptr;
}

void MediaDecoder::InvalidateWithFlags(uint32_t aFlags) {
  if (mVideoFrameContainer) {
    mVideoFrameContainer->InvalidateWithFlags(aFlags);
  }
}

void MediaDecoder::Invalidate() {
  if (mVideoFrameContainer) {
    mVideoFrameContainer->Invalidate();
  }
}

// Constructs the time ranges representing what segments of the media
// are buffered and playable.
media::TimeIntervals MediaDecoder::GetBuffered() {
  MOZ_ASSERT(NS_IsMainThread());
  return mBuffered.Ref();
}

size_t MediaDecoder::SizeOfVideoQueue() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoderStateMachine) {
    return mDecoderStateMachine->SizeOfVideoQueue();
  }
  return 0;
}

size_t MediaDecoder::SizeOfAudioQueue() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoderStateMachine) {
    return mDecoderStateMachine->SizeOfAudioQueue();
  }
  return 0;
}

void MediaDecoder::NotifyReaderDataArrived() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  nsresult rv = mReader->OwnerThread()->Dispatch(
      NewRunnableMethod("MediaFormatReader::NotifyDataArrived", mReader.get(),
                        &MediaFormatReader::NotifyDataArrived));
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  Unused << rv;
}

// Provide access to the state machine object
MediaDecoderStateMachine* MediaDecoder::GetStateMachine() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mDecoderStateMachine;
}

void MediaDecoder::FireTimeUpdate() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  GetOwner()->FireTimeUpdate(true);
}

bool MediaDecoder::CanPlayThrough() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  AbstractThread::AutoEnter context(AbstractMainThread());
  return CanPlayThroughImpl();
}

RefPtr<SetCDMPromise> MediaDecoder::SetCDMProxy(CDMProxy* aProxy) {
  MOZ_ASSERT(NS_IsMainThread());
  return InvokeAsync<RefPtr<CDMProxy>>(mReader->OwnerThread(), mReader.get(),
                                       __func__,
                                       &MediaFormatReader::SetCDMProxy, aProxy);
}

bool MediaDecoder::IsOpusEnabled() { return StaticPrefs::MediaOpusEnabled(); }

bool MediaDecoder::IsOggEnabled() { return StaticPrefs::MediaOggEnabled(); }

bool MediaDecoder::IsWaveEnabled() { return StaticPrefs::MediaWaveEnabled(); }

bool MediaDecoder::IsWebMEnabled() { return StaticPrefs::MediaWebMEnabled(); }

NS_IMETHODIMP
MediaMemoryTracker::CollectReports(nsIHandleReportCallback* aHandleReport,
                                   nsISupports* aData, bool aAnonymize) {
  // NB: When resourceSizes' ref count goes to 0 the promise will report the
  //     resources memory and finish the asynchronous memory report.
  RefPtr<MediaDecoder::ResourceSizes> resourceSizes =
      new MediaDecoder::ResourceSizes(MediaMemoryTracker::MallocSizeOf);

  nsCOMPtr<nsIHandleReportCallback> handleReport = aHandleReport;
  nsCOMPtr<nsISupports> data = aData;

  resourceSizes->Promise()->Then(
      // Don't use SystemGroup::AbstractMainThreadFor() for
      // handleReport->Callback() will run scripts.
      AbstractThread::MainThread(), __func__,
      [handleReport, data](size_t size) {
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
      [](size_t) { /* unused reject function */ });

  int64_t video = 0;
  int64_t audio = 0;
  DecodersArray& decoders = Decoders();
  for (size_t i = 0; i < decoders.Length(); ++i) {
    MediaDecoder* decoder = decoders[i];
    video += decoder->SizeOfVideoQueue();
    audio += decoder->SizeOfAudioQueue();
    decoder->AddSizeOfResources(resourceSizes);
  }

  MOZ_COLLECT_REPORT("explicit/media/decoded/video", KIND_HEAP, UNITS_BYTES,
                     video, "Memory used by decoded video frames.");

  MOZ_COLLECT_REPORT("explicit/media/decoded/audio", KIND_HEAP, UNITS_BYTES,
                     audio, "Memory used by decoded audio chunks.");

  return NS_OK;
}

MediaDecoderOwner* MediaDecoder::GetOwner() const {
  MOZ_ASSERT(NS_IsMainThread());
  // mOwner is valid until shutdown.
  return mOwner;
}

MediaDecoderOwner::NextFrameStatus MediaDecoder::NextFrameBufferedStatus() {
  MOZ_ASSERT(NS_IsMainThread());
  // Next frame hasn't been decoded yet.
  // Use the buffered range to consider if we have the next frame available.
  auto currentPosition = CurrentPosition();
  media::TimeInterval interval(
      currentPosition, currentPosition + DEFAULT_NEXT_FRAME_AVAILABLE_BUFFERED);
  return GetBuffered().Contains(interval)
             ? MediaDecoderOwner::NEXT_FRAME_AVAILABLE
             : MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE;
}

nsCString MediaDecoder::GetDebugInfo() {
  return nsPrintfCString(
      "MediaDecoder=%p: channels=%u rate=%u hasAudio=%d hasVideo=%d "
      "mPlayState=%s",
      this, mInfo ? mInfo->mAudio.mChannels : 0,
      mInfo ? mInfo->mAudio.mRate : 0, mInfo ? mInfo->HasAudio() : 0,
      mInfo ? mInfo->HasVideo() : 0, PlayStateStr());
}

RefPtr<GenericPromise> MediaDecoder::DumpDebugInfo() {
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
    return GenericPromise::CreateAndResolve(true, __func__);
  }

  return GetStateMachine()->RequestDebugInfo()->Then(
      SystemGroup::AbstractMainThreadFor(TaskCategory::Other), __func__,
      [str](const nsACString& aString) {
        DUMP("%s", str.get());
        DUMP("%s", aString.Data());
        return GenericPromise::CreateAndResolve(true, __func__);
      },
      [str]() {
        DUMP("%s", str.get());
        return GenericPromise::CreateAndResolve(true, __func__);
      });
}

RefPtr<MediaDecoder::DebugInfoPromise> MediaDecoder::RequestDebugInfo() {
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  auto str = GetDebugInfo();
  if (!GetStateMachine()) {
    return DebugInfoPromise::CreateAndResolve(str, __func__);
  }

  return GetStateMachine()->RequestDebugInfo()->Then(
      SystemGroup::AbstractMainThreadFor(TaskCategory::Other), __func__,
      [str](const nsACString& aString) {
        nsCString result = str + nsCString("\n") + aString;
        return DebugInfoPromise::CreateAndResolve(result, __func__);
      },
      [str]() { return DebugInfoPromise::CreateAndResolve(str, __func__); });
}

void MediaDecoder::GetMozDebugReaderData(nsACString& aString) {
  aString += nsPrintfCString("Container Type: %s\n",
                             ContainerType().Type().AsString().get());
  if (mReader) {
    mReader->GetMozDebugReaderData(aString);
  }
}

void MediaDecoder::NotifyAudibleStateChanged() {
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  GetOwner()->SetAudibleState(mIsAudioDataAudible);
}

MediaMemoryTracker::MediaMemoryTracker() {}

void MediaMemoryTracker::InitMemoryReporter() {
  RegisterWeakAsyncMemoryReporter(this);
}

MediaMemoryTracker::~MediaMemoryTracker() {
  UnregisterWeakMemoryReporter(this);
}

}  // namespace mozilla

// avoid redefined macro in unified build
#undef DUMP
#undef LOG
#undef NS_DispatchToMainThread
