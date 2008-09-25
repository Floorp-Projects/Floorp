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
#include "nsIRenderingContext.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "nsPresContext.h"
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

// Time in seconds to pause while buffering video data
// on a slow connection.
#define BUFFERING_WAIT 15

// Download rate can take some time to stabalise. For the buffering
// computation there is a minimum rate to ensure a reasonable amount
// gets buffered.
#define BUFFERING_MIN_RATE 50000
#define BUFFERING_RATE(x) ((x)< BUFFERING_MIN_RATE ? BUFFERING_MIN_RATE : (x))

// The number of seconds of buffer data before buffering happens
// based on current playback rate.
#define BUFFERING_SECONDS_WATERMARK 1

/* Handle the state machine for displaying video and playing audio.

   All internal state is synchronised via the decoder
   monitor. NotifyAll on the monitor is called when state is
   changed. The following changes to state cause a notify:

     A frame has been displayed

   See nsOggDecoder.h for more details.
*/
class nsOggDisplayStateMachine : public nsRunnable
{
public:
  nsOggDisplayStateMachine(nsOggDecoder* aDecoder);
  ~nsOggDisplayStateMachine();

  // Called from the main thread to get the current frame time. The decoder
  // monitor must be obtained before calling this.
  float GetCurrentTime();

  // Get and set the audio volume. The decoder monitor must be
  // obtained before calling this.
  float GetVolume();
  void SetVolume(float aVolume);

  NS_IMETHOD Run();

  // Update the current frame time. Used during a seek when the initial frame is
  // displayed and a new time for that frame is computed. Must be called with a
  // lock on the decoder monitor.
  void UpdateFrameTime(float aTime);

protected:
  // Initializes and opens the audio stream. Called from the display
  // thread only. Must be called with the decode monitor held.
  void OpenAudioStream();

  // Closes and releases resources used by the audio stream. Called
  // from the display thread only. Must be called with the decode
  // monitor held.
  void CloseAudioStream();

  // Start playback of audio, either by opening or resuming the audio
  // stream. Must be called with the decode monitor held.
  void StartAudio();

  // Stop playback of audio, either by closing or pausing the audio
  // stream. Must be called with the decode monitor held.
  void StopAudio();

private:
  // The decoder object that created this state machine. The decoder
  // always outlives us since it controls our lifetime.
  nsOggDecoder* mDecoder;

  // The audio stream resource. Used on the display thread and the
  // main thread (Via the Get/SetVolume calls). Synchronisation via
  // mDecoder monitor.
  nsAutoPtr<nsAudioStream> mAudioStream;

  // The time of the current frame in seconds. This is referenced from
  // 0.0 which is the initial start of the stream. Set by the display
  // thread, and read-only from the main thread to get the current
  // time value. Synchronised via decoder monitor.
  float mCurrentFrameTime;

  // The last time interval that a video frame was displayed. Used for
  // computing the sleep period between frames for a/v sync.
  // Read/Write from the display thread only.
  PRIntervalTime mLastFrameTime;

  // Volume of playback. 0.0 = muted. 1.0 = full volume. Read/Written
  // from the display and main threads. Synchronised via decoder
  // monitor.
  float mVolume;
};

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

  All internal state is synchronised via the decoder
  monitor. NotifyAll on the monitor is called when the state of the
  state machine is changed. The following changes to state cause a
  notify:

    mState and data related to that state changed (mSeekTime, etc)
    Ogg Metadata Loaded
    First Frame Loaded  
    Frame decoded    
    
  See nsOggDecoder.h for more details.
*/
class nsOggDecodeStateMachine : public nsRunnable
{
public:
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

  nsOggDecodeStateMachine(nsOggDecoder* aDecoder, nsChannelReader* aReader);
  ~nsOggDecodeStateMachine();

  // Cause state transitions. These methods obtain the decoder monitor
  // to synchronise the change of state, and to notify other threads
  // that the state has changed.
  void Shutdown();
  void Decode();
  void Seek(float aTime);

  NS_IMETHOD Run();

  // Provide access to the current state. Must be called with a lock
  // on the decoder's monitor.
  State GetState()
  {
    return mState;
  }

  // Provide access to the ogg metadata. Must be called with a lock on
  // the decoder's monitor. The metadata is sent when in the state
  // DECODING_METADATA and is read-only after that point. For this
  // reason these methods should only be called if the current state
  // is greater than DECODING_METADATA.
  float GetFramerate()
  {
    NS_ASSERTION(mState > DECODER_STATE_DECODING_METADATA, "GetFramerate() called during invalid state");
    //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "GetFramerate() called without acquiring decoder monitor");
    return mFramerate;
  }

  PRBool HasAudio()
  {
    NS_ASSERTION(mState > DECODER_STATE_DECODING_METADATA, "HasAudio() called during invalid state");
    //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "HasAudio() called without acquiring decoder monitor");
    return mAudioTrack != -1;
  }

  PRInt32 GetAudioRate()
  {
    NS_ASSERTION(mState > DECODER_STATE_DECODING_METADATA, "GetAudioRate() called during invalid state");
    //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "GetAudioRate() called without acquiring decoder monitor");
    return mAudioRate;
  }

  PRInt32 GetAudioChannels()
  {
    NS_ASSERTION(mState > DECODER_STATE_DECODING_METADATA, "GetAudioChannels() called during invalid state");
    //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "GetAudioChannels() called without acquiring decoder monitor");
    return mAudioChannels;
  }
  
  // Return PR_TRUE if we have completed decoding or are shutdown. Must
  // be called with a lock on the decoder's monitor.
  PRBool IsCompleted()
  {
    //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "IsCompleted() called without acquiring decoder monitor");
    return 
      mState == DECODER_STATE_COMPLETED ||
      mState == DECODER_STATE_SHUTDOWN;
  }

  // Decode one frame of data, returning the OggPlay error code. Must
  // be called only when the current state > DECODING_METADATA. The decode 
  // monitor does not need to be locked during this call since liboggplay
  // internally handles locking.
  // Any return value apart from those below is mean decoding cannot continue.
  // E_OGGPLAY_CONTINUE       = One frame decoded and put in buffer list
  // E_OGGPLAY_USER_INTERRUPT = One frame decoded, buffer list is now full
  // E_OGGPLAY_TIMEOUT        = No frames decoded, timed out
  OggPlayErrorCode DecodeFrame();

  // Returns the next decoded frame of data. Resources owned by the
  // frame must be released via a call to ReleaseFrame. This function
  // must be called only when the current state >
  // DECODING_METADATA. The decode monitor lock does not need to be
  // locked during this call since liboggplay internally handles
  // locking.
  OggPlayCallbackInfo** NextFrame();

  // Frees the resources owned by a frame obtained from
  // NextFrame(). This function must be called only when the current
  // state > DECODING_METADATA. The decode monitor must be acquired
  // before calling.
  void ReleaseFrame(OggPlayCallbackInfo** aFrame);

  // These methods handle the display of the decoded OggPlay data.
  // They are usually called by the display thread, but can be called
  // from any thread as long as the decode monitor is locked.  They
  // must be called only when the current state > DECODING_METADATA.
  // Returns the timestamp, in seconds, of the frame that was
  // processed.
  float DisplayFrame(OggPlayCallbackInfo** aFrame, nsAudioStream* aAudioStream);

  // Decode and display the initial frame after the oggplay buffers
  // have been cleared (during a seek for example). The decode monitor
  // is obtained internally by this method for synchronisation.
  float DisplayInitialFrame();

protected:
  // Handle the display of tracks within the frame (audio or
  // video). The decoder monitor must be acquired in the scope of
  // calls to these functions. They must be called only when the
  // current state > DECODING_METADATA. DisplayTrack returns the
  // timestamp, in seconds, of the track that was processed.
  float DisplayTrack(int aTrackNumber, OggPlayCallbackInfo* aTrack, nsAudioStream* aAudioStream);
  void HandleVideoData(int aTrackNum, OggPlayVideoData* aVideoData);
  void HandleAudioData(nsAudioStream* aAudioStream, OggPlayAudioData* aAudioData, int aSize);
  void CopyVideoFrame(int aTrackNum, OggPlayVideoData* aVideoData, float aFramerate);

  // These methods can only be called on the decoding thread.
  void LoadOggHeaders();
  void LoadFirstFrame();

private:
  // The decoder object that created this state machine. The decoder
  // always outlives us since it controls our lifetime.
  nsOggDecoder* mDecoder;

  // The OggPlay handle. Synchronisation of calls to oggplay functions
  // are handled by liboggplay. We control the lifetime of this
  // object, destroying it in our destructor.
  OggPlay* mPlayer;

  // Channel Reader. Originally created by the mDecoder object, it is
  // destroyed when we close the mPlayer handle in the
  // destructor. Used to obtain download and playback rate information
  // for buffering.  Synchronisation for those methods are handled by
  // nsMediaStream.
  nsChannelReader* mReader;

  // Video data. These are initially set when the metadata is loaded.
  // After that they are read only. The decoder monitor lock must be
  // obtained before reading.
  PRInt32 mVideoTrack;
  float   mFramerate;

  // Audio data. These are initially set when the metadata is loaded.
  // After that they are read only. The decoder monitor lock must be
  // obtained before reading.
  PRInt32 mAudioRate;
  PRInt32 mAudioChannels;
  PRInt32 mAudioTrack;

  // The decoder monitor must be obtained before modifying this state.
  // NotifyAll on the monitor must be called when state is changed so
  // other threads can react to changes.
  State mState;

  // Time that buffering started. Used for buffering timeout and only
  // accessed in the decoder thread.
  PRIntervalTime mBufferingStart;

  // Number of bytes to buffer when buffering. Only accessed in the
  // decoder thread.
  PRUint32 mBufferingBytes;

  // Position to seek to when the seek state transition occurs. The
  // decoder monitor lock must be obtained before reading or writing
  // this value.
  float mSeekTime;

  // PR_TRUE if the oggplay buffer is full and we'll block on the next
  // ogg_step_decoding call. The decode monitor lock must be obtained
  // before reading/writing this value.
  PRPackedBool mBufferFull;
};

nsOggDecodeStateMachine::nsOggDecodeStateMachine(nsOggDecoder* aDecoder, nsChannelReader* aReader) :
  mDecoder(aDecoder),
  mPlayer(0),
  mReader(aReader),
  mVideoTrack(-1),
  mFramerate(0.0),
  mAudioRate(0),
  mAudioChannels(0),
  mAudioTrack(-1),
  mState(DECODER_STATE_DECODING_METADATA),
  mBufferingStart(0),
  mBufferingBytes(0),
  mSeekTime(0.0),
  mBufferFull(PR_FALSE)
{
}

nsOggDecodeStateMachine::~nsOggDecodeStateMachine()
{
  oggplay_close(mPlayer);
}


OggPlayErrorCode nsOggDecodeStateMachine::DecodeFrame()
{
  NS_ASSERTION(mState > DECODER_STATE_DECODING_METADATA, "DecodeFrame() called during invalid state");
  return oggplay_step_decoding(mPlayer);
}

OggPlayCallbackInfo** nsOggDecodeStateMachine::NextFrame()
{
  NS_ASSERTION(mState > DECODER_STATE_DECODING_METADATA, "NextFrame() called during invalid state");
  return oggplay_buffer_retrieve_next(mPlayer);
}

void nsOggDecodeStateMachine::ReleaseFrame(OggPlayCallbackInfo** aFrame)
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "ReleaseFrame() called without acquiring decoder monitor");
  NS_ASSERTION(mState > DECODER_STATE_DECODING_METADATA, "ReleaseFrame() called during invalid state");
  oggplay_buffer_release(mPlayer, aFrame);
  mBufferFull = PR_FALSE;
}

float nsOggDecodeStateMachine::DisplayFrame(OggPlayCallbackInfo** aFrame, nsAudioStream* aAudioStream)
{
  NS_ASSERTION(mState > DECODER_STATE_DECODING_METADATA, "DisplayFrame() called during invalid state");
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "DisplayFrame() called without acquiring decoder monitor");
  int num_tracks = oggplay_get_num_tracks(mPlayer);
  float audioTime = 0.0;
  float videoTime = 0.0;

  if (mVideoTrack != -1 && num_tracks > mVideoTrack) {
    videoTime = DisplayTrack(mVideoTrack, aFrame[mVideoTrack], aAudioStream);
  }

  if (aAudioStream && mAudioTrack != -1 && num_tracks > mAudioTrack) {
    audioTime = DisplayTrack(mAudioTrack, aFrame[mAudioTrack], aAudioStream);
  }

  return videoTime > audioTime ? videoTime : audioTime;
}

float nsOggDecodeStateMachine::DisplayInitialFrame()
{
  nsAutoMonitor mon(mDecoder->GetMonitor());
  float time = 0.0;
  OggPlayCallbackInfo **frame = NextFrame();
  mon.Exit();
  while (!frame) {
    OggPlayErrorCode r = DecodeFrame();
    if (r != E_OGGPLAY_CONTINUE &&
        r != E_OGGPLAY_USER_INTERRUPT &&
        r != E_OGGPLAY_TIMEOUT) {
      break;
    }
    mon.Enter();
    if (mState == DECODER_STATE_SHUTDOWN)
      return 0.0;

    mBufferFull = (r == E_OGGPLAY_USER_INTERRUPT);
    frame = NextFrame();
    mon.Exit();
  }
  mon.Enter();
  if (frame) {
    if (mState != DECODER_STATE_SHUTDOWN) {
      // If shutting down, don't display the frame as this can
      // cause events to be posted.
      time = DisplayFrame(frame, nsnull);
    }
    ReleaseFrame(frame);
    mBufferFull = PR_FALSE;
  }
  return time;
}

float nsOggDecodeStateMachine::DisplayTrack(int aTrackNumber, OggPlayCallbackInfo* aTrack, nsAudioStream* aAudioStream)
{
  OggPlayDataType type = oggplay_callback_info_get_type(aTrack);
  OggPlayDataHeader ** headers = oggplay_callback_info_get_headers(aTrack);
  float time = 0.0;

  switch(type) {
  case OGGPLAY_INACTIVE:
    {
      break;
    }
    
  case OGGPLAY_YUV_VIDEO:
    {
      time = ((float)oggplay_callback_info_get_presentation_time(headers[0]))/1000.0;          
      OggPlayVideoData* video_data = oggplay_callback_info_get_video_data(headers[0]);
      HandleVideoData(aTrackNumber, video_data);
    }
    break;
  case OGGPLAY_FLOATS_AUDIO:
    {
      time = ((float)oggplay_callback_info_get_presentation_time(headers[0]))/1000.0;
      int required = oggplay_callback_info_get_required(aTrack);
      for (int j = 0; j < required; ++j) {
        int size = oggplay_callback_info_get_record_size(headers[j]);
        OggPlayAudioData* audio_data = oggplay_callback_info_get_audio_data(headers[j]);
        HandleAudioData(aAudioStream, audio_data, size);
      }
      break;
    }
  case OGGPLAY_CMML:
    {
      if (oggplay_callback_info_get_required(aTrack) > 0) {
        LOG(PR_LOG_DEBUG, ("CMML: %s", oggplay_callback_info_get_text_data(headers[0])));
      }
      break;
    }
  default:
    break;
  }
  return time;
}

void nsOggDecodeStateMachine::HandleVideoData(int aTrackNum, OggPlayVideoData* aVideoData) {
  if (!aVideoData)
    return;

  CopyVideoFrame(aTrackNum, aVideoData, mFramerate);

  nsCOMPtr<nsIRunnable> event = 
    NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, Invalidate); 
  
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}

void nsOggDecodeStateMachine::HandleAudioData(nsAudioStream* aAudioStream, OggPlayAudioData* aAudioData, int aSize) {
  if (aAudioStream) {
    // 'aSize' is number of samples. Multiply by number of channels to
    // get the actual number of floats being sent.
    nsresult rv = aAudioStream->Write(reinterpret_cast<float*>(aAudioData), aSize * mAudioChannels);
    if (!NS_SUCCEEDED(rv)) {
      LOG(PR_LOG_ERROR, ("Could not write audio data to pipe"));
    }
  }
}

void nsOggDecodeStateMachine::CopyVideoFrame(int aTrackNum, OggPlayVideoData* aVideoData, float aFramerate) 
{
  int y_width;
  int y_height;
  oggplay_get_video_y_size(mPlayer, aTrackNum, &y_width, &y_height);
  int uv_width;
  int uv_height;
  oggplay_get_video_uv_size(mPlayer, aTrackNum, &uv_width, &uv_height);

  if (y_width >= MAX_VIDEO_WIDTH || y_height >= MAX_VIDEO_HEIGHT) {
    return;
  }

  {
    nsAutoLock lock(mDecoder->mVideoUpdateLock);

    mDecoder->SetRGBData(y_width, y_height, aFramerate, nsnull);

    // If there is not enough memory to allocate the RGB buffer,
    // don't display it, but continue without error. When enough
    // memory is available the display will start working again.
    if (mDecoder->mRGB) {
      OggPlayYUVChannels yuv;
      OggPlayRGBChannels rgb;
      
      yuv.ptry = aVideoData->y;
      yuv.ptru = aVideoData->u;
      yuv.ptrv = aVideoData->v;
      yuv.uv_width = uv_width;
      yuv.uv_height = uv_height;
      yuv.y_width = y_width;
      yuv.y_height = y_height;
      
      rgb.ptro = mDecoder->mRGB.get();
      rgb.rgb_width = mDecoder->mRGBWidth;
      rgb.rgb_height = mDecoder->mRGBHeight;

      oggplay_yuv2bgr(&yuv, &rgb);
    }
  }
}


void nsOggDecodeStateMachine::Shutdown()
{
  // oggplay_prepare_for_close cannot be undone. Once called, the
  // mPlayer object cannot decode any more frames. Once we've entered
  // the shutdown state here there's no going back.
  nsAutoMonitor mon(mDecoder->GetMonitor());
  oggplay_prepare_for_close(mPlayer);
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
    mon.NotifyAll();
  }
}

void nsOggDecodeStateMachine::Seek(float aTime)
{
  nsAutoMonitor mon(mDecoder->GetMonitor());
  mSeekTime = aTime;
  mState = DECODER_STATE_SEEKING;
  mon.NotifyAll();
}

nsresult nsOggDecodeStateMachine::Run()
{
  nsAutoMonitor mon(mDecoder->GetMonitor());

  while (PR_TRUE) {
    switch(mState) {
    case DECODER_STATE_DECODING_METADATA:
      mon.Exit();
      LoadOggHeaders();
      mon.Enter();
      
      if (mState == DECODER_STATE_DECODING_METADATA) {
        mState = DECODER_STATE_DECODING_FIRSTFRAME;
      }

      mon.NotifyAll();
      break;

    case DECODER_STATE_DECODING_FIRSTFRAME:
      {
        mon.Exit();
        LoadFirstFrame();
        mon.Enter();

        if (mState == DECODER_STATE_DECODING_FIRSTFRAME) {
          mState = DECODER_STATE_DECODING;
        }

        mon.NotifyAll();
      }
      break;

    case DECODER_STATE_DECODING:
      {
        if (mReader->DownloadRate() >= 0 &&
            mReader->Available() < mReader->PlaybackRate() * BUFFERING_SECONDS_WATERMARK) {
          mBufferingStart = PR_IntervalNow();
          mBufferingBytes = PRUint32(BUFFERING_RATE(mReader->PlaybackRate()) * BUFFERING_WAIT);
          mState = DECODER_STATE_BUFFERING;
          mon.NotifyAll();

          nsCOMPtr<nsIRunnable> event =
            NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, BufferingStarted);
          NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
        }
        else {
          // If the oggplay buffer is full, wait until it is not so we don't
          // block inside liboggplay.
          while (mBufferFull && mState == DECODER_STATE_DECODING) {
            mon.Wait();
          }

          // State could have changed while we were waiting
          if (mState != DECODER_STATE_DECODING) {
            continue;
          }

          mon.Exit();
          OggPlayErrorCode r = DecodeFrame();
          mon.Enter();

          mBufferFull = (r == E_OGGPLAY_USER_INTERRUPT);

          if (mState == DECODER_STATE_SHUTDOWN) {
            continue;
          }

          if (r != E_OGGPLAY_CONTINUE && 
              r != E_OGGPLAY_USER_INTERRUPT &&
              r != E_OGGPLAY_TIMEOUT)  {
            mState = DECODER_STATE_COMPLETED;
          }
          
          mon.NotifyAll();
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
        if (mState == DECODER_STATE_SHUTDOWN) {
          continue;
        }

        mBufferFull = PR_FALSE;

        mon.Exit();
        float time = DisplayInitialFrame();
        mon.Enter();

        if (mState == DECODER_STATE_SHUTDOWN) {
          continue;
        }

        mDecoder->mDisplayStateMachine->UpdateFrameTime(time);
        mon.Exit();

        nsCOMPtr<nsIRunnable> stopEvent = 
          NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, SeekingStopped);
        NS_DispatchToMainThread(stopEvent, NS_DISPATCH_SYNC);        
        mon.Enter();

        if (mState == DECODER_STATE_SEEKING && mSeekTime == seekTime) {
          mState = DECODER_STATE_DECODING;
        }
        
        mon.NotifyAll();
      }
      break;

    case DECODER_STATE_BUFFERING:
      if ((PR_IntervalToMilliseconds(PR_IntervalNow() - mBufferingStart) < BUFFERING_WAIT*1000) &&
          mReader->DownloadRate() >= 0 &&            
          mReader->Available() < mBufferingBytes) {
        LOG(PR_LOG_DEBUG, 
            ("Buffering data until %d bytes available or %d milliseconds", 
             (long)(mBufferingBytes - mReader->Available()),
             BUFFERING_WAIT*1000 - (PR_IntervalToMilliseconds(PR_IntervalNow() - mBufferingStart))));
        
        mon.Wait(PR_MillisecondsToInterval(1000));
      }
      else
        mState = DECODER_STATE_DECODING;

      if (mState == DECODER_STATE_SHUTDOWN) {
        continue;
      }

      if (mState != DECODER_STATE_BUFFERING) {
        nsCOMPtr<nsIRunnable> event = 
          NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, BufferingStopped);
        NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
        mon.NotifyAll();
      }

      break;

    case DECODER_STATE_COMPLETED:
      mon.Wait();
      break;

    case DECODER_STATE_SHUTDOWN:
      return NS_OK;
    }
  }

  return NS_OK;
}

void nsOggDecodeStateMachine::LoadOggHeaders() 
{
  LOG(PR_LOG_DEBUG, ("Loading Ogg Headers"));

  mPlayer = oggplay_open_with_reader(mReader);
  if (mPlayer) {
    LOG(PR_LOG_DEBUG, ("There are %d tracks", oggplay_get_num_tracks(mPlayer)));

    for (int i = 0; i < oggplay_get_num_tracks(mPlayer); ++i) {
      LOG(PR_LOG_DEBUG, ("Tracks %d: %s", i, oggplay_get_track_typename(mPlayer, i)));
      if (mVideoTrack == -1 && oggplay_get_track_type(mPlayer, i) == OGGZ_CONTENT_THEORA) {
        oggplay_set_callback_num_frames(mPlayer, i, 1);
        mVideoTrack = i;
        int fpsd, fpsn;
        oggplay_get_video_fps(mPlayer, i, &fpsd, &fpsn);
        mFramerate = fpsd == 0 ? 0.0 : double(fpsn)/double(fpsd);
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
    }

    oggplay_use_buffer(mPlayer, OGGPLAY_BUFFER_SIZE);

    // Inform the element that we've loaded the Ogg metadata
    nsCOMPtr<nsIRunnable> metadataLoadedEvent = 
      NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, MetadataLoaded); 
    
    NS_DispatchToMainThread(metadataLoadedEvent, NS_DISPATCH_NORMAL);
  }
}

void nsOggDecodeStateMachine::LoadFirstFrame()
{
  NS_ASSERTION(!mBufferFull, "Buffer before reading first frame");
  if(DecodeFrame() == E_OGGPLAY_USER_INTERRUPT) {
    nsAutoMonitor mon(mDecoder->GetMonitor());
    mBufferFull = PR_TRUE;
  }

  nsCOMPtr<nsIRunnable> event = 
    NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, FirstFrameLoaded); 
    
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}

nsOggDisplayStateMachine::nsOggDisplayStateMachine(nsOggDecoder* aDecoder) :
  mDecoder(aDecoder),
  mCurrentFrameTime(0.0),
  mLastFrameTime(0),
  mVolume(1.0)
{
}

nsOggDisplayStateMachine::~nsOggDisplayStateMachine()
{
}

float nsOggDisplayStateMachine::GetCurrentTime()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "GetCurrentTime() called without acquiring decoder monitor");
  return mCurrentFrameTime;
}


nsresult nsOggDisplayStateMachine::Run()
{
  nsOggDecodeStateMachine* sm = mDecoder->mDecodeStateMachine;
  PRBool playing = PR_FALSE;
  {
    nsAutoMonitor mon(mDecoder->GetMonitor());

    // Wait until the decode state machine has decoded the first frame
    // before displaying the initial starting frame. This ensures that
    // the mPlayer object has been created (required for displaying a
    // frame) and that the relevant metadata is valid (frame size,
    // rate, etc).
    while (sm->GetState() <= nsOggDecodeStateMachine::DECODER_STATE_DECODING_FIRSTFRAME)
      mon.Wait();

    OggPlayCallbackInfo **frame = sm->NextFrame();
    while (!frame) {
      frame = sm->NextFrame();

      // No Frame, wait for decode state machine to decode one
      if (!frame) {
        mon.Wait();
      }

      if (sm->IsCompleted()) {
        sm->ReleaseFrame(frame);
        mon.NotifyAll();
        return NS_OK;
      }
    }
    mCurrentFrameTime = sm->DisplayFrame(frame, nsnull);
    sm->ReleaseFrame(frame);
    mon.NotifyAll();
  }

  while (PR_TRUE) {
    nsAutoMonitor mon(mDecoder->GetMonitor());
    nsOggDecoder::PlayState state = mDecoder->GetState();

    if (state == nsOggDecoder::PLAY_STATE_PLAYING) {
      if (!playing) {
        // Just changed to play state, resume audio.
        StartAudio();
        playing = PR_TRUE;
        mLastFrameTime = PR_IntervalNow();
      }
      
      // Synchronize video to framerate
      PRIntervalTime target = PR_MillisecondsToInterval(PRInt64(1000.0 / sm->GetFramerate()));
      PRIntervalTime diff = PR_IntervalNow() - mLastFrameTime;
      while (diff < target) {
        mon.Wait(target-diff);
        if (mDecoder->GetState() >= nsOggDecoder::PLAY_STATE_ENDED) {
          return NS_OK;
        }
        diff = PR_IntervalNow() - mLastFrameTime;
      }
      mLastFrameTime = PR_IntervalNow();
      
      OggPlayCallbackInfo **frame = sm->NextFrame();
      if (frame) {
        mCurrentFrameTime = sm->DisplayFrame(frame, mAudioStream);
        sm->ReleaseFrame(frame);
        mon.NotifyAll();
      }
      else {
        if (sm->IsCompleted()) {
          nsCOMPtr<nsIRunnable> event = 
            NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, PlaybackEnded);
            
          NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);        
          return NS_OK;
        }

        // Need to notify that we are about to wait, in case the
        // decoder thread is blocked waiting for us.
        mon.NotifyAll();

        mon.Wait();
      }
    }
    else {
      // Paused
      if (playing) {
        // Just changed to paused state, stop audio.
        StopAudio();
        playing = PR_FALSE;
      }
      mon.Wait();
    }
    if (mDecoder->GetState() >= nsOggDecoder::PLAY_STATE_ENDED) {
      return NS_OK;
    }
  }

  return NS_OK;
}

void nsOggDisplayStateMachine::UpdateFrameTime(float aTime)
{
  mCurrentFrameTime = aTime;
}

void nsOggDisplayStateMachine::OpenAudioStream()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "OpenAudioStream() called without acquiring decoder monitor");
  nsOggDecodeStateMachine* sm = mDecoder->mDecodeStateMachine;
  mAudioStream = new nsAudioStream();
  if (!mAudioStream) {
    LOG(PR_LOG_ERROR, ("Could not create audio stream"));
  }
  else {
    mAudioStream->Init(sm->GetAudioChannels(), sm->GetAudioRate());
    mAudioStream->SetVolume(mVolume);
  }
}

void nsOggDisplayStateMachine::CloseAudioStream()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "CloseAudioStream() called without acquiring decoder monitor");
  if (mAudioStream) {
    mAudioStream->Shutdown();
    mAudioStream = nsnull;
  }
}

void nsOggDisplayStateMachine::StartAudio()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "StartAudio() called without acquiring decoder monitor");
  nsOggDecodeStateMachine* sm = mDecoder->mDecodeStateMachine;
  if (sm->HasAudio()) {
    if (!mAudioStream) {
      OpenAudioStream();
    }
    else if(mAudioStream) {
      mAudioStream->Resume();
    }
  }
}

void nsOggDisplayStateMachine::StopAudio()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "StopAudio() called without acquiring decoder monitor");
  nsOggDecodeStateMachine* sm = mDecoder->mDecodeStateMachine;
  if (sm->HasAudio()) {
    if (mDecoder->GetState() == nsOggDecoder::PLAY_STATE_SEEKING) {
      // Need to close the stream to ensure that audio data buffered
      // from the old seek location does not play when seeking
      // completes and audio is resumed.
      CloseAudioStream();
    }
    else if (mAudioStream) {
      mAudioStream->Pause();
    }
  }
}

float nsOggDisplayStateMachine::GetVolume()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "GetVolume() called without acquiring decoder monitor");
  return mVolume;
}

void nsOggDisplayStateMachine::SetVolume(float volume)
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "SetVolume() called without acquiring decoder monitor");
  if (mAudioStream) {
    mAudioStream->SetVolume(volume);
  }

  mVolume = volume;
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
  return mDisplayStateMachine ? mDisplayStateMachine->GetVolume() : mInitialVolume;
}

void nsOggDecoder::SetVolume(float volume)
{
  nsAutoMonitor mon(mMonitor);
  mInitialVolume = volume;

  if (mDisplayStateMachine) {
    mDisplayStateMachine->SetVolume(volume);
  }
}

float nsOggDecoder::GetDuration()
{
  // Currently not implemented. Video Spec says to return
  // NaN if unknown.
  // TODO: return NaN
  return 0.0;
}

nsOggDecoder::nsOggDecoder() :
  nsMediaDecoder(),
  mBytesDownloaded(0),
  mInitialVolume(0.0),
  mSeekTime(-1.0),
  mNotifyOnShutdown(PR_FALSE),
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

void nsOggDecoder::Shutdown() 
{
  ChangeState(PLAY_STATE_SHUTDOWN);

  Stop();
  nsMediaDecoder::Shutdown();
}

nsOggDecoder::~nsOggDecoder()
{
  MOZ_COUNT_DTOR(nsOggDecoder);
  Shutdown();
  nsAutoMonitor::DestroyMonitor(mMonitor);
}

nsresult nsOggDecoder::Load(nsIURI* aURI) 
{
  nsresult rv;
  mURI = aURI;

  StartProgress();

  RegisterShutdownObserver();

  mReader = new nsChannelReader();
  NS_ENSURE_TRUE(mReader, NS_ERROR_OUT_OF_MEMORY);

  rv = mReader->Init(this, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewThread(getter_AddRefs(mDecodeThread));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewThread(getter_AddRefs(mDisplayThread));
  NS_ENSURE_SUCCESS(rv, rv);

  mDecodeStateMachine = new nsOggDecodeStateMachine(this, mReader);

  rv = mDecodeThread->Dispatch(mDecodeStateMachine, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  ChangeState(PLAY_STATE_LOADING);

  return NS_OK;
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

  mSeekTime = aTime;

  // If we are already in the seeking state, then setting mSeekTime
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
  ChangeState(PLAY_STATE_ENDED);

  StopProgress();

  // Force any outstanding seek and byterange requests to complete
  // to prevent shutdown from deadlocking.
  if (mReader) {
    mReader->Cancel();
    mReader = nsnull;
  }

  // Shutdown must be called on both mDecodeStateMachine and
  // mDisplayStateMachine before deleting these objects.  This is
  // required to ensure that the state machines aren't running in
  // their thread and using internal objects when they are deleted.
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

  if (mDisplayThread) {
    mDisplayThread->Shutdown();
    mDisplayThread = nsnull;
  }
  
  mDecodeStateMachine = nsnull;
  mDisplayStateMachine = nsnull;

  UnregisterShutdownObserver();
}



float nsOggDecoder::GetCurrentTime()
{
  nsAutoMonitor mon(mMonitor);

  if (!mDisplayStateMachine) {
    return 0.0;
  }

  return mDisplayStateMachine->GetCurrentTime();
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
  if (mElement) {
    mElement->MetadataLoaded();
  }

  mDisplayStateMachine = new nsOggDisplayStateMachine(this);
  mDisplayThread->Dispatch(mDisplayStateMachine, NS_DISPATCH_NORMAL);
}

void nsOggDecoder::FirstFrameLoaded()
{
  if (mElement) {
    mElement->FirstFrameLoaded();
  }

  // The element can run javascript via events
  // before reaching here, so only change the 
  // state if we're still set to the original
  // loading state.
  nsAutoMonitor mon(mMonitor);
  if (mPlayState == PLAY_STATE_LOADING) {
    if (mSeekTime >= 0.0)
      ChangeState(PLAY_STATE_SEEKING);
    else
      ChangeState(mNextState);
  }
}

void nsOggDecoder::ResourceLoaded()
{
  if (mElement) {
    mElement->ResourceLoaded();
  }
  StopProgress();
}

PRBool nsOggDecoder::IsSeeking() const
{
  return mPlayState == PLAY_STATE_SEEKING;
}

void nsOggDecoder::PlaybackEnded()
{
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

PRUint32 nsOggDecoder::GetBytesLoaded()
{
  return mBytesDownloaded;
}

PRUint32 nsOggDecoder::GetTotalBytes()
{
  // TODO: Need to compute this for ogg files
  return 0;
}

void nsOggDecoder::UpdateBytesDownloaded(PRUint32 aBytes)
{
  mBytesDownloaded = aBytes;
}

void nsOggDecoder::BufferingStopped()
{
  if (mElement) {
    mElement->ChangeReadyState(nsIDOMHTMLMediaElement::CAN_SHOW_CURRENT_FRAME);
  }
}

void nsOggDecoder::BufferingStarted()
{
  if (mElement) {
    mElement->ChangeReadyState(nsIDOMHTMLMediaElement::DATA_UNAVAILABLE);
  }
}

void nsOggDecoder::SeekingStopped()
{
  {
    nsAutoMonitor mon(mMonitor);
    if (mPlayState == PLAY_STATE_SHUTDOWN)
      return;

    // An additional seek was requested while the current seek was
    // in operation.
    if (mSeekTime >= 0.0)
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
  {
    nsAutoMonitor mon(mMonitor);
    if (mPlayState == PLAY_STATE_SHUTDOWN)
      return;
  }

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
  nsAutoMonitor mon(mMonitor);

  if (mNextState == aState) {
    mNextState = PLAY_STATE_PAUSED;
  }

  if (mPlayState == PLAY_STATE_SHUTDOWN) {
    return;
  }

  if (mPlayState == PLAY_STATE_ENDED &&
      aState != PLAY_STATE_SHUTDOWN) {
    // If we've completed playback then the decode and display threads
    // have been shutdown. To honor the state change request we need
    // to reload the resource and restart the threads.
    mNextState = aState;
    mPlayState = PLAY_STATE_LOADING;
    Load(mURI);
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
    mDecodeStateMachine->Seek(mSeekTime);
    mSeekTime = -1.0;
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

