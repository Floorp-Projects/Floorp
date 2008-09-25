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
Each video element has two threads. They are:
  Decode Thread
    This thread owns the resources for downloading
    and reading the video file. It goes through the file, decoding
    the theora and vorbis data. It uses Oggplay to do the decoding.
    It indirectly uses an nsMediaStream to do the file reading and
    seeking via Oggplay. All file reads and seeks must occur on this
    thread only.
    
  Display Thread 
    This thread goes through the data decoded by the decode thread,
    and generates the RGB buffer. If there is audio data it queues it
    for playing. This thread owns the audio device - all audio
    operations occur on this thread. Once the RGB buffer is available
    an event is sent to the main thread to invalidate the frame.

The advantage of using two threads is that when decoding blocks for
i/o the display thread can continue to update frames from the buffer
of decoded frames. It also prevents audio glitches and video skipping
if decoding takes slightly longer than the framerate.

When the threads are created an event is dispatched to it. The event
runs for the lifetime of the playback of the resource. The events
synchronise via a single monitor held by the nsOggDecoder object.

Each event contains a Run method which consists of an infinite loop
that checks the state that the state machine is in and processes
operations on that state.

The nsOggDecodeStateMachine class is the event that gets dispatched to
the decode thread. It has the following states:

DECODING_METADATA
  The Ogg headers are being loaded, and things like framerate, etc are
  being decoded.  
DECODING_FIRSTFRAME
  The first frame of audio/video data is being decoded.
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
|        | Shutdown()
  v        >-------------------->--------------------------|
  |                                                        |
DECODING_FIRSTFRAME                                        v
  |        | Shutdown()                                    |
  v        >-------------------->--------------------------|
  |  |------------->----->------------------------|        v
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

The nsOggDisplayStateMachine class is the event that gets dispatched
to the display thread. It doesn't have an explicit state but it
computes its state based on the states of the decode state machine and
the main play state. It is either playing, paused or finished.

The Main thread controls the above state machines by sending events
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

player LOADING   decoder DECODING_METADATA, DECODING_FIRSTFRAME
player PLAYING   decoder DECODING, BUFFERING, SEEKING, COMPLETED
player PAUSED    decoder DECODING, BUFFERING, SEEKING, COMPLETED
player SEEKING   decoder SEEKING
player COMPLETED decoder SHUTDOWN
player SHUTDOWN  decoder SHUTDOWN

The general sequence of events with these objects is:

1) The video element calls Load on nsMediaDecoder. This creates the
   decode thread and starts the channel for downloading the file. It
   instantiates and starts the Decode state machines. The high level
   LOADING state is entered, which results in the decode state machine
   to start decoding metadata. These are the headers that give the
   video size, framerate, etc.  It returns immediately to the calling
   video element.

2) When the Ogg metadata has been loaded by the decode thread it will
   call a method on the video element object to inform it that this
   step is done, so it can do the things required by the video
   specification at this stage. At this point the display thread is
   started with the display state machine dispatched to it. The
   decoder then continues to decode the first frame of data.

3) When the first frame of Ogg data has been successfully decoded it
   calls a method on the video element object to inform it that this
   step has been done, once again so it can do the required things by
   the video specification at this stage.

   This results in the high level state changing to PLAYING or PAUSED
   depending on any user action that may have occurred.

   That transition causes the Display thread to start displaying data
   if the PLAYING state is in effect and the decoder is DECODING.
   
a/v synchronisation is done by a combination of liboggplay and the
Display state machine. liboggplay ensures that a decoded frame of data
has both the audio samples and the YUV data for that period of time.
The display state machine computes the time since the last frame it
played and enters the IDLE state if it needs to delay until the next
frame time is reached.

Ideally a/v sync would take into account the actual audio clock of the
audio hardware for the sync rather than computing the time between
HandleVideoData calls. Unfortunately getting valid time data out of
the audio hardware has proven to be unreliable across platforms (and
even distributions in Linux) depending on audio hardware, audio
backend etc. The current approach works fine in practice and is a
compromise until this issue can be sorted.

Shutdown needs to ensure that all events posted to the decode and
display threads are cleanly completed. The decode thread can
potentially block internally inside liboggplay when reading, seeking,
or its internal buffers containing decoded data are full. When blocked
in this manner a call from the main thread to Shutdown() will hang.

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

To reduce the chance of the internal decode buffer filling and
blocking, the return value of the oggplay decode step tells us if the
buffer is full and will block on the next call. If this is the case we
Wait() on the monitor. The display thread Notifies the monitor when it
has displayed a frame allowing us to call the decode again without
blocking.

*/
#if !defined(nsOggDecoder_h_)
#define nsOggDecoder_h_

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
#include "nsMediaDecoder.h"

class nsAudioStream;
class nsOggDecodeStateMachine;
class nsOggDisplayStateMachine;

class nsOggDecoder : public nsMediaDecoder
{
  friend class nsOggDecodeStateMachine;
  friend class nsOggDisplayStateMachine;

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
  PRBool Init();
  void Shutdown();
  
  float GetCurrentTime();

  // Start downloading the video at the given URI. Decode
  // the downloaded data up to the point of the first frame
  // of data. 
  nsresult Load(nsIURI* aURI);

  // Start playback of a video. 'Load' must have previously been
  // called.
  nsresult Play();

  // Stop playback of a video, and stop download of video stream.
  virtual void Stop();

  // Seek to the time position in (seconds) from the start of the video.
  nsresult Seek(float time);

  nsresult PlaybackRateChanged();

  void Pause();
  float GetVolume();
  void SetVolume(float volume);
  float GetDuration();

  void GetCurrentURI(nsIURI** aURI);
  nsIPrincipal* GetCurrentPrincipal();

  virtual void UpdateBytesDownloaded(PRUint32 aBytes);

  // Called when the video file has completed downloading.
  // Call on the main thread only.
  void ResourceLoaded();

  // Call from any thread safely. Return PR_TRUE if we are currently
  // seeking in the media resource.
  virtual PRBool IsSeeking() const;

protected:
  // Change to a new play state. This updates the mState variable and
  // notifies any thread blocking on this objects monitor of the
  // change. Can be called on any thread.
  void ChangeState(PlayState aState);

  // Returns the monitor for other threads to synchronise access to
  // state
  PRMonitor* GetMonitor() 
  { 
    return mMonitor; 
  }

  // Return the current state. The caller must have entered the
  // monitor.
  PlayState GetState()
  {
    return mPlayState;
  }

  /****** 
   * The following methods must only be called on the main
   * thread.
   ******/

  // Called when the metadata from the Ogg file has been read.
  // Call on the main thread only.
  void MetadataLoaded();

  // Called when the first frame has been loaded.
  // Call on the main thread only.
  void FirstFrameLoaded();

  // Called when the video has completed playing.
  // Call on the main thread only.
  void PlaybackEnded();

  // Return the current number of bytes loaded from the video file.
  // This is used for progress events.
  virtual PRUint32 GetBytesLoaded();

  // Return the size of the video file in bytes.
  // This is used for progress events.
  virtual PRUint32 GetTotalBytes();

  // Buffering of data has stopped. Inform the element on the main
  // thread.
  void BufferingStopped();

  // Buffering of data has started. Inform the element on the main
  // thread.
  void BufferingStarted();

  // Seeking has stopped. Inform the element on the main
  // thread.
  void SeekingStopped();

  // Seeking has started. Inform the element on the main
  // thread.
  void SeekingStarted();

private:
  // Register/Unregister with Shutdown Observer. 
  // Call on main thread only.
  void RegisterShutdownObserver();
  void UnregisterShutdownObserver();


  /******
   * The following members should be accessed on the main thread only
   ******/
  // Total number of bytes downloaded so far. 
  PRUint32 mBytesDownloaded;

  // The URI of the current resource
  nsCOMPtr<nsIURI> mURI;

  // Threads to handle decoding of Ogg data.
  nsCOMPtr<nsIThread> mDecodeThread;
  nsCOMPtr<nsIThread> mDisplayThread;

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
  float mSeekTime;

  // True if we are registered with the observer service for shutdown.
  PRPackedBool mNotifyOnShutdown;

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

  // The state machine object for handling the display via oggplay.
  // It is safe to call methods of this object from other threads. Its
  // internal data is synchronised on a monitor. The lifetime of this
  // object is after mPlayState is LOADING and before mPlayState is
  // SHUTDOWN. It is safe to access it during this period.
  nsCOMPtr<nsOggDisplayStateMachine> mDisplayStateMachine;
  
  // OggPlay object used to read data from a channel. Created on main
  // thread. Passed to liboggplay and the locking for multithreaded
  // access is handled by that library. Some methods are called from
  // the decoder thread, and the state machine for that thread keeps
  // a pointer to this reader. This is safe as the only methods called
  // are threadsafe (via the threadsafe nsMediaStream).
  nsChannelReader* mReader;

  // Monitor for detecting when the video play state changes. A call
  // to Wait on this monitor will block the thread until the next
  // state change.
  PRMonitor* mMonitor;

  // Set to one of the valid play states. It is protected by the
  // monitor mMonitor. This monitor must be acquired when reading or
  // writing the state. Any change to the state must call NotifyAll on
  // the monitor.
  PlayState mPlayState;

  // The state to change to after a seek or load operation. It is
  // protected by the monitor mMonitor. This monitor must be acquired
  // when reading or writing the state. Any change to the state must
  // call NotifyAll on the monitor.
  PlayState mNextState;
};

#endif
