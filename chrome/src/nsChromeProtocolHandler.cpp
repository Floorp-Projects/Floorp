/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*

  A protocol handler for ``chrome:''

*/

#include "nsAutoPtr.h"
#include "nsChromeProtocolHandler.h"
#include "nsChromeRegistry.h"
#include "nsCOMPtr.h"
#include "nsContentCID.h"
#include "nsCRT.h"
#include "nsThreadUtils.h"
#include "nsIChannel.h"
#include "nsIChromeRegistry.h"
#include "nsIComponentManager.h"
#include "nsIFastLoadService.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsIFileChannel.h"
#include "nsIIOService.h"
#include "nsIJARChannel.h"
#include "nsIJARURI.h"
#include "nsILoadGroup.h"
#include "nsIObjectOutputStream.h"
#include "nsIScriptSecurityManager.h"
#include "nsIServiceManager.h"
#include "nsIStandardURL.h"
#include "nsIStreamListener.h"
#include "nsNetUtil.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "prlog.h"

#ifdef MOZ_XUL
#include "nsIXULPrototypeCache.h"
#endif

//----------------------------------------------------------------------

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
//  Each is done asynchronously to allow the stack to unwind back to
//  the main event loop. This avoids any weird re-entrancy that occurs
//  if we try to immediately fire the On[Start|Stop]Request().
//
//  For logging information, NSPR_LOG_MODULES=nsCachedChromeChannel:5
//
#define LOG(args) PR_LOG(gLog, PR_LOG_DEBUG, args)

#define NS_CACHEDCHROMECHANNEL_IMPL_IID \
{ 0x281371d3, 0x6bc2, 0x499f, \
  { 0x8d, 0x70, 0xcb, 0xfc, 0x01, 0x1b, 0xa0, 0x43 } }

class nsCachedChromeChannel : public nsIChannel
{
protected:
    ~nsCachedChromeChannel();

    nsCOMPtr<nsIURI>            mURI;
    nsCOMPtr<nsIURI>            mOriginalURI;
    nsCOMPtr<nsILoadGroup>      mLoadGroup;
    nsCOMPtr<nsIStreamListener> mListener;
    nsCOMPtr<nsISupports>       mContext;
    nsLoadFlags                 mLoadFlags;
    nsCOMPtr<nsISupports>       mOwner;
    nsresult                    mStatus;

#ifdef PR_LOGGING
    static PRLogModuleInfo* gLog;
#endif

    void HandleLoadEvent();

public:
    nsCachedChromeChannel(nsIURI* aURI);

    NS_DECLARE_STATIC_IID_ACCESSOR(NS_CACHEDCHROMECHANNEL_IMPL_IID)

    NS_DECL_ISUPPORTS

    // nsIRequest
    NS_IMETHOD GetName(nsACString &result) { return mURI->GetSpec(result); }
    NS_IMETHOD IsPending(PRBool *_retval) { *_retval = (mListener != nsnull); return NS_OK; }
    NS_IMETHOD GetStatus(nsresult *status) { *status = mStatus; return NS_OK; }
    NS_IMETHOD Cancel(nsresult status)  { mStatus = status; return NS_OK; }
    NS_IMETHOD Suspend(void) { return NS_OK; } // XXX technically wrong
    NS_IMETHOD Resume(void)  { return NS_OK; } // XXX technically wrong
    NS_IMETHOD GetLoadGroup(nsILoadGroup **);
    NS_IMETHOD SetLoadGroup(nsILoadGroup *);
    NS_IMETHOD GetLoadFlags(nsLoadFlags *);
    NS_IMETHOD SetLoadFlags(nsLoadFlags);

    // nsIChannel
    NS_DECL_NSICHANNEL
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsCachedChromeChannel,
                              NS_CACHEDCHROMECHANNEL_IMPL_IID)

#ifdef PR_LOGGING
PRLogModuleInfo* nsCachedChromeChannel::gLog;
#endif

NS_IMPL_ISUPPORTS3(nsCachedChromeChannel, nsIChannel, nsIRequest,
                   nsCachedChromeChannel)

nsCachedChromeChannel::nsCachedChromeChannel(nsIURI* aURI)
    : mURI(aURI)
    , mOriginalURI(aURI)
    , mLoadFlags(nsIRequest::LOAD_NORMAL)
    , mStatus(NS_OK)
{
#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("nsCachedChromeChannel");
#endif

    LOG(("nsCachedChromeChannel[%p]: created", this));
}


nsCachedChromeChannel::~nsCachedChromeChannel()
{
    LOG(("nsCachedChromeChannel[%p]: destroyed", this));
}


NS_IMETHODIMP
nsCachedChromeChannel::GetOriginalURI(nsIURI* *aOriginalURI)
{
    *aOriginalURI = mOriginalURI;
    NS_ADDREF(*aOriginalURI);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetOriginalURI(nsIURI* aOriginalURI)
{
    NS_ENSURE_ARG_POINTER(aOriginalURI);
    mOriginalURI = aOriginalURI;
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
nsCachedChromeChannel::Open(nsIInputStream **_retval)
{
//    NS_NOTREACHED("don't do that");
    *_retval = nsnull;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt)
{
    NS_ENSURE_ARG_POINTER(listener);

    nsresult rv;

    // Fire OnStartRequest and OnStopRequest, which will cause the XUL
    // document to get embedded.
    LOG(("nsCachedChromeChannel[%p]: posting load event for %p",
        this, listener));

    nsCOMPtr<nsIRunnable> event =
        NS_NEW_RUNNABLE_METHOD(nsCachedChromeChannel, this, HandleLoadEvent);

    // Queue an event to ourselves to let the stack unwind before
    // calling OnStartRequest(). This allows embedding to occur
    // before we fire OnStopRequest().
    rv = NS_DispatchToCurrentThread(event);
    if (NS_FAILED(rv))
        return rv;

    mContext  = ctxt;
    mListener = listener;

    if (mLoadGroup) {
        LOG(("nsCachedChromeChannel[%p]: adding self to load group %p",
            this, mLoadGroup.get()));

        (void) mLoadGroup->AddRequest(this, nsnull);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    *aLoadFlags = mLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
    mLoadFlags = aLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetOwner(nsISupports * *aOwner)
{
    *aOwner = mOwner;
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
    *aNotificationCallbacks = nsnull;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetNotificationCallbacks(nsIInterfaceRequestor * aNotificationCallbacks)
{
    return NS_OK;	// ignored
}

NS_IMETHODIMP
nsCachedChromeChannel::GetContentType(nsACString &aContentType)
{
    aContentType.AssignLiteral("mozilla.application/cached-xul");
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetContentType(const nsACString &aContentType)
{
    // Do not allow the content-type to be changed.
    NS_NOTREACHED("don't do that");
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetContentCharset(nsACString &aContentCharset)
{
    aContentCharset.Truncate();
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetContentCharset(const nsACString &aContentCharset)
{
    // Do not allow the content charset to be changed.
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

void
nsCachedChromeChannel::HandleLoadEvent()
{
    // Fire the OnStartRequest() for the cached chrome channel, then
    // trigger the OnStopRequest()...

    // If the load has been cancelled, then just bail now. We won't
    // send On[Start|Stop]Request().
    if (NS_FAILED(mStatus))
        return;

    LOG(("nsCachedChromeChannel[%p]: firing OnStartRequest for %p",
        this, mListener.get()));

    mListener->OnStartRequest(this, mContext);

    LOG(("nsCachedChromeChannel[%p]: firing OnStopRequest for %p",
        this, mListener.get()));

    mListener->OnStopRequest(this, mContext, mStatus);

    if (mLoadGroup) {
        LOG(("nsCachedChromeChannel[%p]: removing self from load group %p",
            this, mLoadGroup.get()));
        mLoadGroup->RemoveRequest(this, nsnull, mStatus);
    }

    mListener = nsnull;
    mContext  = nsnull;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_THREADSAFE_ISUPPORTS2(nsChromeProtocolHandler,
                              nsIProtocolHandler,
                              nsISupportsWeakReference)

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsChromeProtocolHandler::GetScheme(nsACString &result)
{
    result.AssignLiteral("chrome");
    return NS_OK;
}

NS_IMETHODIMP
nsChromeProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        // no port for chrome: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsChromeProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.
    *_retval = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsChromeProtocolHandler::GetProtocolFlags(PRUint32 *result)
{
    *result = URI_STD | URI_IS_UI_RESOURCE;
    return NS_OK;
}

NS_IMETHODIMP
nsChromeProtocolHandler::NewURI(const nsACString &aSpec,
                                const char *aCharset,
                                nsIURI *aBaseURI,
                                nsIURI **result)
{
    nsresult rv;

    // Chrome: URLs (currently) have no additional structure beyond that provided
    // by standard URLs, so there is no "outer" given to CreateInstance

    nsCOMPtr<nsIStandardURL> surl(do_CreateInstance(NS_STANDARDURL_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = surl->Init(nsIStandardURL::URLTYPE_STANDARD, -1, aSpec, aCharset, aBaseURI);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIURL> url(do_QueryInterface(surl, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    // Canonify the "chrome:" URL; e.g., so that we collapse
    // "chrome://navigator/content/" and "chrome://navigator/content"
    // and "chrome://navigator/content/navigator.xul".

    rv = nsChromeRegistry::Canonify(url);
    if (NS_FAILED(rv))
        return rv;

    surl->SetMutable(PR_FALSE);

    NS_ADDREF(*result = url);
    return NS_OK;
}

NS_IMETHODIMP
nsChromeProtocolHandler::NewChannel(nsIURI* aURI,
                                    nsIChannel* *aResult)
{
    nsresult rv;

    NS_ENSURE_ARG_POINTER(aURI);
    NS_PRECONDITION(aResult, "Null out param");
    
#ifdef DEBUG
    // Check that the uri we got is already canonified
    nsresult debug_rv;
    nsCOMPtr<nsIURI> debugClone;
    debug_rv = aURI->Clone(getter_AddRefs(debugClone));
    if (NS_SUCCEEDED(debug_rv)) {
        nsCOMPtr<nsIURL> debugURL (do_QueryInterface(debugClone));
        debug_rv = nsChromeRegistry::Canonify(debugURL);
        if (NS_SUCCEEDED(debug_rv)) {
            PRBool same;
            debug_rv = aURI->Equals(debugURL, &same);
            if (NS_SUCCEEDED(debug_rv)) {
                NS_ASSERTION(same, "Non-canonified chrome uri passed to nsChromeProtocolHandler::NewChannel!");
            }
        }
    }
#endif

    nsCOMPtr<nsIChannel> result;

#ifdef MOZ_XUL
    // Check the prototype cache to see if we've already got the
    // document in the cache.
    nsCOMPtr<nsIXULPrototypeCache> cache
        (do_GetService(kXULPrototypeCacheCID));

    PRBool isCached = PR_FALSE;
    if (cache)
        isCached = cache->IsCached(aURI);
    else
        NS_WARNING("Unable to obtain the XUL prototype cache!");

    // Same comment as nsXULDocument::StartDocumentLoad and
    // nsXULDocument::ResumeWalk
    // - Ben Goodger
    //
    // We don't abort on failure here because there are too many valid
    // cases that can return failure, and the null-ness of |proto| is enough
    // to trigger the fail-safe parse-from-disk solution. Example failure cases
    // (for reference) include:
    //
    // NS_ERROR_NOT_AVAILABLE: the URI cannot be found in the FastLoad cache, 
    //                         parse from disk
    // other: the FastLoad cache file, XUL.mfl, could not be found, probably
    //        due to being accessed before a profile has been selected (e.g.
    //        loading chrome for the profile manager itself). This must be 
    //        parsed from disk. 

    if (isCached) {
        // ...in which case, we'll create a dummy stream that'll just
        // load the thing.
        result = new nsCachedChromeChannel(aURI);
        if (! result)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    else {
#endif // MOZ_XUL
        // Miss. Resolve the chrome URL using the registry and do a
        // normal necko load.
        //nsXPIDLCString oldSpec;
        //aURI->GetSpec(getter_Copies(oldSpec));
        //printf("*************************** %s\n", (const char*)oldSpec);

        if (!nsChromeRegistry::gChromeRegistry) {
            // We don't actually want this ref, we just want the service to
            // initialize if it hasn't already.
            nsCOMPtr<nsIChromeRegistry> reg (do_GetService(NS_CHROMEREGISTRY_CONTRACTID));
        }

        NS_ENSURE_TRUE(nsChromeRegistry::gChromeRegistry, NS_ERROR_FAILURE);

        nsCOMPtr<nsIURI> resolvedURI;
        rv = nsChromeRegistry::gChromeRegistry->ConvertChromeURL(aURI, getter_AddRefs(resolvedURI));
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            nsCAutoString spec;
            aURI->GetSpec(spec);
            printf("Couldn't convert chrome URL: %s\n", spec.get());
#endif
            return rv;
        }

        nsCOMPtr<nsIIOService> ioServ (do_GetIOService(&rv));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = ioServ->NewChannelFromURI(resolvedURI, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;

        // XXX Will be removed someday when we handle remote chrome.
        nsCOMPtr<nsIFileChannel> fileChan
            (do_QueryInterface(result));
        if (fileChan) {
#ifdef DEBUG
            nsCOMPtr<nsIFile> file;
            fileChan->GetFile(getter_AddRefs(file));

            PRBool exists = PR_FALSE;
            file->Exists(&exists);
            if (!exists) {
                nsCAutoString path;
                file->GetNativePath(path);
                printf("Chrome file doesn't exist: %s\n", path.get());
            }
#endif
        }
        else {
            nsCOMPtr<nsIJARChannel> jarChan
                (do_QueryInterface(result));
            if (!jarChan) {
                nsRefPtr<nsCachedChromeChannel> cachedChannel;
                if (NS_FAILED(CallQueryInterface(result.get(),
                        static_cast<nsCachedChromeChannel**>(
                            getter_AddRefs(cachedChannel))))) {
                    NS_WARNING("Remote chrome not allowed! Only file:, resource:, jar:, and cached chrome channels are valid.\n");
                    result = nsnull;
                    return NS_ERROR_FAILURE;
                }
            }
        }

        // Make sure that the channel remembers where it was
        // originally loaded from.
        rv = result->SetOriginalURI(aURI);
        if (NS_FAILED(rv)) return rv;

        // Get a system principal for content files and set the owner
        // property of the result
        nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);
        nsCAutoString path;
        rv = url->GetPath(path);
        if (StringBeginsWith(path, NS_LITERAL_CSTRING("/content/")))
        {
            nsCOMPtr<nsIScriptSecurityManager> securityManager =
                     do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIPrincipal> principal;
            rv = securityManager->GetSystemPrincipal(getter_AddRefs(principal));
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsISupports> owner = do_QueryInterface(principal);
            result->SetOwner(owner);
        }

#ifdef MOZ_XUL
        // Track FastLoad file dependencies.
        //
        // This is harder than it ought to be!  While an nsResChannel "is-a"
        // nsIFileChannel, an nsJARChannel is not.  Once you unravel the jar:
        // URI, you may have a resource: URL -- but without a channel for it,
        // you can't get the URI that it yields through substitution!
        //
        // XXXbe fix nsResChannel.cpp to move the substitution code into a new
        //       nsResURL class?
        nsCOMPtr<nsIFastLoadService> fastLoadServ(do_GetFastLoadService());
        if (fastLoadServ) {
            nsCOMPtr<nsIObjectOutputStream> objectOutput;
            fastLoadServ->GetOutputStream(getter_AddRefs(objectOutput));
            if (objectOutput) {
                nsCOMPtr<nsIFile> file;

                if (fileChan) {
                    fileChan->GetFile(getter_AddRefs(file));
                } else {
                    nsCOMPtr<nsIURI> uri;
                    result->GetURI(getter_AddRefs(uri));

                    // Loop, jar URIs can nest (e.g. jar:jar:A.jar!B.jar!C.xml).
                    // Often, however, we have jar:resource:/chrome/A.jar!C.xml.
                    nsCOMPtr<nsIJARURI> jarURI;
                    while ((jarURI = do_QueryInterface(uri)) != nsnull)
                        jarURI->GetJARFile(getter_AddRefs(uri));

                    // Here we have a URL of the form resource:/chrome/A.jar
                    // or file:/some/path/to/A.jar.
                    nsCOMPtr<nsIFileURL> fileURL(do_QueryInterface(uri));
                    if (fileURL)
                        fileURL->GetFile(getter_AddRefs(file));
                }

                if (file) {
                    rv = fastLoadServ->AddDependency(file);
                    if (NS_FAILED(rv))
                        cache->AbortFastLoads();
                }
            }
        }
    }
#endif

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
