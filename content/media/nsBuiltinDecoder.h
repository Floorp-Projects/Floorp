/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
/*
Each video element based on nsBuiltinDecoder has a state machine to manage
its play state and keep the current frame up to date. All state machines
share time in a single shared thread. Each decoder also has one thread
dedicated to decoding audio and video data. This thread is shutdown when
playback is paused. Each decoder also has a thread to push decoded audio
to the hardware. This thread is not created until playback starts, but
currently is not destroyed when paused, only when playback ends.

The decoder owns the resources for downloading the media file, and the
high level state. It holds an owning reference to the state machine
(a subclass of nsDecoderStateMachine; nsBuiltinDecoderStateMachine) that
owns all the resources related to decoding data, and manages the low level
decoding operations and A/V sync. 

Each state machine runs on the shared state machine thread. Every time some
action is required for a state machine, it is scheduled to run on the shared
the state machine thread. The state machine runs one "cycle" on the state
machine thread, and then returns. If necessary, it will schedule itself to
run again in future. While running this cycle, it must not block the
thread, as other state machines' events may need to run. State shared
between a state machine's threads is synchronised via the monitor owned
by its nsBuiltinDecoder object.

The Main thread controls the decode state machine by setting the value
of a mPlayState variable and notifying on the monitor based on the
high level player actions required (Seek, Pause, Play, etc).

The player states are the states requested by the client through the
DOM API.  They represent the desired state of the player, while the
decoder's state represents the actual state of the decoder.

The high level state of the player is maintained via a PlayState value. 
It can have the following states:

START
  The decoder has been initialized but has no resource loaded.
PAUSED
  A request via the API has been received to pause playback.
LOADING
  A request via the API has been received to load a resource.
PLAYING
  A request via the API has been received to start playback.
SEEKING
  A request via the API has been received to start seeking.
COMPLETED
  Playback has completed.
SHUTDOWN
  The decoder is about to be destroyed.

State transition occurs when the Media Element calls the Play, Seek,
etc methods on the nsBuiltinDecoder object. When the transition
occurs nsBuiltinDecoder then calls the methods on the decoder state
machine object to cause it to behave as required by the play state.
State transitions will likely schedule the state machine to run to
affect the change.

An implementation of the nsDecoderStateMachine class is the event
that gets dispatched to the state machine thread. Each time the event is run,
the state machine must cycle the state machine once, and then return.

The state machine has the following states:

DECODING_METADATA
  The media headers are being loaded, and things like framerate, etc are
  being determined, and the first frame of audio/video data is being decoded.
DECODING
  The decode has started. If the PlayState is PLAYING, the decode thread
  should be alive and decoding video and audio frame, the audio thread
  should be playing audio, and the state machine should run periodically
  to update the video frames being displayed.
SEEKING
  A seek operation is in progress. The decode thread should be seeking.
BUFFERING
  Decoding is paused while data is buffered for smooth playback. If playback
  is paused (PlayState transitions to PAUSED) we'll destory the decode thread.
COMPLETED
  The resource has completed decoding, but possibly not finished playback.
  The decode thread will be destroyed. Once playback finished, the audio
  thread will also be destroyed.
SHUTDOWN
  The decoder object and its state machine are about to be destroyed.
  Once the last state machine has been destroyed, the shared state machine
  thread will also be destroyed. It will be recreated later if needed.

The following result in state transitions.

Shutdown()
  Clean up any resources the nsDecoderStateMachine owns.
Play()
  Start decoding and playback of media data.
Buffer
  This is not user initiated. It occurs when the
  available data in the stream drops below a certain point.
Complete
  This is not user initiated. It occurs when the
  stream is completely decoded.
Seek(double)
  Seek to the time position given in the resource.

A state transition diagram:

DECODING_METADATA
  |      |
  v      | Shutdown()
  |      |
  v      -->-------------------->--------------------------|
  |---------------->----->------------------------|        v
DECODING             |          |  |              |        |
  ^                  v Seek(t)  |  |              |        |
  |         Play()   |          v  |              |        |
  ^-----------<----SEEKING      |  v Complete     v        v
  |                  |          |  |              |        |
  |                  |          |  COMPLETED    SHUTDOWN-<-|
  ^                  ^          |  |Shutdown()    |
  |                  |          |  >-------->-----^
  |          Play()  |Seek(t)   |Buffer()         |
  -----------<--------<-------BUFFERING           |
                                |                 ^
                                v Shutdown()      |
                                |                 |
                                ------------>-----|

The following represents the states that the nsBuiltinDecoder object
can be in, and the valid states the nsDecoderStateMachine can be in at that
time:

player LOADING   decoder DECODING_METADATA
player PLAYING   decoder DECODING, BUFFERING, SEEKING, COMPLETED
player PAUSED    decoder DECODING, BUFFERING, SEEKING, COMPLETED
player SEEKING   decoder SEEKING
player COMPLETED decoder SHUTDOWN
player SHUTDOWN  decoder SHUTDOWN

The general sequence of events is:

1) The video element calls Load on nsMediaDecoder. This creates the
   state machine and starts the channel for downloading the
   file. It instantiates and schedules the nsDecoderStateMachine. The
   high level LOADING state is entered, which results in the decode
   thread being created and starting to decode metadata. These are
   the headers that give the video size, framerate, etc. Load() returns
   immediately to the calling video element.

2) When the metadata has been loaded by the decode thread, the state machine
   will call a method on the video element object to inform it that this
   step is done, so it can do the things required by the video specification
   at this stage. The decode thread then continues to decode the first frame
   of data.

3) When the first frame of data has been successfully decoded the state
   machine calls a method on the video element object to inform it that
   this step has been done, once again so it can do the required things
   by the video specification at this stage.

   This results in the high level state changing to PLAYING or PAUSED
   depending on any user action that may have occurred.

   While the play state is PLAYING, the decode thread will decode
   data, and the audio thread will push audio data to the hardware to
   be played. The state machine will run periodically on the shared
   state machine thread to ensure video frames are played at the 
   correct time; i.e. the state machine manages A/V sync.

The Shutdown method on nsBuiltinDecoder closes the download channel, and
signals to the state machine that it should shutdown. The state machine
shuts down asynchronously, and will release the owning reference to the
state machine once its threads are shutdown.

The owning object of a nsBuiltinDecoder object *MUST* call Shutdown when
destroying the nsBuiltinDecoder object.

*/
#if !defined(nsBuiltinDecoder_h_)
#define nsBuiltinDecoder_h_

#include "nsMediaDecoder.h"

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIThread.h"
#include "nsIChannel.h"
#include "nsIObserver.h"
#include "nsIFrame.h"
#include "nsAutoPtr.h"
#include "nsSize.h"
#include "prlog.h"
#include "gfxContext.h"
#include "gfxRect.h"
#include "nsMediaStream.h"
#include "nsMediaDecoder.h"
#include "nsHTMLMediaElement.h"
#include "mozilla/ReentrantMonitor.h"

class nsAudioStream;

static inline PRBool IsCurrentThread(nsIThread* aThread) {
  return NS_GetCurrentThread() == aThread;
}

// Decoder backends must implement this class to perform the codec
// specific parts of decoding the video/audio format.
class nsDecoderStateMachine : public nsRunnable
{
public:
  // Enumeration for the valid decoding states
  enum State {
    DECODER_STATE_DECODING_METADATA,
    DECODER_STATE_DECODING,
    DECODER_STATE_SEEKING,
    DECODER_STATE_BUFFERING,
    DECODER_STATE_COMPLETED,
    DECODER_STATE_SHUTDOWN
  };

  // Initializes the state machine, returns NS_OK on success, or
  // NS_ERROR_FAILURE on failure.
  virtual nsresult Init(nsDecoderStateMachine* aCloneDonor) = 0;

  // Return the current decode state. The decoder monitor must be
  // obtained before calling this.
  virtual State GetState() = 0;

  // Set the audio volume. The decoder monitor must be obtained before
  // calling this.
  virtual void SetVolume(double aVolume) = 0;

  virtual void Shutdown() = 0;

  // Called from the main thread to get the duration. The decoder monitor
  // must be obtained before calling this. It is in units of microseconds.
  virtual PRInt64 GetDuration() = 0;

  // Called from the main thread to set the duration of the media resource
  // if it is able to be obtained via HTTP headers. Called from the 
  // state machine thread to set the duration if it is obtained from the
  // media metadata. The decoder monitor must be obtained before calling this.
  // aDuration is in microseconds.
  virtual void SetDuration(PRInt64 aDuration) = 0;

  // Called while decoding metadata to set the end time of the media
  // resource. The decoder monitor must be obtained before calling this.
  // aEndTime is in microseconds.
  virtual void SetEndTime(PRInt64 aEndTime) = 0;

  // Functions used by assertions to ensure we're calling things
  // on the appropriate threads.
  virtual PRBool OnDecodeThread() const = 0;

  // Returns PR_TRUE if the current thread is the state machine thread.
  virtual PRBool OnStateMachineThread() const = 0;

  virtual nsHTMLMediaElement::NextFrameStatus GetNextFrameStatus() = 0;

  // Cause state transitions. These methods obtain the decoder monitor
  // to synchronise the change of state, and to notify other threads
  // that the state has changed.
  virtual void Play() = 0;

  // Seeks to aTime in seconds
  virtual void Seek(double aTime) = 0;

  // Returns the current playback position in seconds.
  // Called from the main thread to get the current frame time. The decoder
  // monitor must be obtained before calling this.
  virtual double GetCurrentTime() const = 0;

  // Clear the flag indicating that a playback position change event
  // is currently queued. This is called from the main thread and must
  // be called with the decode monitor held.
  virtual void ClearPositionChangeFlag() = 0;

  // Called from the main thread to set whether the media resource can
  // seek into unbuffered ranges. The decoder monitor must be obtained
  // before calling this.
  virtual void SetSeekable(PRBool aSeekable) = 0;

  // Returns PR_TRUE if the media resource can seek into unbuffered ranges,
  // as set by SetSeekable(). The decoder monitor must be obtained before
  // calling this.
  virtual PRBool IsSeekable() = 0;

  // Update the playback position. This can result in a timeupdate event
  // and an invalidate of the frame being dispatched asynchronously if
  // there is no such event currently queued.
  // Only called on the decoder thread. Must be called with
  // the decode monitor held.
  virtual void UpdatePlaybackPosition(PRInt64 aTime) = 0;

  virtual nsresult GetBuffered(nsTimeRanges* aBuffered) = 0;

  virtual PRInt64 VideoQueueMemoryInUse() = 0;
  virtual PRInt64 AudioQueueMemoryInUse() = 0;

  virtual void NotifyDataArrived(const char* aBuffer, PRUint32 aLength, PRUint32 aOffset) = 0;

  // Causes the state machine to switch to buffering state, and to
  // immediately stop playback and buffer downloaded data. Must be called
  // with the decode monitor held. Called on the state machine thread and
  // the main thread.
  virtual void StartBuffering() = 0;

  // Sets the current size of the framebuffer used in MozAudioAvailable events.
  // Called on the state machine thread and the main thread.
  virtual void SetFrameBufferLength(PRUint32 aLength) = 0;
};

class nsBuiltinDecoder : public nsMediaDecoder
{
  // ISupports
  NS_DECL_ISUPPORTS

  // nsIObserver
  NS_DECL_NSIOBSERVER

 public:
  typedef mozilla::ReentrantMonitor ReentrantMonitor;

  // Enumeration for the valid play states (see mPlayState)
  enum PlayState {
    PLAY_STATE_START,
    PLAY_STATE_LOADING,
    PLAY_STATE_PAUSED,
    PLAY_STATE_PLAYING,
    PLAY_STATE_SEEKING,
    PLAY_STATE_ENDED,
    PLAY_STATE_SHUTDOWN
  };

  nsBuiltinDecoder();
  ~nsBuiltinDecoder();
  
  virtual PRBool Init(nsHTMLMediaElement* aElement);

  // This method must be called by the owning object before that
  // object disposes of this decoder object.
  virtual void Shutdown();
  
  virtual double GetCurrentTime();

  virtual nsresult Load(nsMediaStream* aStream,
                        nsIStreamListener** aListener,
                        nsMediaDecoder* aCloneDonor);

  virtual nsDecoderStateMachine* CreateStateMachine() = 0;

  // Start playback of a video. 'Load' must have previously been
  // called.
  virtual nsresult Play();

  // Seek to the time position in (seconds) from the start of the video.
  virtual nsresult Seek(double aTime);

  virtual nsresult PlaybackRateChanged();

  virtual void Pause();
  virtual void SetVolume(double aVolume);
  virtual double GetDuration();

  virtual void SetInfinite(PRBool aInfinite);
  virtual PRBool IsInfinite();

  virtual nsMediaStream* GetCurrentStream();
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal();

  virtual void NotifySuspendedStatusChanged();
  virtual void NotifyBytesDownloaded();
  virtual void NotifyDownloadEnded(nsresult aStatus);
  // Called by the decode thread to keep track of the number of bytes read
  // from the resource.
  void NotifyBytesConsumed(PRInt64 aBytes);

  // Called when the video file has completed downloading.
  // Call on the main thread only.
  void ResourceLoaded();

  // Called if the media file encounters a network error.
  // Call on the main thread only.
  virtual void NetworkError();

  // Call from any thread safely. Return PR_TRUE if we are currently
  // seeking in the media resource.
  virtual PRBool IsSeeking() const;

  // Return PR_TRUE if the decoder has reached the end of playback.
  // Call on the main thread only.
  virtual PRBool IsEnded() const;

  // Set the duration of the media resource in units of seconds.
  // This is called via a channel listener if it can pick up the duration
  // from a content header. Must be called from the main thread only.
  virtual void SetDuration(double aDuration);

  // Set a flag indicating whether seeking is supported
  virtual void SetSeekable(PRBool aSeekable);

  // Return PR_TRUE if seeking is supported.
  virtual PRBool IsSeekable();

  virtual nsresult GetSeekable(nsTimeRanges* aSeekable);

  virtual Statistics GetStatistics();

  // Suspend any media downloads that are in progress. Called by the
  // media element when it is sent to the bfcache. Call on the main
  // thread only.
  virtual void Suspend();

  // Resume any media downloads that have been suspended. Called by the
  // media element when it is restored from the bfcache. Call on the
  // main thread only.
  virtual void Resume(PRBool aForceBuffering);

  // Tells our nsMediaStream to put all loads in the background.
  virtual void MoveLoadsToBackground();

  void AudioAvailable(float* aFrameBuffer, PRUint32 aFrameBufferLength, float aTime);

  // Called by the state machine to notify the decoder that the duration
  // has changed.
  void DurationChanged();

  PRBool OnStateMachineThread() const;

  PRBool OnDecodeThread() const {
    return mDecoderStateMachine->OnDecodeThread();
  }

  // Returns the monitor for other threads to synchronise access to
  // state.
  ReentrantMonitor& GetReentrantMonitor() { 
    return mReentrantMonitor; 
  }

  // Constructs the time ranges representing what segments of the media
  // are buffered and playable.
  virtual nsresult GetBuffered(nsTimeRanges* aBuffered) {
    if (mDecoderStateMachine) {
      return mDecoderStateMachine->GetBuffered(aBuffered);
    }
    return NS_ERROR_FAILURE;
  }

  virtual PRInt64 VideoQueueMemoryInUse() {
    if (mDecoderStateMachine) {
      return mDecoderStateMachine->VideoQueueMemoryInUse();
    }
    return 0;
  }

  virtual PRInt64 AudioQueueMemoryInUse() {
    if (mDecoderStateMachine) {
      return mDecoderStateMachine->AudioQueueMemoryInUse();
    }
    return 0;
  }

  virtual void NotifyDataArrived(const char* aBuffer, PRUint32 aLength, PRUint32 aOffset) {
    return mDecoderStateMachine->NotifyDataArrived(aBuffer, aLength, aOffset);
  }

  // Sets the length of the framebuffer used in MozAudioAvailable events.
  // The new size must be between 512 and 16384.
  virtual nsresult RequestFrameBufferLength(PRUint32 aLength);

 public:
  // Return the current state. Can be called on any thread. If called from
  // a non-main thread, the decoder monitor must be held.
  PlayState GetState() {
    return mPlayState;
  }

  // Stop updating the bytes downloaded for progress notifications. Called
  // when seeking to prevent wild changes to the progress notification.
  // Must be called with the decoder monitor held.
  void StopProgressUpdates();

  // Allow updating the bytes downloaded for progress notifications. Must
  // be called with the decoder monitor held.
  void StartProgressUpdates();

  // Something has changed that could affect the computed playback rate,
  // so recompute it. The monitor must be held.
  void UpdatePlaybackRate();

  // The actual playback rate computation. The monitor must be held.
  double ComputePlaybackRate(PRPackedBool* aReliable);

  // Make the decoder state machine update the playback position. Called by
  // the reader on the decoder thread (Assertions for this checked by 
  // mDecoderStateMachine). This must be called with the decode monitor
  // held.
  void UpdatePlaybackPosition(PRInt64 aTime)
  {
    mDecoderStateMachine->UpdatePlaybackPosition(aTime);
  }

  /****** 
   * The following methods must only be called on the main
   * thread.
   ******/

  // Change to a new play state. This updates the mState variable and
  // notifies any thread blocking on this object's monitor of the
  // change. Call on the main thread only.
  void ChangeState(PlayState aState);

  // Called when the metadata from the media file has been read.
  // Call on the main thread only.
  void MetadataLoaded(PRUint32 aChannels,
                      PRUint32 aRate);

  // Called when the first frame has been loaded.
  // Call on the main thread only.
  void FirstFrameLoaded();

  // Called when the video has completed playing.
  // Call on the main thread only.
  void PlaybackEnded();

  // Seeking has stopped. Inform the element on the main
  // thread.
  void SeekingStopped();

  // Seeking has stopped at the end of the resource. Inform the element on the main
  // thread.
  void SeekingStoppedAtEnd();

  // Seeking has started. Inform the element on the main
  // thread.
  void SeekingStarted();

  // Called when the backend has changed the current playback
  // position. It dispatches a timeupdate event and invalidates the frame.
  // This must be called on the main thread only.
  void PlaybackPositionChanged();

  // Calls mElement->UpdateReadyStateForData, telling it which state we have
  // entered.  Main thread only.
  void NextFrameUnavailableBuffering();
  void NextFrameAvailable();
  void NextFrameUnavailable();

  // Calls mElement->UpdateReadyStateForData, telling it whether we have
  // data for the next frame and if we're buffering. Main thread only.
  void UpdateReadyStateForData();

  // Find the end of the cached data starting at the current decoder
  // position.
  PRInt64 GetDownloadPosition();

  // Updates the approximate byte offset which playback has reached. This is
  // used to calculate the readyState transitions.
  void UpdatePlaybackOffset(PRInt64 aOffset);

  // Provide access to the state machine object
  nsDecoderStateMachine* GetStateMachine() { return mDecoderStateMachine; }

  // Return the current decode state. The decoder monitor must be
  // obtained before calling this.
  nsDecoderStateMachine::State GetDecodeState() { return mDecoderStateMachine->GetState(); }

public:
  // Notifies the element that decoding has failed.
  void DecodeError();

  // Schedules the state machine to run one cycle on the shared state
  // machine thread. Main thread only.
  nsresult ScheduleStateMachineThread();

  /******
   * The following members should be accessed with the decoder lock held.
   ******/

  // Current decoding position in the stream. This is where the decoder
  // is up to consuming the stream. This is not adjusted during decoder
  // seek operations, but it's updated at the end when we start playing
  // back again.
  PRInt64 mDecoderPosition;
  // Current playback position in the stream. This is (approximately)
  // where we're up to playing back the stream. This is not adjusted
  // during decoder seek operations, but it's updated at the end when we
  // start playing back again.
  PRInt64 mPlaybackPosition;
  // Data needed to estimate playback data rate. The timeline used for
  // this estimate is "decode time" (where the "current time" is the
  // time of the last decoded video frame).
  nsChannelStatistics mPlaybackStatistics;

  // The current playback position of the media resource in units of
  // seconds. This is updated approximately at the framerate of the
  // video (if it is a video) or the callback period of the audio.
  // It is read and written from the main thread only.
  double mCurrentTime;

  // Volume that playback should start at.  0.0 = muted. 1.0 = full
  // volume.  Readable/Writeable from the main thread.
  double mInitialVolume;

  // Position to seek to when the seek notification is received by the
  // decode thread. Written by the main thread and read via the
  // decode thread. Synchronised using mReentrantMonitor. If the
  // value is negative then no seek has been requested. When a seek is
  // started this is reset to negative.
  double mRequestedSeekTime;

  // Duration of the media resource. Set to -1 if unknown.
  // Set when the metadata is loaded. Accessed on the main thread
  // only.
  PRInt64 mDuration;

  // True if the media resource is seekable (server supports byte range
  // requests).
  PRPackedBool mSeekable;

  /******
   * The following member variables can be accessed from any thread.
   ******/

  // The state machine object for handling the decoding. It is safe to
  // call methods of this object from other threads. Its internal data
  // is synchronised on a monitor. The lifetime of this object is
  // after mPlayState is LOADING and before mPlayState is SHUTDOWN. It
  // is safe to access it during this period.
  nsCOMPtr<nsDecoderStateMachine> mDecoderStateMachine;

  // Stream of media data.
  nsAutoPtr<nsMediaStream> mStream;

  // ReentrantMonitor for detecting when the video play state changes. A call
  // to Wait on this monitor will block the thread until the next
  // state change.
  ReentrantMonitor mReentrantMonitor;

  // Set to one of the valid play states. It is protected by the
  // monitor mReentrantMonitor. This monitor must be acquired when reading or
  // writing the state. Any change to the state on the main thread
  // must call NotifyAll on the monitor so the decode thread can wake up.
  PlayState mPlayState;

  // The state to change to after a seek or load operation. It must only
  // be changed from the main thread. The decoder monitor must be acquired
  // when writing to the state, or when reading from a non-main thread.
  // Any change to the state must call NotifyAll on the monitor.
  PlayState mNextState;	

  // True when we have fully loaded the resource and reported that
  // to the element (i.e. reached NETWORK_LOADED state).
  // Accessed on the main thread only.
  PRPackedBool mResourceLoaded;

  // True when seeking or otherwise moving the play position around in
  // such a manner that progress event data is inaccurate. This is set
  // during seek and duration operations to prevent the progress indicator
  // from jumping around. Read/Write from any thread. Must have decode monitor
  // locked before accessing.
  PRPackedBool mIgnoreProgressData;

  // PR_TRUE if the stream is infinite (e.g. a webradio).
  PRPackedBool mInfiniteStream;
};

#endif
