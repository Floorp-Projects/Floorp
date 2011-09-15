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

#include "mozilla/Mutex.h"
#include "mozilla/XPCOM.h"
#include "nsIChannel.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsIStreamListener.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsMediaCache.h"

// For HTTP seeking, if number of bytes needing to be
// seeked forward is less than this value then a read is
// done rather than a byte range request.
static const PRInt64 SEEK_VS_READ_THRESHOLD = 32*1024;

static const PRUint32 HTTP_REQUESTED_RANGE_NOT_SATISFIABLE_CODE = 416;

class nsMediaDecoder;

/**
 * This class is useful for estimating rates of data passing through
 * some channel. The idea is that activity on the channel "starts"
 * and "stops" over time. At certain times data passes through the
 * channel (usually while the channel is active; data passing through
 * an inactive channel is ignored). The GetRate() function computes
 * an estimate of the "current rate" of the channel, which is some
 * kind of average of the data passing through over the time the
 * channel is active.
 * 
 * All methods take "now" as a parameter so the user of this class can
 * control the timeline used.
 */
class nsChannelStatistics {
public:
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  nsChannelStatistics() { Reset(); }
  void Reset() {
    mLastStartTime = TimeStamp();
    mAccumulatedTime = TimeDuration(0);
    mAccumulatedBytes = 0;
    mIsStarted = PR_FALSE;
  }
  void Start(TimeStamp aNow) {
    if (mIsStarted)
      return;
    mLastStartTime = aNow;
    mIsStarted = PR_TRUE;
  }
  void Stop(TimeStamp aNow) {
    if (!mIsStarted)
      return;
    mAccumulatedTime += aNow - mLastStartTime;
    mIsStarted = PR_FALSE;
  }
  void AddBytes(PRInt64 aBytes) {
    if (!mIsStarted) {
      // ignore this data, it may be related to seeking or some other
      // operation we don't care about
      return;
    }
    mAccumulatedBytes += aBytes;
  }
  double GetRateAtLastStop(PRPackedBool* aReliable) {
    double seconds = mAccumulatedTime.ToSeconds();
    *aReliable = seconds >= 1.0;
    if (seconds <= 0.0)
      return 0.0;
    return static_cast<double>(mAccumulatedBytes)/seconds;
  }
  double GetRate(TimeStamp aNow, PRPackedBool* aReliable) {
    TimeDuration time = mAccumulatedTime;
    if (mIsStarted) {
      time += aNow - mLastStartTime;
    }
    double seconds = time.ToSeconds();
    *aReliable = seconds >= 3.0;
    if (seconds <= 0.0)
      return 0.0;
    return static_cast<double>(mAccumulatedBytes)/seconds;
  }
private:
  PRInt64      mAccumulatedBytes;
  TimeDuration mAccumulatedTime;
  TimeStamp    mLastStartTime;
  PRPackedBool mIsStarted;
};

// Represents a section of contiguous media, with a start and end offset.
// Used to denote ranges of data which are cached.
class nsByteRange {
public:
  nsByteRange() : mStart(0), mEnd(0) {}

  nsByteRange(PRInt64 aStart, PRInt64 aEnd)
    : mStart(aStart), mEnd(aEnd)
  {
    NS_ASSERTION(mStart < mEnd, "Range should end after start!");
  }

  PRBool IsNull() const {
    return mStart == 0 && mEnd == 0;
  }

  PRInt64 mStart, mEnd;
};

/*
   Provides the ability to open, read and seek into a media stream
   (audio, video). Handles the underlying machinery to do range
   requests, etc as needed by the actual stream type. Instances of
   this class must be created on the main thread. 

   Most methods must be called on the main thread only. Read, Seek and
   Tell must only be called on non-main threads. In the case of the Ogg
   Decoder they are called on the Decode thread for example. You must
   ensure that no threads are calling these methods once Close is called.

   Instances of this class are explicitly managed. 'delete' it when done.
*/
class nsMediaStream 
{
public:
  virtual ~nsMediaStream()
  {
    MOZ_COUNT_DTOR(nsMediaStream);
  }

  // The following can be called on the main thread only:
  // Get the decoder
  nsMediaDecoder* Decoder() { return mDecoder; }
  // Get the URI
  nsIURI* URI() { return mURI; }
  // Close the stream, stop any listeners, channels, etc.
  // Cancels any currently blocking Read request and forces that request to
  // return an error.
  virtual nsresult Close() = 0;
  // Suspend any downloads that are in progress.
  // If aCloseImmediately is set, resources should be released immediately
  // since we don't expect to resume again any time soon. Otherwise we
  // may resume again soon so resources should be held for a little
  // while.
  virtual void Suspend(PRBool aCloseImmediately) = 0;
  // Resume any downloads that have been suspended.
  virtual void Resume() = 0;
  // Get the current principal for the channel
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal() = 0;
  // Create a new stream of the same type that refers to the same URI
  // with a new channel. Any cached data associated with the original
  // stream should be accessible in the new stream too.
  virtual nsMediaStream* CloneData(nsMediaDecoder* aDecoder) = 0;

  // These methods are called off the main thread.
  // The mode is initially MODE_PLAYBACK.
  virtual void SetReadMode(nsMediaCacheStream::ReadMode aMode) = 0;
  // This is the client's estimate of the playback rate assuming
  // the media plays continuously. The cache can't guess this itself
  // because it doesn't know when the decoder was paused, buffering, etc.
  virtual void SetPlaybackRate(PRUint32 aBytesPerSecond) = 0;
  // Read up to aCount bytes from the stream. The buffer must have
  // enough room for at least aCount bytes. Stores the number of
  // actual bytes read in aBytes (0 on end of file).
  // May read less than aCount bytes if the number of
  // available bytes is less than aCount. Always check *aBytes after
  // read, and call again if necessary.
  virtual nsresult Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes) = 0;
  // Seek to the given bytes offset in the stream. aWhence can be
  // one of:
  //   NS_SEEK_SET
  //   NS_SEEK_CUR
  //   NS_SEEK_END
  //
  // In the Http strategy case the cancel will cause the http
  // channel's listener to close the pipe, forcing an i/o error on any
  // blocked read. This will allow the decode thread to complete the
  // event.
  // 
  // In the case of a seek in progress, the byte range request creates
  // a new listener. This is done on the main thread via seek
  // synchronously dispatching an event. This avoids the issue of us
  // closing the listener but an outstanding byte range request
  // creating a new one. They run on the same thread so no explicit
  // synchronisation is required. The byte range request checks for
  // the cancel flag and does not create a new channel or listener if
  // we are cancelling.
  //
  // The default strategy does not do any seeking - the only issue is
  // a blocked read which it handles by causing the listener to close
  // the pipe, as per the http case.
  //
  // The file strategy doesn't block for any great length of time so
  // is fine for a no-op cancel.
  virtual nsresult Seek(PRInt32 aWhence, PRInt64 aOffset) = 0;
  // Report the current offset in bytes from the start of the stream.
  virtual PRInt64 Tell() = 0;
  // Moves any existing channel loads into the background, so that they don't
  // block the load event. Any new loads initiated (for example to seek)
  // will also be in the background.
  void MoveLoadsToBackground();

  // These can be called on any thread.
  // Cached blocks associated with this stream will not be evicted
  // while the stream is pinned.
  virtual void Pin() = 0;
  virtual void Unpin() = 0;
  // Get the estimated download rate in bytes per second (assuming no
  // pausing of the channel is requested by Gecko).
  // *aIsReliable is set to true if we think the estimate is useful.
  virtual double GetDownloadRate(PRPackedBool* aIsReliable) = 0;
  // Get the length of the stream in bytes. Returns -1 if not known.
  // This can change over time; after a seek operation, a misbehaving
  // server may give us a resource of a different length to what it had
  // reported previously --- or it may just lie in its Content-Length
  // header and give us more or less data than it reported. We will adjust
  // the result of GetLength to reflect the data that's actually arriving.
  virtual PRInt64 GetLength() = 0;
  // Returns the offset of the first byte of cached data at or after aOffset,
  // or -1 if there is no such cached data.
  virtual PRInt64 GetNextCachedData(PRInt64 aOffset) = 0;
  // Returns the end of the bytes starting at the given offset
  // which are in cache.
  virtual PRInt64 GetCachedDataEnd(PRInt64 aOffset) = 0;
  // Returns true if all the data from aOffset to the end of the stream
  // is in cache. If the end of the stream is not known, we return false.
  virtual PRBool IsDataCachedToEndOfStream(PRInt64 aOffset) = 0;
  // Returns true if this stream is suspended by the cache because the
  // cache is full. If true then the decoder should try to start consuming
  // data, otherwise we may not be able to make progress.
  // nsMediaDecoder::NotifySuspendedStatusChanged is called when this
  // changes.
  virtual PRBool IsSuspendedByCache() = 0;
  // Returns true if this stream has been suspended.
  virtual PRBool IsSuspended() = 0;
  // Reads only data which is cached in the media cache. If you try to read
  // any data which overlaps uncached data, or if aCount bytes otherwise can't
  // be read, this function will return failure. This function be called from
  // any thread, and it is the only read operation which is safe to call on
  // the main thread, since it's guaranteed to be non blocking.
  virtual nsresult ReadFromCache(char* aBuffer,
                                 PRInt64 aOffset,
                                 PRUint32 aCount) = 0;

  /**
   * Create a stream, reading data from the media resource via the
   * channel. Call on main thread only.
   * The caller must follow up by calling aStream->Open.
   */
  static nsMediaStream* Create(nsMediaDecoder* aDecoder, nsIChannel* aChannel);

  /**
   * Open the stream. This creates a stream listener and returns it in
   * aStreamListener; this listener needs to be notified of incoming data.
   */
  virtual nsresult Open(nsIStreamListener** aStreamListener) = 0;

  /**
   * Fills aRanges with ByteRanges representing the data which is cached
   * in the media cache. Stream should be pinned during call and while
   * aRanges is being used.
   */
  virtual nsresult GetCachedRanges(nsTArray<nsByteRange>& aRanges) = 0;

protected:
  nsMediaStream(nsMediaDecoder* aDecoder, nsIChannel* aChannel, nsIURI* aURI) :
    mDecoder(aDecoder),
    mChannel(aChannel),
    mURI(aURI),
    mLoadInBackground(PR_FALSE)
  {
    MOZ_COUNT_CTOR(nsMediaStream);
  }

  // Set the request's load flags to aFlags.  If the request is part of a
  // load group, the request is removed from the group, the flags are set, and
  // then the request is added back to the load group.
  void ModifyLoadFlags(nsLoadFlags aFlags);

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

  // PR_TRUE if MoveLoadsToBackground() has been called, i.e. the load event
  // has been fired, and all channel loads will be in the background.
  PRPackedBool mLoadInBackground;
};

/**
 * This is the nsMediaStream implementation that wraps Necko channels.
 * Much of its functionality is actually delegated to nsMediaCache via
 * an underlying nsMediaCacheStream.
 *
 * All synchronization is performed by nsMediaCacheStream; all off-main-
 * thread operations are delegated directly to that object.
 */
class nsMediaChannelStream : public nsMediaStream
{
  typedef mozilla::Mutex Mutex;

public:
  nsMediaChannelStream(nsMediaDecoder* aDecoder, nsIChannel* aChannel, nsIURI* aURI);
  ~nsMediaChannelStream();

  // These are called on the main thread by nsMediaCache. These must
  // not block or grab locks, because the media cache is holding its lock.
  // Notify that data is available from the cache. This can happen even
  // if this stream didn't read any data, since another stream might have
  // received data for the same resource.
  void CacheClientNotifyDataReceived();
  // Notify that we reached the end of the stream. This can happen even
  // if this stream didn't read any data, since another stream might have
  // received data for the same resource.
  void CacheClientNotifyDataEnded(nsresult aStatus);

  // These are called on the main thread by nsMediaCache. These shouldn't block,
  // but they may grab locks --- the media cache is not holding its lock
  // when these are called.
  // Start a new load at the given aOffset. The old load is cancelled
  // and no more data from the old load will be notified via
  // nsMediaCacheStream::NotifyDataReceived/Ended.
  // This can fail.
  nsresult CacheClientSeek(PRInt64 aOffset, PRBool aResume);
  // Suspend the current load since data is currently not wanted
  nsresult CacheClientSuspend();
  // Resume the current load since data is wanted again
  nsresult CacheClientResume();

  // Main thread
  virtual nsresult Open(nsIStreamListener** aStreamListener);
  virtual nsresult Close();
  virtual void     Suspend(PRBool aCloseImmediately);
  virtual void     Resume();
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal();
  // Return PR_TRUE if the stream has been closed.
  PRBool IsClosed() const { return mCacheStream.IsClosed(); }
  virtual nsMediaStream* CloneData(nsMediaDecoder* aDecoder);
  virtual nsresult ReadFromCache(char* aBuffer, PRInt64 aOffset, PRUint32 aCount);

  // Other thread
  virtual void     SetReadMode(nsMediaCacheStream::ReadMode aMode);
  virtual void     SetPlaybackRate(PRUint32 aBytesPerSecond);
  virtual nsresult Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes);
  virtual nsresult Seek(PRInt32 aWhence, PRInt64 aOffset);
  virtual PRInt64  Tell();

  // Any thread
  virtual void    Pin();
  virtual void    Unpin();
  virtual double  GetDownloadRate(PRPackedBool* aIsReliable);
  virtual PRInt64 GetLength();
  virtual PRInt64 GetNextCachedData(PRInt64 aOffset);
  virtual PRInt64 GetCachedDataEnd(PRInt64 aOffset);
  virtual PRBool  IsDataCachedToEndOfStream(PRInt64 aOffset);
  virtual PRBool  IsSuspendedByCache();
  virtual PRBool  IsSuspended();

  class Listener : public nsIStreamListener,
                   public nsIInterfaceRequestor,
                   public nsIChannelEventSink
  {
  public:
    Listener(nsMediaChannelStream* aStream) : mStream(aStream) {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSICHANNELEVENTSINK
    NS_DECL_NSIINTERFACEREQUESTOR

    void Revoke() { mStream = nsnull; }

  private:
    nsMediaChannelStream* mStream;
  };
  friend class Listener;

  nsresult GetCachedRanges(nsTArray<nsByteRange>& aRanges);

protected:
  // These are called on the main thread by Listener.
  nsresult OnStartRequest(nsIRequest* aRequest);
  nsresult OnStopRequest(nsIRequest* aRequest, nsresult aStatus);
  nsresult OnDataAvailable(nsIRequest* aRequest,
                           nsIInputStream* aStream,
                           PRUint32 aCount);
  nsresult OnChannelRedirect(nsIChannel* aOld, nsIChannel* aNew, PRUint32 aFlags);

  // Opens the channel, using an HTTP byte range request to start at mOffset
  // if possible. Main thread only.
  nsresult OpenChannel(nsIStreamListener** aStreamListener);
  nsresult RecreateChannel();
  // Add headers to HTTP request. Main thread only.
  void SetupChannelHeaders();
  // Closes the channel. Main thread only.
  void CloseChannel();

  void DoNotifyDataReceived();

  static NS_METHOD CopySegmentToCache(nsIInputStream *aInStream,
                                      void *aClosure,
                                      const char *aFromSegment,
                                      PRUint32 aToOffset,
                                      PRUint32 aCount,
                                      PRUint32 *aWriteCount);

  // Suspend the channel only if the channels is currently downloading data.
  // If it isn't we set a flag, mIgnoreResume, so that PossiblyResume knows
  // whether to acutually resume or not.
  void PossiblySuspend();

  // Resume from a suspend if we actually suspended (See PossiblySuspend).
  void PossiblyResume();

  // Main thread access only
  PRInt64            mOffset;
  nsRefPtr<Listener> mListener;
  // A data received event for the decoder that has been dispatched but has
  // not yet been processed.
  nsRevocableEventPtr<nsRunnableMethod<nsMediaChannelStream, void, false> > mDataReceivedEvent;
  PRUint32           mSuspendCount;
  // When this flag is set, if we get a network error we should silently
  // reopen the stream.
  PRPackedBool       mReopenOnError;
  // When this flag is set, we should not report the next close of the
  // channel.
  PRPackedBool       mIgnoreClose;

  // Any thread access
  nsMediaCacheStream mCacheStream;

  // This lock protects mChannelStatistics and mCacheSuspendCount
  Mutex               mLock;
  nsChannelStatistics mChannelStatistics;
  PRUint32            mCacheSuspendCount;

  // PR_TRUE if we couldn't suspend the stream and we therefore don't want
  // to resume later. This is usually due to the channel not being in the
  // isPending state at the time of the suspend request.
  PRPackedBool mIgnoreResume;
};

#endif
