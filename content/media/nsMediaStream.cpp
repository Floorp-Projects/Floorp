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

#include "nsMediaStream.h"

#include "mozilla/Mutex.h"
#include "nsDebug.h"
#include "nsMediaDecoder.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsIFile.h"
#include "nsIFileChannel.h"
#include "nsIHttpChannel.h"
#include "nsISeekableStream.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIRequestObserver.h"
#include "nsIStreamListener.h"
#include "nsIScriptSecurityManager.h"
#include "nsCrossSiteListenerProxy.h"
#include "nsHTMLMediaElement.h"
#include "nsIDocument.h"
#include "nsDOMError.h"
#include "nsICachingChannel.h"
#include "nsURILoader.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "mozilla/Util.h" // for DebugOnly
#include "nsContentUtils.h"

static const PRUint32 HTTP_OK_CODE = 200;
static const PRUint32 HTTP_PARTIAL_RESPONSE_CODE = 206;

using namespace mozilla;

nsMediaChannelStream::nsMediaChannelStream(nsMediaDecoder* aDecoder,
    nsIChannel* aChannel, nsIURI* aURI)
  : nsMediaStream(aDecoder, aChannel, aURI),
    mOffset(0), mSuspendCount(0),
    mReopenOnError(PR_FALSE), mIgnoreClose(PR_FALSE),
    mCacheStream(this),
    mLock("nsMediaChannelStream.mLock"),
    mCacheSuspendCount(0),
    mIgnoreResume(PR_FALSE)
{
}

nsMediaChannelStream::~nsMediaChannelStream()
{
  if (mListener) {
    // Kill its reference to us since we're going away
    mListener->Revoke();
  }
}

// nsMediaChannelStream::Listener just observes the channel and
// forwards notifications to the nsMediaChannelStream. We use multiple
// listener objects so that when we open a new stream for a seek we can
// disconnect the old listener from the nsMediaChannelStream and hook up
// a new listener, so notifications from the old channel are discarded
// and don't confuse us.
NS_IMPL_ISUPPORTS4(nsMediaChannelStream::Listener,
                   nsIRequestObserver, nsIStreamListener, nsIChannelEventSink,
                   nsIInterfaceRequestor)

nsresult
nsMediaChannelStream::Listener::OnStartRequest(nsIRequest* aRequest,
                                               nsISupports* aContext)
{
  if (!mStream)
    return NS_OK;
  return mStream->OnStartRequest(aRequest);
}

nsresult
nsMediaChannelStream::Listener::OnStopRequest(nsIRequest* aRequest,
                                              nsISupports* aContext,
                                              nsresult aStatus)
{
  if (!mStream)
    return NS_OK;
  return mStream->OnStopRequest(aRequest, aStatus);
}

nsresult
nsMediaChannelStream::Listener::OnDataAvailable(nsIRequest* aRequest,
                                                nsISupports* aContext,
                                                nsIInputStream* aStream,
                                                PRUint32 aOffset,
                                                PRUint32 aCount)
{
  if (!mStream)
    return NS_OK;
  return mStream->OnDataAvailable(aRequest, aStream, aCount);
}

nsresult
nsMediaChannelStream::Listener::AsyncOnChannelRedirect(nsIChannel* aOldChannel,
                                                       nsIChannel* aNewChannel,
                                                       PRUint32 aFlags,
                                                       nsIAsyncVerifyRedirectCallback* cb)
{
  nsresult rv = NS_OK;
  if (mStream)
    rv = mStream->OnChannelRedirect(aOldChannel, aNewChannel, aFlags);

  if (NS_FAILED(rv))
    return rv;

  cb->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

nsresult
nsMediaChannelStream::Listener::GetInterface(const nsIID & aIID, void **aResult)
{
  return QueryInterface(aIID, aResult);
}

nsresult
nsMediaChannelStream::OnStartRequest(nsIRequest* aRequest)
{
  NS_ASSERTION(mChannel.get() == aRequest, "Wrong channel!");

  nsHTMLMediaElement* element = mDecoder->GetMediaElement();
  NS_ENSURE_TRUE(element, NS_ERROR_FAILURE);
  nsresult status;
  nsresult rv = aRequest->GetStatus(&status);
  NS_ENSURE_SUCCESS(rv, rv);

  if (element->ShouldCheckAllowOrigin()) {
    // If the request was cancelled by nsCORSListenerProxy due to failing
    // the CORS security check, send an error through to the media element.
    if (status == NS_ERROR_DOM_BAD_URI) {
      mDecoder->NetworkError();
      return NS_ERROR_DOM_BAD_URI;
    }
  }

  nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(aRequest);
  bool seekable = false;
  if (hc) {
    PRUint32 responseStatus = 0;
    hc->GetResponseStatus(&responseStatus);
    bool succeeded = false;
    hc->GetRequestSucceeded(&succeeded);

    if (!succeeded && NS_SUCCEEDED(status)) {
      // HTTP-level error (e.g. 4xx); treat this as a fatal network-level error.
      // We might get this on a seek.
      // (Note that lower-level errors indicated by NS_FAILED(status) are
      // handled in OnStopRequest.)
      // A 416 error should treated as EOF here... it's possible
      // that we don't get Content-Length, we read N bytes, then we
      // suspend and resume, the resume reopens the channel and we seek to
      // offset N, but there are no more bytes, so we get a 416
      // "Requested Range Not Satisfiable".
      if (responseStatus != HTTP_REQUESTED_RANGE_NOT_SATISFIABLE_CODE) {
        mDecoder->NetworkError();
      }

      // This disconnects our listener so we don't get any more data. We
      // certainly don't want an error page to end up in our cache!
      CloseChannel();
      return NS_OK;
    }

    nsCAutoString ranges;
    hc->GetResponseHeader(NS_LITERAL_CSTRING("Accept-Ranges"),
                          ranges);
    bool acceptsRanges = ranges.EqualsLiteral("bytes");

    if (mOffset == 0) {
      // Look for duration headers from known Ogg content systems.
      // In the case of multiple options for obtaining the duration
      // the order of precedence is:
      // 1) The Media resource metadata if possible (done by the decoder itself).
      // 2) Content-Duration message header.
      // 3) X-AMZ-Meta-Content-Duration.
      // 4) X-Content-Duration.
      // 5) Perform a seek in the decoder to find the value.
      nsCAutoString durationText;
      PRInt32 ec = 0;
      rv = hc->GetResponseHeader(NS_LITERAL_CSTRING("Content-Duration"), durationText);
      if (NS_FAILED(rv)) {
        rv = hc->GetResponseHeader(NS_LITERAL_CSTRING("X-AMZ-Meta-Content-Duration"), durationText);
      }
      if (NS_FAILED(rv)) {
        rv = hc->GetResponseHeader(NS_LITERAL_CSTRING("X-Content-Duration"), durationText);
      }

      if (NS_SUCCEEDED(rv)) {
        double duration = durationText.ToDouble(&ec);
        if (ec == NS_OK && duration >= 0) {
          mDecoder->SetDuration(duration);
        }
      } else {
        mDecoder->SetInfinite(PR_TRUE);
      }
    }

    if (mOffset > 0 && responseStatus == HTTP_OK_CODE) {
      // If we get an OK response but we were seeking, we have to assume
      // that seeking doesn't work. We also need to tell the cache that
      // it's getting data for the start of the stream.
      mCacheStream.NotifyDataStarted(0);
      mOffset = 0;

      // The server claimed it supported range requests.  It lied.
      acceptsRanges = PR_FALSE;
    } else if (mOffset == 0 &&
               (responseStatus == HTTP_OK_CODE ||
                responseStatus == HTTP_PARTIAL_RESPONSE_CODE)) {
      // We weren't seeking and got a valid response status,
      // set the length of the content.
      PRInt32 cl = -1;
      hc->GetContentLength(&cl);
      if (cl >= 0) {
        mCacheStream.NotifyDataLength(cl);
      }
    }
    // XXX we probably should examine the Content-Range header in case
    // the server gave us a range which is not quite what we asked for

    // If we get an HTTP_OK_CODE response to our byte range request,
    // and the server isn't sending Accept-Ranges:bytes then we don't
    // support seeking.
    seekable =
      responseStatus == HTTP_PARTIAL_RESPONSE_CODE || acceptsRanges;

    if (seekable) {
      mDecoder->SetInfinite(PR_FALSE);
    }
  }
  mDecoder->SetSeekable(seekable);
  mCacheStream.SetSeekable(seekable);

  nsCOMPtr<nsICachingChannel> cc = do_QueryInterface(aRequest);
  if (cc) {
    bool fromCache = false;
    rv = cc->IsFromCache(&fromCache);
    if (NS_SUCCEEDED(rv) && !fromCache) {
      cc->SetCacheAsFile(PR_TRUE);
    }
  }

  {
    MutexAutoLock lock(mLock);
    mChannelStatistics.Start(TimeStamp::Now());
  }

  mReopenOnError = PR_FALSE;
  mIgnoreClose = PR_FALSE;
  if (mSuspendCount > 0) {
    // Re-suspend the channel if it needs to be suspended
    // No need to call PossiblySuspend here since the channel is
    // definitely in the right state for us in OneStartRequest.
    mChannel->Suspend();
    mIgnoreResume = PR_FALSE;
  }

  // Fires an initial progress event and sets up the stall counter so stall events
  // fire if no download occurs within the required time frame.
  mDecoder->Progress(PR_FALSE);

  return NS_OK;
}

nsresult
nsMediaChannelStream::OnStopRequest(nsIRequest* aRequest, nsresult aStatus)
{
  NS_ASSERTION(mChannel.get() == aRequest, "Wrong channel!");
  NS_ASSERTION(mSuspendCount == 0,
               "How can OnStopRequest fire while we're suspended?");

  {
    MutexAutoLock lock(mLock);
    mChannelStatistics.Stop(TimeStamp::Now());
  }

  // Note that aStatus might have succeeded --- this might be a normal close
  // --- even in situations where the server cut us off because we were
  // suspended. So we need to "reopen on error" in that case too. The only
  // cases where we don't need to reopen are when *we* closed the stream.
  // But don't reopen if we need to seek and we don't think we can... that would
  // cause us to just re-read the stream, which would be really bad.
  if (mReopenOnError &&
      aStatus != NS_ERROR_PARSED_DATA_CACHED && aStatus != NS_BINDING_ABORTED &&
      (mOffset == 0 || mCacheStream.IsSeekable())) {
    // If the stream did close normally, then if the server is seekable we'll
    // just seek to the end of the resource and get an HTTP 416 error because
    // there's nothing there, so this isn't bad.
    nsresult rv = CacheClientSeek(mOffset, PR_FALSE);
    if (NS_SUCCEEDED(rv))
      return rv;
    // If the reopen/reseek fails, just fall through and treat this
    // error as fatal.
  }

  if (!mIgnoreClose) {
    mCacheStream.NotifyDataEnded(aStatus);

    // Move this request back into the foreground.  This is necessary for
    // requests owned by video documents to ensure the load group fires
    // OnStopRequest when restoring from session history.
    if (mLoadInBackground) {
      mLoadInBackground = PR_FALSE;

      nsLoadFlags loadFlags;
      DebugOnly<nsresult> rv = mChannel->GetLoadFlags(&loadFlags);
      NS_ASSERTION(NS_SUCCEEDED(rv), "GetLoadFlags() failed!");

      loadFlags &= ~nsIRequest::LOAD_BACKGROUND;
      ModifyLoadFlags(loadFlags);
    }
  }

  return NS_OK;
}

nsresult
nsMediaChannelStream::OnChannelRedirect(nsIChannel* aOld, nsIChannel* aNew,
                                        PRUint32 aFlags)
{
  mChannel = aNew;
  SetupChannelHeaders();
  return NS_OK;
}

struct CopySegmentClosure {
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsMediaChannelStream*  mStream;
};

NS_METHOD
nsMediaChannelStream::CopySegmentToCache(nsIInputStream *aInStream,
                                         void *aClosure,
                                         const char *aFromSegment,
                                         PRUint32 aToOffset,
                                         PRUint32 aCount,
                                         PRUint32 *aWriteCount)
{
  CopySegmentClosure* closure = static_cast<CopySegmentClosure*>(aClosure);

  closure->mStream->mDecoder->NotifyDataArrived(aFromSegment, aCount, closure->mStream->mOffset);

  // Keep track of where we're up to
  closure->mStream->mOffset += aCount;
  closure->mStream->mCacheStream.NotifyDataReceived(aCount, aFromSegment,
                                                    closure->mPrincipal);
  *aWriteCount = aCount;
  return NS_OK;
}

nsresult
nsMediaChannelStream::OnDataAvailable(nsIRequest* aRequest,
                                      nsIInputStream* aStream,
                                      PRUint32 aCount)
{
  NS_ASSERTION(mChannel.get() == aRequest, "Wrong channel!");

  {
    MutexAutoLock lock(mLock);
    mChannelStatistics.AddBytes(aCount);
  }

  CopySegmentClosure closure;
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  if (secMan && mChannel) {
    secMan->GetChannelPrincipal(mChannel, getter_AddRefs(closure.mPrincipal));
  }
  closure.mStream = this;

  PRUint32 count = aCount;
  while (count > 0) {
    PRUint32 read;
    nsresult rv = aStream->ReadSegments(CopySegmentToCache, &closure, count,
                                        &read);
    if (NS_FAILED(rv))
      return rv;
    NS_ASSERTION(read > 0, "Read 0 bytes while data was available?");
    count -= read;
  }

  return NS_OK;
}

nsresult nsMediaChannelStream::Open(nsIStreamListener **aStreamListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  nsresult rv = mCacheStream.Init();
  if (NS_FAILED(rv))
    return rv;
  NS_ASSERTION(mOffset == 0, "Who set mOffset already?");

  if (!mChannel) {
    // When we're a clone, the decoder might ask us to Open even though
    // we haven't established an mChannel (because we might not need one)
    NS_ASSERTION(!aStreamListener,
                 "Should have already been given a channel if we're to return a stream listener");
    return NS_OK;
  }

  return OpenChannel(aStreamListener);
}

nsresult nsMediaChannelStream::OpenChannel(nsIStreamListener** aStreamListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  NS_ENSURE_TRUE(mChannel, NS_ERROR_NULL_POINTER);
  NS_ASSERTION(!mListener, "Listener should have been removed by now");

  if (aStreamListener) {
    *aStreamListener = nsnull;
  }

  mListener = new Listener(this);
  NS_ENSURE_TRUE(mListener, NS_ERROR_OUT_OF_MEMORY);

  if (aStreamListener) {
    *aStreamListener = mListener;
    NS_ADDREF(*aStreamListener);
  } else {
    mChannel->SetNotificationCallbacks(mListener.get());

    nsCOMPtr<nsIStreamListener> listener = mListener.get();

    // Ensure that if we're loading cross domain, that the server is sending
    // an authorizing Access-Control header.
    nsHTMLMediaElement* element = mDecoder->GetMediaElement();
    NS_ENSURE_TRUE(element, NS_ERROR_FAILURE);
    if (element->ShouldCheckAllowOrigin()) {
      nsresult rv;
      nsCORSListenerProxy* crossSiteListener =
        new nsCORSListenerProxy(mListener,
                                element->NodePrincipal(),
                                mChannel,
                                PR_FALSE,
                                &rv);
      listener = crossSiteListener;
      NS_ENSURE_TRUE(crossSiteListener, NS_ERROR_OUT_OF_MEMORY);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      nsresult rv = nsContentUtils::GetSecurityManager()->
        CheckLoadURIWithPrincipal(element->NodePrincipal(),
                                  mURI,
                                  nsIScriptSecurityManager::STANDARD);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    SetupChannelHeaders();

    nsresult rv = mChannel->AsyncOpen(listener, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

void nsMediaChannelStream::SetupChannelHeaders()
{
  // Always use a byte range request even if we're reading from the start
  // of the resource.
  // This enables us to detect if the stream supports byte range
  // requests, and therefore seeking, early.
  nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(mChannel);
  if (hc) {
    nsCAutoString rangeString("bytes=");
    rangeString.AppendInt(mOffset);
    rangeString.Append("-");
    hc->SetRequestHeader(NS_LITERAL_CSTRING("Range"), rangeString, PR_FALSE);

    // Send Accept header for video and audio types only (Bug 489071)
    NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");
    nsHTMLMediaElement* element = mDecoder->GetMediaElement();
    if (!element) {
      return;
    }
    element->SetRequestHeaders(hc);
  } else {
    NS_ASSERTION(mOffset == 0, "Don't know how to seek on this channel type");
  }
}

nsresult nsMediaChannelStream::Close()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  mCacheStream.Close();
  CloseChannel();
  return NS_OK;
}

already_AddRefed<nsIPrincipal> nsMediaChannelStream::GetCurrentPrincipal()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  nsCOMPtr<nsIPrincipal> principal = mCacheStream.GetCurrentPrincipal();
  return principal.forget();
}

nsMediaStream* nsMediaChannelStream::CloneData(nsMediaDecoder* aDecoder)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  nsMediaChannelStream* stream = new nsMediaChannelStream(aDecoder, nsnull, mURI);
  if (stream) {
    // Initially the clone is treated as suspended by the cache, because
    // we don't have a channel. If the cache needs to read data from the clone
    // it will call CacheClientResume (or CacheClientSeek with aResume true)
    // which will recreate the channel. This way, if all of the media data
    // is already in the cache we don't create an unneccesary HTTP channel
    // and perform a useless HTTP transaction.
    stream->mSuspendCount = 1;
    stream->mCacheSuspendCount = 1;
    stream->mCacheStream.InitAsClone(&mCacheStream);
  }
  return stream;
}

void nsMediaChannelStream::CloseChannel()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  {
    MutexAutoLock lock(mLock);
    mChannelStatistics.Stop(TimeStamp::Now());
  }

  if (mListener) {
    mListener->Revoke();
    mListener = nsnull;
  }

  if (mChannel) {
    if (mSuspendCount > 0) {
      // Resume the channel before we cancel it
      PossiblyResume();
    }
    // The status we use here won't be passed to the decoder, since
    // we've already revoked the listener. It can however be passed
    // to DocumentViewerImpl::LoadComplete if our channel is the one
    // that kicked off creation of a video document. We don't want that
    // document load to think there was an error.
    // NS_ERROR_PARSED_DATA_CACHED is the best thing we have for that
    // at the moment.
    mChannel->Cancel(NS_ERROR_PARSED_DATA_CACHED);
    mChannel = nsnull;
  }
}

nsresult nsMediaChannelStream::ReadFromCache(char* aBuffer,
                                             PRInt64 aOffset,
                                             PRUint32 aCount)
{
  return mCacheStream.ReadFromCache(aBuffer, aOffset, aCount);
}

nsresult nsMediaChannelStream::Read(char* aBuffer,
                                    PRUint32 aCount,
                                    PRUint32* aBytes)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  return mCacheStream.Read(aBuffer, aCount, aBytes);
}

nsresult nsMediaChannelStream::Seek(PRInt32 aWhence, PRInt64 aOffset)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  return mCacheStream.Seek(aWhence, aOffset);
}

PRInt64 nsMediaChannelStream::Tell()
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  return mCacheStream.Tell();
}

nsresult nsMediaChannelStream::GetCachedRanges(nsTArray<nsByteRange>& aRanges)
{
  return mCacheStream.GetCachedRanges(aRanges);
}

void nsMediaChannelStream::Suspend(bool aCloseImmediately)
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");

  nsHTMLMediaElement* element = mDecoder->GetMediaElement();
  if (!element) {
    // Shutting down; do nothing.
    return;
  }

  if (mChannel) {
    if (aCloseImmediately && mCacheStream.IsSeekable()) {
      // Kill off our channel right now, but don't tell anyone about it.
      mIgnoreClose = PR_TRUE;
      CloseChannel();
      element->DownloadSuspended();
    } else if (mSuspendCount == 0) {
      {
        MutexAutoLock lock(mLock);
        mChannelStatistics.Stop(TimeStamp::Now());
      }
      PossiblySuspend();
      element->DownloadSuspended();
    }
  }

  ++mSuspendCount;
}

void nsMediaChannelStream::Resume()
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");
  NS_ASSERTION(mSuspendCount > 0, "Too many resumes!");

  nsHTMLMediaElement* element = mDecoder->GetMediaElement();
  if (!element) {
    // Shutting down; do nothing.
    return;
  }

  NS_ASSERTION(mSuspendCount > 0, "Resume without previous Suspend!");
  --mSuspendCount;
  if (mSuspendCount == 0) {
    if (mChannel) {
      // Just wake up our existing channel
      {
        MutexAutoLock lock(mLock);
        mChannelStatistics.Start(TimeStamp::Now());
      }
      // if an error occurs after Resume, assume it's because the server
      // timed out the connection and we should reopen it.
      mReopenOnError = PR_TRUE;
      PossiblyResume();
      element->DownloadResumed();
    } else {
      PRInt64 totalLength = mCacheStream.GetLength();
      // If mOffset is at the end of the stream, then we shouldn't try to
      // seek to it. The seek will fail and be wasted anyway. We can leave
      // the channel dead; if the media cache wants to read some other data
      // in the future, it will call CacheClientSeek itself which will reopen the
      // channel.
      if (totalLength < 0 || mOffset < totalLength) {
        // There is (or may be) data to read at mOffset, so start reading it.
        // Need to recreate the channel.
        CacheClientSeek(mOffset, PR_FALSE);
      }
      element->DownloadResumed();
    }
  }
}

nsresult
nsMediaChannelStream::RecreateChannel()
{
  nsLoadFlags loadFlags =
    nsICachingChannel::LOAD_BYPASS_LOCAL_CACHE_IF_BUSY |
    (mLoadInBackground ? nsIRequest::LOAD_BACKGROUND : 0);

  nsHTMLMediaElement* element = mDecoder->GetMediaElement();
  if (!element) {
    // The decoder is being shut down, so don't bother opening a new channel
    return NS_OK;
  }
  nsCOMPtr<nsILoadGroup> loadGroup = element->GetDocumentLoadGroup();
  NS_ENSURE_TRUE(loadGroup, NS_ERROR_NULL_POINTER);

  return NS_NewChannel(getter_AddRefs(mChannel),
                       mURI,
                       nsnull,
                       loadGroup,
                       nsnull,
                       loadFlags);
}

void
nsMediaChannelStream::DoNotifyDataReceived()
{
  mDataReceivedEvent.Revoke();
  mDecoder->NotifyBytesDownloaded();
}

void
nsMediaChannelStream::CacheClientNotifyDataReceived()
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");
  // NOTE: this can be called with the media cache lock held, so don't
  // block or do anything which might try to acquire a lock!

  if (mDataReceivedEvent.IsPending())
    return;

  mDataReceivedEvent =
    NS_NewNonOwningRunnableMethod(this, &nsMediaChannelStream::DoNotifyDataReceived);
  NS_DispatchToMainThread(mDataReceivedEvent.get(), NS_DISPATCH_NORMAL);
}

class DataEnded : public nsRunnable {
public:
  DataEnded(nsMediaDecoder* aDecoder, nsresult aStatus) :
    mDecoder(aDecoder), mStatus(aStatus) {}
  NS_IMETHOD Run() {
    mDecoder->NotifyDownloadEnded(mStatus);
    return NS_OK;
  }
private:
  nsRefPtr<nsMediaDecoder> mDecoder;
  nsresult                 mStatus;
};

void
nsMediaChannelStream::CacheClientNotifyDataEnded(nsresult aStatus)
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");
  // NOTE: this can be called with the media cache lock held, so don't
  // block or do anything which might try to acquire a lock!

  nsCOMPtr<nsIRunnable> event = new DataEnded(mDecoder, aStatus);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}

nsresult
nsMediaChannelStream::CacheClientSeek(PRInt64 aOffset, bool aResume)
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");

  CloseChannel();

  if (aResume) {
    NS_ASSERTION(mSuspendCount > 0, "Too many resumes!");
    // No need to mess with the channel, since we're making a new one
    --mSuspendCount;
    {
      MutexAutoLock lock(mLock);
      NS_ASSERTION(mCacheSuspendCount > 0, "CacheClientSeek(aResume=true) without previous CacheClientSuspend!");
      --mCacheSuspendCount;
    }
  }

  nsresult rv = RecreateChannel();
  if (NS_FAILED(rv))
    return rv;

  mOffset = aOffset;
  return OpenChannel(nsnull);
}

nsresult
nsMediaChannelStream::CacheClientSuspend()
{
  {
    MutexAutoLock lock(mLock);
    ++mCacheSuspendCount;
  }
  Suspend(PR_FALSE);

  mDecoder->NotifySuspendedStatusChanged();
  return NS_OK;
}

nsresult
nsMediaChannelStream::CacheClientResume()
{
  Resume();
  {
    MutexAutoLock lock(mLock);
    NS_ASSERTION(mCacheSuspendCount > 0, "CacheClientResume without previous CacheClientSuspend!");
    --mCacheSuspendCount;
  }

  mDecoder->NotifySuspendedStatusChanged();
  return NS_OK;
}

PRInt64
nsMediaChannelStream::GetNextCachedData(PRInt64 aOffset)
{
  return mCacheStream.GetNextCachedData(aOffset);
}

PRInt64
nsMediaChannelStream::GetCachedDataEnd(PRInt64 aOffset)
{
  return mCacheStream.GetCachedDataEnd(aOffset);
}

bool
nsMediaChannelStream::IsDataCachedToEndOfStream(PRInt64 aOffset)
{
  return mCacheStream.IsDataCachedToEndOfStream(aOffset);
}

bool
nsMediaChannelStream::IsSuspendedByCache()
{
  MutexAutoLock lock(mLock);
  return mCacheSuspendCount > 0;
}

bool
nsMediaChannelStream::IsSuspended()
{
  MutexAutoLock lock(mLock);
  return mSuspendCount > 0;
}

void
nsMediaChannelStream::SetReadMode(nsMediaCacheStream::ReadMode aMode)
{
  mCacheStream.SetReadMode(aMode);
}

void
nsMediaChannelStream::SetPlaybackRate(PRUint32 aBytesPerSecond)
{
  mCacheStream.SetPlaybackRate(aBytesPerSecond);
}

void
nsMediaChannelStream::Pin()
{
  mCacheStream.Pin();
}

void
nsMediaChannelStream::Unpin()
{
  mCacheStream.Unpin();
}

double
nsMediaChannelStream::GetDownloadRate(bool* aIsReliable)
{
  MutexAutoLock lock(mLock);
  return mChannelStatistics.GetRate(TimeStamp::Now(), aIsReliable);
}

PRInt64
nsMediaChannelStream::GetLength()
{
  return mCacheStream.GetLength();
}

void
nsMediaChannelStream::PossiblySuspend()
{
  bool isPending = false;
  nsresult rv = mChannel->IsPending(&isPending);
  if (NS_SUCCEEDED(rv) && isPending) {
    mChannel->Suspend();
    mIgnoreResume = PR_FALSE;
  } else {
    mIgnoreResume = PR_TRUE;
  }
}

void
nsMediaChannelStream::PossiblyResume()
{
  if (!mIgnoreResume) {
    mChannel->Resume();
  } else {
    mIgnoreResume = PR_FALSE;
  }
}

class nsMediaFileStream : public nsMediaStream
{
public:
  nsMediaFileStream(nsMediaDecoder* aDecoder, nsIChannel* aChannel, nsIURI* aURI) :
    nsMediaStream(aDecoder, aChannel, aURI), mSize(-1),
    mLock("nsMediaFileStream.mLock")
  {
  }
  ~nsMediaFileStream()
  {
  }

  // Main thread
  virtual nsresult Open(nsIStreamListener** aStreamListener);
  virtual nsresult Close();
  virtual void     Suspend(bool aCloseImmediately) {}
  virtual void     Resume() {}
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal();
  virtual nsMediaStream* CloneData(nsMediaDecoder* aDecoder);
  virtual nsresult ReadFromCache(char* aBuffer, PRInt64 aOffset, PRUint32 aCount);

  // These methods are called off the main thread.

  // Other thread
  virtual void     SetReadMode(nsMediaCacheStream::ReadMode aMode) {}
  virtual void     SetPlaybackRate(PRUint32 aBytesPerSecond) {}
  virtual nsresult Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes);
  virtual nsresult Seek(PRInt32 aWhence, PRInt64 aOffset);
  virtual PRInt64  Tell();

  // Any thread
  virtual void    Pin() {}
  virtual void    Unpin() {}
  virtual double  GetDownloadRate(bool* aIsReliable)
  {
    // The data's all already here
    *aIsReliable = PR_TRUE;
    return 100*1024*1024; // arbitray, use 100MB/s
  }
  virtual PRInt64 GetLength() { return mSize; }
  virtual PRInt64 GetNextCachedData(PRInt64 aOffset)
  {
    return (aOffset < mSize) ? aOffset : -1;
  }
  virtual PRInt64 GetCachedDataEnd(PRInt64 aOffset) { return NS_MAX(aOffset, mSize); }
  virtual bool    IsDataCachedToEndOfStream(PRInt64 aOffset) { return true; }
  virtual bool    IsSuspendedByCache() { return false; }
  virtual bool    IsSuspended() { return false; }

  nsresult GetCachedRanges(nsTArray<nsByteRange>& aRanges);

private:
  // The file size, or -1 if not known. Immutable after Open().
  PRInt64 mSize;

  // This lock handles synchronisation between calls to Close() and
  // the Read, Seek, etc calls. Close must not be called while a
  // Read or Seek is in progress since it resets various internal
  // values to null.
  // This lock protects mSeekable and mInput.
  Mutex mLock;

  // Seekable stream interface to file. This can be used from any
  // thread.
  nsCOMPtr<nsISeekableStream> mSeekable;

  // Input stream for the media data. This can be used from any
  // thread.
  nsCOMPtr<nsIInputStream>  mInput;
};

class LoadedEvent : public nsRunnable
{
public:
  LoadedEvent(nsMediaDecoder* aDecoder) :
    mDecoder(aDecoder)
  {
    MOZ_COUNT_CTOR(LoadedEvent);
  }
  ~LoadedEvent()
  {
    MOZ_COUNT_DTOR(LoadedEvent);
  }

  NS_IMETHOD Run() {
    mDecoder->NotifyDownloadEnded(NS_OK);
    return NS_OK;
  }

private:
  nsRefPtr<nsMediaDecoder> mDecoder;
};

nsresult nsMediaFileStream::GetCachedRanges(nsTArray<nsByteRange>& aRanges)
{
  if (mSize == -1) {
    return NS_ERROR_FAILURE;
  }
  aRanges.AppendElement(nsByteRange(0, mSize));
  return NS_OK;
}

nsresult nsMediaFileStream::Open(nsIStreamListener** aStreamListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  if (aStreamListener) {
    *aStreamListener = nsnull;
  }

  nsresult rv = NS_OK;
  if (aStreamListener) {
    // The channel is already open. We need a synchronous stream that
    // implements nsISeekableStream, so we have to find the underlying
    // file and reopen it
    nsCOMPtr<nsIFileChannel> fc(do_QueryInterface(mChannel));
    if (!fc)
      return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIFile> file;
    rv = fc->GetFile(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_NewLocalFileInputStream(getter_AddRefs(mInput), file);
  } else {
    // Ensure that we never load a local file from some page on a
    // web server.
    nsHTMLMediaElement* element = mDecoder->GetMediaElement();
    NS_ENSURE_TRUE(element, NS_ERROR_FAILURE);

    rv = nsContentUtils::GetSecurityManager()->
           CheckLoadURIWithPrincipal(element->NodePrincipal(),
                                     mURI,
                                     nsIScriptSecurityManager::STANDARD);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mChannel->Open(getter_AddRefs(mInput));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  mSeekable = do_QueryInterface(mInput);
  if (!mSeekable) {
    // XXX The file may just be a .url or similar
    // shortcut that points to a Web site. We need to fix this by
    // doing an async open and waiting until we locate the real resource,
    // then using that (if it's still a file!).
    return NS_ERROR_FAILURE;
  }

  // Get the file size and inform the decoder. Only files up to 4GB are
  // supported here.
  PRUint32 size;
  rv = mInput->Available(&size);
  if (NS_SUCCEEDED(rv)) {
    mSize = size;
  }

  nsCOMPtr<nsIRunnable> event = new LoadedEvent(mDecoder);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  return NS_OK;
}

nsresult nsMediaFileStream::Close()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  MutexAutoLock lock(mLock);
  if (mChannel) {
    mChannel->Cancel(NS_ERROR_PARSED_DATA_CACHED);
    mChannel = nsnull;
    mInput = nsnull;
    mSeekable = nsnull;
  }

  return NS_OK;
}

already_AddRefed<nsIPrincipal> nsMediaFileStream::GetCurrentPrincipal()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  nsCOMPtr<nsIPrincipal> principal;
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  if (!secMan || !mChannel)
    return nsnull;
  secMan->GetChannelPrincipal(mChannel, getter_AddRefs(principal));
  return principal.forget();
}

nsMediaStream* nsMediaFileStream::CloneData(nsMediaDecoder* aDecoder)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  nsHTMLMediaElement* element = aDecoder->GetMediaElement();
  if (!element) {
    // The decoder is being shut down, so we can't clone
    return nsnull;
  }
  nsCOMPtr<nsILoadGroup> loadGroup = element->GetDocumentLoadGroup();
  NS_ENSURE_TRUE(loadGroup, nsnull);

  nsCOMPtr<nsIChannel> channel;
  nsresult rv =
    NS_NewChannel(getter_AddRefs(channel), mURI, nsnull, loadGroup, nsnull, 0);
  if (NS_FAILED(rv))
    return nsnull;

  return new nsMediaFileStream(aDecoder, channel, mURI);
}

nsresult nsMediaFileStream::ReadFromCache(char* aBuffer, PRInt64 aOffset, PRUint32 aCount)
{
  MutexAutoLock lock(mLock);
  if (!mInput || !mSeekable)
    return NS_ERROR_FAILURE;
  PRInt64 offset = 0;
  nsresult res = mSeekable->Tell(&offset);
  NS_ENSURE_SUCCESS(res,res);
  res = mSeekable->Seek(nsISeekableStream::NS_SEEK_SET, aOffset);
  NS_ENSURE_SUCCESS(res,res);
  PRUint32 bytesRead = 0;
  do {
    PRUint32 x = 0;
    PRUint32 bytesToRead = aCount - bytesRead;
    res = mInput->Read(aBuffer, bytesToRead, &x);
    bytesRead += x;
  } while (bytesRead != aCount && res == NS_OK);

  // Reset read head to original position so we don't disturb any other
  // reading thread.
  nsresult seekres = mSeekable->Seek(nsISeekableStream::NS_SEEK_SET, offset);

  // If a read failed in the loop above, we want to return its failure code.
  NS_ENSURE_SUCCESS(res,res);

  // Else we succeed if the reset-seek succeeds.
  return seekres;
}

nsresult nsMediaFileStream::Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes)
{
  MutexAutoLock lock(mLock);
  if (!mInput)
    return NS_ERROR_FAILURE;
  return mInput->Read(aBuffer, aCount, aBytes);
}

nsresult nsMediaFileStream::Seek(PRInt32 aWhence, PRInt64 aOffset)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  MutexAutoLock lock(mLock);
  if (!mSeekable)
    return NS_ERROR_FAILURE;
  return mSeekable->Seek(aWhence, aOffset);
}

PRInt64 nsMediaFileStream::Tell()
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  MutexAutoLock lock(mLock);
  if (!mSeekable)
    return 0;

  PRInt64 offset = 0;
  mSeekable->Tell(&offset);
  return offset;
}

nsMediaStream*
nsMediaStream::Create(nsMediaDecoder* aDecoder, nsIChannel* aChannel)
{
  NS_ASSERTION(NS_IsMainThread(),
	             "nsMediaStream::Open called on non-main thread");

  // If the channel was redirected, we want the post-redirect URI;
  // but if the URI scheme was expanded, say from chrome: to jar:file:,
  // we want the original URI.
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsCOMPtr<nsIFileChannel> fc = do_QueryInterface(aChannel);
  if (fc) {
    return new nsMediaFileStream(aDecoder, aChannel, uri);
  }
  return new nsMediaChannelStream(aDecoder, aChannel, uri);
}

void nsMediaStream::MoveLoadsToBackground() {
  NS_ASSERTION(!mLoadInBackground, "Why are you calling this more than once?");
  mLoadInBackground = PR_TRUE;
  if (!mChannel) {
    // No channel, resource is probably already loaded.
    return;
  }

  nsresult rv;
  nsHTMLMediaElement* element = mDecoder->GetMediaElement();
  if (!element) {
    NS_WARNING("Null element in nsMediaStream::MoveLoadsToBackground()");
    return;
  }

  bool isPending = false;
  if (NS_SUCCEEDED(mChannel->IsPending(&isPending)) &&
      isPending) {
    nsLoadFlags loadFlags;
    rv = mChannel->GetLoadFlags(&loadFlags);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetLoadFlags() failed!");

    loadFlags |= nsIRequest::LOAD_BACKGROUND;
    ModifyLoadFlags(loadFlags);
  }
}

void nsMediaStream::ModifyLoadFlags(nsLoadFlags aFlags)
{
  nsCOMPtr<nsILoadGroup> loadGroup;
  nsresult rv = mChannel->GetLoadGroup(getter_AddRefs(loadGroup));
  NS_ASSERTION(NS_SUCCEEDED(rv), "GetLoadGroup() failed!");

  nsresult status;
  mChannel->GetStatus(&status);

  // Note: if (NS_FAILED(status)), the channel won't be in the load group.
  if (loadGroup &&
      NS_SUCCEEDED(status)) {
    rv = loadGroup->RemoveRequest(mChannel, nsnull, status);
    NS_ASSERTION(NS_SUCCEEDED(rv), "RemoveRequest() failed!");
  }

  rv = mChannel->SetLoadFlags(aFlags);
  NS_ASSERTION(NS_SUCCEEDED(rv), "SetLoadFlags() failed!");

  if (loadGroup &&
      NS_SUCCEEDED(status)) {
    rv = loadGroup->AddRequest(mChannel, nsnull);
    NS_ASSERTION(NS_SUCCEEDED(rv), "AddRequest() failed!");
  }
}
