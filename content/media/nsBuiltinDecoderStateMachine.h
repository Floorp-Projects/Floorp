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
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
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
/*
Each video element for a media file has two additional threads beyond
those needed by nsBuiltinDecoder.

  1) The Audio thread writes the decoded audio data to the audio
     hardware.  This is done in a seperate thread to ensure that the
     audio hardware gets a constant stream of data without
     interruption due to decoding or display. At some point
     libsydneyaudio will be refactored to have a callback interface
     where it asks for data and an extra thread will no longer be
     needed.

  2) The decode thread. This thread reads from the media stream and
     decodes the Theora and Vorbis data. It places the decoded data in
     a queue for the other threads to pull from.

All file reads and seeks must occur on either the state machine thread
or the decode thread. Synchronisation is done via a monitor owned by
nsBuiltinDecoder.

The decode thread and the audio thread are created and destroyed in
the state machine thread. When playback needs to occur they are
created and events dispatched to them to start them. These events exit
when decoding is completed or no longer required (during seeking or
shutdown).
    
The decode thread has its own monitor to ensure that its internal
state is independent of the other threads, and to ensure that it's not
hogging the nsBuiltinDecoder monitor while decoding.

a/v synchronisation is handled by the state machine thread. It
examines the audio playback time and compares this to the next frame
in the queue of frames. If it is time to play the video frame it is
then displayed.

Frame skipping is done in the following ways:

  1) The state machine thread will skip all frames in the video queue whose
     display time is less than the current audio time. This ensures
     the correct frame for the current time is always displayed.

  2) The decode thread will stop decoding interframes and read to the
     next keyframe if it determines that decoding the remaining
     interframes will cause playback issues. It detects this by:
       a) If the amount of audio data in the audio queue drops
          below a threshold whereby audio may start to skip.
       b) If the video queue drops below a threshold where it
          will be decoding video data that won't be displayed due
          to the decode thread dropping the frame immediately.

YCbCr conversion is done on the decode thread when it is time to display
the video frame. This means frames that are skipped will not have the
YCbCr conversion done, improving playback.

The decode thread pushes decoded audio and videos frames into two
separate queues - one for audio and one for video. These are kept
separate to make it easy to constantly feed audio data to the sound
hardware while allowing frame skipping of video data. These queues are
threadsafe, and neither the decode, audio, or state machine thread should
be able to monopolize them, and cause starvation of the other threads.

Both queues are bounded by a maximum size. When this size is reached
the decode thread will no longer decode video or audio depending on the
queue that has reached the threshold.

During playback the audio thread will be idle (via a Wait() on the
monitor) if the audio queue is empty. Otherwise it constantly pops an
item off the queue and plays it with a blocking write to the audio
hardware (via nsAudioStream and libsydneyaudio).

The decode thread idles if the video queue is empty or if it is
not yet time to display the next frame.
*/
#if !defined(nsBuiltinDecoderStateMachine_h__)
#define nsBuiltinDecoderStateMachine_h__

#include "prmem.h"
#include "nsThreadUtils.h"
#include "nsBuiltinDecoder.h"
#include "nsBuiltinDecoderReader.h"
#include "nsAudioAvailableEventManager.h"
#include "nsHTMLMediaElement.h"
#include "mozilla/Monitor.h"

/*
  The playback state machine class. This manages the decoding in the
  nsBuiltinDecoderReader on the decode thread, seeking and in-sync-playback on the
  state machine thread, and controls the audio "push" thread.

  All internal state is synchronised via the decoder monitor. NotifyAll
  on the monitor is called when the state of the state machine is changed
  by the main thread. The following changes to state cause a notify:

    mState and data related to that state changed (mSeekTime, etc)
    Metadata Loaded
    First Frame Loaded
    Frame decoded
    data pushed or popped from the video and audio queues

  See nsBuiltinDecoder.h for more details.
*/
class nsBuiltinDecoderStateMachine : public nsDecoderStateMachine
{
public:
  typedef mozilla::Monitor Monitor;
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  nsBuiltinDecoderStateMachine(nsBuiltinDecoder* aDecoder, nsBuiltinDecoderReader* aReader);
  ~nsBuiltinDecoderStateMachine();

  // nsDecoderStateMachine interface
  virtual nsresult Init(nsDecoderStateMachine* aCloneDonor);
  State GetState()
  { 
    mDecoder->GetMonitor().AssertCurrentThreadIn();
    return mState; 
  }
  virtual void SetVolume(double aVolume);
  virtual void Shutdown();
  virtual PRInt64 GetDuration();
  virtual void SetDuration(PRInt64 aDuration);
  virtual PRBool OnDecodeThread() const {
    return IsCurrentThread(mDecodeThread);
  }

  virtual nsHTMLMediaElement::NextFrameStatus GetNextFrameStatus();
  virtual void Play();
  virtual void Seek(double aTime);
  virtual double GetCurrentTime() const;
  virtual void ClearPositionChangeFlag();
  virtual void SetSeekable(PRBool aSeekable);
  virtual void UpdatePlaybackPosition(PRInt64 aTime);
  virtual void StartBuffering();


  // Load metadata Called on the state machine thread. The decoder monitor must be held with
  // exactly one lock count.
  virtual void LoadMetadata();

  // State machine thread run function. Polls the state, sends frames to be
  // displayed at appropriate times, and generally manages the decode.
  NS_IMETHOD Run();

  // This is called on the state machine thread and audio thread.
  // The decoder monitor must be obtained before calling this.
  PRBool HasAudio() const {
    mDecoder->GetMonitor().AssertCurrentThreadIn();
    return mInfo.mHasAudio;
  }

  // This is called on the state machine thread and audio thread.
  // The decoder monitor must be obtained before calling this.
  PRBool HasVideo() const {
    mDecoder->GetMonitor().AssertCurrentThreadIn();
    return mInfo.mHasVideo;
  }

  // Should be called by main thread.
  PRBool HaveNextFrameData() const;

  // Must be called with the decode monitor held.
  PRBool IsBuffering() const {
    mDecoder->GetMonitor().AssertCurrentThreadIn();

    return mState == nsBuiltinDecoderStateMachine::DECODER_STATE_BUFFERING;
  }

  // Must be called with the decode monitor held.
  PRBool IsSeeking() const {
    mDecoder->GetMonitor().AssertCurrentThreadIn();

    return mState == nsBuiltinDecoderStateMachine::DECODER_STATE_SEEKING;
  }

  // Functions used by assertions to ensure we're calling things
  // on the appropriate threads.
  PRBool OnAudioThread() {
    return IsCurrentThread(mAudioThread);
  }

  PRBool OnStateMachineThread() {
    return mDecoder->OnStateMachineThread();
  }

  // Decode loop, called on the decode thread.
  void DecodeLoop();

  // The decoder object that created this state machine. The decoder
  // always outlives us since it controls our lifetime. This is accessed
  // read only on the AV, state machine, audio and main thread.
  nsBuiltinDecoder* mDecoder;

  // The decoder monitor must be obtained before modifying this state.
  // NotifyAll on the monitor must be called when the state is changed by
  // the main thread so the decoder thread can wake up.
  // Accessed on state machine, audio, main, and AV thread. 
  State mState;

  nsresult GetBuffered(nsTimeRanges* aBuffered);

  void NotifyDataArrived(const char* aBuffer, PRUint32 aLength, PRUint32 aOffset) {
    NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
    mReader->NotifyDataArrived(aBuffer, aLength, aOffset);
  }

  PRInt64 GetEndMediaTime() const {
    mDecoder->GetMonitor().AssertCurrentThreadIn();
    return mEndTime;
  }

protected:

  // Returns PR_TRUE if we'v got less than aAudioMs ms of decoded and playable
  // data. The decoder monitor must be held.
  PRBool HasLowDecodedData(PRInt64 aAudioMs) const;

  // Returns PR_TRUE if we're running low on data which is not yet decoded.
  // The decoder monitor must be held.
  PRBool HasLowUndecodedData() const;

  // Returns the number of milliseconds of undecoded data available for
  // decoding. The decoder monitor must be held.
  PRInt64 GetUndecodedData() const;

  // Returns the number of unplayed ms of audio we've got decoded and/or
  // pushed to the hardware waiting to play. This is how much audio we can
  // play without having to run the audio decoder. The decoder monitor
  // must be held.
  PRInt64 AudioDecodedMs() const;

  // Returns PR_TRUE when there's decoded audio waiting to play.
  // The decoder monitor must be held.
  PRBool HasFutureAudio() const;

  // Returns PR_TRUE if we recently exited "quick buffering" mode.
  PRBool JustExitedQuickBuffering();

  // Waits on the decoder Monitor for aMs. If the decoder monitor is awoken
  // by a Notify() call, we'll continue waiting, unless we've moved into
  // shutdown state. This enables us to ensure that we wait for a specified
  // time, and that the myriad of Notify()s we do an the decoder monitor
  // don't cause the audio thread to be starved. The decoder monitor must
  // be locked.
  void Wait(PRInt64 aMs);

  // Dispatches an asynchronous event to update the media element's ready state.
  void UpdateReadyState();

  // Resets playback timing data. Called when we seek, on the state machine
  // thread.
  void ResetPlayback();

  // Returns the audio clock, if we have audio, or -1 if we don't.
  // Called on the state machine thread.
  PRInt64 GetAudioClock();

  // Returns the presentation time of the first sample or frame in the media.
  // If the media has video, it returns the first video frame. The decoder
  // monitor must be held with exactly one lock count. Called on the state
  // machine thread.
  VideoData* FindStartTime();

  // Finds the end time of the last frame of data in the file, storing the value
  // in mEndTime if successful. The decoder must be held with exactly one lock
  // count. Called on the state machine thread.
  void FindEndTime();

  // Update only the state machine's current playback position (and duration,
  // if unknown).  Does not update the playback position on the decoder or
  // media element -- use UpdatePlaybackPosition for that.  Called on the state
  // machine thread, caller must hold the decoder lock.
  void UpdatePlaybackPositionInternal(PRInt64 aTime);

  // Performs YCbCr to RGB conversion, and pushes the image down the
  // rendering pipeline. Called on the state machine thread. The decoder
  // monitor must not be held when calling this.
  void RenderVideoFrame(VideoData* aData, TimeStamp aTarget, 
                        nsIntSize aDisplaySize, float aAspectRatio);
 
  // If we have video, display a video frame if it's time for display has
  // arrived, otherwise sleep until it's time for the next sample. Update
  // the current frame time as appropriate, and trigger ready state update.
  // The decoder monitor must be held with exactly one lock count. Called
  // on the state machine thread.
  void AdvanceFrame();

  // Pushes up to aSamples samples of silence onto the audio hardware. Returns
  // the number of samples acutally pushed to the hardware. This pushes up to
  // 32KB worth of samples to the hardware before returning, so must be called
  // in a loop to ensure that the desired number of samples are pushed to the
  // hardware. This ensures that the playback position advances smoothly, and
  // guarantees that we don't try to allocate an impossibly large chunk of
  // memory in order to play back silence. Called on the audio thread.
  PRUint32 PlaySilence(PRUint32 aSamples, PRUint32 aChannels,
                       PRUint64 aSampleOffset);

  // Pops an audio chunk from the front of the audio queue, and pushes its
  // sound data to the audio hardware. MozAudioAvailable sample data is also
  // queued here. Called on the audio thread.
  PRUint32 PlayFromAudioQueue(PRUint64 aSampleOffset, PRUint32 aChannels);

  // Stops the decode threads. The decoder monitor must be held with exactly
  // one lock count. Called on the state machine thread.
  void StopDecodeThreads();

  // Starts the decode threads. The decoder monitor must be held with exactly
  // one lock count. Called on the state machine thread.
  nsresult StartDecodeThreads();

  // The main loop for the audio thread. Sent to the thread as
  // an nsRunnableMethod. This continually does blocking writes to
  // to audio stream to play audio data.
  void AudioLoop();

  // Stop or pause playback of media. This has two modes, denoted by
  // aMode being either AUDIO_PAUSE or AUDIO_SHUTDOWN.
  //
  // AUDIO_PAUSE: Suspends the audio stream to be resumed later.
  // This does not close the OS based audio stream 
  //
  // AUDIO_SHUTDOWN: Closes and destroys the audio stream and
  // releases any OS resources.
  //
  // The decoder monitor must be held with exactly one lock count. Called
  // on the state machine thread.
  enum eStopMode {AUDIO_PAUSE, AUDIO_SHUTDOWN};
  void StopPlayback(eStopMode aMode);

  // Resume playback of media. Must be called with the decode monitor held.
  // This resumes a paused audio stream. The decoder monitor must be held with
  // exactly one lock count. Called on the state machine thread.
  void StartPlayback();

  // Moves the decoder into decoding state. Called on the state machine
  // thread. The decoder monitor must be held.
  void StartDecoding();

  // Returns PR_TRUE if we're currently playing. The decoder monitor must
  // be held.
  PRBool IsPlaying();

  // Returns the "media time". This is the absolute time which the media
  // playback has reached. i.e. this returns values in the range
  // [mStartTime, mEndTime], and mStartTime will not be 0 if the media does
  // not start at 0. Note this is different to the value returned
  // by GetCurrentTime(), which is in the range [0,duration].
  PRInt64 GetMediaTime() const {
    mDecoder->GetMonitor().AssertCurrentThreadIn();
    return mStartTime + mCurrentFrameTime;
  }

  // Returns an upper bound on the number of milliseconds of audio that is
  // decoded and playable. This is the sum of the number of ms of audio which
  // is decoded and in the reader's audio queue, and the ms of unplayed audio
  // which has been pushed to the audio hardware for playback. Note that after
  // calling this, the audio hardware may play some of the audio pushed to
  // hardware, so this can only be used as a upper bound. The decoder monitor
  // must be held when calling this. Called on the decoder thread.
  PRInt64 GetDecodedAudioDuration();

  // Monitor on mAudioStream. This monitor must be held in order to delete
  // or use the audio stream. This stops us destroying the audio stream
  // while it's being used on another thread (typically when it's being
  // written to on the audio thread).
  Monitor mAudioMonitor;

  // The size of the decoded YCbCr frame.
  // Accessed on state machine thread.
  PRUint32 mCbCrSize;

  // Accessed on state machine thread.
  nsAutoArrayPtr<unsigned char> mCbCrBuffer;

  // Thread for pushing audio onto the audio hardware.
  // The "audio push thread".
  nsCOMPtr<nsIThread> mAudioThread;

  // Thread for decoding video in background. The "decode thread".
  nsCOMPtr<nsIThread> mDecodeThread;

  // The time that playback started from the system clock. This is used for
  // timing the presentation of video frames when there's no audio.
  // Accessed only via the state machine thread.
  TimeStamp mPlayStartTime;

  // The amount of time we've spent playing already the media. The current
  // playback position is therefore |Now() - mPlayStartTime +
  // mPlayDuration|, which must be adjusted by mStartTime if used with media
  // timestamps.  Accessed only via the state machine thread.
  TimeDuration mPlayDuration;

  // Time that buffering started. Used for buffering timeout and only
  // accessed on the state machine thread. This is null while we're not
  // buffering.
  TimeStamp mBufferingStart;

  // Start time of the media, in milliseconds. This is the presentation
  // time of the first sample decoded from the media, and is used to calculate
  // duration and as a bounds for seeking. Accessed on state machine and
  // main thread. Access controlled by decoder monitor.
  PRInt64 mStartTime;

  // Time of the last page in the media, in milliseconds. This is the
  // end time of the last sample in the media. Accessed on state
  // machine and main thread. Access controlled by decoder monitor.
  PRInt64 mEndTime;

  // Position to seek to in milliseconds when the seek state transition occurs.
  // The decoder monitor lock must be obtained before reading or writing
  // this value. Accessed on main and state machine thread.
  PRInt64 mSeekTime;

  // The audio stream resource. Used on the state machine, audio, and main
  // threads. You must hold the mAudioMonitor, and must NOT hold the decoder
  // monitor when using the audio stream!
  nsRefPtr<nsAudioStream> mAudioStream;

  // The reader, don't call its methods with the decoder monitor held.
  // This is created in the play state machine's constructor, and destroyed
  // in the play state machine's destructor.
  nsAutoPtr<nsBuiltinDecoderReader> mReader;

  // The time of the current frame in milliseconds. This is referenced from
  // 0 which is the initial playback position. Set by the state machine
  // thread, and read-only from the main thread to get the current
  // time value. Synchronised via decoder monitor.
  PRInt64 mCurrentFrameTime;

  // The presentation time of the first audio sample that was played. We can
  // add this to the audio stream position to determine the current audio time.
  // Accessed on audio and state machine thread. Synchronized by decoder monitor.
  PRInt64 mAudioStartTime;

  // The end time of the last audio sample that's been pushed onto the audio
  // hardware. This will approximately be the end time of the audio stream,
  // unless another sample is pushed to the hardware.
  PRInt64 mAudioEndTime;

  // The presentation end time of the last video frame which has been displayed.
  // Accessed from the state machine thread.
  PRInt64 mVideoFrameEndTime;
  
  // Volume of playback. 0.0 = muted. 1.0 = full volume. Read/Written
  // from the state machine and main threads. Synchronised via decoder
  // monitor.
  double mVolume;

  // Time at which we started decoding. Synchronised via decoder monitor.
  TimeStamp mDecodeStartTime;

  // PR_TRUE if the media resource can be seeked. Accessed from the state
  // machine and main threads. Synchronised via decoder monitor.
  PRPackedBool mSeekable;

  // PR_TRUE if an event to notify about a change in the playback
  // position has been queued, but not yet run. It is set to PR_FALSE when
  // the event is run. This allows coalescing of these events as they can be
  // produced many times per second. Synchronised via decoder monitor.
  // Accessed on main and state machine threads.
  PRPackedBool mPositionChangeQueued;

  // PR_TRUE if the audio playback thread has finished. It is finished
  // when either all the audio samples in the Vorbis bitstream have completed
  // playing, or we've moved into shutdown state, and the threads are to be
  // destroyed. Written by the audio playback thread and read and written by
  // the state machine thread. Synchronised via decoder monitor.
  PRPackedBool mAudioCompleted;

  // PR_TRUE if mDuration has a value obtained from an HTTP header, or from
  // the media index/metadata. Accessed on the state machine thread.
  PRPackedBool mGotDurationFromMetaData;
    
  // PR_FALSE while decode threads should be running. Accessed on audio, 
  // state machine and decode threads. Syncrhonised by decoder monitor.
  PRPackedBool mStopDecodeThreads;

  // If this is PR_TRUE while we're in buffering mode, we can exit early,
  // as it's likely we may be able to playback. This happens when we enter
  // buffering mode soon after the decode starts, because the decode-ahead
  // ran fast enough to exhaust all data while the download is starting up.
  // Synchronised via decoder monitor.
  PRPackedBool mQuickBuffering;

private:
  // Manager for queuing and dispatching MozAudioAvailable events.  The
  // event manager is accessed from the state machine and audio threads,
  // and takes care of synchronizing access to its internal queue.
  nsAudioAvailableEventManager mEventManager;

  // Stores presentation info required for playback. The decoder monitor
  // must be held when accessing this.
  nsVideoInfo mInfo;
};

#endif
