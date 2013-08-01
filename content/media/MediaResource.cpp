/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "MediaResource.h"

#include "mozilla/Mutex.h"
#include "nsDebug.h"
#include "MediaDecoder.h"
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
#include "mozilla/dom/HTMLMediaElement.h"
#include "nsError.h"
#include "nsICachingChannel.h"
#include "nsURILoader.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsContentUtils.h"
#include "nsHostObjectProtocolHandler.h"
#include <algorithm>

#ifdef PR_LOGGING
PRLogModuleInfo* gMediaResourceLog;
#define LOG(msg, ...) PR_LOG(gMediaResourceLog, PR_LOG_DEBUG, \
                             (msg, ##__VA_ARGS__))
// Debug logging macro with object pointer and class name.
#define CMLOG(msg, ...) \
        LOG("%p [ChannelMediaResource]: " msg, this, ##__VA_ARGS__)
#else
#define LOG(msg, ...)
#define CMLOG(msg, ...)
#endif

static const uint32_t HTTP_OK_CODE = 200;
static const uint32_t HTTP_PARTIAL_RESPONSE_CODE = 206;

namespace mozilla {

ChannelMediaResource::ChannelMediaResource(MediaDecoder* aDecoder,
                                           nsIChannel* aChannel,
                                           nsIURI* aURI,
                                           const nsACString& aContentType)
  : BaseMediaResource(aDecoder, aChannel, aURI, aContentType),
    mOffset(0), mSuspendCount(0),
    mReopenOnError(false), mIgnoreClose(false),
    mCacheStream(MOZ_THIS_IN_INITIALIZER_LIST()),
    mLock("ChannelMediaResource.mLock"),
    mIgnoreResume(false),
    mSeekingForMetadata(false),
#ifdef MOZ_DASH
    mByteRangeDownloads(false),
    mByteRangeFirstOpen(true),
    mSeekOffsetMonitor("media.dashseekmonitor"),
    mSeekOffset(-1),
#endif
    mIsTransportSeekable(true)
{
#ifdef PR_LOGGING
  if (!gMediaResourceLog) {
    gMediaResourceLog = PR_NewLogModule("MediaResource");
  }
#endif
}

ChannelMediaResource::~ChannelMediaResource()
{
  if (mListener) {
    // Kill its reference to us since we're going away
    mListener->Revoke();
  }
}

// ChannelMediaResource::Listener just observes the channel and
// forwards notifications to the ChannelMediaResource. We use multiple
// listener objects so that when we open a new stream for a seek we can
// disconnect the old listener from the ChannelMediaResource and hook up
// a new listener, so notifications from the old channel are discarded
// and don't confuse us.
NS_IMPL_ISUPPORTS4(ChannelMediaResource::Listener,
                   nsIRequestObserver, nsIStreamListener, nsIChannelEventSink,
                   nsIInterfaceRequestor)

nsresult
ChannelMediaResource::Listener::OnStartRequest(nsIRequest* aRequest,
                                               nsISupports* aContext)
{
  if (!mResource)
    return NS_OK;
  return mResource->OnStartRequest(aRequest);
}

nsresult
ChannelMediaResource::Listener::OnStopRequest(nsIRequest* aRequest,
                                              nsISupports* aContext,
                                              nsresult aStatus)
{
  if (!mResource)
    return NS_OK;
  return mResource->OnStopRequest(aRequest, aStatus);
}

nsresult
ChannelMediaResource::Listener::OnDataAvailable(nsIRequest* aRequest,
                                                nsISupports* aContext,
                                                nsIInputStream* aStream,
                                                uint64_t aOffset,
                                                uint32_t aCount)
{
  if (!mResource)
    return NS_OK;
  return mResource->OnDataAvailable(aRequest, aStream, aCount);
}

nsresult
ChannelMediaResource::Listener::AsyncOnChannelRedirect(nsIChannel* aOldChannel,
                                                       nsIChannel* aNewChannel,
                                                       uint32_t aFlags,
                                                       nsIAsyncVerifyRedirectCallback* cb)
{
  nsresult rv = NS_OK;
  if (mResource)
    rv = mResource->OnChannelRedirect(aOldChannel, aNewChannel, aFlags);

  if (NS_FAILED(rv))
    return rv;

  cb->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

nsresult
ChannelMediaResource::Listener::GetInterface(const nsIID & aIID, void **aResult)
{
  return QueryInterface(aIID, aResult);
}

nsresult
ChannelMediaResource::OnStartRequest(nsIRequest* aRequest)
{
  NS_ASSERTION(mChannel.get() == aRequest, "Wrong channel!");

  MediaDecoderOwner* owner = mDecoder->GetMediaOwner();
  NS_ENSURE_TRUE(owner, NS_ERROR_FAILURE);
  HTMLMediaElement* element = owner->GetMediaElement();
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
    uint32_t responseStatus = 0;
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
      if (responseStatus == HTTP_REQUESTED_RANGE_NOT_SATISFIABLE_CODE) {
        // OnStopRequest will not be fired, so we need to do some of its
        // work here.
        mCacheStream.NotifyDataEnded(status);
      } else {
        mDecoder->NetworkError();
      }

      // This disconnects our listener so we don't get any more data. We
      // certainly don't want an error page to end up in our cache!
      CloseChannel();
      return NS_OK;
    }

    nsAutoCString ranges;
    hc->GetResponseHeader(NS_LITERAL_CSTRING("Accept-Ranges"),
                          ranges);
    bool acceptsRanges = ranges.EqualsLiteral("bytes");
    // True if this channel will not return an unbounded amount of data
    bool dataIsBounded = false;

    int64_t contentLength = -1;
    hc->GetContentLength(&contentLength);
    if (contentLength >= 0 && responseStatus == HTTP_OK_CODE) {
      // "OK" status means Content-Length is for the whole resource.
      // Since that's bounded, we know we have a finite-length resource.
      dataIsBounded = true;
    }

    if (mOffset == 0) {
      // Look for duration headers from known Ogg content systems.
      // In the case of multiple options for obtaining the duration
      // the order of precedence is:
      // 1) The Media resource metadata if possible (done by the decoder itself).
      // 2) Content-Duration message header.
      // 3) X-AMZ-Meta-Content-Duration.
      // 4) X-Content-Duration.
      // 5) Perform a seek in the decoder to find the value.
      nsAutoCString durationText;
      nsresult ec = NS_OK;
      rv = hc->GetResponseHeader(NS_LITERAL_CSTRING("Content-Duration"), durationText);
      if (NS_FAILED(rv)) {
        rv = hc->GetResponseHeader(NS_LITERAL_CSTRING("X-AMZ-Meta-Content-Duration"), durationText);
      }
      if (NS_FAILED(rv)) {
        rv = hc->GetResponseHeader(NS_LITERAL_CSTRING("X-Content-Duration"), durationText);
      }

      // If there is a Content-Duration header with a valid value, record
      // the duration.
      if (NS_SUCCEEDED(rv)) {
        double duration = durationText.ToDouble(&ec);
        if (ec == NS_OK && duration >= 0) {
          mDecoder->SetDuration(duration);
          // We know the resource must be bounded.
          dataIsBounded = true;
        }
      }
    }

    // Assume Range requests have a bounded upper limit unless the
    // Content-Range header tells us otherwise.
    bool boundedSeekLimit = true;
    // Check response code for byte-range requests (seeking, chunk requests).
    if (!mByteRange.IsNull() && (responseStatus == HTTP_PARTIAL_RESPONSE_CODE)) {
      // Parse Content-Range header.
      int64_t rangeStart = 0;
      int64_t rangeEnd = 0;
      int64_t rangeTotal = 0;
      rv = ParseContentRangeHeader(hc, rangeStart, rangeEnd, rangeTotal);
      if (NS_FAILED(rv)) {
        // Content-Range header text should be parse-able.
        CMLOG("Error processing \'Content-Range' for "
              "HTTP_PARTIAL_RESPONSE_CODE: rv[%x] channel[%p] decoder[%p]",
              rv, hc.get(), mDecoder);
        mDecoder->NetworkError();
        CloseChannel();
        return NS_OK;
      }

      // Give some warnings if the ranges are unexpected.
      // XXX These could be error conditions.
      NS_WARN_IF_FALSE(mByteRange.mStart == rangeStart,
                       "response range start does not match request");
      NS_WARN_IF_FALSE(mOffset == rangeStart,
                       "response range start does not match current offset");
      NS_WARN_IF_FALSE(mByteRange.mEnd == rangeEnd,
                       "response range end does not match request");
      // Notify media cache about the length and start offset of data received.
      // Note: If aRangeTotal == -1, then the total bytes is unknown at this stage.
      //       For now, tell the decoder that the stream is infinite.
      if (rangeTotal == -1) {
        boundedSeekLimit = false;
      } else {
        mCacheStream.NotifyDataLength(rangeTotal);
      }
      mCacheStream.NotifyDataStarted(rangeStart);

      mOffset = rangeStart;
      // We received 'Content-Range', so the server accepts range requests.
      acceptsRanges = true;
    } else if (((mOffset > 0) || !mByteRange.IsNull())
               && (responseStatus == HTTP_OK_CODE)) {
      // If we get an OK response but we were seeking, or requesting a byte
      // range, then we have to assume that seeking doesn't work. We also need
      // to tell the cache that it's getting data for the start of the stream.
      mCacheStream.NotifyDataStarted(0);
      mOffset = 0;

      // The server claimed it supported range requests.  It lied.
      acceptsRanges = false;
    } else if (mOffset == 0 &&
               (responseStatus == HTTP_OK_CODE ||
                responseStatus == HTTP_PARTIAL_RESPONSE_CODE)) {
      if (contentLength >= 0) {
        mCacheStream.NotifyDataLength(contentLength);
      }
    }
    // XXX we probably should examine the Content-Range header in case
    // the server gave us a range which is not quite what we asked for

    // If we get an HTTP_OK_CODE response to our byte range request,
    // and the server isn't sending Accept-Ranges:bytes then we don't
    // support seeking.
    seekable =
      responseStatus == HTTP_PARTIAL_RESPONSE_CODE || acceptsRanges;
    if (seekable && boundedSeekLimit) {
      // If range requests are supported, and we did not see an unbounded
      // upper range limit, we assume the resource is bounded.
      dataIsBounded = true;
    }

    mDecoder->SetInfinite(!dataIsBounded);
  }
  mDecoder->SetTransportSeekable(seekable);
  mCacheStream.SetTransportSeekable(seekable);

  {
    MutexAutoLock lock(mLock);
    mIsTransportSeekable = seekable;
    mChannelStatistics->Start();
  }

  mReopenOnError = false;
  // If we are seeking to get metadata, because we are playing an OGG file,
  // ignore if the channel gets closed without us suspending it explicitly. We
  // don't want to tell the element that the download has finished whereas we
  // just happended to have reached the end of the media while seeking.
  mIgnoreClose = mSeekingForMetadata;

  if (mSuspendCount > 0) {
    // Re-suspend the channel if it needs to be suspended
    // No need to call PossiblySuspend here since the channel is
    // definitely in the right state for us in OnStartRequest.
    mChannel->Suspend();
    mIgnoreResume = false;
  }

  // Fires an initial progress event and sets up the stall counter so stall events
  // fire if no download occurs within the required time frame.
  mDecoder->Progress(false);

  return NS_OK;
}

bool
ChannelMediaResource::IsTransportSeekable()
{
  MutexAutoLock lock(mLock);
  return mIsTransportSeekable;
}

nsresult
ChannelMediaResource::ParseContentRangeHeader(nsIHttpChannel * aHttpChan,
                                              int64_t& aRangeStart,
                                              int64_t& aRangeEnd,
                                              int64_t& aRangeTotal)
{
  NS_ENSURE_ARG(aHttpChan);

  nsAutoCString rangeStr;
  nsresult rv = aHttpChan->GetResponseHeader(NS_LITERAL_CSTRING("Content-Range"),
                                             rangeStr);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_FALSE(rangeStr.IsEmpty(), NS_ERROR_ILLEGAL_VALUE);

  // Parse the range header: e.g. Content-Range: bytes 7000-7999/8000.
  int32_t spacePos = rangeStr.Find(NS_LITERAL_CSTRING(" "));
  int32_t dashPos = rangeStr.Find(NS_LITERAL_CSTRING("-"), true, spacePos);
  int32_t slashPos = rangeStr.Find(NS_LITERAL_CSTRING("/"), true, dashPos);

  nsAutoCString aRangeStartText;
  rangeStr.Mid(aRangeStartText, spacePos+1, dashPos-(spacePos+1));
  aRangeStart = aRangeStartText.ToInteger64(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(0 <= aRangeStart, NS_ERROR_ILLEGAL_VALUE);

  nsAutoCString aRangeEndText;
  rangeStr.Mid(aRangeEndText, dashPos+1, slashPos-(dashPos+1));
  aRangeEnd = aRangeEndText.ToInteger64(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(aRangeStart < aRangeEnd, NS_ERROR_ILLEGAL_VALUE);

  nsAutoCString aRangeTotalText;
  rangeStr.Right(aRangeTotalText, rangeStr.Length()-(slashPos+1));
  if (aRangeTotalText[0] == '*') {
    aRangeTotal = -1;
  } else {
    aRangeTotal = aRangeTotalText.ToInteger64(&rv);
    NS_ENSURE_TRUE(aRangeEnd < aRangeTotal, NS_ERROR_ILLEGAL_VALUE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  CMLOG("Received bytes [%lld] to [%lld] of [%lld] for decoder[%p]",
        aRangeStart, aRangeEnd, aRangeTotal, mDecoder);

  return NS_OK;
}

nsresult
ChannelMediaResource::OnStopRequest(nsIRequest* aRequest, nsresult aStatus)
{
  NS_ASSERTION(mChannel.get() == aRequest, "Wrong channel!");
  NS_ASSERTION(mSuspendCount == 0,
               "How can OnStopRequest fire while we're suspended?");

  {
    MutexAutoLock lock(mLock);
    mChannelStatistics->Stop();
  }

#ifdef MOZ_DASH
  // If we were loading a byte range, notify decoder and return.
  // Skip this for unterminated byte range requests, e.g. seeking for whole
  // file downloads.
  if (mByteRangeDownloads) {
    mDecoder->NotifyDownloadEnded(aStatus);
    return NS_OK;
  }
#endif

  // Note that aStatus might have succeeded --- this might be a normal close
  // --- even in situations where the server cut us off because we were
  // suspended. So we need to "reopen on error" in that case too. The only
  // cases where we don't need to reopen are when *we* closed the stream.
  // But don't reopen if we need to seek and we don't think we can... that would
  // cause us to just re-read the stream, which would be really bad.
  if (mReopenOnError &&
      aStatus != NS_ERROR_PARSED_DATA_CACHED && aStatus != NS_BINDING_ABORTED &&
      (mOffset == 0 || mCacheStream.IsTransportSeekable())) {
    // If the stream did close normally, then if the server is seekable we'll
    // just seek to the end of the resource and get an HTTP 416 error because
    // there's nothing there, so this isn't bad.
    nsresult rv = CacheClientSeek(mOffset, false);
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
    nsLoadFlags loadFlags;
    DebugOnly<nsresult> rv = mChannel->GetLoadFlags(&loadFlags);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetLoadFlags() failed!");

    if (loadFlags & nsIRequest::LOAD_BACKGROUND) {
      ModifyLoadFlags(loadFlags & ~nsIRequest::LOAD_BACKGROUND);
    }
  }

  return NS_OK;
}

nsresult
ChannelMediaResource::OnChannelRedirect(nsIChannel* aOld, nsIChannel* aNew,
                                        uint32_t aFlags)
{
  mChannel = aNew;
  SetupChannelHeaders();
  return NS_OK;
}

struct CopySegmentClosure {
  nsCOMPtr<nsIPrincipal> mPrincipal;
  ChannelMediaResource*  mResource;
};

NS_METHOD
ChannelMediaResource::CopySegmentToCache(nsIInputStream *aInStream,
                                         void *aClosure,
                                         const char *aFromSegment,
                                         uint32_t aToOffset,
                                         uint32_t aCount,
                                         uint32_t *aWriteCount)
{
  CopySegmentClosure* closure = static_cast<CopySegmentClosure*>(aClosure);

  closure->mResource->mDecoder->NotifyDataArrived(aFromSegment, aCount, closure->mResource->mOffset);

#ifdef MOZ_DASH
  // For byte range downloads controlled by |DASHDecoder|, there are cases in
  // which the reader's offset is different enough from the channel offset that
  // |MediaCache| requests a |CacheClientSeek| to the reader's offset. This
  // can happen between calls to |CopySegmentToCache|. To avoid copying at
  // incorrect offsets, ensure |MediaCache| copies to the location that
  // |ChannelMediaResource| expects.
  if (closure->mResource->mByteRangeDownloads) {
    closure->mResource->mCacheStream.NotifyDataStarted(closure->mResource->mOffset);
  }
#endif

  // Keep track of where we're up to.
  LOG("%p [ChannelMediaResource]: CopySegmentToCache at mOffset [%lld] add "
      "[%d] bytes for decoder[%p]",
      closure->mResource, closure->mResource->mOffset, aCount,
      closure->mResource->mDecoder);
  closure->mResource->mOffset += aCount;

  closure->mResource->mCacheStream.NotifyDataReceived(aCount, aFromSegment,
                                                      closure->mPrincipal);
  *aWriteCount = aCount;
  return NS_OK;
}

nsresult
ChannelMediaResource::OnDataAvailable(nsIRequest* aRequest,
                                      nsIInputStream* aStream,
                                      uint32_t aCount)
{
  NS_ASSERTION(mChannel.get() == aRequest, "Wrong channel!");

  {
    MutexAutoLock lock(mLock);
    mChannelStatistics->AddBytes(aCount);
  }

  CopySegmentClosure closure;
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  if (secMan && mChannel) {
    secMan->GetChannelPrincipal(mChannel, getter_AddRefs(closure.mPrincipal));
  }
  closure.mResource = this;

  uint32_t count = aCount;
  while (count > 0) {
    uint32_t read;
    nsresult rv = aStream->ReadSegments(CopySegmentToCache, &closure, count,
                                        &read);
    if (NS_FAILED(rv))
      return rv;
    NS_ASSERTION(read > 0, "Read 0 bytes while data was available?");
    count -= read;
  }

  return NS_OK;
}

#ifdef MOZ_DASH
/* |OpenByteRange|
 * For terminated byte range requests, use this function.
 * Callback is |MediaDecoder|::|NotifyByteRangeDownloaded|().
 * See |CacheClientSeek| also.
 */

nsresult
ChannelMediaResource::OpenByteRange(nsIStreamListener** aStreamListener,
                                    MediaByteRange const & aByteRange)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  mByteRangeDownloads = true;
  mByteRange = aByteRange;

  // OpenByteRange may be called multiple times; same URL, different ranges.
  // For the first call using this URL, forward to Open for some init.
  if (mByteRangeFirstOpen) {
    mByteRangeFirstOpen = false;
    return Open(aStreamListener);
  }

  // For subsequent calls, ensure channel is recreated with correct byte range.
  CloseChannel();

  nsresult rv = RecreateChannel();
  NS_ENSURE_SUCCESS(rv, rv);

  return OpenChannel(aStreamListener);
}
#endif

nsresult ChannelMediaResource::Open(nsIStreamListener **aStreamListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  if (!mChannelStatistics) {
    mChannelStatistics = new MediaChannelStatistics();
  }

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

nsresult ChannelMediaResource::OpenChannel(nsIStreamListener** aStreamListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  NS_ENSURE_TRUE(mChannel, NS_ERROR_NULL_POINTER);
  NS_ASSERTION(!mListener, "Listener should have been removed by now");

  if (aStreamListener) {
    *aStreamListener = nullptr;
  }

  if (mByteRange.IsNull()) {
    // We're not making a byte range request, so set the content length,
    // if it's available as an HTTP header. This ensures that MediaResource
    // wrapping objects for platform libraries that expect to know
    // the length of a resource can get it before OnStartRequest() fires.
    nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(mChannel);
    if (hc) {
      int64_t cl = -1;
      if (NS_SUCCEEDED(hc->GetContentLength(&cl)) && cl != -1) {
        mCacheStream.NotifyDataLength(cl);
      }
    }
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
    MediaDecoderOwner* owner = mDecoder->GetMediaOwner();
    NS_ENSURE_TRUE(owner, NS_ERROR_FAILURE);
    HTMLMediaElement* element = owner->GetMediaElement();
    NS_ENSURE_TRUE(element, NS_ERROR_FAILURE);
    if (element->ShouldCheckAllowOrigin()) {
      nsRefPtr<nsCORSListenerProxy> crossSiteListener =
        new nsCORSListenerProxy(mListener,
                                element->NodePrincipal(),
                                false);
      nsresult rv = crossSiteListener->Init(mChannel);
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

    nsresult rv = mChannel->AsyncOpen(listener, nullptr);
    NS_ENSURE_SUCCESS(rv, rv);
    // Tell the media element that we are fetching data from a channel.
    element->DownloadResumed(true);
  }

  return NS_OK;
}

void ChannelMediaResource::SetupChannelHeaders()
{
  // Always use a byte range request even if we're reading from the start
  // of the resource.
  // This enables us to detect if the stream supports byte range
  // requests, and therefore seeking, early.
  nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(mChannel);
  if (hc) {
    // Use |mByteRange| for a specific chunk, or |mOffset| if seeking in a
    // complete file download.
    nsAutoCString rangeString("bytes=");
    if (!mByteRange.IsNull()) {
      rangeString.AppendInt(mByteRange.mStart);
      mOffset = mByteRange.mStart;
    } else {
      rangeString.AppendInt(mOffset);
    }
    rangeString.Append("-");
    if (!mByteRange.IsNull()) {
      rangeString.AppendInt(mByteRange.mEnd);
    }
    hc->SetRequestHeader(NS_LITERAL_CSTRING("Range"), rangeString, false);

    // Send Accept header for video and audio types only (Bug 489071)
    NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");
    MediaDecoderOwner* owner = mDecoder->GetMediaOwner();
    if (!owner) {
      return;
    }
    HTMLMediaElement* element = owner->GetMediaElement();
    if (!element) {
      return;
    }
    element->SetRequestHeaders(hc);
  } else {
    NS_ASSERTION(mOffset == 0, "Don't know how to seek on this channel type");
  }
}

nsresult ChannelMediaResource::Close()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  mCacheStream.Close();
  CloseChannel();
  return NS_OK;
}

already_AddRefed<nsIPrincipal> ChannelMediaResource::GetCurrentPrincipal()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  nsCOMPtr<nsIPrincipal> principal = mCacheStream.GetCurrentPrincipal();
  return principal.forget();
}

bool ChannelMediaResource::CanClone()
{
  return mCacheStream.IsAvailableForSharing();
}

already_AddRefed<MediaResource> ChannelMediaResource::CloneData(MediaDecoder* aDecoder)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  NS_ASSERTION(mCacheStream.IsAvailableForSharing(), "Stream can't be cloned");

  nsRefPtr<ChannelMediaResource> resource =
    new ChannelMediaResource(aDecoder,
                             nullptr,
                             mURI,
                             GetContentType());
  if (resource) {
    // Initially the clone is treated as suspended by the cache, because
    // we don't have a channel. If the cache needs to read data from the clone
    // it will call CacheClientResume (or CacheClientSeek with aResume true)
    // which will recreate the channel. This way, if all of the media data
    // is already in the cache we don't create an unnecessary HTTP channel
    // and perform a useless HTTP transaction.
    resource->mSuspendCount = 1;
    resource->mCacheStream.InitAsClone(&mCacheStream);
    resource->mChannelStatistics = new MediaChannelStatistics(mChannelStatistics);
    resource->mChannelStatistics->Stop();
  }
  return resource.forget();
}

void ChannelMediaResource::CloseChannel()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  {
    MutexAutoLock lock(mLock);
    mChannelStatistics->Stop();
  }

  if (mListener) {
    mListener->Revoke();
    mListener = nullptr;
  }

  if (mChannel) {
    if (mSuspendCount > 0) {
      // Resume the channel before we cancel it
      PossiblyResume();
    }
    // The status we use here won't be passed to the decoder, since
    // we've already revoked the listener. It can however be passed
    // to nsDocumentViewer::LoadComplete if our channel is the one
    // that kicked off creation of a video document. We don't want that
    // document load to think there was an error.
    // NS_ERROR_PARSED_DATA_CACHED is the best thing we have for that
    // at the moment.
    mChannel->Cancel(NS_ERROR_PARSED_DATA_CACHED);
    mChannel = nullptr;
  }
}

nsresult ChannelMediaResource::ReadFromCache(char* aBuffer,
                                             int64_t aOffset,
                                             uint32_t aCount)
{
  return mCacheStream.ReadFromCache(aBuffer, aOffset, aCount);
}

nsresult ChannelMediaResource::Read(char* aBuffer,
                                    uint32_t aCount,
                                    uint32_t* aBytes)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  return mCacheStream.Read(aBuffer, aCount, aBytes);
}

nsresult ChannelMediaResource::ReadAt(int64_t aOffset,
                                      char* aBuffer,
                                      uint32_t aCount,
                                      uint32_t* aBytes)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  return mCacheStream.ReadAt(aOffset, aBuffer, aCount, aBytes);
}

nsresult ChannelMediaResource::Seek(int32_t aWhence, int64_t aOffset)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  CMLOG("Seek requested for aOffset [%lld] for decoder [%p]",
        aOffset, mDecoder);
#ifdef MOZ_DASH
  // Remember |aOffset|, because Media Cache may request a diff offset later.
  if (mByteRangeDownloads) {
    ReentrantMonitorAutoEnter mon(mSeekOffsetMonitor);
    mSeekOffset = aOffset;
  }
#endif
  return mCacheStream.Seek(aWhence, aOffset);
}

void ChannelMediaResource::StartSeekingForMetadata()
{
  mSeekingForMetadata = true;
}

void ChannelMediaResource::EndSeekingForMetadata()
{
  mSeekingForMetadata = false;
}

int64_t ChannelMediaResource::Tell()
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  return mCacheStream.Tell();
}

nsresult ChannelMediaResource::GetCachedRanges(nsTArray<MediaByteRange>& aRanges)
{
  return mCacheStream.GetCachedRanges(aRanges);
}

void ChannelMediaResource::Suspend(bool aCloseImmediately)
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");

  MediaDecoderOwner* owner = mDecoder->GetMediaOwner();
  if (!owner) {
    // Shutting down; do nothing.
    return;
  }
  HTMLMediaElement* element = owner->GetMediaElement();
  if (!element) {
    // Shutting down; do nothing.
    return;
  }

  if (mChannel) {
    if (aCloseImmediately && mCacheStream.IsTransportSeekable()) {
      // Kill off our channel right now, but don't tell anyone about it.
      mIgnoreClose = true;
      CloseChannel();
      element->DownloadSuspended();
    } else if (mSuspendCount == 0) {
      {
        MutexAutoLock lock(mLock);
        mChannelStatistics->Stop();
      }
      PossiblySuspend();
      element->DownloadSuspended();
    }
  }

  ++mSuspendCount;
}

void ChannelMediaResource::Resume()
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");
  NS_ASSERTION(mSuspendCount > 0, "Too many resumes!");

  MediaDecoderOwner* owner = mDecoder->GetMediaOwner();
  if (!owner) {
    // Shutting down; do nothing.
    return;
  }
  HTMLMediaElement* element = owner->GetMediaElement();
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
        mChannelStatistics->Start();
      }
      // if an error occurs after Resume, assume it's because the server
      // timed out the connection and we should reopen it.
      mReopenOnError = true;
      PossiblyResume();
      element->DownloadResumed();
    } else {
      int64_t totalLength = mCacheStream.GetLength();
      // If mOffset is at the end of the stream, then we shouldn't try to
      // seek to it. The seek will fail and be wasted anyway. We can leave
      // the channel dead; if the media cache wants to read some other data
      // in the future, it will call CacheClientSeek itself which will reopen the
      // channel.
      if (totalLength < 0 || mOffset < totalLength) {
        // There is (or may be) data to read at mOffset, so start reading it.
        // Need to recreate the channel.
        CacheClientSeek(mOffset, false);
      }
      element->DownloadResumed();
    }
  }
}

nsresult
ChannelMediaResource::RecreateChannel()
{
  nsLoadFlags loadFlags =
    nsICachingChannel::LOAD_BYPASS_LOCAL_CACHE_IF_BUSY |
    (mLoadInBackground ? nsIRequest::LOAD_BACKGROUND : 0);

  MediaDecoderOwner* owner = mDecoder->GetMediaOwner();
  if (!owner) {
    // The decoder is being shut down, so don't bother opening a new channel
    return NS_OK;
  }
  HTMLMediaElement* element = owner->GetMediaElement();
  if (!element) {
    // The decoder is being shut down, so don't bother opening a new channel
    return NS_OK;
  }
  nsCOMPtr<nsILoadGroup> loadGroup = element->GetDocumentLoadGroup();
  NS_ENSURE_TRUE(loadGroup, NS_ERROR_NULL_POINTER);

  nsresult rv = NS_NewChannel(getter_AddRefs(mChannel),
                              mURI,
                              nullptr,
                              loadGroup,
                              nullptr,
                              loadFlags);

  // We have cached the Content-Type, which should not change. Give a hint to
  // the channel to avoid a sniffing failure, which would be expected because we
  // are probably seeking in the middle of the bitstream, and sniffing relies
  // on the presence of a magic number at the beginning of the stream.
  NS_ASSERTION(!GetContentType().IsEmpty(),
      "When recreating a channel, we should know the Content-Type.");
  mChannel->SetContentType(GetContentType());

  return rv;
}

void
ChannelMediaResource::DoNotifyDataReceived()
{
  mDataReceivedEvent.Revoke();
  mDecoder->NotifyBytesDownloaded();
}

void
ChannelMediaResource::CacheClientNotifyDataReceived()
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");
  // NOTE: this can be called with the media cache lock held, so don't
  // block or do anything which might try to acquire a lock!

  if (mDataReceivedEvent.IsPending())
    return;

  mDataReceivedEvent =
    NS_NewNonOwningRunnableMethod(this, &ChannelMediaResource::DoNotifyDataReceived);
  NS_DispatchToMainThread(mDataReceivedEvent.get(), NS_DISPATCH_NORMAL);
}

class DataEnded : public nsRunnable {
public:
  DataEnded(MediaDecoder* aDecoder, nsresult aStatus) :
    mDecoder(aDecoder), mStatus(aStatus) {}
  NS_IMETHOD Run() {
    mDecoder->NotifyDownloadEnded(mStatus);
    return NS_OK;
  }
private:
  nsRefPtr<MediaDecoder> mDecoder;
  nsresult                 mStatus;
};

void
ChannelMediaResource::CacheClientNotifyDataEnded(nsresult aStatus)
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");
  // NOTE: this can be called with the media cache lock held, so don't
  // block or do anything which might try to acquire a lock!

  nsCOMPtr<nsIRunnable> event = new DataEnded(mDecoder, aStatus);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}

void
ChannelMediaResource::CacheClientNotifyPrincipalChanged()
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");

  mDecoder->NotifyPrincipalChanged();
}

nsresult
ChannelMediaResource::CacheClientSeek(int64_t aOffset, bool aResume)
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");

  CMLOG("CacheClientSeek requested for aOffset [%lld] for decoder [%p]",
        aOffset, mDecoder);

#ifndef MOZ_DASH
  CloseChannel();
#else
  // |CloseChannel| immediately for non-byte-range downloads.
  if (!mByteRangeDownloads) {
    CloseChannel();
  } else if (mChannel) {
    // Only close byte range channels if they are not in pending state.
    bool isPending = false;
    nsresult rv = mChannel->IsPending(&isPending);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!isPending) {
      CloseChannel();
    }
  }
#endif

  if (aResume) {
    NS_ASSERTION(mSuspendCount > 0, "Too many resumes!");
    // No need to mess with the channel, since we're making a new one
    --mSuspendCount;
  }

#ifdef MOZ_DASH  // Note: For chunked downloads, e.g. DASH, we need to determine which chunk
  // contains the requested offset, |mOffset|. This is either previously
  // requested in |Seek| or updated to the most recent bytes downloaded.
  // So the process below is:
  //   1 - Query decoder for chunk containing desired offset, |mOffset|.
  //       Return silently if the offset is not available; suggests decoder is
  //         yet to get range information.
  //       Return with NetworkError for all other errors.
  //
  //   2 - Adjust |mByteRange|.mStart to |aOffset|, requested by media cache.
  //       For seeking, the media cache always requests the start of the cache
  //       block, so we need to adjust the first chunk of a seek.
  //       E.g. For "DASH-WebM On Demand" this means the first chunk after
  //       seeking will most likely be larger than the subsegment (cluster).
  //
  //   3 - Call |OpenByteRange| requesting |mByteRange| bytes.

  if (mByteRangeDownloads) {
    // Query decoder for chunk containing desired offset.
    nsresult rv;
    {
      ReentrantMonitorAutoEnter mon(mSeekOffsetMonitor);
      // Only continue with seek request if a prior call to |Seek| was made.
      // If |Seek| was not called previously, it means the media cache is
      // seeking on its own.
      // E.g. For those WebM files which are encoded with cues at the end of
      // the file, when the cues are parsed, the reader and media cache
      // automatically return to the first offset not downloaded, normally the
      // first byte after init data. This results in |MediaCache| requesting
      // |aOffset| = 0 (aligning to the start of the cache block. Ignore this
      // and let |DASHDecoder| decide which bytes to download and when.
      if (mSeekOffset >= 0) {
        rv = mDecoder->GetByteRangeForSeek(mSeekOffset, mByteRange);
        // Cache may try to seek from the next uncached byte: this offset may
        // be after the byte range being seeked, i.e. the range containing
        // |mSeekOffset|, which is the offset actually requested by the reader.
        // This case means that the seeked range is already cached. For byte
        // range downloads, we do not permit the cache to request bytes outside
        // the seeked range. Instead, the decoder is responsible for
        // controlling the sequence of byte range downloads. As such, return
        // silently, and do NOT request a new download.
        if (NS_SUCCEEDED(rv) && !mByteRange.IsNull() &&
            aOffset > mByteRange.mEnd) {
          rv = NS_ERROR_NOT_AVAILABLE;
          mByteRange.Clear();
        }
        mSeekOffset = -1;
      } else if (mByteRange.mStart <= aOffset && aOffset <= mByteRange.mEnd) {
        CMLOG("Trying to resume download at offset [%lld].", aOffset);
        rv = NS_OK;
      } else {
        CMLOG("MediaCache [%p] trying to seek independently to offset [%lld].",
            &mCacheStream, aOffset);
        rv = NS_ERROR_NOT_AVAILABLE;
      }
    }
    if (rv == NS_ERROR_NOT_AVAILABLE) {
      // Decoder will not make byte ranges available for non-active streams, or
      // if range information is not yet available, or for metadata bytes if
      // they have already been downloaded and read. In all cases, it is ok to
      // return silently and assume that the decoder will request the correct
      // byte range when range information becomes available.
      CMLOG("Byte range not available for decoder [%p]; returning "
            "silently.", mDecoder);
      return NS_OK;
    } else if (NS_FAILED(rv) || mByteRange.IsNull()) {
      // Decoder reported an error we don't want to handle here; just return.
      CMLOG("Error getting byte range: seek offset[%lld] cache offset[%lld] "
            "decoder[%p]", mSeekOffset, aOffset, mDecoder);
      mDecoder->NetworkError();
      CloseChannel();
      return rv;
    }
    // Adjust the byte range to start where the media cache requested.
    mByteRange.mStart = mOffset = aOffset;
    return OpenByteRange(nullptr, mByteRange);
  }
#endif

  mOffset = aOffset;

  if (mSuspendCount > 0) {
    // Close the existing channel to force the channel to be recreated at
    // the correct offset upon resume.
    if (mChannel) {
      mIgnoreClose = true;
      CloseChannel();
    }
    return NS_OK;
  }

  nsresult rv = RecreateChannel();
  if (NS_FAILED(rv))
    return rv;

  return OpenChannel(nullptr);
}

void
ChannelMediaResource::FlushCache()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  // Ensure that data in the cache's partial block is written to disk.
  mCacheStream.FlushPartialBlock();
}

void
ChannelMediaResource::NotifyLastByteRange()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  // Tell media cache that the last data has been downloaded.
  // Note: subsequent seeks will require re-opening the channel etc.
  mCacheStream.NotifyDataEnded(NS_OK);

}

nsresult
ChannelMediaResource::CacheClientSuspend()
{
  Suspend(false);

  mDecoder->NotifySuspendedStatusChanged();
  return NS_OK;
}

nsresult
ChannelMediaResource::CacheClientResume()
{
  Resume();

  mDecoder->NotifySuspendedStatusChanged();
  return NS_OK;
}

int64_t
ChannelMediaResource::GetNextCachedData(int64_t aOffset)
{
  return mCacheStream.GetNextCachedData(aOffset);
}

int64_t
ChannelMediaResource::GetCachedDataEnd(int64_t aOffset)
{
  return mCacheStream.GetCachedDataEnd(aOffset);
}

bool
ChannelMediaResource::IsDataCachedToEndOfResource(int64_t aOffset)
{
  return mCacheStream.IsDataCachedToEndOfStream(aOffset);
}

void
ChannelMediaResource::EnsureCacheUpToDate()
{
  mCacheStream.EnsureCacheUpdate();
}

bool
ChannelMediaResource::IsSuspendedByCache()
{
  return mCacheStream.AreAllStreamsForResourceSuspended();
}

bool
ChannelMediaResource::IsSuspended()
{
  MutexAutoLock lock(mLock);
  return mSuspendCount > 0;
}

void
ChannelMediaResource::SetReadMode(MediaCacheStream::ReadMode aMode)
{
  mCacheStream.SetReadMode(aMode);
}

void
ChannelMediaResource::SetPlaybackRate(uint32_t aBytesPerSecond)
{
  mCacheStream.SetPlaybackRate(aBytesPerSecond);
}

void
ChannelMediaResource::Pin()
{
  mCacheStream.Pin();
}

void
ChannelMediaResource::Unpin()
{
  mCacheStream.Unpin();
}

double
ChannelMediaResource::GetDownloadRate(bool* aIsReliable)
{
  MutexAutoLock lock(mLock);
  return mChannelStatistics->GetRate(aIsReliable);
}

int64_t
ChannelMediaResource::GetLength()
{
  return mCacheStream.GetLength();
}

void
ChannelMediaResource::PossiblySuspend()
{
  bool isPending = false;
  nsresult rv = mChannel->IsPending(&isPending);
  if (NS_SUCCEEDED(rv) && isPending) {
    mChannel->Suspend();
    mIgnoreResume = false;
  } else {
    mIgnoreResume = true;
  }
}

void
ChannelMediaResource::PossiblyResume()
{
  if (!mIgnoreResume) {
    mChannel->Resume();
  } else {
    mIgnoreResume = false;
  }
}

class FileMediaResource : public BaseMediaResource
{
public:
  FileMediaResource(MediaDecoder* aDecoder,
                    nsIChannel* aChannel,
                    nsIURI* aURI,
                    const nsACString& aContentType) :
    BaseMediaResource(aDecoder, aChannel, aURI, aContentType),
    mSize(-1),
    mLock("FileMediaResource.mLock"),
    mSizeInitialized(false)
  {
  }
  ~FileMediaResource()
  {
  }

  // Main thread
  virtual nsresult Open(nsIStreamListener** aStreamListener);
  virtual nsresult Close();
  virtual void     Suspend(bool aCloseImmediately) {}
  virtual void     Resume() {}
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal();
  virtual bool     CanClone();
  virtual already_AddRefed<MediaResource> CloneData(MediaDecoder* aDecoder);
  virtual nsresult ReadFromCache(char* aBuffer, int64_t aOffset, uint32_t aCount);

  // These methods are called off the main thread.

  // Other thread
  virtual void     SetReadMode(MediaCacheStream::ReadMode aMode) {}
  virtual void     SetPlaybackRate(uint32_t aBytesPerSecond) {}
  virtual nsresult Read(char* aBuffer, uint32_t aCount, uint32_t* aBytes);
  virtual nsresult ReadAt(int64_t aOffset, char* aBuffer,
                          uint32_t aCount, uint32_t* aBytes);
  virtual nsresult Seek(int32_t aWhence, int64_t aOffset);
  virtual void     StartSeekingForMetadata() {};
  virtual void     EndSeekingForMetadata() {};
  virtual int64_t  Tell();

  // Any thread
  virtual void    Pin() {}
  virtual void    Unpin() {}
  virtual double  GetDownloadRate(bool* aIsReliable)
  {
    // The data's all already here
    *aIsReliable = true;
    return 100*1024*1024; // arbitray, use 100MB/s
  }
  virtual int64_t GetLength() {
    MutexAutoLock lock(mLock);

    EnsureSizeInitialized();
    return mSizeInitialized ? mSize : 0;
  }
  virtual int64_t GetNextCachedData(int64_t aOffset)
  {
    MutexAutoLock lock(mLock);

    EnsureSizeInitialized();
    return (aOffset < mSize) ? aOffset : -1;
  }
  virtual int64_t GetCachedDataEnd(int64_t aOffset) {
    MutexAutoLock lock(mLock);

    EnsureSizeInitialized();
    return std::max(aOffset, mSize);
  }
  virtual bool    IsDataCachedToEndOfResource(int64_t aOffset) { return true; }
  virtual bool    IsSuspendedByCache() { return false; }
  virtual bool    IsSuspended() { return false; }
  virtual bool    IsTransportSeekable() MOZ_OVERRIDE { return true; }

  nsresult GetCachedRanges(nsTArray<MediaByteRange>& aRanges);

protected:
  // These Unsafe variants of Read and Seek perform their operations
  // without acquiring mLock. The caller must obtain the lock before
  // calling. The implmentation of Read, Seek and ReadAt obtains the
  // lock before calling these Unsafe variants to read or seek.
  nsresult UnsafeRead(char* aBuffer, uint32_t aCount, uint32_t* aBytes);
  nsresult UnsafeSeek(int32_t aWhence, int64_t aOffset);
private:
  // Ensures mSize is initialized, if it can be.
  // mLock must be held when this is called, and mInput must be non-null.
  void EnsureSizeInitialized();

  // The file size, or -1 if not known. Immutable after Open().
  // Can be used from any thread.
  int64_t mSize;

  // This lock handles synchronisation between calls to Close() and
  // the Read, Seek, etc calls. Close must not be called while a
  // Read or Seek is in progress since it resets various internal
  // values to null.
  // This lock protects mSeekable, mInput, mSize, and mSizeInitialized.
  Mutex mLock;

  // Seekable stream interface to file. This can be used from any
  // thread.
  nsCOMPtr<nsISeekableStream> mSeekable;

  // Input stream for the media data. This can be used from any
  // thread.
  nsCOMPtr<nsIInputStream>  mInput;

  // Whether we've attempted to initialize mSize. Note that mSize can be -1
  // when mSizeInitialized is true if we tried and failed to get the size
  // of the file.
  bool mSizeInitialized;
};

class LoadedEvent : public nsRunnable
{
public:
  LoadedEvent(MediaDecoder* aDecoder) :
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
  nsRefPtr<MediaDecoder> mDecoder;
};

void FileMediaResource::EnsureSizeInitialized()
{
  mLock.AssertCurrentThreadOwns();
  NS_ASSERTION(mInput, "Must have file input stream");
  if (mSizeInitialized) {
    return;
  }
  mSizeInitialized = true;
  // Get the file size and inform the decoder.
  uint64_t size;
  nsresult res = mInput->Available(&size);
  if (NS_SUCCEEDED(res) && size <= INT64_MAX) {
    mSize = (int64_t)size;
    nsCOMPtr<nsIRunnable> event = new LoadedEvent(mDecoder);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }
}

nsresult FileMediaResource::GetCachedRanges(nsTArray<MediaByteRange>& aRanges)
{
  MutexAutoLock lock(mLock);

  EnsureSizeInitialized();
  if (mSize == -1) {
    return NS_ERROR_FAILURE;
  }
  aRanges.AppendElement(MediaByteRange(0, mSize));
  return NS_OK;
}

nsresult FileMediaResource::Open(nsIStreamListener** aStreamListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  if (aStreamListener) {
    *aStreamListener = nullptr;
  }

  nsresult rv = NS_OK;
  if (aStreamListener) {
    // The channel is already open. We need a synchronous stream that
    // implements nsISeekableStream, so we have to find the underlying
    // file and reopen it
    nsCOMPtr<nsIFileChannel> fc(do_QueryInterface(mChannel));
    if (fc) {
      nsCOMPtr<nsIFile> file;
      rv = fc->GetFile(getter_AddRefs(file));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = NS_NewLocalFileInputStream(getter_AddRefs(mInput), file);
    } else if (IsBlobURI(mURI)) {
      rv = NS_GetStreamForBlobURI(mURI, getter_AddRefs(mInput));
    }
  } else {
    // Ensure that we never load a local file from some page on a
    // web server.
    MediaDecoderOwner* owner = mDecoder->GetMediaOwner();
    NS_ENSURE_TRUE(owner, NS_ERROR_FAILURE);
    HTMLMediaElement* element = owner->GetMediaElement();
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

  return NS_OK;
}

nsresult FileMediaResource::Close()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  // Since mChennel is only accessed by main thread, there is no necessary to
  // take the lock.
  if (mChannel) {
    mChannel->Cancel(NS_ERROR_PARSED_DATA_CACHED);
    mChannel = nullptr;
  }

  return NS_OK;
}

already_AddRefed<nsIPrincipal> FileMediaResource::GetCurrentPrincipal()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  nsCOMPtr<nsIPrincipal> principal;
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  if (!secMan || !mChannel)
    return nullptr;
  secMan->GetChannelPrincipal(mChannel, getter_AddRefs(principal));
  return principal.forget();
}

bool FileMediaResource::CanClone()
{
  return true;
}

already_AddRefed<MediaResource> FileMediaResource::CloneData(MediaDecoder* aDecoder)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  MediaDecoderOwner* owner = mDecoder->GetMediaOwner();
  if (!owner) {
    // The decoder is being shut down, so we can't clone
    return nullptr;
  }
  HTMLMediaElement* element = owner->GetMediaElement();
  if (!element) {
    // The decoder is being shut down, so we can't clone
    return nullptr;
  }
  nsCOMPtr<nsILoadGroup> loadGroup = element->GetDocumentLoadGroup();
  NS_ENSURE_TRUE(loadGroup, nullptr);

  nsCOMPtr<nsIChannel> channel;
  nsresult rv =
    NS_NewChannel(getter_AddRefs(channel), mURI, nullptr, loadGroup, nullptr, 0);
  if (NS_FAILED(rv))
    return nullptr;

  nsRefPtr<MediaResource> resource(new FileMediaResource(aDecoder, channel, mURI, GetContentType()));
  return resource.forget();
}

nsresult FileMediaResource::ReadFromCache(char* aBuffer, int64_t aOffset, uint32_t aCount)
{
  MutexAutoLock lock(mLock);

  EnsureSizeInitialized();
  int64_t offset = 0;
  nsresult res = mSeekable->Tell(&offset);
  NS_ENSURE_SUCCESS(res,res);
  res = mSeekable->Seek(nsISeekableStream::NS_SEEK_SET, aOffset);
  NS_ENSURE_SUCCESS(res,res);
  uint32_t bytesRead = 0;
  do {
    uint32_t x = 0;
    uint32_t bytesToRead = aCount - bytesRead;
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

nsresult FileMediaResource::Read(char* aBuffer, uint32_t aCount, uint32_t* aBytes)
{
  MutexAutoLock lock(mLock);
  return UnsafeRead(aBuffer, aCount, aBytes);
}

nsresult FileMediaResource::UnsafeRead(char* aBuffer, uint32_t aCount, uint32_t* aBytes)
{
  EnsureSizeInitialized();
  return mInput->Read(aBuffer, aCount, aBytes);
}

nsresult FileMediaResource::ReadAt(int64_t aOffset, char* aBuffer,
                                   uint32_t aCount, uint32_t* aBytes)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  MutexAutoLock lock(mLock);
  nsresult rv = UnsafeSeek(nsISeekableStream::NS_SEEK_SET, aOffset);
  if (NS_FAILED(rv)) return rv;
  return UnsafeRead(aBuffer, aCount, aBytes);
}

nsresult FileMediaResource::Seek(int32_t aWhence, int64_t aOffset)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  MutexAutoLock lock(mLock);
  return UnsafeSeek(aWhence, aOffset);
}

nsresult FileMediaResource::UnsafeSeek(int32_t aWhence, int64_t aOffset)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  if (!mSeekable)
    return NS_ERROR_FAILURE;
  EnsureSizeInitialized();
  return mSeekable->Seek(aWhence, aOffset);
}

int64_t FileMediaResource::Tell()
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  MutexAutoLock lock(mLock);
  if (!mSeekable)
    return 0;
  EnsureSizeInitialized();

  int64_t offset = 0;
  mSeekable->Tell(&offset);
  return offset;
}

already_AddRefed<MediaResource>
MediaResource::Create(MediaDecoder* aDecoder, nsIChannel* aChannel)
{
  NS_ASSERTION(NS_IsMainThread(),
               "MediaResource::Open called on non-main thread");

  // If the channel was redirected, we want the post-redirect URI;
  // but if the URI scheme was expanded, say from chrome: to jar:file:,
  // we want the original URI.
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsAutoCString contentType;
  aChannel->GetContentType(contentType);

  nsCOMPtr<nsIFileChannel> fc = do_QueryInterface(aChannel);
  nsRefPtr<MediaResource> resource;
  if (fc || IsBlobURI(uri)) {
    resource = new FileMediaResource(aDecoder, aChannel, uri, contentType);
  } else {
    resource = new ChannelMediaResource(aDecoder, aChannel, uri, contentType);
  }
  return resource.forget();
}

void BaseMediaResource::MoveLoadsToBackground() {
  NS_ASSERTION(!mLoadInBackground, "Why are you calling this more than once?");
  mLoadInBackground = true;
  if (!mChannel) {
    // No channel, resource is probably already loaded.
    return;
  }

  MediaDecoderOwner* owner = mDecoder->GetMediaOwner();
  if (!owner) {
    NS_WARNING("Null owner in MediaResource::MoveLoadsToBackground()");
    return;
  }
  HTMLMediaElement* element = owner->GetMediaElement();
  if (!element) {
    NS_WARNING("Null element in MediaResource::MoveLoadsToBackground()");
    return;
  }

  bool isPending = false;
  if (NS_SUCCEEDED(mChannel->IsPending(&isPending)) &&
      isPending) {
    nsLoadFlags loadFlags;
    DebugOnly<nsresult> rv = mChannel->GetLoadFlags(&loadFlags);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetLoadFlags() failed!");

    loadFlags |= nsIRequest::LOAD_BACKGROUND;
    ModifyLoadFlags(loadFlags);
  }
}

void BaseMediaResource::ModifyLoadFlags(nsLoadFlags aFlags)
{
  nsCOMPtr<nsILoadGroup> loadGroup;
  DebugOnly<nsresult> rv = mChannel->GetLoadGroup(getter_AddRefs(loadGroup));
  NS_ASSERTION(NS_SUCCEEDED(rv), "GetLoadGroup() failed!");

  nsresult status;
  mChannel->GetStatus(&status);

  // Note: if (NS_FAILED(status)), the channel won't be in the load group.
  if (loadGroup &&
      NS_SUCCEEDED(status)) {
    rv = loadGroup->RemoveRequest(mChannel, nullptr, status);
    NS_ASSERTION(NS_SUCCEEDED(rv), "RemoveRequest() failed!");
  }

  rv = mChannel->SetLoadFlags(aFlags);
  NS_ASSERTION(NS_SUCCEEDED(rv), "SetLoadFlags() failed!");

  if (loadGroup &&
      NS_SUCCEEDED(status)) {
    rv = loadGroup->AddRequest(mChannel, nullptr);
    NS_ASSERTION(NS_SUCCEEDED(rv), "AddRequest() failed!");
  }
}

} // namespace mozilla

