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

#include "mediasink/AudioSinkWrapper.h"
#include "mediasink/DecodedAudioDataSink.h"
#include "mediasink/DecodedStream.h"
#include "mediasink/OutputStreamManager.h"
#include "mediasink/VideoSink.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Logging.h"
#include "mozilla/mozalloc.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Preferences.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/TaskQueue.h"

#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIEventTarget.h"
#include "nsITimer.h"
#include "nsPrintfCString.h"
#include "nsTArray.h"
#include "nsDeque.h"
#include "prenv.h"

#include "AccurateSeekTask.h"
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
#include "NextFrameSeekTask.h"
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
#undef DECODER_LOG
#undef VERBOSE_LOG
#undef SAMPLE_LOG
#undef DECODER_WARN
#undef DUMP_LOG
#undef SFMT
#undef SLOG
#undef SWARN
#undef SDUMP

#define FMT(x, ...) "Decoder=%p " x, mDecoderID, ##__VA_ARGS__
#define DECODER_LOG(x, ...) MOZ_LOG(gMediaDecoderLog, LogLevel::Debug,   (FMT(x, ##__VA_ARGS__)))
#define VERBOSE_LOG(x, ...) MOZ_LOG(gMediaDecoderLog, LogLevel::Verbose, (FMT(x, ##__VA_ARGS__)))
#define SAMPLE_LOG(x, ...)  MOZ_LOG(gMediaSampleLog,  LogLevel::Debug,   (FMT(x, ##__VA_ARGS__)))
#define DECODER_WARN(x, ...) NS_WARNING(nsPrintfCString(FMT(x, ##__VA_ARGS__)).get())
#define DUMP_LOG(x, ...) NS_DebugBreak(NS_DEBUG_WARNING, nsPrintfCString(FMT(x, ##__VA_ARGS__)).get(), nullptr, nullptr, -1)

// Used by StateObject and its sub-classes
#define SFMT(x, ...) "Decoder=%p state=%s " x, mMaster->mDecoderID, ToStateStr(GetState()), ##__VA_ARGS__
#define SLOG(x, ...) MOZ_LOG(gMediaDecoderLog, LogLevel::Debug, (SFMT(x, ##__VA_ARGS__)))
#define SWARN(x, ...) NS_WARNING(nsPrintfCString(SFMT(x, ##__VA_ARGS__)).get())
#define SDUMP(x, ...) NS_DebugBreak(NS_DEBUG_WARNING, nsPrintfCString(SFMT(x, ##__VA_ARGS__)).get(), nullptr, nullptr, -1)

// Certain constants get stored as member variables and then adjusted by various
// scale factors on a per-decoder basis. We want to make sure to avoid using these
// constants directly, so we put them in a namespace.
namespace detail {

// If audio queue has less than this many usecs of decoded audio, we won't risk
// trying to decode the video, we'll skip decoding video up to the next
// keyframe. We may increase this value for an individual decoder if we
// encounter video frames which take a long time to decode.
static const uint32_t LOW_AUDIO_USECS = 300000;

// If more than this many usecs of decoded audio is queued, we'll hold off
// decoding more audio. If we increase the low audio threshold (see
// LOW_AUDIO_USECS above) we'll also increase this value to ensure it's not
// less than the low audio threshold.
static const int64_t AMPLE_AUDIO_USECS = 2000000;

} // namespace detail

// If we have fewer than LOW_VIDEO_FRAMES decoded frames, and
// we're not "prerolling video", we'll skip the video up to the next keyframe
// which is at or after the current playback position.
static const uint32_t LOW_VIDEO_FRAMES = 2;

// Threshold in usecs that used to check if we are low on decoded video.
// If the last video frame's end time |mDecodedVideoEndTime| is more than
// |LOW_VIDEO_THRESHOLD_USECS*mPlaybackRate| after the current clock in
// Advanceframe(), the video decode is lagging, and we skip to next keyframe.
static const int32_t LOW_VIDEO_THRESHOLD_USECS = 60000;

// Arbitrary "frame duration" when playing only audio.
static const int AUDIO_DURATION_USECS = 40000;

// If we increase our "low audio threshold" (see LOW_AUDIO_USECS above), we
// use this as a factor in all our calculations. Increasing this will cause
// us to be more likely to increase our low audio threshold, and to
// increase it by more.
static const int THRESHOLD_FACTOR = 2;

namespace detail {

// If we have less than this much undecoded data available, we'll consider
// ourselves to be running low on undecoded data. We determine how much
// undecoded data we have remaining using the reader's GetBuffered()
// implementation.
static const int64_t LOW_DATA_THRESHOLD_USECS = 5000000;

// LOW_DATA_THRESHOLD_USECS needs to be greater than AMPLE_AUDIO_USECS, otherwise
// the skip-to-keyframe logic can activate when we're running low on data.
static_assert(LOW_DATA_THRESHOLD_USECS > AMPLE_AUDIO_USECS,
              "LOW_DATA_THRESHOLD_USECS is too small");

} // namespace detail

// Amount of excess usecs of data to add in to the "should we buffer" calculation.
static const uint32_t EXHAUSTED_DATA_MARGIN_USECS = 100000;

static int64_t DurationToUsecs(TimeDuration aDuration) {
  return static_cast<int64_t>(aDuration.ToSeconds() * USECS_PER_S);
}

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
static uint32_t sVideoQueueSendToCompositorSize = VIDEO_QUEUE_SEND_TO_COMPOSITOR_SIZE;

static void InitVideoQueuePrefs() {
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
  virtual ~StateObject() {}
  virtual void Exit() {};  // Exit action.
  virtual void Step() {}   // Perform a 'cycle' of this state object.
  virtual State GetState() const = 0;

  // Event handlers for various events.
  virtual void HandleCDMProxyReady() {}
  virtual void HandleAudioDecoded(MediaData* aAudio) {}
  virtual void HandleVideoDecoded(MediaData* aVideo, TimeStamp aDecodeStart) {}
  virtual void HandleEndOfStream() {}
  virtual void HandleWaitingForData() {}
  virtual void HandleAudioCaptured() {}

  virtual RefPtr<MediaDecoder::SeekPromise> HandleSeek(SeekTarget aTarget);

  virtual RefPtr<ShutdownPromise> HandleShutdown();

  virtual void HandleVideoSuspendTimeout() = 0;

  virtual void HandleResumeVideoDecoding();

  virtual void HandlePlayStateChanged(MediaDecoder::PlayState aPlayState) {}

  virtual void DumpDebugInfo() {}

private:
  template <class S, typename R, typename... As>
  auto ReturnTypeHelper(R(S::*)(As...)) -> R;

protected:
  enum class EventVisibility : int8_t
  {
    Observable,
    Suppressed
  };

  using Master = MediaDecoderStateMachine;
  explicit StateObject(Master* aPtr) : mMaster(aPtr) {}
  TaskQueue* OwnerThread() const { return mMaster->mTaskQueue; }
  MediaResource* Resource() const { return mMaster->mResource; }
  MediaDecoderReaderWrapper* Reader() const { return mMaster->mReader; }
  const MediaInfo& Info() const { return mMaster->Info(); }
  bool IsExpectingMoreData() const
  {
    // We are expecting more data if either the resource states so, or if we
    // have a waiting promise pending (such as with non-MSE EME).
    return Resource()->IsExpectingMoreData() ||
           (Reader()->IsWaitForDataSupported() &&
            (Reader()->IsWaitingAudioData() || Reader()->IsWaitingVideoData()));
  }
  MediaQueue<MediaData>& AudioQueue() { return mMaster->mAudioQueue; }
  MediaQueue<MediaData>& VideoQueue() { return mMaster->mVideoQueue; }

  // Note this function will delete the current state object.
  // Don't access members to avoid UAF after this call.
  template <class S, typename... Ts>
  auto SetState(Ts... aArgs)
    -> decltype(ReturnTypeHelper(&S::Enter))
  {
    // keep mMaster in a local object because mMaster will become invalid after
    // the current state object is deleted.
    auto master = mMaster;

    auto s = new S(master);

    MOZ_ASSERT(master->mState != s->GetState() ||
               master->mState == DECODER_STATE_SEEKING);

    SLOG("change state to: %s", ToStateStr(s->GetState()));

    Exit();

    master->mState = s->GetState();
    master->mStateObj.reset(s);
    return s->Enter(Move(aArgs)...);
  }

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
  explicit DecodeMetadataState(Master* aPtr) : StateObject(aPtr) {}

  void Enter()
  {
    MOZ_ASSERT(!mMaster->mVideoDecodeSuspended);
    MOZ_ASSERT(!mMetadataRequest.Exists());
    SLOG("Dispatching AsyncReadMetadata");

    // Set mode to METADATA since we are about to read metadata.
    Resource()->SetReadMode(MediaCacheStream::MODE_METADATA);

    // We disconnect mMetadataRequest in Exit() so it is fine to capture
    // a raw pointer here.
    mMetadataRequest.Begin(Reader()->ReadMetadata()
      ->Then(OwnerThread(), __func__,
        [this] (MetadataHolder* aMetadata) {
          OnMetadataRead(aMetadata);
        },
        [this] (const MediaResult& aError) {
          OnMetadataNotRead(aError);
        }));
  }

  void Exit() override
  {
    mMetadataRequest.DisconnectIfExists();
  }

  State GetState() const override
  {
    return DECODER_STATE_DECODING_METADATA;
  }

  RefPtr<MediaDecoder::SeekPromise> HandleSeek(SeekTarget aTarget) override
  {
    MOZ_DIAGNOSTIC_ASSERT(false, "Can't seek while decoding metadata.");
    return MediaDecoder::SeekPromise::CreateAndReject(true, __func__);
  }

  void HandleVideoSuspendTimeout() override
  {
    // Do nothing since no decoders are created yet.
  }

  void HandleResumeVideoDecoding() override
  {
    // We never suspend video decoding in this state.
    MOZ_ASSERT(false, "Shouldn't have suspended video decoding.");
  }

private:
  void OnMetadataRead(MetadataHolder* aMetadata);

  void OnMetadataNotRead(const MediaResult& aError)
  {
    mMetadataRequest.Complete();
    SWARN("Decode metadata failed, shutting down decoder");
    mMaster->DecodeError(aError);
  }

  MozPromiseRequestHolder<MediaDecoderReader::MetadataPromise> mMetadataRequest;
};

/**
 * Purpose: wait for the CDM to start decoding.
 *
 * Transition to other states when CDM is ready:
 *   DECODING_FIRSTFRAME otherwise.
 */
class MediaDecoderStateMachine::WaitForCDMState
  : public MediaDecoderStateMachine::StateObject
{
public:
  explicit WaitForCDMState(Master* aPtr) : StateObject(aPtr) {}

  void Enter()
  {
    MOZ_ASSERT(!mMaster->mVideoDecodeSuspended);
  }

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
    mPendingSeek.mTarget = aTarget;
    return mPendingSeek.mPromise.Ensure(__func__);
  }

  void HandleVideoSuspendTimeout() override
  {
    // Do nothing since no decoders are created yet.
  }

  void HandleResumeVideoDecoding() override
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
  explicit DormantState(Master* aPtr) : StateObject(aPtr) {}

  void Enter()
  {
    if (mMaster->IsPlaying()) {
      mMaster->StopPlayback();
    }

    // Calculate the position to seek to when exiting dormant.
    auto t = mMaster->mMediaSink->IsStarted()
      ? mMaster->GetClock()
      : mMaster->GetMediaTime();
    mPendingSeek.mTarget = SeekTarget(t, SeekTarget::Accurate);
    // SeekJob asserts |mTarget.IsValid() == !mPromise.IsEmpty()| so we
    // need to create the promise even it is not used at all.
    RefPtr<MediaDecoder::SeekPromise> x = mPendingSeek.mPromise.Ensure(__func__);

    mMaster->Reset();
    mMaster->mReader->ReleaseResources();
  }

  void Exit() override
  {
    // mPendingSeek is either moved when exiting dormant or
    // should be rejected here before transition to SHUTDOWN.
    mPendingSeek.RejectIfExists(__func__);
  }

  State GetState() const override
  {
    return DECODER_STATE_DORMANT;
  }

  void HandleVideoSuspendTimeout() override
  {
    // Do nothing since we've released decoders in Enter().
  }

  void HandleResumeVideoDecoding() override
  {
    // Do nothing since we won't resume decoding until exiting dormant.
  }

  void HandlePlayStateChanged(MediaDecoder::PlayState aPlayState) override;

private:
  SeekJob mPendingSeek;
};

/**
 * Purpose: decode the 1st audio and video frames to fire the 'loadeddata' event.
 *
 * Transition to:
 *   SHUTDOWN if any decode error.
 *   SEEKING if any pending seek and seek is possible.
 *   DECODING when the 'loadeddata' event is fired.
 */
class MediaDecoderStateMachine::DecodingFirstFrameState
  : public MediaDecoderStateMachine::StateObject
{
public:
  explicit DecodingFirstFrameState(Master* aPtr) : StateObject(aPtr) {}

  void Enter(SeekJob aPendingSeek);

  void Exit() override
  {
    // mPendingSeek is either moved before transition to SEEKING,
    // or should be rejected here before transition to SHUTDOWN.
    mPendingSeek.RejectIfExists(__func__);
  }

  State GetState() const override
  {
    return DECODER_STATE_DECODING_FIRSTFRAME;
  }

  void HandleAudioDecoded(MediaData* aAudio) override
  {
    mMaster->Push(aAudio, MediaData::AUDIO_DATA);
    MaybeFinishDecodeFirstFrame();
  }

  void HandleVideoDecoded(MediaData* aVideo, TimeStamp aDecodeStart) override
  {
    mMaster->Push(aVideo, MediaData::VIDEO_DATA);
    MaybeFinishDecodeFirstFrame();
  }

  void HandleEndOfStream() override
  {
    MaybeFinishDecodeFirstFrame();
  }

  RefPtr<MediaDecoder::SeekPromise> HandleSeek(SeekTarget aTarget) override;

  void HandleVideoSuspendTimeout() override
  {
    // Do nothing for we need to decode the 1st video frame to get the dimensions.
  }

  void HandleResumeVideoDecoding() override
  {
    // We never suspend video decoding in this state.
    MOZ_ASSERT(false, "Shouldn't have suspended video decoding.");
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
  }

  void Step() override
  {
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

    MOZ_ASSERT(!mMaster->IsPlaying() ||
               mMaster->IsStateMachineScheduled(),
               "Must have timer scheduled");

    MaybeStartBuffering();
  }

  State GetState() const override
  {
    return DECODER_STATE_DECODING;
  }

  void HandleAudioDecoded(MediaData* aAudio) override
  {
    mMaster->Push(aAudio, MediaData::AUDIO_DATA);
    MaybeStopPrerolling();
  }

  void HandleVideoDecoded(MediaData* aVideo, TimeStamp aDecodeStart) override
  {
    mMaster->Push(aVideo, MediaData::VIDEO_DATA);
    MaybeStopPrerolling();
    CheckSlowDecoding(aDecodeStart);
  }

  void HandleEndOfStream() override;

  void HandleWaitingForData() override
  {
    MaybeStopPrerolling();
  }

  void HandleAudioCaptured() override
  {
    MaybeStopPrerolling();
    // MediaSink is changed. Schedule Step() to check if we can start playback.
    mMaster->ScheduleStateMachine();
  }

  void HandleVideoSuspendTimeout() override
  {
    if (mMaster->HasVideo()) {
      mMaster->mVideoDecodeSuspended = true;
      mMaster->mOnPlaybackEvent.Notify(MediaEventType::EnterVideoSuspend);
      Reader()->SetVideoBlankDecode(true);
    }
  }

  void HandlePlayStateChanged(MediaDecoder::PlayState aPlayState) override
  {
    if (aPlayState == MediaDecoder::PLAY_STATE_PLAYING) {
      // Schedule Step() to check if we can start playback.
      mMaster->ScheduleStateMachine();
    }

    if (aPlayState == MediaDecoder::PLAY_STATE_PAUSED) {
      StartDormantTimer();
    } else {
      mDormantTimer.Reset();
    }
  }

  void DumpDebugInfo() override
  {
    SDUMP("mIsPrerolling=%d", mIsPrerolling);
  }

private:
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
    int64_t adjustedTime = THRESHOLD_FACTOR * DurationToUsecs(decodeTime);
    if (adjustedTime > mMaster->mLowAudioThresholdUsecs &&
        !mMaster->HasLowBufferedData())
    {
      mMaster->mLowAudioThresholdUsecs =
        std::min(adjustedTime, mMaster->mAmpleAudioThresholdUsecs);

      mMaster->mAmpleAudioThresholdUsecs =
        std::max(THRESHOLD_FACTOR * mMaster->mLowAudioThresholdUsecs,
                 mMaster->mAmpleAudioThresholdUsecs);

      SLOG("Slow video decode, set "
           "mLowAudioThresholdUsecs=%lld "
           "mAmpleAudioThresholdUsecs=%lld",
           mMaster->mLowAudioThresholdUsecs,
           mMaster->mAmpleAudioThresholdUsecs);
    }
  }

  bool DonePrerollingAudio()
  {
    return !mMaster->IsAudioDecoding() ||
           mMaster->GetDecodedAudioDuration() >=
             mMaster->AudioPrerollUsecs() * mMaster->mPlaybackRate;
  }

  bool DonePrerollingVideo()
  {
    return !mMaster->IsVideoDecoding() ||
           static_cast<uint32_t>(mMaster->VideoQueue().GetSize()) >=
             mMaster->VideoPrerollFrames() * mMaster->mPlaybackRate + 1;
  }

  void MaybeStopPrerolling()
  {
    if (mIsPrerolling &&
        (DonePrerollingAudio() || Reader()->IsWaitingAudioData()) &&
        (DonePrerollingVideo() || Reader()->IsWaitingVideoData())) {
      mIsPrerolling = false;
      // Check if we can start playback.
      mMaster->ScheduleStateMachine();
    }
  }

  void EnterDormant()
  {
    SetState<DormantState>();
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
      EnterDormant();
      return;
    }

    TimeStamp target = TimeStamp::Now() +
      TimeDuration::FromMilliseconds(timeout);

    mDormantTimer.Ensure(target,
      [this] () {
        mDormantTimer.CompleteRequest();
        EnterDormant();
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
};

/**
 * Purpose: seek to a particular new playback position.
 *
 * Transition to:
 *   SEEKING if any new seek request.
 *   SHUTDOWN if seek failed.
 *   COMPLETED if the new playback position is the end of the media resource.
 *   DECODING otherwise.
 */
class MediaDecoderStateMachine::SeekingState
  : public MediaDecoderStateMachine::StateObject
{
public:
  explicit SeekingState(Master* aPtr) : StateObject(aPtr) {}

  RefPtr<MediaDecoder::SeekPromise> Enter(SeekJob aSeekJob,
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

    // SeekTask will register its callbacks to MediaDecoderReaderWrapper.
    mMaster->CancelMediaDecoderReaderWrapperCallback();

    // Create a new SeekTask instance for the incoming seek task.
    if (mSeekJob.mTarget.IsAccurate() ||
        mSeekJob.mTarget.IsFast()) {
      mSeekTask = new AccurateSeekTask(
        mMaster->mDecoderID, OwnerThread(), Reader(), mSeekJob.mTarget,
        Info(), mMaster->Duration(), mMaster->GetMediaTime());
    } else if (mSeekJob.mTarget.IsNextFrame()) {
      mSeekTask = new NextFrameSeekTask(
        mMaster->mDecoderID, OwnerThread(), Reader(), mSeekJob.mTarget,
        Info(), mMaster->Duration(),mMaster->GetMediaTime(),
        AudioQueue(), VideoQueue());
    } else {
      MOZ_DIAGNOSTIC_ASSERT(false, "Cannot handle this seek task.");
    }

    // Don't stop playback for a video-only seek since audio is playing.
    if (!mSeekJob.mTarget.IsVideoOnly()) {
      mMaster->StopPlayback();
    }

    // mSeekJob.mTarget.mTime might be different from
    // mSeekTask->GetSeekTarget().mTime because the seek task might clamp the
    // seek target to [0, duration]. We want to update the playback position to
    // the clamped value.
    mMaster->UpdatePlaybackPositionInternal(
      mSeekTask->GetSeekTarget().GetTime().ToMicroseconds());

    if (mVisibility == EventVisibility::Observable) {
      mMaster->mOnPlaybackEvent.Notify(MediaEventType::SeekStarted);
      // We want dormant actions to be transparent to the user.
      // So we only notify the change when the seek request is from the user.
      mMaster->UpdateNextFrameStatus(MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_SEEKING);
    }

    // Reset our state machine and decoding pipeline before seeking.
    if (mSeekTask->NeedToResetMDSM()) {
      if (mSeekJob.mTarget.IsVideoOnly()) {
        mMaster->Reset(TrackInfo::kVideoTrack);
      } else {
        mMaster->Reset();
      }
    }

    // Do the seek.
    mSeekTaskRequest.Begin(mSeekTask->Seek(mMaster->Duration())
      ->Then(OwnerThread(), __func__,
             [this] (const SeekTaskResolveValue& aValue) {
               OnSeekTaskResolved(aValue);
             },
             [this] (const SeekTaskRejectValue& aValue) {
               OnSeekTaskRejected(aValue);
             }));

    return mSeekJob.mPromise.Ensure(__func__);
  }

  void Exit() override
  {
    mSeekTaskRequest.DisconnectIfExists();
    mSeekJob.RejectIfExists(__func__);
    mSeekTask->Discard();

    // Reset the MediaDecoderReaderWrapper's callbask.
    mMaster->SetMediaDecoderReaderWrapperCallback();
  }

  State GetState() const override
  {
    return DECODER_STATE_SEEKING;
  }

  void HandleAudioDecoded(MediaData* aAudio) override
  {
    MOZ_ASSERT(false);
  }

  void HandleVideoDecoded(MediaData* aVideo, TimeStamp aDecodeStart) override
  {
    MOZ_ASSERT(false);
  }

  void HandleVideoSuspendTimeout() override
  {
    // Do nothing since we want a valid video frame to show when seek is done.
  }

  void HandleResumeVideoDecoding() override
  {
    // We set mVideoDecodeSuspended to false in Enter().
    MOZ_ASSERT(false, "Shouldn't have suspended video decoding.");
  }

private:
  void OnSeekTaskResolved(const SeekTaskResolveValue& aValue)
  {
    mSeekTaskRequest.Complete();

    if (aValue.mSeekedAudioData) {
      mMaster->Push(aValue.mSeekedAudioData, MediaData::AUDIO_DATA);
      mMaster->mDecodedAudioEndTime = std::max(
        aValue.mSeekedAudioData->GetEndTime(), mMaster->mDecodedAudioEndTime);
    }

    if (aValue.mSeekedVideoData) {
      mMaster->Push(aValue.mSeekedVideoData, MediaData::VIDEO_DATA);
      mMaster->mDecodedVideoEndTime = std::max(
        aValue.mSeekedVideoData->GetEndTime(), mMaster->mDecodedVideoEndTime);
    }

    if (aValue.mIsAudioQueueFinished) {
      AudioQueue().Finish();
    }

    if (aValue.mIsVideoQueueFinished) {
      VideoQueue().Finish();
    }

    SeekCompleted();
  }

  void OnSeekTaskRejected(const SeekTaskRejectValue& aValue)
  {
    mSeekTaskRequest.Complete();

    if (aValue.mIsAudioQueueFinished) {
      AudioQueue().Finish();
    }

    if (aValue.mIsVideoQueueFinished) {
      VideoQueue().Finish();
    }

    mMaster->DecodeError(aValue.mError);
  }

  void SeekCompleted();

  SeekJob mSeekJob;
  EventVisibility mVisibility = EventVisibility::Observable;
  MozPromiseRequestHolder<SeekTask::SeekTaskPromise> mSeekTaskRequest;
  RefPtr<SeekTask> mSeekTask;
};

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
  explicit BufferingState(Master* aPtr) : StateObject(aPtr) {}

  void Enter()
  {
    if (mMaster->IsPlaying()) {
      mMaster->StopPlayback();
    }

    mBufferingStart = TimeStamp::Now();

    MediaStatistics stats = mMaster->GetStatistics();
    SLOG("Playback rate: %.1lfKB/s%s download rate: %.1lfKB/s%s",
         stats.mPlaybackRate/1024, stats.mPlaybackRateReliable ? "" : " (unreliable)",
         stats.mDownloadRate/1024, stats.mDownloadRateReliable ? "" : " (unreliable)");

    mMaster->ScheduleStateMachineIn(USECS_PER_S);

    mMaster->UpdateNextFrameStatus(MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_BUFFERING);
  }

  void Step() override;

  State GetState() const override
  {
    return DECODER_STATE_BUFFERING;
  }

  void HandleAudioDecoded(MediaData* aAudio) override
  {
    // This might be the sample we need to exit buffering.
    // Schedule Step() to check it.
    mMaster->Push(aAudio, MediaData::AUDIO_DATA);
    mMaster->ScheduleStateMachine();
  }

  void HandleVideoDecoded(MediaData* aVideo, TimeStamp aDecodeStart) override
  {
    // This might be the sample we need to exit buffering.
    // Schedule Step() to check it.
    mMaster->Push(aVideo, MediaData::VIDEO_DATA);
    mMaster->ScheduleStateMachine();
  }

  void HandleEndOfStream() override;

  void HandleVideoSuspendTimeout() override
  {
    if (mMaster->HasVideo()) {
      mMaster->mVideoDecodeSuspended = true;
      mMaster->mOnPlaybackEvent.Notify(MediaEventType::EnterVideoSuspend);
      Reader()->SetVideoBlankDecode(true);
    }
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
 */
class MediaDecoderStateMachine::CompletedState
  : public MediaDecoderStateMachine::StateObject
{
public:
  explicit CompletedState(Master* aPtr) : StateObject(aPtr) {}

  void Enter()
  {
    // We've decoded all samples. We don't need decoders anymore.
    Reader()->ReleaseResources();

    bool hasNextFrame = (!mMaster->HasAudio() || !mMaster->mAudioCompleted)
      && (!mMaster->HasVideo() || !mMaster->mVideoCompleted);

    mMaster->UpdateNextFrameStatus(hasNextFrame
      ? MediaDecoderOwner::NEXT_FRAME_AVAILABLE
      : MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE);

    Step();
  }

  void Exit() override
  {
    mSentPlaybackEndedEvent = false;
  }

  void Step() override
  {
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
      MOZ_ASSERT(!mMaster->IsPlaying() ||
                 mMaster->IsStateMachineScheduled(),
                 "Must have timer scheduled");
      return;
    }

    // StopPlayback in order to reset the IsPlaying() state so audio
    // is restarted correctly.
    mMaster->StopPlayback();

    if (!mSentPlaybackEndedEvent) {
      int64_t clockTime = std::max(mMaster->AudioEndTime(), mMaster->VideoEndTime());
      clockTime = std::max(int64_t(0), std::max(clockTime, mMaster->Duration().ToMicroseconds()));
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
  explicit ShutdownState(Master* aPtr) : StateObject(aPtr) {}

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

  void HandleResumeVideoDecoding() override
  {
    MOZ_DIAGNOSTIC_ASSERT(false, "Already shutting down.");
  }
};

RefPtr<MediaDecoder::SeekPromise>
MediaDecoderStateMachine::
StateObject::HandleSeek(SeekTarget aTarget)
{
  SLOG("Changed state to SEEKING (to %lld)", aTarget.GetTime().ToMicroseconds());
  SeekJob seekJob;
  seekJob.mTarget = aTarget;
  return SetState<SeekingState>(Move(seekJob), EventVisibility::Observable);
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
StateObject::HandleResumeVideoDecoding()
{
  MOZ_ASSERT(mMaster->mVideoDecodeSuspended);

  // Start counting recovery time from right now.
  TimeStamp start = TimeStamp::Now();

  // Local reference to mInfo, so that it will be copied in the lambda below.
  auto& info = Info();
  bool hw = Reader()->VideoIsHardwareAccelerated();

  // Start video-only seek to the current time.
  SeekJob seekJob;

  const SeekTarget::Type type = mMaster->HasAudio()
                                ? SeekTarget::Type::Accurate
                                : SeekTarget::Type::PrevSyncPoint;

  seekJob.mTarget = SeekTarget(mMaster->GetMediaTime(),
                               type,
                               true /* aVideoOnly */);

  SetState<SeekingState>(Move(seekJob), EventVisibility::Suppressed)->Then(
    AbstractThread::MainThread(), __func__,
    [start, info, hw](){ ReportRecoveryTelemetry(start, info, hw); },
    [](){});
}

void
MediaDecoderStateMachine::
DecodeMetadataState::OnMetadataRead(MetadataHolder* aMetadata)
{
  mMetadataRequest.Complete();

  // Set mode to PLAYBACK after reading metadata.
  Resource()->SetReadMode(MediaCacheStream::MODE_PLAYBACK);

  mMaster->mInfo = Some(aMetadata->mInfo);
  mMaster->mMetadataTags = aMetadata->mTags.forget();
  mMaster->mMediaSeekable = Info().mMediaSeekable;
  mMaster->mMediaSeekableOnlyInBufferedRanges = Info().mMediaSeekableOnlyInBufferedRanges;

  if (Info().mMetadataDuration.isSome()) {
    mMaster->RecomputeDuration();
  } else if (Info().mUnadjustedMetadataEndTime.isSome()) {
    const TimeUnit unadjusted = Info().mUnadjustedMetadataEndTime.ref();
    const TimeUnit adjustment = Info().mStartTime;
    mMaster->mInfo->mMetadataDuration.emplace(unadjusted - adjustment);
    mMaster->RecomputeDuration();
  }

  if (mMaster->HasVideo()) {
    SLOG("Video decode isAsync=%d HWAccel=%d videoQueueSize=%d",
         Reader()->IsAsync(),
         Reader()->VideoIsHardwareAccelerated(),
         mMaster->GetAmpleVideoFrames());
  }

  MOZ_ASSERT(mMaster->mDuration.Ref().isSome());

  mMaster->EnqueueLoadedMetadataEvent();

  if (Info().IsEncrypted() && !mMaster->mCDMProxy) {
    // Metadata parsing was successful but we're still waiting for CDM caps
    // to become available so that we can build the correct decryptor/decoder.
    SetState<WaitForCDMState>();
  } else {
    SetState<DecodingFirstFrameState>(SeekJob{});
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
    SetState<SeekingState>(Move(mPendingSeek), EventVisibility::Suppressed);
  }
}

void
MediaDecoderStateMachine::
WaitForCDMState::HandleCDMProxyReady()
{
  SetState<DecodingFirstFrameState>(Move(mPendingSeek));
}

void
MediaDecoderStateMachine::
DecodingFirstFrameState::Enter(SeekJob aPendingSeek)
{
  // Handle pending seek.
  if (aPendingSeek.Exists() &&
      (mMaster->mSentFirstFrameLoadedEvent ||
       Reader()->ForceZeroStartTime())) {
    SetState<SeekingState>(Move(aPendingSeek), EventVisibility::Observable);
    return;
  }

  // Transition to DECODING if we've decoded first frames.
  if (mMaster->mSentFirstFrameLoadedEvent) {
    SetState<DecodingState>();
    return;
  }

  MOZ_ASSERT(!mMaster->mVideoDecodeSuspended);

  mPendingSeek = Move(aPendingSeek);

  // Dispatch tasks to decode first frames.
  mMaster->DispatchDecodeTasksIfNeeded();
}

RefPtr<MediaDecoder::SeekPromise>
MediaDecoderStateMachine::
DecodingFirstFrameState::HandleSeek(SeekTarget aTarget)
{
  // Should've transitioned to DECODING in Enter()
  // if mSentFirstFrameLoadedEvent is true.
  MOZ_ASSERT(!mMaster->mSentFirstFrameLoadedEvent);

  if (!Reader()->ForceZeroStartTime()) {
    SLOG("Not Enough Data to seek at this stage, queuing seek");
    mPendingSeek.RejectIfExists(__func__);
    mPendingSeek.mTarget = aTarget;
    return mPendingSeek.mPromise.Ensure(__func__);
  }

  // Since ForceZeroStartTime() is true, we should've transitioned to SEEKING
  // in Enter() if there is any pending seek.
  MOZ_ASSERT(!mPendingSeek.Exists());

  return StateObject::HandleSeek(aTarget);
}

void
MediaDecoderStateMachine::
DecodingFirstFrameState::MaybeFinishDecodeFirstFrame()
{
  MOZ_ASSERT(!mMaster->mSentFirstFrameLoadedEvent);

  if ((mMaster->IsAudioDecoding() && AudioQueue().GetSize() == 0) ||
      (mMaster->IsVideoDecoding() && VideoQueue().GetSize() == 0)) {
    return;
  }

  mMaster->FinishDecodeFirstFrame();

  if (mPendingSeek.Exists()) {
    SetState<SeekingState>(Move(mPendingSeek), EventVisibility::Observable);
  } else {
    SetState<DecodingState>();
  }
}

void
MediaDecoderStateMachine::
DecodingState::Enter()
{
  MOZ_ASSERT(mMaster->mSentFirstFrameLoadedEvent);

  if (!mMaster->mIsVisible &&
      !mMaster->mVideoDecodeSuspendTimer.IsScheduled() &&
      !mMaster->mVideoDecodeSuspended) {
    // If we are not visible and the timer is not schedule, it means the timer
    // has timed out and we should suspend video decoding now if necessary.
    HandleVideoSuspendTimeout();
  }

  if (mMaster->CheckIfDecodeComplete()) {
    SetState<CompletedState>();
    return;
  }

  mMaster->UpdateNextFrameStatus(MediaDecoderOwner::NEXT_FRAME_AVAILABLE);

  mDecodeStartTime = TimeStamp::Now();

  MaybeStopPrerolling();

  // Ensure that we've got tasks enqueued to decode data if we need to.
  mMaster->DispatchDecodeTasksIfNeeded();

  mMaster->ScheduleStateMachine();

  // Will enter dormant when playback is paused for a while.
  if (mMaster->mPlayState == MediaDecoder::PLAY_STATE_PAUSED) {
    StartDormantTimer();
  }
}

void
MediaDecoderStateMachine::
DecodingState::HandleEndOfStream()
{
  if (mMaster->CheckIfDecodeComplete()) {
    SetState<CompletedState>();
  } else {
    MaybeStopPrerolling();
  }
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
    shouldBuffer = IsExpectingMoreData() &&
                   mMaster->HasLowDecodedData() &&
                   mMaster->HasLowBufferedData();
  } else {
    MOZ_ASSERT(Reader()->IsWaitForDataSupported());
    shouldBuffer =
      (mMaster->OutOfDecodedAudio() && Reader()->IsWaitingAudioData()) ||
      (mMaster->OutOfDecodedVideo() && Reader()->IsWaitingVideoData());
  }
  if (shouldBuffer) {
    SetState<BufferingState>();
  }
}

void
MediaDecoderStateMachine::
SeekingState::SeekCompleted()
{
  int64_t seekTime = mSeekTask->GetSeekTarget().GetTime().ToMicroseconds();
  int64_t newCurrentTime = seekTime;

  // Setup timestamp state.
  RefPtr<MediaData> video = mMaster->VideoQueue().PeekFront();
  if (seekTime == mMaster->Duration().ToMicroseconds()) {
    newCurrentTime = seekTime;
  } else if (mMaster->HasAudio()) {
    RefPtr<MediaData> audio = AudioQueue().PeekFront();
    // Though we adjust the newCurrentTime in audio-based, and supplemented
    // by video. For better UX, should NOT bind the slide position to
    // the first audio data timestamp directly.
    // While seeking to a position where there's only either audio or video, or
    // seeking to a position lies before audio or video, we need to check if
    // seekTime is bounded in suitable duration. See Bug 1112438.
    int64_t audioStart = audio ? audio->mTime : seekTime;
    // We only pin the seek time to the video start time if the video frame
    // contains the seek time.
    if (video && video->mTime <= seekTime && video->GetEndTime() > seekTime) {
      newCurrentTime = std::min(audioStart, video->mTime);
    } else {
      newCurrentTime = audioStart;
    }
  } else {
    newCurrentTime = video ? video->mTime : seekTime;
  }

  bool isLiveStream = Resource()->IsLiveStream();
  if (newCurrentTime == mMaster->Duration().ToMicroseconds() && !isLiveStream) {
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
  }

  // We want to resolve the seek request prior finishing the first frame
  // to ensure that the seeked event is fired prior loadeded.
  mSeekJob.Resolve(__func__);

  // Notify FirstFrameLoaded now if we haven't since we've decoded some data
  // for readyState to transition to HAVE_CURRENT_DATA and fire 'loadeddata'.
  if (!mMaster->mSentFirstFrameLoadedEvent) {
    // Only MSE can start seeking before finishing decoding first frames.
    MOZ_ASSERT(Reader()->ForceZeroStartTime());
    mMaster->FinishDecodeFirstFrame();
  }

  // Ensure timestamps are up to date.
  if (!mSeekJob.mTarget.IsVideoOnly()) {
    // Don't update playback position for video-only seek.
    // Otherwise we might have |newCurrentTime > mMediaSink->GetPosition()|
    // and fail the assertion in GetClock() since we didn't stop MediaSink.
    mMaster->UpdatePlaybackPositionInternal(newCurrentTime);
  }

  // Try to decode another frame to detect if we're at the end...
  SLOG("Seek completed, mCurrentPosition=%lld", mMaster->mCurrentPosition.Ref());

  if (video) {
    mMaster->mMediaSink->Redraw(Info().mVideo);
    mMaster->mOnPlaybackEvent.Notify(MediaEventType::Invalidate);
  }

  SetState<DecodingState>();
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
    if ((isLiveStream || !mMaster->CanPlayThrough()) &&
        elapsed < TimeDuration::FromSeconds(mBufferingWait * mMaster->mPlaybackRate) &&
        mMaster->HasLowBufferedData(mBufferingWait * USECS_PER_S) &&
        IsExpectingMoreData()) {
      SLOG("Buffering: wait %ds, timeout in %.3lfs",
           mBufferingWait, mBufferingWait - elapsed.ToSeconds());
      mMaster->ScheduleStateMachineIn(USECS_PER_S);
      return;
    }
  } else if (mMaster->OutOfDecodedAudio() || mMaster->OutOfDecodedVideo()) {
    MOZ_ASSERT(Reader()->IsWaitForDataSupported(),
               "Don't yet have a strategy for non-heuristic + non-WaitForData");
    mMaster->DispatchDecodeTasksIfNeeded();
    MOZ_ASSERT(mMaster->mMinimizePreroll ||
               !mMaster->OutOfDecodedAudio() ||
               Reader()->IsRequestingAudioData() ||
               Reader()->IsWaitingAudioData());
    MOZ_ASSERT(mMaster->mMinimizePreroll ||
               !mMaster->OutOfDecodedVideo() ||
               Reader()->IsRequestingVideoData() ||
               Reader()->IsWaitingVideoData());
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
BufferingState::HandleEndOfStream()
{
  if (mMaster->CheckIfDecodeComplete()) {
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

  master->mIsShutdown = true;
  master->mDelayedScheduler.Reset();
  master->mBufferedUpdateRequest.DisconnectIfExists();

  // Shutdown happens while decode timer is active, we need to disconnect and
  // dispose of the timer.
  master->mVideoDecodeSuspendTimer.Reset();

  master->mCDMProxyPromise.DisconnectIfExists();

  if (master->IsPlaying()) {
    master->StopPlayback();
  }

  // To break the cycle-reference between MediaDecoderReaderWrapper and MDSM.
  master->CancelMediaDecoderReaderWrapperCallback();

  master->Reset();

  master->mMediaSink->Shutdown();

  // Prevent dangling pointers by disconnecting the listeners.
  master->mAudioQueueListener.Disconnect();
  master->mVideoQueueListener.Disconnect();
  master->mMetadataManager.Disconnect();
  master->mOnMediaNotSeekable.Disconnect();

  // Disconnect canonicals and mirrors before shutting down our task queue.
  master->mBuffered.DisconnectIfConnected();
  master->mEstimatedDuration.DisconnectIfConnected();
  master->mExplicitDuration.DisconnectIfConnected();
  master->mPlayState.DisconnectIfConnected();
  master->mNextPlayState.DisconnectIfConnected();
  master->mVolume.DisconnectIfConnected();
  master->mPreservesPitch.DisconnectIfConnected();
  master->mSameOriginMedia.DisconnectIfConnected();
  master->mMediaPrincipalHandle.DisconnectIfConnected();
  master->mPlaybackBytesPerSecond.DisconnectIfConnected();
  master->mPlaybackRateReliable.DisconnectIfConnected();
  master->mDecoderPosition.DisconnectIfConnected();
  master->mIsVisible.DisconnectIfConnected();

  master->mDuration.DisconnectAll();
  master->mIsShutdown.DisconnectAll();
  master->mNextFrameStatus.DisconnectAll();
  master->mCurrentPosition.DisconnectAll();
  master->mPlaybackOffset.DisconnectAll();
  master->mIsAudioDataAudible.DisconnectAll();

  // Shut down the watch manager to stop further notifications.
  master->mWatchManager.Shutdown();

  return Reader()->Shutdown()
    ->Then(OwnerThread(), __func__, master,
           &MediaDecoderStateMachine::FinishShutdown,
           &MediaDecoderStateMachine::FinishShutdown)
    ->CompletionPromise();
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
  mFrameStats(&aDecoder->GetFrameStatistics()),
  mVideoFrameContainer(aDecoder->GetVideoFrameContainer()),
  mAudioChannel(aDecoder->GetAudioChannel()),
  mTaskQueue(new TaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK),
                           /* aSupportsTailDispatch = */ true)),
  mWatchManager(this, mTaskQueue),
  mDispatchedStateMachine(false),
  mDelayedScheduler(mTaskQueue),
  mCurrentFrameID(0),
  INIT_WATCHABLE(mObservedDuration, TimeUnit()),
  mFragmentEndTime(-1),
  mReader(new MediaDecoderReaderWrapper(mTaskQueue, aReader)),
  mDecodedAudioEndTime(0),
  mDecodedVideoEndTime(0),
  mPlaybackRate(1.0),
  mLowAudioThresholdUsecs(detail::LOW_AUDIO_USECS),
  mAmpleAudioThresholdUsecs(detail::AMPLE_AUDIO_USECS),
  mAudioCaptured(false),
  mMinimizePreroll(false),
  mSentLoadedMetadataEvent(false),
  mSentFirstFrameLoadedEvent(false),
  mVideoDecodeSuspended(false),
  mVideoDecodeSuspendTimer(mTaskQueue),
  mOutputStreamManager(new OutputStreamManager()),
  mResource(aDecoder->GetResource()),
  mAudioOffloading(false),
  INIT_MIRROR(mBuffered, TimeIntervals()),
  INIT_MIRROR(mEstimatedDuration, NullableTimeUnit()),
  INIT_MIRROR(mExplicitDuration, Maybe<double>()),
  INIT_MIRROR(mPlayState, MediaDecoder::PLAY_STATE_LOADING),
  INIT_MIRROR(mNextPlayState, MediaDecoder::PLAY_STATE_PAUSED),
  INIT_MIRROR(mVolume, 1.0),
  INIT_MIRROR(mPreservesPitch, true),
  INIT_MIRROR(mSameOriginMedia, false),
  INIT_MIRROR(mMediaPrincipalHandle, PRINCIPAL_HANDLE_NONE),
  INIT_MIRROR(mPlaybackBytesPerSecond, 0.0),
  INIT_MIRROR(mPlaybackRateReliable, true),
  INIT_MIRROR(mDecoderPosition, 0),
  INIT_MIRROR(mIsVisible, true),
  INIT_CANONICAL(mDuration, NullableTimeUnit()),
  INIT_CANONICAL(mIsShutdown, false),
  INIT_CANONICAL(mNextFrameStatus, MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE),
  INIT_CANONICAL(mCurrentPosition, 0),
  INIT_CANONICAL(mPlaybackOffset, 0),
  INIT_CANONICAL(mIsAudioDataAudible, false)
{
  MOZ_COUNT_CTOR(MediaDecoderStateMachine);
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  InitVideoQueuePrefs();

#ifdef XP_WIN
  // Ensure high precision timers are enabled on Windows, otherwise the state
  // machine isn't woken up at reliable intervals to set the next frame,
  // and we drop frames while painting. Note that multiple calls to this
  // function per-process is OK, provided each call is matched by a corresponding
  // timeEndPeriod() call.
  timeBeginPeriod(1);
#endif
}

#undef INIT_WATCHABLE
#undef INIT_MIRROR
#undef INIT_CANONICAL

MediaDecoderStateMachine::~MediaDecoderStateMachine()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  MOZ_COUNT_DTOR(MediaDecoderStateMachine);

#ifdef XP_WIN
  timeEndPeriod(1);
#endif
}

void
MediaDecoderStateMachine::InitializationTask(MediaDecoder* aDecoder)
{
  MOZ_ASSERT(OnTaskQueue());

  // Connect mirrors.
  mBuffered.Connect(mReader->CanonicalBuffered());
  mEstimatedDuration.Connect(aDecoder->CanonicalEstimatedDuration());
  mExplicitDuration.Connect(aDecoder->CanonicalExplicitDuration());
  mPlayState.Connect(aDecoder->CanonicalPlayState());
  mNextPlayState.Connect(aDecoder->CanonicalNextPlayState());
  mVolume.Connect(aDecoder->CanonicalVolume());
  mPreservesPitch.Connect(aDecoder->CanonicalPreservesPitch());
  mSameOriginMedia.Connect(aDecoder->CanonicalSameOriginMedia());
  mMediaPrincipalHandle.Connect(aDecoder->CanonicalMediaPrincipalHandle());
  mPlaybackBytesPerSecond.Connect(aDecoder->CanonicalPlaybackBytesPerSecond());
  mPlaybackRateReliable.Connect(aDecoder->CanonicalPlaybackRateReliable());
  mDecoderPosition.Connect(aDecoder->CanonicalDecoderPosition());

  // Initialize watchers.
  mWatchManager.Watch(mBuffered, &MediaDecoderStateMachine::BufferedRangeUpdated);
  mWatchManager.Watch(mVolume, &MediaDecoderStateMachine::VolumeChanged);
  mWatchManager.Watch(mPreservesPitch, &MediaDecoderStateMachine::PreservesPitchChanged);
  mWatchManager.Watch(mEstimatedDuration, &MediaDecoderStateMachine::RecomputeDuration);
  mWatchManager.Watch(mExplicitDuration, &MediaDecoderStateMachine::RecomputeDuration);
  mWatchManager.Watch(mObservedDuration, &MediaDecoderStateMachine::RecomputeDuration);
  mWatchManager.Watch(mPlayState, &MediaDecoderStateMachine::PlayStateChanged);

  if (MediaPrefs::MDSMSuspendBackgroundVideoEnabled()) {
    mIsVisible.Connect(aDecoder->CanonicalIsVisible());
    mWatchManager.Watch(mIsVisible, &MediaDecoderStateMachine::VisibilityChanged);
  }

  // Configure MediaDecoderReaderWrapper.
  SetMediaDecoderReaderWrapperCallback();
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
    DecodedAudioDataSink* audioSink = new DecodedAudioDataSink(
      self->mTaskQueue, self->mAudioQueue, self->GetMediaTime(),
      self->Info().mAudio, self->mAudioChannel);

    self->mAudibleListener = audioSink->AudibleEvent().Connect(
      self->mTaskQueue, self.get(), &MediaDecoderStateMachine::AudioAudibleChanged);
    return audioSink;
  };
  return new AudioSinkWrapper(mTaskQueue, audioSinkCreator);
}

already_AddRefed<media::MediaSink>
MediaDecoderStateMachine::CreateMediaSink(bool aAudioCaptured)
{
  RefPtr<media::MediaSink> audioSink = aAudioCaptured
    ? new DecodedStream(mTaskQueue, mAudioQueue, mVideoQueue,
                        mOutputStreamManager, mSameOriginMedia.Ref(),
                        mMediaPrincipalHandle.Ref())
    : CreateAudioSink();

  RefPtr<media::MediaSink> mediaSink =
    new VideoSink(mTaskQueue, audioSink, mVideoQueue,
                  mVideoFrameContainer, *mFrameStats,
                  sVideoQueueSendToCompositorSize);
  return mediaSink.forget();
}

int64_t
MediaDecoderStateMachine::GetDecodedAudioDuration()
{
  MOZ_ASSERT(OnTaskQueue());
  if (mMediaSink->IsStarted()) {
    // mDecodedAudioEndTime might be smaller than GetClock() when there is
    // overlap between 2 adjacent audio samples or when we are playing
    // a chained ogg file.
    return std::max<int64_t>(mDecodedAudioEndTime - GetClock(), 0);
  }
  // MediaSink not started. All audio samples are in the queue.
  return AudioQueue().Duration();
}

bool MediaDecoderStateMachine::HaveEnoughDecodedAudio()
{
  MOZ_ASSERT(OnTaskQueue());

  int64_t ampleAudioUSecs = mAmpleAudioThresholdUsecs * mPlaybackRate;
  if (AudioQueue().GetSize() == 0 ||
      GetDecodedAudioDuration() < ampleAudioUSecs) {
    return false;
  }

  // MDSM will ensure buffering level is high enough for playback speed at 1x
  // at which the DecodedStream is playing.
  return true;
}

bool MediaDecoderStateMachine::HaveEnoughDecodedVideo()
{
  MOZ_ASSERT(OnTaskQueue());

  if (VideoQueue().GetSize() == 0) {
    return false;
  }

  if (VideoQueue().GetSize() - 1 < GetAmpleVideoFrames() * mPlaybackRate) {
    return false;
  }

  return true;
}

bool
MediaDecoderStateMachine::NeedToDecodeVideo()
{
  MOZ_ASSERT(OnTaskQueue());
  SAMPLE_LOG("NeedToDecodeVideo() isDec=%d minPrl=%d enufVid=%d",
             IsVideoDecoding(), mMinimizePreroll, HaveEnoughDecodedVideo());
  return IsVideoDecoding() &&
         mState != DECODER_STATE_SEEKING &&
         ((!mSentFirstFrameLoadedEvent && VideoQueue().GetSize() == 0) ||
          (!mMinimizePreroll && !HaveEnoughDecodedVideo()));
}

bool
MediaDecoderStateMachine::NeedToSkipToNextKeyframe()
{
  MOZ_ASSERT(OnTaskQueue());
  // Don't skip when we're still decoding first frames.
  if (!mSentFirstFrameLoadedEvent) {
    return false;
  }
  MOZ_ASSERT(mState == DECODER_STATE_DECODING ||
             mState == DECODER_STATE_BUFFERING ||
             mState == DECODER_STATE_SEEKING);

  // Since GetClock() can only be called after starting MediaSink, we return
  // false quickly if it is not started because we won't fall behind playback
  // when not consuming media data.
  if (!mMediaSink->IsStarted()) {
    return false;
  }

  // We are in seeking or buffering states, don't skip frame.
  if (!IsVideoDecoding() || mState == DECODER_STATE_BUFFERING ||
      mState == DECODER_STATE_SEEKING) {
    return false;
  }

  // Don't skip frame for video-only decoded stream because the clock time of
  // the stream relies on the video frame.
  if (mAudioCaptured && !HasAudio()) {
    return false;
  }

  // We'll skip the video decode to the next keyframe if we're low on
  // audio, or if we're low on video, provided we're not running low on
  // data to decode. If we're running low on downloaded data to decode,
  // we won't start keyframe skipping, as we'll be pausing playback to buffer
  // soon anyway and we'll want to be able to display frames immediately
  // after buffering finishes. We ignore the low audio calculations for
  // readers that are async, as since their audio decode runs on a different
  // task queue it should never run low and skipping won't help their decode.
  bool isLowOnDecodedAudio = !mReader->IsAsync() &&
                             IsAudioDecoding() &&
                             (GetDecodedAudioDuration() <
                              mLowAudioThresholdUsecs * mPlaybackRate);
  bool isLowOnDecodedVideo = (GetClock() - mDecodedVideoEndTime) * mPlaybackRate >
                             LOW_VIDEO_THRESHOLD_USECS;
  bool lowBuffered = HasLowBufferedData();

  if ((isLowOnDecodedAudio || isLowOnDecodedVideo) && !lowBuffered) {
    DECODER_LOG("Skipping video decode to the next keyframe lowAudio=%d lowVideo=%d lowUndecoded=%d async=%d",
                isLowOnDecodedAudio, isLowOnDecodedVideo, lowBuffered, mReader->IsAsync());
    return true;
  }

  return false;
}

bool
MediaDecoderStateMachine::NeedToDecodeAudio()
{
  MOZ_ASSERT(OnTaskQueue());
  SAMPLE_LOG("NeedToDecodeAudio() isDec=%d minPrl=%d enufAud=%d",
             IsAudioDecoding(), mMinimizePreroll, HaveEnoughDecodedAudio());

  return IsAudioDecoding() &&
         mState != DECODER_STATE_SEEKING &&
         ((!mSentFirstFrameLoadedEvent && AudioQueue().GetSize() == 0) ||
          (!mMinimizePreroll && !HaveEnoughDecodedAudio()));
}

void
MediaDecoderStateMachine::OnAudioDecoded(MediaData* aAudio)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(aAudio);

  // audio->GetEndTime() is not always mono-increasing in chained ogg.
  mDecodedAudioEndTime = std::max(aAudio->GetEndTime(), mDecodedAudioEndTime);

  SAMPLE_LOG("OnAudioDecoded [%lld,%lld]", aAudio->mTime, aAudio->GetEndTime());

  mStateObj->HandleAudioDecoded(aAudio);
}

void
MediaDecoderStateMachine::Push(MediaData* aSample, MediaData::Type aSampleType)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(aSample);

  if (aSample->mType == MediaData::AUDIO_DATA) {
    // TODO: Send aSample to MSG and recalculate readystate before pushing,
    // otherwise AdvanceFrame may pop the sample before we have a chance
    // to reach playing.
    AudioQueue().Push(aSample);
  } else if (aSample->mType == MediaData::VIDEO_DATA) {
    // TODO: Send aSample to MSG and recalculate readystate before pushing,
    // otherwise AdvanceFrame may pop the sample before we have a chance
    // to reach playing.
    aSample->As<VideoData>()->mFrameID = ++mCurrentFrameID;
    VideoQueue().Push(aSample);
  } else {
    // TODO: Handle MediaRawData, determine which queue should be pushed.
  }
  DispatchDecodeTasksIfNeeded();
}

void
MediaDecoderStateMachine::OnAudioPopped(const RefPtr<MediaData>& aSample)
{
  MOZ_ASSERT(OnTaskQueue());

  mPlaybackOffset = std::max(mPlaybackOffset.Ref(), aSample->mOffset);
  DispatchAudioDecodeTaskIfNeeded();
}

void
MediaDecoderStateMachine::OnVideoPopped(const RefPtr<MediaData>& aSample)
{
  MOZ_ASSERT(OnTaskQueue());
  mPlaybackOffset = std::max(mPlaybackOffset.Ref(), aSample->mOffset);
  DispatchVideoDecodeTaskIfNeeded();
}

void
MediaDecoderStateMachine::OnNotDecoded(MediaData::Type aType,
                                       const MediaResult& aError)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mState != DECODER_STATE_SEEKING);

  SAMPLE_LOG("OnNotDecoded (aType=%u, aError=%u)", aType, aError.Code());
  bool isAudio = aType == MediaData::AUDIO_DATA;
  MOZ_ASSERT_IF(!isAudio, aType == MediaData::VIDEO_DATA);

  if (IsShutdown()) {
    // Already shutdown;
    return;
  }

  // If the decoder is waiting for data, we tell it to call us back when the
  // data arrives.
  if (aError == NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA) {
    MOZ_ASSERT(mReader->IsWaitForDataSupported(),
               "Readers that send WAITING_FOR_DATA need to implement WaitForData");
    mReader->WaitForData(aType);
    mStateObj->HandleWaitingForData();
    return;
  }

  if (aError == NS_ERROR_DOM_MEDIA_CANCELED) {
    if (isAudio) {
      EnsureAudioDecodeTaskQueued();
    } else {
      EnsureVideoDecodeTaskQueued();
    }
    return;
  }

  // If this is a decode error, delegate to the generic error path.
  if (aError != NS_ERROR_DOM_MEDIA_END_OF_STREAM) {
    DecodeError(aError);
    return;
  }

  // This is an EOS. Finish off the queue, and then handle things based on our
  // state.
  if (isAudio) {
    AudioQueue().Finish();
  } else {
    VideoQueue().Finish();
  }

  mStateObj->HandleEndOfStream();
}

void
MediaDecoderStateMachine::OnVideoDecoded(MediaData* aVideo,
                                         TimeStamp aDecodeStartTime)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(aVideo);

  // Handle abnormal or negative timestamps.
  mDecodedVideoEndTime = std::max(mDecodedVideoEndTime, aVideo->GetEndTime());

  SAMPLE_LOG("OnVideoDecoded [%lld,%lld]", aVideo->mTime, aVideo->GetEndTime());

  mStateObj->HandleVideoDecoded(aVideo, aDecodeStartTime);
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

bool
MediaDecoderStateMachine::CheckIfDecodeComplete()
{
  MOZ_ASSERT(OnTaskQueue());
  // DecodeComplete is possible only after decoding first frames.
  MOZ_ASSERT(mSentFirstFrameLoadedEvent);
  MOZ_ASSERT(mState == DECODER_STATE_DECODING ||
             mState == DECODER_STATE_BUFFERING);
  return !IsVideoDecoding() && !IsAudioDecoding();
}

bool MediaDecoderStateMachine::IsPlaying() const
{
  MOZ_ASSERT(OnTaskQueue());
  return mMediaSink->IsPlaying();
}

nsresult MediaDecoderStateMachine::Init(MediaDecoder* aDecoder)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Dispatch initialization that needs to happen on that task queue.
  nsCOMPtr<nsIRunnable> r = NewRunnableMethod<RefPtr<MediaDecoder>>(
    this, &MediaDecoderStateMachine::InitializationTask, aDecoder);
  mTaskQueue->Dispatch(r.forget());

  mAudioQueueListener = AudioQueue().PopEvent().Connect(
    mTaskQueue, this, &MediaDecoderStateMachine::OnAudioPopped);
  mVideoQueueListener = VideoQueue().PopEvent().Connect(
    mTaskQueue, this, &MediaDecoderStateMachine::OnVideoPopped);

  mMetadataManager.Connect(mReader->TimedMetadataEvent(), OwnerThread());

  mOnMediaNotSeekable = mReader->OnMediaNotSeekable().Connect(
    OwnerThread(), [this] () {
      mMediaSeekable = false;
    });

  mMediaSink = CreateMediaSink(mAudioCaptured);

  mCDMProxyPromise.Begin(aDecoder->RequestCDMProxy()->Then(
    OwnerThread(), __func__, this,
    &MediaDecoderStateMachine::OnCDMProxyReady,
    &MediaDecoderStateMachine::OnCDMProxyNotReady));

  nsresult rv = mReader->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<MediaDecoderStateMachine> self = this;
  OwnerThread()->Dispatch(NS_NewRunnableFunction([self] () {
    MOZ_ASSERT(self->mState == DECODER_STATE_DECODING_METADATA);
    MOZ_ASSERT(!self->mStateObj);
    auto s = new DecodeMetadataState(self);
    self->mStateObj.reset(s);
    s->Enter();
  }));

  return NS_OK;
}

void
MediaDecoderStateMachine::SetMediaDecoderReaderWrapperCallback()
{
  MOZ_ASSERT(OnTaskQueue());

  mAudioCallback = mReader->AudioCallback().Connect(
    mTaskQueue, [this] (AudioCallbackData aData) {
    if (aData.is<MediaData*>()) {
      OnAudioDecoded(aData.as<MediaData*>());
    } else {
      OnNotDecoded(MediaData::AUDIO_DATA, aData.as<MediaResult>());
    }
  });

  mVideoCallback = mReader->VideoCallback().Connect(
    mTaskQueue, [this] (VideoCallbackData aData) {
    typedef Tuple<MediaData*, TimeStamp> Type;
    if (aData.is<Type>()) {
      auto&& v = aData.as<Type>();
      OnVideoDecoded(Get<0>(v), Get<1>(v));
    } else {
      OnNotDecoded(MediaData::VIDEO_DATA, aData.as<MediaResult>());
    }
  });

  mAudioWaitCallback = mReader->AudioWaitCallback().Connect(
    mTaskQueue, [this] (WaitCallbackData aData) {
    if (aData.is<MediaData::Type>()) {
      EnsureAudioDecodeTaskQueued();
    }
  });

  mVideoWaitCallback = mReader->VideoWaitCallback().Connect(
    mTaskQueue, [this] (WaitCallbackData aData) {
    if (aData.is<MediaData::Type>()) {
      EnsureVideoDecodeTaskQueued();
    }
  });
}

void
MediaDecoderStateMachine::CancelMediaDecoderReaderWrapperCallback()
{
  MOZ_ASSERT(OnTaskQueue());
  mAudioCallback.Disconnect();
  mVideoCallback.Disconnect();
  mAudioWaitCallback.Disconnect();
  mVideoWaitCallback.Disconnect();
}

void MediaDecoderStateMachine::StopPlayback()
{
  MOZ_ASSERT(OnTaskQueue());
  DECODER_LOG("StopPlayback()");

  mOnPlaybackEvent.Notify(MediaEventType::PlaybackStopped);

  if (IsPlaying()) {
    mMediaSink->SetPlaying(false);
    MOZ_ASSERT(!IsPlaying());
  }

  DispatchDecodeTasksIfNeeded();
}

void MediaDecoderStateMachine::MaybeStartPlayback()
{
  MOZ_ASSERT(OnTaskQueue());
  // Should try to start playback only after decoding first frames.
  MOZ_ASSERT(mSentFirstFrameLoadedEvent);
  MOZ_ASSERT(mState == DECODER_STATE_DECODING ||
             mState == DECODER_STATE_COMPLETED);

  if (IsPlaying()) {
    // Logging this case is really spammy - don't do it.
    return;
  }

  bool playStatePermits = mPlayState == MediaDecoder::PLAY_STATE_PLAYING;
  if (!playStatePermits || mAudioOffloading) {
    DECODER_LOG("Not starting playback [playStatePermits: %d, "
                "mAudioOffloading: %d]",
                playStatePermits, mAudioOffloading);
    return;
  }

  DECODER_LOG("MaybeStartPlayback() starting playback");
  mOnPlaybackEvent.Notify(MediaEventType::PlaybackStarted);
  StartMediaSink();

  if (!IsPlaying()) {
    mMediaSink->SetPlaying(true);
    MOZ_ASSERT(IsPlaying());
  }

  DispatchDecodeTasksIfNeeded();
}

void MediaDecoderStateMachine::UpdatePlaybackPositionInternal(int64_t aTime)
{
  MOZ_ASSERT(OnTaskQueue());
  SAMPLE_LOG("UpdatePlaybackPositionInternal(%lld)", aTime);

  mCurrentPosition = aTime;
  NS_ASSERTION(mCurrentPosition >= 0, "CurrentTime should be positive!");
  mObservedDuration = std::max(mObservedDuration.Ref(),
                               TimeUnit::FromMicroseconds(mCurrentPosition.Ref()));
}

void MediaDecoderStateMachine::UpdatePlaybackPosition(int64_t aTime)
{
  MOZ_ASSERT(OnTaskQueue());
  UpdatePlaybackPositionInternal(aTime);

  bool fragmentEnded = mFragmentEndTime >= 0 && GetMediaTime() >= mFragmentEndTime;
  mMetadataManager.DispatchMetadataIfNeeded(TimeUnit::FromMicroseconds(aTime));

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
  return ToStateStr(mState);
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
  } else if (mEstimatedDuration.Ref().isSome()) {
    duration = mEstimatedDuration.Ref().ref();
  } else if (Info().mMetadataDuration.isSome()) {
    duration = Info().mMetadataDuration.ref();
  } else {
    return;
  }

  // Only adjust the duration when an explicit duration isn't set (MSE).
  // The duration is always exactly known with MSE and there's no need to adjust
  // it based on what may have been seen in the past; in particular as this data
  // may no longer exist such as when the mediasource duration was reduced.
  if (mExplicitDuration.Ref().isNothing() &&
      duration < mObservedDuration.Ref()) {
    duration = mObservedDuration;
  }

  MOZ_ASSERT(duration.ToMicroseconds() >= 0);
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
    mVideoDecodeSuspendTimer.Reset();
  } else if (mMinimizePreroll) {
    // Once we start playing, we don't want to minimize our prerolling, as we
    // assume the user is likely to want to keep playing in future. This needs to
    // happen before we invoke StartDecoding().
    mMinimizePreroll = false;
    DispatchDecodeTasksIfNeeded();
  }

  mStateObj->HandlePlayStateChanged(mPlayState);
}

void MediaDecoderStateMachine::VisibilityChanged()
{
  MOZ_ASSERT(OnTaskQueue());
  DECODER_LOG("VisibilityChanged: mIsVisible=%d, mVideoDecodeSuspended=%c",
              mIsVisible.Ref(), mVideoDecodeSuspended ? 'T' : 'F');

  // Start timer to trigger suspended decoding state when going invisible.
  if (!mIsVisible) {
    TimeStamp target = TimeStamp::Now() + SuspendBackgroundVideoDelay();

    RefPtr<MediaDecoderStateMachine> self = this;
    mVideoDecodeSuspendTimer.Ensure(target,
                                    [=]() { self->OnSuspendTimerResolved(); },
                                    [=]() { self->OnSuspendTimerRejected(); });
    return;
  }

  // Resuming from suspended decoding

  // If suspend timer exists, destroy it.
  mVideoDecodeSuspendTimer.Reset();

  if (mVideoDecodeSuspended) {
    mStateObj->HandleResumeVideoDecoding();
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
MediaDecoderStateMachine::Seek(SeekTarget aTarget)
{
  MOZ_ASSERT(OnTaskQueue());

  if (IsShutdown()) {
    return MediaDecoder::SeekPromise::CreateAndReject(/* aIgnored = */ true, __func__);
  }

  // We need to be able to seek in some way
  if (!mMediaSeekable && !mMediaSeekableOnlyInBufferedRanges) {
    DECODER_WARN("Seek() function should not be called on a non-seekable state machine");
    return MediaDecoder::SeekPromise::CreateAndReject(/* aIgnored = */ true, __func__);
  }

  if (aTarget.IsNextFrame() && !HasVideo()) {
    DECODER_WARN("Ignore a NextFrameSeekTask on a media file without video track.");
    return MediaDecoder::SeekPromise::CreateAndReject(/* aIgnored = */ true, __func__);
  }

  MOZ_ASSERT(mDuration.Ref().isSome(), "We should have got duration already");

  return mStateObj->HandleSeek(aTarget);
}

RefPtr<MediaDecoder::SeekPromise>
MediaDecoderStateMachine::InvokeSeek(SeekTarget aTarget)
{
  return InvokeAsync(OwnerThread(), this, __func__,
                     &MediaDecoderStateMachine::Seek, aTarget);
}

void MediaDecoderStateMachine::StopMediaSink()
{
  MOZ_ASSERT(OnTaskQueue());
  if (mMediaSink->IsStarted()) {
    DECODER_LOG("Stop MediaSink");
    mAudibleListener.DisconnectIfExists();

    mMediaSink->Stop();
    mMediaSinkAudioPromise.DisconnectIfExists();
    mMediaSinkVideoPromise.DisconnectIfExists();
  }
}

void
MediaDecoderStateMachine::DispatchDecodeTasksIfNeeded()
{
  MOZ_ASSERT(OnTaskQueue());

  if (mState != DECODER_STATE_DECODING &&
      mState != DECODER_STATE_DECODING_FIRSTFRAME &&
      mState != DECODER_STATE_BUFFERING &&
      mState != DECODER_STATE_SEEKING) {
    return;
  }

  // NeedToDecodeAudio() can go from false to true while we hold the
  // monitor, but it can't go from true to false. This can happen because
  // NeedToDecodeAudio() takes into account the amount of decoded audio
  // that's been written to the AudioStream but not played yet. So if we
  // were calling NeedToDecodeAudio() twice and we thread-context switch
  // between the calls, audio can play, which can affect the return value
  // of NeedToDecodeAudio() giving inconsistent results. So we cache the
  // value returned by NeedToDecodeAudio(), and make decisions
  // based on the cached value. If NeedToDecodeAudio() has
  // returned false, and then subsequently returns true and we're not
  // playing, it will probably be OK since we don't need to consume data
  // anyway.

  const bool needToDecodeAudio = NeedToDecodeAudio();
  const bool needToDecodeVideo = NeedToDecodeVideo();

  // If we're in completed state, we should not need to decode anything else.
  MOZ_ASSERT(mState != DECODER_STATE_COMPLETED ||
             (!needToDecodeAudio && !needToDecodeVideo));

  bool needIdle = !IsLogicallyPlaying() &&
                  mState != DECODER_STATE_SEEKING &&
                  !needToDecodeAudio &&
                  !needToDecodeVideo &&
                  !IsPlaying();

  SAMPLE_LOG("DispatchDecodeTasksIfNeeded needAudio=%d audioStatus=%s needVideo=%d videoStatus=%s needIdle=%d",
             needToDecodeAudio, AudioRequestStatus(),
             needToDecodeVideo, VideoRequestStatus(),
             needIdle);

  if (needToDecodeAudio) {
    EnsureAudioDecodeTaskQueued();
  }
  if (needToDecodeVideo) {
    EnsureVideoDecodeTaskQueued();
  }

  if (needIdle) {
    DECODER_LOG("Dispatching SetIdle() audioQueue=%lld videoQueue=%lld",
                GetDecodedAudioDuration(),
                VideoQueue().Duration());
    mReader->SetIdle();
  }
}

void
MediaDecoderStateMachine::DispatchAudioDecodeTaskIfNeeded()
{
  MOZ_ASSERT(OnTaskQueue());
  if (!IsShutdown() && NeedToDecodeAudio()) {
    EnsureAudioDecodeTaskQueued();
  }
}

void
MediaDecoderStateMachine::EnsureAudioDecodeTaskQueued()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mState != DECODER_STATE_SEEKING);

  SAMPLE_LOG("EnsureAudioDecodeTaskQueued isDecoding=%d status=%s",
              IsAudioDecoding(), AudioRequestStatus());

  if (mState != DECODER_STATE_DECODING &&
      mState != DECODER_STATE_DECODING_FIRSTFRAME &&
      mState != DECODER_STATE_BUFFERING) {
    return;
  }

  if (!IsAudioDecoding() ||
      mReader->IsRequestingAudioData() ||
      mReader->IsWaitingAudioData()) {
    return;
  }

  RequestAudioData();
}

void
MediaDecoderStateMachine::RequestAudioData()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mState != DECODER_STATE_SEEKING);

  SAMPLE_LOG("Queueing audio task - queued=%i, decoder-queued=%o",
             AudioQueue().GetSize(), mReader->SizeOfAudioQueueInFrames());

  mReader->RequestAudioData();
}

void
MediaDecoderStateMachine::DispatchVideoDecodeTaskIfNeeded()
{
  MOZ_ASSERT(OnTaskQueue());
  if (!IsShutdown() && NeedToDecodeVideo()) {
    EnsureVideoDecodeTaskQueued();
  }
}

void
MediaDecoderStateMachine::EnsureVideoDecodeTaskQueued()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mState != DECODER_STATE_SEEKING);

  SAMPLE_LOG("EnsureVideoDecodeTaskQueued isDecoding=%d status=%s",
             IsVideoDecoding(), VideoRequestStatus());

  if (mState != DECODER_STATE_DECODING &&
      mState != DECODER_STATE_DECODING_FIRSTFRAME &&
      mState != DECODER_STATE_BUFFERING) {
    return;
  }

  if (!IsVideoDecoding() ||
      mReader->IsRequestingVideoData() ||
      mReader->IsWaitingVideoData()) {
    return;
  }

  RequestVideoData();
}

void
MediaDecoderStateMachine::RequestVideoData()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mState != DECODER_STATE_SEEKING);

  bool skipToNextKeyFrame = NeedToSkipToNextKeyframe();

  media::TimeUnit currentTime = media::TimeUnit::FromMicroseconds(GetMediaTime());

  SAMPLE_LOG("Queueing video task - queued=%i, decoder-queued=%o, skip=%i, time=%lld",
             VideoQueue().GetSize(), mReader->SizeOfVideoQueueInFrames(), skipToNextKeyFrame,
             currentTime.ToMicroseconds());

  // MediaDecoderReaderWrapper::RequestVideoData() records the decoding start
  // time and sent it back to MDSM::OnVideoDecoded() so that if the decoding is
  // slow, we can increase our low audio threshold to reduce the chance of an
  // audio underrun while we're waiting for a video decode to complete.
  mReader->RequestVideoData(skipToNextKeyFrame, currentTime);
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
      mMediaSinkAudioPromise.Begin(audioPromise->Then(
        OwnerThread(), __func__, this,
        &MediaDecoderStateMachine::OnMediaSinkAudioComplete,
        &MediaDecoderStateMachine::OnMediaSinkAudioError));
    }
    if (videoPromise) {
      mMediaSinkVideoPromise.Begin(videoPromise->Then(
        OwnerThread(), __func__, this,
        &MediaDecoderStateMachine::OnMediaSinkVideoComplete,
        &MediaDecoderStateMachine::OnMediaSinkVideoError));
    }
  }
}

bool
MediaDecoderStateMachine::HasLowDecodedAudio()
{
  MOZ_ASSERT(OnTaskQueue());
  return IsAudioDecoding() &&
         GetDecodedAudioDuration() < EXHAUSTED_DATA_MARGIN_USECS * mPlaybackRate;
}

bool
MediaDecoderStateMachine::HasLowDecodedVideo()
{
  MOZ_ASSERT(OnTaskQueue());
  return IsVideoDecoding() &&
         VideoQueue().GetSize() < LOW_VIDEO_FRAMES * mPlaybackRate;
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
    return IsAudioDecoding() && !AudioQueue().IsFinished() &&
           AudioQueue().GetSize() == 0 &&
           !mMediaSink->HasUnplayedFrames(TrackInfo::kAudioTrack);
}

bool MediaDecoderStateMachine::HasLowBufferedData()
{
  MOZ_ASSERT(OnTaskQueue());
  return HasLowBufferedData(detail::LOW_DATA_THRESHOLD_USECS);
}

bool MediaDecoderStateMachine::HasLowBufferedData(int64_t aUsecs)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mState >= DECODER_STATE_DECODING,
             "Must have loaded first frame for mBuffered to be valid");

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
  int64_t endOfDecodedVideoData =
    (HasVideo() && !VideoQueue().IsFinished())
      ? mDecodedVideoEndTime
      : INT64_MAX;
  int64_t endOfDecodedAudioData =
    (HasAudio() && !AudioQueue().IsFinished())
      ? mDecodedAudioEndTime
      : INT64_MAX;

  int64_t endOfDecodedData = std::min(endOfDecodedVideoData, endOfDecodedAudioData);
  if (Duration().ToMicroseconds() < endOfDecodedData) {
    // Our duration is not up to date. No point buffering.
    return false;
  }

  if (endOfDecodedData == INT64_MAX) {
    // Have decoded all samples. No point buffering.
    return false;
  }

  int64_t start = endOfDecodedData;
  int64_t end = std::min(GetMediaTime() + aUsecs, Duration().ToMicroseconds());
  if (start >= end) {
    // Duration of decoded samples is greater than our threshold.
    return false;
  }
  media::TimeInterval interval(media::TimeUnit::FromMicroseconds(start),
                               media::TimeUnit::FromMicroseconds(end));
  return !mBuffered.Ref().Contains(interval);
}

void
MediaDecoderStateMachine::DecodeError(const MediaResult& aError)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(!IsShutdown());
  DECODER_WARN("Decode error");
  // Notify the decode error and MediaDecoder will shut down MDSM.
  mOnPlaybackErrorEvent.Notify(aError);
}

void
MediaDecoderStateMachine::EnqueueLoadedMetadataEvent()
{
  MOZ_ASSERT(OnTaskQueue());
  MediaDecoderEventVisibility visibility =
    mSentLoadedMetadataEvent ? MediaDecoderEventVisibility::Suppressed
                             : MediaDecoderEventVisibility::Observable;
  mMetadataLoadedEvent.Notify(nsAutoPtr<MediaInfo>(new MediaInfo(Info())),
                              Move(mMetadataTags),
                              visibility);
  mSentLoadedMetadataEvent = true;
}

void
MediaDecoderStateMachine::EnqueueFirstFrameLoadedEvent()
{
  MOZ_ASSERT(OnTaskQueue());
  // Track value of mSentFirstFrameLoadedEvent from before updating it
  bool firstFrameBeenLoaded = mSentFirstFrameLoadedEvent;
  mSentFirstFrameLoadedEvent = true;
  RefPtr<MediaDecoderStateMachine> self = this;
  mBufferedUpdateRequest.Begin(
    mReader->UpdateBufferedWithPromise()
    ->Then(OwnerThread(),
    __func__,
    // Resolve
    [self, firstFrameBeenLoaded]() {
      self->mBufferedUpdateRequest.Complete();
      MediaDecoderEventVisibility visibility =
        firstFrameBeenLoaded ? MediaDecoderEventVisibility::Suppressed
                             : MediaDecoderEventVisibility::Observable;
      self->mFirstFrameLoadedEvent.Notify(
        nsAutoPtr<MediaInfo>(new MediaInfo(self->Info())), visibility);
    },
    // Reject
    []() { MOZ_CRASH("Should not reach"); }));
}

void
MediaDecoderStateMachine::FinishDecodeFirstFrame()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(!mSentFirstFrameLoadedEvent);
  DECODER_LOG("FinishDecodeFirstFrame");

  mMediaSink->Redraw(Info().mVideo);

  // If we don't know the duration by this point, we assume infinity, per spec.
  if (mDuration.Ref().isNothing()) {
    mDuration = Some(TimeUnit::FromInfinity());
  }

  DECODER_LOG("Media duration %lld, "
              "transportSeekable=%d, mediaSeekable=%d",
              Duration().ToMicroseconds(), mResource->IsTransportSeekable(), mMediaSeekable);

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
  MOZ_ASSERT(mState == DECODER_STATE_SHUTDOWN,
             "How did we escape from the shutdown state?");
  DECODER_LOG("Shutting down state machine task queue");
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
MediaDecoderStateMachine::Reset(TrackSet aTracks)
{
  MOZ_ASSERT(OnTaskQueue());
  DECODER_LOG("MediaDecoderStateMachine::Reset");

  // We should be resetting because we're seeking, shutting down, or entering
  // dormant state. We could also be in the process of going dormant, and have
  // just switched to exiting dormant before we finished entering dormant,
  // hence the DECODING_NONE case below.
  MOZ_ASSERT(IsShutdown() ||
             mState == DECODER_STATE_SEEKING ||
             mState == DECODER_STATE_DORMANT);

  // Assert that aTracks specifies to reset the video track because we
  // don't currently support resetting just the audio track.
  MOZ_ASSERT(aTracks.contains(TrackInfo::kVideoTrack));

  if (aTracks.contains(TrackInfo::kAudioTrack) &&
      aTracks.contains(TrackInfo::kVideoTrack)) {
    // Stop the audio thread. Otherwise, MediaSink might be accessing AudioQueue
    // outside of the decoder monitor while we are clearing the queue and causes
    // crash for no samples to be popped.
    StopMediaSink();
  }

  if (aTracks.contains(TrackInfo::kVideoTrack)) {
    mDecodedVideoEndTime = 0;
    mVideoCompleted = false;
    VideoQueue().Reset();
  }

  if (aTracks.contains(TrackInfo::kAudioTrack)) {
    mDecodedAudioEndTime = 0;
    mAudioCompleted = false;
    AudioQueue().Reset();
  }

  mPlaybackOffset = 0;

  mReader->ResetDecode(aTracks);
}

int64_t
MediaDecoderStateMachine::GetClock(TimeStamp* aTimeStamp) const
{
  MOZ_ASSERT(OnTaskQueue());
  int64_t clockTime = mMediaSink->GetPosition(aTimeStamp);
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
  if (VideoEndTime() != -1 || AudioEndTime() != -1) {

    const int64_t clockTime = GetClock();
    // Skip frames up to the frame at the playback position, and figure out
    // the time remaining until it's time to display the next frame and drop
    // the current frame.
    NS_ASSERTION(clockTime >= 0, "Should have positive clock time.");

    // These will be non -1 if we've displayed a video frame, or played an audio frame.
    int64_t t = std::min(clockTime, std::max(VideoEndTime(), AudioEndTime()));
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
  ScheduleStateMachineIn(delay);
}

/* static */ const char*
MediaDecoderStateMachine::ToStr(NextFrameStatus aStatus)
{
  switch (aStatus) {
    case MediaDecoderOwner::NEXT_FRAME_AVAILABLE: return "NEXT_FRAME_AVAILABLE";
    case MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE: return "NEXT_FRAME_UNAVAILABLE";
    case MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_BUFFERING: return "NEXT_FRAME_UNAVAILABLE_BUFFERING";
    case MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_SEEKING: return "NEXT_FRAME_UNAVAILABLE_SEEKING";
    case MediaDecoderOwner::NEXT_FRAME_UNINITIALIZED: return "NEXT_FRAME_UNINITIALIZED";
  }
  return "UNKNOWN";
}

void
MediaDecoderStateMachine::UpdateNextFrameStatus(NextFrameStatus aStatus)
{
  MOZ_ASSERT(OnTaskQueue());
  if (aStatus != mNextFrameStatus) {
    DECODER_LOG("Changed mNextFrameStatus to %s", ToStr(aStatus));
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
  result.mDownloadRate = mResource->GetDownloadRate(&result.mDownloadRateReliable);
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

  OwnerThread()->Dispatch(NewRunnableMethod(this, &MediaDecoderStateMachine::RunStateMachine));
}

void
MediaDecoderStateMachine::ScheduleStateMachineIn(int64_t aMicroseconds)
{
  MOZ_ASSERT(OnTaskQueue());          // mDelayedScheduler.Ensure() may Disconnect()
                                      // the promise, which must happen on the state
                                      // machine task queue.
  MOZ_ASSERT(aMicroseconds > 0);
  if (mDispatchedStateMachine) {
    return;
  }

  TimeStamp now = TimeStamp::Now();
  TimeStamp target = now + TimeDuration::FromMicroseconds(aMicroseconds);

  // It is OK to capture 'this' without causing UAF because the callback
  // always happens before shutdown.
  mDelayedScheduler.Ensure(target, [this] () {
    mDelayedScheduler.CompleteRequest();
    RunStateMachine();
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

bool
MediaDecoderStateMachine::IsShutdown() const
{
  MOZ_ASSERT(OnTaskQueue());
  return mIsShutdown;
}

int64_t
MediaDecoderStateMachine::AudioEndTime() const
{
  MOZ_ASSERT(OnTaskQueue());
  if (mMediaSink->IsStarted()) {
    return mMediaSink->GetEndTime(TrackInfo::kAudioTrack);
  }
  return -1;
}

int64_t
MediaDecoderStateMachine::VideoEndTime() const
{
  MOZ_ASSERT(OnTaskQueue());
  if (mMediaSink->IsStarted()) {
    return mMediaSink->GetEndTime(TrackInfo::kVideoTrack);
  }
  return -1;
}

void
MediaDecoderStateMachine::OnMediaSinkVideoComplete()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(HasVideo());
  VERBOSE_LOG("[%s]", __func__);

  mMediaSinkVideoPromise.Complete();
  mVideoCompleted = true;
  ScheduleStateMachine();
}

void
MediaDecoderStateMachine::OnMediaSinkVideoError()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(HasVideo());
  VERBOSE_LOG("[%s]", __func__);

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
  VERBOSE_LOG("[%s]", __func__);

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
  VERBOSE_LOG("[%s]", __func__);

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
  mAmpleAudioThresholdUsecs = mAudioCaptured ?
                              detail::AMPLE_AUDIO_USECS / 2 :
                              detail::AMPLE_AUDIO_USECS;

  mStateObj->HandleAudioCaptured();
}

uint32_t MediaDecoderStateMachine::GetAmpleVideoFrames() const
{
  MOZ_ASSERT(OnTaskQueue());
  return (mReader->IsAsync() && mReader->VideoIsHardwareAccelerated())
    ? std::max<uint32_t>(sVideoQueueHWAccelSize, MIN_VIDEO_QUEUE_SIZE)
    : std::max<uint32_t>(sVideoQueueDefaultSize, MIN_VIDEO_QUEUE_SIZE);
}

void
MediaDecoderStateMachine::DumpDebugInfo()
{
  MOZ_ASSERT(NS_IsMainThread());

  // It is fine to capture a raw pointer here because MediaDecoder only call
  // this function before shutdown begins.
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([this] () {
    mMediaSink->DumpDebugInfo();
    mStateObj->DumpDebugInfo();
    DUMP_LOG(
      "GetMediaTime=%lld GetClock=%lld mMediaSink=%p "
      "mState=%s mPlayState=%d mSentFirstFrameLoadedEvent=%d IsPlaying=%d "
      "mAudioStatus=%s mVideoStatus=%s mDecodedAudioEndTime=%lld mDecodedVideoEndTime=%lld "
      "mAudioCompleted=%d mVideoCompleted=%d",
      GetMediaTime(), mMediaSink->IsStarted() ? GetClock() : -1, mMediaSink.get(),
      ToStateStr(), mPlayState.Ref(), mSentFirstFrameLoadedEvent, IsPlaying(),
      AudioRequestStatus(), VideoRequestStatus(), mDecodedAudioEndTime, mDecodedVideoEndTime,
      mAudioCompleted, mVideoCompleted);
  });

  // Since the task is run asynchronously, it is possible other tasks get first
  // and change the object states before we print them. Therefore we want to
  // dispatch this task immediately without waiting for the tail dispatching
  // phase so object states are less likely to change before being printed.
  OwnerThread()->Dispatch(r.forget(),
    AbstractThread::AssertDispatchSuccess, AbstractThread::TailDispatch);
}

void MediaDecoderStateMachine::AddOutputStream(ProcessedMediaStream* aStream,
                                               bool aFinishWhenEnded)
{
  MOZ_ASSERT(NS_IsMainThread());
  DECODER_LOG("AddOutputStream aStream=%p!", aStream);
  mOutputStreamManager->Add(aStream, aFinishWhenEnded);
  nsCOMPtr<nsIRunnable> r = NewRunnableMethod<bool>(
    this, &MediaDecoderStateMachine::SetAudioCaptured, true);
  OwnerThread()->Dispatch(r.forget());
}

void MediaDecoderStateMachine::RemoveOutputStream(MediaStream* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  DECODER_LOG("RemoveOutputStream=%p!", aStream);
  mOutputStreamManager->Remove(aStream);
  if (mOutputStreamManager->IsEmpty()) {
    nsCOMPtr<nsIRunnable> r = NewRunnableMethod<bool>(
      this, &MediaDecoderStateMachine::SetAudioCaptured, false);
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
  if (mReader->IsRequestingAudioData()) {
    MOZ_DIAGNOSTIC_ASSERT(!mReader->IsWaitingAudioData());
    return "pending";
  } else if (mReader->IsWaitingAudioData()) {
    return "waiting";
  }
  return "idle";
}

const char*
MediaDecoderStateMachine::VideoRequestStatus() const
{
  MOZ_ASSERT(OnTaskQueue());
  if (mReader->IsRequestingVideoData()) {
    MOZ_DIAGNOSTIC_ASSERT(!mReader->IsWaitingVideoData());
    return "pending";
  } else if (mReader->IsWaitingVideoData()) {
    return "waiting";
  }
  return "idle";
}

void
MediaDecoderStateMachine::OnSuspendTimerResolved()
{
  DECODER_LOG("OnSuspendTimerResolved");
  mVideoDecodeSuspendTimer.CompleteRequest();
  mStateObj->HandleVideoSuspendTimeout();
}

void
MediaDecoderStateMachine::OnSuspendTimerRejected()
{
  DECODER_LOG("OnSuspendTimerRejected");
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(!mVideoDecodeSuspended);
  mVideoDecodeSuspendTimer.CompleteRequest();
}

} // namespace mozilla

#undef NS_DispatchToMainThread
