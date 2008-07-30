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
    
  Presentation Thread This thread goes through the data decoded by the
    decode thread, generates the RGB buffer. If there is audio data it
    queues it for playing. This thread owns the audio device - all
    audio operations occur on this thread. The video data is actually
    displayed by a timer that goes off at the right framerate
    interval. This timer sends an Invalidate event to the frame.

Operations are performed in the threads by sending Events via the 
Dispatch method on the thread.

The general sequence of events with this objects is:

1) The video element calls Load on nsVideoDecoder. This creates the 
   threads and starts the channel for downloading the file. It sends
   an event to the decode thread to load the initial Ogg metadata. 
   These are the headers that give the video size, framerate, etc.
   It returns immediately to the calling video element.

2) When the Ogg metadata has been loaded by the decode thread it will
   call a method on the video element object to inform it that this step
   is done, so it can do the required things by the video specification
   at this stage. 

   It then queues an event to the decode thread to decode the first
   frame of Ogg data.

3) When the first frame of Ogg data has been successfully decoded it
   calls a method on the video element object to inform it that this
   step has been done, once again so it can do the required things by
   the video specification at this stage.

   It then queues an event to the decode thread to enter the standard
   decoding loop. This loop continuously reads data from the channel
   and decodes it. It does this by reading the data, decoding it, and 
   if there is more data to decode, queues itself back to the thread.

   The final step of the 'first frame event' is to notify a condition
   variable that we have decoded the first frame. The presentation thread
   uses this notification to know when to start displaying video.

4) At some point the video element calls Play() on the decoder object.
   This queues an event to the presentation thread which goes through
   the decoded data, displaying it if it is video, or playing it if it
   is audio. 

   Before starting this event will wait on the condition variable 
   indicating if the first frame has decoded. 

Pausing is handled by stopping the presentation thread. Resuming is 
handled by restarting the presentation thread. The decode thread never
gets too far ahead as it is throttled by the Oggplay library.
*/
#if !defined(nsOggDecoder_h___)
#define nsOggDecoder_h___

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIThread.h"
#include "nsIChannel.h"
#include "nsChannelReader.h"
#include "nsIObserver.h"
#include "nsIFrame.h"
#include "nsAutoPtr.h"
#include "nsSize.h"
#include "prlock.h"
#include "prcvar.h"
#include "prlog.h"
#include "gfxContext.h"
#include "gfxRect.h"
#include "oggplay/oggplay.h"
#include "nsVideoDecoder.h"

class nsAudioStream;
class nsVideoDecodeEvent;
class nsVideoPresentationEvent;
class nsChannelToPipeListener;

class nsOggDecoder : public nsVideoDecoder
{
  friend class nsVideoDecodeEvent;
  friend class nsVideoPresentationEvent;
  friend class nsChannelToPipeListener;

  // ISupports
  NS_DECL_ISUPPORTS

  // nsIObserver
  NS_DECL_NSIOBSERVER

 public:
  nsOggDecoder();
  PRBool Init();
  void Shutdown();
  ~nsOggDecoder();
  
  // Returns the current video frame width and height.
  // If there is no video frame, returns the given default size.
  nsIntSize GetVideoSize(nsIntSize defaultSize);
  double GetVideoFramerate();

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

  virtual void UpdateBytesDownloaded(PRUint32 aBytes);

protected:
  /******
   * The following methods must only be called on the presentation
   * thread.
   ******/

  // Find and render the first frame of video data. Call on 
  // presentation thread only.
  void DisplayFirstFrame();

  // Process one frame of video/audio data.
  // Call on presentation thread only.
  PRBool StepDisplay();

  // Process audio or video from one track of the Ogg stream.
  // Call on presentation thread only.
  void ProcessTrack(int aTrackNumber, OggPlayCallbackInfo* aTrackInfo);

  // Return the time in seconds that the video display is
  // synchronised to. This can be based on the current time of
  // the audio buffer if available, or the system clock. Call
  // on presentation thread only.
  double GetSyncTime();

  // Return true if the video is currently paused. Call on the
  // presentation thread only.
  PRBool IsPaused();

  // Process the video/audio data. Call on presentation thread only. 
  void HandleVideoData(int track_num, OggPlayVideoData* video_data);
  void HandleAudioData(OggPlayAudioData* audio_data, int size);

  // Pause the audio. Call on presentation thread only.
  void DoPause();

  // Initializes and opens the audio stream. Call from the
  // presentation thread only.
  void OpenAudioStream();

  // Closes and releases resources used by the audio stream.
  // Call from the presentation thread only.
  void CloseAudioStream();

  // Initializes the resources owned by the presentation thread,.
  // Call from the presentation thread only.
  void StartPresentationThread();

  /******
   * The following methods must only be called on the decode
   * thread.
   ******/

  // Loads the header information from the ogg resource and
  // stores the information about framerate, etc in member
  // variables. Must be called from the decoder thread only.
  void LoadOggHeaders();

  // Loads the First frame of the video data, making it available
  // in the RGB buffer. Must be called from the decoder thread only.
  void LoadFirstFrame();

  // Decode some data from the media file. This is placed in a 
  // buffer that is used by the presentation thread. Must
  // be called from the decoder thread only.
  PRBool StepDecoding();

  // Ensure that there is enough data buffered from the video 
  // that we can have a reasonable playback experience. Must
  // be called from the decoder thread only.
  void BufferData();

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

  // Called when the video file has completed downloading.
  // Call on the main thread only.
  void ResourceLoaded();

  // Called when the video has completed playing.
  // Call on the main thread only.
  void PlaybackCompleted();

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

private:
  // Starts the threads and timers that handle displaying the playing
  // video and invalidating the frame. Called on the main thread only.
  void StartPlaybackThreads();

  /******
   * The following member variables can be accessed from the
   * decoding thread only.
   ******/

  // Total number of bytes downloaded so far. 
  PRUint32 mBytesDownloaded;

  /******
   * The following members should be accessed on the main thread only
   ******/
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsChannelToPipeListener> mListener;

  // The URI of the current resource
  nsCOMPtr<nsIURI> mURI;

  // The audio stream resource. It should only be accessed from
  // the presentation thread.
  nsAutoPtr<nsAudioStream> mAudioStream;

  // The time that the next video frame should be displayed in
  // seconds. This is referenced from 0.0 which is the initial start
  // of the video stream.
  double mVideoNextFrameTime;

  // A load of the media resource is currently in progress. It is 
  // complete when the media metadata is loaded.
  PRPackedBool mLoadInProgress;

  // A boolean that indicates that once the load has completed loading
  // the metadata then it should start playing.
  PRPackedBool mPlayAfterLoad;

  /******
   * The following member variables can be accessed from any thread.
   ******/
  
  // Threads to handle decoding of Ogg data. Methods on these are
  // called from the main thread only, but they can be read from other
  // threads safely to see if they have been created/set.
  nsCOMPtr<nsIThread> mDecodeThread;
  nsCOMPtr<nsIThread> mPresentationThread;

  // Events for doing the Ogg decoding, displaying and repainting on
  // different threads. These are created on the main thread and are
  // dispatched to other threads. Threads only access them to dispatch
  // the event to other threads.
  nsCOMPtr<nsVideoDecodeEvent> mDecodeEvent;
  nsCOMPtr<nsVideoPresentationEvent> mPresentationEvent;

  // The time of the current frame from the video stream in
  // seconds. This is referenced from 0.0 which is the initial start
  // of the video stream. Set by the presentation thread, and
  // read-only from the main thread to get the current time value.
  float mVideoCurrentFrameTime;

  // Volume that playback should start at.  0.0 = muted. 1.0 = full
  // volume.  Readable/Writeable from the main thread. Read from the
  // audio thread when it is first started to get the initial volume
  // level.
  double mInitialVolume;

  // Audio data. These are initially set on the Decoder thread when
  // the metadata is loaded. They are read from the presentation
  // thread after this.
  PRInt32 mAudioRate;
  PRInt32 mAudioChannels;
  PRInt32 mAudioTrack;

  // Video data. Initially set on the Decoder thread when the metadata
  // is loaded. Read from the presentation thread after this.
  PRInt32 mVideoTrack;

  // liboggplay State. Passed to liboggplay functions on any
  // thread. liboggplay handles a lock internally for this.
  OggPlay* mPlayer;

  // OggPlay object used to read data from a channel. Created on main
  // thread. Passed to liboggplay and the locking for multithreaded
  // access is handled by that library.
  nsChannelReader* mReader;

  // True if the video playback is paused. Read/Write from the main
  // thread. Read from the decoder thread to not buffer data if
  // paused.
  PRPackedBool mPaused;

  // True if the first frame of data has been loaded. This member,
  // along with the condition variable and lock is used by threads 
  // that need to wait for the first frame to be loaded before 
  // performing some action. In particular it is used for 'autoplay' to
  // start playback on loading of the first frame.
  PRPackedBool mFirstFrameLoaded;
  PRCondVar* mFirstFrameCondVar;
  PRLock* mFirstFrameLock;

  // System time in seconds since video start, or last pause/resume.
  // Used for synching video framerate to the system clock if there is
  // no audio hardware or no audio track. Written by main thread, read
  // by presentation thread to handle frame rate synchronisation.
  double mSystemSyncSeconds;

  // The media resource has been completely loaded into the pipe. No
  // need to attempt to buffer if it starves. Written on the main
  // thread, read from the decoding thread.
  PRPackedBool mResourceLoaded;

  // PR_TRUE if the metadata for the video has been loaded. Written on
  // the main thread, read from the decoding thread.
  PRPackedBool mMetadataLoaded;
};

#endif
