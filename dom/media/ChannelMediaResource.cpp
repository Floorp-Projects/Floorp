/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChannelMediaResource.h"

#include "mozilla/dom/HTMLMediaElement.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsICachingChannel.h"
#include "nsIClassOfService.h"
#include "nsIInputStream.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsNetUtil.h"

static const uint32_t HTTP_PARTIAL_RESPONSE_CODE = 206;
static const uint32_t HTTP_OK_CODE = 200;
static const uint32_t HTTP_REQUESTED_RANGE_NOT_SATISFIABLE_CODE = 416;

mozilla::LazyLogModule gMediaResourceLog("MediaResource");
// Debug logging macro with object pointer and class name.
#define LOG(msg, ...)                                                          \
  DDMOZ_LOG(gMediaResourceLog, mozilla::LogLevel::Debug, msg, ##__VA_ARGS__)

namespace mozilla {

ChannelMediaResource::ChannelMediaResource(MediaResourceCallback* aCallback,
                                           nsIChannel* aChannel,
                                           nsIURI* aURI,
                                           bool aIsPrivateBrowsing)
  : BaseMediaResource(aCallback, aChannel, aURI)
  , mCacheStream(this, aIsPrivateBrowsing)
  , mSuspendAgent(mCacheStream)
{
}

ChannelMediaResource::~ChannelMediaResource()
{
  MOZ_ASSERT(mClosed);
  MOZ_ASSERT(!mChannel);
  MOZ_ASSERT(!mListener);
  if (mSharedInfo) {
    mSharedInfo->mResources.RemoveElement(this);
  }
}

// ChannelMediaResource::Listener just observes the channel and
// forwards notifications to the ChannelMediaResource. We use multiple
// listener objects so that when we open a new stream for a seek we can
// disconnect the old listener from the ChannelMediaResource and hook up
// a new listener, so notifications from the old channel are discarded
// and don't confuse us.
NS_IMPL_ISUPPORTS(ChannelMediaResource::Listener,
                  nsIRequestObserver,
                  nsIStreamListener,
                  nsIChannelEventSink,
                  nsIInterfaceRequestor,
                  nsIThreadRetargetableStreamListener)

nsresult
ChannelMediaResource::Listener::OnStartRequest(nsIRequest* aRequest,
                                               nsISupports* aContext)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mResource)
    return NS_OK;
  return mResource->OnStartRequest(aRequest, mOffset);
}

nsresult
ChannelMediaResource::Listener::OnStopRequest(nsIRequest* aRequest,
                                              nsISupports* aContext,
                                              nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());
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
  // This might happen off the main thread.
  RefPtr<ChannelMediaResource> res;
  {
    MutexAutoLock lock(mMutex);
    res = mResource;
  }
  // Note Rekove() might happen at the same time to reset mResource. We check
  // the load ID to determine if the data is from an old channel.
  return res ? res->OnDataAvailable(mLoadID, aStream, aCount) : NS_OK;
}

nsresult
ChannelMediaResource::Listener::AsyncOnChannelRedirect(
  nsIChannel* aOld,
  nsIChannel* aNew,
  uint32_t aFlags,
  nsIAsyncVerifyRedirectCallback* cb)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = NS_OK;
  if (mResource) {
    rv = mResource->OnChannelRedirect(aOld, aNew, aFlags, mOffset);
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  cb->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

nsresult
ChannelMediaResource::Listener::CheckListenerChain()
{
  return NS_OK;
}

nsresult
ChannelMediaResource::Listener::GetInterface(const nsIID& aIID, void** aResult)
{
  return QueryInterface(aIID, aResult);
}

void
ChannelMediaResource::Listener::Revoke()
{
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);
  mResource = nullptr;
}

static bool
IsPayloadCompressed(nsIHttpChannel* aChannel)
{
  nsAutoCString encoding;
  Unused << aChannel->GetResponseHeader(NS_LITERAL_CSTRING("Content-Encoding"), encoding);
  return encoding.Length() > 0;
}

nsresult
ChannelMediaResource::OnStartRequest(nsIRequest* aRequest,
                                     int64_t aRequestOffset)
{
  NS_ASSERTION(mChannel.get() == aRequest, "Wrong channel!");
  MOZ_DIAGNOSTIC_ASSERT(!mClosed);

  MediaDecoderOwner* owner = mCallback->GetMediaOwner();
  MOZ_DIAGNOSTIC_ASSERT(owner);
  dom::HTMLMediaElement* element = owner->GetMediaElement();
  MOZ_DIAGNOSTIC_ASSERT(element);

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
      mCallback->NotifyNetworkError(MediaResult(status, "CORS not allowed"));
      return NS_ERROR_DOM_BAD_URI;
    }
  }

  nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(aRequest);
  bool seekable = false;
  int64_t length = -1;
  int64_t startOffset = aRequestOffset;

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
        // work here. Note we need to pass the load ID first so the following
        // NotifyDataEnded() can pass the ID check.
        mCacheStream.NotifyLoadID(mLoadID);
        mCacheStream.NotifyDataEnded(mLoadID, status);
      } else {
        mCallback->NotifyNetworkError(
          MediaResult(NS_ERROR_FAILURE, "HTTP error"));
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

    int64_t contentLength = -1;
    const bool isCompressed = IsPayloadCompressed(hc);
    if (!isCompressed) {
      hc->GetContentLength(&contentLength);
    }

    // Check response code for byte-range requests (seeking, chunk requests).
    // We don't expect to get a 206 response for a compressed stream, but
    // double check just to be sure.
    if (!isCompressed && responseStatus == HTTP_PARTIAL_RESPONSE_CODE) {
      // Parse Content-Range header.
      int64_t rangeStart = 0;
      int64_t rangeEnd = 0;
      int64_t rangeTotal = 0;
      rv = ParseContentRangeHeader(hc, rangeStart, rangeEnd, rangeTotal);

      // We received 'Content-Range', so the server accepts range requests.
      bool gotRangeHeader = NS_SUCCEEDED(rv);

      if (gotRangeHeader) {
        startOffset = rangeStart;
        // We received 'Content-Range', so the server accepts range requests.
        // Notify media cache about the length and start offset of data received.
        // Note: If aRangeTotal == -1, then the total bytes is unknown at this stage.
        //       For now, tell the decoder that the stream is infinite.
        if (rangeTotal != -1) {
          contentLength = std::max(contentLength, rangeTotal);
        }
      }
      acceptsRanges = gotRangeHeader;
    } else if (responseStatus == HTTP_OK_CODE) {
      // HTTP_OK_CODE means data will be sent from the start of the stream.
      startOffset = 0;

      if (aRequestOffset > 0) {
        // If HTTP_OK_CODE is responded for a non-zero range request, we have
        // to assume seeking doesn't work.
        acceptsRanges = false;
      }
    }
    if (aRequestOffset == 0 && contentLength >= 0 &&
        (responseStatus == HTTP_OK_CODE ||
         responseStatus == HTTP_PARTIAL_RESPONSE_CODE)) {
      length = contentLength;
    }
    // XXX we probably should examine the Content-Range header in case
    // the server gave us a range which is not quite what we asked for

    // If we get an HTTP_OK_CODE response to our byte range request,
    // and the server isn't sending Accept-Ranges:bytes then we don't
    // support seeking. We also can't seek in compressed streams.
    seekable = !isCompressed && acceptsRanges;
  } else {
    // Not an HTTP channel. Assume data will be sent from position zero.
    startOffset = 0;
  }

  // Update principals before OnDataAvailable() putting the data in the cache.
  // This is important, we want to make sure all principals are updated before
  // any consumer can see the new data.
  UpdatePrincipal();

  mCacheStream.NotifyDataStarted(mLoadID, startOffset, seekable, length);
  mIsTransportSeekable = seekable;

  mSuspendAgent.Delegate(mChannel);

  // Fires an initial progress event.
  owner->DownloadProgressed();

  nsCOMPtr<nsIThreadRetargetableRequest> retarget;
  if (Preferences::GetBool("media.omt_data_delivery.enabled", false) &&
      (retarget = do_QueryInterface(aRequest))) {
    // Note this will not always succeed. We need to handle the case where
    // all resources sharing the same cache might run their data callbacks
    // on different threads.
    retarget->RetargetDeliveryTo(mCacheStream.OwnerThread());
  }

  return NS_OK;
}

bool
ChannelMediaResource::IsTransportSeekable()
{
  MOZ_ASSERT(NS_IsMainThread());
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
  MOZ_DIAGNOSTIC_ASSERT(!mClosed);

  // Move this request back into the foreground.  This is necessary for
  // requests owned by video documents to ensure the load group fires
  // OnStopRequest when restoring from session history.
  nsLoadFlags loadFlags;
  DebugOnly<nsresult> rv = mChannel->GetLoadFlags(&loadFlags);
  NS_ASSERTION(NS_SUCCEEDED(rv), "GetLoadFlags() failed!");

  if (loadFlags & nsIRequest::LOAD_BACKGROUND) {
    ModifyLoadFlags(loadFlags & ~nsIRequest::LOAD_BACKGROUND);
  }

  mCacheStream.NotifyDataEnded(mLoadID, aStatus, true /*aReopenOnError*/);
  return NS_OK;
}

nsresult
ChannelMediaResource::OnChannelRedirect(nsIChannel* aOld,
                                        nsIChannel* aNew,
                                        uint32_t aFlags,
                                        int64_t aOffset)
{
  // OnChannelRedirect() is followed by OnStartRequest() where we will
  // call mSuspendAgent.Delegate().
  mChannel = aNew;
  return SetupChannelHeaders(aOffset);
}

nsresult
ChannelMediaResource::CopySegmentToCache(nsIInputStream* aInStream,
                                         void* aClosure,
                                         const char* aFromSegment,
                                         uint32_t aToOffset,
                                         uint32_t aCount,
                                         uint32_t* aWriteCount)
{
  *aWriteCount = aCount;
  Closure* closure = static_cast<Closure*>(aClosure);
  MediaCacheStream* cacheStream = &closure->mResource->mCacheStream;
  if (cacheStream->OwnerThread()->IsOnCurrentThread()) {
    cacheStream->NotifyDataReceived(
      closure->mLoadID, aCount, reinterpret_cast<const uint8_t*>(aFromSegment));
    return NS_OK;
  }

  RefPtr<ChannelMediaResource> self = closure->mResource;
  uint32_t loadID = closure->mLoadID;
  UniquePtr<uint8_t[]> data = MakeUnique<uint8_t[]>(aCount);
  memcpy(data.get(), aFromSegment, aCount);
  cacheStream->OwnerThread()->Dispatch(NS_NewRunnableFunction(
    "MediaCacheStream::NotifyDataReceived",
    [ self, loadID, data = Move(data), aCount ]() {
      self->mCacheStream.NotifyDataReceived(loadID, aCount, data.get());
    }));

  return NS_OK;
}

nsresult
ChannelMediaResource::OnDataAvailable(uint32_t aLoadID,
                                      nsIInputStream* aStream,
                                      uint32_t aCount)
{
  // This might happen off the main thread.
  Closure closure{ aLoadID, this };
  uint32_t count = aCount;
  while (count > 0) {
    uint32_t read;
    nsresult rv =
      aStream->ReadSegments(CopySegmentToCache, &closure, count, &read);
    if (NS_FAILED(rv))
      return rv;
    NS_ASSERTION(read > 0, "Read 0 bytes while data was available?");
    count -= read;
  }

  return NS_OK;
}

nsresult
ChannelMediaResource::Open(nsIStreamListener** aStreamListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  MOZ_ASSERT(aStreamListener);
  MOZ_ASSERT(mChannel);

  int64_t cl = -1;
  nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(mChannel);
  if (hc && !IsPayloadCompressed(hc)) {
    if (NS_FAILED(hc->GetContentLength(&cl))) {
      cl = -1;
    }
  }

  nsresult rv = mCacheStream.Init(cl);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mSharedInfo = new SharedInfo;
  mSharedInfo->mResources.AppendElement(this);

  mIsLiveStream = cl < 0;
  mListener = new Listener(this, 0, ++mLoadID);
  *aStreamListener = mListener;
  NS_ADDREF(*aStreamListener);
  return NS_OK;
}

nsresult
ChannelMediaResource::OpenChannel(int64_t aOffset)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!mClosed);
  MOZ_ASSERT(mChannel);
  MOZ_ASSERT(!mListener, "Listener should have been removed by now");

  mListener = new Listener(this, aOffset, ++mLoadID);
  nsresult rv = mChannel->SetNotificationCallbacks(mListener.get());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetupChannelHeaders(aOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mChannel->AsyncOpen2(mListener);
  NS_ENSURE_SUCCESS(rv, rv);

  // Tell the media element that we are fetching data from a channel.
  MediaDecoderOwner* owner = mCallback->GetMediaOwner();
  MOZ_DIAGNOSTIC_ASSERT(owner);
  dom::HTMLMediaElement* element = owner->GetMediaElement();
  MOZ_DIAGNOSTIC_ASSERT(element);
  element->DownloadResumed();

  return NS_OK;
}

nsresult
ChannelMediaResource::SetupChannelHeaders(int64_t aOffset)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!mClosed);

  // Always use a byte range request even if we're reading from the start
  // of the resource.
  // This enables us to detect if the stream supports byte range
  // requests, and therefore seeking, early.
  nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(mChannel);
  if (hc) {
    // Use |mOffset| if seeking in a complete file download.
    nsAutoCString rangeString("bytes=");
    rangeString.AppendInt(aOffset);
    rangeString.Append('-');
    nsresult rv = hc->SetRequestHeader(NS_LITERAL_CSTRING("Range"), rangeString, false);
    NS_ENSURE_SUCCESS(rv, rv);

    // Send Accept header for video and audio types only (Bug 489071)
    MediaDecoderOwner* owner = mCallback->GetMediaOwner();
    MOZ_DIAGNOSTIC_ASSERT(owner);
    dom::HTMLMediaElement* element = owner->GetMediaElement();
    MOZ_DIAGNOSTIC_ASSERT(element);
    element->SetRequestHeaders(hc);
  } else {
    NS_ASSERTION(aOffset == 0, "Don't know how to seek on this channel type");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult ChannelMediaResource::Close()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  if (!mClosed) {
    CloseChannel();
    mCacheStream.Close();
    mClosed = true;
  }
  return NS_OK;
}

already_AddRefed<nsIPrincipal>
ChannelMediaResource::GetCurrentPrincipal()
{
  MOZ_ASSERT(NS_IsMainThread());
  return do_AddRef(mSharedInfo->mPrincipal);
}

bool ChannelMediaResource::CanClone()
{
  return !mClosed && mCacheStream.IsAvailableForSharing();
}

already_AddRefed<BaseMediaResource>
ChannelMediaResource::CloneData(MediaResourceCallback* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(CanClone(), "Stream can't be cloned");

  RefPtr<ChannelMediaResource> resource =
    new ChannelMediaResource(aCallback, nullptr, mURI);

  resource->mIsLiveStream = mIsLiveStream;
  resource->mIsTransportSeekable = mIsTransportSeekable;
  resource->mSharedInfo = mSharedInfo;
  mSharedInfo->mResources.AppendElement(resource.get());

  // Initially the clone is treated as suspended by the cache, because
  // we don't have a channel. If the cache needs to read data from the clone
  // it will call CacheClientResume (or CacheClientSeek with aResume true)
  // which will recreate the channel. This way, if all of the media data
  // is already in the cache we don't create an unnecessary HTTP channel
  // and perform a useless HTTP transaction.
  nsresult rv = resource->mCacheStream.InitAsClone(&mCacheStream);
  if (NS_FAILED(rv)) {
    resource->Close();
    return nullptr;
  }
  // mSuspendAgent.Suspend() accesses mCacheStream which is not ready
  // until InitAsClone() is done.
  resource->mSuspendAgent.Suspend();

  return resource.forget();
}

void ChannelMediaResource::CloseChannel()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  if (mChannel) {
    mSuspendAgent.Revoke();
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

  if (mListener) {
    mListener->Revoke();
    mListener = nullptr;
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
  return mCacheStream.ReadAt(aOffset, aBuffer, aCount, aBytes);
}

void
ChannelMediaResource::ThrottleReadahead(bool bThrottle)
{
  mCacheStream.ThrottleReadahead(bThrottle);
}

nsresult ChannelMediaResource::GetCachedRanges(MediaByteRangeSet& aRanges)
{
  return mCacheStream.GetCachedRanges(aRanges);
}

void
ChannelMediaResource::Suspend(bool aCloseImmediately)
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");

  if (mClosed) {
    // Nothing to do when we are closed.
    return;
  }

  MediaDecoderOwner* owner = mCallback->GetMediaOwner();
  MOZ_DIAGNOSTIC_ASSERT(owner);
  dom::HTMLMediaElement* element = owner->GetMediaElement();
  MOZ_DIAGNOSTIC_ASSERT(element);

  if (mChannel && aCloseImmediately && mIsTransportSeekable) {
    CloseChannel();
    element->DownloadSuspended();
  }

  if (mSuspendAgent.Suspend()) {
    if (mChannel) {
      element->DownloadSuspended();
    }
  }
}

void
ChannelMediaResource::Resume()
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");

  if (mClosed) {
    // Nothing to do when we are closed.
    return;
  }

  MediaDecoderOwner* owner = mCallback->GetMediaOwner();
  MOZ_DIAGNOSTIC_ASSERT(owner);
  dom::HTMLMediaElement* element = owner->GetMediaElement();
  MOZ_DIAGNOSTIC_ASSERT(element);

  if (mSuspendAgent.Resume()) {
    if (mChannel) {
      // Just wake up our existing channel
      element->DownloadResumed();
    } else {
      mCacheStream.NotifyResume();
    }
  }
}

nsresult
ChannelMediaResource::RecreateChannel()
{
  MOZ_DIAGNOSTIC_ASSERT(!mClosed);

  nsLoadFlags loadFlags =
    nsICachingChannel::LOAD_BYPASS_LOCAL_CACHE_IF_BUSY |
    nsIChannel::LOAD_CLASSIFY_URI |
    (mLoadInBackground ? nsIRequest::LOAD_BACKGROUND : 0);

  MediaDecoderOwner* owner = mCallback->GetMediaOwner();
  MOZ_DIAGNOSTIC_ASSERT(owner);
  dom::HTMLMediaElement* element = owner->GetMediaElement();
  MOZ_DIAGNOSTIC_ASSERT(element);

  nsCOMPtr<nsILoadGroup> loadGroup = element->GetDocumentLoadGroup();
  NS_ENSURE_TRUE(loadGroup, NS_ERROR_NULL_POINTER);

  nsSecurityFlags securityFlags = element->ShouldCheckAllowOrigin()
                                  ? nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS
                                  : nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS;

  MOZ_ASSERT(element->IsAnyOfHTMLElements(nsGkAtoms::audio, nsGkAtoms::video));
  nsContentPolicyType contentPolicyType = element->IsHTMLElement(nsGkAtoms::audio) ?
    nsIContentPolicy::TYPE_INTERNAL_AUDIO : nsIContentPolicy::TYPE_INTERNAL_VIDEO;

  // If element has 'triggeringprincipal' attribute, we will use the value as
  // triggeringPrincipal for the channel, otherwise it will default to use
  // aElement->NodePrincipal().
  // This function returns true when element has 'triggeringprincipal', so if
  // setAttrs is true we will override the origin attributes on the channel
  // later.
  nsCOMPtr<nsIPrincipal> triggeringPrincipal;
  bool setAttrs =
    nsContentUtils::QueryTriggeringPrincipal(element,
                                             getter_AddRefs(triggeringPrincipal));

  nsresult rv = NS_NewChannelWithTriggeringPrincipal(getter_AddRefs(mChannel),
                                                     mURI,
                                                     element,
                                                     triggeringPrincipal,
                                                     securityFlags,
                                                     contentPolicyType,
                                                     loadGroup,
                                                     nullptr,  // aCallbacks
                                                     loadFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  if (setAttrs) {
    nsCOMPtr<nsILoadInfo> loadInfo = mChannel->GetLoadInfo();
    if (loadInfo) {
      // The function simply returns NS_OK, so we ignore the return value.
      Unused << loadInfo->SetOriginAttributes(triggeringPrincipal->OriginAttributesRef());
   }
  }

  nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(mChannel));
  if (cos) {
    // Unconditionally disable throttling since we want the media to fluently
    // play even when we switch the tab to background.
    cos->AddClassFlags(nsIClassOfService::DontThrottle);
  }

  return rv;
}

void
ChannelMediaResource::CacheClientNotifyDataReceived()
{
  mCallback->AbstractMainThread()->Dispatch(
    NewRunnableMethod("MediaResourceCallback::NotifyDataArrived",
                      mCallback.get(),
                      &MediaResourceCallback::NotifyDataArrived));
}

void
ChannelMediaResource::CacheClientNotifyDataEnded(nsresult aStatus)
{
  mCallback->AbstractMainThread()->Dispatch(NS_NewRunnableFunction(
    "ChannelMediaResource::CacheClientNotifyDataEnded",
    [ self = RefPtr<ChannelMediaResource>(this), aStatus ]() {
      if (NS_SUCCEEDED(aStatus)) {
        self->mIsLiveStream = false;
      }
      self->mCallback->NotifyDataEnded(aStatus);
    }));
}

void
ChannelMediaResource::CacheClientNotifyPrincipalChanged()
{
  NS_ASSERTION(NS_IsMainThread(), "Don't call on non-main thread");

  mCallback->NotifyPrincipalChanged();
}

void
ChannelMediaResource::UpdatePrincipal()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mChannel);
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  if (!secMan) {
    return;
  }
  nsCOMPtr<nsIPrincipal> principal;
  secMan->GetChannelResultPrincipal(mChannel, getter_AddRefs(principal));
  if (nsContentUtils::CombineResourcePrincipals(&mSharedInfo->mPrincipal,
                                                principal)) {
    for (auto* r : mSharedInfo->mResources) {
      r->CacheClientNotifyPrincipalChanged();
    }
  }
}

void
ChannelMediaResource::CacheClientNotifySuspendedStatusChanged(bool aSuspended)
{
  mCallback->AbstractMainThread()->Dispatch(NewRunnableMethod<bool>(
    "MediaResourceCallback::NotifySuspendedStatusChanged",
    mCallback.get(),
    &MediaResourceCallback::NotifySuspendedStatusChanged,
    aSuspended));
}

nsresult
ChannelMediaResource::Seek(int64_t aOffset, bool aResume)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mClosed) {
    // Nothing to do when we are closed.
    return NS_OK;
  }

  LOG("Seek requested for aOffset [%" PRId64 "]", aOffset);

  CloseChannel();

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

  return OpenChannel(aOffset);
}

void
ChannelMediaResource::CacheClientSeek(int64_t aOffset, bool aResume)
{
  RefPtr<ChannelMediaResource> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
    "ChannelMediaResource::Seek", [self, aOffset, aResume]() {
      nsresult rv = self->Seek(aOffset, aResume);
      if (NS_FAILED(rv)) {
        // Close the streams that failed due to error. This will cause all
        // client Read and Seek operations on those streams to fail. Blocked
        // Reads will also be woken up.
        self->Close();
      }
    });
  mCallback->AbstractMainThread()->Dispatch(r.forget());
}

void
ChannelMediaResource::CacheClientSuspend()
{
  mCallback->AbstractMainThread()->Dispatch(
    NewRunnableMethod<bool>("ChannelMediaResource::Suspend",
                            this,
                            &ChannelMediaResource::Suspend,
                            false));
}

void
ChannelMediaResource::CacheClientResume()
{
  mCallback->AbstractMainThread()->Dispatch(NewRunnableMethod(
    "ChannelMediaResource::Resume", this, &ChannelMediaResource::Resume));
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
  return mCacheStream.GetDownloadRate(aIsReliable);
}

int64_t
ChannelMediaResource::GetLength()
{
  return mCacheStream.GetLength();
}

nsCString
ChannelMediaResource::GetDebugInfo()
{
  return NS_LITERAL_CSTRING("ChannelMediaResource: ") +
         mCacheStream.GetDebugInfo();
}

// ChannelSuspendAgent

bool
ChannelSuspendAgent::Suspend()
{
  MOZ_ASSERT(NS_IsMainThread());
  SuspendInternal();
  if (++mSuspendCount == 1) {
    mCacheStream.NotifyClientSuspended(true);
    return true;
  }
  return false;
}

void
ChannelSuspendAgent::SuspendInternal()
{
  MOZ_ASSERT(NS_IsMainThread());
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
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IsSuspended(), "Resume without suspend!");

  if (--mSuspendCount == 0) {
    if (mChannel && mIsChannelSuspended) {
      mChannel->Resume();
      mIsChannelSuspended = false;
    }
    mCacheStream.NotifyClientSuspended(false);
    return true;
  }
  return false;
}

void
ChannelSuspendAgent::Delegate(nsIChannel* aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aChannel);
  MOZ_ASSERT(!mChannel, "The previous channel not closed.");
  MOZ_ASSERT(!mIsChannelSuspended);

  mChannel = aChannel;
  // Ensure the suspend status of the channel matches our suspend count.
  if (IsSuspended()) {
    SuspendInternal();
  }
}

void
ChannelSuspendAgent::Revoke()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mChannel) {
    // Channel already revoked. Nothing to do.
    return;
  }

  // Before closing the channel, it needs to be resumed to make sure its internal
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
  MOZ_ASSERT(NS_IsMainThread());
  return (mSuspendCount > 0);
}

} // mozilla namespace
