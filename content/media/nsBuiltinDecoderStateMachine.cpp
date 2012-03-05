/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: ML 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
 *  Chris Pearce <chris@pearce.org.nz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <limits>
#include "nsAudioStream.h"
#include "nsTArray.h"
#include "nsBuiltinDecoder.h"
#include "nsBuiltinDecoderReader.h"
#include "nsBuiltinDecoderStateMachine.h"
#include "mozilla/mozalloc.h"
#include "VideoUtils.h"
#include "nsTimeRanges.h"
#include "nsDeque.h"

#include "mozilla/Preferences.h"
#include "mozilla/StandardInteger.h"
#include "mozilla/Util.h"

using namespace mozilla;
using namespace mozilla::layers;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gBuiltinDecoderLog;
#define LOG(type, msg) PR_LOG(gBuiltinDecoderLog, type, msg)
#else
#define LOG(type, msg)
#endif

// Wait this number of milliseconds when buffering, then leave and play
// as best as we can if the required amount of data hasn't been
// retrieved.
static const PRUint32 BUFFERING_WAIT = 30000;

// If audio queue has less than this many usecs of decoded audio, we won't risk
// trying to decode the video, we'll skip decoding video up to the next
// keyframe. We may increase this value for an individual decoder if we
// encounter video frames which take a long time to decode.
static const PRUint32 LOW_AUDIO_USECS = 300000;

// If more than this many usecs of decoded audio is queued, we'll hold off
// decoding more audio. If we increase the low audio threshold (see
// LOW_AUDIO_USECS above) we'll also increase this value to ensure it's not
// less than the low audio threshold.
const PRInt64 AMPLE_AUDIO_USECS = 1000000;

// Maximum number of bytes we'll allocate and write at once to the audio
// hardware when the audio stream contains missing frames and we're
// writing silence in order to fill the gap. We limit our silence-writes
// to 32KB in order to avoid allocating an impossibly large chunk of
// memory if we encounter a large chunk of silence.
const PRUint32 SILENCE_BYTES_CHUNK = 32 * 1024;

// If we have fewer than LOW_VIDEO_FRAMES decoded frames, and
// we're not "pumping video", we'll skip the video up to the next keyframe
// which is at or after the current playback position.
static const PRUint32 LOW_VIDEO_FRAMES = 1;

// If we've got more than AMPLE_VIDEO_FRAMES decoded video frames waiting in
// the video queue, we will not decode any more video frames until some have
// been consumed by the play state machine thread.
static const PRUint32 AMPLE_VIDEO_FRAMES = 10;

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
static const PRInt64 LOW_DATA_THRESHOLD_USECS = 5000000;

// LOW_DATA_THRESHOLD_USECS needs to be greater than AMPLE_AUDIO_USECS, otherwise
// the skip-to-keyframe logic can activate when we're running low on data.
PR_STATIC_ASSERT(LOW_DATA_THRESHOLD_USECS > AMPLE_AUDIO_USECS);

// Amount of excess usecs of data to add in to the "should we buffer" calculation.
static const PRUint32 EXHAUSTED_DATA_MARGIN_USECS = 60000;

// If we enter buffering within QUICK_BUFFER_THRESHOLD_USECS seconds of starting
// decoding, we'll enter "quick buffering" mode, which exits a lot sooner than
// normal buffering mode. This exists so that if the decode-ahead exhausts the
// downloaded data while decode/playback is just starting up (for example
// after a seek while the media is still playing, or when playing a media
// as soon as it's load started), we won't necessarily stop for 30s and wait
// for buffering. We may actually be able to playback in this case, so exit
// buffering early and try to play. If it turns out we can't play, we'll fall
// back to buffering normally.
static const PRUint32 QUICK_BUFFER_THRESHOLD_USECS = 2000000;

// If we're quick buffering, we'll remain in buffering mode while we have less than
// QUICK_BUFFERING_LOW_DATA_USECS of decoded data available.
static const PRUint32 QUICK_BUFFERING_LOW_DATA_USECS = 1000000;

// If QUICK_BUFFERING_LOW_DATA_USECS is > AMPLE_AUDIO_USECS, we won't exit
// quick buffering in a timely fashion, as the decode pauses when it
// reaches AMPLE_AUDIO_USECS decoded data, and thus we'll never reach
// QUICK_BUFFERING_LOW_DATA_USECS.
PR_STATIC_ASSERT(QUICK_BUFFERING_LOW_DATA_USECS <= AMPLE_AUDIO_USECS);

static TimeDuration UsecsToDuration(PRInt64 aUsecs) {
  return TimeDuration::FromMilliseconds(static_cast<double>(aUsecs) / USECS_PER_MS);
}

static PRInt64 DurationToUsecs(TimeDuration aDuration) {
  return static_cast<PRInt64>(aDuration.ToSeconds() * USECS_PER_S);
}

class nsAudioMetadataEventRunner : public nsRunnable
{
private:
  nsCOMPtr<nsBuiltinDecoder> mDecoder;
public:
  nsAudioMetadataEventRunner(nsBuiltinDecoder* aDecoder, PRUint32 aChannels,
                             PRUint32 aRate) :
    mDecoder(aDecoder),
    mChannels(aChannels),
    mRate(aRate)
  {
  }

  NS_IMETHOD Run()
  {
    mDecoder->MetadataLoaded(mChannels, mRate);
    return NS_OK;
  }

  const PRUint32 mChannels;
  const PRUint32 mRate;
};

// Shuts down a thread asynchronously.
class ShutdownThreadEvent : public nsRunnable 
{
public:
  ShutdownThreadEvent(nsIThread* aThread) : mThread(aThread) {}
  ~ShutdownThreadEvent() {}
  NS_IMETHOD Run() {
    mThread->Shutdown();
    mThread = nsnull;
    return NS_OK;
  }
private:
  nsCOMPtr<nsIThread> mThread;
};

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
    mStateMachineThread(nsnull)
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
  // thread in the nsBuiltinDecoderStateMachine constructor resulting
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
  nsresult RequestCreateDecodeThread(nsBuiltinDecoderStateMachine* aStateMachine);

  // Cancels a request made by RequestCreateDecodeThread to create a decode
  // thread for aStateMachine.
  nsresult CancelCreateDecodeThread(nsBuiltinDecoderStateMachine* aStateMachine);

  // Maximum number of active decode threads allowed. When more
  // than this number are active the thread creation will fail.
  static const PRUint32 MAX_DECODE_THREADS = 25;

  // Returns the number of active decode threads.
  // Call on any thread. Holds the internal monitor so don't
  // call with any other monitor held to avoid deadlock.
  PRUint32 GetDecodeThreadCount();

  // Keep track of the fact that a decode thread was destroyed.
  // Call on any thread. Holds the internal monitor so don't
  // call with any other monitor held to avoid deadlock.
  void NoteDecodeThreadDestroyed();

#ifdef DEBUG
  // Returns true if aStateMachine has a pending request for a
  // decode thread.
  bool IsQueued(nsBuiltinDecoderStateMachine* aStateMachine);
#endif

private:
  // Holds global instance of StateMachineTracker.
  // Writable on main thread only.
  static StateMachineTracker* mInstance;

  // Reentrant monitor that must be obtained to access
  // the decode thread count member and methods.
  ReentrantMonitor mMonitor;

  // Number of instances of nsBuiltinDecoderStateMachine
  // that are currently instantiated. Access on the
  // main thread only.
  PRUint32 mStateMachineCount;

  // Number of instances of decoder threads that are
  // currently instantiated. Access only with the
  // mMonitor lock held. Can be used from any thread.
  PRUint32 mDecodeThreadCount;

  // Global state machine thread. Write on the main thread
  // only, read from the decoder threads. Synchronized via
  // the mMonitor.
  nsIThread* mStateMachineThread;

  // Queue of state machines waiting for decode threads. Entries at the front
  // get their threads first.
  nsDeque mPending;
};

StateMachineTracker* StateMachineTracker::mInstance = nsnull;

StateMachineTracker& StateMachineTracker::Instance()
{
  if (!mInstance) {
    NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
    mInstance = new StateMachineTracker();
  }
  return *mInstance;
}

void StateMachineTracker::EnsureGlobalStateMachine() 
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  ReentrantMonitorAutoEnter mon(mMonitor);
  if (mStateMachineCount == 0) {
    NS_ASSERTION(!mStateMachineThread, "Should have null state machine thread!");
    DebugOnly<nsresult> rv = NS_NewThread(&mStateMachineThread, nsnull);
    NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv), "Can't create media state machine thread");
  }
  mStateMachineCount++;
}

#ifdef DEBUG
bool StateMachineTracker::IsQueued(nsBuiltinDecoderStateMachine* aStateMachine)
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  PRInt32 size = mPending.GetSize();
  for (int i = 0; i < size; ++i) {
    nsBuiltinDecoderStateMachine* m =
      static_cast<nsBuiltinDecoderStateMachine*>(mPending.ObjectAt(i));
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
      mStateMachineThread = nsnull;
      NS_DispatchToMainThread(event);

      NS_ASSERTION(mDecodeThreadCount == 0, "Decode thread count must be zero.");
      mInstance = nsnull;
    }
    delete this;
  }
}

void StateMachineTracker::NoteDecodeThreadDestroyed()
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  --mDecodeThreadCount;
  while (mDecodeThreadCount < MAX_DECODE_THREADS && mPending.GetSize() > 0) {
    nsBuiltinDecoderStateMachine* m =
      static_cast<nsBuiltinDecoderStateMachine*>(mPending.PopFront());
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

PRUint32 StateMachineTracker::GetDecodeThreadCount()
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  return mDecodeThreadCount;
}

nsresult StateMachineTracker::CancelCreateDecodeThread(nsBuiltinDecoderStateMachine* aStateMachine) {
  ReentrantMonitorAutoEnter mon(mMonitor);
  PRInt32 size = mPending.GetSize();
  for (PRInt32 i = 0; i < size; ++i) {
    void* m = static_cast<nsBuiltinDecoderStateMachine*>(mPending.ObjectAt(i));
    if (m == aStateMachine) {
      mPending.RemoveObjectAt(i);
      break;
    }
  }
  NS_ASSERTION(!IsQueued(aStateMachine), "State machine should no longer have queued request.");
  return NS_OK;
}

nsresult StateMachineTracker::RequestCreateDecodeThread(nsBuiltinDecoderStateMachine* aStateMachine)
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

nsBuiltinDecoderStateMachine::nsBuiltinDecoderStateMachine(nsBuiltinDecoder* aDecoder,
                                                           nsBuiltinDecoderReader* aReader,
                                                           bool aRealTime) :
  mDecoder(aDecoder),
  mState(DECODER_STATE_DECODING_METADATA),
  mCbCrSize(0),
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
  mSeekable(true),
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
  mRequestedNewDecodeThread(false),
  mEventManager(aDecoder)
{
  MOZ_COUNT_CTOR(nsBuiltinDecoderStateMachine);
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  StateMachineTracker::Instance().EnsureGlobalStateMachine();

  // only enable realtime mode when "media.realtime_decoder.enabled" is true.
  if (Preferences::GetBool("media.realtime_decoder.enabled", false) == false)
    mRealTime = false;

  mBufferingWait = mRealTime ? 0 : BUFFERING_WAIT;
  mLowDataThresholdUsecs = mRealTime ? 0 : LOW_DATA_THRESHOLD_USECS;
}

nsBuiltinDecoderStateMachine::~nsBuiltinDecoderStateMachine()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  MOZ_COUNT_DTOR(nsBuiltinDecoderStateMachine);
  NS_ASSERTION(!StateMachineTracker::Instance().IsQueued(this),
    "Should not have a pending request for a new decode thread");
  NS_ASSERTION(!mRequestedNewDecodeThread,
    "Should not have (or flagged) a pending request for a new decode thread");
  if (mTimer)
    mTimer->Cancel();
  mTimer = nsnull;
 
  StateMachineTracker::Instance().CleanupGlobalStateMachine();
}

bool nsBuiltinDecoderStateMachine::HasFutureAudio() const {
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  NS_ASSERTION(HasAudio(), "Should only call HasFutureAudio() when we have audio");
  // We've got audio ready to play if:
  // 1. We've not completed playback of audio, and
  // 2. we either have more than the threshold of decoded audio available, or
  //    we've completely decoded all audio (but not finished playing it yet
  //    as per 1).
  return !mAudioCompleted &&
         (AudioDecodedUsecs() > LOW_AUDIO_USECS || mReader->mAudioQueue.IsFinished());
}

bool nsBuiltinDecoderStateMachine::HaveNextFrameData() const {
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  return (!HasAudio() || HasFutureAudio()) &&
         (!HasVideo() || mReader->mVideoQueue.GetSize() > 0);
}

PRInt64 nsBuiltinDecoderStateMachine::GetDecodedAudioDuration() {
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  PRInt64 audioDecoded = mReader->mAudioQueue.Duration();
  if (mAudioEndTime != -1) {
    audioDecoded += mAudioEndTime - GetMediaTime();
  }
  return audioDecoded;
}

void nsBuiltinDecoderStateMachine::DecodeThreadRun()
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  if (mState == DECODER_STATE_DECODING_METADATA) {
    if (NS_FAILED(DecodeMetadata())) {
      NS_ASSERTION(mState == DECODER_STATE_SHUTDOWN,
                   "Should be in shutdown state if metadata loading fails.");
      LOG(PR_LOG_DEBUG, ("Decode metadata failed, shutting down decode thread"));
    }
  }

  while (mState != DECODER_STATE_SHUTDOWN &&
         mState != DECODER_STATE_COMPLETED &&
         !mStopDecodeThread)
  {
    if (mState == DECODER_STATE_DECODING || mState == DECODER_STATE_BUFFERING) {
      DecodeLoop();
    } else if (mState == DECODER_STATE_SEEKING) {
      DecodeSeek();
    }
  }

  mDecodeThreadIdle = true;
  LOG(PR_LOG_DEBUG, ("%p Decode thread finished", mDecoder.get()));
}

void nsBuiltinDecoderStateMachine::DecodeLoop()
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
  const unsigned videoPumpThreshold = mRealTime ? 0 : AMPLE_VIDEO_FRAMES / 2;

  // After the audio decode fills with more than audioPumpThreshold usecs
  // of decoded audio, we'll start to check whether the audio or video decode
  // is falling behind.
  const unsigned audioPumpThreshold = mRealTime ? 0 : LOW_AUDIO_USECS * 2;

  // Our local low audio threshold. We may increase this if we're slow to
  // decode video frames, in order to reduce the chance of audio underruns.
  PRInt64 lowAudioThreshold = LOW_AUDIO_USECS;

  // Our local ample audio threshold. If we increase lowAudioThreshold, we'll
  // also increase this too appropriately (we don't want lowAudioThreshold to
  // be greater than ampleAudioThreshold, else we'd stop decoding!).
  PRInt64 ampleAudioThreshold = AMPLE_AUDIO_USECS;

  MediaQueue<VideoData>& videoQueue = mReader->mVideoQueue;
  MediaQueue<AudioData>& audioQueue = mReader->mAudioQueue;

  // Main decode loop.
  bool videoPlaying = HasVideo();
  bool audioPlaying = HasAudio();
  while ((mState == DECODER_STATE_DECODING || mState == DECODER_STATE_BUFFERING) &&
         !mStopDecodeThread &&
         (videoPlaying || audioPlaying))
  {
    // We don't want to consider skipping to the next keyframe if we've
    // only just started up the decode loop, so wait until we've decoded
    // some frames before enabling the keyframe skip logic on video.
    if (videoPump &&
        static_cast<PRUint32>(videoQueue.GetSize()) >= videoPumpThreshold)
    {
      videoPump = false;
    }

    // We don't want to consider skipping to the next keyframe if we've
    // only just started up the decode loop, so wait until we've decoded
    // some audio data before enabling the keyframe skip logic on audio.
    if (audioPump && GetDecodedAudioDuration() >= audioPumpThreshold) {
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
        ((!audioPump && audioPlaying && GetDecodedAudioDuration() < lowAudioThreshold) ||
         (!videoPump &&
           videoPlaying &&
           static_cast<PRUint32>(videoQueue.GetSize()) < LOW_VIDEO_FRAMES)) &&
        !HasLowUndecodedData())

    {
      skipToNextKeyframe = true;
      LOG(PR_LOG_DEBUG, ("%p Skipping video decode to the next keyframe", mDecoder.get()));
    }

    // Video decode.
    if (videoPlaying &&
        static_cast<PRUint32>(videoQueue.GetSize()) < AMPLE_VIDEO_FRAMES)
    {
      // Time the video decode, so that if it's slow, we can increase our low
      // audio threshold to reduce the chance of an audio underrun while we're
      // waiting for a video decode to complete.
      TimeDuration decodeTime;
      {
        PRInt64 currentTime = GetMediaTime();
        ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
        TimeStamp start = TimeStamp::Now();
        videoPlaying = mReader->DecodeVideoFrame(skipToNextKeyframe, currentTime);
        decodeTime = TimeStamp::Now() - start;
      }
      if (THRESHOLD_FACTOR * DurationToUsecs(decodeTime) > lowAudioThreshold &&
          !HasLowUndecodedData())
      {
        lowAudioThreshold =
          NS_MIN(THRESHOLD_FACTOR * DurationToUsecs(decodeTime), AMPLE_AUDIO_USECS);
        ampleAudioThreshold = NS_MAX(THRESHOLD_FACTOR * lowAudioThreshold,
                                     ampleAudioThreshold);
        LOG(PR_LOG_DEBUG,
            ("Slow video decode, set lowAudioThreshold=%lld ampleAudioThreshold=%lld",
             lowAudioThreshold, ampleAudioThreshold));
      }
    }

    // Audio decode.
    if (audioPlaying &&
        (GetDecodedAudioDuration() < ampleAudioThreshold || audioQueue.GetSize() == 0))
    {
      ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
      audioPlaying = mReader->DecodeAudioData();
    }

    // Notify to ensure that the AudioLoop() is not waiting, in case it was
    // waiting for more audio to be decoded.
    mDecoder->GetReentrantMonitor().NotifyAll();

    // The ready state can change when we've decoded data, so update the
    // ready state, so that DOM events can fire.
    UpdateReadyState();

    if ((mState == DECODER_STATE_DECODING || mState == DECODER_STATE_BUFFERING) &&
        !mStopDecodeThread &&
        (videoPlaying || audioPlaying) &&
        (!audioPlaying || (GetDecodedAudioDuration() >= ampleAudioThreshold &&
                           audioQueue.GetSize() > 0))
        &&
        (!videoPlaying ||
          static_cast<PRUint32>(videoQueue.GetSize()) >= AMPLE_VIDEO_FRAMES))
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
      if (mDecoder->GetState() != nsBuiltinDecoder::PLAY_STATE_PLAYING) {
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
      mState != DECODER_STATE_SEEKING)
  {
    mState = DECODER_STATE_COMPLETED;
    ScheduleStateMachine();
  }

  LOG(PR_LOG_DEBUG, ("%p Exiting DecodeLoop", mDecoder.get()));
}

bool nsBuiltinDecoderStateMachine::IsPlaying()
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  return !mPlayStartTime.IsNull();
}

void nsBuiltinDecoderStateMachine::AudioLoop()
{
  NS_ASSERTION(OnAudioThread(), "Should be on audio thread.");
  LOG(PR_LOG_DEBUG, ("%p Begun audio thread/loop", mDecoder.get()));
  PRInt64 audioDuration = 0;
  PRInt64 audioStartTime = -1;
  PRUint32 channels, rate;
  double volume = -1;
  bool setVolume;
  PRInt32 minWriteFrames = -1;
  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mAudioCompleted = false;
    audioStartTime = mAudioStartTime;
    channels = mInfo.mAudioChannels;
    rate = mInfo.mAudioRate;
    NS_ASSERTION(audioStartTime != -1, "Should have audio start time by now");
  }

  // It is unsafe to call some methods of nsAudioStream with the decoder
  // monitor held, as on Android those methods do a synchronous dispatch to
  // the main thread. If the audio thread holds the decoder monitor while
  // it does a synchronous dispatch to the main thread, we can get deadlocks
  // if the main thread tries to acquire the decoder monitor before the
  // dispatched event has finished (or even started!) running. Methods which
  // are unsafe to call with the decoder monitor held are documented as such
  // in nsAudioStream.h.
  nsRefPtr<nsAudioStream> audioStream = nsAudioStream::AllocateStream();
  audioStream->Init(channels, rate, MOZ_AUDIO_DATA_FORMAT);

  {
    // We must hold the monitor while setting mAudioStream or whenever we query
    // the playback position off the audio thread. This ensures the audio stream
    // is always alive when we use it off the audio thread. Note that querying
    // the playback position does not do a synchronous dispatch to the main
    // thread, so it's safe to call with the decoder monitor held.
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mAudioStream = audioStream;
    volume = mVolume;
    mAudioStream->SetVolume(volume);
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
              (mReader->mAudioQueue.GetSize() == 0 &&
               !mReader->mAudioQueue.AtEndOfStream())))
      {
        if (!IsPlaying() && !mAudioStream->IsPaused()) {
          mAudioStream->Pause();
        }
        mon.Wait();
      }

      // If we're shutting down, break out and exit the audio thread.
      if (mState == DECODER_STATE_SHUTDOWN ||
          mStopAudioThread ||
          mReader->mAudioQueue.AtEndOfStream())
      {
        break;
      }

      // We only want to go to the expense of changing the volume if
      // the volume has changed.
      setVolume = volume != mVolume;
      volume = mVolume;

      // Note audio stream IsPaused() does not do synchronous dispatch to the
      // main thread on Android, so can be called safely with the decoder
      // monitor held.
      if (IsPlaying() && mAudioStream->IsPaused()) {
        mAudioStream->Resume();
      }
    }

    if (setVolume) {
      mAudioStream->SetVolume(volume);
    }
    if (minWriteFrames == -1) {
      minWriteFrames = mAudioStream->GetMinWriteSize();
    }
    NS_ASSERTION(mReader->mAudioQueue.GetSize() > 0,
                 "Should have data to play");
    // See if there's a gap in the audio. If there is, push silence into the
    // audio hardware, so we can play across the gap.
    const AudioData* s = mReader->mAudioQueue.PeekFront();

    // Calculate the number of frames that have been pushed onto the audio
    // hardware.
    CheckedInt64 playedFrames = UsecsToFrames(audioStartTime, rate) +
                                              audioDuration;
    // Calculate the timestamp of the next chunk of audio in numbers of
    // samples.
    CheckedInt64 sampleTime = UsecsToFrames(s->mTime, rate);
    CheckedInt64 missingFrames = sampleTime - playedFrames;
    if (!missingFrames.valid() || !sampleTime.valid()) {
      NS_WARNING("Int overflow adding in AudioLoop()");
      break;
    }

    PRInt64 framesWritten = 0;
    if (missingFrames.value() > 0) {
      // The next audio chunk begins some time after the end of the last chunk
      // we pushed to the audio hardware. We must push silence into the audio
      // hardware so that the next audio chunk begins playback at the correct
      // time.
      missingFrames = NS_MIN(static_cast<PRInt64>(PR_UINT32_MAX),
                             missingFrames.value());
      framesWritten = PlaySilence(static_cast<PRUint32>(missingFrames.value()),
                                  channels, playedFrames.value());
    } else {
      framesWritten = PlayFromAudioQueue(sampleTime.value(), channels);
    }
    audioDuration += framesWritten;
    {
      ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
      CheckedInt64 playedUsecs = FramesToUsecs(audioDuration, rate) + audioStartTime;
      if (!playedUsecs.valid()) {
        NS_WARNING("Int overflow calculating audio end time");
        break;
      }
      mAudioEndTime = playedUsecs.value();
    }
  }
  if (mReader->mAudioQueue.AtEndOfStream() &&
      mState != DECODER_STATE_SHUTDOWN &&
      !mStopAudioThread)
  {
    // Last frame pushed to audio hardware, wait for the audio to finish,
    // before the audio thread terminates.
    bool seeking = false;
    {
      ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
      PRInt64 unplayedFrames = audioDuration % minWriteFrames;
      if (minWriteFrames > 1 && unplayedFrames > 0) {
        // Sound is written by libsydneyaudio to the hardware in blocks of
        // frames of size minWriteFrames. So if the number of frames we've
        // written isn't an exact multiple of minWriteFrames, we'll have
        // left over audio data which hasn't yet been written to the hardware,
        // and so that audio will not start playing. Write silence to ensure
        // the last block gets pushed to hardware, so that playback starts.
        PRInt64 framesToWrite = minWriteFrames - unplayedFrames;
        if (framesToWrite < PR_UINT32_MAX / channels) {
          // Write silence manually rather than using PlaySilence(), so that
          // the AudioAPI doesn't get a copy of the audio frames.
          PRUint32 numSamples = framesToWrite * channels;
          nsAutoArrayPtr<AudioDataValue> buf(new AudioDataValue[numSamples]);
          memset(buf.get(), 0, numSamples * sizeof(AudioDataValue));
          mAudioStream->Write(buf, framesToWrite);
        }
      }

      PRInt64 oldPosition = -1;
      PRInt64 position = GetMediaTime();
      while (oldPosition != position &&
             mAudioEndTime - position > 0 &&
             mState != DECODER_STATE_SEEKING &&
             mState != DECODER_STATE_SHUTDOWN)
      {
        const PRInt64 DRAIN_BLOCK_USECS = 100000;
        Wait(NS_MIN(mAudioEndTime - position, DRAIN_BLOCK_USECS));
        oldPosition = position;
        position = GetMediaTime();
      }
      seeking = mState == DECODER_STATE_SEEKING;
    }

    if (!seeking && !mAudioStream->IsPaused()) {
      mAudioStream->Drain();
      // Fire one last event for any extra frames that didn't fill a framebuffer.
      mEventManager.Drain(mAudioEndTime);
    }
  }
  LOG(PR_LOG_DEBUG, ("%p Reached audio stream end.", mDecoder.get()));
  {
    // Must hold lock while anulling the audio stream to prevent
    // state machine thread trying to use it while we're destroying it.
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mAudioStream = nsnull;
    mEventManager.Clear();
    mAudioCompleted = true;
    UpdateReadyState();
    // Kick the decode thread; it may be sleeping waiting for this to finish.
    mDecoder->GetReentrantMonitor().NotifyAll();
  }

  // Must not hold the decoder monitor while we shutdown the audio stream, as
  // it makes a synchronous dispatch on Android.
  audioStream->Shutdown();
  audioStream = nsnull;

  LOG(PR_LOG_DEBUG, ("%p Audio stream finished playing, audio thread exit", mDecoder.get()));
}

PRUint32 nsBuiltinDecoderStateMachine::PlaySilence(PRUint32 aFrames,
                                                   PRUint32 aChannels,
                                                   PRUint64 aFrameOffset)

{
  NS_ASSERTION(OnAudioThread(), "Only call on audio thread.");
  NS_ASSERTION(!mAudioStream->IsPaused(), "Don't play when paused");
  PRUint32 maxFrames = SILENCE_BYTES_CHUNK / aChannels / sizeof(AudioDataValue);
  PRUint32 frames = NS_MIN(aFrames, maxFrames);
  PRUint32 numSamples = frames * aChannels;
  nsAutoArrayPtr<AudioDataValue> buf(new AudioDataValue[numSamples]);
  memset(buf.get(), 0, numSamples * sizeof(AudioDataValue));
  mAudioStream->Write(buf, frames);
  // Dispatch events to the DOM for the audio just written.
  mEventManager.QueueWrittenAudioData(buf.get(), frames * aChannels,
                                      (aFrameOffset + frames) * aChannels);
  return frames;
}

PRUint32 nsBuiltinDecoderStateMachine::PlayFromAudioQueue(PRUint64 aFrameOffset,
                                                          PRUint32 aChannels)
{
  NS_ASSERTION(OnAudioThread(), "Only call on audio thread.");
  NS_ASSERTION(!mAudioStream->IsPaused(), "Don't play when paused");
  nsAutoPtr<AudioData> audio(mReader->mAudioQueue.PopFront());
  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    NS_WARN_IF_FALSE(IsPlaying(), "Should be playing");
    // Awaken the decode loop if it's waiting for space to free up in the
    // audio queue.
    mDecoder->GetReentrantMonitor().NotifyAll();
  }
  PRInt64 offset = -1;
  PRUint32 frames = 0;
  // The state machine could have paused since we've released the decoder
  // monitor and acquired the audio monitor. Rather than acquire both
  // monitors, the audio stream also maintains whether its paused or not.
  // This prevents us from doing a blocking write while holding the audio
  // monitor while paused; we would block, and the state machine won't be
  // able to acquire the audio monitor in order to resume or destroy the
  // audio stream.
  if (!mAudioStream->IsPaused()) {
    mAudioStream->Write(audio->mAudioData,
                        audio->mFrames);

    offset = audio->mOffset;
    frames = audio->mFrames;

    // Dispatch events to the DOM for the audio just written.
    mEventManager.QueueWrittenAudioData(audio->mAudioData.get(),
                                        audio->mFrames * aChannels,
                                        (aFrameOffset + frames) * aChannels);
  } else {
    mReader->mAudioQueue.PushFront(audio);
    audio.forget();
  }
  if (offset != -1) {
    mDecoder->UpdatePlaybackOffset(offset);
  }
  return frames;
}

nsresult nsBuiltinDecoderStateMachine::Init(nsDecoderStateMachine* aCloneDonor)
{
  nsBuiltinDecoderReader* cloneReader = nsnull;
  if (aCloneDonor) {
    cloneReader = static_cast<nsBuiltinDecoderStateMachine*>(aCloneDonor)->mReader;
  }
  return mReader->Init(cloneReader);
}

void nsBuiltinDecoderStateMachine::StopPlayback()
{
  LOG(PR_LOG_DEBUG, ("%p StopPlayback()", mDecoder.get()));

  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  mDecoder->mPlaybackStatistics.Stop(TimeStamp::Now());

  // Reset mPlayStartTime before we pause/shutdown the nsAudioStream. This is
  // so that if the audio loop is about to write audio, it will have the chance
  // to check to see if we're paused and not write the audio. If not, the
  // audio thread can block in the write, and we deadlock trying to acquire
  // the audio monitor upon resume playback.
  if (IsPlaying()) {
    mPlayDuration += DurationToUsecs(TimeStamp::Now() - mPlayStartTime);
    mPlayStartTime = TimeStamp();
  }
  // Notify the audio thread, so that it notices that we've stopped playing,
  // so it can pause audio playback.
  mDecoder->GetReentrantMonitor().NotifyAll();
  NS_ASSERTION(!IsPlaying(), "Should report not playing at end of StopPlayback()");
}

void nsBuiltinDecoderStateMachine::StartPlayback()
{
  LOG(PR_LOG_DEBUG, ("%p StartPlayback()", mDecoder.get()));

  NS_ASSERTION(!IsPlaying(), "Shouldn't be playing when StartPlayback() is called");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  LOG(PR_LOG_DEBUG, ("%p StartPlayback", mDecoder.get()));
  mDecoder->mPlaybackStatistics.Start(TimeStamp::Now());
  mPlayStartTime = TimeStamp::Now();

  NS_ASSERTION(IsPlaying(), "Should report playing by end of StartPlayback()");
  if (NS_FAILED(StartAudioThread())) {
    NS_WARNING("Failed to create audio thread"); 
  }
  mDecoder->GetReentrantMonitor().NotifyAll();
}

void nsBuiltinDecoderStateMachine::UpdatePlaybackPositionInternal(PRInt64 aTime)
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
      NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::DurationChanged);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }
}

void nsBuiltinDecoderStateMachine::UpdatePlaybackPosition(PRInt64 aTime)
{
  UpdatePlaybackPositionInternal(aTime);

  bool fragmentEnded = mFragmentEndTime >= 0 && GetMediaTime() >= mFragmentEndTime;
  if (!mPositionChangeQueued || fragmentEnded) {
    mPositionChangeQueued = true;
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::PlaybackPositionChanged);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }

  // Notify DOM of any queued up audioavailable events
  mEventManager.DispatchPendingEvents(GetMediaTime());

  if (fragmentEnded) {
    StopPlayback();
  }
}

void nsBuiltinDecoderStateMachine::ClearPositionChangeFlag()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  mPositionChangeQueued = false;
}

nsHTMLMediaElement::NextFrameStatus nsBuiltinDecoderStateMachine::GetNextFrameStatus()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  if (IsBuffering() || IsSeeking()) {
    return nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE_BUFFERING;
  } else if (HaveNextFrameData()) {
    return nsHTMLMediaElement::NEXT_FRAME_AVAILABLE;
  }
  return nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE;
}

void nsBuiltinDecoderStateMachine::SetVolume(double volume)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  mVolume = volume;
}

double nsBuiltinDecoderStateMachine::GetCurrentTime() const
{
  NS_ASSERTION(NS_IsMainThread() ||
               OnStateMachineThread() ||
               OnDecodeThread(),
               "Should be on main, decode, or state machine thread.");

  return static_cast<double>(mCurrentFrameTime) / static_cast<double>(USECS_PER_S);
}

PRInt64 nsBuiltinDecoderStateMachine::GetDuration()
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  if (mEndTime == -1 || mStartTime == -1)
    return -1;
  return mEndTime - mStartTime;
}

void nsBuiltinDecoderStateMachine::SetDuration(PRInt64 aDuration)
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

void nsBuiltinDecoderStateMachine::SetEndTime(PRInt64 aEndTime)
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  mEndTime = aEndTime;
}

void nsBuiltinDecoderStateMachine::SetFragmentEndTime(PRInt64 aEndTime)
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  mFragmentEndTime = aEndTime < 0 ? aEndTime : aEndTime + mStartTime;
}

void nsBuiltinDecoderStateMachine::SetSeekable(bool aSeekable)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  mSeekable = aSeekable;
}

void nsBuiltinDecoderStateMachine::Shutdown()
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

void nsBuiltinDecoderStateMachine::StartDecoding()
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

void nsBuiltinDecoderStateMachine::Play()
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

void nsBuiltinDecoderStateMachine::ResetPlayback()
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  mVideoFrameEndTime = -1;
  mAudioStartTime = -1;
  mAudioEndTime = -1;
  mAudioCompleted = false;
}

void nsBuiltinDecoderStateMachine::NotifyDataArrived(const char* aBuffer,
                                                     PRUint32 aLength,
                                                     PRInt64 aOffset)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  mReader->NotifyDataArrived(aBuffer, aLength, aOffset);

  // While playing an unseekable stream of unknown duration, mEndTime is
  // updated (in AdvanceFrame()) as we play. But if data is being downloaded
  // faster than played, mEndTime won't reflect the end of playable data
  // since we haven't played the frame at the end of buffered data. So update
  // mEndTime here as new data is downloaded to prevent such a lag.
  nsTimeRanges buffered;
  if (mDecoder->IsInfinite() &&
      NS_SUCCEEDED(mDecoder->GetBuffered(&buffered)))
  {
    PRUint32 length = 0;
    buffered.GetLength(&length);
    if (length) {
      double end = 0;
      buffered.End(length - 1, &end);
      ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
      mEndTime = NS_MAX<PRInt64>(mEndTime, end * USECS_PER_S);
    }
  }
}

void nsBuiltinDecoderStateMachine::Seek(double aTime)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  // nsBuiltinDecoder::mPlayState should be SEEKING while we seek, and
  // in that case nsBuiltinDecoder shouldn't be calling us.
  NS_ASSERTION(mState != DECODER_STATE_SEEKING,
               "We shouldn't already be seeking");
  NS_ASSERTION(mState >= DECODER_STATE_DECODING,
               "We should have loaded metadata");
  double t = aTime * static_cast<double>(USECS_PER_S);
  if (t > INT64_MAX) {
    // Prevent integer overflow.
    return;
  }

  mSeekTime = static_cast<PRInt64>(t) + mStartTime;
  NS_ASSERTION(mSeekTime >= mStartTime && mSeekTime <= mEndTime,
               "Can only seek in range [0,duration]");

  // Bound the seek time to be inside the media range.
  NS_ASSERTION(mStartTime != -1, "Should know start time by now");
  NS_ASSERTION(mEndTime != -1, "Should know end time by now");
  mSeekTime = NS_MIN(mSeekTime, mEndTime);
  mSeekTime = NS_MAX(mStartTime, mSeekTime);
  LOG(PR_LOG_DEBUG, ("%p Changed state to SEEKING (to %f)", mDecoder.get(), aTime));
  mState = DECODER_STATE_SEEKING;
  ScheduleStateMachine();
}

void nsBuiltinDecoderStateMachine::StopDecodeThread()
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
    mDecodeThread = nsnull;
    mDecodeThreadIdle = false;
  }
  NS_ASSERTION(!mRequestedNewDecodeThread,
    "Any pending requests for decode threads must be canceled and unflagged");
  NS_ASSERTION(!StateMachineTracker::Instance().IsQueued(this),
    "Any pending requests for decode threads must be canceled");
}

void nsBuiltinDecoderStateMachine::StopAudioThread()
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  mStopAudioThread = true;
  mDecoder->GetReentrantMonitor().NotifyAll();
  if (mAudioThread) {
    LOG(PR_LOG_DEBUG, ("%p Shutdown audio thread", mDecoder.get()));
    {
      ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
      mAudioThread->Shutdown();
    }
    mAudioThread = nsnull;
  }
}

nsresult
nsBuiltinDecoderStateMachine::ScheduleDecodeThread()
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
        NS_NewRunnableMethod(this, &nsBuiltinDecoderStateMachine::DecodeThreadRun);
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
nsBuiltinDecoderStateMachine::StartDecodeThread()
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

  nsresult rv = NS_NewThread(getter_AddRefs(mDecodeThread),
                              nsnull,
                              MEDIA_THREAD_STACK_SIZE);
  if (NS_FAILED(rv)) {
    // Give up, report error to media element.
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::DecodeError);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
    return rv;
  }

  nsCOMPtr<nsIRunnable> event =
    NS_NewRunnableMethod(this, &nsBuiltinDecoderStateMachine::DecodeThreadRun);
  mDecodeThread->Dispatch(event, NS_DISPATCH_NORMAL);
  mDecodeThreadIdle = false;

  return NS_OK;
}

nsresult
nsBuiltinDecoderStateMachine::StartAudioThread()
{
  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine or decode thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  mStopAudioThread = false;
  if (HasAudio() && !mAudioThread) {
    nsresult rv = NS_NewThread(getter_AddRefs(mAudioThread),
                               nsnull,
                               MEDIA_THREAD_STACK_SIZE);
    if (NS_FAILED(rv)) {
      LOG(PR_LOG_DEBUG, ("%p Changed state to SHUTDOWN because failed to create audio thread", mDecoder.get()));
      mState = DECODER_STATE_SHUTDOWN;
      return rv;
    }
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &nsBuiltinDecoderStateMachine::AudioLoop);
    mAudioThread->Dispatch(event, NS_DISPATCH_NORMAL);
  }
  return NS_OK;
}

PRInt64 nsBuiltinDecoderStateMachine::AudioDecodedUsecs() const
{
  NS_ASSERTION(HasAudio(),
               "Should only call AudioDecodedUsecs() when we have audio");
  // The amount of audio we have decoded is the amount of audio data we've
  // already decoded and pushed to the hardware, plus the amount of audio
  // data waiting to be pushed to the hardware.
  PRInt64 pushed = (mAudioEndTime != -1) ? (mAudioEndTime - GetMediaTime()) : 0;
  return pushed + mReader->mAudioQueue.Duration();
}

bool nsBuiltinDecoderStateMachine::HasLowDecodedData(PRInt64 aAudioUsecs) const
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  // We consider ourselves low on decoded data if we're low on audio,
  // provided we've not decoded to the end of the audio stream, or
  // if we're only playing video and we're low on video frames, provided
  // we've not decoded to the end of the video stream.
  return ((HasAudio() &&
           !mReader->mAudioQueue.IsFinished() &&
           AudioDecodedUsecs() < aAudioUsecs)
          ||
         (!HasAudio() &&
          HasVideo() &&
          !mReader->mVideoQueue.IsFinished() &&
          static_cast<PRUint32>(mReader->mVideoQueue.GetSize()) < LOW_VIDEO_FRAMES));
}

bool nsBuiltinDecoderStateMachine::HasLowUndecodedData() const
{
  return GetUndecodedData() < mLowDataThresholdUsecs;
}

PRInt64 nsBuiltinDecoderStateMachine::GetUndecodedData() const
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  NS_ASSERTION(mState > DECODER_STATE_DECODING_METADATA,
               "Must have loaded metadata for GetBuffered() to work");
  nsTimeRanges buffered;
  
  nsresult res = mDecoder->GetBuffered(&buffered);
  NS_ENSURE_SUCCESS(res, 0);
  double currentTime = GetCurrentTime();

  nsIDOMTimeRanges* r = static_cast<nsIDOMTimeRanges*>(&buffered);
  PRUint32 length = 0;
  res = r->GetLength(&length);
  NS_ENSURE_SUCCESS(res, 0);

  for (PRUint32 index = 0; index < length; ++index) {
    double start, end;
    res = r->Start(index, &start);
    NS_ENSURE_SUCCESS(res, 0);

    res = r->End(index, &end);
    NS_ENSURE_SUCCESS(res, 0);

    if (start <= currentTime && end >= currentTime) {
      return static_cast<PRInt64>((end - currentTime) * USECS_PER_S);
    }
  }
  return 0;
}

void nsBuiltinDecoderStateMachine::SetFrameBufferLength(PRUint32 aLength)
{
  NS_ASSERTION(aLength >= 512 && aLength <= 16384,
               "The length must be between 512 and 16384");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  mEventManager.SetSignalBufferLength(aLength);
}

nsresult nsBuiltinDecoderStateMachine::DecodeMetadata()
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  NS_ASSERTION(mState == DECODER_STATE_DECODING_METADATA,
               "Only call when in metadata decoding state");

  LOG(PR_LOG_DEBUG, ("%p Decoding Media Headers", mDecoder.get()));
  nsresult res;
  nsVideoInfo info;
  {
    ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
    res = mReader->ReadMetadata(&info);
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
      NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::DecodeError);
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
  NS_ASSERTION((!HasVideo() && !HasAudio()) ||
                !mSeekable || mEndTime != -1,
                "Active seekable media should have end time");
  NS_ASSERTION(!mSeekable || GetDuration() != -1, "Seekable media should have duration");
  LOG(PR_LOG_DEBUG, ("%p Media goes from %lld to %lld (duration %lld) seekable=%d",
                      mDecoder.get(), mStartTime, mEndTime, GetDuration(), mSeekable));

  // Inform the element that we've loaded the metadata and the first frame,
  // setting the default framebuffer size for audioavailable events.  Also,
  // if there is audio, let the MozAudioAvailable event manager know about
  // the metadata.
  if (HasAudio()) {
    mEventManager.Init(mInfo.mAudioChannels, mInfo.mAudioRate);
    // Set the buffer length at the decoder level to be able, to be able
    // to retrive the value via media element method. The RequestFrameBufferLength
    // will call the nsBuiltinDecoderStateMachine::SetFrameBufferLength().
    PRUint32 frameBufferLength = mInfo.mAudioChannels * FRAMEBUFFER_LENGTH_PER_CHANNEL;
    mDecoder->RequestFrameBufferLength(frameBufferLength);
  }
  nsCOMPtr<nsIRunnable> metadataLoadedEvent =
    new nsAudioMetadataEventRunner(mDecoder, mInfo.mAudioChannels, mInfo.mAudioRate);
  NS_DispatchToMainThread(metadataLoadedEvent, NS_DISPATCH_NORMAL);

  if (mState == DECODER_STATE_DECODING_METADATA) {
    LOG(PR_LOG_DEBUG, ("%p Changed state from DECODING_METADATA to DECODING", mDecoder.get()));
    StartDecoding();
  }

  if ((mState == DECODER_STATE_DECODING || mState == DECODER_STATE_COMPLETED) &&
      mDecoder->GetState() == nsBuiltinDecoder::PLAY_STATE_PLAYING &&
      !IsPlaying())
  {
    StartPlayback();
  }

  return NS_OK;
}

void nsBuiltinDecoderStateMachine::DecodeSeek()
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  NS_ASSERTION(mState == DECODER_STATE_SEEKING,
               "Only call when in seeking state");

  // During the seek, don't have a lock on the decoder state,
  // otherwise long seek operations can block the main thread.
  // The events dispatched to the main thread are SYNC calls.
  // These calls are made outside of the decode monitor lock so
  // it is safe for the main thread to makes calls that acquire
  // the lock since it won't deadlock. We check the state when
  // acquiring the lock again in case shutdown has occurred
  // during the time when we didn't have the lock.
  PRInt64 seekTime = mSeekTime;
  mDecoder->StopProgressUpdates();

  bool currentTimeChanged = false;
  PRInt64 mediaTime = GetMediaTime();
  if (mediaTime != seekTime) {
    currentTimeChanged = true;
    UpdatePlaybackPositionInternal(seekTime);
  }

  // SeekingStarted will do a UpdateReadyStateForData which will
  // inform the element and its users that we have no frames
  // to display
  {
    ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
    nsCOMPtr<nsIRunnable> startEvent =
      NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::SeekingStarted);
    NS_DispatchToMainThread(startEvent, NS_DISPATCH_SYNC);
  }

  if (currentTimeChanged) {
    // The seek target is different than the current playback position,
    // we'll need to seek the playback position, so shutdown our decode
    // and audio threads.
    StopPlayback();
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
      AudioData* audio = HasAudio() ? mReader->mAudioQueue.PeekFront() : nsnull;
      NS_ASSERTION(!audio || (audio->mTime <= seekTime &&
                              seekTime <= audio->mTime + audio->mDuration),
                    "Seek target should lie inside the first audio block after seek");
      PRInt64 startTime = (audio && audio->mTime < seekTime) ? audio->mTime : seekTime;
      mAudioStartTime = startTime;
      mPlayDuration = startTime - mStartTime;
      if (HasVideo()) {
        nsAutoPtr<VideoData> video(mReader->mVideoQueue.PeekFront());
        if (video) {
          NS_ASSERTION(video->mTime <= seekTime && seekTime <= video->mEndTime,
                        "Seek target should lie inside the first frame after seek");
          {
            ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
            RenderVideoFrame(video, TimeStamp::Now());
          }
          mReader->mVideoQueue.PopFront();
          nsCOMPtr<nsIRunnable> event =
            NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::Invalidate);
          NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
        }
      }
    }
  }
  mDecoder->StartProgressUpdates();
  if (mState == DECODER_STATE_SHUTDOWN)
    return;

  // Try to decode another frame to detect if we're at the end...
  LOG(PR_LOG_DEBUG, ("%p Seek completed, mCurrentFrameTime=%lld\n",
      mDecoder.get(), mCurrentFrameTime));

  // Change state to DECODING or COMPLETED now. SeekingStopped will
  // call nsBuiltinDecoderStateMachine::Seek to reset our state to SEEKING
  // if we need to seek again.

  nsCOMPtr<nsIRunnable> stopEvent;
  bool isLiveStream = mDecoder->GetResource()->GetLength() == -1;
  if (GetMediaTime() == mEndTime && !isLiveStream) {
    // Seeked to end of media, move to COMPLETED state. Note we don't do
    // this if we're playing a live stream, since the end of media will advance
    // once we download more data!
    LOG(PR_LOG_DEBUG, ("%p Changed state from SEEKING (to %lld) to COMPLETED",
                        mDecoder.get(), seekTime));
    stopEvent = NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::SeekingStoppedAtEnd);
    mState = DECODER_STATE_COMPLETED;
  } else {
    LOG(PR_LOG_DEBUG, ("%p Changed state from SEEKING (to %lld) to DECODING",
                        mDecoder.get(), seekTime));
    stopEvent = NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::SeekingStopped);
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
  nsDecoderDisposeEvent(already_AddRefed<nsBuiltinDecoder> aDecoder,
                        already_AddRefed<nsBuiltinDecoderStateMachine> aStateMachine)
    : mDecoder(aDecoder), mStateMachine(aStateMachine) {}
  NS_IMETHOD Run() {
    NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");
    mStateMachine->ReleaseDecoder();
    mDecoder->ReleaseStateMachine();
    mStateMachine = nsnull;
    mDecoder = nsnull;
    return NS_OK;
  }
private:
  nsRefPtr<nsBuiltinDecoder> mDecoder;
  nsCOMPtr<nsBuiltinDecoderStateMachine> mStateMachine;
};

// Runnable which dispatches an event to the main thread to dispose of the
// decoder and state machine. This runs on the state machine thread after
// the state machine has shutdown, and all events for that state machine have
// finished running.
class nsDispatchDisposeEvent : public nsRunnable {
public:
  nsDispatchDisposeEvent(nsBuiltinDecoder* aDecoder,
                         nsBuiltinDecoderStateMachine* aStateMachine)
    : mDecoder(aDecoder), mStateMachine(aStateMachine) {}
  NS_IMETHOD Run() {
    NS_DispatchToMainThread(new nsDecoderDisposeEvent(mDecoder.forget(),
                                                      mStateMachine.forget()));
    return NS_OK;
  }
private:
  nsRefPtr<nsBuiltinDecoder> mDecoder;
  nsCOMPtr<nsBuiltinDecoderStateMachine> mStateMachine;
};

nsresult nsBuiltinDecoderStateMachine::RunStateMachine()
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
      StopDecodeThread();
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

    case DECODER_STATE_DECODING_METADATA: {
      // Ensure we have a decode thread to decode metadata.
      return ScheduleDecodeThread();
    }
  
    case DECODER_STATE_DECODING: {
      if (mDecoder->GetState() != nsBuiltinDecoder::PLAY_STATE_PLAYING &&
          IsPlaying())
      {
        // We're playing, but the element/decoder is in paused state. Stop
        // playing! Note we do this before StopDecodeThread() below because
        // that blocks this state machine's execution, and can cause a
        // perceptible delay between the pause command, and playback actually
        // pausing.
        StopPlayback();
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
      NS_ASSERTION(mDecoder->GetState() != nsBuiltinDecoder::PLAY_STATE_PLAYING ||
                   IsStateMachineScheduled(), "Must have timer scheduled");
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
      bool isLiveStream = mDecoder->GetResource()->GetLength() == -1;
      if ((isLiveStream || !mDecoder->CanPlayThrough()) &&
            elapsed < TimeDuration::FromSeconds(mBufferingWait) &&
            (mQuickBuffering ? HasLowDecodedData(QUICK_BUFFERING_LOW_DATA_USECS)
                            : (GetUndecodedData() < mBufferingWait * USECS_PER_S / 1000)) &&
            !resource->IsDataCachedToEndOfResource(mDecoder->mDecoderPosition) &&
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
      if (mDecoder->GetState() == nsBuiltinDecoder::PLAY_STATE_PLAYING &&
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
          (mReader->mVideoQueue.GetSize() > 0 ||
          (HasAudio() && !mAudioCompleted)))
      {
        AdvanceFrame();
        NS_ASSERTION(mDecoder->GetState() != nsBuiltinDecoder::PLAY_STATE_PLAYING ||
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
      if (mDecoder->GetState() == nsBuiltinDecoder::PLAY_STATE_PLAYING) {
        PRInt64 videoTime = HasVideo() ? mVideoFrameEndTime : 0;
        PRInt64 clockTime = NS_MAX(mEndTime, NS_MAX(videoTime, GetAudioClock()));
        UpdatePlaybackPosition(clockTime);
        nsCOMPtr<nsIRunnable> event =
          NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::PlaybackEnded);
        NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
      }
      return NS_OK;
    }
  }

  return NS_OK;
}

void nsBuiltinDecoderStateMachine::RenderVideoFrame(VideoData* aData,
                                                    TimeStamp aTarget)
{
  NS_ASSERTION(OnStateMachineThread() || OnDecodeThread(),
               "Should be on state machine or decode thread.");
  mDecoder->GetReentrantMonitor().AssertNotCurrentThreadIn();

  if (aData->mDuplicate) {
    return;
  }

  VideoFrameContainer* container = mDecoder->GetVideoFrameContainer();
  if (container) {
    container->SetCurrentFrame(aData->mDisplay, aData->mImage, aTarget);
  }
}

PRInt64
nsBuiltinDecoderStateMachine::GetAudioClock()
{
  NS_ASSERTION(OnStateMachineThread(), "Should be on state machine thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  if (!HasAudio())
    return -1;
  // We must hold the decoder monitor while using the audio stream off the
  // audio thread to ensure that it doesn't get destroyed on the audio thread
  // while we're using it.
  if (!mAudioStream) {
    // Audio thread hasn't played any data yet.
    return mAudioStartTime;
  }
  // Note that querying the playback position does not do a synchronous
  // dispatch to the main thread on Android, so it's safe to call with
  // the decoder monitor held here.
  PRInt64 t = mAudioStream->GetPosition();
  return (t == -1) ? -1 : t + mAudioStartTime;
}

void nsBuiltinDecoderStateMachine::AdvanceFrame()
{
  NS_ASSERTION(OnStateMachineThread(), "Should be on state machine thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  NS_ASSERTION(!HasAudio() || mAudioStartTime != -1,
               "Should know audio start time if we have audio.");

  if (mDecoder->GetState() != nsBuiltinDecoder::PLAY_STATE_PLAYING) {
    return;
  }

  // Determine the clock time. If we've got audio, and we've not reached
  // the end of the audio, use the audio clock. However if we've finished
  // audio, or don't have audio, use the system clock.
  PRInt64 clock_time = -1;
  if (!IsPlaying()) {
    clock_time = mPlayDuration + mStartTime;
  } else {
    PRInt64 audio_time = GetAudioClock();
    if (HasAudio() && !mAudioCompleted && audio_time != -1) {
      clock_time = audio_time;
      // Resync against the audio clock, while we're trusting the
      // audio clock. This ensures no "drift", particularly on Linux.
      mPlayDuration = clock_time - mStartTime;
      mPlayStartTime = TimeStamp::Now();
    } else {
      // Audio is disabled on this system. Sync to the system clock.
      clock_time = DurationToUsecs(TimeStamp::Now() - mPlayStartTime) + mPlayDuration;
      // Ensure the clock can never go backwards.
      NS_ASSERTION(mCurrentFrameTime <= clock_time, "Clock should go forwards");
      clock_time = NS_MAX(mCurrentFrameTime, clock_time) + mStartTime;
    }
  }

  // Skip frames up to the frame at the playback position, and figure out
  // the time remaining until it's time to display the next frame.
  PRInt64 remainingTime = AUDIO_DURATION_USECS;
  NS_ASSERTION(clock_time >= mStartTime, "Should have positive clock time.");
  nsAutoPtr<VideoData> currentFrame;
  if (mReader->mVideoQueue.GetSize() > 0) {
    VideoData* frame = mReader->mVideoQueue.PeekFront();
    while (mRealTime || clock_time >= frame->mTime) {
      mVideoFrameEndTime = frame->mEndTime;
      currentFrame = frame;
      mReader->mVideoQueue.PopFront();
      // Notify the decode thread that the video queue's buffers may have
      // free'd up space for more frames.
      mDecoder->GetReentrantMonitor().NotifyAll();
      mDecoder->UpdatePlaybackOffset(frame->mOffset);
      if (mReader->mVideoQueue.GetSize() == 0)
        break;
      frame = mReader->mVideoQueue.PeekFront();
    }
    // Current frame has already been presented, wait until it's time to
    // present the next frame.
    if (frame && !currentFrame) {
      PRInt64 now = IsPlaying()
        ? (DurationToUsecs(TimeStamp::Now() - mPlayStartTime) + mPlayDuration)
        : mPlayDuration;
      remainingTime = frame->mTime - mStartTime - now;
    }
  }

  // Check to see if we don't have enough data to play up to the next frame.
  // If we don't, switch to buffering mode.
  MediaResource* resource = mDecoder->GetResource();
  if (mState == DECODER_STATE_DECODING &&
      mDecoder->GetState() == nsBuiltinDecoder::PLAY_STATE_PLAYING &&
      HasLowDecodedData(remainingTime + EXHAUSTED_DATA_MARGIN_USECS) &&
      !resource->IsDataCachedToEndOfResource(mDecoder->mDecoderPosition) &&
      !resource->IsSuspended() &&
      (JustExitedQuickBuffering() || HasLowUndecodedData()))
  {
    if (currentFrame) {
      mReader->mVideoQueue.PushFront(currentFrame.forget());
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
    mDecoder->GetFrameStatistics().NotifyPresentedFrame();
    PRInt64 now = DurationToUsecs(TimeStamp::Now() - mPlayStartTime) + mPlayDuration;
    remainingTime = currentFrame->mEndTime - mStartTime - now;
    currentFrame = nsnull;
  }

  // Cap the current time to the larger of the audio and video end time.
  // This ensures that if we're running off the system clock, we don't
  // advance the clock to after the media end time.
  if (mVideoFrameEndTime != -1 || mAudioEndTime != -1) {
    // These will be non -1 if we've displayed a video frame, or played an audio frame.
    clock_time = NS_MIN(clock_time, NS_MAX(mVideoFrameEndTime, mAudioEndTime));
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

void nsBuiltinDecoderStateMachine::Wait(PRInt64 aUsecs) {
  NS_ASSERTION(OnAudioThread(), "Only call on the audio thread");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  TimeStamp end = TimeStamp::Now() + UsecsToDuration(NS_MAX<PRInt64>(USECS_PER_MS, aUsecs));
  TimeStamp now;
  while ((now = TimeStamp::Now()) < end &&
         mState != DECODER_STATE_SHUTDOWN &&
         mState != DECODER_STATE_SEEKING &&
         !mStopAudioThread &&
         IsPlaying())
  {
    PRInt64 ms = static_cast<PRInt64>(NS_round((end - now).ToSeconds() * 1000));
    if (ms == 0 || ms > PR_UINT32_MAX) {
      break;
    }
    mDecoder->GetReentrantMonitor().Wait(PR_MillisecondsToInterval(static_cast<PRUint32>(ms)));
  }
}

VideoData* nsBuiltinDecoderStateMachine::FindStartTime()
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  PRInt64 startTime = 0;
  mStartTime = 0;
  VideoData* v = nsnull;
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

void nsBuiltinDecoderStateMachine::UpdateReadyState() {
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  nsCOMPtr<nsIRunnable> event;
  switch (GetNextFrameStatus()) {
    case nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE_BUFFERING:
      event = NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::NextFrameUnavailableBuffering);
      break;
    case nsHTMLMediaElement::NEXT_FRAME_AVAILABLE:
      event = NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::NextFrameAvailable);
      break;
    case nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE:
      event = NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::NextFrameUnavailable);
      break;
    default:
      PR_NOT_REACHED("unhandled frame state");
  }

  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}

bool nsBuiltinDecoderStateMachine::JustExitedQuickBuffering()
{
  return !mDecodeStartTime.IsNull() &&
    mQuickBuffering &&
    (TimeStamp::Now() - mDecodeStartTime) < TimeDuration::FromSeconds(QUICK_BUFFER_THRESHOLD_USECS);
}

void nsBuiltinDecoderStateMachine::StartBuffering()
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
  nsMediaDecoder::Statistics stats = mDecoder->GetStatistics();
  LOG(PR_LOG_DEBUG, ("%p Playback rate: %.1lfKB/s%s download rate: %.1lfKB/s%s",
    mDecoder.get(),
    stats.mPlaybackRate/1024, stats.mPlaybackRateReliable ? "" : " (unreliable)",
    stats.mDownloadRate/1024, stats.mDownloadRateReliable ? "" : " (unreliable)"));
}

nsresult nsBuiltinDecoderStateMachine::GetBuffered(nsTimeRanges* aBuffered) {
  MediaResource* resource = mDecoder->GetResource();
  NS_ENSURE_TRUE(resource, NS_ERROR_FAILURE);
  resource->Pin();
  nsresult res = mReader->GetBuffered(aBuffered, mStartTime);
  resource->Unpin();
  return res;
}

bool nsBuiltinDecoderStateMachine::IsPausedAndDecoderWaiting() {
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  NS_ASSERTION(OnStateMachineThread(), "Should be on state machine thread.");

  return
    mDecodeThreadWaiting &&
    mDecoder->GetState() != nsBuiltinDecoder::PLAY_STATE_PLAYING &&
    (mState == DECODER_STATE_DECODING || mState == DECODER_STATE_BUFFERING);
}

nsresult nsBuiltinDecoderStateMachine::Run()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  NS_ASSERTION(OnStateMachineThread(), "Should be on state machine thread.");

  return CallRunStateMachine();
}

nsresult nsBuiltinDecoderStateMachine::CallRunStateMachine()
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  NS_ASSERTION(OnStateMachineThread(), "Should be on state machine thread.");
  // This will be set to true by ScheduleStateMachine() if it's called
  // while we're in RunStateMachine().
  mRunAgain = false;

  // Set to true whenever we dispatch an event to run this state machine.
  // This flag prevents us from dispatching
  mDispatchedRunEvent = false;

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
  nsBuiltinDecoderStateMachine *machine =
    static_cast<nsBuiltinDecoderStateMachine*>(aClosure);
  NS_ASSERTION(machine, "Must have been passed state machine");
  machine->TimeoutExpired();
}

void nsBuiltinDecoderStateMachine::TimeoutExpired()
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

nsresult nsBuiltinDecoderStateMachine::ScheduleStateMachine() {
  return ScheduleStateMachine(0);
}

nsresult nsBuiltinDecoderStateMachine::ScheduleStateMachine(PRInt64 aUsecs) {
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  NS_ABORT_IF_FALSE(GetStateMachineThread(),
    "Must have a state machine thread to schedule");

  if (mState == DECODER_STATE_SHUTDOWN) {
    return NS_ERROR_FAILURE;
  }
  aUsecs = PR_MAX(aUsecs, 0);

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

  PRUint32 ms = static_cast<PRUint32>((aUsecs / USECS_PER_MS) & 0xFFFFFFFF);
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

  res = mTimer->InitWithFuncCallback(::TimeoutExpired,
                                     this,
                                     ms,
                                     nsITimer::TYPE_ONE_SHOT);
  return res;
}

bool nsBuiltinDecoderStateMachine::OnStateMachineThread() const
{
    return IsCurrentThread(GetStateMachineThread());
}
 
nsIThread* nsBuiltinDecoderStateMachine::GetStateMachineThread()
{
  return StateMachineTracker::Instance().GetGlobalStateMachineThread();
}

void nsBuiltinDecoderStateMachine::NotifyAudioAvailableListener()
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  mEventManager.NotifyAudioAvailableListener();
}
