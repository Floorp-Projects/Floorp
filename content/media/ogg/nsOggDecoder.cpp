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
#include "prlog.h"
#include "prmem.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsThreadUtils.h"
#include "nsIDOMHTMLMediaElement.h"
#include "nsNetUtil.h"
#include "nsAudioStream.h"
#include "nsChannelReader.h"
#include "nsHTMLVideoElement.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsAutoLock.h"
#include "nsTArray.h"
#include "nsNetUtil.h"
#include "nsOggDecoder.h"

using mozilla::TimeDuration;
using mozilla::TimeStamp;

/* 
   The maximum height and width of the video. Used for
   sanitizing the memory allocation of the RGB buffer
*/
#define MAX_VIDEO_WIDTH  2000
#define MAX_VIDEO_HEIGHT 2000

// The number of entries in oggplay buffer list. This value
// is the one used by the oggplay examples.
#define OGGPLAY_BUFFER_SIZE 20

// The number of frames to read before audio callback is called.
// This value is the one used by the oggplay examples.
#define OGGPLAY_FRAMES_PER_CALLBACK 2048

// Offset into Ogg buffer containing audio information. This value
// is the one used by the oggplay examples.
#define OGGPLAY_AUDIO_OFFSET 250L

// Wait this number of seconds when buffering, then leave and play
// as best as we can if the required amount of data hasn't been
// retrieved.
#define BUFFERING_WAIT 15

// The amount of data to retrieve during buffering is computed based
// on the download rate. BUFFERING_MIN_RATE is the minimum download
// rate to be used in that calculation to help avoid constant buffering
// attempts at a time when the average download rate has not stabilised.
#define BUFFERING_MIN_RATE 50000
#define BUFFERING_RATE(x) ((x)< BUFFERING_MIN_RATE ? BUFFERING_MIN_RATE : (x))

// The number of seconds of buffer data before buffering happens
// based on current playback rate.
#define BUFFERING_SECONDS_LOW_WATER_MARK 1

// The minimum size buffered byte range inside which we'll consider
// trying a bounded-seek. When we seek, we first try to seek inside all
// buffered ranges larger than this, and if they all fail we fall back to
// an unbounded seek over the whole media. 64K is approximately 16 pages.
#define MIN_BOUNDED_SEEK_SIZE (64 * 1024)

class nsOggStepDecodeEvent;

/* 
  All reading (including seeks) from the nsMediaStream are done on the
  decoding thread. The decoder thread is informed before closing that
  the stream is about to close via the Shutdown
  event. oggplay_prepare_for_close is called before sending the
  shutdown event to tell liboggplay to shutdown.

  This call results in oggplay internally not calling any
  read/write/seek/tell methods, and returns a value that results in
  stopping the decoder thread.

  oggplay_close is called in the destructor which results in the media
  stream being closed. This is how the nsMediaStream contract that no
  read/seeking must occur during or after Close is called is enforced.

  This object keeps pointers to the nsOggDecoder and nsChannelReader
  objects.  Since the lifetime of nsOggDecodeStateMachine is
  controlled by nsOggDecoder it will never have a stale reference to
  these objects. The reader is destroyed by the call to oggplay_close
  which is done in the destructor so again this will never be a stale
  reference.

  All internal state is synchronised via the decoder monitor. NotifyAll
  on the monitor is called when the state of the state machine is changed
  by the main thread. The following changes to state cause a notify:

    mState and data related to that state changed (mSeekTime, etc)
    Ogg Metadata Loaded
    First Frame Loaded  
    Frame decoded    
    
  See nsOggDecoder.h for more details.
*/
class nsOggDecodeStateMachine : public nsRunnable
{
  friend class nsOggStepDecodeEvent;
public:
  // Object to hold the decoded data from a frame
  class FrameData {
  public:
    FrameData() :
      mVideoHeader(nsnull),
      mVideoWidth(0),
      mVideoHeight(0),
      mUVWidth(0),
      mUVHeight(0),
      mDecodedFrameTime(0.0),
      mTime(0.0)
    {
      MOZ_COUNT_CTOR(FrameData);
    }

    ~FrameData()
    {
      MOZ_COUNT_DTOR(FrameData);
      ClearVideoHeader();
    }

    void ClearVideoHeader() {
      if (mVideoHeader) {
        oggplay_callback_info_unlock_item(mVideoHeader);
        mVideoHeader = nsnull;
      }
    }

    // Write the audio data from the frame to the Audio stream.
    void Write(nsAudioStream* aStream)
    {
      aStream->Write(mAudioData.Elements(), mAudioData.Length());
      mAudioData.Clear(); 
    }

    void SetVideoHeader(OggPlayDataHeader* aVideoHeader)
    {
      NS_ABORT_IF_FALSE(!mVideoHeader, "Frame already owns a video header");
      mVideoHeader = aVideoHeader;
      oggplay_callback_info_lock_item(mVideoHeader);
    }

    // The position in the stream where this frame ended, in bytes
    PRInt64 mEndStreamPosition;
    OggPlayDataHeader* mVideoHeader;
    nsTArray<float> mAudioData;
    int mVideoWidth;
    int mVideoHeight;
    int mUVWidth;
    int mUVHeight;
    float mDecodedFrameTime;
    float mTime;
    OggPlayStreamInfo mState;
  };

  // A queue of decoded video frames. 
  class FrameQueue
  {
  public:
    FrameQueue() :
      mHead(0),
      mTail(0),
      mCount(0)
    {
    }

    void Push(FrameData* frame)
    {
      NS_ASSERTION(!IsFull(), "FrameQueue is full");
      mQueue[mTail] = frame;
      mTail = (mTail+1) % OGGPLAY_BUFFER_SIZE;
      ++mCount;
    }

    FrameData* Peek() const
    {
      NS_ASSERTION(mCount > 0, "FrameQueue is empty");

      return mQueue[mHead];
    }

    FrameData* Pop()
    {
      NS_ASSERTION(mCount, "FrameQueue is empty");

      FrameData* result = mQueue[mHead];
      mHead = (mHead + 1) % OGGPLAY_BUFFER_SIZE;
      --mCount;
      return result;
    }

    PRBool IsEmpty() const
    {
      return mCount == 0;
    }

    PRUint32 GetCount() const
    {
      return mCount;
    }

    PRBool IsFull() const
    {
      return mCount == OGGPLAY_BUFFER_SIZE;
    }

    PRUint32 ResetTimes(float aPeriod)
    {
      PRUint32 frames = 0;
      if (mCount > 0) {
        PRUint32 current = mHead;
        do {
          mQueue[current]->mTime = frames * aPeriod;
          frames += 1;
          current = (current + 1) % OGGPLAY_BUFFER_SIZE;
        } while (current != mTail);
      }
      return frames;
    }

  private:
    FrameData* mQueue[OGGPLAY_BUFFER_SIZE];
    PRUint32 mHead;
    PRUint32 mTail;
    // This isn't redundant with mHead/mTail, since when mHead == mTail
    // it's ambiguous whether the queue is full or empty
    PRUint32 mCount;
  };

  // Enumeration for the valid states
  enum State {
    DECODER_STATE_DECODING_METADATA,
    DECODER_STATE_DECODING,
    DECODER_STATE_SEEKING,
    DECODER_STATE_BUFFERING,
    DECODER_STATE_COMPLETED,
    DECODER_STATE_SHUTDOWN
  };

  nsOggDecodeStateMachine(nsOggDecoder* aDecoder);
  ~nsOggDecodeStateMachine();

  // Cause state transitions. These methods obtain the decoder monitor
  // to synchronise the change of state, and to notify other threads
  // that the state has changed.
  void Shutdown();
  void Decode();
  void Seek(float aTime);
  void StopStepDecodeThread(nsAutoMonitor* aMonitor);

  NS_IMETHOD Run();

  PRBool HasAudio()
  {
    NS_ASSERTION(mState > DECODER_STATE_DECODING_METADATA, "HasAudio() called during invalid state");
    //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "HasAudio() called without acquiring decoder monitor");
    return mAudioTrack != -1;
  }

  // Decode one frame of data, returning the OggPlay error code. Must
  // be called only when the current state > DECODING_METADATA. The decode 
  // monitor MUST NOT be locked during this call since it can take a long
  // time. liboggplay internally handles locking.
  // Any return value apart from those below is mean decoding cannot continue.
  // E_OGGPLAY_CONTINUE       = One frame decoded and put in buffer list
  // E_OGGPLAY_USER_INTERRUPT = One frame decoded, buffer list is now full
  // E_OGGPLAY_TIMEOUT        = No frames decoded, timed out
  OggPlayErrorCode DecodeFrame();

  // Handle any errors returned by liboggplay when decoding a frame.
  // Since this function can change the decoding state it must be called
  // with the decoder lock held.
  void HandleDecodeErrors(OggPlayErrorCode r);

  // Returns the next decoded frame of data. The caller is responsible
  // for freeing the memory returned. This function must be called
  // only when the current state > DECODING_METADATA. The decode
  // monitor lock does not need to be locked during this call since
  // liboggplay internally handles locking.
  FrameData* NextFrame();

  // Play a frame of decoded video. The decode monitor is obtained
  // internally by this method for synchronisation.
  void PlayFrame();

  // Play the video data from the given frame. The decode monitor
  // must be locked when calling this method.
  void PlayVideo(FrameData* aFrame);

  // Plays the audio for the frame, plus any outstanding audio data
  // buffered by nsAudioStream and not yet written to the
  // hardware. The audio data for the frame is cleared out so
  // subsequent calls with the same frame do not re-write the data.
  // The decode monitor must be locked when calling this method.
  void PlayAudio(FrameData* aFrame);

  // Called from the main thread to get the current frame time. The decoder
  // monitor must be obtained before calling this.
  float GetCurrentTime();

  // Called from the main thread to get the duration. The decoder monitor
  // must be obtained before calling this. It is in units of milliseconds.
  PRInt64 GetDuration();

  // Called from the main thread to set the duration of the media resource
  // if it is able to be obtained via HTTP headers. The decoder monitor
  // must be obtained before calling this.
  void SetDuration(PRInt64 aDuration);

  // Called from the main thread to set whether the media resource can
  // be seeked. The decoder monitor must be obtained before calling this.
  void SetSeekable(PRBool aSeekable);

  // Set the audio volume. The decoder monitor must be obtained before
  // calling this.
  void SetVolume(float aVolume);

  // Clear the flag indicating that a playback position change event
  // is currently queued. This is called from the main thread and must
  // be called with the decode monitor held.
  void ClearPositionChangeFlag();

  // Must be called with the decode monitor held. Can be called by main
  // thread.
  PRBool HaveNextFrameData() const {
    return !mDecodedFrames.IsEmpty() &&
      (mDecodedFrames.Peek()->mDecodedFrameTime > mCurrentFrameTime ||
       mDecodedFrames.GetCount() > 1);
  }

  // Must be called with the decode monitor held. Can be called by main
  // thread.
  PRBool IsBuffering() const {
    PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mDecoder->GetMonitor());
    return mState == nsOggDecodeStateMachine::DECODER_STATE_BUFFERING;
  }

  // Must be called with the decode monitor held. Can be called by main
  // thread.
  PRBool IsSeeking() const {
    PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mDecoder->GetMonitor());
    return mState == nsOggDecodeStateMachine::DECODER_STATE_SEEKING;
  }

protected:

  // Decodes from the current position until encountering a frame with time
  // greater or equal to aSeekTime.
  void DecodeToFrame(nsAutoMonitor& aMonitor,
                     float aSeekTime);

  // Convert the OggPlay frame information into a format used by Gecko
  // (RGB for video, float for sound, etc).The decoder monitor must be
  // acquired in the scope of calls to these functions. They must be
  // called only when the current state > DECODING_METADATA.
  void HandleVideoData(FrameData* aFrame, int aTrackNum, OggPlayDataHeader* aVideoHeader);
  void HandleAudioData(FrameData* aFrame, OggPlayAudioData* aAudioData, int aSize);

  // These methods can only be called on the decoding thread.
  void LoadOggHeaders(nsChannelReader* aReader);

  // Initializes and opens the audio stream. Called from the decode
  // thread only. Must be called with the decode monitor held.
  void OpenAudioStream();

  // Closes and releases resources used by the audio stream. Called
  // from the decode thread only. Must be called with the decode
  // monitor held.
  void CloseAudioStream();

  // Start playback of audio, either by opening or resuming the audio
  // stream. Must be called with the decode monitor held.
  void StartAudio();

  // Stop playback of audio, either by closing or pausing the audio
  // stream. Must be called with the decode monitor held.
  void StopAudio();

  // Start playback of media. Must be called with the decode monitor held.
  // This opens or re-opens the audio stream for playback to start.
  void StartPlayback();

  // Stop playback of media. Must be called with the decode monitor held.
  // This actually closes the audio stream and releases any OS resources.
  void StopPlayback();

  // Pause playback of media. Must be called with the decode monitor held.
  // This does not close the OS based audio stream - it suspends it to be
  // resumed later.
  void PausePlayback();

  // Resume playback of media. Must be called with the decode monitor held.
  // This resumes a paused audio stream.
  void ResumePlayback();

  // Update the playback position. This can result in a timeupdate event
  // and an invalidate of the frame being dispatched asynchronously if
  // there is no such event currently queued.
  // Only called on the decoder thread. Must be called with
  // the decode monitor held.
  void UpdatePlaybackPosition(float aTime);

  // Takes decoded frames from liboggplay's internal buffer and
  // places them in our frame queue. Must be called with the decode
  // monitor held.
  void QueueDecodedFrames();

  // Seeks the OggPlay to aTime, inside buffered byte ranges in aReader's
  // media stream.
  nsresult Seek(float aTime, nsChannelReader* aReader);

  // Sets the current video and audio track to active in liboggplay.
  // Called from the decoder thread only.
  void SetTracksActive();

private:
  // *****
  // The follow fields are only accessed by the decoder thread
  // *****

  // The decoder object that created this state machine. The decoder
  // always outlives us since it controls our lifetime.
  nsOggDecoder* mDecoder;

  // The OggPlay handle. Synchronisation of calls to oggplay functions
  // are handled by liboggplay. We control the lifetime of this
  // object, destroying it in our destructor.
  OggPlay* mPlayer;

  // Frame data containing decoded video/audio for the frame the
  // current frame and the previous frame. Always accessed with monitor
  // held. Written only via the decoder thread, but can be tested on
  // main thread via HaveNextFrameData.
  FrameQueue mDecodedFrames;

  // The time that playback started from the system clock. This is used
  // for synchronising frames.  It is reset after a seek as the mTime member
  // of FrameData is reset to start at 0 from the first frame after a seek.
  // Accessed only via the decoder thread.
  TimeStamp mPlayStartTime;

  // The time that playback was most recently paused, either via
  // buffering or pause. This is used to compute mPauseDuration for
  // a/v sync adjustments.  Accessed only via the decoder thread.
  TimeStamp mPauseStartTime;

  // The total time that has been spent in completed pauses (via
  // 'pause' or buffering). This is used to adjust for these
  // pauses when computing a/v synchronisation. Accessed only via the
  // decoder thread.
  TimeDuration mPauseDuration;

  // PR_TRUE if the media is playing and the decoder has started
  // the sound and adjusted the sync time for pauses. PR_FALSE
  // if the media is paused and the decoder has stopped the sound
  // and adjusted the sync time for pauses. Accessed only via the
  // decoder thread.
  PRPackedBool mPlaying;

  // Number of seconds of data video/audio data held in a frame.
  // Accessed only via the decoder thread.
  double mCallbackPeriod;

  // Video data. These are initially set when the metadata is loaded.
  // They are only accessed from the decoder thread.
  PRInt32 mVideoTrack;
  float   mFramerate;
  float   mAspectRatio;

  // Audio data. These are initially set when the metadata is loaded.
  // They are only accessed from the decoder thread.
  PRInt32 mAudioRate;
  PRInt32 mAudioChannels;
  PRInt32 mAudioTrack;

  // Time that buffering started. Used for buffering timeout and only
  // accessed in the decoder thread.
  TimeStamp mBufferingStart;

  // Download position where we should stop buffering. Only
  // accessed in the decoder thread.
  PRInt64 mBufferingEndOffset;

  // The last decoded video frame. Used for computing the sleep period
  // between frames for a/v sync.  Read/Write from the decode thread only.
  PRUint64 mLastFrame;

  // The decoder position of the end of the last decoded video frame.
  // Read/Write from the decode thread only.
  PRInt64 mLastFramePosition;

  // Thread that steps through decoding each frame using liboggplay. Only accessed
  // via the decode thread.
  nsCOMPtr<nsIThread> mStepDecodeThread;

  // *****
  // The follow fields are accessed by the decoder thread or
  // the main thread.
  // *****

  // The decoder monitor must be obtained before modifying this state.
  // NotifyAll on the monitor must be called when the state is changed by
  // the main thread so the decoder thread can wake up.
  State mState;

  // Position to seek to when the seek state transition occurs. The
  // decoder monitor lock must be obtained before reading or writing
  // this value.
  float mSeekTime;

  // The audio stream resource. Used on the decode thread and the
  // main thread (Via the SetVolume call). Synchronisation via
  // mDecoder monitor.
  nsAutoPtr<nsAudioStream> mAudioStream;

  // The time of the current frame in seconds. This is referenced from
  // 0.0 which is the initial start of the stream. Set by the decode
  // thread, and read-only from the main thread to get the current
  // time value. Synchronised via decoder monitor.
  float mCurrentFrameTime;

  // The presentation times of the first frame that was decoded. This is 
  // the start time of the frame. This is subtracted from each frames'
  // timestamp, so that playback appears to start at time 0 and end at
  // time mDuration. Read/Written from the decode thread, read from the 
  // main thread. Synchronised via decoder monitor.
  float mPlaybackStartTime;

  // Volume of playback. 0.0 = muted. 1.0 = full volume. Read/Written
  // from the decode and main threads. Synchronised via decoder
  // monitor.
  float mVolume;

  // Duration of the media resource. It is accessed from the decoder and main
  // threads. Synchronised via decoder monitor. It is in units of
  // milliseconds.
  PRInt64 mDuration;

  // PR_TRUE if the media resource can be seeked. Accessed from the decoder
  // and main threads. Synchronised via decoder monitor.
  PRPackedBool mSeekable;

  // PR_TRUE if an event to notify about a change in the playback
  // position has been queued, but not yet run. It is set to PR_FALSE when
  // the event is run. This allows coalescing of these events as they can be
  // produced many times per second. Synchronised via decoder monitor.
  PRPackedBool mPositionChangeQueued;

  // PR_TRUE if the step decode loop thread has finished decoding. It is
  // written by the step decode thread and read and written by the state
  // machine thread (but only written by the state machine thread while
  // the step decode thread is not running).
  // Synchronised via decoder monitor.
  PRPackedBool mDecodingCompleted;

  // PR_TRUE if the step decode loop thread should exit now. It is
  // written by the state machine thread and read by the step decode thread.
  // Synchronised via decoder monitor.
  PRPackedBool mExitStepDecodeThread;

  // PR_TRUE if the step decode loop has indicated that we need to buffer.
  // Accessed by the step decode thread and the decode state machine thread.
  // Synchronised via the decoder monitor.
  PRPackedBool mBufferExhausted;

  // PR_TRUE if mDuration has a value obtained from an HTTP header.
  // Read/Written from the decode and main threads. Synchronised via the
  // decoder monitor.
  PRPackedBool mGotDurationFromHeader;
};

// Event that gets posted to the thread that is responsible for decoding
// Ogg frames. Decodes each frame of an Ogg file. Locking of liboggplay 
// is managed by liboggplay. The thread is created when the frames first
// need to be decoded and is shutdown when decoding is not needed (either
// completed, or seeking).
class nsOggStepDecodeEvent : public nsRunnable {
private:
  // Since the lifetime of this event loop is controlled by the
  // decode state machine object, it is safe to keep an 
  // unreferenced counted pointer to it, so we can inform 
  // it when we've finished decoding.
  nsOggDecodeStateMachine* mDecodeStateMachine;

  // The lifetime of this player is managed by the decode state
  // machine thread. This event is created and destroyed before
  // the mPlayer object itself is deleted. 
  OggPlay* mPlayer;

public:
  nsOggStepDecodeEvent(nsOggDecodeStateMachine* machine, OggPlay* player) : 
    mDecodeStateMachine(machine), mPlayer(player) {}
  
  // Return true if we are in a state where the decoder should not be running.
  PRBool InStopDecodingState() {
    PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mDecodeStateMachine->mDecoder->GetMonitor());
    return 
      mDecodeStateMachine->mState != nsOggDecodeStateMachine::DECODER_STATE_DECODING &&
      mDecodeStateMachine->mState != nsOggDecodeStateMachine::DECODER_STATE_BUFFERING;
  }
  
  // This method will block on oggplay_step_decoding when oggplay's
  // internal buffers are full. It is unblocked by the decode
  // state machine thread via a call to oggplay_prepare_for_close
  // during the shutdown protocol. It is unblocked during seeking
  // by release frames from liboggplay's frame queue.
  NS_IMETHOD Run() {
    OggPlayErrorCode r = E_OGGPLAY_TIMEOUT;
    nsAutoMonitor mon(mDecodeStateMachine->mDecoder->GetMonitor());
    nsOggDecoder* decoder = mDecodeStateMachine->mDecoder;
    NS_ASSERTION(!mDecodeStateMachine->mDecodingCompleted,
                 "State machine should have cleared this flag");

    while (!mDecodeStateMachine->mExitStepDecodeThread &&
           !InStopDecodingState() &&
           (r == E_OGGPLAY_TIMEOUT ||
            r == E_OGGPLAY_USER_INTERRUPT ||
            r == E_OGGPLAY_CONTINUE)) {
      if (mDecodeStateMachine->mBufferExhausted) {
        mon.Wait();
      } else {
        // decoder and decoder->mReader are never null here because
        // they are non-null through the lifetime of the state machine
        // thread, which includes the lifetime of this thread.
        PRInt64 initialDownloadPosition =
          decoder->mReader->Stream()->GetCachedDataEnd(decoder->mDecoderPosition);

        mon.Exit();
        r = oggplay_step_decoding(mPlayer);
        mon.Enter();

        mDecodeStateMachine->HandleDecodeErrors(r);

        // Check whether decoding the last frame required us to read data
        // that wasn't available at the start of the frame. That means
        // we should probably start buffering.
        if (decoder->mDecoderPosition > initialDownloadPosition) {
          mDecodeStateMachine->mBufferExhausted = PR_TRUE;
        }

        // If PlayFrame is waiting, wake it up so we can run the
        // decoder loop and move frames from the oggplay queue to our
        // queue. Also needed to wake up the decoder loop that waits
        // for a frame to be ready to display.
        mon.NotifyAll();
      }
    }

    mDecodeStateMachine->mDecodingCompleted = PR_TRUE;
    return NS_OK;
  }
};

nsOggDecodeStateMachine::nsOggDecodeStateMachine(nsOggDecoder* aDecoder) :
  mDecoder(aDecoder),
  mPlayer(0),
  mPlayStartTime(),
  mPauseStartTime(),
  mPauseDuration(0),
  mPlaying(PR_FALSE),
  mCallbackPeriod(1.0),
  mVideoTrack(-1),
  mFramerate(0.0),
  mAspectRatio(1.0),
  mAudioRate(0),
  mAudioChannels(0),
  mAudioTrack(-1),
  mBufferingStart(),
  mBufferingEndOffset(0),
  mLastFrame(0),
  mLastFramePosition(-1),
  mState(DECODER_STATE_DECODING_METADATA),
  mSeekTime(0.0),
  mCurrentFrameTime(0.0),
  mPlaybackStartTime(0.0), 
  mVolume(1.0),
  mDuration(-1),
  mSeekable(PR_TRUE),
  mPositionChangeQueued(PR_FALSE),
  mDecodingCompleted(PR_FALSE),
  mExitStepDecodeThread(PR_FALSE),
  mBufferExhausted(PR_FALSE),
  mGotDurationFromHeader(PR_FALSE)
{
}

nsOggDecodeStateMachine::~nsOggDecodeStateMachine()
{
  while (!mDecodedFrames.IsEmpty()) {
    delete mDecodedFrames.Pop();
  }
  oggplay_close(mPlayer);
}

OggPlayErrorCode nsOggDecodeStateMachine::DecodeFrame()
{
  OggPlayErrorCode r = oggplay_step_decoding(mPlayer);
  return r;
}

void nsOggDecodeStateMachine::HandleDecodeErrors(OggPlayErrorCode aErrorCode)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mDecoder->GetMonitor());

  if (aErrorCode != E_OGGPLAY_TIMEOUT &&
      aErrorCode != E_OGGPLAY_OK &&
      aErrorCode != E_OGGPLAY_USER_INTERRUPT &&
      aErrorCode != E_OGGPLAY_CONTINUE) {
    mState = DECODER_STATE_SHUTDOWN;
    nsCOMPtr<nsIRunnable> event =
      NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, NetworkError);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }
}

nsOggDecodeStateMachine::FrameData* nsOggDecodeStateMachine::NextFrame()
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mDecoder->GetMonitor());
  OggPlayCallbackInfo** info = oggplay_buffer_retrieve_next(mPlayer);
  if (!info)
    return nsnull;

  FrameData* frame = new FrameData();
  if (!frame) {
    return nsnull;
  }

  frame->mTime = mCallbackPeriod * mLastFrame;
  frame->mEndStreamPosition = mDecoder->mDecoderPosition;
  mLastFrame += 1;

  if (mLastFramePosition >= 0) {
    NS_ASSERTION(frame->mEndStreamPosition >= mLastFramePosition,
                 "Playback positions must not decrease without an intervening reset");
    TimeStamp base = mPlayStartTime;
    if (base.IsNull()) {
      // It doesn't really matter what 'base' is, so just use 'now' if
      // we haven't started playback.
      base = TimeStamp::Now();
    }
    mDecoder->mPlaybackStatistics.Start(
        base + TimeDuration::FromMilliseconds(NS_round(frame->mTime*1000)));
    mDecoder->mPlaybackStatistics.AddBytes(frame->mEndStreamPosition - mLastFramePosition);
    mDecoder->mPlaybackStatistics.Stop(
        base + TimeDuration::FromMilliseconds(NS_round(mCallbackPeriod*mLastFrame*1000)));
    mDecoder->UpdatePlaybackRate();
  }
  mLastFramePosition = frame->mEndStreamPosition;

  int num_tracks = oggplay_get_num_tracks(mPlayer);
  float audioTime = -1.0;
  float videoTime = -1.0;

  if (mVideoTrack != -1 &&
      num_tracks > mVideoTrack &&
      oggplay_callback_info_get_type(info[mVideoTrack]) == OGGPLAY_YUV_VIDEO) {
    OggPlayDataHeader** headers = oggplay_callback_info_get_headers(info[mVideoTrack]);
    if (headers[0]) {
      videoTime = ((float)oggplay_callback_info_get_presentation_time(headers[0]))/1000.0;
      HandleVideoData(frame, mVideoTrack, headers[0]);
    }
  }

  // If the audio stream has finished, but there's still video frames to
  // be rendered, we need to send blank audio data to the audio hardware,
  // so that the audio clock, which maintains the presentation time, keeps
  // incrementing.
  PRBool needSilence = PR_FALSE;

  if (mAudioTrack != -1 && num_tracks > mAudioTrack) {
    OggPlayDataType type = oggplay_callback_info_get_type(info[mAudioTrack]);
    needSilence = (type == OGGPLAY_INACTIVE);
    if (type == OGGPLAY_FLOATS_AUDIO) {
      OggPlayDataHeader** headers = oggplay_callback_info_get_headers(info[mAudioTrack]);
      if (headers[0]) {
        audioTime = ((float)oggplay_callback_info_get_presentation_time(headers[0]))/1000.0;
        int required = oggplay_callback_info_get_required(info[mAudioTrack]);
        for (int j = 0; j < required; ++j) {
          int size = oggplay_callback_info_get_record_size(headers[j]);
          OggPlayAudioData* audio_data = oggplay_callback_info_get_audio_data(headers[j]);
          HandleAudioData(frame, audio_data, size);
        }
      }
    }
  }

  if (needSilence) {
    // Write silence to keep audio clock moving for av sync
    size_t count = mAudioChannels * mAudioRate * mCallbackPeriod;
    // count must be evenly divisble by number of channels.
    count = mAudioChannels * PRInt32(NS_ceil(mAudioRate*mCallbackPeriod));
    float* data = frame->mAudioData.AppendElements(count);
    if (data) {
      memset(data, 0, sizeof(float)*count);
    }
  }

  // Pick one stream to act as the reference track to indicate if the
  // stream has ended, seeked, etc.
  if (videoTime >= 0) {
    frame->mState = oggplay_callback_info_get_stream_info(info[mVideoTrack]);
    frame->mDecodedFrameTime = videoTime;
  } else if (audioTime >= 0) {
    frame->mState = oggplay_callback_info_get_stream_info(info[mAudioTrack]);
    frame->mDecodedFrameTime = audioTime;
  } else {
    NS_WARNING("Encountered frame with no audio or video data");
    frame->mState = OGGPLAY_STREAM_UNINITIALISED;
    frame->mDecodedFrameTime = 0.0;
  }

  oggplay_buffer_release(mPlayer, info);
  return frame;
}

void nsOggDecodeStateMachine::HandleVideoData(FrameData* aFrame, int aTrackNum, OggPlayDataHeader* aVideoHeader) {
  if (!aVideoHeader)
    return;

  int y_width;
  int y_height;
  oggplay_get_video_y_size(mPlayer, aTrackNum, &y_width, &y_height);
  int uv_width;
  int uv_height;
  oggplay_get_video_uv_size(mPlayer, aTrackNum, &uv_width, &uv_height);

  if (y_width >= MAX_VIDEO_WIDTH || y_height >= MAX_VIDEO_HEIGHT) {
    return;
  }

  aFrame->mVideoWidth = y_width;
  aFrame->mVideoHeight = y_height;
  aFrame->mUVWidth = uv_width;
  aFrame->mUVHeight = uv_height;
  aFrame->SetVideoHeader(aVideoHeader);
}

void nsOggDecodeStateMachine::HandleAudioData(FrameData* aFrame, OggPlayAudioData* aAudioData, int aSize) {
  // 'aSize' is number of samples. Multiply by number of channels to
  // get the actual number of floats being sent.
  int size = aSize * mAudioChannels;

  aFrame->mAudioData.AppendElements(reinterpret_cast<float*>(aAudioData), size);
}

void nsOggDecodeStateMachine::PlayFrame() {
  // Play a frame of video and/or audio data.
  // If we are playing we open the audio stream if needed
  // If there are decoded frames in the queue a single frame
  // is popped off and played if it is time for that frame
  // to display. 
  // If it is not time yet to display the frame, we either
  // continue decoding frames, or wait until it is time for
  // the frame to display if the queue is full.
  //
  // If the decode state is not PLAYING then we just exit
  // so we can continue decoding frames. If the queue is
  // full we wait for a state change.
  nsAutoMonitor mon(mDecoder->GetMonitor());

  if (mDecoder->GetState() == nsOggDecoder::PLAY_STATE_PLAYING) {
    if (!mPlaying) {
      ResumePlayback();
    }

    if (!mDecodedFrames.IsEmpty()) {
      FrameData* frame = mDecodedFrames.Peek();
      if (frame->mState == OGGPLAY_STREAM_JUST_SEEKED) {
        // After returning from a seek all mTime members of
        // FrameData start again from a time position of 0.
        // Reset the play start time.
        mPlayStartTime = TimeStamp::Now();
        mPauseDuration = TimeDuration(0);
        frame->mState = OGGPLAY_STREAM_INITIALISED;
      }

      double time;
      PRUint32 hasAudio = frame->mAudioData.Length();
      for (;;) {
        // Even if the frame has had its audio data written we call
        // PlayAudio to ensure that any data we have buffered in the
        // nsAudioStream is written to the hardware.
        PlayAudio(frame);
        double hwtime = mAudioStream && hasAudio ? mAudioStream->GetPosition() : -1.0;
        time = hwtime < 0.0 ?
          (TimeStamp::Now() - mPlayStartTime - mPauseDuration).ToSeconds() :
          hwtime;
        // Is it time for the next frame?  Using an integer here avoids f.p.
        // rounding errors that can cause multiple 0ms waits (Bug 495352)
        PRInt64 wait = PRInt64((frame->mTime - time)*1000);
        if (wait <= 0)
          break;
        mon.Wait(PR_MillisecondsToInterval(wait));
        if (mState == DECODER_STATE_SHUTDOWN)
          return;
      }

      mDecodedFrames.Pop();
      QueueDecodedFrames();

      // Skip frames up to the one we should be showing.
      while (!mDecodedFrames.IsEmpty() && time >= mDecodedFrames.Peek()->mTime) {
        LOG(PR_LOG_DEBUG, ("Skipping frame time %f with audio at time %f", mDecodedFrames.Peek()->mTime, time));
        PlayAudio(frame);
        delete frame;
        frame = mDecodedFrames.Peek();
        mDecodedFrames.Pop();
      }
      if (time < frame->mTime + mCallbackPeriod) {
        PlayAudio(frame);
        PlayVideo(frame);
        mDecoder->mPlaybackPosition = frame->mEndStreamPosition;
        UpdatePlaybackPosition(frame->mDecodedFrameTime);
        delete frame;
      }
      else {
        PlayAudio(frame);
        delete frame;
        frame = 0;
      }
    }
  }
  else {
    if (mPlaying) {
      PausePlayback();
    }

    if (mState == DECODER_STATE_DECODING) {
      mon.Wait();
      if (mState == DECODER_STATE_SHUTDOWN) {
        return;
      }
    }
  }
}

void nsOggDecodeStateMachine::PlayVideo(FrameData* aFrame)
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "PlayVideo() called without acquiring decoder monitor");
  if (aFrame && aFrame->mVideoHeader) {
    OggPlayVideoData* videoData = oggplay_callback_info_get_video_data(aFrame->mVideoHeader);

    OggPlayYUVChannels yuv;
    yuv.ptry = videoData->y;
    yuv.ptru = videoData->u;
    yuv.ptrv = videoData->v;
    yuv.uv_width = aFrame->mUVWidth;
    yuv.uv_height = aFrame->mUVHeight;
    yuv.y_width = aFrame->mVideoWidth;
    yuv.y_height = aFrame->mVideoHeight;

    size_t size = aFrame->mVideoWidth * aFrame->mVideoHeight * 4;
    nsAutoArrayPtr<unsigned char> buffer(new unsigned char[size]);
    if (!buffer)
      return;

    OggPlayRGBChannels rgb;
    rgb.ptro = buffer;
    rgb.rgb_width = aFrame->mVideoWidth;
    rgb.rgb_height = aFrame->mVideoHeight;

    oggplay_yuv2bgra(&yuv, &rgb);

    mDecoder->SetRGBData(aFrame->mVideoWidth, aFrame->mVideoHeight,
                         mFramerate, mAspectRatio, buffer.forget());

    // Don't play the frame's video data more than once.
    aFrame->ClearVideoHeader();
  }
}

void nsOggDecodeStateMachine::PlayAudio(FrameData* aFrame)
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "PlayAudio() called without acquiring decoder monitor");
  if (!mAudioStream)
    return;

  aFrame->Write(mAudioStream);
}

void nsOggDecodeStateMachine::OpenAudioStream()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "OpenAudioStream() called without acquiring decoder monitor");
  mAudioStream = new nsAudioStream();
  if (!mAudioStream) {
    LOG(PR_LOG_ERROR, ("Could not create audio stream"));
  }
  else {
    mAudioStream->Init(mAudioChannels, mAudioRate, nsAudioStream::FORMAT_FLOAT32);
    mAudioStream->SetVolume(mVolume);
  }
}

void nsOggDecodeStateMachine::CloseAudioStream()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "CloseAudioStream() called without acquiring decoder monitor");
  if (mAudioStream) {
    mAudioStream->Shutdown();
    mAudioStream = nsnull;
  }
}

void nsOggDecodeStateMachine::StartAudio()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "StartAudio() called without acquiring decoder monitor");
  if (HasAudio()) {
    OpenAudioStream();
  }
}

void nsOggDecodeStateMachine::StopAudio()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "StopAudio() called without acquiring decoder monitor");
  if (HasAudio()) {
    CloseAudioStream();
  }
}

void nsOggDecodeStateMachine::StartPlayback()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "StartPlayback() called without acquiring decoder monitor");
  StartAudio();
  mPlaying = PR_TRUE;

  // If this is the very first play, then set the initial start time
  if (mPlayStartTime.IsNull()) {
    mPlayStartTime = TimeStamp::Now();
  }

  // If we have been paused previously, then compute duration spent paused
  if (!mPauseStartTime.IsNull()) {
    mPauseDuration += TimeStamp::Now() - mPauseStartTime;
    // Null out mPauseStartTime
    mPauseStartTime = TimeStamp();
  }
  mPlayStartTime = TimeStamp::Now();
  mPauseDuration = 0;

}

void nsOggDecodeStateMachine::StopPlayback()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "StopPlayback() called without acquiring decoder monitor");
  mLastFrame = mDecodedFrames.ResetTimes(mCallbackPeriod);
  StopAudio();
  mPlaying = PR_FALSE;
  mPauseStartTime = TimeStamp::Now();
}

void nsOggDecodeStateMachine::PausePlayback()
{
  if (!mAudioStream) {
    StopPlayback();
    return;
  }
  mAudioStream->Pause();
  mPlaying = PR_FALSE;
  mPauseStartTime = TimeStamp::Now();
  if (mAudioStream->GetPosition() < 0) {
    mLastFrame = mDecodedFrames.ResetTimes(mCallbackPeriod);
  }
}

void nsOggDecodeStateMachine::ResumePlayback()
{
 if (!mAudioStream) {
    StartPlayback();
    return;
 }
 
 mAudioStream->Resume();
 mPlaying = PR_TRUE;

 // Compute duration spent paused
 if (!mPauseStartTime.IsNull()) {
   mPauseDuration += TimeStamp::Now() - mPauseStartTime;
   // Null out mPauseStartTime
   mPauseStartTime = TimeStamp();
 }
 mPlayStartTime = TimeStamp::Now();
 mPauseDuration = 0;
}

void nsOggDecodeStateMachine::UpdatePlaybackPosition(float aTime)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mDecoder->GetMonitor());
  mCurrentFrameTime = aTime - mPlaybackStartTime;
  if (!mPositionChangeQueued) {
    mPositionChangeQueued = PR_TRUE;
    nsCOMPtr<nsIRunnable> event =
      NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, PlaybackPositionChanged);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }
}

void nsOggDecodeStateMachine::QueueDecodedFrames()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "QueueDecodedFrames() called without acquiring decoder monitor");
  FrameData* frame;
  while (!mDecodedFrames.IsFull() && (frame = NextFrame())) {
    if (mDecodedFrames.GetCount() < 2) {
      // Transitioning from 0 to 1 frames or from 1 to 2 frames could
      // affect HaveNextFrameData and hence what UpdateReadyStateForData does.
      // This could change us from HAVE_CURRENT_DATA to HAVE_FUTURE_DATA
      // (or even HAVE_ENOUGH_DATA), so we'd better trigger an
      // UpdateReadyStateForData.
      nsCOMPtr<nsIRunnable> event = 
        NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, UpdateReadyStateForData);
      NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
    }
    mDecodedFrames.Push(frame);
  }
}

void nsOggDecodeStateMachine::ClearPositionChangeFlag()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "ClearPositionChangeFlag() called without acquiring decoder monitor");
  mPositionChangeQueued = PR_FALSE;
}

void nsOggDecodeStateMachine::SetVolume(float volume)
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "SetVolume() called without acquiring decoder monitor");
  if (mAudioStream) {
    mAudioStream->SetVolume(volume);
  }

  mVolume = volume;
}

float nsOggDecodeStateMachine::GetCurrentTime()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "GetCurrentTime() called without acquiring decoder monitor");
  return mCurrentFrameTime;
}

PRInt64 nsOggDecodeStateMachine::GetDuration()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "GetDuration() called without acquiring decoder monitor");
  return mDuration;
}

void nsOggDecodeStateMachine::SetDuration(PRInt64 aDuration)
{
   //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "SetDuration() called without acquiring decoder monitor");
  mDuration = aDuration;
}

void nsOggDecodeStateMachine::SetSeekable(PRBool aSeekable)
{
   //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "SetSeekable() called without acquiring decoder monitor");
  mSeekable = aSeekable;
}

void nsOggDecodeStateMachine::Shutdown()
{
  // oggplay_prepare_for_close cannot be undone. Once called, the
  // mPlayer object cannot decode any more frames. Once we've entered
  // the shutdown state here there's no going back.
  nsAutoMonitor mon(mDecoder->GetMonitor());

  // Change state before issuing shutdown request to threads so those
  // threads can start exiting cleanly during the Shutdown call.
  LOG(PR_LOG_DEBUG, ("Changed state to SHUTDOWN"));
  mState = DECODER_STATE_SHUTDOWN;
  mon.NotifyAll();

  if (mPlayer) {
    // This will unblock the step decode loop in the
    // StepDecode thread. The thread can then be safely
    // shutdown.
    oggplay_prepare_for_close(mPlayer);
  }
}

void nsOggDecodeStateMachine::Decode()
{
  // When asked to decode, switch to decoding only if
  // we are currently buffering.
  nsAutoMonitor mon(mDecoder->GetMonitor());
  if (mState == DECODER_STATE_BUFFERING) {
    LOG(PR_LOG_DEBUG, ("Changed state from BUFFERING to DECODING"));
    mState = DECODER_STATE_DECODING;
    mon.NotifyAll();
  }
}

void nsOggDecodeStateMachine::Seek(float aTime)
{
  nsAutoMonitor mon(mDecoder->GetMonitor());
  // nsOggDecoder::mPlayState should be SEEKING while we seek, and
  // in that case nsOggDecoder shouldn't be calling us.
  NS_ASSERTION(mState != DECODER_STATE_SEEKING,
               "We shouldn't already be seeking");
  mSeekTime = aTime + mPlaybackStartTime;
  float duration = static_cast<float>(mDuration) / 1000.0;
  NS_ASSERTION(mSeekTime >= 0 && mSeekTime <= duration,
               "Can only seek in range [0,duration]");
  LOG(PR_LOG_DEBUG, ("Changed state to SEEKING (to %f)", aTime));
  mState = DECODER_STATE_SEEKING;
}

class ByteRange {
public:
  ByteRange() : mStart(-1), mEnd(-1) {}
  ByteRange(PRInt64 aStart, PRInt64 aEnd) : mStart(aStart), mEnd(aEnd) {}
  PRInt64 mStart, mEnd;
};

static void GetBufferedBytes(nsMediaStream* aStream, nsTArray<ByteRange>& aRanges)
{
  PRInt64 startOffset = 0;
  while (PR_TRUE) {
    PRInt64 endOffset = aStream->GetCachedDataEnd(startOffset);
    if (endOffset == startOffset) {
      // Uncached at startOffset.
      endOffset = aStream->GetNextCachedData(startOffset);
      if (endOffset == -1) {
        // Uncached at startOffset until endOffset of stream, or we're at
        // the end of stream.
        break;
      }
    } else {
      // Bytes [startOffset..endOffset] are cached.
      PRInt64 cachedLength = endOffset - startOffset;
      // Only bother trying to seek inside ranges greater than
      // MIN_BOUNDED_SEEK_SIZE, so that the bounded seek is unlikely to
      // read outside of the range when finding Ogg page boundaries.
      if (cachedLength > MIN_BOUNDED_SEEK_SIZE) {
        aRanges.AppendElement(ByteRange(startOffset, endOffset));
      }
    }
    startOffset = endOffset;
  }
}

nsresult nsOggDecodeStateMachine::Seek(float aTime, nsChannelReader* aReader)
{
  LOG(PR_LOG_DEBUG, ("About to seek OggPlay to %fms", aTime));
  nsMediaStream* stream = aReader->Stream(); 
  nsAutoTArray<ByteRange, 16> ranges;
  stream->Pin();
  GetBufferedBytes(stream, ranges);
  PRInt64 rv = -1;
  for (PRUint32 i = 0; rv < 0 && i < ranges.Length(); i++) {
    rv = oggplay_seek_to_keyframe(mPlayer,
                                  ogg_int64_t(aTime * 1000),
                                  ranges[i].mStart,
                                  ranges[i].mEnd);
  }
  stream->Unpin();

  if (rv < 0) {
    // Could not seek in a buffered range, fall back to seeking over the
    // entire media.
    rv = oggplay_seek_to_keyframe(mPlayer,
                                  ogg_int64_t(aTime * 1000),
                                  0,
                                  stream->GetLength());
  }

  LOG(PR_LOG_DEBUG, ("Finished seeking OggPlay"));

  return (rv < 0) ? NS_ERROR_FAILURE : NS_OK;
}

void nsOggDecodeStateMachine::DecodeToFrame(nsAutoMonitor& aMonitor,
                                            float aTime)
{
  // Drop frames before the target time.
  float target = aTime - mCallbackPeriod / 2.0;
  FrameData* frame = nsnull;
  OggPlayErrorCode r;
  mLastFrame = 0;

  // Some of the audio data from previous frames actually belongs
  // to this frame and later frames. So rescue that data and stuff
  // it into the first frame.
  float audioTime = 0;
  nsTArray<float> audioData;
  do {
    if (frame) {
      audioData.AppendElements(frame->mAudioData);
      audioTime += frame->mAudioData.Length() /
        (float)mAudioRate / (float)mAudioChannels;
    }
    do {
      aMonitor.Exit();
      r = DecodeFrame();
      aMonitor.Enter();
    } while (mState != DECODER_STATE_SHUTDOWN && r == E_OGGPLAY_TIMEOUT);

    HandleDecodeErrors(r);

    if (mState == DECODER_STATE_SHUTDOWN)
      break;

    FrameData* nextFrame = NextFrame();
    if (!nextFrame)
      break;

    delete frame;
    frame = nextFrame;
  } while (frame->mDecodedFrameTime < target);

  if (mState == DECODER_STATE_SHUTDOWN) {
    delete frame;
    return;
  }

  NS_ASSERTION(frame != nsnull, "No frame after decode!");
  if (frame) {
    if (audioTime > frame->mTime) {
      // liboggplay gave us more data than expected, we need to prepend
      // the extra data to the current frame to keep audio in sync.
      audioTime -= frame->mTime;
      // numExtraSamples must be evenly divisble by number of channels.
      size_t numExtraSamples = mAudioChannels *
        PRInt32(NS_ceil(mAudioRate*audioTime));
      float* data = audioData.Elements() + audioData.Length() - numExtraSamples;
      float* dst = frame->mAudioData.InsertElementsAt(0, numExtraSamples);
      memcpy(dst, data, numExtraSamples * sizeof(float));
    }

    mLastFrame = 0;
    frame->mTime = 0;
    frame->mState = OGGPLAY_STREAM_JUST_SEEKED;
    mDecodedFrames.Push(frame);
    UpdatePlaybackPosition(frame->mDecodedFrameTime);
    PlayVideo(frame);
  }
}

void nsOggDecodeStateMachine::StopStepDecodeThread(nsAutoMonitor* aMonitor)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mDecoder->GetMonitor());

  if (!mStepDecodeThread)
    return;

  if (!mDecodingCompleted) {
    // Break the step-decode thread out of the decoding loop. First
    // set the exit flag so it will exit the loop.
    mExitStepDecodeThread = PR_TRUE;
    // Remove liboggplay frame buffer so that the step-decode thread
    // can unblock in liboggplay.
    delete NextFrame();
    // Now notify to wake it up if it's waiting on the monitor.
    aMonitor->NotifyAll();
  }

  aMonitor->Exit();
  mStepDecodeThread->Shutdown();
  aMonitor->Enter();
  mStepDecodeThread = nsnull;
}

nsresult nsOggDecodeStateMachine::Run()
{
  nsChannelReader* reader = mDecoder->GetReader();
  NS_ENSURE_TRUE(reader, NS_ERROR_NULL_POINTER);
  while (PR_TRUE) {
   nsAutoMonitor mon(mDecoder->GetMonitor());
   switch(mState) {
    case DECODER_STATE_SHUTDOWN:
      if (mPlaying) {
        StopPlayback();
      }
      StopStepDecodeThread(&mon);
      NS_ASSERTION(mState == DECODER_STATE_SHUTDOWN,
                   "How did we escape from the shutdown state???");
      return NS_OK;

    case DECODER_STATE_DECODING_METADATA:
      {
        mon.Exit();
        LoadOggHeaders(reader);
        mon.Enter();
      
        OggPlayErrorCode r = E_OGGPLAY_TIMEOUT;
        while (mState != DECODER_STATE_SHUTDOWN && r == E_OGGPLAY_TIMEOUT) {
          mon.Exit();
          r = DecodeFrame();
          mon.Enter();
        }

        HandleDecodeErrors(r);

        if (mState == DECODER_STATE_SHUTDOWN)
          continue;

        mLastFrame = 0;
        FrameData* frame = NextFrame();
        if (frame) {
          mDecodedFrames.Push(frame);
          mDecoder->mPlaybackPosition = frame->mEndStreamPosition;
          mPlaybackStartTime = frame->mDecodedFrameTime;
          UpdatePlaybackPosition(frame->mDecodedFrameTime);
          // Now that we know the start offset, we can tell the channel
          // reader the last frame time.
          if (mGotDurationFromHeader) {
            // Duration was in HTTP header, so the last frame time is
            // start frame time + duration.
            reader->SetLastFrameTime((PRInt64)(mPlaybackStartTime * 1000) + mDuration);
          }
          else if (mDuration != -1) {
            // Got duration by seeking to end and getting timestamp of last
            // page; mDuration holds the timestamp of the end of the last page.
            reader->SetLastFrameTime(mDuration);
            // Duration needs to be corrected so it's the length of media, not
            // the last frame's end time. Note mPlaybackStartTime is
            // presentation time, which is the start-time of the frame.
            mDuration -= (PRInt64)(mPlaybackStartTime * 1000);
          }
          PlayVideo(frame);
        }

        // Inform the element that we've loaded the Ogg metadata and the
        // first frame.
        nsCOMPtr<nsIRunnable> metadataLoadedEvent = 
          NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, MetadataLoaded); 
        NS_DispatchToMainThread(metadataLoadedEvent, NS_DISPATCH_NORMAL);

        if (mState == DECODER_STATE_DECODING_METADATA) {
          LOG(PR_LOG_DEBUG, ("Changed state from DECODING_METADATA to DECODING"));
          mState = DECODER_STATE_DECODING;
        }
      }
      break;

    case DECODER_STATE_DECODING:
      {
        // If there is no step decode thread,  start it. It may not be running
        // due to us having completed and then restarted playback, seeking, 
        // or if this is the initial play.
        if (!mStepDecodeThread) {
          nsresult rv = NS_NewThread(getter_AddRefs(mStepDecodeThread));
          if (NS_FAILED(rv)) {
            mState = DECODER_STATE_SHUTDOWN;
            continue;
          }

          mBufferExhausted = PR_FALSE;
          mDecodingCompleted = PR_FALSE;
          mExitStepDecodeThread = PR_FALSE;
          nsCOMPtr<nsIRunnable> event = new nsOggStepDecodeEvent(this, mPlayer);
          mStepDecodeThread->Dispatch(event, NS_DISPATCH_NORMAL);
        }

        // Get the decoded frames and store them in our queue of decoded frames
        QueueDecodedFrames();
        while (mDecodedFrames.IsEmpty() && !mDecodingCompleted &&
               !mBufferExhausted) {
          mon.Wait(PR_MillisecondsToInterval(PRInt64(mCallbackPeriod*500)));
          if (mState != DECODER_STATE_DECODING)
            break;
          QueueDecodedFrames();
        }

        if (mState != DECODER_STATE_DECODING)
          continue;

        if (mDecodingCompleted) {
          LOG(PR_LOG_DEBUG, ("Changed state from DECODING to COMPLETED"));
          mState = DECODER_STATE_COMPLETED;
          StopStepDecodeThread(&mon);
          continue;
        }

        // Show at least the first frame if we're not playing
        // so we have a poster frame on initial load and after seek.
        if (!mPlaying && !mDecodedFrames.IsEmpty()) {
          PlayVideo(mDecodedFrames.Peek());
        }

        if (mBufferExhausted &&
            mDecoder->GetState() == nsOggDecoder::PLAY_STATE_PLAYING &&
            !mDecoder->mReader->Stream()->IsDataCachedToEndOfStream(mDecoder->mDecoderPosition) &&
            !mDecoder->mReader->Stream()->IsSuspendedByCache()) {
          // There is at most one frame in the queue and there's
          // more data to load. Let's buffer to make sure we can play a
          // decent amount of video in the future.
          if (mPlaying) {
            PausePlayback();
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
          nsCOMPtr<nsIRunnable> event = 
            NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, UpdateReadyStateForData);
          NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);

          mBufferingStart = TimeStamp::Now();
          PRPackedBool reliable;
          double playbackRate = mDecoder->ComputePlaybackRate(&reliable);
          mBufferingEndOffset = mDecoder->mDecoderPosition +
              BUFFERING_RATE(playbackRate) * BUFFERING_WAIT;
          mState = DECODER_STATE_BUFFERING;
          if (mPlaying) {
            PausePlayback();
          }
          LOG(PR_LOG_DEBUG, ("Changed state from DECODING to BUFFERING"));
        } else {
          if (mBufferExhausted) {
            // This will wake up the step decode thread and force it to
            // call oggplay_step_decoding at least once. This guarantees
            // we make progress.
            mBufferExhausted = PR_FALSE;
            mon.NotifyAll();
          }
          PlayFrame();
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
        StopStepDecodeThread(&mon);
        if (mState == DECODER_STATE_SHUTDOWN)
          continue;

        float seekTime = mSeekTime;
        mDecoder->StopProgressUpdates();

        StopPlayback();

        // Remove all frames decoded prior to seek from the queue
        while (!mDecodedFrames.IsEmpty()) {
          delete mDecodedFrames.Pop();
        }
        // SeekingStarted will do a UpdateReadyStateForData which will
        // inform the element and its users that we have no frames
        // to display

        mon.Exit();
        nsCOMPtr<nsIRunnable> startEvent = 
          NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, SeekingStarted);
        NS_DispatchToMainThread(startEvent, NS_DISPATCH_SYNC);
        
        nsresult res = Seek(seekTime, reader);

        // Reactivate all tracks. Liboggplay deactivates tracks when it
        // reads to the end of stream, but they must be reactivated in order
        // to start reading from them again.
        SetTracksActive();

        mon.Enter();
        mDecoder->StartProgressUpdates();
        mLastFramePosition = mDecoder->mPlaybackPosition;
        if (mState == DECODER_STATE_SHUTDOWN)
          continue;

        if (NS_SUCCEEDED(res)) {
          DecodeToFrame(mon, seekTime);
          // mSeekTime should not have changed. While we seek, mPlayState
          // should always be PLAY_STATE_SEEKING and no-one will call
          // nsOggDecoderStateMachine::Seek.
          NS_ASSERTION(seekTime == mSeekTime, "No-one should have changed mSeekTime");
          if (mState == DECODER_STATE_SHUTDOWN) {
            continue;
          }

          OggPlayErrorCode r;
          // Now try to decode another frame to see if we're at the end.
          do {
            mon.Exit();
            r = DecodeFrame();
            mon.Enter();
          } while (mState != DECODER_STATE_SHUTDOWN && r == E_OGGPLAY_TIMEOUT);
          HandleDecodeErrors(r);
          if (mState == DECODER_STATE_SHUTDOWN)
            continue;
          QueueDecodedFrames();
        }

        // Change state to DECODING now. SeekingStopped will call
        // nsOggDecodeStateMachine::Seek to reset our state to SEEKING
        // if we need to seek again.
        LOG(PR_LOG_DEBUG, ("Changed state from SEEKING (to %f) to DECODING", seekTime));
        mState = DECODER_STATE_DECODING;
        nsCOMPtr<nsIRunnable> stopEvent;
        if (mDecodedFrames.GetCount() > 1) {
          stopEvent = NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, SeekingStopped);
          mState = DECODER_STATE_DECODING;
        } else {
          stopEvent = NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, SeekingStoppedAtEnd);
          mState = DECODER_STATE_COMPLETED;
        }
        mon.NotifyAll();

        mon.Exit();
        NS_DispatchToMainThread(stopEvent, NS_DISPATCH_SYNC);        
        mon.Enter();
      }
      break;

    case DECODER_STATE_BUFFERING:
      {
        TimeStamp now = TimeStamp::Now();
        if (now - mBufferingStart < TimeDuration::FromSeconds(BUFFERING_WAIT) &&
            mDecoder->mReader->Stream()->GetCachedDataEnd(mDecoder->mDecoderPosition) < mBufferingEndOffset &&
            !mDecoder->mReader->Stream()->IsDataCachedToEndOfStream(mDecoder->mDecoderPosition) &&
            !mDecoder->mReader->Stream()->IsSuspendedByCache()) {
          LOG(PR_LOG_DEBUG, 
              ("In buffering: buffering data until %d bytes available or %f seconds", 
               PRUint32(mBufferingEndOffset - mDecoder->mReader->Stream()->GetCachedDataEnd(mDecoder->mDecoderPosition)),
               BUFFERING_WAIT - (now - mBufferingStart).ToSeconds()));
          mon.Wait(PR_MillisecondsToInterval(1000));
          if (mState == DECODER_STATE_SHUTDOWN)
            continue;
        } else {
          LOG(PR_LOG_DEBUG, ("Changed state from BUFFERING to DECODING"));
          mState = DECODER_STATE_DECODING;
        }

        if (mState != DECODER_STATE_BUFFERING) {
          mBufferExhausted = PR_FALSE;
          // Notify to allow blocked decoder thread to continue
          mon.NotifyAll();
          nsCOMPtr<nsIRunnable> event = 
            NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, UpdateReadyStateForData);
          NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
          if (mDecoder->GetState() == nsOggDecoder::PLAY_STATE_PLAYING) {
            if (!mPlaying) {
              ResumePlayback();
            }
          }
        }

        break;
      }

    case DECODER_STATE_COMPLETED:
      {
        // Get all the remaining decoded frames in the liboggplay buffer and
        // place them in the frame queue.
        QueueDecodedFrames();

        // Play the remaining frames in the frame queue
        while (mState == DECODER_STATE_COMPLETED &&
               !mDecodedFrames.IsEmpty()) {
          PlayFrame();
          if (mState == DECODER_STATE_COMPLETED) {
            // Wait for the time of one frame so we don't tight loop
            // and we need to release the monitor so timeupdate and
            // invalidate's on the main thread can occur.
            mon.Wait(PR_MillisecondsToInterval(PRInt64(mCallbackPeriod*1000)));
            QueueDecodedFrames();
          }
        }

        if (mState != DECODER_STATE_COMPLETED)
          continue;

        if (mAudioStream) {
          mon.Exit();
          LOG(PR_LOG_DEBUG, ("Begin nsAudioStream::Drain"));
          mAudioStream->Drain();
          LOG(PR_LOG_DEBUG, ("End nsAudioStream::Drain"));
          mon.Enter();

          // After the drain call the audio stream is unusable. Close it so that
          // next time audio is used a new stream is created. The StopPlayback
          // call also resets the playing flag so audio is restarted correctly.
          StopPlayback();

          if (mState != DECODER_STATE_COMPLETED)
            continue;
        }

        // Set the right current time
        mCurrentFrameTime += mCallbackPeriod;
        if (mDuration >= 0) {
          mCurrentFrameTime = PR_MAX(mCurrentFrameTime, mDuration / 1000.0);
        }

        mon.Exit();
        nsCOMPtr<nsIRunnable> event =
          NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, PlaybackEnded);
        NS_DispatchToMainThread(event, NS_DISPATCH_SYNC);
        mon.Enter();

        while (mState == DECODER_STATE_COMPLETED) {
          mon.Wait();
        }
      }
      break;
    }
  }

  return NS_OK;
}

void nsOggDecodeStateMachine::LoadOggHeaders(nsChannelReader* aReader) 
{
  LOG(PR_LOG_DEBUG, ("Loading Ogg Headers"));
  mPlayer = oggplay_open_with_reader(aReader);
  if (mPlayer) {
    LOG(PR_LOG_DEBUG, ("There are %d tracks", oggplay_get_num_tracks(mPlayer)));

    for (int i = 0; i < oggplay_get_num_tracks(mPlayer); ++i) {
      LOG(PR_LOG_DEBUG, ("Tracks %d: %s", i, oggplay_get_track_typename(mPlayer, i)));
      if (mVideoTrack == -1 && oggplay_get_track_type(mPlayer, i) == OGGZ_CONTENT_THEORA) {
        oggplay_set_callback_num_frames(mPlayer, i, 1);
        mVideoTrack = i;

        int fpsd, fpsn;
        oggplay_get_video_fps(mPlayer, i, &fpsd, &fpsn);
        mFramerate = fpsd == 0 ? 0.0 : float(fpsn)/float(fpsd);
        mCallbackPeriod = 1.0 / mFramerate;
        LOG(PR_LOG_DEBUG, ("Frame rate: %f", mFramerate));

        int aspectd, aspectn;
        // this can return E_OGGPLAY_UNINITIALIZED if the video has
        // no aspect ratio data. We assume 1.0 in that case.
        OggPlayErrorCode r =
          oggplay_get_video_aspect_ratio(mPlayer, i, &aspectd, &aspectn);
        mAspectRatio = r == E_OGGPLAY_OK && aspectd > 0 ?
            float(aspectn)/float(aspectd) : 1.0;

        int y_width;
        int y_height;
        oggplay_get_video_y_size(mPlayer, i, &y_width, &y_height);
        mDecoder->SetRGBData(y_width, y_height, mFramerate, mAspectRatio, nsnull);
      }
      else if (mAudioTrack == -1 && oggplay_get_track_type(mPlayer, i) == OGGZ_CONTENT_VORBIS) {
        mAudioTrack = i;
        oggplay_set_offset(mPlayer, i, OGGPLAY_AUDIO_OFFSET);
        oggplay_get_audio_samplerate(mPlayer, i, &mAudioRate);
        oggplay_get_audio_channels(mPlayer, i, &mAudioChannels);
        LOG(PR_LOG_DEBUG, ("samplerate: %d, channels: %d", mAudioRate, mAudioChannels));
      }
    }

    SetTracksActive();

    if (mVideoTrack == -1) {
      oggplay_set_callback_num_frames(mPlayer, mAudioTrack, OGGPLAY_FRAMES_PER_CALLBACK);
      mCallbackPeriod = 1.0 / (float(mAudioRate) / OGGPLAY_FRAMES_PER_CALLBACK);
    }
    LOG(PR_LOG_DEBUG, ("Callback Period: %f", mCallbackPeriod));

    oggplay_use_buffer(mPlayer, OGGPLAY_BUFFER_SIZE);

    // Get the duration from the Ogg file. We only do this if the
    // content length of the resource is known as we need to seek
    // to the end of the file to get the last time field. We also
    // only do this if the resource is seekable and if we haven't
    // already obtained the duration via an HTTP header.
    {
      nsAutoMonitor mon(mDecoder->GetMonitor());
      mGotDurationFromHeader = (mDuration != -1);
      if (mState != DECODER_STATE_SHUTDOWN &&
          aReader->Stream()->GetLength() >= 0 &&
          mSeekable &&
          mDuration == -1) {
        mDecoder->StopProgressUpdates();
        // Don't hold the monitor during the duration
        // call as it can issue seek requests
        // and blocks until these are completed.
        mon.Exit();
        PRInt64 d = oggplay_get_duration(mPlayer);
        oggplay_seek(mPlayer, 0);
        mon.Enter();
        mDuration = d;
        mDecoder->StartProgressUpdates();
        mDecoder->UpdatePlaybackRate();
      }
      if (mState == DECODER_STATE_SHUTDOWN)
        return;
    }
  }
}

void nsOggDecodeStateMachine::SetTracksActive()
{
  if (mVideoTrack != -1 && 
      oggplay_set_track_active(mPlayer, mVideoTrack) < 0)  {
    LOG(PR_LOG_ERROR, ("Could not set track %d active", mVideoTrack));
  }

  if (mAudioTrack != -1 && 
      oggplay_set_track_active(mPlayer, mAudioTrack) < 0)  {
    LOG(PR_LOG_ERROR, ("Could not set track %d active", mAudioTrack));
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsOggDecoder, nsIObserver)

void nsOggDecoder::Pause() 
{
  nsAutoMonitor mon(mMonitor);
  if (mPlayState == PLAY_STATE_SEEKING || mPlayState == PLAY_STATE_ENDED) {
    mNextState = PLAY_STATE_PAUSED;
    return;
  }

  ChangeState(PLAY_STATE_PAUSED);
}

void nsOggDecoder::SetVolume(float volume)
{
  nsAutoMonitor mon(mMonitor);
  mInitialVolume = volume;

  if (mDecodeStateMachine) {
    mDecodeStateMachine->SetVolume(volume);
  }
}

float nsOggDecoder::GetDuration()
{
  if (mDuration >= 0) {
     return static_cast<float>(mDuration) / 1000.0;
  }

  return std::numeric_limits<float>::quiet_NaN();
}

nsOggDecoder::nsOggDecoder() :
  nsMediaDecoder(),
  mDecoderPosition(0),
  mPlaybackPosition(0),
  mCurrentTime(0.0),
  mInitialVolume(0.0),
  mRequestedSeekTime(-1.0),
  mDuration(-1),
  mSeekable(PR_TRUE),
  mReader(nsnull),
  mMonitor(nsnull),
  mPlayState(PLAY_STATE_PAUSED),
  mNextState(PLAY_STATE_PAUSED),
  mResourceLoaded(PR_FALSE),
  mIgnoreProgressData(PR_FALSE)
{
  MOZ_COUNT_CTOR(nsOggDecoder);
}

PRBool nsOggDecoder::Init(nsHTMLMediaElement* aElement)
{
  if (!nsMediaDecoder::Init(aElement))
    return PR_FALSE;

  mMonitor = nsAutoMonitor::NewMonitor("media.decoder");
  if (!mMonitor)
    return PR_FALSE;

  RegisterShutdownObserver();

  mReader = new nsChannelReader();
  NS_ENSURE_TRUE(mReader, PR_FALSE);

  return PR_TRUE;
}

void nsOggDecoder::Stop()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be called on main thread");

  // The decode thread must die before the state machine can die.
  // The state machine must die before the reader.
  // The state machine must die before the decoder.
  if (mDecodeThread)
    mDecodeThread->Shutdown();

  mDecodeThread = nsnull;
  mDecodeStateMachine = nsnull;
  mReader = nsnull;
}

void nsOggDecoder::Shutdown()
{
  NS_ASSERTION(NS_IsMainThread(), 
               "nsOggDecoder::Shutdown called on non-main thread");  
  
  if (mShuttingDown)
    return;

  mShuttingDown = PR_TRUE;

  // This changes the decoder state to SHUTDOWN and does other things
  // necessary to unblock the state machine thread if it's blocked, so
  // the asynchronous shutdown in nsDestroyStateMachine won't deadlock.
  if (mDecodeStateMachine) {
    mDecodeStateMachine->Shutdown();
  }

  // Force any outstanding seek and byterange requests to complete
  // to prevent shutdown from deadlocking.
  mReader->Stream()->Close();

  ChangeState(PLAY_STATE_SHUTDOWN);
  nsMediaDecoder::Shutdown();

  // We can't destroy mDecodeStateMachine until mDecodeThread is shut down.
  // It's unsafe to Shutdown() the decode thread here, as
  // nsIThread::Shutdown() may run events, such as JS event handlers,
  // and we could be running at an unsafe time such as during element
  // destruction.
  // So we destroy the decoder on the main thread in an asynchronous event.
  // See bug 468721.
  nsCOMPtr<nsIRunnable> event =
    NS_NEW_RUNNABLE_METHOD(nsOggDecoder, this, Stop);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);

  UnregisterShutdownObserver();
}

nsOggDecoder::~nsOggDecoder()
{
  MOZ_COUNT_DTOR(nsOggDecoder);
  nsAutoMonitor::DestroyMonitor(mMonitor);
}

nsresult nsOggDecoder::Load(nsMediaStream* aStream,
                            nsIStreamListener** aStreamListener)
{
  NS_ASSERTION(aStreamListener, "A listener should be requested here");

  *aStreamListener = nsnull;

  {
    // Hold the lock while we do this to set proper lock ordering
    // expectations for dynamic deadlock detectors: decoder lock(s)
    // should be grabbed before the cache lock
    nsAutoMonitor mon(mMonitor);

    nsresult rv = aStream->Open(aStreamListener);
    if (NS_FAILED(rv)) {
      delete aStream;
      return rv;
    }

    mReader->Init(aStream);
  }

  nsresult rv = NS_NewThread(getter_AddRefs(mDecodeThread));
  NS_ENSURE_SUCCESS(rv, rv);

  mDecodeStateMachine = new nsOggDecodeStateMachine(this);
  {
    nsAutoMonitor mon(mMonitor);
    mDecodeStateMachine->SetSeekable(mSeekable);
  }

  ChangeState(PLAY_STATE_LOADING);

  return mDecodeThread->Dispatch(mDecodeStateMachine, NS_DISPATCH_NORMAL);
}

nsresult nsOggDecoder::Play()
{
  nsAutoMonitor mon(mMonitor);
  if (mPlayState == PLAY_STATE_SEEKING) {
    mNextState = PLAY_STATE_PLAYING;
    return NS_OK;
  }
  if (mPlayState == PLAY_STATE_ENDED)
    return Seek(0);

  ChangeState(PLAY_STATE_PLAYING);

  return NS_OK;
}

nsresult nsOggDecoder::Seek(float aTime)
{
  nsAutoMonitor mon(mMonitor);

  if (aTime < 0.0)
    return NS_ERROR_FAILURE;

  mRequestedSeekTime = aTime;

  // If we are already in the seeking state, then setting mRequestedSeekTime
  // above will result in the new seek occurring when the current seek
  // completes.
  if (mPlayState != PLAY_STATE_SEEKING) {
    if (mPlayState == PLAY_STATE_ENDED) {
      mNextState = PLAY_STATE_PLAYING;
    } else {
      mNextState = mPlayState;
    }
    ChangeState(PLAY_STATE_SEEKING);
  }

  return NS_OK;
}

nsresult nsOggDecoder::PlaybackRateChanged()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

float nsOggDecoder::GetCurrentTime()
{
  return mCurrentTime;
}

nsMediaStream* nsOggDecoder::GetCurrentStream()
{
  return mReader ? mReader->Stream() : nsnull;
}

already_AddRefed<nsIPrincipal> nsOggDecoder::GetCurrentPrincipal()
{
  if (!mReader)
    return nsnull;
  return mReader->Stream()->GetCurrentPrincipal();
}

void nsOggDecoder::MetadataLoaded()
{
  if (mShuttingDown)
    return;

  // Only inform the element of MetadataLoaded if not doing a load() in order
  // to fulfill a seek, otherwise we'll get multiple metadataloaded events.
  PRBool notifyElement = PR_TRUE;
  {
    nsAutoMonitor mon(mMonitor);
    mDuration = mDecodeStateMachine ? mDecodeStateMachine->GetDuration() : -1;
    notifyElement = mNextState != PLAY_STATE_SEEKING;
  }

  if (mElement && notifyElement) {
    // Make sure the element and the frame (if any) are told about
    // our new size.
    Invalidate();
    mElement->MetadataLoaded();
  }

  if (!mResourceLoaded) {
    StartProgress();
  }
  else if (mElement)
  {
    // Resource was loaded during metadata loading, when progress
    // events are being ignored. Fire the final progress event.
    mElement->DispatchAsyncProgressEvent(NS_LITERAL_STRING("progress"));
  }
 
  // Only inform the element of FirstFrameLoaded if not doing a load() in order
  // to fulfill a seek, otherwise we'll get multiple loadedfirstframe events.
  PRBool resourceIsLoaded = !mResourceLoaded && mReader &&
    mReader->Stream()->IsDataCachedToEndOfStream(mDecoderPosition);
  if (mElement && notifyElement) {
    mElement->FirstFrameLoaded(resourceIsLoaded);
  }

  // The element can run javascript via events
  // before reaching here, so only change the 
  // state if we're still set to the original
  // loading state.
  nsAutoMonitor mon(mMonitor);
  if (mPlayState == PLAY_STATE_LOADING) {
    if (mRequestedSeekTime >= 0.0) {
      ChangeState(PLAY_STATE_SEEKING);
    }
    else {
      ChangeState(mNextState);
    }
  }

  if (resourceIsLoaded) {
    ResourceLoaded();
  }
}

void nsOggDecoder::ResourceLoaded()
{
  // Don't handle ResourceLoaded if we are shutting down, or if
  // we need to ignore progress data due to seeking (in the case
  // that the seek results in reaching end of file, we get a bogus call
  // to ResourceLoaded).
  if (mShuttingDown)
    return;

  {
    // If we are seeking or loading then the resource loaded notification we get
    // should be ignored, since it represents the end of the seek request.
    nsAutoMonitor mon(mMonitor);
    if (mIgnoreProgressData || mResourceLoaded || mPlayState == PLAY_STATE_LOADING)
      return;

    Progress(PR_FALSE);

    mResourceLoaded = PR_TRUE;
    StopProgress();
  }

  // Ensure the final progress event gets fired
  if (mElement) {
    mElement->DispatchAsyncProgressEvent(NS_LITERAL_STRING("progress"));
    mElement->ResourceLoaded();
  }
}

void nsOggDecoder::NetworkError()
{
  if (mShuttingDown)
    return;

  if (mElement)
    mElement->NetworkError();

  Shutdown();
}

PRBool nsOggDecoder::IsSeeking() const
{
  return mPlayState == PLAY_STATE_SEEKING || mNextState == PLAY_STATE_SEEKING;
}

PRBool nsOggDecoder::IsEnded() const
{
  return mPlayState == PLAY_STATE_ENDED || mPlayState == PLAY_STATE_SHUTDOWN;
}

void nsOggDecoder::PlaybackEnded()
{
  if (mShuttingDown || mPlayState == nsOggDecoder::PLAY_STATE_SEEKING)
    return;

  PlaybackPositionChanged();
  ChangeState(PLAY_STATE_ENDED);

  if (mElement)  {
    UpdateReadyStateForData();
    mElement->PlaybackEnded();
  }
}

NS_IMETHODIMP nsOggDecoder::Observe(nsISupports *aSubjet,
                                      const char *aTopic,
                                      const PRUnichar *someData)
{
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    Shutdown();
  }

  return NS_OK;
}

nsMediaDecoder::Statistics
nsOggDecoder::GetStatistics()
{
  Statistics result;

  nsAutoMonitor mon(mMonitor);
  if (mReader) {
    result.mDownloadRate = 
      mReader->Stream()->GetDownloadRate(&result.mDownloadRateReliable);
    result.mDownloadPosition =
      mReader->Stream()->GetCachedDataEnd(mDecoderPosition);
    result.mTotalBytes = mReader->Stream()->GetLength();
    result.mPlaybackRate = ComputePlaybackRate(&result.mPlaybackRateReliable);
    result.mDecoderPosition = mDecoderPosition;
    result.mPlaybackPosition = mPlaybackPosition;
  } else {
    result.mDownloadRate = 0;
    result.mDownloadRateReliable = PR_TRUE;
    result.mPlaybackRate = 0;
    result.mPlaybackRateReliable = PR_TRUE;
    result.mDecoderPosition = 0;
    result.mPlaybackPosition = 0;
    result.mDownloadPosition = 0;
    result.mTotalBytes = 0;
  }

  return result;
}

double nsOggDecoder::ComputePlaybackRate(PRPackedBool* aReliable)
{
  PRInt64 length = mReader ? mReader->Stream()->GetLength() : -1;
  if (mDuration >= 0 && length >= 0) {
    *aReliable = PR_TRUE;
    return double(length)*1000.0/mDuration;
  }
  return mPlaybackStatistics.GetRateAtLastStop(aReliable);
}

void nsOggDecoder::UpdatePlaybackRate()
{
  if (!mReader)
    return;
  PRPackedBool reliable;
  PRUint32 rate = PRUint32(ComputePlaybackRate(&reliable));
  if (reliable) {
    // Avoid passing a zero rate
    rate = PR_MAX(rate, 1);
  } else {
    // Set a minimum rate of 10,000 bytes per second ... sometimes we just
    // don't have good data
    rate = PR_MAX(rate, 10000);
  }
  mReader->Stream()->SetPlaybackRate(rate);
}

void nsOggDecoder::NotifySuspendedStatusChanged()
{
  NS_ASSERTION(NS_IsMainThread(), 
               "nsOggDecoder::NotifyDownloadSuspended called on non-main thread");
  if (!mReader)
    return;
  if (mReader->Stream()->IsSuspendedByCache() && mElement) {
    // if this is an autoplay element, we need to kick off its autoplaying
    // now so we consume data and hopefully free up cache space
    mElement->NotifyAutoplayDataReady();
  }
}

void nsOggDecoder::NotifyBytesDownloaded()
{
  NS_ASSERTION(NS_IsMainThread(),
               "nsOggDecoder::NotifyBytesDownloaded called on non-main thread");   
  UpdateReadyStateForData();
}

void nsOggDecoder::NotifyDownloadEnded(nsresult aStatus)
{
  if (aStatus == NS_BINDING_ABORTED)
    return;

  {
    nsAutoMonitor mon(mMonitor);
    UpdatePlaybackRate();
  }

  if (NS_SUCCEEDED(aStatus)) {
    ResourceLoaded();
  } else if (aStatus != NS_BASE_STREAM_CLOSED) {
    NetworkError();
  }
  UpdateReadyStateForData();
}

void nsOggDecoder::NotifyBytesConsumed(PRInt64 aBytes)
{
  nsAutoMonitor mon(mMonitor);
  if (!mIgnoreProgressData) {
    mDecoderPosition += aBytes;
  }
}

void nsOggDecoder::UpdateReadyStateForData()
{
  if (!mElement || mShuttingDown || !mDecodeStateMachine)
    return;

  nsHTMLMediaElement::NextFrameStatus frameStatus;
  {
    nsAutoMonitor mon(mMonitor);
    if (mDecodeStateMachine->IsBuffering() ||
        mDecodeStateMachine->IsSeeking()) {
      frameStatus = nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE_BUFFERING;
    } else if (mDecodeStateMachine->HaveNextFrameData()) {
      frameStatus = nsHTMLMediaElement::NEXT_FRAME_AVAILABLE;
    } else {
      frameStatus = nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE;
    }
  }
  mElement->UpdateReadyStateForData(frameStatus);
}

void nsOggDecoder::SeekingStopped()
{
  if (mShuttingDown)
    return;

  {
    nsAutoMonitor mon(mMonitor);

    // An additional seek was requested while the current seek was
    // in operation.
    if (mRequestedSeekTime >= 0.0)
      ChangeState(PLAY_STATE_SEEKING);
    else
      ChangeState(mNextState);
  }

  if (mElement) {
    UpdateReadyStateForData();
    mElement->SeekCompleted();
  }
}

// This is called when seeking stopped *and* we're at the end of the
// media.
void nsOggDecoder::SeekingStoppedAtEnd()
{
  if (mShuttingDown)
    return;

  PRBool fireEnded = PR_FALSE;
  {
    nsAutoMonitor mon(mMonitor);

    // An additional seek was requested while the current seek was
    // in operation.
    if (mRequestedSeekTime >= 0.0) {
      ChangeState(PLAY_STATE_SEEKING);
    } else {
      fireEnded = mNextState != PLAY_STATE_PLAYING;
      ChangeState(fireEnded ? PLAY_STATE_ENDED : mNextState);
    }
  }

  if (mElement) {
    UpdateReadyStateForData();
    mElement->SeekCompleted();
    if (fireEnded) {
      mElement->PlaybackEnded();
    }
  }
}

void nsOggDecoder::SeekingStarted()
{
  if (mShuttingDown)
    return;

  if (mElement) {
    UpdateReadyStateForData();
    mElement->SeekStarted();
  }
}

void nsOggDecoder::RegisterShutdownObserver()
{
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1");
  if (observerService) {
    observerService->AddObserver(this, 
                                 NS_XPCOM_SHUTDOWN_OBSERVER_ID, 
                                 PR_FALSE);
  } else {
    NS_WARNING("Could not get an observer service. Video decoding events may not shutdown cleanly.");
  }
}

void nsOggDecoder::UnregisterShutdownObserver()
{
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1");
  if (observerService) {
    observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  }
}

void nsOggDecoder::ChangeState(PlayState aState)
{
  NS_ASSERTION(NS_IsMainThread(), 
               "nsOggDecoder::ChangeState called on non-main thread");   
  nsAutoMonitor mon(mMonitor);

  if (mNextState == aState) {
    mNextState = PLAY_STATE_PAUSED;
  }

  if (mPlayState == PLAY_STATE_SHUTDOWN) {
    mon.NotifyAll();
    return;
  }

  mPlayState = aState;
  switch (aState) {
  case PLAY_STATE_PAUSED:
    /* No action needed */
    break;
  case PLAY_STATE_PLAYING:
    mDecodeStateMachine->Decode();
    break;
  case PLAY_STATE_SEEKING:
    mDecodeStateMachine->Seek(mRequestedSeekTime);
    mRequestedSeekTime = -1.0;
    break;
  case PLAY_STATE_LOADING:
    /* No action needed */
    break;
  case PLAY_STATE_START:
    /* No action needed */
    break;
  case PLAY_STATE_ENDED:
    /* No action needed */
    break;
  case PLAY_STATE_SHUTDOWN:
    /* No action needed */
    break;
  }
  mon.NotifyAll();
}

void nsOggDecoder::PlaybackPositionChanged()
{
  if (mShuttingDown)
    return;

  float lastTime = mCurrentTime;

  // Control the scope of the monitor so it is not
  // held while the timeupdate and the invalidate is run.
  {
    nsAutoMonitor mon(mMonitor);

    if (mDecodeStateMachine) {
      mCurrentTime = mDecodeStateMachine->GetCurrentTime();
      mDecodeStateMachine->ClearPositionChangeFlag();
    }
  }

  // Invalidate the frame so any video data is displayed.
  // Do this before the timeupdate event so that if that
  // event runs JavaScript that queries the media size, the
  // frame has reflowed and the size updated beforehand.
  Invalidate();

  if (mElement && lastTime != mCurrentTime) {
    mElement->DispatchSimpleEvent(NS_LITERAL_STRING("timeupdate"));
  }
}

void nsOggDecoder::SetDuration(PRInt64 aDuration)
{
  mDuration = aDuration;
  if (mDecodeStateMachine) {
    nsAutoMonitor mon(mMonitor);
    mDecodeStateMachine->SetDuration(mDuration);
    UpdatePlaybackRate();
  }
}

void nsOggDecoder::SetSeekable(PRBool aSeekable)
{
  mSeekable = aSeekable;
  if (mDecodeStateMachine) {
    nsAutoMonitor mon(mMonitor);
    mDecodeStateMachine->SetSeekable(aSeekable);
  }
}

PRBool nsOggDecoder::GetSeekable()
{
  return mSeekable;
}

void nsOggDecoder::Suspend()
{
  if (mReader) {
    mReader->Stream()->Suspend(PR_TRUE);
  }
}

void nsOggDecoder::Resume()
{
  if (mReader) {
    mReader->Stream()->Resume();
  }
}

void nsOggDecoder::StopProgressUpdates()
{
  mIgnoreProgressData = PR_TRUE;
  if (mReader) {
    mReader->Stream()->SetReadMode(nsMediaCacheStream::MODE_METADATA);
  }
}

void nsOggDecoder::StartProgressUpdates()
{
  mIgnoreProgressData = PR_FALSE;
  if (mReader) {
    mReader->Stream()->SetReadMode(nsMediaCacheStream::MODE_PLAYBACK);
    mDecoderPosition = mPlaybackPosition = mReader->Stream()->Tell();
  }
}

void nsOggDecoder::MoveLoadsToBackground()
{
  if (mReader && mReader->Stream()) {
    mReader->Stream()->MoveLoadsToBackground();
  }
}

