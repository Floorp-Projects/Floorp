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

#include "mediasink/DecodedAudioDataSink.h"
#include "mediasink/AudioSinkWrapper.h"
#include "mediasink/DecodedStream.h"
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
#include "MediaDecoderStateMachine.h"
#include "MediaShutdownManager.h"
#include "MediaTimer.h"
#include "TimeUnits.h"
#include "VideoSegment.h"
#include "VideoUtils.h"

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
  MOZ_LOG(m, l, ("Decoder=%p " x, mDecoder.get(), ##__VA_ARGS__))
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
  DECODER_WARN_HELPER(0, (nsPrintfCString("Decoder=%p " x, mDecoder.get(), ##__VA_ARGS__).get()))

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
const int64_t AMPLE_AUDIO_USECS = 1000000;

} // namespace detail

// When we're only playing audio and we don't have a video stream, we divide
// AMPLE_AUDIO_USECS and LOW_AUDIO_USECS by the following value. This reduces
// the amount of decoded audio we buffer, reducing our memory usage. We only
// need to decode far ahead when we're decoding video using software decoding,
// as otherwise a long video decode could cause an audio underrun.
const int64_t NO_VIDEO_AMPLE_AUDIO_DIVISOR = 8;

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
static const uint32_t VIDEO_QUEUE_SEND_TO_COMPOSITOR_SIZE = 9999;

static uint32_t sVideoQueueDefaultSize = MAX_VIDEO_QUEUE_SIZE;
static uint32_t sVideoQueueHWAccelSize = MIN_VIDEO_QUEUE_SIZE;
static uint32_t sVideoQueueSendToCompositorSize = VIDEO_QUEUE_SEND_TO_COMPOSITOR_SIZE;

MediaDecoderStateMachine::MediaDecoderStateMachine(MediaDecoder* aDecoder,
                                                   MediaDecoderReader* aReader,
                                                   bool aRealTime) :
  mDecoder(aDecoder),
  mTaskQueue(new TaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK),
                           /* aSupportsTailDispatch = */ true)),
  mWatchManager(this, mTaskQueue),
  mProducerID(ImageContainer::AllocateProducerID()),
  mRealTime(aRealTime),
  mDispatchedStateMachine(false),
  mDelayedScheduler(this),
  mState(DECODER_STATE_DECODING_NONE, "MediaDecoderStateMachine::mState"),
  mCurrentFrameID(0),
  mObservedDuration(TimeUnit(), "MediaDecoderStateMachine::mObservedDuration"),
  mFragmentEndTime(-1),
  mReader(aReader),
  mDecodedAudioEndTime(-1),
  mVideoFrameEndTime(-1),
  mDecodedVideoEndTime(-1),
  mPlaybackRate(1.0),
  mLowAudioThresholdUsecs(detail::LOW_AUDIO_USECS),
  mAmpleAudioThresholdUsecs(detail::AMPLE_AUDIO_USECS),
  mQuickBufferingLowDataThresholdUsecs(detail::QUICK_BUFFERING_LOW_DATA_USECS),
  mIsAudioPrerolling(false),
  mIsVideoPrerolling(false),
  mAudioCaptured(false, "MediaDecoderStateMachine::mAudioCaptured"),
  mAudioCompleted(false, "MediaDecoderStateMachine::mAudioCompleted"),
  mNotifyMetadataBeforeFirstFrame(false),
  mDispatchedEventToDecode(false),
  mQuickBuffering(false),
  mMinimizePreroll(false),
  mDecodeThreadWaiting(false),
  mDropAudioUntilNextDiscontinuity(false),
  mDropVideoUntilNextDiscontinuity(false),
  mDecodeToSeekTarget(false),
  mCurrentTimeBeforeSeek(0),
  mCorruptFrames(60),
  mDecodingFirstFrame(true),
  mSentLoadedMetadataEvent(false),
  mSentFirstFrameLoadedEvent(false, "MediaDecoderStateMachine::mSentFirstFrameLoadedEvent"),
  mSentPlaybackEndedEvent(false),
  mStreamSink(new DecodedStream(mTaskQueue, mAudioQueue, mVideoQueue)),
  mResource(aDecoder->GetResource()),
  mAudioOffloading(false),
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
  mDuration(mTaskQueue, NullableTimeUnit(),
            "MediaDecoderStateMachine::mDuration (Canonical"),
  mIsShutdown(mTaskQueue, false,
              "MediaDecoderStateMachine::mIsShutdown (Canonical)"),
  mNextFrameStatus(mTaskQueue, MediaDecoderOwner::NEXT_FRAME_UNINITIALIZED,
                   "MediaDecoderStateMachine::mNextFrameStatus (Canonical)"),
  mCurrentPosition(mTaskQueue, 0,
                   "MediaDecoderStateMachine::mCurrentPosition (Canonical)"),
  mPlaybackOffset(mTaskQueue, 0,
                  "MediaDecoderStateMachine::mPlaybackOffset (Canonical)")
{
  MOZ_COUNT_CTOR(MediaDecoderStateMachine);
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  // Dispatch initialization that needs to happen on that task queue.
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethod(this, &MediaDecoderStateMachine::InitializationTask);
  mTaskQueue->Dispatch(r.forget());

  static bool sPrefCacheInit = false;
  if (!sPrefCacheInit) {
    sPrefCacheInit = true;
    Preferences::AddUintVarCache(&sVideoQueueDefaultSize,
                                 "media.video-queue.default-size",
                                 MAX_VIDEO_QUEUE_SIZE);
    Preferences::AddUintVarCache(&sVideoQueueHWAccelSize,
                                 "media.video-queue.hw-accel-size",
                                 MIN_VIDEO_QUEUE_SIZE);
    Preferences::AddUintVarCache(&sVideoQueueSendToCompositorSize,
                                 "media.video-queue.send-to-compositor-size",
                                 VIDEO_QUEUE_SEND_TO_COMPOSITOR_SIZE);
  }

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

  mMediaSink = CreateAudioSink();

#ifdef MOZ_EME
  mCDMProxyPromise.Begin(mDecoder->RequestCDMProxy()->Then(
    OwnerThread(), __func__, this,
    &MediaDecoderStateMachine::OnCDMProxyReady,
    &MediaDecoderStateMachine::OnCDMProxyNotReady));
#endif
}

MediaDecoderStateMachine::~MediaDecoderStateMachine()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  MOZ_COUNT_DTOR(MediaDecoderStateMachine);

  mReader = nullptr;

#ifdef XP_WIN
  timeEndPeriod(1);
#endif
}

void
MediaDecoderStateMachine::InitializationTask()
{
  MOZ_ASSERT(OnTaskQueue());

  // Connect mirrors.
  mBuffered.Connect(mReader->CanonicalBuffered());
  mEstimatedDuration.Connect(mDecoder->CanonicalEstimatedDuration());
  mExplicitDuration.Connect(mDecoder->CanonicalExplicitDuration());
  mPlayState.Connect(mDecoder->CanonicalPlayState());
  mNextPlayState.Connect(mDecoder->CanonicalNextPlayState());
  mLogicallySeeking.Connect(mDecoder->CanonicalLogicallySeeking());
  mVolume.Connect(mDecoder->CanonicalVolume());
  mLogicalPlaybackRate.Connect(mDecoder->CanonicalPlaybackRate());
  mPreservesPitch.Connect(mDecoder->CanonicalPreservesPitch());
  mSameOriginMedia.Connect(mDecoder->CanonicalSameOriginMedia());
  mPlaybackBytesPerSecond.Connect(mDecoder->CanonicalPlaybackBytesPerSecond());
  mPlaybackRateReliable.Connect(mDecoder->CanonicalPlaybackRateReliable());
  mDecoderPosition.Connect(mDecoder->CanonicalDecoderPosition());
  mMediaSeekable.Connect(mDecoder->CanonicalMediaSeekable());

  // Initialize watchers.
  mWatchManager.Watch(mBuffered, &MediaDecoderStateMachine::BufferedRangeUpdated);
  mWatchManager.Watch(mState, &MediaDecoderStateMachine::UpdateNextFrameStatus);
  mWatchManager.Watch(mAudioCompleted, &MediaDecoderStateMachine::UpdateNextFrameStatus);
  mWatchManager.Watch(mVolume, &MediaDecoderStateMachine::VolumeChanged);
  mWatchManager.Watch(mLogicalPlaybackRate, &MediaDecoderStateMachine::LogicalPlaybackRateChanged);
  mWatchManager.Watch(mPreservesPitch, &MediaDecoderStateMachine::PreservesPitchChanged);
  mWatchManager.Watch(mEstimatedDuration, &MediaDecoderStateMachine::RecomputeDuration);
  mWatchManager.Watch(mExplicitDuration, &MediaDecoderStateMachine::RecomputeDuration);
  mWatchManager.Watch(mObservedDuration, &MediaDecoderStateMachine::RecomputeDuration);
  mWatchManager.Watch(mPlayState, &MediaDecoderStateMachine::PlayStateChanged);
  mWatchManager.Watch(mLogicallySeeking, &MediaDecoderStateMachine::LogicallySeekingChanged);
  mWatchManager.Watch(mSameOriginMedia, &MediaDecoderStateMachine::SameOriginMediaChanged);
  mWatchManager.Watch(mSentFirstFrameLoadedEvent, &MediaDecoderStateMachine::AdjustAudioThresholds);
  mWatchManager.Watch(mAudioCaptured, &MediaDecoderStateMachine::AdjustAudioThresholds);

  // Propagate mSameOriginMedia to mDecodedStream.
  SameOriginMediaChanged();
}

media::MediaSink*
MediaDecoderStateMachine::CreateAudioSink()
{
  nsRefPtr<MediaDecoderStateMachine> self = this;
  auto audioSinkCreator = [self] () {
    MOZ_ASSERT(self->OnTaskQueue());
    return new DecodedAudioDataSink(
      self->mAudioQueue, self->GetMediaTime(),
      self->mInfo.mAudio, self->mDecoder->GetAudioChannel());
  };
  return new AudioSinkWrapper(mTaskQueue, audioSinkCreator);
}

bool MediaDecoderStateMachine::HasFutureAudio()
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();
  NS_ASSERTION(HasAudio(), "Should only call HasFutureAudio() when we have audio");
  // We've got audio ready to play if:
  // 1. We've not completed playback of audio, and
  // 2. we either have more than the threshold of decoded audio available, or
  //    we've completely decoded all audio (but not finished playing it yet
  //    as per 1).
  return !mAudioCompleted &&
         (AudioDecodedUsecs() >
            mLowAudioThresholdUsecs * mPlaybackRate ||
          AudioQueue().IsFinished());
}

bool MediaDecoderStateMachine::HaveNextFrameData()
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();
  return (!HasAudio() || HasFutureAudio()) &&
         (!HasVideo() || VideoQueue().GetSize() > 1);
}

int64_t MediaDecoderStateMachine::GetDecodedAudioDuration()
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();
  int64_t audioDecoded = AudioQueue().Duration();
  if (mMediaSink->IsStarted()) {
    audioDecoded += AudioEndTime() - GetMediaTime();
  }
  return audioDecoded;
}

void MediaDecoderStateMachine::DiscardStreamData()
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();

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
      nsRefPtr<MediaData> releaseMe = AudioQueue().PopFront();
      continue;
    }
    break;
  }
}

bool MediaDecoderStateMachine::HaveEnoughDecodedAudio(int64_t aAmpleAudioUSecs)
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();

  if (AudioQueue().GetSize() == 0 ||
      GetDecodedAudioDuration() < aAmpleAudioUSecs) {
    return false;
  }

  // MDSM will ensure buffering level is high enough for playback speed at 1x
  // at which the DecodedStream is playing.
  return true;
}

bool MediaDecoderStateMachine::HaveEnoughDecodedVideo()
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();

  if (VideoQueue().GetSize() - 1 < GetAmpleVideoFrames() * mPlaybackRate) {
    return false;
  }

  return true;
}

bool
MediaDecoderStateMachine::NeedToDecodeVideo()
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();
  SAMPLE_LOG("NeedToDecodeVideo() isDec=%d decToTar=%d minPrl=%d seek=%d enufVid=%d",
             IsVideoDecoding(), mDecodeToSeekTarget, mMinimizePreroll,
             mState == DECODER_STATE_SEEKING,
             HaveEnoughDecodedVideo());
  return IsVideoDecoding() &&
         ((mState == DECODER_STATE_SEEKING && mDecodeToSeekTarget) ||
          (IsDecodingFirstFrame() &&
           IsVideoDecoding() && VideoQueue().GetSize() == 0) ||
          (!mMinimizePreroll && !HaveEnoughDecodedVideo()));
}

bool
MediaDecoderStateMachine::NeedToSkipToNextKeyframe()
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();
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
  AssertCurrentThreadInMonitor();
  SAMPLE_LOG("NeedToDecodeAudio() isDec=%d decToTar=%d minPrl=%d seek=%d enufAud=%d",
             IsAudioDecoding(), mDecodeToSeekTarget, mMinimizePreroll,
             mState == DECODER_STATE_SEEKING,
             HaveEnoughDecodedAudio(mAmpleAudioThresholdUsecs * mPlaybackRate));

  return IsAudioDecoding() &&
         ((mState == DECODER_STATE_SEEKING && mDecodeToSeekTarget) ||
          (IsDecodingFirstFrame() &&
           IsAudioDecoding() && AudioQueue().GetSize() == 0) ||
          (!mMinimizePreroll &&
          !HaveEnoughDecodedAudio(mAmpleAudioThresholdUsecs * mPlaybackRate) &&
          (mState != DECODER_STATE_SEEKING || mDecodeToSeekTarget)));
}

bool
MediaDecoderStateMachine::IsAudioSeekComplete()
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();
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
  AssertCurrentThreadInMonitor();
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
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  nsRefPtr<MediaData> audio(aAudioSample);
  MOZ_ASSERT(audio);
  mAudioDataRequest.Complete();
  aAudioSample->AdjustForStartTime(StartTime());
  mDecodedAudioEndTime = audio->GetEndTime();

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
        if (mCurrentSeek.mTarget.mType == SeekTarget::PrevSyncPoint &&
            mCurrentSeek.mTarget.mTime > mCurrentTimeBeforeSeek &&
            audio->mTime < mCurrentTimeBeforeSeek) {
          // We are doing a fastSeek, but we ended up *before* the previous
          // playback position. This is surprising UX, so switch to an accurate
          // seek and decode to the seek target. This is not conformant to the
          // spec, fastSeek should always be fast, but until we get the time to
          // change all Readers to seek to the keyframe after the currentTime
          // in this case, we'll just decode forward. Bug 1026330.
          mCurrentSeek.mTarget.mType = SeekTarget::Accurate;
        }
        if (mCurrentSeek.mTarget.mType == SeekTarget::PrevSyncPoint) {
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
MediaDecoderStateMachine::PushFront(MediaData* aSample, MediaData::Type aSampleType)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(aSample);
  if (aSample->mType == MediaData::AUDIO_DATA) {
    AudioQueue().PushFront(aSample);
  } else if (aSample->mType == MediaData::VIDEO_DATA) {
    aSample->As<VideoData>()->mFrameID = ++mCurrentFrameID;
    VideoQueue().PushFront(aSample);
  } else {
    // TODO: Handle MediaRawData, determine which queue should be pushed.
  }
  UpdateNextFrameStatus();
}

void
MediaDecoderStateMachine::OnAudioPopped(const nsRefPtr<MediaData>& aSample)
{
  MOZ_ASSERT(OnTaskQueue());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  mPlaybackOffset = std::max(mPlaybackOffset.Ref(), aSample->mOffset);
  UpdateNextFrameStatus();
  DispatchAudioDecodeTaskIfNeeded();
  MaybeStartBuffering();
}

void
MediaDecoderStateMachine::OnVideoPopped(const nsRefPtr<MediaData>& aSample)
{
  MOZ_ASSERT(OnTaskQueue());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
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
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
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
    nsRefPtr<MediaDecoderStateMachine> self = this;
    WaitRequestRef(aType).Begin(InvokeAsync(DecodeTaskQueue(), mReader.get(), __func__,
                                            &MediaDecoderReader::WaitForData, aType)
      ->Then(OwnerThread(), __func__,
             [self] (MediaData::Type aType) -> void {
               ReentrantMonitorAutoEnter mon(self->mDecoder->GetReentrantMonitor());
               self->WaitRequestRef(aType).Complete();
               self->DispatchDecodeTasksIfNeeded();
             },
             [self] (WaitForDataRejectValue aRejection) -> void {
               ReentrantMonitorAutoEnter mon(self->mDecoder->GetReentrantMonitor());
               self->WaitRequestRef(aRejection.mType).Complete();
             }));

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
  AssertCurrentThreadInMonitor();
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
  mPendingSeek.Steal(mQueuedSeek);
  SetState(DECODER_STATE_SEEKING);
  ScheduleStateMachine();
  return true;
}

void
MediaDecoderStateMachine::OnVideoDecoded(MediaData* aVideoSample)
{
  MOZ_ASSERT(OnTaskQueue());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  nsRefPtr<MediaData> video(aVideoSample);
  MOZ_ASSERT(video);
  mVideoDataRequest.Complete();
  aVideoSample->AdjustForStartTime(StartTime());
  mDecodedVideoEndTime = video ? video->GetEndTime() : mDecodedVideoEndTime;

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

      // Schedule the state machine to send stream data as soon as possible or
      // the VideoQueue() is empty before the Push().
      // VideoQueue() is empty implies the state machine thread doesn't have
      // precise time information about video frames. Once the first video
      // frame pushed in the queue, schedule the state machine as soon as
      // possible to render the video frame or delay the state machine thread
      // accurately.
      if (VideoQueue().GetSize() == 1) {
        ScheduleStateMachine();
      }

      // For non async readers, if the requested video sample was slow to
      // arrive, increase the amount of audio we buffer to ensure that we
      // don't run out of audio. This is unnecessary for async readers,
      // since they decode audio and video on different threads so they
      // are unlikely to run out of decoded audio.
      if (mReader->IsAsync()) {
        return;
      }
      TimeDuration decodeTime = TimeStamp::Now() - mVideoDecodeStartTime;
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
        if (mCurrentSeek.mTarget.mType == SeekTarget::PrevSyncPoint &&
            mCurrentSeek.mTarget.mTime > mCurrentTimeBeforeSeek &&
            video->mTime < mCurrentTimeBeforeSeek) {
          // We are doing a fastSeek, but we ended up *before* the previous
          // playback position. This is surprising UX, so switch to an accurate
          // seek and decode to the seek target. This is not conformant to the
          // spec, fastSeek should always be fast, but until we get the time to
          // change all Readers to seek to the keyframe after the currentTime
          // in this case, we'll just decode forward. Bug 1026330.
          mCurrentSeek.mTarget.mType = SeekTarget::Accurate;
        }
        if (mCurrentSeek.mTarget.mType == SeekTarget::PrevSyncPoint ||
            mPendingSeek.Exists()) {
          // Non-precise seek; or a pending seek exists ; we can stop the seek
          // at the first sample.
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
  AssertCurrentThreadInMonitor();
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
    mDecodeToSeekTarget = false;
    SeekCompleted();
  }
}

bool
MediaDecoderStateMachine::IsAudioDecoding()
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();
  return HasAudio() && !AudioQueue().IsFinished();
}

bool
MediaDecoderStateMachine::IsVideoDecoding()
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();
  return HasVideo() && !VideoQueue().IsFinished();
}

void
MediaDecoderStateMachine::CheckIfDecodeComplete()
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();
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
  AssertCurrentThreadInMonitor();
  return mMediaSink->IsPlaying();
}

nsresult MediaDecoderStateMachine::Init(MediaDecoderStateMachine* aCloneDonor)
{
  MOZ_ASSERT(NS_IsMainThread());

  MediaDecoderReader* cloneReader = nullptr;
  if (aCloneDonor) {
    cloneReader = aCloneDonor->mReader;
  }

  nsresult rv = mReader->Init(cloneReader);
  NS_ENSURE_SUCCESS(rv, rv);
  ScheduleStateMachineCrossThread();
  return NS_OK;
}

void MediaDecoderStateMachine::StopPlayback()
{
  MOZ_ASSERT(OnTaskQueue());
  DECODER_LOG("StopPlayback()");

  AssertCurrentThreadInMonitor();

  mDecoder->DispatchPlaybackStopped();

  if (IsPlaying()) {
    RenderVideoFrames(1);
    mMediaSink->SetPlaying(false);
    MOZ_ASSERT(!IsPlaying());
  }

  DispatchDecodeTasksIfNeeded();
}

void MediaDecoderStateMachine::MaybeStartPlayback()
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();
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
  mDecoder->DispatchPlaybackStarted();
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
  AssertCurrentThreadInMonitor();

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
  AssertCurrentThreadInMonitor();

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
  "NONE",
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
  AssertCurrentThreadInMonitor();
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
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  mMediaSink->SetVolume(mVolume);
}

void MediaDecoderStateMachine::RecomputeDuration()
{
  MOZ_ASSERT(OnTaskQueue());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

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
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  if (IsShutdown()) {
    return;
  }

  if (!mReader) {
    return;
  }

  DECODER_LOG("SetDormant=%d", aDormant);

  if (aDormant) {
    if (mState == DECODER_STATE_SEEKING) {
      if (mQueuedSeek.Exists()) {
        // Keep latest seek target
      } else if (mPendingSeek.Exists()) {
        mQueuedSeek.Steal(mPendingSeek);
      } else if (mCurrentSeek.Exists()) {
        mQueuedSeek.Steal(mCurrentSeek);
      } else {
        mQueuedSeek.mTarget = SeekTarget(mCurrentPosition,
                                         SeekTarget::Accurate,
                                         MediaDecoderEventVisibility::Suppressed);
        // XXXbholley - Nobody is listening to this promise. Do we need to pass it
        // back to MediaDecoder when we come out of dormant?
        nsRefPtr<MediaDecoder::SeekPromise> unused = mQueuedSeek.mPromise.Ensure(__func__);
      }
    } else {
      mQueuedSeek.mTarget = SeekTarget(mCurrentPosition,
                                       SeekTarget::Accurate,
                                       MediaDecoderEventVisibility::Suppressed);
      // XXXbholley - Nobody is listening to this promise. Do we need to pass it
      // back to MediaDecoder when we come out of dormant?
      nsRefPtr<MediaDecoder::SeekPromise> unused = mQueuedSeek.mPromise.Ensure(__func__);
    }
    mPendingSeek.RejectIfExists(__func__);
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
    ScheduleStateMachine();
    mDecodingFirstFrame = true;
    SetState(DECODER_STATE_DECODING_NONE);
  }
}

void MediaDecoderStateMachine::Shutdown()
{
  MOZ_ASSERT(OnTaskQueue());

  // Once we've entered the shutdown state here there's no going back.
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  // Change state before issuing shutdown request to threads so those
  // threads can start exiting cleanly during the Shutdown call.
  ScheduleStateMachine();
  SetState(DECODER_STATE_SHUTDOWN);

  mQueuedSeek.RejectIfExists(__func__);
  mPendingSeek.RejectIfExists(__func__);
  mCurrentSeek.RejectIfExists(__func__);

#ifdef MOZ_EME
  mCDMProxyPromise.DisconnectIfExists();
#endif

  if (IsPlaying()) {
    StopPlayback();
  }

  Reset();

  mMediaSink->Shutdown();

  // Shut down our start time rendezvous.
  if (mStartTimeRendezvous) {
    mStartTimeRendezvous->Destroy();
  }

  // Put a task in the decode queue to shutdown the reader.
  // the queue to spin down.
  InvokeAsync(DecodeTaskQueue(), mReader.get(), __func__, &MediaDecoderReader::Shutdown)
    ->Then(OwnerThread(), __func__, this,
           &MediaDecoderStateMachine::FinishShutdown,
           &MediaDecoderStateMachine::FinishShutdown);
  DECODER_LOG("Shutdown started");
}

void MediaDecoderStateMachine::StartDecoding()
{
  MOZ_ASSERT(OnTaskQueue());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
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
      mPendingSeek.Steal(mQueuedSeek);
      SetState(DECODER_STATE_SEEKING);
      ScheduleStateMachine();
      return;
    }
  }

  mDecodeStartTime = TimeStamp::Now();

  CheckIfDecodeComplete();
  if (mState == DECODER_STATE_COMPLETED) {
    return;
  }

  // Reset other state to pristine values before starting decode.
  mIsAudioPrerolling = !DonePrerollingAudio();
  mIsVideoPrerolling = !DonePrerollingVideo();

  // Ensure that we've got tasks enqueued to decode data if we need to.
  DispatchDecodeTasksIfNeeded();

  ScheduleStateMachine();
}

void MediaDecoderStateMachine::PlayStateChanged()
{
  MOZ_ASSERT(OnTaskQueue());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

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
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  ScheduleStateMachine();
}

void MediaDecoderStateMachine::SameOriginMediaChanged()
{
  MOZ_ASSERT(OnTaskQueue());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  mStreamSink->SetSameOrigin(mSameOriginMedia);
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

nsRefPtr<MediaDecoder::SeekPromise>
MediaDecoderStateMachine::Seek(SeekTarget aTarget)
{
  MOZ_ASSERT(OnTaskQueue());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  if (IsShutdown()) {
    return MediaDecoder::SeekPromise::CreateAndReject(/* aIgnored = */ true, __func__);
  }

  // We need to be able to seek both at a transport level and at a media level
  // to seek.
  if (!mMediaSeekable) {
    DECODER_WARN("Seek() function should not be called on a non-seekable state machine");
    return MediaDecoder::SeekPromise::CreateAndReject(/* aIgnored = */ true, __func__);
  }

  NS_ASSERTION(mState > DECODER_STATE_DECODING_METADATA,
               "We should have got duration already");

  if (mState < DECODER_STATE_DECODING ||
      (IsDecodingFirstFrame() && !mReader->ForceZeroStartTime())) {
    DECODER_LOG("Seek() Not Enough Data to continue at this stage, queuing seek");
    mQueuedSeek.RejectIfExists(__func__);
    mQueuedSeek.mTarget = aTarget;
    return mQueuedSeek.mPromise.Ensure(__func__);
  }
  mQueuedSeek.RejectIfExists(__func__);
  mPendingSeek.RejectIfExists(__func__);
  mPendingSeek.mTarget = aTarget;

  DECODER_LOG("Changed state to SEEKING (to %lld)", mPendingSeek.mTarget.mTime);
  SetState(DECODER_STATE_SEEKING);
  ScheduleStateMachine();

  return mPendingSeek.mPromise.Ensure(__func__);
}

nsRefPtr<MediaDecoder::SeekPromise>
MediaDecoderStateMachine::InvokeSeek(SeekTarget aTarget)
{
  return InvokeAsync(OwnerThread(), this, __func__,
                     &MediaDecoderStateMachine::Seek, aTarget);
}

void MediaDecoderStateMachine::StopMediaSink()
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();

  if (mMediaSink->IsStarted()) {
    DECODER_LOG("Stop MediaSink");
    mMediaSink->Stop();
    mMediaSinkPromise.DisconnectIfExists();
  }
}

void
MediaDecoderStateMachine::DispatchDecodeTasksIfNeeded()
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();

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
MediaDecoderStateMachine::InitiateSeek()
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();

  mCurrentSeek.RejectIfExists(__func__);
  mCurrentSeek.Steal(mPendingSeek);

  // Bound the seek time to be inside the media range.
  int64_t end = Duration().ToMicroseconds();
  NS_ASSERTION(end != -1, "Should know end time by now");
  int64_t seekTime = mCurrentSeek.mTarget.mTime;
  seekTime = std::min(seekTime, end);
  seekTime = std::max(int64_t(0), seekTime);
  NS_ASSERTION(seekTime >= 0 && seekTime <= end,
               "Can only seek in range [0,duration]");
  mCurrentSeek.mTarget.mTime = seekTime;

  mDropAudioUntilNextDiscontinuity = HasAudio();
  mDropVideoUntilNextDiscontinuity = HasVideo();
  mCurrentTimeBeforeSeek = GetMediaTime();

  // Stop playback now to ensure that while we're outside the monitor
  // dispatching SeekingStarted, playback doesn't advance and mess with
  // mCurrentPosition that we've setting to seekTime here.
  StopPlayback();
  UpdatePlaybackPositionInternal(mCurrentSeek.mTarget.mTime);

  nsCOMPtr<nsIRunnable> startEvent =
      NS_NewRunnableMethodWithArg<MediaDecoderEventVisibility>(
        mDecoder,
        &MediaDecoder::SeekingStarted,
        mCurrentSeek.mTarget.mEventVisibility);
  AbstractThread::MainThread()->Dispatch(startEvent.forget());

  // Reset our state machine and decoding pipeline before seeking.
  Reset();

  // Do the seek.
  nsRefPtr<MediaDecoderStateMachine> self = this;
  mSeekRequest.Begin(InvokeAsync(DecodeTaskQueue(), mReader.get(), __func__,
                                 &MediaDecoderReader::Seek, mCurrentSeek.mTarget.mTime,
                                 Duration().ToMicroseconds())
    ->Then(OwnerThread(), __func__,
           [self] (int64_t) -> void {
             ReentrantMonitorAutoEnter mon(self->mDecoder->GetReentrantMonitor());
             self->mSeekRequest.Complete();
             // We must decode the first samples of active streams, so we can determine
             // the new stream time. So dispatch tasks to do that.
             self->mDecodeToSeekTarget = true;
             self->DispatchDecodeTasksIfNeeded();
           }, [self] (nsresult aResult) -> void {
             ReentrantMonitorAutoEnter mon(self->mDecoder->GetReentrantMonitor());
             self->mSeekRequest.Complete();
             MOZ_ASSERT(NS_FAILED(aResult), "Cancels should also disconnect mSeekRequest");
             self->DecodeError();
           }));
}

nsresult
MediaDecoderStateMachine::DispatchAudioDecodeTaskIfNeeded()
{
  MOZ_ASSERT(OnTaskQueue());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

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
  AssertCurrentThreadInMonitor();

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
  AssertCurrentThreadInMonitor();

  SAMPLE_LOG("Queueing audio task - queued=%i, decoder-queued=%o",
             AudioQueue().GetSize(), mReader->SizeOfAudioQueueInFrames());

  if (mSentFirstFrameLoadedEvent) {
    mAudioDataRequest.Begin(InvokeAsync(DecodeTaskQueue(), mReader.get(),
                                        __func__, &MediaDecoderReader::RequestAudioData)
      ->Then(OwnerThread(), __func__, this,
             &MediaDecoderStateMachine::OnAudioDecoded,
             &MediaDecoderStateMachine::OnAudioNotDecoded));
  } else {
    mAudioDataRequest.Begin(
      InvokeAsync(DecodeTaskQueue(), mReader.get(), __func__,
                  &MediaDecoderReader::RequestAudioData)
      ->Then(OwnerThread(), __func__, mStartTimeRendezvous.get(),
             &StartTimeRendezvous::ProcessFirstSample<AudioDataPromise, MediaData::AUDIO_DATA>,
             &StartTimeRendezvous::FirstSampleRejected<MediaData::AUDIO_DATA>)
      ->CompletionPromise()
      ->Then(OwnerThread(), __func__, this,
             &MediaDecoderStateMachine::OnAudioDecoded,
             &MediaDecoderStateMachine::OnAudioNotDecoded)
    );
  }
}

nsresult
MediaDecoderStateMachine::DispatchVideoDecodeTaskIfNeeded()
{
  MOZ_ASSERT(OnTaskQueue());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

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
  AssertCurrentThreadInMonitor();

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
  AssertCurrentThreadInMonitor();

  // Time the video decode, so that if it's slow, we can increase our low
  // audio threshold to reduce the chance of an audio underrun while we're
  // waiting for a video decode to complete.
  mVideoDecodeStartTime = TimeStamp::Now();

  bool skipToNextKeyFrame = mSentFirstFrameLoadedEvent &&
    NeedToSkipToNextKeyframe();
  int64_t currentTime = mState == DECODER_STATE_SEEKING ? 0 : GetMediaTime();

  SAMPLE_LOG("Queueing video task - queued=%i, decoder-queued=%o, skip=%i, time=%lld",
             VideoQueue().GetSize(), mReader->SizeOfVideoQueueInFrames(), skipToNextKeyFrame,
             currentTime);

  if (mSentFirstFrameLoadedEvent) {
    mVideoDataRequest.Begin(
      InvokeAsync(DecodeTaskQueue(), mReader.get(), __func__,
                  &MediaDecoderReader::RequestVideoData,
                  skipToNextKeyFrame, currentTime)
      ->Then(OwnerThread(), __func__, this,
             &MediaDecoderStateMachine::OnVideoDecoded,
             &MediaDecoderStateMachine::OnVideoNotDecoded));
  } else {
    mVideoDataRequest.Begin(
      InvokeAsync(DecodeTaskQueue(), mReader.get(), __func__,
                  &MediaDecoderReader::RequestVideoData,
                  skipToNextKeyFrame, currentTime)
      ->Then(OwnerThread(), __func__, mStartTimeRendezvous.get(),
             &StartTimeRendezvous::ProcessFirstSample<VideoDataPromise, MediaData::VIDEO_DATA>,
             &StartTimeRendezvous::FirstSampleRejected<MediaData::VIDEO_DATA>)
      ->CompletionPromise()
      ->Then(OwnerThread(), __func__, this,
             &MediaDecoderStateMachine::OnVideoDecoded,
             &MediaDecoderStateMachine::OnVideoNotDecoded));
  }
}

void
MediaDecoderStateMachine::StartMediaSink()
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();

  if (!mMediaSink->IsStarted()) {
    mAudioCompleted = false;
    mMediaSink->Start(GetMediaTime(), mInfo);

    auto promise = mMediaSink->OnEnded(TrackInfo::kAudioTrack);
    if (promise) {
      mMediaSinkPromise.Begin(promise->Then(
        OwnerThread(), __func__, this,
        &MediaDecoderStateMachine::OnMediaSinkComplete,
        &MediaDecoderStateMachine::OnMediaSinkError));
    }
  }
}

int64_t MediaDecoderStateMachine::AudioDecodedUsecs()
{
  MOZ_ASSERT(OnTaskQueue());
  NS_ASSERTION(HasAudio(),
               "Should only call AudioDecodedUsecs() when we have audio");
  // The amount of audio we have decoded is the amount of audio data we've
  // already decoded and pushed to the hardware, plus the amount of audio
  // data waiting to be pushed to the hardware.
  int64_t pushed = mMediaSink->IsStarted() ? (AudioEndTime() - GetMediaTime()) : 0;

  // Currently for real time streams, AudioQueue().Duration() produce
  // wrong values (Bug 1114434), so we use frame counts to calculate duration.
  if (IsRealTime()) {
    return pushed + FramesToUsecs(AudioQueue().FrameCount(), mInfo.mAudio.mRate).value();
  }
  return pushed + AudioQueue().Duration();
}

bool MediaDecoderStateMachine::HasLowDecodedData(int64_t aAudioUsecs)
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();
  MOZ_ASSERT(mReader->UseBufferingHeuristics());
  // We consider ourselves low on decoded data if we're low on audio,
  // provided we've not decoded to the end of the audio stream, or
  // if we're low on video frames, provided
  // we've not decoded to the end of the video stream.
  return ((IsAudioDecoding() && AudioDecodedUsecs() < aAudioUsecs) ||
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
  AssertCurrentThreadInMonitor();
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

  int64_t endOfDecodedVideoData = INT64_MAX;
  if (HasVideo() && !VideoQueue().AtEndOfStream()) {
    endOfDecodedVideoData = VideoQueue().Peek() ? VideoQueue().Peek()->GetEndTime() : mVideoFrameEndTime;
  }
  int64_t endOfDecodedAudioData = INT64_MAX;
  if (HasAudio() && !AudioQueue().AtEndOfStream()) {
    // mDecodedAudioEndTime could be -1 when no audio samples are decoded.
    // But that is fine since we consider ourself as low in decoded data when
    // we don't have any decoded audio samples at all.
    endOfDecodedAudioData = mDecodedAudioEndTime;
  }
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
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
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
  nsCOMPtr<nsIRunnable> event =
    NS_NewRunnableMethod(mDecoder, &MediaDecoder::DecodeError);
  AbstractThread::MainThread()->Dispatch(event.forget());
}

void
MediaDecoderStateMachine::OnMetadataRead(MetadataHolder* aMetadata)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_ASSERT(mState == DECODER_STATE_DECODING_METADATA);
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  mMetadataRequest.Complete();

  // Set mode to PLAYBACK after reading metadata.
  mResource->SetReadMode(MediaCacheStream::MODE_PLAYBACK);
  mDecoder->DispatchSetMediaSeekable(mReader->IsMediaSeekable());
  mInfo = aMetadata->mInfo;
  mMetadataTags = aMetadata->mTags.forget();
  nsRefPtr<MediaDecoderStateMachine> self = this;

  // Set up the start time rendezvous if it doesn't already exist (which is
  // generally the case, unless we're coming out of dormant mode).
  if (!mStartTimeRendezvous) {
    mStartTimeRendezvous = new StartTimeRendezvous(OwnerThread(), HasAudio(), HasVideo(),
                                                   mReader->ForceZeroStartTime() || IsRealTime());

    mStartTimeRendezvous->AwaitStartTime()->Then(OwnerThread(), __func__,
      [self] () -> void {
        NS_ENSURE_TRUE_VOID(!self->IsShutdown());
        self->mReader->DispatchSetStartTime(self->StartTime());
      },
      [] () -> void { NS_WARNING("Setting start time on reader failed"); }
    );
  }

  if (mInfo.mMetadataDuration.isSome()) {
    RecomputeDuration();
  } else if (mInfo.mUnadjustedMetadataEndTime.isSome()) {
    mStartTimeRendezvous->AwaitStartTime()->Then(OwnerThread(), __func__,
      [self] () -> void {
        NS_ENSURE_TRUE_VOID(!self->IsShutdown());
        TimeUnit unadjusted = self->mInfo.mUnadjustedMetadataEndTime.ref();
        TimeUnit adjustment = TimeUnit::FromMicroseconds(self->StartTime());
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
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  mMetadataRequest.Complete();
  DECODER_WARN("Decode metadata failed, shutting down decoder");
  DecodeError();
}

void
MediaDecoderStateMachine::EnqueueLoadedMetadataEvent()
{
  MOZ_ASSERT(OnTaskQueue());
  nsAutoPtr<MediaInfo> info(new MediaInfo());
  *info = mInfo;
  MediaDecoderEventVisibility visibility = mSentLoadedMetadataEvent?
                                    MediaDecoderEventVisibility::Suppressed :
                                    MediaDecoderEventVisibility::Observable;
  nsCOMPtr<nsIRunnable> metadataLoadedEvent =
    new MetadataEventRunner(mDecoder, info, mMetadataTags, visibility);
  AbstractThread::MainThread()->Dispatch(metadataLoadedEvent.forget());
  mSentLoadedMetadataEvent = true;
}

void
MediaDecoderStateMachine::EnqueueFirstFrameLoadedEvent()
{
  MOZ_ASSERT(OnTaskQueue());
  nsAutoPtr<MediaInfo> info(new MediaInfo());
  *info = mInfo;
  MediaDecoderEventVisibility visibility = mSentFirstFrameLoadedEvent?
                                    MediaDecoderEventVisibility::Suppressed :
                                    MediaDecoderEventVisibility::Observable;
  nsCOMPtr<nsIRunnable> event =
    new FirstFrameLoadedEventRunner(mDecoder, info, visibility);
  AbstractThread::MainThread()->Dispatch(event.forget());
  mSentFirstFrameLoadedEvent = true;
}

bool
MediaDecoderStateMachine::IsDecodingFirstFrame()
{
  return mState == DECODER_STATE_DECODING && mDecodingFirstFrame;
}

void
MediaDecoderStateMachine::AdjustAudioThresholds()
{
  MOZ_ASSERT(OnTaskQueue());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  // Experiments show that we need to buffer more if audio is captured to avoid
  // audio glitch. See bug 1188643 comment 16 for the details.
  int64_t divisor = mAudioCaptured ? NO_VIDEO_AMPLE_AUDIO_DIVISOR / 2
                                   : NO_VIDEO_AMPLE_AUDIO_DIVISOR;

  // We're playing audio only. We don't need to worry about slow video
  // decodes causing audio underruns, so don't buffer so much audio in
  // order to reduce memory usage.
  if (HasAudio() && !HasVideo() && mSentFirstFrameLoadedEvent) {
    mAmpleAudioThresholdUsecs = detail::AMPLE_AUDIO_USECS / divisor;
    mLowAudioThresholdUsecs = detail::LOW_AUDIO_USECS / divisor;
    mQuickBufferingLowDataThresholdUsecs =
      detail::QUICK_BUFFERING_LOW_DATA_USECS / divisor;

    // Check if we need to stop audio prerolling for thresholds changed.
    if (mIsAudioPrerolling && DonePrerollingAudio()) {
      StopPrerollingAudio();
    }
  }
}

void
MediaDecoderStateMachine::FinishDecodeFirstFrame()
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();
  DECODER_LOG("FinishDecodeFirstFrame");

  if (!IsRealTime() && !mSentFirstFrameLoadedEvent) {
    RenderVideoFrames(1);
  }

  // If we don't know the duration by this point, we assume infinity, per spec.
  if (mDuration.Ref().isNothing()) {
    mDuration = Some(TimeUnit::FromInfinity());
  }

  DECODER_LOG("Media duration %lld, "
              "transportSeekable=%d, mediaSeekable=%d",
              Duration().ToMicroseconds(), mResource->IsTransportSeekable(), mMediaSeekable.Ref());

  // Get potentially updated metadata
  {
    ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
    mReader->ReadUpdatedMetadata(&mInfo);
  }

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
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  MOZ_ASSERT(mState == DECODER_STATE_SEEKING);

  int64_t seekTime = mCurrentSeek.mTarget.mTime;
  int64_t newCurrentTime = seekTime;

  // Setup timestamp state.
  nsRefPtr<MediaData> video = VideoQueue().PeekFront();
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
    int64_t videoStart = video ? video->mTime : seekTime;
    int64_t audioStart = audio ? audio->mTime : seekTime;
    newCurrentTime = std::min(audioStart, videoStart);
  } else {
    newCurrentTime = video ? video->mTime : seekTime;
  }

  if (mDecodingFirstFrame) {
    // We were resuming from dormant, or initiated a seek early.
    // We can fire loadeddata now.
    FinishDecodeFirstFrame();
  }

  // Change state to DECODING or COMPLETED now. SeekingStopped will
  // call MediaDecoderStateMachine::Seek to reset our state to SEEKING
  // if we need to seek again.

  bool isLiveStream = mResource->IsLiveStream();
  if (mPendingSeek.Exists()) {
    // A new seek target came in while we were processing the old one. No rest
    // for the seeking.
    DECODER_LOG("A new seek came along while we were finishing the old one - staying in SEEKING");
    SetState(DECODER_STATE_SEEKING);
  } else if (GetMediaTime() == Duration().ToMicroseconds() && !isLiveStream) {
    // Seeked to end of media, move to COMPLETED state. Note we don't do
    // this when playing a live stream, since the end of media will advance
    // once we download more data!
    DECODER_LOG("Changed state from SEEKING (to %lld) to COMPLETED", seekTime);
    // Explicitly set our state so we don't decode further, and so
    // we report playback ended to the media element.
    SetState(DECODER_STATE_COMPLETED);
    DispatchDecodeTasksIfNeeded();
  } else {
    DECODER_LOG("Changed state from SEEKING (to %lld) to DECODING", seekTime);
    StartDecoding();
  }

  // Ensure timestamps are up to date.
  UpdatePlaybackPositionInternal(newCurrentTime);

  // Try to decode another frame to detect if we're at the end...
  DECODER_LOG("Seek completed, mCurrentPosition=%lld", mCurrentPosition.Ref());

  // Reset quick buffering status. This ensures that if we began the
  // seek while quick-buffering, we won't bypass quick buffering mode
  // if we need to buffer after the seek.
  mQuickBuffering = false;

  mCurrentSeek.Resolve(mState == DECODER_STATE_COMPLETED, __func__);
  ScheduleStateMachine();

  if (video) {
    RenderVideoFrames(1);
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(mDecoder, &MediaDecoder::Invalidate);
    AbstractThread::MainThread()->Dispatch(event.forget());
  }
}

class DecoderDisposer
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DecoderDisposer)
  DecoderDisposer(MediaDecoder* aDecoder, MediaDecoderStateMachine* aStateMachine)
    : mDecoder(aDecoder), mStateMachine(aStateMachine) {}

  void OnTaskQueueShutdown()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mStateMachine);
    MOZ_ASSERT(mDecoder);
    mStateMachine->BreakCycles();
    mDecoder->BreakCycles();
    mStateMachine = nullptr;
    mDecoder = nullptr;
  }

private:
  virtual ~DecoderDisposer() {}
  nsRefPtr<MediaDecoder> mDecoder;
  nsRefPtr<MediaDecoderStateMachine> mStateMachine;
};

void
MediaDecoderStateMachine::DispatchShutdown()
{
  mStreamSink->BeginShutdown();
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &MediaDecoderStateMachine::Shutdown);
  OwnerThread()->Dispatch(runnable.forget());
}

void
MediaDecoderStateMachine::FinishShutdown()
{
  MOZ_ASSERT(OnTaskQueue());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

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

  mDuration.DisconnectAll();
  mIsShutdown.DisconnectAll();
  mNextFrameStatus.DisconnectAll();
  mCurrentPosition.DisconnectAll();
  mPlaybackOffset.DisconnectAll();

  // Shut down the watch manager before shutting down our task queue.
  mWatchManager.Shutdown();

  MOZ_ASSERT(mState == DECODER_STATE_SHUTDOWN,
             "How did we escape from the shutdown state?");
  // We must daisy-chain these events to destroy the decoder. We must
  // destroy the decoder on the main thread, but we can't destroy the
  // decoder while this thread holds the decoder monitor. We can't
  // dispatch an event to the main thread to destroy the decoder from
  // here, as the event may run before the dispatch returns, and we
  // hold the decoder monitor here. We also want to guarantee that the
  // state machine is destroyed on the main thread, and so the
  // event runner running this function (which holds a reference to the
  // state machine) needs to finish and be released in order to allow
  // that. So we dispatch an event to run after this event runner has
  // finished and released its monitor/references. That event then will
  // dispatch an event to the main thread to release the decoder and
  // state machine.
  DECODER_LOG("Shutting down state machine task queue");
  RefPtr<DecoderDisposer> disposer = new DecoderDisposer(mDecoder, this);
  OwnerThread()->BeginShutdown()->Then(AbstractThread::MainThread(), __func__,
                                       disposer.get(),
                                       &DecoderDisposer::OnTaskQueueShutdown,
                                       &DecoderDisposer::OnTaskQueueShutdown);
}

nsresult MediaDecoderStateMachine::RunStateMachine()
{
  MOZ_ASSERT(OnTaskQueue());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  mDelayedScheduler.Reset(); // Must happen on state machine task queue.
  mDispatchedStateMachine = false;

  MediaResource* resource = mResource;
  NS_ENSURE_TRUE(resource, NS_ERROR_NULL_POINTER);

  switch (mState) {
    case DECODER_STATE_ERROR:
    case DECODER_STATE_SHUTDOWN:
    case DECODER_STATE_DORMANT:
    case DECODER_STATE_WAIT_FOR_CDM:
      return NS_OK;

    case DECODER_STATE_DECODING_NONE: {
      SetState(DECODER_STATE_DECODING_METADATA);
      ScheduleStateMachine();
      return NS_OK;
    }

    case DECODER_STATE_DECODING_METADATA: {
      if (!mMetadataRequest.Exists()) {
        DECODER_LOG("Dispatching AsyncReadMetadata");
        // Set mode to METADATA since we are about to read metadata.
        mResource->SetReadMode(MediaCacheStream::MODE_METADATA);
        mMetadataRequest.Begin(InvokeAsync(DecodeTaskQueue(), mReader.get(), __func__,
                                           &MediaDecoderReader::AsyncReadMetadata)
          ->Then(OwnerThread(), __func__, this,
                 &MediaDecoderStateMachine::OnMetadataRead,
                 &MediaDecoderStateMachine::OnMetadataNotRead));

      }
      return NS_OK;
    }

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

      UpdateRenderedVideoFrames();
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
      if (mPendingSeek.Exists()) {
        InitiateSeek();
      }
      return NS_OK;
    }

    case DECODER_STATE_COMPLETED: {
      if (mPlayState != MediaDecoder::PLAY_STATE_PLAYING && IsPlaying()) {
        StopPlayback();
      }
      // Play the remaining media. We want to run AdvanceFrame() at least
      // once to ensure the current playback position is advanced to the
      // end of the media, and so that we update the readyState.
      if (VideoQueue().GetSize() > 1 ||
          (HasAudio() && !mAudioCompleted) ||
          (mAudioCaptured && !mStreamSink->IsFinished()))
      {
        // Start playback if necessary to play the remaining media.
        MaybeStartPlayback();
        UpdateRenderedVideoFrames();
        NS_ASSERTION(!IsPlaying() ||
                     mLogicallySeeking ||
                     IsStateMachineScheduled(),
                     "Must have timer scheduled");
        return NS_OK;
      }

      // StopPlayback in order to reset the IsPlaying() state so audio
      // is restarted correctly.
      StopPlayback();

      if (mState != DECODER_STATE_COMPLETED) {
        // While we're presenting a frame we can change state. Whatever changed
        // our state should have scheduled another state machine run.
        NS_ASSERTION(IsStateMachineScheduled(), "Must have timer scheduled");
        return NS_OK;
      }

      if (mPlayState == MediaDecoder::PLAY_STATE_PLAYING &&
          !mSentPlaybackEndedEvent)
      {
        int64_t clockTime = std::max(AudioEndTime(), mVideoFrameEndTime);
        clockTime = std::max(int64_t(0), std::max(clockTime, Duration().ToMicroseconds()));
        UpdatePlaybackPosition(clockTime);

        nsCOMPtr<nsIRunnable> event =
          NS_NewRunnableMethod(mDecoder, &MediaDecoder::PlaybackEnded);
        AbstractThread::MainThread()->Dispatch(event.forget());

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
  AssertCurrentThreadInMonitor();
  DECODER_LOG("MediaDecoderStateMachine::Reset");

  // We should be resetting because we're seeking, shutting down, or entering
  // dormant state. We could also be in the process of going dormant, and have
  // just switched to exiting dormant before we finished entering dormant,
  // hence the DECODING_NONE case below.
  MOZ_ASSERT(IsShutdown() ||
             mState == DECODER_STATE_SEEKING ||
             mState == DECODER_STATE_DORMANT ||
             mState == DECODER_STATE_DECODING_NONE);

  // Stop the audio thread. Otherwise, MediaSink might be accessing AudioQueue
  // outside of the decoder monitor while we are clearing the queue and causes
  // crash for no samples to be popped.
  StopMediaSink();

  mVideoFrameEndTime = -1;
  mDecodedVideoEndTime = -1;
  mDecodedAudioEndTime = -1;
  mAudioCompleted = false;
  AudioQueue().Reset();
  VideoQueue().Reset();
  mFirstVideoFrameAfterSeek = nullptr;
  mDropAudioUntilNextDiscontinuity = true;
  mDropVideoUntilNextDiscontinuity = true;
  mDecodeToSeekTarget = false;

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

bool MediaDecoderStateMachine::CheckFrameValidity(VideoData* aData)
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();

  // If we've sent this frame before then only return the valid state,
  // don't update the statistics.
  if (aData->mSentToCompositor) {
    return !aData->mImage || aData->mImage->IsValid();
  }

  // Update corrupt-frames statistics
  if (aData->mImage && !aData->mImage->IsValid()) {
    FrameStatistics& frameStats = mDecoder->GetFrameStatistics();
    frameStats.NotifyCorruptFrame();
    // If more than 10% of the last 30 frames have been corrupted, then try disabling
    // hardware acceleration. We use 10 as the corrupt value because RollingMean<>
    // only supports integer types.
    mCorruptFrames.insert(10);
    if (mReader->VideoIsHardwareAccelerated() &&
        frameStats.GetPresentedFrames() > 60 &&
        mCorruptFrames.mean() >= 2 /* 20% */) {
        nsCOMPtr<nsIRunnable> task =
          NS_NewRunnableMethod(mReader, &MediaDecoderReader::DisableHardwareAcceleration);
        DecodeTaskQueue()->Dispatch(task.forget());
        mCorruptFrames.clear();
      gfxCriticalNote << "Too many dropped/corrupted frames, disabling DXVA";
    }
    return false;
  } else {
    mCorruptFrames.insert(0);
    return true;
  }
}

void MediaDecoderStateMachine::RenderVideoFrames(int32_t aMaxFrames,
                                                 int64_t aClockTime,
                                                 const TimeStamp& aClockTimeStamp)
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();

  VideoFrameContainer* container = mDecoder->GetVideoFrameContainer();
  nsAutoTArray<nsRefPtr<MediaData>,16> frames;
  VideoQueue().GetFirstElements(aMaxFrames, &frames);
  if (frames.IsEmpty() || !container) {
    return;
  }

  nsAutoTArray<ImageContainer::NonOwningImage,16> images;
  TimeStamp lastFrameTime;
  for (uint32_t i = 0; i < frames.Length(); ++i) {
    VideoData* frame = frames[i]->As<VideoData>();

    bool valid = CheckFrameValidity(frame);
    frame->mSentToCompositor = true;

    if (!valid) {
      continue;
    }

    int64_t frameTime = frame->mTime;
    if (frameTime < 0) {
      // Frame times before the start time are invalid; drop such frames
      continue;
    }


    TimeStamp t;
    if (aMaxFrames > 1) {
      MOZ_ASSERT(!aClockTimeStamp.IsNull());
      int64_t delta = frame->mTime - aClockTime;
      t = aClockTimeStamp +
          TimeDuration::FromMicroseconds(delta / mPlaybackRate);
      if (!lastFrameTime.IsNull() && t <= lastFrameTime) {
        // Timestamps out of order; drop the new frame. In theory we should
        // probably replace the previous frame with the new frame if the
        // timestamps are equal, but this is a corrupt video file already so
        // never mind.
        continue;
      }
      lastFrameTime = t;
    }

    ImageContainer::NonOwningImage* img = images.AppendElement();
    img->mTimeStamp = t;
    img->mImage = frame->mImage;
    img->mFrameID = frame->mFrameID;
    img->mProducerID = mProducerID;

    VERBOSE_LOG("playing video frame %lld (id=%x) (queued=%i, state-machine=%i, decoder-queued=%i)",
                frame->mTime, frame->mFrameID,
                VideoQueue().GetSize() + mReader->SizeOfVideoQueueInFrames(),
                VideoQueue().GetSize(), mReader->SizeOfVideoQueueInFrames());
  }

  container->SetCurrentFrames(frames[0]->As<VideoData>()->mDisplay, images);
}

int64_t
MediaDecoderStateMachine::GetClock(TimeStamp* aTimeStamp) const
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();
  int64_t clockTime = mMediaSink->GetPosition(aTimeStamp);
  NS_ASSERTION(GetMediaTime() <= clockTime, "Clock should go forwards.");
  return clockTime;
}

void MediaDecoderStateMachine::UpdateRenderedVideoFrames()
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();

  if (!IsPlaying() || mLogicallySeeking) {
    return;
  }

  if (mAudioCaptured) {
    DiscardStreamData();
  }

  TimeStamp nowTime;
  const int64_t clockTime = GetClock(&nowTime);
  // Skip frames up to the frame at the playback position, and figure out
  // the time remaining until it's time to display the next frame and drop
  // the current frame.
  NS_ASSERTION(clockTime >= 0, "Should have positive clock time.");
  int64_t remainingTime = AUDIO_DURATION_USECS;
  if (VideoQueue().GetSize() > 0) {
    nsRefPtr<MediaData> currentFrame = VideoQueue().PopFront();
    int32_t framesRemoved = 0;
    while (VideoQueue().GetSize() > 0) {
      MediaData* nextFrame = VideoQueue().PeekFront();
      if (!IsRealTime() && nextFrame->mTime > clockTime) {
        remainingTime = nextFrame->mTime - clockTime;
        break;
      }
      ++framesRemoved;
      if (!currentFrame->As<VideoData>()->mSentToCompositor) {
        mDecoder->NotifyDecodedFrames(0, 0, 1);
        VERBOSE_LOG("discarding video frame mTime=%lld clock_time=%lld",
                    currentFrame->mTime, clockTime);
      }
      currentFrame = VideoQueue().PopFront();

    }
    VideoQueue().PushFront(currentFrame);
    if (framesRemoved > 0) {
      mVideoFrameEndTime = currentFrame->GetEndTime();
      FrameStatistics& frameStats = mDecoder->GetFrameStatistics();
      frameStats.NotifyPresentedFrame();
    }
  }

  RenderVideoFrames(sVideoQueueSendToCompositorSize, clockTime, nowTime);

  // Cap the current time to the larger of the audio and video end time.
  // This ensures that if we're running off the system clock, we don't
  // advance the clock to after the media end time.
  if (mVideoFrameEndTime != -1 || AudioEndTime() != -1) {
    // These will be non -1 if we've displayed a video frame, or played an audio frame.
    int64_t t = std::min(clockTime, std::max(mVideoFrameEndTime, AudioEndTime()));
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

  int64_t delay = std::max<int64_t>(1, remainingTime / mPlaybackRate);
  ScheduleStateMachineIn(delay);
}

nsresult
MediaDecoderStateMachine::DropVideoUpToSeekTarget(MediaData* aSample)
{
  MOZ_ASSERT(OnTaskQueue());
  nsRefPtr<VideoData> video(aSample->As<VideoData>());
  MOZ_ASSERT(video);
  DECODER_LOG("DropVideoUpToSeekTarget() frame [%lld, %lld]",
              video->mTime, video->GetEndTime());
  MOZ_ASSERT(mCurrentSeek.Exists());
  const int64_t target = mCurrentSeek.mTarget.mTime;

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
      nsRefPtr<VideoData> temp = VideoData::ShallowCopyUpdateTimestamp(video, target);
      video = temp;
    }
    mFirstVideoFrameAfterSeek = nullptr;

    DECODER_LOG("DropVideoUpToSeekTarget() found video frame [%lld, %lld] containing target=%lld",
                video->mTime, video->GetEndTime(), target);

    PushFront(video, MediaData::VIDEO_DATA);
  }

  return NS_OK;
}

nsresult
MediaDecoderStateMachine::DropAudioUpToSeekTarget(MediaData* aSample)
{
  MOZ_ASSERT(OnTaskQueue());
  nsRefPtr<AudioData> audio(aSample->As<AudioData>());
  MOZ_ASSERT(audio &&
             mCurrentSeek.Exists() &&
             mCurrentSeek.mTarget.mType == SeekTarget::Accurate);

  CheckedInt64 sampleDuration =
    FramesToUsecs(audio->mFrames, mInfo.mAudio.mRate);
  if (!sampleDuration.isValid()) {
    return NS_ERROR_FAILURE;
  }

  if (audio->mTime + sampleDuration.value() <= mCurrentSeek.mTarget.mTime) {
    // Our seek target lies after the frames in this AudioData. Don't
    // push it onto the audio queue, and keep decoding forwards.
    return NS_OK;
  }

  if (audio->mTime > mCurrentSeek.mTarget.mTime) {
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
  NS_ASSERTION(mCurrentSeek.mTarget.mTime >= audio->mTime,
               "Target must at or be after data start.");
  NS_ASSERTION(mCurrentSeek.mTarget.mTime < audio->mTime + sampleDuration.value(),
               "Data must end after target.");

  CheckedInt64 framesToPrune =
    UsecsToFrames(mCurrentSeek.mTarget.mTime - audio->mTime, mInfo.mAudio.mRate);
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
  nsAutoArrayPtr<AudioDataValue> audioData(new AudioDataValue[frames * channels]);
  memcpy(audioData.get(),
         audio->mAudioData.get() + (framesToPrune.value() * channels),
         frames * channels * sizeof(AudioDataValue));
  CheckedInt64 duration = FramesToUsecs(frames, mInfo.mAudio.mRate);
  if (!duration.isValid()) {
    return NS_ERROR_FAILURE;
  }
  nsRefPtr<AudioData> data(new AudioData(audio->mOffset,
                                         mCurrentSeek.mTarget.mTime,
                                         duration.value(),
                                         frames,
                                         audioData.forget(),
                                         channels,
                                         audio->mRate));
  PushFront(data, MediaData::AUDIO_DATA);

  return NS_OK;
}

void MediaDecoderStateMachine::UpdateNextFrameStatus()
{
  MOZ_ASSERT(OnTaskQueue());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

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
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  if (mState != DECODER_STATE_DECODING) {
    // We only move into BUFFERING state if we're actually decoding.
    // If we're currently doing something else, we don't need to buffer,
    // and more importantly, we shouldn't overwrite mState to interrupt
    // the current operation, as that could leave us in an inconsistent
    // state!
    return;
  }

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
  AssertCurrentThreadInMonitor();
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
  AssertCurrentThreadInMonitor();
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
  mDelayedScheduler.Ensure(target);
}

bool MediaDecoderStateMachine::OnDecodeTaskQueue() const
{
  return !DecodeTaskQueue() || DecodeTaskQueue()->IsCurrentThreadIn();
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
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  if (mLogicalPlaybackRate == 0) {
    // This case is handled in MediaDecoder by pausing playback.
    return;
  }

  mPlaybackRate = mLogicalPlaybackRate;
  mMediaSink->SetPlaybackRate(mPlaybackRate);

  ScheduleStateMachine();
}

void MediaDecoderStateMachine::PreservesPitchChanged()
{
  MOZ_ASSERT(OnTaskQueue());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
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
  AssertCurrentThreadInMonitor();
  if (mMediaSink->IsStarted()) {
    return mMediaSink->GetEndTime(TrackInfo::kAudioTrack);
  }
  MOZ_ASSERT(!HasAudio());
  return -1;
}

void MediaDecoderStateMachine::OnMediaSinkComplete()
{
  MOZ_ASSERT(OnTaskQueue());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  mMediaSinkPromise.Complete();
  // Set true only when we have audio.
  mAudioCompleted = mInfo.HasAudio();
  // To notify PlaybackEnded as soon as possible.
  ScheduleStateMachine();
}

void MediaDecoderStateMachine::OnMediaSinkError()
{
  MOZ_ASSERT(OnTaskQueue());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  mMediaSinkPromise.Complete();
  // Set true only when we have audio.
  mAudioCompleted = mInfo.HasAudio();

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
MediaDecoderStateMachine::OnCDMProxyReady(nsRefPtr<CDMProxy> aProxy)
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
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  if (aCaptured == mAudioCaptured) {
    return;
  }

  // Backup current playback parameters.
  MediaSink::PlaybackParams params = mMediaSink->GetPlaybackParams();

  // Stop and shut down the existing sink.
  StopMediaSink();
  mMediaSink->Shutdown();

  // Create a new sink according to whether audio is captured.
  // TODO: We can't really create a new DecodedStream until OutputStreamManager
  //       is extracted. It is tricky that the implementation of DecodedStream
  //       happens to allow reuse after shutdown without creating a new one.
  mMediaSink = aCaptured ? mStreamSink : CreateAudioSink();

  // Restore playback parameters.
  mMediaSink->SetPlaybackParams(params);

  // We don't need to call StartMediaSink() here because IsPlaying() is now
  // always in sync with the playing state of MediaSink. It will be started in
  // MaybeStartPlayback() in the next cycle if necessary.

  mAudioCaptured = aCaptured;
  ScheduleStateMachine();
}

uint32_t MediaDecoderStateMachine::GetAmpleVideoFrames() const
{
  MOZ_ASSERT(OnTaskQueue());
  AssertCurrentThreadInMonitor();
  return (mReader->IsAsync() && mReader->VideoIsHardwareAccelerated())
    ? std::max<uint32_t>(sVideoQueueHWAccelSize, MIN_VIDEO_QUEUE_SIZE)
    : std::max<uint32_t>(sVideoQueueDefaultSize, MIN_VIDEO_QUEUE_SIZE);
}

void MediaDecoderStateMachine::AddOutputStream(ProcessedMediaStream* aStream,
                                               bool aFinishWhenEnded)
{
  MOZ_ASSERT(NS_IsMainThread());
  DECODER_LOG("AddOutputStream aStream=%p!", aStream);
  mStreamSink->AddOutput(aStream, aFinishWhenEnded);
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableMethodWithArg<bool>(
    this, &MediaDecoderStateMachine::SetAudioCaptured, true);
  OwnerThread()->Dispatch(r.forget());
}

void MediaDecoderStateMachine::RemoveOutputStream(MediaStream* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  DECODER_LOG("RemoveOutputStream=%p!", aStream);
  mStreamSink->RemoveOutput(aStream);
  if (!mStreamSink->HasConsumers()) {
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
