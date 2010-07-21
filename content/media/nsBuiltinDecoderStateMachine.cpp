/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

using namespace mozilla;
using namespace mozilla::layers;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gBuiltinDecoderLog;
#define LOG(type, msg) PR_LOG(gBuiltinDecoderLog, type, msg)
#else
#define LOG(type, msg)
#endif

// Wait this number of seconds when buffering, then leave and play
// as best as we can if the required amount of data hasn't been
// retrieved.
#define BUFFERING_WAIT 30

// The amount of data to retrieve during buffering is computed based
// on the download rate. BUFFERING_MIN_RATE is the minimum download
// rate to be used in that calculation to help avoid constant buffering
// attempts at a time when the average download rate has not stabilised.
#define BUFFERING_MIN_RATE 50000
#define BUFFERING_RATE(x) ((x)< BUFFERING_MIN_RATE ? BUFFERING_MIN_RATE : (x))

// If audio queue has less than this many ms of decoded audio, we won't risk
// trying to decode the video, we'll skip decoding video up to the next
// keyframe.
//
// Also if the decode catches up with the end of the downloaded data,
// we'll only go into BUFFERING state if we've got audio and have queued
// less than LOW_AUDIO_MS of audio, or if we've got video and have queued
// less than LOW_VIDEO_FRAMES frames.
static const PRUint32 LOW_AUDIO_MS = 300;

// If more than this many ms of decoded audio is queued, we'll hold off
// decoding more audio.
const unsigned AMPLE_AUDIO_MS = 2000;

// If we have fewer than LOW_VIDEO_FRAMES decoded frames, and
// we're not "pumping video", we'll skip the video up to the next keyframe
// which is at or after the current playback position.
//
// Also if the decode catches up with the end of the downloaded data,
// we'll only go into BUFFERING state if we've got audio and have queued
// less than LOW_AUDIO_MS of audio, or if we've got video and have queued
// less than LOW_VIDEO_FRAMES frames.
static const PRUint32 LOW_VIDEO_FRAMES = 1;

// Arbitrary "frame duration" when playing only audio.
static const int AUDIO_DURATION_MS = 40;

nsBuiltinDecoderStateMachine::nsBuiltinDecoderStateMachine(nsBuiltinDecoder* aDecoder,
                                                           nsBuiltinDecoderReader* aReader) :
  mDecoder(aDecoder),
  mState(DECODER_STATE_DECODING_METADATA),
  mAudioMonitor("media.audiostream"),
  mCbCrSize(0),
  mPlayDuration(0),
  mBufferingEndOffset(0),
  mStartTime(-1),
  mEndTime(-1),
  mSeekTime(0),
  mReader(aReader),
  mCurrentFrameTime(0),
  mAudioStartTime(-1),
  mAudioEndTime(-1),
  mVideoFrameEndTime(-1),
  mVolume(1.0),
  mSeekable(PR_TRUE),
  mPositionChangeQueued(PR_FALSE),
  mAudioCompleted(PR_FALSE),
  mBufferExhausted(PR_FALSE),
  mGotDurationFromHeader(PR_FALSE),
  mStopDecodeThreads(PR_TRUE)
{
  MOZ_COUNT_CTOR(nsBuiltinDecoderStateMachine);
}

nsBuiltinDecoderStateMachine::~nsBuiltinDecoderStateMachine()
{
  MOZ_COUNT_DTOR(nsBuiltinDecoderStateMachine);
}

PRBool nsBuiltinDecoderStateMachine::HasFutureAudio() const {
  mDecoder->GetMonitor().AssertCurrentThreadIn();
  PRBool aboveLowAudioThreshold = PR_FALSE;
  if (mAudioEndTime != -1) {
    aboveLowAudioThreshold = mAudioEndTime - mCurrentFrameTime + mStartTime > LOW_AUDIO_MS;
  }
  return HasAudio() &&
    !mAudioCompleted &&
    (mReader->mAudioQueue.GetSize() > 0 || aboveLowAudioThreshold);
}

PRBool nsBuiltinDecoderStateMachine::HaveNextFrameData() const {
    return ((!HasAudio() || mReader->mAudioQueue.AtEndOfStream()) && 
             mReader->mVideoQueue.GetSize() > 0) ||
            HasFutureAudio();
}

void nsBuiltinDecoderStateMachine::DecodeLoop()
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");
  PRBool videoPlaying = PR_FALSE;
  PRBool audioPlaying = PR_FALSE;
  {
    MonitorAutoEnter mon(mDecoder->GetMonitor());
    videoPlaying = HasVideo();
    audioPlaying = HasAudio();
  }

  // We want to "pump" the decode until we've got a few frames/samples decoded
  // before we consider whether decode is falling behind.
  PRBool audioPump = PR_TRUE;
  PRBool videoPump = PR_TRUE;

  // If the video decode is falling behind the audio, we'll start dropping the
  // inter-frames up until the next keyframe which is at or before the current
  // playback position. skipToNextKeyframe is PR_TRUE if we're currently
  // skipping up to the next keyframe.
  PRBool skipToNextKeyframe = PR_FALSE;

  // Once we've decoded more than videoPumpThreshold video frames, we'll
  // no longer be considered to be "pumping video".
  const unsigned videoPumpThreshold = 5;

  // If we've got more than videoWaitThreshold decoded video frames waiting in
  // the video queue, we will not decode any more video frames until they've
  // been consumed by the play state machine thread.
  const unsigned videoWaitThreshold = 10;

  // After the audio decode fills with more than audioPumpThresholdMs ms
  // of decoded audio, we'll start to check whether the audio or video decode
  // is falling behind.
  const unsigned audioPumpThresholdMs = LOW_AUDIO_MS * 2;

  // Main decode loop.
  while (videoPlaying || audioPlaying) {
    PRBool audioWait = !audioPlaying;
    PRBool videoWait = !videoPlaying;
    {
      // Wait for more data to download if we've exhausted all our
      // buffered data.
      MonitorAutoEnter mon(mDecoder->GetMonitor());
      while (!mStopDecodeThreads &&
             mBufferExhausted &&
             mState != DECODER_STATE_SHUTDOWN)
      {
        mon.Wait();
      }
      if (mState == DECODER_STATE_SHUTDOWN || mStopDecodeThreads)
        break;
    }

    PRUint32 videoQueueSize = mReader->mVideoQueue.GetSize();
    // Don't decode any more frames if we've filled our buffers.
    // Limits memory consumption.
    if (videoQueueSize > videoWaitThreshold) {
      videoWait = PR_TRUE;
    }

    // We don't want to consider skipping to the next keyframe if we've
    // only just started up the decode loop, so wait until we've decoded
    // some frames before allowing the keyframe skip.
    if (videoPump && videoQueueSize >= videoPumpThreshold) {
      videoPump = PR_FALSE;
    }
    if (!videoPump &&
        videoPlaying &&
        videoQueueSize < LOW_VIDEO_FRAMES)
    {
      skipToNextKeyframe = PR_TRUE;
    }

    // Determine how much audio data is decoded ahead of the current playback
    // position.
    int audioQueueSize = mReader->mAudioQueue.GetSize();
    PRInt64 initialDownloadPosition = 0;
    PRInt64 currentTime = 0;
    PRInt64 audioDecoded = 0;
    {
      MonitorAutoEnter mon(mDecoder->GetMonitor());
      currentTime = mCurrentFrameTime + mStartTime;
      audioDecoded = mReader->mAudioQueue.Duration();
      if (mAudioEndTime != -1) {
        audioDecoded += mAudioEndTime - currentTime;
      }
      initialDownloadPosition =
        mDecoder->GetCurrentStream()->GetCachedDataEnd(mDecoder->mDecoderPosition);
    }

    // Don't decode any audio if the audio decode is way ahead.
    if (audioDecoded > AMPLE_AUDIO_MS) {
      audioWait = PR_TRUE;
    }
    if (audioPump && audioDecoded > audioPumpThresholdMs) {
      audioPump = PR_FALSE;
    }
    if (!audioPump && audioPlaying && audioDecoded < LOW_AUDIO_MS) {
      skipToNextKeyframe = PR_TRUE;
    }

    if (videoPlaying && !videoWait) {
      videoPlaying = mReader->DecodeVideoFrame(skipToNextKeyframe, currentTime);
      {
        MonitorAutoEnter mon(mDecoder->GetMonitor());
        if (mDecoder->mDecoderPosition > initialDownloadPosition) {
          mBufferExhausted = PR_TRUE;
        }
      }
    }
    {
      MonitorAutoEnter mon(mDecoder->GetMonitor());
      initialDownloadPosition =
        mDecoder->GetCurrentStream()->GetCachedDataEnd(mDecoder->mDecoderPosition);
      mDecoder->GetMonitor().NotifyAll();
    }

    if (audioPlaying && !audioWait) {
      audioPlaying = mReader->DecodeAudioData();
      {
        MonitorAutoEnter mon(mDecoder->GetMonitor());
        if (mDecoder->mDecoderPosition > initialDownloadPosition) {
          mBufferExhausted = PR_TRUE;
        }
      }
    }

    {
      MonitorAutoEnter mon(mDecoder->GetMonitor());

      if (!IsPlaying() &&
          (!audioWait || !videoWait) &&
          (videoQueueSize < 2 || audioQueueSize < 2))
      {
        // Transitioning from 0 to 1 frames or from 1 to 2 frames could
        // affect HaveNextFrameData and hence what UpdateReadyStateForData does.
        // This could change us from HAVE_CURRENT_DATA to HAVE_FUTURE_DATA
        // (or even HAVE_ENOUGH_DATA), so we'd better trigger an
        // update to the ready state. We only need to do this if we're
        // not playing; if we're playing the playback code will post an update
        // whenever it advances a frame.
        UpdateReadyState();
      }

      if (mState == DECODER_STATE_SHUTDOWN || mStopDecodeThreads) {
        break;
      }
      if ((!HasAudio() || (audioWait && audioPlaying)) &&
          (!HasVideo() || (videoWait && videoPlaying)))
      {
        // All active bitstreams' decode is well ahead of the playback
        // position, we may as well wait have for the playback to catch up.
        mon.Wait();
      }
    }
  }

  {
    MonitorAutoEnter mon(mDecoder->GetMonitor());
    if (!mStopDecodeThreads &&
        mState != DECODER_STATE_SHUTDOWN &&
        mState != DECODER_STATE_SEEKING)
    {
      mState = DECODER_STATE_COMPLETED;
      mDecoder->GetMonitor().NotifyAll();
    }
  }
  LOG(PR_LOG_DEBUG, ("Shutting down DecodeLoop this=%p", this));
}

PRBool nsBuiltinDecoderStateMachine::IsPlaying()
{
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  return !mPlayStartTime.IsNull();
}

void nsBuiltinDecoderStateMachine::AudioLoop()
{
  NS_ASSERTION(OnAudioThread(), "Should be on audio thread.");
  LOG(PR_LOG_DEBUG, ("Begun audio thread/loop"));
  {
    MonitorAutoEnter mon(mDecoder->GetMonitor());
    mAudioCompleted = PR_FALSE;
  }
  PRInt64 audioStartTime = -1;
  while (1) {

    // Wait while we're not playing, and we're not shutting down, or we're 
    // playing and we've got no audio to play.
    {
      MonitorAutoEnter mon(mDecoder->GetMonitor());
      NS_ASSERTION(mState != DECODER_STATE_DECODING_METADATA,
                   "Should have meta data before audio started playing.");
      while (mState != DECODER_STATE_SHUTDOWN &&
             !mStopDecodeThreads &&
             (!IsPlaying() ||
              mState == DECODER_STATE_BUFFERING ||
              (mReader->mAudioQueue.GetSize() == 0 &&
               !mReader->mAudioQueue.AtEndOfStream())))
      {
        mon.Wait();
      }

      // If we're shutting down, break out and exit the audio thread.
      if (mState == DECODER_STATE_SHUTDOWN ||
          mStopDecodeThreads ||
          mReader->mAudioQueue.AtEndOfStream())
      {
        break;
      }
    }

    NS_ASSERTION(mReader->mAudioQueue.GetSize() > 0,
                 "Should have data to play");
    nsAutoPtr<SoundData> sound(mReader->mAudioQueue.PopFront());
    {
      MonitorAutoEnter mon(mDecoder->GetMonitor());
      NS_WARN_IF_FALSE(IsPlaying(), "Should be playing");
      // Awaken the decode loop if it's waiting for space to free up in the
      // audio queue.
      mDecoder->GetMonitor().NotifyAll();
    }

    if (audioStartTime == -1) {
      // Remember the presentation time of the first audio sample we play.
      // We add this to the position/played duration of the audio stream to
      // determine the audio clock time. Used for A/V sync.
      MonitorAutoEnter mon(mDecoder->GetMonitor());
      mAudioStartTime = audioStartTime = sound->mTime;
      LOG(PR_LOG_DEBUG, ("First audio sample has timestamp %lldms", mAudioStartTime));
    }

    PRInt64 audioEndTime = -1;
    {
      MonitorAutoEnter audioMon(mAudioMonitor);
      if (mAudioStream) {
        // The state machine could have paused since we've released the decoder
        // monitor and acquired the audio monitor. Rather than acquire both
        // monitors, the audio stream also maintains whether its paused or not.
        // This prevents us from doing a blocking write while holding the audio
        // monitor while paused; we would block, and the state machine won't be 
        // able to acquire the audio monitor in order to resume or destroy the
        // audio stream.
        if (!mAudioStream->IsPaused()) {
          mAudioStream->Write(sound->mAudioData,
                              sound->AudioDataLength(),
                              PR_TRUE);
          audioEndTime = sound->mTime + sound->mDuration;
          mDecoder->UpdatePlaybackOffset(sound->mOffset);
        } else {
          mReader->mAudioQueue.PushFront(sound);
          sound.forget();
        }
      }
    }
    sound = nsnull;

    {
      MonitorAutoEnter mon(mDecoder->GetMonitor());
      if (audioEndTime != -1) {
        mAudioEndTime = audioEndTime;
      }
      PRInt64 audioAhead = mAudioEndTime - mCurrentFrameTime - mStartTime;
      if (audioAhead > AMPLE_AUDIO_MS) {
        // We've pushed enough audio onto the hardware that we've queued up a
        // significant amount ahead of the playback position. The decode
        // thread will be going to sleep, so we won't get any new samples
        // anyway, so sleep until we need to push to the hardware again.
        Wait(AMPLE_AUDIO_MS / 2);
        // Kick the decode thread; since above we only do a NotifyAll when
        // we pop an audio chunk of the queue, the decoder won't wake up if
        // we've got no more decoded chunks to push to the hardware. We can
        // hit this condition if the last sample in the stream doesn't have
        // it's EOS flag set, and the decode thread sleeps just after decoding
        // that packet, but before realising there's no more packets.
        mon.NotifyAll();
      }
    }
  }
  if (mReader->mAudioQueue.AtEndOfStream() &&
      mState != DECODER_STATE_SHUTDOWN &&
      !mStopDecodeThreads)
  {
    // Last sample pushed to audio hardware, wait for the audio to finish,
    // before the audio thread terminates.
    MonitorAutoEnter audioMon(mAudioMonitor);
    if (mAudioStream) {
      mAudioStream->Drain();
    }
    LOG(PR_LOG_DEBUG, ("%p Reached audio stream end.", mDecoder));
  }
  {
    MonitorAutoEnter mon(mDecoder->GetMonitor());
    mAudioCompleted = PR_TRUE;
    UpdateReadyState();
    // Kick the decode and state machine threads; they may be sleeping waiting
    // for this to finish.
    mDecoder->GetMonitor().NotifyAll();
  }
  LOG(PR_LOG_DEBUG, ("Audio stream finished playing, audio thread exit"));
}

nsresult nsBuiltinDecoderStateMachine::Init()
{
  return mReader->Init();
}

void nsBuiltinDecoderStateMachine::StopPlayback(eStopMode aMode)
{
  NS_ASSERTION(IsCurrentThread(mDecoder->mStateMachineThread),
               "Should be on state machine thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  // Reset mPlayStartTime before we pause/shutdown the nsAudioStream. This is
  // so that if the audio loop is about to write audio, it will have the chance
  // to check to see if we're paused and not write the audio. If not, the
  // audio thread can block in the write, and we deadlock trying to acquire
  // the audio monitor upon resume playback.
  if (IsPlaying()) {
    mPlayDuration += TimeStamp::Now() - mPlayStartTime;
    mPlayStartTime = TimeStamp();
  }
  if (HasAudio()) {
    MonitorAutoExit exitMon(mDecoder->GetMonitor());
    MonitorAutoEnter audioMon(mAudioMonitor);
    if (mAudioStream) {
      if (aMode == AUDIO_PAUSE) {
        mAudioStream->Pause();
      } else if (aMode == AUDIO_SHUTDOWN) {
        mAudioStream->Shutdown();
        mAudioStream = nsnull;
      }
    }
  }
}

void nsBuiltinDecoderStateMachine::StartPlayback()
{
  NS_ASSERTION(IsCurrentThread(mDecoder->mStateMachineThread),
               "Should be on state machine thread.");
  NS_ASSERTION(!IsPlaying(), "Shouldn't be playing when StartPlayback() is called");
  mDecoder->GetMonitor().AssertCurrentThreadIn();
  LOG(PR_LOG_DEBUG, ("%p StartPlayback", mDecoder));
  if (HasAudio()) {
    MonitorAutoExit exitMon(mDecoder->GetMonitor());
    MonitorAutoEnter audioMon(mAudioMonitor);
    if (mAudioStream) {
      // We have an audiostream, so it must have been paused the last time
      // StopPlayback() was called.
      mAudioStream->Resume();
    } else {
      // No audiostream, create one.
      const nsVideoInfo& info = mReader->GetInfo();
      mAudioStream = new nsAudioStream();
      mAudioStream->Init(info.mAudioChannels,
                         info.mAudioRate,
                         nsAudioStream::FORMAT_FLOAT32);
      mAudioStream->SetVolume(mVolume);
    }
  }
  mPlayStartTime = TimeStamp::Now();
  mDecoder->GetMonitor().NotifyAll();
}

void nsBuiltinDecoderStateMachine::UpdatePlaybackPosition(PRInt64 aTime)
{
  NS_ASSERTION(IsCurrentThread(mDecoder->mStateMachineThread),
               "Should be on state machine thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();

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
  if (!mPositionChangeQueued) {
    mPositionChangeQueued = PR_TRUE;
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::PlaybackPositionChanged);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }
}

void nsBuiltinDecoderStateMachine::ClearPositionChangeFlag()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  mPositionChangeQueued = PR_FALSE;
}

nsHTMLMediaElement::NextFrameStatus nsBuiltinDecoderStateMachine::GetNextFrameStatus()
{
  MonitorAutoEnter mon(mDecoder->GetMonitor());
  if (IsBuffering() || IsSeeking()) {
    return nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE_BUFFERING;
  } else if (HaveNextFrameData()) {
    return nsHTMLMediaElement::NEXT_FRAME_AVAILABLE;
  }
  return nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE;
}

void nsBuiltinDecoderStateMachine::SetVolume(float volume)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  {
    MonitorAutoEnter audioMon(mAudioMonitor);
    if (mAudioStream) {
      mAudioStream->SetVolume(volume);
    }
  }
  {
    MonitorAutoEnter mon(mDecoder->GetMonitor());
    mVolume = volume;
  }
}

float nsBuiltinDecoderStateMachine::GetCurrentTime()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  return (float)mCurrentFrameTime / 1000.0;
}

PRInt64 nsBuiltinDecoderStateMachine::GetDuration()
{
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  if (mEndTime == -1 || mStartTime == -1)
    return -1;
  return mEndTime - mStartTime;
}

void nsBuiltinDecoderStateMachine::SetDuration(PRInt64 aDuration)
{
  NS_ASSERTION(NS_IsMainThread() || mDecoder->OnStateMachineThread(),
    "Should be on main or state machine thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  if (mStartTime != -1) {
    mEndTime = mStartTime + aDuration;
  } else {
    mStartTime = 0;
    mEndTime = aDuration;
  }
}

void nsBuiltinDecoderStateMachine::SetSeekable(PRBool aSeekable)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  mSeekable = aSeekable;
}

void nsBuiltinDecoderStateMachine::Shutdown()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  // Once we've entered the shutdown state here there's no going back.
  MonitorAutoEnter mon(mDecoder->GetMonitor());

  // Change state before issuing shutdown request to threads so those
  // threads can start exiting cleanly during the Shutdown call.
  LOG(PR_LOG_DEBUG, ("%p Changed state to SHUTDOWN", mDecoder));
  mState = DECODER_STATE_SHUTDOWN;
  mDecoder->GetMonitor().NotifyAll();
}

void nsBuiltinDecoderStateMachine::Decode()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  // When asked to decode, switch to decoding only if
  // we are currently buffering.
  MonitorAutoEnter mon(mDecoder->GetMonitor());
  if (mState == DECODER_STATE_BUFFERING) {
    LOG(PR_LOG_DEBUG, ("%p Changed state from BUFFERING to DECODING", mDecoder));
    mState = DECODER_STATE_DECODING;
    mDecoder->GetMonitor().NotifyAll();
  }
}

void nsBuiltinDecoderStateMachine::ResetPlayback()
{
  NS_ASSERTION(IsCurrentThread(mDecoder->mStateMachineThread),
               "Should be on state machine thread.");
  mVideoFrameEndTime = -1;
  mAudioStartTime = -1;
  mAudioEndTime = -1;
  mAudioCompleted = PR_FALSE;
}

void nsBuiltinDecoderStateMachine::Seek(float aTime)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  MonitorAutoEnter mon(mDecoder->GetMonitor());
  // nsBuiltinDecoder::mPlayState should be SEEKING while we seek, and
  // in that case nsBuiltinDecoder shouldn't be calling us.
  NS_ASSERTION(mState != DECODER_STATE_SEEKING,
               "We shouldn't already be seeking");
  NS_ASSERTION(mState >= DECODER_STATE_DECODING,
               "We should have loaded metadata");
  double t = aTime * 1000.0;
  if (t > PR_INT64_MAX) {
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
  LOG(PR_LOG_DEBUG, ("%p Changed state to SEEKING (to %f)", mDecoder, aTime));
  mState = DECODER_STATE_SEEKING;
}

void nsBuiltinDecoderStateMachine::StopDecodeThreads()
{
  NS_ASSERTION(IsCurrentThread(mDecoder->mStateMachineThread),
               "Should be on state machine thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();
  mStopDecodeThreads = PR_TRUE;
  mDecoder->GetMonitor().NotifyAll();
  if (mDecodeThread) {
    {
      MonitorAutoExit exitMon(mDecoder->GetMonitor());
      mDecodeThread->Shutdown();
    }
    mDecodeThread = nsnull;
  }
  if (mAudioThread) {
    {
      MonitorAutoExit exitMon(mDecoder->GetMonitor());
      mAudioThread->Shutdown();
    }
    mAudioThread = nsnull;
  }
}

nsresult
nsBuiltinDecoderStateMachine::StartDecodeThreads()
{
  NS_ASSERTION(IsCurrentThread(mDecoder->mStateMachineThread),
               "Should be on state machine thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();
  mStopDecodeThreads = PR_FALSE;
  if (!mDecodeThread && mState < DECODER_STATE_COMPLETED) {
    nsresult rv = NS_NewThread(getter_AddRefs(mDecodeThread));
    if (NS_FAILED(rv)) {
      mState = DECODER_STATE_SHUTDOWN;
      return rv;
    }
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &nsBuiltinDecoderStateMachine::DecodeLoop);
    mDecodeThread->Dispatch(event, NS_DISPATCH_NORMAL);
  }
  if (HasAudio() && !mAudioThread) {
    nsresult rv = NS_NewThread(getter_AddRefs(mAudioThread));
    if (NS_FAILED(rv)) {
      mState = DECODER_STATE_SHUTDOWN;
      return rv;
    }
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &nsBuiltinDecoderStateMachine::AudioLoop);
    mAudioThread->Dispatch(event, NS_DISPATCH_NORMAL);
  }
  return NS_OK;
}

nsresult nsBuiltinDecoderStateMachine::Run()
{
  NS_ASSERTION(IsCurrentThread(mDecoder->mStateMachineThread),
               "Should be on state machine thread.");
  nsMediaStream* stream = mDecoder->GetCurrentStream();
  NS_ENSURE_TRUE(stream, NS_ERROR_NULL_POINTER);

  while (PR_TRUE) {
    MonitorAutoEnter mon(mDecoder->GetMonitor());
    switch (mState) {
    case DECODER_STATE_SHUTDOWN:
      if (IsPlaying()) {
        StopPlayback(AUDIO_SHUTDOWN);
      }
      StopDecodeThreads();
      NS_ASSERTION(mState == DECODER_STATE_SHUTDOWN,
                   "How did we escape from the shutdown state???");
      return NS_OK;

    case DECODER_STATE_DECODING_METADATA:
      {
        LoadMetadata();
        if (mState == DECODER_STATE_SHUTDOWN) {
          continue;
        }

        VideoData* videoData = FindStartTime();
        if (videoData) {
          MonitorAutoExit exitMon(mDecoder->GetMonitor());
          RenderVideoFrame(videoData);
        }

        // Start the decode threads, so that we can pre buffer the streams.
        // and calculate the start time in order to determine the duration.
        if (NS_FAILED(StartDecodeThreads())) {
          continue;
        }

        NS_ASSERTION(mStartTime != -1, "Must have start time");
        NS_ASSERTION((!HasVideo() && !HasAudio()) ||
                     !mSeekable || mEndTime != -1,
                     "Active seekable media should have end time");
        NS_ASSERTION(!mSeekable || GetDuration() != -1, "Seekable media should have duration");
        LOG(PR_LOG_DEBUG, ("%p Media goes from %lldms to %lldms (duration %lldms) seekable=%d",
                           mDecoder, mStartTime, mEndTime, GetDuration(), mSeekable));

        if (mState == DECODER_STATE_SHUTDOWN)
          continue;

        // Inform the element that we've loaded the metadata and the
        // first frame.
        nsCOMPtr<nsIRunnable> metadataLoadedEvent =
          NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::MetadataLoaded);
        NS_DispatchToMainThread(metadataLoadedEvent, NS_DISPATCH_NORMAL);

        if (mState == DECODER_STATE_DECODING_METADATA) {
          LOG(PR_LOG_DEBUG, ("%p Changed state from DECODING_METADATA to DECODING", mDecoder));
          mState = DECODER_STATE_DECODING;
        }

        // Start playback.
        if (mDecoder->GetState() == nsBuiltinDecoder::PLAY_STATE_PLAYING) {
          if (!IsPlaying()) {
            StartPlayback();
          }
        }
      }
      break;

    case DECODER_STATE_DECODING:
      {
        if (NS_FAILED(StartDecodeThreads())) {
          continue;
        }

        AdvanceFrame();

        if (mState != DECODER_STATE_DECODING)
          continue;

        if (mBufferExhausted &&
            mDecoder->GetState() == nsBuiltinDecoder::PLAY_STATE_PLAYING &&
            !mDecoder->GetCurrentStream()->IsDataCachedToEndOfStream(mDecoder->mDecoderPosition) &&
            !mDecoder->GetCurrentStream()->IsSuspendedByCache() &&
            ((HasAudio() && mReader->mAudioQueue.Duration() < LOW_AUDIO_MS) ||
             (HasVideo() && (PRUint32)mReader->mVideoQueue.GetSize() < LOW_VIDEO_FRAMES)))
        {
          // There is at most one frame in the queue and there's
          // more data to load. Let's buffer to make sure we can play a
          // decent amount of video in the future.
          if (IsPlaying()) {
            StopPlayback(AUDIO_PAUSE);
            mDecoder->GetMonitor().NotifyAll();
          }

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

          mBufferingStart = TimeStamp::Now();
          PRPackedBool reliable;
          double playbackRate = mDecoder->ComputePlaybackRate(&reliable);
          mBufferingEndOffset = mDecoder->mDecoderPosition +
              BUFFERING_RATE(playbackRate) * BUFFERING_WAIT;
          mState = DECODER_STATE_BUFFERING;
          LOG(PR_LOG_DEBUG, ("Changed state from DECODING to BUFFERING"));
        } else {
          if (mBufferExhausted) {
            // This will wake up the decode thread and force it to try to
            // decode video and audio. This guarantees we make progress.
            mBufferExhausted = PR_FALSE;
            mDecoder->GetMonitor().NotifyAll();
          }
        }

      }
      break;

    case DECODER_STATE_SEEKING:
      {
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

        StopPlayback(AUDIO_SHUTDOWN);
        StopDecodeThreads();
        ResetPlayback();

        // SeekingStarted will do a UpdateReadyStateForData which will
        // inform the element and its users that we have no frames
        // to display
        {
          MonitorAutoExit exitMon(mDecoder->GetMonitor());
          nsCOMPtr<nsIRunnable> startEvent =
            NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::SeekingStarted);
          NS_DispatchToMainThread(startEvent, NS_DISPATCH_SYNC);
        }
        if (mCurrentFrameTime != mSeekTime - mStartTime) {
          nsresult res;
          {
            MonitorAutoExit exitMon(mDecoder->GetMonitor());
            // Now perform the seek. We must not hold the state machine monitor
            // while we seek, since the seek decodes.
            res = mReader->Seek(seekTime, mStartTime, mEndTime);
          }
          if (NS_SUCCEEDED(res)){
            SoundData* audio = HasAudio() ? mReader->mAudioQueue.PeekFront() : nsnull;
            if (audio) {
              mPlayDuration = TimeDuration::FromMilliseconds(audio->mTime);
            }
            if (HasVideo()) {
              nsAutoPtr<VideoData> video(mReader->mVideoQueue.PeekFront());
              if (video) {
                RenderVideoFrame(video);
                if (!audio) {
                  NS_ASSERTION(video->mTime <= seekTime &&
                               seekTime <= video->mEndTime,
                               "Seek target should lie inside the first frame after seek");
                  mPlayDuration = TimeDuration::FromMilliseconds(seekTime);
                }
              }
              mReader->mVideoQueue.PopFront();
            }
            UpdatePlaybackPosition(seekTime);
          }
        }
        mDecoder->StartProgressUpdates();
        if (mState == DECODER_STATE_SHUTDOWN)
          continue;

        // Try to decode another frame to detect if we're at the end...
        LOG(PR_LOG_DEBUG, ("Seek completed, mCurrentFrameTime=%lld\n", mCurrentFrameTime));

        // Change state to DECODING or COMPLETED now. SeekingStopped will
        // call nsBuiltinDecoderStateMachine::Seek to reset our state to SEEKING
        // if we need to seek again.
        
        nsCOMPtr<nsIRunnable> stopEvent;
        if (mCurrentFrameTime == mEndTime) {
          LOG(PR_LOG_DEBUG, ("%p Changed state from SEEKING (to %lldms) to COMPLETED",
                             mDecoder, seekTime));
          stopEvent = NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::SeekingStoppedAtEnd);
          mState = DECODER_STATE_COMPLETED;
        } else {
          LOG(PR_LOG_DEBUG, ("%p Changed state from SEEKING (to %lldms) to DECODING",
                             mDecoder, seekTime));
          stopEvent = NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::SeekingStopped);
          mState = DECODER_STATE_DECODING;
        }
        mBufferExhausted = PR_FALSE;
        mDecoder->GetMonitor().NotifyAll();

        {
          MonitorAutoExit exitMon(mDecoder->GetMonitor());
          NS_DispatchToMainThread(stopEvent, NS_DISPATCH_SYNC);
        }
      }
      break;

    case DECODER_STATE_BUFFERING:
      {
        TimeStamp now = TimeStamp::Now();
        if (now - mBufferingStart < TimeDuration::FromSeconds(BUFFERING_WAIT) &&
            mDecoder->GetCurrentStream()->GetCachedDataEnd(mDecoder->mDecoderPosition) < mBufferingEndOffset &&
            !mDecoder->GetCurrentStream()->IsDataCachedToEndOfStream(mDecoder->mDecoderPosition) &&
            !mDecoder->GetCurrentStream()->IsSuspendedByCache()) {
          LOG(PR_LOG_DEBUG,
              ("In buffering: buffering data until %d bytes available or %f seconds",
               PRUint32(mBufferingEndOffset - mDecoder->GetCurrentStream()->GetCachedDataEnd(mDecoder->mDecoderPosition)),
               BUFFERING_WAIT - (now - mBufferingStart).ToSeconds()));
          Wait(1000);
          if (mState == DECODER_STATE_SHUTDOWN)
            continue;
        } else {
          LOG(PR_LOG_DEBUG, ("%p Changed state from BUFFERING to DECODING", mDecoder));
          LOG(PR_LOG_DEBUG, ("%p Buffered for %lf seconds",
                             mDecoder,
                             (TimeStamp::Now() - mBufferingStart).ToSeconds()));
          mState = DECODER_STATE_DECODING;
        }

        if (mState != DECODER_STATE_BUFFERING) {
          mBufferExhausted = PR_FALSE;
          // Notify to allow blocked decoder thread to continue
          mDecoder->GetMonitor().NotifyAll();
          UpdateReadyState();
          if (mDecoder->GetState() == nsBuiltinDecoder::PLAY_STATE_PLAYING) {
            if (!IsPlaying()) {
              StartPlayback();
            }
          }
        }

        break;
      }

    case DECODER_STATE_COMPLETED:
      {
        if (NS_FAILED(StartDecodeThreads())) {
          continue;
        }

        // Play the remaining media. We want to run AdvanceFrame() at least
        // once to ensure the current playback position is advanced to the
        // end of the media, and so that we update the readyState.
        do {
          AdvanceFrame();
        } while (mState == DECODER_STATE_COMPLETED &&
                 (mReader->mVideoQueue.GetSize() > 0 ||
                 (HasAudio() && !mAudioCompleted)));

        if (mAudioStream) {
          // Close the audio stream so that next time audio is used a new stream
          // is created. The StopPlayback call also resets the IsPlaying() state
          // so audio is restarted correctly.
          StopPlayback(AUDIO_SHUTDOWN);
        }

        if (mState != DECODER_STATE_COMPLETED)
          continue;

        LOG(PR_LOG_DEBUG, ("Shutting down the state machine thread"));
        StopDecodeThreads();

        if (mDecoder->GetState() == nsBuiltinDecoder::PLAY_STATE_PLAYING) {
          PRInt64 videoTime = HasVideo() ? mVideoFrameEndTime : 0;
          PRInt64 clockTime = NS_MAX(mEndTime, NS_MAX(videoTime, GetAudioClock()));
          UpdatePlaybackPosition(clockTime);
          {
            MonitorAutoExit exitMon(mDecoder->GetMonitor());
            nsCOMPtr<nsIRunnable> event =
              NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::PlaybackEnded);
            NS_DispatchToMainThread(event, NS_DISPATCH_SYNC);
          }
        }

        while (mState == DECODER_STATE_COMPLETED) {
          mDecoder->GetMonitor().Wait();
        }
      }
      break;
    }
  }

  return NS_OK;
}

void nsBuiltinDecoderStateMachine::RenderVideoFrame(VideoData* aData)
{
  NS_ASSERTION(IsCurrentThread(mDecoder->mStateMachineThread), "Should be on state machine thread.");

  if (aData->mDuplicate) {
    return;
  }

  nsRefPtr<Image> image = aData->mImage;
  if (image) {
    const nsVideoInfo& info = mReader->GetInfo();
    mDecoder->SetVideoData(gfxIntSize(info.mPicture.width, info.mPicture.height), info.mPixelAspectRatio, image);
  }
}

PRInt64
nsBuiltinDecoderStateMachine::GetAudioClock()
{
  NS_ASSERTION(IsCurrentThread(mDecoder->mStateMachineThread), "Should be on state machine thread.");
  if (!mAudioStream || !HasAudio())
    return -1;
  PRInt64 t = mAudioStream->GetPosition();
  return (t == -1) ? -1 : t + mAudioStartTime;
}

void nsBuiltinDecoderStateMachine::AdvanceFrame()
{
  NS_ASSERTION(IsCurrentThread(mDecoder->mStateMachineThread), "Should be on state machine thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  // When it's time to display a frame, decode the frame and display it.
  if (mDecoder->GetState() == nsBuiltinDecoder::PLAY_STATE_PLAYING) {
    if (!IsPlaying()) {
      StartPlayback();
      mDecoder->GetMonitor().NotifyAll();
    }

    if (HasAudio() && mAudioStartTime == -1 && !mAudioCompleted) {
      // We've got audio (so we should sync off the audio clock), but we've not
      // played a sample on the audio thread, so we can't get a time from the
      // audio clock. Just wait and then return, to give the audio clock time
      // to tick.  This should really wait for a specific signal from the audio
      // thread rather than polling after a sleep.  See bug 568431 comment 4.
      Wait(AUDIO_DURATION_MS);
      return;
    }

    // Determine the clock time. If we've got audio, and we've not reached
    // the end of the audio, use the audio clock. However if we've finished
    // audio, or don't have audio, use the system clock.
    PRInt64 clock_time = -1;
    PRInt64 audio_time = GetAudioClock();
    if (HasAudio() && !mAudioCompleted && audio_time != -1) {
      clock_time = audio_time;
      // Resync against the audio clock, while we're trusting the
      // audio clock. This ensures no "drift", particularly on Linux.
      mPlayStartTime = TimeStamp::Now() - TimeDuration::FromMilliseconds(clock_time);
    } else {
      // Sound is disabled on this system. Sync to the system clock.
      TimeDuration t = TimeStamp::Now() - mPlayStartTime + mPlayDuration;
      clock_time = (PRInt64)(1000 * t.ToSeconds());
      // Ensure the clock can never go backwards.
      NS_ASSERTION(mCurrentFrameTime <= clock_time, "Clock should go forwards");
      clock_time = NS_MAX(mCurrentFrameTime, clock_time) + mStartTime;
    }

    NS_ASSERTION(clock_time >= mStartTime, "Should have positive clock time.");
    nsAutoPtr<VideoData> videoData;
    if (mReader->mVideoQueue.GetSize() > 0) {
      VideoData* data = mReader->mVideoQueue.PeekFront();
      while (clock_time >= data->mTime) {
        mVideoFrameEndTime = data->mEndTime;
        videoData = data;
        mReader->mVideoQueue.PopFront();
        mDecoder->UpdatePlaybackOffset(data->mOffset);
        if (mReader->mVideoQueue.GetSize() == 0)
          break;
        data = mReader->mVideoQueue.PeekFront();
      }
    }

    PRInt64 frameDuration = AUDIO_DURATION_MS;
    if (videoData) {
      // Decode one frame and display it
      NS_ASSERTION(videoData->mTime >= mStartTime, "Should have positive frame time");
      {
        MonitorAutoExit exitMon(mDecoder->GetMonitor());
        // If we have video, we want to increment the clock in steps of the frame
        // duration.
        RenderVideoFrame(videoData);
      }
      mDecoder->GetMonitor().NotifyAll();
      frameDuration = videoData->mEndTime - videoData->mTime;
      videoData = nsnull;
    }

    // Cap the current time to the larger of the audio and video end time.
    // This ensures that if we're running off the system clock, we don't
    // advance the clock to after the media end time.
    if (mVideoFrameEndTime != -1 || mAudioEndTime != -1) {
      // These will be non -1 if we've displayed a video frame, or played an audio sample.
      clock_time = NS_MIN(clock_time, NS_MAX(mVideoFrameEndTime, mAudioEndTime));
      if (clock_time - mStartTime > mCurrentFrameTime) {
        // Only update the playback position if the clock time is greater
        // than the previous playback position. The audio clock can
        // sometimes report a time less than its previously reported in
        // some situations, and we need to gracefully handle that.
        UpdatePlaybackPosition(clock_time);
      }
    }

    // If the number of audio/video samples queued has changed, either by
    // this function popping and playing a video sample, or by the audio
    // thread popping and playing an audio sample, we may need to update our
    // ready state. Post an update to do so.
    UpdateReadyState();

    NS_ASSERTION(frameDuration >= 0, "Frame duration must be positive.");
    Wait(frameDuration);
  } else {
    if (IsPlaying()) {
      StopPlayback(AUDIO_PAUSE);
      mDecoder->GetMonitor().NotifyAll();
    }

    if (mState == DECODER_STATE_DECODING ||
        mState == DECODER_STATE_COMPLETED) {
      mDecoder->GetMonitor().Wait();
    }
  }
}

void nsBuiltinDecoderStateMachine::Wait(PRUint32 aMs) {
  mDecoder->GetMonitor().AssertCurrentThreadIn();
  TimeStamp end = TimeStamp::Now() + TimeDuration::FromMilliseconds(aMs);
  TimeStamp now;
  while ((now = TimeStamp::Now()) < end &&
         mState != DECODER_STATE_SHUTDOWN &&
         mState != DECODER_STATE_SEEKING)
  {
    TimeDuration d = end - now;
    PRInt64 ms = d.ToSeconds() * 1000;
    if (ms == 0) {
      break;
    }
    NS_ASSERTION(ms <= aMs && ms > 0,
                 "nsBuiltinDecoderStateMachine::Wait interval very wrong!");
    mDecoder->GetMonitor().Wait(PR_MillisecondsToInterval(ms));
  }
}

VideoData* nsBuiltinDecoderStateMachine::FindStartTime()
{
  NS_ASSERTION(IsCurrentThread(mDecoder->mStateMachineThread), "Should be on state machine thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();
  PRInt64 startTime = 0;
  mStartTime = 0;
  VideoData* v = nsnull;
  {
    MonitorAutoExit exitMon(mDecoder->GetMonitor());
    v = mReader->FindStartTime(mReader->GetInfo().mDataOffset, startTime);
  }
  if (startTime != 0) {
    mStartTime = startTime;
    if (mGotDurationFromHeader) {
      NS_ASSERTION(mEndTime != -1,
                   "We should have mEndTime as supplied duration here");
      // We were specified a duration from a Content-Duration HTTP header.
      // Adjust mEndTime so that mEndTime-mStartTime matches the specified
      // duration.
      mEndTime = mStartTime + mEndTime;
    }
  }
  LOG(PR_LOG_DEBUG, ("%p Media start time is %lldms", mDecoder, mStartTime));
  return v;
}

void nsBuiltinDecoderStateMachine::FindEndTime() 
{
  NS_ASSERTION(OnStateMachineThread(), "Should be on state machine thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  nsMediaStream* stream = mDecoder->GetCurrentStream();

  // Seek to the end of file to find the length and duration.
  PRInt64 length = stream->GetLength();
  NS_ASSERTION(length > 0, "Must have a content length to get end time");

  mEndTime = 0;
  PRInt64 endTime = 0;
  {
    MonitorAutoExit exitMon(mDecoder->GetMonitor());
    endTime = mReader->FindEndTime(length);
  }
  if (endTime != -1) {
    mEndTime = endTime;
  }

  LOG(PR_LOG_DEBUG, ("%p Media end time is %lldms", mDecoder, mEndTime));   
}

void nsBuiltinDecoderStateMachine::UpdateReadyState() {
  mDecoder->GetMonitor().AssertCurrentThreadIn();

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

void nsBuiltinDecoderStateMachine::LoadMetadata()
{
  NS_ASSERTION(IsCurrentThread(mDecoder->mStateMachineThread),
               "Should be on state machine thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  LOG(PR_LOG_DEBUG, ("Loading Media Headers"));

  {
    MonitorAutoExit exitMon(mDecoder->GetMonitor());
    mReader->ReadMetadata();
  }
  mDecoder->StartProgressUpdates();
  const nsVideoInfo& info = mReader->GetInfo();

  if (!info.mHasVideo && !info.mHasAudio) {
    mState = DECODER_STATE_SHUTDOWN;      
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(mDecoder, &nsBuiltinDecoder::DecodeError);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
    return;
  }
}
