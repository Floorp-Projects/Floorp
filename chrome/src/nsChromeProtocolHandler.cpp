/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*

  A protocol handler for ``chrome:''

*/
 
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsChromeProtocolHandler.h"
#include "nsIChannel.h"
#include "nsIChromeRegistry.h"
#include "nsIComponentManager.h"
#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsIIOService.h"
#include "nsILoadGroup.h"
#include "nsIResChannel.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamListener.h"
#include "nsIServiceManager.h"
#include "nsIXULDocument.h"
#include "nsIXULPrototypeCache.h"
#include "nsIXULPrototypeDocument.h"
#include "nsRDFCID.h"
#include "nsXPIDLString.h"
#include "prlog.h"

//----------------------------------------------------------------------

static NS_DEFINE_CID(kChromeRegistryCID,         NS_CHROMEREGISTRY_CID);
static NS_DEFINE_CID(kEventQueueServiceCID,      NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);
static NS_DEFINE_CID(kStandardURLCID,            NS_STANDARDURL_CID);
static NS_DEFINE_CID(kXULDocumentCID,            NS_XULDOCUMENT_CID);
static NS_DEFINE_CID(kXULPrototypeCacheCID,      NS_XULPROTOTYPECACHE_CID);

//----------------------------------------------------------------------
//
//  A channel that's used for loading cached chrome documents. Since a
//  cached chrome document really doesn't have anything to do to load,
//  this is just the puppeteer that pulls the webshell's strings at the
//  right time.
//
//  Specifically, when AsyncOpen() is called, it adds the channel to
//  the load group, and queues an asychronous event to fire the
//  listener's OnStartRequest().
//
//  After triggering OnStartRequest(), it then queues another event
//  which will fire the listener's OnStopRequest() and remove the
//  channel from the load group.
//
//  Each is done asynchronously to allbow the stack to unwind back to
//  the main event loop. This avoids any weird re-entrancy that occurs
//  if we try to immediately fire the On[Start|Stop]Request().
//
//  For logging information, NSPR_LOG_MODULES=nsCachedChromeChannel:5
//

class nsCachedChromeChannel : public nsIChannel
{
protected:
    nsCachedChromeChannel(nsIURI* aURI);
    virtual ~nsCachedChromeChannel();

    nsCOMPtr<nsIURI>            mURI;
    nsCOMPtr<nsILoadGroup>      mLoadGroup;
    nsCOMPtr<nsIStreamListener> mListener;
    nsCOMPtr<nsISupports>       mContext;
    nsLoadFlags                 mLoadAttributes;
    nsCOMPtr<nsISupports>       mOwner;
    nsresult                    mStatus;

    struct LoadEvent {
        PLEvent                mEvent;
        nsCachedChromeChannel* mChannel;
    };

    static nsresult
    PostLoadEvent(nsCachedChromeChannel* aChannel, PLHandleEventProc aHandler);

    static void* PR_CALLBACK HandleStartLoadEvent(PLEvent* aEvent);
    static void* PR_CALLBACK HandleStopLoadEvent(PLEvent* aEvent);
    static void PR_CALLBACK DestroyLoadEvent(PLEvent* aEvent);

#ifdef PR_LOGGING
    static PRLogModuleInfo* gLog;
#endif

public:
    static nsresult
    Create(nsIURI* aURI, nsIChannel** aResult);
	
    NS_DECL_ISUPPORTS

    // nsIRequest
    NS_IMETHOD GetName(PRUnichar* *result) { 
        NS_NOTREACHED("nsCachedChromeChannel::GetName");
        return NS_ERROR_NOT_IMPLEMENTED;
    }
    NS_IMETHOD IsPending(PRBool *_retval) { *_retval = PR_TRUE; return NS_OK; }
    NS_IMETHOD GetStatus(nsresult *status) { *status = mStatus; return NS_OK; }
    NS_IMETHOD Cancel(nsresult status)  { mStatus = status; return NS_OK; }
    NS_IMETHOD Suspend(void) { return NS_OK; }
    NS_IMETHOD Resume(void)  { return NS_OK; }

    // nsIChannel    
    NS_DECL_NSICHANNEL
};

#ifdef PR_LOGGING
PRLogModuleInfo* nsCachedChromeChannel::gLog;
#endif

NS_IMPL_ADDREF(nsCachedChromeChannel);
NS_IMPL_RELEASE(nsCachedChromeChannel);
NS_IMPL_QUERY_INTERFACE2(nsCachedChromeChannel, nsIRequest, nsIChannel);

nsresult
nsCachedChromeChannel::Create(nsIURI* aURI, nsIChannel** aResult)
{
    NS_PRECONDITION(aURI != nsnull, "null ptr");
    if (! aURI)
        return NS_ERROR_NULL_POINTER;

    nsCachedChromeChannel* channel = new nsCachedChromeChannel(aURI);
    if (! channel)
        return NS_ERROR_OUT_OF_MEMORY;

    *aResult = channel;
    NS_ADDREF(*aResult);
    return NS_OK;
}


nsCachedChromeChannel::nsCachedChromeChannel(nsIURI* aURI)
    : mURI(aURI), mLoadGroup(nsnull), mLoadAttributes (nsIChannel::LOAD_NORMAL), mStatus(NS_OK)
{
    NS_INIT_REFCNT();

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("nsCachedChromeChannel");
#endif

    PR_LOG(gLog, PR_LOG_DEBUG,
           ("nsCachedChromeChannel[%p]: created", this));
}


nsCachedChromeChannel::~nsCachedChromeChannel()
{
    PR_LOG(gLog, PR_LOG_DEBUG,
           ("nsCachedChromeChannel[%p]: destroyed", this));
}


NS_IMETHODIMP
nsCachedChromeChannel::GetOriginalURI(nsIURI* *aOriginalURI)
{
    *aOriginalURI = mURI;
    NS_ADDREF(*aOriginalURI);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetOriginalURI(nsIURI* aOriginalURI)
{
  // don't stp on a uri if we already have one there...this is a work around fix
  // for Bug #34769.
  if (!mURI)
    mURI = aOriginalURI;
  return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetURI(nsIURI* *aURI)
{
    *aURI = mURI;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetURI(nsIURI* aURI)
{
    mURI = aURI;
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::OpenInputStream(nsIInputStream **_retval)
{
//    NS_NOTREACHED("don't do that");
    *_retval = nsnull;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::OpenOutputStream(nsIOutputStream **_retval)
{
    NS_NOTREACHED("don't do that");
    *_retval = nsnull;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::AsyncRead(nsIStreamListener *listener, nsISupports *ctxt)
{
    if (listener) {
        nsresult rv;

        if (mLoadGroup) {
            PR_LOG(gLog, PR_LOG_DEBUG,
                   ("nsCachedChromeChannel[%p]: adding self to load group %p",
                    this, mLoadGroup.get()));

            rv = mLoadGroup->AddChannel(this, nsnull);
            if (NS_FAILED(rv)) return rv;
        }

        // Fire the OnStartRequest(), which will cause the XUL
        // document to get embedded.
        PR_LOG(gLog, PR_LOG_DEBUG,
               ("nsCachedChromeChannel[%p]: firing OnStartRequest for %p",
                this, listener));

        // Queue an event to ourselves to let the stack unwind before
        // calling OnStartRequest(). This allows embedding to occur
        // before we fire OnStopRequest().
        rv = PostLoadEvent(this, HandleStartLoadEvent);
        if (NS_FAILED(rv)) {
            if (mLoadGroup) {
                PR_LOG(gLog, PR_LOG_DEBUG,
                       ("nsCachedChromeChannel[%p]: removing self from load group %p",
                        this, mLoadGroup.get()));

                (void) mLoadGroup->RemoveChannel(this, nsnull, nsnull, nsnull);
            }

            return rv;
        }

        mContext  = ctxt;
        mListener = listener;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::AsyncWrite(nsIInputStream *fromStream, nsIStreamObserver *observer, nsISupports *ctxt)
{
    NS_NOTREACHED("don't do that");
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetLoadAttributes(nsLoadFlags *aLoadAttributes)
{
    *aLoadAttributes = mLoadAttributes; 
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetLoadAttributes(nsLoadFlags aLoadAttributes)
{
    mLoadAttributes = aLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetContentType(char * *aContentType)
{
    *aContentType = nsXPIDLCString::Copy("text/cached-xul");
    return *aContentType ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetContentType(const char *aContentType)
{
    // Do not allow the content-type to be changed.
    NS_NOTREACHED("don't do that");
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetContentLength(PRInt32 *aContentLength)
{
    NS_NOTREACHED("don't do that");
    *aContentLength = 0;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetContentLength(PRInt32 aContentLength)
{
    NS_NOTREACHED("nsCachedChromeChannel::SetContentLength");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetTransferOffset(PRUint32 *aTransferOffset)
{
    NS_NOTREACHED("nsCachedChromeChannel::GetTransferOffset");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetTransferOffset(PRUint32 aTransferOffset)
{
    NS_NOTREACHED("nsCachedChromeChannel::SetTransferOffset");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetTransferCount(PRInt32 *aTransferCount)
{
    NS_NOTREACHED("nsCachedChromeChannel::GetTransferCount");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetTransferCount(PRInt32 aTransferCount)
{
    NS_NOTREACHED("nsCachedChromeChannel::SetTransferCount");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetBufferSegmentSize(PRUint32 *aBufferSegmentSize)
{
    NS_NOTREACHED("nsCachedChromeChannel::GetBufferSegmentSize");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetBufferSegmentSize(PRUint32 aBufferSegmentSize)
{
    NS_NOTREACHED("nsCachedChromeChannel::SetBufferSegmentSize");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetBufferMaxSize(PRUint32 *aBufferMaxSize)
{
    NS_NOTREACHED("nsCachedChromeChannel::GetBufferMaxSize");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetBufferMaxSize(PRUint32 aBufferMaxSize)
{
    NS_NOTREACHED("nsCachedChromeChannel::SetBufferMaxSize");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetLocalFile(nsIFile* *file)
{
    *file = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetPipeliningAllowed(PRBool *aPipeliningAllowed)
{
    *aPipeliningAllowed = PR_FALSE;
    return NS_OK;
}
 
NS_IMETHODIMP
nsCachedChromeChannel::SetPipeliningAllowed(PRBool aPipeliningAllowed)
{
    NS_NOTREACHED("SetPipeliningAllowed");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetOwner(nsISupports * *aOwner)
{
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetOwner(nsISupports * aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetLoadGroup(nsILoadGroup * aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetNotificationCallbacks(nsIInterfaceRequestor * *aNotificationCallbacks)
{
    NS_NOTREACHED("don't do that");
    *aNotificationCallbacks = nsnull;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetNotificationCallbacks(nsIInterfaceRequestor * aNotificationCallbacks)
{
    return NS_OK;	// ignored
}


NS_IMETHODIMP 
nsCachedChromeChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}

nsresult
nsCachedChromeChannel::PostLoadEvent(nsCachedChromeChannel* aChannel,
                                     PLHandleEventProc aHandler)
{
    nsresult rv;

    nsCOMPtr<nsIEventQueueService> svc = do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    if (! svc)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIEventQueue> queue;
    rv = svc->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
    if (NS_FAILED(rv)) return rv;

    if (! queue)
        return NS_ERROR_UNEXPECTED;

    LoadEvent* event = new LoadEvent;
    if (! event)
        return NS_ERROR_OUT_OF_MEMORY;

    PL_InitEvent(NS_REINTERPRET_CAST(PLEvent*, event),
                 nsnull,
                 aHandler,
                 DestroyLoadEvent);

    event->mChannel = aChannel;
    NS_ADDREF(event->mChannel);

    rv = queue->EnterMonitor();
    if (NS_SUCCEEDED(rv)) {
        (void) queue->PostEvent(NS_REINTERPRET_CAST(PLEvent*, event));
        (void) queue->ExitMonitor();
        return NS_OK;
    }

    // If we get here, something bad happened. Clean up.
    NS_RELEASE(event->mChannel);
    delete event;
    return rv;
}

void* PR_CALLBACK
nsCachedChromeChannel::HandleStartLoadEvent(PLEvent* aEvent)
{
    // Fire the OnStartRequest() for the cached chrome channel, then
    // queue another event to trigger the OnStopRequest()...
    LoadEvent* event = NS_REINTERPRET_CAST(LoadEvent*, aEvent);
    nsCachedChromeChannel* channel = event->mChannel;

    // If the load has been cancelled, then just bail now. We won't
    // send On[Start|Stop]Request().
    if (NS_FAILED(channel->mStatus))
      return nsnull;
    
    PR_LOG(gLog, PR_LOG_DEBUG,
              ("nsCachedChromeChannel[%p]: firing OnStartRequest for %p",
               channel, channel->mListener.get()));

    (void) channel->mListener->OnStartRequest(channel, channel->mContext);
    (void) PostLoadEvent(channel, HandleStopLoadEvent);
    return nsnull;
}


void* PR_CALLBACK
nsCachedChromeChannel::HandleStopLoadEvent(PLEvent* aEvent)
{
    // Fire the OnStopRequest() for the cached chrome channel, and
    // remove it from the load group.
    LoadEvent* event = NS_REINTERPRET_CAST(LoadEvent*, aEvent);
    nsCachedChromeChannel* channel = event->mChannel;

    PR_LOG(gLog, PR_LOG_DEBUG,
           ("nsCachedChromeChannel[%p]: firing OnStopRequest for %p",
            channel, channel->mListener.get()));

    (void) channel->mListener->OnStopRequest(channel, channel->mContext, 
                                             channel->mStatus, nsnull);

    if (channel->mLoadGroup) {
        PR_LOG(gLog, PR_LOG_DEBUG,
               ("nsCachedChromeChannel[%p]: removing self from load group %p",
                channel, channel->mLoadGroup.get()));

        (void) channel->mLoadGroup->RemoveChannel(channel, nsnull, nsnull, nsnull);
    }

    channel->mListener = nsnull;
    channel->mContext  = nsnull;

    return nsnull;
}

void PR_CALLBACK
nsCachedChromeChannel::DestroyLoadEvent(PLEvent* aEvent)
{
    LoadEvent* event = NS_REINTERPRET_CAST(LoadEvent*, aEvent);
    NS_RELEASE(event->mChannel);
    delete event;
}

////////////////////////////////////////////////////////////////////////////////

nsChromeProtocolHandler::nsChromeProtocolHandler()
{
    NS_INIT_REFCNT();
}

nsresult
nsChromeProtocolHandler::Init()
{
    return NS_OK;
}

nsChromeProtocolHandler::~nsChromeProtocolHandler()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsChromeProtocolHandler, nsIProtocolHandler, nsISupportsWeakReference);

NS_METHOD
nsChromeProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsChromeProtocolHandler* ph = new nsChromeProtocolHandler();
    if (ph == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(ph);
    nsresult rv = ph->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = ph->QueryInterface(aIID, aResult);
    }
    NS_RELEASE(ph);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsChromeProtocolHandler::GetScheme(char* *result)
{
    *result = nsCRT::strdup("chrome");
    if (*result == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsChromeProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        // no port for chrome: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsChromeProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                                nsIURI **result)
{
    nsresult rv;

    // Chrome: URLs (currently) have no additional structure beyond that provided by standard
    // URLs, so there is no "outer" given to CreateInstance 

    nsIURI* url;
    rv = nsComponentManager::CreateInstance(kStandardURLCID, nsnull,
                                            NS_GET_IID(nsIURI),
                                            (void**)&url);
    if (NS_FAILED(rv)) return rv;

    if (aBaseURI) {
        nsXPIDLCString aResolvedURI;
        rv = aBaseURI->Resolve(aSpec, getter_Copies(aResolvedURI));
        if (NS_FAILED(rv)) return rv;
        rv = url->SetSpec(aResolvedURI);
    }
    else {
        rv = url->SetSpec((char*)aSpec);
    }

    if (NS_FAILED(rv)) {
        NS_RELEASE(url);
        return rv;
    }

    *result = url;
    return rv;
}

NS_IMETHODIMP
nsChromeProtocolHandler::NewChannel(nsIURI* aURI,
                                    nsIChannel* *aResult)
{
    nsresult rv;
    nsCOMPtr<nsIChannel> result;

    // Canonify the "chrome:" URL; e.g., so that we collapse
    // "chrome://navigator/content/navigator.xul" and "chrome://navigator/content"
    // and "chrome://navigator/content/navigator.xul".
    NS_WITH_SERVICE(nsIChromeRegistry, reg, kChromeRegistryCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = reg->Canonify(aURI);
    if (NS_FAILED(rv)) return rv;

    // Check the prototype cache to see if we've already got the
    // document in the cache.
    NS_WITH_SERVICE(nsIXULPrototypeCache, cache, kXULPrototypeCacheCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIXULPrototypeDocument> proto;
    rv = cache->GetPrototype(aURI, getter_AddRefs(proto));
    if (NS_FAILED(rv)) return rv;

    if (proto) {
        // ...in which case, we'll create a dummy stream that'll just
        // load the thing.
        rv = nsCachedChromeChannel::Create(aURI, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;
    }
    else {
        // Miss. Resolve the chrome URL using the registry and do a
        // normal necko load.
        nsCOMPtr<nsIURI> chromeURI;
        rv = aURI->Clone(getter_AddRefs(chromeURI));        // don't mangle the original
        if (NS_FAILED(rv)) return rv;

        //nsXPIDLCString oldSpec;
        //aURI->GetSpec(getter_Copies(oldSpec));
        //printf("*************************** %s\n", (const char*)oldSpec);

        nsXPIDLCString spec;
        rv = reg->ConvertChromeURL(chromeURI, getter_Copies(spec));
        if (NS_FAILED(rv)) return rv;

        NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;
        
        rv = serv->NewURI(spec, nsnull, getter_AddRefs(chromeURI));
        if (NS_FAILED(rv)) return rv;
        
        rv = serv->NewChannelFromURI(chromeURI, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;

        // XXX Will be removed someday when we handle remote chrome.
        nsCOMPtr<nsIResChannel> res(do_QueryInterface(result));
        nsCOMPtr<nsIFileChannel> file(do_QueryInterface(result));
        if (!res && !file) {
          NS_WARNING("Remote chrome not allowed! Only file: and resource: are valid.\n");
          result = nsnull;
          return NS_ERROR_FAILURE;
        }
        
        // Make sure that the channel remembers where it was
        // originally loaded from.
        rv = result->SetOriginalURI(aURI);
        if (NS_FAILED(rv)) return rv;

        // Get a system principal for xul files and set the owner
        // property of the result
        nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);
        nsXPIDLCString fileExtension;
        rv = url->GetFileExtension(getter_Copies(fileExtension));
        if (PL_strcmp(fileExtension, "xul") == 0)
        {
            NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, 
                            NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
            if (NS_FAILED(rv)) return rv;
            
            nsCOMPtr<nsIPrincipal> principal;
            rv = securityManager->GetSystemPrincipal(getter_AddRefs(principal));
            if (NS_FAILED(rv)) return rv;
            
            nsCOMPtr<nsISupports> owner = do_QueryInterface(principal);
            result->SetOwner(owner);
        }
    }

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
