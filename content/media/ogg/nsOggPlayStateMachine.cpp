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
#include "nsOggDecoder.h"
#include "nsOggReader.h"
#include "nsOggPlayStateMachine.h"
#include "oggplay/oggplay.h"
#include "mozilla/mozalloc.h"
#include "nsOggHacks.h"

using namespace mozilla::layers;
using mozilla::MonitorAutoExit;

// Adds two 32bit unsigned numbers, retuns PR_TRUE if addition succeeded,
// or PR_FALSE the if addition would result in an overflow.
static PRBool AddOverflow(PRUint32 a, PRUint32 b, PRUint32& aResult);

#ifdef PR_LOGGING
extern PRLogModuleInfo* gOggDecoderLog;
#define LOG(type, msg) PR_LOG(gOggDecoderLog, type, msg)
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

// The frame rate to use if there is no video data in the resource to
// be played.
#define AUDIO_FRAME_RATE 25.0

nsOggPlayStateMachine::nsOggPlayStateMachine(nsOggDecoder* aDecoder) :
  mDecoder(aDecoder),
  mState(DECODER_STATE_DECODING_METADATA),
  mAudioMonitor("media.audiostream"),
  mCbCrSize(0),
  mPlayDuration(0),
  mBufferingEndOffset(0),
  mStartTime(-1),
  mEndTime(-1),
  mSeekTime(0),
  mCurrentFrameTime(0),
  mAudioStartTime(-1),
  mAudioEndTime(-1),
  mVideoFrameTime(-1),
  mVolume(1.0),
  mSeekable(PR_TRUE),
  mPositionChangeQueued(PR_FALSE),
  mAudioCompleted(PR_FALSE),
  mBufferExhausted(PR_FALSE),
  mGotDurationFromHeader(PR_FALSE),
  mStopDecodeThreads(PR_TRUE)
{
  MOZ_COUNT_CTOR(nsOggPlayStateMachine);
}

nsOggPlayStateMachine::~nsOggPlayStateMachine()
{
  MOZ_COUNT_DTOR(nsOggPlayStateMachine);
}

void nsOggPlayStateMachine::DecodeLoop()
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

  // If we have fewer than videoKeyframeSkipThreshold decoded frames, and
  // we're not "pumping video", we'll skip the video up to the next keyframe
  // which is at or after the current playback position.
  const unsigned videoKeyframeSkipThreshold = 1;

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
  const unsigned audioPumpThresholdMs = 250;

  // If audio queue has less than this many ms of decoded audio, we won't risk
  // trying to decode the video, we'll skip decoding video up to the next
  // keyframe.
  const unsigned lowAudioThresholdMs = 100;

  // If more than this many ms of decoded audio is queued, we'll hold off
  // decoding more audio.
  const unsigned audioWaitThresholdMs = 2000;

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
        videoQueueSize < videoKeyframeSkipThreshold)
    {
      skipToNextKeyframe = PR_TRUE;
    }

    PRInt64 initialDownloadPosition = 0;
    PRInt64 currentTime = 0;
    {
      MonitorAutoEnter mon(mDecoder->GetMonitor());
      initialDownloadPosition =
        mDecoder->mStream->GetCachedDataEnd(mDecoder->mDecoderPosition);
      currentTime = mCurrentFrameTime + mStartTime;
    }

    // Determine how much audio data is decoded ahead of the current playback
    // position.
    int audioQueueSize = mReader->mAudioQueue.GetSize();
    PRInt64 audioDecoded = mReader->mAudioQueue.Duration();

    // Don't decode any audio if the audio decode is way ahead, or if we're
    // skipping to the next video keyframe and the audio is marginally ahead.
    if (audioDecoded > audioWaitThresholdMs ||
        (skipToNextKeyframe && audioDecoded > audioPumpThresholdMs)) {
      audioWait = PR_TRUE;
    }
    if (audioPump && audioDecoded > audioPumpThresholdMs) {
      audioPump = PR_FALSE;
    }
    if (!audioPump && audioPlaying && audioDecoded < lowAudioThresholdMs) {
      skipToNextKeyframe = PR_TRUE;
    }

    if (videoPlaying && !videoWait) {
      videoPlaying = mReader->DecodeVideoPage(skipToNextKeyframe, currentTime);
      {
        MonitorAutoEnter mon(mDecoder->GetMonitor());
        if (mDecoder->mDecoderPosition >= initialDownloadPosition) {
          mBufferExhausted = PR_TRUE;
        }
      }
    }
    {
      MonitorAutoEnter mon(mDecoder->GetMonitor());
      initialDownloadPosition =
        mDecoder->mStream->GetCachedDataEnd(mDecoder->mDecoderPosition);
      mDecoder->GetMonitor().NotifyAll();
    }

    if (audioPlaying && !audioWait) {
      audioPlaying = mReader->DecodeAudioPage();
      {
        MonitorAutoEnter mon(mDecoder->GetMonitor());
        if (mDecoder->mDecoderPosition >= initialDownloadPosition) {
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

PRBool nsOggPlayStateMachine::IsPlaying()
{
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  return !mPlayStartTime.IsNull();
}

void nsOggPlayStateMachine::AudioLoop()
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
      NS_ASSERTION(IsPlaying(), "Should be playing");
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
        } else {
          mReader->mAudioQueue.PushFront(sound);
          sound.forget();
        }
      }
    }
    mAudioEndTime = sound->mTime + sound->mDuration;
    sound = nsnull;

    if (mReader->mAudioQueue.AtEndOfStream()) {
      // Last sample pushed to audio hardware, wait for the audio to finish,
      // before the audio thread terminates.
      MonitorAutoEnter audioMon(mAudioMonitor);
      if (mAudioStream) {
        mAudioStream->Drain();
      }
      LOG(PR_LOG_DEBUG, ("%p Reached audio stream end.", mDecoder));
    }
  }
  {
    MonitorAutoEnter mon(mDecoder->GetMonitor());
    mAudioCompleted = PR_TRUE;
  }
  LOG(PR_LOG_DEBUG, ("Audio stream finished playing, audio thread exit"));
}

nsresult nsOggPlayStateMachine::Init()
{
  mReader = new nsOggReader(this);
  return mReader->Init();
}

void nsOggPlayStateMachine::StopPlayback(eStopMode aMode)
{
  NS_ASSERTION(IsThread(mDecoder->mStateMachineThread),
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

void nsOggPlayStateMachine::StartPlayback()
{
  NS_ASSERTION(IsThread(mDecoder->mStateMachineThread),
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
      mAudioStream = new nsAudioStream();
      mAudioStream->Init(mInfo.mAudioChannels,
                         mInfo.mAudioRate,
                         nsAudioStream::FORMAT_FLOAT32);
      mAudioStream->SetVolume(mVolume);
    }
  }
  mPlayStartTime = TimeStamp::Now();
  mDecoder->GetMonitor().NotifyAll();
}

void nsOggPlayStateMachine::UpdatePlaybackPosition(PRInt64 aTime)
{
  NS_ASSERTION(IsThread(mDecoder->mStateMachineThread),
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
      NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, DurationChanged);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }
  if (!mPositionChangeQueued) {
    mPositionChangeQueued = PR_TRUE;
    nsCOMPtr<nsIRunnable> event =
      NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, PlaybackPositionChanged);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }
}

void nsOggPlayStateMachine::ClearPositionChangeFlag()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  mPositionChangeQueued = PR_FALSE;
}

nsHTMLMediaElement::NextFrameStatus nsOggPlayStateMachine::GetNextFrameStatus()
{
  MonitorAutoEnter mon(mDecoder->GetMonitor());
  if (IsBuffering() || IsSeeking()) {
    return nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE_BUFFERING;
  } else if (HaveNextFrameData()) {
    return nsHTMLMediaElement::NEXT_FRAME_AVAILABLE;
  }
  return nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE;
}

void nsOggPlayStateMachine::SetVolume(float volume)
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

float nsOggPlayStateMachine::GetCurrentTime()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  return (float)mCurrentFrameTime / 1000.0;
}

PRInt64 nsOggPlayStateMachine::GetDuration()
{
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  if (mEndTime == -1 || mStartTime == -1)
    return -1;
  return mEndTime - mStartTime;
}

void nsOggPlayStateMachine::SetDuration(PRInt64 aDuration)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  if (mStartTime != -1) {
    mEndTime = mStartTime + aDuration;
  } else {
    mStartTime = 0;
    mEndTime = aDuration;
  }
}

void nsOggPlayStateMachine::SetSeekable(PRBool aSeekable)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  mSeekable = aSeekable;
}

void nsOggPlayStateMachine::Shutdown()
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

void nsOggPlayStateMachine::Decode()
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

void nsOggPlayStateMachine::ResetPlayback()
{
  NS_ASSERTION(IsThread(mDecoder->mStateMachineThread),
               "Should be on state machine thread.");
  mVideoFrameTime = -1;
  mAudioStartTime = -1;
  mAudioEndTime = -1;
  mAudioCompleted = PR_FALSE;
}

void nsOggPlayStateMachine::Seek(float aTime)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  MonitorAutoEnter mon(mDecoder->GetMonitor());
  // nsOggDecoder::mPlayState should be SEEKING while we seek, and
  // in that case nsOggDecoder shouldn't be calling us.
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

void nsOggPlayStateMachine::StopDecodeThreads()
{
  NS_ASSERTION(IsThread(mDecoder->mStateMachineThread),
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
nsOggPlayStateMachine::StartDecodeThreads()
{
  NS_ASSERTION(IsThread(mDecoder->mStateMachineThread),
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
      NS_NEW_RUNNABLE_METHOD(nsOggPlayStateMachine, this, DecodeLoop);
    mDecodeThread->Dispatch(event, NS_DISPATCH_NORMAL);
  }
  if (HasAudio() && !mAudioThread) {
    nsresult rv = NS_NewThread(getter_AddRefs(mAudioThread));
    if (NS_FAILED(rv)) {
      mState = DECODER_STATE_SHUTDOWN;
      return rv;
    }
    nsCOMPtr<nsIRunnable> event =
      NS_NEW_RUNNABLE_METHOD(nsOggPlayStateMachine, this, AudioLoop);
    mAudioThread->Dispatch(event, NS_DISPATCH_NORMAL);
  }
  return NS_OK;
}

nsresult nsOggPlayStateMachine::Run()
{
  NS_ASSERTION(IsThread(mDecoder->mStateMachineThread),
               "Should be on state machine thread.");
  nsMediaStream* stream = mDecoder->mStream;
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
        LoadOggHeaders();
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

        // Inform the element that we've loaded the Ogg metadata and the
        // first frame.
        nsCOMPtr<nsIRunnable> metadataLoadedEvent =
          NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, MetadataLoaded);
        NS_DispatchToMainThread(metadataLoadedEvent, NS_DISPATCH_NORMAL);

        if (mState == DECODER_STATE_DECODING_METADATA) {
          LOG(PR_LOG_DEBUG, ("%p Changed state from DECODING_METADATA to DECODING", mDecoder));
          mState = DECODER_STATE_DECODING;
        }

        // Start playback.
        if (mDecoder->GetState() == nsOggDecoder::PLAY_STATE_PLAYING) {
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
            mDecoder->GetState() == nsOggDecoder::PLAY_STATE_PLAYING &&
            !mDecoder->mStream->IsDataCachedToEndOfStream(mDecoder->mDecoderPosition) &&
            !mDecoder->mStream->IsSuspendedByCache()) {
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
            NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, SeekingStarted);
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
                               seekTime <= video->mTime + mInfo.mCallbackPeriod,
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
        // call nsOggPlayStateMachine::Seek to reset our state to SEEKING
        // if we need to seek again.
        
        nsCOMPtr<nsIRunnable> stopEvent;
        if (mCurrentFrameTime == mEndTime) {
          LOG(PR_LOG_DEBUG, ("%p Changed state from SEEKING (to %lldms) to COMPLETED",
                             mDecoder, seekTime));
          stopEvent = NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, SeekingStoppedAtEnd);
          mState = DECODER_STATE_COMPLETED;
        } else {
          LOG(PR_LOG_DEBUG, ("%p Changed state from SEEKING (to %lldms) to DECODING",
                             mDecoder, seekTime));
          stopEvent = NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, SeekingStopped);
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
            mDecoder->mStream->GetCachedDataEnd(mDecoder->mDecoderPosition) < mBufferingEndOffset &&
            !mDecoder->mStream->IsDataCachedToEndOfStream(mDecoder->mDecoderPosition) &&
            !mDecoder->mStream->IsSuspendedByCache()) {
          LOG(PR_LOG_DEBUG,
              ("In buffering: buffering data until %d bytes available or %f seconds",
               PRUint32(mBufferingEndOffset - mDecoder->mStream->GetCachedDataEnd(mDecoder->mDecoderPosition)),
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
          if (mDecoder->GetState() == nsOggDecoder::PLAY_STATE_PLAYING) {
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

        // Play the remaining media.
        while (mState == DECODER_STATE_COMPLETED &&
               (mReader->mVideoQueue.GetSize() > 0 ||
                (HasAudio() && !mAudioCompleted)))
        {
          AdvanceFrame();
        }

        if (mAudioStream) {
          // Close the audop stream so that next time audio is used a new stream
          // is created. The StopPlayback call also resets the IsPlaying() state
          // so audio is restarted correctly.
          StopPlayback(AUDIO_SHUTDOWN);
        }

        if (mState != DECODER_STATE_COMPLETED)
          continue;

        LOG(PR_LOG_DEBUG, ("Shutting down the state machine thread"));
        StopDecodeThreads();

        if (mDecoder->GetState() == nsOggDecoder::PLAY_STATE_PLAYING) {
          PRInt64 videoTime = HasVideo() ? (mVideoFrameTime + mInfo.mCallbackPeriod) : 0;
          PRInt64 clockTime = NS_MAX(mEndTime, NS_MAX(videoTime, GetAudioClock()));
          UpdatePlaybackPosition(clockTime);
          {
            MonitorAutoExit exitMon(mDecoder->GetMonitor());
            nsCOMPtr<nsIRunnable> event =
              NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, PlaybackEnded);
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

static void ToARGBHook(const PlanarYCbCrImage::Data& aData, PRUint8* aOutput)
{
  OggPlayYUVChannels yuv;
  NS_ASSERTION(aData.mYStride == aData.mYSize.width,
               "Stride not supported");
  NS_ASSERTION(aData.mCbCrStride == aData.mCbCrSize.width,
               "Stride not supported");
  yuv.ptry = aData.mYChannel;
  yuv.ptru = aData.mCbChannel;
  yuv.ptrv = aData.mCrChannel;
  yuv.uv_width = aData.mCbCrSize.width;
  yuv.uv_height = aData.mCbCrSize.height;
  yuv.y_width = aData.mYSize.width;
  yuv.y_height = aData.mYSize.height;

  OggPlayRGBChannels rgb;
  rgb.ptro = aOutput;
  rgb.rgb_width = aData.mYSize.width;
  rgb.rgb_height = aData.mYSize.height;

  oggplay_yuv2bgra(&yuv, &rgb);  
}

void nsOggPlayStateMachine::RenderVideoFrame(VideoData* aData)
{
  NS_ASSERTION(IsThread(mDecoder->mStateMachineThread), "Should be on state machine thread.");

  if (aData->mDuplicate) {
    return;
  }

  unsigned xSubsample = (aData->mBuffer[1].width != 0) ?
    (aData->mBuffer[0].width / aData->mBuffer[1].width) : 0;

  unsigned ySubsample = (aData->mBuffer[1].height != 0) ?
    (aData->mBuffer[0].height / aData->mBuffer[1].height) : 0;

  if (xSubsample == 0 || ySubsample == 0) {
    // We can't perform yCbCr to RGB, so we can't render the frame...
    return;
  }
  NS_ASSERTION(mInfo.mPicture.width != 0 && mInfo.mPicture.height != 0,
               "We can only render non-zero-sized video");

  unsigned cbCrStride = mInfo.mPicture.width / xSubsample;
  unsigned cbCrHeight = mInfo.mPicture.height / ySubsample;

  // Ensure the picture size specified in the headers can be extracted out of
  // the frame we've been supplied without indexing out of bounds.
  PRUint32 picXLimit;
  PRUint32 picYLimit;
  if (!AddOverflow(mInfo.mPicture.x, mInfo.mPicture.width, picXLimit) ||
      picXLimit > PR_ABS(aData->mBuffer[0].stride) ||
      !AddOverflow(mInfo.mPicture.y, mInfo.mPicture.height, picYLimit) ||
      picYLimit > PR_ABS(aData->mBuffer[0].height))
  {
    // The specified picture dimensions can't be contained inside the video
    // frame, we'll stomp memory if we try to copy it. Fail.
    return;
  }

  unsigned cbCrSize = PR_ABS(aData->mBuffer[0].stride * aData->mBuffer[0].height) +
                      PR_ABS(aData->mBuffer[1].stride * aData->mBuffer[1].height) * 2;
  if (cbCrSize != mCbCrSize) {
    mCbCrSize = cbCrSize;
    mCbCrBuffer = static_cast<unsigned char*>(moz_xmalloc(cbCrSize));
    if (!mCbCrBuffer) {
      // Malloc failed...
      NS_WARNING("Malloc failure allocating YCbCr->RGB buffer");
      return;
    }
  }

  unsigned char* data = mCbCrBuffer.get();

  unsigned char* y = data;
  unsigned char* cb = y + (mInfo.mPicture.width * PR_ABS(aData->mBuffer[0].height));
  unsigned char* cr = cb + (cbCrStride * PR_ABS(aData->mBuffer[1].height));

  unsigned char* p = y;
  unsigned yStride = mInfo.mPicture.width;
  unsigned char* q = aData->mBuffer[0].data + mInfo.mPicture.x +
                     aData->mBuffer[0].stride * mInfo.mPicture.y;
  for(unsigned i=0; i < mInfo.mPicture.height; ++i) {
    NS_ASSERTION(q + mInfo.mPicture.width <
                 aData->mBuffer[0].data + aData->mBuffer[0].stride * aData->mBuffer[0].height,
                 "Y read must be in bounds");
    NS_ASSERTION(p + mInfo.mPicture.width < data + mCbCrSize,
                 "Memory copy 1 will stomp");
    memcpy(p, q, mInfo.mPicture.width);
    p += mInfo.mPicture.width;
    q += aData->mBuffer[0].stride;
  }

  unsigned xo = xSubsample ? (mInfo.mPicture.x / xSubsample) : 0;
  unsigned yo = ySubsample ? aData->mBuffer[1].stride * (mInfo.mPicture.y / ySubsample) : 0;

  unsigned cbCrOffset = xo+yo;
  p = cb;
  q = aData->mBuffer[1].data + cbCrOffset;
  unsigned char* p2 = cr;
  unsigned char* q2 = aData->mBuffer[2].data + cbCrOffset;
#ifdef DEBUG
  unsigned char* buffer1Limit =
    aData->mBuffer[1].data + aData->mBuffer[1].stride * aData->mBuffer[1].height;
  unsigned char* buffer2Limit =
    aData->mBuffer[2].data + aData->mBuffer[2].stride * aData->mBuffer[2].height;
#endif
  for(unsigned i=0; i < cbCrHeight; ++i) {
    NS_ASSERTION(q + cbCrStride <= buffer1Limit,
                 "Cb source read must be within bounds");
    NS_ASSERTION(q2 + cbCrStride <= buffer2Limit,
                 "Cr source read must be within bounds");
    NS_ASSERTION(p + cbCrStride < data + mCbCrSize,
                 "Cb write destination must be within bounds");
    NS_ASSERTION(p2 + cbCrStride < data + mCbCrSize,
                 "Cr write destination must be within bounds");
    memcpy(p, q, cbCrStride);
    memcpy(p2, q2, cbCrStride);
    p += cbCrStride;
    p2 += cbCrStride;
    q += aData->mBuffer[1].stride;
    q2 += aData->mBuffer[2].stride;
  }

  ImageContainer* container = mDecoder->GetImageContainer();
  // Currently our Ogg decoder only knows how to output to PLANAR_YCBCR
  // format.
  Image::Format format = Image::PLANAR_YCBCR;
  nsRefPtr<Image> image;
  if (container) {
    image = container->CreateImage(&format, 1);
  }
  if (image) {
    NS_ASSERTION(image->GetFormat() == Image::PLANAR_YCBCR,
                 "Wrong format?");
    PlanarYCbCrImage* videoImage = static_cast<PlanarYCbCrImage*>(image.get());
    // XXX this is only temporary until we get YCbCr code in the layer
    // system.
    videoImage->SetRGBConverter(ToARGBHook);
    PlanarYCbCrImage::Data data;
    data.mYChannel = y;
    data.mYSize = gfxIntSize(mInfo.mPicture.width, mInfo.mPicture.height);
    data.mYStride = mInfo.mPicture.width;
    data.mCbChannel = cb;
    data.mCrChannel = cr;
    data.mCbCrSize = gfxIntSize(cbCrStride, cbCrHeight);
    data.mCbCrStride = cbCrStride;
    videoImage->SetData(data);
    mDecoder->SetVideoData(data.mYSize, mInfo.mAspectRatio, image);
  }
}

PRInt64
nsOggPlayStateMachine::GetAudioClock()
{
  NS_ASSERTION(IsThread(mDecoder->mStateMachineThread), "Should be on state machine thread.");
  if (!mAudioStream || !HasAudio())
    return -1;
  PRInt64 t = mAudioStream->GetPosition();
  return (t == -1) ? -1 : t + mAudioStartTime;
}

void nsOggPlayStateMachine::AdvanceFrame()
{
  NS_ASSERTION(IsThread(mDecoder->mStateMachineThread), "Should be on state machine thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  // When it's time to display a frame, decode the frame and display it.
  if (mDecoder->GetState() == nsOggDecoder::PLAY_STATE_PLAYING) {
    if (!IsPlaying()) {
      StartPlayback();
      mDecoder->GetMonitor().NotifyAll();
    }

    if (HasAudio() && mAudioStartTime == -1 && !mAudioCompleted) {
      // We've got audio (so we should sync off the audio clock), but we've
      // not played a sample on the audio thread, so we can't get a time
      // from the audio clock. Just wait and then return, to give the audio
      // clock time to tick.
      Wait(mInfo.mCallbackPeriod);
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
        mVideoFrameTime = data->mTime;
        videoData = data;
        mReader->mVideoQueue.PopFront();
        if (mReader->mVideoQueue.GetSize() == 0)
          break;
        data = mReader->mVideoQueue.PeekFront();
      }
    }

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
      videoData = nsnull;
    }

    // Cap the current time to the larger of the audio and video end time.
    // This ensures that if we're running off the system clock, we don't
    // advance the clock to after the media end time.
    if (mVideoFrameTime != -1 || mAudioEndTime != -1) {
      // These will be non -1 if we've displayed a video frame, or played an audio sample.
      clock_time = NS_MIN(clock_time, NS_MAX(mVideoFrameTime, mAudioEndTime));
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

    Wait(mInfo.mCallbackPeriod);
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

void nsOggPlayStateMachine::Wait(PRUint32 aMs) {
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
                 "nsOggPlayStateMachine::Wait interval very wrong!");
    mDecoder->GetMonitor().Wait(PR_MillisecondsToInterval(ms));
  }
}

void nsOggPlayStateMachine::LoadOggHeaders()
{
  NS_ASSERTION(IsThread(mDecoder->mStateMachineThread),
               "Should be on state machine thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  LOG(PR_LOG_DEBUG, ("Loading Ogg Headers"));

  nsMediaStream* stream = mDecoder->mStream;

  nsOggInfo info;
  {
    MonitorAutoExit exitMon(mDecoder->GetMonitor());
    mReader->ReadOggHeaders(info);
  }
  mInfo = info;
  mDecoder->StartProgressUpdates();

  if (!mInfo.mHasVideo && !mInfo.mHasAudio) {
    mState = DECODER_STATE_SHUTDOWN;      
    nsCOMPtr<nsIRunnable> event =
      NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, DecodeError);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
    return;
  }

  if (!mInfo.mHasVideo) {
    mInfo.mCallbackPeriod = 1000 / AUDIO_FRAME_RATE;
  }
  LOG(PR_LOG_DEBUG, ("%p Callback Period: %u", mDecoder, mInfo.mCallbackPeriod));

  // TODO: Get the duration from Skeleton index, if available.

  // Get the duration from the Ogg file. We only do this if the
  // content length of the resource is known as we need to seek
  // to the end of the file to get the last time field. We also
  // only do this if the resource is seekable and if we haven't
  // already obtained the duration via an HTTP header.
  mGotDurationFromHeader = (GetDuration() != -1);
  if (mState != DECODER_STATE_SHUTDOWN &&
      stream->GetLength() >= 0 &&
      mSeekable &&
      mEndTime == -1)
  {
    mDecoder->StopProgressUpdates();
    FindEndTime();
    mDecoder->StartProgressUpdates();
    mDecoder->UpdatePlaybackRate();
  }
}

VideoData* nsOggPlayStateMachine::FindStartTime()
{
  NS_ASSERTION(IsThread(mDecoder->mStateMachineThread), "Should be on state machine thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();
  PRInt64 startTime = 0;
  mStartTime = 0;
  VideoData* v = nsnull;
  {
    MonitorAutoExit exitMon(mDecoder->GetMonitor());
    v = mReader->FindStartTime(mInfo.mDataOffset, startTime);
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

void nsOggPlayStateMachine::FindEndTime() 
{
  NS_ASSERTION(OnStateMachineThread(), "Should be on state machine thread.");
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  nsMediaStream* stream = mDecoder->mStream;

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

  NS_ASSERTION(mInfo.mDataOffset > 0,
               "Should have offset of first non-header page");
  {
    MonitorAutoExit exitMon(mDecoder->GetMonitor());
    stream->Seek(nsISeekableStream::NS_SEEK_SET, mInfo.mDataOffset);
  }
  LOG(PR_LOG_DEBUG, ("%p Media end time is %lldms", mDecoder, mEndTime));   
}

void nsOggPlayStateMachine::UpdateReadyState() {
  mDecoder->GetMonitor().AssertCurrentThreadIn();

  nsCOMPtr<nsIRunnable> event;
  switch (GetNextFrameStatus()) {
    case nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE_BUFFERING:
      event = NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, NextFrameUnavailableBuffering);
      break;
    case nsHTMLMediaElement::NEXT_FRAME_AVAILABLE:
      event = NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, NextFrameAvailable);
      break;
    case nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE:
      event = NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, NextFrameUnavailable);
      break;
    default:
      PR_NOT_REACHED("unhandled frame state");
  }

  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}


static PRBool AddOverflow(PRUint32 a, PRUint32 b, PRUint32& aResult) {
  PRUint64 rl = static_cast<PRUint64>(a) + static_cast<PRUint64>(b);
  if (rl > PR_UINT32_MAX) {
    return PR_FALSE;
  }
  aResult = static_cast<PRUint32>(rl);
  return true;
}
