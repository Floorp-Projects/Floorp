/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "DecoderTraits.h"
#include "MediaResource.h"
#include "MediaResourceCallback.h"

#include "mozilla/Mutex.h"
#include "nsDebug.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsIFile.h"
#include "nsIFileChannel.h"
#include "nsIFileStreams.h"
#include "nsIHttpChannel.h"
#include "nsISeekableStream.h"
#include "nsIInputStream.h"
#include "nsIRequestObserver.h"
#include "nsIStreamListener.h"
#include "nsIScriptSecurityManager.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "nsError.h"
#include "nsICachingChannel.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsContentUtils.h"
#include "nsHostObjectProtocolHandler.h"
#include <algorithm>
#include "nsProxyRelease.h"
#include "nsIContentPolicy.h"

#ifdef MOZ_ANDROID_HLS_SUPPORT
#include "HLSResource.h"
#endif

using mozilla::media::TimeUnit;

#undef LOG
#undef ILOG

mozilla::LazyLogModule gMediaResourceLog("MediaResource");
// Debug logging macro with object pointer and class name.
#define LOG(msg, ...) MOZ_LOG(gMediaResourceLog, mozilla::LogLevel::Debug, \
  ("%p " msg, this, ##__VA_ARGS__))

mozilla::LazyLogModule gMediaResourceIndexLog("MediaResourceIndex");
// Debug logging macro with object pointer and class name.
#define ILOG(msg, ...)                                                         \
  MOZ_LOG(gMediaResourceIndexLog,                                              \
          mozilla::LogLevel::Debug,                                            \
          ("%p " msg, this, ##__VA_ARGS__))

static const uint32_t HTTP_OK_CODE = 200;
static const uint32_t HTTP_PARTIAL_RESPONSE_CODE = 206;

namespace mozilla {

void
MediaResource::Destroy()
{
  // Ensures we only delete the MediaResource on the main thread.
  if (NS_IsMainThread()) {
    delete this;
    return;
  }
  nsresult rv = SystemGroup::Dispatch(
    "MediaResource::Destroy",
    TaskCategory::Other,
    NewNonOwningRunnableMethod(
      "MediaResource::Destroy", this, &MediaResource::Destroy));
  MOZ_ALWAYS_SUCCEEDS(rv);
}

NS_IMPL_ADDREF(MediaResource)
NS_IMPL_RELEASE_WITH_DESTROY(MediaResource, Destroy())
NS_IMPL_QUERY_INTERFACE0(MediaResource)

ChannelMediaResource::ChannelMediaResource(
  MediaResourceCallback* aCallback,
  nsIChannel* aChannel,
  nsIURI* aURI,
  const MediaContainerType& aContainerType,
  bool aIsPrivateBrowsing)
  : BaseMediaResource(aCallback, aChannel, aURI, aContainerType)
  , mOffset(0)
  , mReopenOnError(false)
  , mIgnoreClose(false)
  , mCacheStream(this, aIsPrivateBrowsing)
  , mLock("ChannelMediaResource.mLock")
  , mIgnoreResume(false)
  , mSuspendAgent(mChannel)
{
}

ChannelMediaResource::ChannelMediaResource(
  MediaResourceCallback* aCallback,
  nsIChannel* aChannel,
  nsIURI* aURI,
  const MediaContainerType& aContainerType,
  const MediaChannelStatistics& aStatistics)
  : BaseMediaResource(aCallback, aChannel, aURI, aContainerType)
  , mOffset(0)
  , mReopenOnError(false)
  , mIgnoreClose(false)
  , mCacheStream(this, /* aIsPrivateBrowsing = */ false)
  , mLock("ChannelMediaResource.mLock")
  , mChannelStatistics(aStatistics)
  , mIgnoreResume(false)
  , mSuspendAgent(mChannel)
{
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
NS_IMPL_ISUPPORTS(ChannelMediaResource::Listener,
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

  MediaDecoderOwner* owner = mCallback->GetMediaOwner();
  NS_ENSURE_TRUE(owner, NS_ERROR_FAILURE);
  dom::HTMLMediaElement* element = owner->GetMediaElement();
  NS_ENSURE_TRUE(element, NS_ERROR_FAILURE);
  nsresult status;
  nsresult rv = aRequest->GetStatus(&status);
  NS_ENSURE_SUCCESS(rv, rv);

  if (status == NS_BINDING_ABORTED) {
    // Request was aborted before we had a chance to receive any data, or
    // even an OnStartRequest(). Close the channel. This is important, as
    // we don't want to mess up our state, as if we're cloned that would
    // cause the clone to copy incorrect metadata (like whether we're
    // infinite for example).
    CloseChannel();
    return status;
  }

  if (element->ShouldCheckAllowOrigin()) {
    // If the request was cancelled by nsCORSListenerProxy due to failing
    // the CORS security check, send an error through to the media element.
    if (status == NS_ERROR_DOM_BAD_URI) {
      mCallback->NotifyNetworkError();
      return NS_ERROR_DOM_BAD_URI;
    }
  }

  nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(aRequest);
  bool seekable = false;
  if (hc) {
    uint32_t responseStatus = 0;
    Unused << hc->GetResponseStatus(&responseStatus);
    bool succeeded = false;
    Unused << hc->GetRequestSucceeded(&succeeded);

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
        mCallback->NotifyNetworkError();
      }

      // This disconnects our listener so we don't get any more data. We
      // certainly don't want an error page to end up in our cache!
      CloseChannel();
      return NS_OK;
    }

    nsAutoCString ranges;
    Unused << hc->GetResponseHeader(NS_LITERAL_CSTRING("Accept-Ranges"),
                                    ranges);
    bool acceptsRanges = ranges.EqualsLiteral("bytes");
    // True if this channel will not return an unbounded amount of data
    bool dataIsBounded = false;

    int64_t contentLength = -1;
    hc->GetContentLength(&contentLength);
    if (contentLength >= 0 &&
        (responseStatus == HTTP_OK_CODE ||
         responseStatus == HTTP_PARTIAL_RESPONSE_CODE)) {
      // "OK" status means Content-Length is for the whole resource.
      // Since that's bounded, we know we have a finite-length resource.
      dataIsBounded = true;
    }

    // Assume Range requests have a bounded upper limit unless the
    // Content-Range header tells us otherwise.
    bool boundedSeekLimit = true;
    // Check response code for byte-range requests (seeking, chunk requests).
    if (responseStatus == HTTP_PARTIAL_RESPONSE_CODE) {
      // Parse Content-Range header.
      int64_t rangeStart = 0;
      int64_t rangeEnd = 0;
      int64_t rangeTotal = 0;
      rv = ParseContentRangeHeader(hc, rangeStart, rangeEnd, rangeTotal);

      // We received 'Content-Range', so the server accepts range requests.
      bool gotRangeHeader = NS_SUCCEEDED(rv);

      if (gotRangeHeader) {
        // We received 'Content-Range', so the server accepts range requests.
        // Notify media cache about the length and start offset of data received.
        // Note: If aRangeTotal == -1, then the total bytes is unknown at this stage.
        //       For now, tell the decoder that the stream is infinite.
        if (rangeTotal == -1) {
          boundedSeekLimit = false;
        } else {
          contentLength = std::max(contentLength, rangeTotal);
        }
        // Give some warnings if the ranges are unexpected.
        // XXX These could be error conditions.
        NS_WARNING_ASSERTION(
          mOffset == rangeStart,
          "response range start does not match current offset");
        mOffset = rangeStart;
        mCacheStream.NotifyDataStarted(rangeStart);
      }
      acceptsRanges = gotRangeHeader;
    } else if (mOffset > 0 && responseStatus == HTTP_OK_CODE) {
      // If we get an OK response but we were seeking, or requesting a byte
      // range, then we have to assume that seeking doesn't work. We also need
      // to tell the cache that it's getting data for the start of the stream.
      mCacheStream.NotifyDataStarted(0);
      mOffset = 0;

      // The server claimed it supported range requests.  It lied.
      acceptsRanges = false;
    }
    if (mOffset == 0 && contentLength >= 0 &&
        (responseStatus == HTTP_OK_CODE ||
         responseStatus == HTTP_PARTIAL_RESPONSE_CODE)) {
      mCacheStream.NotifyDataLength(contentLength);
    }
    // XXX we probably should examine the Content-Range header in case
    // the server gave us a range which is not quite what we asked for

    // If we get an HTTP_OK_CODE response to our byte range request,
    // and the server isn't sending Accept-Ranges:bytes then we don't
    // support seeking.
    seekable = acceptsRanges;
    if (seekable && boundedSeekLimit) {
      // If range requests are supported, and we did not see an unbounded
      // upper range limit, we assume the resource is bounded.
      dataIsBounded = true;
    }

    mCallback->SetInfinite(!dataIsBounded);
  }
  mCacheStream.SetTransportSeekable(seekable);

  {
    MutexAutoLock lock(mLock);
    mChannelStatistics.Start();
  }

  mReopenOnError = false;
  mIgnoreClose = false;

  mSuspendAgent.UpdateSuspendedStatusIfNeeded();

  // Fires an initial progress event.
  owner->DownloadProgressed();

  return NS_OK;
}

bool
ChannelMediaResource::IsTransportSeekable()
{
  return mCacheStream.IsTransportSeekable();
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

  LOG("Received bytes [%" PRId64 "] to [%" PRId64 "] of [%" PRId64 "] for decoder[%p]",
      aRangeStart, aRangeEnd, aRangeTotal, mCallback.get());

  return NS_OK;
}

nsresult
ChannelMediaResource::OnStopRequest(nsIRequest* aRequest, nsresult aStatus)
{
  NS_ASSERTION(mChannel.get() == aRequest, "Wrong channel!");
  NS_ASSERTION(!mSuspendAgent.IsSuspended(),
               "How can OnStopRequest fire while we're suspended?");

  {
    MutexAutoLock lock(mLock);
    mChannelStatistics.Stop();
  }

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
  mSuspendAgent.NotifyChannelOpened(mChannel);
  return SetupChannelHeaders();
}

nsresult
ChannelMediaResource::CopySegmentToCache(nsIPrincipal* aPrincipal,
                                         const char* aFromSegment,
                                         uint32_t aCount,
                                         uint32_t* aWriteCount)
{
  // Keep track of where we're up to.
  LOG("CopySegmentToCache at mOffset [%" PRId64 "] add "
      "[%d] bytes for decoder[%p]",
      mOffset, aCount, mCallback.get());
  mOffset += aCount;
  mCacheStream.NotifyDataReceived(aCount, aFromSegment, aPrincipal);
  *aWriteCount = aCount;
  return NS_OK;
}


struct CopySegmentClosure {
  nsCOMPtr<nsIPrincipal> mPrincipal;
  ChannelMediaResource*  mResource;
};

nsresult
ChannelMediaResource::CopySegmentToCache(nsIInputStream* aInStream,
                                         void* aClosure,
                                         const char* aFromSegment,
                                         uint32_t aToOffset,
                                         uint32_t aCount,
                                         uint32_t* aWriteCount)
{
  CopySegmentClosure* closure = static_cast<CopySegmentClosure*>(aClosure);
  return closure->mResource->CopySegmentToCache(
    closure->mPrincipal, aFromSegment, aCount, aWriteCount);
}

nsresult
ChannelMediaResource::OnDataAvailable(nsIRequest* aRequest,
                                      nsIInputStream* aStream,
                                      uint32_t aCount)
{
  NS_ASSERTION(mChannel.get() == aRequest, "Wrong channel!");

  {
    MutexAutoLock lock(mLock);
    mChannelStatistics.AddBytes(aCount);
  }

  CopySegmentClosure closure;
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  if (secMan && mChannel) {
    secMan->GetChannelResultPrincipal(mChannel, getter_AddRefs(closure.mPrincipal));
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

nsresult ChannelMediaResource::Open(nsIStreamListener **aStreamListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  int64_t cl = -1;
  if (mChannel) {
    nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(mChannel);
    if (hc) {
      if (NS_FAILED(hc->GetContentLength(&cl))) {
        cl = -1;
      }
    }
  }

  nsresult rv = mCacheStream.Init(cl);
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

  // Set the content length, if it's available as an HTTP header.
  // This ensures that MediaResource wrapping objects for platform libraries
  // that expect to know the length of a resource can get it before
  // OnStartRequest() fires.
  nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(mChannel);
  if (hc) {
    int64_t cl = -1;
    if (NS_SUCCEEDED(hc->GetContentLength(&cl)) && cl != -1) {
      mCacheStream.NotifyDataLength(cl);
    }
  }

  mListener = new Listener(this);
  if (aStreamListener) {
    *aStreamListener = mListener;
    NS_ADDREF(*aStreamListener);
  } else {
    nsresult rv = mChannel->SetNotificationCallbacks(mListener.get());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = SetupChannelHeaders();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mChannel->AsyncOpen2(mListener);
    NS_ENSURE_SUCCESS(rv, rv);

    // Tell the media element that we are fetching data from a channel.
    MediaDecoderOwner* owner = mCallback->GetMediaOwner();
    NS_ENSURE_TRUE(owner, NS_ERROR_FAILURE);
    dom::HTMLMediaElement* element = owner->GetMediaElement();
    element->DownloadResumed(true);
  }

  return NS_OK;
}

nsresult ChannelMediaResource::SetupChannelHeaders()
{
  // Always use a byte range request even if we're reading from the start
  // of the resource.
  // This enables us to detect if the stream supports byte range
  // requests, and therefore seeking, early.
  nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(mChannel);
  if (hc) {
    // Use |mOffset| if seeking in a complete file download.
    nsAutoCString rangeString("bytes=");
    rangeString.AppendInt(mOffset);
    rangeString.Append('-');
    nsresult rv = hc->SetRequestHeader(NS_LITERAL_CSTRING("Range"), rangeString, false);
    NS_ENSURE_SUCCESS(rv, rv);

    // Send Accept header for video and audio types only (Bug 489071)
    NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");
    MediaDecoderOwner* owner = mCallback->GetMediaOwner();
    NS_ENSURE_TRUE(owner, NS_ERROR_FAILURE);
    dom::HTMLMediaElement* element = owner->GetMediaElement();
    NS_ENSURE_TRUE(element, NS_ERROR_FAILURE);
    element->SetRequestHeaders(hc);
  } else {
    NS_ASSERTION(mOffset == 0, "Don't know how to seek on this channel type");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
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

already_AddRefed<MediaResource> ChannelMediaResource::CloneData(MediaResourceCallback* aCallback)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  NS_ASSERTION(mCacheStream.IsAvailableForSharing(), "Stream can't be cloned");

  RefPtr<ChannelMediaResource> resource = new ChannelMediaResource(
    aCallback, nullptr, mURI, GetContentType(), mChannelStatistics);
  if (resource) {
    // Initially the clone is treated as suspended by the cache, because
    // we don't have a channel. If the cache needs to read data from the clone
    // it will call CacheClientResume (or CacheClientSeek with aResume true)
    // which will recreate the channel. This way, if all of the media data
    // is already in the cache we don't create an unnecessary HTTP channel
    // and perform a useless HTTP transaction.
    resource->mSuspendAgent.Suspend();
    resource->mCacheStream.InitAsClone(&mCacheStream);
    resource->mChannelStatistics.Stop();
  }
  return resource.forget();
}

void ChannelMediaResource::CloseChannel()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  {
    MutexAutoLock lock(mLock);
    mChannelStatistics.Stop();
  }

  if (mListener) {
    mListener->Revoke();
    mListener = nullptr;
  }

  if (mChannel) {
    mSuspendAgent.NotifyChannelClosing();
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

nsresult ChannelMediaResource::ReadAt(int64_t aOffset,
                                      char* aBuffer,
                                      uint32_t aCount,
                                      uint32_t* aBytes)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  nsresult rv = mCacheStream.ReadAt(aOffset, aBuffer, aCount, aBytes);
  if (NS_SUCCEEDED(rv)) {
    DispatchBytesConsumed(*aBytes, aOffset);
  }
  return rv;
}

void
ChannelMediaResource::ThrottleReadahead(bool bThrottle)
{
  mCacheStream.ThrottleReadahead(bThrottle);
}

int64_t ChannelMediaResource::Tell()
{
  return mCacheStream.Tell();
}

nsresult ChannelMediaResource::GetCachedRanges(MediaByteRangeSet& aRanges)
{
  return mCacheStream.GetCachedRanges(aRanges);
}

void ChannelMediaResource::Suspend(bool aCloseImmediately)
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");

  MediaDecoderOwner* owner = mCallback->GetMediaOwner();
  if (!owner) {
    // Shutting down; do nothing.
    return;
  }
  dom::HTMLMediaElement* element = owner->GetMediaElement();
  if (!element) {
    // Shutting down; do nothing.
    return;
  }

  if (mChannel && aCloseImmediately && mCacheStream.IsTransportSeekable()) {
    // Kill off our channel right now, but don't tell anyone about it.
    mIgnoreClose = true;
    CloseChannel();
    element->DownloadSuspended();
  }

  if (mSuspendAgent.Suspend()) {
    if (mChannel) {
      {
        MutexAutoLock lock(mLock);
        mChannelStatistics.Stop();
      }
      element->DownloadSuspended();
    }
  }
}

void ChannelMediaResource::Resume()
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");

  MediaDecoderOwner* owner = mCallback->GetMediaOwner();
  if (!owner) {
    // Shutting down; do nothing.
    return;
  }
  dom::HTMLMediaElement* element = owner->GetMediaElement();
  if (!element) {
    // Shutting down; do nothing.
    return;
  }

  if (mSuspendAgent.Resume()) {
    if (mChannel) {
      // Just wake up our existing channel
      {
        MutexAutoLock lock(mLock);
        mChannelStatistics.Start();
      }
      // if an error occurs after Resume, assume it's because the server
      // timed out the connection and we should reopen it.
      mReopenOnError = true;
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
        element->DownloadResumed();
      } else {
        // The channel remains dead. Do not notify DownloadResumed() which
        // will leave the media element in NETWORK_LOADING state.
      }
    }
  }
}

nsresult
ChannelMediaResource::RecreateChannel()
{
  nsLoadFlags loadFlags =
    nsICachingChannel::LOAD_BYPASS_LOCAL_CACHE_IF_BUSY |
    nsIChannel::LOAD_CLASSIFY_URI |
    (mLoadInBackground ? nsIRequest::LOAD_BACKGROUND : 0);

  MediaDecoderOwner* owner = mCallback->GetMediaOwner();
  if (!owner) {
    // The decoder is being shut down, so don't bother opening a new channel
    return NS_OK;
  }
  dom::HTMLMediaElement* element = owner->GetMediaElement();
  if (!element) {
    // The decoder is being shut down, so don't bother opening a new channel
    return NS_OK;
  }
  nsCOMPtr<nsILoadGroup> loadGroup = element->GetDocumentLoadGroup();
  NS_ENSURE_TRUE(loadGroup, NS_ERROR_NULL_POINTER);

  nsSecurityFlags securityFlags = element->ShouldCheckAllowOrigin()
                                  ? nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS
                                  : nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS;

  MOZ_ASSERT(element->IsAnyOfHTMLElements(nsGkAtoms::audio, nsGkAtoms::video));
  nsContentPolicyType contentPolicyType = element->IsHTMLElement(nsGkAtoms::audio) ?
    nsIContentPolicy::TYPE_INTERNAL_AUDIO : nsIContentPolicy::TYPE_INTERNAL_VIDEO;

  nsresult rv = NS_NewChannel(getter_AddRefs(mChannel),
                              mURI,
                              element,
                              securityFlags,
                              contentPolicyType,
                              loadGroup,
                              nullptr,  // aCallbacks
                              loadFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  // We have cached the Content-Type, which should not change. Give a hint to
  // the channel to avoid a sniffing failure, which would be expected because we
  // are probably seeking in the middle of the bitstream, and sniffing relies
  // on the presence of a magic number at the beginning of the stream.
  mChannel->SetContentType(GetContentType().OriginalString());
  mSuspendAgent.NotifyChannelOpened(mChannel);

  // Tell the cache to reset the download status when the channel is reopened.
  mCacheStream.NotifyChannelRecreated();

  return rv;
}

void
ChannelMediaResource::DoNotifyDataReceived()
{
  mDataReceivedEvent.Revoke();
  mCallback->NotifyDataArrived();
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
    NewNonOwningRunnableMethod("ChannelMediaResource::DoNotifyDataReceived",
                               this, &ChannelMediaResource::DoNotifyDataReceived);

  nsCOMPtr<nsIRunnable> event = mDataReceivedEvent.get();

  SystemGroup::AbstractMainThreadFor(TaskCategory::Other)->Dispatch(event.forget());
}

void
ChannelMediaResource::CacheClientNotifyDataEnded(nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());
  mCallback->NotifyDataEnded(aStatus);
}

void
ChannelMediaResource::CacheClientNotifyPrincipalChanged()
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");

  mCallback->NotifyPrincipalChanged();
}

void
ChannelMediaResource::CacheClientNotifySuspendedStatusChanged()
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");
  mCallback->NotifySuspendedStatusChanged();
}

nsresult
ChannelMediaResource::CacheClientSeek(int64_t aOffset, bool aResume)
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");

  LOG("CacheClientSeek requested for aOffset [%" PRId64 "] for decoder [%p]",
      aOffset, mCallback.get());

  CloseChannel();

  mOffset = aOffset;

  // Don't report close of the channel because the channel is not closed for
  // download ended, but for internal changes in the read position.
  mIgnoreClose = true;

  if (aResume) {
    mSuspendAgent.Resume();
  }

  // Don't create a new channel if we are still suspended. The channel will
  // be recreated when we are resumed.
  if (mSuspendAgent.IsSuspended()) {
    return NS_OK;
  }

  nsresult rv = RecreateChannel();
  NS_ENSURE_SUCCESS(rv, rv);

  return OpenChannel(nullptr);
}

nsresult
ChannelMediaResource::CacheClientSuspend()
{
  Suspend(false);
  return NS_OK;
}

nsresult
ChannelMediaResource::CacheClientResume()
{
  Resume();
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
  return mSuspendAgent.IsSuspended();
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
  return mChannelStatistics.GetRate(aIsReliable);
}

int64_t
ChannelMediaResource::GetLength()
{
  return mCacheStream.GetLength();
}

// ChannelSuspendAgent

bool
ChannelSuspendAgent::Suspend()
{
  SuspendInternal();
  return (++mSuspendCount == 1);
}

void
ChannelSuspendAgent::SuspendInternal()
{
  if (mChannel) {
    bool isPending = false;
    nsresult rv = mChannel->IsPending(&isPending);
    if (NS_SUCCEEDED(rv) && isPending && !mIsChannelSuspended) {
      mChannel->Suspend();
      mIsChannelSuspended = true;
    }
  }
}

bool
ChannelSuspendAgent::Resume()
{
  MOZ_ASSERT(IsSuspended(), "Resume without suspend!");
  --mSuspendCount;

  if (mSuspendCount == 0) {
    if (mChannel && mIsChannelSuspended) {
      mChannel->Resume();
      mIsChannelSuspended = false;
    }
    return true;
  }
  return false;
}

void
ChannelSuspendAgent::UpdateSuspendedStatusIfNeeded()
{
  if (!mIsChannelSuspended && IsSuspended()) {
    SuspendInternal();
  }
}

void
ChannelSuspendAgent::NotifyChannelOpened(nsIChannel* aChannel)
{
  MOZ_ASSERT(aChannel);
  mChannel = aChannel;
}

void
ChannelSuspendAgent::NotifyChannelClosing()
{
  MOZ_ASSERT(mChannel);
  // Before close the channel, it need to be resumed to make sure its internal
  // state is correct. Besides, We need to suspend the channel after recreating.
  if (mIsChannelSuspended) {
    mChannel->Resume();
    mIsChannelSuspended = false;
  }
  mChannel = nullptr;
}

bool
ChannelSuspendAgent::IsSuspended()
{
  return (mSuspendCount > 0);
}

// FileMediaResource

class FileMediaResource : public BaseMediaResource
{
public:
  FileMediaResource(MediaResourceCallback* aCallback,
                    nsIChannel* aChannel,
                    nsIURI* aURI,
                    const MediaContainerType& aContainerType) :
    BaseMediaResource(aCallback, aChannel, aURI, aContainerType),
    mSize(-1),
    mLock("FileMediaResource.mLock"),
    mSizeInitialized(false)
  {
  }
  ~FileMediaResource()
  {
  }

  // Main thread
  nsresult Open(nsIStreamListener** aStreamListener) override;
  nsresult Close() override;
  void     Suspend(bool aCloseImmediately) override {}
  void     Resume() override {}
  already_AddRefed<nsIPrincipal> GetCurrentPrincipal() override;
  nsresult ReadFromCache(char* aBuffer, int64_t aOffset, uint32_t aCount) override;

  // These methods are called off the main thread.

  // Other thread
  void     SetReadMode(MediaCacheStream::ReadMode aMode) override {}
  void     SetPlaybackRate(uint32_t aBytesPerSecond) override {}
  nsresult ReadAt(int64_t aOffset, char* aBuffer,
                  uint32_t aCount, uint32_t* aBytes) override;
  // (Probably) file-based, caching recommended.
  bool ShouldCacheReads() override { return true; }
  int64_t  Tell() override;

  // Any thread
  void    Pin() override {}
  void    Unpin() override {}
  double  GetDownloadRate(bool* aIsReliable) override
  {
    // The data's all already here
    *aIsReliable = true;
    return 100*1024*1024; // arbitray, use 100MB/s
  }
  int64_t GetLength() override {
    MutexAutoLock lock(mLock);

    EnsureSizeInitialized();
    return mSizeInitialized ? mSize : 0;
  }
  int64_t GetNextCachedData(int64_t aOffset) override
  {
    MutexAutoLock lock(mLock);

    EnsureSizeInitialized();
    return (aOffset < mSize) ? aOffset : -1;
  }
  int64_t GetCachedDataEnd(int64_t aOffset) override {
    MutexAutoLock lock(mLock);

    EnsureSizeInitialized();
    return std::max(aOffset, mSize);
  }
  bool    IsDataCachedToEndOfResource(int64_t aOffset) override { return true; }
  bool    IsSuspendedByCache() override { return true; }
  bool    IsSuspended() override { return true; }
  bool    IsTransportSeekable() override { return true; }

  nsresult GetCachedRanges(MediaByteRangeSet& aRanges) override;

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    // Might be useful to track in the future:
    // - mInput
    return BaseMediaResource::SizeOfExcludingThis(aMallocSizeOf);
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

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
  already_AddRefed<MediaByteBuffer> UnsafeMediaReadAt(
                        int64_t aOffset, uint32_t aCount);

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
    mCallback->NotifyDataEnded(NS_OK);
  }
}

nsresult FileMediaResource::GetCachedRanges(MediaByteRangeSet& aRanges)
{
  MutexAutoLock lock(mLock);

  EnsureSizeInitialized();
  if (mSize == -1) {
    return NS_ERROR_FAILURE;
  }
  aRanges += MediaByteRange(0, mSize);
  return NS_OK;
}

nsresult FileMediaResource::Open(nsIStreamListener** aStreamListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  MOZ_ASSERT(aStreamListener);

  *aStreamListener = nullptr;
  nsresult rv = NS_OK;

  // The channel is already open. We need a synchronous stream that
  // implements nsISeekableStream, so we have to find the underlying
  // file and reopen it
  nsCOMPtr<nsIFileChannel> fc(do_QueryInterface(mChannel));
  if (fc) {
    nsCOMPtr<nsIFile> file;
    rv = fc->GetFile(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_NewLocalFileInputStream(
      getter_AddRefs(mInput), file, -1, -1, nsIFileInputStream::SHARE_DELETE);
  } else if (IsBlobURI(mURI)) {
    rv = NS_GetStreamForBlobURI(mURI, getter_AddRefs(mInput));
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
  secMan->GetChannelResultPrincipal(mChannel, getter_AddRefs(principal));
  return principal.forget();
}

nsresult FileMediaResource::ReadFromCache(char* aBuffer, int64_t aOffset, uint32_t aCount)
{
  MutexAutoLock lock(mLock);

  EnsureSizeInitialized();
  if (!aCount) {
    return NS_OK;
  }
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
    if (!x) {
      res = NS_ERROR_FAILURE;
    }
  } while (bytesRead != aCount && res == NS_OK);

  // Reset read head to original position so we don't disturb any other
  // reading thread.
  nsresult seekres = mSeekable->Seek(nsISeekableStream::NS_SEEK_SET, offset);

  // If a read failed in the loop above, we want to return its failure code.
  NS_ENSURE_SUCCESS(res,res);

  // Else we succeed if the reset-seek succeeds.
  return seekres;
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

  nsresult rv;
  {
    MutexAutoLock lock(mLock);
    rv = UnsafeSeek(nsISeekableStream::NS_SEEK_SET, aOffset);
    if (NS_FAILED(rv)) return rv;
    rv = UnsafeRead(aBuffer, aCount, aBytes);
  }
  if (NS_SUCCEEDED(rv)) {
    DispatchBytesConsumed(*aBytes, aOffset);
  }
  return rv;
}

already_AddRefed<MediaByteBuffer>
FileMediaResource::UnsafeMediaReadAt(int64_t aOffset, uint32_t aCount)
{
  RefPtr<MediaByteBuffer> bytes = new MediaByteBuffer();
  bool ok = bytes->SetLength(aCount, fallible);
  NS_ENSURE_TRUE(ok, nullptr);
  nsresult rv = UnsafeSeek(nsISeekableStream::NS_SEEK_SET, aOffset);
  NS_ENSURE_SUCCESS(rv, nullptr);
  char* curr = reinterpret_cast<char*>(bytes->Elements());
  const char* start = curr;
  while (aCount > 0) {
    uint32_t bytesRead;
    rv = UnsafeRead(curr, aCount, &bytesRead);
    NS_ENSURE_SUCCESS(rv, nullptr);
    if (!bytesRead) {
      break;
    }
    aCount -= bytesRead;
    curr += bytesRead;
  }
  bytes->SetLength(curr - start);
  return bytes.forget();
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
  MutexAutoLock lock(mLock);
  EnsureSizeInitialized();

  int64_t offset = 0;
  // Return mSize as offset (end of stream) in case of error
  if (!mSeekable || NS_FAILED(mSeekable->Tell(&offset)))
    return mSize;
  return offset;
}

already_AddRefed<MediaResource>
MediaResource::Create(MediaResourceCallback* aCallback,
                      nsIChannel* aChannel, bool aIsPrivateBrowsing)
{
  NS_ASSERTION(NS_IsMainThread(),
               "MediaResource::Open called on non-main thread");

  // If the channel was redirected, we want the post-redirect URI;
  // but if the URI scheme was expanded, say from chrome: to jar:file:,
  // we want the original URI.
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsAutoCString contentTypeString;
  aChannel->GetContentType(contentTypeString);
  Maybe<MediaContainerType> containerType = MakeMediaContainerType(contentTypeString);
  if (!containerType) {
    return nullptr;
  }

  RefPtr<MediaResource> resource;

#ifdef MOZ_ANDROID_HLS_SUPPORT
  if (DecoderTraits::IsHttpLiveStreamingType(containerType.value())) {
    resource = new HLSResource(aCallback, aChannel, uri, *containerType);
    return resource.forget();
  }
#endif

  // Let's try to create a FileMediaResource in case the channel is a nsIFile
  nsCOMPtr<nsIFileChannel> fc = do_QueryInterface(aChannel);
  if (fc) {
    resource = new FileMediaResource(aCallback, aChannel, uri, *containerType);
  }

  // If the URL is blobURL with a seekable inputStream, we can still use a
  // FileMediaResource. This basically means that the blobURL and its Blob have
  // been created in the current process.

  if (!resource) {
    nsCOMPtr<nsIInputStream> stream;
    nsCOMPtr<nsISeekableStream> seekableStream;
    if (IsBlobURI(uri) &&
        NS_SUCCEEDED(NS_GetStreamForBlobURI(uri, getter_AddRefs(stream))) &&
        (seekableStream = do_QueryInterface(stream))) {
      resource =
        new FileMediaResource(aCallback, aChannel, uri, *containerType);
    }
  }

  if (!resource) {
    resource =
      new ChannelMediaResource(aCallback, aChannel, uri, *containerType,
                               aIsPrivateBrowsing);
  }

  return resource.forget();
}

void BaseMediaResource::SetLoadInBackground(bool aLoadInBackground) {
  if (aLoadInBackground == mLoadInBackground) {
    return;
  }
  mLoadInBackground = aLoadInBackground;
  if (!mChannel) {
    // No channel, resource is probably already loaded.
    return;
  }

  MediaDecoderOwner* owner = mCallback->GetMediaOwner();
  if (!owner) {
    NS_WARNING("Null owner in MediaResource::SetLoadInBackground()");
    return;
  }
  dom::HTMLMediaElement* element = owner->GetMediaElement();
  if (!element) {
    NS_WARNING("Null element in MediaResource::SetLoadInBackground()");
    return;
  }

  bool isPending = false;
  if (NS_SUCCEEDED(mChannel->IsPending(&isPending)) &&
      isPending) {
    nsLoadFlags loadFlags;
    DebugOnly<nsresult> rv = mChannel->GetLoadFlags(&loadFlags);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetLoadFlags() failed!");

    if (aLoadInBackground) {
      loadFlags |= nsIRequest::LOAD_BACKGROUND;
    } else {
      loadFlags &= ~nsIRequest::LOAD_BACKGROUND;
    }
    ModifyLoadFlags(loadFlags);
  }
}

void BaseMediaResource::ModifyLoadFlags(nsLoadFlags aFlags)
{
  nsCOMPtr<nsILoadGroup> loadGroup;
  nsresult rv = mChannel->GetLoadGroup(getter_AddRefs(loadGroup));
  MOZ_ASSERT(NS_SUCCEEDED(rv), "GetLoadGroup() failed!");

  nsresult status;
  mChannel->GetStatus(&status);

  bool inLoadGroup = false;
  if (loadGroup) {
    rv = loadGroup->RemoveRequest(mChannel, nullptr, status);
    if (NS_SUCCEEDED(rv)) {
      inLoadGroup = true;
    }
  }

  rv = mChannel->SetLoadFlags(aFlags);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "SetLoadFlags() failed!");

  if (inLoadGroup) {
    rv = loadGroup->AddRequest(mChannel, nullptr);
    MOZ_ASSERT(NS_SUCCEEDED(rv), "AddRequest() failed!");
  }
}

void BaseMediaResource::DispatchBytesConsumed(int64_t aNumBytes, int64_t aOffset)
{
  if (aNumBytes <= 0) {
    return;
  }
  mCallback->NotifyBytesConsumed(aNumBytes, aOffset);
}

nsresult
MediaResourceIndex::Read(char* aBuffer, uint32_t aCount, uint32_t* aBytes)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  // We purposefuly don't check that we may attempt to read past
  // mResource->GetLength() as the resource's length may change over time.

  nsresult rv = ReadAt(mOffset, aBuffer, aCount, aBytes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  mOffset += *aBytes;
  return NS_OK;
}

static nsCString
ResultName(nsresult aResult)
{
  nsCString name;
  GetErrorName(aResult, name);
  return name;
}

nsresult
MediaResourceIndex::ReadAt(int64_t aOffset,
                           char* aBuffer,
                           uint32_t aCount,
                           uint32_t* aBytes)
{
  if (mCacheBlockSize == 0) {
    return UncachedReadAt(aOffset, aBuffer, aCount, aBytes);
  }

  *aBytes = 0;

  if (aCount == 0) {
    return NS_OK;
  }

  const int64_t endOffset = aOffset + aCount;
  const int64_t lastBlockOffset = CacheOffsetContaining(endOffset - 1);

  if (mCachedBytes != 0 && mCachedOffset + mCachedBytes >= aOffset &&
      mCachedOffset < endOffset) {
    // There is data in the cache that is not completely before aOffset and not
    // completely after endOffset, so it could be usable (with potential top-up).
    if (aOffset < mCachedOffset) {
      // We need to read before the cached data.
      const uint32_t toRead = uint32_t(mCachedOffset - aOffset);
      MOZ_ASSERT(toRead > 0);
      MOZ_ASSERT(toRead < aCount);
      uint32_t read = 0;
      nsresult rv = UncachedReadAt(aOffset, aBuffer, toRead, &read);
      if (NS_FAILED(rv)) {
        ILOG("ReadAt(%" PRIu32 "@%" PRId64
             ") uncached read before cache -> %s, %" PRIu32,
             aCount,
             aOffset,
             ResultName(rv).get(),
             *aBytes);
        return rv;
      }
      *aBytes = read;
      if (read < toRead) {
        // Could not read everything we wanted, we're done.
        ILOG("ReadAt(%" PRIu32 "@%" PRId64
             ") uncached read before cache, incomplete -> OK, %" PRIu32,
             aCount,
             aOffset,
             *aBytes);
        return NS_OK;
      }
      ILOG("ReadAt(%" PRIu32 "@%" PRId64
           ") uncached read before cache: %" PRIu32 ", remaining: %" PRIu32
           "@%" PRId64 "...",
           aCount,
           aOffset,
           read,
           aCount - read,
           aOffset + read);
      aOffset += read;
      aBuffer += read;
      aCount -= read;
      // We should have reached the cache.
      MOZ_ASSERT(aOffset == mCachedOffset);
    }
    MOZ_ASSERT(aOffset >= mCachedOffset);

    // We've reached our cache.
    const uint32_t toCopy =
      std::min(aCount, uint32_t(mCachedOffset + mCachedBytes - aOffset));
    // Note that we could in fact be just after the last byte of the cache, in
    // which case we can't actually read from it! (But we will top-up next.)
    if (toCopy != 0) {
      memcpy(aBuffer, &mCachedBlock[IndexInCache(aOffset)], toCopy);
      *aBytes += toCopy;
      aCount -= toCopy;
      if (aCount == 0) {
        // All done!
        ILOG("ReadAt(%" PRIu32 "@%" PRId64 ") copied everything (%" PRIu32
             ") from cache(%" PRIu32 "@%" PRId64 ") :-D -> OK, %" PRIu32,
             aCount,
             aOffset,
             toCopy,
             mCachedBytes,
             mCachedOffset,
             *aBytes);
        return NS_OK;
      }
      aOffset += toCopy;
      aBuffer += toCopy;
      ILOG("ReadAt(%" PRIu32 "@%" PRId64 ") copied %" PRIu32
           " from cache(%" PRIu32 "@%" PRId64 ") :-), remaining: %" PRIu32
           "@%" PRId64 "...",
           aCount + toCopy,
           aOffset - toCopy,
           toCopy,
           mCachedBytes,
           mCachedOffset,
           aCount,
           aOffset);
    }

    if (aOffset - 1 >= lastBlockOffset) {
      // We were already reading cached data from the last block, we need more
      // from it -> try to top-up, read what we can, and we'll be done.
      MOZ_ASSERT(aOffset == mCachedOffset + mCachedBytes);
      MOZ_ASSERT(endOffset <= lastBlockOffset + mCacheBlockSize);
      return CacheOrReadAt(aOffset, aBuffer, aCount, aBytes);
    }

    // We were not in the last block (but we may just have crossed the line now)
    MOZ_ASSERT(aOffset <= lastBlockOffset);
    // Continue below...
  } else if (aOffset >= lastBlockOffset) {
    // There was nothing we could get from the cache.
    // But we're already in the last block -> Cache or read what we can.
    // Make sure to invalidate the cache first.
    mCachedBytes = 0;
    return CacheOrReadAt(aOffset, aBuffer, aCount, aBytes);
  }

  // If we're here, either there was nothing usable in the cache, or we've just
  // read what was in the cache but there's still more to read.

  if (aOffset < lastBlockOffset) {
    // We need to read before the last block.
    // Start with an uncached read up to the last block.
    const uint32_t toRead = uint32_t(lastBlockOffset - aOffset);
    MOZ_ASSERT(toRead > 0);
    MOZ_ASSERT(toRead < aCount);
    uint32_t read = 0;
    nsresult rv = UncachedReadAt(aOffset, aBuffer, toRead, &read);
    if (NS_FAILED(rv)) {
      ILOG("ReadAt(%" PRIu32 "@%" PRId64
           ") uncached read before last block failed -> %s, %" PRIu32,
           aCount,
           aOffset,
           ResultName(rv).get(),
           *aBytes);
      return rv;
    }
    if (read == 0) {
      ILOG("ReadAt(%" PRIu32 "@%" PRId64
           ") uncached read 0 before last block -> OK, %" PRIu32,
           aCount,
           aOffset,
           *aBytes);
      return NS_OK;
    }
    *aBytes += read;
    if (read < toRead) {
      // Could not read everything we wanted, we're done.
      ILOG("ReadAt(%" PRIu32 "@%" PRId64
           ") uncached read before last block, incomplete -> OK, %" PRIu32,
           aCount,
           aOffset,
           *aBytes);
      return NS_OK;
    }
    ILOG("ReadAt(%" PRIu32 "@%" PRId64 ") read %" PRIu32
         " before last block, remaining: %" PRIu32 "@%" PRId64 "...",
         aCount,
         aOffset,
         read,
         aCount - read,
         aOffset + read);
    aOffset += read;
    aBuffer += read;
    aCount -= read;
  }

  // We should just have reached the start of the last block.
  MOZ_ASSERT(aOffset == lastBlockOffset);
  MOZ_ASSERT(aCount <= mCacheBlockSize);
  // Make sure to invalidate the cache first.
  mCachedBytes = 0;
  return CacheOrReadAt(aOffset, aBuffer, aCount, aBytes);
}

nsresult
MediaResourceIndex::CacheOrReadAt(int64_t aOffset,
                                  char* aBuffer,
                                  uint32_t aCount,
                                  uint32_t* aBytes)
{
  // We should be here because there is more data to read.
  MOZ_ASSERT(aCount > 0);
  // We should be in the last block, so we shouldn't try to read past it.
  MOZ_ASSERT(IndexInCache(aOffset) + aCount <= mCacheBlockSize);

  const int64_t length = GetLength();
  // If length is unknown (-1), look at resource-cached data.
  // If length is known and equal or greater than requested, also look at
  // resource-cached data.
  // Otherwise, if length is known but same, or less than(!?), requested, don't
  // attempt to access resource-cached data, as we're not expecting it to ever
  // be greater than the length.
  if (length < 0 || length >= aOffset + aCount) {
    // Is there cached data covering at least the requested range?
    const int64_t cachedDataEnd = mResource->GetCachedDataEnd(aOffset);
    if (cachedDataEnd >= aOffset + aCount) {
      // Try to read as much resource-cached data as can fill our local cache.
      // Assume we can read as much as is cached without blocking.
      const uint32_t cacheIndex = IndexInCache(aOffset);
      const uint32_t toRead =
        uint32_t(std::min(cachedDataEnd - aOffset,
                          int64_t(mCacheBlockSize - cacheIndex)));
      MOZ_ASSERT(toRead >= aCount);
      uint32_t read = 0;
      // We would like `toRead` if possible, but ok with at least `aCount`.
      nsresult rv = UncachedRangedReadAt(
        aOffset, &mCachedBlock[cacheIndex], aCount, toRead - aCount, &read);
      if (NS_SUCCEEDED(rv)) {
        if (read == 0) {
          ILOG("ReadAt(%" PRIu32 "@%" PRId64 ") - UncachedRangedReadAt(%" PRIu32
               "..%" PRIu32 "@%" PRId64
               ") to top-up succeeded but read nothing -> OK anyway",
               aCount,
               aOffset,
               aCount,
               toRead,
               aOffset);
          // Couldn't actually read anything, but didn't error out, so count
          // that as success.
          return NS_OK;
        }
        if (mCachedOffset + mCachedBytes == aOffset) {
          // We were topping-up the cache, just update its size.
          ILOG("ReadAt(%" PRIu32 "@%" PRId64 ") - UncachedRangedReadAt(%" PRIu32
               "..%" PRIu32 "@%" PRId64 ") to top-up succeeded to read %" PRIu32
               "...",
               aCount,
               aOffset,
               aCount,
               toRead,
               aOffset,
               read);
          mCachedBytes += read;
        } else {
          // We were filling the cache from scratch, save new cache information.
          ILOG("ReadAt(%" PRIu32 "@%" PRId64 ") - UncachedRangedReadAt(%" PRIu32
               "..%" PRIu32 "@%" PRId64
               ") to fill cache succeeded to read %" PRIu32 "...",
               aCount,
               aOffset,
               aCount,
               toRead,
               aOffset,
               read);
          mCachedOffset = aOffset;
          mCachedBytes = read;
        }
        // Copy relevant part into output.
        uint32_t toCopy = std::min(aCount, read);
        memcpy(aBuffer, &mCachedBlock[cacheIndex], toCopy);
        *aBytes += toCopy;
        ILOG("ReadAt(%" PRIu32 "@%" PRId64 ") - copied %" PRIu32 "@%" PRId64
             " -> OK, %" PRIu32,
             aCount,
             aOffset,
             toCopy,
             aOffset,
             *aBytes);
        // We may not have read all that was requested, but we got everything
        // we could get, so we're done.
        return NS_OK;
      }
      ILOG("ReadAt(%" PRIu32 "@%" PRId64 ") - UncachedRangedReadAt(%" PRIu32
           "..%" PRIu32 "@%" PRId64
           ") failed: %s, will fallback to blocking read...",
           aCount,
           aOffset,
           aCount,
           toRead,
           aOffset,
           ResultName(rv).get());
      // Failure during reading. Note that this may be due to the cache
      // changing between `GetCachedDataEnd` and `ReadAt`, so it's not
      // totally unexpected, just hopefully rare; but we do need to handle it.

      // Invalidate part of cache that may have been partially overridden.
      if (mCachedOffset + mCachedBytes == aOffset) {
        // We were topping-up the cache, just keep the old untouched data.
        // (i.e., nothing to do here.)
      } else {
        // We were filling the cache from scratch, invalidate cache.
        mCachedBytes = 0;
      }
    } else {
      ILOG("ReadAt(%" PRIu32 "@%" PRId64
           ") - no cached data, will fallback to blocking read...",
           aCount,
           aOffset);
    }
  } else {
    ILOG("ReadAt(%" PRIu32 "@%" PRId64 ") - length is %" PRId64
         " (%s), will fallback to blocking read as the caller requested...",
         aCount,
         aOffset,
         length,
         length < 0 ? "unknown" : "too short!");
  }
  uint32_t read = 0;
  nsresult rv = UncachedReadAt(aOffset, aBuffer, aCount, &read);
  if (NS_SUCCEEDED(rv)) {
    *aBytes += read;
    ILOG("ReadAt(%" PRIu32 "@%" PRId64 ") - fallback uncached read got %" PRIu32
         " bytes -> %s, %" PRIu32,
         aCount,
         aOffset,
         read,
         ResultName(rv).get(),
         *aBytes);
  } else {
    ILOG("ReadAt(%" PRIu32 "@%" PRId64
         ") - fallback uncached read failed -> %s, %" PRIu32,
         aCount,
         aOffset,
         ResultName(rv).get(),
         *aBytes);
  }
  return rv;
}

nsresult
MediaResourceIndex::UncachedReadAt(int64_t aOffset,
                                   char* aBuffer,
                                   uint32_t aCount,
                                   uint32_t* aBytes) const
{
  *aBytes = 0;
  if (aCount != 0) {
    for (;;) {
      uint32_t bytesRead = 0;
      nsresult rv = mResource->ReadAt(aOffset, aBuffer, aCount, &bytesRead);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (bytesRead == 0) {
        break;
      }
      *aBytes += bytesRead;
      aCount -= bytesRead;
      if (aCount == 0) {
        break;
      }
      aOffset += bytesRead;
      aBuffer += bytesRead;
    }
  }
  return NS_OK;
}

nsresult
MediaResourceIndex::UncachedRangedReadAt(int64_t aOffset,
                                         char* aBuffer,
                                         uint32_t aRequestedCount,
                                         uint32_t aExtraCount,
                                         uint32_t* aBytes) const
{
  *aBytes = 0;
  uint32_t count = aRequestedCount + aExtraCount;
  if (count != 0) {
    for (;;) {
      uint32_t bytesRead = 0;
      nsresult rv = mResource->ReadAt(aOffset, aBuffer, count, &bytesRead);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (bytesRead == 0) {
        break;
      }
      *aBytes += bytesRead;
      count -= bytesRead;
      if (count <= aExtraCount) {
        // We have read at least aRequestedCount, don't loop anymore.
        break;
      }
      aOffset += bytesRead;
      aBuffer += bytesRead;
    }
  }
  return NS_OK;
}

nsresult
MediaResourceIndex::Seek(int32_t aWhence, int64_t aOffset)
{
  switch (aWhence) {
    case SEEK_SET:
      break;
    case SEEK_CUR:
      aOffset += mOffset;
      break;
    case SEEK_END:
    {
      int64_t length = mResource->GetLength();
      if (length == -1 || length - aOffset < 0) {
        return NS_ERROR_FAILURE;
      }
      aOffset = mResource->GetLength() - aOffset;
    }
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  mOffset = aOffset;

  return NS_OK;
}

} // namespace mozilla

// avoid redefined macro in unified build
#undef LOG
#undef ILOG
