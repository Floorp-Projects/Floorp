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
public:
  // Object to hold the decoded data from a frame
  class FrameData {
  public:
    FrameData() :
      mVideoWidth(0),
      mVideoHeight(0),
      mDecodedFrameTime(0.0),
      mTime(0.0)
    {
      MOZ_COUNT_CTOR(FrameData);
    }

    ~FrameData()
    {
      MOZ_COUNT_DTOR(FrameData);
    }


    nsAutoArrayPtr<unsigned char> mVideoData;
    nsTArray<float> mAudioData;
    int mVideoWidth;
    int mVideoHeight;
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
      mEmpty(PR_TRUE)
    {
    }

    void Push(FrameData* frame)
    {
      NS_ASSERTION(!IsFull(), "FrameQueue is full");
      mQueue[mTail] = frame;
      mTail = (mTail+1) % OGGPLAY_BUFFER_SIZE;
      mEmpty = PR_FALSE;
    }

    FrameData* Peek()
    {
      NS_ASSERTION(!mEmpty, "FrameQueue is empty");

      return mQueue[mHead];
    }

    FrameData* Pop()
    {
      NS_ASSERTION(!mEmpty, "FrameQueue is empty");

      FrameData* result = mQueue[mHead];
      mHead = (mHead + 1) % OGGPLAY_BUFFER_SIZE;
      mEmpty = mHead == mTail;
      return result;
    }

    PRBool IsEmpty()
    {
      return mEmpty;
    }

    PRBool IsFull()
    {
      return !mEmpty && mHead == mTail;
    }

  private:
    FrameData* mQueue[OGGPLAY_BUFFER_SIZE];
    PRInt32 mHead;
    PRInt32 mTail;
    PRPackedBool mEmpty;
  };

  // Enumeration for the valid states
  enum State {
    DECODER_STATE_DECODING_METADATA,
    DECODER_STATE_DECODING_FIRSTFRAME,
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

  // Play the audio data from the given frame. The decode monitor must
  // be locked when calling this method. Returns PR_FALSE if unable to
  // write to the audio device without blocking.
  PRBool PlayAudio(FrameData* aFrame);

  // Called from the main thread to get the current frame time. The decoder
  // monitor must be obtained before calling this.
  float GetCurrentTime();

  // Called from the main thread to get the duration. The decoder monitor
  // must be obtained before calling this. It is in units of milliseconds.
  PRInt64 GetDuration();

  // Called from the main thread to set the content length of the media
  // resource. The decoder monitor must be obtained before calling this.
  void SetContentLength(PRInt64 aLength);

  // Called from the main thread to set whether the media resource can
  // be seeked. The decoder monitor must be obtained before calling this.
  void SetSeekable(PRBool aSeekable);

  // Get and set the audio volume. The decoder monitor must be
  // obtained before calling this.
  float GetVolume();
  void SetVolume(float aVolume);

  // Clear the flag indicating that a playback position change event
  // is currently queued. This is called from the main thread and must
  // be called with the decode monitor held.
  void ClearPositionChangeFlag();

protected:
  // Convert the OggPlay frame information into a format used by Gecko
  // (RGB for video, float for sound, etc).The decoder monitor must be
  // acquired in the scope of calls to these functions. They must be
  // called only when the current state > DECODING_METADATA.
  void HandleVideoData(FrameData* aFrame, int aTrackNum, OggPlayVideoData* aVideoData);
  void HandleAudioData(FrameData* aFrame, OggPlayAudioData* aAudioData, int aSize);

  // These methods can only be called on the decoding thread.
  void LoadOggHeaders();

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
  void StartPlayback();

  // Stop playback of media. Must be called with the decode monitor held.
  void StopPlayback();

  // Update the playback position. This can result in a timeupdate event
  // and an invalidate of the frame being dispatched asynchronously if
  // there is no such event currently queued.
  // Only called on the decoder thread. Must be called with
  // the decode monitor held.
  void UpdatePlaybackPosition(float aTime);

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
  // current frame and the previous frame. Accessed only via the
  // decoder thread.
  FrameQueue mDecodedFrames;

  // The time that playback started from the system clock. This is used
  // for synchronising frames.  It is reset after a seek as the mTime member
  // of FrameData is reset to start at 0 from the first frame after a seek.
  // Accessed only via the decoder thread.
  PRIntervalTime mPlayStartTime;

  // The time that playback was most recently paused, either via
  // buffering or pause. This is used to compute mPauseDuration for
  // a/v sync adjustments.  Accessed only via the decoder thread.
  PRIntervalTime mPauseStartTime;

  // The total time that has been spent in completed pauses (via
  // 'pause' or buffering). This is used to adjust for these
  // pauses when computing a/v synchronisation. Accessed only via the
  // decoder thread.
  PRIntervalTime mPauseDuration;

  // PR_TRUE if the media is playing and the decoder has started
  // the sound and adjusted the sync time for pauses. PR_FALSE
  // if the media is paused and the decoder has stopped the sound
  // and adjusted the sync time for pauses. Accessed only via the
  // decoder thread.
  PRPackedBool mPlaying;

  // Number of seconds of data video/audio data held in a frame.
  // Accessed only via the decoder thread.
  float mCallbackPeriod;

  // Video data. These are initially set when the metadata is loaded.
  // They are only accessed from the decoder thread.
  PRInt32 mVideoTrack;
  float   mFramerate;

  // Audio data. These are initially set when the metadata is loaded.
  // They are only accessed from the decoder thread.
  PRInt32 mAudioRate;
  PRInt32 mAudioChannels;
  PRInt32 mAudioTrack;

  // Time that buffering started. Used for buffering timeout and only
  // accessed in the decoder thread.
  PRIntervalTime mBufferingStart;

  // Number of bytes to buffer when buffering. Only accessed in the
  // decoder thread.
  PRUint32 mBufferingBytes;

  // The time value of the last decoded video frame. Used for
  // computing the sleep period between frames for a/v sync.
  // Read/Write from the decode thread only.
  float mLastFrameTime;

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
  // main thread (Via the Get/SetVolume calls). Synchronisation via
  // mDecoder monitor.
  nsAutoPtr<nsAudioStream> mAudioStream;

  // The time of the current frame in seconds. This is referenced from
  // 0.0 which is the initial start of the stream. Set by the decode
  // thread, and read-only from the main thread to get the current
  // time value. Synchronised via decoder monitor.
  float mCurrentFrameTime;

  // Volume of playback. 0.0 = muted. 1.0 = full volume. Read/Written
  // from the decode and main threads. Synchronised via decoder
  // monitor.
  float mVolume;

  // Duration of the media resource. It is accessed from the decoder and main
  // threads. Synchronised via decoder monitor. It is in units of
  // milliseconds.
  PRInt64 mDuration;

  // Content Length of the media resource if known. If it is -1 then the
  // size is unknown. Accessed from the decoder and main threads. Synchronised
  // via decoder monitor.
  PRInt64 mContentLength;

  // PR_TRUE if the media resource can be seeked. Accessed from the decoder
  // and main threads. Synchronised via decoder monitor.
  PRPackedBool mSeekable;

  // PR_TRUE if an event to notify about a change in the playback
  // position has been queued, but not yet run. It is set to PR_FALSE when
  // the event is run. This allows coalescing of these events as they can be
  // produced many times per second. Synchronised via decoder monitor.
  PRPackedBool mPositionChangeQueued;
};

nsOggDecodeStateMachine::nsOggDecodeStateMachine(nsOggDecoder* aDecoder) :
  mDecoder(aDecoder),
  mPlayer(0),
  mPlayStartTime(0),
  mPauseStartTime(0),
  mPauseDuration(0),
  mPlaying(PR_FALSE),
  mCallbackPeriod(1.0),
  mVideoTrack(-1),
  mFramerate(0.0),
  mAudioRate(0),
  mAudioChannels(0),
  mAudioTrack(-1),
  mBufferingStart(0),
  mBufferingBytes(0),
  mLastFrameTime(0),
  mState(DECODER_STATE_DECODING_METADATA),
  mSeekTime(0.0),
  mCurrentFrameTime(0.0),
  mVolume(1.0),
  mDuration(-1),
  mContentLength(-1),
  mSeekable(PR_TRUE),
  mPositionChangeQueued(PR_FALSE)
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
  NS_ASSERTION(mState > DECODER_STATE_DECODING_METADATA, "DecodeFrame() called during invalid state");
  return oggplay_step_decoding(mPlayer);
}

nsOggDecodeStateMachine::FrameData* nsOggDecodeStateMachine::NextFrame()
{
  NS_ASSERTION(mState > DECODER_STATE_DECODING_METADATA, "NextFrame() called during invalid state");
  OggPlayCallbackInfo** info = oggplay_buffer_retrieve_next(mPlayer);
  if (!info)
    return nsnull;

  FrameData* frame = new FrameData();
  if (!frame) {
    return nsnull;
  }

  frame->mTime = mLastFrameTime;
  mLastFrameTime += mCallbackPeriod;
  int num_tracks = oggplay_get_num_tracks(mPlayer);
  float audioTime = 0.0;
  float videoTime = 0.0;

  if (mVideoTrack != -1 &&
      num_tracks > mVideoTrack &&
      oggplay_callback_info_get_type(info[mVideoTrack]) == OGGPLAY_YUV_VIDEO) {
    OggPlayDataHeader** headers = oggplay_callback_info_get_headers(info[mVideoTrack]);
    videoTime = ((float)oggplay_callback_info_get_presentation_time(headers[0]))/1000.0;
    HandleVideoData(frame, mVideoTrack, oggplay_callback_info_get_video_data(headers[0]));
  }

  if (mAudioTrack != -1 &&
      num_tracks > mAudioTrack &&
      oggplay_callback_info_get_type(info[mAudioTrack]) == OGGPLAY_FLOATS_AUDIO) {
    OggPlayDataHeader** headers = oggplay_callback_info_get_headers(info[mAudioTrack]);
    audioTime = ((float)oggplay_callback_info_get_presentation_time(headers[0]))/1000.0;
    int required = oggplay_callback_info_get_required(info[mAudioTrack]);
    for (int j = 0; j < required; ++j) {
      int size = oggplay_callback_info_get_record_size(headers[j]);
      OggPlayAudioData* audio_data = oggplay_callback_info_get_audio_data(headers[j]);
      HandleAudioData(frame, audio_data, size);
    }
  }

  // Pick one stream to act as the reference track to indicate if the
  // stream has ended, seeked, etc.
  if (mVideoTrack >= 0 )
    frame->mState = oggplay_callback_info_get_stream_info(info[mVideoTrack]);
  else if (mAudioTrack >= 0)
    frame->mState = oggplay_callback_info_get_stream_info(info[mAudioTrack]);
  else
    frame->mState = OGGPLAY_STREAM_UNINITIALISED;

  frame->mDecodedFrameTime = mVideoTrack == -1 ? audioTime : videoTime;

  oggplay_buffer_release(mPlayer, info);
  return frame;
}

void nsOggDecodeStateMachine::HandleVideoData(FrameData* aFrame, int aTrackNum, OggPlayVideoData* aVideoData) {
  if (!aVideoData)
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
  aFrame->mVideoData = new unsigned char[y_width * y_height * 4];
  if (!aFrame->mVideoData) {
    return;
  }

  OggPlayYUVChannels yuv;
  OggPlayRGBChannels rgb;
      
  yuv.ptry = aVideoData->y;
  yuv.ptru = aVideoData->u;
  yuv.ptrv = aVideoData->v;
  yuv.uv_width = uv_width;
  yuv.uv_height = uv_height;
  yuv.y_width = y_width;
  yuv.y_height = y_height;
      
  rgb.ptro = aFrame->mVideoData;
  rgb.rgb_width = aFrame->mVideoWidth;
  rgb.rgb_height = aFrame->mVideoHeight;

  oggplay_yuv2bgr(&yuv, &rgb);
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
      StartPlayback();
    }

    if (!mDecodedFrames.IsEmpty()) {
      FrameData* frame = mDecodedFrames.Peek();
      if (frame->mState == OGGPLAY_STREAM_JUST_SEEKED) {
        // After returning from a seek all mTime members of
        // FrameData start again from a time position of 0.
        // Reset the play start time.
        mPlayStartTime = PR_IntervalNow();
        mPauseDuration = 0;
      }

      double time = (PR_IntervalToMilliseconds(PR_IntervalNow()-mPlayStartTime-mPauseDuration)/1000.0);
      if (time >= frame->mTime) {
        mDecodedFrames.Pop();
        // Audio for the current frame is played, but video for the next frame
        // is displayed, to account for lag from the time the audio is written
        // to when it is played. This will go away when we move to a/v sync
        // using the audio hardware clock.
        PlayVideo(mDecodedFrames.IsEmpty() ? frame : mDecodedFrames.Peek());
        PlayAudio(frame);
        UpdatePlaybackPosition(frame->mDecodedFrameTime);
        delete frame;
      }
      else {
        // If the queue of decoded frame is full then we wait for the
        // approximate time until the next frame. 
        if (mDecodedFrames.IsFull()) {
          mon.Wait(PR_MillisecondsToInterval(PRInt64((frame->mTime - time)*1000)));
          if (mState == DECODER_STATE_SHUTDOWN) {
            return;
          }
        }
      }
    }
  }
  else {
    if (mPlaying) {
      StopPlayback();
    }

    if (mDecodedFrames.IsFull() && mState == DECODER_STATE_DECODING) {
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
  if (aFrame) {
    if (aFrame->mVideoData) {
      nsAutoLock lock(mDecoder->mVideoUpdateLock);

      mDecoder->SetRGBData(aFrame->mVideoWidth, aFrame->mVideoHeight, mFramerate, aFrame->mVideoData);
    }
  }
}

PRBool nsOggDecodeStateMachine::PlayAudio(FrameData* aFrame)
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "PlayAudio() called without acquiring decoder monitor");
  if (mAudioStream && aFrame && !aFrame->mAudioData.IsEmpty()) {
    if (PRUint32(mAudioStream->Available()) < aFrame->mAudioData.Length())
      return PR_FALSE;

    mAudioStream->Write(aFrame->mAudioData.Elements(), aFrame->mAudioData.Length());
  }

  return PR_TRUE;
}

void nsOggDecodeStateMachine::OpenAudioStream()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "OpenAudioStream() called without acquiring decoder monitor");
  mAudioStream = new nsAudioStream();
  if (!mAudioStream) {
    LOG(PR_LOG_ERROR, ("Could not create audio stream"));
  }
  else {
    mAudioStream->Init(mAudioChannels, mAudioRate, nsAudioStream::FORMAT_FLOAT32_LE);
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
  if (mPlayStartTime == 0) {
    mPlayStartTime = PR_IntervalNow();
  }

  // If we have been paused previously, then compute duration spent paused
  if (mPauseStartTime != 0) {
    mPauseDuration += PR_IntervalNow() - mPauseStartTime;
  }
}

void nsOggDecodeStateMachine::StopPlayback()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "StopPlayback() called without acquiring decoder monitor");
  StopAudio();
  mPlaying = PR_FALSE;
  mPauseStartTime = PR_IntervalNow();
}

void nsOggDecodeStateMachine::UpdatePlaybackPosition(float aTime)
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "UpdatePlaybackPosition() called without acquiring decoder monitor");
  mCurrentFrameTime = aTime;
  if (!mPositionChangeQueued) {
    mPositionChangeQueued = PR_TRUE;
    nsCOMPtr<nsIRunnable> event =
      NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, PlaybackPositionChanged);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }
}

void nsOggDecodeStateMachine::ClearPositionChangeFlag()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "ClearPositionChangeFlag() called without acquiring decoder monitor");
  mPositionChangeQueued = PR_FALSE;
}

float nsOggDecodeStateMachine::GetVolume()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "GetVolume() called without acquiring decoder monitor");
  return mVolume;
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

void nsOggDecodeStateMachine::SetContentLength(PRInt64 aLength)
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "SetContentLength() called without acquiring decoder monitor");
  mContentLength = aLength;
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
  if (mPlayer) {
    oggplay_prepare_for_close(mPlayer);
  }
  mState = DECODER_STATE_SHUTDOWN;
  mon.NotifyAll();
}

void nsOggDecodeStateMachine::Decode()
{
  // When asked to decode, switch to decoding only if
  // we are currently buffering.
  nsAutoMonitor mon(mDecoder->GetMonitor());
  if (mState == DECODER_STATE_BUFFERING) {
    mState = DECODER_STATE_DECODING;
  }
}

void nsOggDecodeStateMachine::Seek(float aTime)
{
  nsAutoMonitor mon(mDecoder->GetMonitor());
  mSeekTime = aTime;
  mState = DECODER_STATE_SEEKING;
}

nsresult nsOggDecodeStateMachine::Run()
{
  nsChannelReader* reader = mDecoder->GetReader();
  NS_ENSURE_TRUE(reader, NS_ERROR_NULL_POINTER);
  while (PR_TRUE) {
   nsAutoMonitor mon(mDecoder->GetMonitor());
   switch(mState) {
    case DECODER_STATE_SHUTDOWN:
      return NS_OK;

    case DECODER_STATE_DECODING_METADATA:
      mon.Exit();
      LoadOggHeaders();
      mon.Enter();
      
      if (mState == DECODER_STATE_DECODING_METADATA) {
        mState = DECODER_STATE_DECODING_FIRSTFRAME;
      }
      break;

    case DECODER_STATE_DECODING_FIRSTFRAME:
      {
        OggPlayErrorCode r;
        do {
          mon.Exit();
          r = DecodeFrame();
          mon.Enter();
        } while (mState != DECODER_STATE_SHUTDOWN && r == E_OGGPLAY_TIMEOUT);

        if (mState == DECODER_STATE_SHUTDOWN)
          continue;

        mLastFrameTime = 0;
        FrameData* frame = NextFrame();
        if (frame) {
          mDecodedFrames.Push(frame);
          UpdatePlaybackPosition(frame->mDecodedFrameTime);
          PlayVideo(frame);
        }

        nsCOMPtr<nsIRunnable> event =
          NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, FirstFrameLoaded);
        NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);

        if (mState == DECODER_STATE_DECODING_FIRSTFRAME) {
          mState = DECODER_STATE_DECODING;
        }
      }
      break;

    case DECODER_STATE_DECODING:
      {
        // Before decoding check if we should buffer more data
        if (reader->DownloadRate() >= 0 &&
            reader->Available() < reader->PlaybackRate() * BUFFERING_SECONDS_LOW_WATER_MARK) {
          if (mDecoder->GetState() == nsOggDecoder::PLAY_STATE_PLAYING) {
            if (mPlaying) {
              StopPlayback();
            }
          }

          mBufferingStart = PR_IntervalNow();
          mBufferingBytes = PRUint32(BUFFERING_RATE(reader->PlaybackRate()) * BUFFERING_WAIT);
          mState = DECODER_STATE_BUFFERING;

          nsCOMPtr<nsIRunnable> event =
            NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, BufferingStarted);
          NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
        }
        else {
          if (!mDecodedFrames.IsFull()) {
            mon.Exit();
            OggPlayErrorCode r = DecodeFrame();
            mon.Enter();

            if (mState != DECODER_STATE_DECODING)
              continue;

            // Get the decoded frame and store it in our queue of decoded frames
            FrameData* frame = NextFrame();
            if (frame) {
              mDecodedFrames.Push(frame);
            }

            if (r != E_OGGPLAY_CONTINUE &&
                r != E_OGGPLAY_USER_INTERRUPT &&
                r != E_OGGPLAY_TIMEOUT)  {
              mState = DECODER_STATE_COMPLETED;
            }
          }

          // Show at least the first frame if we're not playing
          // so we have a poster frame on initial load and after seek.
          if (!mPlaying && !mDecodedFrames.IsEmpty()) {
            PlayVideo(mDecodedFrames.Peek());
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
        float seekTime = mSeekTime;
        mon.Exit();
        nsCOMPtr<nsIRunnable> startEvent = 
          NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, SeekingStarted);
        NS_DispatchToMainThread(startEvent, NS_DISPATCH_SYNC);
        
        oggplay_seek(mPlayer, ogg_int64_t(seekTime * 1000));

        mon.Enter();
        if (mState == DECODER_STATE_SHUTDOWN)
          continue;

        // Remove all frames decoded prior to seek from the queue
        while (!mDecodedFrames.IsEmpty()) {
          delete mDecodedFrames.Pop();
        }

        OggPlayErrorCode r;
        do {
          mon.Exit();
          r = DecodeFrame();
          mon.Enter();
        } while (mState != DECODER_STATE_SHUTDOWN && r == E_OGGPLAY_TIMEOUT);

        if (mState == DECODER_STATE_SHUTDOWN)
          continue;

        mLastFrameTime = 0;
        FrameData* frame = NextFrame();
        if (frame) {
          mDecodedFrames.Push(frame);
          UpdatePlaybackPosition(frame->mDecodedFrameTime);
          PlayVideo(frame);
        }
        mon.Exit();
        nsCOMPtr<nsIRunnable> stopEvent = 
          NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, SeekingStopped);
        NS_DispatchToMainThread(stopEvent, NS_DISPATCH_SYNC);        
        mon.Enter();

        if (mState == DECODER_STATE_SEEKING && mSeekTime == seekTime) {
          mState = DECODER_STATE_DECODING;
        }
      }
      break;

    case DECODER_STATE_BUFFERING:
      if ((PR_IntervalToMilliseconds(PR_IntervalNow() - mBufferingStart) < BUFFERING_WAIT*1000) &&
          reader->DownloadRate() >= 0 &&            
          reader->Available() < mBufferingBytes) {
        LOG(PR_LOG_DEBUG, 
            ("Buffering data until %d bytes available or %d milliseconds", 
             mBufferingBytes - reader->Available(),
             BUFFERING_WAIT*1000 - (PR_IntervalToMilliseconds(PR_IntervalNow() - mBufferingStart))));
        mon.Wait(PR_MillisecondsToInterval(1000));
        if (mState == DECODER_STATE_SHUTDOWN)
          continue;
      }
      else {
        mState = DECODER_STATE_DECODING;
      }

      if (mState != DECODER_STATE_BUFFERING) {
        nsCOMPtr<nsIRunnable> event = 
          NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, BufferingStopped);
        NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
        if (mDecoder->GetState() == nsOggDecoder::PLAY_STATE_PLAYING) {
          if (!mPlaying) {
            StartPlayback();
          }
        }
      }

      break;

    case DECODER_STATE_COMPLETED:
      {
        while (mState == DECODER_STATE_COMPLETED &&
               !mDecodedFrames.IsEmpty()) {
          PlayFrame();
          if (mState != DECODER_STATE_SHUTDOWN) {
            // Wait for the time of one frame so we don't tight loop
            // and we need to release the monitor so timeupdate and
            // invalidate's on the main thread can occur.
            mon.Wait(PR_MillisecondsToInterval(PRInt64(mCallbackPeriod*1000)));
          }
        }

        if (mState != DECODER_STATE_COMPLETED)
          continue;

        nsCOMPtr<nsIRunnable> event =
          NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, PlaybackEnded);
        NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
        do {
          mon.Wait();
        } while (mState != DECODER_STATE_SHUTDOWN);
      }
      break;
    }
  }

  return NS_OK;
}

void nsOggDecodeStateMachine::LoadOggHeaders() 
{
  LOG(PR_LOG_DEBUG, ("Loading Ogg Headers"));
  mPlayer = oggplay_open_with_reader(mDecoder->GetReader());
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
      }
      else if (mAudioTrack == -1 && oggplay_get_track_type(mPlayer, i) == OGGZ_CONTENT_VORBIS) {
        mAudioTrack = i;
        oggplay_set_offset(mPlayer, i, OGGPLAY_AUDIO_OFFSET);
        oggplay_get_audio_samplerate(mPlayer, i, &mAudioRate);
        oggplay_get_audio_channels(mPlayer, i, &mAudioChannels);
        LOG(PR_LOG_DEBUG, ("samplerate: %d, channels: %d", mAudioRate, mAudioChannels));
      }
 
      if (oggplay_set_track_active(mPlayer, i) < 0)  {
        LOG(PR_LOG_ERROR, ("Could not set track %d active", i));
      }
    }

    if (mVideoTrack == -1) {
      oggplay_set_callback_num_frames(mPlayer, mAudioTrack, OGGPLAY_FRAMES_PER_CALLBACK);
      mCallbackPeriod = 1.0 / (float(mAudioRate) / OGGPLAY_FRAMES_PER_CALLBACK);
    }
    LOG(PR_LOG_DEBUG, ("Callback Period: %f", mCallbackPeriod));

    oggplay_use_buffer(mPlayer, OGGPLAY_BUFFER_SIZE);

    // Get the duration from the Ogg file. We only do this if the
    // content length of the resource is known as we need to seek
    // to the end of the file to get the last time field. We also
    // only do this if the resource is seekable.
    {
      nsAutoMonitor mon(mDecoder->GetMonitor());
      if (mState != DECODER_STATE_SHUTDOWN &&
          mContentLength >= 0 && 
          mSeekable) {
        // Don't hold the monitor during the duration
        // call as it can issue seek requests
        // and blocks until these are completed.
        mon.Exit();
        PRInt64 d = oggplay_get_duration(mPlayer);
        mon.Enter();
        mDuration = d;
      }
      if (mState == DECODER_STATE_SHUTDOWN)
        return;
    }

    // Inform the element that we've loaded the Ogg metadata
    nsCOMPtr<nsIRunnable> metadataLoadedEvent = 
      NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, MetadataLoaded); 
    
    NS_DispatchToMainThread(metadataLoadedEvent, NS_DISPATCH_NORMAL);
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsOggDecoder, nsIObserver)

void nsOggDecoder::Pause() 
{
  nsAutoMonitor mon(mMonitor);
  if (mPlayState == PLAY_STATE_SEEKING) {
    mNextState = PLAY_STATE_PAUSED;
    return;
  }

  ChangeState(PLAY_STATE_PAUSED);
}

float nsOggDecoder::GetVolume()
{
  nsAutoMonitor mon(mMonitor);
  return mDecodeStateMachine ? mDecodeStateMachine->GetVolume() : mInitialVolume;
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
  mBytesDownloaded(0),
  mCurrentTime(0.0),
  mInitialVolume(0.0),
  mRequestedSeekTime(-1.0),
  mContentLength(-1),
  mNotifyOnShutdown(PR_FALSE),
  mSeekable(PR_TRUE),
  mReader(0),
  mMonitor(0),
  mPlayState(PLAY_STATE_PAUSED),
  mNextState(PLAY_STATE_PAUSED)
{
  MOZ_COUNT_CTOR(nsOggDecoder);
}

PRBool nsOggDecoder::Init() 
{
  mMonitor = nsAutoMonitor::NewMonitor("media.decoder");
  return mMonitor && nsMediaDecoder::Init();
}

// An event that gets posted to the main thread, when the media
// element is being destroyed, to destroy the decoder. Since the
// decoder shutdown can block and post events this cannot be done
// inside destructor calls. So this event is posted asynchronously to
// the main thread to perform the shutdown. It keeps a strong
// reference to the decoder to ensure it does not get deleted when the
// element is deleted.
class nsOggDecoderShutdown : public nsRunnable
{
public:
  nsOggDecoderShutdown(nsOggDecoder* aDecoder) :
    mDecoder(aDecoder)
  {
  }

  NS_IMETHOD Run()
  {
    mDecoder->Stop();
    return NS_OK;
  }

private:
  nsRefPtr<nsOggDecoder> mDecoder;
};


void nsOggDecoder::Shutdown() 
{
  mShuttingDown = PR_TRUE;

  ChangeState(PLAY_STATE_SHUTDOWN);
  nsMediaDecoder::Shutdown();

  nsCOMPtr<nsIRunnable> event = new nsOggDecoderShutdown(this);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}

nsOggDecoder::~nsOggDecoder()
{
  MOZ_COUNT_DTOR(nsOggDecoder);
  nsAutoMonitor::DestroyMonitor(mMonitor);
}

nsresult nsOggDecoder::Load(nsIURI* aURI, nsIChannel* aChannel,
                            nsIStreamListener** aStreamListener)
{
  // Reset Stop guard flag flag, else shutdown won't occur properly when
  // reusing decoder.
  mStopping = PR_FALSE;

  if (aStreamListener) {
    *aStreamListener = nsnull;
  }

  if (aURI) {
    NS_ASSERTION(!aStreamListener, "No listener should be requested here");
    mURI = aURI;
  } else {
    NS_ASSERTION(aChannel, "Either a URI or a channel is required");
    NS_ASSERTION(aStreamListener, "A listener should be requested here");

    // If the channel was redirected, we want the post-redirect URI;
    // but if the URI scheme was expanded, say from chrome: to jar:file:,
    // we want the original URI.
    nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(mURI));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  StartProgress();

  RegisterShutdownObserver();

  mReader = new nsChannelReader();
  NS_ENSURE_TRUE(mReader, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = mReader->Init(this, mURI, aChannel, aStreamListener);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewThread(getter_AddRefs(mDecodeThread));
  NS_ENSURE_SUCCESS(rv, rv);

  mDecodeStateMachine = new nsOggDecodeStateMachine(this);
  {
    nsAutoMonitor mon(mMonitor);
    mDecodeStateMachine->SetContentLength(mContentLength);
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

  ChangeState(PLAY_STATE_PLAYING);

  return NS_OK;
}

nsresult nsOggDecoder::Seek(float aTime)
{
  nsAutoMonitor mon(mMonitor);

  if (aTime < 0.0)
    return NS_ERROR_FAILURE;

  if (mPlayState == PLAY_STATE_LOADING && aTime == 0.0) {
    return NS_OK;
  }

  mRequestedSeekTime = aTime;

  // If we are already in the seeking state, then setting mRequestedSeekTime
  // above will result in the new seek occurring when the current seek
  // completes.
  if (mPlayState != PLAY_STATE_SEEKING) {
    mNextState = mPlayState;
    ChangeState(PLAY_STATE_SEEKING);
  }

  return NS_OK;
}

nsresult nsOggDecoder::PlaybackRateChanged()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void nsOggDecoder::Stop()
{
  NS_ASSERTION(NS_IsMainThread(), 
               "nsOggDecoder::Stop called on non-main thread");  
  
  if (mStopping)
    return;

  mStopping = PR_TRUE;

  ChangeState(PLAY_STATE_ENDED);

  StopProgress();

  // Force any outstanding seek and byterange requests to complete
  // to prevent shutdown from deadlocking.
  if (mReader) {
    mReader->Cancel();
  }

  // Shutdown must be on called the mDecodeStateMachine before deleting.
  // This is required to ensure that the state machine isn't running
  // in the thread and using internal objects when it is deleted.
  if (mDecodeStateMachine) {
    mDecodeStateMachine->Shutdown();
  }

  // The state machines must be Shutdown() before the thread is
  // Shutdown. The Shutdown() on the state machine unblocks any
  // blocking calls preventing the thread Shutdown from deadlocking.
  if (mDecodeThread) {
    mDecodeThread->Shutdown();
    mDecodeThread = nsnull;
  }

  mDecodeStateMachine = nsnull;
  mReader = nsnull;
  UnregisterShutdownObserver();
}

float nsOggDecoder::GetCurrentTime()
{
  return mCurrentTime;
}

void nsOggDecoder::GetCurrentURI(nsIURI** aURI)
{
  NS_IF_ADDREF(*aURI = mURI);
}

nsIPrincipal* nsOggDecoder::GetCurrentPrincipal()
{
  if (!mReader) {
    return nsnull;
  }

  return mReader->GetCurrentPrincipal();
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
    mElement->MetadataLoaded();
  }
}

void nsOggDecoder::FirstFrameLoaded()
{
  if (mShuttingDown)
    return;
 
  // Only inform the element of FirstFrameLoaded if not doing a load() in order
  // to fulfill a seek, otherwise we'll get multiple loadedfirstframe events.
  PRBool notifyElement = PR_TRUE;
  {
    nsAutoMonitor mon(mMonitor);
    notifyElement = mNextState != PLAY_STATE_SEEKING;
  }  

  if (mElement && notifyElement) {
    mElement->FirstFrameLoaded();
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
}

void nsOggDecoder::ResourceLoaded()
{
  if (mShuttingDown)
    return;

  if (mElement) {
    mElement->ResourceLoaded();
  }
  StopProgress();
}

void nsOggDecoder::NetworkError()
{
  if (mShuttingDown)
    return;

  if (mElement)
    mElement->NetworkError();
  Stop();
}

PRBool nsOggDecoder::IsSeeking() const
{
  return mPlayState == PLAY_STATE_SEEKING;
}

void nsOggDecoder::PlaybackEnded()
{
  if (mShuttingDown)
    return;

  Stop();
  if (mElement)  {
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

PRUint64 nsOggDecoder::GetBytesLoaded()
{
  return mBytesDownloaded;
}

PRInt64 nsOggDecoder::GetTotalBytes()
{
  return mContentLength;
}

void nsOggDecoder::SetTotalBytes(PRInt64 aBytes)
{
  mContentLength = aBytes;
  if (mDecodeStateMachine) {
    nsAutoMonitor mon(mMonitor);
    mDecodeStateMachine->SetContentLength(aBytes);
  } 
}

void nsOggDecoder::UpdateBytesDownloaded(PRUint64 aBytes)
{
  mBytesDownloaded = aBytes;
}

void nsOggDecoder::BufferingStopped()
{
  if (mShuttingDown)
    return;

  if (mElement) {
    mElement->ChangeReadyState(nsIDOMHTMLMediaElement::CAN_SHOW_CURRENT_FRAME);
  }
}

void nsOggDecoder::BufferingStarted()
{
  if (mShuttingDown)
    return;

  if (mElement) {
    mElement->ChangeReadyState(nsIDOMHTMLMediaElement::DATA_UNAVAILABLE);
  }
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
    mElement->SeekCompleted();
  }
}

void nsOggDecoder::SeekingStarted()
{
  if (mShuttingDown)
    return;

  if (mElement) {
    mElement->SeekStarted();
  }
}

void nsOggDecoder::RegisterShutdownObserver()
{
  if (!mNotifyOnShutdown) {
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");
    if (observerService) {
      mNotifyOnShutdown = 
        NS_SUCCEEDED(observerService->AddObserver(this, 
                                                  NS_XPCOM_SHUTDOWN_OBSERVER_ID, 
                                                  PR_FALSE));
    }
    else {
      NS_WARNING("Could not get an observer service. Video decoding events may not shutdown cleanly.");
    }
  }
}

void nsOggDecoder::UnregisterShutdownObserver()
{
  if (mNotifyOnShutdown) {
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");
    if (observerService) {
      mNotifyOnShutdown = PR_FALSE;
      observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }
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

  if (mPlayState == PLAY_STATE_ENDED &&
      aState != PLAY_STATE_SHUTDOWN) {
    // If we've completed playback then the decode and display threads
    // have been shutdown. To honor the state change request we need
    // to reload the resource and restart the threads.
    // Like seeking, this will require opening a new channel, which means
    // we may not actually get the same resource --- a server may send
    // us something different.
    mNextState = aState;
    mPlayState = PLAY_STATE_LOADING;
    Load(mURI, nsnull, nsnull);
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

