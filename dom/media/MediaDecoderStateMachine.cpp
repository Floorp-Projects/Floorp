/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <stdint.h>
#include <utility>

#include "mediasink/AudioSink.h"
#include "mediasink/AudioSinkWrapper.h"
#include "mediasink/DecodedStream.h"
#include "mediasink/OutputStreamManager.h"
#include "mediasink/VideoSink.h"
#include "mozilla/Logging.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/NotNull.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/Tuple.h"
#include "nsIMemoryReporter.h"
#include "nsPrintfCString.h"
#include "nsTArray.h"
#include "DOMMediaStream.h"
#include "ImageContainer.h"
#include "MediaDecoder.h"
#include "MediaDecoderStateMachine.h"
#include "MediaShutdownManager.h"
#include "MediaTimer.h"
#include "ReaderProxy.h"
#include "TimeUnits.h"
#include "VideoUtils.h"

namespace mozilla {

using namespace mozilla::media;

#define NS_DispatchToMainThread(...) \
  CompileError_UseAbstractThreadDispatchInstead

// avoid redefined macro in unified build
#undef FMT
#undef LOG
#undef LOGV
#undef LOGW
#undef LOGE
#undef SFMT
#undef SLOG
#undef SLOGW
#undef SLOGE

#define FMT(x, ...) "Decoder=%p " x, mDecoderID, ##__VA_ARGS__
#define LOG(x, ...)                                                         \
  DDMOZ_LOG(gMediaDecoderLog, LogLevel::Debug, "Decoder=%p " x, mDecoderID, \
            ##__VA_ARGS__)
#define LOGV(x, ...)                                                          \
  DDMOZ_LOG(gMediaDecoderLog, LogLevel::Verbose, "Decoder=%p " x, mDecoderID, \
            ##__VA_ARGS__)
#define LOGW(x, ...) NS_WARNING(nsPrintfCString(FMT(x, ##__VA_ARGS__)).get())
#define LOGE(x, ...)                                                   \
  NS_DebugBreak(NS_DEBUG_WARNING,                                      \
                nsPrintfCString(FMT(x, ##__VA_ARGS__)).get(), nullptr, \
                __FILE__, __LINE__)

// Used by StateObject and its sub-classes
#define SFMT(x, ...)                                                     \
  "Decoder=%p state=%s " x, mMaster->mDecoderID, ToStateStr(GetState()), \
      ##__VA_ARGS__
#define SLOG(x, ...)                                                     \
  DDMOZ_LOGEX(mMaster, gMediaDecoderLog, LogLevel::Debug, "state=%s " x, \
              ToStateStr(GetState()), ##__VA_ARGS__)
#define SLOGW(x, ...) NS_WARNING(nsPrintfCString(SFMT(x, ##__VA_ARGS__)).get())
#define SLOGE(x, ...)                                                   \
  NS_DebugBreak(NS_DEBUG_WARNING,                                       \
                nsPrintfCString(SFMT(x, ##__VA_ARGS__)).get(), nullptr, \
                __FILE__, __LINE__)

// Certain constants get stored as member variables and then adjusted by various
// scale factors on a per-decoder basis. We want to make sure to avoid using
// these constants directly, so we put them in a namespace.
namespace detail {

// Resume a suspended video decoder to the current playback position plus this
// time premium for compensating the seeking delay.
static constexpr auto RESUME_VIDEO_PREMIUM = TimeUnit::FromMicroseconds(125000);

static const int64_t AMPLE_AUDIO_USECS = 2000000;

// If more than this much decoded audio is queued, we'll hold off
// decoding more audio.
static constexpr auto AMPLE_AUDIO_THRESHOLD =
    TimeUnit::FromMicroseconds(AMPLE_AUDIO_USECS);

}  // namespace detail

// If we have fewer than LOW_VIDEO_FRAMES decoded frames, and
// we're not "prerolling video", we'll skip the video up to the next keyframe
// which is at or after the current playback position.
static const uint32_t LOW_VIDEO_FRAMES = 2;

// Arbitrary "frame duration" when playing only audio.
static const int AUDIO_DURATION_USECS = 40000;

namespace detail {

// If we have less than this much buffered data available, we'll consider
// ourselves to be running low on buffered data. We determine how much
// buffered data we have remaining using the reader's GetBuffered()
// implementation.
static const int64_t LOW_BUFFER_THRESHOLD_USECS = 5000000;

static constexpr auto LOW_BUFFER_THRESHOLD =
    TimeUnit::FromMicroseconds(LOW_BUFFER_THRESHOLD_USECS);

// LOW_BUFFER_THRESHOLD_USECS needs to be greater than AMPLE_AUDIO_USECS,
// otherwise the skip-to-keyframe logic can activate when we're running low on
// data.
static_assert(LOW_BUFFER_THRESHOLD_USECS > AMPLE_AUDIO_USECS,
              "LOW_BUFFER_THRESHOLD_USECS is too small");

}  // namespace detail

// Amount of excess data to add in to the "should we buffer" calculation.
static constexpr auto EXHAUSTED_DATA_MARGIN =
    TimeUnit::FromMicroseconds(100000);

static const uint32_t MIN_VIDEO_QUEUE_SIZE = 3;
static const uint32_t MAX_VIDEO_QUEUE_SIZE = 10;
#ifdef MOZ_APPLEMEDIA
static const uint32_t HW_VIDEO_QUEUE_SIZE = 10;
#else
static const uint32_t HW_VIDEO_QUEUE_SIZE = 3;
#endif
static const uint32_t VIDEO_QUEUE_SEND_TO_COMPOSITOR_SIZE = 9999;

static uint32_t sVideoQueueDefaultSize = MAX_VIDEO_QUEUE_SIZE;
static uint32_t sVideoQueueHWAccelSize = HW_VIDEO_QUEUE_SIZE;
static uint32_t sVideoQueueSendToCompositorSize =
    VIDEO_QUEUE_SEND_TO_COMPOSITOR_SIZE;

static void InitVideoQueuePrefs() {
  MOZ_ASSERT(NS_IsMainThread());
  static bool sPrefInit = false;
  if (!sPrefInit) {
    sPrefInit = true;
    sVideoQueueDefaultSize = Preferences::GetUint(
        "media.video-queue.default-size", MAX_VIDEO_QUEUE_SIZE);
    sVideoQueueHWAccelSize = Preferences::GetUint(
        "media.video-queue.hw-accel-size", HW_VIDEO_QUEUE_SIZE);
    sVideoQueueSendToCompositorSize =
        Preferences::GetUint("media.video-queue.send-to-compositor-size",
                             VIDEO_QUEUE_SEND_TO_COMPOSITOR_SIZE);
  }
}

template <typename Type, typename Function>
static void DiscardFramesFromTail(MediaQueue<Type>& aQueue,
                                  const Function&& aTest) {
  while (aQueue.GetSize()) {
    if (aTest(aQueue.PeekBack()->mTime.ToMicroseconds())) {
      RefPtr<Type> releaseMe = aQueue.PopBack();
      continue;
    }
    break;
  }
}

// Delay, in milliseconds, that tabs needs to be in background before video
// decoding is suspended.
static TimeDuration SuspendBackgroundVideoDelay() {
  return TimeDuration::FromMilliseconds(
      StaticPrefs::MediaSuspendBkgndVideoDelayMs());
}

class MediaDecoderStateMachine::StateObject {
 public:
  virtual ~StateObject() {}
  virtual void Exit() {}  // Exit action.
  virtual void Step() {}  // Perform a 'cycle' of this state object.
  virtual State GetState() const = 0;

  // Event handlers for various events.
  virtual void HandleAudioCaptured() {}
  virtual void HandleAudioDecoded(AudioData* aAudio) {
    Crash("Unexpected event!", __func__);
  }
  virtual void HandleVideoDecoded(VideoData* aVideo, TimeStamp aDecodeStart) {
    Crash("Unexpected event!", __func__);
  }
  virtual void HandleAudioWaited(MediaData::Type aType) {
    Crash("Unexpected event!", __func__);
  }
  virtual void HandleVideoWaited(MediaData::Type aType) {
    Crash("Unexpected event!", __func__);
  }
  virtual void HandleWaitingForAudio() { Crash("Unexpected event!", __func__); }
  virtual void HandleAudioCanceled() { Crash("Unexpected event!", __func__); }
  virtual void HandleEndOfAudio() { Crash("Unexpected event!", __func__); }
  virtual void HandleWaitingForVideo() { Crash("Unexpected event!", __func__); }
  virtual void HandleVideoCanceled() { Crash("Unexpected event!", __func__); }
  virtual void HandleEndOfVideo() { Crash("Unexpected event!", __func__); }

  virtual RefPtr<MediaDecoder::SeekPromise> HandleSeek(SeekTarget aTarget);

  virtual RefPtr<ShutdownPromise> HandleShutdown();

  virtual void HandleVideoSuspendTimeout() = 0;

  virtual void HandleResumeVideoDecoding(const TimeUnit& aTarget);

  virtual void HandlePlayStateChanged(MediaDecoder::PlayState aPlayState) {}

  virtual nsCString GetDebugInfo() { return nsCString(); }

  virtual void HandleLoopingChanged() {}

 private:
  template <class S, typename R, typename... As>
  auto ReturnTypeHelper(R (S::*)(As...)) -> R;

  void Crash(const char* aReason, const char* aSite) {
    char buf[1024];
    SprintfLiteral(buf, "%s state=%s callsite=%s", aReason,
                   ToStateStr(GetState()), aSite);
    MOZ_ReportAssertionFailure(buf, __FILE__, __LINE__);
    MOZ_CRASH();
  }

 protected:
  enum class EventVisibility : int8_t { Observable, Suppressed };

  using Master = MediaDecoderStateMachine;
  explicit StateObject(Master* aPtr) : mMaster(aPtr) {}
  TaskQueue* OwnerThread() const { return mMaster->mTaskQueue; }
  ReaderProxy* Reader() const { return mMaster->mReader; }
  const MediaInfo& Info() const { return mMaster->Info(); }
  MediaQueue<AudioData>& AudioQueue() const { return mMaster->mAudioQueue; }
  MediaQueue<VideoData>& VideoQueue() const { return mMaster->mVideoQueue; }

  template <class S, typename... Args, size_t... Indexes>
  auto CallEnterMemberFunction(S* aS, Tuple<Args...>& aTuple,
                               std::index_sequence<Indexes...>)
      -> decltype(ReturnTypeHelper(&S::Enter)) {
    return aS->Enter(std::move(Get<Indexes>(aTuple))...);
  }

  // Note this function will delete the current state object.
  // Don't access members to avoid UAF after this call.
  template <class S, typename... Ts>
  auto SetState(Ts&&... aArgs) -> decltype(ReturnTypeHelper(&S::Enter)) {
    // |aArgs| must be passed by reference to avoid passing MOZ_NON_PARAM class
    // SeekJob by value.  See bug 1287006 and bug 1338374.  But we still *must*
    // copy the parameters, because |Exit()| can modify them.  See bug 1312321.
    // So we 1) pass the parameters by reference, but then 2) immediately copy
    // them into a Tuple to be safe against modification, and finally 3) move
    // the elements of the Tuple into the final function call.
    auto copiedArgs = MakeTuple(std::forward<Ts>(aArgs)...);

    // Copy mMaster which will reset to null.
    auto master = mMaster;

    auto* s = new S(master);

    MOZ_ASSERT(GetState() != s->GetState() ||
               GetState() == DECODER_STATE_SEEKING);

    SLOG("change state to: %s", ToStateStr(s->GetState()));

    Exit();

    // Delete the old state asynchronously to avoid UAF if the caller tries to
    // access its members after SetState() returns.
    master->OwnerThread()->DispatchDirectTask(
        NS_NewRunnableFunction("MDSM::StateObject::DeleteOldState",
                               [toDelete = std::move(master->mStateObj)]() {}));
    // Also reset mMaster to catch potentail UAF.
    mMaster = nullptr;

    master->mStateObj.reset(s);
    return CallEnterMemberFunction(s, copiedArgs,
                                   std::index_sequence_for<Ts...>{});
  }

  RefPtr<MediaDecoder::SeekPromise> SetSeekingState(
      SeekJob&& aSeekJob, EventVisibility aVisibility);

  void SetDecodingState();

  // Take a raw pointer in order not to change the life cycle of MDSM.
  // It is guaranteed to be valid by MDSM.
  Master* mMaster;
};

/**
 * Purpose: decode metadata like duration and dimensions of the media resource.
 *
 * Transition to other states when decoding metadata is done:
 *   SHUTDOWN if failing to decode metadata.
 *   DECODING_FIRSTFRAME otherwise.
 */
class MediaDecoderStateMachine::DecodeMetadataState
    : public MediaDecoderStateMachine::StateObject {
 public:
  explicit DecodeMetadataState(Master* aPtr) : StateObject(aPtr) {}

  void Enter() {
    MOZ_ASSERT(!mMaster->mVideoDecodeSuspended);
    MOZ_ASSERT(!mMetadataRequest.Exists());
    SLOG("Dispatching AsyncReadMetadata");

    // We disconnect mMetadataRequest in Exit() so it is fine to capture
    // a raw pointer here.
    Reader()
        ->ReadMetadata()
        ->Then(
            OwnerThread(), __func__,
            [this](MetadataHolder&& aMetadata) {
              OnMetadataRead(std::move(aMetadata));
            },
            [this](const MediaResult& aError) { OnMetadataNotRead(aError); })
        ->Track(mMetadataRequest);
  }

  void Exit() override { mMetadataRequest.DisconnectIfExists(); }

  State GetState() const override { return DECODER_STATE_DECODING_METADATA; }

  RefPtr<MediaDecoder::SeekPromise> HandleSeek(SeekTarget aTarget) override {
    MOZ_DIAGNOSTIC_ASSERT(false, "Can't seek while decoding metadata.");
    return MediaDecoder::SeekPromise::CreateAndReject(true, __func__);
  }

  void HandleVideoSuspendTimeout() override {
    // Do nothing since no decoders are created yet.
  }

  void HandleResumeVideoDecoding(const TimeUnit&) override {
    // We never suspend video decoding in this state.
    MOZ_ASSERT(false, "Shouldn't have suspended video decoding.");
  }

 private:
  void OnMetadataRead(MetadataHolder&& aMetadata);

  void OnMetadataNotRead(const MediaResult& aError) {
    mMetadataRequest.Complete();
    SLOGE("Decode metadata failed, shutting down decoder");
    mMaster->DecodeError(aError);
  }

  MozPromiseRequestHolder<MediaFormatReader::MetadataPromise> mMetadataRequest;
};

/**
 * Purpose: release decoder resources to save memory and hardware resources.
 *
 * Transition to:
 *   SEEKING if any seek request or play state changes to PLAYING.
 */
class MediaDecoderStateMachine::DormantState
    : public MediaDecoderStateMachine::StateObject {
 public:
  explicit DormantState(Master* aPtr) : StateObject(aPtr) {}

  void Enter() {
    if (mMaster->IsPlaying()) {
      mMaster->StopPlayback();
    }

    // Calculate the position to seek to when exiting dormant.
    auto t = mMaster->mMediaSink->IsStarted() ? mMaster->GetClock()
                                              : mMaster->GetMediaTime();
    mMaster->AdjustByLooping(t);
    mPendingSeek.mTarget.emplace(t, SeekTarget::Accurate);
    // SeekJob asserts |mTarget.IsValid() == !mPromise.IsEmpty()| so we
    // need to create the promise even it is not used at all.
    // The promise may be used when coming out of DormantState into
    // SeekingState.
    RefPtr<MediaDecoder::SeekPromise> x =
        mPendingSeek.mPromise.Ensure(__func__);

    // No need to call ResetDecode() and StopMediaSink() here.
    // We will do them during seeking when exiting dormant.

    // Ignore WAIT_FOR_DATA since we won't decode in dormant.
    mMaster->mAudioWaitRequest.DisconnectIfExists();
    mMaster->mVideoWaitRequest.DisconnectIfExists();

    MaybeReleaseResources();
  }

  void Exit() override {
    // mPendingSeek is either moved when exiting dormant or
    // should be rejected here before transition to SHUTDOWN.
    mPendingSeek.RejectIfExists(__func__);
  }

  State GetState() const override { return DECODER_STATE_DORMANT; }

  RefPtr<MediaDecoder::SeekPromise> HandleSeek(SeekTarget aTarget) override;

  void HandleVideoSuspendTimeout() override {
    // Do nothing since we've released decoders in Enter().
  }

  void HandleResumeVideoDecoding(const TimeUnit&) override {
    // Do nothing since we won't resume decoding until exiting dormant.
  }

  void HandlePlayStateChanged(MediaDecoder::PlayState aPlayState) override;

  void HandleAudioDecoded(AudioData*) override { MaybeReleaseResources(); }
  void HandleVideoDecoded(VideoData*, TimeStamp) override {
    MaybeReleaseResources();
  }
  void HandleWaitingForAudio() override { MaybeReleaseResources(); }
  void HandleWaitingForVideo() override { MaybeReleaseResources(); }
  void HandleAudioCanceled() override { MaybeReleaseResources(); }
  void HandleVideoCanceled() override { MaybeReleaseResources(); }
  void HandleEndOfAudio() override { MaybeReleaseResources(); }
  void HandleEndOfVideo() override { MaybeReleaseResources(); }

 private:
  void MaybeReleaseResources() {
    if (!mMaster->mAudioDataRequest.Exists() &&
        !mMaster->mVideoDataRequest.Exists()) {
      // Release decoders only when they are idle. Otherwise it might cause
      // decode error later when resetting decoders during seeking.
      mMaster->mReader->ReleaseResources();
    }
  }

  SeekJob mPendingSeek;
};

/**
 * Purpose: decode the 1st audio and video frames to fire the 'loadeddata'
 * event.
 *
 * Transition to:
 *   SHUTDOWN if any decode error.
 *   SEEKING if any seek request.
 *   DECODING/LOOPING_DECODING when the 'loadeddata' event is fired.
 */
class MediaDecoderStateMachine::DecodingFirstFrameState
    : public MediaDecoderStateMachine::StateObject {
 public:
  explicit DecodingFirstFrameState(Master* aPtr) : StateObject(aPtr) {}

  void Enter();

  void Exit() override {
    // mPendingSeek is either moved in MaybeFinishDecodeFirstFrame()
    // or should be rejected here before transition to SHUTDOWN.
    mPendingSeek.RejectIfExists(__func__);
  }

  State GetState() const override { return DECODER_STATE_DECODING_FIRSTFRAME; }

  void HandleAudioDecoded(AudioData* aAudio) override {
    mMaster->PushAudio(aAudio);
    MaybeFinishDecodeFirstFrame();
  }

  void HandleVideoDecoded(VideoData* aVideo, TimeStamp aDecodeStart) override {
    mMaster->PushVideo(aVideo);
    MaybeFinishDecodeFirstFrame();
  }

  void HandleWaitingForAudio() override {
    mMaster->WaitForData(MediaData::Type::AUDIO_DATA);
  }

  void HandleAudioCanceled() override { mMaster->RequestAudioData(); }

  void HandleEndOfAudio() override {
    AudioQueue().Finish();
    MaybeFinishDecodeFirstFrame();
  }

  void HandleWaitingForVideo() override {
    mMaster->WaitForData(MediaData::Type::VIDEO_DATA);
  }

  void HandleVideoCanceled() override {
    mMaster->RequestVideoData(media::TimeUnit());
  }

  void HandleEndOfVideo() override {
    VideoQueue().Finish();
    MaybeFinishDecodeFirstFrame();
  }

  void HandleAudioWaited(MediaData::Type aType) override {
    mMaster->RequestAudioData();
  }

  void HandleVideoWaited(MediaData::Type aType) override {
    mMaster->RequestVideoData(media::TimeUnit());
  }

  void HandleVideoSuspendTimeout() override {
    // Do nothing for we need to decode the 1st video frame to get the
    // dimensions.
  }

  void HandleResumeVideoDecoding(const TimeUnit&) override {
    // We never suspend video decoding in this state.
    MOZ_ASSERT(false, "Shouldn't have suspended video decoding.");
  }

  RefPtr<MediaDecoder::SeekPromise> HandleSeek(SeekTarget aTarget) override {
    if (mMaster->mIsMSE) {
      return StateObject::HandleSeek(aTarget);
    }
    // Delay seek request until decoding first frames for non-MSE media.
    SLOG("Not Enough Data to seek at this stage, queuing seek");
    mPendingSeek.RejectIfExists(__func__);
    mPendingSeek.mTarget.emplace(aTarget);
    return mPendingSeek.mPromise.Ensure(__func__);
  }

 private:
  // Notify FirstFrameLoaded if having decoded first frames and
  // transition to SEEKING if there is any pending seek, or DECODING otherwise.
  void MaybeFinishDecodeFirstFrame();

  SeekJob mPendingSeek;
};

/**
 * Purpose: decode audio/video data for playback.
 *
 * Transition to:
 *   DORMANT if playback is paused for a while.
 *   SEEKING if any seek request.
 *   SHUTDOWN if any decode error.
 *   BUFFERING if playback can't continue due to lack of decoded data.
 *   COMPLETED when having decoded all audio/video data.
 *   LOOPING_DECODING when media start seamless looping
 */
class MediaDecoderStateMachine::DecodingState
    : public MediaDecoderStateMachine::StateObject {
 public:
  explicit DecodingState(Master* aPtr)
      : StateObject(aPtr), mDormantTimer(OwnerThread()) {}

  void Enter();

  void Exit() override {
    if (!mDecodeStartTime.IsNull()) {
      TimeDuration decodeDuration = TimeStamp::Now() - mDecodeStartTime;
      SLOG("Exiting DECODING, decoded for %.3lfs", decodeDuration.ToSeconds());
    }
    mDormantTimer.Reset();
    mOnAudioPopped.DisconnectIfExists();
    mOnVideoPopped.DisconnectIfExists();
  }

  void Step() override;

  State GetState() const override { return DECODER_STATE_DECODING; }

  void HandleAudioDecoded(AudioData* aAudio) override {
    mMaster->PushAudio(aAudio);
    DispatchDecodeTasksIfNeeded();
    MaybeStopPrerolling();
  }

  void HandleVideoDecoded(VideoData* aVideo, TimeStamp aDecodeStart) override {
    mMaster->PushVideo(aVideo);
    DispatchDecodeTasksIfNeeded();
    MaybeStopPrerolling();
  }

  void HandleAudioCanceled() override { mMaster->RequestAudioData(); }

  void HandleVideoCanceled() override {
    mMaster->RequestVideoData(mMaster->GetMediaTime());
  }

  void HandleEndOfAudio() override;
  void HandleEndOfVideo() override;

  void HandleWaitingForAudio() override {
    mMaster->WaitForData(MediaData::Type::AUDIO_DATA);
    MaybeStopPrerolling();
  }

  void HandleWaitingForVideo() override {
    mMaster->WaitForData(MediaData::Type::VIDEO_DATA);
    MaybeStopPrerolling();
  }

  void HandleAudioWaited(MediaData::Type aType) override {
    mMaster->RequestAudioData();
  }

  void HandleVideoWaited(MediaData::Type aType) override {
    mMaster->RequestVideoData(mMaster->GetMediaTime());
  }

  void HandleAudioCaptured() override {
    MaybeStopPrerolling();
    // MediaSink is changed. Schedule Step() to check if we can start playback.
    mMaster->ScheduleStateMachine();
  }

  void HandleVideoSuspendTimeout() override {
    // No video, so nothing to suspend.
    if (!mMaster->HasVideo()) {
      return;
    }

    mMaster->mVideoDecodeSuspended = true;
    mMaster->mOnPlaybackEvent.Notify(MediaPlaybackEvent::EnterVideoSuspend);
    Reader()->SetVideoBlankDecode(true);
  }

  void HandlePlayStateChanged(MediaDecoder::PlayState aPlayState) override {
    if (aPlayState == MediaDecoder::PLAY_STATE_PLAYING) {
      // Schedule Step() to check if we can start playback.
      mMaster->ScheduleStateMachine();
      // Try to dispatch decoding tasks for mMinimizePreroll might be reset.
      DispatchDecodeTasksIfNeeded();
    }

    if (aPlayState == MediaDecoder::PLAY_STATE_PAUSED) {
      StartDormantTimer();
    } else {
      mDormantTimer.Reset();
    }
  }

  nsCString GetDebugInfo() override {
    return nsPrintfCString("mIsPrerolling=%d", mIsPrerolling);
  }

  void HandleLoopingChanged() override { SetDecodingState(); }

 protected:
  virtual void EnsureAudioDecodeTaskQueued();

 private:
  void DispatchDecodeTasksIfNeeded();
  void EnsureVideoDecodeTaskQueued();
  void MaybeStartBuffering();

  // At the start of decoding we want to "preroll" the decode until we've
  // got a few frames decoded before we consider whether decode is falling
  // behind. Otherwise our "we're falling behind" logic will trigger
  // unnecessarily if we start playing as soon as the first sample is
  // decoded. These two fields store how many video frames and audio
  // samples we must consume before are considered to be finished prerolling.
  TimeUnit AudioPrerollThreshold() const {
    return mMaster->mAmpleAudioThreshold / 2;
  }

  uint32_t VideoPrerollFrames() const {
    return mMaster->GetAmpleVideoFrames() / 2;
  }

  bool DonePrerollingAudio() {
    return !mMaster->IsAudioDecoding() ||
           mMaster->GetDecodedAudioDuration() >=
               AudioPrerollThreshold().MultDouble(mMaster->mPlaybackRate);
  }

  bool DonePrerollingVideo() {
    return !mMaster->IsVideoDecoding() ||
           static_cast<uint32_t>(mMaster->VideoQueue().GetSize()) >=
               VideoPrerollFrames() * mMaster->mPlaybackRate + 1;
  }

  void MaybeStopPrerolling() {
    if (mIsPrerolling &&
        (DonePrerollingAudio() || mMaster->IsWaitingAudioData()) &&
        (DonePrerollingVideo() || mMaster->IsWaitingVideoData())) {
      mIsPrerolling = false;
      // Check if we can start playback.
      mMaster->ScheduleStateMachine();
    }
  }

  void StartDormantTimer() {
    if (!mMaster->mMediaSeekable) {
      // Don't enter dormant if the media is not seekable because we need to
      // seek when exiting dormant.
      return;
    }

    auto timeout = StaticPrefs::MediaDormantOnPauseTimeoutMs();
    if (timeout < 0) {
      // Disabled when timeout is negative.
      return;
    } else if (timeout == 0) {
      // Enter dormant immediately without scheduling a timer.
      SetState<DormantState>();
      return;
    }

    if (mMaster->mMinimizePreroll) {
      SetState<DormantState>();
      return;
    }

    TimeStamp target =
        TimeStamp::Now() + TimeDuration::FromMilliseconds(timeout);

    mDormantTimer.Ensure(
        target,
        [this]() {
          mDormantTimer.CompleteRequest();
          SetState<DormantState>();
        },
        [this]() { mDormantTimer.CompleteRequest(); });
  }

  // Time at which we started decoding.
  TimeStamp mDecodeStartTime;

  // When we start decoding (either for the first time, or after a pause)
  // we may be low on decoded data. We don't want our "low data" logic to
  // kick in and decide that we're low on decoded data because the download
  // can't keep up with the decode, and cause us to pause playback. So we
  // have a "preroll" stage, where we ignore the results of our "low data"
  // logic during the first few frames of our decode. This occurs during
  // playback.
  bool mIsPrerolling = true;

  // Fired when playback is paused for a while to enter dormant.
  DelayedScheduler mDormantTimer;

  MediaEventListener mOnAudioPopped;
  MediaEventListener mOnVideoPopped;
};

/**
 * Purpose: decode audio/video data for playback when media is in seamless
 * looping, we will adjust media time to make samples time monotonically
 * increasing.
 *
 * Transition to:
 *   DORMANT if playback is paused for a while.
 *   SEEKING if any seek request.
 *   SHUTDOWN if any decode error.
 *   BUFFERING if playback can't continue due to lack of decoded data.
 *   COMPLETED when having decoded all audio/video data.
 *   DECODING when media stop seamless looping
 */
class MediaDecoderStateMachine::LoopingDecodingState
    : public MediaDecoderStateMachine::DecodingState {
 public:
  explicit LoopingDecodingState(Master* aPtr)
      : DecodingState(aPtr), mIsReachingAudioEOS(!mMaster->IsAudioDecoding()) {
    MOZ_ASSERT(mMaster->mLooping);
  }

  void Enter() {
    if (mIsReachingAudioEOS) {
      SLOG("audio has ended, request the data again.");
      UpdatePlaybackPositionToZeroIfNeeded();
      RequestAudioDataFromStartPosition();
    }
    DecodingState::Enter();
  }

  void Exit() override {
    if (ShouldDiscardLoopedAudioData()) {
      mMaster->mAudioDataRequest.DisconnectIfExists();
      DiscardLoopedAudioData();
    }
    if (HasDecodedLastAudioFrame()) {
      AudioQueue().Finish();
    }
    mAudioDataRequest.DisconnectIfExists();
    mAudioSeekRequest.DisconnectIfExists();
    DecodingState::Exit();
  }

  State GetState() const override { return DECODER_STATE_LOOPING_DECODING; }

  void HandleAudioDecoded(AudioData* aAudio) override {
    MediaResult rv = LoopingAudioTimeAdjustment(aAudio);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mMaster->DecodeError(rv);
      return;
    }
    mMaster->mDecodedAudioEndTime =
        std::max(aAudio->GetEndTime(), mMaster->mDecodedAudioEndTime);
    SLOG("sample after time-adjustment [%" PRId64 ",%" PRId64 "]",
         aAudio->mTime.ToMicroseconds(), aAudio->GetEndTime().ToMicroseconds());
    DecodingState::HandleAudioDecoded(aAudio);
  }

  void HandleEndOfAudio() override {
    mIsReachingAudioEOS = true;
    // The data time in the audio queue is assumed to be increased linearly,
    // so we need to add the last ending time as the offset to correct the
    // audio data time in the next round when seamless looping is enabled.
    mAudioLoopingOffset = mMaster->mDecodedAudioEndTime;

    if (mMaster->mAudioDecodedDuration.isNothing()) {
      mMaster->mAudioDecodedDuration.emplace(mMaster->mDecodedAudioEndTime);
    }

    SLOG(
        "received EOS when seamless looping, starts seeking, "
        "AudioLoopingOffset=[%" PRId64 "]",
        mAudioLoopingOffset.ToMicroseconds());
    RequestAudioDataFromStartPosition();
  }

 private:
  void RequestAudioDataFromStartPosition() {
    Reader()->ResetDecode(TrackInfo::kAudioTrack);
    Reader()
        ->Seek(SeekTarget(media::TimeUnit::Zero(), SeekTarget::Accurate))
        ->Then(
            OwnerThread(), __func__,
            [this]() -> void {
              mAudioSeekRequest.Complete();
              SLOG(
                  "seeking completed, start to request first sample, "
                  "queueing audio task - queued=%zu, decoder-queued=%zu",
                  AudioQueue().GetSize(), Reader()->SizeOfAudioQueueInFrames());

              Reader()
                  ->RequestAudioData()
                  ->Then(
                      OwnerThread(), __func__,
                      [this](RefPtr<AudioData> aAudio) {
                        mIsReachingAudioEOS = false;
                        mAudioDataRequest.Complete();
                        SLOG(
                            "got audio decoded sample "
                            "[%" PRId64 ",%" PRId64 "]",
                            aAudio->mTime.ToMicroseconds(),
                            aAudio->GetEndTime().ToMicroseconds());
                        HandleAudioDecoded(aAudio);
                      },
                      [this](const MediaResult& aError) {
                        mAudioDataRequest.Complete();
                        HandleError(aError);
                      })
                  ->Track(mAudioDataRequest);
            },
            [this](const SeekRejectValue& aReject) -> void {
              mAudioSeekRequest.Complete();
              HandleError(aReject.mError);
            })
        ->Track(mAudioSeekRequest);
  }

  void UpdatePlaybackPositionToZeroIfNeeded() {
    MOZ_ASSERT(mIsReachingAudioEOS);
    MOZ_ASSERT(mAudioLoopingOffset == media::TimeUnit::Zero());
    // If we have already reached EOS before starting media sink, the sink
    // has not started yet and the current position is larger than last decoded
    // end time, that means we directly seeked to EOS and playback would start
    // from the start position soon. Therefore, we should reset the position to
    // 0s so that when media sink starts we can make it start from 0s, not from
    // EOS position which would result in wrong estimation of decoded audio
    // duration because decoded data's time which can't be adjusted as offset is
    // zero would be always less than media sink time.
    if (!mMaster->mMediaSink->IsStarted() &&
        mMaster->mCurrentPosition.Ref() > mMaster->mDecodedAudioEndTime) {
      mMaster->UpdatePlaybackPositionInternal(TimeUnit::Zero());
    }
  }

  void HandleError(const MediaResult& aError);

  void EnsureAudioDecodeTaskQueued() override {
    if (mAudioSeekRequest.Exists() || mAudioDataRequest.Exists()) {
      return;
    }
    DecodingState::EnsureAudioDecodeTaskQueued();
  }

  MediaResult LoopingAudioTimeAdjustment(AudioData* aAudio) {
    if (mAudioLoopingOffset != media::TimeUnit::Zero()) {
      aAudio->mTime += mAudioLoopingOffset;
    }
    return aAudio->mTime.IsValid()
               ? MediaResult(NS_OK)
               : MediaResult(
                     NS_ERROR_DOM_MEDIA_OVERFLOW_ERR,
                     "Audio sample overflow during looping time adjustment");
  }

  bool ShouldDiscardLoopedAudioData() const {
    if (!mMaster->mMediaSink->IsStarted()) {
      return false;
    }
    /**
     * If media cancels looping, we should check whether there are audio data
     * whose time is later than EOS. If so, we should discard them because we
     * won't have a chance to play them.
     *
     *    playback                     last decoded
     *    position          EOS        data time
     *   ----|---------------|------------|---------> (Increasing timeline)
     *    mCurrent        mLooping      mMaster's
     *    ClockTime        Offset      mDecodedAudioEndTime
     *
     */
    return (mAudioLoopingOffset != media::TimeUnit::Zero() &&
            mMaster->GetClock() < mAudioLoopingOffset &&
            mAudioLoopingOffset < mMaster->mDecodedAudioEndTime);
  }

  void DiscardLoopedAudioData() {
    if (mAudioLoopingOffset == media::TimeUnit::Zero()) {
      return;
    }

    SLOG("Discard frames after the time=%" PRId64,
         mAudioLoopingOffset.ToMicroseconds());
    DiscardFramesFromTail(AudioQueue(), [&](int64_t aSampleTime) {
      return aSampleTime > mAudioLoopingOffset.ToMicroseconds();
    });
  }

  bool HasDecodedLastAudioFrame() const {
    // when we're going to leave looping state and have got EOS before, we
    // should mark audio queue as ended because we have got all data we need.
    return mAudioDataRequest.Exists() || mAudioSeekRequest.Exists() ||
           ShouldDiscardLoopedAudioData();
  }

  bool mIsReachingAudioEOS;
  media::TimeUnit mAudioLoopingOffset = media::TimeUnit::Zero();
  MozPromiseRequestHolder<MediaFormatReader::SeekPromise> mAudioSeekRequest;
  MozPromiseRequestHolder<AudioDataPromise> mAudioDataRequest;
};

/**
 * Purpose: seek to a particular new playback position.
 *
 * Transition to:
 *   SEEKING if any new seek request.
 *   SHUTDOWN if seek failed.
 *   COMPLETED if the new playback position is the end of the media resource.
 *   NextFrameSeekingState if completing a NextFrameSeekingFromDormantState.
 *   DECODING/LOOPING_DECODING otherwise.
 */
class MediaDecoderStateMachine::SeekingState
    : public MediaDecoderStateMachine::StateObject {
 public:
  explicit SeekingState(Master* aPtr)
      : StateObject(aPtr), mVisibility(static_cast<EventVisibility>(0)) {}

  RefPtr<MediaDecoder::SeekPromise> Enter(SeekJob&& aSeekJob,
                                          EventVisibility aVisibility) {
    mSeekJob = std::move(aSeekJob);
    mVisibility = aVisibility;

    // Suppressed visibility comes from two cases: (1) leaving dormant state,
    // and (2) resuming suspended video decoder. We want both cases to be
    // transparent to the user. So we only notify the change when the seek
    // request is from the user.
    if (mVisibility == EventVisibility::Observable) {
      // Don't stop playback for a video-only seek since we want to keep playing
      // audio and we don't need to stop playback while leaving dormant for the
      // playback should has been stopped.
      mMaster->StopPlayback();
      mMaster->UpdatePlaybackPositionInternal(mSeekJob.mTarget->GetTime());
      mMaster->mOnPlaybackEvent.Notify(MediaPlaybackEvent::SeekStarted);
      mMaster->mOnNextFrameStatus.Notify(
          MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_SEEKING);
    }

    RefPtr<MediaDecoder::SeekPromise> p = mSeekJob.mPromise.Ensure(__func__);

    DoSeek();

    return p;
  }

  virtual void Exit() override = 0;

  State GetState() const override { return DECODER_STATE_SEEKING; }

  void HandleAudioDecoded(AudioData* aAudio) override = 0;
  void HandleVideoDecoded(VideoData* aVideo,
                          TimeStamp aDecodeStart) override = 0;
  void HandleAudioWaited(MediaData::Type aType) override = 0;
  void HandleVideoWaited(MediaData::Type aType) override = 0;

  void HandleVideoSuspendTimeout() override {
    // Do nothing since we want a valid video frame to show when seek is done.
  }

  void HandleResumeVideoDecoding(const TimeUnit&) override {
    // Do nothing. We will resume video decoding in the decoding state.
  }

  // We specially handle next frame seeks by ignoring them if we're already
  // seeking.
  RefPtr<MediaDecoder::SeekPromise> HandleSeek(SeekTarget aTarget) override {
    if (aTarget.IsNextFrame()) {
      // We ignore next frame seeks if we already have a seek pending
      SLOG("Already SEEKING, ignoring seekToNextFrame");
      MOZ_ASSERT(!mSeekJob.mPromise.IsEmpty(), "Seek shouldn't be finished");
      return MediaDecoder::SeekPromise::CreateAndReject(/* aIgnored = */ true,
                                                        __func__);
    }

    return StateObject::HandleSeek(aTarget);
  }

 protected:
  SeekJob mSeekJob;
  EventVisibility mVisibility;

  virtual void DoSeek() = 0;
  // Transition to the next state (defined by the subclass) when seek is
  // completed.
  virtual void GoToNextState() { SetDecodingState(); }
  void SeekCompleted();
  virtual TimeUnit CalculateNewCurrentTime() const = 0;
};

class MediaDecoderStateMachine::AccurateSeekingState
    : public MediaDecoderStateMachine::SeekingState {
 public:
  explicit AccurateSeekingState(Master* aPtr) : SeekingState(aPtr) {}

  RefPtr<MediaDecoder::SeekPromise> Enter(SeekJob&& aSeekJob,
                                          EventVisibility aVisibility) {
    MOZ_ASSERT(aSeekJob.mTarget->IsAccurate() || aSeekJob.mTarget->IsFast());
    mCurrentTimeBeforeSeek = mMaster->GetMediaTime();
    return SeekingState::Enter(std::move(aSeekJob), aVisibility);
  }

  void Exit() override {
    // Disconnect MediaDecoder.
    mSeekJob.RejectIfExists(__func__);

    // Disconnect ReaderProxy.
    mSeekRequest.DisconnectIfExists();

    mWaitRequest.DisconnectIfExists();
  }

  void HandleAudioDecoded(AudioData* aAudio) override {
    MOZ_ASSERT(!mDoneAudioSeeking || !mDoneVideoSeeking,
               "Seek shouldn't be finished");
    MOZ_ASSERT(aAudio);

    AdjustFastSeekIfNeeded(aAudio);

    if (mSeekJob.mTarget->IsFast()) {
      // Non-precise seek; we can stop the seek at the first sample.
      mMaster->PushAudio(aAudio);
      mDoneAudioSeeking = true;
    } else {
      nsresult rv = DropAudioUpToSeekTarget(aAudio);
      if (NS_FAILED(rv)) {
        mMaster->DecodeError(rv);
        return;
      }
    }

    if (!mDoneAudioSeeking) {
      RequestAudioData();
      return;
    }
    MaybeFinishSeek();
  }

  void HandleVideoDecoded(VideoData* aVideo, TimeStamp aDecodeStart) override {
    MOZ_ASSERT(!mDoneAudioSeeking || !mDoneVideoSeeking,
               "Seek shouldn't be finished");
    MOZ_ASSERT(aVideo);

    AdjustFastSeekIfNeeded(aVideo);

    if (mSeekJob.mTarget->IsFast()) {
      // Non-precise seek. We can stop the seek at the first sample.
      mMaster->PushVideo(aVideo);
      mDoneVideoSeeking = true;
    } else {
      nsresult rv = DropVideoUpToSeekTarget(aVideo);
      if (NS_FAILED(rv)) {
        mMaster->DecodeError(rv);
        return;
      }
    }

    if (!mDoneVideoSeeking) {
      RequestVideoData();
      return;
    }
    MaybeFinishSeek();
  }

  void HandleWaitingForAudio() override {
    MOZ_ASSERT(!mDoneAudioSeeking);
    mMaster->WaitForData(MediaData::Type::AUDIO_DATA);
  }

  void HandleAudioCanceled() override {
    MOZ_ASSERT(!mDoneAudioSeeking);
    RequestAudioData();
  }

  void HandleEndOfAudio() override {
    HandleEndOfAudioInternal();
    MaybeFinishSeek();
  }

  void HandleWaitingForVideo() override {
    MOZ_ASSERT(!mDoneVideoSeeking);
    mMaster->WaitForData(MediaData::Type::VIDEO_DATA);
  }

  void HandleVideoCanceled() override {
    MOZ_ASSERT(!mDoneVideoSeeking);
    RequestVideoData();
  }

  void HandleEndOfVideo() override {
    HandleEndOfVideoInternal();
    MaybeFinishSeek();
  }

  void HandleAudioWaited(MediaData::Type aType) override {
    MOZ_ASSERT(!mDoneAudioSeeking || !mDoneVideoSeeking,
               "Seek shouldn't be finished");

    RequestAudioData();
  }

  void HandleVideoWaited(MediaData::Type aType) override {
    MOZ_ASSERT(!mDoneAudioSeeking || !mDoneVideoSeeking,
               "Seek shouldn't be finished");

    RequestVideoData();
  }

  void DoSeek() override {
    mDoneAudioSeeking = !Info().HasAudio();
    mDoneVideoSeeking = !Info().HasVideo();

    mMaster->ResetDecode();
    mMaster->StopMediaSink();

    DemuxerSeek();
  }

  TimeUnit CalculateNewCurrentTime() const override {
    const auto seekTime = mSeekJob.mTarget->GetTime();

    // For the accurate seek, we always set the newCurrentTime = seekTime so
    // that the updated HTMLMediaElement.currentTime will always be the seek
    // target; we rely on the MediaSink to handles the gap between the
    // newCurrentTime and the real decoded samples' start time.
    if (mSeekJob.mTarget->IsAccurate()) {
      return seekTime;
    }

    // For the fast seek, we update the newCurrentTime with the decoded audio
    // and video samples, set it to be the one which is closet to the seekTime.
    if (mSeekJob.mTarget->IsFast()) {
      RefPtr<AudioData> audio = AudioQueue().PeekFront();
      RefPtr<VideoData> video = VideoQueue().PeekFront();

      // A situation that both audio and video approaches the end.
      if (!audio && !video) {
        return seekTime;
      }

      const int64_t audioStart =
          audio ? audio->mTime.ToMicroseconds() : INT64_MAX;
      const int64_t videoStart =
          video ? video->mTime.ToMicroseconds() : INT64_MAX;
      const int64_t audioGap = std::abs(audioStart - seekTime.ToMicroseconds());
      const int64_t videoGap = std::abs(videoStart - seekTime.ToMicroseconds());
      return TimeUnit::FromMicroseconds(audioGap <= videoGap ? audioStart
                                                             : videoStart);
    }

    MOZ_ASSERT(false, "AccurateSeekTask doesn't handle other seek types.");
    return TimeUnit::Zero();
  }

 protected:
  void DemuxerSeek() {
    // Request the demuxer to perform seek.
    Reader()
        ->Seek(mSeekJob.mTarget.ref())
        ->Then(
            OwnerThread(), __func__,
            [this](const media::TimeUnit& aUnit) { OnSeekResolved(aUnit); },
            [this](const SeekRejectValue& aReject) { OnSeekRejected(aReject); })
        ->Track(mSeekRequest);
  }

  void OnSeekResolved(media::TimeUnit) {
    mSeekRequest.Complete();

    // We must decode the first samples of active streams, so we can determine
    // the new stream time. So dispatch tasks to do that.
    if (!mDoneVideoSeeking) {
      RequestVideoData();
    }
    if (!mDoneAudioSeeking) {
      RequestAudioData();
    }
  }

  void OnSeekRejected(const SeekRejectValue& aReject) {
    mSeekRequest.Complete();

    if (aReject.mError == NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA) {
      SLOG("OnSeekRejected reason=WAITING_FOR_DATA type=%s",
           MediaData::TypeToStr(aReject.mType));
      MOZ_ASSERT_IF(aReject.mType == MediaData::Type::AUDIO_DATA,
                    !mMaster->IsRequestingAudioData());
      MOZ_ASSERT_IF(aReject.mType == MediaData::Type::VIDEO_DATA,
                    !mMaster->IsRequestingVideoData());
      MOZ_ASSERT_IF(aReject.mType == MediaData::Type::AUDIO_DATA,
                    !mMaster->IsWaitingAudioData());
      MOZ_ASSERT_IF(aReject.mType == MediaData::Type::VIDEO_DATA,
                    !mMaster->IsWaitingVideoData());

      // Fire 'waiting' to notify the player that we are waiting for data.
      mMaster->mOnNextFrameStatus.Notify(
          MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_SEEKING);

      Reader()
          ->WaitForData(aReject.mType)
          ->Then(
              OwnerThread(), __func__,
              [this](MediaData::Type aType) {
                SLOG("OnSeekRejected wait promise resolved");
                mWaitRequest.Complete();
                DemuxerSeek();
              },
              [this](const WaitForDataRejectValue& aRejection) {
                SLOG("OnSeekRejected wait promise rejected");
                mWaitRequest.Complete();
                mMaster->DecodeError(NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA);
              })
          ->Track(mWaitRequest);
      return;
    }

    if (aReject.mError == NS_ERROR_DOM_MEDIA_END_OF_STREAM) {
      if (!mDoneAudioSeeking) {
        HandleEndOfAudioInternal();
      }
      if (!mDoneVideoSeeking) {
        HandleEndOfVideoInternal();
      }
      MaybeFinishSeek();
      return;
    }

    MOZ_ASSERT(NS_FAILED(aReject.mError),
               "Cancels should also disconnect mSeekRequest");
    mMaster->DecodeError(aReject.mError);
  }

  void RequestAudioData() {
    MOZ_ASSERT(!mDoneAudioSeeking);
    mMaster->RequestAudioData();
  }

  virtual void RequestVideoData() {
    MOZ_ASSERT(!mDoneVideoSeeking);
    mMaster->RequestVideoData(media::TimeUnit());
  }

  void AdjustFastSeekIfNeeded(MediaData* aSample) {
    if (mSeekJob.mTarget->IsFast() &&
        mSeekJob.mTarget->GetTime() > mCurrentTimeBeforeSeek &&
        aSample->mTime < mCurrentTimeBeforeSeek) {
      // We are doing a fastSeek, but we ended up *before* the previous
      // playback position. This is surprising UX, so switch to an accurate
      // seek and decode to the seek target. This is not conformant to the
      // spec, fastSeek should always be fast, but until we get the time to
      // change all Readers to seek to the keyframe after the currentTime
      // in this case, we'll just decode forward. Bug 1026330.
      mSeekJob.mTarget->SetType(SeekTarget::Accurate);
    }
  }

  nsresult DropAudioUpToSeekTarget(AudioData* aAudio) {
    MOZ_ASSERT(aAudio && mSeekJob.mTarget->IsAccurate());

    if (mSeekJob.mTarget->GetTime() >= aAudio->GetEndTime()) {
      // Our seek target lies after the frames in this AudioData. Don't
      // push it onto the audio queue, and keep decoding forwards.
      return NS_OK;
    }

    if (aAudio->mTime > mSeekJob.mTarget->GetTime()) {
      // The seek target doesn't lie in the audio block just after the last
      // audio frames we've seen which were before the seek target. This
      // could have been the first audio data we've seen after seek, i.e. the
      // seek terminated after the seek target in the audio stream. Just
      // abort the audio decode-to-target, the state machine will play
      // silence to cover the gap. Typically this happens in poorly muxed
      // files.
      SLOGW("Audio not synced after seek, maybe a poorly muxed file?");
      mMaster->PushAudio(aAudio);
      mDoneAudioSeeking = true;
      return NS_OK;
    }

    bool ok = aAudio->SetTrimWindow(
        {mSeekJob.mTarget->GetTime(), aAudio->GetEndTime()});
    if (!ok) {
      return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
    }

    MOZ_ASSERT(AudioQueue().GetSize() == 0,
               "Should be the 1st sample after seeking");
    mMaster->PushAudio(aAudio);
    mDoneAudioSeeking = true;

    return NS_OK;
  }

  nsresult DropVideoUpToSeekTarget(VideoData* aVideo) {
    MOZ_ASSERT(aVideo);
    SLOG("DropVideoUpToSeekTarget() frame [%" PRId64 ", %" PRId64 "]",
         aVideo->mTime.ToMicroseconds(), aVideo->GetEndTime().ToMicroseconds());
    const auto target = GetSeekTarget();

    // If the frame end time is less than the seek target, we won't want
    // to display this frame after the seek, so discard it.
    if (target >= aVideo->GetEndTime()) {
      SLOG("DropVideoUpToSeekTarget() pop video frame [%" PRId64 ", %" PRId64
           "] target=%" PRId64,
           aVideo->mTime.ToMicroseconds(),
           aVideo->GetEndTime().ToMicroseconds(), target.ToMicroseconds());
      mFirstVideoFrameAfterSeek = aVideo;
    } else {
      if (target >= aVideo->mTime && aVideo->GetEndTime() >= target) {
        // The seek target lies inside this frame's time slice. Adjust the
        // frame's start time to match the seek target.
        aVideo->UpdateTimestamp(target);
      }
      mFirstVideoFrameAfterSeek = nullptr;

      SLOG("DropVideoUpToSeekTarget() found video frame [%" PRId64 ", %" PRId64
           "] containing target=%" PRId64,
           aVideo->mTime.ToMicroseconds(),
           aVideo->GetEndTime().ToMicroseconds(), target.ToMicroseconds());

      MOZ_ASSERT(VideoQueue().GetSize() == 0,
                 "Should be the 1st sample after seeking");
      mMaster->PushVideo(aVideo);
      mDoneVideoSeeking = true;
    }

    return NS_OK;
  }

  void HandleEndOfAudioInternal() {
    MOZ_ASSERT(!mDoneAudioSeeking);
    AudioQueue().Finish();
    mDoneAudioSeeking = true;
  }

  void HandleEndOfVideoInternal() {
    MOZ_ASSERT(!mDoneVideoSeeking);
    if (mFirstVideoFrameAfterSeek) {
      // Hit the end of stream. Move mFirstVideoFrameAfterSeek into
      // mSeekedVideoData so we have something to display after seeking.
      mMaster->PushVideo(mFirstVideoFrameAfterSeek);
    }
    VideoQueue().Finish();
    mDoneVideoSeeking = true;
  }

  void MaybeFinishSeek() {
    if (mDoneAudioSeeking && mDoneVideoSeeking) {
      SeekCompleted();
    }
  }

  /*
   * Track the current seek promise made by the reader.
   */
  MozPromiseRequestHolder<MediaFormatReader::SeekPromise> mSeekRequest;

  /*
   * Internal state.
   */
  media::TimeUnit mCurrentTimeBeforeSeek;
  bool mDoneAudioSeeking = false;
  bool mDoneVideoSeeking = false;
  MozPromiseRequestHolder<WaitForDataPromise> mWaitRequest;

  // This temporarily stores the first frame we decode after we seek.
  // This is so that if we hit end of stream while we're decoding to reach
  // the seek target, we will still have a frame that we can display as the
  // last frame in the media.
  RefPtr<VideoData> mFirstVideoFrameAfterSeek;

 private:
  virtual media::TimeUnit GetSeekTarget() const {
    return mSeekJob.mTarget->GetTime();
  }
};

/*
 * Remove samples from the queue until aCompare() returns false.
 * aCompare A function object with the signature bool(int64_t) which returns
 *          true for samples that should be removed.
 */
template <typename Type, typename Function>
static void DiscardFrames(MediaQueue<Type>& aQueue, const Function& aCompare) {
  while (aQueue.GetSize() > 0) {
    if (aCompare(aQueue.PeekFront()->mTime.ToMicroseconds())) {
      RefPtr<Type> releaseMe = aQueue.PopFront();
      continue;
    }
    break;
  }
}

class MediaDecoderStateMachine::NextFrameSeekingState
    : public MediaDecoderStateMachine::SeekingState {
 public:
  explicit NextFrameSeekingState(Master* aPtr) : SeekingState(aPtr) {}

  RefPtr<MediaDecoder::SeekPromise> Enter(SeekJob&& aSeekJob,
                                          EventVisibility aVisibility) {
    MOZ_ASSERT(aSeekJob.mTarget->IsNextFrame());
    mCurrentTime = mMaster->GetMediaTime();
    mDuration = mMaster->Duration();
    return SeekingState::Enter(std::move(aSeekJob), aVisibility);
  }

  void Exit() override {
    // Disconnect my async seek operation.
    if (mAsyncSeekTask) {
      mAsyncSeekTask->Cancel();
    }

    // Disconnect MediaDecoder.
    mSeekJob.RejectIfExists(__func__);
  }

  void HandleAudioDecoded(AudioData* aAudio) override {
    mMaster->PushAudio(aAudio);
  }

  void HandleVideoDecoded(VideoData* aVideo, TimeStamp aDecodeStart) override {
    MOZ_ASSERT(aVideo);
    MOZ_ASSERT(!mSeekJob.mPromise.IsEmpty(), "Seek shouldn't be finished");
    MOZ_ASSERT(NeedMoreVideo());

    if (aVideo->mTime > mCurrentTime) {
      mMaster->PushVideo(aVideo);
      FinishSeek();
    } else {
      RequestVideoData();
    }
  }

  void HandleWaitingForAudio() override {
    MOZ_ASSERT(!mSeekJob.mPromise.IsEmpty(), "Seek shouldn't be finished");
    // We don't care about audio decode errors in this state which will be
    // handled by other states after seeking.
  }

  void HandleAudioCanceled() override {
    MOZ_ASSERT(!mSeekJob.mPromise.IsEmpty(), "Seek shouldn't be finished");
    // We don't care about audio decode errors in this state which will be
    // handled by other states after seeking.
  }

  void HandleEndOfAudio() override {
    MOZ_ASSERT(!mSeekJob.mPromise.IsEmpty(), "Seek shouldn't be finished");
    // We don't care about audio decode errors in this state which will be
    // handled by other states after seeking.
  }

  void HandleWaitingForVideo() override {
    MOZ_ASSERT(!mSeekJob.mPromise.IsEmpty(), "Seek shouldn't be finished");
    MOZ_ASSERT(NeedMoreVideo());
    mMaster->WaitForData(MediaData::Type::VIDEO_DATA);
  }

  void HandleVideoCanceled() override {
    MOZ_ASSERT(!mSeekJob.mPromise.IsEmpty(), "Seek shouldn't be finished");
    MOZ_ASSERT(NeedMoreVideo());
    RequestVideoData();
  }

  void HandleEndOfVideo() override {
    MOZ_ASSERT(!mSeekJob.mPromise.IsEmpty(), "Seek shouldn't be finished");
    MOZ_ASSERT(NeedMoreVideo());
    VideoQueue().Finish();
    FinishSeek();
  }

  void HandleAudioWaited(MediaData::Type aType) override {
    // We don't care about audio in this state.
  }

  void HandleVideoWaited(MediaData::Type aType) override {
    MOZ_ASSERT(!mSeekJob.mPromise.IsEmpty(), "Seek shouldn't be finished");
    MOZ_ASSERT(NeedMoreVideo());
    RequestVideoData();
  }

  TimeUnit CalculateNewCurrentTime() const override {
    // The HTMLMediaElement.currentTime should be updated to the seek target
    // which has been updated to the next frame's time.
    return mSeekJob.mTarget->GetTime();
  }

  void DoSeek() override {
    auto currentTime = mCurrentTime;
    DiscardFrames(VideoQueue(), [currentTime](int64_t aSampleTime) {
      return aSampleTime <= currentTime.ToMicroseconds();
    });

    // If there is a pending video request, finish the seeking if we don't need
    // more data, or wait for HandleVideoDecoded() to finish seeking.
    if (mMaster->IsRequestingVideoData()) {
      if (!NeedMoreVideo()) {
        FinishSeek();
      }
      return;
    }

    // Otherwise, we need to do the seek operation asynchronously for a special
    // case (bug504613.ogv) which has no data at all, the 1st seekToNextFrame()
    // operation reaches the end of the media. If we did the seek operation
    // synchronously, we immediately resolve the SeekPromise in mSeekJob and
    // then switch to the CompletedState which dispatches an "ended" event.
    // However, the ThenValue of the SeekPromise has not yet been set, so the
    // promise resolving is postponed and then the JS developer receives the
    // "ended" event before the seek promise is resolved.
    // An asynchronous seek operation helps to solve this issue since while the
    // seek is actually performed, the ThenValue of SeekPromise has already
    // been set so that it won't be postponed.
    RefPtr<Runnable> r = mAsyncSeekTask = new AysncNextFrameSeekTask(this);
    nsresult rv = OwnerThread()->Dispatch(r.forget());
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

 private:
  void DoSeekInternal() {
    // We don't need to discard frames to the mCurrentTime here because we have
    // done it at DoSeek() and any video data received in between either
    // finishes the seek operation or be discarded, see HandleVideoDecoded().

    if (!NeedMoreVideo()) {
      FinishSeek();
    } else if (!mMaster->IsRequestingVideoData() &&
               !mMaster->IsWaitingVideoData()) {
      RequestVideoData();
    }
  }

  class AysncNextFrameSeekTask : public Runnable {
   public:
    explicit AysncNextFrameSeekTask(NextFrameSeekingState* aStateObject)
        : Runnable(
              "MediaDecoderStateMachine::NextFrameSeekingState::"
              "AysncNextFrameSeekTask"),
          mStateObj(aStateObject) {}

    void Cancel() { mStateObj = nullptr; }

    NS_IMETHOD Run() override {
      if (mStateObj) {
        mStateObj->DoSeekInternal();
      }
      return NS_OK;
    }

   private:
    NextFrameSeekingState* mStateObj;
  };

  void RequestVideoData() { mMaster->RequestVideoData(media::TimeUnit()); }

  bool NeedMoreVideo() const {
    // Need to request video when we have none and video queue is not finished.
    return VideoQueue().GetSize() == 0 && !VideoQueue().IsFinished();
  }

  // Update the seek target's time before resolving this seek task, the updated
  // time will be used in the MDSM::SeekCompleted() to update the MDSM's
  // position.
  void UpdateSeekTargetTime() {
    RefPtr<VideoData> data = VideoQueue().PeekFront();
    if (data) {
      mSeekJob.mTarget->SetTime(data->mTime);
    } else {
      MOZ_ASSERT(VideoQueue().AtEndOfStream());
      mSeekJob.mTarget->SetTime(mDuration);
    }
  }

  void FinishSeek() {
    MOZ_ASSERT(!NeedMoreVideo());
    UpdateSeekTargetTime();
    auto time = mSeekJob.mTarget->GetTime().ToMicroseconds();
    DiscardFrames(AudioQueue(),
                  [time](int64_t aSampleTime) { return aSampleTime < time; });
    SeekCompleted();
  }

  /*
   * Internal state.
   */
  TimeUnit mCurrentTime;
  TimeUnit mDuration;
  RefPtr<AysncNextFrameSeekTask> mAsyncSeekTask;
};

class MediaDecoderStateMachine::NextFrameSeekingFromDormantState
    : public MediaDecoderStateMachine::AccurateSeekingState {
 public:
  explicit NextFrameSeekingFromDormantState(Master* aPtr)
      : AccurateSeekingState(aPtr) {}

  RefPtr<MediaDecoder::SeekPromise> Enter(SeekJob&& aCurrentSeekJob,
                                          SeekJob&& aFutureSeekJob) {
    mFutureSeekJob = std::move(aFutureSeekJob);

    AccurateSeekingState::Enter(std::move(aCurrentSeekJob),
                                EventVisibility::Suppressed);

    // Once seekToNextFrame() is called, we assume the user is likely to keep
    // calling seekToNextFrame() repeatedly, and so, we should prevent the MDSM
    // from getting into Dormant state.
    mMaster->mMinimizePreroll = false;

    return mFutureSeekJob.mPromise.Ensure(__func__);
  }

  void Exit() override {
    mFutureSeekJob.RejectIfExists(__func__);
    AccurateSeekingState::Exit();
  }

 private:
  SeekJob mFutureSeekJob;

  // We don't want to transition to DecodingState once this seek completes,
  // instead, we transition to NextFrameSeekingState.
  void GoToNextState() override {
    SetState<NextFrameSeekingState>(std::move(mFutureSeekJob),
                                    EventVisibility::Observable);
  }
};

class MediaDecoderStateMachine::VideoOnlySeekingState
    : public MediaDecoderStateMachine::AccurateSeekingState {
 public:
  explicit VideoOnlySeekingState(Master* aPtr) : AccurateSeekingState(aPtr) {}

  RefPtr<MediaDecoder::SeekPromise> Enter(SeekJob&& aSeekJob,
                                          EventVisibility aVisibility) {
    MOZ_ASSERT(aSeekJob.mTarget->IsVideoOnly());
    MOZ_ASSERT(aVisibility == EventVisibility::Suppressed);

    RefPtr<MediaDecoder::SeekPromise> p =
        AccurateSeekingState::Enter(std::move(aSeekJob), aVisibility);

    // Dispatch a mozvideoonlyseekbegin event to indicate UI for corresponding
    // changes.
    mMaster->mOnPlaybackEvent.Notify(MediaPlaybackEvent::VideoOnlySeekBegin);

    return p.forget();
  }

  void Exit() override {
    // We are completing or discarding this video-only seek operation now,
    // dispatch an event so that the UI can change in response to the end
    // of video-only seek.
    mMaster->mOnPlaybackEvent.Notify(
        MediaPlaybackEvent::VideoOnlySeekCompleted);

    AccurateSeekingState::Exit();
  }

  void HandleAudioDecoded(AudioData* aAudio) override {
    MOZ_ASSERT(mDoneAudioSeeking && !mDoneVideoSeeking,
               "Seek shouldn't be finished");
    MOZ_ASSERT(aAudio);

    // Video-only seek doesn't reset audio decoder. There might be pending audio
    // requests when AccurateSeekTask::Seek() begins. We will just store the
    // data without checking |mDiscontinuity| or calling
    // DropAudioUpToSeekTarget().
    mMaster->PushAudio(aAudio);
  }

  void HandleWaitingForAudio() override {}

  void HandleAudioCanceled() override {}

  void HandleEndOfAudio() override {}

  void HandleAudioWaited(MediaData::Type aType) override {
    MOZ_ASSERT(!mDoneAudioSeeking || !mDoneVideoSeeking,
               "Seek shouldn't be finished");

    // Ignore pending requests from video-only seek.
  }

  void DoSeek() override {
    // TODO: keep decoding audio.
    mDoneAudioSeeking = true;
    mDoneVideoSeeking = !Info().HasVideo();

    mMaster->ResetDecode(TrackInfo::kVideoTrack);

    DemuxerSeek();
  }

 protected:
  // Allow skip-to-next-key-frame to kick in if we fall behind the current
  // playback position so decoding has a better chance to catch up.
  void RequestVideoData() override {
    MOZ_ASSERT(!mDoneVideoSeeking);

    const auto& clock = mMaster->mMediaSink->IsStarted()
                            ? mMaster->GetClock()
                            : mMaster->GetMediaTime();
    const auto& nextKeyFrameTime = GetNextKeyFrameTime();

    auto threshold = clock;

    if (nextKeyFrameTime.IsValid() &&
        clock >= (nextKeyFrameTime - sSkipToNextKeyFrameThreshold)) {
      threshold = nextKeyFrameTime;
    }

    mMaster->RequestVideoData(threshold);
  }

 private:
  // Trigger skip to next key frame if the current playback position is very
  // close the next key frame's time.
  static constexpr TimeUnit sSkipToNextKeyFrameThreshold =
      TimeUnit::FromMicroseconds(5000);

  // If the media is playing, drop video until catch up playback position.
  media::TimeUnit GetSeekTarget() const override {
    return mMaster->mMediaSink->IsStarted() ? mMaster->GetClock()
                                            : mSeekJob.mTarget->GetTime();
  }

  media::TimeUnit GetNextKeyFrameTime() const {
    // We only call this method in RequestVideoData() and we only request video
    // data if we haven't done video seeking.
    MOZ_DIAGNOSTIC_ASSERT(!mDoneVideoSeeking);
    MOZ_DIAGNOSTIC_ASSERT(mMaster->VideoQueue().GetSize() == 0);

    if (mFirstVideoFrameAfterSeek) {
      return mFirstVideoFrameAfterSeek->NextKeyFrameTime();
    }

    return TimeUnit::Invalid();
  }
};

constexpr TimeUnit MediaDecoderStateMachine::VideoOnlySeekingState::
    sSkipToNextKeyFrameThreshold;

RefPtr<MediaDecoder::SeekPromise>
MediaDecoderStateMachine::DormantState::HandleSeek(SeekTarget aTarget) {
  if (aTarget.IsNextFrame()) {
    // NextFrameSeekingState doesn't reset the decoder unlike
    // AccurateSeekingState. So we first must come out of dormant by seeking to
    // mPendingSeek and continue later with the NextFrameSeek
    SLOG("Changed state to SEEKING (to %" PRId64 ")",
         aTarget.GetTime().ToMicroseconds());
    SeekJob seekJob;
    seekJob.mTarget = Some(aTarget);
    return StateObject::SetState<NextFrameSeekingFromDormantState>(
        std::move(mPendingSeek), std::move(seekJob));
  }

  return StateObject::HandleSeek(aTarget);
}

/**
 * Purpose: stop playback until enough data is decoded to continue playback.
 *
 * Transition to:
 *   SEEKING if any seek request.
 *   SHUTDOWN if any decode error.
 *   COMPLETED when having decoded all audio/video data.
 *   DECODING/LOOPING_DECODING when having decoded enough data to continue
 * playback.
 */
class MediaDecoderStateMachine::BufferingState
    : public MediaDecoderStateMachine::StateObject {
 public:
  explicit BufferingState(Master* aPtr) : StateObject(aPtr) {}

  void Enter() {
    if (mMaster->IsPlaying()) {
      mMaster->StopPlayback();
    }

    mBufferingStart = TimeStamp::Now();
    mMaster->ScheduleStateMachineIn(TimeUnit::FromMicroseconds(USECS_PER_S));
    mMaster->mOnNextFrameStatus.Notify(
        MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_BUFFERING);
  }

  void Step() override;

  State GetState() const override { return DECODER_STATE_BUFFERING; }

  void HandleAudioDecoded(AudioData* aAudio) override {
    mMaster->PushAudio(aAudio);
    if (!mMaster->HaveEnoughDecodedAudio()) {
      mMaster->RequestAudioData();
    }
    // This might be the sample we need to exit buffering.
    // Schedule Step() to check it.
    mMaster->ScheduleStateMachine();
  }

  void HandleVideoDecoded(VideoData* aVideo, TimeStamp aDecodeStart) override {
    mMaster->PushVideo(aVideo);
    if (!mMaster->HaveEnoughDecodedVideo()) {
      mMaster->RequestVideoData(media::TimeUnit());
    }
    // This might be the sample we need to exit buffering.
    // Schedule Step() to check it.
    mMaster->ScheduleStateMachine();
  }

  void HandleAudioCanceled() override { mMaster->RequestAudioData(); }

  void HandleVideoCanceled() override {
    mMaster->RequestVideoData(media::TimeUnit());
  }

  void HandleWaitingForAudio() override {
    mMaster->WaitForData(MediaData::Type::AUDIO_DATA);
  }

  void HandleWaitingForVideo() override {
    mMaster->WaitForData(MediaData::Type::VIDEO_DATA);
  }

  void HandleAudioWaited(MediaData::Type aType) override {
    mMaster->RequestAudioData();
  }

  void HandleVideoWaited(MediaData::Type aType) override {
    mMaster->RequestVideoData(media::TimeUnit());
  }

  void HandleEndOfAudio() override;
  void HandleEndOfVideo() override;

  void HandleVideoSuspendTimeout() override {
    // No video, so nothing to suspend.
    if (!mMaster->HasVideo()) {
      return;
    }

    mMaster->mVideoDecodeSuspended = true;
    mMaster->mOnPlaybackEvent.Notify(MediaPlaybackEvent::EnterVideoSuspend);
    Reader()->SetVideoBlankDecode(true);
  }

 private:
  TimeStamp mBufferingStart;

  // The maximum number of second we spend buffering when we are short on
  // unbuffered data.
  const uint32_t mBufferingWait = 15;
};

/**
 * Purpose: play all the decoded data and fire the 'ended' event.
 *
 * Transition to:
 *   SEEKING if any seek request.
 *   LOOPING_DECODING if MDSM enable looping.
 */
class MediaDecoderStateMachine::CompletedState
    : public MediaDecoderStateMachine::StateObject {
 public:
  explicit CompletedState(Master* aPtr) : StateObject(aPtr) {}

  void Enter() {
    // On Android, the life cycle of graphic buffer is equal to Android's codec,
    // we couldn't release it if we still need to render the frame.
#ifndef MOZ_WIDGET_ANDROID
    if (!mMaster->mLooping) {
      // We've decoded all samples.
      // We don't need decoders anymore if not looping.
      Reader()->ReleaseResources();
    }
#endif
    bool hasNextFrame = (!mMaster->HasAudio() || !mMaster->mAudioCompleted) &&
                        (!mMaster->HasVideo() || !mMaster->mVideoCompleted);

    mMaster->mOnNextFrameStatus.Notify(
        hasNextFrame ? MediaDecoderOwner::NEXT_FRAME_AVAILABLE
                     : MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE);

    Step();
  }

  void Exit() override { mSentPlaybackEndedEvent = false; }

  void Step() override {
    if (mMaster->mPlayState != MediaDecoder::PLAY_STATE_PLAYING &&
        mMaster->IsPlaying()) {
      mMaster->StopPlayback();
    }

    // Play the remaining media. We want to run AdvanceFrame() at least
    // once to ensure the current playback position is advanced to the
    // end of the media, and so that we update the readyState.
    if ((mMaster->HasVideo() && !mMaster->mVideoCompleted) ||
        (mMaster->HasAudio() && !mMaster->mAudioCompleted)) {
      // Start playback if necessary to play the remaining media.
      mMaster->MaybeStartPlayback();
      mMaster->UpdatePlaybackPositionPeriodically();
      MOZ_ASSERT(!mMaster->IsPlaying() || mMaster->IsStateMachineScheduled(),
                 "Must have timer scheduled");
      return;
    }

    // StopPlayback in order to reset the IsPlaying() state so audio
    // is restarted correctly.
    mMaster->StopPlayback();

    if (!mSentPlaybackEndedEvent) {
      auto clockTime =
          std::max(mMaster->AudioEndTime(), mMaster->VideoEndTime());
      // Correct the time over the end once looping was turned on.
      mMaster->AdjustByLooping(clockTime);
      if (mMaster->mDuration.Ref()->IsInfinite()) {
        // We have a finite duration when playback reaches the end.
        mMaster->mDuration = Some(clockTime);
        DDLOGEX(mMaster, DDLogCategory::Property, "duration_us",
                mMaster->mDuration.Ref()->ToMicroseconds());
      }
      mMaster->UpdatePlaybackPosition(clockTime);

      // Ensure readyState is updated before firing the 'ended' event.
      mMaster->mOnNextFrameStatus.Notify(
          MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE);

      mMaster->mOnPlaybackEvent.Notify(MediaPlaybackEvent::PlaybackEnded);

      mSentPlaybackEndedEvent = true;

      // MediaSink::GetEndTime() must be called before stopping playback.
      mMaster->StopMediaSink();
    }
  }

  State GetState() const override { return DECODER_STATE_COMPLETED; }

  void HandleLoopingChanged() override {
    if (mMaster->mLooping) {
      SetDecodingState();
    }
  }

  void HandleAudioCaptured() override {
    // MediaSink is changed. Schedule Step() to check if we can start playback.
    mMaster->ScheduleStateMachine();
  }

  void HandleVideoSuspendTimeout() override {
    // Do nothing since no decoding is going on.
  }

  void HandleResumeVideoDecoding(const TimeUnit&) override {
    // Resume the video decoder and seek to the last video frame.
    // This triggers a video-only seek which won't update the playback position.
    StateObject::HandleResumeVideoDecoding(mMaster->mDecodedVideoEndTime);
  }

  void HandlePlayStateChanged(MediaDecoder::PlayState aPlayState) override {
    if (aPlayState == MediaDecoder::PLAY_STATE_PLAYING) {
      // Schedule Step() to check if we can start playback.
      mMaster->ScheduleStateMachine();
    }
  }

 private:
  bool mSentPlaybackEndedEvent = false;
};

/**
 * Purpose: release all resources allocated by MDSM.
 *
 * Transition to:
 *   None since this is the final state.
 *
 * Transition from:
 *   Any states other than SHUTDOWN.
 */
class MediaDecoderStateMachine::ShutdownState
    : public MediaDecoderStateMachine::StateObject {
 public:
  explicit ShutdownState(Master* aPtr) : StateObject(aPtr) {}

  RefPtr<ShutdownPromise> Enter();

  void Exit() override {
    MOZ_DIAGNOSTIC_ASSERT(false, "Shouldn't escape the SHUTDOWN state.");
  }

  State GetState() const override { return DECODER_STATE_SHUTDOWN; }

  RefPtr<MediaDecoder::SeekPromise> HandleSeek(SeekTarget aTarget) override {
    MOZ_DIAGNOSTIC_ASSERT(false, "Can't seek in shutdown state.");
    return MediaDecoder::SeekPromise::CreateAndReject(true, __func__);
  }

  RefPtr<ShutdownPromise> HandleShutdown() override {
    MOZ_DIAGNOSTIC_ASSERT(false, "Already shutting down.");
    return nullptr;
  }

  void HandleVideoSuspendTimeout() override {
    MOZ_DIAGNOSTIC_ASSERT(false, "Already shutting down.");
  }

  void HandleResumeVideoDecoding(const TimeUnit&) override {
    MOZ_DIAGNOSTIC_ASSERT(false, "Already shutting down.");
  }
};

RefPtr<MediaDecoder::SeekPromise>
MediaDecoderStateMachine::StateObject::HandleSeek(SeekTarget aTarget) {
  SLOG("Changed state to SEEKING (to %" PRId64 ")",
       aTarget.GetTime().ToMicroseconds());
  SeekJob seekJob;
  seekJob.mTarget = Some(aTarget);
  return SetSeekingState(std::move(seekJob), EventVisibility::Observable);
}

RefPtr<ShutdownPromise>
MediaDecoderStateMachine::StateObject::HandleShutdown() {
  return SetState<ShutdownState>();
}

static void ReportRecoveryTelemetry(const TimeStamp& aRecoveryStart,
                                    const MediaInfo& aMediaInfo,
                                    bool aIsHardwareAccelerated) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!aMediaInfo.HasVideo()) {
    return;
  }

  // Keyed by audio+video or video alone, hardware acceleration,
  // and by a resolution range.
  nsCString key(aMediaInfo.HasAudio() ? "AV" : "V");
  key.AppendASCII(aIsHardwareAccelerated ? "(hw)," : ",");
  static const struct {
    int32_t mH;
    const char* mRes;
  } sResolutions[] = {{240, "0-240"},
                      {480, "241-480"},
                      {720, "481-720"},
                      {1080, "721-1080"},
                      {2160, "1081-2160"}};
  const char* resolution = "2161+";
  int32_t height = aMediaInfo.mVideo.mImage.height;
  for (const auto& res : sResolutions) {
    if (height <= res.mH) {
      resolution = res.mRes;
      break;
    }
  }
  key.AppendASCII(resolution);

  TimeDuration duration = TimeStamp::Now() - aRecoveryStart;
  double duration_ms = duration.ToMilliseconds();
  Telemetry::Accumulate(Telemetry::VIDEO_SUSPEND_RECOVERY_TIME_MS, key,
                        uint32_t(duration_ms + 0.5));
  Telemetry::Accumulate(Telemetry::VIDEO_SUSPEND_RECOVERY_TIME_MS,
                        NS_LITERAL_CSTRING("All"), uint32_t(duration_ms + 0.5));
}

void MediaDecoderStateMachine::StateObject::HandleResumeVideoDecoding(
    const TimeUnit& aTarget) {
  MOZ_ASSERT(mMaster->mVideoDecodeSuspended);

  mMaster->mVideoDecodeSuspended = false;
  mMaster->mOnPlaybackEvent.Notify(MediaPlaybackEvent::ExitVideoSuspend);
  Reader()->SetVideoBlankDecode(false);

  // Start counting recovery time from right now.
  TimeStamp start = TimeStamp::Now();

  // Local reference to mInfo, so that it will be copied in the lambda below.
  auto& info = Info();
  bool hw = Reader()->VideoIsHardwareAccelerated();

  // Start video-only seek to the current time.
  SeekJob seekJob;

  // We use fastseek to optimize the resuming time.
  // FastSeek is only used for video-only media since we don't need to worry
  // about A/V sync.
  // Don't use fastSeek if we want to seek to the end because it might seek to a
  // keyframe before the last frame (if the last frame itself is not a keyframe)
  // and we always want to present the final frame to the user when seeking to
  // the end.
  const auto type = mMaster->HasAudio() || aTarget == mMaster->Duration()
                        ? SeekTarget::Type::Accurate
                        : SeekTarget::Type::PrevSyncPoint;

  seekJob.mTarget.emplace(aTarget, type, true /* aVideoOnly */);

  // Hold mMaster->mAbstractMainThread here because this->mMaster will be
  // invalid after the current state object is deleted in SetState();
  RefPtr<AbstractThread> mainThread = mMaster->mAbstractMainThread;

  SetSeekingState(std::move(seekJob), EventVisibility::Suppressed)
      ->Then(
          mainThread, __func__,
          [start, info, hw]() { ReportRecoveryTelemetry(start, info, hw); },
          []() {});
}

RefPtr<MediaDecoder::SeekPromise>
MediaDecoderStateMachine::StateObject::SetSeekingState(
    SeekJob&& aSeekJob, EventVisibility aVisibility) {
  if (aSeekJob.mTarget->IsAccurate() || aSeekJob.mTarget->IsFast()) {
    if (aSeekJob.mTarget->IsVideoOnly()) {
      return SetState<VideoOnlySeekingState>(std::move(aSeekJob), aVisibility);
    }
    return SetState<AccurateSeekingState>(std::move(aSeekJob), aVisibility);
  }

  if (aSeekJob.mTarget->IsNextFrame()) {
    return SetState<NextFrameSeekingState>(std::move(aSeekJob), aVisibility);
  }

  MOZ_ASSERT_UNREACHABLE("Unknown SeekTarget::Type.");
  return nullptr;
}

void MediaDecoderStateMachine::StateObject::SetDecodingState() {
  if (mMaster->IsInSeamlessLooping()) {
    SetState<LoopingDecodingState>();
    return;
  }
  SetState<DecodingState>();
}

void MediaDecoderStateMachine::DecodeMetadataState::OnMetadataRead(
    MetadataHolder&& aMetadata) {
  mMetadataRequest.Complete();

  mMaster->mInfo.emplace(*aMetadata.mInfo);
  mMaster->mMediaSeekable = Info().mMediaSeekable;
  mMaster->mMediaSeekableOnlyInBufferedRanges =
      Info().mMediaSeekableOnlyInBufferedRanges;

  if (Info().mMetadataDuration.isSome()) {
    mMaster->mDuration = Info().mMetadataDuration;
  } else if (Info().mUnadjustedMetadataEndTime.isSome()) {
    const TimeUnit unadjusted = Info().mUnadjustedMetadataEndTime.ref();
    const TimeUnit adjustment = Info().mStartTime;
    mMaster->mInfo->mMetadataDuration.emplace(unadjusted - adjustment);
    mMaster->mDuration = Info().mMetadataDuration;
  }

  // If we don't know the duration by this point, we assume infinity, per spec.
  if (mMaster->mDuration.Ref().isNothing()) {
    mMaster->mDuration = Some(TimeUnit::FromInfinity());
  }

  DDLOGEX(mMaster, DDLogCategory::Property, "duration_us",
          mMaster->mDuration.Ref()->ToMicroseconds());

  if (mMaster->HasVideo()) {
    SLOG("Video decode HWAccel=%d videoQueueSize=%d",
         Reader()->VideoIsHardwareAccelerated(),
         mMaster->GetAmpleVideoFrames());
  }

  MOZ_ASSERT(mMaster->mDuration.Ref().isSome());

  mMaster->mMetadataLoadedEvent.Notify(std::move(aMetadata.mInfo),
                                       std::move(aMetadata.mTags),
                                       MediaDecoderEventVisibility::Observable);

  // Check whether the media satisfies the requirement of seamless looing.
  // (Before checking the media is audio only, we need to get metadata first.)
  mMaster->mSeamlessLoopingAllowed = StaticPrefs::MediaSeamlessLooping() &&
                                     mMaster->HasAudio() &&
                                     !mMaster->HasVideo();

  SetState<DecodingFirstFrameState>();
}

void MediaDecoderStateMachine::DormantState::HandlePlayStateChanged(
    MediaDecoder::PlayState aPlayState) {
  if (aPlayState == MediaDecoder::PLAY_STATE_PLAYING) {
    // Exit dormant when the user wants to play.
    MOZ_ASSERT(mMaster->mSentFirstFrameLoadedEvent);
    SetSeekingState(std::move(mPendingSeek), EventVisibility::Suppressed);
  }
}

void MediaDecoderStateMachine::DecodingFirstFrameState::Enter() {
  // Transition to DECODING if we've decoded first frames.
  if (mMaster->mSentFirstFrameLoadedEvent) {
    SetDecodingState();
    return;
  }

  MOZ_ASSERT(!mMaster->mVideoDecodeSuspended);

  // Dispatch tasks to decode first frames.
  if (mMaster->HasAudio()) {
    mMaster->RequestAudioData();
  }
  if (mMaster->HasVideo()) {
    mMaster->RequestVideoData(media::TimeUnit());
  }
}

void MediaDecoderStateMachine::DecodingFirstFrameState::
    MaybeFinishDecodeFirstFrame() {
  MOZ_ASSERT(!mMaster->mSentFirstFrameLoadedEvent);

  if ((mMaster->IsAudioDecoding() && AudioQueue().GetSize() == 0) ||
      (mMaster->IsVideoDecoding() && VideoQueue().GetSize() == 0)) {
    return;
  }

  mMaster->FinishDecodeFirstFrame();
  if (mPendingSeek.Exists()) {
    SetSeekingState(std::move(mPendingSeek), EventVisibility::Observable);
  } else {
    SetDecodingState();
  }
}

void MediaDecoderStateMachine::DecodingState::Enter() {
  MOZ_ASSERT(mMaster->mSentFirstFrameLoadedEvent);

  if (mMaster->mVideoDecodeSuspended &&
      mMaster->mVideoDecodeMode == VideoDecodeMode::Normal) {
    StateObject::HandleResumeVideoDecoding(mMaster->GetMediaTime());
    return;
  }

  if (mMaster->mVideoDecodeMode == VideoDecodeMode::Suspend &&
      !mMaster->mVideoDecodeSuspendTimer.IsScheduled() &&
      !mMaster->mVideoDecodeSuspended) {
    // If the VideoDecodeMode is Suspend and the timer is not schedule, it means
    // the timer has timed out and we should suspend video decoding now if
    // necessary.
    HandleVideoSuspendTimeout();
  }

  // If we're in the normal decoding mode and the decoding has finished, then we
  // should go to `completed` state because we don't need to decode anything
  // later. However, if we're in the saemless decoding mode, we will restart
  // decoding ASAP so we can still stay in `decoding` state.
  if (!mMaster->IsVideoDecoding() && !mMaster->IsAudioDecoding() &&
      !mMaster->IsInSeamlessLooping()) {
    SetState<CompletedState>();
    return;
  }

  mOnAudioPopped =
      AudioQueue().PopFrontEvent().Connect(OwnerThread(), [this]() {
        if (mMaster->IsAudioDecoding() && !mMaster->HaveEnoughDecodedAudio()) {
          EnsureAudioDecodeTaskQueued();
        }
      });
  mOnVideoPopped =
      VideoQueue().PopFrontEvent().Connect(OwnerThread(), [this]() {
        if (mMaster->IsVideoDecoding() && !mMaster->HaveEnoughDecodedVideo()) {
          EnsureVideoDecodeTaskQueued();
        }
      });

  mMaster->mOnNextFrameStatus.Notify(MediaDecoderOwner::NEXT_FRAME_AVAILABLE);

  mDecodeStartTime = TimeStamp::Now();

  MaybeStopPrerolling();

  // Ensure that we've got tasks enqueued to decode data if we need to.
  DispatchDecodeTasksIfNeeded();

  mMaster->ScheduleStateMachine();

  // Will enter dormant when playback is paused for a while.
  if (mMaster->mPlayState == MediaDecoder::PLAY_STATE_PAUSED) {
    StartDormantTimer();
  }
}

void MediaDecoderStateMachine::DecodingState::Step() {
  if (mMaster->mPlayState != MediaDecoder::PLAY_STATE_PLAYING &&
      mMaster->IsPlaying()) {
    // We're playing, but the element/decoder is in paused state. Stop
    // playing!
    mMaster->StopPlayback();
  }

  // Start playback if necessary so that the clock can be properly queried.
  if (!mIsPrerolling) {
    mMaster->MaybeStartPlayback();
  }

  mMaster->UpdatePlaybackPositionPeriodically();
  MOZ_ASSERT(!mMaster->IsPlaying() || mMaster->IsStateMachineScheduled(),
             "Must have timer scheduled");

  MaybeStartBuffering();
}

void MediaDecoderStateMachine::DecodingState::HandleEndOfAudio() {
  AudioQueue().Finish();
  if (!mMaster->IsVideoDecoding()) {
    SetState<CompletedState>();
  } else {
    MaybeStopPrerolling();
  }
}

void MediaDecoderStateMachine::DecodingState::HandleEndOfVideo() {
  VideoQueue().Finish();
  if (!mMaster->IsAudioDecoding()) {
    SetState<CompletedState>();
  } else {
    MaybeStopPrerolling();
  }
}

void MediaDecoderStateMachine::DecodingState::DispatchDecodeTasksIfNeeded() {
  if (mMaster->IsAudioDecoding() && !mMaster->mMinimizePreroll &&
      !mMaster->HaveEnoughDecodedAudio()) {
    EnsureAudioDecodeTaskQueued();
  }

  if (mMaster->IsVideoDecoding() && !mMaster->mMinimizePreroll &&
      !mMaster->HaveEnoughDecodedVideo()) {
    EnsureVideoDecodeTaskQueued();
  }
}

void MediaDecoderStateMachine::DecodingState::EnsureAudioDecodeTaskQueued() {
  if (!mMaster->IsAudioDecoding() || mMaster->IsRequestingAudioData() ||
      mMaster->IsWaitingAudioData()) {
    return;
  }
  mMaster->RequestAudioData();
}

void MediaDecoderStateMachine::DecodingState::EnsureVideoDecodeTaskQueued() {
  if (!mMaster->IsVideoDecoding() || mMaster->IsRequestingVideoData() ||
      mMaster->IsWaitingVideoData()) {
    return;
  }
  mMaster->RequestVideoData(mMaster->GetMediaTime());
}

void MediaDecoderStateMachine::DecodingState::MaybeStartBuffering() {
  // Buffering makes senses only after decoding first frames.
  MOZ_ASSERT(mMaster->mSentFirstFrameLoadedEvent);

  // Don't enter buffering when MediaDecoder is not playing.
  if (mMaster->mPlayState != MediaDecoder::PLAY_STATE_PLAYING) {
    return;
  }

  // Don't enter buffering while prerolling so that the decoder has a chance to
  // enqueue some decoded data before we give up and start buffering.
  if (!mMaster->IsPlaying()) {
    return;
  }

  // Note we could have a wait promise pending when playing non-MSE EME.
  if ((mMaster->OutOfDecodedAudio() && mMaster->IsWaitingAudioData()) ||
      (mMaster->OutOfDecodedVideo() && mMaster->IsWaitingVideoData())) {
    SetState<BufferingState>();
    return;
  }

  if (Reader()->UseBufferingHeuristics() && mMaster->HasLowDecodedData() &&
      mMaster->HasLowBufferedData() && !mMaster->mCanPlayThrough) {
    SetState<BufferingState>();
  }
}

void MediaDecoderStateMachine::LoopingDecodingState::HandleError(
    const MediaResult& aError) {
  SLOG("audio looping failed, aError=%s", aError.ErrorName().get());
  switch (aError.Code()) {
    case NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA:
      HandleWaitingForAudio();
      break;
    case NS_ERROR_DOM_MEDIA_END_OF_STREAM:
      // This would happen after we've closed resouce so that we won't be able
      // to get any sample anymore.
      SetState<CompletedState>();
      break;
    default:
      mMaster->DecodeError(aError);
      break;
  }
}

void MediaDecoderStateMachine::SeekingState::SeekCompleted() {
  const auto newCurrentTime = CalculateNewCurrentTime();

  if (newCurrentTime == mMaster->Duration() && !mMaster->mIsLiveStream) {
    // Seeked to end of media. Explicitly finish the queues so DECODING
    // will transition to COMPLETED immediately. Note we don't do
    // this when playing a live stream, since the end of media will advance
    // once we download more data!
    AudioQueue().Finish();
    VideoQueue().Finish();

    // We won't start MediaSink when paused. m{Audio,Video}Completed will
    // remain false and 'playbackEnded' won't be notified. Therefore we
    // need to set these flags explicitly when seeking to the end.
    mMaster->mAudioCompleted = true;
    mMaster->mVideoCompleted = true;

    // There might still be a pending audio request when doing video-only or
    // next-frame seek. Discard it so we won't break the invariants of the
    // COMPLETED state by adding audio samples to a finished queue.
    mMaster->mAudioDataRequest.DisconnectIfExists();
  }

  // We want to resolve the seek request prior finishing the first frame
  // to ensure that the seeked event is fired prior loadeded.
  // Note: SeekJob.Resolve() resets SeekJob.mTarget. Don't use mSeekJob anymore
  //       hereafter.
  mSeekJob.Resolve(__func__);

  // Notify FirstFrameLoaded now if we haven't since we've decoded some data
  // for readyState to transition to HAVE_CURRENT_DATA and fire 'loadeddata'.
  if (!mMaster->mSentFirstFrameLoadedEvent) {
    mMaster->FinishDecodeFirstFrame();
  }

  // Ensure timestamps are up to date.
  // Suppressed visibility comes from two cases: (1) leaving dormant state,
  // and (2) resuming suspended video decoder. We want both cases to be
  // transparent to the user. So we only notify the change when the seek
  // request is from the user.
  if (mVisibility == EventVisibility::Observable) {
    // Don't update playback position for video-only seek.
    // Otherwise we might have |newCurrentTime > mMediaSink->GetPosition()|
    // and fail the assertion in GetClock() since we didn't stop MediaSink.
    mMaster->UpdatePlaybackPositionInternal(newCurrentTime);
  }

  // Try to decode another frame to detect if we're at the end...
  SLOG("Seek completed, mCurrentPosition=%" PRId64,
       mMaster->mCurrentPosition.Ref().ToMicroseconds());

  if (mMaster->VideoQueue().PeekFront()) {
    mMaster->mMediaSink->Redraw(Info().mVideo);
    mMaster->mOnPlaybackEvent.Notify(MediaPlaybackEvent::Invalidate);
  }

  GoToNextState();
}

void MediaDecoderStateMachine::BufferingState::Step() {
  TimeStamp now = TimeStamp::Now();
  MOZ_ASSERT(!mBufferingStart.IsNull(), "Must know buffering start time.");

  if (Reader()->UseBufferingHeuristics()) {
    if (mMaster->IsWaitingAudioData() || mMaster->IsWaitingVideoData()) {
      // Can't exit buffering when we are still waiting for data.
      // Note we don't schedule next loop for we will do that when the wait
      // promise is resolved.
      return;
    }
    // With buffering heuristics, we exit buffering state when we:
    // 1. can play through or
    // 2. time out (specified by mBufferingWait) or
    // 3. have enough buffered data.
    TimeDuration elapsed = now - mBufferingStart;
    TimeDuration timeout =
        TimeDuration::FromSeconds(mBufferingWait * mMaster->mPlaybackRate);
    bool stopBuffering =
        mMaster->mCanPlayThrough || elapsed >= timeout ||
        !mMaster->HasLowBufferedData(TimeUnit::FromSeconds(mBufferingWait));
    if (!stopBuffering) {
      SLOG("Buffering: wait %ds, timeout in %.3lfs", mBufferingWait,
           mBufferingWait - elapsed.ToSeconds());
      mMaster->ScheduleStateMachineIn(TimeUnit::FromMicroseconds(USECS_PER_S));
      return;
    }
  } else if (mMaster->OutOfDecodedAudio() || mMaster->OutOfDecodedVideo()) {
    MOZ_ASSERT(!mMaster->OutOfDecodedAudio() ||
               mMaster->IsRequestingAudioData() ||
               mMaster->IsWaitingAudioData());
    MOZ_ASSERT(!mMaster->OutOfDecodedVideo() ||
               mMaster->IsRequestingVideoData() ||
               mMaster->IsWaitingVideoData());
    SLOG(
        "In buffering mode, waiting to be notified: outOfAudio: %d, "
        "mAudioStatus: %s, outOfVideo: %d, mVideoStatus: %s",
        mMaster->OutOfDecodedAudio(), mMaster->AudioRequestStatus(),
        mMaster->OutOfDecodedVideo(), mMaster->VideoRequestStatus());
    return;
  }

  SLOG("Buffered for %.3lfs", (now - mBufferingStart).ToSeconds());
  SetDecodingState();
}

void MediaDecoderStateMachine::BufferingState::HandleEndOfAudio() {
  AudioQueue().Finish();
  if (!mMaster->IsVideoDecoding()) {
    SetState<CompletedState>();
  } else {
    // Check if we can exit buffering.
    mMaster->ScheduleStateMachine();
  }
}

void MediaDecoderStateMachine::BufferingState::HandleEndOfVideo() {
  VideoQueue().Finish();
  if (!mMaster->IsAudioDecoding()) {
    SetState<CompletedState>();
  } else {
    // Check if we can exit buffering.
    mMaster->ScheduleStateMachine();
  }
}

RefPtr<ShutdownPromise> MediaDecoderStateMachine::ShutdownState::Enter() {
  auto master = mMaster;

  master->mDelayedScheduler.Reset();

  // Shutdown happens while decode timer is active, we need to disconnect and
  // dispose of the timer.
  master->CancelSuspendTimer();

  if (master->IsPlaying()) {
    master->StopPlayback();
  }

  master->mAudioDataRequest.DisconnectIfExists();
  master->mVideoDataRequest.DisconnectIfExists();
  master->mAudioWaitRequest.DisconnectIfExists();
  master->mVideoWaitRequest.DisconnectIfExists();

  master->ResetDecode();
  master->StopMediaSink();
  master->mMediaSink->Shutdown();

  // Prevent dangling pointers by disconnecting the listeners.
  master->mAudioQueueListener.Disconnect();
  master->mVideoQueueListener.Disconnect();
  master->mMetadataManager.Disconnect();
  master->mOnMediaNotSeekable.Disconnect();

  // Disconnect canonicals and mirrors before shutting down our task queue.
  master->mBuffered.DisconnectIfConnected();
  master->mPlayState.DisconnectIfConnected();
  master->mVolume.DisconnectIfConnected();
  master->mPreservesPitch.DisconnectIfConnected();
  master->mLooping.DisconnectIfConnected();
  master->mSameOriginMedia.DisconnectIfConnected();

  master->mDuration.DisconnectAll();
  master->mCurrentPosition.DisconnectAll();
  master->mIsAudioDataAudible.DisconnectAll();

  // Shut down the watch manager to stop further notifications.
  master->mWatchManager.Shutdown();

  return Reader()->Shutdown()->Then(OwnerThread(), __func__, master,
                                    &MediaDecoderStateMachine::FinishShutdown,
                                    &MediaDecoderStateMachine::FinishShutdown);
}

#define INIT_WATCHABLE(name, val) name(val, "MediaDecoderStateMachine::" #name)
#define INIT_MIRROR(name, val) \
  name(mTaskQueue, val, "MediaDecoderStateMachine::" #name " (Mirror)")
#define INIT_CANONICAL(name, val) \
  name(mTaskQueue, val, "MediaDecoderStateMachine::" #name " (Canonical)")

MediaDecoderStateMachine::MediaDecoderStateMachine(MediaDecoder* aDecoder,
                                                   MediaFormatReader* aReader)
    : mDecoderID(aDecoder),
      mAbstractMainThread(aDecoder->AbstractMainThread()),
      mFrameStats(&aDecoder->GetFrameStatistics()),
      mVideoFrameContainer(aDecoder->GetVideoFrameContainer()),
      mTaskQueue(new TaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK),
                               "MDSM::mTaskQueue",
                               /* aSupportsTailDispatch = */ true)),
      mWatchManager(this, mTaskQueue),
      mDispatchedStateMachine(false),
      mDelayedScheduler(mTaskQueue, true /*aFuzzy*/),
      mCurrentFrameID(0),
      mReader(new ReaderProxy(mTaskQueue, aReader)),
      mPlaybackRate(1.0),
      mAmpleAudioThreshold(detail::AMPLE_AUDIO_THRESHOLD),
      mAudioCaptured(false),
      mMinimizePreroll(aDecoder->GetMinimizePreroll()),
      mSentFirstFrameLoadedEvent(false),
      mVideoDecodeSuspended(false),
      mVideoDecodeSuspendTimer(mTaskQueue),
      mOutputStreamManager(nullptr),
      mVideoDecodeMode(VideoDecodeMode::Normal),
      mIsMSE(aDecoder->IsMSE()),
      mSeamlessLoopingAllowed(false),
      INIT_MIRROR(mBuffered, TimeIntervals()),
      INIT_MIRROR(mPlayState, MediaDecoder::PLAY_STATE_LOADING),
      INIT_MIRROR(mVolume, 1.0),
      INIT_MIRROR(mPreservesPitch, true),
      INIT_MIRROR(mLooping, false),
      INIT_MIRROR(mSameOriginMedia, false),
      INIT_CANONICAL(mDuration, NullableTimeUnit()),
      INIT_CANONICAL(mCurrentPosition, TimeUnit::Zero()),
      INIT_CANONICAL(mIsAudioDataAudible, false),
      mSetSinkRequestsCount(0) {
  MOZ_COUNT_CTOR(MediaDecoderStateMachine);
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  InitVideoQueuePrefs();

  DDLINKCHILD("reader", aReader);
}

#undef INIT_WATCHABLE
#undef INIT_MIRROR
#undef INIT_CANONICAL

MediaDecoderStateMachine::~MediaDecoderStateMachine() {
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  MOZ_COUNT_DTOR(MediaDecoderStateMachine);
}

void MediaDecoderStateMachine::InitializationTask(MediaDecoder* aDecoder) {
  MOZ_ASSERT(OnTaskQueue());

  // Connect mirrors.
  mBuffered.Connect(mReader->CanonicalBuffered());
  mPlayState.Connect(aDecoder->CanonicalPlayState());
  mVolume.Connect(aDecoder->CanonicalVolume());
  mPreservesPitch.Connect(aDecoder->CanonicalPreservesPitch());
  mLooping.Connect(aDecoder->CanonicalLooping());
  mSameOriginMedia.Connect(aDecoder->CanonicalSameOriginMedia());

  // Initialize watchers.
  mWatchManager.Watch(mBuffered,
                      &MediaDecoderStateMachine::BufferedRangeUpdated);
  mWatchManager.Watch(mVolume, &MediaDecoderStateMachine::VolumeChanged);
  mWatchManager.Watch(mPreservesPitch,
                      &MediaDecoderStateMachine::PreservesPitchChanged);
  mWatchManager.Watch(mPlayState, &MediaDecoderStateMachine::PlayStateChanged);
  mWatchManager.Watch(mLooping, &MediaDecoderStateMachine::LoopingChanged);

  MOZ_ASSERT(!mStateObj);
  auto* s = new DecodeMetadataState(this);
  mStateObj.reset(s);
  s->Enter();
}

void MediaDecoderStateMachine::AudioAudibleChanged(bool aAudible) {
  mIsAudioDataAudible = aAudible;
}

MediaSink* MediaDecoderStateMachine::CreateAudioSink() {
  RefPtr<MediaDecoderStateMachine> self = this;
  auto audioSinkCreator = [self]() {
    MOZ_ASSERT(self->OnTaskQueue());
    AudioSink* audioSink =
        new AudioSink(self->mTaskQueue, self->mAudioQueue, self->GetMediaTime(),
                      self->Info().mAudio);

    self->mAudibleListener = audioSink->AudibleEvent().Connect(
        self->mTaskQueue, self.get(),
        &MediaDecoderStateMachine::AudioAudibleChanged);
    return audioSink;
  };
  return new AudioSinkWrapper(mTaskQueue, mAudioQueue, audioSinkCreator);
}

already_AddRefed<MediaSink> MediaDecoderStateMachine::CreateMediaSink(
    bool aAudioCaptured, OutputStreamManager* aManager) {
  MOZ_ASSERT_IF(aAudioCaptured, aManager);
  RefPtr<MediaSink> audioSink =
      aAudioCaptured
          ? new DecodedStream(mTaskQueue, mAbstractMainThread, mAudioQueue,
                              mVideoQueue, aManager, mSameOriginMedia.Ref())
          : CreateAudioSink();

  RefPtr<MediaSink> mediaSink =
      new VideoSink(mTaskQueue, audioSink, mVideoQueue, mVideoFrameContainer,
                    *mFrameStats, sVideoQueueSendToCompositorSize);
  return mediaSink.forget();
}

TimeUnit MediaDecoderStateMachine::GetDecodedAudioDuration() {
  MOZ_ASSERT(OnTaskQueue());
  if (mMediaSink->IsStarted()) {
    // mDecodedAudioEndTime might be smaller than GetClock() when there is
    // overlap between 2 adjacent audio samples or when we are playing
    // a chained ogg file.
    return std::max(mDecodedAudioEndTime - GetClock(), TimeUnit::Zero());
  }
  // MediaSink not started. All audio samples are in the queue.
  return TimeUnit::FromMicroseconds(AudioQueue().Duration());
}

bool MediaDecoderStateMachine::HaveEnoughDecodedAudio() {
  MOZ_ASSERT(OnTaskQueue());
  auto ampleAudio = mAmpleAudioThreshold.MultDouble(mPlaybackRate);
  return AudioQueue().GetSize() > 0 && GetDecodedAudioDuration() >= ampleAudio;
}

bool MediaDecoderStateMachine::HaveEnoughDecodedVideo() {
  MOZ_ASSERT(OnTaskQueue());
  return VideoQueue().GetSize() >= GetAmpleVideoFrames() * mPlaybackRate + 1;
}

void MediaDecoderStateMachine::PushAudio(AudioData* aSample) {
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(aSample);
  AudioQueue().Push(aSample);
}

void MediaDecoderStateMachine::PushVideo(VideoData* aSample) {
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(aSample);
  aSample->mFrameID = ++mCurrentFrameID;
  VideoQueue().Push(aSample);
}

void MediaDecoderStateMachine::OnAudioPopped(const RefPtr<AudioData>& aSample) {
  MOZ_ASSERT(OnTaskQueue());
  mPlaybackOffset = std::max(mPlaybackOffset, aSample->mOffset);
}

void MediaDecoderStateMachine::OnVideoPopped(const RefPtr<VideoData>& aSample) {
  MOZ_ASSERT(OnTaskQueue());
  mPlaybackOffset = std::max(mPlaybackOffset, aSample->mOffset);
}

bool MediaDecoderStateMachine::IsAudioDecoding() {
  MOZ_ASSERT(OnTaskQueue());
  return HasAudio() && !AudioQueue().IsFinished();
}

bool MediaDecoderStateMachine::IsVideoDecoding() {
  MOZ_ASSERT(OnTaskQueue());
  return HasVideo() && !VideoQueue().IsFinished();
}

bool MediaDecoderStateMachine::IsPlaying() const {
  MOZ_ASSERT(OnTaskQueue());
  return mMediaSink->IsPlaying();
}

void MediaDecoderStateMachine::SetMediaNotSeekable() { mMediaSeekable = false; }

nsresult MediaDecoderStateMachine::Init(MediaDecoder* aDecoder) {
  MOZ_ASSERT(NS_IsMainThread());

  // Dispatch initialization that needs to happen on that task queue.
  nsCOMPtr<nsIRunnable> r = NewRunnableMethod<RefPtr<MediaDecoder>>(
      "MediaDecoderStateMachine::InitializationTask", this,
      &MediaDecoderStateMachine::InitializationTask, aDecoder);
  mTaskQueue->DispatchStateChange(r.forget());

  mAudioQueueListener = AudioQueue().PopFrontEvent().Connect(
      mTaskQueue, this, &MediaDecoderStateMachine::OnAudioPopped);
  mVideoQueueListener = VideoQueue().PopFrontEvent().Connect(
      mTaskQueue, this, &MediaDecoderStateMachine::OnVideoPopped);

  mMetadataManager.Connect(mReader->TimedMetadataEvent(), OwnerThread());

  mOnMediaNotSeekable = mReader->OnMediaNotSeekable().Connect(
      OwnerThread(), this, &MediaDecoderStateMachine::SetMediaNotSeekable);

  mMediaSink = CreateMediaSink(mAudioCaptured, mOutputStreamManager);

  nsresult rv = mReader->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  mReader->SetCanonicalDuration(&mDuration);

  return NS_OK;
}

void MediaDecoderStateMachine::StopPlayback() {
  MOZ_ASSERT(OnTaskQueue());
  LOG("StopPlayback()");

  if (IsPlaying()) {
    mOnPlaybackEvent.Notify(MediaPlaybackEvent{
        MediaPlaybackEvent::PlaybackStopped, mPlaybackOffset});
    mMediaSink->SetPlaying(false);
    MOZ_ASSERT(!IsPlaying());
  }
}

void MediaDecoderStateMachine::MaybeStartPlayback() {
  MOZ_ASSERT(OnTaskQueue());
  // Should try to start playback only after decoding first frames.
  MOZ_ASSERT(mSentFirstFrameLoadedEvent);

  if (IsPlaying()) {
    // Logging this case is really spammy - don't do it.
    return;
  }

  if (mPlayState != MediaDecoder::PLAY_STATE_PLAYING) {
    LOG("Not starting playback [mPlayState=%d]", mPlayState.Ref());
    return;
  }

  LOG("MaybeStartPlayback() starting playback");
  StartMediaSink();

  if (!IsPlaying()) {
    mMediaSink->SetPlaying(true);
    MOZ_ASSERT(IsPlaying());
  }

  mOnPlaybackEvent.Notify(
      MediaPlaybackEvent{MediaPlaybackEvent::PlaybackStarted, mPlaybackOffset});
}

void MediaDecoderStateMachine::UpdatePlaybackPositionInternal(
    const TimeUnit& aTime) {
  MOZ_ASSERT(OnTaskQueue());
  LOGV("UpdatePlaybackPositionInternal(%" PRId64 ")", aTime.ToMicroseconds());

  mCurrentPosition = aTime;
  NS_ASSERTION(mCurrentPosition.Ref() >= TimeUnit::Zero(),
               "CurrentTime should be positive!");
  if (mDuration.Ref().ref() < mCurrentPosition.Ref()) {
    mDuration = Some(mCurrentPosition.Ref());
    DDLOG(DDLogCategory::Property, "duration_us",
          mDuration.Ref()->ToMicroseconds());
  }
}

void MediaDecoderStateMachine::UpdatePlaybackPosition(const TimeUnit& aTime) {
  MOZ_ASSERT(OnTaskQueue());
  UpdatePlaybackPositionInternal(aTime);

  bool fragmentEnded =
      mFragmentEndTime.IsValid() && GetMediaTime() >= mFragmentEndTime;
  mMetadataManager.DispatchMetadataIfNeeded(aTime);

  if (fragmentEnded) {
    StopPlayback();
  }
}

/* static */ const char* MediaDecoderStateMachine::ToStateStr(State aState) {
  switch (aState) {
    case DECODER_STATE_DECODING_METADATA:
      return "DECODING_METADATA";
    case DECODER_STATE_DORMANT:
      return "DORMANT";
    case DECODER_STATE_DECODING_FIRSTFRAME:
      return "DECODING_FIRSTFRAME";
    case DECODER_STATE_DECODING:
      return "DECODING";
    case DECODER_STATE_SEEKING:
      return "SEEKING";
    case DECODER_STATE_BUFFERING:
      return "BUFFERING";
    case DECODER_STATE_COMPLETED:
      return "COMPLETED";
    case DECODER_STATE_SHUTDOWN:
      return "SHUTDOWN";
    case DECODER_STATE_LOOPING_DECODING:
      return "LOOPING_DECODING";
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid state.");
  }
  return "UNKNOWN";
}

const char* MediaDecoderStateMachine::ToStateStr() {
  MOZ_ASSERT(OnTaskQueue());
  return ToStateStr(mStateObj->GetState());
}

void MediaDecoderStateMachine::VolumeChanged() {
  MOZ_ASSERT(OnTaskQueue());
  mMediaSink->SetVolume(mVolume);
}

RefPtr<ShutdownPromise> MediaDecoderStateMachine::Shutdown() {
  MOZ_ASSERT(OnTaskQueue());
  return mStateObj->HandleShutdown();
}

void MediaDecoderStateMachine::PlayStateChanged() {
  MOZ_ASSERT(OnTaskQueue());

  if (mPlayState != MediaDecoder::PLAY_STATE_PLAYING) {
    CancelSuspendTimer();
  } else if (mMinimizePreroll) {
    // Once we start playing, we don't want to minimize our prerolling, as we
    // assume the user is likely to want to keep playing in future. This needs
    // to happen before we invoke StartDecoding().
    mMinimizePreroll = false;
  }

  mStateObj->HandlePlayStateChanged(mPlayState);
}

void MediaDecoderStateMachine::SetVideoDecodeMode(VideoDecodeMode aMode) {
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIRunnable> r = NewRunnableMethod<VideoDecodeMode>(
      "MediaDecoderStateMachine::SetVideoDecodeModeInternal", this,
      &MediaDecoderStateMachine::SetVideoDecodeModeInternal, aMode);
  OwnerThread()->DispatchStateChange(r.forget());
}

void MediaDecoderStateMachine::SetVideoDecodeModeInternal(
    VideoDecodeMode aMode) {
  MOZ_ASSERT(OnTaskQueue());

  LOG("SetVideoDecodeModeInternal(), VideoDecodeMode=(%s->%s), "
      "mVideoDecodeSuspended=%c",
      mVideoDecodeMode == VideoDecodeMode::Normal ? "Normal" : "Suspend",
      aMode == VideoDecodeMode::Normal ? "Normal" : "Suspend",
      mVideoDecodeSuspended ? 'T' : 'F');

  // Should not suspend decoding if we don't turn on the pref.
  if (!StaticPrefs::MediaSuspendBkgndVideoEnabled() &&
      aMode == VideoDecodeMode::Suspend) {
    LOG("SetVideoDecodeModeInternal(), early return because preference off and "
        "set to Suspend");
    return;
  }

  if (aMode == mVideoDecodeMode) {
    LOG("SetVideoDecodeModeInternal(), early return because the mode does not "
        "change");
    return;
  }

  // Set new video decode mode.
  mVideoDecodeMode = aMode;

  // Start timer to trigger suspended video decoding.
  if (mVideoDecodeMode == VideoDecodeMode::Suspend) {
    TimeStamp target = TimeStamp::Now() + SuspendBackgroundVideoDelay();

    RefPtr<MediaDecoderStateMachine> self = this;
    mVideoDecodeSuspendTimer.Ensure(
        target, [=]() { self->OnSuspendTimerResolved(); },
        []() { MOZ_DIAGNOSTIC_ASSERT(false); });
    mOnPlaybackEvent.Notify(MediaPlaybackEvent::StartVideoSuspendTimer);
    return;
  }

  // Resuming from suspended decoding

  // If suspend timer exists, destroy it.
  CancelSuspendTimer();

  if (mVideoDecodeSuspended) {
    const auto target = mMediaSink->IsStarted() ? GetClock() : GetMediaTime();
    mStateObj->HandleResumeVideoDecoding(target + detail::RESUME_VIDEO_PREMIUM);
  }
}

void MediaDecoderStateMachine::BufferedRangeUpdated() {
  MOZ_ASSERT(OnTaskQueue());

  // While playing an unseekable stream of unknown duration, mDuration
  // is updated as we play. But if data is being downloaded
  // faster than played, mDuration won't reflect the end of playable data
  // since we haven't played the frame at the end of buffered data. So update
  // mDuration here as new data is downloaded to prevent such a lag.
  if (mBuffered.Ref().IsInvalid()) {
    return;
  }

  bool exists;
  media::TimeUnit end{mBuffered.Ref().GetEnd(&exists)};
  if (!exists) {
    return;
  }

  // Use estimated duration from buffer ranges when mDuration is unknown or
  // the estimated duration is larger.
  if (mDuration.Ref().isNothing() || mDuration.Ref()->IsInfinite() ||
      end > mDuration.Ref().ref()) {
    mDuration = Some(end);
    DDLOG(DDLogCategory::Property, "duration_us",
          mDuration.Ref()->ToMicroseconds());
  }
}

RefPtr<MediaDecoder::SeekPromise> MediaDecoderStateMachine::Seek(
    const SeekTarget& aTarget) {
  MOZ_ASSERT(OnTaskQueue());

  // We need to be able to seek in some way
  if (!mMediaSeekable && !mMediaSeekableOnlyInBufferedRanges) {
    LOGW("Seek() should not be called on a non-seekable media");
    return MediaDecoder::SeekPromise::CreateAndReject(/* aIgnored = */ true,
                                                      __func__);
  }

  if (aTarget.IsNextFrame() && !HasVideo()) {
    LOGW("Ignore a NextFrameSeekTask on a media file without video track.");
    return MediaDecoder::SeekPromise::CreateAndReject(/* aIgnored = */ true,
                                                      __func__);
  }

  MOZ_ASSERT(mDuration.Ref().isSome(), "We should have got duration already");

  return mStateObj->HandleSeek(aTarget);
}

RefPtr<MediaDecoder::SeekPromise> MediaDecoderStateMachine::InvokeSeek(
    const SeekTarget& aTarget) {
  return InvokeAsync(OwnerThread(), this, __func__,
                     &MediaDecoderStateMachine::Seek, aTarget);
}

void MediaDecoderStateMachine::StopMediaSink() {
  MOZ_ASSERT(OnTaskQueue());
  if (mMediaSink->IsStarted()) {
    LOG("Stop MediaSink");
    mAudibleListener.DisconnectIfExists();

    mMediaSink->Stop();
    mMediaSinkAudioEndedPromise.DisconnectIfExists();
    mMediaSinkVideoEndedPromise.DisconnectIfExists();
  }
}

void MediaDecoderStateMachine::RequestAudioData() {
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(IsAudioDecoding());
  MOZ_ASSERT(!IsRequestingAudioData());
  MOZ_ASSERT(!IsWaitingAudioData());
  LOGV("Queueing audio task - queued=%zu, decoder-queued=%zu",
       AudioQueue().GetSize(), mReader->SizeOfAudioQueueInFrames());

  RefPtr<MediaDecoderStateMachine> self = this;
  mReader->RequestAudioData()
      ->Then(
          OwnerThread(), __func__,
          [this, self](RefPtr<AudioData> aAudio) {
            MOZ_ASSERT(aAudio);
            mAudioDataRequest.Complete();
            // audio->GetEndTime() is not always mono-increasing in chained
            // ogg.
            mDecodedAudioEndTime =
                std::max(aAudio->GetEndTime(), mDecodedAudioEndTime);
            LOGV("OnAudioDecoded [%" PRId64 ",%" PRId64 "]",
                 aAudio->mTime.ToMicroseconds(),
                 aAudio->GetEndTime().ToMicroseconds());
            mStateObj->HandleAudioDecoded(aAudio);
          },
          [this, self](const MediaResult& aError) {
            LOGV("OnAudioNotDecoded aError=%s", aError.ErrorName().get());
            mAudioDataRequest.Complete();
            switch (aError.Code()) {
              case NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA:
                mStateObj->HandleWaitingForAudio();
                break;
              case NS_ERROR_DOM_MEDIA_CANCELED:
                mStateObj->HandleAudioCanceled();
                break;
              case NS_ERROR_DOM_MEDIA_END_OF_STREAM:
                mStateObj->HandleEndOfAudio();
                break;
              default:
                DecodeError(aError);
            }
          })
      ->Track(mAudioDataRequest);
}

void MediaDecoderStateMachine::RequestVideoData(
    const media::TimeUnit& aCurrentTime) {
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(IsVideoDecoding());
  MOZ_ASSERT(!IsRequestingVideoData());
  MOZ_ASSERT(!IsWaitingVideoData());
  LOGV(
      "Queueing video task - queued=%zu, decoder-queued=%zo"
      ", stime=%" PRId64,
      VideoQueue().GetSize(), mReader->SizeOfVideoQueueInFrames(),
      aCurrentTime.ToMicroseconds());

  TimeStamp videoDecodeStartTime = TimeStamp::Now();
  RefPtr<MediaDecoderStateMachine> self = this;
  mReader->RequestVideoData(aCurrentTime)
      ->Then(
          OwnerThread(), __func__,
          [this, self, videoDecodeStartTime](RefPtr<VideoData> aVideo) {
            MOZ_ASSERT(aVideo);
            mVideoDataRequest.Complete();
            // Handle abnormal or negative timestamps.
            mDecodedVideoEndTime =
                std::max(mDecodedVideoEndTime, aVideo->GetEndTime());
            LOGV("OnVideoDecoded [%" PRId64 ",%" PRId64 "]",
                 aVideo->mTime.ToMicroseconds(),
                 aVideo->GetEndTime().ToMicroseconds());
            mStateObj->HandleVideoDecoded(aVideo, videoDecodeStartTime);
          },
          [this, self](const MediaResult& aError) {
            LOGV("OnVideoNotDecoded aError=%s", aError.ErrorName().get());
            mVideoDataRequest.Complete();
            switch (aError.Code()) {
              case NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA:
                mStateObj->HandleWaitingForVideo();
                break;
              case NS_ERROR_DOM_MEDIA_CANCELED:
                mStateObj->HandleVideoCanceled();
                break;
              case NS_ERROR_DOM_MEDIA_END_OF_STREAM:
                mStateObj->HandleEndOfVideo();
                break;
              default:
                DecodeError(aError);
            }
          })
      ->Track(mVideoDataRequest);
}

void MediaDecoderStateMachine::WaitForData(MediaData::Type aType) {
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(aType == MediaData::Type::AUDIO_DATA ||
             aType == MediaData::Type::VIDEO_DATA);
  RefPtr<MediaDecoderStateMachine> self = this;
  if (aType == MediaData::Type::AUDIO_DATA) {
    mReader->WaitForData(MediaData::Type::AUDIO_DATA)
        ->Then(
            OwnerThread(), __func__,
            [self](MediaData::Type aType) {
              self->mAudioWaitRequest.Complete();
              MOZ_ASSERT(aType == MediaData::Type::AUDIO_DATA);
              self->mStateObj->HandleAudioWaited(aType);
            },
            [self](const WaitForDataRejectValue& aRejection) {
              self->mAudioWaitRequest.Complete();
              self->DecodeError(NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA);
            })
        ->Track(mAudioWaitRequest);
  } else {
    mReader->WaitForData(MediaData::Type::VIDEO_DATA)
        ->Then(
            OwnerThread(), __func__,
            [self](MediaData::Type aType) {
              self->mVideoWaitRequest.Complete();
              MOZ_ASSERT(aType == MediaData::Type::VIDEO_DATA);
              self->mStateObj->HandleVideoWaited(aType);
            },
            [self](const WaitForDataRejectValue& aRejection) {
              self->mVideoWaitRequest.Complete();
              self->DecodeError(NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA);
            })
        ->Track(mVideoWaitRequest);
  }
}

nsresult MediaDecoderStateMachine::StartMediaSink() {
  MOZ_ASSERT(OnTaskQueue());

  if (mMediaSink->IsStarted()) {
    return NS_OK;
  }

  mAudioCompleted = false;
  nsresult rv = mMediaSink->Start(GetMediaTime(), Info());

  auto videoPromise = mMediaSink->OnEnded(TrackInfo::kVideoTrack);
  auto audioPromise = mMediaSink->OnEnded(TrackInfo::kAudioTrack);

  if (audioPromise) {
    audioPromise
        ->Then(OwnerThread(), __func__, this,
               &MediaDecoderStateMachine::OnMediaSinkAudioComplete,
               &MediaDecoderStateMachine::OnMediaSinkAudioError)
        ->Track(mMediaSinkAudioEndedPromise);
  }
  if (videoPromise) {
    videoPromise
        ->Then(OwnerThread(), __func__, this,
               &MediaDecoderStateMachine::OnMediaSinkVideoComplete,
               &MediaDecoderStateMachine::OnMediaSinkVideoError)
        ->Track(mMediaSinkVideoEndedPromise);
  }
  // Remember the initial offset when playback starts. This will be used
  // to calculate the rate at which bytes are consumed as playback moves on.
  RefPtr<MediaData> sample = mAudioQueue.PeekFront();
  mPlaybackOffset = sample ? sample->mOffset : 0;
  sample = mVideoQueue.PeekFront();
  if (sample && sample->mOffset > mPlaybackOffset) {
    mPlaybackOffset = sample->mOffset;
  }
  return rv;
}

bool MediaDecoderStateMachine::HasLowDecodedAudio() {
  MOZ_ASSERT(OnTaskQueue());
  return IsAudioDecoding() &&
         GetDecodedAudioDuration() <
             EXHAUSTED_DATA_MARGIN.MultDouble(mPlaybackRate);
}

bool MediaDecoderStateMachine::HasLowDecodedVideo() {
  MOZ_ASSERT(OnTaskQueue());
  return IsVideoDecoding() &&
         VideoQueue().GetSize() < LOW_VIDEO_FRAMES * mPlaybackRate;
}

bool MediaDecoderStateMachine::HasLowDecodedData() {
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mReader->UseBufferingHeuristics());
  return HasLowDecodedAudio() || HasLowDecodedVideo();
}

bool MediaDecoderStateMachine::OutOfDecodedAudio() {
  MOZ_ASSERT(OnTaskQueue());
  return IsAudioDecoding() && !AudioQueue().IsFinished() &&
         AudioQueue().GetSize() == 0 &&
         !mMediaSink->HasUnplayedFrames(TrackInfo::kAudioTrack);
}

bool MediaDecoderStateMachine::HasLowBufferedData() {
  MOZ_ASSERT(OnTaskQueue());
  return HasLowBufferedData(detail::LOW_BUFFER_THRESHOLD);
}

bool MediaDecoderStateMachine::HasLowBufferedData(const TimeUnit& aThreshold) {
  MOZ_ASSERT(OnTaskQueue());

  // If we don't have a duration, mBuffered is probably not going to have
  // a useful buffered range. Return false here so that we don't get stuck in
  // buffering mode for live streams.
  if (Duration().IsInfinite()) {
    return false;
  }

  if (mBuffered.Ref().IsInvalid()) {
    return false;
  }

  // We are never low in decoded data when we don't have audio/video or have
  // decoded all audio/video samples.
  TimeUnit endOfDecodedVideo = (HasVideo() && !VideoQueue().IsFinished())
                                   ? mDecodedVideoEndTime
                                   : TimeUnit::FromNegativeInfinity();
  TimeUnit endOfDecodedAudio = (HasAudio() && !AudioQueue().IsFinished())
                                   ? mDecodedAudioEndTime
                                   : TimeUnit::FromNegativeInfinity();

  auto endOfDecodedData = std::max(endOfDecodedVideo, endOfDecodedAudio);
  if (Duration() < endOfDecodedData) {
    // Our duration is not up to date. No point buffering.
    return false;
  }

  if (endOfDecodedData.IsInfinite()) {
    // Have decoded all samples. No point buffering.
    return false;
  }

  auto start = endOfDecodedData;
  auto end = std::min(GetMediaTime() + aThreshold, Duration());
  if (start >= end) {
    // Duration of decoded samples is greater than our threshold.
    return false;
  }
  media::TimeInterval interval(start, end);
  return !mBuffered.Ref().Contains(interval);
}

void MediaDecoderStateMachine::DecodeError(const MediaResult& aError) {
  MOZ_ASSERT(OnTaskQueue());
  LOGE("Decode error: %s", aError.Description().get());
  // Notify the decode error and MediaDecoder will shut down MDSM.
  mOnPlaybackErrorEvent.Notify(aError);
}

void MediaDecoderStateMachine::EnqueueFirstFrameLoadedEvent() {
  MOZ_ASSERT(OnTaskQueue());
  // Track value of mSentFirstFrameLoadedEvent from before updating it
  bool firstFrameBeenLoaded = mSentFirstFrameLoadedEvent;
  mSentFirstFrameLoadedEvent = true;
  MediaDecoderEventVisibility visibility =
      firstFrameBeenLoaded ? MediaDecoderEventVisibility::Suppressed
                           : MediaDecoderEventVisibility::Observable;
  mFirstFrameLoadedEvent.Notify(nsAutoPtr<MediaInfo>(new MediaInfo(Info())),
                                visibility);
}

void MediaDecoderStateMachine::FinishDecodeFirstFrame() {
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(!mSentFirstFrameLoadedEvent);
  LOG("FinishDecodeFirstFrame");

  mMediaSink->Redraw(Info().mVideo);

  LOG("Media duration %" PRId64 ", mediaSeekable=%d",
      Duration().ToMicroseconds(), mMediaSeekable);

  // Get potentially updated metadata
  mReader->ReadUpdatedMetadata(mInfo.ptr());

  EnqueueFirstFrameLoadedEvent();
}

RefPtr<ShutdownPromise> MediaDecoderStateMachine::BeginShutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mOutputStreamManager) {
    mOutputStreamManager->Disconnect();
    mNextOutputStreamTrackID = mOutputStreamManager->NextTrackID();
  }
  return InvokeAsync(OwnerThread(), this, __func__,
                     &MediaDecoderStateMachine::Shutdown);
}

RefPtr<ShutdownPromise> MediaDecoderStateMachine::FinishShutdown() {
  MOZ_ASSERT(OnTaskQueue());
  LOG("Shutting down state machine task queue");
  return OwnerThread()->BeginShutdown();
}

void MediaDecoderStateMachine::RunStateMachine() {
  MOZ_ASSERT(OnTaskQueue());

  mDelayedScheduler.Reset();  // Must happen on state machine task queue.
  mDispatchedStateMachine = false;
  mStateObj->Step();
}

void MediaDecoderStateMachine::ResetDecode(TrackSet aTracks) {
  MOZ_ASSERT(OnTaskQueue());
  LOG("MediaDecoderStateMachine::Reset");

  // Assert that aTracks specifies to reset the video track because we
  // don't currently support resetting just the audio track.
  MOZ_ASSERT(aTracks.contains(TrackInfo::kVideoTrack));

  if (aTracks.contains(TrackInfo::kVideoTrack)) {
    mDecodedVideoEndTime = TimeUnit::Zero();
    mVideoCompleted = false;
    VideoQueue().Reset();
    mVideoDataRequest.DisconnectIfExists();
    mVideoWaitRequest.DisconnectIfExists();
  }

  if (aTracks.contains(TrackInfo::kAudioTrack)) {
    mDecodedAudioEndTime = TimeUnit::Zero();
    mAudioCompleted = false;
    AudioQueue().Reset();
    mAudioDataRequest.DisconnectIfExists();
    mAudioWaitRequest.DisconnectIfExists();
  }

  mReader->ResetDecode(aTracks);
}

media::TimeUnit MediaDecoderStateMachine::GetClock(
    TimeStamp* aTimeStamp) const {
  MOZ_ASSERT(OnTaskQueue());
  auto clockTime = mMediaSink->GetPosition(aTimeStamp);
  NS_ASSERTION(GetMediaTime() <= clockTime, "Clock should go forwards.");
  return clockTime;
}

void MediaDecoderStateMachine::UpdatePlaybackPositionPeriodically() {
  MOZ_ASSERT(OnTaskQueue());

  if (!IsPlaying()) {
    return;
  }

  // Cap the current time to the larger of the audio and video end time.
  // This ensures that if we're running off the system clock, we don't
  // advance the clock to after the media end time.
  if (VideoEndTime() > TimeUnit::Zero() || AudioEndTime() > TimeUnit::Zero()) {
    auto clockTime = GetClock();

    // Once looping was turned on, the time is probably larger than the duration
    // of the media track, so the time over the end should be corrected.
    AdjustByLooping(clockTime);
    bool loopback = clockTime < GetMediaTime() && mLooping;

    // Skip frames up to the frame at the playback position, and figure out
    // the time remaining until it's time to display the next frame and drop
    // the current frame.
    NS_ASSERTION(clockTime >= TimeUnit::Zero(),
                 "Should have positive clock time.");

    // These will be non -1 if we've displayed a video frame, or played an audio
    // frame.
    auto maxEndTime = std::max(VideoEndTime(), AudioEndTime());
    auto t = std::min(clockTime, maxEndTime);
    // FIXME: Bug 1091422 - chained ogg files hit this assertion.
    // MOZ_ASSERT(t >= GetMediaTime());
    if (loopback || t > GetMediaTime()) {
      UpdatePlaybackPosition(t);
    }
  }
  // Note we have to update playback position before releasing the monitor.
  // Otherwise, MediaDecoder::AddOutputStream could kick in when we are outside
  // the monitor and get a staled value from GetCurrentTimeUs() which hits the
  // assertion in GetClock().

  int64_t delay = std::max<int64_t>(1, AUDIO_DURATION_USECS / mPlaybackRate);
  ScheduleStateMachineIn(TimeUnit::FromMicroseconds(delay));

  // Notify the listener as we progress in the playback offset. Note it would
  // be too intensive to send notifications for each popped audio/video sample.
  // It is good enough to send 'PlaybackProgressed' events every 40us (defined
  // by AUDIO_DURATION_USECS), and we ensure 'PlaybackProgressed' events are
  // always sent after 'PlaybackStarted' and before 'PlaybackStopped'.
  mOnPlaybackEvent.Notify(MediaPlaybackEvent{
      MediaPlaybackEvent::PlaybackProgressed, mPlaybackOffset});
}

void MediaDecoderStateMachine::ScheduleStateMachine() {
  MOZ_ASSERT(OnTaskQueue());
  if (mDispatchedStateMachine) {
    return;
  }
  mDispatchedStateMachine = true;

  nsresult rv = OwnerThread()->Dispatch(
      NewRunnableMethod("MediaDecoderStateMachine::RunStateMachine", this,
                        &MediaDecoderStateMachine::RunStateMachine));
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  Unused << rv;
}

void MediaDecoderStateMachine::ScheduleStateMachineIn(const TimeUnit& aTime) {
  MOZ_ASSERT(OnTaskQueue());  // mDelayedScheduler.Ensure() may Disconnect()
                              // the promise, which must happen on the state
                              // machine task queue.
  MOZ_ASSERT(aTime > TimeUnit::Zero());
  if (mDispatchedStateMachine) {
    return;
  }

  TimeStamp target = TimeStamp::Now() + aTime.ToTimeDuration();

  // It is OK to capture 'this' without causing UAF because the callback
  // always happens before shutdown.
  RefPtr<MediaDecoderStateMachine> self = this;
  mDelayedScheduler.Ensure(
      target,
      [self]() {
        self->mDelayedScheduler.CompleteRequest();
        self->RunStateMachine();
      },
      []() { MOZ_DIAGNOSTIC_ASSERT(false); });
}

bool MediaDecoderStateMachine::OnTaskQueue() const {
  return OwnerThread()->IsCurrentThreadIn();
}

bool MediaDecoderStateMachine::IsStateMachineScheduled() const {
  MOZ_ASSERT(OnTaskQueue());
  return mDispatchedStateMachine || mDelayedScheduler.IsScheduled();
}

void MediaDecoderStateMachine::SetPlaybackRate(double aPlaybackRate) {
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(aPlaybackRate != 0, "Should be handled by MediaDecoder::Pause()");

  mPlaybackRate = aPlaybackRate;
  mMediaSink->SetPlaybackRate(mPlaybackRate);

  // Schedule next cycle to check if we can stop prerolling.
  ScheduleStateMachine();
}

void MediaDecoderStateMachine::PreservesPitchChanged() {
  MOZ_ASSERT(OnTaskQueue());
  mMediaSink->SetPreservesPitch(mPreservesPitch);
}

void MediaDecoderStateMachine::LoopingChanged() {
  MOZ_ASSERT(OnTaskQueue());
  LOGV("LoopingChanged, looping=%d", mLooping.Ref());
  if (mSeamlessLoopingAllowed) {
    mStateObj->HandleLoopingChanged();
  }
}

RefPtr<GenericPromise> MediaDecoderStateMachine::InvokeSetSink(
    RefPtr<AudioDeviceInfo> aSink) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSink);

  Unused << ++mSetSinkRequestsCount;
  return InvokeAsync(OwnerThread(), this, __func__,
                     &MediaDecoderStateMachine::SetSink, aSink);
}

RefPtr<GenericPromise> MediaDecoderStateMachine::SetSink(
    RefPtr<AudioDeviceInfo> aSink) {
  MOZ_ASSERT(OnTaskQueue());
  if (mAudioCaptured) {
    // Not supported yet.
    return GenericPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
  }

  // Backup current playback parameters.
  bool wasPlaying = mMediaSink->IsPlaying();

  if (--mSetSinkRequestsCount > 0) {
    MOZ_ASSERT(mSetSinkRequestsCount > 0);
    return GenericPromise::CreateAndResolve(wasPlaying, __func__);
  }

  MediaSink::PlaybackParams params = mMediaSink->GetPlaybackParams();
  params.mSink = std::move(aSink);

  if (!mMediaSink->IsStarted()) {
    mMediaSink->SetPlaybackParams(params);
    return GenericPromise::CreateAndResolve(false, __func__);
  }

  // Stop and shutdown the existing sink.
  StopMediaSink();
  mMediaSink->Shutdown();
  // Create a new sink according to whether audio is captured.
  mMediaSink = CreateMediaSink(false);
  // Restore playback parameters.
  mMediaSink->SetPlaybackParams(params);
  // Start the new sink
  if (wasPlaying) {
    nsresult rv = StartMediaSink();
    if (NS_FAILED(rv)) {
      return GenericPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
    }
  }
  return GenericPromise::CreateAndResolve(wasPlaying, __func__);
}

void MediaDecoderStateMachine::SetSecondaryVideoContainer(
    const RefPtr<VideoFrameContainer>& aSecondary) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<MediaDecoderStateMachine> self = this;
  Unused << InvokeAsync(OwnerThread(), __func__, [self, aSecondary]() {
    self->mMediaSink->SetSecondaryVideoContainer(aSecondary);
    return GenericPromise::CreateAndResolve(true, __func__);
  });
}

TimeUnit MediaDecoderStateMachine::AudioEndTime() const {
  MOZ_ASSERT(OnTaskQueue());
  if (mMediaSink->IsStarted()) {
    return mMediaSink->GetEndTime(TrackInfo::kAudioTrack);
  }
  return GetMediaTime();
}

TimeUnit MediaDecoderStateMachine::VideoEndTime() const {
  MOZ_ASSERT(OnTaskQueue());
  if (mMediaSink->IsStarted()) {
    return mMediaSink->GetEndTime(TrackInfo::kVideoTrack);
  }
  return GetMediaTime();
}

void MediaDecoderStateMachine::OnMediaSinkVideoComplete() {
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(HasVideo());
  LOG("[%s]", __func__);

  mMediaSinkVideoEndedPromise.Complete();
  mVideoCompleted = true;
  ScheduleStateMachine();
}

void MediaDecoderStateMachine::OnMediaSinkVideoError() {
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(HasVideo());
  LOGE("[%s]", __func__);

  mMediaSinkVideoEndedPromise.Complete();
  mVideoCompleted = true;
  if (HasAudio()) {
    return;
  }
  DecodeError(MediaResult(NS_ERROR_DOM_MEDIA_MEDIASINK_ERR, __func__));
}

void MediaDecoderStateMachine::OnMediaSinkAudioComplete() {
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(HasAudio());
  LOG("[%s]", __func__);

  mMediaSinkAudioEndedPromise.Complete();
  mAudioCompleted = true;
  // To notify PlaybackEnded as soon as possible.
  ScheduleStateMachine();

  // Report OK to Decoder Doctor (to know if issue may have been resolved).
  mOnDecoderDoctorEvent.Notify(
      DecoderDoctorEvent{DecoderDoctorEvent::eAudioSinkStartup, NS_OK});
}

void MediaDecoderStateMachine::OnMediaSinkAudioError(nsresult aResult) {
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(HasAudio());
  LOGE("[%s]", __func__);

  mMediaSinkAudioEndedPromise.Complete();
  mAudioCompleted = true;

  // Result should never be NS_OK in this *error* handler. Report to Dec-Doc.
  MOZ_ASSERT(NS_FAILED(aResult));
  mOnDecoderDoctorEvent.Notify(
      DecoderDoctorEvent{DecoderDoctorEvent::eAudioSinkStartup, aResult});

  // Make the best effort to continue playback when there is video.
  if (HasVideo()) {
    return;
  }

  // Otherwise notify media decoder/element about this error for it makes
  // no sense to play an audio-only file without sound output.
  DecodeError(MediaResult(NS_ERROR_DOM_MEDIA_MEDIASINK_ERR, __func__));
}

void MediaDecoderStateMachine::SetAudioCaptured(bool aCaptured,
                                                OutputStreamManager* aManager) {
  MOZ_ASSERT(OnTaskQueue());

  if (aCaptured == mAudioCaptured) {
    return;
  }

  // Rest these flags so they are consistent with the status of the sink.
  // TODO: Move these flags into MediaSink to improve cohesion so we don't need
  // to reset these flags when switching MediaSinks.
  mAudioCompleted = false;
  mVideoCompleted = false;

  // Backup current playback parameters.
  MediaSink::PlaybackParams params = mMediaSink->GetPlaybackParams();

  // Stop and shut down the existing sink.
  StopMediaSink();
  mMediaSink->Shutdown();

  // Create a new sink according to whether audio is captured.
  mMediaSink = CreateMediaSink(aCaptured, aManager);

  // Restore playback parameters.
  mMediaSink->SetPlaybackParams(params);

  mAudioCaptured = aCaptured;

  // Don't buffer as much when audio is captured because we don't need to worry
  // about high latency audio devices.
  mAmpleAudioThreshold = mAudioCaptured ? detail::AMPLE_AUDIO_THRESHOLD / 2
                                        : detail::AMPLE_AUDIO_THRESHOLD;

  mStateObj->HandleAudioCaptured();
}

uint32_t MediaDecoderStateMachine::GetAmpleVideoFrames() const {
  MOZ_ASSERT(OnTaskQueue());
  return mReader->VideoIsHardwareAccelerated()
             ? std::max<uint32_t>(sVideoQueueHWAccelSize, MIN_VIDEO_QUEUE_SIZE)
             : std::max<uint32_t>(sVideoQueueDefaultSize, MIN_VIDEO_QUEUE_SIZE);
}

nsCString MediaDecoderStateMachine::GetDebugInfo() {
  MOZ_ASSERT(OnTaskQueue());
  int64_t duration =
      mDuration.Ref() ? mDuration.Ref().ref().ToMicroseconds() : -1;
  auto str = nsPrintfCString(
      "MDSM: duration=%" PRId64 " GetMediaTime=%" PRId64
      " GetClock="
      "%" PRId64
      " mMediaSink=%p state=%s mPlayState=%d "
      "mSentFirstFrameLoadedEvent=%d IsPlaying=%d mAudioStatus=%s "
      "mVideoStatus=%s mDecodedAudioEndTime=%" PRId64
      " mDecodedVideoEndTime=%" PRId64
      " mAudioCompleted=%d "
      "mVideoCompleted=%d %s",
      duration, GetMediaTime().ToMicroseconds(),
      mMediaSink->IsStarted() ? GetClock().ToMicroseconds() : -1,
      mMediaSink.get(), ToStateStr(), mPlayState.Ref(),
      mSentFirstFrameLoadedEvent, IsPlaying(), AudioRequestStatus(),
      VideoRequestStatus(), mDecodedAudioEndTime.ToMicroseconds(),
      mDecodedVideoEndTime.ToMicroseconds(), mAudioCompleted, mVideoCompleted,
      mStateObj->GetDebugInfo().get());

  AppendStringIfNotEmpty(str, mMediaSink->GetDebugInfo());

  return std::move(str);
}

RefPtr<MediaDecoder::DebugInfoPromise>
MediaDecoderStateMachine::RequestDebugInfo() {
  using PromiseType = MediaDecoder::DebugInfoPromise;
  RefPtr<PromiseType::Private> p = new PromiseType::Private(__func__);
  RefPtr<MediaDecoderStateMachine> self = this;
  nsresult rv = OwnerThread()->Dispatch(
      NS_NewRunnableFunction(
          "MediaDecoderStateMachine::RequestDebugInfo",
          [self, p]() { p->Resolve(self->GetDebugInfo(), __func__); }),
      AbstractThread::TailDispatch);
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  Unused << rv;
  return p.forget();
}

void MediaDecoderStateMachine::SetOutputStreamPrincipal(
    const nsCOMPtr<nsIPrincipal>& aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  mOutputStreamPrincipal = aPrincipal;
  if (mOutputStreamManager) {
    mOutputStreamManager->SetPrincipal(mOutputStreamPrincipal);
  }
}

void MediaDecoderStateMachine::SetOutputStreamCORSMode(CORSMode aCORSMode) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOutputStreamCORSMode == CORS_NONE);
  MOZ_ASSERT(!mOutputStreamManager);
  mOutputStreamCORSMode = aCORSMode;
}

void MediaDecoderStateMachine::AddOutputStream(DOMMediaStream* aStream) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("AddOutputStream aStream=%p!", aStream);
  mOutputStreamManager->Add(aStream);
  nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction("MediaDecoderStateMachine::SetAudioCaptured",
                             [self = RefPtr<MediaDecoderStateMachine>(this),
                              manager = mOutputStreamManager]() {
                               self->SetAudioCaptured(true, manager);
                             });
  nsresult rv = OwnerThread()->Dispatch(r.forget());
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  Unused << rv;
}

void MediaDecoderStateMachine::RemoveOutputStream(DOMMediaStream* aStream) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("RemoveOutputStream=%p!", aStream);
  mOutputStreamManager->Remove(aStream);
  if (mOutputStreamManager->IsEmpty()) {
    mOutputStreamManager->Disconnect();
    mOutputStreamManager = nullptr;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
        "MediaDecoderStateMachine::SetAudioCaptured",
        [self = RefPtr<MediaDecoderStateMachine>(this)]() {
          self->SetAudioCaptured(false);
        });
    nsresult rv = OwnerThread()->Dispatch(r.forget());
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }
}

void MediaDecoderStateMachine::EnsureOutputStreamManager(
    MediaStreamGraph* aGraph) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mOutputStreamManager) {
    return;
  }
  mOutputStreamManager = new OutputStreamManager(
      aGraph->CreateSourceStream(), mNextOutputStreamTrackID,
      mOutputStreamPrincipal, mOutputStreamCORSMode, mAbstractMainThread);
}

void MediaDecoderStateMachine::EnsureOutputStreamManagerHasTracks(
    const MediaInfo& aLoadedInfo) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mOutputStreamManager) {
    return;
  }
  if ((!aLoadedInfo.HasAudio() ||
       mOutputStreamManager->HasTrackType(MediaSegment::AUDIO)) &&
      (!aLoadedInfo.HasVideo() ||
       mOutputStreamManager->HasTrackType(MediaSegment::VIDEO))) {
    return;
  }
  if (aLoadedInfo.HasAudio()) {
    MOZ_ASSERT(!mOutputStreamManager->HasTrackType(MediaSegment::AUDIO));
    mOutputStreamManager->AddTrack(MediaSegment::AUDIO);
    LOG("Pre-created audio track with id %d",
        mOutputStreamManager->GetLiveTrackIDFor(MediaSegment::AUDIO));
  }
  if (aLoadedInfo.HasVideo()) {
    MOZ_ASSERT(!mOutputStreamManager->HasTrackType(MediaSegment::VIDEO));
    mOutputStreamManager->AddTrack(MediaSegment::VIDEO);
    LOG("Pre-created video track with id %d",
        mOutputStreamManager->GetLiveTrackIDFor(MediaSegment::VIDEO));
  }
}

void MediaDecoderStateMachine::SetNextOutputStreamTrackID(
    TrackID aNextTrackID) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("SetNextOutputStreamTrackID aNextTrackID=%d", aNextTrackID);
  mNextOutputStreamTrackID = aNextTrackID;
}

TrackID MediaDecoderStateMachine::GetNextOutputStreamTrackID() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mOutputStreamManager) {
    return mOutputStreamManager->NextTrackID();
  }
  return mNextOutputStreamTrackID;
}

class VideoQueueMemoryFunctor : public nsDequeFunctor {
 public:
  VideoQueueMemoryFunctor() : mSize(0) {}

  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf);

  virtual void operator()(void* aObject) override {
    const VideoData* v = static_cast<const VideoData*>(aObject);
    mSize += v->SizeOfIncludingThis(MallocSizeOf);
  }

  size_t mSize;
};

class AudioQueueMemoryFunctor : public nsDequeFunctor {
 public:
  AudioQueueMemoryFunctor() : mSize(0) {}

  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf);

  virtual void operator()(void* aObject) override {
    const AudioData* audioData = static_cast<const AudioData*>(aObject);
    mSize += audioData->SizeOfIncludingThis(MallocSizeOf);
  }

  size_t mSize;
};

size_t MediaDecoderStateMachine::SizeOfVideoQueue() const {
  VideoQueueMemoryFunctor functor;
  mVideoQueue.LockedForEach(functor);
  return functor.mSize;
}

size_t MediaDecoderStateMachine::SizeOfAudioQueue() const {
  AudioQueueMemoryFunctor functor;
  mAudioQueue.LockedForEach(functor);
  return functor.mSize;
}

AbstractCanonical<media::TimeIntervals>*
MediaDecoderStateMachine::CanonicalBuffered() const {
  return mReader->CanonicalBuffered();
}

MediaEventSource<void>& MediaDecoderStateMachine::OnMediaNotSeekable() const {
  return mReader->OnMediaNotSeekable();
}

const char* MediaDecoderStateMachine::AudioRequestStatus() const {
  MOZ_ASSERT(OnTaskQueue());
  if (IsRequestingAudioData()) {
    MOZ_DIAGNOSTIC_ASSERT(!IsWaitingAudioData());
    return "pending";
  } else if (IsWaitingAudioData()) {
    return "waiting";
  }
  return "idle";
}

const char* MediaDecoderStateMachine::VideoRequestStatus() const {
  MOZ_ASSERT(OnTaskQueue());
  if (IsRequestingVideoData()) {
    MOZ_DIAGNOSTIC_ASSERT(!IsWaitingVideoData());
    return "pending";
  } else if (IsWaitingVideoData()) {
    return "waiting";
  }
  return "idle";
}

void MediaDecoderStateMachine::OnSuspendTimerResolved() {
  LOG("OnSuspendTimerResolved");
  mVideoDecodeSuspendTimer.CompleteRequest();
  mStateObj->HandleVideoSuspendTimeout();
}

void MediaDecoderStateMachine::CancelSuspendTimer() {
  LOG("CancelSuspendTimer: State: %s, Timer.IsScheduled: %c",
      ToStateStr(mStateObj->GetState()),
      mVideoDecodeSuspendTimer.IsScheduled() ? 'T' : 'F');
  MOZ_ASSERT(OnTaskQueue());
  if (mVideoDecodeSuspendTimer.IsScheduled()) {
    mOnPlaybackEvent.Notify(MediaPlaybackEvent::CancelVideoSuspendTimer);
  }
  mVideoDecodeSuspendTimer.Reset();
}

void MediaDecoderStateMachine::AdjustByLooping(media::TimeUnit& aTime) const {
  MOZ_ASSERT(OnTaskQueue());
  if (mAudioDecodedDuration.isSome() &&
      mAudioDecodedDuration.ref().IsPositive()) {
    aTime = aTime % mAudioDecodedDuration.ref();
  }
}

bool MediaDecoderStateMachine::IsInSeamlessLooping() const {
  return mLooping && mSeamlessLoopingAllowed;
}

}  // namespace mozilla

// avoid redefined macro in unified build
#undef LOG
#undef LOGV
#undef LOGW
#undef LOGE
#undef SLOGW
#undef SLOGE
#undef NS_DispatchToMainThread
