/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPluginStreamListenerPeer.h"
#include "nsIStreamConverterService.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIFileChannel.h"
#include "nsICachingChannel.h"
#include "nsMimeTypes.h"
#include "nsISupportsPrimitives.h"
#include "nsNetCID.h"
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
#include "sampler.h"

#define MAGIC_REQUEST_CONTEXT 0x01020304

// nsPluginByteRangeStreamListener

class nsPluginByteRangeStreamListener
  : public nsIStreamListener
  , public nsIInterfaceRequestor
{
public:
  nsPluginByteRangeStreamListener(nsIWeakReference* aWeakPtr);
  virtual ~nsPluginByteRangeStreamListener();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  
private:
  nsCOMPtr<nsIStreamListener> mStreamConverter;
  nsWeakPtr mWeakPtrPluginStreamListenerPeer;
  bool mRemoveMagicNumber;
};

NS_IMPL_ISUPPORTS3(nsPluginByteRangeStreamListener,
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
                                nsnull,
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
  
  PRUint32 responseCode = 0;
  rv = httpChannel->GetResponseStatus(&responseCode);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }
  
  if (responseCode != 200) {
    bool bWantsAllNetworkStreams = false;
    rv = pslp->GetPluginInstance()->GetValueFromPlugin(NPPVpluginWantsAllNetworkStreams,
                                                       &bWantsAllNetworkStreams);
    // If the call returned an error code make sure we still use our default value.
    if (NS_FAILED(rv)) {
      bWantsAllNetworkStreams = false;
    }

    if (!bWantsAllNetworkStreams){
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
      PRUint32 magicNumber = 0;
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
                                                 nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
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
    

// nsPRUintKey

class nsPRUintKey : public nsHashKey {
protected:
  PRUint32 mKey;
public:
  nsPRUintKey(PRUint32 key) : mKey(key) {}
  
  PRUint32 HashCode() const {
    return mKey;
  }
  
  bool Equals(const nsHashKey *aKey) const {
    return mKey == ((const nsPRUintKey*)aKey)->mKey;
  }
  nsHashKey *Clone() const {
    return new nsPRUintKey(mKey);
  }
  PRUint32 GetValue() { return mKey; }
};

// nsPluginStreamListenerPeer

NS_IMPL_ISUPPORTS8(nsPluginStreamListenerPeer,
                   nsIStreamListener,
                   nsIRequestObserver,
                   nsIHttpHeaderVisitor,
                   nsISupportsWeakReference,
                   nsIPluginStreamInfo,
                   nsINPAPIPluginStreamInfo,
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
  mDataForwardToRequest = nsnull;
  
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
    mPStreamListener->SetStreamListenerPeer(this);
  }

  // close FD of mFileCacheOutputStream if it's still open
  // or we won't be able to remove the cache file
  if (mFileCacheOutputStream)
    mFileCacheOutputStream = nsnull;
  
  delete mDataForwardToRequest;

  if (mPluginInstance)
    mPluginInstance->FileCachedStreamListeners()->RemoveElement(this);
}

// Called as a result of GetURL and PostURL
nsresult nsPluginStreamListenerPeer::Initialize(nsIURI *aURL,
                                                nsNPAPIPluginInstance *aInstance,
                                                nsNPAPIPluginStreamListener* aListener)
{
#ifdef PLUGIN_LOGGING
  nsCAutoString urlSpec;
  if (aURL != nsnull) aURL->GetAsciiSpec(urlSpec);
  
  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
         ("nsPluginStreamListenerPeer::Initialize instance=%p, url=%s\n", aInstance, urlSpec.get()));
  
  PR_LogFlush();
#endif
  
  mURL = aURL;
  
  mPluginInstance = aInstance;

  mPStreamListener = aListener;
  mPStreamListener->SetStreamListenerPeer(this);

  mPendingRequests = 1;
  
  mDataForwardToRequest = new nsHashtable(16, false);
  if (!mDataForwardToRequest)
    return NS_ERROR_FAILURE;
  
  return NS_OK;
}

nsresult nsPluginStreamListenerPeer::InitializeEmbedded(nsIURI *aURL,
                                                        nsNPAPIPluginInstance* aInstance,
                                                        nsObjectLoadingContent *aContent)
{
#ifdef PLUGIN_LOGGING
  nsCAutoString urlSpec;
  aURL->GetSpec(urlSpec);
  
  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NORMAL,
         ("nsPluginStreamListenerPeer::InitializeEmbedded url=%s\n", urlSpec.get()));
  
  PR_LogFlush();
#endif

  // We have to have one or the other.
  if (!aInstance && !aContent) {
    return NS_ERROR_FAILURE;
  }

  mURL = aURL;
  
  if (aInstance) {
    NS_ASSERTION(mPluginInstance == nsnull, "nsPluginStreamListenerPeer::InitializeEmbedded mPluginInstance != nsnull");
    mPluginInstance = aInstance;
  } else {
    mContent = aContent;
  }
  
  mPendingRequests = 1;
  
  mDataForwardToRequest = new nsHashtable(16, false);
  if (!mDataForwardToRequest)
    return NS_ERROR_FAILURE;
  
  return NS_OK;
}

// Called by NewFullPagePluginStream()
nsresult nsPluginStreamListenerPeer::InitializeFullPage(nsIURI* aURL, nsNPAPIPluginInstance *aInstance)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
             ("nsPluginStreamListenerPeer::InitializeFullPage instance=%p\n",aInstance));
  
  NS_ASSERTION(mPluginInstance == nsnull, "nsPluginStreamListenerPeer::InitializeFullPage mPluginInstance != nsnull");
  mPluginInstance = aInstance;
  
  mURL = aURL;
  
  mDataForwardToRequest = new nsHashtable(16, false);
  if (!mDataForwardToRequest)
    return NS_ERROR_FAILURE;

  mPendingRequests = 1;
  
  return NS_OK;
}

// SetupPluginCacheFile is called if we have to save the stream to disk.
// the most likely cause for this is either there is no disk cache available
// or the stream is coming from a https server.
//
// These files will be deleted when the host is destroyed.
//
// TODO? What if we fill up the the dest dir?
nsresult
nsPluginStreamListenerPeer::SetupPluginCacheFile(nsIChannel* channel)
{
  nsresult rv = NS_OK;
  
  bool useExistingCacheFile = false;
  nsRefPtr<nsPluginHost> pluginHost = dont_AddRef(nsPluginHost::GetInst());

  // Look for an existing cache file for the URI.
  nsTArray< nsRefPtr<nsNPAPIPluginInstance> > *instances = pluginHost->InstanceArray();
  for (PRUint32 i = 0; i < instances->Length(); i++) {
    // most recent streams are at the end of list
    nsTArray<nsPluginStreamListenerPeer*> *streamListeners = instances->ElementAt(i)->FileCachedStreamListeners();
    for (PRInt32 i = streamListeners->Length() - 1; i >= 0; --i) {
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
    
    nsCAutoString filename;
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
  SAMPLE_LABEL("nsPluginStreamListenerPeer", "OnStartRequest");

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
    PRUint32 responseCode = 0;
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
      bool bWantsAllNetworkStreams = false;

      // We don't always have an instance here already, but if we do, check
      // to see if it wants all streams.
      if (mPluginInstance) {
        rv = mPluginInstance->GetValueFromPlugin(NPPVpluginWantsAllNetworkStreams,
                                                 &bWantsAllNetworkStreams);
        // If the call returned an error code make sure we still use our default value.
        if (NS_FAILED(rv)) {
          bWantsAllNetworkStreams = false;
        }
      }

      if (!bWantsAllNetworkStreams) {
        mRequestFailed = true;
        return NS_ERROR_FAILURE;
      }
    }
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
  
  PRInt32 length;
  rv = channel->GetContentLength(&length);
  
  // it's possible for the server to not send a Content-Length.
  // we should still work in this case.
  if (NS_FAILED(rv) || length == -1) {
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
    mLength = length;
  }
  
  nsCAutoString aContentType; // XXX but we already got the type above!
  rv = channel->GetContentType(aContentType);
  if (NS_FAILED(rv))
    return rv;
  
  nsCOMPtr<nsIURI> aURL;
  rv = channel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv))
    return rv;
  
  aURL->GetSpec(mURLSpec);
  
  if (!aContentType.IsEmpty())
    mContentType = aContentType;
  
#ifdef PLUGIN_LOGGING
  PR_LOG(nsPluginLogging::gPluginLog, PLUGIN_LOG_NOISY,
         ("nsPluginStreamListenerPeer::OnStartRequest this=%p request=%p mime=%s, url=%s\n",
          this, request, aContentType.get(), mURLSpec.get()));
  
  PR_LogFlush();
#endif

  // If we don't have an instance yet it means we weren't able to load
  // a plugin previously because we didn't have the mimetype. Try again
  // if we have a mime type now.
  if (!mPluginInstance && mContent && !aContentType.IsEmpty()) {
    nsObjectLoadingContent *olc = static_cast<nsObjectLoadingContent*>(mContent.get());
    rv = olc->InstantiatePluginInstance(aContentType.get(), aURL.get());
    if (NS_SUCCEEDED(rv)) {
      rv = olc->GetPluginInstance(getter_AddRefs(mPluginInstance));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  // Set up the stream listener...
  rv = SetUpStreamListener(request, aURL);
  if (NS_FAILED(rv)) return rv;
  
  return rv;
}

NS_IMETHODIMP nsPluginStreamListenerPeer::OnProgress(nsIRequest *request,
                                                     nsISupports* aContext,
                                                     PRUint64 aProgress,
                                                     PRUint64 aProgressMax)
{
  nsresult rv = NS_OK;
  return rv;
}

NS_IMETHODIMP nsPluginStreamListenerPeer::OnStatus(nsIRequest *request,
                                                   nsISupports* aContext,
                                                   nsresult aStatus,
                                                   const PRUnichar* aStatusArg)
{
  return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamListenerPeer::GetContentType(char** result)
{
  *result = const_cast<char*>(mContentType.get());
  return NS_OK;
}


NS_IMETHODIMP
nsPluginStreamListenerPeer::IsSeekable(bool* result)
{
  *result = mSeekable;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamListenerPeer::GetLength(PRUint32* result)
{
  *result = mLength;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamListenerPeer::GetLastModified(PRUint32* result)
{
  *result = mModified;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamListenerPeer::GetURL(const char** result)
{
  *result = mURLSpec.get();
  return NS_OK;
}

void
nsPluginStreamListenerPeer::MakeByteRangeString(NPByteRange* aRangeList, nsACString &rangeRequest,
                                                PRInt32 *numRequests)
{
  rangeRequest.Truncate();
  *numRequests  = 0;
  //the string should look like this: bytes=500-700,601-999
  if (!aRangeList)
    return;
  
  PRInt32 requestCnt = 0;
  nsCAutoString string("bytes=");
  
  for (NPByteRange * range = aRangeList; range != nsnull; range = range->next) {
    // XXX zero length?
    if (!range->length)
      continue;
    
    // XXX needs to be fixed for negative offsets
    string.AppendInt(range->offset);
    string.Append("-");
    string.AppendInt(range->offset + range->length - 1);
    if (range->next)
      string += ",";
    
    requestCnt++;
  }
  
  // get rid of possible trailing comma
  string.Trim(",", false);
  
  rangeRequest = string;
  *numRequests  = requestCnt;
  return;
}

NS_IMETHODIMP
nsPluginStreamListenerPeer::RequestRead(NPByteRange* rangeList)
{
  nsCAutoString rangeString;
  PRInt32 numRequests;
  
  MakeByteRangeString(rangeList, rangeString, &numRequests);
  
  if (numRequests == 0)
    return NS_ERROR_FAILURE;
  
  nsresult rv = NS_OK;
  
  nsCOMPtr<nsIInterfaceRequestor> callbacks = do_QueryReferent(mWeakPtrChannelCallbacks);
  nsCOMPtr<nsILoadGroup> loadGroup = do_QueryReferent(mWeakPtrChannelLoadGroup);
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), mURL, nsnull, loadGroup, callbacks);
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

NS_IMETHODIMP
nsPluginStreamListenerPeer::GetStreamOffset(PRInt32* result)
{
  *result = mStreamOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamListenerPeer::SetStreamOffset(PRInt32 value)
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
  nsCOMPtr<nsIPluginInstanceOwner> owner;
  mPluginInstance->GetOwner(getter_AddRefs(owner));
  if (owner) {
    NPWindow* window = nsnull;
    owner->GetWindow(window);
#if defined(MOZ_WIDGET_GTK2) || defined(MOZ_WIDGET_QT)
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
  
  // then check it out if browser cache is not available
  nsCOMPtr<nsICachingChannel> cacheChannel = do_QueryInterface(request);
  if (!(cacheChannel && (NS_SUCCEEDED(cacheChannel->SetCacheAsFile(true))))) {
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    if (channel) {
      SetupPluginCacheFile(channel);
    }
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
                                                          PRUint32 sourceOffset,
                                                          PRUint32 aLength)
{
  NS_ASSERTION(mRequests.IndexOfObject(GetBaseRequest(request)) != -1,
               "Received OnDataAvailable for untracked request.");
  
  if (mRequestFailed)
    return NS_ERROR_FAILURE;
  
  if (mAbort) {
    PRUint32 magicNumber = 0;  // set it to something that is not the magic number.
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
  
  const char * url = nsnull;
  GetURL(&url);
  
  PLUGIN_LOG(PLUGIN_LOG_NOISY,
             ("nsPluginStreamListenerPeer::OnDataAvailable this=%p request=%p, offset=%d, length=%d, url=%s\n",
              this, request, sourceOffset, aLength, url ? url : "no url set"));
  
  // if the plugin has requested an AsFileOnly stream, then don't
  // call OnDataAvailable
  if (mStreamType != NP_ASFILEONLY) {
    // get the absolute offset of the request, if one exists.
    nsCOMPtr<nsIByteRangeRequest> brr = do_QueryInterface(request);
    if (brr) {
      if (!mDataForwardToRequest)
        return NS_ERROR_FAILURE;
      
      PRInt64 absoluteOffset64 = LL_ZERO;
      brr->GetStartRange(&absoluteOffset64);
      
      // XXX handle 64-bit for real
      PRInt32 absoluteOffset = (PRInt32)PRInt64(absoluteOffset64);
      
      // we need to track how much data we have forwarded to the
      // plugin.
      
      // FIXME: http://bugzilla.mozilla.org/show_bug.cgi?id=240130
      //
      // Why couldn't this be tracked on the plugin info, and not in a
      // *hash table*?
      nsPRUintKey key(absoluteOffset);
      PRInt32 amtForwardToPlugin =
      NS_PTR_TO_INT32(mDataForwardToRequest->Get(&key));
      mDataForwardToRequest->Put(&key, NS_INT32_TO_PTR(amtForwardToPlugin + aLength));
      
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
    PRUint32 amountRead, amountWrote = 0;
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
    PRInt64 absoluteOffset64 = LL_ZERO;
    brr->GetStartRange(&absoluteOffset64);
    // XXX support 64-bit offsets
    PRInt32 absoluteOffset = (PRInt32)PRInt64(absoluteOffset64);
    
    nsPRUintKey key(absoluteOffset);
    
    // remove the request from our data forwarding count hash.
    mDataForwardToRequest->Remove(&key);
    
    
    PLUGIN_LOG(PLUGIN_LOG_NOISY,
               ("                          ::OnStopRequest for ByteRangeRequest Started=%d\n",
                absoluteOffset));
  } else {
    // if this is not byte range request and
    // if we are writting the stream to disk ourselves,
    // close & tear it down here
    mFileCacheOutputStream = nsnull;
  }
  
  // if we still have pending stuff to do, lets not close the plugin socket.
  if (--mPendingRequests > 0)
    return NS_OK;
  
  // we keep our connections around...
  nsCOMPtr<nsISupportsPRUint32> container = do_QueryInterface(aContext);
  if (container) {
    PRUint32 magicNumber = 0;  // set it to something that is not the magic number.
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
  nsCAutoString aContentType;
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
      nsCOMPtr<nsICachingChannel> cacheChannel = do_QueryInterface(request);
      if (cacheChannel) {
        cacheChannel->GetCacheFile(getter_AddRefs(localFile));
      } else {
        // see if it is a file channel.
        nsCOMPtr<nsIFileChannel> fileChannel = do_QueryInterface(request);
        if (fileChannel) {
          fileChannel->GetFile(getter_AddRefs(localFile));
        }
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
    rv = mPluginInstance->NewStreamListener(nsnull, nsnull,
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
    PRUint32 statusNum;
    if (NS_SUCCEEDED(httpChannel->GetResponseStatus(&statusNum)) &&
        statusNum < 1000) {
      // HTTP version: provide if available.  Defaults to empty string.
      nsCString ver;
      nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal =
      do_QueryInterface(channel);
      if (httpChannelInternal) {
        PRUint32 major, minor;
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
    nsCAutoString contentEncoding;
    if (NS_SUCCEEDED(httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("Content-Encoding"),
                                                    contentEncoding))) {
      useLocalCache = true;
    } else {
      // set seekability (seekable if the stream has a known length and if the
      // http server accepts byte ranges).
      PRUint32 length;
      GetLength(&length);
      if (length) {
        nsCAutoString range;
        if (NS_SUCCEEDED(httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("accept-ranges"), range)) &&
            range.Equals(NS_LITERAL_CSTRING("bytes"), nsCaseInsensitiveCStringComparator())) {
          mSeekable = true;
        }
      }
    }
    
    // we require a content len
    // get Last-Modified header for plugin info
    nsCAutoString lastModified;
    if (NS_SUCCEEDED(httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("last-modified"), lastModified)) &&
        !lastModified.IsEmpty()) {
      PRTime time64;
      PR_ParseTimeString(lastModified.get(), true, &time64);  //convert string time to integer time
      
      // Convert PRTime to unix-style time_t, i.e. seconds since the epoch
      double fpTime;
      LL_L2D(fpTime, time64);
      mModified = (PRUint32)(fpTime * 1e-6 + 0.5);
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
      // and browser cache is not available
      nsCOMPtr<nsICachingChannel> cacheChannel = do_QueryInterface(request);
      if (!(cacheChannel && (NS_SUCCEEDED(cacheChannel->SetCacheAsFile(true))))) {
        useLocalCache = true;
      }
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
  
  nsCAutoString path;
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

  nsCOMPtr<nsIPluginInstanceOwner> owner;
  mPluginInstance->GetOwner(getter_AddRefs(owner));
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
  ChannelRedirectProxyCallback(nsINPAPIPluginStreamInfo* listener,
                               nsIAsyncVerifyRedirectCallback* parent,
                               nsIChannel* oldChannel,
                               nsIChannel* newChannel)
    : mWeakListener(do_GetWeakReference(listener))
    , mParent(parent)
    , mOldChannel(oldChannel)
    , mNewChannel(newChannel)
  {
  }

  ChannelRedirectProxyCallback() {}
  virtual ~ChannelRedirectProxyCallback() {}

  NS_DECL_ISUPPORTS

  NS_IMETHODIMP OnRedirectVerifyCallback(nsresult result)
  {
    if (NS_SUCCEEDED(result)) {
      nsCOMPtr<nsINPAPIPluginStreamInfo> listener = do_QueryReferent(mWeakListener);
      if (listener)
        listener->ReplaceRequest(mOldChannel, mNewChannel);
    }
    return mParent->OnRedirectVerifyCallback(result);
  }

private:
  nsWeakPtr mWeakListener;
  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mParent;
  nsCOMPtr<nsIChannel> mOldChannel;
  nsCOMPtr<nsIChannel> mNewChannel;
};

NS_IMPL_ISUPPORTS1(ChannelRedirectProxyCallback, nsIAsyncVerifyRedirectCallback)
    

NS_IMETHODIMP
nsPluginStreamListenerPeer::AsyncOnChannelRedirect(nsIChannel *oldChannel, nsIChannel *newChannel,
                                                   PRUint32 flags, nsIAsyncVerifyRedirectCallback* callback)
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
    PRUint32 responseStatus;
    nsresult rv = oldHttpChannel->GetResponseStatus(&responseStatus);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (responseStatus == 307) {
      nsCAutoString method;
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
