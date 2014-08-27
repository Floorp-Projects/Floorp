/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef XP_WIN
// Include Windows headers required for enabling high precision timers.
#include "windows.h"
#include "mmsystem.h"
#endif

#include "mozilla/DebugOnly.h"
#include <stdint.h>

#include "MediaDecoderStateMachine.h"
#include "MediaDecoderStateMachineScheduler.h"
#include "AudioSink.h"
#include "nsTArray.h"
#include "MediaDecoder.h"
#include "MediaDecoderReader.h"
#include "mozilla/mozalloc.h"
#include "VideoUtils.h"
#include "mozilla/dom/TimeRanges.h"
#include "nsDeque.h"
#include "AudioSegment.h"
#include "VideoSegment.h"
#include "ImageContainer.h"
#include "nsComponentManagerUtils.h"
#include "nsITimer.h"
#include "nsContentUtils.h"
#include "MediaShutdownManager.h"
#include "SharedThreadPool.h"
#include "MediaTaskQueue.h"
#include "nsIEventTarget.h"
#include "prenv.h"
#include "mozilla/Preferences.h"
#include "gfx2DGlue.h"
#include "nsPrintfCString.h"

#include <algorithm>

namespace mozilla {

using namespace mozilla::layers;
using namespace mozilla::dom;
using namespace mozilla::gfx;

// avoid redefined macro in unified build
#undef DECODER_LOG
#undef VERBOSE_LOG

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaDecoderLog;
#define DECODER_LOG(x, ...) \
  PR_LOG(gMediaDecoderLog, PR_LOG_DEBUG, ("Decoder=%p " x, mDecoder.get(), ##__VA_ARGS__))
#define VERBOSE_LOG(x, ...)                            \
    PR_BEGIN_MACRO                                     \
      if (!PR_GetEnv("MOZ_QUIET")) {                   \
        DECODER_LOG(x, ##__VA_ARGS__);                 \
      }                                                \
    PR_END_MACRO
#define SAMPLE_LOG(x, ...)                             \
    PR_BEGIN_MACRO                                     \
      if (PR_GetEnv("MEDIA_LOG_SAMPLES")) {            \
        DECODER_LOG(x, ##__VA_ARGS__);                 \
      }                                                \
    PR_END_MACRO
#else
#define DECODER_LOG(x, ...)
#define VERBOSE_LOG(x, ...)
#define SAMPLE_LOG(x, ...)
#endif

// Somehow MSVC doesn't correctly delete the comma before ##__VA_ARGS__
// when __VA_ARGS__ expands to nothing. This is a workaround for it.
#define DECODER_WARN_HELPER(a, b) NS_WARNING b
#define DECODER_WARN(x, ...) \
  DECODER_WARN_HELPER(0, (nsPrintfCString("Decoder=%p " x, mDecoder.get(), ##__VA_ARGS__).get()))

// GetCurrentTime is defined in winbase.h as zero argument macro forwarding to
// GetTickCount() and conflicts with MediaDecoderStateMachine::GetCurrentTime
// implementation.  With unified builds, putting this in headers is not enough.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

// Wait this number of seconds when buffering, then leave and play
// as best as we can if the required amount of data hasn't been
// retrieved.
static const uint32_t BUFFERING_WAIT_S = 30;

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

// When we're only playing audio and we don't have a video stream, we divide
// AMPLE_AUDIO_USECS and LOW_AUDIO_USECS by the following value. This reduces
// the amount of decoded audio we buffer, reducing our memory usage. We only
// need to decode far ahead when we're decoding video using software decoding,
// as otherwise a long video decode could cause an audio underrun.
const int64_t NO_VIDEO_AMPLE_AUDIO_DIVISOR = 8;

// If we have fewer than LOW_VIDEO_FRAMES decoded frames, and
// we're not "prerolling video", we'll skip the video up to the next keyframe
// which is at or after the current playback position.
static const uint32_t LOW_VIDEO_FRAMES = 1;

// Arbitrary "frame duration" when playing only audio.
static const int AUDIO_DURATION_USECS = 40000;

// If we increase our "low audio threshold" (see LOW_AUDIO_USECS above), we
// use this as a factor in all our calculations. Increasing this will cause
// us to be more likely to increase our low audio threshold, and to
// increase it by more.
static const int THRESHOLD_FACTOR = 2;

// If we have less than this much undecoded data available, we'll consider
// ourselves to be running low on undecoded data. We determine how much
// undecoded data we have remaining using the reader's GetBuffered()
// implementation.
static const int64_t LOW_DATA_THRESHOLD_USECS = 5000000;

// LOW_DATA_THRESHOLD_USECS needs to be greater than AMPLE_AUDIO_USECS, otherwise
// the skip-to-keyframe logic can activate when we're running low on data.
static_assert(LOW_DATA_THRESHOLD_USECS > AMPLE_AUDIO_USECS,
              "LOW_DATA_THRESHOLD_USECS is too small");

// Amount of excess usecs of data to add in to the "should we buffer" calculation.
static const uint32_t EXHAUSTED_DATA_MARGIN_USECS = 60000;

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

// If we're quick buffering, we'll remain in buffering mode while we have less than
// QUICK_BUFFERING_LOW_DATA_USECS of decoded data available.
static const uint32_t QUICK_BUFFERING_LOW_DATA_USECS = 1000000;

// If QUICK_BUFFERING_LOW_DATA_USECS is > AMPLE_AUDIO_USECS, we won't exit
// quick buffering in a timely fashion, as the decode pauses when it
// reaches AMPLE_AUDIO_USECS decoded data, and thus we'll never reach
// QUICK_BUFFERING_LOW_DATA_USECS.
static_assert(QUICK_BUFFERING_LOW_DATA_USECS <= AMPLE_AUDIO_USECS,
              "QUICK_BUFFERING_LOW_DATA_USECS is too large");

// The amount of instability we tollerate in calls to
// MediaDecoderStateMachine::UpdateEstimatedDuration(); changes of duration
// less than this are ignored, as they're assumed to be the result of
// instability in the duration estimation.
static const int64_t ESTIMATED_DURATION_FUZZ_FACTOR_USECS = USECS_PER_S / 2;

static TimeDuration UsecsToDuration(int64_t aUsecs) {
  return TimeDuration::FromMilliseconds(static_cast<double>(aUsecs) / USECS_PER_MS);
}

static int64_t DurationToUsecs(TimeDuration aDuration) {
  return static_cast<int64_t>(aDuration.ToSeconds() * USECS_PER_S);
}

MediaDecoderStateMachine::MediaDecoderStateMachine(MediaDecoder* aDecoder,
                                                   MediaDecoderReader* aReader,
                                                   bool aRealTime) :
  mDecoder(aDecoder),
  mScheduler(new MediaDecoderStateMachineScheduler(
      aDecoder->GetReentrantMonitor(),
      &MediaDecoderStateMachine::TimeoutExpired,
      MOZ_THIS_IN_INITIALIZER_LIST(), aRealTime)),
  mState(DECODER_STATE_DECODING_NONE),
  mSyncPointInMediaStream(-1),
  mSyncPointInDecodedStream(-1),
  mPlayDuration(0),
  mStartTime(-1),
  mEndTime(-1),
  mFragmentEndTime(-1),
  mReader(aReader),
  mCurrentFrameTime(0),
  mAudioStartTime(-1),
  mAudioEndTime(-1),
  mVideoFrameEndTime(-1),
  mVolume(1.0),
  mPlaybackRate(1.0),
  mPreservesPitch(true),
  mAmpleVideoFrames(2),
  mLowAudioThresholdUsecs(LOW_AUDIO_USECS),
  mAmpleAudioThresholdUsecs(AMPLE_AUDIO_USECS),
  mAudioRequestPending(false),
  mVideoRequestPending(false),
  mAudioCaptured(false),
  mPositionChangeQueued(false),
  mAudioCompleted(false),
  mGotDurationFromMetaData(false),
  mDispatchedEventToDecode(false),
  mStopAudioThread(true),
  mQuickBuffering(false),
  mMinimizePreroll(false),
  mDecodeThreadWaiting(false),
  mDropAudioUntilNextDiscontinuity(false),
  mDropVideoUntilNextDiscontinuity(false),
  mDecodeToSeekTarget(false),
  mCurrentTimeBeforeSeek(0),
  mLastFrameStatus(MediaDecoderOwner::NEXT_FRAME_UNINITIALIZED)
{
  MOZ_COUNT_CTOR(MediaDecoderStateMachine);
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  mAmpleVideoFrames =
    std::max<uint32_t>(Preferences::GetUint("media.video-queue.default-size", 10), 3);

  mBufferingWait = mScheduler->IsRealTime() ? 0 : BUFFERING_WAIT_S;
  mLowDataThresholdUsecs = mScheduler->IsRealTime() ? 0 : LOW_DATA_THRESHOLD_USECS;

  mVideoPrerollFrames = mScheduler->IsRealTime() ? 0 : mAmpleVideoFrames / 2;
  mAudioPrerollUsecs = mScheduler->IsRealTime() ? 0 : LOW_AUDIO_USECS * 2;

#ifdef XP_WIN
  // Ensure high precision timers are enabled on Windows, otherwise the state
  // machine thread isn't woken up at reliable intervals to set the next frame,
  // and we drop frames while painting. Note that multiple calls to this
  // function per-process is OK, provided each call is matched by a corresponding
  // timeEndPeriod() call.
  timeBeginPeriod(1);
#endif
}

MediaDecoderStateMachine::~MediaDecoderStateMachine()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  MOZ_COUNT_DTOR(MediaDecoderStateMachine);
  NS_ASSERTION(!mPendingWakeDecoder.get(),
               "WakeDecoder should have been revoked already");

  MOZ_ASSERT(!mDecodeTaskQueue, "Should be released in SHUTDOWN");
  mReader = nullptr;

#ifdef XP_WIN
  timeEndPeriod(1);
#endif
}

bool MediaDecoderStateMachine::HasFutureAudio() {
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

bool MediaDecoderStateMachine::HaveNextFrameData() {
  AssertCurrentThreadInMonitor();
  return (!HasAudio() || HasFutureAudio()) &&
         (!HasVideo() || VideoQueue().GetSize() > 0);
}

int64_t MediaDecoderStateMachine::GetDecodedAudioDuration() {
  NS_ASSERTION(OnDecodeThread() || OnStateMachineThread(),
               "Should be on decode thread or state machine thread");
  AssertCurrentThreadInMonitor();
  int64_t audioDecoded = AudioQueue().Duration();
  if (mAudioEndTime != -1) {
    audioDecoded += mAudioEndTime - GetMediaTime();
  }
  return audioDecoded;
}

void MediaDecoderStateMachine::SendStreamAudio(AudioData* aAudio,
                                               DecodedStreamData* aStream,
                                               AudioSegment* aOutput)
{
  NS_ASSERTION(OnDecodeThread() ||
               OnStateMachineThread(), "Should be on decode thread or state machine thread");
  AssertCurrentThreadInMonitor();

  if (aAudio->mTime <= aStream->mLastAudioPacketTime) {
    // ignore packet that we've already processed
    return;
  }
  aStream->mLastAudioPacketTime = aAudio->mTime;
  aStream->mLastAudioPacketEndTime = aAudio->GetEndTime();

  // This logic has to mimic AudioSink closely to make sure we write
  // the exact same silences
  CheckedInt64 audioWrittenOffset = UsecsToFrames(mInfo.mAudio.mRate,
      aStream->mInitialTime + mStartTime) + aStream->mAudioFramesWritten;
  CheckedInt64 frameOffset = UsecsToFrames(mInfo.mAudio.mRate, aAudio->mTime);
  if (!audioWrittenOffset.isValid() || !frameOffset.isValid())
    return;
  if (audioWrittenOffset.value() < frameOffset.value()) {
    // Write silence to catch up
    VERBOSE_LOG("writing %d frames of silence to MediaStream",
                int32_t(frameOffset.value() - audioWrittenOffset.value()));
    AudioSegment silence;
    silence.InsertNullDataAtStart(frameOffset.value() - audioWrittenOffset.value());
    aStream->mAudioFramesWritten += silence.GetDuration();
    aOutput->AppendFrom(&silence);
  }

  int64_t offset;
  if (aStream->mAudioFramesWritten == 0) {
    NS_ASSERTION(frameOffset.value() <= audioWrittenOffset.value(),
                 "Otherwise we'd have taken the write-silence path");
    // We're starting in the middle of a packet. Split the packet.
    offset = audioWrittenOffset.value() - frameOffset.value();
  } else {
    // Write the entire packet.
    offset = 0;
  }

  if (offset >= aAudio->mFrames)
    return;

  aAudio->EnsureAudioBuffer();
  nsRefPtr<SharedBuffer> buffer = aAudio->mAudioBuffer;
  AudioDataValue* bufferData = static_cast<AudioDataValue*>(buffer->Data());
  nsAutoTArray<const AudioDataValue*,2> channels;
  for (uint32_t i = 0; i < aAudio->mChannels; ++i) {
    channels.AppendElement(bufferData + i*aAudio->mFrames + offset);
  }
  aOutput->AppendFrames(buffer.forget(), channels, aAudio->mFrames);
  VERBOSE_LOG("writing %d frames of data to MediaStream for AudioData at %lld",
              aAudio->mFrames - int32_t(offset), aAudio->mTime);
  aStream->mAudioFramesWritten += aAudio->mFrames - int32_t(offset);
}

static void WriteVideoToMediaStream(layers::Image* aImage,
                                    int64_t aDuration,
                                    const IntSize& aIntrinsicSize,
                                    VideoSegment* aOutput)
{
  nsRefPtr<layers::Image> image = aImage;
  aOutput->AppendFrame(image.forget(), aDuration, aIntrinsicSize);
}

static const TrackID TRACK_AUDIO = 1;
static const TrackID TRACK_VIDEO = 2;
static const TrackRate RATE_VIDEO = USECS_PER_S;

void MediaDecoderStateMachine::SendStreamData()
{
  NS_ASSERTION(OnDecodeThread() || OnStateMachineThread(),
               "Should be on decode thread or state machine thread");
  AssertCurrentThreadInMonitor();
  MOZ_ASSERT(mState != DECODER_STATE_DECODING_NONE);

  DecodedStreamData* stream = mDecoder->GetDecodedStream();
  if (!stream) {
    return;
  }

  if (mState == DECODER_STATE_DECODING_METADATA) {
    return;
  }

  // If there's still an audio sink alive, then we can't send any stream
  // data yet since both SendStreamData and the audio sink want to be in
  // charge of popping the audio queue. We're waiting for the audio sink
  if (mAudioSink) {
    return;
  }

  int64_t minLastAudioPacketTime = INT64_MAX;
  bool finished =
      (!mInfo.HasAudio() || AudioQueue().IsFinished()) &&
      (!mInfo.HasVideo() || VideoQueue().IsFinished());
  if (mDecoder->IsSameOriginMedia()) {
    SourceMediaStream* mediaStream = stream->mStream;
    StreamTime endPosition = 0;

    if (!stream->mStreamInitialized) {
      if (mInfo.HasAudio()) {
        AudioSegment* audio = new AudioSegment();
        mediaStream->AddTrack(TRACK_AUDIO, mInfo.mAudio.mRate, 0, audio);
        stream->mStream->DispatchWhenNotEnoughBuffered(TRACK_AUDIO,
            GetStateMachineThread(), GetWakeDecoderRunnable());
      }
      if (mInfo.HasVideo()) {
        VideoSegment* video = new VideoSegment();
        mediaStream->AddTrack(TRACK_VIDEO, RATE_VIDEO, 0, video);
        stream->mStream->DispatchWhenNotEnoughBuffered(TRACK_VIDEO,
            GetStateMachineThread(), GetWakeDecoderRunnable());
      }
      stream->mStreamInitialized = true;
    }

    if (mInfo.HasAudio()) {
      nsAutoTArray<AudioData*,10> audio;
      // It's OK to hold references to the AudioData because while audio
      // is captured, only the decoder thread pops from the queue (see below).
      AudioQueue().GetElementsAfter(stream->mLastAudioPacketTime, &audio);
      AudioSegment output;
      for (uint32_t i = 0; i < audio.Length(); ++i) {
        SendStreamAudio(audio[i], stream, &output);
      }
      if (output.GetDuration() > 0) {
        mediaStream->AppendToTrack(TRACK_AUDIO, &output);
      }
      if (AudioQueue().IsFinished() && !stream->mHaveSentFinishAudio) {
        mediaStream->EndTrack(TRACK_AUDIO);
        stream->mHaveSentFinishAudio = true;
      }
      minLastAudioPacketTime = std::min(minLastAudioPacketTime, stream->mLastAudioPacketTime);
      endPosition = std::max(endPosition,
          mediaStream->TicksToTimeRoundDown(mInfo.mAudio.mRate,
                                            stream->mAudioFramesWritten));
    }

    if (mInfo.HasVideo()) {
      nsAutoTArray<VideoData*,10> video;
      // It's OK to hold references to the VideoData only the decoder thread
      // pops from the queue.
      VideoQueue().GetElementsAfter(stream->mNextVideoTime, &video);
      VideoSegment output;
      for (uint32_t i = 0; i < video.Length(); ++i) {
        VideoData* v = video[i];
        if (stream->mNextVideoTime < v->mTime) {
          VERBOSE_LOG("writing last video to MediaStream %p for %lldus",
                      mediaStream, v->mTime - stream->mNextVideoTime);
          // Write last video frame to catch up. mLastVideoImage can be null here
          // which is fine, it just means there's no video.
          WriteVideoToMediaStream(stream->mLastVideoImage,
            v->mTime - stream->mNextVideoTime, stream->mLastVideoImageDisplaySize,
              &output);
          stream->mNextVideoTime = v->mTime;
        }
        if (stream->mNextVideoTime < v->GetEndTime()) {
          VERBOSE_LOG("writing video frame %lldus to MediaStream %p for %lldus",
                      v->mTime, mediaStream, v->GetEndTime() - stream->mNextVideoTime);
          WriteVideoToMediaStream(v->mImage,
              v->GetEndTime() - stream->mNextVideoTime, v->mDisplay,
              &output);
          stream->mNextVideoTime = v->GetEndTime();
          stream->mLastVideoImage = v->mImage;
          stream->mLastVideoImageDisplaySize = v->mDisplay;
        } else {
          VERBOSE_LOG("skipping writing video frame %lldus (end %lldus) to MediaStream",
                      v->mTime, v->GetEndTime());
        }
      }
      if (output.GetDuration() > 0) {
        mediaStream->AppendToTrack(TRACK_VIDEO, &output);
      }
      if (VideoQueue().IsFinished() && !stream->mHaveSentFinishVideo) {
        mediaStream->EndTrack(TRACK_VIDEO);
        stream->mHaveSentFinishVideo = true;
      }
      endPosition = std::max(endPosition,
          mediaStream->TicksToTimeRoundDown(RATE_VIDEO, stream->mNextVideoTime - stream->mInitialTime));
    }

    if (!stream->mHaveSentFinish) {
      stream->mStream->AdvanceKnownTracksTime(endPosition);
    }

    if (finished && !stream->mHaveSentFinish) {
      stream->mHaveSentFinish = true;
      stream->mStream->Finish();
    }
  }

  if (mAudioCaptured) {
    // Discard audio packets that are no longer needed.
    while (true) {
      const AudioData* a = AudioQueue().PeekFront();
      // Packet times are not 100% reliable so this may discard packets that
      // actually contain data for mCurrentFrameTime. This means if someone might
      // create a new output stream and we actually don't have the audio for the
      // very start. That's OK, we'll play silence instead for a brief moment.
      // That's OK. Seeking to this time would have a similar issue for such
      // badly muxed resources.
      if (!a || a->GetEndTime() >= minLastAudioPacketTime)
        break;
      OnAudioEndTimeUpdate(std::max(mAudioEndTime, a->GetEndTime()));
      delete AudioQueue().PopFront();
    }

    if (finished) {
      mAudioCompleted = true;
      UpdateReadyState();
    }
  }
}

MediaDecoderStateMachine::WakeDecoderRunnable*
MediaDecoderStateMachine::GetWakeDecoderRunnable()
{
  AssertCurrentThreadInMonitor();

  if (!mPendingWakeDecoder.get()) {
    mPendingWakeDecoder = new WakeDecoderRunnable(this);
  }
  return mPendingWakeDecoder.get();
}

bool MediaDecoderStateMachine::HaveEnoughDecodedAudio(int64_t aAmpleAudioUSecs)
{
  AssertCurrentThreadInMonitor();

  if (AudioQueue().GetSize() == 0 ||
      GetDecodedAudioDuration() < aAmpleAudioUSecs) {
    return false;
  }
  if (!mAudioCaptured) {
    return true;
  }

  DecodedStreamData* stream = mDecoder->GetDecodedStream();
  if (stream && stream->mStreamInitialized && !stream->mHaveSentFinishAudio) {
    if (!stream->mStream->HaveEnoughBuffered(TRACK_AUDIO)) {
      return false;
    }
    stream->mStream->DispatchWhenNotEnoughBuffered(TRACK_AUDIO,
        GetStateMachineThread(), GetWakeDecoderRunnable());
  }

  return true;
}

bool MediaDecoderStateMachine::HaveEnoughDecodedVideo()
{
  AssertCurrentThreadInMonitor();

  if (static_cast<uint32_t>(VideoQueue().GetSize()) < mAmpleVideoFrames * mPlaybackRate) {
    return false;
  }

  DecodedStreamData* stream = mDecoder->GetDecodedStream();
  if (stream && stream->mStreamInitialized && !stream->mHaveSentFinishVideo) {
    if (!stream->mStream->HaveEnoughBuffered(TRACK_VIDEO)) {
      return false;
    }
    stream->mStream->DispatchWhenNotEnoughBuffered(TRACK_VIDEO,
        GetStateMachineThread(), GetWakeDecoderRunnable());
  }

  return true;
}

bool
MediaDecoderStateMachine::NeedToDecodeVideo()
{
  AssertCurrentThreadInMonitor();
  return IsVideoDecoding() &&
         ((mState == DECODER_STATE_SEEKING && mDecodeToSeekTarget) ||
          (!mMinimizePreroll && !HaveEnoughDecodedVideo()));
}

void
MediaDecoderStateMachine::DecodeVideo()
{
  int64_t currentTime = 0;
  bool skipToNextKeyFrame = false;
  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");

    if (mState != DECODER_STATE_DECODING &&
        mState != DECODER_STATE_BUFFERING &&
        mState != DECODER_STATE_SEEKING) {
      mVideoRequestPending = false;
      DispatchDecodeTasksIfNeeded();
      return;
    }

    // We don't want to consider skipping to the next keyframe if we've
    // only just started up the decode loop, so wait until we've decoded
    // some frames before enabling the keyframe skip logic on video.
    if (mIsVideoPrerolling &&
        (static_cast<uint32_t>(VideoQueue().GetSize())
          >= mVideoPrerollFrames * mPlaybackRate))
    {
      mIsVideoPrerolling = false;
    }

    // We'll skip the video decode to the nearest keyframe if we're low on
    // audio, or if we're low on video, provided we're not running low on
    // data to decode. If we're running low on downloaded data to decode,
    // we won't start keyframe skipping, as we'll be pausing playback to buffer
    // soon anyway and we'll want to be able to display frames immediately
    // after buffering finishes.
    if (mState == DECODER_STATE_DECODING &&
        IsVideoDecoding() &&
        ((!mIsAudioPrerolling && IsAudioDecoding() &&
          GetDecodedAudioDuration() < mLowAudioThresholdUsecs * mPlaybackRate) ||
          (!mIsVideoPrerolling && IsVideoDecoding() &&
           // don't skip frame when |clock time| <= |mVideoFrameEndTime| for
           // we are still in the safe range without underrunning video frames
           GetClock() > mVideoFrameEndTime &&
          (static_cast<uint32_t>(VideoQueue().GetSize())
            < LOW_VIDEO_FRAMES * mPlaybackRate))) &&
        !HasLowUndecodedData())
    {
      skipToNextKeyFrame = true;
      DECODER_LOG("Skipping video decode to the next keyframe");
    }
    currentTime = mState == DECODER_STATE_SEEKING ? 0 : GetMediaTime();

    // Time the video decode, so that if it's slow, we can increase our low
    // audio threshold to reduce the chance of an audio underrun while we're
    // waiting for a video decode to complete.
    mVideoDecodeStartTime = TimeStamp::Now();
  }

  mReader->RequestVideoData(skipToNextKeyFrame, currentTime);
}

bool
MediaDecoderStateMachine::NeedToDecodeAudio()
{
  AssertCurrentThreadInMonitor();
  return IsAudioDecoding() &&
         ((mState == DECODER_STATE_SEEKING && mDecodeToSeekTarget) ||
          (!mMinimizePreroll &&
          !HaveEnoughDecodedAudio(mAmpleAudioThresholdUsecs * mPlaybackRate) &&
          (mState != DECODER_STATE_SEEKING || mDecodeToSeekTarget)));
}

void
MediaDecoderStateMachine::DecodeAudio()
{
  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");

    if (mState != DECODER_STATE_DECODING &&
        mState != DECODER_STATE_BUFFERING &&
        mState != DECODER_STATE_SEEKING) {
      mAudioRequestPending = false;
      DispatchDecodeTasksIfNeeded();
      mon.NotifyAll();
      return;
    }

    // We don't want to consider skipping to the next keyframe if we've
    // only just started up the decode loop, so wait until we've decoded
    // some audio data before enabling the keyframe skip logic on audio.
    if (mIsAudioPrerolling &&
        GetDecodedAudioDuration() >= mAudioPrerollUsecs * mPlaybackRate) {
      mIsAudioPrerolling = false;
    }
  }
  mReader->RequestAudioData();
}

bool
MediaDecoderStateMachine::IsAudioSeekComplete()
{
  AssertCurrentThreadInMonitor();
  SAMPLE_LOG("IsAudioSeekComplete() curTarVal=%d mAudDis=%d aqFin=%d aqSz=%d",
    mCurrentSeekTarget.IsValid(), mDropAudioUntilNextDiscontinuity, AudioQueue().IsFinished(), AudioQueue().GetSize());
  return
    !HasAudio() ||
    (mCurrentSeekTarget.IsValid() &&
     !mDropAudioUntilNextDiscontinuity &&
     (AudioQueue().IsFinished() || AudioQueue().GetSize() > 0));
}

bool
MediaDecoderStateMachine::IsVideoSeekComplete()
{
  AssertCurrentThreadInMonitor();
  SAMPLE_LOG("IsVideoSeekComplete() curTarVal=%d mVidDis=%d vqFin=%d vqSz=%d",
    mCurrentSeekTarget.IsValid(), mDropVideoUntilNextDiscontinuity, VideoQueue().IsFinished(), VideoQueue().GetSize());
  return
    !HasVideo() ||
    (mCurrentSeekTarget.IsValid() &&
     !mDropVideoUntilNextDiscontinuity &&
     (VideoQueue().IsFinished() || VideoQueue().GetSize() > 0));
}

void
MediaDecoderStateMachine::OnAudioEOS()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  SAMPLE_LOG("OnAudioEOS");
  mAudioRequestPending = false;
  AudioQueue().Finish();
  switch (mState) {
    case DECODER_STATE_DECODING_METADATA: {
      MaybeFinishDecodeMetadata();
      return;
    }
    case DECODER_STATE_BUFFERING:
    case DECODER_STATE_DECODING: {
      CheckIfDecodeComplete();
      SendStreamData();
      // The ready state can change when we've decoded data, so update the
      // ready state, so that DOM events can fire.
      UpdateReadyState();
      mDecoder->GetReentrantMonitor().NotifyAll();
      return;
    }

    case DECODER_STATE_SEEKING: {
      if (!mCurrentSeekTarget.IsValid()) {
        // We've received an EOS from a previous decode. Discard it.
        return;
      }
      mDropAudioUntilNextDiscontinuity = false;
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
MediaDecoderStateMachine::OnAudioDecoded(AudioData* aAudioSample)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  nsAutoPtr<AudioData> audio(aAudioSample);
  MOZ_ASSERT(audio);
  mAudioRequestPending = false;

  SAMPLE_LOG("OnAudioDecoded [%lld,%lld] disc=%d",
             (audio ? audio->mTime : -1),
             (audio ? audio->GetEndTime() : -1),
             (audio ? audio->mDiscontinuity : 0));

  switch (mState) {
    case DECODER_STATE_DECODING_METADATA: {
      Push(audio.forget());
      MaybeFinishDecodeMetadata();
      return;
    }

    case DECODER_STATE_BUFFERING:
    case DECODER_STATE_DECODING: {
      // In buffering and decoding state, we simply enqueue samples.
      Push(audio.forget());
      return;
    }

    case DECODER_STATE_SEEKING: {
      if (!mCurrentSeekTarget.IsValid()) {
        // We've received a sample from a previous decode. Discard it.
        return;
      }
      if (audio->mDiscontinuity) {
        mDropAudioUntilNextDiscontinuity = false;
      }
      if (!mDropAudioUntilNextDiscontinuity) {
        // We must be after the discontinuity; we're receiving samples
        // at or after the seek target.
        if (mCurrentSeekTarget.mType == SeekTarget::PrevSyncPoint &&
            mCurrentSeekTarget.mTime > mCurrentTimeBeforeSeek &&
            audio->mTime < mCurrentTimeBeforeSeek) {
          // We are doing a fastSeek, but we ended up *before* the previous
          // playback position. This is surprising UX, so switch to an accurate
          // seek and decode to the seek target. This is not conformant to the
          // spec, fastSeek should always be fast, but until we get the time to
          // change all Readers to seek to the keyframe after the currentTime
          // in this case, we'll just decode forward. Bug 1026330.
          mCurrentSeekTarget.mType = SeekTarget::Accurate;
        }
        if (mCurrentSeekTarget.mType == SeekTarget::PrevSyncPoint) {
          // Non-precise seek; we can stop the seek at the first sample.
          AudioQueue().Push(audio.forget());
        } else {
          // We're doing an accurate seek. We must discard
          // MediaData up to the one containing exact seek target.
          if (NS_FAILED(DropAudioUpToSeekTarget(audio.forget()))) {
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
MediaDecoderStateMachine::Push(AudioData* aSample)
{
  MOZ_ASSERT(aSample);
  // TODO: Send aSample to MSG and recalculate readystate before pushing,
  // otherwise AdvanceFrame may pop the sample before we have a chance
  // to reach playing.
  AudioQueue().Push(aSample);
  if (mState > DECODER_STATE_DECODING_METADATA) {
    SendStreamData();
    // The ready state can change when we've decoded data, so update the
    // ready state, so that DOM events can fire.
    UpdateReadyState();
    DispatchDecodeTasksIfNeeded();
    mDecoder->GetReentrantMonitor().NotifyAll();
  }
}

void
MediaDecoderStateMachine::Push(VideoData* aSample)
{
  MOZ_ASSERT(aSample);
  // TODO: Send aSample to MSG and recalculate readystate before pushing,
  // otherwise AdvanceFrame may pop the sample before we have a chance
  // to reach playing.
  VideoQueue().Push(aSample);
  if (mState > DECODER_STATE_DECODING_METADATA) {
    SendStreamData();
    // The ready state can change when we've decoded data, so update the
    // ready state, so that DOM events can fire.
    UpdateReadyState();
    DispatchDecodeTasksIfNeeded();
    mDecoder->GetReentrantMonitor().NotifyAll();
  }
}

void
MediaDecoderStateMachine::OnDecodeError()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  DecodeError();
}

void
MediaDecoderStateMachine::MaybeFinishDecodeMetadata()
{
  AssertCurrentThreadInMonitor();
  if ((IsAudioDecoding() && AudioQueue().GetSize() == 0) ||
      (IsVideoDecoding() && VideoQueue().GetSize() == 0)) {
    return;
  }
  if (NS_FAILED(FinishDecodeMetadata())) {
    DecodeError();
  }
}

void
MediaDecoderStateMachine::OnVideoEOS()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  SAMPLE_LOG("OnVideoEOS");
  mVideoRequestPending = false;
  switch (mState) {
    case DECODER_STATE_DECODING_METADATA: {
      VideoQueue().Finish();
      MaybeFinishDecodeMetadata();
      return;
    }

    case DECODER_STATE_BUFFERING:
    case DECODER_STATE_DECODING: {
      VideoQueue().Finish();
      CheckIfDecodeComplete();
      SendStreamData();
      // The ready state can change when we've decoded data, so update the
      // ready state, so that DOM events can fire.
      UpdateReadyState();
      mDecoder->GetReentrantMonitor().NotifyAll();
      return;
    }
    case DECODER_STATE_SEEKING: {
      if (!mCurrentSeekTarget.IsValid()) {
        // We've received a sample from a previous decode. Discard it.
        return;
      }
      // Null sample. Hit end of stream. If we have decoded a frame,
      // insert it into the queue so that we have something to display.
      if (mFirstVideoFrameAfterSeek) {
        VideoQueue().Push(mFirstVideoFrameAfterSeek.forget());
      }
      VideoQueue().Finish();
      mDropVideoUntilNextDiscontinuity = false;
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
MediaDecoderStateMachine::OnVideoDecoded(VideoData* aVideoSample)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  nsAutoPtr<VideoData> video(aVideoSample);
  mVideoRequestPending = false;

  SAMPLE_LOG("OnVideoDecoded [%lld,%lld] disc=%d",
             (video ? video->mTime : -1),
             (video ? video->GetEndTime() : -1),
             (video ? video->mDiscontinuity : 0));

  switch (mState) {
    case DECODER_STATE_DECODING_METADATA: {
      Push(video.forget());
      MaybeFinishDecodeMetadata();
      return;
    }

    case DECODER_STATE_BUFFERING:
    case DECODER_STATE_DECODING: {
      Push(video.forget());
      // If the requested video sample was slow to arrive, increase the
      // amount of audio we buffer to ensure that we don't run out of audio.
      // TODO: Detect when we're truly async, and don't do this if so, as
      // it's not necessary.
      TimeDuration decodeTime = TimeStamp::Now() - mVideoDecodeStartTime;
      if (THRESHOLD_FACTOR * DurationToUsecs(decodeTime) > mLowAudioThresholdUsecs &&
          !HasLowUndecodedData())
      {
        mLowAudioThresholdUsecs =
          std::min(THRESHOLD_FACTOR * DurationToUsecs(decodeTime), AMPLE_AUDIO_USECS);
        mAmpleAudioThresholdUsecs = std::max(THRESHOLD_FACTOR * mLowAudioThresholdUsecs,
                                              mAmpleAudioThresholdUsecs);
        DECODER_LOG("Slow video decode, set mLowAudioThresholdUsecs=%lld mAmpleAudioThresholdUsecs=%lld",
                    mLowAudioThresholdUsecs, mAmpleAudioThresholdUsecs);
      }
      return;
    }
    case DECODER_STATE_SEEKING: {
      if (!mCurrentSeekTarget.IsValid()) {
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
        if (mCurrentSeekTarget.mType == SeekTarget::PrevSyncPoint &&
            mCurrentSeekTarget.mTime > mCurrentTimeBeforeSeek &&
            video->mTime < mCurrentTimeBeforeSeek) {
          // We are doing a fastSeek, but we ended up *before* the previous
          // playback position. This is surprising UX, so switch to an accurate
          // seek and decode to the seek target. This is not conformant to the
          // spec, fastSeek should always be fast, but until we get the time to
          // change all Readers to seek to the keyframe after the currentTime
          // in this case, we'll just decode forward. Bug 1026330.
          mCurrentSeekTarget.mType = SeekTarget::Accurate;
        }
        if (mCurrentSeekTarget.mType == SeekTarget::PrevSyncPoint) {
          // Non-precise seek; we can stop the seek at the first sample.
          VideoQueue().Push(video.forget());
        } else {
          // We're doing an accurate seek. We still need to discard
          // MediaData up to the one containing exact seek target.
          if (NS_FAILED(DropVideoUpToSeekTarget(video.forget()))) {
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
  AssertCurrentThreadInMonitor();

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
    RefPtr<nsIRunnable> task(
      NS_NewRunnableMethod(this, &MediaDecoderStateMachine::SeekCompleted));
    nsresult rv = mDecodeTaskQueue->Dispatch(task);
    if (NS_FAILED(rv)) {
      DecodeError();
    }
  }
}

bool
MediaDecoderStateMachine::IsAudioDecoding()
{
  AssertCurrentThreadInMonitor();
  return HasAudio() && !AudioQueue().IsFinished();
}

bool
MediaDecoderStateMachine::IsVideoDecoding()
{
  AssertCurrentThreadInMonitor();
  return HasVideo() && !VideoQueue().IsFinished();
}

void
MediaDecoderStateMachine::CheckIfDecodeComplete()
{
  AssertCurrentThreadInMonitor();
  if (mState == DECODER_STATE_SHUTDOWN ||
      mState == DECODER_STATE_SEEKING ||
      mState == DECODER_STATE_COMPLETED) {
    // Don't change our state if we've already been shutdown, or we're seeking,
    // since we don't want to abort the shutdown or seek processes.
    return;
  }
  if (!IsVideoDecoding() && !IsAudioDecoding()) {
    // We've finished decoding all active streams,
    // so move to COMPLETED state.
    mState = DECODER_STATE_COMPLETED;
    DispatchDecodeTasksIfNeeded();
    ScheduleStateMachine();
  }
  DECODER_LOG("CheckIfDecodeComplete %scompleted",
              ((mState == DECODER_STATE_COMPLETED) ? "" : "NOT "));
}

bool MediaDecoderStateMachine::IsPlaying()
{
  AssertCurrentThreadInMonitor();

  return !mPlayStartTime.IsNull();
}

nsresult MediaDecoderStateMachine::Init(MediaDecoderStateMachine* aCloneDonor)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<SharedThreadPool> decodePool(GetMediaDecodeThreadPool());
  NS_ENSURE_TRUE(decodePool, NS_ERROR_FAILURE);

  mDecodeTaskQueue = new MediaTaskQueue(decodePool.forget());
  NS_ENSURE_TRUE(mDecodeTaskQueue, NS_ERROR_FAILURE);

  MediaDecoderReader* cloneReader = nullptr;
  if (aCloneDonor) {
    cloneReader = aCloneDonor->mReader;
  }

  nsresult rv = mScheduler->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  // Note: This creates a cycle, broken in shutdown.
  mMediaDecodedListener =
    new MediaDataDecodedListener<MediaDecoderStateMachine>(this,
                                                           mDecodeTaskQueue);
  mReader->SetCallback(mMediaDecodedListener);
  mReader->SetTaskQueue(mDecodeTaskQueue);

  rv = mReader->Init(cloneReader);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void MediaDecoderStateMachine::StopPlayback()
{
  DECODER_LOG("StopPlayback()");

  AssertCurrentThreadInMonitor();

  mDecoder->NotifyPlaybackStopped();

  if (IsPlaying()) {
    mPlayDuration = GetClock() - mStartTime;
    SetPlayStartTime(TimeStamp());
  }
  // Notify the audio sink, so that it notices that we've stopped playing,
  // so it can pause audio playback.
  mDecoder->GetReentrantMonitor().NotifyAll();
  NS_ASSERTION(!IsPlaying(), "Should report not playing at end of StopPlayback()");
  mDecoder->UpdateStreamBlockingForStateMachinePlaying();

  DispatchDecodeTasksIfNeeded();
}

void MediaDecoderStateMachine::SetSyncPointForMediaStream()
{
  AssertCurrentThreadInMonitor();

  DecodedStreamData* stream = mDecoder->GetDecodedStream();
  if (!stream) {
    return;
  }

  mSyncPointInMediaStream = stream->GetLastOutputTime();
  mSyncPointInDecodedStream = mStartTime + mPlayDuration;
}

int64_t MediaDecoderStateMachine::GetCurrentTimeViaMediaStreamSync()
{
  AssertCurrentThreadInMonitor();
  NS_ASSERTION(mSyncPointInDecodedStream >= 0, "Should have set up sync point");
  DecodedStreamData* stream = mDecoder->GetDecodedStream();
  int64_t streamDelta = stream->GetLastOutputTime() - mSyncPointInMediaStream;
  return mSyncPointInDecodedStream + streamDelta;
}

void MediaDecoderStateMachine::StartPlayback()
{
  DECODER_LOG("StartPlayback()");

  NS_ASSERTION(!IsPlaying(), "Shouldn't be playing when StartPlayback() is called");
  AssertCurrentThreadInMonitor();

  if (mDecoder->CheckDecoderCanOffloadAudio()) {
    DECODER_LOG("Offloading playback");
    return;
  }

  mDecoder->NotifyPlaybackStarted();
  SetPlayStartTime(TimeStamp::Now());

  NS_ASSERTION(IsPlaying(), "Should report playing by end of StartPlayback()");
  if (NS_FAILED(StartAudioThread())) {
    DECODER_WARN("Failed to create audio thread");
  }
  mDecoder->GetReentrantMonitor().NotifyAll();
  mDecoder->UpdateStreamBlockingForStateMachinePlaying();
  DispatchDecodeTasksIfNeeded();
}

void MediaDecoderStateMachine::UpdatePlaybackPositionInternal(int64_t aTime)
{
  SAMPLE_LOG("UpdatePlaybackPositionInternal(%lld) (mStartTime=%lld)", aTime, mStartTime);
  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine thread.");
  AssertCurrentThreadInMonitor();

  NS_ASSERTION(mStartTime >= 0, "Should have positive mStartTime");
  mCurrentFrameTime = aTime - mStartTime;
  NS_ASSERTION(mCurrentFrameTime >= 0, "CurrentTime should be positive!");
  if (aTime > mEndTime) {
    NS_ASSERTION(mCurrentFrameTime > GetDuration(),
                 "CurrentTime must be after duration if aTime > endTime!");
    mEndTime = aTime;
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(mDecoder, &MediaDecoder::DurationChanged);
    NS_DispatchToMainThread(event);
  }
}

void MediaDecoderStateMachine::UpdatePlaybackPosition(int64_t aTime)
{
  UpdatePlaybackPositionInternal(aTime);

  bool fragmentEnded = mFragmentEndTime >= 0 && GetMediaTime() >= mFragmentEndTime;
  if (!mPositionChangeQueued || fragmentEnded) {
    mPositionChangeQueued = true;
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(mDecoder, &MediaDecoder::PlaybackPositionChanged);
    NS_DispatchToMainThread(event);
  }

  mMetadataManager.DispatchMetadataIfNeeded(mDecoder, aTime);

  if (fragmentEnded) {
    StopPlayback();
  }
}

void MediaDecoderStateMachine::ClearPositionChangeFlag()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  AssertCurrentThreadInMonitor();

  mPositionChangeQueued = false;
}

MediaDecoderOwner::NextFrameStatus MediaDecoderStateMachine::GetNextFrameStatus()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  if (IsBuffering() || IsSeeking()) {
    return MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE_BUFFERING;
  } else if (HaveNextFrameData()) {
    return MediaDecoderOwner::NEXT_FRAME_AVAILABLE;
  }
  return MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE;
}

void MediaDecoderStateMachine::SetVolume(double volume)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  mVolume = volume;
  if (mAudioSink) {
    mAudioSink->SetVolume(mVolume);
  }
}

void MediaDecoderStateMachine::SetAudioCaptured(bool aCaptured)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  if (!mAudioCaptured && aCaptured && !mStopAudioThread) {
    // Make sure the state machine runs as soon as possible. That will
    // stop the audio sink.
    // If mStopAudioThread is true then we're already stopping the audio sink
    // and since we set mAudioCaptured to true, nothing can start it again.
    ScheduleStateMachine();
  }
  mAudioCaptured = aCaptured;
}

double MediaDecoderStateMachine::GetCurrentTime() const
{
  NS_ASSERTION(NS_IsMainThread() ||
               OnStateMachineThread() ||
               OnDecodeThread(),
               "Should be on main, decode, or state machine thread.");

  return static_cast<double>(mCurrentFrameTime) / static_cast<double>(USECS_PER_S);
}

int64_t MediaDecoderStateMachine::GetDuration()
{
  AssertCurrentThreadInMonitor();

  if (mEndTime == -1 || mStartTime == -1)
    return -1;
  return mEndTime - mStartTime;
}

void MediaDecoderStateMachine::SetDuration(int64_t aDuration)
{
  NS_ASSERTION(NS_IsMainThread() || OnDecodeThread(),
               "Should be on main or decode thread.");
  AssertCurrentThreadInMonitor();

  if (aDuration == -1) {
    return;
  }

  if (mStartTime != -1) {
    mEndTime = mStartTime + aDuration;
  } else {
    mStartTime = 0;
    mEndTime = aDuration;
  }
}

void MediaDecoderStateMachine::UpdateEstimatedDuration(int64_t aDuration)
{
  AssertCurrentThreadInMonitor();
  int64_t duration = GetDuration();
  if (aDuration != duration &&
      abs(aDuration - duration) > ESTIMATED_DURATION_FUZZ_FACTOR_USECS) {
    SetDuration(aDuration);
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(mDecoder, &MediaDecoder::DurationChanged);
    NS_DispatchToMainThread(event);
  }
}

void MediaDecoderStateMachine::SetMediaEndTime(int64_t aEndTime)
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread");
  AssertCurrentThreadInMonitor();

  mEndTime = aEndTime;
}

void MediaDecoderStateMachine::SetFragmentEndTime(int64_t aEndTime)
{
  AssertCurrentThreadInMonitor();

  mFragmentEndTime = aEndTime < 0 ? aEndTime : aEndTime + mStartTime;
}

bool MediaDecoderStateMachine::IsDormantNeeded()
{
  return mReader->IsDormantNeeded();
}

void MediaDecoderStateMachine::SetDormant(bool aDormant)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  AssertCurrentThreadInMonitor();

  if (!mReader) {
    return;
  }

  DECODER_LOG("SetDormant=%d", aDormant);

  if (aDormant) {
    ScheduleStateMachine();
    mState = DECODER_STATE_DORMANT;
    mDecoder->GetReentrantMonitor().NotifyAll();
  } else if ((aDormant != true) && (mState == DECODER_STATE_DORMANT)) {
    ScheduleStateMachine();
    mStartTime = 0;
    mCurrentFrameTime = 0;
    mState = DECODER_STATE_DECODING_NONE;
    mDecoder->GetReentrantMonitor().NotifyAll();
  }
}

void MediaDecoderStateMachine::Shutdown()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  // Once we've entered the shutdown state here there's no going back.
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  // Change state before issuing shutdown request to threads so those
  // threads can start exiting cleanly during the Shutdown call.
  DECODER_LOG("Changed state to SHUTDOWN");
  mState = DECODER_STATE_SHUTDOWN;
  mScheduler->ScheduleAndShutdown();
  if (mAudioSink) {
    mAudioSink->PrepareToShutdown();
  }
  mDecoder->GetReentrantMonitor().NotifyAll();
}

void MediaDecoderStateMachine::StartDecoding()
{
  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine or decode thread.");
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  if (mState == DECODER_STATE_DECODING) {
    return;
  }
  mState = DECODER_STATE_DECODING;

  mDecodeStartTime = TimeStamp::Now();

  CheckIfDecodeComplete();
  if (mState == DECODER_STATE_COMPLETED) {
    return;
  }

  // Reset other state to pristine values before starting decode.
  mIsAudioPrerolling = true;
  mIsVideoPrerolling = true;

  // Ensure that we've got tasks enqueued to decode data if we need to.
  DispatchDecodeTasksIfNeeded();

  ScheduleStateMachine();
}

void MediaDecoderStateMachine::StartWaitForResources()
{
  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine or decode thread.");
  AssertCurrentThreadInMonitor();
  mState = DECODER_STATE_WAIT_FOR_RESOURCES;
  DECODER_LOG("StartWaitForResources");
}

void MediaDecoderStateMachine::NotifyWaitingForResourcesStatusChanged()
{
  AssertCurrentThreadInMonitor();
  if (mState != DECODER_STATE_WAIT_FOR_RESOURCES ||
      mReader->IsWaitingMediaResources()) {
    return;
  }
  DECODER_LOG("NotifyWaitingForResourcesStatusChanged");
  // The reader is no longer waiting for resources (say a hardware decoder),
  // we can now proceed to decode metadata.
  mState = DECODER_STATE_DECODING_NONE;
  ScheduleStateMachine();
}

void MediaDecoderStateMachine::Play()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  // When asked to play, switch to decoding state only if
  // we are currently buffering. In other cases, we'll start playing anyway
  // when the state machine notices the decoder's state change to PLAYING.
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  if (mState == DECODER_STATE_BUFFERING) {
    DECODER_LOG("Changed state from BUFFERING to DECODING");
    mState = DECODER_STATE_DECODING;
    mDecodeStartTime = TimeStamp::Now();
  }
  // Once we start playing, we don't want to minimize our prerolling, as we
  // assume the user is likely to want to keep playing in future.
  mMinimizePreroll = false;
  ScheduleStateMachine();
}

void MediaDecoderStateMachine::ResetPlayback()
{
  MOZ_ASSERT(mState == DECODER_STATE_SEEKING ||
             mState == DECODER_STATE_SHUTDOWN ||
             mState == DECODER_STATE_DORMANT);
  mVideoFrameEndTime = -1;
  mAudioStartTime = -1;
  mAudioEndTime = -1;
  mAudioCompleted = false;
  AudioQueue().Reset();
  VideoQueue().Reset();
  mFirstVideoFrameAfterSeek = nullptr;
  mDropAudioUntilNextDiscontinuity = true;
  mDropVideoUntilNextDiscontinuity = true;
}

void MediaDecoderStateMachine::NotifyDataArrived(const char* aBuffer,
                                                     uint32_t aLength,
                                                     int64_t aOffset)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  mReader->NotifyDataArrived(aBuffer, aLength, aOffset);

  // While playing an unseekable stream of unknown duration, mEndTime is
  // updated (in AdvanceFrame()) as we play. But if data is being downloaded
  // faster than played, mEndTime won't reflect the end of playable data
  // since we haven't played the frame at the end of buffered data. So update
  // mEndTime here as new data is downloaded to prevent such a lag.
  nsRefPtr<dom::TimeRanges> buffered = new dom::TimeRanges();
  if (mDecoder->IsInfinite() &&
      NS_SUCCEEDED(mDecoder->GetBuffered(buffered)))
  {
    uint32_t length = 0;
    buffered->GetLength(&length);
    if (length) {
      double end = 0;
      buffered->End(length - 1, &end);
      ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
      mEndTime = std::max<int64_t>(mEndTime, end * USECS_PER_S);
    }
  }
}

void MediaDecoderStateMachine::Seek(const SeekTarget& aTarget)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  // We need to be able to seek both at a transport level and at a media level
  // to seek.
  if (!mDecoder->IsMediaSeekable()) {
    DECODER_WARN("Seek() function should not be called on a non-seekable state machine");
    return;
  }
  // MediaDecoder::mPlayState should be SEEKING while we seek, and
  // in that case MediaDecoder shouldn't be calling us.
  NS_ASSERTION(mState != DECODER_STATE_SEEKING,
               "We shouldn't already be seeking");
  NS_ASSERTION(mState >= DECODER_STATE_DECODING,
               "We should have loaded metadata");

  // Bound the seek time to be inside the media range.
  NS_ASSERTION(mStartTime != -1, "Should know start time by now");
  NS_ASSERTION(mEndTime != -1, "Should know end time by now");
  int64_t seekTime = aTarget.mTime + mStartTime;
  seekTime = std::min(seekTime, mEndTime);
  seekTime = std::max(mStartTime, seekTime);
  NS_ASSERTION(seekTime >= mStartTime && seekTime <= mEndTime,
               "Can only seek in range [0,duration]");
  mSeekTarget = SeekTarget(seekTime, aTarget.mType);

  DECODER_LOG("Changed state to SEEKING (to %lld)", mSeekTarget.mTime);
  mState = DECODER_STATE_SEEKING;
  if (mDecoder->GetDecodedStream()) {
    mDecoder->RecreateDecodedStream(seekTime - mStartTime);
  }
  ScheduleStateMachine();
}

void MediaDecoderStateMachine::StopAudioThread()
{
  NS_ASSERTION(OnDecodeThread() ||
               OnStateMachineThread(), "Should be on decode thread or state machine thread");
  AssertCurrentThreadInMonitor();

  if (mStopAudioThread) {
    // Nothing to do, since the thread is already stopping
    return;
  }

  mStopAudioThread = true;
  mDecoder->GetReentrantMonitor().NotifyAll();
  if (mAudioSink) {
    DECODER_LOG("Shutdown audio thread");
    mAudioSink->PrepareToShutdown();
    {
      ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
      mAudioSink->Shutdown();
    }
    mAudioSink = nullptr;
    // Now that the audio sink is dead, try sending data to our MediaStream(s).
    // That may have been waiting for the audio thread to stop.
    SendStreamData();
  }
}

nsresult
MediaDecoderStateMachine::EnqueueDecodeMetadataTask()
{
  AssertCurrentThreadInMonitor();
  MOZ_ASSERT(mState == DECODER_STATE_DECODING_METADATA);

  RefPtr<nsIRunnable> task(
    NS_NewRunnableMethod(this, &MediaDecoderStateMachine::CallDecodeMetadata));
  nsresult rv = mDecodeTaskQueue->Dispatch(task);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

void
MediaDecoderStateMachine::SetReaderIdle()
{
#ifdef PR_LOGGING
  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    DECODER_LOG("SetReaderIdle() audioQueue=%lld videoQueue=%lld",
                GetDecodedAudioDuration(),
                VideoQueue().Duration());
  }
#endif
  MOZ_ASSERT(OnDecodeThread());
  mReader->SetIdle();
}

void
MediaDecoderStateMachine::DispatchDecodeTasksIfNeeded()
{
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

  bool needIdle = !mDecoder->IsLogicallyPlaying() &&
                  mState != DECODER_STATE_SEEKING &&
                  !needToDecodeAudio &&
                  !needToDecodeVideo &&
                  !IsPlaying();

  if (needToDecodeAudio) {
    EnsureAudioDecodeTaskQueued();
  }
  if (needToDecodeVideo) {
    EnsureVideoDecodeTaskQueued();
  }

  SAMPLE_LOG("DispatchDecodeTasksIfNeeded needAudio=%d dispAudio=%d needVideo=%d dispVid=%d needIdle=%d",
             needToDecodeAudio, mAudioRequestPending,
             needToDecodeVideo, mVideoRequestPending,
             needIdle);

  if (needIdle) {
    RefPtr<nsIRunnable> event = NS_NewRunnableMethod(
        this, &MediaDecoderStateMachine::SetReaderIdle);
    nsresult rv = mDecodeTaskQueue->Dispatch(event.forget());
    if (NS_FAILED(rv) && mState != DECODER_STATE_SHUTDOWN) {
      DECODER_WARN("Failed to dispatch event to set decoder idle state");
    }
  }
}

nsresult
MediaDecoderStateMachine::EnqueueDecodeSeekTask()
{
  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine or decode thread.");
  AssertCurrentThreadInMonitor();

  if (mState != DECODER_STATE_SEEKING ||
      !mSeekTarget.IsValid() ||
      mCurrentSeekTarget.IsValid()) {
    return NS_OK;
  }
  mCurrentSeekTarget = mSeekTarget;
  mSeekTarget.Reset();
  mDropAudioUntilNextDiscontinuity = HasAudio();
  mDropVideoUntilNextDiscontinuity = HasVideo();

  RefPtr<nsIRunnable> task(
    NS_NewRunnableMethod(this, &MediaDecoderStateMachine::DecodeSeek));
  nsresult rv = mDecodeTaskQueue->Dispatch(task);
  if (NS_FAILED(rv)) {
    DECODER_WARN("Dispatch DecodeSeek task failed.");
    mCurrentSeekTarget.Reset();
    DecodeError();
  }
  return rv;
}

nsresult
MediaDecoderStateMachine::DispatchAudioDecodeTaskIfNeeded()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine or decode thread.");

  if (NeedToDecodeAudio()) {
    return EnsureAudioDecodeTaskQueued();
  }

  return NS_OK;
}

nsresult
MediaDecoderStateMachine::EnsureAudioDecodeTaskQueued()
{
  AssertCurrentThreadInMonitor();
  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine or decode thread.");

  SAMPLE_LOG("EnsureAudioDecodeTaskQueued isDecoding=%d dispatched=%d",
              IsAudioDecoding(), mAudioRequestPending);

  if (mState >= DECODER_STATE_COMPLETED) {
    return NS_OK;
  }

  MOZ_ASSERT(mState > DECODER_STATE_DECODING_METADATA);

  if (IsAudioDecoding() && !mAudioRequestPending) {
    RefPtr<nsIRunnable> task(
      NS_NewRunnableMethod(this, &MediaDecoderStateMachine::DecodeAudio));
    nsresult rv = mDecodeTaskQueue->Dispatch(task);
    if (NS_SUCCEEDED(rv)) {
      mAudioRequestPending = true;
    } else {
      DECODER_WARN("Failed to dispatch task to decode audio");
    }
  }

  return NS_OK;
}

nsresult
MediaDecoderStateMachine::DispatchVideoDecodeTaskIfNeeded()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine or decode thread.");

  if (NeedToDecodeVideo()) {
    return EnsureVideoDecodeTaskQueued();
  }

  return NS_OK;
}

nsresult
MediaDecoderStateMachine::EnsureVideoDecodeTaskQueued()
{
  AssertCurrentThreadInMonitor();

  SAMPLE_LOG("EnsureVideoDecodeTaskQueued isDecoding=%d dispatched=%d",
             IsVideoDecoding(), mVideoRequestPending);

  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine or decode thread.");

  if (mState >= DECODER_STATE_COMPLETED) {
    return NS_OK;
  }

  MOZ_ASSERT(mState > DECODER_STATE_DECODING_METADATA);

  if (IsVideoDecoding() && !mVideoRequestPending) {
    RefPtr<nsIRunnable> task(
      NS_NewRunnableMethod(this, &MediaDecoderStateMachine::DecodeVideo));
    nsresult rv = mDecodeTaskQueue->Dispatch(task);
    if (NS_SUCCEEDED(rv)) {
      mVideoRequestPending = true;
    } else {
      DECODER_WARN("Failed to dispatch task to decode video");
    }
  }

  return NS_OK;
}

nsresult
MediaDecoderStateMachine::StartAudioThread()
{
  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine or decode thread.");
  AssertCurrentThreadInMonitor();
  if (mAudioCaptured) {
    NS_ASSERTION(mStopAudioThread, "mStopAudioThread must always be true if audio is captured");
    return NS_OK;
  }

  mStopAudioThread = false;
  if (HasAudio() && !mAudioSink) {
    mAudioCompleted = false;
    mAudioSink = new AudioSink(this,
                               mAudioStartTime, mInfo.mAudio, mDecoder->GetAudioChannel());
    nsresult rv = mAudioSink->Init();
    if (NS_FAILED(rv)) {
      DECODER_WARN("Changed state to SHUTDOWN because audio sink initialization failed");
      mState = DECODER_STATE_SHUTDOWN;
      mScheduler->ScheduleAndShutdown();
      return rv;
    }

    mAudioSink->SetVolume(mVolume);
    mAudioSink->SetPlaybackRate(mPlaybackRate);
    mAudioSink->SetPreservesPitch(mPreservesPitch);
  }
  return NS_OK;
}

int64_t MediaDecoderStateMachine::AudioDecodedUsecs()
{
  NS_ASSERTION(HasAudio(),
               "Should only call AudioDecodedUsecs() when we have audio");
  // The amount of audio we have decoded is the amount of audio data we've
  // already decoded and pushed to the hardware, plus the amount of audio
  // data waiting to be pushed to the hardware.
  int64_t pushed = (mAudioEndTime != -1) ? (mAudioEndTime - GetMediaTime()) : 0;
  return pushed + AudioQueue().Duration();
}

bool MediaDecoderStateMachine::HasLowDecodedData(int64_t aAudioUsecs)
{
  AssertCurrentThreadInMonitor();
  // We consider ourselves low on decoded data if we're low on audio,
  // provided we've not decoded to the end of the audio stream, or
  // if we're low on video frames, provided
  // we've not decoded to the end of the video stream.
  return ((IsAudioDecoding() && AudioDecodedUsecs() < aAudioUsecs) ||
         (IsVideoDecoding() &&
          static_cast<uint32_t>(VideoQueue().GetSize()) < LOW_VIDEO_FRAMES));
}

bool MediaDecoderStateMachine::HasLowUndecodedData()
{
  return HasLowUndecodedData(mLowDataThresholdUsecs);
}

bool MediaDecoderStateMachine::HasLowUndecodedData(double aUsecs)
{
  AssertCurrentThreadInMonitor();
  NS_ASSERTION(mState > DECODER_STATE_DECODING_METADATA,
               "Must have loaded metadata for GetBuffered() to work");

  bool reliable;
  double bytesPerSecond = mDecoder->ComputePlaybackRate(&reliable);
  if (!reliable) {
    // Default to assuming we have enough
    return false;
  }

  MediaResource* stream = mDecoder->GetResource();
  int64_t currentPos = stream->Tell();
  int64_t requiredPos = currentPos + int64_t((aUsecs/1000000.0)*bytesPerSecond);
  int64_t length = stream->GetLength();
  if (length >= 0) {
    requiredPos = std::min(requiredPos, length);
  }

  return stream->GetCachedDataEnd(currentPos) < requiredPos;
}

void
MediaDecoderStateMachine::DecodeError()
{
  AssertCurrentThreadInMonitor();
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");

  if (mState == DECODER_STATE_SHUTDOWN) {
    // Already shutdown.
    return;
  }

  // Change state to shutdown before sending error report to MediaDecoder
  // and the HTMLMediaElement, so that our pipeline can start exiting
  // cleanly during the sync dispatch below.
  DECODER_WARN("Decode error, changed state to SHUTDOWN due to error");
  mState = DECODER_STATE_SHUTDOWN;
  mScheduler->ScheduleAndShutdown();
  mDecoder->GetReentrantMonitor().NotifyAll();

  // Dispatch the event to call DecodeError synchronously. This ensures
  // we're in shutdown state by the time we exit the decode thread.
  // If we just moved to shutdown state here on the decode thread, we may
  // cause the state machine to shutdown/free memory without closing its
  // media stream properly, and we'll get callbacks from the media stream
  // causing a crash.
 {
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(mDecoder, &MediaDecoder::DecodeError);
    ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
    NS_DispatchToMainThread(event, NS_DISPATCH_SYNC);
  }
}

void
MediaDecoderStateMachine::CallDecodeMetadata()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  if (mState != DECODER_STATE_DECODING_METADATA) {
    return;
  }
  if (NS_FAILED(DecodeMetadata())) {
    DECODER_WARN("Decode metadata failed, shutting down decoder");
    DecodeError();
  }
}

nsresult MediaDecoderStateMachine::DecodeMetadata()
{
  AssertCurrentThreadInMonitor();
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  MOZ_ASSERT(mState == DECODER_STATE_DECODING_METADATA);
  DECODER_LOG("Decoding Media Headers");

  nsresult res;
  MediaInfo info;
  {
    ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
    res = mReader->ReadMetadata(&info, getter_Transfers(mMetadataTags));
  }

  if (NS_SUCCEEDED(res)) {
    if (mState == DECODER_STATE_DECODING_METADATA &&
        mReader->IsWaitingMediaResources()) {
      // change state to DECODER_STATE_WAIT_FOR_RESOURCES
      StartWaitForResources();
      // affect values only if ReadMetadata succeeds
      return NS_OK;
    }
  }

  if (NS_SUCCEEDED(res)) {
    mDecoder->SetMediaSeekable(mReader->IsMediaSeekable());
  }

  mInfo = info;

  if (NS_FAILED(res) || (!info.HasValidMedia())) {
    DECODER_WARN("ReadMetadata failed, res=%x HasValidMedia=%d", res, info.HasValidMedia());
    return NS_ERROR_FAILURE;
  }
  mDecoder->StartProgressUpdates();
  mGotDurationFromMetaData = (GetDuration() != -1);

  if (HasAudio()) {
    RefPtr<nsIRunnable> decodeTask(
      NS_NewRunnableMethod(this, &MediaDecoderStateMachine::DispatchAudioDecodeTaskIfNeeded));
    AudioQueue().AddPopListener(decodeTask, mDecodeTaskQueue);
  }
  if (HasVideo()) {
    RefPtr<nsIRunnable> decodeTask(
      NS_NewRunnableMethod(this, &MediaDecoderStateMachine::DispatchVideoDecodeTaskIfNeeded));
    VideoQueue().AddPopListener(decodeTask, mDecodeTaskQueue);
  }

  if (mScheduler->IsRealTime()) {
    SetStartTime(0);
    res = FinishDecodeMetadata();
    NS_ENSURE_SUCCESS(res, res);
  } else {
    if (HasAudio()) {
      ReentrantMonitorAutoExit unlock(mDecoder->GetReentrantMonitor());
      mReader->RequestAudioData();
    }
    if (HasVideo()) {
      ReentrantMonitorAutoExit unlock(mDecoder->GetReentrantMonitor());
      mReader->RequestVideoData(false, 0);
    }
  }

  return NS_OK;
}

nsresult
MediaDecoderStateMachine::FinishDecodeMetadata()
{
  AssertCurrentThreadInMonitor();
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  DECODER_LOG("FinishDecodeMetadata");

  if (mState == DECODER_STATE_SHUTDOWN) {
    return NS_ERROR_FAILURE;
  }

  if (!mScheduler->IsRealTime()) {

    const VideoData* v = VideoQueue().PeekFront();
    const AudioData* a = AudioQueue().PeekFront();

    int64_t startTime = std::min<int64_t>(a ? a->mTime : INT64_MAX,
                                          v ? v->mTime : INT64_MAX);
    if (startTime == INT64_MAX) {
      startTime = 0;
    }
    DECODER_LOG("DecodeMetadata first video frame start %lld", v ? v->mTime : -1);
    DECODER_LOG("DecodeMetadata first audio frame start %lld", a ? a->mTime : -1);
    SetStartTime(startTime);
    if (VideoQueue().GetSize()) {
      ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
      RenderVideoFrame(VideoQueue().PeekFront(), TimeStamp::Now());
    }
  }

  NS_ASSERTION(mStartTime != -1, "Must have start time");
  MOZ_ASSERT((!HasVideo() && !HasAudio()) ||
               !(mDecoder->IsMediaSeekable() && mDecoder->IsTransportSeekable()) ||
               mEndTime != -1,
             "Active seekable media should have end time");
  MOZ_ASSERT(!(mDecoder->IsMediaSeekable() && mDecoder->IsTransportSeekable()) ||
               GetDuration() != -1,
             "Seekable media should have duration");
  DECODER_LOG("Media goes from %lld to %lld (duration %lld) "
              "transportSeekable=%d, mediaSeekable=%d",
              mStartTime, mEndTime, GetDuration(),
              mDecoder->IsTransportSeekable(), mDecoder->IsMediaSeekable());

  if (HasAudio() && !HasVideo()) {
    // We're playing audio only. We don't need to worry about slow video
    // decodes causing audio underruns, so don't buffer so much audio in
    // order to reduce memory usage.
    mAmpleAudioThresholdUsecs /= NO_VIDEO_AMPLE_AUDIO_DIVISOR;
    mLowAudioThresholdUsecs /= NO_VIDEO_AMPLE_AUDIO_DIVISOR;
  }

  // Inform the element that we've loaded the metadata and the first frame.
  nsAutoPtr<MediaInfo> info(new MediaInfo());
  *info = mInfo;
  nsCOMPtr<nsIRunnable> metadataLoadedEvent =
    new MetadataEventRunner(mDecoder, info.forget(), mMetadataTags.forget());
  NS_DispatchToMainThread(metadataLoadedEvent, NS_DISPATCH_NORMAL);

  if (mState == DECODER_STATE_DECODING_METADATA) {
    DECODER_LOG("Changed state from DECODING_METADATA to DECODING");
    StartDecoding();
  }

  // For very short media the metadata decode can decode the entire media.
  // So we need to check if this has occurred, else our decode pipeline won't
  // run (since it doesn't need to) and we won't detect end of stream.
  CheckIfDecodeComplete();

  if ((mState == DECODER_STATE_DECODING || mState == DECODER_STATE_COMPLETED) &&
      mDecoder->GetState() == MediaDecoder::PLAY_STATE_PLAYING &&
      !IsPlaying())
  {
    StartPlayback();
  }

  return NS_OK;
}

void MediaDecoderStateMachine::DecodeSeek()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  if (mState != DECODER_STATE_SEEKING) {
    return;
  }

  // During the seek, don't have a lock on the decoder state,
  // otherwise long seek operations can block the main thread.
  // The events dispatched to the main thread are SYNC calls.
  // These calls are made outside of the decode monitor lock so
  // it is safe for the main thread to makes calls that acquire
  // the lock since it won't deadlock. We check the state when
  // acquiring the lock again in case shutdown has occurred
  // during the time when we didn't have the lock.
  int64_t seekTime = mCurrentSeekTarget.mTime;
  mDecoder->StopProgressUpdates();

  bool currentTimeChanged = false;
  mCurrentTimeBeforeSeek = GetMediaTime();
  if (mCurrentTimeBeforeSeek != seekTime) {
    currentTimeChanged = true;
    // Stop playback now to ensure that while we're outside the monitor
    // dispatching SeekingStarted, playback doesn't advance and mess with
    // mCurrentFrameTime that we've setting to seekTime here.
    StopPlayback();
    UpdatePlaybackPositionInternal(seekTime);
  }

  // SeekingStarted will do a UpdateReadyStateForData which will
  // inform the element and its users that we have no frames
  // to display
  {
    ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
    nsCOMPtr<nsIRunnable> startEvent =
      NS_NewRunnableMethod(mDecoder, &MediaDecoder::SeekingStarted);
    NS_DispatchToMainThread(startEvent, NS_DISPATCH_SYNC);
  }
  if (mState != DECODER_STATE_SEEKING) {
    // May have shutdown while we released the monitor.
    return;
  }

  if (!currentTimeChanged) {
    DECODER_LOG("Seek !currentTimeChanged...");
    mDecodeToSeekTarget = false;
    nsresult rv = mDecodeTaskQueue->Dispatch(
      NS_NewRunnableMethod(this, &MediaDecoderStateMachine::SeekCompleted));
    if (NS_FAILED(rv)) {
      DecodeError();
    }
  } else {
    // The seek target is different than the current playback position,
    // we'll need to seek the playback position, so shutdown our decode
    // thread and audio sink.
    StopAudioThread();
    ResetPlayback();

    nsresult res;
    {
      ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
      // We must not hold the state machine monitor while we call into
      // the reader, since it could do I/O or deadlock some other way.
      res = mReader->ResetDecode();
      if (NS_SUCCEEDED(res)) {
        res = mReader->Seek(seekTime,
                            mStartTime,
                            mEndTime,
                            mCurrentTimeBeforeSeek);
      }
    }
    if (NS_FAILED(res)) {
      DecodeError();
      return;
    }

    // We must decode the first samples of active streams, so we can determine
    // the new stream time. So dispatch tasks to do that.
    mDecodeToSeekTarget = true;
    DispatchDecodeTasksIfNeeded();
  }
}

void
MediaDecoderStateMachine::SeekCompleted()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  // We must reset the seek target when exiting this function, but not
  // before, as if we dropped the monitor in any function called here,
  // we may begin a new seek on the state machine thread, and be in
  // an inconsistent state.
  AutoSetOnScopeExit<SeekTarget> reset(mCurrentSeekTarget, SeekTarget());

  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  if (mState != DECODER_STATE_SEEKING) {
    return;
  }

  int64_t seekTime = mCurrentSeekTarget.mTime;
  int64_t newCurrentTime = mCurrentSeekTarget.mTime;

  // Setup timestamp state.
  VideoData* video = VideoQueue().PeekFront();
  if (seekTime == mEndTime) {
    newCurrentTime = mAudioStartTime = seekTime;
  } else if (HasAudio()) {
    AudioData* audio = AudioQueue().PeekFront();
    newCurrentTime = mAudioStartTime = audio ? audio->mTime : seekTime;
  } else {
    newCurrentTime = video ? video->mTime : seekTime;
  }
  mPlayDuration = newCurrentTime - mStartTime;

  if (HasVideo()) {
    if (video) {
      {
        ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
        RenderVideoFrame(video, TimeStamp::Now());
      }
      nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethod(mDecoder, &MediaDecoder::Invalidate);
      NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
    }
  }

  MOZ_ASSERT(mState != DECODER_STATE_DECODING_NONE);

  mDecoder->StartProgressUpdates();
  if (mState == DECODER_STATE_DECODING_METADATA ||
      mState == DECODER_STATE_DORMANT ||
      mState == DECODER_STATE_SHUTDOWN) {
    return;
  }

  // Change state to DECODING or COMPLETED now. SeekingStopped will
  // call MediaDecoderStateMachine::Seek to reset our state to SEEKING
  // if we need to seek again.

  nsCOMPtr<nsIRunnable> stopEvent;
  bool isLiveStream = mDecoder->GetResource()->GetLength() == -1;
  if (GetMediaTime() == mEndTime && !isLiveStream) {
    // Seeked to end of media, move to COMPLETED state. Note we don't do
    // this if we're playing a live stream, since the end of media will advance
    // once we download more data!
    DECODER_LOG("Changed state from SEEKING (to %lld) to COMPLETED", seekTime);
    stopEvent = NS_NewRunnableMethod(mDecoder, &MediaDecoder::SeekingStoppedAtEnd);
    // Explicitly set our state so we don't decode further, and so
    // we report playback ended to the media element.
    mState = DECODER_STATE_COMPLETED;
    DispatchDecodeTasksIfNeeded();
  } else {
    DECODER_LOG("Changed state from SEEKING (to %lld) to DECODING", seekTime);
    stopEvent = NS_NewRunnableMethod(mDecoder, &MediaDecoder::SeekingStopped);
    StartDecoding();
  }

  // Ensure timestamps are up to date.
  UpdatePlaybackPositionInternal(newCurrentTime);
  if (mDecoder->GetDecodedStream()) {
    SetSyncPointForMediaStream();
  }

  // Try to decode another frame to detect if we're at the end...
  DECODER_LOG("Seek completed, mCurrentFrameTime=%lld", mCurrentFrameTime);

  // Prevent changes in playback position before 'seeked' is fired for we
  // expect currentTime equals seek target in 'seeked' callback.
  mScheduler->FreezeScheduling();
  {
    ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
    NS_DispatchToMainThread(stopEvent, NS_DISPATCH_SYNC);
  }

  // Reset quick buffering status. This ensures that if we began the
  // seek while quick-buffering, we won't bypass quick buffering mode
  // if we need to buffer after the seek.
  mQuickBuffering = false;

  ScheduleStateMachine();
  mScheduler->ThawScheduling();
}

// Runnable to dispose of the decoder and state machine on the main thread.
class nsDecoderDisposeEvent : public nsRunnable {
public:
  nsDecoderDisposeEvent(already_AddRefed<MediaDecoder> aDecoder,
                        already_AddRefed<MediaDecoderStateMachine> aStateMachine)
    : mDecoder(aDecoder), mStateMachine(aStateMachine) {}
  NS_IMETHOD Run() {
    NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");
    MOZ_ASSERT(mStateMachine);
    MOZ_ASSERT(mDecoder);
    mStateMachine->BreakCycles();
    mDecoder->BreakCycles();
    mStateMachine = nullptr;
    mDecoder = nullptr;
    return NS_OK;
  }
private:
  nsRefPtr<MediaDecoder> mDecoder;
  nsRefPtr<MediaDecoderStateMachine> mStateMachine;
};

// Runnable which dispatches an event to the main thread to dispose of the
// decoder and state machine. This runs on the state machine thread after
// the state machine has shutdown, and all events for that state machine have
// finished running.
class nsDispatchDisposeEvent : public nsRunnable {
public:
  nsDispatchDisposeEvent(MediaDecoder* aDecoder,
                         MediaDecoderStateMachine* aStateMachine)
    : mDecoder(aDecoder), mStateMachine(aStateMachine) {}
  NS_IMETHOD Run() {
    NS_DispatchToMainThread(new nsDecoderDisposeEvent(mDecoder.forget(),
                                                      mStateMachine.forget()));
    return NS_OK;
  }
private:
  nsRefPtr<MediaDecoder> mDecoder;
  nsRefPtr<MediaDecoderStateMachine> mStateMachine;
};

nsresult MediaDecoderStateMachine::RunStateMachine()
{
  AssertCurrentThreadInMonitor();

  MediaResource* resource = mDecoder->GetResource();
  NS_ENSURE_TRUE(resource, NS_ERROR_NULL_POINTER);

  switch (mState) {
    case DECODER_STATE_SHUTDOWN: {
      if (IsPlaying()) {
        StopPlayback();
      }

      FlushDecoding();

      // Put a task in the decode queue to shutdown the reader.
      RefPtr<nsIRunnable> task;
      task = NS_NewRunnableMethod(mReader, &MediaDecoderReader::Shutdown);
      mDecodeTaskQueue->Dispatch(task);

      StopAudioThread();
      // If mAudioSink is non-null after StopAudioThread completes, we are
      // running in a nested event loop waiting for Shutdown() on
      // mAudioSink to complete.  Return to the event loop and let it
      // finish processing before continuing with shutdown.
      if (mAudioSink) {
        MOZ_ASSERT(mStopAudioThread);
        return NS_OK;
      }

      {
        // Wait for the thread decoding to exit.
        ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
        mDecodeTaskQueue->Shutdown();
        mDecodeTaskQueue = nullptr;
      }

      // The reader's listeners hold references to the state machine,
      // creating a cycle which keeps the state machine and its shared
      // thread pools alive. So break it here.
      AudioQueue().ClearListeners();
      VideoQueue().ClearListeners();

      // Now that those threads are stopped, there's no possibility of
      // mPendingWakeDecoder being needed again. Revoke it.
      mPendingWakeDecoder = nullptr;

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
      GetStateMachineThread()->Dispatch(
        new nsDispatchDisposeEvent(mDecoder, this), NS_DISPATCH_NORMAL);

      DECODER_LOG("SHUTDOWN OK");
      return NS_OK;
    }

    case DECODER_STATE_DORMANT: {
      if (IsPlaying()) {
        StopPlayback();
      }
      FlushDecoding();
      StopAudioThread();
      // Now that those threads are stopped, there's no possibility of
      // mPendingWakeDecoder being needed again. Revoke it.
      mPendingWakeDecoder = nullptr;
      {
        ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
        // Wait for the thread decoding, if any, to exit.
        mDecodeTaskQueue->AwaitIdle();
        mReader->ReleaseMediaResources();
      }
      return NS_OK;
    }

    case DECODER_STATE_WAIT_FOR_RESOURCES: {
      return NS_OK;
    }

    case DECODER_STATE_DECODING_NONE: {
      mState = DECODER_STATE_DECODING_METADATA;
      // Ensure we have a decode thread to decode metadata.
      return EnqueueDecodeMetadataTask();
    }

    case DECODER_STATE_DECODING_METADATA: {
      return NS_OK;
    }

    case DECODER_STATE_DECODING: {
      if (mDecoder->GetState() != MediaDecoder::PLAY_STATE_PLAYING &&
          IsPlaying())
      {
        // We're playing, but the element/decoder is in paused state. Stop
        // playing!
        StopPlayback();
      }

      if (mDecoder->GetState() == MediaDecoder::PLAY_STATE_PLAYING &&
          !IsPlaying()) {
        // We are playing, but the state machine does not know it yet. Tell it
        // that it is, so that the clock can be properly queried.
        StartPlayback();
      }

      AdvanceFrame();
      NS_ASSERTION(mDecoder->GetState() != MediaDecoder::PLAY_STATE_PLAYING ||
                   IsStateMachineScheduled() ||
                   mPlaybackRate == 0.0, "Must have timer scheduled");
      return NS_OK;
    }

    case DECODER_STATE_BUFFERING: {
      TimeStamp now = TimeStamp::Now();
      NS_ASSERTION(!mBufferingStart.IsNull(), "Must know buffering start time.");

      // We will remain in the buffering state if we've not decoded enough
      // data to begin playback, or if we've not downloaded a reasonable
      // amount of data inside our buffering time.
      TimeDuration elapsed = now - mBufferingStart;
      bool isLiveStream = resource->GetLength() == -1;
      if ((isLiveStream || !mDecoder->CanPlayThrough()) &&
            elapsed < TimeDuration::FromSeconds(mBufferingWait * mPlaybackRate) &&
            (mQuickBuffering ? HasLowDecodedData(QUICK_BUFFERING_LOW_DATA_USECS)
                            : HasLowUndecodedData(mBufferingWait * USECS_PER_S)) &&
            !mDecoder->IsDataCachedToEndOfResource() &&
            !resource->IsSuspended())
      {
        DECODER_LOG("Buffering: wait %ds, timeout in %.3lfs %s",
                    mBufferingWait, mBufferingWait - elapsed.ToSeconds(),
                    (mQuickBuffering ? "(quick exit)" : ""));
        ScheduleStateMachine(USECS_PER_S);
        return NS_OK;
      } else {
        DECODER_LOG("Changed state from BUFFERING to DECODING");
        DECODER_LOG("Buffered for %.3lfs", (now - mBufferingStart).ToSeconds());
        StartDecoding();
      }

      // Notify to allow blocked decoder thread to continue
      mDecoder->GetReentrantMonitor().NotifyAll();
      UpdateReadyState();
      if (mDecoder->GetState() == MediaDecoder::PLAY_STATE_PLAYING &&
          !IsPlaying())
      {
        StartPlayback();
      }
      NS_ASSERTION(IsStateMachineScheduled(), "Must have timer scheduled");
      return NS_OK;
    }

    case DECODER_STATE_SEEKING: {
      return EnqueueDecodeSeekTask();
    }

    case DECODER_STATE_COMPLETED: {
      // Play the remaining media. We want to run AdvanceFrame() at least
      // once to ensure the current playback position is advanced to the
      // end of the media, and so that we update the readyState.
      if (VideoQueue().GetSize() > 0 ||
          (HasAudio() && !mAudioCompleted) ||
          (mDecoder->GetDecodedStream() && !mDecoder->GetDecodedStream()->IsFinished()))
      {
        AdvanceFrame();
        NS_ASSERTION(mDecoder->GetState() != MediaDecoder::PLAY_STATE_PLAYING ||
                     mPlaybackRate == 0 ||
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

      StopAudioThread();
      // When we're decoding to a stream, the stream's main-thread finish signal
      // will take care of calling MediaDecoder::PlaybackEnded.
      if (mDecoder->GetState() == MediaDecoder::PLAY_STATE_PLAYING &&
          !mDecoder->GetDecodedStream()) {
        int64_t videoTime = HasVideo() ? mVideoFrameEndTime : 0;
        int64_t clockTime = std::max(mEndTime, videoTime);
        UpdatePlaybackPosition(clockTime);

        {
          // Wait for the state change is completed in the main thread,
          // otherwise we might see |mDecoder->GetState() == MediaDecoder::PLAY_STATE_PLAYING|
          // in next loop and send |MediaDecoder::PlaybackEnded| again to trigger 'ended'
          // event twice in the media element.
          ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
          nsCOMPtr<nsIRunnable> event =
            NS_NewRunnableMethod(mDecoder, &MediaDecoder::PlaybackEnded);
          NS_DispatchToMainThread(event, NS_DISPATCH_SYNC);
        }
      }
      return NS_OK;
    }
  }

  return NS_OK;
}

void
MediaDecoderStateMachine::FlushDecoding()
{
  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine or decode thread.");
  mDecoder->GetReentrantMonitor().AssertNotCurrentThreadIn();

  {
    // Put a task in the decode queue to abort any decoding operations.
    // The reader is not supposed to put any tasks to deliver samples into
    // the queue after this runs (unless we request another sample from it).
    RefPtr<nsIRunnable> task;
    task = NS_NewRunnableMethod(mReader, &MediaDecoderReader::ResetDecode);

    // Wait for the ResetDecode to run and for the decoder to abort
    // decoding operations and run any pending callbacks. This is
    // important, as we don't want any pending tasks posted to the task
    // queue by the reader to deliver any samples after we've posted the
    // reader Shutdown() task below, as the sample-delivery tasks will
    // keep video frames alive until after we've called Reader::Shutdown(),
    // and shutdown on B2G will fail as there are outstanding video frames
    // alive.
    ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
    mDecodeTaskQueue->FlushAndDispatch(task);
  }

  // We must reset playback so that all references to frames queued
  // in the state machine are dropped, else subsequent calls to Shutdown()
  // or ReleaseMediaResources() can fail on B2G.
  ResetPlayback();
}

void MediaDecoderStateMachine::RenderVideoFrame(VideoData* aData,
                                                TimeStamp aTarget)
{
  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine or decode thread.");
  mDecoder->GetReentrantMonitor().AssertNotCurrentThreadIn();

  if (aData->mDuplicate) {
    return;
  }

  VERBOSE_LOG("playing video frame %lld", aData->mTime);

  VideoFrameContainer* container = mDecoder->GetVideoFrameContainer();
  if (container) {
    container->SetCurrentFrame(ThebesIntSize(aData->mDisplay), aData->mImage,
                               aTarget);
  }
}

int64_t
MediaDecoderStateMachine::GetAudioClock()
{
  // We must hold the decoder monitor while using the audio stream off the
  // audio sink to ensure that it doesn't get destroyed on the audio sink
  // while we're using it.
  AssertCurrentThreadInMonitor();
  if (!HasAudio() || mAudioCaptured)
    return -1;
  if (!mAudioSink) {
    // Audio sink hasn't played any data yet.
    return mAudioStartTime;
  }
  int64_t t = mAudioSink->GetPosition();
  return (t == -1) ? -1 : t + mAudioStartTime;
}

int64_t MediaDecoderStateMachine::GetVideoStreamPosition()
{
  AssertCurrentThreadInMonitor();

  if (!IsPlaying()) {
    return mPlayDuration + mStartTime;
  }

  // Time elapsed since we started playing.
  int64_t delta = DurationToUsecs(TimeStamp::Now() - mPlayStartTime);
  // Take playback rate into account.
  delta *= mPlaybackRate;
  return mStartTime + mPlayDuration + delta;
}

int64_t MediaDecoderStateMachine::GetClock()
{
  AssertCurrentThreadInMonitor();

  // Determine the clock time. If we've got audio, and we've not reached
  // the end of the audio, use the audio clock. However if we've finished
  // audio, or don't have audio, use the system clock. If our output is being
  // fed to a MediaStream, use that stream as the source of the clock.
  int64_t clock_time = -1;
  DecodedStreamData* stream = mDecoder->GetDecodedStream();
  if (!IsPlaying()) {
    clock_time = mPlayDuration + mStartTime;
  } else if (stream) {
    clock_time = GetCurrentTimeViaMediaStreamSync();
  } else {
    int64_t audio_time = GetAudioClock();
    if (HasAudio() && !mAudioCompleted && audio_time != -1) {
      clock_time = audio_time;
      // Resync against the audio clock, while we're trusting the
      // audio clock. This ensures no "drift", particularly on Linux.
      mPlayDuration = clock_time - mStartTime;
      SetPlayStartTime(TimeStamp::Now());
    } else {
      // Audio is disabled on this system. Sync to the system clock.
      clock_time = GetVideoStreamPosition();
      // Ensure the clock can never go backwards.
      NS_ASSERTION(mCurrentFrameTime <= clock_time || mPlaybackRate <= 0,
          "Clock should go forwards if the playback rate is > 0.");
    }
  }
  return clock_time;
}

void MediaDecoderStateMachine::AdvanceFrame()
{
  NS_ASSERTION(OnStateMachineThread(), "Should be on state machine thread.");
  AssertCurrentThreadInMonitor();
  NS_ASSERTION(!HasAudio() || mAudioStartTime != -1,
               "Should know audio start time if we have audio.");

  if (mDecoder->GetState() != MediaDecoder::PLAY_STATE_PLAYING) {
    return;
  }

  // If playbackRate is 0.0, we should stop the progress, but not be in paused
  // state, per spec.
  if (mPlaybackRate == 0.0) {
    return;
  }

  int64_t clock_time = GetClock();
  // Skip frames up to the frame at the playback position, and figure out
  // the time remaining until it's time to display the next frame.
  int64_t remainingTime = AUDIO_DURATION_USECS;
  NS_ASSERTION(clock_time >= mStartTime, "Should have positive clock time.");
  nsAutoPtr<VideoData> currentFrame;
  if (VideoQueue().GetSize() > 0) {
    VideoData* frame = VideoQueue().PeekFront();
#ifdef PR_LOGGING
    int32_t droppedFrames = 0;
#endif
    while (mScheduler->IsRealTime() || clock_time >= frame->mTime) {
      mVideoFrameEndTime = frame->GetEndTime();
#ifdef PR_LOGGING
      if (currentFrame) {
        VERBOSE_LOG("discarding video frame mTime=%lld clock_time=%lld (%d so far)",
                    currentFrame->mTime, clock_time, ++droppedFrames);
      }
#endif
      currentFrame = frame;
      VideoQueue().PopFront();
      // Notify the decode thread that the video queue's buffers may have
      // free'd up space for more frames.
      mDecoder->GetReentrantMonitor().NotifyAll();
      OnPlaybackOffsetUpdate(frame->mOffset);
      if (VideoQueue().GetSize() == 0)
        break;
      frame = VideoQueue().PeekFront();
    }
    // Current frame has already been presented, wait until it's time to
    // present the next frame.
    if (frame && !currentFrame) {
      int64_t now = IsPlaying() ? clock_time : mStartTime + mPlayDuration;

      remainingTime = frame->mTime - now;
    }
  }

  // Check to see if we don't have enough data to play up to the next frame.
  // If we don't, switch to buffering mode.
  MediaResource* resource = mDecoder->GetResource();
  if (mState == DECODER_STATE_DECODING &&
      mDecoder->GetState() == MediaDecoder::PLAY_STATE_PLAYING &&
      HasLowDecodedData(remainingTime + EXHAUSTED_DATA_MARGIN_USECS) &&
      !mDecoder->IsDataCachedToEndOfResource() &&
      !resource->IsSuspended()) {
    if (JustExitedQuickBuffering() || HasLowUndecodedData()) {
      if (currentFrame) {
        VideoQueue().PushFront(currentFrame.forget());
      }
      StartBuffering();
      // Don't go straight back to the state machine loop since that might
      // cause us to start decoding again and we could flip-flop between
      // decoding and quick-buffering.
      ScheduleStateMachine(USECS_PER_S);
      return;
    }
  }

  // We've got enough data to keep playing until at least the next frame.
  // Start playing now if need be.
  if (!IsPlaying() && ((mFragmentEndTime >= 0 && clock_time < mFragmentEndTime) || mFragmentEndTime < 0)) {
    StartPlayback();
  }

  if (currentFrame) {
    // Decode one frame and display it.
    TimeStamp presTime = mPlayStartTime - UsecsToDuration(mPlayDuration) +
                          UsecsToDuration(currentFrame->mTime - mStartTime);
    NS_ASSERTION(currentFrame->mTime >= mStartTime, "Should have positive frame time");
    // Filter out invalid frames by checking the frame time. FrameTime could be
    // zero if it's a initial frame.
    int64_t frameTime = currentFrame->mTime - mStartTime;
    if (frameTime > 0  || (frameTime == 0 && mPlayDuration == 0)) {
      ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
      // If we have video, we want to increment the clock in steps of the frame
      // duration.
      RenderVideoFrame(currentFrame, presTime);
    }
    // If we're no longer playing after dropping and reacquiring the lock,
    // playback must've been stopped on the decode thread (by a seek, for
    // example).  In that case, the current frame is probably out of date.
    if (!IsPlaying()) {
      ScheduleStateMachine();
      return;
    }
    MediaDecoder::FrameStatistics& frameStats = mDecoder->GetFrameStatistics();
    frameStats.NotifyPresentedFrame();
    remainingTime = currentFrame->GetEndTime() - clock_time;
    currentFrame = nullptr;
  }

  // Cap the current time to the larger of the audio and video end time.
  // This ensures that if we're running off the system clock, we don't
  // advance the clock to after the media end time.
  if (mVideoFrameEndTime != -1 || mAudioEndTime != -1) {
    // These will be non -1 if we've displayed a video frame, or played an audio frame.
    clock_time = std::min(clock_time, std::max(mVideoFrameEndTime, mAudioEndTime));
    if (clock_time > GetMediaTime()) {
      // Only update the playback position if the clock time is greater
      // than the previous playback position. The audio clock can
      // sometimes report a time less than its previously reported in
      // some situations, and we need to gracefully handle that.
      UpdatePlaybackPosition(clock_time);
    }
  }

  // If the number of audio/video frames queued has changed, either by
  // this function popping and playing a video frame, or by the audio
  // thread popping and playing an audio frame, we may need to update our
  // ready state. Post an update to do so.
  UpdateReadyState();

  ScheduleStateMachine(remainingTime);
}

nsresult
MediaDecoderStateMachine::DropVideoUpToSeekTarget(VideoData* aSample)
{
  nsAutoPtr<VideoData> video(aSample);
  MOZ_ASSERT(video);
  DECODER_LOG("DropVideoUpToSeekTarget() frame [%lld, %lld] dup=%d",
              video->mTime, video->GetEndTime(), video->mDuplicate);
  const int64_t target = mCurrentSeekTarget.mTime;

  // Duplicate handling: if we're dropping frames up the seek target, we must
  // be wary of Theora duplicate frames. They don't have an image, so if the
  // target frame is in a run of duplicates, we won't have an image to draw
  // after the seek. So store the last frame encountered while dropping, and
  // copy its Image forward onto duplicate frames, so that every frame has
  // an Image.
  if (video->mDuplicate &&
      mFirstVideoFrameAfterSeek &&
      !mFirstVideoFrameAfterSeek->mDuplicate) {
    VideoData* temp =
      VideoData::ShallowCopyUpdateTimestampAndDuration(mFirstVideoFrameAfterSeek,
                                                       video->mTime,
                                                       video->mDuration);
    video = temp;
  }

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
      VideoData* temp = VideoData::ShallowCopyUpdateTimestamp(video, target);
      video = temp;
    }
    mFirstVideoFrameAfterSeek = nullptr;

    DECODER_LOG("DropVideoUpToSeekTarget() found video frame [%lld, %lld] containing target=%lld",
                video->mTime, video->GetEndTime(), target);

    VideoQueue().PushFront(video.forget());

  }
  return NS_OK;
}

nsresult
MediaDecoderStateMachine::DropAudioUpToSeekTarget(AudioData* aSample)
{
  nsAutoPtr<AudioData> audio(aSample);
  MOZ_ASSERT(audio &&
             mCurrentSeekTarget.IsValid() &&
             mCurrentSeekTarget.mType == SeekTarget::Accurate);

  CheckedInt64 startFrame = UsecsToFrames(audio->mTime,
                                          mInfo.mAudio.mRate);
  CheckedInt64 targetFrame = UsecsToFrames(mCurrentSeekTarget.mTime,
                                           mInfo.mAudio.mRate);
  if (!startFrame.isValid() || !targetFrame.isValid()) {
    return NS_ERROR_FAILURE;
  }
  if (startFrame.value() + audio->mFrames <= targetFrame.value()) {
    // Our seek target lies after the frames in this AudioData. Don't
    // push it onto the audio queue, and keep decoding forwards.
    return NS_OK;
  }
  if (startFrame.value() > targetFrame.value()) {
    // The seek target doesn't lie in the audio block just after the last
    // audio frames we've seen which were before the seek target. This
    // could have been the first audio data we've seen after seek, i.e. the
    // seek terminated after the seek target in the audio stream. Just
    // abort the audio decode-to-target, the state machine will play
    // silence to cover the gap. Typically this happens in poorly muxed
    // files.
    DECODER_WARN("Audio not synced after seek, maybe a poorly muxed file?");
    AudioQueue().Push(audio.forget());
    return NS_OK;
  }

  // The seek target lies somewhere in this AudioData's frames, strip off
  // any frames which lie before the seek target, so we'll begin playback
  // exactly at the seek target.
  NS_ASSERTION(targetFrame.value() >= startFrame.value(),
               "Target must at or be after data start.");
  NS_ASSERTION(targetFrame.value() < startFrame.value() + audio->mFrames,
               "Data must end after target.");

  int64_t framesToPrune = targetFrame.value() - startFrame.value();
  if (framesToPrune > audio->mFrames) {
    // We've messed up somehow. Don't try to trim frames, the |frames|
    // variable below will overflow.
    DECODER_WARN("Can't prune more frames that we have!");
    return NS_ERROR_FAILURE;
  }
  uint32_t frames = audio->mFrames - static_cast<uint32_t>(framesToPrune);
  uint32_t channels = audio->mChannels;
  nsAutoArrayPtr<AudioDataValue> audioData(new AudioDataValue[frames * channels]);
  memcpy(audioData.get(),
         audio->mAudioData.get() + (framesToPrune * channels),
         frames * channels * sizeof(AudioDataValue));
  CheckedInt64 duration = FramesToUsecs(frames, mInfo.mAudio.mRate);
  if (!duration.isValid()) {
    return NS_ERROR_FAILURE;
  }
  nsAutoPtr<AudioData> data(new AudioData(audio->mOffset,
                                          mCurrentSeekTarget.mTime,
                                          duration.value(),
                                          frames,
                                          audioData.forget(),
                                          channels,
                                          audio->mRate));
  AudioQueue().PushFront(data.forget());

  return NS_OK;
}

void MediaDecoderStateMachine::SetStartTime(int64_t aStartTimeUsecs)
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  DECODER_LOG("SetStartTime(%lld)", aStartTimeUsecs);
  mStartTime = 0;
  if (aStartTimeUsecs != 0) {
    mStartTime = aStartTimeUsecs;
    if (mGotDurationFromMetaData) {
      NS_ASSERTION(mEndTime != -1,
                   "We should have mEndTime as supplied duration here");
      // We were specified a duration from a Content-Duration HTTP header.
      // Adjust mEndTime so that mEndTime-mStartTime matches the specified
      // duration.
      mEndTime = mStartTime + mEndTime;
    }
  }
  // Set the audio start time to be start of media. If this lies before the
  // first actual audio frame we have, we'll inject silence during playback
  // to ensure the audio starts at the correct time.
  mAudioStartTime = mStartTime;
  DECODER_LOG("Set media start time to %lld", mStartTime);
}

void MediaDecoderStateMachine::UpdateReadyState() {
  AssertCurrentThreadInMonitor();

  MediaDecoderOwner::NextFrameStatus nextFrameStatus = GetNextFrameStatus();
  if (nextFrameStatus == mLastFrameStatus) {
    return;
  }
  mLastFrameStatus = nextFrameStatus;

  /* This is a bit tricky. MediaDecoder::UpdateReadyStateForData will run on
   * the main thread and re-evaluate GetNextFrameStatus there, passing it to
   * HTMLMediaElement::UpdateReadyStateForData. It doesn't use the value of
   * GetNextFrameStatus we computed here, because what we're computing here
   * could be stale by the time MediaDecoder::UpdateReadyStateForData runs.
   * We only compute GetNextFrameStatus here to avoid posting runnables to the main
   * thread unnecessarily.
   */
  nsCOMPtr<nsIRunnable> event;
  event = NS_NewRunnableMethod(mDecoder, &MediaDecoder::UpdateReadyStateForData);
  NS_DispatchToMainThread(event);
}

bool MediaDecoderStateMachine::JustExitedQuickBuffering()
{
  return !mDecodeStartTime.IsNull() &&
    mQuickBuffering &&
    (TimeStamp::Now() - mDecodeStartTime) < TimeDuration::FromMicroseconds(QUICK_BUFFER_THRESHOLD_USECS);
}

void MediaDecoderStateMachine::StartBuffering()
{
  AssertCurrentThreadInMonitor();

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

  // We need to tell the element that buffering has started.
  // We can't just directly send an asynchronous runnable that
  // eventually fires the "waiting" event. The problem is that
  // there might be pending main-thread events, such as "data
  // received" notifications, that mean we're not actually still
  // buffering by the time this runnable executes. So instead
  // we just trigger UpdateReadyStateForData; when it runs, it
  // will check the current state and decide whether to tell
  // the element we're buffering or not.
  UpdateReadyState();
  mState = DECODER_STATE_BUFFERING;
  DECODER_LOG("Changed state from DECODING to BUFFERING, decoded for %.3lfs",
              decodeDuration.ToSeconds());
#ifdef PR_LOGGING
  MediaDecoder::Statistics stats = mDecoder->GetStatistics();
  DECODER_LOG("Playback rate: %.1lfKB/s%s download rate: %.1lfKB/s%s",
              stats.mPlaybackRate/1024, stats.mPlaybackRateReliable ? "" : " (unreliable)",
              stats.mDownloadRate/1024, stats.mDownloadRateReliable ? "" : " (unreliable)");
#endif
}

nsresult MediaDecoderStateMachine::GetBuffered(dom::TimeRanges* aBuffered) {
  MediaResource* resource = mDecoder->GetResource();
  NS_ENSURE_TRUE(resource, NS_ERROR_FAILURE);
  resource->Pin();
  nsresult res = mReader->GetBuffered(aBuffered, mStartTime);
  resource->Unpin();
  return res;
}

void MediaDecoderStateMachine::SetPlayStartTime(const TimeStamp& aTimeStamp)
{
  AssertCurrentThreadInMonitor();
  mPlayStartTime = aTimeStamp;
  if (!mAudioSink) {
    return;
  }
  if (!mPlayStartTime.IsNull()) {
    mAudioSink->StartPlayback();
  } else {
    mAudioSink->StopPlayback();
  }
}

nsresult MediaDecoderStateMachine::CallRunStateMachine()
{
  AssertCurrentThreadInMonitor();
  NS_ASSERTION(OnStateMachineThread(), "Should be on state machine thread.");

  // If audio is being captured, stop the audio sink if it's running
  if (mAudioCaptured) {
    StopAudioThread();
  }

  return RunStateMachine();
}

nsresult MediaDecoderStateMachine::TimeoutExpired(void* aClosure)
{
  MediaDecoderStateMachine* p = static_cast<MediaDecoderStateMachine*>(aClosure);
  return p->CallRunStateMachine();
}

void MediaDecoderStateMachine::ScheduleStateMachineWithLockAndWakeDecoder() {
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  DispatchAudioDecodeTaskIfNeeded();
  DispatchVideoDecodeTaskIfNeeded();
}

nsresult MediaDecoderStateMachine::ScheduleStateMachine(int64_t aUsecs) {
  return mScheduler->Schedule(aUsecs);
}

bool MediaDecoderStateMachine::OnDecodeThread() const
{
  return mDecodeTaskQueue->IsCurrentThreadIn();
}

bool MediaDecoderStateMachine::OnStateMachineThread() const
{
  return mScheduler->OnStateMachineThread();
}

nsIEventTarget* MediaDecoderStateMachine::GetStateMachineThread() const
{
  return mScheduler->GetStateMachineThread();
}

bool MediaDecoderStateMachine::IsStateMachineScheduled() const
{
  return mScheduler->IsScheduled();
}

void MediaDecoderStateMachine::SetPlaybackRate(double aPlaybackRate)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ASSERTION(aPlaybackRate != 0,
      "PlaybackRate == 0 should be handled before this function.");
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  if (mPlaybackRate == aPlaybackRate) {
    return;
  }

  // AudioStream will handle playback rate change when we have audio.
  // Do nothing while we are not playing. Change in playback rate will
  // take effect next time we start playing again.
  if (!HasAudio() && IsPlaying()) {
    // Remember how much time we've spent in playing the media
    // for playback rate will change from now on.
    mPlayDuration = GetVideoStreamPosition() - mStartTime;
    SetPlayStartTime(TimeStamp::Now());
  }

  mPlaybackRate = aPlaybackRate;
  if (mAudioSink) {
    mAudioSink->SetPlaybackRate(mPlaybackRate);
  }
}

void MediaDecoderStateMachine::SetPreservesPitch(bool aPreservesPitch)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  mPreservesPitch = aPreservesPitch;
  if (mAudioSink) {
    mAudioSink->SetPreservesPitch(mPreservesPitch);
  }
}

void
MediaDecoderStateMachine::SetMinimizePrerollUntilPlaybackStarts()
{
  AssertCurrentThreadInMonitor();
  mMinimizePreroll = true;
}

bool MediaDecoderStateMachine::IsShutdown()
{
  AssertCurrentThreadInMonitor();
  return GetState() == DECODER_STATE_SHUTDOWN;
}

void MediaDecoderStateMachine::QueueMetadata(int64_t aPublishTime,
                                             MediaInfo* aInfo,
                                             MetadataTags* aTags)
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  AssertCurrentThreadInMonitor();
  TimedMetadata* metadata = new TimedMetadata;
  metadata->mPublishTime = aPublishTime;
  metadata->mInfo = aInfo;
  metadata->mTags = aTags;
  mMetadataManager.QueueMetadata(metadata);
}

void MediaDecoderStateMachine::OnAudioEndTimeUpdate(int64_t aAudioEndTime)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  MOZ_ASSERT(aAudioEndTime >= mAudioEndTime);
  mAudioEndTime = aAudioEndTime;
}

void MediaDecoderStateMachine::OnPlaybackOffsetUpdate(int64_t aPlaybackOffset)
{
  mDecoder->UpdatePlaybackOffset(aPlaybackOffset);
}

void MediaDecoderStateMachine::OnAudioSinkComplete()
{
  AssertCurrentThreadInMonitor();
  if (mAudioCaptured) {
    return;
  }
  mAudioCompleted = true;
  UpdateReadyState();
  // Kick the decode thread; it may be sleeping waiting for this to finish.
  mDecoder->GetReentrantMonitor().NotifyAll();
}

} // namespace mozilla

// avoid redefined macro in unified build
#undef DECODER_LOG
#undef VERBOSE_LOG
#undef DECODER_WARN
#undef DECODER_WARN_HELPER
