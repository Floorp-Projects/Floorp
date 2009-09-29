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
/*
Each video element has two threads. The first thread, called the Decode thread,
owns the resources for downloading and reading the video file. It goes through the
file, prcessing any decoded theora and vorbis data. It handles the sending of the
audio data to the sound device and the presentation of the video data at the correct
frame rate.

The second thread is the step decode thread. It uses OggPlay to decode the video and
audio data. It indirectly uses an nsMediaStream to do the file reading and seeking via 
Oggplay. 

All file reads and seeks must occur on these two threads. Synchronisation is done via
liboggplay internal mutexes to ensure that access to the liboggplay structures is
done correctly in the presence of the threads.

The step decode thread is created and destroyed in the decode thread. When decoding
needs to be done it is created and event dispatched to it to start the decode loop.
This event exits when decoding is completed or no longer required (during seeking
or shutdown).
    
When the decode thread is created an event is dispatched to it. The event
runs for the lifetime of the playback of the resource. The decode thread
synchronises with the main thread via a single monitor held by the 
nsOggDecoder object.

The event contains a Run method which consists of an infinite loop
that checks the state that the state machine is in and processes
operations on that state.

The nsOggDecodeStateMachine class is the event that gets dispatched to
the decode thread. It has the following states:

DECODING_METADATA
  The Ogg headers are being loaded, and things like framerate, etc are
  being determined, and the first frame of audio/video data is being decoded.
DECODING
  Video/Audio frames are being decoded.
SEEKING
  A seek operation is in progress.
BUFFERING
  Decoding is paused while data is buffered for smooth playback.
COMPLETED
  The resource has completed decoding. 
SHUTDOWN
  The decoder object is about to be destroyed.

The following result in state transitions.

Shutdown()
  Clean up any resources the nsOggDecodeStateMachine owns.
Decode()
  Start decoding video frames.
Buffer
  This is not user initiated. It occurs when the
  available data in the stream drops below a certain point.
Complete
  This is not user initiated. It occurs when the
  stream is completely decoded.
Seek(float)
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
  |         Decode() |          v  |              |        |
  ^-----------<----SEEKING      |  v Complete     v        v
  |                  |          |  |              |        |
  |                  |          |  COMPLETED    SHUTDOWN-<-|
  ^                  ^          |  |Shutdown()    |
  |                  |          |  >-------->-----^
  |         Decode() |Seek(t)   |Buffer()         |
  -----------<--------<-------BUFFERING           |
                                |                 ^
                                v Shutdown()      |
                                |                 |
                                ------------>-----|

The Main thread controls the decode state machine by setting the value
of a mPlayState variable and notifying on the monitor
based on the high level player actions required (Seek, Pause, Play, etc).

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
etc methods on the nsOggDecoder object. When the transition occurs
nsOggDecoder then calls the methods on the decoder state machine
object to cause it to behave appropriate to the play state.

The following represents the states that the player can be in, and the
valid states the decode thread can be in at that time:

player LOADING   decoder DECODING_METADATA
player PLAYING   decoder DECODING, BUFFERING, SEEKING, COMPLETED
player PAUSED    decoder DECODING, BUFFERING, SEEKING, COMPLETED
player SEEKING   decoder SEEKING
player COMPLETED decoder SHUTDOWN
player SHUTDOWN  decoder SHUTDOWN

The general sequence of events with these objects is:

1) The video element calls Load on nsMediaDecoder. This creates the
   decode thread and starts the channel for downloading the file. It
   instantiates and starts the Decode state machine. The high level
   LOADING state is entered, which results in the decode state machine
   to start decoding metadata. These are the headers that give the
   video size, framerate, etc.  It returns immediately to the calling
   video element.

2) When the Ogg metadata has been loaded by the decode thread it will
   call a method on the video element object to inform it that this
   step is done, so it can do the things required by the video
   specification at this stage. The decoder then continues to decode
   the first frame of data.

3) When the first frame of Ogg data has been successfully decoded it
   calls a method on the video element object to inform it that this
   step has been done, once again so it can do the required things by
   the video specification at this stage.

   This results in the high level state changing to PLAYING or PAUSED
   depending on any user action that may have occurred.

   The decode thread, while in the DECODING state, plays audio and
   video, if the correct frame time comes around and the decoder
   play state is PLAYING.
   
a/v synchronisation is done by a combination of liboggplay and the
Decoder state machine. liboggplay ensures that a decoded frame of data
has both the audio samples and the YUV data for that period of time.

When a frame is decoded by the decode state machine it converts the
YUV encoded video to RGB and copies the sound data to an internal
FrameData object. This is stored in a queue of available decoded frames.
Included in the FrameData object is the time that that frame should
be displayed.

The display state machine keeps track of the time since the last frame it
played. After decoding a frame it checks if it is time to display the next
item in the decoded frame queue. If so, it pops the item off the queue
and displays it.

Ideally a/v sync would take into account the actual audio clock of the
audio hardware for the sync rather than using the system clock.
Unfortunately getting valid time data out of the audio hardware has proven
to be unreliable across platforms (and even distributions in Linux) depending
on audio hardware, audio backend etc. The current approach works fine in practice
and is a compromise until this issue can be sorted. The plan is to eventually
move to synchronising using the audio hardware.

To prevent audio skipping and framerate dropping it is very important to
make sure no blocking occurs during the decoding process and minimise
expensive time operations at the time a frame is to be displayed. This is
managed by immediately converting video data to RGB on decode (an expensive
operation to do at frame display time) and checking if the sound device will
not block before writing sound data to it.

Shutdown needs to ensure that the event posted to the decode
thread is completed. The decode thread can potentially block internally
inside liboggplay when reading, seeking, or its internal buffers containing
decoded data are full. When blocked in this manner a call from the main thread
to Shutdown() will hang.  

This is fixed with a protocol to ensure that the decode event cleanly
completes. The nsMediaStream that the nsChannelReader uses has a
Cancel() method. Calling this before Shutdown() will close any
internal streams or listeners resulting in blocked i/o completing with
an error, and all future i/o on the stream having an error.

This causes the decode thread to exit and Shutdown() can occur.

If the decode thread is seeking then the same Cancel() operation
causes an error to be returned from the seek call to liboggplay which
exits out of the seek operation, and stops the seek state running on the
decode thread.

If the decode thread is blocked due to internal decode buffers being
full, it is unblocked during the shutdown process by calling
oggplay_prepare_for_close.

In practice the OggPlay internal buffer should never fill as we retrieve and
process the frame immediately on decoding.

The Shutdown method on nsOggDecoder can spin the event loop as it waits
for threads to complete. Spinning the event loop is a bad thing to happen
during certain times like destruction of the media element. To work around
this the Shutdown method does nothing by queue an event to the main thread
to perform the actual Shutdown. This way the shutdown can occur at a safe
time. 

This means the owning object of a nsOggDecoder object *MUST* call Shutdown
when destroying the nsOggDecoder object.
*/
#if !defined(nsOggDecoder_h_)
#define nsOggDecoder_h_

#include "nsMediaDecoder.h"

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIThread.h"
#include "nsIChannel.h"
#include "nsChannelReader.h"
#include "nsIObserver.h"
#include "nsIFrame.h"
#include "nsAutoPtr.h"
#include "nsSize.h"
#include "prlog.h"
#include "prmon.h"
#include "gfxContext.h"
#include "gfxRect.h"
#include "oggplay/oggplay.h"

class nsAudioStream;
class nsOggDecodeStateMachine;
class nsOggStepDecodeEvent;

class nsOggDecoder : public nsMediaDecoder
{
  friend class nsOggDecodeStateMachine;
  friend class nsOggStepDecodeEvent;

  // ISupports
  NS_DECL_ISUPPORTS

  // nsIObserver
  NS_DECL_NSIOBSERVER

 public:
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

  nsOggDecoder();
  ~nsOggDecoder();
  
  virtual nsMediaDecoder* Clone() { return new nsOggDecoder(); }

  virtual PRBool Init(nsHTMLMediaElement* aElement);

  // This method must be called by the owning object before that
  // object disposes of this decoder object.
  virtual void Shutdown();
  
  virtual float GetCurrentTime();

  virtual nsresult Load(nsMediaStream* aStream,
                        nsIStreamListener** aListener);

  // Start playback of a video. 'Load' must have previously been
  // called.
  virtual nsresult Play();

  // Seek to the time position in (seconds) from the start of the video.
  virtual nsresult Seek(float time);

  virtual nsresult PlaybackRateChanged();

  virtual void Pause();
  virtual void SetVolume(float volume);
  virtual float GetDuration();

  virtual nsMediaStream* GetCurrentStream();
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal();

  virtual void NotifySuspendedStatusChanged();
  virtual void NotifyBytesDownloaded();
  virtual void NotifyDownloadEnded(nsresult aStatus);
  // Called by nsChannelReader on the decoder thread
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

  // Set the duration of the media resource in units of milliseconds.
  // This is called via a channel listener if it can pick up the duration
  // from a content header. Must be called from the main thread only.
  virtual void SetDuration(PRInt64 aDuration);

  // Set a flag indicating whether seeking is supported
  virtual void SetSeekable(PRBool aSeekable);

  // Return PR_TRUE if seeking is supported.
  virtual PRBool GetSeekable();

  // Returns the channel reader.
  nsChannelReader* GetReader() { return mReader; }

  virtual Statistics GetStatistics();

  // Suspend any media downloads that are in progress. Called by the
  // media element when it is sent to the bfcache. Call on the main
  // thread only.
  virtual void Suspend();

  // Resume any media downloads that have been suspended. Called by the
  // media element when it is restored from the bfcache. Call on the
  // main thread only.
  virtual void Resume();

  // Tells our nsMediaStream to put all loads in the background.
  virtual void MoveLoadsToBackground();

  // Stop the state machine thread and drop references to the thread,
  // state machine and channel reader.
  void Stop();

protected:

  // Returns the monitor for other threads to synchronise access to
  // state.
  PRMonitor* GetMonitor() 
  { 
    return mMonitor; 
  }

  // Return the current state. Can be called on any thread. If called from
  // a non-main thread, the decoder monitor must be held.
  PlayState GetState()
  {
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

  /****** 
   * The following methods must only be called on the main
   * thread.
   ******/

  // Change to a new play state. This updates the mState variable and
  // notifies any thread blocking on this object's monitor of the
  // change. Call on the main thread only.
  void ChangeState(PlayState aState);

  // Called when the metadata from the Ogg file has been read.
  // Call on the main thread only.
  void MetadataLoaded();

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

  // Calls mElement->UpdateReadyStateForData, telling it whether we have
  // data for the next frame and if we're buffering. Main thread only.
  void UpdateReadyStateForData();

  // Find the end of the cached data starting at the current decoder
  // position.
  PRInt64 GetDownloadPosition();

private:
  // Register/Unregister with Shutdown Observer. 
  // Call on main thread only.
  void RegisterShutdownObserver();
  void UnregisterShutdownObserver();

  // Notifies the element that decoding has failed.
  void DecodeError();

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

  // Thread to handle decoding of Ogg data.
  nsCOMPtr<nsIThread> mDecodeThread;

  // The current playback position of the media resource in units of
  // seconds. This is updated approximately at the framerate of the
  // video (if it is a video) or the callback period of the audio.
  // It is read and written from the main thread only.
  float mCurrentTime;

  // Volume that playback should start at.  0.0 = muted. 1.0 = full
  // volume.  Readable/Writeable from the main thread. Read from the
  // audio thread when it is first started to get the initial volume
  // level.
  float mInitialVolume;

  // Position to seek to when the seek notification is received by the
  // decoding thread. Written by the main thread and read via the
  // decoding thread. Synchronised using mPlayStateMonitor. If the
  // value is negative then no seek has been requested. When a seek is
  // started this is reset to negative.
  float mRequestedSeekTime;

  // Duration of the media resource. Set to -1 if unknown.
  // Set when the Ogg metadata is loaded. Accessed on the main thread
  // only.
  PRInt64 mDuration;

  // True if the media resource is seekable (server supports byte range
  // requests).
  PRPackedBool mSeekable;

  /******
   * The following member variables can be accessed from any thread.
   ******/

  // The state machine object for handling the decoding via
  // oggplay. It is safe to call methods of this object from other
  // threads. Its internal data is synchronised on a monitor. The
  // lifetime of this object is after mPlayState is LOADING and before
  // mPlayState is SHUTDOWN. It is safe to access it during this
  // period.
  nsCOMPtr<nsOggDecodeStateMachine> mDecodeStateMachine;

  // OggPlay object used to read data from a channel. Created on main
  // thread. Passed to liboggplay and the locking for multithreaded
  // access is handled by that library. Some methods are called from
  // the decoder thread, and the state machine for that thread keeps
  // a pointer to this reader. This is safe as the only methods called
  // are threadsafe (via the threadsafe nsMediaStream).
  nsAutoPtr<nsChannelReader> mReader;

  // Monitor for detecting when the video play state changes. A call
  // to Wait on this monitor will block the thread until the next
  // state change.
  PRMonitor* mMonitor;

  // Set to one of the valid play states. It is protected by the
  // monitor mMonitor. This monitor must be acquired when reading or
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
};

#endif
