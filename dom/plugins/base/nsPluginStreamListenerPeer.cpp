/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPluginStreamListenerPeer.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsIStreamConverterService.h"
#include "nsIStreamLoader.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIFileChannel.h"
#include "nsMimeTypes.h"
#include "nsISupportsPrimitives.h"
#include "nsNetCID.h"
#include "nsPluginInstanceOwner.h"
#include "nsPluginLogging.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsPluginHost.h"
#include "nsIMultiPartChannel.h"
#include "nsIInputStreamTee.h"
#include "nsPrintfCString.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocument.h"
#include "nsIWebNavigation.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"
#include "nsPluginNativeWindow.h"
#include "GeckoProfiler.h"
#include "nsPluginInstanceOwner.h"
#include "nsDataHashtable.h"
#include "NullPrincipal.h"

// nsPluginStreamListenerPeer

NS_IMPL_ISUPPORTS(nsPluginStreamListenerPeer,
                  nsIStreamListener,
                  nsIRequestObserver,
                  nsIHttpHeaderVisitor,
                  nsISupportsWeakReference,
                  nsIInterfaceRequestor,
                  nsIChannelEventSink)

nsPluginStreamListenerPeer::nsPluginStreamListenerPeer()
{
  mStreamType = NP_NORMAL;
  mStartBinding = false;
  mRequestFailed = false;

  mPendingRequests = 0;
  mHaveFiredOnStartRequest = false;

  mUseLocalCache = false;
  mModified = 0;
  mStreamOffset = 0;
  mStreamComplete = 0;
}

nsPluginStreamListenerPeer::~nsPluginStreamListenerPeer()
{
#ifdef PLUGIN_LOGGING
  MOZ_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
         ("nsPluginStreamListenerPeer::dtor this=%p, url=%s\n",this, mURLSpec.get()));
#endif

  if (mPStreamListener) {
    mPStreamListener->SetStreamListenerPeer(nullptr);
  }
}

// Called as a result of GetURL and PostURL, or by the host in the case of the
// initial plugin stream.
nsresult nsPluginStreamListenerPeer::Initialize(nsIURI *aURL,
                                                nsNPAPIPluginInstance *aInstance,
                                                nsNPAPIPluginStreamListener* aListener)
{
#ifdef PLUGIN_LOGGING
  MOZ_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
          ("nsPluginStreamListenerPeer::Initialize instance=%p, url=%s\n",
           aInstance, aURL ? aURL->GetSpecOrDefault().get() : ""));

  PR_LogFlush();
#endif

  // Not gonna work out
  if (!aInstance) {
    return NS_ERROR_FAILURE;
  }

  mURL = aURL;

  NS_ASSERTION(mPluginInstance == nullptr,
               "nsPluginStreamListenerPeer::Initialize mPluginInstance != nullptr");
  mPluginInstance = aInstance;

  // If the plugin did not request this stream, e.g. the initial stream, we wont
  // have a nsNPAPIPluginStreamListener yet - this will be handled by
  // SetUpStreamListener
  if (aListener) {
    mPStreamListener = aListener;
    mPStreamListener->SetStreamListenerPeer(this);
  }

  mPendingRequests = 1;

  return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamListenerPeer::OnStartRequest(nsIRequest *request,
                                           nsISupports* aContext)
{
  nsresult rv = NS_OK;
  AUTO_PROFILER_LABEL("nsPluginStreamListenerPeer::OnStartRequest", OTHER);

  if (mRequests.IndexOfObject(request) == -1) {
    NS_ASSERTION(mRequests.Count() == 0,
                 "Only our initial stream should be unknown!");
    TrackRequest(request);
  }

  if (mHaveFiredOnStartRequest) {
    return NS_OK;
  }

  mHaveFiredOnStartRequest = true;

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  NS_ENSURE_TRUE(channel, NS_ERROR_FAILURE);

  // deal with 404 (Not Found) HTTP response,
  // just return, this causes the request to be ignored.
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    uint32_t responseCode = 0;
    rv = httpChannel->GetResponseStatus(&responseCode);
    if (NS_FAILED(rv)) {
      // NPP_Notify() will be called from OnStopRequest
      // in nsNPAPIPluginStreamListener::CleanUpStream
      // return error will cancel this request
      // ...and we also need to tell the plugin that
      mRequestFailed = true;
      return NS_ERROR_FAILURE;
    }

    if (responseCode > 206) { // not normal
      uint32_t wantsAllNetworkStreams = 0;

      // We don't always have an instance here already, but if we do, check
      // to see if it wants all streams.
      if (mPluginInstance) {
        rv = mPluginInstance->GetValueFromPlugin(NPPVpluginWantsAllNetworkStreams,
                                                 &wantsAllNetworkStreams);
        // If the call returned an error code make sure we still use our default value.
        if (NS_FAILED(rv)) {
          wantsAllNetworkStreams = 0;
        }
      }

      if (!wantsAllNetworkStreams) {
        mRequestFailed = true;
        return NS_ERROR_FAILURE;
      }
    }
  }

  nsAutoCString contentType;
  rv = channel->GetContentType(contentType);
  if (NS_FAILED(rv))
    return rv;

  // Check ShouldProcess with content policy
  nsCOMPtr<nsILoadInfo> loadInfo = channel->GetLoadInfo();

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentProcessPolicy(mURL,
                                    loadInfo,
                                    contentType,
                                    &shouldLoad);
  if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
    mRequestFailed = true;
    return NS_ERROR_CONTENT_BLOCKED;
  }

  // Get the notification callbacks from the channel and save it as
  // week ref we'll use it in nsPluginStreamInfo::RequestRead() when
  // we'll create channel for byte range request.
  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  channel->GetNotificationCallbacks(getter_AddRefs(callbacks));
  if (callbacks)
    mWeakPtrChannelCallbacks = do_GetWeakReference(callbacks);

  nsCOMPtr<nsILoadGroup> loadGroup;
  channel->GetLoadGroup(getter_AddRefs(loadGroup));
  if (loadGroup)
    mWeakPtrChannelLoadGroup = do_GetWeakReference(loadGroup);

  int64_t length;
  rv = channel->GetContentLength(&length);

  // it's possible for the server to not send a Content-Length.
  // we should still work in this case.
  if (NS_FAILED(rv) || length < 0 || length > UINT32_MAX) {
    // check out if this is file channel
    nsCOMPtr<nsIFileChannel> fileChannel = do_QueryInterface(channel);
    if (fileChannel) {
      // file does not exist
      mRequestFailed = true;
      return NS_ERROR_FAILURE;
    }
    mLength = 0;
  }
  else {
    mLength = uint32_t(length);
  }

  nsCOMPtr<nsIURI> aURL;
  rv = channel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv))
    return rv;

  aURL->GetSpec(mURLSpec);

  if (!contentType.IsEmpty())
    mContentType = contentType;

#ifdef PLUGIN_LOGGING
  MOZ_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NOISY,
         ("nsPluginStreamListenerPeer::OnStartRequest this=%p request=%p mime=%s, url=%s\n",
          this, request, contentType.get(), mURLSpec.get()));

  PR_LogFlush();
#endif

  // Set up the stream listener...
  rv = SetUpStreamListener(request, aURL);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return rv;
}

NS_IMETHODIMP nsPluginStreamListenerPeer::OnProgress(nsIRequest *request,
                                                     nsISupports* aContext,
                                                     int64_t aProgress,
                                                     int64_t aProgressMax)
{
  nsresult rv = NS_OK;
  return rv;
}

NS_IMETHODIMP nsPluginStreamListenerPeer::OnStatus(nsIRequest *request,
                                                   nsISupports* aContext,
                                                   nsresult aStatus,
                                                   const char16_t* aStatusArg)
{
  return NS_OK;
}

nsresult
nsPluginStreamListenerPeer::GetContentType(char** result)
{
  *result = const_cast<char*>(mContentType.get());
  return NS_OK;
}


nsresult
nsPluginStreamListenerPeer::GetLength(uint32_t* result)
{
  *result = mLength;
  return NS_OK;
}

nsresult
nsPluginStreamListenerPeer::GetLastModified(uint32_t* result)
{
  *result = mModified;
  return NS_OK;
}

nsresult
nsPluginStreamListenerPeer::GetURL(const char** result)
{
  *result = mURLSpec.get();
  return NS_OK;
}

// XXX: Converting the channel within nsPluginStreamListenerPeer
// to use asyncOpen2() and do not want to touch the fragile logic
// of byte range requests. Hence we just introduce this lightweight
// wrapper to proxy the context.
class PluginContextProxy final : public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS

  PluginContextProxy(nsIStreamListener *aListener, nsISupports* aContext)
    : mListener(aListener)
    , mContext(aContext)
  {
    MOZ_ASSERT(aListener);
    MOZ_ASSERT(aContext);
  }

  NS_IMETHOD
  OnDataAvailable(nsIRequest* aRequest,
                  nsISupports* aContext,
                  nsIInputStream *aIStream,
                  uint64_t aSourceOffset,
                  uint32_t aLength) override
  {
    // Proxy OnDataAvailable using the internal context
    return mListener->OnDataAvailable(aRequest,
                                      mContext,
                                      aIStream,
                                      aSourceOffset,
                                      aLength);
  }

  NS_IMETHOD
  OnStartRequest(nsIRequest* aRequest, nsISupports* aContext) override
  {
    // Proxy OnStartRequest using the internal context
    return mListener->OnStartRequest(aRequest, mContext);
  }

  NS_IMETHOD
  OnStopRequest(nsIRequest* aRequest, nsISupports* aContext,
                nsresult aStatusCode) override
  {
    // Proxy OnStopRequest using the inernal context
    return mListener->OnStopRequest(aRequest,
                                    mContext,
                                    aStatusCode);
  }

private:
  ~PluginContextProxy() {}
  nsCOMPtr<nsIStreamListener> mListener;
  nsCOMPtr<nsISupports> mContext;
};

NS_IMPL_ISUPPORTS(PluginContextProxy, nsIStreamListener)

nsresult
nsPluginStreamListenerPeer::GetStreamOffset(int32_t* result)
{
  *result = mStreamOffset;
  return NS_OK;
}

nsresult
nsPluginStreamListenerPeer::SetStreamOffset(int32_t value)
{
  mStreamOffset = value;
  return NS_OK;
}

NS_IMETHODIMP nsPluginStreamListenerPeer::OnDataAvailable(nsIRequest *request,
                                                          nsISupports* aContext,
                                                          nsIInputStream *aIStream,
                                                          uint64_t sourceOffset,
                                                          uint32_t aLength)
{
  if (mRequests.IndexOfObject(request) == -1) {
    MOZ_ASSERT(false, "Received OnDataAvailable for untracked request.");
    return NS_ERROR_UNEXPECTED;
  }

  if (mRequestFailed)
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;

  if (!mPStreamListener)
    return NS_ERROR_FAILURE;

  const char * url = nullptr;
  GetURL(&url);

  PLUGIN_LOG(PLUGIN_LOG_NOISY,
             ("nsPluginStreamListenerPeer::OnDataAvailable this=%p request=%p, offset=%" PRIu64 ", length=%u, url=%s\n",
              this, request, sourceOffset, aLength, url ? url : "no url set"));

  nsCOMPtr<nsIInputStream> stream = aIStream;
  rv = mPStreamListener->OnDataAvailable(this,
                                         stream,
                                         aLength);

  // if a plugin returns an error, the peer must kill the stream
  //   else the stream and PluginStreamListener leak
  if (NS_FAILED(rv)) {
    request->Cancel(rv);
  }

  return rv;
}

NS_IMETHODIMP nsPluginStreamListenerPeer::OnStopRequest(nsIRequest *request,
                                                        nsISupports* aContext,
                                                        nsresult aStatus)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIMultiPartChannel> mp = do_QueryInterface(request);
  if (!mp) {
    bool found = mRequests.RemoveObject(request);
    if (!found) {
      NS_ERROR("Received OnStopRequest for untracked request.");
    }
  }

  PLUGIN_LOG(PLUGIN_LOG_NOISY,
             ("nsPluginStreamListenerPeer::OnStopRequest this=%p aStatus=%" PRIu32 " request=%p\n",
              this, static_cast<uint32_t>(aStatus), request));

  // if we still have pending stuff to do, lets not close the plugin socket.
  if (--mPendingRequests > 0)
    return NS_OK;

  if (!mPStreamListener)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  if (!channel)
    return NS_ERROR_FAILURE;
  // Set the content type to ensure we don't pass null to the plugin
  nsAutoCString aContentType;
  rv = channel->GetContentType(aContentType);
  if (NS_FAILED(rv) && !mRequestFailed)
    return rv;

  if (!aContentType.IsEmpty())
    mContentType = aContentType;

  // set error status if stream failed so we notify the plugin
  if (mRequestFailed)
    aStatus = NS_ERROR_FAILURE;

  if (NS_FAILED(aStatus)) {
    // on error status cleanup the stream
    // and return w/o OnFileAvailable()
    mPStreamListener->OnStopBinding(this, aStatus);
    return NS_OK;
  }

  if (mStartBinding) {
    // On start binding has been called
    mPStreamListener->OnStopBinding(this, aStatus);
  } else {
    // OnStartBinding hasn't been called, so complete the action.
    mPStreamListener->OnStartBinding(this);
    mPStreamListener->OnStopBinding(this, aStatus);
  }

  if (NS_SUCCEEDED(aStatus)) {
    mStreamComplete = true;
  }

  return NS_OK;
}

nsresult nsPluginStreamListenerPeer::SetUpStreamListener(nsIRequest *request,
                                                         nsIURI* aURL)
{
  nsresult rv = NS_OK;

  // If we don't yet have a stream listener, we need to get
  // one from the plugin.
  // NOTE: this should only happen when a stream was NOT created
  // with GetURL or PostURL (i.e. it's the initial stream we
  // send to the plugin as determined by the SRC or DATA attribute)
  if (!mPStreamListener) {
    if (!mPluginInstance) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<nsNPAPIPluginStreamListener> streamListener;
    rv = mPluginInstance->NewStreamListener(nullptr, nullptr,
                                            getter_AddRefs(streamListener));
    if (NS_FAILED(rv) || !streamListener) {
      return NS_ERROR_FAILURE;
    }

    mPStreamListener = static_cast<nsNPAPIPluginStreamListener*>(streamListener.get());
  }

  mPStreamListener->SetStreamListenerPeer(this);

  // get httpChannel to retrieve some info we need for nsIPluginStreamInfo setup
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);

  /*
   * Assumption
   * By the time nsPluginStreamListenerPeer::OnDataAvailable() gets
   * called, all the headers have been read.
   */
  if (httpChannel) {
    // Reassemble the HTTP response status line and provide it to our
    // listener.  Would be nice if we could get the raw status line,
    // but nsIHttpChannel doesn't currently provide that.
    // Status code: required; the status line isn't useful without it.
    uint32_t statusNum;
    if (NS_SUCCEEDED(httpChannel->GetResponseStatus(&statusNum)) &&
        statusNum < 1000) {
      // HTTP version: provide if available.  Defaults to empty string.
      nsCString ver;
      nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal =
      do_QueryInterface(channel);
      if (httpChannelInternal) {
        uint32_t major, minor;
        if (NS_SUCCEEDED(httpChannelInternal->GetResponseVersion(&major,
                                                                 &minor))) {
          ver = nsPrintfCString("/%" PRIu32 ".%" PRIu32, major, minor);
        }
      }

      // Status text: provide if available.  Defaults to "OK".
      nsCString statusText;
      if (NS_FAILED(httpChannel->GetResponseStatusText(statusText))) {
        statusText = "OK";
      }

      // Assemble everything and pass to listener.
      nsPrintfCString status("HTTP%s %" PRIu32 " %s", ver.get(), statusNum,
                             statusText.get());
      static_cast<nsIHTTPHeaderListener*>(mPStreamListener)->StatusLine(status.get());
    }

    // Also provide all HTTP response headers to our listener.
    rv = httpChannel->VisitResponseHeaders(this);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    // we require a content len
    // get Last-Modified header for plugin info
    nsAutoCString lastModified;
    if (NS_SUCCEEDED(httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("last-modified"), lastModified)) &&
        !lastModified.IsEmpty()) {
      PRTime time64;
      PR_ParseTimeString(lastModified.get(), true, &time64);  //convert string time to integer time

      // Convert PRTime to unix-style time_t, i.e. seconds since the epoch
      double fpTime = double(time64);
      mModified = (uint32_t)(fpTime * 1e-6 + 0.5);
    }
  }

  MOZ_ASSERT(!mRequest);
  mRequest = request;

  rv = mPStreamListener->OnStartBinding(this);

  mStartBinding = true;

  if (NS_FAILED(rv))
    return rv;

  return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamListenerPeer::VisitHeader(const nsACString &header, const nsACString &value)
{
  return mPStreamListener->NewResponseHeader(PromiseFlatCString(header).get(),
                                             PromiseFlatCString(value).get());
}

nsresult
nsPluginStreamListenerPeer::GetInterfaceGlobal(const nsIID& aIID, void** result)
{
  if (!mPluginInstance) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsPluginInstanceOwner> owner = mPluginInstance->GetOwner();
  if (!owner) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocument> doc;
  nsresult rv = owner->GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(rv) || !doc) {
    return NS_ERROR_FAILURE;
  }

  nsPIDOMWindowOuter *window = doc->GetWindow();
  if (!window) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(window);
  nsCOMPtr<nsIInterfaceRequestor> ir = do_QueryInterface(webNav);
  if (!ir) {
    return NS_ERROR_FAILURE;
  }

  return ir->GetInterface(aIID, result);
}

NS_IMETHODIMP
nsPluginStreamListenerPeer::GetInterface(const nsIID& aIID, void** result)
{
  // Provide nsIChannelEventSink ourselves, otherwise let our document's
  // script global object owner provide the interface.
  if (aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
    return QueryInterface(aIID, result);
  }

  return GetInterfaceGlobal(aIID, result);
}

/**
 * Proxy class which forwards async redirect notifications back to the necko
 * callback, keeping nsPluginStreamListenerPeer::mRequests in sync with
 * which channel is active.
 */
class ChannelRedirectProxyCallback : public nsIAsyncVerifyRedirectCallback
{
public:
  ChannelRedirectProxyCallback(nsPluginStreamListenerPeer* listener,
                               nsIAsyncVerifyRedirectCallback* parent,
                               nsIChannel* oldChannel,
                               nsIChannel* newChannel)
    : mWeakListener(do_GetWeakReference(static_cast<nsIStreamListener*>(listener)))
    , mParent(parent)
    , mOldChannel(oldChannel)
    , mNewChannel(newChannel)
  {
  }

  ChannelRedirectProxyCallback() {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD OnRedirectVerifyCallback(nsresult aResult) override
  {
    if (NS_SUCCEEDED(aResult)) {
      nsCOMPtr<nsIStreamListener> listener = do_QueryReferent(mWeakListener);
      if (listener)
        static_cast<nsPluginStreamListenerPeer*>(listener.get())->ReplaceRequest(mOldChannel, mNewChannel);
    }
    return mParent->OnRedirectVerifyCallback(aResult);
  }

private:
  virtual ~ChannelRedirectProxyCallback() {}

  nsWeakPtr mWeakListener;
  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mParent;
  nsCOMPtr<nsIChannel> mOldChannel;
  nsCOMPtr<nsIChannel> mNewChannel;
};

NS_IMPL_ISUPPORTS(ChannelRedirectProxyCallback, nsIAsyncVerifyRedirectCallback)


NS_IMETHODIMP
nsPluginStreamListenerPeer::AsyncOnChannelRedirect(nsIChannel *oldChannel, nsIChannel *newChannel,
                                                   uint32_t flags, nsIAsyncVerifyRedirectCallback* callback)
{
  // Disallow redirects if we don't have a stream listener.
  if (!mPStreamListener) {
    return NS_ERROR_FAILURE;
  }

  // Don't allow cross-origin 307 POST redirects.
  nsCOMPtr<nsIHttpChannel> oldHttpChannel(do_QueryInterface(oldChannel));
  if (oldHttpChannel) {
    uint32_t responseStatus;
    nsresult rv = oldHttpChannel->GetResponseStatus(&responseStatus);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (responseStatus == 307) {
      nsAutoCString method;
      rv = oldHttpChannel->GetRequestMethod(method);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (method.EqualsLiteral("POST")) {
        rv = nsContentUtils::CheckSameOrigin(oldChannel, newChannel);
        if (NS_FAILED(rv)) {
          return rv;
        }
      }
    }
  }

  nsCOMPtr<nsIAsyncVerifyRedirectCallback> proxyCallback =
    new ChannelRedirectProxyCallback(this, callback, oldChannel, newChannel);

  // Give NPAPI a chance to control redirects.
  bool notificationHandled = mPStreamListener->HandleRedirectNotification(oldChannel, newChannel, proxyCallback);
  if (notificationHandled) {
    return NS_OK;
  }

  // Fall back to channel event sink for window.
  nsCOMPtr<nsIChannelEventSink> channelEventSink;
  nsresult rv = GetInterfaceGlobal(NS_GET_IID(nsIChannelEventSink), getter_AddRefs(channelEventSink));
  if (NS_FAILED(rv)) {
    return rv;
  }

  return channelEventSink->AsyncOnChannelRedirect(oldChannel, newChannel, flags, proxyCallback);
}
