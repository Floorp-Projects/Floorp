/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDecoder.h"

#include "AudioDeviceInfo.h"
#include "DOMMediaStream.h"
#include "DecoderBenchmark.h"
#include "ImageContainer.h"
#include "Layers.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "MediaResource.h"
#include "MediaShutdownManager.h"
#include "MediaTrackGraph.h"
#include "TelemetryProbesReporter.h"
#include "VideoFrameContainer.h"
#include "VideoUtils.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsIMemoryReporter.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"
#include <algorithm>
#include <limits>

using namespace mozilla::dom;
using namespace mozilla::layers;
using namespace mozilla::media;

namespace mozilla {

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
        AbstractThread::MainThread(), __func__,
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

void MediaDecoder::NotifyOwnerActivityChanged(bool aIsOwnerInvisible,
                                              bool aIsOwnerConnected) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  SetElementVisibility(aIsOwnerInvisible, aIsOwnerConnected);

  NotifyCompositor();
}

void MediaDecoder::Pause() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  LOG("Pause");
  if (mPlayState == PLAY_STATE_LOADING || IsEnded()) {
    mNextState = PLAY_STATE_PAUSED;
    return;
  }
  ChangeState(PLAY_STATE_PAUSED);
}

void MediaDecoder::SetVolume(double aVolume) {
  MOZ_ASSERT(NS_IsMainThread());
  mVolume = aVolume;
}

RefPtr<GenericPromise> MediaDecoder::SetSink(AudioDeviceInfo* aSinkDevice) {
  MOZ_ASSERT(NS_IsMainThread());
  mSinkDevice = aSinkDevice;
  return GetStateMachine()->InvokeSetSink(aSinkDevice);
}

void MediaDecoder::SetOutputCaptureState(OutputCaptureState aState,
                                         SharedDummyTrack* aDummyTrack) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDecoderStateMachine, "Must be called after Load().");
  MOZ_ASSERT_IF(aState == OutputCaptureState::Capture, aDummyTrack);
  mOutputCaptureState = aState;
  if (mOutputDummyTrack.Ref().get() != aDummyTrack) {
    mOutputDummyTrack = nsMainThreadPtrHandle<SharedDummyTrack>(
        MakeAndAddRef<nsMainThreadPtrHolder<SharedDummyTrack>>(
            "MediaDecoder::mOutputDummyTrack", aDummyTrack));
  }
}

void MediaDecoder::AddOutputTrack(RefPtr<ProcessedMediaTrack> aTrack) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDecoderStateMachine, "Must be called after Load().");
  CopyableTArray<RefPtr<ProcessedMediaTrack>> tracks = mOutputTracks;
  tracks.AppendElement(std::move(aTrack));
  mOutputTracks = tracks;
}

void MediaDecoder::RemoveOutputTrack(
    const RefPtr<ProcessedMediaTrack>& aTrack) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDecoderStateMachine, "Must be called after Load().");
  CopyableTArray<RefPtr<ProcessedMediaTrack>> tracks = mOutputTracks;
  if (tracks.RemoveElement(aTrack)) {
    mOutputTracks = tracks;
  }
}

void MediaDecoder::SetOutputTracksPrincipal(
    const RefPtr<nsIPrincipal>& aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDecoderStateMachine, "Must be called after Load().");
  mOutputPrincipal = MakePrincipalHandle(aPrincipal);
}

double MediaDecoder::GetDuration() {
  MOZ_ASSERT(NS_IsMainThread());
  return mDuration;
}

bool MediaDecoder::IsInfinite() const {
  MOZ_ASSERT(NS_IsMainThread());
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
      mDecoderBenchmark(new DecoderBenchmark()),
      mVideoFrameContainer(aInit.mOwner->GetVideoFrameContainer()),
      mMinimizePreroll(aInit.mMinimizePreroll),
      mFiredMetadataLoaded(false),
      mIsOwnerInvisible(false),
      mIsOwnerConnected(false),
      mForcedHidden(false),
      mHasSuspendTaint(aInit.mHasSuspendTaint),
      mPlaybackRate(aInit.mPlaybackRate),
      mLogicallySeeking(false, "MediaDecoder::mLogicallySeeking"),
      INIT_MIRROR(mBuffered, TimeIntervals()),
      INIT_MIRROR(mCurrentPosition, TimeUnit::Zero()),
      INIT_MIRROR(mStateMachineDuration, NullableTimeUnit()),
      INIT_MIRROR(mIsAudioDataAudible, false),
      INIT_CANONICAL(mVolume, aInit.mVolume),
      INIT_CANONICAL(mPreservesPitch, aInit.mPreservesPitch),
      INIT_CANONICAL(mLooping, aInit.mLooping),
      INIT_CANONICAL(mStreamName, aInit.mStreamName),
      INIT_CANONICAL(mSinkDevice, nullptr),
      INIT_CANONICAL(mSecondaryVideoContainer, nullptr),
      INIT_CANONICAL(mOutputCaptureState, OutputCaptureState::None),
      INIT_CANONICAL(mOutputDummyTrack, nullptr),
      INIT_CANONICAL(mOutputTracks, nsTArray<RefPtr<ProcessedMediaTrack>>()),
      INIT_CANONICAL(mOutputPrincipal, PRINCIPAL_HANDLE_NONE),
      INIT_CANONICAL(mPlayState, PLAY_STATE_LOADING),
      mSameOriginMedia(false),
      mVideoDecodingOberver(
          new BackgroundVideoDecodingPermissionObserver(this)),
      mIsBackgroundVideoDecodingAllowed(false),
      mTelemetryReported(false),
      mContainerType(aInit.mContainerType),
      mTelemetryProbesReporter(
          new TelemetryProbesReporter(aInit.mReporterOwner)) {
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
    mOnSecondaryVideoContainerInstalled.Disconnect();
    mOnStoreDecoderBenchmark.Disconnect();

    mDecoderStateMachine->BeginShutdown()->Then(
        mAbstractMainThread, __func__, this, &MediaDecoder::FinishShutdown,
        &MediaDecoder::FinishShutdown);
  } else {
    // Ensure we always unregister asynchronously in order not to disrupt
    // the hashtable iterating in MediaShutdownManager::Shutdown().
    RefPtr<MediaDecoder> self = this;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
        "MediaDecoder::Shutdown", [self]() { self->ShutdownInternal(); });
    mAbstractMainThread->Dispatch(r.forget());
  }

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
      GetOwner()->DispatchAsyncEvent(u"mozentervideosuspend"_ns);
      mTelemetryProbesReporter->OnDecodeSuspended();
      mIsVideoDecodingSuspended = true;
      break;
    case MediaPlaybackEvent::ExitVideoSuspend:
      GetOwner()->DispatchAsyncEvent(u"mozexitvideosuspend"_ns);
      mTelemetryProbesReporter->OnDecodeResumed();
      mIsVideoDecodingSuspended = false;
      break;
    case MediaPlaybackEvent::StartVideoSuspendTimer:
      GetOwner()->DispatchAsyncEvent(u"mozstartvideosuspendtimer"_ns);
      break;
    case MediaPlaybackEvent::CancelVideoSuspendTimer:
      GetOwner()->DispatchAsyncEvent(u"mozcancelvideosuspendtimer"_ns);
      break;
    case MediaPlaybackEvent::VideoOnlySeekBegin:
      GetOwner()->DispatchAsyncEvent(u"mozvideoonlyseekbegin"_ns);
      break;
    case MediaPlaybackEvent::VideoOnlySeekCompleted:
      GetOwner()->DispatchAsyncEvent(u"mozvideoonlyseekcompleted"_ns);
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

void MediaDecoder::OnSecondaryVideoContainerInstalled(
    const RefPtr<VideoFrameContainer>& aSecondaryVideoContainer) {
  MOZ_ASSERT(NS_IsMainThread());
  GetOwner()->OnSecondaryVideoContainerInstalled(aSecondaryVideoContainer);
}

void MediaDecoder::OnStoreDecoderBenchmark(const VideoInfo& aInfo) {
  MOZ_ASSERT(NS_IsMainThread());

  int32_t videoFrameRate = aInfo.GetFrameRate().ref();

  if (mFrameStats && videoFrameRate) {
    DecoderBenchmarkInfo benchmarkInfo{
        aInfo.mMimeType,
        aInfo.mDisplay.width,
        aInfo.mDisplay.height,
        videoFrameRate,
        BitDepthForColorDepth(aInfo.mColorDepth),
    };

    LOG("Store benchmark: Video width=%d, height=%d, frameRate=%d, content "
        "type = %s\n",
        benchmarkInfo.mWidth, benchmarkInfo.mHeight, benchmarkInfo.mFrameRate,
        benchmarkInfo.mContentType.BeginReading());

    mDecoderBenchmark->Store(benchmarkInfo, mFrameStats);
  }
}

void MediaDecoder::ShutdownInternal() {
  MOZ_ASSERT(NS_IsMainThread());
  mVideoFrameContainer = nullptr;
  mSecondaryVideoContainer = nullptr;
  MediaShutdownManager::Instance().Unregister(this);
}

void MediaDecoder::FinishShutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  SetStateMachine(nullptr);
  ShutdownInternal();
}

nsresult MediaDecoder::InitializeStateMachine() {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ASSERTION(mDecoderStateMachine, "Cannot initialize null state machine!");

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
  mOnSecondaryVideoContainerInstalled =
      mDecoderStateMachine->OnSecondaryVideoContainerInstalled().Connect(
          mAbstractMainThread, this,
          &MediaDecoder::OnSecondaryVideoContainerInstalled);
  mOnStoreDecoderBenchmark = mReader->OnStoreDecoderBenchmark().Connect(
      mAbstractMainThread, this, &MediaDecoder::OnStoreDecoderBenchmark);

  mOnEncrypted = mReader->OnEncrypted().Connect(
      mAbstractMainThread, GetOwner(), &MediaDecoderOwner::DispatchEncrypted);
  mOnWaitingForKey = mReader->OnWaitingForKey().Connect(
      mAbstractMainThread, GetOwner(), &MediaDecoderOwner::NotifyWaitingForKey);
  mOnDecodeWarning = mReader->OnDecodeWarning().Connect(
      mAbstractMainThread, GetOwner(), &MediaDecoderOwner::DecodeWarning);
}

void MediaDecoder::Play() {
  MOZ_ASSERT(NS_IsMainThread());

  NS_ASSERTION(mDecoderStateMachine != nullptr, "Should have state machine.");
  LOG("Play");
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

  MOZ_ASSERT(aTime >= 0.0, "Cannot seek to a negative value.");

  LOG("Seek");
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

void MediaDecoder::SetDelaySeekMode(bool aShouldDelaySeek) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("SetDelaySeekMode, shouldDelaySeek=%d", aShouldDelaySeek);
  if (mShouldDelaySeek == aShouldDelaySeek) {
    return;
  }
  mShouldDelaySeek = aShouldDelaySeek;
  if (!mShouldDelaySeek && mDelayedSeekTarget) {
    Seek(mDelayedSeekTarget->GetTime().ToSeconds(),
         mDelayedSeekTarget->GetType());
    mDelayedSeekTarget.reset();
  }
}

void MediaDecoder::DiscardOngoingSeekIfExists() {
  MOZ_ASSERT(NS_IsMainThread());
  mSeekRequest.DisconnectIfExists();
}

void MediaDecoder::CallSeek(const SeekTarget& aTarget) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mShouldDelaySeek) {
    LOG("Delay seek to %f and store it to delayed seek target",
        mDelayedSeekTarget->GetTime().ToSeconds());
    mDelayedSeekTarget = Some(aTarget);
    return;
  }
  DiscardOngoingSeekIfExists();
  mDecoderStateMachine->InvokeSeek(aTarget)
      ->Then(mAbstractMainThread, __func__, this, &MediaDecoder::OnSeekResolved,
             &MediaDecoder::OnSeekRejected)
      ->Track(mSeekRequest);
}

double MediaDecoder::GetCurrentTime() {
  MOZ_ASSERT(NS_IsMainThread());
  return mLogicalPosition;
}

void MediaDecoder::OnMetadataUpdate(TimedMetadata&& aMetadata) {
  MOZ_ASSERT(NS_IsMainThread());
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
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  LOG("MetadataLoaded, channels=%u rate=%u hasAudio=%d hasVideo=%d",
      aInfo->mAudio.mChannels, aInfo->mAudio.mRate, aInfo->HasAudio(),
      aInfo->HasVideo());

  mMediaSeekable = aInfo->mMediaSeekable;
  mMediaSeekableOnlyInBufferedRanges =
      aInfo->mMediaSeekableOnlyInBufferedRanges;
  mInfo = std::move(aInfo);

  // Make sure the element and the frame (if any) are told about
  // our new size.
  if (aEventVisibility != MediaDecoderEventVisibility::Suppressed) {
    mFiredMetadataLoaded = true;
    GetOwner()->MetadataLoaded(mInfo.get(), std::move(aTags));
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
  return ToPlayStateStr(mPlayState);
}

void MediaDecoder::FirstFrameLoaded(
    UniquePtr<MediaInfo> aInfo, MediaDecoderEventVisibility aEventVisibility) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  LOG("FirstFrameLoaded, channels=%u rate=%u hasAudio=%d hasVideo=%d "
      "mPlayState=%s transportSeekable=%d",
      aInfo->mAudio.mChannels, aInfo->mAudio.mRate, aInfo->HasAudio(),
      aInfo->HasVideo(), PlayStateStr(), IsTransportSeekable());

  mInfo = std::move(aInfo);

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
  GetOwner()->NotifyDecoderPrincipalChanged();
}

void MediaDecoder::OnSeekResolved() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  mLogicallySeeking = false;

  // Ensure logical position is updated after seek.
  UpdateLogicalPositionInternal();
  mSeekRequest.Complete();

  GetOwner()->SeekCompleted();
}

void MediaDecoder::OnSeekRejected() {
  MOZ_ASSERT(NS_IsMainThread());
  mSeekRequest.Complete();
  mLogicallySeeking = false;

  GetOwner()->SeekAborted();
}

void MediaDecoder::SeekingStarted() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  GetOwner()->SeekStarted();
}

void MediaDecoder::ChangeState(PlayState aState) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsShutdown(), "SHUTDOWN is the final state.");

  if (mNextState == aState) {
    mNextState = PLAY_STATE_PAUSED;
  }

  if (mPlayState != aState) {
    DDLOG(DDLogCategory::Property, "play_state", ToPlayStateStr(aState));
  }
  LOG("Play state changes from %s to %s", ToPlayStateStr(mPlayState),
      ToPlayStateStr(aState));
  mPlayState = aState;
  UpdateTelemetryHelperBasedOnPlayState(aState);
}

TelemetryProbesReporter::Visibility MediaDecoder::OwnerVisibility() const {
  return GetOwner()->IsActuallyInvisible() || mForcedHidden
             ? TelemetryProbesReporter::Visibility::eInvisible
             : TelemetryProbesReporter::Visibility::eVisible;
}

void MediaDecoder::UpdateTelemetryHelperBasedOnPlayState(
    PlayState aState) const {
  if (aState == PlayState::PLAY_STATE_PLAYING) {
    mTelemetryProbesReporter->OnPlay(OwnerVisibility());
  } else if (aState == PlayState::PLAY_STATE_PAUSED ||
             aState == PlayState::PLAY_STATE_ENDED) {
    mTelemetryProbesReporter->OnPause(OwnerVisibility());
  } else if (aState == PLAY_STATE_SHUTDOWN) {
    mTelemetryProbesReporter->OnShutdown();
  }
}

MediaDecoder::PositionUpdate MediaDecoder::GetPositionUpdateReason(
    double aPrevPos, double aCurPos) const {
  MOZ_ASSERT(NS_IsMainThread());
  // If current position is earlier than previous position and we didn't do
  // seek, that means we looped back to the start position, which currently
  // happens on audio only.
  const bool notSeeking = !mSeekRequest.Exists();
  if (mLooping && notSeeking && aCurPos < aPrevPos) {
    return PositionUpdate::eSeamlessLoopingSeeking;
  }
  return aPrevPos != aCurPos && notSeeking ? PositionUpdate::ePeriodicUpdate
                                           : PositionUpdate::eOther;
}

void MediaDecoder::UpdateLogicalPositionInternal() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

  double currentPosition = CurrentPosition().ToSeconds();
  if (mPlayState == PLAY_STATE_ENDED) {
    currentPosition = std::max(currentPosition, mDuration);
  }

  const PositionUpdate reason =
      GetPositionUpdateReason(mLogicalPosition, currentPosition);
  switch (reason) {
    case PositionUpdate::ePeriodicUpdate:
      SetLogicalPosition(currentPosition);
      // This is actually defined in `TimeMarchesOn`, but we do that in decoder.
      // https://html.spec.whatwg.org/multipage/media.html#playing-the-media-resource:event-media-timeupdate-7
      // TODO (bug 1688137): should we move it back to `TimeMarchesOn`?
      GetOwner()->MaybeQueueTimeupdateEvent();
      break;
    case PositionUpdate::eSeamlessLoopingSeeking:
      // When seamless seeking occurs, seeking was performed on the demuxer so
      // the decoder doesn't know. That means decoder still thinks it's in
      // playing. Therefore, we have to manually call those methods to notify
      // the owner about seeking.
      GetOwner()->SeekStarted();
      SetLogicalPosition(currentPosition);
      GetOwner()->SeekCompleted();
      break;
    default:
      MOZ_ASSERT(reason == PositionUpdate::eOther);
      SetLogicalPosition(currentPosition);
      break;
  }

  // Invalidate the frame so any video data is displayed.
  // Do this before the timeupdate event so that if that
  // event runs JavaScript that queries the media size, the
  // frame has reflowed and the size updated beforehand.
  Invalidate();
}

void MediaDecoder::SetLogicalPosition(double aNewPosition) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mLogicalPosition == aNewPosition) {
    return;
  }
  mLogicalPosition = aNewPosition;
  DDLOG(DDLogCategory::Property, "currentTime", mLogicalPosition);
}

void MediaDecoder::DurationChanged() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());

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
    GetOwner()->DispatchAsyncEvent(u"durationchange"_ns);
  }

  if (CurrentPosition() > TimeUnit::FromSeconds(mDuration)) {
    Seek(mDuration, SeekTarget::Accurate);
  }
}

already_AddRefed<KnowsCompositor> MediaDecoder::GetCompositor() {
  MediaDecoderOwner* owner = GetOwner();
  Document* ownerDoc = owner ? owner->GetDocument() : nullptr;
  WindowRenderer* renderer =
      ownerDoc ? nsContentUtils::WindowRendererForDocument(ownerDoc) : nullptr;
  RefPtr<KnowsCompositor> knows =
      renderer ? renderer->AsKnowsCompositor() : nullptr;
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

void MediaDecoder::SetElementVisibility(bool aIsOwnerInvisible,
                                        bool aIsOwnerConnected) {
  MOZ_ASSERT(NS_IsMainThread());
  mIsOwnerInvisible = aIsOwnerInvisible;
  mIsOwnerConnected = aIsOwnerConnected;
  mTelemetryProbesReporter->OnVisibilityChanged(OwnerVisibility());
  UpdateVideoDecodeMode();
}

void MediaDecoder::SetForcedHidden(bool aForcedHidden) {
  MOZ_ASSERT(NS_IsMainThread());
  mForcedHidden = aForcedHidden;
  mTelemetryProbesReporter->OnVisibilityChanged(OwnerVisibility());
  UpdateVideoDecodeMode();
}

void MediaDecoder::SetSuspendTaint(bool aTainted) {
  MOZ_ASSERT(NS_IsMainThread());
  mHasSuspendTaint = aTainted;
  UpdateVideoDecodeMode();
}

void MediaDecoder::UpdateVideoDecodeMode() {
  MOZ_ASSERT(NS_IsMainThread());

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

  // If mSecondaryVideoContainer is set, never suspend the video decoder.
  if (mSecondaryVideoContainer.Ref()) {
    LOG("UpdateVideoDecodeMode(), set Normal because the element is cloning "
        "itself visually to another video container.");
    mDecoderStateMachine->SetVideoDecodeMode(VideoDecodeMode::Normal);
    return;
  }

  // Don't suspend elements that is not in a connected tree.
  if (!mIsOwnerConnected) {
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

  if (mIsOwnerInvisible) {
    LOG("UpdateVideoDecodeMode(), set Suspend because of invisible element.");
    mDecoderStateMachine->SetVideoDecodeMode(VideoDecodeMode::Suspend);
  } else {
    LOG("UpdateVideoDecodeMode(), set Normal because of visible element.");
    mDecoderStateMachine->SetVideoDecodeMode(VideoDecodeMode::Normal);
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

void MediaDecoder::SetSecondaryVideoContainer(
    RefPtr<VideoFrameContainer> aSecondaryVideoContainer) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mSecondaryVideoContainer.Ref() == aSecondaryVideoContainer) {
    return;
  }
  mSecondaryVideoContainer = std::move(aSecondaryVideoContainer);
  UpdateVideoDecodeMode();
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
  mPreservesPitch = aPreservesPitch;
}

void MediaDecoder::SetLooping(bool aLooping) {
  MOZ_ASSERT(NS_IsMainThread());
  mLooping = aLooping;
}

void MediaDecoder::SetStreamName(const nsAutoString& aStreamName) {
  MOZ_ASSERT(NS_IsMainThread());
  mStreamName = aStreamName;
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

void MediaDecoder::Suspend() {
  MOZ_ASSERT(NS_IsMainThread());
  GetStateMachine()->InvokeSuspendMediaSink();
}

void MediaDecoder::Resume() {
  MOZ_ASSERT(NS_IsMainThread());
  GetStateMachine()->InvokeResumeMediaSink();
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

bool MediaDecoder::CanPlayThrough() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  return CanPlayThroughImpl();
}

RefPtr<SetCDMPromise> MediaDecoder::SetCDMProxy(CDMProxy* aProxy) {
  MOZ_ASSERT(NS_IsMainThread());
  return InvokeAsync<RefPtr<CDMProxy>>(mReader->OwnerThread(), mReader.get(),
                                       __func__,
                                       &MediaFormatReader::SetCDMProxy, aProxy);
}

bool MediaDecoder::IsOpusEnabled() { return StaticPrefs::media_opus_enabled(); }

bool MediaDecoder::IsOggEnabled() { return StaticPrefs::media_ogg_enabled(); }

bool MediaDecoder::IsWaveEnabled() { return StaticPrefs::media_wave_enabled(); }

bool MediaDecoder::IsWebMEnabled() { return StaticPrefs::media_webm_enabled(); }

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
      AbstractThread::MainThread(), __func__,
      [handleReport, data](size_t size) {
        handleReport->Callback(
            ""_ns, "explicit/media/resources"_ns, KIND_HEAP, UNITS_BYTES, size,
            nsLiteralCString("Memory used by media resources including "
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

void MediaDecoder::GetDebugInfo(dom::MediaDecoderDebugInfo& aInfo) {
  CopyUTF8toUTF16(nsPrintfCString("%p", this), aInfo.mInstance);
  aInfo.mChannels = mInfo ? mInfo->mAudio.mChannels : 0;
  aInfo.mRate = mInfo ? mInfo->mAudio.mRate : 0;
  aInfo.mHasAudio = mInfo ? mInfo->HasAudio() : false;
  aInfo.mHasVideo = mInfo ? mInfo->HasVideo() : false;
  CopyUTF8toUTF16(MakeStringSpan(PlayStateStr()), aInfo.mPlayState);
  aInfo.mContainerType =
      NS_ConvertUTF8toUTF16(ContainerType().Type().AsString());
  mReader->GetDebugInfo(aInfo.mReader);
}

RefPtr<GenericPromise> MediaDecoder::RequestDebugInfo(
    MediaDecoderDebugInfo& aInfo) {
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  GetDebugInfo(aInfo);

  if (!GetStateMachine()) {
    return GenericPromise::CreateAndResolve(true, __func__);
  }

  return GetStateMachine()
      ->RequestDebugInfo(aInfo.mStateMachine)
      ->Then(
          AbstractThread::MainThread(), __func__,
          []() { return GenericPromise::CreateAndResolve(true, __func__); },
          []() {
            MOZ_ASSERT_UNREACHABLE("Unexpected RequestDebugInfo() rejection");
            return GenericPromise::CreateAndResolve(false, __func__);
          });
}

void MediaDecoder::NotifyAudibleStateChanged() {
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  GetOwner()->SetAudibleState(mIsAudioDataAudible);
}

double MediaDecoder::GetTotalPlayTimeInSeconds() const {
  return mTelemetryProbesReporter->GetTotalPlayTimeInSeconds();
}

double MediaDecoder::GetVisibleVideoPlayTimeInSeconds() const {
  return mTelemetryProbesReporter->GetVisibleVideoPlayTimeInSeconds();
}

double MediaDecoder::GetInvisibleVideoPlayTimeInSeconds() const {
  return mTelemetryProbesReporter->GetInvisibleVideoPlayTimeInSeconds();
}

double MediaDecoder::GetVideoDecodeSuspendedTimeInSeconds() const {
  return mTelemetryProbesReporter->GetVideoDecodeSuspendedTimeInSeconds();
}

MediaMemoryTracker::MediaMemoryTracker() = default;

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
