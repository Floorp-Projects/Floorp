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
#if !defined(nsMediaDecoder_h_)
#define nsMediaDecoder_h_

#include "nsIObserver.h"
#include "nsIPrincipal.h"
#include "nsSize.h"
#include "prlog.h"
#include "gfxContext.h"
#include "gfxRect.h"
#include "nsITimer.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gVideoDecoderLog;
#define LOG(type, msg) PR_LOG(gVideoDecoderLog, type, msg)
#else
#define LOG(type, msg)
#endif

class nsHTMLMediaElement;

// All methods of nsMediaDecoder must be called from the main thread only
// with the exception of SetRGBData. The latter can be called from any thread.
class nsMediaDecoder : public nsIObserver
{
 public:
  nsMediaDecoder();
  virtual ~nsMediaDecoder();

  // Initialize the logging object
  static nsresult InitLogger();

  // Perform any initialization required for the decoder.
  // Return PR_TRUE on successful initialisation, PR_FALSE
  // on failure.
  virtual PRBool Init();

  // Return the current URI being played or downloaded.
  virtual void GetCurrentURI(nsIURI** aURI) = 0;

  // Return the principal of the current URI being played or downloaded.
  virtual nsIPrincipal* GetCurrentPrincipal() = 0;

  // Return the time position in the video stream being
  // played measured in seconds.
  virtual float GetCurrentTime() = 0;

  // Seek to the time position in (seconds) from the start of the video.
  virtual nsresult Seek(float time) = 0;

  // Called by the element when the playback rate has been changed.
  // Adjust the speed of the playback, optionally with pitch correction,
  // when this is called.
  virtual nsresult PlaybackRateChanged() = 0;

  // Return the duration of the video in seconds.
  virtual float GetDuration() = 0;
  
  // Pause video playback.
  virtual void Pause() = 0;

  // Return the current audio volume that the video plays at. 
  // This is a value form 0 through to 1.0.
  virtual float GetVolume() = 0;

  // Set the audio volume. It should be a value from 0 to 1.0.
  virtual void SetVolume(float volume) = 0;

  // Start playback of a video. 'Load' must have previously been
  // called.
  virtual nsresult Play() = 0;

  // Stop playback of a video, and stop download of video stream.
  virtual void Stop() = 0;

  // Start downloading the video. Decode the downloaded data up to the
  // point of the first frame of data.
  // Exactly one of aURI and aChannel must be null. aListener must be
  // null if and only if aChannel is.
  virtual nsresult Load(nsIURI* aURI,
                        nsIChannel* aChannel,
                        nsIStreamListener **aListener) = 0;

  // Draw the latest video data. This is done
  // here instead of in nsVideoFrame so that the lock around the
  // RGB buffer doesn't have to be exposed publically.
  // The current video frame is drawn to fill aRect.
  // Called in the main thread only.
  virtual void Paint(gfxContext* aContext, const gfxRect& aRect);

  // Called when the video file has completed downloading.
  virtual void ResourceLoaded() = 0;

  // Called if the media file encounters a network error.
  virtual void NetworkError() = 0;

  // Call from any thread safely. Return PR_TRUE if we are currently
  // seeking in the media resource.
  virtual PRBool IsSeeking() const = 0;

  // Return PR_TRUE if the decoder has reached the end of playback.
  // Call in the main thread only.
  virtual PRBool IsEnded() const = 0;

  // Return the current number of bytes loaded from the video file.
  // This is used for progress events.
  virtual PRUint64 GetBytesLoaded() = 0;

  // Return the size of the video file in bytes. Return 0 if the
  // size is unknown or the stream is infinite.
  virtual PRInt64 GetTotalBytes() = 0;

  // Set the size of the video file in bytes.
  virtual void SetTotalBytes(PRInt64 aBytes) = 0;

  // Set a flag indicating whether seeking is supported
  virtual void SetSeekable(PRBool aSeekable) = 0;

  // Return PR_TRUE if seeking is supported.
  virtual PRBool GetSeekable() = 0;

  // Called when the HTML DOM element is bound.
  virtual void ElementAvailable(nsHTMLMediaElement* anElement);

  // Called when the HTML DOM element is unbound.
  virtual void ElementUnavailable();

  // Invalidate the frame.
  virtual void Invalidate();

  // Fire progress events if needed according to the time and byte
  // constraints outlined in the specification. aTimer is PR_TRUE
  // if the method is called as a result of the progress timer rather
  // than the result of downloaded data.
  virtual void Progress(PRBool aTimer);

  // Keep track of the number of bytes downloaded
  virtual void UpdateBytesDownloaded(PRUint64 aBytes) = 0;

  // Cleanup internal data structures. Must be called on the main
  // thread by the owning object before that object disposes of this object.  
  virtual void Shutdown();

protected:

  // Start timer to update download progress information.
  nsresult StartProgress();

  // Stop progress information timer.
  nsresult StopProgress();

  // Set the RGB width, height and framerate. The passed RGB buffer is
  // copied to the mRGB buffer. This also allocates the mRGB buffer if
  // needed.
  // This is the only nsMediaDecoder method that may be called 
  // from threads other than the main thread.
  // It must be called with the mVideoUpdateLock held.
  void SetRGBData(PRInt32 aWidth, 
                  PRInt32 aHeight, 
                  float aFramerate, 
                  unsigned char* aRGBBuffer);

protected:
  // Timer used for updating progress events 
  nsCOMPtr<nsITimer> mProgressTimer;

  // The element is not reference counted. Instead the decoder is
  // notified when it is able to be used. It should only ever be
  // accessed from the main thread.
  nsHTMLMediaElement* mElement;

  // RGB data for last decoded frame of video data.
  // The size of the buffer is mRGBWidth*mRGBHeight*4 bytes and
  // contains bytes in RGBA format.
  nsAutoArrayPtr<unsigned char> mRGB;

  PRInt32 mRGBWidth;
  PRInt32 mRGBHeight;

  // Time that the last progress event was fired. Read/Write from the
  // main thread only.
  PRIntervalTime mProgressTime;

  // Time that data was last read from the media resource. Used for
  // computing if the download has stalled. A value of 0 indicates that
  // a stall event has already fired and not to fire another one until
  // more data is received. Read/Write from the main thread only.
  PRIntervalTime mDataTime;

  // Has our size changed since the last repaint?
  PRPackedBool mSizeChanged;

  // Lock around the video RGB, width and size data. This
  // is used in the decoder backend threads and the main thread
  // to ensure that repainting the video does not use these
  // values while they are out of sync (width changed but
  // not height yet, etc).
  // Backends that are updating the height, width or writing
  // to the RGB buffer must obtain this lock first to ensure that
  // the video element does not use video data or sizes that are
  // in the midst of being changed.
  PRLock* mVideoUpdateLock;

  // Framerate of video being displayed in the element
  // expressed in numbers of frames per second.
  float mFramerate;

  // True if the decoder is being shutdown. At this point all events that
  // are currently queued need to return immediately to prevent javascript
  // being run that operates on the element and decoder during shutdown.
  // Read/Write from the main thread only.
  PRPackedBool mShuttingDown;

  // True if the decoder is currently in the Stop() method. This is used to
  // prevent recursive calls into Stop while it is spinning the event loop
  // waiting for the playback event loop to shutdown. Read/Write from the
  // main thread only.
  PRPackedBool mStopping;

  // True when seeking or otherwise moving the play position around in
  // such a manner that progress event data is inaccurate. This is set
  // before a seek or during loading of metadata to prevent the progress indicator
  // from jumping around. Accessed on the main thread only.
  PRPackedBool mIgnoreProgressData;
};

#endif
