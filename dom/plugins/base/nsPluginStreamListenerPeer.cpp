/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPluginStreamListenerPeer.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsIDOMElement.h"
#include "nsIStreamConverterService.h"
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
#include "nsIByteRangeRequest.h"
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

#define MAGIC_REQUEST_CONTEXT 0x01020304

// nsPluginByteRangeStreamListener

class nsPluginByteRangeStreamListener
  : public nsIStreamListener
  , public nsIInterfaceRequestor
{
public:
  explicit nsPluginByteRangeStreamListener(nsIWeakReference* aWeakPtr);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR

private:
  virtual ~nsPluginByteRangeStreamListener();

  nsCOMPtr<nsIStreamListener> mStreamConverter;
  nsWeakPtr mWeakPtrPluginStreamListenerPeer;
  bool mRemoveMagicNumber;
};

NS_IMPL_ISUPPORTS(nsPluginByteRangeStreamListener,
                  nsIRequestObserver,
                  nsIStreamListener,
                  nsIInterfaceRequestor)

nsPluginByteRangeStreamListener::nsPluginByteRangeStreamListener(nsIWeakReference* aWeakPtr)
{
  mWeakPtrPluginStreamListenerPeer = aWeakPtr;
  mRemoveMagicNumber = false;
}

nsPluginByteRangeStreamListener::~nsPluginByteRangeStreamListener()
{
  mStreamConverter = 0;
  mWeakPtrPluginStreamListenerPeer = 0;
}

/**
 * Unwrap any byte-range requests so that we can check whether the base channel
 * is being tracked properly.
 */
static nsCOMPtr<nsIRequest>
GetBaseRequest(nsIRequest* r)
{
  nsCOMPtr<nsIMultiPartChannel> mp = do_QueryInterface(r);
  if (!mp)
    return r;

  nsCOMPtr<nsIChannel> base;
  mp->GetBaseChannel(getter_AddRefs(base));
  return already_AddRefed<nsIRequest>(base.forget());
}

NS_IMETHODIMP
nsPluginByteRangeStreamListener::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  nsresult rv;

  nsCOMPtr<nsIStreamListener> finalStreamListener = do_QueryReferent(mWeakPtrPluginStreamListenerPeer);
  if (!finalStreamListener)
    return NS_ERROR_FAILURE;

  nsPluginStreamListenerPeer *pslp =
    static_cast<nsPluginStreamListenerPeer*>(finalStreamListener.get());

  NS_ASSERTION(pslp->mRequests.IndexOfObject(GetBaseRequest(request)) != -1,
               "Untracked byte-range request?");

  nsCOMPtr<nsIStreamConverterService> serv = do_GetService(NS_STREAMCONVERTERSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = serv->AsyncConvertData(MULTIPART_BYTERANGES,
                                "*/*",
                                finalStreamListener,
                                nullptr,
                                getter_AddRefs(mStreamConverter));
    if (NS_SUCCEEDED(rv)) {
      rv = mStreamConverter->OnStartRequest(request, ctxt);
      if (NS_SUCCEEDED(rv))
        return rv;
    }
  }
  mStreamConverter = 0;

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(request));
  if (!httpChannel) {
    return NS_ERROR_FAILURE;
  }

  uint32_t responseCode = 0;
  rv = httpChannel->GetResponseStatus(&responseCode);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  if (responseCode != 200) {
    uint32_t wantsAllNetworkStreams = 0;
    rv = pslp->GetPluginInstance()->GetValueFromPlugin(NPPVpluginWantsAllNetworkStreams,
                                                       &wantsAllNetworkStreams);
    // If the call returned an error code make sure we still use our default value.
    if (NS_FAILED(rv)) {
      wantsAllNetworkStreams = 0;
    }

    if (!wantsAllNetworkStreams){
      return NS_ERROR_FAILURE;
    }
  }

  // if server cannot continue with byte range (206 status) and sending us whole object (200 status)
  // reset this seekable stream & try serve it to plugin instance as a file
  mStreamConverter = finalStreamListener;
  mRemoveMagicNumber = true;

  rv = pslp->ServeStreamAsFile(request, ctxt);
  return rv;
}

NS_IMETHODIMP
nsPluginByteRangeStreamListener::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                                               nsresult status)
{
  if (!mStreamConverter)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStreamListener> finalStreamListener = do_QueryReferent(mWeakPtrPluginStreamListenerPeer);
  if (!finalStreamListener)
    return NS_ERROR_FAILURE;

  nsPluginStreamListenerPeer *pslp =
    static_cast<nsPluginStreamListenerPeer*>(finalStreamListener.get());
  bool found = pslp->mRequests.RemoveObject(request);
  if (!found) {
    NS_ERROR("OnStopRequest received for untracked byte-range request!");
  }

  if (mRemoveMagicNumber) {
    // remove magic number from container
    nsCOMPtr<nsISupportsPRUint32> container = do_QueryInterface(ctxt);
    if (container) {
      uint32_t magicNumber = 0;
      container->GetData(&magicNumber);
      if (magicNumber == MAGIC_REQUEST_CONTEXT) {
        // to allow properly finish nsPluginStreamListenerPeer->OnStopRequest()
        // set it to something that is not the magic number.
        container->SetData(0);
      }
    } else {
      NS_WARNING("Bad state of nsPluginByteRangeStreamListener");
    }
  }

  return mStreamConverter->OnStopRequest(request, ctxt, status);
}

// CachedFileHolder

CachedFileHolder::CachedFileHolder(nsIFile* cacheFile)
: mFile(cacheFile)
{
  NS_ASSERTION(mFile, "Empty CachedFileHolder");
}

CachedFileHolder::~CachedFileHolder()
{
  mFile->Remove(false);
}

void
CachedFileHolder::AddRef()
{
  ++mRefCnt;
  NS_LOG_ADDREF(this, mRefCnt, "CachedFileHolder", sizeof(*this));
}

void
CachedFileHolder::Release()
{
  --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "CachedFileHolder");
  if (0 == mRefCnt)
    delete this;
}


NS_IMETHODIMP
nsPluginByteRangeStreamListener::OnDataAvailable(nsIRequest *request, nsISupports *ctxt,
                                                 nsIInputStream *inStr,
                                                 uint64_t sourceOffset,
                                                 uint32_t count)
{
  if (!mStreamConverter)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStreamListener> finalStreamListener = do_QueryReferent(mWeakPtrPluginStreamListenerPeer);
  if (!finalStreamListener)
    return NS_ERROR_FAILURE;

  return mStreamConverter->OnDataAvailable(request, ctxt, inStr, sourceOffset, count);
}

NS_IMETHODIMP
nsPluginByteRangeStreamListener::GetInterface(const nsIID& aIID, void** result)
{
  // Forward interface requests to our parent
  nsCOMPtr<nsIInterfaceRequestor> finalStreamListener = do_QueryReferent(mWeakPtrPluginStreamListenerPeer);
  if (!finalStreamListener)
    return NS_ERROR_FAILURE;

  return finalStreamListener->GetInterface(aIID, result);
}

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
  mAbort = false;
  mRequestFailed = false;

  mPendingRequests = 0;
  mHaveFiredOnStartRequest = false;
  mDataForwardToRequest = nullptr;

  mSeekable = false;
  mModified = 0;
  mStreamOffset = 0;
  mStreamComplete = 0;
}

nsPluginStreamListenerPeer::~nsPluginStreamListenerPeer()
{
#ifdef PLUGIN_LOGGING
  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
         ("nsPluginStreamListenerPeer::dtor this=%p, url=%s\n",this, mURLSpec.get()));
#endif

  if (mPStreamListener) {
    mPStreamListener->SetStreamListenerPeer(nullptr);
  }

  // close FD of mFileCacheOutputStream if it's still open
  // or we won't be able to remove the cache file
  if (mFileCacheOutputStream)
    mFileCacheOutputStream = nullptr;

  delete mDataForwardToRequest;

  if (mPluginInstance)
    mPluginInstance->FileCachedStreamListeners()->RemoveElement(this);
}

// Called as a result of GetURL and PostURL, or by the host in the case of the
// initial plugin stream.
nsresult nsPluginStreamListenerPeer::Initialize(nsIURI *aURL,
                                                nsNPAPIPluginInstance *aInstance,
                                                nsNPAPIPluginStreamListener* aListener)
{
#ifdef PLUGIN_LOGGING
  nsAutoCString urlSpec;
  if (aURL != nullptr) aURL->GetSpec(urlSpec);

  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
         ("nsPluginStreamListenerPeer::Initialize instance=%p, url=%s\n", aInstance, urlSpec.get()));

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

  mDataForwardToRequest = new nsDataHashtable<nsUint32HashKey, uint32_t>();

  return NS_OK;
}

// SetupPluginCacheFile is called if we have to save the stream to disk.
//
// These files will be deleted when the host is destroyed.
//
// TODO? What if we fill up the the dest dir?
nsresult
nsPluginStreamListenerPeer::SetupPluginCacheFile(nsIChannel* channel)
{
  nsresult rv = NS_OK;

  bool useExistingCacheFile = false;
  nsRefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();

  // Look for an existing cache file for the URI.
  nsTArray< nsRefPtr<nsNPAPIPluginInstance> > *instances = pluginHost->InstanceArray();
  for (uint32_t i = 0; i < instances->Length(); i++) {
    // most recent streams are at the end of list
    nsTArray<nsPluginStreamListenerPeer*> *streamListeners = instances->ElementAt(i)->FileCachedStreamListeners();
    for (int32_t i = streamListeners->Length() - 1; i >= 0; --i) {
      nsPluginStreamListenerPeer *lp = streamListeners->ElementAt(i);
      if (lp && lp->mLocalCachedFileHolder) {
        useExistingCacheFile = lp->UseExistingPluginCacheFile(this);
        if (useExistingCacheFile) {
          mLocalCachedFileHolder = lp->mLocalCachedFileHolder;
          break;
        }
      }
      if (useExistingCacheFile)
        break;
    }
  }

  // Create a new cache file if one could not be found.
  if (!useExistingCacheFile) {
    nsCOMPtr<nsIFile> pluginTmp;
    rv = nsPluginHost::GetPluginTempDir(getter_AddRefs(pluginTmp));
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Get the filename from the channel
    nsCOMPtr<nsIURI> uri;
    rv = channel->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
    if (!url)
      return NS_ERROR_FAILURE;

    nsAutoCString filename;
    url->GetFileName(filename);
    if (NS_FAILED(rv))
      return rv;

    // Create a file to save our stream into. Should we scramble the name?
    filename.Insert(NS_LITERAL_CSTRING("plugin-"), 0);
    rv = pluginTmp->AppendNative(filename);
    if (NS_FAILED(rv))
      return rv;

    // Yes, make it unique.
    rv = pluginTmp->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
    if (NS_FAILED(rv))
      return rv;

    // create a file output stream to write to...
    nsCOMPtr<nsIOutputStream> outstream;
    rv = NS_NewLocalFileOutputStream(getter_AddRefs(mFileCacheOutputStream), pluginTmp, -1, 00600);
    if (NS_FAILED(rv))
      return rv;

    // save the file.
    mLocalCachedFileHolder = new CachedFileHolder(pluginTmp);
  }

  // add this listenerPeer to list of stream peers for this instance
  mPluginInstance->FileCachedStreamListeners()->AppendElement(this);

  return rv;
}

NS_IMETHODIMP
nsPluginStreamListenerPeer::OnStartRequest(nsIRequest *request,
                                           nsISupports* aContext)
{
  nsresult rv = NS_OK;
  PROFILER_LABEL("nsPluginStreamListenerPeer", "OnStartRequest",
    js::ProfileEntry::Category::OTHER);

  if (mRequests.IndexOfObject(GetBaseRequest(request)) == -1) {
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
  nsRefPtr<nsPluginInstanceOwner> owner;
  if (mPluginInstance) {
    owner = mPluginInstance->GetOwner();
  }
  nsCOMPtr<nsIDOMElement> element;
  nsCOMPtr<nsIDocument> doc;
  if (owner) {
    owner->GetDOMElement(getter_AddRefs(element));
    owner->GetDocument(getter_AddRefs(doc));
  }
  nsCOMPtr<nsIPrincipal> principal = doc ? doc->NodePrincipal() : nullptr;

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentProcessPolicy(nsIContentPolicy::TYPE_OBJECT_SUBREQUEST,
                                    mURL,
                                    principal,
                                    element,
                                    contentType,
                                    nullptr,
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
  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NOISY,
         ("nsPluginStreamListenerPeer::OnStartRequest this=%p request=%p mime=%s, url=%s\n",
          this, request, contentType.get(), mURLSpec.get()));

  PR_LogFlush();
#endif

  // Set up the stream listener...
  rv = SetUpStreamListener(request, aURL);
  if (NS_FAILED(rv)) return rv;

  return rv;
}

NS_IMETHODIMP nsPluginStreamListenerPeer::OnProgress(nsIRequest *request,
                                                     nsISupports* aContext,
                                                     uint64_t aProgress,
                                                     uint64_t aProgressMax)
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
nsPluginStreamListenerPeer::IsSeekable(bool* result)
{
  *result = mSeekable;
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

void
nsPluginStreamListenerPeer::MakeByteRangeString(NPByteRange* aRangeList, nsACString &rangeRequest,
                                                int32_t *numRequests)
{
  rangeRequest.Truncate();
  *numRequests  = 0;
  //the string should look like this: bytes=500-700,601-999
  if (!aRangeList)
    return;

  int32_t requestCnt = 0;
  nsAutoCString string("bytes=");

  for (NPByteRange * range = aRangeList; range != nullptr; range = range->next) {
    // XXX zero length?
    if (!range->length)
      continue;

    // XXX needs to be fixed for negative offsets
    string.AppendInt(range->offset);
    string.Append('-');
    string.AppendInt(range->offset + range->length - 1);
    if (range->next)
      string.Append(',');

    requestCnt++;
  }

  // get rid of possible trailing comma
  string.Trim(",", false);

  rangeRequest = string;
  *numRequests  = requestCnt;
  return;
}

nsresult
nsPluginStreamListenerPeer::RequestRead(NPByteRange* rangeList)
{
  nsAutoCString rangeString;
  int32_t numRequests;

  MakeByteRangeString(rangeList, rangeString, &numRequests);

  if (numRequests == 0)
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;

  nsRefPtr<nsPluginInstanceOwner> owner = mPluginInstance->GetOwner();
  nsCOMPtr<nsIDOMElement> element;
  nsCOMPtr<nsIDocument> doc;
  if (owner) {
    rv = owner->GetDOMElement(getter_AddRefs(element));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = owner->GetDocument(getter_AddRefs(doc));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIInterfaceRequestor> callbacks = do_QueryReferent(mWeakPtrChannelCallbacks);
  nsCOMPtr<nsILoadGroup> loadGroup = do_QueryReferent(mWeakPtrChannelLoadGroup);

  nsCOMPtr<nsIChannel> channel;
  nsCOMPtr<nsINode> requestingNode(do_QueryInterface(element));
  if (requestingNode) {
    rv = NS_NewChannel(getter_AddRefs(channel),
                       mURL,
                       requestingNode,
                       nsILoadInfo::SEC_NORMAL,
                       nsIContentPolicy::TYPE_OTHER,
                       loadGroup,
                       callbacks);
  }
  else {
    // in this else branch we really don't know where the load is coming
    // from and in fact should use something better than just using
    // a nullPrincipal as the loadingPrincipal.
    nsCOMPtr<nsIPrincipal> principal =
      do_CreateInstance("@mozilla.org/nullprincipal;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = NS_NewChannel(getter_AddRefs(channel),
                       mURL,
                       principal,
                       nsILoadInfo::SEC_NORMAL,
                       nsIContentPolicy::TYPE_OTHER,
                       loadGroup,
                       callbacks);
  }

  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (!httpChannel)
    return NS_ERROR_FAILURE;

  httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Range"), rangeString, false);

  mAbort = true; // instruct old stream listener to cancel
  // the request on the next ODA.

  nsCOMPtr<nsIStreamListener> converter;

  if (numRequests == 1) {
    converter = this;
    // set current stream offset equal to the first offset in the range list
    // it will work for single byte range request
    // for multy range we'll reset it in ODA
    SetStreamOffset(rangeList->offset);
  } else {
    nsWeakPtr weakpeer =
    do_GetWeakReference(static_cast<nsISupportsWeakReference*>(this));
    nsPluginByteRangeStreamListener *brrListener =
    new nsPluginByteRangeStreamListener(weakpeer);
    if (brrListener)
      converter = brrListener;
    else
      return NS_ERROR_OUT_OF_MEMORY;
  }

  mPendingRequests += numRequests;

  nsCOMPtr<nsISupportsPRUint32> container = do_CreateInstance(NS_SUPPORTS_PRUINT32_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;
  rv = container->SetData(MAGIC_REQUEST_CONTEXT);
  if (NS_FAILED(rv))
    return rv;

  rv = channel->AsyncOpen(converter, container);
  if (NS_SUCCEEDED(rv))
    TrackRequest(channel);
  return rv;
}

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

nsresult nsPluginStreamListenerPeer::ServeStreamAsFile(nsIRequest *request,
                                                       nsISupports* aContext)
{
  if (!mPluginInstance)
    return NS_ERROR_FAILURE;

  // mPluginInstance->Stop calls mPStreamListener->CleanUpStream(), so stream will be properly clean up
  mPluginInstance->Stop();
  mPluginInstance->Start();
  nsRefPtr<nsPluginInstanceOwner> owner = mPluginInstance->GetOwner();
  if (owner) {
    NPWindow* window = nullptr;
    owner->GetWindow(window);
#if (MOZ_WIDGET_GTK == 2) || defined(MOZ_WIDGET_QT)
    // Should call GetPluginPort() here.
    // This part is copied from nsPluginInstanceOwner::GetPluginPort().
    nsCOMPtr<nsIWidget> widget;
    ((nsPluginNativeWindow*)window)->GetPluginWidget(getter_AddRefs(widget));
    if (widget) {
      window->window = widget->GetNativeData(NS_NATIVE_PLUGIN_PORT);
    }
#endif
    owner->CallSetWindow();
  }

  mSeekable = false;
  mPStreamListener->OnStartBinding(this);
  mStreamOffset = 0;

  // force the plugin to use stream as file
  mStreamType = NP_ASFILE;

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  if (channel) {
    SetupPluginCacheFile(channel);
  }

  // unset mPendingRequests
  mPendingRequests = 0;

  return NS_OK;
}

bool
nsPluginStreamListenerPeer::UseExistingPluginCacheFile(nsPluginStreamListenerPeer* psi)
{
  NS_ENSURE_TRUE(psi, false);

  if (psi->mLength == mLength &&
      psi->mModified == mModified &&
      mStreamComplete &&
      mURLSpec.Equals(psi->mURLSpec))
  {
    return true;
  }
  return false;
}

NS_IMETHODIMP nsPluginStreamListenerPeer::OnDataAvailable(nsIRequest *request,
                                                          nsISupports* aContext,
                                                          nsIInputStream *aIStream,
                                                          uint64_t sourceOffset,
                                                          uint32_t aLength)
{
  if (mRequests.IndexOfObject(GetBaseRequest(request)) == -1) {
    MOZ_ASSERT(false, "Received OnDataAvailable for untracked request.");
    return NS_ERROR_UNEXPECTED;
  }

  if (mRequestFailed)
    return NS_ERROR_FAILURE;

  if (mAbort) {
    uint32_t magicNumber = 0;  // set it to something that is not the magic number.
    nsCOMPtr<nsISupportsPRUint32> container = do_QueryInterface(aContext);
    if (container)
      container->GetData(&magicNumber);

    if (magicNumber != MAGIC_REQUEST_CONTEXT) {
      // this is not one of our range requests
      mAbort = false;
      return NS_BINDING_ABORTED;
    }
  }

  nsresult rv = NS_OK;

  if (!mPStreamListener)
    return NS_ERROR_FAILURE;

  const char * url = nullptr;
  GetURL(&url);

  PLUGIN_LOG(PLUGIN_LOG_NOISY,
             ("nsPluginStreamListenerPeer::OnDataAvailable this=%p request=%p, offset=%llu, length=%u, url=%s\n",
              this, request, sourceOffset, aLength, url ? url : "no url set"));

  // if the plugin has requested an AsFileOnly stream, then don't
  // call OnDataAvailable
  if (mStreamType != NP_ASFILEONLY) {
    // get the absolute offset of the request, if one exists.
    nsCOMPtr<nsIByteRangeRequest> brr = do_QueryInterface(request);
    if (brr) {
      if (!mDataForwardToRequest)
        return NS_ERROR_FAILURE;

      int64_t absoluteOffset64 = 0;
      brr->GetStartRange(&absoluteOffset64);

      // XXX handle 64-bit for real
      int32_t absoluteOffset = (int32_t)int64_t(absoluteOffset64);

      // we need to track how much data we have forwarded to the
      // plugin.

      // FIXME: http://bugzilla.mozilla.org/show_bug.cgi?id=240130
      //
      // Why couldn't this be tracked on the plugin info, and not in a
      // *hash table*?
      int32_t amtForwardToPlugin = mDataForwardToRequest->Get(absoluteOffset);
      mDataForwardToRequest->Put(absoluteOffset, (amtForwardToPlugin + aLength));

      SetStreamOffset(absoluteOffset + amtForwardToPlugin);
    }

    nsCOMPtr<nsIInputStream> stream = aIStream;

    // if we are caching the file ourselves to disk, we want to 'tee' off
    // the data as the plugin read from the stream.  We do this by the magic
    // of an input stream tee.

    if (mFileCacheOutputStream) {
      rv = NS_NewInputStreamTee(getter_AddRefs(stream), aIStream, mFileCacheOutputStream);
      if (NS_FAILED(rv))
        return rv;
    }

    rv =  mPStreamListener->OnDataAvailable(this,
                                            stream,
                                            aLength);

    // if a plugin returns an error, the peer must kill the stream
    //   else the stream and PluginStreamListener leak
    if (NS_FAILED(rv))
      request->Cancel(rv);
  }
  else
  {
    // if we don't read from the stream, OnStopRequest will never be called
    char* buffer = new char[aLength];
    uint32_t amountRead, amountWrote = 0;
    rv = aIStream->Read(buffer, aLength, &amountRead);

    // if we are caching this to disk ourselves, lets write the bytes out.
    if (mFileCacheOutputStream) {
      while (amountWrote < amountRead && NS_SUCCEEDED(rv)) {
        rv = mFileCacheOutputStream->Write(buffer, amountRead, &amountWrote);
      }
    }
    delete [] buffer;
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
             ("nsPluginStreamListenerPeer::OnStopRequest this=%p aStatus=%d request=%p\n",
              this, aStatus, request));

  // for ByteRangeRequest we're just updating the mDataForwardToRequest hash and return.
  nsCOMPtr<nsIByteRangeRequest> brr = do_QueryInterface(request);
  if (brr) {
    int64_t absoluteOffset64 = 0;
    brr->GetStartRange(&absoluteOffset64);
    // XXX support 64-bit offsets
    int32_t absoluteOffset = (int32_t)int64_t(absoluteOffset64);

    // remove the request from our data forwarding count hash.
    mDataForwardToRequest->Remove(absoluteOffset);


    PLUGIN_LOG(PLUGIN_LOG_NOISY,
               ("                          ::OnStopRequest for ByteRangeRequest Started=%d\n",
                absoluteOffset));
  } else {
    // if this is not byte range request and
    // if we are writting the stream to disk ourselves,
    // close & tear it down here
    mFileCacheOutputStream = nullptr;
  }

  // if we still have pending stuff to do, lets not close the plugin socket.
  if (--mPendingRequests > 0)
    return NS_OK;

  // we keep our connections around...
  nsCOMPtr<nsISupportsPRUint32> container = do_QueryInterface(aContext);
  if (container) {
    uint32_t magicNumber = 0;  // set it to something that is not the magic number.
    container->GetData(&magicNumber);
    if (magicNumber == MAGIC_REQUEST_CONTEXT) {
      // this is one of our range requests
      return NS_OK;
    }
  }

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

  // call OnFileAvailable if plugin requests stream type StreamType_AsFile or StreamType_AsFileOnly
  if (mStreamType >= NP_ASFILE) {
    nsCOMPtr<nsIFile> localFile;
    if (mLocalCachedFileHolder)
      localFile = mLocalCachedFileHolder->file();
    else {
      // see if it is a file channel.
      nsCOMPtr<nsIFileChannel> fileChannel = do_QueryInterface(request);
      if (fileChannel) {
        fileChannel->GetFile(getter_AddRefs(localFile));
      }
    }

    if (localFile) {
      OnFileAvailable(localFile);
    }
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

    nsRefPtr<nsNPAPIPluginStreamListener> streamListener;
    rv = mPluginInstance->NewStreamListener(nullptr, nullptr,
                                            getter_AddRefs(streamListener));
    if (NS_FAILED(rv) || !streamListener) {
      return NS_ERROR_FAILURE;
    }

    mPStreamListener = static_cast<nsNPAPIPluginStreamListener*>(streamListener.get());
  }

  mPStreamListener->SetStreamListenerPeer(this);

  bool useLocalCache = false;

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
          ver = nsPrintfCString("/%lu.%lu", major, minor);
        }
      }

      // Status text: provide if available.  Defaults to "OK".
      nsCString statusText;
      if (NS_FAILED(httpChannel->GetResponseStatusText(statusText))) {
        statusText = "OK";
      }

      // Assemble everything and pass to listener.
      nsPrintfCString status("HTTP%s %lu %s", ver.get(), statusNum,
                             statusText.get());
      static_cast<nsIHTTPHeaderListener*>(mPStreamListener)->StatusLine(status.get());
    }

    // Also provide all HTTP response headers to our listener.
    httpChannel->VisitResponseHeaders(this);

    mSeekable = false;
    // first we look for a content-encoding header. If we find one, we tell the
    // plugin that stream is not seekable, because the plugin always sees
    // uncompressed data, so it can't make meaningful range requests on a
    // compressed entity.  Also, we force the plugin to use
    // nsPluginStreamType_AsFile stream type and we have to save decompressed
    // file into local plugin cache, because necko cache contains original
    // compressed file.
    nsAutoCString contentEncoding;
    if (NS_SUCCEEDED(httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("Content-Encoding"),
                                                    contentEncoding))) {
      useLocalCache = true;
    } else {
      // set seekability (seekable if the stream has a known length and if the
      // http server accepts byte ranges).
      uint32_t length;
      GetLength(&length);
      if (length) {
        nsAutoCString range;
        if (NS_SUCCEEDED(httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("accept-ranges"), range)) &&
            range.Equals(NS_LITERAL_CSTRING("bytes"), nsCaseInsensitiveCStringComparator())) {
          mSeekable = true;
        }
      }
    }

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

  rv = mPStreamListener->OnStartBinding(this);

  mStartBinding = true;

  if (NS_FAILED(rv))
    return rv;

  mPStreamListener->GetStreamType(&mStreamType);

  if (!useLocalCache && mStreamType >= NP_ASFILE) {
    // check it out if this is not a file channel.
    nsCOMPtr<nsIFileChannel> fileChannel = do_QueryInterface(request);
    if (!fileChannel) {
        useLocalCache = true;
    }
  }

  if (useLocalCache) {
    SetupPluginCacheFile(channel);
  }

  return NS_OK;
}

nsresult
nsPluginStreamListenerPeer::OnFileAvailable(nsIFile* aFile)
{
  nsresult rv;
  if (!mPStreamListener)
    return NS_ERROR_FAILURE;

  nsAutoCString path;
  rv = aFile->GetNativePath(path);
  if (NS_FAILED(rv)) return rv;

  if (path.IsEmpty()) {
    NS_WARNING("empty path");
    return NS_OK;
  }

  rv = mPStreamListener->OnFileAvailable(this, path.get());
  return rv;
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

  nsRefPtr<nsPluginInstanceOwner> owner = mPluginInstance->GetOwner();
  if (owner) {
    nsCOMPtr<nsIDocument> doc;
    nsresult rv = owner->GetDocument(getter_AddRefs(doc));
    if (NS_SUCCEEDED(rv) && doc) {
      nsPIDOMWindow *window = doc->GetWindow();
      if (window) {
        nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(window);
        nsCOMPtr<nsIInterfaceRequestor> ir = do_QueryInterface(webNav);
        return ir->GetInterface(aIID, result);
      }
    }
  }

  return NS_ERROR_FAILURE;
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

  NS_IMETHODIMP OnRedirectVerifyCallback(nsresult result)
  {
    if (NS_SUCCEEDED(result)) {
      nsCOMPtr<nsIStreamListener> listener = do_QueryReferent(mWeakListener);
      if (listener)
        static_cast<nsPluginStreamListenerPeer*>(listener.get())->ReplaceRequest(mOldChannel, mNewChannel);
    }
    return mParent->OnRedirectVerifyCallback(result);
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

  nsCOMPtr<nsIAsyncVerifyRedirectCallback> proxyCallback =
    new ChannelRedirectProxyCallback(this, callback, oldChannel, newChannel);

  // Give NPAPI a chance to control redirects.
  bool notificationHandled = mPStreamListener->HandleRedirectNotification(oldChannel, newChannel, proxyCallback);
  if (notificationHandled) {
    return NS_OK;
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

  // Fall back to channel event sink for window.
  nsCOMPtr<nsIChannelEventSink> channelEventSink;
  nsresult rv = GetInterfaceGlobal(NS_GET_IID(nsIChannelEventSink), getter_AddRefs(channelEventSink));
  if (NS_FAILED(rv)) {
    return rv;
  }

  return channelEventSink->AsyncOnChannelRedirect(oldChannel, newChannel, flags, proxyCallback);
}
