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
#include "nsDebug.h"
#include "nsMediaStream.h"
#include "nsMediaDecoder.h"
#include "nsNetUtil.h"
#include "nsAutoLock.h"
#include "nsThreadUtils.h"
#include "nsIFileChannel.h"
#include "nsIHttpChannel.h"
#include "nsISeekableStream.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIRequestObserver.h"
#include "nsIStreamListener.h"
#include "nsIScriptSecurityManager.h"
#include "nsChannelToPipeListener.h"

class nsDefaultStreamStrategy : public nsStreamStrategy
{
public:
  nsDefaultStreamStrategy(nsMediaDecoder* aDecoder, nsIChannel* aChannel, nsIURI* aURI) :
    nsStreamStrategy(aDecoder, aChannel, aURI)
  {
  }
  
  // These methods have the same thread calling requirements 
  // as those with the same name in nsMediaStream
  virtual nsresult Open();
  virtual nsresult Close();
  virtual nsresult Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes);
  virtual nsresult Seek(PRInt32 aWhence, PRInt64 aOffset);
  virtual PRInt64  Tell();
  virtual PRUint32 Available();
  virtual float    DownloadRate();
  virtual void     Cancel();
  virtual nsIPrincipal* GetCurrentPrincipal();

private:
  // Listener attached to channel to constantly download the
  // media data asynchronously and store it in the pipe. The 
  // data is obtainable via the mPipeInput member. Use on 
  // main thread only.
  nsCOMPtr<nsChannelToPipeListener> mListener;

  // Input stream for the media data currently downloaded 
  // and stored in the pipe. This can be used from any thread.
  nsCOMPtr<nsIInputStream>  mPipeInput;
};

nsresult nsDefaultStreamStrategy::Open()
{
  nsresult rv;

  mListener = new nsChannelToPipeListener(mDecoder);
  NS_ENSURE_TRUE(mListener, NS_ERROR_OUT_OF_MEMORY);

  rv = mListener->Init();
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = mChannel->AsyncOpen(mListener, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = mListener->GetInputStream(getter_AddRefs(mPipeInput));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult nsDefaultStreamStrategy::Close()
{
  nsAutoLock lock(mLock);
  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nsnull;

    mPipeInput->Close();
    mPipeInput = nsnull;

    mListener = nsnull;
  }

  return NS_OK;
}

nsresult nsDefaultStreamStrategy::Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes)
{
  // The read request pulls from the pipe, not the channels input
  // stream. This allows calling from any thread as the pipe is
  // threadsafe.
  nsAutoLock lock(mLock);
  return mPipeInput ? mPipeInput->Read(aBuffer, aCount, aBytes) : NS_ERROR_FAILURE;
}

nsresult nsDefaultStreamStrategy::Seek(PRInt32 aWhence, PRInt64 aOffset) 
{
  // Default streams cannot be seeked
  return NS_ERROR_FAILURE;
}

PRInt64 nsDefaultStreamStrategy::Tell()
{
  // Default streams cannot be seeked
  return 0;
}

PRUint32 nsDefaultStreamStrategy::Available()
{
  // The request pulls from the pipe, not the channels input
  // stream. This allows calling from any thread as the pipe is
  // threadsafe.
  nsAutoLock lock(mLock);
  if (!mPipeInput)
    return 0;

  PRUint32 count = 0;
  mPipeInput->Available(&count);
  return count;
}

float nsDefaultStreamStrategy::DownloadRate()
{
  nsAutoLock lock(mLock);
  return mListener ? mListener->BytesPerSecond() : NS_MEDIA_UNKNOWN_RATE;
}

void nsDefaultStreamStrategy::Cancel()
{
  if (mListener)
    mListener->Cancel();
}

nsIPrincipal* nsDefaultStreamStrategy::GetCurrentPrincipal()
{
  if (!mListener)
    return nsnull;

  return mListener->GetCurrentPrincipal();
}

class nsFileStreamStrategy : public nsStreamStrategy
{
public:
  nsFileStreamStrategy(nsMediaDecoder* aDecoder, nsIChannel* aChannel, nsIURI* aURI) :
    nsStreamStrategy(aDecoder, aChannel, aURI)
  {
  }
  
  // These methods have the same thread calling requirements 
  // as those with the same name in nsMediaStream
  virtual nsresult Open();
  virtual nsresult Close();
  virtual nsresult Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes);
  virtual nsresult Seek(PRInt32 aWhence, PRInt64 aOffset);
  virtual PRInt64  Tell();
  virtual PRUint32 Available();
  virtual float    DownloadRate();
  virtual nsIPrincipal* GetCurrentPrincipal();

private:
  // Seekable stream interface to file. This can be used from any
  // thread.
  nsCOMPtr<nsISeekableStream> mSeekable;

  // Input stream for the media data. This can be used from any
  // thread.
  nsCOMPtr<nsIInputStream>  mInput;

  // Security Principal
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

nsresult nsFileStreamStrategy::Open()
{
  nsresult rv;

  rv = mChannel->Open(getter_AddRefs(mInput));
  NS_ENSURE_SUCCESS(rv, rv);

  mSeekable = do_QueryInterface(mInput);

  /* Get our principal */
  nsCOMPtr<nsIScriptSecurityManager> secMan =
    do_GetService("@mozilla.org/scriptsecuritymanager;1");
  if (secMan) {
    nsresult rv = secMan->GetChannelPrincipal(mChannel,
                                              getter_AddRefs(mPrincipal));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // For a file stream the resource is considered loaded since there
  // is no buffering delays, etc reading.
  nsCOMPtr<nsIRunnable> event = 
    NS_NEW_RUNNABLE_METHOD(nsMediaDecoder, mDecoder, ResourceLoaded); 
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  
  return mSeekable ? NS_OK : NS_ERROR_FAILURE;
}

nsresult nsFileStreamStrategy::Close()
{
  nsAutoLock lock(mLock);
  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nsnull;
    mInput = nsnull;
    mSeekable = nsnull;
  }

  return NS_OK;
}

nsresult nsFileStreamStrategy::Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes)
{
  nsAutoLock lock(mLock);
  return mInput ? mInput->Read(aBuffer, aCount, aBytes) : NS_ERROR_FAILURE;
}

nsresult nsFileStreamStrategy::Seek(PRInt32 aWhence, PRInt64 aOffset) 
{  
  nsAutoLock lock(mLock);
  return mSeekable ? mSeekable->Seek(aWhence, aOffset) : NS_ERROR_FAILURE;
}

PRInt64 nsFileStreamStrategy::Tell()
{
  nsAutoLock lock(mLock);
  if (!mSeekable)
    return 0;

  PRInt64 offset = 0;
  mSeekable->Tell(&offset);
  return offset;
}

PRUint32 nsFileStreamStrategy::Available()
{
  nsAutoLock lock(mLock);
  if (!mInput)
    return 0;

  PRUint32 count = 0;
  mInput->Available(&count);
  return count;
}

float nsFileStreamStrategy::DownloadRate()
{
  return NS_MEDIA_UNKNOWN_RATE;
}

nsIPrincipal* nsFileStreamStrategy::GetCurrentPrincipal()
{
  return mPrincipal;
}

class nsHttpStreamStrategy : public nsStreamStrategy
{
public:
  nsHttpStreamStrategy(nsMediaDecoder* aDecoder, nsIChannel* aChannel, nsIURI* aURI) :
    nsStreamStrategy(aDecoder, aChannel, aURI),
    mPosition(0),
    mEOFPosition(-1),
    mCancelled(PR_FALSE)
  {
  }
  
  // These methods have the same thread calling requirements 
  // as those with the same name in nsMediaStream
  virtual nsresult Open();
  virtual nsresult Close();
  virtual nsresult Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes);
  virtual nsresult Seek(PRInt32 aWhence, PRInt64 aOffset);
  virtual PRInt64  Tell();
  virtual PRUint32 Available();
  virtual float    DownloadRate();
  virtual void     Cancel();
  virtual nsIPrincipal* GetCurrentPrincipal();

  // Return PR_TRUE if the stream has been cancelled.
  PRBool IsCancelled() const;

  // This must be called on the main thread only, and at a
  // time when the strategy is not reading from the current
  // channel/stream. It's primary purpose is to be called from
  // a Seek to reset to the new byte range request http channel.
  void Reset(nsIChannel* aChannel, 
             nsChannelToPipeListener* aListener, 
             nsIInputStream* aStream);

private:
  // Listener attached to channel to constantly download the
  // media data asynchronously and store it in the pipe. The 
  // data is obtainable via the mPipeInput member. Use on 
  // main thread only.
  nsCOMPtr<nsChannelToPipeListener> mListener;

  // Input stream for the media data currently downloaded 
  // and stored in the pipe. This can be used from any thread.
  nsCOMPtr<nsIInputStream>  mPipeInput;

  // Current seek position. Need to compute this manually due to
  // seeking with byte range requests meaning the position in the pipe
  // is not valid. This is initially set on the main thread during the
  // Open call. After that it is read and written by a single thread
  // only (the thread that calls the read/seek operations).
  PRInt64 mPosition;

  // End of file seek position. Set when liboggz requests a 
  // seek to end of file, otherwise -1. This is written and read
  // from a single thread only (the thread that calls the read/seek
  // operations).
  PRInt64 mEOFPosition;

  // PR_TRUE if the media stream requested this strategy is cancelled.
  // This is read and written on the main thread only.
  PRPackedBool mCancelled;
};

void nsHttpStreamStrategy::Reset(nsIChannel* aChannel, 
                                 nsChannelToPipeListener* aListener, 
                                 nsIInputStream* aStream)
{
  nsAutoLock lock(mLock);
  mChannel = aChannel;
  mListener = aListener;
  mPipeInput = aStream;
}

nsresult nsHttpStreamStrategy::Open()
{
  nsresult rv;

  mListener = new nsChannelToPipeListener(mDecoder);
  NS_ENSURE_TRUE(mListener, NS_ERROR_OUT_OF_MEMORY);

  rv = mListener->Init();
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = mChannel->AsyncOpen(mListener, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = mListener->GetInputStream(getter_AddRefs(mPipeInput));
  NS_ENSURE_SUCCESS(rv, rv);

  mPosition = 0;

  return NS_OK;
}

nsresult nsHttpStreamStrategy::Close()
{
  nsAutoLock lock(mLock);
  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nsnull;

    mPipeInput->Close();
    mPipeInput = nsnull;

    mListener = nsnull;
  }

  return NS_OK;
}

nsresult nsHttpStreamStrategy::Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes)
{
  // The read request pulls from the pipe, not the channels input
  // stream. This allows calling from any thread as the pipe is
  // threadsafe.
  nsAutoLock lock(mLock);
  if (!mPipeInput)
    return NS_ERROR_FAILURE;

  nsresult rv = mPipeInput->Read(aBuffer, aCount, aBytes);
  mPosition += *aBytes;

  return rv;
}

class nsByteRangeEvent : public nsRunnable 
{
public:
  nsByteRangeEvent(nsHttpStreamStrategy* aStrategy, 
                   nsMediaDecoder* aDecoder, 
                   nsIURI* aURI, 
                   PRInt64 aOffset) :
    mStrategy(aStrategy),
    mDecoder(aDecoder),
    mURI(aURI),
    mOffset(aOffset),
    mResult(NS_OK)
  {
    MOZ_COUNT_CTOR(nsByteRangeEvent);
  }

  ~nsByteRangeEvent()
  {
    MOZ_COUNT_DTOR(nsByteRangeEvent);
  }

  nsresult GetResult()
  {
    return mResult;
  }

  NS_IMETHOD Run() {
    // This event runs in the main thread. The same
    // thread as that which can block waiting for the
    // decode event to complete when the stream
    // is cancelled. Check for to see if we are cancelled
    // in case this event is processed after the cancel flag
    // which would otherwise cause the listener to be recreated.
    if (mStrategy->IsCancelled()) {
      mResult = NS_ERROR_FAILURE;
      return NS_OK;
    }

    mStrategy->Close();
    mResult = NS_NewChannel(getter_AddRefs(mChannel),
                            mURI,
                            nsnull,
                            nsnull,
                            nsnull,
                            nsIRequest::LOAD_NORMAL);
    NS_ENSURE_SUCCESS(mResult, mResult);

    nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(mChannel);
    if (hc) {
      nsCAutoString rangeString("bytes=");
      rangeString.AppendInt(mOffset);
      rangeString.Append("-");
      hc->SetRequestHeader(NS_LITERAL_CSTRING("Range"), rangeString, PR_FALSE);
    }

    mListener = new nsChannelToPipeListener(mDecoder);
    NS_ENSURE_TRUE(mListener, NS_ERROR_OUT_OF_MEMORY);

    mResult = mListener->Init();
    NS_ENSURE_SUCCESS(mResult, mResult);

    mResult = mChannel->AsyncOpen(mListener, nsnull);
    NS_ENSURE_SUCCESS(mResult, mResult);

    mResult = mListener->GetInputStream(getter_AddRefs(mStream));
    NS_ENSURE_SUCCESS(mResult, mResult);

    mStrategy->Reset(mChannel, mListener, mStream);
    return NS_OK;
  }

private:
  nsHttpStreamStrategy* mStrategy;
  nsMediaDecoder* mDecoder;
  nsIURI* mURI;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsChannelToPipeListener> mListener;
  nsCOMPtr<nsIInputStream> mStream;
  PRInt64 mOffset;
  nsresult mResult;
};

nsresult nsHttpStreamStrategy::Seek(PRInt32 aWhence, PRInt64 aOffset) 
{
  {
    nsAutoLock lock(mLock);
    if (!mChannel || !mPipeInput) 
      return NS_ERROR_FAILURE;

    // When seeking liboggz will first seek to the end of the file to
    // obtain the length of the file. It immediately does a 'tell' to
    // get the position and reseeks somewhere else. This traps the seek
    // to end of file and just returns the content length.
    if(aWhence == nsISeekableStream::NS_SEEK_END && aOffset == 0) {
      PRInt32 length;
      mChannel->GetContentLength(&length);
      if (length == -1)
        return NS_ERROR_FAILURE;
      
      mEOFPosition = length;
      return NS_OK;
    }

    // Handle cases of aWhence not being NS_SEEK_SET but converting to
    // NS_SEEK_SET
    switch (aWhence) {
    case nsISeekableStream::NS_SEEK_END: {
      PRInt32 length;
      mChannel->GetContentLength(&length);
      if (length == -1)
        return NS_ERROR_FAILURE;
      
      aOffset -= length; 
      aWhence = nsISeekableStream::NS_SEEK_SET;
      break;
    }
    case nsISeekableStream::NS_SEEK_CUR: {
      aOffset += mPosition;
      aWhence = nsISeekableStream::NS_SEEK_SET;
      break;
    }
    default:
      // Do nothing, we are NS_SEEK_SET 
      break;
    };
    
    // If we are already at the correct position, do nothing
    if (aOffset == mPosition) 
      return NS_OK;

    // If we are seeking to a byterange that we already have buffered in
    // the listener then move to that and avoid the need to send a byte
    // range request.
    PRInt32 bytesAhead = aOffset - mPosition;
    PRUint32 available = 0;
    nsresult rv = mPipeInput->Available(&available);
    if (NS_SUCCEEDED(rv) && bytesAhead > 0 && available >= PRUint32(bytesAhead)) {
      nsAutoArrayPtr<char> data(new char[bytesAhead]);
      if (!data)
        return NS_ERROR_OUT_OF_MEMORY;
      
      PRUint32 bytes = 0;
      nsresult rv = mPipeInput->Read(data.get(), bytesAhead, &bytes);
      
      mPosition += bytesAhead;
      return rv;
    }
  }

  // Don't acquire mLock in this scope as we do a synchronous call to the main thread
  // which would deadlock if that thread is calling Close().
  nsCOMPtr<nsByteRangeEvent> event = new nsByteRangeEvent(this, mDecoder, mURI, aOffset);
  NS_DispatchToMainThread(event, NS_DISPATCH_SYNC);

  mPosition = aOffset;
  return event->GetResult();
}

PRInt64 nsHttpStreamStrategy::Tell()
{
  // Handle the case of a seek to EOF by liboggz
  // (See Seek for details)
  if (mEOFPosition != -1) {
    PRInt64 result = mEOFPosition;
    mEOFPosition = -1;
    return result;
  }

  return mPosition;
}

PRUint32 nsHttpStreamStrategy::Available()
{
  // The request pulls from the pipe, not the channels input
  // stream. This allows calling from any thread as the pipe is
  // threadsafe.
  nsAutoLock lock(mLock);
  if (!mPipeInput)
    return 0;

  PRUint32 count = 0;
  mPipeInput->Available(&count);
  return count;
}

float nsHttpStreamStrategy::DownloadRate()
{
  nsAutoLock lock(mLock);
  if (!mListener)
    return NS_MEDIA_UNKNOWN_RATE;
  return mListener->BytesPerSecond();
}

void nsHttpStreamStrategy::Cancel()
{
  mCancelled = PR_TRUE;
  if (mListener)
    mListener->Cancel();
}

PRBool nsHttpStreamStrategy::IsCancelled() const
{
  return mCancelled;
}

nsIPrincipal* nsHttpStreamStrategy::GetCurrentPrincipal()
{
  if (!mListener)
    return nsnull;

  return mListener->GetCurrentPrincipal();
}

nsMediaStream::nsMediaStream()  :
  mPlaybackRateCount(0)
{
  NS_ASSERTION(NS_IsMainThread(), 
	       "nsMediaStream created on non-main thread");
  MOZ_COUNT_CTOR(nsMediaStream);
}

nsMediaStream::~nsMediaStream()
{
  MOZ_COUNT_DTOR(nsMediaStream);
}

nsresult nsMediaStream::Open(nsMediaDecoder* aDecoder, nsIURI* aURI)
{
  NS_ASSERTION(NS_IsMainThread(), 
	       "nsMediaStream::Open called on non-main thread");

  nsresult rv;

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), 
                     aURI, 
                     nsnull,
                     nsnull,
                     nsnull,
                     nsIRequest::LOAD_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFileChannel> fc = do_QueryInterface(channel);
  nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(channel);
  if (hc) 
    mStreamStrategy = new nsHttpStreamStrategy(aDecoder, channel, aURI);
  else if (fc) 
    mStreamStrategy = new nsFileStreamStrategy(aDecoder, channel, aURI);
  else
    mStreamStrategy = new nsDefaultStreamStrategy(aDecoder, channel, aURI);

  mPlaybackRateCount = 0;
  mPlaybackRateStart = PR_IntervalNow();

  return mStreamStrategy->Open();
}

nsresult nsMediaStream::Close()
{
  NS_ASSERTION(NS_IsMainThread(), 
	       "nsMediaStream::Close called on non-main thread");

  return mStreamStrategy->Close();
}

nsresult nsMediaStream::Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes)
{
  nsresult rv = mStreamStrategy->Read(aBuffer, aCount, aBytes);
  mPlaybackRateCount += *aBytes;    
  return rv;
}

nsresult nsMediaStream::Seek(PRInt32 aWhence, PRInt64 aOffset) 
{
  return mStreamStrategy->Seek(aWhence, aOffset);
}

PRInt64 nsMediaStream::Tell()
{
  return mStreamStrategy->Tell();
}

PRUint32 nsMediaStream::Available()
{
  return mStreamStrategy->Available();
}

float nsMediaStream::DownloadRate()
{
  return mStreamStrategy->DownloadRate();
}

float nsMediaStream::PlaybackRate()
{
  PRIntervalTime now = PR_IntervalNow();
  PRUint32 interval = PR_IntervalToMilliseconds(now - mPlaybackRateStart);
  return static_cast<float>(mPlaybackRateCount) * 1000 / interval;
}

void nsMediaStream::Cancel()
{
  NS_ASSERTION(NS_IsMainThread(), 
	       "nsMediaStream::Cancel called on non-main thread");

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
  mStreamStrategy->Cancel();
}

nsIPrincipal* nsMediaStream::GetCurrentPrincipal()
{
  return mStreamStrategy->GetCurrentPrincipal();
}
