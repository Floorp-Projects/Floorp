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

#include "AudioSegment.h"
#include "DOMMediaStream.h"
#include "ImageContainer.h"
#include "MediaDecoder.h"
#include "MediaDecoderReader.h"
#include "MediaDecoderReaderWrapper.h"
#include "MediaDecoderStateMachine.h"
#include "MediaShutdownManager.h"
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
#undef LOG
#undef DECODER_LOG
#undef VERBOSE_LOG

#define LOG(m, l, x, ...) \
  MOZ_LOG(m, l, ("Decoder=%p " x, mDecoderID, ##__VA_ARGS__))
#define DECODER_LOG(x, ...) \
  LOG(gMediaDecoderLog, LogLevel::Debug, x, ##__VA_ARGS__)
#define VERBOSE_LOG(x, ...) \
  LOG(gMediaDecoderLog, LogLevel::Verbose, x, ##__VA_ARGS__)
#define SAMPLE_LOG(x, ...) \
  LOG(gMediaSampleLog, LogLevel::Debug, x, ##__VA_ARGS__)

// Somehow MSVC doesn't correctly delete the comma before ##__VA_ARGS__
// when __VA_ARGS__ expands to nothing. This is a workaround for it.
#define DECODER_WARN_HELPER(a, b) NS_WARNING b
#define DECODER_WARN(x, ...) \
  DECODER_WARN_HELPER(0, (nsPrintfCString("Decoder=%p " x, mDecoderID, ##__VA_ARGS__).get()))

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

// When the continuous silent data is over this threshold, means the a/v does
// not produce any sound. This time is decided by UX suggestion, see
// https://bugzilla.mozilla.org/show_bug.cgi?id=1235612#c18
static const uint32_t SILENT_DATA_THRESHOLD_USECS = 10000000;

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

// If we enter buffering within QUICK_BUFFER_THRESHOLD_USECS seconds of starting
// decoding, we'll enter "quick buffering" mode, which exits a lot sooner than
// normal buffering mode. This exists so that if the decode-ahead exhausts the
// downloaded data while decode/playback is just starting up (for example
// after a seek while the media is still playing, or when playing a media
// as soon as it's load started), we won't necessarily stop for 30s and wait
// for buffering. We may actually be able to playback in this case, so exit
// buffering early and try to play. If it turns out we can't play, we'll fall
// back to buffering normally.
static const uint32_t QUICK_BUFFER_THRESHOLD_USECS = 2000000;

namespace detail {

// If we're quick buffering, we'll remain in buffering mode while we have less than
// QUICK_BUFFERING_LOW_DATA_USECS of decoded data available.
static const uint32_t QUICK_BUFFERING_LOW_DATA_USECS = 1000000;

// If QUICK_BUFFERING_LOW_DATA_USECS is > AMPLE_AUDIO_USECS, we won't exit
// quick buffering in a timely fashion, as the decode pauses when it
// reaches AMPLE_AUDIO_USECS decoded data, and thus we'll never reach
// QUICK_BUFFERING_LOW_DATA_USECS.
static_assert(QUICK_BUFFERING_LOW_DATA_USECS <= AMPLE_AUDIO_USECS,
              "QUICK_BUFFERING_LOW_DATA_USECS is too large");

} // namespace detail

static TimeDuration UsecsToDuration(int64_t aUsecs) {
  return TimeDuration::FromMicroseconds(aUsecs);
}

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

MediaDecoderStateMachine::MediaDecoderStateMachine(MediaDecoder* aDecoder,
                                                   MediaDecoderReader* aReader,
                                                   bool aRealTime) :
  mDecoderID(aDecoder),
  mFrameStats(&aDecoder->GetFrameStatistics()),
  mVideoFrameContainer(aDecoder->GetVideoFrameContainer()),
  mAudioChannel(aDecoder->GetAudioChannel()),
  mTaskQueue(new TaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK),
                           /* aSupportsTailDispatch = */ true)),
  mWatchManager(this, mTaskQueue),
  mRealTime(aRealTime),
  mDispatchedStateMachine(false),
  mDelayedScheduler(mTaskQueue),
  mState(DECODER_STATE_DECODING_METADATA, "MediaDecoderStateMachine::mState"),
  mCurrentFrameID(0),
  mObservedDuration(TimeUnit(), "MediaDecoderStateMachine::mObservedDuration"),
  mFragmentEndTime(-1),
  mReader(aReader),
  mReaderWrapper(new MediaDecoderReaderWrapper(aRealTime, mTaskQueue, aReader)),
  mDecodedAudioEndTime(0),
  mDecodedVideoEndTime(0),
  mPlaybackRate(1.0),
  mLowAudioThresholdUsecs(detail::LOW_AUDIO_USECS),
  mAmpleAudioThresholdUsecs(detail::AMPLE_AUDIO_USECS),
  mQuickBufferingLowDataThresholdUsecs(detail::QUICK_BUFFERING_LOW_DATA_USECS),
  mIsAudioPrerolling(false),
  mIsVideoPrerolling(false),
  mAudioCaptured(false),
  mAudioCompleted(false, "MediaDecoderStateMachine::mAudioCompleted"),
  mVideoCompleted(false, "MediaDecoderStateMachine::mVideoCompleted"),
  mNotifyMetadataBeforeFirstFrame(false),
  mDispatchedEventToDecode(false),
  mQuickBuffering(false),
  mMinimizePreroll(false),
  mDecodeThreadWaiting(false),
  mDropAudioUntilNextDiscontinuity(false),
  mDropVideoUntilNextDiscontinuity(false),
  mCurrentTimeBeforeSeek(0),
  mDecodingFirstFrame(true),
  mSentLoadedMetadataEvent(false),
  mSentFirstFrameLoadedEvent(false),
  mSentPlaybackEndedEvent(false),
  mOutputStreamManager(new OutputStreamManager()),
  mResource(aDecoder->GetResource()),
  mAudioOffloading(false),
  mSilentDataDuration(0),
  mBuffered(mTaskQueue, TimeIntervals(),
            "MediaDecoderStateMachine::mBuffered (Mirror)"),
  mEstimatedDuration(mTaskQueue, NullableTimeUnit(),
                    "MediaDecoderStateMachine::mEstimatedDuration (Mirror)"),
  mExplicitDuration(mTaskQueue, Maybe<double>(),
                    "MediaDecoderStateMachine::mExplicitDuration (Mirror)"),
  mPlayState(mTaskQueue, MediaDecoder::PLAY_STATE_LOADING,
             "MediaDecoderStateMachine::mPlayState (Mirror)"),
  mNextPlayState(mTaskQueue, MediaDecoder::PLAY_STATE_PAUSED,
                 "MediaDecoderStateMachine::mNextPlayState (Mirror)"),
  mLogicallySeeking(mTaskQueue, false,
                    "MediaDecoderStateMachine::mLogicallySeeking (Mirror)"),
  mVolume(mTaskQueue, 1.0, "MediaDecoderStateMachine::mVolume (Mirror)"),
  mLogicalPlaybackRate(mTaskQueue, 1.0,
                       "MediaDecoderStateMachine::mLogicalPlaybackRate (Mirror)"),
  mPreservesPitch(mTaskQueue, true,
                  "MediaDecoderStateMachine::mPreservesPitch (Mirror)"),
  mSameOriginMedia(mTaskQueue, false,
                   "MediaDecoderStateMachine::mSameOriginMedia (Mirror)"),
  mPlaybackBytesPerSecond(mTaskQueue, 0.0,
                          "MediaDecoderStateMachine::mPlaybackBytesPerSecond (Mirror)"),
  mPlaybackRateReliable(mTaskQueue, true,
                        "MediaDecoderStateMachine::mPlaybackRateReliable (Mirror)"),
  mDecoderPosition(mTaskQueue, 0,
                   "MediaDecoderStateMachine::mDecoderPosition (Mirror)"),
  mMediaSeekable(mTaskQueue, true,
                 "MediaDecoderStateMachine::mMediaSeekable (Mirror)"),
  mMediaSeekableOnlyInBufferedRanges(mTaskQueue, false,
                 "MediaDecoderStateMachine::mMediaSeekableOnlyInBufferedRanges (Mirror)"),
  mDuration(mTaskQueue, NullableTimeUnit(),
            "MediaDecoderStateMachine::mDuration (Canonical"),
  mIsShutdown(mTaskQueue, false,
              "MediaDecoderStateMachine::mIsShutdown (Canonical)"),
  mNextFrameStatus(mTaskQueue, MediaDecoderOwner::NEXT_FRAME_UNINITIALIZED,
                   "MediaDecoderStateMachine::mNextFrameStatus (Canonical)"),
  mCurrentPosition(mTaskQueue, 0,
                   "MediaDecoderStateMachine::mCurrentPosition (Canonical)"),
  mPlaybackOffset(mTaskQueue, 0,
                  "MediaDecoderStateMachine::mPlaybackOffset (Canonical)"),
  mIsAudioDataAudible(mTaskQueue, false,
                     "MediaDecoderStateMachine::mIsAudioDataAudible (Canonical)")
{
  MOZ_COUNT_CTOR(MediaDecoderStateMachine);
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  // Dispatch initialization that needs to happen on that task queue.
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethodWithArg<RefPtr<MediaDecoder>>(
    this, &MediaDecoderStateMachine::InitializationTask, aDecoder);
  mTaskQueue->Dispatch(r.forget());

  InitVideoQueuePrefs();

  mBufferingWait = IsRealTime() ? 0 : 15;
  mLowDataThresholdUsecs = IsRealTime() ? 0 : detail::LOW_DATA_THRESHOLD_USECS;

#ifdef XP_WIN
  // Ensure high precision timers are enabled on Windows, otherwise the state
  // machine isn't woken up at reliable intervals to set the next frame,
  // and we drop frames while painting. Note that multiple calls to this
  // function per-process is OK, provided each call is matched by a corresponding
  // timeEndPeriod() call.
  timeBeginPeriod(1);
#endif

  mAudioQueueListener = AudioQueue().PopEvent().Connect(
    mTaskQueue, this, &MediaDecoderStateMachine::OnAudioPopped);
  mVideoQueueListener = VideoQueue().PopEvent().Connect(
    mTaskQueue, this, &MediaDecoderStateMachine::OnVideoPopped);

  mMetadataManager.Connect(mReader->TimedMetadataEvent(), OwnerThread());

  mMediaSink = CreateMediaSink(mAudioCaptured);

#ifdef MOZ_EME
  mCDMProxyPromise.Begin(aDecoder->RequestCDMProxy()->Then(
    OwnerThread(), __func__, this,
    &MediaDecoderStateMachine::OnCDMProxyReady,
    &MediaDecoderStateMachine::OnCDMProxyNotReady));
#endif
}

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
  mLogicallySeeking.Connect(aDecoder->CanonicalLogicallySeeking());
  mVolume.Connect(aDecoder->CanonicalVolume());
  mLogicalPlaybackRate.Connect(aDecoder->CanonicalPlaybackRate());
  mPreservesPitch.Connect(aDecoder->CanonicalPreservesPitch());
  mSameOriginMedia.Connect(aDecoder->CanonicalSameOriginMedia());
  mPlaybackBytesPerSecond.Connect(aDecoder->CanonicalPlaybackBytesPerSecond());
  mPlaybackRateReliable.Connect(aDecoder->CanonicalPlaybackRateReliable());
  mDecoderPosition.Connect(aDecoder->CanonicalDecoderPosition());
  mMediaSeekable.Connect(aDecoder->CanonicalMediaSeekable());
  mMediaSeekableOnlyInBufferedRanges.Connect(aDecoder->CanonicalMediaSeekableOnlyInBufferedRanges());

  // Initialize watchers.
  mWatchManager.Watch(mBuffered, &MediaDecoderStateMachine::BufferedRangeUpdated);
  mWatchManager.Watch(mState, &MediaDecoderStateMachine::UpdateNextFrameStatus);
  mWatchManager.Watch(mAudioCompleted, &MediaDecoderStateMachine::UpdateNextFrameStatus);
  mWatchManager.Watch(mVideoCompleted, &MediaDecoderStateMachine::UpdateNextFrameStatus);
  mWatchManager.Watch(mVolume, &MediaDecoderStateMachine::VolumeChanged);
  mWatchManager.Watch(mLogicalPlaybackRate, &MediaDecoderStateMachine::LogicalPlaybackRateChanged);
  mWatchManager.Watch(mPreservesPitch, &MediaDecoderStateMachine::PreservesPitchChanged);
  mWatchManager.Watch(mEstimatedDuration, &MediaDecoderStateMachine::RecomputeDuration);
  mWatchManager.Watch(mExplicitDuration, &MediaDecoderStateMachine::RecomputeDuration);
  mWatchManager.Watch(mObservedDuration, &MediaDecoderStateMachine::RecomputeDuration);
  mWatchManager.Watch(mPlayState, &MediaDecoderStateMachine::PlayStateChanged);
  mWatchManager.Watch(mLogicallySeeking, &MediaDecoderStateMachine::LogicallySeekingChanged);
}

media::MediaSink*
MediaDecoderStateMachine::CreateAudioSink()
{
  RefPtr<MediaDecoderStateMachine> self = this;
  auto audioSinkCreator = [self] () {
    MOZ_ASSERT(self->OnTaskQueue());
    return new DecodedAudioDataSink(
      self->mAudioQueue, self->GetMediaTime(),
      self->mInfo.mAudio, self->mAudioChannel);
  };
  return new AudioSinkWrapper(mTaskQueue, audioSinkCreator);
}

already_AddRefed<media::MediaSink>
MediaDecoderStateMachine::CreateMediaSink(bool aAudioCaptured)
{
  RefPtr<media::MediaSink> audioSink = aAudioCaptured
    ? new DecodedStream(mTaskQueue, mAudioQueue, mVideoQueue,
                        mOutputStreamManager, mSameOriginMedia.Ref())
    : CreateAudioSink();

  RefPtr<media::MediaSink> mediaSink =
    new VideoSink(mTaskQueue, audioSink, mVideoQueue,
                  mVideoFrameContainer, *mFrameStats,
                  sVideoQueueSendToCompositorSize);
  return mediaSink.forget();
}

bool MediaDecoderStateMachine::HasFutureAudio()
{
  MOZ_ASSERT(OnTaskQueue());
  NS_ASSERTION(HasAudio(), "Should only call HasFutureAudio() when we have audio");
  // We've got audio ready to play if:
  // 1. We've not completed playback of audio, and
  // 2. we either have more than the threshold of decoded audio available, or
  //    we've completely decoded all audio (but not finished playing it yet
  //    as per 1).
  return !mAudioCompleted &&
         (GetDecodedAudioDuration() >
            mLowAudioThresholdUsecs * mPlaybackRate ||
          AudioQueue().IsFinished());
}

bool MediaDecoderStateMachine::HaveNextFrameData()
{
  MOZ_ASSERT(OnTaskQueue());
  return (!HasAudio() || HasFutureAudio()) &&
         (!HasVideo() || VideoQueue().GetSize() > 1);
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

void MediaDecoderStateMachine::DiscardStreamData()
{
  MOZ_ASSERT(OnTaskQueue());

  const auto clockTime = GetClock();
  while (true) {
    const MediaData* a = AudioQueue().PeekFront();

    // If we discard audio samples fed to the stream immediately, we will
    // keep decoding audio samples till the end and consume a lot of memory.
    // Therefore we only discard those behind the stream clock to throttle
    // the decoding speed.
    // Note we don't discard a sample when |a->mTime == clockTime| because that
    // will discard the 1st sample when clockTime is still 0.
    if (a && a->mTime < clockTime) {
      RefPtr<MediaData> releaseMe = AudioQueue().PopFront();
      continue;
    }
    break;
  }
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
         ((IsDecodingFirstFrame() && VideoQueue().GetSize() == 0) ||
          (!mMinimizePreroll && !HaveEnoughDecodedVideo()));
}

bool
MediaDecoderStateMachine::NeedToSkipToNextKeyframe()
{
  MOZ_ASSERT(OnTaskQueue());
  if (IsDecodingFirstFrame()) {
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
                             !mIsAudioPrerolling && IsAudioDecoding() &&
                             (GetDecodedAudioDuration() <
                              mLowAudioThresholdUsecs * mPlaybackRate);
  bool isLowOnDecodedVideo = !mIsVideoPrerolling &&
                             ((GetClock() - mDecodedVideoEndTime) * mPlaybackRate >
                              LOW_VIDEO_THRESHOLD_USECS);
  bool lowUndecoded = HasLowUndecodedData();

  if ((isLowOnDecodedAudio || isLowOnDecodedVideo) && !lowUndecoded) {
    DECODER_LOG("Skipping video decode to the next keyframe lowAudio=%d lowVideo=%d lowUndecoded=%d async=%d",
                isLowOnDecodedAudio, isLowOnDecodedVideo, lowUndecoded, mReader->IsAsync());
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
         ((IsDecodingFirstFrame() && AudioQueue().GetSize() == 0) ||
          (!mMinimizePreroll && !HaveEnoughDecodedAudio()));
}

bool
MediaDecoderStateMachine::IsAudioSeekComplete()
{
  MOZ_ASSERT(OnTaskQueue());
  SAMPLE_LOG("IsAudioSeekComplete() curTarVal=%d mAudDis=%d aqFin=%d aqSz=%d",
    mCurrentSeek.Exists(), mDropAudioUntilNextDiscontinuity, AudioQueue().IsFinished(), AudioQueue().GetSize());
  return
    !HasAudio() ||
    (mCurrentSeek.Exists() &&
     !mDropAudioUntilNextDiscontinuity &&
     (AudioQueue().IsFinished() || AudioQueue().GetSize() > 0));
}

bool
MediaDecoderStateMachine::IsVideoSeekComplete()
{
  MOZ_ASSERT(OnTaskQueue());
  SAMPLE_LOG("IsVideoSeekComplete() curTarVal=%d mVidDis=%d vqFin=%d vqSz=%d",
    mCurrentSeek.Exists(), mDropVideoUntilNextDiscontinuity, VideoQueue().IsFinished(), VideoQueue().GetSize());
  return
    !HasVideo() ||
    (mCurrentSeek.Exists() &&
     !mDropVideoUntilNextDiscontinuity &&
     (VideoQueue().IsFinished() || VideoQueue().GetSize() > 0));
}

void
MediaDecoderStateMachine::OnAudioDecoded(MediaData* aAudioSample)
{
  MOZ_ASSERT(OnTaskQueue());
  RefPtr<MediaData> audio(aAudioSample);
  MOZ_ASSERT(audio);
  mAudioDataRequest.Complete();

  // audio->GetEndTime() is not always mono-increasing in chained ogg.
  mDecodedAudioEndTime = std::max(audio->GetEndTime(), mDecodedAudioEndTime);

  SAMPLE_LOG("OnAudioDecoded [%lld,%lld] disc=%d",
             (audio ? audio->mTime : -1),
             (audio ? audio->GetEndTime() : -1),
             (audio ? audio->mDiscontinuity : 0));

  switch (mState) {
    case DECODER_STATE_BUFFERING: {
      // If we're buffering, this may be the sample we need to stop buffering.
      // Save it and schedule the state machine.
      Push(audio, MediaData::AUDIO_DATA);
      ScheduleStateMachine();
      return;
    }

    case DECODER_STATE_DECODING: {
      Push(audio, MediaData::AUDIO_DATA);
      if (MaybeFinishDecodeFirstFrame()) {
        return;
      }
      if (mIsAudioPrerolling && DonePrerollingAudio()) {
        StopPrerollingAudio();
      }
      return;
    }

    case DECODER_STATE_SEEKING: {
      if (!mCurrentSeek.Exists()) {
        // We've received a sample from a previous decode. Discard it.
        return;
      }
      if (audio->mDiscontinuity) {
        mDropAudioUntilNextDiscontinuity = false;
      }
      if (!mDropAudioUntilNextDiscontinuity) {
        // We must be after the discontinuity; we're receiving samples
        // at or after the seek target.
        if (mCurrentSeek.mTarget.IsFast() &&
            mCurrentSeek.mTarget.GetTime().ToMicroseconds() > mCurrentTimeBeforeSeek &&
            audio->mTime < mCurrentTimeBeforeSeek) {
          // We are doing a fastSeek, but we ended up *before* the previous
          // playback position. This is surprising UX, so switch to an accurate
          // seek and decode to the seek target. This is not conformant to the
          // spec, fastSeek should always be fast, but until we get the time to
          // change all Readers to seek to the keyframe after the currentTime
          // in this case, we'll just decode forward. Bug 1026330.
          mCurrentSeek.mTarget.SetType(SeekTarget::Accurate);
        }
        if (mCurrentSeek.mTarget.IsFast()) {
          // Non-precise seek; we can stop the seek at the first sample.
          Push(audio, MediaData::AUDIO_DATA);
        } else {
          // We're doing an accurate seek. We must discard
          // MediaData up to the one containing exact seek target.
          if (NS_FAILED(DropAudioUpToSeekTarget(audio))) {
            DecodeError();
            return;
          }
        }
      }
      CheckIfSeekComplete();
      return;
    }
    default: {
      // Ignore other cases.
      return;
    }
  }
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
  UpdateNextFrameStatus();
  DispatchDecodeTasksIfNeeded();
}

void
MediaDecoderStateMachine::CheckIsAudible(const MediaData* aSample)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(aSample->mType == MediaData::AUDIO_DATA);

  const AudioData* data = aSample->As<AudioData>();
  bool isAudible = data->IsAudible();
  if (isAudible && !mIsAudioDataAudible) {
    mIsAudioDataAudible = true;
    mSilentDataDuration = 0;
  } else if (isAudible && mIsAudioDataAudible) {
    mSilentDataDuration += data->mDuration;
    if (mSilentDataDuration > SILENT_DATA_THRESHOLD_USECS) {
      mIsAudioDataAudible = false;
      mSilentDataDuration = 0;
    }
  }
}

void
MediaDecoderStateMachine::OnAudioPopped(const RefPtr<MediaData>& aSample)
{
  MOZ_ASSERT(OnTaskQueue());

  mPlaybackOffset = std::max(mPlaybackOffset.Ref(), aSample->mOffset);
  UpdateNextFrameStatus();
  DispatchAudioDecodeTaskIfNeeded();
  MaybeStartBuffering();
  CheckIsAudible(aSample);
}

void
MediaDecoderStateMachine::OnVideoPopped(const RefPtr<MediaData>& aSample)
{
  MOZ_ASSERT(OnTaskQueue());
  mPlaybackOffset = std::max(mPlaybackOffset.Ref(), aSample->mOffset);
  UpdateNextFrameStatus();
  DispatchVideoDecodeTaskIfNeeded();
  MaybeStartBuffering();
}

void
MediaDecoderStateMachine::OnNotDecoded(MediaData::Type aType,
                                       MediaDecoderReader::NotDecodedReason aReason)
{
  MOZ_ASSERT(OnTaskQueue());
  SAMPLE_LOG("OnNotDecoded (aType=%u, aReason=%u)", aType, aReason);
  bool isAudio = aType == MediaData::AUDIO_DATA;
  MOZ_ASSERT_IF(!isAudio, aType == MediaData::VIDEO_DATA);

  if (isAudio) {
    mAudioDataRequest.Complete();
  } else {
    mVideoDataRequest.Complete();
  }
  if (IsShutdown()) {
    // Already shutdown;
    return;
  }

  // If this is a decode error, delegate to the generic error path.
  if (aReason == MediaDecoderReader::DECODE_ERROR) {
    DecodeError();
    return;
  }

  // If the decoder is waiting for data, we tell it to call us back when the
  // data arrives.
  if (aReason == MediaDecoderReader::WAITING_FOR_DATA) {
    MOZ_ASSERT(mReader->IsWaitForDataSupported(),
               "Readers that send WAITING_FOR_DATA need to implement WaitForData");
    RefPtr<MediaDecoderStateMachine> self = this;
    WaitRequestRef(aType).Begin(InvokeAsync(DecodeTaskQueue(), mReader.get(), __func__,
                                            &MediaDecoderReader::WaitForData, aType)
      ->Then(OwnerThread(), __func__,
             [self] (MediaData::Type aType) -> void {
               self->WaitRequestRef(aType).Complete();
               self->DispatchDecodeTasksIfNeeded();
             },
             [self] (WaitForDataRejectValue aRejection) -> void {
               self->WaitRequestRef(aRejection.mType).Complete();
             }));

    // We are out of data to decode and will enter buffering mode soon.
    // We want to play the frames we have already decoded, so we stop pre-rolling
    // and ensure that loadeddata is fired as required.
    if (isAudio) {
      StopPrerollingAudio();
    } else {
      StopPrerollingVideo();
    }
    if (mState == DECODER_STATE_BUFFERING || mState == DECODER_STATE_DECODING) {
        MaybeFinishDecodeFirstFrame();
    }
    return;
  }

  if (aReason == MediaDecoderReader::CANCELED) {
    DispatchDecodeTasksIfNeeded();
    return;
  }

  // This is an EOS. Finish off the queue, and then handle things based on our
  // state.
  MOZ_ASSERT(aReason == MediaDecoderReader::END_OF_STREAM);
  if (!isAudio && mState == DECODER_STATE_SEEKING &&
      mCurrentSeek.Exists() && mFirstVideoFrameAfterSeek) {
    // Null sample. Hit end of stream. If we have decoded a frame,
    // insert it into the queue so that we have something to display.
    // We make sure to do this before invoking VideoQueue().Finish()
    // below.
    Push(mFirstVideoFrameAfterSeek, MediaData::VIDEO_DATA);
    mFirstVideoFrameAfterSeek = nullptr;
  }
  if (isAudio) {
    AudioQueue().Finish();
    StopPrerollingAudio();
  } else {
    VideoQueue().Finish();
    StopPrerollingVideo();
  }
  switch (mState) {
    case DECODER_STATE_BUFFERING:
    case DECODER_STATE_DECODING: {
      if (MaybeFinishDecodeFirstFrame()) {
        return;
      }
      CheckIfDecodeComplete();

      // Schedule next cycle to see if we can leave buffering state.
      if (mState == DECODER_STATE_BUFFERING) {
        ScheduleStateMachine();
      }
      return;
    }
    case DECODER_STATE_SEEKING: {
      if (!mCurrentSeek.Exists()) {
        // We've received a sample from a previous decode. Discard it.
        return;
      }

      if (isAudio) {
        mDropAudioUntilNextDiscontinuity = false;
      } else {
        mDropVideoUntilNextDiscontinuity = false;
      }

      CheckIfSeekComplete();
      return;
    }
    default: {
      return;
    }
  }
}

bool
MediaDecoderStateMachine::MaybeFinishDecodeFirstFrame()
{
  MOZ_ASSERT(OnTaskQueue());
  if (!IsDecodingFirstFrame() ||
      (IsAudioDecoding() && AudioQueue().GetSize() == 0) ||
      (IsVideoDecoding() && VideoQueue().GetSize() == 0)) {
    return false;
  }
  FinishDecodeFirstFrame();
  if (!mQueuedSeek.Exists()) {
    return false;
  }

  // We can now complete the pending seek.
  SetState(DECODER_STATE_SEEKING);
  InitiateSeek(Move(mQueuedSeek));
  return true;
}

void
MediaDecoderStateMachine::OnVideoDecoded(MediaData* aVideoSample,
                                         TimeStamp aDecodeStartTime)
{
  MOZ_ASSERT(OnTaskQueue());
  RefPtr<MediaData> video(aVideoSample);
  MOZ_ASSERT(video);
  mVideoDataRequest.Complete();

  // Handle abnormal or negative timestamps.
  mDecodedVideoEndTime = std::max(mDecodedVideoEndTime, video->GetEndTime());

  SAMPLE_LOG("OnVideoDecoded [%lld,%lld] disc=%d",
             (video ? video->mTime : -1),
             (video ? video->GetEndTime() : -1),
             (video ? video->mDiscontinuity : 0));

  switch (mState) {
    case DECODER_STATE_BUFFERING: {
      // If we're buffering, this may be the sample we need to stop buffering.
      // Save it and schedule the state machine.
      Push(video, MediaData::VIDEO_DATA);
      ScheduleStateMachine();
      return;
    }

    case DECODER_STATE_DECODING: {
      Push(video, MediaData::VIDEO_DATA);
      if (MaybeFinishDecodeFirstFrame()) {
        return;
      }
      if (mIsVideoPrerolling && DonePrerollingVideo()) {
        StopPrerollingVideo();
      }

      // For non async readers, if the requested video sample was slow to
      // arrive, increase the amount of audio we buffer to ensure that we
      // don't run out of audio. This is unnecessary for async readers,
      // since they decode audio and video on different threads so they
      // are unlikely to run out of decoded audio.
      if (mReader->IsAsync()) {
        return;
      }
      TimeDuration decodeTime = TimeStamp::Now() - aDecodeStartTime;
      if (!IsDecodingFirstFrame() &&
          THRESHOLD_FACTOR * DurationToUsecs(decodeTime) > mLowAudioThresholdUsecs &&
          !HasLowUndecodedData())
      {
        mLowAudioThresholdUsecs =
          std::min(THRESHOLD_FACTOR * DurationToUsecs(decodeTime), mAmpleAudioThresholdUsecs);
        mAmpleAudioThresholdUsecs = std::max(THRESHOLD_FACTOR * mLowAudioThresholdUsecs,
                                              mAmpleAudioThresholdUsecs);
        DECODER_LOG("Slow video decode, set mLowAudioThresholdUsecs=%lld mAmpleAudioThresholdUsecs=%lld",
                    mLowAudioThresholdUsecs, mAmpleAudioThresholdUsecs);
      }
      return;
    }
    case DECODER_STATE_SEEKING: {
      if (!mCurrentSeek.Exists()) {
        // We've received a sample from a previous decode. Discard it.
        return;
      }
      if (mDropVideoUntilNextDiscontinuity) {
        if (video->mDiscontinuity) {
          mDropVideoUntilNextDiscontinuity = false;
        }
      }
      if (!mDropVideoUntilNextDiscontinuity) {
        // We must be after the discontinuity; we're receiving samples
        // at or after the seek target.
        if (mCurrentSeek.mTarget.IsFast() &&
            mCurrentSeek.mTarget.GetTime().ToMicroseconds() > mCurrentTimeBeforeSeek &&
            video->mTime < mCurrentTimeBeforeSeek) {
          // We are doing a fastSeek, but we ended up *before* the previous
          // playback position. This is surprising UX, so switch to an accurate
          // seek and decode to the seek target. This is not conformant to the
          // spec, fastSeek should always be fast, but until we get the time to
          // change all Readers to seek to the keyframe after the currentTime
          // in this case, we'll just decode forward. Bug 1026330.
          mCurrentSeek.mTarget.SetType(SeekTarget::Accurate);
        }
        if (mCurrentSeek.mTarget.IsFast()) {
          // Non-precise seek. We can stop the seek at the first sample.
          Push(video, MediaData::VIDEO_DATA);
        } else {
          // We're doing an accurate seek. We still need to discard
          // MediaData up to the one containing exact seek target.
          if (NS_FAILED(DropVideoUpToSeekTarget(video))) {
            DecodeError();
            return;
          }
        }
      }
      CheckIfSeekComplete();
      return;
    }
    default: {
      // Ignore other cases.
      return;
    }
  }
}

void
MediaDecoderStateMachine::CheckIfSeekComplete()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mState == DECODER_STATE_SEEKING);

  const bool videoSeekComplete = IsVideoSeekComplete();
  if (HasVideo() && !videoSeekComplete) {
    // We haven't reached the target. Ensure we have requested another sample.
    if (NS_FAILED(EnsureVideoDecodeTaskQueued())) {
      DECODER_WARN("Failed to request video during seek");
      DecodeError();
    }
  }

  const bool audioSeekComplete = IsAudioSeekComplete();
  if (HasAudio() && !audioSeekComplete) {
    // We haven't reached the target. Ensure we have requested another sample.
    if (NS_FAILED(EnsureAudioDecodeTaskQueued())) {
      DECODER_WARN("Failed to request audio during seek");
      DecodeError();
    }
  }

  SAMPLE_LOG("CheckIfSeekComplete() audioSeekComplete=%d videoSeekComplete=%d",
             audioSeekComplete, videoSeekComplete);

  if (audioSeekComplete && videoSeekComplete) {
    NS_ASSERTION(AudioQueue().GetSize() <= 1, "Should decode at most one sample");
    NS_ASSERTION(VideoQueue().GetSize() <= 1, "Should decode at most one sample");
    SeekCompleted();
  }
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

void
MediaDecoderStateMachine::CheckIfDecodeComplete()
{
  MOZ_ASSERT(OnTaskQueue());

  if (IsShutdown() ||
      mState == DECODER_STATE_SEEKING ||
      mState == DECODER_STATE_COMPLETED) {
    // Don't change our state if we've already been shutdown, or we're seeking,
    // since we don't want to abort the shutdown or seek processes.
    return;
  }
  if (!IsVideoDecoding() && !IsAudioDecoding()) {
    // We've finished decoding all active streams,
    // so move to COMPLETED state.
    SetState(DECODER_STATE_COMPLETED);
    DispatchDecodeTasksIfNeeded();
    ScheduleStateMachine();
  }
  DECODER_LOG("CheckIfDecodeComplete %scompleted",
              ((mState == DECODER_STATE_COMPLETED) ? "" : "NOT "));
}

bool MediaDecoderStateMachine::IsPlaying() const
{
  MOZ_ASSERT(OnTaskQueue());
  return mMediaSink->IsPlaying();
}

nsresult MediaDecoderStateMachine::Init()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv = mReader->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(
    this, &MediaDecoderStateMachine::ReadMetadata);
  OwnerThread()->Dispatch(r.forget());

  return NS_OK;
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
  MOZ_ASSERT(mState == DECODER_STATE_DECODING ||
             mState == DECODER_STATE_COMPLETED);

  if (IsPlaying()) {
    // Logging this case is really spammy - don't do it.
    return;
  }

  bool playStatePermits = mPlayState == MediaDecoder::PLAY_STATE_PLAYING;
  if (!playStatePermits || mIsAudioPrerolling ||
      mIsVideoPrerolling || mAudioOffloading) {
    DECODER_LOG("Not starting playback [playStatePermits: %d, "
                "mIsAudioPrerolling: %d, mIsVideoPrerolling: %d, "
                "mAudioOffloading: %d]",
                (int)playStatePermits, (int)mIsAudioPrerolling,
                (int)mIsVideoPrerolling, (int)mAudioOffloading);
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

void
MediaDecoderStateMachine::MaybeStartBuffering()
{
  MOZ_ASSERT(OnTaskQueue());

  if (mState == DECODER_STATE_DECODING &&
      mPlayState == MediaDecoder::PLAY_STATE_PLAYING &&
      mResource->IsExpectingMoreData()) {
    bool shouldBuffer;
    if (mReader->UseBufferingHeuristics()) {
      shouldBuffer = HasLowDecodedData(EXHAUSTED_DATA_MARGIN_USECS) &&
                     (JustExitedQuickBuffering() || HasLowUndecodedData());
    } else {
      MOZ_ASSERT(mReader->IsWaitForDataSupported());
      shouldBuffer = (OutOfDecodedAudio() && mAudioWaitRequest.Exists()) ||
                     (OutOfDecodedVideo() && mVideoWaitRequest.Exists());
    }
    if (shouldBuffer) {
      StartBuffering();
      // Don't go straight back to the state machine loop since that might
      // cause us to start decoding again and we could flip-flop between
      // decoding and quick-buffering.
      ScheduleStateMachineIn(USECS_PER_S);
    }
  }
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

static const char* const gMachineStateStr[] = {
  "DECODING_METADATA",
  "WAIT_FOR_CDM",
  "DORMANT",
  "DECODING",
  "SEEKING",
  "BUFFERING",
  "COMPLETED",
  "SHUTDOWN",
  "ERROR"
};

void MediaDecoderStateMachine::SetState(State aState)
{
  MOZ_ASSERT(OnTaskQueue());
  if (mState == aState) {
    return;
  }
  DECODER_LOG("Change machine state from %s to %s",
              gMachineStateStr[mState], gMachineStateStr[aState]);

  mState = aState;

  mIsShutdown = mState == DECODER_STATE_ERROR || mState == DECODER_STATE_SHUTDOWN;

  // Clear state-scoped state.
  mSentPlaybackEndedEvent = false;
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
  } else if (mInfo.mMetadataDuration.isSome()) {
    duration = mInfo.mMetadataDuration.ref();
  } else {
    return;
  }

  if (duration < mObservedDuration.Ref()) {
    duration = mObservedDuration;
  }

  MOZ_ASSERT(duration.ToMicroseconds() >= 0);
  mDuration = Some(duration);
}

void
MediaDecoderStateMachine::DispatchSetDormant(bool aDormant)
{
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethodWithArg<bool>(
    this, &MediaDecoderStateMachine::SetDormant, aDormant);
  OwnerThread()->Dispatch(r.forget());
}

void
MediaDecoderStateMachine::SetDormant(bool aDormant)
{
  MOZ_ASSERT(OnTaskQueue());

  if (IsShutdown()) {
    return;
  }

  if (mMetadataRequest.Exists()) {
    if (mPendingDormant && mPendingDormant.ref() != aDormant && !aDormant) {
      // We already have a dormant request pending; the new request would have
      // resumed from dormant, we can just cancel any pending dormant requests.
      mPendingDormant.reset();
    } else {
      mPendingDormant = Some(aDormant);
    }
    return;
  }
  mPendingDormant.reset();

  DECODER_LOG("SetDormant=%d", aDormant);

  if (aDormant) {
    if (mState == DECODER_STATE_SEEKING) {
      if (mQueuedSeek.Exists()) {
        // Keep latest seek target
      } else if (mCurrentSeek.Exists()) {
        mQueuedSeek = Move(mCurrentSeek);
      } else {
        mQueuedSeek.mTarget = SeekTarget(mCurrentPosition,
                                         SeekTarget::Accurate,
                                         MediaDecoderEventVisibility::Suppressed);
        // XXXbholley - Nobody is listening to this promise. Do we need to pass it
        // back to MediaDecoder when we come out of dormant?
        RefPtr<MediaDecoder::SeekPromise> unused = mQueuedSeek.mPromise.Ensure(__func__);
      }
    } else {
      mQueuedSeek.mTarget = SeekTarget(mCurrentPosition,
                                       SeekTarget::Accurate,
                                       MediaDecoderEventVisibility::Suppressed);
      // XXXbholley - Nobody is listening to this promise. Do we need to pass it
      // back to MediaDecoder when we come out of dormant?
      RefPtr<MediaDecoder::SeekPromise> unused = mQueuedSeek.mPromise.Ensure(__func__);
    }
    mCurrentSeek.RejectIfExists(__func__);
    SetState(DECODER_STATE_DORMANT);
    if (IsPlaying()) {
      StopPlayback();
    }

    Reset();

    // Note that we do not wait for the decode task queue to go idle before
    // queuing the ReleaseMediaResources task - instead, we disconnect promises,
    // reset state, and put a ResetDecode in the decode task queue. Any tasks
    // that run after ResetDecode are supposed to run with a clean slate. We rely
    // on that in other places (i.e. seeking), so it seems reasonable to rely on
    // it here as well.
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(mReader, &MediaDecoderReader::ReleaseMediaResources);
    DecodeTaskQueue()->Dispatch(r.forget());
  } else if ((aDormant != true) && (mState == DECODER_STATE_DORMANT)) {
    mDecodingFirstFrame = true;
    SetState(DECODER_STATE_DECODING_METADATA);
    ReadMetadata();
  }
}

RefPtr<ShutdownPromise>
MediaDecoderStateMachine::Shutdown()
{
  MOZ_ASSERT(OnTaskQueue());

  // Once we've entered the shutdown state here there's no going back.
  // Change state before issuing shutdown request to threads so those
  // threads can start exiting cleanly during the Shutdown call.
  ScheduleStateMachine();
  SetState(DECODER_STATE_SHUTDOWN);

  mBufferedUpdateRequest.DisconnectIfExists();

  mQueuedSeek.RejectIfExists(__func__);
  mCurrentSeek.RejectIfExists(__func__);

#ifdef MOZ_EME
  mCDMProxyPromise.DisconnectIfExists();
#endif

  if (IsPlaying()) {
    StopPlayback();
  }

  Reset();

  mMediaSink->Shutdown();
  mReaderWrapper->Shutdown();

  DECODER_LOG("Shutdown started");

  // Put a task in the decode queue to shutdown the reader.
  // the queue to spin down.
  return InvokeAsync(DecodeTaskQueue(), mReader.get(), __func__,
                     &MediaDecoderReader::Shutdown)
    ->Then(OwnerThread(), __func__, this,
           &MediaDecoderStateMachine::FinishShutdown,
           &MediaDecoderStateMachine::FinishShutdown)
    ->CompletionPromise();
}

void MediaDecoderStateMachine::StartDecoding()
{
  MOZ_ASSERT(OnTaskQueue());
  if (mState == DECODER_STATE_DECODING && !mDecodingFirstFrame) {
    return;
  }
  SetState(DECODER_STATE_DECODING);

  if (mDecodingFirstFrame &&
      (IsRealTime() || mSentFirstFrameLoadedEvent)) {
    if (IsRealTime()) {
      FinishDecodeFirstFrame();
    } else {
      // We're resuming from dormant state, so we don't need to request
      // the first samples in order to determine the media start time,
      // we have the start time from last time we loaded.
      // FinishDecodeFirstFrame will be launched upon completion of the seek when
      // we have data ready to play.
      MOZ_ASSERT(mQueuedSeek.Exists() && mSentFirstFrameLoadedEvent,
                 "Return from dormant must have queued seek");
    }
    if (mQueuedSeek.Exists()) {
      SetState(DECODER_STATE_SEEKING);
      InitiateSeek(Move(mQueuedSeek));
      return;
    }
  }

  mDecodeStartTime = TimeStamp::Now();

  CheckIfDecodeComplete();
  if (mState == DECODER_STATE_COMPLETED) {
    return;
  }

  // Reset other state to pristine values before starting decode.
  mIsAudioPrerolling = !DonePrerollingAudio() && !mAudioWaitRequest.Exists();
  mIsVideoPrerolling = !DonePrerollingVideo() && !mVideoWaitRequest.Exists();

  // Ensure that we've got tasks enqueued to decode data if we need to.
  DispatchDecodeTasksIfNeeded();

  ScheduleStateMachine();
}

void MediaDecoderStateMachine::PlayStateChanged()
{
  MOZ_ASSERT(OnTaskQueue());

  // This method used to be a Play() method invoked by MediaDecoder when the
  // play state became PLAY_STATE_PLAYING. As such, it doesn't have any work to
  // do for other state changes. That could change.
  if (mPlayState != MediaDecoder::PLAY_STATE_PLAYING) {
    return;
  }

  // Once we start playing, we don't want to minimize our prerolling, as we
  // assume the user is likely to want to keep playing in future. This needs to
  // happen before we invoke StartDecoding().
  if (mMinimizePreroll) {
    mMinimizePreroll = false;
    DispatchDecodeTasksIfNeeded();
  }

  // Some state transitions still happen synchronously on the main thread. So
  // if the main thread invokes Play() and then Seek(), the seek will initiate
  // synchronously on the main thread, and the asynchronous PlayInternal task
  // will arrive when it's no longer valid. The proper thing to do is to move
  // all state transitions to the state machine task queue, but for now we just
  // make sure that none of the possible main-thread state transitions (Seek(),
  // SetDormant(), and Shutdown()) have not occurred.
  if (mState != DECODER_STATE_DECODING && mState != DECODER_STATE_BUFFERING &&
      mState != DECODER_STATE_COMPLETED)
  {
    DECODER_LOG("Unexpected state - Bailing out of PlayInternal()");
    return;
  }

  // When asked to play, switch to decoding state only if
  // we are currently buffering. In other cases, we'll start playing anyway
  // when the state machine notices the decoder's state change to PLAYING.
  if (mState == DECODER_STATE_BUFFERING) {
    StartDecoding();
  }

  ScheduleStateMachine();
}

void MediaDecoderStateMachine::LogicallySeekingChanged()
{
  MOZ_ASSERT(OnTaskQueue());
  ScheduleStateMachine();
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

void
MediaDecoderStateMachine::ReadMetadata()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(!IsShutdown());
  MOZ_ASSERT(mState == DECODER_STATE_DECODING_METADATA);
  MOZ_ASSERT(!mMetadataRequest.Exists());

  DECODER_LOG("Dispatching AsyncReadMetadata");
  // Set mode to METADATA since we are about to read metadata.
  mResource->SetReadMode(MediaCacheStream::MODE_METADATA);
  mMetadataRequest.Begin(mReaderWrapper->ReadMetadata()
    ->Then(OwnerThread(), __func__, this,
           &MediaDecoderStateMachine::OnMetadataRead,
           &MediaDecoderStateMachine::OnMetadataNotRead));
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

  MOZ_ASSERT(mState > DECODER_STATE_DECODING_METADATA,
               "We should have got duration already");

  if (mState < DECODER_STATE_DECODING ||
      (IsDecodingFirstFrame() && !mReader->ForceZeroStartTime())) {
    DECODER_LOG("Seek() Not Enough Data to continue at this stage, queuing seek");
    mQueuedSeek.RejectIfExists(__func__);
    mQueuedSeek.mTarget = aTarget;
    return mQueuedSeek.mPromise.Ensure(__func__);
  }
  mQueuedSeek.RejectIfExists(__func__);

  DECODER_LOG("Changed state to SEEKING (to %lld)", aTarget.GetTime().ToMicroseconds());
  SetState(DECODER_STATE_SEEKING);

  SeekJob seekJob;
  seekJob.mTarget = aTarget;
  InitiateSeek(Move(seekJob));
  return mCurrentSeek.mPromise.Ensure(__func__);
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
    nsCOMPtr<nsIRunnable> task = NS_NewRunnableMethod(mReader, &MediaDecoderReader::SetIdle);
    DecodeTaskQueue()->Dispatch(task.forget());
  }
}

void
MediaDecoderStateMachine::InitiateSeek(SeekJob aSeekJob)
{
  MOZ_ASSERT(OnTaskQueue());

  mCurrentSeek.RejectIfExists(__func__);
  mCurrentSeek = Move(aSeekJob);

  // Bound the seek time to be inside the media range.
  int64_t end = Duration().ToMicroseconds();
  NS_ASSERTION(end != -1, "Should know end time by now");
  int64_t seekTime = mCurrentSeek.mTarget.GetTime().ToMicroseconds();
  seekTime = std::min(seekTime, end);
  seekTime = std::max(int64_t(0), seekTime);
  NS_ASSERTION(seekTime >= 0 && seekTime <= end,
               "Can only seek in range [0,duration]");
  mCurrentSeek.mTarget.SetTime(media::TimeUnit::FromMicroseconds(seekTime));

  mDropAudioUntilNextDiscontinuity = HasAudio();
  mDropVideoUntilNextDiscontinuity = HasVideo();
  mCurrentTimeBeforeSeek = GetMediaTime();

  // Stop playback now to ensure that while we're outside the monitor
  // dispatching SeekingStarted, playback doesn't advance and mess with
  // mCurrentPosition that we've setting to seekTime here.
  StopPlayback();
  UpdatePlaybackPositionInternal(mCurrentSeek.mTarget.GetTime().ToMicroseconds());

  mOnSeekingStart.Notify(mCurrentSeek.mTarget.mEventVisibility);

  // Reset our state machine and decoding pipeline before seeking.
  Reset();

  // Do the seek.
  RefPtr<MediaDecoderStateMachine> self = this;
  mSeekRequest.Begin(
    mReaderWrapper->Seek(mCurrentSeek.mTarget, Duration())
    ->Then(OwnerThread(), __func__,
           [self] (media::TimeUnit) -> void {
             self->mSeekRequest.Complete();
             // We must decode the first samples of active streams, so we can determine
             // the new stream time. So dispatch tasks to do that.
             self->EnsureAudioDecodeTaskQueued();
             self->EnsureVideoDecodeTaskQueued();
           }, [self] (nsresult aResult) -> void {
             self->mSeekRequest.Complete();
             MOZ_ASSERT(NS_FAILED(aResult), "Cancels should also disconnect mSeekRequest");
             self->DecodeError();
           }));
}

nsresult
MediaDecoderStateMachine::DispatchAudioDecodeTaskIfNeeded()
{
  MOZ_ASSERT(OnTaskQueue());

  if (IsShutdown()) {
    return NS_ERROR_FAILURE;
  }

  if (NeedToDecodeAudio()) {
    return EnsureAudioDecodeTaskQueued();
  }

  return NS_OK;
}

nsresult
MediaDecoderStateMachine::EnsureAudioDecodeTaskQueued()
{
  MOZ_ASSERT(OnTaskQueue());

  SAMPLE_LOG("EnsureAudioDecodeTaskQueued isDecoding=%d status=%s",
              IsAudioDecoding(), AudioRequestStatus());

  if (mState != DECODER_STATE_DECODING &&
      mState != DECODER_STATE_BUFFERING &&
      mState != DECODER_STATE_SEEKING) {
    return NS_OK;
  }

  if (!IsAudioDecoding() || mAudioDataRequest.Exists() ||
      mAudioWaitRequest.Exists() || mSeekRequest.Exists()) {
    return NS_OK;
  }

  RequestAudioData();
  return NS_OK;
}

void
MediaDecoderStateMachine::RequestAudioData()
{
  MOZ_ASSERT(OnTaskQueue());

  SAMPLE_LOG("Queueing audio task - queued=%i, decoder-queued=%o",
             AudioQueue().GetSize(), mReader->SizeOfAudioQueueInFrames());

  mAudioDataRequest.Begin(
    mReaderWrapper->RequestAudioData()
    ->Then(OwnerThread(), __func__, this,
           &MediaDecoderStateMachine::OnAudioDecoded,
           &MediaDecoderStateMachine::OnAudioNotDecoded));
}

nsresult
MediaDecoderStateMachine::DispatchVideoDecodeTaskIfNeeded()
{
  MOZ_ASSERT(OnTaskQueue());

  if (IsShutdown()) {
    return NS_ERROR_FAILURE;
  }

  if (NeedToDecodeVideo()) {
    return EnsureVideoDecodeTaskQueued();
  }

  return NS_OK;
}

nsresult
MediaDecoderStateMachine::EnsureVideoDecodeTaskQueued()
{
  MOZ_ASSERT(OnTaskQueue());

  SAMPLE_LOG("EnsureVideoDecodeTaskQueued isDecoding=%d status=%s",
             IsVideoDecoding(), VideoRequestStatus());

  if (mState != DECODER_STATE_DECODING &&
      mState != DECODER_STATE_BUFFERING &&
      mState != DECODER_STATE_SEEKING) {
    return NS_OK;
  }

  if (!IsVideoDecoding() || mVideoDataRequest.Exists() ||
      mVideoWaitRequest.Exists() || mSeekRequest.Exists()) {
    return NS_OK;
  }

  RequestVideoData();
  return NS_OK;
}

void
MediaDecoderStateMachine::RequestVideoData()
{
  MOZ_ASSERT(OnTaskQueue());

  // Time the video decode, so that if it's slow, we can increase our low
  // audio threshold to reduce the chance of an audio underrun while we're
  // waiting for a video decode to complete.
  TimeStamp videoDecodeStartTime = TimeStamp::Now();

  bool skipToNextKeyFrame = mSentFirstFrameLoadedEvent &&
    NeedToSkipToNextKeyframe();

  media::TimeUnit currentTime = media::TimeUnit::FromMicroseconds(
    mState == DECODER_STATE_SEEKING ? 0 : GetMediaTime());

  SAMPLE_LOG("Queueing video task - queued=%i, decoder-queued=%o, skip=%i, time=%lld",
             VideoQueue().GetSize(), mReader->SizeOfVideoQueueInFrames(), skipToNextKeyFrame,
             currentTime.ToMicroseconds());

  RefPtr<MediaDecoderStateMachine> self = this;
  mVideoDataRequest.Begin(
    mReaderWrapper->RequestVideoData(skipToNextKeyFrame, currentTime)
    ->Then(OwnerThread(), __func__,
           [self, videoDecodeStartTime] (MediaData* aVideoSample) {
             self->OnVideoDecoded(aVideoSample, videoDecodeStartTime);
           },
           [self] (MediaDecoderReader::NotDecodedReason aReason) {
             self->OnVideoNotDecoded(aReason);
           }));
}

void
MediaDecoderStateMachine::StartMediaSink()
{
  MOZ_ASSERT(OnTaskQueue());
  if (!mMediaSink->IsStarted()) {
    mAudioCompleted = false;
    mMediaSink->Start(GetMediaTime(), mInfo);

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

bool MediaDecoderStateMachine::HasLowDecodedData(int64_t aAudioUsecs)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mReader->UseBufferingHeuristics());
  // We consider ourselves low on decoded data if we're low on audio,
  // provided we've not decoded to the end of the audio stream, or
  // if we're low on video frames, provided
  // we've not decoded to the end of the video stream.
  return ((IsAudioDecoding() && GetDecodedAudioDuration() < aAudioUsecs) ||
         (IsVideoDecoding() &&
          static_cast<uint32_t>(VideoQueue().GetSize()) < LOW_VIDEO_FRAMES));
}

bool MediaDecoderStateMachine::OutOfDecodedAudio()
{
    MOZ_ASSERT(OnTaskQueue());
    return IsAudioDecoding() && !AudioQueue().IsFinished() &&
           AudioQueue().GetSize() == 0 &&
           !mMediaSink->HasUnplayedFrames(TrackInfo::kAudioTrack);
}

bool MediaDecoderStateMachine::HasLowUndecodedData()
{
  MOZ_ASSERT(OnTaskQueue());
  return HasLowUndecodedData(mLowDataThresholdUsecs);
}

bool MediaDecoderStateMachine::HasLowUndecodedData(int64_t aUsecs)
{
  MOZ_ASSERT(OnTaskQueue());
  NS_ASSERTION(mState >= DECODER_STATE_DECODING && !IsDecodingFirstFrame(),
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
  media::TimeInterval interval(media::TimeUnit::FromMicroseconds(endOfDecodedData),
                               media::TimeUnit::FromMicroseconds(std::min(endOfDecodedData + aUsecs, Duration().ToMicroseconds())));
  return endOfDecodedData != INT64_MAX && !mBuffered.Ref().Contains(interval);
}

void
MediaDecoderStateMachine::DecodeError()
{
  MOZ_ASSERT(OnTaskQueue());

  if (IsShutdown()) {
    // Already shutdown.
    return;
  }

  // Change state to error, which will cause the state machine to wait until
  // the MediaDecoder shuts it down.
  SetState(DECODER_STATE_ERROR);
  ScheduleStateMachine();
  DECODER_WARN("Decode error, changed state to ERROR");

  // MediaDecoder::DecodeError notifies the owner, and then shuts down the state
  // machine.
  mOnPlaybackEvent.Notify(MediaEventType::DecodeError);
}

void
MediaDecoderStateMachine::OnMetadataRead(MetadataHolder* aMetadata)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mState == DECODER_STATE_DECODING_METADATA);
  mMetadataRequest.Complete();

  if (mPendingDormant) {
    SetDormant(mPendingDormant.ref());
    return;
  }

  // Set mode to PLAYBACK after reading metadata.
  mResource->SetReadMode(MediaCacheStream::MODE_PLAYBACK);
  mInfo = aMetadata->mInfo;
  mMetadataTags = aMetadata->mTags.forget();
  RefPtr<MediaDecoderStateMachine> self = this;

  if (mInfo.mMetadataDuration.isSome()) {
    RecomputeDuration();
  } else if (mInfo.mUnadjustedMetadataEndTime.isSome()) {
    mReaderWrapper->AwaitStartTime()->Then(OwnerThread(), __func__,
      [self] () -> void {
        NS_ENSURE_TRUE_VOID(!self->IsShutdown());
        TimeUnit unadjusted = self->mInfo.mUnadjustedMetadataEndTime.ref();
        TimeUnit adjustment = self->mReaderWrapper->StartTime();
        self->mInfo.mMetadataDuration.emplace(unadjusted - adjustment);
        self->RecomputeDuration();
      }, [] () -> void { NS_WARNING("Adjusting metadata end time failed"); }
    );
  }

  if (HasVideo()) {
    DECODER_LOG("Video decode isAsync=%d HWAccel=%d videoQueueSize=%d",
                mReader->IsAsync(),
                mReader->VideoIsHardwareAccelerated(),
                GetAmpleVideoFrames());
  }

  // In general, we wait until we know the duration before notifying the decoder.
  // However, we notify  unconditionally in this case without waiting for the start
  // time, since the caller might be waiting on metadataloaded to be fired before
  // feeding in the CDM, which we need to decode the first frame (and
  // thus get the metadata). We could fix this if we could compute the start
  // time by demuxing without necessaring decoding.
  bool waitingForCDM =
#ifdef MOZ_EME
    mInfo.IsEncrypted() && !mCDMProxy;
#else
    false;
#endif
  mNotifyMetadataBeforeFirstFrame = mDuration.Ref().isSome() || waitingForCDM;
  if (mNotifyMetadataBeforeFirstFrame) {
    EnqueueLoadedMetadataEvent();
  }

  if (waitingForCDM) {
    // Metadata parsing was successful but we're still waiting for CDM caps
    // to become available so that we can build the correct decryptor/decoder.
    SetState(DECODER_STATE_WAIT_FOR_CDM);
    return;
  }

  StartDecoding();

  ScheduleStateMachine();
}

void
MediaDecoderStateMachine::OnMetadataNotRead(ReadMetadataFailureReason aReason)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mState == DECODER_STATE_DECODING_METADATA);
  mMetadataRequest.Complete();
  DECODER_WARN("Decode metadata failed, shutting down decoder");
  DecodeError();
}

void
MediaDecoderStateMachine::EnqueueLoadedMetadataEvent()
{
  MOZ_ASSERT(OnTaskQueue());
  MediaDecoderEventVisibility visibility =
    mSentLoadedMetadataEvent ? MediaDecoderEventVisibility::Suppressed
                             : MediaDecoderEventVisibility::Observable;
  mMetadataLoadedEvent.Notify(nsAutoPtr<MediaInfo>(new MediaInfo(mInfo)),
                              Move(mMetadataTags),
                              Move(visibility));
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
  mBufferedUpdateRequest.Begin(InvokeAsync(DecodeTaskQueue(), mReader.get(), __func__,
    &MediaDecoderReader::UpdateBufferedWithPromise)
    ->Then(OwnerThread(),
    __func__,
    // Resolve
    [self, firstFrameBeenLoaded]() {
      self->mBufferedUpdateRequest.Complete();
      MediaDecoderEventVisibility visibility =
        firstFrameBeenLoaded ? MediaDecoderEventVisibility::Suppressed
                             : MediaDecoderEventVisibility::Observable;
      self->mFirstFrameLoadedEvent.Notify(nsAutoPtr<MediaInfo>(new MediaInfo(self->mInfo)),
                                          Move(visibility));
    },
    // Reject
    []() { MOZ_CRASH("Should not reach"); }));
}

bool
MediaDecoderStateMachine::IsDecodingFirstFrame()
{
  return mState == DECODER_STATE_DECODING && mDecodingFirstFrame;
}

void
MediaDecoderStateMachine::FinishDecodeFirstFrame()
{
  MOZ_ASSERT(OnTaskQueue());
  DECODER_LOG("FinishDecodeFirstFrame");

  if (!IsRealTime() && !mSentFirstFrameLoadedEvent) {
    mMediaSink->Redraw();
  }

  // If we don't know the duration by this point, we assume infinity, per spec.
  if (mDuration.Ref().isNothing()) {
    mDuration = Some(TimeUnit::FromInfinity());
  }

  DECODER_LOG("Media duration %lld, "
              "transportSeekable=%d, mediaSeekable=%d",
              Duration().ToMicroseconds(), mResource->IsTransportSeekable(), mMediaSeekable.Ref());

  // Get potentially updated metadata
  mReader->ReadUpdatedMetadata(&mInfo);

  if (!mNotifyMetadataBeforeFirstFrame) {
    // If we didn't have duration and/or start time before, we should now.
    EnqueueLoadedMetadataEvent();
  }
  EnqueueFirstFrameLoadedEvent();

  mDecodingFirstFrame = false;
}

void
MediaDecoderStateMachine::SeekCompleted()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mState == DECODER_STATE_SEEKING);

  int64_t seekTime = mCurrentSeek.mTarget.GetTime().ToMicroseconds();
  int64_t newCurrentTime = seekTime;

  // Setup timestamp state.
  RefPtr<MediaData> video = VideoQueue().PeekFront();
  if (seekTime == Duration().ToMicroseconds()) {
    newCurrentTime = seekTime;
  } else if (HasAudio()) {
    MediaData* audio = AudioQueue().PeekFront();
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

  // Change state to DECODING or COMPLETED now.
  bool isLiveStream = mResource->IsLiveStream();
  State nextState;
  if (GetMediaTime() == Duration().ToMicroseconds() && !isLiveStream) {
    // Seeked to end of media, move to COMPLETED state. Note we don't do
    // this when playing a live stream, since the end of media will advance
    // once we download more data!
    DECODER_LOG("Changed state from SEEKING (to %lld) to COMPLETED", seekTime);
    // Explicitly set our state so we don't decode further, and so
    // we report playback ended to the media element.
    nextState = DECODER_STATE_COMPLETED;
  } else {
    DECODER_LOG("Changed state from SEEKING (to %lld) to DECODING", seekTime);
    nextState = DECODER_STATE_DECODING;
  }

  // We want to resolve the seek request prior finishing the first frame
  // to ensure that the seeked event is fired prior loadeded.
  mCurrentSeek.Resolve(nextState == DECODER_STATE_COMPLETED, __func__);

  if (mDecodingFirstFrame) {
    // We were resuming from dormant, or initiated a seek early.
    // We can fire loadeddata now.
    FinishDecodeFirstFrame();
  }

  if (nextState == DECODER_STATE_DECODING) {
    StartDecoding();
  } else {
    SetState(nextState);
  }

  // Ensure timestamps are up to date.
  UpdatePlaybackPositionInternal(newCurrentTime);

  // Try to decode another frame to detect if we're at the end...
  DECODER_LOG("Seek completed, mCurrentPosition=%lld", mCurrentPosition.Ref());

  // Reset quick buffering status. This ensures that if we began the
  // seek while quick-buffering, we won't bypass quick buffering mode
  // if we need to buffer after the seek.
  mQuickBuffering = false;

  ScheduleStateMachine();

  if (video) {
    mMediaSink->Redraw();
    mOnPlaybackEvent.Notify(MediaEventType::Invalidate);
  }
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

  // The reader's listeners hold references to the state machine,
  // creating a cycle which keeps the state machine and its shared
  // thread pools alive. So break it here.

  // Prevent dangling pointers by disconnecting the listeners.
  mAudioQueueListener.Disconnect();
  mVideoQueueListener.Disconnect();
  mMetadataManager.Disconnect();

  // Disconnect canonicals and mirrors before shutting down our task queue.
  mBuffered.DisconnectIfConnected();
  mEstimatedDuration.DisconnectIfConnected();
  mExplicitDuration.DisconnectIfConnected();
  mPlayState.DisconnectIfConnected();
  mNextPlayState.DisconnectIfConnected();
  mLogicallySeeking.DisconnectIfConnected();
  mVolume.DisconnectIfConnected();
  mLogicalPlaybackRate.DisconnectIfConnected();
  mPreservesPitch.DisconnectIfConnected();
  mSameOriginMedia.DisconnectIfConnected();
  mPlaybackBytesPerSecond.DisconnectIfConnected();
  mPlaybackRateReliable.DisconnectIfConnected();
  mDecoderPosition.DisconnectIfConnected();
  mMediaSeekable.DisconnectIfConnected();
  mMediaSeekableOnlyInBufferedRanges.DisconnectIfConnected();

  mDuration.DisconnectAll();
  mIsShutdown.DisconnectAll();
  mNextFrameStatus.DisconnectAll();
  mCurrentPosition.DisconnectAll();
  mPlaybackOffset.DisconnectAll();
  mIsAudioDataAudible.DisconnectAll();

  // Shut down the watch manager before shutting down our task queue.
  mWatchManager.Shutdown();

  MOZ_ASSERT(mState == DECODER_STATE_SHUTDOWN,
             "How did we escape from the shutdown state?");
  DECODER_LOG("Shutting down state machine task queue");
  return OwnerThread()->BeginShutdown();
}

nsresult MediaDecoderStateMachine::RunStateMachine()
{
  MOZ_ASSERT(OnTaskQueue());

  mDelayedScheduler.Reset(); // Must happen on state machine task queue.
  mDispatchedStateMachine = false;

  MediaResource* resource = mResource;
  NS_ENSURE_TRUE(resource, NS_ERROR_NULL_POINTER);

  switch (mState) {
    case DECODER_STATE_ERROR:
    case DECODER_STATE_SHUTDOWN:
    case DECODER_STATE_DORMANT:
    case DECODER_STATE_WAIT_FOR_CDM:
    case DECODER_STATE_DECODING_METADATA:
      return NS_OK;

    case DECODER_STATE_DECODING: {
      if (IsDecodingFirstFrame()) {
        // We haven't completed decoding our first frames, we can't start
        // playback yet.
        return NS_OK;
      }
      if (mPlayState != MediaDecoder::PLAY_STATE_PLAYING && IsPlaying())
      {
        // We're playing, but the element/decoder is in paused state. Stop
        // playing!
        StopPlayback();
      }

      // Start playback if necessary so that the clock can be properly queried.
      MaybeStartPlayback();

      UpdatePlaybackPositionPeriodically();
      NS_ASSERTION(!IsPlaying() ||
                   mLogicallySeeking ||
                   IsStateMachineScheduled(),
                   "Must have timer scheduled");
      return NS_OK;
    }

    case DECODER_STATE_BUFFERING: {
      TimeStamp now = TimeStamp::Now();
      NS_ASSERTION(!mBufferingStart.IsNull(), "Must know buffering start time.");

      // With buffering heuristics we will remain in the buffering state if
      // we've not decoded enough data to begin playback, or if we've not
      // downloaded a reasonable amount of data inside our buffering time.
      if (mReader->UseBufferingHeuristics()) {
        TimeDuration elapsed = now - mBufferingStart;
        bool isLiveStream = resource->IsLiveStream();
        if ((isLiveStream || !CanPlayThrough()) &&
              elapsed < TimeDuration::FromSeconds(mBufferingWait * mPlaybackRate) &&
              (mQuickBuffering ? HasLowDecodedData(mQuickBufferingLowDataThresholdUsecs)
                               : HasLowUndecodedData(mBufferingWait * USECS_PER_S)) &&
              mResource->IsExpectingMoreData())
        {
          DECODER_LOG("Buffering: wait %ds, timeout in %.3lfs %s",
                      mBufferingWait, mBufferingWait - elapsed.ToSeconds(),
                      (mQuickBuffering ? "(quick exit)" : ""));
          ScheduleStateMachineIn(USECS_PER_S);
          return NS_OK;
        }
      } else if (OutOfDecodedAudio() || OutOfDecodedVideo()) {
        MOZ_ASSERT(mReader->IsWaitForDataSupported(),
                   "Don't yet have a strategy for non-heuristic + non-WaitForData");
        DispatchDecodeTasksIfNeeded();
        MOZ_ASSERT_IF(!mMinimizePreroll && OutOfDecodedAudio(), mAudioDataRequest.Exists() || mAudioWaitRequest.Exists());
        MOZ_ASSERT_IF(!mMinimizePreroll && OutOfDecodedVideo(), mVideoDataRequest.Exists() || mVideoWaitRequest.Exists());
        DECODER_LOG("In buffering mode, waiting to be notified: outOfAudio: %d, "
                    "mAudioStatus: %s, outOfVideo: %d, mVideoStatus: %s",
                    OutOfDecodedAudio(), AudioRequestStatus(),
                    OutOfDecodedVideo(), VideoRequestStatus());
        return NS_OK;
      }

      DECODER_LOG("Changed state from BUFFERING to DECODING");
      DECODER_LOG("Buffered for %.3lfs", (now - mBufferingStart).ToSeconds());
      StartDecoding();

      NS_ASSERTION(IsStateMachineScheduled(), "Must have timer scheduled");
      return NS_OK;
    }

    case DECODER_STATE_SEEKING: {
      return NS_OK;
    }

    case DECODER_STATE_COMPLETED: {
      if (mPlayState != MediaDecoder::PLAY_STATE_PLAYING && IsPlaying()) {
        StopPlayback();
      }
      // Play the remaining media. We want to run AdvanceFrame() at least
      // once to ensure the current playback position is advanced to the
      // end of the media, and so that we update the readyState.
      if ((HasVideo() && !mVideoCompleted) ||
          (HasAudio() && !mAudioCompleted)) {
        // Start playback if necessary to play the remaining media.
        MaybeStartPlayback();
        UpdatePlaybackPositionPeriodically();
        NS_ASSERTION(!IsPlaying() ||
                     mLogicallySeeking ||
                     IsStateMachineScheduled(),
                     "Must have timer scheduled");
        return NS_OK;
      }

      // StopPlayback in order to reset the IsPlaying() state so audio
      // is restarted correctly.
      StopPlayback();

      if (mPlayState == MediaDecoder::PLAY_STATE_PLAYING &&
          !mSentPlaybackEndedEvent)
      {
        int64_t clockTime = std::max(AudioEndTime(), VideoEndTime());
        clockTime = std::max(int64_t(0), std::max(clockTime, Duration().ToMicroseconds()));
        UpdatePlaybackPosition(clockTime);

        // Ensure readyState is updated before firing the 'ended' event.
        UpdateNextFrameStatus();

        mOnPlaybackEvent.Notify(MediaEventType::PlaybackEnded);

        mSentPlaybackEndedEvent = true;

        // MediaSink::GetEndTime() must be called before stopping playback.
        StopMediaSink();
      }

      return NS_OK;
    }
  }

  return NS_OK;
}

void
MediaDecoderStateMachine::Reset()
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

  // Stop the audio thread. Otherwise, MediaSink might be accessing AudioQueue
  // outside of the decoder monitor while we are clearing the queue and causes
  // crash for no samples to be popped.
  StopMediaSink();

  mDecodedVideoEndTime = 0;
  mDecodedAudioEndTime = 0;
  mAudioCompleted = false;
  mVideoCompleted = false;
  AudioQueue().Reset();
  VideoQueue().Reset();
  mFirstVideoFrameAfterSeek = nullptr;
  mDropAudioUntilNextDiscontinuity = true;
  mDropVideoUntilNextDiscontinuity = true;

  mMetadataRequest.DisconnectIfExists();
  mAudioDataRequest.DisconnectIfExists();
  mAudioWaitRequest.DisconnectIfExists();
  mVideoDataRequest.DisconnectIfExists();
  mVideoWaitRequest.DisconnectIfExists();
  mSeekRequest.DisconnectIfExists();

  mPlaybackOffset = 0;

  nsCOMPtr<nsIRunnable> resetTask =
    NS_NewRunnableMethod(mReader, &MediaDecoderReader::ResetDecode);
  DecodeTaskQueue()->Dispatch(resetTask.forget());
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

  if (!IsPlaying() || mLogicallySeeking) {
    return;
  }

  if (mAudioCaptured) {
    DiscardStreamData();
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

nsresult
MediaDecoderStateMachine::DropVideoUpToSeekTarget(MediaData* aSample)
{
  MOZ_ASSERT(OnTaskQueue());
  RefPtr<VideoData> video(aSample->As<VideoData>());
  MOZ_ASSERT(video);
  DECODER_LOG("DropVideoUpToSeekTarget() frame [%lld, %lld]",
              video->mTime, video->GetEndTime());
  MOZ_ASSERT(mCurrentSeek.Exists());
  const int64_t target = mCurrentSeek.mTarget.GetTime().ToMicroseconds();

  // If the frame end time is less than the seek target, we won't want
  // to display this frame after the seek, so discard it.
  if (target >= video->GetEndTime()) {
    DECODER_LOG("DropVideoUpToSeekTarget() pop video frame [%lld, %lld] target=%lld",
                video->mTime, video->GetEndTime(), target);
    mFirstVideoFrameAfterSeek = video;
  } else {
    if (target >= video->mTime && video->GetEndTime() >= target) {
      // The seek target lies inside this frame's time slice. Adjust the frame's
      // start time to match the seek target. We do this by replacing the
      // first frame with a shallow copy which has the new timestamp.
      RefPtr<VideoData> temp = VideoData::ShallowCopyUpdateTimestamp(video, target);
      video = temp;
    }
    mFirstVideoFrameAfterSeek = nullptr;

    DECODER_LOG("DropVideoUpToSeekTarget() found video frame [%lld, %lld] containing target=%lld",
                video->mTime, video->GetEndTime(), target);

    MOZ_ASSERT(VideoQueue().GetSize() == 0, "Should be the 1st sample after seeking");
    Push(video, MediaData::VIDEO_DATA);
  }

  return NS_OK;
}

nsresult
MediaDecoderStateMachine::DropAudioUpToSeekTarget(MediaData* aSample)
{
  MOZ_ASSERT(OnTaskQueue());
  RefPtr<AudioData> audio(aSample->As<AudioData>());
  MOZ_ASSERT(audio &&
             mCurrentSeek.Exists() &&
             mCurrentSeek.mTarget.IsAccurate());

  CheckedInt64 sampleDuration =
    FramesToUsecs(audio->mFrames, mInfo.mAudio.mRate);
  if (!sampleDuration.isValid()) {
    return NS_ERROR_FAILURE;
  }

  if (audio->mTime + sampleDuration.value() <= mCurrentSeek.mTarget.GetTime().ToMicroseconds()) {
    // Our seek target lies after the frames in this AudioData. Don't
    // push it onto the audio queue, and keep decoding forwards.
    return NS_OK;
  }

  if (audio->mTime > mCurrentSeek.mTarget.GetTime().ToMicroseconds()) {
    // The seek target doesn't lie in the audio block just after the last
    // audio frames we've seen which were before the seek target. This
    // could have been the first audio data we've seen after seek, i.e. the
    // seek terminated after the seek target in the audio stream. Just
    // abort the audio decode-to-target, the state machine will play
    // silence to cover the gap. Typically this happens in poorly muxed
    // files.
    DECODER_WARN("Audio not synced after seek, maybe a poorly muxed file?");
    Push(audio, MediaData::AUDIO_DATA);
    return NS_OK;
  }

  // The seek target lies somewhere in this AudioData's frames, strip off
  // any frames which lie before the seek target, so we'll begin playback
  // exactly at the seek target.
  NS_ASSERTION(mCurrentSeek.mTarget.GetTime().ToMicroseconds() >= audio->mTime,
               "Target must at or be after data start.");
  NS_ASSERTION(mCurrentSeek.mTarget.GetTime().ToMicroseconds() < audio->mTime + sampleDuration.value(),
               "Data must end after target.");

  CheckedInt64 framesToPrune =
    UsecsToFrames(mCurrentSeek.mTarget.GetTime().ToMicroseconds() - audio->mTime, mInfo.mAudio.mRate);
  if (!framesToPrune.isValid()) {
    return NS_ERROR_FAILURE;
  }
  if (framesToPrune.value() > audio->mFrames) {
    // We've messed up somehow. Don't try to trim frames, the |frames|
    // variable below will overflow.
    DECODER_WARN("Can't prune more frames that we have!");
    return NS_ERROR_FAILURE;
  }
  uint32_t frames = audio->mFrames - static_cast<uint32_t>(framesToPrune.value());
  uint32_t channels = audio->mChannels;
  auto audioData = MakeUnique<AudioDataValue[]>(frames * channels);
  memcpy(audioData.get(),
         audio->mAudioData.get() + (framesToPrune.value() * channels),
         frames * channels * sizeof(AudioDataValue));
  CheckedInt64 duration = FramesToUsecs(frames, mInfo.mAudio.mRate);
  if (!duration.isValid()) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<AudioData> data(new AudioData(audio->mOffset,
                                       mCurrentSeek.mTarget.GetTime().ToMicroseconds(),
                                       duration.value(),
                                       frames,
                                       Move(audioData),
                                       channels,
                                       audio->mRate));
  MOZ_ASSERT(AudioQueue().GetSize() == 0, "Should be the 1st sample after seeking");
  Push(data, MediaData::AUDIO_DATA);

  return NS_OK;
}

void MediaDecoderStateMachine::UpdateNextFrameStatus()
{
  MOZ_ASSERT(OnTaskQueue());

  MediaDecoderOwner::NextFrameStatus status;
  const char* statusString;
  if (mState <= DECODER_STATE_WAIT_FOR_CDM || IsDecodingFirstFrame()) {
    status = MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE;
    statusString = "NEXT_FRAME_UNAVAILABLE";
  } else if (IsBuffering()) {
    status = MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_BUFFERING;
    statusString = "NEXT_FRAME_UNAVAILABLE_BUFFERING";
  } else if (IsSeeking()) {
    status = MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_SEEKING;
    statusString = "NEXT_FRAME_UNAVAILABLE_SEEKING";
  } else if (HaveNextFrameData()) {
    status = MediaDecoderOwner::NEXT_FRAME_AVAILABLE;
    statusString = "NEXT_FRAME_AVAILABLE";
  } else {
    status = MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE;
    statusString = "NEXT_FRAME_UNAVAILABLE";
  }

  if (status != mNextFrameStatus) {
    DECODER_LOG("Changed mNextFrameStatus to %s", statusString);
  }

  mNextFrameStatus = status;
}

bool MediaDecoderStateMachine::JustExitedQuickBuffering()
{
  MOZ_ASSERT(OnTaskQueue());
  return !mDecodeStartTime.IsNull() &&
    mQuickBuffering &&
    (TimeStamp::Now() - mDecodeStartTime) < TimeDuration::FromMicroseconds(QUICK_BUFFER_THRESHOLD_USECS);
}

bool
MediaDecoderStateMachine::CanPlayThrough()
{
  MOZ_ASSERT(OnTaskQueue());
  return IsRealTime() || GetStatistics().CanPlayThrough();
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

void MediaDecoderStateMachine::StartBuffering()
{
  MOZ_ASSERT(OnTaskQueue());

  if (mState != DECODER_STATE_DECODING) {
    // We only move into BUFFERING state if we're actually decoding.
    // If we're currently doing something else, we don't need to buffer,
    // and more importantly, we shouldn't overwrite mState to interrupt
    // the current operation, as that could leave us in an inconsistent
    // state!
    return;
  }

  // Update playback position again before entering BUFFERING so the currentTime
  // of the media element is more accurate during buffering.
  UpdatePlaybackPositionPeriodically();

  if (IsPlaying()) {
    StopPlayback();
  }

  TimeDuration decodeDuration = TimeStamp::Now() - mDecodeStartTime;
  // Go into quick buffering mode provided we've not just left buffering using
  // a "quick exit". This stops us flip-flopping between playing and buffering
  // when the download speed is similar to the decode speed.
  mQuickBuffering =
    !JustExitedQuickBuffering() &&
    decodeDuration < UsecsToDuration(QUICK_BUFFER_THRESHOLD_USECS);
  mBufferingStart = TimeStamp::Now();

  SetState(DECODER_STATE_BUFFERING);
  DECODER_LOG("Changed state from DECODING to BUFFERING, decoded for %.3lfs",
              decodeDuration.ToSeconds());
  MediaStatistics stats = GetStatistics();
  DECODER_LOG("Playback rate: %.1lfKB/s%s download rate: %.1lfKB/s%s",
              stats.mPlaybackRate/1024, stats.mPlaybackRateReliable ? "" : " (unreliable)",
              stats.mDownloadRate/1024, stats.mDownloadRateReliable ? "" : " (unreliable)");
}

void
MediaDecoderStateMachine::ScheduleStateMachine()
{
  MOZ_ASSERT(OnTaskQueue());
  if (mDispatchedStateMachine) {
    return;
  }
  mDispatchedStateMachine = true;

  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableMethod(this, &MediaDecoderStateMachine::RunStateMachine);
  OwnerThread()->Dispatch(task.forget());
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

  // Real-time weirdness.
  if (IsRealTime()) {
    aMicroseconds = std::min(aMicroseconds, int64_t(40000));
  }

  TimeStamp now = TimeStamp::Now();
  TimeStamp target = now + TimeDuration::FromMicroseconds(aMicroseconds);

  SAMPLE_LOG("Scheduling state machine for %lf ms from now", (target - now).ToMilliseconds());

  RefPtr<MediaDecoderStateMachine> self = this;
  mDelayedScheduler.Ensure(target, [self] () {
    self->OnDelayedSchedule();
  }, [self] () {
    self->NotReached();
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
MediaDecoderStateMachine::LogicalPlaybackRateChanged()
{
  MOZ_ASSERT(OnTaskQueue());

  if (mLogicalPlaybackRate == 0) {
    // This case is handled in MediaDecoder by pausing playback.
    return;
  }

  mPlaybackRate = mLogicalPlaybackRate;
  mMediaSink->SetPlaybackRate(mPlaybackRate);

  if (mIsAudioPrerolling && DonePrerollingAudio()) {
    StopPrerollingAudio();
  }
  if (mIsVideoPrerolling && DonePrerollingVideo()) {
    StopPrerollingVideo();
  }

  ScheduleStateMachine();
}

void MediaDecoderStateMachine::PreservesPitchChanged()
{
  MOZ_ASSERT(OnTaskQueue());
  mMediaSink->SetPreservesPitch(mPreservesPitch);
}

bool MediaDecoderStateMachine::IsShutdown()
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
  MOZ_ASSERT(!HasAudio());
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
  MOZ_ASSERT(mInfo.HasVideo());
  VERBOSE_LOG("[%s]", __func__);

  mMediaSinkVideoPromise.Complete();
  mVideoCompleted = true;
  ScheduleStateMachine();
}

void
MediaDecoderStateMachine::OnMediaSinkVideoError()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mInfo.HasVideo());
  VERBOSE_LOG("[%s]", __func__);

  mMediaSinkVideoPromise.Complete();
  mVideoCompleted = true;
  if (HasAudio()) {
    return;
  }
  DecodeError();
}

void MediaDecoderStateMachine::OnMediaSinkAudioComplete()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mInfo.HasAudio());
  VERBOSE_LOG("[%s]", __func__);

  mMediaSinkAudioPromise.Complete();
  mAudioCompleted = true;
  // To notify PlaybackEnded as soon as possible.
  ScheduleStateMachine();
}

void MediaDecoderStateMachine::OnMediaSinkAudioError()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mInfo.HasAudio());
  VERBOSE_LOG("[%s]", __func__);

  mMediaSinkAudioPromise.Complete();
  mAudioCompleted = true;

  // Make the best effort to continue playback when there is video.
  if (HasVideo()) {
    return;
  }

  // Otherwise notify media decoder/element about this error for it makes
  // no sense to play an audio-only file without sound output.
  DecodeError();
}

#ifdef MOZ_EME
void
MediaDecoderStateMachine::OnCDMProxyReady(RefPtr<CDMProxy> aProxy)
{
  MOZ_ASSERT(OnTaskQueue());
  mCDMProxyPromise.Complete();
  mCDMProxy = aProxy;
  mReader->SetCDMProxy(aProxy);
  if (mState == DECODER_STATE_WAIT_FOR_CDM) {
    StartDecoding();
  }
}

void
MediaDecoderStateMachine::OnCDMProxyNotReady()
{
  MOZ_ASSERT(OnTaskQueue());
  mCDMProxyPromise.Complete();
}
#endif

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

  // We don't need to call StartMediaSink() here because IsPlaying() is now
  // always in sync with the playing state of MediaSink. It will be started in
  // MaybeStartPlayback() in the next cycle if necessary.

  mAudioCaptured = aCaptured;
  ScheduleStateMachine();

  // Don't buffer as much when audio is captured because we don't need to worry
  // about high latency audio devices.
  mAmpleAudioThresholdUsecs = mAudioCaptured ?
                              detail::AMPLE_AUDIO_USECS / 2 :
                              detail::AMPLE_AUDIO_USECS;
  if (mIsAudioPrerolling && DonePrerollingAudio()) {
    StopPrerollingAudio();
  }
}

uint32_t MediaDecoderStateMachine::GetAmpleVideoFrames() const
{
  MOZ_ASSERT(OnTaskQueue());
  return (mReader->IsAsync() && mReader->VideoIsHardwareAccelerated())
    ? std::max<uint32_t>(sVideoQueueHWAccelSize, MIN_VIDEO_QUEUE_SIZE)
    : std::max<uint32_t>(sVideoQueueDefaultSize, MIN_VIDEO_QUEUE_SIZE);
}

void MediaDecoderStateMachine::AddOutputStream(ProcessedMediaStream* aStream,
                                               bool aFinishWhenEnded)
{
  MOZ_ASSERT(NS_IsMainThread());
  DECODER_LOG("AddOutputStream aStream=%p!", aStream);
  mOutputStreamManager->Add(aStream, aFinishWhenEnded);
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethodWithArg<bool>(
    this, &MediaDecoderStateMachine::SetAudioCaptured, true);
  OwnerThread()->Dispatch(r.forget());
}

void MediaDecoderStateMachine::RemoveOutputStream(MediaStream* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  DECODER_LOG("RemoveOutputStream=%p!", aStream);
  mOutputStreamManager->Remove(aStream);
  if (mOutputStreamManager->IsEmpty()) {
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethodWithArg<bool>(
      this, &MediaDecoderStateMachine::SetAudioCaptured, false);
    OwnerThread()->Dispatch(r.forget());
  }
}

} // namespace mozilla

// avoid redefined macro in unified build
#undef LOG
#undef DECODER_LOG
#undef VERBOSE_LOG
#undef DECODER_WARN
#undef DECODER_WARN_HELPER

#undef NS_DispatchToMainThread
