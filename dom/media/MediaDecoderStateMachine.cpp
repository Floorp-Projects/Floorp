/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef XP_WIN
// Include Windows headers required for enabling high precision timers.
#include "windows.h"
#include "mmsystem.h"
#endif

#include <algorithm>
#include <stdint.h>

#include "gfx2DGlue.h"

#include "mediasink/AudioSink.h"
#include "mediasink/AudioSinkWrapper.h"
#include "mediasink/DecodedStream.h"
#include "mediasink/OutputStreamManager.h"
#include "mediasink/VideoSink.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/IndexSequence.h"
#include "mozilla/Logging.h"
#include "mozilla/mozalloc.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Preferences.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/SizePrintfMacros.h"
#include "mozilla/Sprintf.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/Tuple.h"

#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIEventTarget.h"
#include "nsITimer.h"
#include "nsPrintfCString.h"
#include "nsTArray.h"
#include "nsDeque.h"
#include "prenv.h"

#include "AudioSegment.h"
#include "DOMMediaStream.h"
#include "ImageContainer.h"
#include "MediaDecoder.h"
#include "MediaDecoderReader.h"
#include "MediaDecoderReaderWrapper.h"
#include "MediaDecoderStateMachine.h"
#include "MediaShutdownManager.h"
#include "MediaPrefs.h"
#include "MediaTimer.h"
#include "TimeUnits.h"
#include "VideoSegment.h"
#include "VideoUtils.h"
#include "gfxPrefs.h"

namespace mozilla {

using namespace mozilla::dom;
using namespace mozilla::layers;
using namespace mozilla::media;

#define NS_DispatchToMainThread(...) CompileError_UseAbstractThreadDispatchInstead

// avoid redefined macro in unified build
#undef FMT
#undef LOG
#undef LOGV
#undef LOGW
#undef SFMT
#undef SLOG
#undef SLOGW

#define FMT(x, ...) "Decoder=%p " x, mDecoderID, ##__VA_ARGS__
#define LOG(x, ...) MOZ_LOG(gMediaDecoderLog, LogLevel::Debug,   (FMT(x, ##__VA_ARGS__)))
#define LOGV(x, ...) MOZ_LOG(gMediaDecoderLog, LogLevel::Verbose, (FMT(x, ##__VA_ARGS__)))
#define LOGW(x, ...) NS_WARNING(nsPrintfCString(FMT(x, ##__VA_ARGS__)).get())

// Used by StateObject and its sub-classes
#define SFMT(x, ...) "Decoder=%p state=%s " x, mMaster->mDecoderID, ToStateStr(GetState()), ##__VA_ARGS__
#define SLOG(x, ...) MOZ_LOG(gMediaDecoderLog, LogLevel::Debug, (SFMT(x, ##__VA_ARGS__)))
#define SLOGW(x, ...) NS_WARNING(nsPrintfCString(SFMT(x, ##__VA_ARGS__)).get())

// Certain constants get stored as member variables and then adjusted by various
// scale factors on a per-decoder basis. We want to make sure to avoid using these
// constants directly, so we put them in a namespace.
namespace detail {

// If audio queue has less than this much decoded audio, we won't risk
// trying to decode the video, we'll skip decoding video up to the next
// keyframe. We may increase this value for an individual decoder if we
// encounter video frames which take a long time to decode.
static constexpr auto LOW_AUDIO_THRESHOLD = TimeUnit::FromMicroseconds(300000);

static const int64_t AMPLE_AUDIO_USECS = 2000000;

// If more than this much decoded audio is queued, we'll hold off
// decoding more audio. If we increase the low audio threshold (see
// LOW_AUDIO_THRESHOLD above) we'll also increase this value to ensure it's not
// less than the low audio threshold.
static constexpr auto AMPLE_AUDIO_THRESHOLD = TimeUnit::FromMicroseconds(AMPLE_AUDIO_USECS);

} // namespace detail

// If we have fewer than LOW_VIDEO_FRAMES decoded frames, and
// we're not "prerolling video", we'll skip the video up to the next keyframe
// which is at or after the current playback position.
static const uint32_t LOW_VIDEO_FRAMES = 2;

// Threshold that used to check if we are low on decoded video.
// If the last video frame's end time |mDecodedVideoEndTime| is more than
// |LOW_VIDEO_THRESHOLD*mPlaybackRate| after the current clock in
// Advanceframe(), the video decode is lagging, and we skip to next keyframe.
static constexpr auto LOW_VIDEO_THRESHOLD = TimeUnit::FromMicroseconds(60000);

// Arbitrary "frame duration" when playing only audio.
static const int AUDIO_DURATION_USECS = 40000;

// If we increase our "low audio threshold" (see LOW_AUDIO_THRESHOLD above), we
// use this as a factor in all our calculations. Increasing this will cause
// us to be more likely to increase our low audio threshold, and to
// increase it by more.
static const int THRESHOLD_FACTOR = 2;

namespace detail {

// If we have less than this much buffered data available, we'll consider
// ourselves to be running low on buffered data. We determine how much
// buffered data we have remaining using the reader's GetBuffered()
// implementation.
static const int64_t LOW_BUFFER_THRESHOLD_USECS = 5000000;

static constexpr auto LOW_BUFFER_THRESHOLD = TimeUnit::FromMicroseconds(LOW_BUFFER_THRESHOLD_USECS);

// LOW_BUFFER_THRESHOLD_USECS needs to be greater than AMPLE_AUDIO_USECS, otherwise
// the skip-to-keyframe logic can activate when we're running low on data.
static_assert(LOW_BUFFER_THRESHOLD_USECS > AMPLE_AUDIO_USECS,
              "LOW_BUFFER_THRESHOLD_USECS is too small");

} // namespace detail

// Amount of excess data to add in to the "should we buffer" calculation.
static constexpr auto EXHAUSTED_DATA_MARGIN = TimeUnit::FromMicroseconds(100000);

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

static void InitVideoQueuePrefs()
{
  MOZ_ASSERT(NS_IsMainThread());
  static bool sPrefInit = false;
  if (!sPrefInit) {
    sPrefInit = true;
    sVideoQueueDefaultSize = Preferences::GetUint(
      "media.video-queue.default-size", MAX_VIDEO_QUEUE_SIZE);
    sVideoQueueHWAccelSize = Preferences::GetUint(
      "media.video-queue.hw-accel-size", HW_VIDEO_QUEUE_SIZE);
    sVideoQueueSendToCompositorSize = Preferences::GetUint(
      "media.video-queue.send-to-compositor-size", VIDEO_QUEUE_SEND_TO_COMPOSITOR_SIZE);
  }
}

// Delay, in milliseconds, that tabs needs to be in background before video
// decoding is suspended.
static TimeDuration
SuspendBackgroundVideoDelay()
{
  return TimeDuration::FromMilliseconds(
    MediaPrefs::MDSMSuspendBackgroundVideoDelay());
}

class MediaDecoderStateMachine::StateObject
{
public:
  virtual ~StateObject() { }
  virtual void Exit() { }   // Exit action.
  virtual void Step() { }   // Perform a 'cycle' of this state object.
  virtual State GetState() const = 0;

  // Event handlers for various events.
  virtual void HandleCDMProxyReady() { }
  virtual void HandleAudioCaptured() { }
  virtual void HandleAudioDecoded(AudioData* aAudio)
  {
    Crash("Unexpected event!", __func__);
  }
  virtual void HandleVideoDecoded(VideoData* aVideo, TimeStamp aDecodeStart)
  {
    Crash("Unexpected event!", __func__);
  }
  virtual void HandleAudioWaited(MediaData::Type aType)
  {
    Crash("Unexpected event!", __func__);
  }
  virtual void HandleVideoWaited(MediaData::Type aType)
  {
    Crash("Unexpected event!", __func__);
  }
  virtual void HandleWaitingForAudio()
  {
    Crash("Unexpected event!", __func__);
  }
  virtual void HandleAudioCanceled()
  {
    Crash("Unexpected event!", __func__);
  }
  virtual void HandleEndOfAudio()
  {
    Crash("Unexpected event!", __func__);
  }
  virtual void HandleWaitingForVideo()
  {
    Crash("Unexpected event!", __func__);
  }
  virtual void HandleVideoCanceled()
  {
    Crash("Unexpected event!", __func__);
  }
  virtual void HandleEndOfVideo()
  {
    Crash("Unexpected event!", __func__);
  }

  virtual RefPtr<MediaDecoder::SeekPromise> HandleSeek(SeekTarget aTarget);

  virtual RefPtr<ShutdownPromise> HandleShutdown();

  virtual void HandleVideoSuspendTimeout() = 0;

  virtual void HandleResumeVideoDecoding(const TimeUnit& aTarget);

  virtual void HandlePlayStateChanged(MediaDecoder::PlayState aPlayState) { }

  virtual nsCString GetDebugInfo() { return nsCString(); }

private:
  template <class S, typename R, typename... As>
  auto ReturnTypeHelper(R(S::*)(As...)) -> R;

  void Crash(const char* aReason, const char* aSite)
  {
    char buf[1024];
    SprintfLiteral(buf, "%s state=%s callsite=%s", aReason,
                   ToStateStr(GetState()), aSite);
    MOZ_ReportAssertionFailure(buf, __FILE__, __LINE__);
    MOZ_CRASH();
  }

protected:
  enum class EventVisibility : int8_t
  {
    Observable,
    Suppressed
  };

  using Master = MediaDecoderStateMachine;
  explicit StateObject(Master* aPtr) : mMaster(aPtr) { }
  TaskQueue* OwnerThread() const { return mMaster->mTaskQueue; }
  MediaResource* Resource() const { return mMaster->mResource; }
  MediaDecoderReaderWrapper* Reader() const { return mMaster->mReader; }
  const MediaInfo& Info() const { return mMaster->Info(); }
  bool IsExpectingMoreData() const
  {
    // We are expecting more data if either the resource states so, or if we
    // have a waiting promise pending (such as with non-MSE EME).
    return Resource()->IsExpectingMoreData()
           || mMaster->IsWaitingAudioData()
           || mMaster->IsWaitingVideoData();
  }
  MediaQueue<AudioData>& AudioQueue() const { return mMaster->mAudioQueue; }
  MediaQueue<VideoData>& VideoQueue() const { return mMaster->mVideoQueue; }

  template <class S, typename... Args, size_t... Indexes>
  auto
  CallEnterMemberFunction(S* aS,
                          Tuple<Args...>& aTuple,
                          IndexSequence<Indexes...>)
    -> decltype(ReturnTypeHelper(&S::Enter))
  {
    return aS->Enter(Move(Get<Indexes>(aTuple))...);
  }

  // Note this function will delete the current state object.
  // Don't access members to avoid UAF after this call.
  template <class S, typename... Ts>
  auto SetState(Ts&&... aArgs)
    -> decltype(ReturnTypeHelper(&S::Enter))
  {
    // |aArgs| must be passed by reference to avoid passing MOZ_NON_PARAM class
    // SeekJob by value.  See bug 1287006 and bug 1338374.  But we still *must*
    // copy the parameters, because |Exit()| can modify them.  See bug 1312321.
    // So we 1) pass the parameters by reference, but then 2) immediately copy
    // them into a Tuple to be safe against modification, and finally 3) move
    // the elements of the Tuple into the final function call.
    auto copiedArgs = MakeTuple(Forward<Ts>(aArgs)...);

    // keep mMaster in a local object because mMaster will become invalid after
    // the current state object is deleted.
    auto master = mMaster;

    auto* s = new S(master);

    MOZ_ASSERT(GetState() != s->GetState()
               || GetState() == DECODER_STATE_SEEKING);

    SLOG("change state to: %s", ToStateStr(s->GetState()));

    Exit();

    master->mStateObj.reset(s);
    return CallEnterMemberFunction(s, copiedArgs,
                                   typename IndexSequenceFor<Ts...>::Type());
  }

  RefPtr<MediaDecoder::SeekPromise>
  SetSeekingState(SeekJob&& aSeekJob, EventVisibility aVisibility);

  // Take a raw pointer in order not to change the life cycle of MDSM.
  // It is guaranteed to be valid by MDSM.
  Master* mMaster;
};

/**
 * Purpose: decode metadata like duration and dimensions of the media resource.
 *
 * Transition to other states when decoding metadata is done:
 *   SHUTDOWN if failing to decode metadata.
 *   WAIT_FOR_CDM if the media is encrypted and CDM is not available.
 *   DECODING_FIRSTFRAME otherwise.
 */
class MediaDecoderStateMachine::DecodeMetadataState
  : public MediaDecoderStateMachine::StateObject
{
public:
  explicit DecodeMetadataState(Master* aPtr) : StateObject(aPtr) { }

  void Enter()
  {
    MOZ_ASSERT(!mMaster->mVideoDecodeSuspended);
    MOZ_ASSERT(!mMetadataRequest.Exists());
    SLOG("Dispatching AsyncReadMetadata");

    // Set mode to METADATA since we are about to read metadata.
    Resource()->SetReadMode(MediaCacheStream::MODE_METADATA);

    // We disconnect mMetadataRequest in Exit() so it is fine to capture
    // a raw pointer here.
    Reader()->ReadMetadata()
      ->Then(OwnerThread(), __func__,
        [this] (MetadataHolder&& aMetadata) {
          OnMetadataRead(Move(aMetadata));
        },
        [this] (const MediaResult& aError) {
          OnMetadataNotRead(aError);
        })
      ->Track(mMetadataRequest);
  }

  void Exit() override { mMetadataRequest.DisconnectIfExists(); }

  State GetState() const override { return DECODER_STATE_DECODING_METADATA; }

  RefPtr<MediaDecoder::SeekPromise> HandleSeek(SeekTarget aTarget) override
  {
    MOZ_DIAGNOSTIC_ASSERT(false, "Can't seek while decoding metadata.");
    return MediaDecoder::SeekPromise::CreateAndReject(true, __func__);
  }

  void HandleVideoSuspendTimeout() override
  {
    // Do nothing since no decoders are created yet.
  }

  void HandleResumeVideoDecoding(const TimeUnit&) override
  {
    // We never suspend video decoding in this state.
    MOZ_ASSERT(false, "Shouldn't have suspended video decoding.");
  }

private:
  void OnMetadataRead(MetadataHolder&& aMetadata);

  void OnMetadataNotRead(const MediaResult& aError)
  {
    mMetadataRequest.Complete();
    SLOGW("Decode metadata failed, shutting down decoder");
    mMaster->DecodeError(aError);
  }

  MozPromiseRequestHolder<MediaDecoderReader::MetadataPromise> mMetadataRequest;
};

/**
 * Purpose: wait for the CDM to start decoding.
 *
 * Transition to other states when CDM is ready:
 *   SEEKING if any pending seek request.
 *   DECODING_FIRSTFRAME otherwise.
 */
class MediaDecoderStateMachine::WaitForCDMState
  : public MediaDecoderStateMachine::StateObject
{
public:
  explicit WaitForCDMState(Master* aPtr) : StateObject(aPtr) { }

  void Enter() { MOZ_ASSERT(!mMaster->mVideoDecodeSuspended); }

  void Exit() override
  {
    // mPendingSeek is either moved in HandleCDMProxyReady() or should be
    // rejected here before transition to SHUTDOWN.
    mPendingSeek.RejectIfExists(__func__);
  }

  State GetState() const override
  {
    return DECODER_STATE_WAIT_FOR_CDM;
  }

  void HandleCDMProxyReady() override;

  RefPtr<MediaDecoder::SeekPromise> HandleSeek(SeekTarget aTarget) override
  {
    SLOG("Not Enough Data to seek at this stage, queuing seek");
    mPendingSeek.RejectIfExists(__func__);
    mPendingSeek.mTarget.emplace(aTarget);
    return mPendingSeek.mPromise.Ensure(__func__);
  }

  void HandleVideoSuspendTimeout() override
  {
    // Do nothing since no decoders are created yet.
  }

  void HandleResumeVideoDecoding(const TimeUnit&) override
  {
    // We never suspend video decoding in this state.
    MOZ_ASSERT(false, "Shouldn't have suspended video decoding.");
  }

private:
  SeekJob mPendingSeek;
};

/**
 * Purpose: release decoder resources to save memory and hardware resources.
 *
 * Transition to:
 *   SEEKING if any seek request or play state changes to PLAYING.
 */
class MediaDecoderStateMachine::DormantState
  : public MediaDecoderStateMachine::StateObject
{
public:
  explicit DormantState(Master* aPtr) : StateObject(aPtr) { }

  void Enter()
  {
    if (mMaster->IsPlaying()) {
      mMaster->StopPlayback();
    }

    // Calculate the position to seek to when exiting dormant.
    auto t = mMaster->mMediaSink->IsStarted()
      ? mMaster->GetClock() : mMaster->GetMediaTime();
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

  void Exit() override
  {
    // mPendingSeek is either moved when exiting dormant or
    // should be rejected here before transition to SHUTDOWN.
    mPendingSeek.RejectIfExists(__func__);
  }

  State GetState() const override { return DECODER_STATE_DORMANT; }

  RefPtr<MediaDecoder::SeekPromise> HandleSeek(SeekTarget aTarget) override;

  void HandleVideoSuspendTimeout() override
  {
    // Do nothing since we've released decoders in Enter().
  }

  void HandleResumeVideoDecoding(const TimeUnit&) override
  {
    // Do nothing since we won't resume decoding until exiting dormant.
  }

  void HandlePlayStateChanged(MediaDecoder::PlayState aPlayState) override;

  void HandleAudioDecoded(AudioData*) override { MaybeReleaseResources(); }
  void HandleVideoDecoded(VideoData*, TimeStamp) override
  {
    MaybeReleaseResources();
  }
  void HandleWaitingForAudio() override { MaybeReleaseResources(); }
  void HandleWaitingForVideo() override { MaybeReleaseResources(); }
  void HandleAudioCanceled() override { MaybeReleaseResources(); }
  void HandleVideoCanceled() override { MaybeReleaseResources(); }
  void HandleEndOfAudio() override { MaybeReleaseResources(); }
  void HandleEndOfVideo() override { MaybeReleaseResources(); }

private:
  void MaybeReleaseResources()
  {
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
 * Purpose: decode the 1st audio and video frames to fire the 'loadeddata' event.
 *
 * Transition to:
 *   SHUTDOWN if any decode error.
 *   SEEKING if any seek request.
 *   DECODING when the 'loadeddata' event is fired.
 */
class MediaDecoderStateMachine::DecodingFirstFrameState
  : public MediaDecoderStateMachine::StateObject
{
public:
  explicit DecodingFirstFrameState(Master* aPtr) : StateObject(aPtr) { }

  void Enter();

  void Exit() override
  {
    // mPendingSeek is either moved in MaybeFinishDecodeFirstFrame()
    // or should be rejected here before transition to SHUTDOWN.
    mPendingSeek.RejectIfExists(__func__);
  }

  State GetState() const override { return DECODER_STATE_DECODING_FIRSTFRAME; }

  void HandleAudioDecoded(AudioData* aAudio) override
  {
    mMaster->PushAudio(aAudio);
    MaybeFinishDecodeFirstFrame();
  }

  void HandleVideoDecoded(VideoData* aVideo, TimeStamp aDecodeStart) override
  {
    mMaster->PushVideo(aVideo);
    MaybeFinishDecodeFirstFrame();
  }

  void HandleWaitingForAudio() override
  {
    mMaster->WaitForData(MediaData::AUDIO_DATA);
  }

  void HandleAudioCanceled() override
  {
    mMaster->RequestAudioData();
  }

  void HandleEndOfAudio() override
  {
    AudioQueue().Finish();
    MaybeFinishDecodeFirstFrame();
  }

  void HandleWaitingForVideo() override
  {
    mMaster->WaitForData(MediaData::VIDEO_DATA);
  }

  void HandleVideoCanceled() override
  {
    mMaster->RequestVideoData(media::TimeUnit());
  }

  void HandleEndOfVideo() override
  {
    VideoQueue().Finish();
    MaybeFinishDecodeFirstFrame();
  }

  void HandleAudioWaited(MediaData::Type aType) override
  {
    mMaster->RequestAudioData();
  }

  void HandleVideoWaited(MediaData::Type aType) override
  {
    mMaster->RequestVideoData(media::TimeUnit());
  }

  void HandleVideoSuspendTimeout() override
  {
    // Do nothing for we need to decode the 1st video frame to get the
    // dimensions.
  }

  void HandleResumeVideoDecoding(const TimeUnit&) override
  {
    // We never suspend video decoding in this state.
    MOZ_ASSERT(false, "Shouldn't have suspended video decoding.");
  }

  RefPtr<MediaDecoder::SeekPromise> HandleSeek(SeekTarget aTarget) override
  {
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
 */
class MediaDecoderStateMachine::DecodingState
  : public MediaDecoderStateMachine::StateObject
{
public:
  explicit DecodingState(Master* aPtr)
    : StateObject(aPtr)
    , mDormantTimer(OwnerThread())
  {
  }

  void Enter();

  void Exit() override
  {
    if (!mDecodeStartTime.IsNull()) {
      TimeDuration decodeDuration = TimeStamp::Now() - mDecodeStartTime;
      SLOG("Exiting DECODING, decoded for %.3lfs", decodeDuration.ToSeconds());
    }
    mDormantTimer.Reset();
    mOnAudioPopped.DisconnectIfExists();
    mOnVideoPopped.DisconnectIfExists();
  }

  void Step() override
  {
    if (mMaster->mPlayState != MediaDecoder::PLAY_STATE_PLAYING
        && mMaster->IsPlaying()) {
      // We're playing, but the element/decoder is in paused state. Stop
      // playing!
      mMaster->StopPlayback();
    }

    // Start playback if necessary so that the clock can be properly queried.
    if (!mIsPrerolling) {
      mMaster->MaybeStartPlayback();
    }

    mMaster->UpdatePlaybackPositionPeriodically();

    MOZ_ASSERT(!mMaster->IsPlaying()
               || mMaster->IsStateMachineScheduled(),
               "Must have timer scheduled");

    MaybeStartBuffering();
  }

  State GetState() const override
  {
    return DECODER_STATE_DECODING;
  }

  void HandleAudioDecoded(AudioData* aAudio) override
  {
    mMaster->PushAudio(aAudio);
    DispatchDecodeTasksIfNeeded();
    MaybeStopPrerolling();
  }

  void HandleVideoDecoded(VideoData* aVideo, TimeStamp aDecodeStart) override
  {
    mMaster->PushVideo(aVideo);
    DispatchDecodeTasksIfNeeded();
    MaybeStopPrerolling();
    CheckSlowDecoding(aDecodeStart);
  }

  void HandleAudioCanceled() override
  {
    mMaster->RequestAudioData();
  }

  void HandleVideoCanceled() override
  {
    mMaster->RequestVideoData(mMaster->GetMediaTime());
  }

  void HandleEndOfAudio() override;
  void HandleEndOfVideo() override;

  void HandleWaitingForAudio() override
  {
    mMaster->WaitForData(MediaData::AUDIO_DATA);
    MaybeStopPrerolling();
  }

  void HandleWaitingForVideo() override
  {
    mMaster->WaitForData(MediaData::VIDEO_DATA);
    MaybeStopPrerolling();
  }

  void HandleAudioWaited(MediaData::Type aType) override
  {
    mMaster->RequestAudioData();
  }

  void HandleVideoWaited(MediaData::Type aType) override
  {
    mMaster->RequestVideoData(mMaster->GetMediaTime());
  }

  void HandleAudioCaptured() override
  {
    MaybeStopPrerolling();
    // MediaSink is changed. Schedule Step() to check if we can start playback.
    mMaster->ScheduleStateMachine();
  }

  void HandleVideoSuspendTimeout() override
  {
    // No video, so nothing to suspend.
    if (!mMaster->HasVideo()) {
      return;
    }

    mMaster->mVideoDecodeSuspended = true;
    mMaster->mOnPlaybackEvent.Notify(MediaEventType::EnterVideoSuspend);
    Reader()->SetVideoBlankDecode(true);
  }

  void HandlePlayStateChanged(MediaDecoder::PlayState aPlayState) override
  {
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

  nsCString GetDebugInfo() override
  {
    return nsPrintfCString("mIsPrerolling=%d", mIsPrerolling);
  }

private:
  void DispatchDecodeTasksIfNeeded();
  void EnsureAudioDecodeTaskQueued();
  void EnsureVideoDecodeTaskQueued();
  void MaybeStartBuffering();

  void CheckSlowDecoding(TimeStamp aDecodeStart)
  {
    // For non async readers, if the requested video sample was slow to
    // arrive, increase the amount of audio we buffer to ensure that we
    // don't run out of audio. This is unnecessary for async readers,
    // since they decode audio and video on different threads so they
    // are unlikely to run out of decoded audio.
    if (Reader()->IsAsync()) {
      return;
    }

    TimeDuration decodeTime = TimeStamp::Now() - aDecodeStart;
    auto adjusted = TimeUnit::FromTimeDuration(decodeTime * THRESHOLD_FACTOR);
    if (adjusted > mMaster->mLowAudioThreshold
        && !mMaster->HasLowBufferedData())
    {
      mMaster->mLowAudioThreshold = std::min(
        adjusted, mMaster->mAmpleAudioThreshold);

      mMaster->mAmpleAudioThreshold = std::max(
        mMaster->mLowAudioThreshold * THRESHOLD_FACTOR,
        mMaster->mAmpleAudioThreshold);

      SLOG("Slow video decode, set "
           "mLowAudioThreshold=%" PRId64
           " mAmpleAudioThreshold=%" PRId64,
           mMaster->mLowAudioThreshold.ToMicroseconds(),
           mMaster->mAmpleAudioThreshold.ToMicroseconds());
    }
  }

  // At the start of decoding we want to "preroll" the decode until we've
  // got a few frames decoded before we consider whether decode is falling
  // behind. Otherwise our "we're falling behind" logic will trigger
  // unnecessarily if we start playing as soon as the first sample is
  // decoded. These two fields store how many video frames and audio
  // samples we must consume before are considered to be finished prerolling.
  TimeUnit AudioPrerollThreshold() const
  {
    return mMaster->mAmpleAudioThreshold / 2;
  }

  uint32_t VideoPrerollFrames() const
  {
    return mMaster->GetAmpleVideoFrames() / 2;
  }

  bool DonePrerollingAudio()
  {
    return !mMaster->IsAudioDecoding()
           || mMaster->GetDecodedAudioDuration()
              >= AudioPrerollThreshold().MultDouble(mMaster->mPlaybackRate);
  }

  bool DonePrerollingVideo()
  {
    return !mMaster->IsVideoDecoding()
           || static_cast<uint32_t>(mMaster->VideoQueue().GetSize())
              >= VideoPrerollFrames() * mMaster->mPlaybackRate + 1;
  }

  void MaybeStopPrerolling()
  {
    if (mIsPrerolling
        && (DonePrerollingAudio() || mMaster->IsWaitingAudioData())
        && (DonePrerollingVideo() || mMaster->IsWaitingVideoData())) {
      mIsPrerolling = false;
      // Check if we can start playback.
      mMaster->ScheduleStateMachine();
    }
  }

  void StartDormantTimer()
  {
    if (!mMaster->mMediaSeekable) {
      // Don't enter dormant if the media is not seekable because we need to
      // seek when exiting dormant.
      return;
    }

    auto timeout = MediaPrefs::DormantOnPauseTimeout();
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

    TimeStamp target = TimeStamp::Now() +
      TimeDuration::FromMilliseconds(timeout);

    mDormantTimer.Ensure(target,
      [this] () {
        mDormantTimer.CompleteRequest();
        SetState<DormantState>();
      }, [this] () {
        mDormantTimer.CompleteRequest();
      });
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
 * Purpose: seek to a particular new playback position.
 *
 * Transition to:
 *   SEEKING if any new seek request.
 *   SHUTDOWN if seek failed.
 *   COMPLETED if the new playback position is the end of the media resource.
 *   NextFrameSeekingState if completing a NextFrameSeekingFromDormantState.
 *   DECODING otherwise.
 */
class MediaDecoderStateMachine::SeekingState
  : public MediaDecoderStateMachine::StateObject
{
public:
  explicit SeekingState(Master* aPtr) : StateObject(aPtr) { }

  RefPtr<MediaDecoder::SeekPromise> Enter(SeekJob&& aSeekJob,
                                          EventVisibility aVisibility)
  {
    mSeekJob = Move(aSeekJob);
    mVisibility = aVisibility;

    // Always switch off the blank decoder otherwise we might become visible
    // in the middle of seeking and won't have a valid video frame to show
    // when seek is done.
    if (mMaster->mVideoDecodeSuspended) {
      mMaster->mVideoDecodeSuspended = false;
      mMaster->mOnPlaybackEvent.Notify(MediaEventType::ExitVideoSuspend);
      Reader()->SetVideoBlankDecode(false);
    }

    // Dispatch a mozvideoonlyseekbegin event to indicate UI for corresponding
    // changes.
    if (mSeekJob.mTarget->IsVideoOnly()) {
      mMaster->mOnPlaybackEvent.Notify(MediaEventType::VideoOnlySeekBegin);
    }

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
      mMaster->mOnPlaybackEvent.Notify(MediaEventType::SeekStarted);
      mMaster->UpdateNextFrameStatus(
        MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_SEEKING);
    }

    RefPtr<MediaDecoder::SeekPromise> p = mSeekJob.mPromise.Ensure(__func__);

    DoSeek();

    return p;
  }

  virtual void Exit() override = 0;

  State GetState() const override
  {
    return DECODER_STATE_SEEKING;
  }

  void HandleAudioDecoded(AudioData* aAudio) override = 0;
  void HandleVideoDecoded(VideoData* aVideo,
                          TimeStamp aDecodeStart) override = 0;
  void HandleAudioWaited(MediaData::Type aType) override = 0;
  void HandleVideoWaited(MediaData::Type aType) override = 0;

  void HandleVideoSuspendTimeout() override
  {
    // Do nothing since we want a valid video frame to show when seek is done.
  }

  void HandleResumeVideoDecoding(const TimeUnit&) override
  {
    // We set mVideoDecodeSuspended to false in Enter().
    MOZ_ASSERT(false, "Shouldn't have suspended video decoding.");
  }

protected:
  SeekJob mSeekJob;
  EventVisibility mVisibility;

  virtual void DoSeek() = 0;
  // Transition to the next state (defined by the subclass) when seek is completed.
  virtual void GoToNextState() { SetState<DecodingState>(); }
  void SeekCompleted();
  virtual TimeUnit CalculateNewCurrentTime() const = 0;
};

class MediaDecoderStateMachine::AccurateSeekingState
  : public MediaDecoderStateMachine::SeekingState
{
public:
  explicit AccurateSeekingState(Master* aPtr) : SeekingState(aPtr) { }

  RefPtr<MediaDecoder::SeekPromise> Enter(SeekJob&& aSeekJob,
                                          EventVisibility aVisibility)
  {
    MOZ_ASSERT(aSeekJob.mTarget->IsAccurate() || aSeekJob.mTarget->IsFast());
    mCurrentTimeBeforeSeek = mMaster->GetMediaTime();
    return SeekingState::Enter(Move(aSeekJob), aVisibility);
  }

  void Exit() override
  {
    if (mSeekJob.Exists() &&
        mSeekJob.mTarget.isSome() &&
        mSeekJob.mTarget->IsVideoOnly()) {
      // We are discarding this video-only seek operation now, and we still need
      // to dispatch an event so that the UI can change in response to the end
      // of video-only seek.
      mMaster->mOnPlaybackEvent.Notify(MediaEventType::VideoOnlySeekCompleted);
    }

    // Disconnect MediaDecoder.
    mSeekJob.RejectIfExists(__func__);

    // Disconnect MediaDecoderReaderWrapper.
    mSeekRequest.DisconnectIfExists();

    mWaitRequest.DisconnectIfExists();
  }

  void HandleAudioDecoded(AudioData* aAudio) override
  {
    MOZ_ASSERT(!mDoneAudioSeeking || !mDoneVideoSeeking,
               "Seek shouldn't be finished");
    MOZ_ASSERT(aAudio);

    // Video-only seek doesn't reset audio decoder. There might be pending audio
    // requests when AccurateSeekTask::Seek() begins. We will just store the
    // data without checking |mDiscontinuity| or calling
    // DropAudioUpToSeekTarget().
    if (mSeekJob.mTarget->IsVideoOnly()) {
      mMaster->PushAudio(aAudio);
      return;
    }

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

  void HandleVideoDecoded(VideoData* aVideo, TimeStamp aDecodeStart) override
  {
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

  void HandleWaitingForAudio() override
  {
    if (!mSeekJob.mTarget->IsVideoOnly()) {
      MOZ_ASSERT(!mDoneAudioSeeking);
      mMaster->WaitForData(MediaData::AUDIO_DATA);
    }
  }

  void HandleAudioCanceled() override
  {
    if (!mSeekJob.mTarget->IsVideoOnly()) {
      MOZ_ASSERT(!mDoneAudioSeeking);
      RequestAudioData();
    }
  }

  void HandleEndOfAudio() override
  {
    if (!mSeekJob.mTarget->IsVideoOnly()) {
      MOZ_ASSERT(!mDoneAudioSeeking);
      AudioQueue().Finish();
      mDoneAudioSeeking = true;
      MaybeFinishSeek();
    }
  }

  void HandleWaitingForVideo() override
  {
    MOZ_ASSERT(!mDoneVideoSeeking);
    mMaster->WaitForData(MediaData::VIDEO_DATA);
  }

  void HandleVideoCanceled() override
  {
    MOZ_ASSERT(!mDoneVideoSeeking);
    RequestVideoData();
  }

  void HandleEndOfVideo() override
  {
    MOZ_ASSERT(!mDoneVideoSeeking);
    if (mFirstVideoFrameAfterSeek) {
      // Hit the end of stream. Move mFirstVideoFrameAfterSeek into
      // mSeekedVideoData so we have something to display after seeking.
      mMaster->PushVideo(mFirstVideoFrameAfterSeek);
    }
    VideoQueue().Finish();
    mDoneVideoSeeking = true;
    MaybeFinishSeek();
  }

  void HandleAudioWaited(MediaData::Type aType) override
  {
    MOZ_ASSERT(!mDoneAudioSeeking || !mDoneVideoSeeking,
               "Seek shouldn't be finished");

    // Ignore pending requests from video-only seek.
    if (mSeekJob.mTarget->IsVideoOnly()) {
      return;
    }
    RequestAudioData();
  }

  void HandleVideoWaited(MediaData::Type aType) override
  {
    MOZ_ASSERT(!mDoneAudioSeeking || !mDoneVideoSeeking,
               "Seek shouldn't be finished");

    RequestVideoData();
  }

  void DoSeek() override
  {
    mDoneAudioSeeking = !Info().HasAudio() || mSeekJob.mTarget->IsVideoOnly();
    mDoneVideoSeeking = !Info().HasVideo();

    if (mSeekJob.mTarget->IsVideoOnly()) {
      mMaster->ResetDecode(TrackInfo::kVideoTrack);
    } else {
      mMaster->ResetDecode();
      mMaster->StopMediaSink();
    }

    DemuxerSeek();
  }

  TimeUnit CalculateNewCurrentTime() const override
  {
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
      return TimeUnit::FromMicroseconds(
        audioGap <= videoGap ? audioStart : videoStart);
    }

    MOZ_ASSERT(false, "AccurateSeekTask doesn't handle other seek types.");
    return TimeUnit::Zero();
  }

protected:
  void DemuxerSeek()
  {
    // Request the demuxer to perform seek.
    Reader()->Seek(mSeekJob.mTarget.ref())
      ->Then(OwnerThread(), __func__,
             [this] (const media::TimeUnit& aUnit) {
               OnSeekResolved(aUnit);
             },
             [this] (const SeekRejectValue& aReject) {
               OnSeekRejected(aReject);
             })
      ->Track(mSeekRequest);
  }

  void OnSeekResolved(media::TimeUnit)
  {
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

  void OnSeekRejected(const SeekRejectValue& aReject)
  {
    mSeekRequest.Complete();

    if (aReject.mError == NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA) {
      SLOG("OnSeekRejected reason=WAITING_FOR_DATA type=%d", aReject.mType);
      MOZ_ASSERT_IF(aReject.mType == MediaData::AUDIO_DATA, !mMaster->IsRequestingAudioData());
      MOZ_ASSERT_IF(aReject.mType == MediaData::VIDEO_DATA, !mMaster->IsRequestingVideoData());
      MOZ_ASSERT_IF(aReject.mType == MediaData::AUDIO_DATA, !mMaster->IsWaitingAudioData());
      MOZ_ASSERT_IF(aReject.mType == MediaData::VIDEO_DATA, !mMaster->IsWaitingVideoData());

      // Fire 'waiting' to notify the player that we are waiting for data.
      mMaster->UpdateNextFrameStatus(
        MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_SEEKING);
      Reader()
        ->WaitForData(aReject.mType)
        ->Then(OwnerThread(), __func__,
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
      HandleEndOfAudio();
      HandleEndOfVideo();
      return;
    }

    MOZ_ASSERT(NS_FAILED(aReject.mError),
               "Cancels should also disconnect mSeekRequest");
    mMaster->DecodeError(aReject.mError);
  }

  void RequestAudioData()
  {
    MOZ_ASSERT(!mDoneAudioSeeking);
    mMaster->RequestAudioData();
  }

  void RequestVideoData()
  {
    MOZ_ASSERT(!mDoneVideoSeeking);
    mMaster->RequestVideoData(media::TimeUnit());
  }

  void AdjustFastSeekIfNeeded(MediaData* aSample)
  {
    if (mSeekJob.mTarget->IsFast()
        && mSeekJob.mTarget->GetTime() > mCurrentTimeBeforeSeek
        && aSample->mTime < mCurrentTimeBeforeSeek) {
      // We are doing a fastSeek, but we ended up *before* the previous
      // playback position. This is surprising UX, so switch to an accurate
      // seek and decode to the seek target. This is not conformant to the
      // spec, fastSeek should always be fast, but until we get the time to
      // change all Readers to seek to the keyframe after the currentTime
      // in this case, we'll just decode forward. Bug 1026330.
      mSeekJob.mTarget->SetType(SeekTarget::Accurate);
    }
  }

  nsresult DropAudioUpToSeekTarget(AudioData* aAudio)
  {
    MOZ_ASSERT(aAudio && mSeekJob.mTarget->IsAccurate());

    auto sampleDuration = FramesToTimeUnit(
      aAudio->mFrames, Info().mAudio.mRate);
    if (!sampleDuration.IsValid()) {
      return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
    }

    auto audioTime = aAudio->mTime;
    if (audioTime + sampleDuration <= mSeekJob.mTarget->GetTime()) {
      // Our seek target lies after the frames in this AudioData. Don't
      // push it onto the audio queue, and keep decoding forwards.
      return NS_OK;
    }

    if (audioTime > mSeekJob.mTarget->GetTime()) {
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

    // The seek target lies somewhere in this AudioData's frames, strip off
    // any frames which lie before the seek target, so we'll begin playback
    // exactly at the seek target.
    NS_ASSERTION(mSeekJob.mTarget->GetTime() >= audioTime,
                 "Target must at or be after data start.");
    NS_ASSERTION(mSeekJob.mTarget->GetTime() < audioTime + sampleDuration,
                 "Data must end after target.");

    CheckedInt64 framesToPrune = TimeUnitToFrames(
      mSeekJob.mTarget->GetTime() - audioTime, Info().mAudio.mRate);
    if (!framesToPrune.isValid()) {
      return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
    }
    if (framesToPrune.value() > aAudio->mFrames) {
      // We've messed up somehow. Don't try to trim frames, the |frames|
      // variable below will overflow.
      SLOGW("Can't prune more frames that we have!");
      return NS_ERROR_FAILURE;
    }
    uint32_t frames = aAudio->mFrames - uint32_t(framesToPrune.value());
    uint32_t channels = aAudio->mChannels;
    AlignedAudioBuffer audioData(frames * channels);
    if (!audioData) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    memcpy(audioData.get(),
           aAudio->mAudioData.get() + (framesToPrune.value() * channels),
           frames * channels * sizeof(AudioDataValue));
    auto duration = FramesToTimeUnit(frames, Info().mAudio.mRate);
    if (!duration.IsValid()) {
      return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
    }
    RefPtr<AudioData> data(new AudioData(
      aAudio->mOffset, mSeekJob.mTarget->GetTime(),
      duration, frames, Move(audioData), channels,
      aAudio->mRate));
    MOZ_ASSERT(AudioQueue().GetSize() == 0,
               "Should be the 1st sample after seeking");
    mMaster->PushAudio(data);
    mDoneAudioSeeking = true;

    return NS_OK;
  }

  nsresult DropVideoUpToSeekTarget(VideoData* aVideo)
  {
    MOZ_ASSERT(aVideo);
    SLOG("DropVideoUpToSeekTarget() frame [%" PRId64 ", %" PRId64 "]",
         aVideo->mTime.ToMicroseconds(), aVideo->GetEndTime().ToMicroseconds());
    const auto target = mSeekJob.mTarget->GetTime();

    // If the frame end time is less than the seek target, we won't want
    // to display this frame after the seek, so discard it.
    if (target >= aVideo->GetEndTime()) {
      SLOG("DropVideoUpToSeekTarget() pop video frame [%" PRId64 ", %" PRId64
           "] target=%" PRId64,
           aVideo->mTime.ToMicroseconds(),
           aVideo->GetEndTime().ToMicroseconds(),
           target.ToMicroseconds());
      mFirstVideoFrameAfterSeek = aVideo;
    } else {
      if (target >= aVideo->mTime &&
          aVideo->GetEndTime() >= target) {
        // The seek target lies inside this frame's time slice. Adjust the
        // frame's start time to match the seek target.
        aVideo->UpdateTimestamp(target);
      }
      mFirstVideoFrameAfterSeek = nullptr;

      SLOG("DropVideoUpToSeekTarget() found video frame [%" PRId64 ", %" PRId64
           "] containing target=%" PRId64,
           aVideo->mTime.ToMicroseconds(),
           aVideo->GetEndTime().ToMicroseconds(),
           target.ToMicroseconds());

      MOZ_ASSERT(VideoQueue().GetSize() == 0,
                 "Should be the 1st sample after seeking");
      mMaster->PushVideo(aVideo);
      mDoneVideoSeeking = true;
    }

    return NS_OK;
  }

  void MaybeFinishSeek()
  {
    if (mDoneAudioSeeking && mDoneVideoSeeking) {
      SeekCompleted();
    }
  }

  /*
   * Track the current seek promise made by the reader.
   */
  MozPromiseRequestHolder<MediaDecoderReader::SeekPromise> mSeekRequest;

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
};

/*
 * Remove samples from the queue until aCompare() returns false.
 * aCompare A function object with the signature bool(int64_t) which returns
 *          true for samples that should be removed.
 */
template <typename Type, typename Function>
static void
DiscardFrames(MediaQueue<Type>& aQueue, const Function& aCompare)
{
  while(aQueue.GetSize() > 0) {
    if (aCompare(aQueue.PeekFront()->mTime.ToMicroseconds())) {
      RefPtr<Type> releaseMe = aQueue.PopFront();
      continue;
    }
    break;
  }
}

class MediaDecoderStateMachine::NextFrameSeekingState
  : public MediaDecoderStateMachine::SeekingState
{
public:
  explicit NextFrameSeekingState(Master* aPtr) : SeekingState(aPtr) { }

  RefPtr<MediaDecoder::SeekPromise> Enter(SeekJob&& aSeekJob,
                                          EventVisibility aVisibility)
  {
    MOZ_ASSERT(aSeekJob.mTarget->IsNextFrame());
    mCurrentTime = mMaster->GetMediaTime();
    mDuration = mMaster->Duration();
    return SeekingState::Enter(Move(aSeekJob), aVisibility);
  }

  void Exit() override
  {
    // Disconnect my async seek operation.
    if (mAsyncSeekTask) { mAsyncSeekTask->Cancel(); }

    // Disconnect MediaDecoder.
    mSeekJob.RejectIfExists(__func__);
  }

  void HandleAudioDecoded(AudioData* aAudio) override
  {
    mMaster->PushAudio(aAudio);
  }

  void HandleVideoDecoded(VideoData* aVideo, TimeStamp aDecodeStart) override
  {
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

  void HandleWaitingForAudio() override
  {
    MOZ_ASSERT(!mSeekJob.mPromise.IsEmpty(), "Seek shouldn't be finished");
    // We don't care about audio decode errors in this state which will be
    // handled by other states after seeking.
  }

  void HandleAudioCanceled() override
  {
    MOZ_ASSERT(!mSeekJob.mPromise.IsEmpty(), "Seek shouldn't be finished");
    // We don't care about audio decode errors in this state which will be
    // handled by other states after seeking.
  }

  void HandleEndOfAudio() override
  {
    MOZ_ASSERT(!mSeekJob.mPromise.IsEmpty(), "Seek shouldn't be finished");
    // We don't care about audio decode errors in this state which will be
    // handled by other states after seeking.
  }

  void HandleWaitingForVideo() override
  {
    MOZ_ASSERT(!mSeekJob.mPromise.IsEmpty(), "Seek shouldn't be finished");
    MOZ_ASSERT(NeedMoreVideo());
    mMaster->WaitForData(MediaData::VIDEO_DATA);
  }

  void HandleVideoCanceled() override
  {
    MOZ_ASSERT(!mSeekJob.mPromise.IsEmpty(), "Seek shouldn't be finished");
    MOZ_ASSERT(NeedMoreVideo());
    RequestVideoData();
  }

  void HandleEndOfVideo() override
  {
    MOZ_ASSERT(!mSeekJob.mPromise.IsEmpty(), "Seek shouldn't be finished");
    MOZ_ASSERT(NeedMoreVideo());
    VideoQueue().Finish();
    FinishSeek();
  }

  void HandleAudioWaited(MediaData::Type aType) override
  {
    // We don't care about audio in this state.
  }

  void HandleVideoWaited(MediaData::Type aType) override
  {
    MOZ_ASSERT(!mSeekJob.mPromise.IsEmpty(), "Seek shouldn't be finished");
    MOZ_ASSERT(NeedMoreVideo());
    RequestVideoData();
  }

  TimeUnit CalculateNewCurrentTime() const override
  {
    // The HTMLMediaElement.currentTime should be updated to the seek target
    // which has been updated to the next frame's time.
    return mSeekJob.mTarget->GetTime();
  }

  void DoSeek() override
  {
    auto currentTime = mCurrentTime;
    DiscardFrames(VideoQueue(), [currentTime] (int64_t aSampleTime) {
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
    OwnerThread()->Dispatch(r.forget());
  }

private:
  void DoSeekInternal()
  {
    // We don't need to discard frames to the mCurrentTime here because we have
    // done it at DoSeek() and any video data received in between either
    // finishes the seek operation or be discarded, see HandleVideoDecoded().

    if (!NeedMoreVideo()) {
      FinishSeek();
    } else if (!mMaster->IsRequestingVideoData()
               && !mMaster->IsWaitingVideoData()) {
      RequestVideoData();
    }
  }

  class AysncNextFrameSeekTask : public Runnable
  {
  public:
    explicit AysncNextFrameSeekTask(NextFrameSeekingState* aStateObject)
      : Runnable("MediaDecoderStateMachine::NextFrameSeekingState::"
                 "AysncNextFrameSeekTask")
      , mStateObj(aStateObject)
    {
    }

    void Cancel() { mStateObj = nullptr; }

    NS_IMETHOD Run() override
    {
      if (mStateObj) {
        mStateObj->DoSeekInternal();
      }
      return NS_OK;
    }

  private:
    NextFrameSeekingState* mStateObj;
  };

  void RequestVideoData()
  {
    mMaster->RequestVideoData(media::TimeUnit());
  }

  bool NeedMoreVideo() const
  {
    // Need to request video when we have none and video queue is not finished.
    return VideoQueue().GetSize() == 0
           && !VideoQueue().IsFinished();
  }

  // Update the seek target's time before resolving this seek task, the updated
  // time will be used in the MDSM::SeekCompleted() to update the MDSM's
  // position.
  void UpdateSeekTargetTime()
  {
    RefPtr<VideoData> data = VideoQueue().PeekFront();
    if (data) {
      mSeekJob.mTarget->SetTime(data->mTime);
    } else {
      MOZ_ASSERT(VideoQueue().AtEndOfStream());
      mSeekJob.mTarget->SetTime(mDuration);
    }
  }

  void FinishSeek()
  {
    MOZ_ASSERT(!NeedMoreVideo());
    UpdateSeekTargetTime();
    auto time = mSeekJob.mTarget->GetTime().ToMicroseconds();
    DiscardFrames(AudioQueue(), [time] (int64_t aSampleTime) {
      return aSampleTime < time;
    });
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
  : public MediaDecoderStateMachine::AccurateSeekingState
{
public:
  explicit NextFrameSeekingFromDormantState(Master* aPtr)
    : AccurateSeekingState(aPtr)
  {
  }

  RefPtr<MediaDecoder::SeekPromise> Enter(SeekJob&& aCurrentSeekJob,
                                          SeekJob&& aFutureSeekJob)
  {
    mFutureSeekJob = Move(aFutureSeekJob);

    AccurateSeekingState::Enter(Move(aCurrentSeekJob),
                                EventVisibility::Suppressed);

    return mFutureSeekJob.mPromise.Ensure(__func__);
  }

  void Exit() override
  {
    mFutureSeekJob.RejectIfExists(__func__);
    AccurateSeekingState::Exit();
  }

private:
  SeekJob mFutureSeekJob;

  // We don't want to transition to DecodingState once this seek completes,
  // instead, we transition to NextFrameSeekingState.
  void GoToNextState() override
  {
    SetState<NextFrameSeekingState>(Move(mFutureSeekJob),
                                    EventVisibility::Observable);
  }
};

class MediaDecoderStateMachine::VideoOnlySeekingState
  : public MediaDecoderStateMachine::AccurateSeekingState
{
public:
  explicit VideoOnlySeekingState(Master* aPtr) : AccurateSeekingState(aPtr) { }
};

RefPtr<MediaDecoder::SeekPromise>
MediaDecoderStateMachine::DormantState::HandleSeek(SeekTarget aTarget)
{
  if (aTarget.IsNextFrame()) {
    // NextFrameSeekingState doesn't reset the decoder unlike
    // AccurateSeekingState. So we first must come out of dormant by seeking to
    // mPendingSeek and continue later with the NextFrameSeek
    SLOG("Changed state to SEEKING (to %" PRId64 ")",
        aTarget.GetTime().ToMicroseconds());
    SeekJob seekJob;
    seekJob.mTarget = Some(aTarget);
    return StateObject::SetState<NextFrameSeekingFromDormantState>(
      Move(mPendingSeek), Move(seekJob));
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
 *   DECODING when having decoded enough data to continue playback.
 */
class MediaDecoderStateMachine::BufferingState
  : public MediaDecoderStateMachine::StateObject
{
public:
  explicit BufferingState(Master* aPtr) : StateObject(aPtr) { }

  void Enter()
  {
    if (mMaster->IsPlaying()) {
      mMaster->StopPlayback();
    }

    mBufferingStart = TimeStamp::Now();

    MediaStatistics stats = mMaster->GetStatistics();
    SLOG("Playback rate: %.1lfKB/s%s download rate: %.1lfKB/s%s",
         stats.mPlaybackRate / 1024,
         stats.mPlaybackRateReliable ? "" : " (unreliable)",
         stats.mDownloadRate / 1024,
         stats.mDownloadRateReliable ? "" : " (unreliable)");

    mMaster->ScheduleStateMachineIn(TimeUnit::FromMicroseconds(USECS_PER_S));

    mMaster->UpdateNextFrameStatus(
      MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_BUFFERING);
  }

  void Step() override;

  State GetState() const override { return DECODER_STATE_BUFFERING; }

  void HandleAudioDecoded(AudioData* aAudio) override
  {
    // This might be the sample we need to exit buffering.
    // Schedule Step() to check it.
    mMaster->PushAudio(aAudio);
    mMaster->ScheduleStateMachine();
  }

  void HandleVideoDecoded(VideoData* aVideo, TimeStamp aDecodeStart) override
  {
    // This might be the sample we need to exit buffering.
    // Schedule Step() to check it.
    mMaster->PushVideo(aVideo);
    mMaster->ScheduleStateMachine();
  }

  void HandleAudioCanceled() override { mMaster->RequestAudioData(); }

  void HandleVideoCanceled() override
  {
    mMaster->RequestVideoData(media::TimeUnit());
  }

  void HandleWaitingForAudio() override
  {
    mMaster->WaitForData(MediaData::AUDIO_DATA);
  }

  void HandleWaitingForVideo() override
  {
    mMaster->WaitForData(MediaData::VIDEO_DATA);
  }

  void HandleAudioWaited(MediaData::Type aType) override
  {
    mMaster->RequestAudioData();
  }

  void HandleVideoWaited(MediaData::Type aType) override
  {
    mMaster->RequestVideoData(media::TimeUnit());
  }

  void HandleEndOfAudio() override;
  void HandleEndOfVideo() override;

  void HandleVideoSuspendTimeout() override
  {
    // No video, so nothing to suspend.
    if (!mMaster->HasVideo()) {
     return;
    }

    mMaster->mVideoDecodeSuspended = true;
    mMaster->mOnPlaybackEvent.Notify(MediaEventType::EnterVideoSuspend);
    Reader()->SetVideoBlankDecode(true);
  }

private:
  void DispatchDecodeTasksIfNeeded();

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
 */
class MediaDecoderStateMachine::CompletedState
  : public MediaDecoderStateMachine::StateObject
{
public:
  explicit CompletedState(Master* aPtr) : StateObject(aPtr) { }

  void Enter()
  {
    // TODO : use more approriate way to decide whether need to release
    // resource in bug1367983.
#ifndef MOZ_WIDGET_ANDROID
    if (!mMaster->mLooping) {
      // We've decoded all samples.
      // We don't need decoders anymore if not looping.
      Reader()->ReleaseResources();
    }
#endif
    bool hasNextFrame = (!mMaster->HasAudio() || !mMaster->mAudioCompleted)
                        && (!mMaster->HasVideo() || !mMaster->mVideoCompleted);

    mMaster->UpdateNextFrameStatus(
      hasNextFrame ? MediaDecoderOwner::NEXT_FRAME_AVAILABLE
                   : MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE);

    Step();
  }

  void Exit() override
  {
    mSentPlaybackEndedEvent = false;
  }

  void Step() override
  {
    if (mMaster->mPlayState != MediaDecoder::PLAY_STATE_PLAYING
        && mMaster->IsPlaying()) {
      mMaster->StopPlayback();
    }

    // Play the remaining media. We want to run AdvanceFrame() at least
    // once to ensure the current playback position is advanced to the
    // end of the media, and so that we update the readyState.
    if ((mMaster->HasVideo() && !mMaster->mVideoCompleted)
        || (mMaster->HasAudio() && !mMaster->mAudioCompleted)) {
      // Start playback if necessary to play the remaining media.
      mMaster->MaybeStartPlayback();
      mMaster->UpdatePlaybackPositionPeriodically();
      MOZ_ASSERT(!mMaster->IsPlaying()
                 || mMaster->IsStateMachineScheduled(),
                 "Must have timer scheduled");
      return;
    }

    // StopPlayback in order to reset the IsPlaying() state so audio
    // is restarted correctly.
    mMaster->StopPlayback();

    if (!mSentPlaybackEndedEvent) {
      auto clockTime =
        std::max(mMaster->AudioEndTime(), mMaster->VideoEndTime());
      clockTime = std::max(clockTime, mMaster->Duration());
      mMaster->UpdatePlaybackPosition(clockTime);

      // Ensure readyState is updated before firing the 'ended' event.
      mMaster->UpdateNextFrameStatus(MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE);

      mMaster->mOnPlaybackEvent.Notify(MediaEventType::PlaybackEnded);

      mSentPlaybackEndedEvent = true;

      // MediaSink::GetEndTime() must be called before stopping playback.
      mMaster->StopMediaSink();
    }
  }

  State GetState() const override
  {
    return DECODER_STATE_COMPLETED;
  }

  void HandleAudioCaptured() override
  {
    // MediaSink is changed. Schedule Step() to check if we can start playback.
    mMaster->ScheduleStateMachine();
  }

  void HandleVideoSuspendTimeout() override
  {
    // Do nothing since no decoding is going on.
  }

  void HandleResumeVideoDecoding(const TimeUnit&) override
  {
    // Resume the video decoder and seek to the last video frame.
    // This triggers a video-only seek which won't update the playback position.
    StateObject::HandleResumeVideoDecoding(mMaster->mDecodedVideoEndTime);
  }

  void HandlePlayStateChanged(MediaDecoder::PlayState aPlayState) override
  {
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
  : public MediaDecoderStateMachine::StateObject
{
public:
  explicit ShutdownState(Master* aPtr) : StateObject(aPtr) { }

  RefPtr<ShutdownPromise> Enter();

  void Exit() override
  {
    MOZ_DIAGNOSTIC_ASSERT(false, "Shouldn't escape the SHUTDOWN state.");
  }

  State GetState() const override
  {
    return DECODER_STATE_SHUTDOWN;
  }

  RefPtr<MediaDecoder::SeekPromise> HandleSeek(SeekTarget aTarget) override
  {
    MOZ_DIAGNOSTIC_ASSERT(false, "Can't seek in shutdown state.");
    return MediaDecoder::SeekPromise::CreateAndReject(true, __func__);
  }

  RefPtr<ShutdownPromise> HandleShutdown() override
  {
    MOZ_DIAGNOSTIC_ASSERT(false, "Already shutting down.");
    return nullptr;
  }

  void HandleVideoSuspendTimeout() override
  {
    MOZ_DIAGNOSTIC_ASSERT(false, "Already shutting down.");
  }

  void HandleResumeVideoDecoding(const TimeUnit&) override
  {
    MOZ_DIAGNOSTIC_ASSERT(false, "Already shutting down.");
  }
};

RefPtr<MediaDecoder::SeekPromise>
MediaDecoderStateMachine::
StateObject::HandleSeek(SeekTarget aTarget)
{
  SLOG("Changed state to SEEKING (to %" PRId64 ")", aTarget.GetTime().ToMicroseconds());
  SeekJob seekJob;
  seekJob.mTarget = Some(aTarget);
  return SetSeekingState(Move(seekJob), EventVisibility::Observable);
}

RefPtr<ShutdownPromise>
MediaDecoderStateMachine::
StateObject::HandleShutdown()
{
  return SetState<ShutdownState>();
}

static void
ReportRecoveryTelemetry(const TimeStamp& aRecoveryStart,
                        const MediaInfo& aMediaInfo,
                        bool aIsHardwareAccelerated)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aMediaInfo.HasVideo()) {
    return;
  }

  // Keyed by audio+video or video alone, hardware acceleration,
  // and by a resolution range.
  nsCString key(aMediaInfo.HasAudio() ? "AV" : "V");
  key.AppendASCII(aIsHardwareAccelerated ? "(hw)," : ",");
  static const struct { int32_t mH; const char* mRes; } sResolutions[] = {
    {  240, "0-240" },
    {  480, "241-480" },
    {  720, "481-720" },
    { 1080, "721-1080" },
    { 2160, "1081-2160" }
  };
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
  Telemetry::Accumulate(Telemetry::VIDEO_SUSPEND_RECOVERY_TIME_MS,
                        key,
                        uint32_t(duration_ms + 0.5));
  Telemetry::Accumulate(Telemetry::VIDEO_SUSPEND_RECOVERY_TIME_MS,
                        NS_LITERAL_CSTRING("All"),
                        uint32_t(duration_ms + 0.5));
}

void
MediaDecoderStateMachine::
StateObject::HandleResumeVideoDecoding(const TimeUnit& aTarget)
{
  MOZ_ASSERT(mMaster->mVideoDecodeSuspended);

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

  SetSeekingState(Move(seekJob), EventVisibility::Suppressed)->Then(
    mainThread, __func__,
    [start, info, hw](){ ReportRecoveryTelemetry(start, info, hw); },
    [](){});
}

RefPtr<MediaDecoder::SeekPromise>
MediaDecoderStateMachine::
StateObject::SetSeekingState(SeekJob&& aSeekJob, EventVisibility aVisibility)
{
  if (aSeekJob.mTarget->IsAccurate() || aSeekJob.mTarget->IsFast()) {
    if (aSeekJob.mTarget->IsVideoOnly()) {
      return SetState<VideoOnlySeekingState>(Move(aSeekJob), aVisibility);
    }
    return SetState<AccurateSeekingState>(Move(aSeekJob), aVisibility);
  }

  if (aSeekJob.mTarget->IsNextFrame()) {
    return SetState<NextFrameSeekingState>(Move(aSeekJob), aVisibility);
  }

  MOZ_ASSERT_UNREACHABLE("Unknown SeekTarget::Type.");
  return nullptr;
}

void
MediaDecoderStateMachine::
DecodeMetadataState::OnMetadataRead(MetadataHolder&& aMetadata)
{
  mMetadataRequest.Complete();

  // Set mode to PLAYBACK after reading metadata.
  Resource()->SetReadMode(MediaCacheStream::MODE_PLAYBACK);

  mMaster->mInfo.emplace(*aMetadata.mInfo);
  mMaster->mMediaSeekable = Info().mMediaSeekable;
  mMaster->mMediaSeekableOnlyInBufferedRanges =
    Info().mMediaSeekableOnlyInBufferedRanges;

  if (Info().mMetadataDuration.isSome()) {
    mMaster->RecomputeDuration();
  } else if (Info().mUnadjustedMetadataEndTime.isSome()) {
    const TimeUnit unadjusted = Info().mUnadjustedMetadataEndTime.ref();
    const TimeUnit adjustment = Info().mStartTime;
    mMaster->mInfo->mMetadataDuration.emplace(unadjusted - adjustment);
    mMaster->RecomputeDuration();
  }

  // If we don't know the duration by this point, we assume infinity, per spec.
  if (mMaster->mDuration.Ref().isNothing()) {
    mMaster->mDuration = Some(TimeUnit::FromInfinity());
  }

  if (mMaster->HasVideo()) {
    SLOG("Video decode isAsync=%d HWAccel=%d videoQueueSize=%d",
         Reader()->IsAsync(),
         Reader()->VideoIsHardwareAccelerated(),
         mMaster->GetAmpleVideoFrames());
  }

  MOZ_ASSERT(mMaster->mDuration.Ref().isSome());

  mMaster->mMetadataLoadedEvent.Notify(
    Move(aMetadata.mInfo),
    Move(aMetadata.mTags),
    MediaDecoderEventVisibility::Observable);

  if (Info().IsEncrypted() && !mMaster->mCDMProxy) {
    // Metadata parsing was successful but we're still waiting for CDM caps
    // to become available so that we can build the correct decryptor/decoder.
    SetState<WaitForCDMState>();
  } else {
    SetState<DecodingFirstFrameState>();
  }
}

void
MediaDecoderStateMachine::
DormantState::HandlePlayStateChanged(MediaDecoder::PlayState aPlayState)
{
  if (aPlayState == MediaDecoder::PLAY_STATE_PLAYING) {
    // Exit dormant when the user wants to play.
    MOZ_ASSERT(!Info().IsEncrypted() || mMaster->mCDMProxy);
    MOZ_ASSERT(mMaster->mSentFirstFrameLoadedEvent);
    SetSeekingState(Move(mPendingSeek), EventVisibility::Suppressed);
  }
}

void
MediaDecoderStateMachine::
WaitForCDMState::HandleCDMProxyReady()
{
  if (mPendingSeek.Exists()) {
    SetSeekingState(Move(mPendingSeek), EventVisibility::Observable);
  } else {
    SetState<DecodingFirstFrameState>();
  }
}

void
MediaDecoderStateMachine::
DecodingFirstFrameState::Enter()
{
  // Transition to DECODING if we've decoded first frames.
  if (mMaster->mSentFirstFrameLoadedEvent) {
    SetState<DecodingState>();
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

void
MediaDecoderStateMachine::
DecodingFirstFrameState::MaybeFinishDecodeFirstFrame()
{
  MOZ_ASSERT(!mMaster->mSentFirstFrameLoadedEvent);

  if ((mMaster->IsAudioDecoding() && AudioQueue().GetSize() == 0)
      || (mMaster->IsVideoDecoding() && VideoQueue().GetSize() == 0)) {
    return;
  }

  mMaster->FinishDecodeFirstFrame();
  if (mPendingSeek.Exists()) {
    SetSeekingState(Move(mPendingSeek), EventVisibility::Observable);
  } else {
    SetState<DecodingState>();
  }
}

void
MediaDecoderStateMachine::
DecodingState::Enter()
{
  MOZ_ASSERT(mMaster->mSentFirstFrameLoadedEvent);

  if (mMaster->mVideoDecodeMode == VideoDecodeMode::Suspend
      && !mMaster->mVideoDecodeSuspendTimer.IsScheduled()
      && !mMaster->mVideoDecodeSuspended) {
    // If the VideoDecodeMode is Suspend and the timer is not schedule, it means
    // the timer has timed out and we should suspend video decoding now if
    // necessary.
    HandleVideoSuspendTimeout();
  }

  if (!mMaster->IsVideoDecoding() && !mMaster->IsAudioDecoding()) {
    SetState<CompletedState>();
    return;
  }

  mOnAudioPopped = AudioQueue().PopEvent().Connect(
    OwnerThread(), [this] () {
    if (mMaster->IsAudioDecoding() && !mMaster->HaveEnoughDecodedAudio()) {
      EnsureAudioDecodeTaskQueued();
    }
  });
  mOnVideoPopped = VideoQueue().PopEvent().Connect(
    OwnerThread(), [this] () {
    if (mMaster->IsVideoDecoding() && !mMaster->HaveEnoughDecodedVideo()) {
      EnsureVideoDecodeTaskQueued();
    }
  });

  mMaster->UpdateNextFrameStatus(MediaDecoderOwner::NEXT_FRAME_AVAILABLE);

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

void
MediaDecoderStateMachine::
DecodingState::HandleEndOfAudio()
{
  AudioQueue().Finish();
  if (!mMaster->IsVideoDecoding()) {
    SetState<CompletedState>();
  } else {
    MaybeStopPrerolling();
  }
}

void
MediaDecoderStateMachine::
DecodingState::HandleEndOfVideo()
{
  VideoQueue().Finish();
  if (!mMaster->IsAudioDecoding()) {
    SetState<CompletedState>();
  } else {
    MaybeStopPrerolling();
  }
}

void
MediaDecoderStateMachine::
DecodingState::DispatchDecodeTasksIfNeeded()
{
  if (mMaster->IsAudioDecoding()
      && !mMaster->mMinimizePreroll
      && !mMaster->HaveEnoughDecodedAudio()) {
    EnsureAudioDecodeTaskQueued();
  }

  if (mMaster->IsVideoDecoding()
      && !mMaster->mMinimizePreroll
      && !mMaster->HaveEnoughDecodedVideo()) {
    EnsureVideoDecodeTaskQueued();
  }
}

void
MediaDecoderStateMachine::
DecodingState::EnsureAudioDecodeTaskQueued()
{
  if (!mMaster->IsAudioDecoding()
      || mMaster->IsRequestingAudioData()
      || mMaster->IsWaitingAudioData()) {
    return;
  }
  mMaster->RequestAudioData();
}

void
MediaDecoderStateMachine::
DecodingState::EnsureVideoDecodeTaskQueued()
{
  if (!mMaster->IsVideoDecoding()
      || mMaster->IsRequestingVideoData()
      || mMaster->IsWaitingVideoData()) {
    return;
  }
  mMaster->RequestVideoData(mMaster->GetMediaTime());
}

void
MediaDecoderStateMachine::
DecodingState::MaybeStartBuffering()
{
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

  bool shouldBuffer;
  if (Reader()->UseBufferingHeuristics()) {
    shouldBuffer = IsExpectingMoreData()
                   && mMaster->HasLowDecodedData()
                   && mMaster->HasLowBufferedData();
  } else {
    shouldBuffer =
      (mMaster->OutOfDecodedAudio() && mMaster->IsWaitingAudioData())
      || (mMaster->OutOfDecodedVideo() && mMaster->IsWaitingVideoData());
  }
  if (shouldBuffer) {
    SetState<BufferingState>();
  }
}

void
MediaDecoderStateMachine::
SeekingState::SeekCompleted()
{
  const auto newCurrentTime = CalculateNewCurrentTime();

  bool isLiveStream = Resource()->IsLiveStream();
  if (newCurrentTime == mMaster->Duration() && !isLiveStream) {
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

  // Cache mTarget for mSeekJob.Resolve() below will reset it.
  SeekTarget target = mSeekJob.mTarget.ref();

  // We want to resolve the seek request prior finishing the first frame
  // to ensure that the seeked event is fired prior loadeded.
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

  // Dispatch an event so that the UI can change in response to the end of
  // video-only seek
  if (target.IsVideoOnly()) {
    mMaster->mOnPlaybackEvent.Notify(MediaEventType::VideoOnlySeekCompleted);
  }

  // Try to decode another frame to detect if we're at the end...
  SLOG("Seek completed, mCurrentPosition=%" PRId64,
       mMaster->mCurrentPosition.Ref().ToMicroseconds());

  if (mMaster->VideoQueue().PeekFront()) {
    mMaster->mMediaSink->Redraw(Info().mVideo);
    mMaster->mOnPlaybackEvent.Notify(MediaEventType::Invalidate);
  }

  GoToNextState();
}

void
MediaDecoderStateMachine::
BufferingState::DispatchDecodeTasksIfNeeded()
{
  if (mMaster->IsAudioDecoding()
      && !mMaster->HaveEnoughDecodedAudio()
      && !mMaster->IsRequestingAudioData()
      && !mMaster->IsWaitingAudioData()) {
    mMaster->RequestAudioData();
  }

  if (mMaster->IsVideoDecoding()
      && !mMaster->HaveEnoughDecodedVideo()
      && !mMaster->IsRequestingVideoData()
      && !mMaster->IsWaitingVideoData()) {
    mMaster->RequestVideoData(media::TimeUnit());
  }
}

void
MediaDecoderStateMachine::
BufferingState::Step()
{
  TimeStamp now = TimeStamp::Now();
  MOZ_ASSERT(!mBufferingStart.IsNull(), "Must know buffering start time.");

  // With buffering heuristics we will remain in the buffering state if
  // we've not decoded enough data to begin playback, or if we've not
  // downloaded a reasonable amount of data inside our buffering time.
  if (Reader()->UseBufferingHeuristics()) {
    TimeDuration elapsed = now - mBufferingStart;
    bool isLiveStream = Resource()->IsLiveStream();
    if ((isLiveStream || !mMaster->CanPlayThrough())
        && elapsed
           < TimeDuration::FromSeconds(mBufferingWait * mMaster->mPlaybackRate)
        && mMaster->HasLowBufferedData(TimeUnit::FromSeconds(mBufferingWait))
        && IsExpectingMoreData()) {
      SLOG("Buffering: wait %ds, timeout in %.3lfs",
           mBufferingWait, mBufferingWait - elapsed.ToSeconds());
      mMaster->ScheduleStateMachineIn(TimeUnit::FromMicroseconds(USECS_PER_S));
      DispatchDecodeTasksIfNeeded();
      return;
    }
  } else if (mMaster->OutOfDecodedAudio() || mMaster->OutOfDecodedVideo()) {
    DispatchDecodeTasksIfNeeded();
    MOZ_ASSERT(!mMaster->OutOfDecodedAudio()
               || mMaster->IsRequestingAudioData()
               || mMaster->IsWaitingAudioData());
    MOZ_ASSERT(!mMaster->OutOfDecodedVideo()
               || mMaster->IsRequestingVideoData()
               || mMaster->IsWaitingVideoData());
    SLOG("In buffering mode, waiting to be notified: outOfAudio: %d, "
         "mAudioStatus: %s, outOfVideo: %d, mVideoStatus: %s",
         mMaster->OutOfDecodedAudio(), mMaster->AudioRequestStatus(),
         mMaster->OutOfDecodedVideo(), mMaster->VideoRequestStatus());
    return;
  }

  SLOG("Buffered for %.3lfs", (now - mBufferingStart).ToSeconds());
  SetState<DecodingState>();
}

void
MediaDecoderStateMachine::
BufferingState::HandleEndOfAudio()
{
  AudioQueue().Finish();
  if (!mMaster->IsVideoDecoding()) {
    SetState<CompletedState>();
  } else {
    // Check if we can exit buffering.
    mMaster->ScheduleStateMachine();
  }
}

void
MediaDecoderStateMachine::
BufferingState::HandleEndOfVideo()
{
  VideoQueue().Finish();
  if (!mMaster->IsAudioDecoding()) {
    SetState<CompletedState>();
  } else {
    // Check if we can exit buffering.
    mMaster->ScheduleStateMachine();
  }
}

RefPtr<ShutdownPromise>
MediaDecoderStateMachine::
ShutdownState::Enter()
{
  auto master = mMaster;

  master->mDelayedScheduler.Reset();

  // Shutdown happens while decode timer is active, we need to disconnect and
  // dispose of the timer.
  master->CancelSuspendTimer();

  master->mCDMProxyPromise.DisconnectIfExists();

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
  master->mExplicitDuration.DisconnectIfConnected();
  master->mPlayState.DisconnectIfConnected();
  master->mNextPlayState.DisconnectIfConnected();
  master->mVolume.DisconnectIfConnected();
  master->mPreservesPitch.DisconnectIfConnected();
  master->mLooping.DisconnectIfConnected();
  master->mSameOriginMedia.DisconnectIfConnected();
  master->mMediaPrincipalHandle.DisconnectIfConnected();
  master->mPlaybackBytesPerSecond.DisconnectIfConnected();
  master->mPlaybackRateReliable.DisconnectIfConnected();
  master->mDecoderPosition.DisconnectIfConnected();

  master->mDuration.DisconnectAll();
  master->mNextFrameStatus.DisconnectAll();
  master->mCurrentPosition.DisconnectAll();
  master->mPlaybackOffset.DisconnectAll();
  master->mIsAudioDataAudible.DisconnectAll();

  // Shut down the watch manager to stop further notifications.
  master->mWatchManager.Shutdown();

  return Reader()->Shutdown()->Then(
    OwnerThread(), __func__, master,
    &MediaDecoderStateMachine::FinishShutdown,
    &MediaDecoderStateMachine::FinishShutdown);
}

#define INIT_WATCHABLE(name, val) \
  name(val, "MediaDecoderStateMachine::" #name)
#define INIT_MIRROR(name, val) \
  name(mTaskQueue, val, "MediaDecoderStateMachine::" #name " (Mirror)")
#define INIT_CANONICAL(name, val) \
  name(mTaskQueue, val, "MediaDecoderStateMachine::" #name " (Canonical)")

MediaDecoderStateMachine::MediaDecoderStateMachine(MediaDecoder* aDecoder,
                                                   MediaDecoderReader* aReader) :
  mDecoderID(aDecoder),
  mAbstractMainThread(aDecoder->AbstractMainThread()),
  mFrameStats(&aDecoder->GetFrameStatistics()),
  mVideoFrameContainer(aDecoder->GetVideoFrameContainer()),
  mAudioChannel(aDecoder->GetAudioChannel()),
  mTaskQueue(new TaskQueue(
    GetMediaThreadPool(MediaThreadType::PLAYBACK),
    "MDSM::mTaskQueue", /* aSupportsTailDispatch = */ true)),
  mWatchManager(this, mTaskQueue),
  mDispatchedStateMachine(false),
  mDelayedScheduler(mTaskQueue),
  mCurrentFrameID(0),
  INIT_WATCHABLE(mObservedDuration, TimeUnit()),
  mReader(new MediaDecoderReaderWrapper(mTaskQueue, aReader)),
  mPlaybackRate(1.0),
  mLowAudioThreshold(detail::LOW_AUDIO_THRESHOLD),
  mAmpleAudioThreshold(detail::AMPLE_AUDIO_THRESHOLD),
  mAudioCaptured(false),
  mMinimizePreroll(aDecoder->GetMinimizePreroll()),
  mSentFirstFrameLoadedEvent(false),
  mVideoDecodeSuspended(false),
  mVideoDecodeSuspendTimer(mTaskQueue),
  mOutputStreamManager(new OutputStreamManager()),
  mResource(aDecoder->GetResource()),
  mVideoDecodeMode(VideoDecodeMode::Normal),
  mIsMSE(aDecoder->IsMSE()),
  INIT_MIRROR(mBuffered, TimeIntervals()),
  INIT_MIRROR(mExplicitDuration, Maybe<double>()),
  INIT_MIRROR(mPlayState, MediaDecoder::PLAY_STATE_LOADING),
  INIT_MIRROR(mNextPlayState, MediaDecoder::PLAY_STATE_PAUSED),
  INIT_MIRROR(mVolume, 1.0),
  INIT_MIRROR(mPreservesPitch, true),
  INIT_MIRROR(mLooping, false),
  INIT_MIRROR(mSameOriginMedia, false),
  INIT_MIRROR(mMediaPrincipalHandle, PRINCIPAL_HANDLE_NONE),
  INIT_MIRROR(mPlaybackBytesPerSecond, 0.0),
  INIT_MIRROR(mPlaybackRateReliable, true),
  INIT_MIRROR(mDecoderPosition, 0),
  INIT_CANONICAL(mDuration, NullableTimeUnit()),
  INIT_CANONICAL(mNextFrameStatus, MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE),
  INIT_CANONICAL(mCurrentPosition, TimeUnit::Zero()),
  INIT_CANONICAL(mPlaybackOffset, 0),
  INIT_CANONICAL(mIsAudioDataAudible, false)
#ifdef XP_WIN
  , mShouldUseHiResTimers(Preferences::GetBool("media.hi-res-timers.enabled", true))
#endif
{
  MOZ_COUNT_CTOR(MediaDecoderStateMachine);
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  InitVideoQueuePrefs();
}

#undef INIT_WATCHABLE
#undef INIT_MIRROR
#undef INIT_CANONICAL

MediaDecoderStateMachine::~MediaDecoderStateMachine()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  MOZ_COUNT_DTOR(MediaDecoderStateMachine);

#ifdef XP_WIN
  MOZ_ASSERT(!mHiResTimersRequested);
#endif
}

void
MediaDecoderStateMachine::InitializationTask(MediaDecoder* aDecoder)
{
  MOZ_ASSERT(OnTaskQueue());

  // Connect mirrors.
  mBuffered.Connect(mReader->CanonicalBuffered());
  mExplicitDuration.Connect(aDecoder->CanonicalExplicitDuration());
  mPlayState.Connect(aDecoder->CanonicalPlayState());
  mNextPlayState.Connect(aDecoder->CanonicalNextPlayState());
  mVolume.Connect(aDecoder->CanonicalVolume());
  mPreservesPitch.Connect(aDecoder->CanonicalPreservesPitch());
  mLooping.Connect(aDecoder->CanonicalLooping());
  mSameOriginMedia.Connect(aDecoder->CanonicalSameOriginMedia());
  mMediaPrincipalHandle.Connect(aDecoder->CanonicalMediaPrincipalHandle());
  mPlaybackBytesPerSecond.Connect(aDecoder->CanonicalPlaybackBytesPerSecond());
  mPlaybackRateReliable.Connect(aDecoder->CanonicalPlaybackRateReliable());
  mDecoderPosition.Connect(aDecoder->CanonicalDecoderPosition());

  // Initialize watchers.
  mWatchManager.Watch(mBuffered,
                      &MediaDecoderStateMachine::BufferedRangeUpdated);
  mWatchManager.Watch(mVolume, &MediaDecoderStateMachine::VolumeChanged);
  mWatchManager.Watch(mPreservesPitch,
                      &MediaDecoderStateMachine::PreservesPitchChanged);
  mWatchManager.Watch(mExplicitDuration,
                      &MediaDecoderStateMachine::RecomputeDuration);
  mWatchManager.Watch(mObservedDuration,
                      &MediaDecoderStateMachine::RecomputeDuration);
  mWatchManager.Watch(mPlayState, &MediaDecoderStateMachine::PlayStateChanged);

  MOZ_ASSERT(!mStateObj);
  auto* s = new DecodeMetadataState(this);
  mStateObj.reset(s);
  s->Enter();
}

void
MediaDecoderStateMachine::AudioAudibleChanged(bool aAudible)
{
  mIsAudioDataAudible = aAudible;
}

media::MediaSink*
MediaDecoderStateMachine::CreateAudioSink()
{
  RefPtr<MediaDecoderStateMachine> self = this;
  auto audioSinkCreator = [self] () {
    MOZ_ASSERT(self->OnTaskQueue());
    AudioSink* audioSink = new AudioSink(
      self->mTaskQueue, self->mAudioQueue,
      self->GetMediaTime(),
      self->Info().mAudio, self->mAudioChannel);

    self->mAudibleListener = audioSink->AudibleEvent().Connect(
      self->mTaskQueue, self.get(),
      &MediaDecoderStateMachine::AudioAudibleChanged);
    return audioSink;
  };
  return new AudioSinkWrapper(mTaskQueue, audioSinkCreator);
}

already_AddRefed<media::MediaSink>
MediaDecoderStateMachine::CreateMediaSink(bool aAudioCaptured)
{
  RefPtr<media::MediaSink> audioSink =
    aAudioCaptured
    ? new DecodedStream(mTaskQueue, mAbstractMainThread, mAudioQueue,
                        mVideoQueue, mOutputStreamManager,
                        mSameOriginMedia.Ref(), mMediaPrincipalHandle.Ref())
    : CreateAudioSink();

  RefPtr<media::MediaSink> mediaSink =
    new VideoSink(mTaskQueue, audioSink, mVideoQueue,
                  mVideoFrameContainer, *mFrameStats,
                  sVideoQueueSendToCompositorSize);
  return mediaSink.forget();
}

TimeUnit
MediaDecoderStateMachine::GetDecodedAudioDuration()
{
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

bool
MediaDecoderStateMachine::HaveEnoughDecodedAudio()
{
  MOZ_ASSERT(OnTaskQueue());
  auto ampleAudio = mAmpleAudioThreshold.MultDouble(mPlaybackRate);
  return AudioQueue().GetSize() > 0
         && GetDecodedAudioDuration() >= ampleAudio;
}

bool MediaDecoderStateMachine::HaveEnoughDecodedVideo()
{
  MOZ_ASSERT(OnTaskQueue());
  return VideoQueue().GetSize() >= GetAmpleVideoFrames() * mPlaybackRate + 1;
}

void
MediaDecoderStateMachine::PushAudio(AudioData* aSample)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(aSample);
  AudioQueue().Push(aSample);
}

void
MediaDecoderStateMachine::PushVideo(VideoData* aSample)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(aSample);
  aSample->mFrameID = ++mCurrentFrameID;
  VideoQueue().Push(aSample);
}

void
MediaDecoderStateMachine::OnAudioPopped(const RefPtr<AudioData>& aSample)
{
  MOZ_ASSERT(OnTaskQueue());
  mPlaybackOffset = std::max(mPlaybackOffset.Ref(), aSample->mOffset);
}

void
MediaDecoderStateMachine::OnVideoPopped(const RefPtr<VideoData>& aSample)
{
  MOZ_ASSERT(OnTaskQueue());
  mPlaybackOffset = std::max(mPlaybackOffset.Ref(), aSample->mOffset);
}

bool
MediaDecoderStateMachine::IsAudioDecoding()
{
  MOZ_ASSERT(OnTaskQueue());
  return HasAudio() && !AudioQueue().IsFinished();
}

bool
MediaDecoderStateMachine::IsVideoDecoding()
{
  MOZ_ASSERT(OnTaskQueue());
  return HasVideo() && !VideoQueue().IsFinished();
}

bool MediaDecoderStateMachine::IsPlaying() const
{
  MOZ_ASSERT(OnTaskQueue());
  return mMediaSink->IsPlaying();
}

void MediaDecoderStateMachine::SetMediaNotSeekable()
{
  mMediaSeekable = false;
}

nsresult MediaDecoderStateMachine::Init(MediaDecoder* aDecoder)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Dispatch initialization that needs to happen on that task queue.
  nsCOMPtr<nsIRunnable> r = NewRunnableMethod<RefPtr<MediaDecoder>>(
    "MediaDecoderStateMachine::InitializationTask",
    this,
    &MediaDecoderStateMachine::InitializationTask,
    aDecoder);
  mTaskQueue->DispatchStateChange(r.forget());

  mAudioQueueListener = AudioQueue().PopEvent().Connect(
    mTaskQueue, this, &MediaDecoderStateMachine::OnAudioPopped);
  mVideoQueueListener = VideoQueue().PopEvent().Connect(
    mTaskQueue, this, &MediaDecoderStateMachine::OnVideoPopped);

  mMetadataManager.Connect(mReader->TimedMetadataEvent(), OwnerThread());

  mOnMediaNotSeekable = mReader->OnMediaNotSeekable().Connect(
    OwnerThread(), this, &MediaDecoderStateMachine::SetMediaNotSeekable);

  mMediaSink = CreateMediaSink(mAudioCaptured);

  aDecoder->RequestCDMProxy()->Then(
    OwnerThread(), __func__, this,
    &MediaDecoderStateMachine::OnCDMProxyReady,
    &MediaDecoderStateMachine::OnCDMProxyNotReady)
  ->Track(mCDMProxyPromise);

  nsresult rv = mReader->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
MediaDecoderStateMachine::StopPlayback()
{
  MOZ_ASSERT(OnTaskQueue());
  LOG("StopPlayback()");

  mOnPlaybackEvent.Notify(MediaEventType::PlaybackStopped);

  if (IsPlaying()) {
    mMediaSink->SetPlaying(false);
    MOZ_ASSERT(!IsPlaying());
#ifdef XP_WIN
    if (mHiResTimersRequested) {
      mHiResTimersRequested = false;
      timeEndPeriod(1);
    }
#endif
  }
}

void MediaDecoderStateMachine::MaybeStartPlayback()
{
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
  mOnPlaybackEvent.Notify(MediaEventType::PlaybackStarted);
  StartMediaSink();

#ifdef XP_WIN
  if (!mHiResTimersRequested && mShouldUseHiResTimers) {
    mHiResTimersRequested = true;
    // Ensure high precision timers are enabled on Windows, otherwise the state
    // machine isn't woken up at reliable intervals to set the next frame, and we
    // drop frames while painting. Note that each call must be matched by a
    // corresponding timeEndPeriod() call. Enabling high precision timers causes
    // the CPU to wake up more frequently on Windows 7 and earlier, which causes
    // more CPU load and battery use. So we only enable high precision timers
    // when we're actually playing.
    timeBeginPeriod(1);
  }
#endif

  if (!IsPlaying()) {
    mMediaSink->SetPlaying(true);
    MOZ_ASSERT(IsPlaying());
  }
}

void
MediaDecoderStateMachine::UpdatePlaybackPositionInternal(const TimeUnit& aTime)
{
  MOZ_ASSERT(OnTaskQueue());
  LOGV("UpdatePlaybackPositionInternal(%" PRId64 ")", aTime.ToMicroseconds());

  mCurrentPosition = aTime;
  NS_ASSERTION(mCurrentPosition.Ref() >= TimeUnit::Zero(),
               "CurrentTime should be positive!");
  mObservedDuration = std::max(mObservedDuration.Ref(), mCurrentPosition.Ref());
}

void
MediaDecoderStateMachine::UpdatePlaybackPosition(const TimeUnit& aTime)
{
  MOZ_ASSERT(OnTaskQueue());
  UpdatePlaybackPositionInternal(aTime);

  bool fragmentEnded = mFragmentEndTime.IsValid()
    && GetMediaTime() >= mFragmentEndTime;
  mMetadataManager.DispatchMetadataIfNeeded(aTime);

  if (fragmentEnded) {
    StopPlayback();
  }
}

/* static */ const char*
MediaDecoderStateMachine::ToStateStr(State aState)
{
  switch (aState) {
    case DECODER_STATE_DECODING_METADATA:   return "DECODING_METADATA";
    case DECODER_STATE_WAIT_FOR_CDM:        return "WAIT_FOR_CDM";
    case DECODER_STATE_DORMANT:             return "DORMANT";
    case DECODER_STATE_DECODING_FIRSTFRAME: return "DECODING_FIRSTFRAME";
    case DECODER_STATE_DECODING:            return "DECODING";
    case DECODER_STATE_SEEKING:             return "SEEKING";
    case DECODER_STATE_BUFFERING:           return "BUFFERING";
    case DECODER_STATE_COMPLETED:           return "COMPLETED";
    case DECODER_STATE_SHUTDOWN:            return "SHUTDOWN";
    default: MOZ_ASSERT_UNREACHABLE("Invalid state.");
  }
  return "UNKNOWN";
}

const char*
MediaDecoderStateMachine::ToStateStr()
{
  MOZ_ASSERT(OnTaskQueue());
  return ToStateStr(mStateObj->GetState());
}

void MediaDecoderStateMachine::VolumeChanged()
{
  MOZ_ASSERT(OnTaskQueue());
  mMediaSink->SetVolume(mVolume);
}

void MediaDecoderStateMachine::RecomputeDuration()
{
  MOZ_ASSERT(OnTaskQueue());

  TimeUnit duration;
  if (mExplicitDuration.Ref().isSome()) {
    double d = mExplicitDuration.Ref().ref();
    if (IsNaN(d)) {
      // We have an explicit duration (which means that we shouldn't look at
      // any other duration sources), but the duration isn't ready yet.
      return;
    }
    // We don't fire duration changed for this case because it should have
    // already been fired on the main thread when the explicit duration was set.
    duration = TimeUnit::FromSeconds(d);
  } else if (mInfo.isSome() && Info().mMetadataDuration.isSome()) {
    // We need to check mInfo.isSome() because that this method might be invoked
    // while mObservedDuration is changed which might before the metadata been
    // read.
    duration = Info().mMetadataDuration.ref();
  } else {
    return;
  }

  // Only adjust the duration when an explicit duration isn't set (MSE).
  // The duration is always exactly known with MSE and there's no need to adjust
  // it based on what may have been seen in the past; in particular as this data
  // may no longer exist such as when the mediasource duration was reduced.
  if (mExplicitDuration.Ref().isNothing()
      && duration < mObservedDuration.Ref()) {
    duration = mObservedDuration;
  }

  MOZ_ASSERT(duration >= TimeUnit::Zero());
  mDuration = Some(duration);
}

RefPtr<ShutdownPromise>
MediaDecoderStateMachine::Shutdown()
{
  MOZ_ASSERT(OnTaskQueue());
  return mStateObj->HandleShutdown();
}

void MediaDecoderStateMachine::PlayStateChanged()
{
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

void MediaDecoderStateMachine::SetVideoDecodeMode(VideoDecodeMode aMode)
{
  nsCOMPtr<nsIRunnable> r = NewRunnableMethod<VideoDecodeMode>(
    "MediaDecoderStateMachine::SetVideoDecodeModeInternal",
    this,
    &MediaDecoderStateMachine::SetVideoDecodeModeInternal,
    aMode);
  OwnerThread()->DispatchStateChange(r.forget());
}

void MediaDecoderStateMachine::SetVideoDecodeModeInternal(VideoDecodeMode aMode)
{
  MOZ_ASSERT(OnTaskQueue());
  LOG("VideoDecodeModeChanged: VideoDecodeMode=(%s->%s), mVideoDecodeSuspended=%c",
      mVideoDecodeMode == VideoDecodeMode::Normal ? "Normal" : "Suspend",
      aMode == VideoDecodeMode::Normal ? "Normal" : "Suspend",
      mVideoDecodeSuspended ? 'T' : 'F');

  if (!MediaPrefs::MDSMSuspendBackgroundVideoEnabled()) {
    return;
  }

  if (aMode == mVideoDecodeMode) {
    return;
  }

  // Set new video decode mode.
  mVideoDecodeMode = aMode;

  // Start timer to trigger suspended video decoding.
  if (mVideoDecodeMode == VideoDecodeMode::Suspend) {
    TimeStamp target = TimeStamp::Now() + SuspendBackgroundVideoDelay();

    RefPtr<MediaDecoderStateMachine> self = this;
    mVideoDecodeSuspendTimer.Ensure(target,
                                    [=]() { self->OnSuspendTimerResolved(); },
                                    [] () { MOZ_DIAGNOSTIC_ASSERT(false); });
    mOnPlaybackEvent.Notify(MediaEventType::StartVideoSuspendTimer);
    return;
  }

  // Resuming from suspended decoding

  // If suspend timer exists, destroy it.
  CancelSuspendTimer();

  if (mVideoDecodeSuspended) {
    mStateObj->HandleResumeVideoDecoding(GetMediaTime());
  }
}

void MediaDecoderStateMachine::BufferedRangeUpdated()
{
  MOZ_ASSERT(OnTaskQueue());

  // While playing an unseekable stream of unknown duration, mObservedDuration
  // is updated (in AdvanceFrame()) as we play. But if data is being downloaded
  // faster than played, mObserved won't reflect the end of playable data
  // since we haven't played the frame at the end of buffered data. So update
  // mObservedDuration here as new data is downloaded to prevent such a lag.
  if (!mBuffered.Ref().IsInvalid()) {
    bool exists;
    media::TimeUnit end{mBuffered.Ref().GetEnd(&exists)};
    if (exists) {
      mObservedDuration = std::max(mObservedDuration.Ref(), end);
    }
  }
}

RefPtr<MediaDecoder::SeekPromise>
MediaDecoderStateMachine::Seek(const SeekTarget& aTarget)
{
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

RefPtr<MediaDecoder::SeekPromise>
MediaDecoderStateMachine::InvokeSeek(const SeekTarget& aTarget)
{
  return InvokeAsync(
           OwnerThread(), this, __func__,
           &MediaDecoderStateMachine::Seek, aTarget);
}

void MediaDecoderStateMachine::StopMediaSink()
{
  MOZ_ASSERT(OnTaskQueue());
  if (mMediaSink->IsStarted()) {
    LOG("Stop MediaSink");
    mAudibleListener.DisconnectIfExists();

    mMediaSink->Stop();
    mMediaSinkAudioPromise.DisconnectIfExists();
    mMediaSinkVideoPromise.DisconnectIfExists();
  }
}

void
MediaDecoderStateMachine::RequestAudioData()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(IsAudioDecoding());
  MOZ_ASSERT(!IsRequestingAudioData());
  MOZ_ASSERT(!IsWaitingAudioData());
  LOGV("Queueing audio task - queued=%" PRIuSIZE ", decoder-queued=%" PRIuSIZE,
       AudioQueue().GetSize(), mReader->SizeOfAudioQueueInFrames());

  RefPtr<MediaDecoderStateMachine> self = this;
  mReader->RequestAudioData()->Then(
    OwnerThread(), __func__,
    [this, self] (RefPtr<AudioData> aAudio) {
      MOZ_ASSERT(aAudio);
      mAudioDataRequest.Complete();
      // audio->GetEndTime() is not always mono-increasing in chained ogg.
      mDecodedAudioEndTime = std::max(
        aAudio->GetEndTime(), mDecodedAudioEndTime);
      LOGV("OnAudioDecoded [%" PRId64 ",%" PRId64 "]",
           aAudio->mTime.ToMicroseconds(),
           aAudio->GetEndTime().ToMicroseconds());
      mStateObj->HandleAudioDecoded(aAudio);
    },
    [this, self] (const MediaResult& aError) {
      LOGV("OnAudioNotDecoded aError=%" PRIu32, static_cast<uint32_t>(aError.Code()));
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
    })->Track(mAudioDataRequest);
}

void
MediaDecoderStateMachine::RequestVideoData(const media::TimeUnit& aCurrentTime)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(IsVideoDecoding());
  MOZ_ASSERT(!IsRequestingVideoData());
  MOZ_ASSERT(!IsWaitingVideoData());
  LOGV("Queueing video task - queued=%" PRIuSIZE ", decoder-queued=%" PRIoSIZE
       ", stime=%" PRId64,
       VideoQueue().GetSize(), mReader->SizeOfVideoQueueInFrames(),
       aCurrentTime.ToMicroseconds());

  TimeStamp videoDecodeStartTime = TimeStamp::Now();
  RefPtr<MediaDecoderStateMachine> self = this;
  mReader->RequestVideoData(aCurrentTime)->Then(
    OwnerThread(), __func__,
    [this, self, videoDecodeStartTime] (RefPtr<VideoData> aVideo) {
      MOZ_ASSERT(aVideo);
      mVideoDataRequest.Complete();
      // Handle abnormal or negative timestamps.
      mDecodedVideoEndTime = std::max(
        mDecodedVideoEndTime, aVideo->GetEndTime());
      LOGV("OnVideoDecoded [%" PRId64 ",%" PRId64 "]",
           aVideo->mTime.ToMicroseconds(),
           aVideo->GetEndTime().ToMicroseconds());
      mStateObj->HandleVideoDecoded(aVideo, videoDecodeStartTime);
    },
    [this, self] (const MediaResult& aError) {
      LOGV("OnVideoNotDecoded aError=%" PRIu32 , static_cast<uint32_t>(aError.Code()));
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
    })->Track(mVideoDataRequest);
}

void
MediaDecoderStateMachine::WaitForData(MediaData::Type aType)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(aType == MediaData::AUDIO_DATA || aType == MediaData::VIDEO_DATA);
  RefPtr<MediaDecoderStateMachine> self = this;
  if (aType == MediaData::AUDIO_DATA) {
    mReader->WaitForData(MediaData::AUDIO_DATA)->Then(
      OwnerThread(), __func__,
      [self] (MediaData::Type aType) {
        self->mAudioWaitRequest.Complete();
        MOZ_ASSERT(aType == MediaData::AUDIO_DATA);
        self->mStateObj->HandleAudioWaited(aType);
      },
      [self] (const WaitForDataRejectValue& aRejection) {
        self->mAudioWaitRequest.Complete();
        self->DecodeError(NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA);
      })->Track(mAudioWaitRequest);
  } else {
    mReader->WaitForData(MediaData::VIDEO_DATA)->Then(
      OwnerThread(), __func__,
      [self] (MediaData::Type aType) {
        self->mVideoWaitRequest.Complete();
        MOZ_ASSERT(aType == MediaData::VIDEO_DATA);
        self->mStateObj->HandleVideoWaited(aType);
      },
      [self] (const WaitForDataRejectValue& aRejection) {
        self->mVideoWaitRequest.Complete();
        self->DecodeError(NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA);
      })->Track(mVideoWaitRequest);
  }
}

void
MediaDecoderStateMachine::StartMediaSink()
{
  MOZ_ASSERT(OnTaskQueue());
  if (!mMediaSink->IsStarted()) {
    mAudioCompleted = false;
    mMediaSink->Start(GetMediaTime(), Info());

    auto videoPromise = mMediaSink->OnEnded(TrackInfo::kVideoTrack);
    auto audioPromise = mMediaSink->OnEnded(TrackInfo::kAudioTrack);

    if (audioPromise) {
      audioPromise->Then(
        OwnerThread(), __func__, this,
        &MediaDecoderStateMachine::OnMediaSinkAudioComplete,
        &MediaDecoderStateMachine::OnMediaSinkAudioError)
      ->Track(mMediaSinkAudioPromise);
    }
    if (videoPromise) {
      videoPromise->Then(
        OwnerThread(), __func__, this,
        &MediaDecoderStateMachine::OnMediaSinkVideoComplete,
        &MediaDecoderStateMachine::OnMediaSinkVideoError)
      ->Track(mMediaSinkVideoPromise);
    }
  }
}

bool
MediaDecoderStateMachine::HasLowDecodedAudio()
{
  MOZ_ASSERT(OnTaskQueue());
  return IsAudioDecoding()
         && GetDecodedAudioDuration()
            < EXHAUSTED_DATA_MARGIN.MultDouble(mPlaybackRate);
}

bool
MediaDecoderStateMachine::HasLowDecodedVideo()
{
  MOZ_ASSERT(OnTaskQueue());
  return IsVideoDecoding()
         && VideoQueue().GetSize() < LOW_VIDEO_FRAMES * mPlaybackRate;
}

bool
MediaDecoderStateMachine::HasLowDecodedData()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mReader->UseBufferingHeuristics());
  return HasLowDecodedAudio() || HasLowDecodedVideo();
}

bool MediaDecoderStateMachine::OutOfDecodedAudio()
{
    MOZ_ASSERT(OnTaskQueue());
    return IsAudioDecoding() && !AudioQueue().IsFinished()
           && AudioQueue().GetSize() == 0
           && !mMediaSink->HasUnplayedFrames(TrackInfo::kAudioTrack);
}

bool
MediaDecoderStateMachine::HasLowBufferedData()
{
  MOZ_ASSERT(OnTaskQueue());
  return HasLowBufferedData(detail::LOW_BUFFER_THRESHOLD);
}

bool
MediaDecoderStateMachine::HasLowBufferedData(const TimeUnit& aThreshold)
{
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
    ? mDecodedVideoEndTime : TimeUnit::FromInfinity();
  TimeUnit endOfDecodedAudio = (HasAudio() && !AudioQueue().IsFinished())
    ? mDecodedAudioEndTime : TimeUnit::FromInfinity();

  auto endOfDecodedData = std::min(endOfDecodedVideo, endOfDecodedAudio);
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

void
MediaDecoderStateMachine::DecodeError(const MediaResult& aError)
{
  MOZ_ASSERT(OnTaskQueue());
  LOGW("Decode error");
  // Notify the decode error and MediaDecoder will shut down MDSM.
  mOnPlaybackErrorEvent.Notify(aError);
}

void
MediaDecoderStateMachine::EnqueueFirstFrameLoadedEvent()
{
  MOZ_ASSERT(OnTaskQueue());
  // Track value of mSentFirstFrameLoadedEvent from before updating it
  bool firstFrameBeenLoaded = mSentFirstFrameLoadedEvent;
  mSentFirstFrameLoadedEvent = true;
  MediaDecoderEventVisibility visibility =
    firstFrameBeenLoaded ? MediaDecoderEventVisibility::Suppressed
                         : MediaDecoderEventVisibility::Observable;
  mFirstFrameLoadedEvent.Notify(
    nsAutoPtr<MediaInfo>(new MediaInfo(Info())), visibility);
}

void
MediaDecoderStateMachine::FinishDecodeFirstFrame()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(!mSentFirstFrameLoadedEvent);
  LOG("FinishDecodeFirstFrame");

  mMediaSink->Redraw(Info().mVideo);

  LOG("Media duration %" PRId64 ", "
      "transportSeekable=%d, mediaSeekable=%d",
      Duration().ToMicroseconds(), mResource->IsTransportSeekable(),
      mMediaSeekable);

  // Get potentially updated metadata
  mReader->ReadUpdatedMetadata(mInfo.ptr());

  EnqueueFirstFrameLoadedEvent();
}

RefPtr<ShutdownPromise>
MediaDecoderStateMachine::BeginShutdown()
{
  return InvokeAsync(OwnerThread(), this, __func__,
                     &MediaDecoderStateMachine::Shutdown);
}

RefPtr<ShutdownPromise>
MediaDecoderStateMachine::FinishShutdown()
{
  MOZ_ASSERT(OnTaskQueue());
  LOG("Shutting down state machine task queue");
  return OwnerThread()->BeginShutdown();
}

void
MediaDecoderStateMachine::RunStateMachine()
{
  MOZ_ASSERT(OnTaskQueue());

  mDelayedScheduler.Reset(); // Must happen on state machine task queue.
  mDispatchedStateMachine = false;
  mStateObj->Step();
}

void
MediaDecoderStateMachine::ResetDecode(TrackSet aTracks)
{
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

  mPlaybackOffset = 0;

  mReader->ResetDecode(aTracks);
}

media::TimeUnit
MediaDecoderStateMachine::GetClock(TimeStamp* aTimeStamp) const
{
  MOZ_ASSERT(OnTaskQueue());
  auto clockTime = mMediaSink->GetPosition(aTimeStamp);
  NS_ASSERTION(GetMediaTime() <= clockTime, "Clock should go forwards.");
  return clockTime;
}

void
MediaDecoderStateMachine::UpdatePlaybackPositionPeriodically()
{
  MOZ_ASSERT(OnTaskQueue());

  if (!IsPlaying()) {
    return;
  }

  // Cap the current time to the larger of the audio and video end time.
  // This ensures that if we're running off the system clock, we don't
  // advance the clock to after the media end time.
  if (VideoEndTime() > TimeUnit::Zero() || AudioEndTime() > TimeUnit::Zero()) {

    const auto clockTime = GetClock();
    // Skip frames up to the frame at the playback position, and figure out
    // the time remaining until it's time to display the next frame and drop
    // the current frame.
    NS_ASSERTION(clockTime >= TimeUnit::Zero(), "Should have positive clock time.");

    // These will be non -1 if we've displayed a video frame, or played an audio
    // frame.
    auto maxEndTime = std::max(VideoEndTime(), AudioEndTime());
    auto t = std::min(clockTime, maxEndTime);
    // FIXME: Bug 1091422 - chained ogg files hit this assertion.
    //MOZ_ASSERT(t >= GetMediaTime());
    if (t > GetMediaTime()) {
      UpdatePlaybackPosition(t);
    }
  }
  // Note we have to update playback position before releasing the monitor.
  // Otherwise, MediaDecoder::AddOutputStream could kick in when we are outside
  // the monitor and get a staled value from GetCurrentTimeUs() which hits the
  // assertion in GetClock().

  int64_t delay = std::max<int64_t>(1, AUDIO_DURATION_USECS / mPlaybackRate);
  ScheduleStateMachineIn(TimeUnit::FromMicroseconds(delay));
}

/* static */ const char*
MediaDecoderStateMachine::ToStr(NextFrameStatus aStatus)
{
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

void
MediaDecoderStateMachine::UpdateNextFrameStatus(NextFrameStatus aStatus)
{
  MOZ_ASSERT(OnTaskQueue());
  if (aStatus != mNextFrameStatus) {
    LOG("Changed mNextFrameStatus to %s", ToStr(aStatus));
    mNextFrameStatus = aStatus;
  }
}

bool
MediaDecoderStateMachine::CanPlayThrough()
{
  MOZ_ASSERT(OnTaskQueue());
  return GetStatistics().CanPlayThrough();
}

MediaStatistics
MediaDecoderStateMachine::GetStatistics()
{
  MOZ_ASSERT(OnTaskQueue());
  MediaStatistics result;
  result.mDownloadRate =
    mResource->GetDownloadRate(&result.mDownloadRateReliable);
  result.mDownloadPosition = mResource->GetCachedDataEnd(mDecoderPosition);
  result.mTotalBytes = mResource->GetLength();
  result.mPlaybackRate = mPlaybackBytesPerSecond;
  result.mPlaybackRateReliable = mPlaybackRateReliable;
  result.mDecoderPosition = mDecoderPosition;
  result.mPlaybackPosition = mPlaybackOffset;
  return result;
}

void
MediaDecoderStateMachine::ScheduleStateMachine()
{
  MOZ_ASSERT(OnTaskQueue());
  if (mDispatchedStateMachine) {
    return;
  }
  mDispatchedStateMachine = true;

  OwnerThread()->Dispatch(
    NewRunnableMethod("MediaDecoderStateMachine::RunStateMachine",
                      this,
                      &MediaDecoderStateMachine::RunStateMachine));
}

void
MediaDecoderStateMachine::ScheduleStateMachineIn(const TimeUnit& aTime)
{
  MOZ_ASSERT(OnTaskQueue()); // mDelayedScheduler.Ensure() may Disconnect()
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
  mDelayedScheduler.Ensure(target, [self] () {
    self->mDelayedScheduler.CompleteRequest();
    self->RunStateMachine();
  }, [] () {
    MOZ_DIAGNOSTIC_ASSERT(false);
  });
}

bool MediaDecoderStateMachine::OnTaskQueue() const
{
  return OwnerThread()->IsCurrentThreadIn();
}

bool MediaDecoderStateMachine::IsStateMachineScheduled() const
{
  MOZ_ASSERT(OnTaskQueue());
  return mDispatchedStateMachine || mDelayedScheduler.IsScheduled();
}

void
MediaDecoderStateMachine::SetPlaybackRate(double aPlaybackRate)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(aPlaybackRate != 0, "Should be handled by MediaDecoder::Pause()");

  mPlaybackRate = aPlaybackRate;
  mMediaSink->SetPlaybackRate(mPlaybackRate);

  // Schedule next cycle to check if we can stop prerolling.
  ScheduleStateMachine();
}

void MediaDecoderStateMachine::PreservesPitchChanged()
{
  MOZ_ASSERT(OnTaskQueue());
  mMediaSink->SetPreservesPitch(mPreservesPitch);
}

TimeUnit
MediaDecoderStateMachine::AudioEndTime() const
{
  MOZ_ASSERT(OnTaskQueue());
  if (mMediaSink->IsStarted()) {
    return mMediaSink->GetEndTime(TrackInfo::kAudioTrack);
  }
  return TimeUnit::Zero();
}

TimeUnit
MediaDecoderStateMachine::VideoEndTime() const
{
  MOZ_ASSERT(OnTaskQueue());
  if (mMediaSink->IsStarted()) {
    return mMediaSink->GetEndTime(TrackInfo::kVideoTrack);
  }
  return TimeUnit::Zero();
}

void
MediaDecoderStateMachine::OnMediaSinkVideoComplete()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(HasVideo());
  LOG("[%s]", __func__);

  mMediaSinkVideoPromise.Complete();
  mVideoCompleted = true;
  ScheduleStateMachine();
}

void
MediaDecoderStateMachine::OnMediaSinkVideoError()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(HasVideo());
  LOGW("[%s]", __func__);

  mMediaSinkVideoPromise.Complete();
  mVideoCompleted = true;
  if (HasAudio()) {
    return;
  }
  DecodeError(MediaResult(NS_ERROR_DOM_MEDIA_MEDIASINK_ERR, __func__));
}

void MediaDecoderStateMachine::OnMediaSinkAudioComplete()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(HasAudio());
  LOG("[%s]", __func__);

  mMediaSinkAudioPromise.Complete();
  mAudioCompleted = true;
  // To notify PlaybackEnded as soon as possible.
  ScheduleStateMachine();

  // Report OK to Decoder Doctor (to know if issue may have been resolved).
  mOnDecoderDoctorEvent.Notify(
    DecoderDoctorEvent{DecoderDoctorEvent::eAudioSinkStartup, NS_OK});
}

void MediaDecoderStateMachine::OnMediaSinkAudioError(nsresult aResult)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(HasAudio());
  LOGW("[%s]", __func__);

  mMediaSinkAudioPromise.Complete();
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

void
MediaDecoderStateMachine::OnCDMProxyReady(RefPtr<CDMProxy> aProxy)
{
  MOZ_ASSERT(OnTaskQueue());
  mCDMProxyPromise.Complete();
  mCDMProxy = aProxy;
  mReader->SetCDMProxy(aProxy);
  mStateObj->HandleCDMProxyReady();
}

void
MediaDecoderStateMachine::OnCDMProxyNotReady()
{
  MOZ_ASSERT(OnTaskQueue());
  mCDMProxyPromise.Complete();
}

void
MediaDecoderStateMachine::SetAudioCaptured(bool aCaptured)
{
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
  mMediaSink = CreateMediaSink(aCaptured);

  // Restore playback parameters.
  mMediaSink->SetPlaybackParams(params);

  mAudioCaptured = aCaptured;

  // Don't buffer as much when audio is captured because we don't need to worry
  // about high latency audio devices.
  mAmpleAudioThreshold = mAudioCaptured
    ? detail::AMPLE_AUDIO_THRESHOLD / 2 : detail::AMPLE_AUDIO_THRESHOLD;

  mStateObj->HandleAudioCaptured();
}

uint32_t MediaDecoderStateMachine::GetAmpleVideoFrames() const
{
  MOZ_ASSERT(OnTaskQueue());
  return (mReader->IsAsync() && mReader->VideoIsHardwareAccelerated())
         ? std::max<uint32_t>(sVideoQueueHWAccelSize, MIN_VIDEO_QUEUE_SIZE)
         : std::max<uint32_t>(sVideoQueueDefaultSize, MIN_VIDEO_QUEUE_SIZE);
}

nsCString
MediaDecoderStateMachine::GetDebugInfo()
{
  MOZ_ASSERT(OnTaskQueue());
  return nsPrintfCString(
           "MediaDecoderStateMachine State: GetMediaTime=%" PRId64 " GetClock="
           "%" PRId64 " mMediaSink=%p state=%s mPlayState=%d "
           "mSentFirstFrameLoadedEvent=%d IsPlaying=%d mAudioStatus=%s "
           "mVideoStatus=%s mDecodedAudioEndTime=%" PRId64
           " mDecodedVideoEndTime=%" PRId64 "mAudioCompleted=%d "
           "mVideoCompleted=%d",
           GetMediaTime().ToMicroseconds(),
           mMediaSink->IsStarted() ? GetClock().ToMicroseconds() : -1,
           mMediaSink.get(), ToStateStr(), mPlayState.Ref(),
           mSentFirstFrameLoadedEvent, IsPlaying(), AudioRequestStatus(),
           VideoRequestStatus(), mDecodedAudioEndTime.ToMicroseconds(),
           mDecodedVideoEndTime.ToMicroseconds(),
           mAudioCompleted, mVideoCompleted)
         + mStateObj->GetDebugInfo() + nsCString("\n")
         + mMediaSink->GetDebugInfo();
}

RefPtr<MediaDecoder::DebugInfoPromise>
MediaDecoderStateMachine::RequestDebugInfo()
{
  using PromiseType = MediaDecoder::DebugInfoPromise;
  RefPtr<PromiseType::Private> p = new PromiseType::Private(__func__);
  RefPtr<MediaDecoderStateMachine> self = this;
  OwnerThread()->Dispatch(
    NS_NewRunnableFunction(
      "MediaDecoderStateMachine::RequestDebugInfo",
      [self, p]() { p->Resolve(self->GetDebugInfo(), __func__); }),
    AbstractThread::AssertDispatchSuccess,
    AbstractThread::TailDispatch);
  return p.forget();
}

void MediaDecoderStateMachine::AddOutputStream(ProcessedMediaStream* aStream,
                                               bool aFinishWhenEnded)
{
  MOZ_ASSERT(NS_IsMainThread());
  LOG("AddOutputStream aStream=%p!", aStream);
  mOutputStreamManager->Add(aStream, aFinishWhenEnded);
  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod<bool>("MediaDecoderStateMachine::SetAudioCaptured",
                            this,
                            &MediaDecoderStateMachine::SetAudioCaptured,
                            true);
  OwnerThread()->Dispatch(r.forget());
}

void MediaDecoderStateMachine::RemoveOutputStream(MediaStream* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  LOG("RemoveOutputStream=%p!", aStream);
  mOutputStreamManager->Remove(aStream);
  if (mOutputStreamManager->IsEmpty()) {
    nsCOMPtr<nsIRunnable> r =
      NewRunnableMethod<bool>("MediaDecoderStateMachine::SetAudioCaptured",
                              this,
                              &MediaDecoderStateMachine::SetAudioCaptured,
                              false);
    OwnerThread()->Dispatch(r.forget());
  }
}

size_t
MediaDecoderStateMachine::SizeOfVideoQueue() const
{
  return mReader->SizeOfVideoQueueInBytes();
}

size_t
MediaDecoderStateMachine::SizeOfAudioQueue() const
{
  return mReader->SizeOfAudioQueueInBytes();
}

AbstractCanonical<media::TimeIntervals>*
MediaDecoderStateMachine::CanonicalBuffered() const
{
  return mReader->CanonicalBuffered();
}

MediaEventSource<void>&
MediaDecoderStateMachine::OnMediaNotSeekable() const
{
  return mReader->OnMediaNotSeekable();
}

const char*
MediaDecoderStateMachine::AudioRequestStatus() const
{
  MOZ_ASSERT(OnTaskQueue());
  if (IsRequestingAudioData()) {
    MOZ_DIAGNOSTIC_ASSERT(!IsWaitingAudioData());
    return "pending";
  } else if (IsWaitingAudioData()) {
    return "waiting";
  }
  return "idle";
}

const char*
MediaDecoderStateMachine::VideoRequestStatus() const
{
  MOZ_ASSERT(OnTaskQueue());
  if (IsRequestingVideoData()) {
    MOZ_DIAGNOSTIC_ASSERT(!IsWaitingVideoData());
    return "pending";
  } else if (IsWaitingVideoData()) {
    return "waiting";
  }
  return "idle";
}

void
MediaDecoderStateMachine::OnSuspendTimerResolved()
{
  LOG("OnSuspendTimerResolved");
  mVideoDecodeSuspendTimer.CompleteRequest();
  mStateObj->HandleVideoSuspendTimeout();
}

void
MediaDecoderStateMachine::CancelSuspendTimer()
{
  LOG("CancelSuspendTimer: State: %s, Timer.IsScheduled: %c",
      ToStateStr(mStateObj->GetState()),
      mVideoDecodeSuspendTimer.IsScheduled() ? 'T' : 'F');
  MOZ_ASSERT(OnTaskQueue());
  if (mVideoDecodeSuspendTimer.IsScheduled()) {
    mOnPlaybackEvent.Notify(MediaEventType::CancelVideoSuspendTimer);
  }
  mVideoDecodeSuspendTimer.Reset();
}

} // namespace mozilla

// avoid redefined macro in unified build
#undef LOG
#undef LOGV
#undef LOGW
#undef SLOGW
#undef NS_DispatchToMainThread
