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
#if !defined(nsMediaStream_h_)
#define nsMediaStream_h_

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIChannel.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "prlock.h"

class nsMediaDecoder;

// An abstract class that implements the low level functionality of nsMediaStream to open, close,
// read and seek in streams. nsMediaStream constructs a concrete implementation of this class based
// on the channel type to enable efficient seeking, reading and writing optimized for the channel type.
class nsStreamStrategy 
{
public:
 nsStreamStrategy(nsMediaDecoder* aDecoder, nsIChannel* aChannel, nsIURI* aURI) :
    mDecoder(aDecoder),
    mChannel(aChannel),
    mURI(aURI),
    mLock(nsnull)  
  {
    MOZ_COUNT_CTOR(nsStreamStrategy);
    mLock = PR_NewLock();
  }

  virtual ~nsStreamStrategy()
  {
    PR_DestroyLock(mLock);
    MOZ_COUNT_DTOR(nsStreamStrategy);
  }

  // These methods have the same thread calling requirements 
  // as those with the same name in nsMediaStream

  /**
   * @param aStreamListener if null, the strategy should open mChannel
   * itself. Otherwise, mChannel is already open and the strategy
   * should just return its stream listener in aStreamListener (or set
   * *aStreamListener to null, if it doesn't need a listener).
   */
  virtual nsresult Open(nsIStreamListener** aStreamListener) = 0;
  virtual nsresult Close() = 0;
  virtual nsresult Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes) = 0;
  virtual nsresult Seek(PRInt32 aWhence, PRInt64 aOffset) = 0;
  virtual PRInt64  Tell() = 0;
  virtual PRUint32 Available() = 0;
  virtual float    DownloadRate() = 0;
  virtual void     Cancel() { }
  virtual nsIPrincipal* GetCurrentPrincipal() = 0;

protected:
  // This is not an nsCOMPointer to prevent a circular reference
  // between the decoder to the media stream object. The stream never
  // outlives the lifetime of the decoder.
  nsMediaDecoder* mDecoder;

  // Channel used to download the media data. Must be accessed
  // from the main thread only.
  nsCOMPtr<nsIChannel> mChannel;

  // URI in case the stream needs to be re-opened. Access from
  // main thread only.
  nsCOMPtr<nsIURI> mURI;

  // This lock handles synchronisation between calls to Close() and
  // the Read, Seek, etc calls. Close must not be called while a
  // Read or Seek is in progress since it resets various internal
  // values to null.
  PRLock* mLock;
};

/* 
   Provides the ability to open, read and seek into a media stream
   (audio, video). Handles the underlying machinery to do range
   requests, etc as needed by the actual stream type. Instances of
   this class must be created on the main thread. 

   Open, Close and Cancel must be called on the main thread only. Once
   the stream is open the remaining methods (except for Close and
   Cancel) may be called on another thread which may be a non main
   thread. They may not be called on multiple other threads though. In
   the case of the Ogg Decoder they are called on the Decode thread
   for example. You must ensure that no threads are calling these
   methods once Close is called.
*/
class nsMediaStream
{
 public:
  nsMediaStream();
  ~nsMediaStream();

  /**
   * Create a channel for the stream, reading data from the 
   * media resource at the URI. Call on main thread only.
   * @param aChannel if non-null, this channel is used and aListener
   * is set to the listener we want for the channel. aURI must
   * be the URI for the channel, obtained via NS_GetFinalChannelURI.
   */
  nsresult Open(nsMediaDecoder* aDecoder, nsIURI* aURI,
                nsIChannel* aChannel, nsIStreamListener** aListener);

  // Close the stream, stop any listeners, channels, etc.
  // Call on main thread only.
  nsresult Close();

  // Read up to aCount bytes from the stream. The buffer must have
  // enough room for at least aCount bytes. Stores the number of
  // actual bytes read in aBytes (0 on end of file). Can be called
  // from any thread. May read less than aCount bytes if the number of
  // available bytes is less than aCount. Always check *aBytes after
  // read, and call again if necessary.
  nsresult Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes);

  // Seek to the given bytes offset in the stream. aWhence can be
  // one of:
  //   NS_SEEK_SET
  //   NS_SEEK_CUR
  //   NS_SEEK_END
  //
  // Can be called from any thread.
  nsresult Seek(PRInt32 aWhence, PRInt64 aOffset);

  // Report the current offset in bytes from the start of the stream.
  // Can be called from any thread.
  PRInt64 Tell();

  // Return the number of bytes available in the stream that can be
  // read without blocking. Can be called from any thread.
  PRUint32 Available();

  // Return the current download rate in bytes per second. Returns less than
  // zero if the download has completed. Can be called from any
  // thread.
  float DownloadRate();

  // Return the current playback rate in bytes per second. Can be
  // called from any thread.
  float PlaybackRate();

  // Cancels any currently blocking request and forces that request to
  // return an error. Call on main thread only.
  void Cancel();

  // Call on main thread only.
  nsIPrincipal* GetCurrentPrincipal();

 private:
  // Strategy object that is used for the handling seeking, etc
  // Accessed from any thread. Set on the Open call on the main thread
  // only. Open is always called first on the main thread before any
  // other calls from other threads.
  nsAutoPtr<nsStreamStrategy> mStreamStrategy;

  // Time used for computing average playback rate. Written on the 
  // main thread only during the Open call. Read from any thread during
  // calls to PlaybackRate() - which can only ever happen after Open.
  PRIntervalTime mPlaybackRateStart;

  // Bytes downloaded for average playback rate computation. Initialized
  // on the main thread during Open(). After that it is read and written
  // possibly on a different thread, but exclusively from that
  // thread. In the case of the Ogg Decoder, it is the Decoder thread.
  PRUint32 mPlaybackRateCount;
};

#endif
