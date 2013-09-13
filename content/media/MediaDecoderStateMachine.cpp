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
#include "AudioStream.h"
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

#include "prenv.h"
#include "mozilla/Preferences.h"
#include <algorithm>

namespace mozilla {

using namespace mozilla::layers;
using namespace mozilla::dom;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaDecoderLog;
#define LOG(type, msg) PR_LOG(gMediaDecoderLog, type, msg)
#else
#define LOG(type, msg)
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

// Maximum number of bytes we'll allocate and write at once to the audio
// hardware when the audio stream contains missing frames and we're
// writing silence in order to fill the gap. We limit our silence-writes
// to 32KB in order to avoid allocating an impossibly large chunk of
// memory if we encounter a large chunk of silence.
const uint32_t SILENCE_BYTES_CHUNK = 32 * 1024;

// If we have fewer than LOW_VIDEO_FRAMES decoded frames, and
// we're not "pumping video", we'll skip the video up to the next keyframe
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
PR_STATIC_ASSERT(LOW_DATA_THRESHOLD_USECS > AMPLE_AUDIO_USECS);

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
PR_STATIC_ASSERT(QUICK_BUFFERING_LOW_DATA_USECS <= AMPLE_AUDIO_USECS);

// This value has been chosen empirically.
static const uint32_t AUDIOSTREAM_MIN_WRITE_BEFORE_START_USECS = 200000;

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

// Owns the global state machine thread and counts of
// state machine and decoder threads. There should
// only be one instance of this class.
class StateMachineTracker
{
private:
  StateMachineTracker() :
    mMonitor("media.statemachinetracker"),
    mStateMachineCount(0),
    mDecodeThreadCount(0),
    mStateMachineThread(nullptr)
  {
     MOZ_COUNT_CTOR(StateMachineTracker);
     NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  }

  ~StateMachineTracker()
  {
    NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

    MOZ_COUNT_DTOR(StateMachineTracker);
  }

public:
  // Access singleton instance. This is initially called on the main
  // thread in the MediaDecoderStateMachine constructor resulting
  // in the global object being created lazily. Non-main thread
  // access always occurs after this and uses the monitor to
  // safely access the decode thread counts.
  static StateMachineTracker& Instance();

  // Instantiate the global state machine thread if required.
  // Call on main thread only.
  void EnsureGlobalStateMachine();

  // Destroy global state machine thread if required.
  // Call on main thread only.
  void CleanupGlobalStateMachine();

  // Return the global state machine thread. Call from any thread.
  nsIThread* GetGlobalStateMachineThread()
  {
    ReentrantMonitorAutoEnter mon(mMonitor);
    NS_ASSERTION(mStateMachineThread, "Should have non-null state machine thread!");
    return mStateMachineThread;
  }

  // Requests that a decode thread be created for aStateMachine. The thread
  // may be created immediately, or after some delay, once a thread becomes
  // available. The request can be cancelled using CancelCreateDecodeThread().
  // It's the callers responsibility to not call this more than once for any
  // given state machine.
  nsresult RequestCreateDecodeThread(MediaDecoderStateMachine* aStateMachine);

  // Cancels a request made by RequestCreateDecodeThread to create a decode
  // thread for aStateMachine.
  nsresult CancelCreateDecodeThread(MediaDecoderStateMachine* aStateMachine);

  // Maximum number of active decode threads allowed. When more
  // than this number are active the thread creation will fail.
  static const uint32_t MAX_DECODE_THREADS = 25;

  // Returns the number of active decode threads.
  // Call on any thread. Holds the internal monitor so don't
  // call with any other monitor held to avoid deadlock.
  uint32_t GetDecodeThreadCount();

  // Keep track of the fact that a decode thread was destroyed.
  // Call on any thread. Holds the internal monitor so don't
  // call with any other monitor held to avoid deadlock.
  void NoteDecodeThreadDestroyed();

#ifdef DEBUG
  // Returns true if aStateMachine has a pending request for a
  // decode thread.
  bool IsQueued(MediaDecoderStateMachine* aStateMachine);
#endif

private:
  // Holds global instance of StateMachineTracker.
  // Writable on main thread only.
  static StateMachineTracker* sInstance;

  // Reentrant monitor that must be obtained to access
  // the decode thread count member and methods.
  ReentrantMonitor mMonitor;

  // Number of instances of MediaDecoderStateMachine
  // that are currently instantiated. Access on the
  // main thread only.
  uint32_t mStateMachineCount;

  // Number of instances of decoder threads that are
  // currently instantiated. Access only with the
  // mMonitor lock held. Can be used from any thread.
  uint32_t mDecodeThreadCount;

  // Global state machine thread. Write on the main thread
  // only, read from the decoder threads. Synchronized via
  // the mMonitor.
  nsIThread* mStateMachineThread;

  // Queue of state machines waiting for decode threads. Entries at the front
  // get their threads first.
  nsDeque mPending;
};

StateMachineTracker* StateMachineTracker::sInstance = nullptr;

StateMachineTracker& StateMachineTracker::Instance()
{
  if (!sInstance) {
    NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
    sInstance = new StateMachineTracker();
  }
  return *sInstance;
}

void StateMachineTracker::EnsureGlobalStateMachine()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  ReentrantMonitorAutoEnter mon(mMonitor);
  if (mStateMachineCount == 0) {
    NS_ASSERTION(!mStateMachineThread, "Should have null state machine thread!");
    DebugOnly<nsresult> rv = NS_NewNamedThread("Media State", &mStateMachineThread);
    NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv), "Can't create media state machine thread");
  }
  mStateMachineCount++;
}

#ifdef DEBUG
bool StateMachineTracker::IsQueued(MediaDecoderStateMachine* aStateMachine)
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  int32_t size = mPending.GetSize();
  for (int i = 0; i < size; ++i) {
    MediaDecoderStateMachine* m =
      static_cast<MediaDecoderStateMachine*>(mPending.ObjectAt(i));
    if (m == aStateMachine) {
      return true;
    }
  }
  return false;
}
#endif

void StateMachineTracker::CleanupGlobalStateMachine()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ABORT_IF_FALSE(mStateMachineCount > 0,
    "State machine ref count must be > 0");
  mStateMachineCount--;
  if (mStateMachineCount == 0) {
    LOG(PR_LOG_DEBUG, ("Destroying media state machine thread"));
    NS_ASSERTION(mPending.GetSize() == 0, "Shouldn't all requests be handled by now?");
    {
      ReentrantMonitorAutoEnter mon(mMonitor);
      nsCOMPtr<nsIRunnable> event = new ShutdownThreadEvent(mStateMachineThread);
      NS_RELEASE(mStateMachineThread);
      mStateMachineThread = nullptr;
      NS_DispatchToMainThread(event);

      NS_ASSERTION(mDecodeThreadCount == 0, "Decode thread count must be zero.");
      sInstance = nullptr;
    }
    delete this;
  }
}

void StateMachineTracker::NoteDecodeThreadDestroyed()
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  --mDecodeThreadCount;
  while (mDecodeThreadCount < MAX_DECODE_THREADS && mPending.GetSize() > 0) {
    MediaDecoderStateMachine* m =
      static_cast<MediaDecoderStateMachine*>(mPending.PopFront());
    nsresult rv;
    {
      ReentrantMonitorAutoExit exitMon(mMonitor);
      rv = m->StartDecodeThread();
    }
    if (NS_SUCCEEDED(rv)) {
      ++mDecodeThreadCount;
    }
  }
}

uint32_t StateMachineTracker::GetDecodeThreadCount()
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  return mDecodeThreadCount;
}

nsresult StateMachineTracker::CancelCreateDecodeThread(MediaDecoderStateMachine* aStateMachine) {
  ReentrantMonitorAutoEnter mon(mMonitor);
  int32_t size = mPending.GetSize();
  for (int32_t i = 0; i < size; ++i) {
    void* m = static_cast<MediaDecoderStateMachine*>(mPending.ObjectAt(i));
    if (m == aStateMachine) {
      mPending.RemoveObjectAt(i);
      break;
    }
  }
  NS_ASSERTION(!IsQueued(aStateMachine), "State machine should no longer have queued request.");
  return NS_OK;
}

nsresult StateMachineTracker::RequestCreateDecodeThread(MediaDecoderStateMachine* aStateMachine)
{
  NS_ENSURE_STATE(aStateMachine);
  ReentrantMonitorAutoEnter mon(mMonitor);
  if (mPending.GetSize() > 0 || mDecodeThreadCount + 1 >= MAX_DECODE_THREADS) {
    // If there's already state machines in the queue, or we've exceeded the
    // limit, append the state machine to the queue of state machines waiting
    // for a decode thread. This ensures state machines already waiting get
    // their threads first.
    mPending.Push(aStateMachine);
    return NS_OK;
  }
  nsresult rv;
  {
    ReentrantMonitorAutoExit exitMon(mMonitor);
    rv = aStateMachine->StartDecodeThread();
  }
  if (NS_SUCCEEDED(rv)) {
    ++mDecodeThreadCount;
  }
  NS_ASSERTION(mDecodeThreadCount <= MAX_DECODE_THREADS,
                "Should keep to thread limit!");
  return NS_OK;
}

MediaDecoderStateMachine::MediaDecoderStateMachine(MediaDecoder* aDecoder,
                                                   MediaDecoderReader* aReader,
                                                   bool aRealTime) :
  mDecoder(aDecoder),
  mState(DECODER_STATE_DECODING_METADATA),
  mResetPlayStartTime(false),
  mPlayDuration(0),
  mStartTime(-1),
  mEndTime(-1),
  mSeekTime(0),
  mFragmentEndTime(-1),
  mReader(aReader),
  mCurrentFrameTime(0),
  mAudioStartTime(-1),
  mAudioEndTime(-1),
  mVideoFrameEndTime(-1),
  mVolume(1.0),
  mPlaybackRate(1.0),
  mPreservesPitch(true),
  mBasePosition(0),
  mAudioCaptured(false),
  mTransportSeekable(true),
  mMediaSeekable(true),
  mPositionChangeQueued(false),
  mAudioCompleted(false),
  mGotDurationFromMetaData(false),
  mStopDecodeThread(true),
  mDecodeThreadIdle(false),
  mStopAudioThread(true),
  mQuickBuffering(false),
  mIsRunning(false),
  mRunAgain(false),
  mDispatchedRunEvent(false),
  mDecodeThreadWaiting(false),
  mRealTime(aRealTime),
  mDidThrottleAudioDecoding(false),
  mDidThrottleVideoDecoding(false),
  mRequestedNewDecodeThread(false),
  mEventManager(aDecoder),
  mLastFrameStatus(MediaDecoderOwner::NEXT_FRAME_UNINITIALIZED)
{
  MOZ_COUNT_CTOR(MediaDecoderStateMachine);
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  StateMachineTracker::Instance().EnsureGlobalStateMachine();

  // only enable realtime mode when "media.realtime_decoder.enabled" is true.
  if (Preferences::GetBool("media.realtime_decoder.enabled", false) == false)
    mRealTime = false;

  mBufferingWait = mRealTime ? 0 : BUFFERING_WAIT_S;
  mLowDataThresholdUsecs = mRealTime ? 0 : LOW_DATA_THRESHOLD_USECS;

  // If we've got more than mAmpleVideoFrames decoded video frames waiting in
  // the video queue, we will not decode any more video frames until some have
  // been consumed by the play state machine thread.
#if defined(MOZ_OMX_DECODER) || defined(MOZ_MEDIA_PLUGINS)
  // On B2G and Android this is decided by a similar value which varies for
  // each OMX decoder |OMX_PARAM_PORTDEFINITIONTYPE::nBufferCountMin|. This
  // number must be less than the OMX equivalent or gecko will think it is
  // chronically starved of video frames. All decoders seen so far have a value
  // of at least 4.
  mAmpleVideoFrames = Preferences::GetUint("media.video-queue.default-size", 3);
#else
  mAmpleVideoFrames = Preferences::GetUint("media.video-queue.default-size", 10);
#endif
  if (mAmpleVideoFrames < 2) {
    mAmpleVideoFrames = 2;
  }
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
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  MOZ_COUNT_DTOR(MediaDecoderStateMachine);
  NS_ASSERTION(!mPendingWakeDecoder.get(),
               "WakeDecoder should have been revoked already");
  NS_ASSERTION(!StateMachineTracker::Instance().IsQueued(this),
    "Should not have a pending request for a new decode thread");
  NS_ASSERTION(!mRequestedNewDecodeThread,
    "Should not have (or flagged) a pending request for a new decode thread");
  if (mTimer)
    mTimer->Cancel();
  mTimer = nullptr;
  mReader = nullptr;

  StateMachineTracker::Instance().CleanupGlobalStateMachine();
#ifdef XP_WIN
  timeEndPeriod(1);
#endif
}

bool MediaDecoderStateMachine::HasFutureAudio() const {
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  NS_ASSERTION(HasAudio(), "Should only call HasFutureAudio() when we have audio");
  // We've got audio ready to play if:
  // 1. We've not completed playback of audio, and
  // 2. we either have more than the threshold of decoded audio available, or
  //    we've completely decoded all audio (but not finished playing it yet
  //    as per 1).
  return !mAudioCompleted &&
         (AudioDecodedUsecs() > LOW_AUDIO_USECS * mPlaybackRate || mReader->AudioQueue().IsFinished());
}

bool MediaDecoderStateMachine::HaveNextFrameData() const {
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  return (!HasAudio() || HasFutureAudio()) &&
         (!HasVideo() || mReader->VideoQueue().GetSize() > 0);
}

int64_t MediaDecoderStateMachine::GetDecodedAudioDuration() {
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  int64_t audioDecoded = mReader->AudioQueue().Duration();
  if (mAudioEndTime != -1) {
    audioDecoded += mAudioEndTime - GetMediaTime();
  }
  return audioDecoded;
}

void MediaDecoderStateMachine::DecodeThreadRun()
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  mReader->OnDecodeThreadStart();

  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

    if (mState == DECODER_STATE_DECODING_METADATA &&
        NS_FAILED(DecodeMetadata())) {
      NS_ASSERTION(mState == DECODER_STATE_SHUTDOWN,
                   "Should be in shutdown state if metadata loading fails.");
      LOG(PR_LOG_DEBUG, ("Decode metadata failed, shutting down decode thread"));
    }

    while (mState != DECODER_STATE_SHUTDOWN &&
           mState != DECODER_STATE_COMPLETED &&
           mState != DECODER_STATE_DORMANT &&
           !mStopDecodeThread)
    {
      if (mState == DECODER_STATE_DECODING || mState == DECODER_STATE_BUFFERING) {
        DecodeLoop();
      } else if (mState == DECODER_STATE_SEEKING) {
        DecodeSeek();
      } else if (mState == DECODER_STATE_DECODING_METADATA) {
        if (NS_FAILED(DecodeMetadata())) {
          NS_ASSERTION(mState == DECODER_STATE_SHUTDOWN,
                       "Should be in shutdown state if metadata loading fails.");
          LOG(PR_LOG_DEBUG, ("Decode metadata failed, shutting down decode thread"));
        }
      } else if (mState == DECODER_STATE_WAIT_FOR_RESOURCES) {
        mDecoder->GetReentrantMonitor().Wait();

        if (!mReader->IsWaitingMediaResources()) {
          // change state to DECODER_STATE_WAIT_FOR_RESOURCES
          StartDecodeMetadata();
        }
      } else if (mState == DECODER_STATE_DORMANT) {
        mDecoder->GetReentrantMonitor().Wait();
      }
    }

    mDecodeThreadIdle = true;
    LOG(PR_LOG_DEBUG, ("%p Decode thread finished", mDecoder.get()));
  }

  mReader->OnDecodeThreadFinish();
}

void MediaDecoderStateMachine::SendStreamAudio(AudioData* aAudio,
                                               DecodedStreamData* aStream,
                                               AudioSegment* aOutput)
{
  NS_ASSERTION(OnDecodeThread() ||
               OnStateMachineThread(), "Should be on decode thread or state machine thread");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  if (aAudio->mTime <= aStream->mLastAudioPacketTime) {
    // ignore packet that we've already processed
    return;
  }
  aStream->mLastAudioPacketTime = aAudio->mTime;
  aStream->mLastAudioPacketEndTime = aAudio->GetEnd();

  // This logic has to mimic AudioLoop closely to make sure we write
  // the exact same silences
  CheckedInt64 audioWrittenOffset = UsecsToFrames(mInfo.mAudioRate,
      aStream->mInitialTime + mStartTime) + aStream->mAudioFramesWritten;
  CheckedInt64 frameOffset = UsecsToFrames(mInfo.mAudioRate, aAudio->mTime);
  if (!audioWrittenOffset.isValid() || !frameOffset.isValid())
    return;
  if (audioWrittenOffset.value() < frameOffset.value()) {
    // Write silence to catch up
    LOG(PR_LOG_DEBUG, ("%p Decoder writing %d frames of silence to MediaStream",
                       mDecoder.get(), int32_t(frameOffset.value() - audioWrittenOffset.value())));
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
  LOG(PR_LOG_DEBUG, ("%p Decoder writing %d frames of data to MediaStream for AudioData at %lld",
                     mDecoder.get(), aAudio->mFrames - int32_t(offset), aAudio->mTime));
  aStream->mAudioFramesWritten += aAudio->mFrames - int32_t(offset);
}

static void WriteVideoToMediaStream(layers::Image* aImage,
                                    int64_t aDuration, const gfxIntSize& aIntrinsicSize,
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
  NS_ASSERTION(OnDecodeThread() ||
               OnStateMachineThread(), "Should be on decode thread or state machine thread");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  DecodedStreamData* stream = mDecoder->GetDecodedStream();
  if (!stream)
    return;

  if (mState == DECODER_STATE_DECODING_METADATA)
    return;

  if (!mDecoder->IsSameOriginMedia()) {
    return;
  }

  // If there's still an audio thread alive, then we can't send any stream
  // data yet since both SendStreamData and the audio thread want to be in
  // charge of popping the audio queue. We're waiting for the audio thread
  // to die before sending anything to our stream.
  if (mAudioThread)
    return;

  int64_t minLastAudioPacketTime = INT64_MAX;
  SourceMediaStream* mediaStream = stream->mStream;
  StreamTime endPosition = 0;

  if (!stream->mStreamInitialized) {
    if (mInfo.mHasAudio) {
      AudioSegment* audio = new AudioSegment();
      mediaStream->AddTrack(TRACK_AUDIO, mInfo.mAudioRate, 0, audio);
    }
    if (mInfo.mHasVideo) {
      VideoSegment* video = new VideoSegment();
      mediaStream->AddTrack(TRACK_VIDEO, RATE_VIDEO, 0, video);
    }
    stream->mStreamInitialized = true;
  }

  if (mInfo.mHasAudio) {
    nsAutoTArray<AudioData*,10> audio;
    // It's OK to hold references to the AudioData because while audio
    // is captured, only the decoder thread pops from the queue (see below).
    mReader->AudioQueue().GetElementsAfter(stream->mLastAudioPacketTime, &audio);
    AudioSegment output;
    for (uint32_t i = 0; i < audio.Length(); ++i) {
      SendStreamAudio(audio[i], stream, &output);
    }
    if (output.GetDuration() > 0) {
      mediaStream->AppendToTrack(TRACK_AUDIO, &output);
    }
    if (mReader->AudioQueue().IsFinished() && !stream->mHaveSentFinishAudio) {
      mediaStream->EndTrack(TRACK_AUDIO);
      stream->mHaveSentFinishAudio = true;
    }
    minLastAudioPacketTime = std::min(minLastAudioPacketTime, stream->mLastAudioPacketTime);
    endPosition = std::max(endPosition,
        TicksToTimeRoundDown(mInfo.mAudioRate, stream->mAudioFramesWritten));
  }

  if (mInfo.mHasVideo) {
    nsAutoTArray<VideoData*,10> video;
    // It's OK to hold references to the VideoData only the decoder thread
    // pops from the queue.
    mReader->VideoQueue().GetElementsAfter(stream->mNextVideoTime + mStartTime, &video);
    VideoSegment output;
    for (uint32_t i = 0; i < video.Length(); ++i) {
      VideoData* v = video[i];
      if (stream->mNextVideoTime + mStartTime < v->mTime) {
        LOG(PR_LOG_DEBUG, ("%p Decoder writing last video to MediaStream %p for %lld ms",
                           mDecoder.get(), mediaStream,
                           v->mTime - (stream->mNextVideoTime + mStartTime)));
        // Write last video frame to catch up. mLastVideoImage can be null here
        // which is fine, it just means there's no video.
        WriteVideoToMediaStream(stream->mLastVideoImage,
          v->mTime - (stream->mNextVideoTime + mStartTime), stream->mLastVideoImageDisplaySize,
            &output);
        stream->mNextVideoTime = v->mTime - mStartTime;
      }
      if (stream->mNextVideoTime + mStartTime < v->mEndTime) {
        LOG(PR_LOG_DEBUG, ("%p Decoder writing video frame %lld to MediaStream %p for %lld ms",
                           mDecoder.get(), v->mTime, mediaStream,
                           v->mEndTime - (stream->mNextVideoTime + mStartTime)));
        WriteVideoToMediaStream(v->mImage,
            v->mEndTime - (stream->mNextVideoTime + mStartTime), v->mDisplay,
            &output);
        stream->mNextVideoTime = v->mEndTime - mStartTime;
        stream->mLastVideoImage = v->mImage;
        stream->mLastVideoImageDisplaySize = v->mDisplay;
      } else {
        LOG(PR_LOG_DEBUG, ("%p Decoder skipping writing video frame %lld to MediaStream",
                           mDecoder.get(), v->mTime));
      }
    }
    if (output.GetDuration() > 0) {
      mediaStream->AppendToTrack(TRACK_VIDEO, &output);
    }
    if (mReader->VideoQueue().IsFinished() && !stream->mHaveSentFinishVideo) {
      mediaStream->EndTrack(TRACK_VIDEO);
      stream->mHaveSentFinishVideo = true;
    }
    endPosition = std::max(endPosition,
        TicksToTimeRoundDown(RATE_VIDEO, stream->mNextVideoTime - stream->mInitialTime));
  }

  if (!stream->mHaveSentFinish) {
    stream->mStream->AdvanceKnownTracksTime(endPosition);
  }

  bool finished =
      (!mInfo.mHasAudio || mReader->AudioQueue().IsFinished()) &&
      (!mInfo.mHasVideo || mReader->VideoQueue().IsFinished());
  if (finished && !stream->mHaveSentFinish) {
    stream->mHaveSentFinish = true;
    stream->mStream->Finish();
  }

  if (mAudioCaptured) {
    // Discard audio packets that are no longer needed.
    while (true) {
      nsAutoPtr<AudioData> a(mReader->AudioQueue().PopFront());
      if (!a)
        break;
      // Packet times are not 100% reliable so this may discard packets that
      // actually contain data for mCurrentFrameTime. This means if someone might
      // create a new output stream and we actually don't have the audio for the
      // very start. That's OK, we'll play silence instead for a brief moment.
      // That's OK. Seeking to this time would have a similar issue for such
      // badly muxed resources.
      if (a->GetEnd() >= minLastAudioPacketTime) {
        mReader->AudioQueue().PushFront(a.forget());
        break;
      }
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
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  if (!mPendingWakeDecoder.get()) {
    mPendingWakeDecoder = new WakeDecoderRunnable(this);
  }
  return mPendingWakeDecoder.get();
}

bool MediaDecoderStateMachine::HaveEnoughDecodedAudio(int64_t aAmpleAudioUSecs)
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  if (mReader->AudioQueue().GetSize() == 0 ||
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
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  if (static_cast<uint32_t>(mReader->VideoQueue().GetSize()) < GetAmpleVideoFrames() * mPlaybackRate) {
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

void MediaDecoderStateMachine::DecodeLoop()
{
  LOG(PR_LOG_DEBUG, ("%p Start DecodeLoop()", mDecoder.get()));

  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");

  // We want to "pump" the decode until we've got a few frames decoded
  // before we consider whether decode is falling behind.
  bool audioPump = true;
  bool videoPump = true;

  // If the video decode is falling behind the audio, we'll start dropping the
  // inter-frames up until the next keyframe which is at or before the current
  // playback position. skipToNextKeyframe is true if we're currently
  // skipping up to the next keyframe.
  bool skipToNextKeyframe = false;

  // Once we've decoded more than videoPumpThreshold video frames, we'll
  // no longer be considered to be "pumping video".
  const unsigned videoPumpThreshold = mRealTime ? 0 : GetAmpleVideoFrames() / 2;

  // After the audio decode fills with more than audioPumpThreshold usecs
  // of decoded audio, we'll start to check whether the audio or video decode
  // is falling behind.
  const unsigned audioPumpThreshold = mRealTime ? 0 : LOW_AUDIO_USECS * 2;

  // Our local low audio threshold. We may increase this if we're slow to
  // decode video frames, in order to reduce the chance of audio underruns.
  int64_t lowAudioThreshold = LOW_AUDIO_USECS;

  // Our local ample audio threshold. If we increase lowAudioThreshold, we'll
  // also increase this too appropriately (we don't want lowAudioThreshold to
  // be greater than ampleAudioThreshold, else we'd stop decoding!).
  int64_t ampleAudioThreshold = AMPLE_AUDIO_USECS;

  // Main decode loop.
  bool videoPlaying = HasVideo();
  bool audioPlaying = HasAudio();
  while ((mState == DECODER_STATE_DECODING || mState == DECODER_STATE_BUFFERING) &&
         !mStopDecodeThread &&
         (videoPlaying || audioPlaying))
  {
#ifdef MOZ_DASH
    mReader->PrepareToDecode();
#endif

    // We don't want to consider skipping to the next keyframe if we've
    // only just started up the decode loop, so wait until we've decoded
    // some frames before enabling the keyframe skip logic on video.
    if (videoPump &&
        (static_cast<uint32_t>(mReader->VideoQueue().GetSize())
         >= videoPumpThreshold * mPlaybackRate))
    {
      videoPump = false;
    }

    // We don't want to consider skipping to the next keyframe if we've
    // only just started up the decode loop, so wait until we've decoded
    // some audio data before enabling the keyframe skip logic on audio.
    if (audioPump && GetDecodedAudioDuration() >= audioPumpThreshold * mPlaybackRate) {
      audioPump = false;
    }

    // We'll skip the video decode to the nearest keyframe if we're low on
    // audio, or if we're low on video, provided we're not running low on
    // data to decode. If we're running low on downloaded data to decode,
    // we won't start keyframe skipping, as we'll be pausing playback to buffer
    // soon anyway and we'll want to be able to display frames immediately
    // after buffering finishes.
    if (mState == DECODER_STATE_DECODING &&
        !skipToNextKeyframe &&
        videoPlaying &&
        ((!audioPump && audioPlaying && !mDidThrottleAudioDecoding &&
          GetDecodedAudioDuration() < lowAudioThreshold * mPlaybackRate) ||
         (!videoPump && videoPlaying && !mDidThrottleVideoDecoding &&
          (static_cast<uint32_t>(mReader->VideoQueue().GetSize())
           < LOW_VIDEO_FRAMES * mPlaybackRate))) &&
        !HasLowUndecodedData())
    {
      skipToNextKeyframe = true;
      LOG(PR_LOG_DEBUG, ("%p Skipping video decode to the next keyframe", mDecoder.get()));
    }

    // Video decode.
    bool throttleVideoDecoding = !videoPlaying || HaveEnoughDecodedVideo();
    if (mDidThrottleVideoDecoding && !throttleVideoDecoding) {
      videoPump = true;
    }
    mDidThrottleVideoDecoding = throttleVideoDecoding;
    if (!throttleVideoDecoding)
    {
      // Time the video decode, so that if it's slow, we can increase our low
      // audio threshold to reduce the chance of an audio underrun while we're
      // waiting for a video decode to complete.
      TimeDuration decodeTime;
      {
        int64_t currentTime = GetMediaTime();
        ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
        TimeStamp start = TimeStamp::Now();
        videoPlaying = mReader->DecodeVideoFrame(skipToNextKeyframe, currentTime);
        decodeTime = TimeStamp::Now() - start;
        if (!videoPlaying) {
          // Playback ended for this stream, close the sample queue.
          mReader->VideoQueue().Finish();
        }
      }
      if (THRESHOLD_FACTOR * DurationToUsecs(decodeTime) > lowAudioThreshold &&
          !HasLowUndecodedData())
      {
        lowAudioThreshold =
          std::min(THRESHOLD_FACTOR * DurationToUsecs(decodeTime), AMPLE_AUDIO_USECS);
        ampleAudioThreshold = std::max(THRESHOLD_FACTOR * lowAudioThreshold,
                                     ampleAudioThreshold);
        LOG(PR_LOG_DEBUG,
            ("Slow video decode, set lowAudioThreshold=%lld ampleAudioThreshold=%lld",
             lowAudioThreshold, ampleAudioThreshold));
      }
    }

    // Audio decode.
    bool throttleAudioDecoding = !audioPlaying || HaveEnoughDecodedAudio(ampleAudioThreshold * mPlaybackRate);
    if (mDidThrottleAudioDecoding && !throttleAudioDecoding) {
      audioPump = true;
    }
    mDidThrottleAudioDecoding = throttleAudioDecoding;
    if (!mDidThrottleAudioDecoding) {
      ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
      audioPlaying = mReader->DecodeAudioData();
      if (!audioPlaying) {
        // Playback ended for this stream, close the sample queue.
        mReader->AudioQueue().Finish();
      }
    }

    SendStreamData();

    // Notify to ensure that the AudioLoop() is not waiting, in case it was
    // waiting for more audio to be decoded.
    mDecoder->GetReentrantMonitor().NotifyAll();

    // The ready state can change when we've decoded data, so update the
    // ready state, so that DOM events can fire.
    UpdateReadyState();

    if ((mState == DECODER_STATE_DECODING || mState == DECODER_STATE_BUFFERING) &&
        !mStopDecodeThread &&
        (videoPlaying || audioPlaying) &&
        throttleAudioDecoding && throttleVideoDecoding)
    {
      // All active bitstreams' decode is well ahead of the playback
      // position, we may as well wait for the playback to catch up. Note the
      // audio push thread acquires and notifies the decoder monitor every time
      // it pops AudioData off the audio queue. So if the audio push thread pops
      // the last AudioData off the audio queue right after that queue reported
      // it was non-empty here, we'll receive a notification on the decoder
      // monitor which will wake us up shortly after we sleep, thus preventing
      // both the decode and audio push threads waiting at the same time.
      // See bug 620326.
      mDecodeThreadWaiting = true;
      if (mDecoder->GetState() != MediaDecoder::PLAY_STATE_PLAYING) {
        // We're not playing, and the decode is about to wait. This means
        // the decode thread may not be needed in future. Signal the state
        // machine thread to run, so it can decide whether to shutdown the
        // decode thread.
        ScheduleStateMachine();
      }
      mDecoder->GetReentrantMonitor().Wait();
      mDecodeThreadWaiting = false;
    }

  } // End decode loop.

  if (!mStopDecodeThread &&
      mState != DECODER_STATE_SHUTDOWN &&
      mState != DECODER_STATE_DORMANT &&
      mState != DECODER_STATE_SEEKING)
  {
    mState = DECODER_STATE_COMPLETED;
    ScheduleStateMachine();
  }

  LOG(PR_LOG_DEBUG, ("%p Exiting DecodeLoop", mDecoder.get()));
}

bool MediaDecoderStateMachine::IsPlaying()
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  return !mPlayStartTime.IsNull();
}

// If we have already written enough frames to the AudioStream, start the
// playback.
static void
StartAudioStreamPlaybackIfNeeded(AudioStream* aStream)
{
  // We want to have enough data in the buffer to start the stream.
  if (static_cast<double>(aStream->GetWritten()) / aStream->GetRate() >=
      static_cast<double>(AUDIOSTREAM_MIN_WRITE_BEFORE_START_USECS) / USECS_PER_S) {
    aStream->Start();
  }
}

static void WriteSilence(AudioStream* aStream, uint32_t aFrames)
{
  uint32_t numSamples = aFrames * aStream->GetChannels();
  nsAutoTArray<AudioDataValue, 1000> buf;
  buf.SetLength(numSamples);
  memset(buf.Elements(), 0, numSamples * sizeof(AudioDataValue));
  aStream->Write(buf.Elements(), aFrames);

  StartAudioStreamPlaybackIfNeeded(aStream);
}

void MediaDecoderStateMachine::AudioLoop()
{
  NS_ASSERTION(OnAudioThread(), "Should be on audio thread.");
  LOG(PR_LOG_DEBUG, ("%p Begun audio thread/loop", mDecoder.get()));
  int64_t audioDuration = 0;
  int64_t audioStartTime = -1;
  uint32_t channels, rate;
  double volume = -1;
  bool setVolume;
  double playbackRate = -1;
  bool setPlaybackRate;
  bool preservesPitch;
  bool setPreservesPitch;
  AudioChannelType audioChannelType;

  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mAudioCompleted = false;
    audioStartTime = mAudioStartTime;
    NS_ASSERTION(audioStartTime != -1, "Should have audio start time by now");
    channels = mInfo.mAudioChannels;
    rate = mInfo.mAudioRate;

    audioChannelType = mDecoder->GetAudioChannelType();
    volume = mVolume;
    preservesPitch = mPreservesPitch;
    playbackRate = mPlaybackRate;
  }

  {
    // AudioStream initialization can block for extended periods in unusual
    // circumstances, so we take care to drop the decoder monitor while
    // initializing.
    nsAutoPtr<AudioStream> audioStream(AudioStream::AllocateStream());
    audioStream->Init(channels, rate, audioChannelType);
    audioStream->SetVolume(volume);
    if (audioStream->SetPreservesPitch(preservesPitch) != NS_OK) {
      NS_WARNING("Setting the pitch preservation failed at AudioLoop start.");
    }
    if (playbackRate != 1.0) {
      NS_ASSERTION(playbackRate != 0,
                   "Don't set the playbackRate to 0 on an AudioStream.");
      if (audioStream->SetPlaybackRate(playbackRate) != NS_OK) {
        NS_WARNING("Setting the playback rate failed at AudioLoop start.");
      }
    }

    {
      ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
      mAudioStream = audioStream;
    }
  }

  while (1) {
    // Wait while we're not playing, and we're not shutting down, or we're
    // playing and we've got no audio to play.
    {
      ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
      NS_ASSERTION(mState != DECODER_STATE_DECODING_METADATA,
                   "Should have meta data before audio started playing.");
      while (mState != DECODER_STATE_SHUTDOWN &&
             !mStopAudioThread &&
             (!IsPlaying() ||
              mState == DECODER_STATE_BUFFERING ||
              (mReader->AudioQueue().GetSize() == 0 &&
               !mReader->AudioQueue().AtEndOfStream())))
      {
        if (!IsPlaying() && !mAudioStream->IsPaused()) {
          mAudioStream->Pause();
        }
        mon.Wait();
      }

      // If we're shutting down, break out and exit the audio thread.
      // Also break out if audio is being captured.
      if (mState == DECODER_STATE_SHUTDOWN ||
          mStopAudioThread ||
          mReader->AudioQueue().AtEndOfStream())
      {
        break;
      }

      // We only want to go to the expense of changing the volume if
      // the volume has changed.
      setVolume = volume != mVolume;
      volume = mVolume;

      // Same for the playbackRate.
      setPlaybackRate = playbackRate != mPlaybackRate;
      playbackRate = mPlaybackRate;

      // Same for the pitch preservation.
      setPreservesPitch = preservesPitch != mPreservesPitch;
      preservesPitch = mPreservesPitch;

      if (IsPlaying() && mAudioStream->IsPaused()) {
        mAudioStream->Resume();
      }
    }

    if (setVolume) {
      mAudioStream->SetVolume(volume);
    }
    if (setPlaybackRate) {
      NS_ASSERTION(playbackRate != 0,
                   "Don't set the playbackRate to 0 in the AudioStreams");
      if (mAudioStream->SetPlaybackRate(playbackRate) != NS_OK) {
        NS_WARNING("Setting the playback rate failed in AudioLoop.");
      }
    }
    if (setPreservesPitch) {
      if (mAudioStream->SetPreservesPitch(preservesPitch) != NS_OK) {
        NS_WARNING("Setting the pitch preservation failed in AudioLoop.");
      }
    }
    NS_ASSERTION(mReader->AudioQueue().GetSize() > 0,
                 "Should have data to play");
    // See if there's a gap in the audio. If there is, push silence into the
    // audio hardware, so we can play across the gap.
    const AudioData* s = mReader->AudioQueue().PeekFront();

    // Calculate the number of frames that have been pushed onto the audio
    // hardware.
    CheckedInt64 playedFrames = UsecsToFrames(audioStartTime, rate) +
                                              audioDuration;
    // Calculate the timestamp of the next chunk of audio in numbers of
    // samples.
    CheckedInt64 sampleTime = UsecsToFrames(s->mTime, rate);
    CheckedInt64 missingFrames = sampleTime - playedFrames;
    if (!missingFrames.isValid() || !sampleTime.isValid()) {
      NS_WARNING("Int overflow adding in AudioLoop()");
      break;
    }

    int64_t framesWritten = 0;
    if (missingFrames.value() > 0) {
      // The next audio chunk begins some time after the end of the last chunk
      // we pushed to the audio hardware. We must push silence into the audio
      // hardware so that the next audio chunk begins playback at the correct
      // time.
      missingFrames = std::min<int64_t>(UINT32_MAX, missingFrames.value());
      LOG(PR_LOG_DEBUG, ("%p Decoder playing %d frames of silence",
                         mDecoder.get(), int32_t(missingFrames.value())));
      framesWritten = PlaySilence(static_cast<uint32_t>(missingFrames.value()),
                                  channels, playedFrames.value());
    } else {
      framesWritten = PlayFromAudioQueue(sampleTime.value(), channels);
    }
    audioDuration += framesWritten;
    {
      ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
      CheckedInt64 playedUsecs = FramesToUsecs(audioDuration, rate) + audioStartTime;
      if (!playedUsecs.isValid()) {
        NS_WARNING("Int overflow calculating audio end time");
        break;
      }
      mAudioEndTime = playedUsecs.value();
    }
  }
  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    if (mReader->AudioQueue().AtEndOfStream() &&
        mState != DECODER_STATE_SHUTDOWN &&
        !mStopAudioThread)
    {
      // If the media was too short to trigger the start of the audio stream,
      // start it now.
      mAudioStream->Start();
      // Last frame pushed to audio hardware, wait for the audio to finish,
      // before the audio thread terminates.
      bool seeking = false;
      {
        int64_t oldPosition = -1;
        int64_t position = GetMediaTime();
        while (oldPosition != position &&
               mAudioEndTime - position > 0 &&
               mState != DECODER_STATE_SEEKING &&
               mState != DECODER_STATE_SHUTDOWN)
        {
          const int64_t DRAIN_BLOCK_USECS = 100000;
          Wait(std::min(mAudioEndTime - position, DRAIN_BLOCK_USECS));
          oldPosition = position;
          position = GetMediaTime();
        }
        seeking = mState == DECODER_STATE_SEEKING;
      }

      if (!seeking && !mAudioStream->IsPaused()) {
        {
          ReentrantMonitorAutoExit exit(mDecoder->GetReentrantMonitor());
          mAudioStream->Drain();
        }
        // Fire one last event for any extra frames that didn't fill a framebuffer.
        mEventManager.Drain(mAudioEndTime);
      }
    }
  }
  LOG(PR_LOG_DEBUG, ("%p Reached audio stream end.", mDecoder.get()));
  {
    // Must hold lock while shutting down and anulling the audio stream to prevent
    // state machine thread trying to use it while we're destroying it.
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mAudioStream->Shutdown();
    mAudioStream = nullptr;
    mEventManager.Clear();
    if (!mAudioCaptured) {
      mAudioCompleted = true;
      UpdateReadyState();
      // Kick the decode thread; it may be sleeping waiting for this to finish.
      mDecoder->GetReentrantMonitor().NotifyAll();
    }
  }

  LOG(PR_LOG_DEBUG, ("%p Audio stream finished playing, audio thread exit", mDecoder.get()));
}

uint32_t MediaDecoderStateMachine::PlaySilence(uint32_t aFrames,
                                                   uint32_t aChannels,
                                                   uint64_t aFrameOffset)

{
  NS_ASSERTION(OnAudioThread(), "Only call on audio thread.");
  NS_ASSERTION(!mAudioStream->IsPaused(), "Don't play when paused");
  uint32_t maxFrames = SILENCE_BYTES_CHUNK / aChannels / sizeof(AudioDataValue);
  uint32_t frames = std::min(aFrames, maxFrames);
  WriteSilence(mAudioStream, frames);
  // Dispatch events to the DOM for the audio just written.
  mEventManager.QueueWrittenAudioData(nullptr, frames * aChannels,
                                      (aFrameOffset + frames) * aChannels);
  return frames;
}

uint32_t MediaDecoderStateMachine::PlayFromAudioQueue(uint64_t aFrameOffset,
                                                      uint32_t aChannels)
{
  NS_ASSERTION(OnAudioThread(), "Only call on audio thread.");
  NS_ASSERTION(!mAudioStream->IsPaused(), "Don't play when paused");
  nsAutoPtr<AudioData> audio(mReader->AudioQueue().PopFront());
  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    NS_WARN_IF_FALSE(IsPlaying(), "Should be playing");
    // Awaken the decode loop if it's waiting for space to free up in the
    // audio queue.
    mDecoder->GetReentrantMonitor().NotifyAll();
  }
  int64_t offset = -1;
  uint32_t frames = 0;
  if (!PR_GetEnv("MOZ_QUIET")) {
    LOG(PR_LOG_DEBUG, ("%p Decoder playing %d frames of data to stream for AudioData at %lld",
                       mDecoder.get(), audio->mFrames, audio->mTime));
  }
  mAudioStream->Write(audio->mAudioData,
                      audio->mFrames);

  StartAudioStreamPlaybackIfNeeded(mAudioStream);

  offset = audio->mOffset;
  frames = audio->mFrames;

  // Dispatch events to the DOM for the audio just written.
  mEventManager.QueueWrittenAudioData(audio->mAudioData.get(),
                                      audio->mFrames * aChannels,
                                      (aFrameOffset + frames) * aChannels);
  if (offset != -1) {
    mDecoder->UpdatePlaybackOffset(offset);
  }
  return frames;
}

nsresult MediaDecoderStateMachine::Init(MediaDecoderStateMachine* aCloneDonor)
{
  MediaDecoderReader* cloneReader = nullptr;
  if (aCloneDonor) {
    cloneReader = static_cast<MediaDecoderStateMachine*>(aCloneDonor)->mReader;
  }
  return mReader->Init(cloneReader);
}

void MediaDecoderStateMachine::StopPlayback()
{
  LOG(PR_LOG_DEBUG, ("%p StopPlayback()", mDecoder.get()));

  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine thread or the decoder thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  mDecoder->NotifyPlaybackStopped();

  if (IsPlaying()) {
    mPlayDuration += DurationToUsecs(TimeStamp::Now() - mPlayStartTime);
    mPlayStartTime = TimeStamp();
  }
  // Notify the audio thread, so that it notices that we've stopped playing,
  // so it can pause audio playback.
  mDecoder->GetReentrantMonitor().NotifyAll();
  NS_ASSERTION(!IsPlaying(), "Should report not playing at end of StopPlayback()");
}

void MediaDecoderStateMachine::StartPlayback()
{
  LOG(PR_LOG_DEBUG, ("%p StartPlayback()", mDecoder.get()));

  NS_ASSERTION(!IsPlaying(), "Shouldn't be playing when StartPlayback() is called");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  mDecoder->NotifyPlaybackStarted();
  mPlayStartTime = TimeStamp::Now();

  NS_ASSERTION(IsPlaying(), "Should report playing by end of StartPlayback()");
  if (NS_FAILED(StartAudioThread())) {
    NS_WARNING("Failed to create audio thread");
  }
  mDecoder->GetReentrantMonitor().NotifyAll();
}

void MediaDecoderStateMachine::UpdatePlaybackPositionInternal(int64_t aTime)
{
  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  NS_ASSERTION(mStartTime >= 0, "Should have positive mStartTime");
  mCurrentFrameTime = aTime - mStartTime;
  NS_ASSERTION(mCurrentFrameTime >= 0, "CurrentTime should be positive!");
  if (aTime > mEndTime) {
    NS_ASSERTION(mCurrentFrameTime > GetDuration(),
                 "CurrentTime must be after duration if aTime > endTime!");
    mEndTime = aTime;
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(mDecoder, &MediaDecoder::DurationChanged);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
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
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }

  // Notify DOM of any queued up audioavailable events
  mEventManager.DispatchPendingEvents(GetMediaTime());

  mMetadataManager.DispatchMetadataIfNeeded(mDecoder, aTime);

  if (fragmentEnded) {
    StopPlayback();
  }
}

void MediaDecoderStateMachine::ClearPositionChangeFlag()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

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
}

void MediaDecoderStateMachine::SetAudioCaptured(bool aCaptured)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  if (!mAudioCaptured && aCaptured && !mStopAudioThread) {
    // Make sure the state machine runs as soon as possible. That will
    // stop the audio thread.
    // If mStopAudioThread is true then we're already stopping the audio thread
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
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  if (mEndTime == -1 || mStartTime == -1)
    return -1;
  return mEndTime - mStartTime;
}

void MediaDecoderStateMachine::SetDuration(int64_t aDuration)
{
  NS_ASSERTION(NS_IsMainThread() || OnDecodeThread(),
               "Should be on main or decode thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

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
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  int64_t duration = GetDuration();
  if (aDuration != duration &&
      abs(aDuration - duration) > ESTIMATED_DURATION_FUZZ_FACTOR_USECS) {
    SetDuration(aDuration);
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(mDecoder, &MediaDecoder::DurationChanged);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }
}

void MediaDecoderStateMachine::SetMediaEndTime(int64_t aEndTime)
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  mEndTime = aEndTime;
}

void MediaDecoderStateMachine::SetFragmentEndTime(int64_t aEndTime)
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  mFragmentEndTime = aEndTime < 0 ? aEndTime : aEndTime + mStartTime;
}

void MediaDecoderStateMachine::SetTransportSeekable(bool aTransportSeekable)
{
  NS_ASSERTION(NS_IsMainThread() || OnDecodeThread(),
      "Should be on main thread or the decoder thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  mTransportSeekable = aTransportSeekable;
}

void MediaDecoderStateMachine::SetMediaSeekable(bool aMediaSeekable)
{
  NS_ASSERTION(NS_IsMainThread() || OnDecodeThread(),
      "Should be on main thread or the decoder thread.");

  mMediaSeekable = aMediaSeekable;
}

bool MediaDecoderStateMachine::IsDormantNeeded()
{
  return mReader->IsDormantNeeded();
}

void MediaDecoderStateMachine::SetDormant(bool aDormant)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  if (!mReader) {
    return;
  }

  if (aDormant) {
    ScheduleStateMachine();
    mState = DECODER_STATE_DORMANT;
    mDecoder->GetReentrantMonitor().NotifyAll();
  } else if ((aDormant != true) && (mState == DECODER_STATE_DORMANT)) {
    ScheduleStateMachine();
    mStartTime = 0;
    mCurrentFrameTime = 0;
    mState = DECODER_STATE_DECODING_METADATA;
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
  LOG(PR_LOG_DEBUG, ("%p Changed state to SHUTDOWN", mDecoder.get()));
  ScheduleStateMachine();
  mState = DECODER_STATE_SHUTDOWN;
  mDecoder->GetReentrantMonitor().NotifyAll();
}

void MediaDecoderStateMachine::StartDecoding()
{
  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine or decode thread.");
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  if (mState != DECODER_STATE_DECODING) {
    mDecodeStartTime = TimeStamp::Now();
  }
  mState = DECODER_STATE_DECODING;
  ScheduleStateMachine();
}

void MediaDecoderStateMachine::StartWaitForResources()
{
  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine or decode thread.");
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  mState = DECODER_STATE_WAIT_FOR_RESOURCES;
}

void MediaDecoderStateMachine::StartDecodeMetadata()
{
  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine or decode thread.");
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  mState = DECODER_STATE_DECODING_METADATA;
}

void MediaDecoderStateMachine::Play()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  // When asked to play, switch to decoding state only if
  // we are currently buffering. In other cases, we'll start playing anyway
  // when the state machine notices the decoder's state change to PLAYING.
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  if (mState == DECODER_STATE_BUFFERING) {
    LOG(PR_LOG_DEBUG, ("%p Changed state from BUFFERING to DECODING", mDecoder.get()));
    mState = DECODER_STATE_DECODING;
    mDecodeStartTime = TimeStamp::Now();
  }
  ScheduleStateMachine();
}

void MediaDecoderStateMachine::ResetPlayback()
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  mVideoFrameEndTime = -1;
  mAudioStartTime = -1;
  mAudioEndTime = -1;
  mAudioCompleted = false;
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
  TimeRanges buffered;
  if (mDecoder->IsInfinite() &&
      NS_SUCCEEDED(mDecoder->GetBuffered(&buffered)))
  {
    uint32_t length = 0;
    buffered.GetLength(&length);
    if (length) {
      double end = 0;
      buffered.End(length - 1, &end);
      ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
      mEndTime = std::max<int64_t>(mEndTime, end * USECS_PER_S);
    }
  }
}

void MediaDecoderStateMachine::Seek(double aTime)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  // We need to be able to seek both at a transport level and at a media level
  // to seek.
  if (!mMediaSeekable) {
    return;
  }
  // MediaDecoder::mPlayState should be SEEKING while we seek, and
  // in that case MediaDecoder shouldn't be calling us.
  NS_ASSERTION(mState != DECODER_STATE_SEEKING,
               "We shouldn't already be seeking");
  NS_ASSERTION(mState >= DECODER_STATE_DECODING,
               "We should have loaded metadata");
  double t = aTime * static_cast<double>(USECS_PER_S);
  if (t > INT64_MAX) {
    // Prevent integer overflow.
    return;
  }

  mSeekTime = static_cast<int64_t>(t) + mStartTime;
  NS_ASSERTION(mSeekTime >= mStartTime && mSeekTime <= mEndTime,
               "Can only seek in range [0,duration]");

  // Bound the seek time to be inside the media range.
  NS_ASSERTION(mStartTime != -1, "Should know start time by now");
  NS_ASSERTION(mEndTime != -1, "Should know end time by now");
  mSeekTime = std::min(mSeekTime, mEndTime);
  mSeekTime = std::max(mStartTime, mSeekTime);
  mBasePosition = mSeekTime - mStartTime;
  LOG(PR_LOG_DEBUG, ("%p Changed state to SEEKING (to %f)", mDecoder.get(), aTime));
  mState = DECODER_STATE_SEEKING;
  if (mDecoder->GetDecodedStream()) {
    mDecoder->RecreateDecodedStream(mSeekTime - mStartTime);
  }
  ScheduleStateMachine();
}

void MediaDecoderStateMachine::StopDecodeThread()
{
  NS_ASSERTION(OnStateMachineThread(), "Should be on state machine thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  if (mRequestedNewDecodeThread) {
    // We've requested that the decode be created, but it hasn't been yet.
    // Cancel that request.
    NS_ASSERTION(!mDecodeThread,
      "Shouldn't have a decode thread until after request processed");
    StateMachineTracker::Instance().CancelCreateDecodeThread(this);
    mRequestedNewDecodeThread = false;
  }
  mStopDecodeThread = true;
  mDecoder->GetReentrantMonitor().NotifyAll();
  if (mDecodeThread) {
    LOG(PR_LOG_DEBUG, ("%p Shutdown decode thread", mDecoder.get()));
    {
      ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
      mDecodeThread->Shutdown();
      StateMachineTracker::Instance().NoteDecodeThreadDestroyed();
    }
    mDecodeThread = nullptr;
    mDecodeThreadIdle = false;
  }
  NS_ASSERTION(!mRequestedNewDecodeThread,
    "Any pending requests for decode threads must be canceled and unflagged");
  NS_ASSERTION(!StateMachineTracker::Instance().IsQueued(this),
    "Any pending requests for decode threads must be canceled");
}

void MediaDecoderStateMachine::StopAudioThread()
{
  NS_ASSERTION(OnDecodeThread() ||
               OnStateMachineThread(), "Should be on decode thread or state machine thread");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  if (mStopAudioThread) {
    // Nothing to do, since the thread is already stopping
    return;
  }

  mStopAudioThread = true;
  mDecoder->GetReentrantMonitor().NotifyAll();
  if (mAudioThread) {
    LOG(PR_LOG_DEBUG, ("%p Shutdown audio thread", mDecoder.get()));
    {
      ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
      mAudioThread->Shutdown();
    }
    mAudioThread = nullptr;
    // Now that the audio thread is dead, try sending data to our MediaStream(s).
    // That may have been waiting for the audio thread to stop.
    SendStreamData();
  }
}

nsresult
MediaDecoderStateMachine::ScheduleDecodeThread()
{
  NS_ASSERTION(OnStateMachineThread(), "Should be on state machine thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  mStopDecodeThread = false;
  if (mState >= DECODER_STATE_COMPLETED) {
    return NS_OK;
  }
  if (mDecodeThread) {
    NS_ASSERTION(!mRequestedNewDecodeThread,
      "Shouldn't have requested new decode thread when we have a decode thread");
    // We already have a decode thread...
    if (mDecodeThreadIdle) {
      // ... and it's not been shutdown yet, wake it up.
      nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethod(this, &MediaDecoderStateMachine::DecodeThreadRun);
      mDecodeThread->Dispatch(event, NS_DISPATCH_NORMAL);
      mDecodeThreadIdle = false;
    }
    return NS_OK;
  } else if (!mRequestedNewDecodeThread) {
  // We don't already have a decode thread, request a new one.
    mRequestedNewDecodeThread = true;
    ReentrantMonitorAutoExit mon(mDecoder->GetReentrantMonitor());
    StateMachineTracker::Instance().RequestCreateDecodeThread(this);
  }
  return NS_OK;
}

nsresult
MediaDecoderStateMachine::StartDecodeThread()
{
  NS_ASSERTION(StateMachineTracker::Instance().GetDecodeThreadCount() <
               StateMachineTracker::MAX_DECODE_THREADS,
               "Should not have reached decode thread limit");

  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  NS_ASSERTION(!StateMachineTracker::Instance().IsQueued(this),
    "Should not already have a pending request for a new decode thread.");
  NS_ASSERTION(OnStateMachineThread(), "Should be on state machine thread.");
  NS_ASSERTION(!mDecodeThread, "Should not have decode thread yet");
  NS_ASSERTION(mRequestedNewDecodeThread, "Should have requested this...");

  mRequestedNewDecodeThread = false;

  nsresult rv = NS_NewNamedThread("Media Decode",
                                  getter_AddRefs(mDecodeThread),
                                  nullptr,
                                  MEDIA_THREAD_STACK_SIZE);
  if (NS_FAILED(rv)) {
    // Give up, report error to media element.
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(mDecoder, &MediaDecoder::DecodeError);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
    return rv;
  }

  nsCOMPtr<nsIRunnable> event =
    NS_NewRunnableMethod(this, &MediaDecoderStateMachine::DecodeThreadRun);
  mDecodeThread->Dispatch(event, NS_DISPATCH_NORMAL);
  mDecodeThreadIdle = false;

  return NS_OK;
}

nsresult
MediaDecoderStateMachine::StartAudioThread()
{
  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine or decode thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  if (mAudioCaptured) {
    NS_ASSERTION(mStopAudioThread, "mStopAudioThread must always be true if audio is captured");
    return NS_OK;
  }

  mStopAudioThread = false;
  if (HasAudio() && !mAudioThread) {
    nsresult rv = NS_NewNamedThread("Media Audio",
                                    getter_AddRefs(mAudioThread),
                                    nullptr,
                                    MEDIA_THREAD_STACK_SIZE);
    if (NS_FAILED(rv)) {
      LOG(PR_LOG_DEBUG, ("%p Changed state to SHUTDOWN because failed to create audio thread", mDecoder.get()));
      mState = DECODER_STATE_SHUTDOWN;
      return rv;
    }

    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &MediaDecoderStateMachine::AudioLoop);
    mAudioThread->Dispatch(event, NS_DISPATCH_NORMAL);
  }
  return NS_OK;
}

int64_t MediaDecoderStateMachine::AudioDecodedUsecs() const
{
  NS_ASSERTION(HasAudio(),
               "Should only call AudioDecodedUsecs() when we have audio");
  // The amount of audio we have decoded is the amount of audio data we've
  // already decoded and pushed to the hardware, plus the amount of audio
  // data waiting to be pushed to the hardware.
  int64_t pushed = (mAudioEndTime != -1) ? (mAudioEndTime - GetMediaTime()) : 0;
  return pushed + mReader->AudioQueue().Duration();
}

bool MediaDecoderStateMachine::HasLowDecodedData(int64_t aAudioUsecs) const
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  // We consider ourselves low on decoded data if we're low on audio,
  // provided we've not decoded to the end of the audio stream, or
  // if we're only playing video and we're low on video frames, provided
  // we've not decoded to the end of the video stream.
  return ((HasAudio() &&
           !mReader->AudioQueue().IsFinished() &&
           AudioDecodedUsecs() < aAudioUsecs)
          ||
         (!HasAudio() &&
          HasVideo() &&
          !mReader->VideoQueue().IsFinished() &&
          static_cast<uint32_t>(mReader->VideoQueue().GetSize()) < LOW_VIDEO_FRAMES));
}

bool MediaDecoderStateMachine::HasLowUndecodedData() const
{
  return GetUndecodedData() < mLowDataThresholdUsecs;
}

int64_t MediaDecoderStateMachine::GetUndecodedData() const
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  NS_ASSERTION(mState > DECODER_STATE_DECODING_METADATA,
               "Must have loaded metadata for GetBuffered() to work");
  TimeRanges buffered;

  nsresult res = mDecoder->GetBuffered(&buffered);
  NS_ENSURE_SUCCESS(res, 0);
  double currentTime = GetCurrentTime();

  nsIDOMTimeRanges* r = static_cast<nsIDOMTimeRanges*>(&buffered);
  uint32_t length = 0;
  res = r->GetLength(&length);
  NS_ENSURE_SUCCESS(res, 0);

  for (uint32_t index = 0; index < length; ++index) {
    double start, end;
    res = r->Start(index, &start);
    NS_ENSURE_SUCCESS(res, 0);

    res = r->End(index, &end);
    NS_ENSURE_SUCCESS(res, 0);

    if (start <= currentTime && end >= currentTime) {
      return static_cast<int64_t>((end - currentTime) * USECS_PER_S);
    }
  }
  return 0;
}

void MediaDecoderStateMachine::SetFrameBufferLength(uint32_t aLength)
{
  NS_ASSERTION(aLength >= 512 && aLength <= 16384,
               "The length must be between 512 and 16384");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  mEventManager.SetSignalBufferLength(aLength);
}

nsresult MediaDecoderStateMachine::DecodeMetadata()
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  NS_ASSERTION(mState == DECODER_STATE_DECODING_METADATA,
               "Only call when in metadata decoding state");

  LOG(PR_LOG_DEBUG, ("%p Decoding Media Headers", mDecoder.get()));
  nsresult res;
  VideoInfo info;
  MetadataTags* tags;
  {
    ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
    res = mReader->ReadMetadata(&info, &tags);
  }
  if (NS_SUCCEEDED(res) && (mState == DECODER_STATE_DECODING_METADATA) && (mReader->IsWaitingMediaResources())) {
    // change state to DECODER_STATE_WAIT_FOR_RESOURCES
    StartWaitForResources();
    return NS_OK;
  }

  mInfo = info;

  if (NS_FAILED(res) || (!info.mHasVideo && !info.mHasAudio)) {
    // Dispatch the event to call DecodeError synchronously. This ensures
    // we're in shutdown state by the time we exit the decode thread.
    // If we just moved to shutdown state here on the decode thread, we may
    // cause the state machine to shutdown/free memory without closing its
    // media stream properly, and we'll get callbacks from the media stream
    // causing a crash. Note the state machine shutdown joins this decode
    // thread during shutdown (and other state machines can run on the state
    // machine thread while the join is waiting), so it's safe to do this
    // synchronously.
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(mDecoder, &MediaDecoder::DecodeError);
    ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
    NS_DispatchToMainThread(event, NS_DISPATCH_SYNC);
    return NS_ERROR_FAILURE;
  }
  mDecoder->StartProgressUpdates();
  mGotDurationFromMetaData = (GetDuration() != -1);

  VideoData* videoData = FindStartTime();
  if (videoData) {
    ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
    RenderVideoFrame(videoData, TimeStamp::Now());
  }

  if (mState == DECODER_STATE_SHUTDOWN) {
    return NS_ERROR_FAILURE;
  }

  NS_ASSERTION(mStartTime != -1, "Must have start time");
  MOZ_ASSERT((!HasVideo() && !HasAudio()) ||
              !(mMediaSeekable && mTransportSeekable) || mEndTime != -1,
              "Active seekable media should have end time");
  MOZ_ASSERT(!(mMediaSeekable && mTransportSeekable) ||
             GetDuration() != -1, "Seekable media should have duration");
  LOG(PR_LOG_DEBUG, ("%p Media goes from %lld to %lld (duration %lld)"
                     " transportSeekable=%d, mediaSeekable=%d",
                     mDecoder.get(), mStartTime, mEndTime, GetDuration(),
                     mTransportSeekable, mMediaSeekable));

  // Inform the element that we've loaded the metadata and the first frame,
  // setting the default framebuffer size for audioavailable events.  Also,
  // if there is audio, let the MozAudioAvailable event manager know about
  // the metadata.
  if (HasAudio()) {
    mEventManager.Init(mInfo.mAudioChannels, mInfo.mAudioRate);
    // Set the buffer length at the decoder level to be able, to be able
    // to retrive the value via media element method. The RequestFrameBufferLength
    // will call the MediaDecoderStateMachine::SetFrameBufferLength().
    uint32_t frameBufferLength = mInfo.mAudioChannels * FRAMEBUFFER_LENGTH_PER_CHANNEL;
    mDecoder->RequestFrameBufferLength(frameBufferLength);
  }

  nsCOMPtr<nsIRunnable> metadataLoadedEvent =
    new AudioMetadataEventRunner(mDecoder,
                                 mInfo.mAudioChannels,
                                 mInfo.mAudioRate,
                                 HasAudio(),
                                 HasVideo(),
                                 tags);
  NS_DispatchToMainThread(metadataLoadedEvent, NS_DISPATCH_NORMAL);

  if (mState == DECODER_STATE_DECODING_METADATA) {
    LOG(PR_LOG_DEBUG, ("%p Changed state from DECODING_METADATA to DECODING", mDecoder.get()));
    StartDecoding();
  }

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
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  NS_ASSERTION(mState == DECODER_STATE_SEEKING,
               "Only call when in seeking state");

  mDidThrottleAudioDecoding = false;
  mDidThrottleVideoDecoding = false;

  // During the seek, don't have a lock on the decoder state,
  // otherwise long seek operations can block the main thread.
  // The events dispatched to the main thread are SYNC calls.
  // These calls are made outside of the decode monitor lock so
  // it is safe for the main thread to makes calls that acquire
  // the lock since it won't deadlock. We check the state when
  // acquiring the lock again in case shutdown has occurred
  // during the time when we didn't have the lock.
  int64_t seekTime = mSeekTime;
  mDecoder->StopProgressUpdates();

  bool currentTimeChanged = false;
  int64_t mediaTime = GetMediaTime();
  if (mediaTime != seekTime) {
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

  if (currentTimeChanged) {
    // The seek target is different than the current playback position,
    // we'll need to seek the playback position, so shutdown our decode
    // and audio threads.
    StopAudioThread();
    ResetPlayback();
    nsresult res;
    {
      ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
      // Now perform the seek. We must not hold the state machine monitor
      // while we seek, since the seek reads, which could block on I/O.
      res = mReader->Seek(seekTime,
                          mStartTime,
                          mEndTime,
                          mediaTime);
    }
    if (NS_SUCCEEDED(res)) {
      AudioData* audio = HasAudio() ? mReader->AudioQueue().PeekFront() : nullptr;
      NS_ASSERTION(!audio || (audio->mTime <= seekTime &&
                              seekTime <= audio->mTime + audio->mDuration),
                    "Seek target should lie inside the first audio block after seek");
      int64_t startTime = (audio && audio->mTime < seekTime) ? audio->mTime : seekTime;
      mAudioStartTime = startTime;
      mPlayDuration = startTime - mStartTime;
      if (HasVideo()) {
        VideoData* video = mReader->VideoQueue().PeekFront();
        if (video) {
          NS_ASSERTION((video->mTime <= seekTime && seekTime <= video->mEndTime) ||
                        mReader->VideoQueue().IsFinished(),
            "Seek target should lie inside the first frame after seek, unless it's the last frame.");
          {
            ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
            RenderVideoFrame(video, TimeStamp::Now());
          }
          nsCOMPtr<nsIRunnable> event =
            NS_NewRunnableMethod(mDecoder, &MediaDecoder::Invalidate);
          NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
        }
      }
    }
  }
  mDecoder->StartProgressUpdates();
  if (mState == DECODER_STATE_DECODING_METADATA ||
      mState == DECODER_STATE_DORMANT ||
      mState == DECODER_STATE_SHUTDOWN) {
    return;
  }

  // Try to decode another frame to detect if we're at the end...
  LOG(PR_LOG_DEBUG, ("%p Seek completed, mCurrentFrameTime=%lld\n",
      mDecoder.get(), mCurrentFrameTime));

  // Change state to DECODING or COMPLETED now. SeekingStopped will
  // call MediaDecoderStateMachine::Seek to reset our state to SEEKING
  // if we need to seek again.

  nsCOMPtr<nsIRunnable> stopEvent;
  bool isLiveStream = mDecoder->GetResource()->GetLength() == -1;
  if (GetMediaTime() == mEndTime && !isLiveStream) {
    // Seeked to end of media, move to COMPLETED state. Note we don't do
    // this if we're playing a live stream, since the end of media will advance
    // once we download more data!
    LOG(PR_LOG_DEBUG, ("%p Changed state from SEEKING (to %lld) to COMPLETED",
                        mDecoder.get(), seekTime));
    stopEvent = NS_NewRunnableMethod(mDecoder, &MediaDecoder::SeekingStoppedAtEnd);
    mState = DECODER_STATE_COMPLETED;
  } else {
    LOG(PR_LOG_DEBUG, ("%p Changed state from SEEKING (to %lld) to DECODING",
                        mDecoder.get(), seekTime));
    stopEvent = NS_NewRunnableMethod(mDecoder, &MediaDecoder::SeekingStopped);
    StartDecoding();
  }
  {
    ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
    NS_DispatchToMainThread(stopEvent, NS_DISPATCH_SYNC);
  }

  // Reset quick buffering status. This ensures that if we began the
  // seek while quick-buffering, we won't bypass quick buffering mode
  // if we need to buffer after the seek.
  mQuickBuffering = false;

  ScheduleStateMachine();
}

// Runnable to dispose of the decoder and state machine on the main thread.
class nsDecoderDisposeEvent : public nsRunnable {
public:
  nsDecoderDisposeEvent(already_AddRefed<MediaDecoder> aDecoder,
                        already_AddRefed<MediaDecoderStateMachine> aStateMachine)
    : mDecoder(aDecoder), mStateMachine(aStateMachine) {}
  NS_IMETHOD Run() {
    NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");
    mStateMachine->ReleaseDecoder();
    mDecoder->ReleaseStateMachine();
    mStateMachine = nullptr;
    mDecoder = nullptr;
    return NS_OK;
  }
private:
  nsRefPtr<MediaDecoder> mDecoder;
  nsCOMPtr<MediaDecoderStateMachine> mStateMachine;
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
  nsCOMPtr<MediaDecoderStateMachine> mStateMachine;
};

nsresult MediaDecoderStateMachine::RunStateMachine()
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  MediaResource* resource = mDecoder->GetResource();
  NS_ENSURE_TRUE(resource, NS_ERROR_NULL_POINTER);

  switch (mState) {
    case DECODER_STATE_SHUTDOWN: {
      if (IsPlaying()) {
        StopPlayback();
      }
      StopAudioThread();
      // If mAudioThread is non-null after StopAudioThread completes, we are
      // running in a nested event loop waiting for Shutdown() on
      // mAudioThread to complete.  Return to the event loop and let it
      // finish processing before continuing with shutdown.
      if (mAudioThread) {
        MOZ_ASSERT(mStopAudioThread);
        return NS_OK;
      }
      StopDecodeThread();
      // Now that those threads are stopped, there's no possibility of
      // mPendingWakeDecoder being needed again. Revoke it.
      mPendingWakeDecoder = nullptr;
      {
        ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
        mReader->ReleaseMediaResources();
      }
      NS_ASSERTION(mState == DECODER_STATE_SHUTDOWN,
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
      NS_DispatchToCurrentThread(new nsDispatchDisposeEvent(mDecoder, this));
      return NS_OK;
    }

    case DECODER_STATE_DORMANT: {
      if (IsPlaying()) {
        StopPlayback();
      }
      StopAudioThread();
      StopDecodeThread();
      // Now that those threads are stopped, there's no possibility of
      // mPendingWakeDecoder being needed again. Revoke it.
      mPendingWakeDecoder = nullptr;
      {
        ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
        mReader->ReleaseMediaResources();
      }
      return NS_OK;
    }

    case DECODER_STATE_WAIT_FOR_RESOURCES: {
      return NS_OK;
    }

    case DECODER_STATE_DECODING_METADATA: {
      // Ensure we have a decode thread to decode metadata.
      return ScheduleDecodeThread();
    }

    case DECODER_STATE_DECODING: {
      if (mDecoder->GetState() != MediaDecoder::PLAY_STATE_PLAYING &&
          IsPlaying())
      {
        // We're playing, but the element/decoder is in paused state. Stop
        // playing! Note we do this before StopDecodeThread() below because
        // that blocks this state machine's execution, and can cause a
        // perceptible delay between the pause command, and playback actually
        // pausing.
        StopPlayback();
      }

      if (mDecoder->GetState() == MediaDecoder::PLAY_STATE_PLAYING &&
          !IsPlaying()) {
        // We are playing, but the state machine does not know it yet. Tell it
        // that it is, so that the clock can be properly queried.
        StartPlayback();
      }

      if (IsPausedAndDecoderWaiting()) {
        // The decode buffers are full, and playback is paused. Shutdown the
        // decode thread.
        StopDecodeThread();
        return NS_OK;
      }

      // We're playing and/or our decode buffers aren't full. Ensure we have
      // an active decode thread.
      if (NS_FAILED(ScheduleDecodeThread())) {
        NS_WARNING("Failed to start media decode thread!");
        return NS_ERROR_FAILURE;
      }

      AdvanceFrame();
      NS_ASSERTION(mDecoder->GetState() != MediaDecoder::PLAY_STATE_PLAYING ||
                   IsStateMachineScheduled() ||
                   mPlaybackRate == 0.0, "Must have timer scheduled");
      return NS_OK;
    }

    case DECODER_STATE_BUFFERING: {
      if (IsPausedAndDecoderWaiting()) {
        // The decode buffers are full, and playback is paused. Shutdown the
        // decode thread.
        StopDecodeThread();
        return NS_OK;
      }

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
                            : (GetUndecodedData() < mBufferingWait * mPlaybackRate * USECS_PER_S)) &&
            !mDecoder->IsDataCachedToEndOfResource() &&
            !resource->IsSuspended())
      {
        LOG(PR_LOG_DEBUG,
            ("%p Buffering: %.3lfs/%ds, timeout in %.3lfs %s",
              mDecoder.get(),
              GetUndecodedData() / static_cast<double>(USECS_PER_S),
              mBufferingWait,
              mBufferingWait - elapsed.ToSeconds(),
              (mQuickBuffering ? "(quick exit)" : "")));
        ScheduleStateMachine(USECS_PER_S);
        return NS_OK;
      } else {
        LOG(PR_LOG_DEBUG, ("%p Changed state from BUFFERING to DECODING", mDecoder.get()));
        LOG(PR_LOG_DEBUG, ("%p Buffered for %.3lfs",
                            mDecoder.get(),
                            (now - mBufferingStart).ToSeconds()));
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
      // Ensure we have a decode thread to perform the seek.
     return ScheduleDecodeThread();
    }

    case DECODER_STATE_COMPLETED: {
      StopDecodeThread();

      if (mState != DECODER_STATE_COMPLETED) {
        // While we're waiting for the decode thread to shutdown, we can
        // change state, for example to seeking or shutdown state.
        // Whatever changed our state should have scheduled another state
        // machine run.
        NS_ASSERTION(IsStateMachineScheduled(), "Must have timer scheduled");
        return NS_OK;
      }

      // Play the remaining media. We want to run AdvanceFrame() at least
      // once to ensure the current playback position is advanced to the
      // end of the media, and so that we update the readyState.
      if (mState == DECODER_STATE_COMPLETED &&
          (mReader->VideoQueue().GetSize() > 0 ||
          (HasAudio() && !mAudioCompleted)))
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
      if (mDecoder->GetState() == MediaDecoder::PLAY_STATE_PLAYING) {
        int64_t videoTime = HasVideo() ? mVideoFrameEndTime : 0;
        int64_t clockTime = std::max(mEndTime, std::max(videoTime, GetAudioClock()));
        UpdatePlaybackPosition(clockTime);
        nsCOMPtr<nsIRunnable> event =
          NS_NewRunnableMethod(mDecoder, &MediaDecoder::PlaybackEnded);
        NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
      }
      return NS_OK;
    }
  }

  return NS_OK;
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

  if (!PR_GetEnv("MOZ_QUIET")) {
    LOG(PR_LOG_DEBUG, ("%p Decoder playing video frame %lld",
                       mDecoder.get(), aData->mTime));
  }

  VideoFrameContainer* container = mDecoder->GetVideoFrameContainer();
  if (container) {
    container->SetCurrentFrame(aData->mDisplay, aData->mImage, aTarget);
  }
}

int64_t
MediaDecoderStateMachine::GetAudioClock()
{
  NS_ASSERTION(OnStateMachineThread(), "Should be on state machine thread.");
  // We must hold the decoder monitor while using the audio stream off the
  // audio thread to ensure that it doesn't get destroyed on the audio thread
  // while we're using it.
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  if (!HasAudio() || mAudioCaptured)
    return -1;
  if (!mAudioStream) {
    // Audio thread hasn't played any data yet.
    return mAudioStartTime;
  }
  int64_t t = mAudioStream->GetPosition();
  return (t == -1) ? -1 : t + mAudioStartTime;
}

int64_t MediaDecoderStateMachine::GetVideoStreamPosition()
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  if (!IsPlaying()) {
    return mPlayDuration + mStartTime;
  }

  // The playbackRate has been just been changed, reset the playstartTime.
  if (mResetPlayStartTime) {
    mPlayStartTime = TimeStamp::Now();
    mResetPlayStartTime = false;
  }

  int64_t pos = DurationToUsecs(TimeStamp::Now() - mPlayStartTime) + mPlayDuration;
  pos -= mBasePosition;
  NS_ASSERTION(pos >= 0, "Video stream position should be positive.");
  return mBasePosition + pos * mPlaybackRate + mStartTime;
}

int64_t MediaDecoderStateMachine::GetClock() {
  NS_ASSERTION(OnStateMachineThread(), "Should be on state machine thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  // Determine the clock time. If we've got audio, and we've not reached
  // the end of the audio, use the audio clock. However if we've finished
  // audio, or don't have audio, use the system clock.
  int64_t clock_time = -1;
  if (!IsPlaying()) {
    clock_time = mPlayDuration + mStartTime;
  } else {
    int64_t audio_time = GetAudioClock();
    if (HasAudio() && !mAudioCompleted && audio_time != -1) {
      clock_time = audio_time;
      // Resync against the audio clock, while we're trusting the
      // audio clock. This ensures no "drift", particularly on Linux.
      mPlayDuration = clock_time - mStartTime;
      mPlayStartTime = TimeStamp::Now();
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
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
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
#ifdef PR_LOGGING
  int32_t droppedFrames = 0;
#endif
  if (mReader->VideoQueue().GetSize() > 0) {
    VideoData* frame = mReader->VideoQueue().PeekFront();
    while (mRealTime || clock_time >= frame->mTime) {
      mVideoFrameEndTime = frame->mEndTime;
      currentFrame = frame;
#ifdef PR_LOGGING
      if (!PR_GetEnv("MOZ_QUIET")) {
        LOG(PR_LOG_DEBUG, ("%p Decoder discarding video frame %lld", mDecoder.get(), frame->mTime));
        if (droppedFrames++) {
          LOG(PR_LOG_DEBUG, ("%p Decoder discarding video frame %lld (%d so far)",
            mDecoder.get(), frame->mTime, droppedFrames - 1));
        }
      }
#endif
      mReader->VideoQueue().PopFront();
      // Notify the decode thread that the video queue's buffers may have
      // free'd up space for more frames.
      mDecoder->GetReentrantMonitor().NotifyAll();
      mDecoder->UpdatePlaybackOffset(frame->mOffset);
      if (mReader->VideoQueue().GetSize() == 0)
        break;
      frame = mReader->VideoQueue().PeekFront();
    }
    // Current frame has already been presented, wait until it's time to
    // present the next frame.
    if (frame && !currentFrame) {
      int64_t now = IsPlaying() ? clock_time : mPlayDuration;

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
      !resource->IsSuspended() &&
      (JustExitedQuickBuffering() || HasLowUndecodedData()))
  {
    if (currentFrame) {
      mReader->VideoQueue().PushFront(currentFrame.forget());
    }
    StartBuffering();
    ScheduleStateMachine();
    return;
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
    {
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
    double frameDelay = double(clock_time - currentFrame->mTime) / USECS_PER_S;
    NS_ASSERTION(frameDelay >= 0.0, "Frame should never be displayed early.");
    frameStats.NotifyFrameDelay(frameDelay);
    remainingTime = currentFrame->mEndTime - clock_time;
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

void MediaDecoderStateMachine::Wait(int64_t aUsecs) {
  NS_ASSERTION(OnAudioThread(), "Only call on the audio thread");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  TimeStamp end = TimeStamp::Now() + UsecsToDuration(std::max<int64_t>(USECS_PER_MS, aUsecs));
  TimeStamp now;
  while ((now = TimeStamp::Now()) < end &&
         mState != DECODER_STATE_SHUTDOWN &&
         mState != DECODER_STATE_SEEKING &&
         !mStopAudioThread &&
         IsPlaying())
  {
    int64_t ms = static_cast<int64_t>(NS_round((end - now).ToSeconds() * 1000));
    if (ms == 0 || ms > UINT32_MAX) {
      break;
    }
    mDecoder->GetReentrantMonitor().Wait(PR_MillisecondsToInterval(static_cast<uint32_t>(ms)));
  }
}

VideoData* MediaDecoderStateMachine::FindStartTime()
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  int64_t startTime = 0;
  mStartTime = 0;
  VideoData* v = nullptr;
  {
    ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
    v = mReader->FindStartTime(startTime);
  }
  if (startTime != 0) {
    mStartTime = startTime;
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
  LOG(PR_LOG_DEBUG, ("%p Media start time is %lld", mDecoder.get(), mStartTime));
  return v;
}

void MediaDecoderStateMachine::UpdateReadyState() {
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

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
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}

bool MediaDecoderStateMachine::JustExitedQuickBuffering()
{
  return !mDecodeStartTime.IsNull() &&
    mQuickBuffering &&
    (TimeStamp::Now() - mDecodeStartTime) < TimeDuration::FromMicroseconds(QUICK_BUFFER_THRESHOLD_USECS);
}

void MediaDecoderStateMachine::StartBuffering()
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

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
  LOG(PR_LOG_DEBUG, ("%p Changed state from DECODING to BUFFERING, decoded for %.3lfs",
                     mDecoder.get(), decodeDuration.ToSeconds()));
#ifdef PR_LOGGING
  MediaDecoder::Statistics stats = mDecoder->GetStatistics();
#endif
  LOG(PR_LOG_DEBUG, ("%p Playback rate: %.1lfKB/s%s download rate: %.1lfKB/s%s",
    mDecoder.get(),
    stats.mPlaybackRate/1024, stats.mPlaybackRateReliable ? "" : " (unreliable)",
    stats.mDownloadRate/1024, stats.mDownloadRateReliable ? "" : " (unreliable)"));
}

nsresult MediaDecoderStateMachine::GetBuffered(TimeRanges* aBuffered) {
  MediaResource* resource = mDecoder->GetResource();
  NS_ENSURE_TRUE(resource, NS_ERROR_FAILURE);
  resource->Pin();
  nsresult res = mReader->GetBuffered(aBuffered, mStartTime);
  resource->Unpin();
  return res;
}

bool MediaDecoderStateMachine::IsPausedAndDecoderWaiting() {
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  NS_ASSERTION(OnStateMachineThread(), "Should be on state machine thread.");

  return
    mDecodeThreadWaiting &&
    mDecoder->GetState() != MediaDecoder::PLAY_STATE_PLAYING &&
    (mState == DECODER_STATE_DECODING || mState == DECODER_STATE_BUFFERING);
}

nsresult MediaDecoderStateMachine::Run()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  NS_ASSERTION(OnStateMachineThread(), "Should be on state machine thread.");

  return CallRunStateMachine();
}

nsresult MediaDecoderStateMachine::CallRunStateMachine()
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  NS_ASSERTION(OnStateMachineThread(), "Should be on state machine thread.");
  // This will be set to true by ScheduleStateMachine() if it's called
  // while we're in RunStateMachine().
  mRunAgain = false;

  // Set to true whenever we dispatch an event to run this state machine.
  // This flag prevents us from dispatching
  mDispatchedRunEvent = false;

  // If audio is being captured, stop the audio thread if it's running
  if (mAudioCaptured) {
    StopAudioThread();
  }

  mTimeout = TimeStamp();

  mIsRunning = true;
  nsresult res = RunStateMachine();
  mIsRunning = false;

  if (mRunAgain && !mDispatchedRunEvent) {
    mDispatchedRunEvent = true;
    return NS_DispatchToCurrentThread(this);
  }

  return res;
}

static void TimeoutExpired(nsITimer *aTimer, void *aClosure) {
  MediaDecoderStateMachine *machine =
    static_cast<MediaDecoderStateMachine*>(aClosure);
  NS_ASSERTION(machine, "Must have been passed state machine");
  machine->TimeoutExpired();
}

void MediaDecoderStateMachine::TimeoutExpired()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  NS_ASSERTION(OnStateMachineThread(), "Must be on state machine thread");
  if (mIsRunning) {
    mRunAgain = true;
  } else if (!mDispatchedRunEvent) {
    // We don't have an event dispatched to run the state machine, so we
    // can just run it from here.
    CallRunStateMachine();
  }
  // Otherwise, an event has already been dispatched to run the state machine
  // as soon as possible. Nothing else needed to do, the state machine is
  // going to run anyway.
}

void MediaDecoderStateMachine::ScheduleStateMachineWithLockAndWakeDecoder() {
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  mon.NotifyAll();
  ScheduleStateMachine();
}

nsresult MediaDecoderStateMachine::ScheduleStateMachine(int64_t aUsecs) {
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  NS_ABORT_IF_FALSE(GetStateMachineThread(),
    "Must have a state machine thread to schedule");

  if (mState == DECODER_STATE_SHUTDOWN) {
    return NS_ERROR_FAILURE;
  }
  aUsecs = std::max<int64_t>(aUsecs, 0);

  TimeStamp timeout = TimeStamp::Now() + UsecsToDuration(aUsecs);
  if (!mTimeout.IsNull()) {
    if (timeout >= mTimeout) {
      // We've already scheduled a timer set to expire at or before this time,
      // or have an event dispatched to run the state machine.
      return NS_OK;
    }
    if (mTimer) {
      // We've been asked to schedule a timer to run before an existing timer.
      // Cancel the existing timer.
      mTimer->Cancel();
    }
  }

  uint32_t ms = static_cast<uint32_t>((aUsecs / USECS_PER_MS) & 0xFFFFFFFF);
  if (mRealTime && ms > 40)
    ms = 40;
  if (ms == 0) {
    if (mIsRunning) {
      // We're currently running this state machine on the state machine
      // thread. Signal it to run again once it finishes its current cycle.
      mRunAgain = true;
      return NS_OK;
    } else if (!mDispatchedRunEvent) {
      // We're not currently running this state machine on the state machine
      // thread. Dispatch an event to run one cycle of the state machine.
      mDispatchedRunEvent = true;
      return GetStateMachineThread()->Dispatch(this, NS_DISPATCH_NORMAL);
    }
    // We're not currently running this state machine on the state machine
    // thread, but something has already dispatched an event to run it again,
    // so just exit; it's going to run real soon.
    return NS_OK;
  }

  mTimeout = timeout;

  nsresult res;
  if (!mTimer) {
    mTimer = do_CreateInstance("@mozilla.org/timer;1", &res);
    if (NS_FAILED(res)) return res;
    mTimer->SetTarget(GetStateMachineThread());
  }

  res = mTimer->InitWithFuncCallback(mozilla::TimeoutExpired,
                                     this,
                                     ms,
                                     nsITimer::TYPE_ONE_SHOT);
  return res;
}

bool MediaDecoderStateMachine::OnStateMachineThread() const
{
    return IsCurrentThread(GetStateMachineThread());
}

nsIThread* MediaDecoderStateMachine::GetStateMachineThread()
{
  return StateMachineTracker::Instance().GetGlobalStateMachineThread();
}

void MediaDecoderStateMachine::NotifyAudioAvailableListener()
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  mEventManager.NotifyAudioAvailableListener();
}

void MediaDecoderStateMachine::SetPlaybackRate(double aPlaybackRate)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ASSERTION(aPlaybackRate != 0,
      "PlaybackRate == 0 should be handled before this function.");
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  // We don't currently support more than two channels when changing playback
  // rate.
  if (mAudioStream && mAudioStream->GetChannels() > 2) {
    return;
  }

  if (mPlaybackRate == aPlaybackRate) {
    return;
  }

  // Get position of the last time we changed the rate.
  if (!HasAudio()) {
    // mBasePosition is a position in the video stream, not an absolute time.
    if (mState == DECODER_STATE_SEEKING) {
      mBasePosition = mSeekTime - mStartTime;
    } else {
      mBasePosition = GetVideoStreamPosition();
    }
    mPlayDuration = mBasePosition;
    mResetPlayStartTime = true;
    mPlayStartTime = TimeStamp::Now();
  }

  mPlaybackRate = aPlaybackRate;
}

void MediaDecoderStateMachine::SetPreservesPitch(bool aPreservesPitch)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  mPreservesPitch = aPreservesPitch;

  return;
}

bool MediaDecoderStateMachine::IsShutdown()
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  return GetState() == DECODER_STATE_SHUTDOWN;
}

void MediaDecoderStateMachine::QueueMetadata(int64_t aPublishTime,
                                             int aChannels,
                                             int aRate,
                                             bool aHasAudio,
                                             bool aHasVideo,
                                             MetadataTags* aTags)
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  TimedMetadata* metadata = new TimedMetadata;
  metadata->mPublishTime = aPublishTime;
  metadata->mChannels = aChannels;
  metadata->mRate = aRate;
  metadata->mHasAudio = aHasAudio;
  metadata->mHasVideo = aHasVideo;
  metadata->mTags = aTags;
  mMetadataManager.QueueMetadata(metadata);
}

} // namespace mozilla

