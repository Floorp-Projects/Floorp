/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Contributor(s): Radha Kulkarni(radha@netscape.com)
 */

#include "nsWyciwygChannel.h"
#include "nsIServiceManager.h"
#include "nsILoadGroup.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"
#include "nsICacheService.h"
#include "nsICacheSession.h"


PRLogModuleInfo * gWyciwygLog = nsnull;

#define wyciwyg_TYPE "text/html"

// nsWyciwygChannel methods 
nsWyciwygChannel::nsWyciwygChannel()
    : mStatus(NS_OK),
      mLoadFlags(LOAD_NORMAL),
      mIsPending(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
}

nsWyciwygChannel::~nsWyciwygChannel() 
{
}

NS_IMPL_THREADSAFE_ISUPPORTS8(nsWyciwygChannel, nsIChannel, nsIRequest,
                              nsIStreamListener, nsICacheListener, 
                              nsIInterfaceRequestor, nsIWyciwygChannel,
                              nsIRequestObserver, nsIProgressEventSink)

nsresult
nsWyciwygChannel::Init(nsIURI* uri)
{
  if (!uri)
    return NS_ERROR_NULL_POINTER;
  mURI = uri;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsWyciwygChannel::GetInterface(const nsIID &aIID, void **aResult)
{

  if (aIID.Equals(NS_GET_IID(nsIProgressEventSink))) {
    //
    // we return ourselves as the progress event sink so we can intercept
    // notifications and set the correct request and context parameters.
    // but, if we don't have a progress sink to forward those messages
    // to, then there's no point in handing out a reference to ourselves.
    //
    if (!mProgressSink)
      return NS_ERROR_NO_INTERFACE;

    return QueryInterface(aIID, aResult);
  }

  if (mCallbacks)
    return mCallbacks->GetInterface(aIID, aResult);
  
  return NS_ERROR_NO_INTERFACE;
}

///////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:
///////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsWyciwygChannel::GetName(nsACString &aName)
{
  return mURI->GetSpec(aName);
}
 
NS_IMETHODIMP
nsWyciwygChannel::IsPending(PRBool *aIsPending)
{
  *aIsPending = mIsPending;
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::GetStatus(nsresult *aStatus)
{
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::Cancel(nsresult aStatus)
{
  LOG(("nsWyciwygChannel::Cancel [this=%x status=%x]\n", this, aStatus));
  NS_ASSERTION(NS_FAILED(aStatus), "shouldn't cancel with a success code");
  
  mStatus = aStatus;
  if (mCacheReadRequest)
    mCacheReadRequest->Cancel(aStatus);
  // Clear out all cache handles.
  CloseCacheEntry();
  return NS_OK;
}
 
NS_IMETHODIMP
nsWyciwygChannel::Suspend(void)
{
  LOG(("nsWyciwygChannel::Suspend [this=%x]\n", this));
  if (mCacheReadRequest)
    return mCacheReadRequest->Suspend();
  return NS_OK;
}
 
NS_IMETHODIMP
nsWyciwygChannel::Resume(void)
{
  LOG(("nsWyciwygChannel::Resume [this=%x]\n", this));
  if (mCacheReadRequest)
    return mCacheReadRequest->Resume();
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
  NS_ENSURE_ARG_POINTER(aLoadGroup);
  *aLoadGroup = mLoadGroup;
  NS_IF_ADDREF(*aLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::SetLoadFlags(PRUint32 aLoadFlags)
{
  mLoadFlags = aLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::GetLoadFlags(PRUint32 * aLoadFlags)
{
  NS_ENSURE_ARG_POINTER(aLoadFlags);
  *aLoadFlags = mLoadFlags;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:
///////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsWyciwygChannel::GetOriginalURI(nsIURI* *aURI)
{
  // Let's hope this isn't called before mOriginalURI is set or we will
  // return the full wyciwyg URI for our originalURI  :S
  NS_ASSERTION(mOriginalURI, "nsWyciwygChannel::GetOriginalURI - mOriginalURI not set!\n");

  *aURI = mOriginalURI ? mOriginalURI : mURI;
  NS_IF_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::SetOriginalURI(nsIURI* aURI)
{
  mOriginalURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::GetURI(nsIURI* *aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  *aURI = mURI;
  NS_IF_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::GetOwner(nsISupports* *aOwner)
{
  nsresult rv = NS_OK;
  if (!mOwner) {
    // Create codebase principal with URI of original document, not our URI
    NS_ASSERTION(mOriginalURI,
        "nsWyciwygChannel::GetOwner without an owner or an original URI!");
    if (mOriginalURI) {
      nsIPrincipal* pIPrincipal = nsnull;
      nsCOMPtr<nsIScriptSecurityManager> secMan(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv));
      if (secMan) {
        rv = secMan->GetCodebasePrincipal(mOriginalURI, &pIPrincipal);
        if (NS_SUCCEEDED(rv)) {
          mOwner = pIPrincipal;
          NS_RELEASE(pIPrincipal);
        }
      }
    } else {
      // Uh oh, must set originalURI before we can return an owner!
      return NS_ERROR_FAILURE;
    }
  }
  NS_ASSERTION(mOriginalURI,
      "nsWyciwygChannel::GetOwner unable to get owner!");
  if (mOwner) {
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
  } else {
    *aOwner = nsnull;
  }
  return rv;
}

NS_IMETHODIMP
nsWyciwygChannel::SetOwner(nsISupports* aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aCallbacks)
{
  *aCallbacks = mCallbacks.get();
  NS_IF_ADDREF(*aCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
  mCallbacks = aNotificationCallbacks;
  mProgressSink = do_GetInterface(mCallbacks);
  return NS_OK;
}

NS_IMETHODIMP 
nsWyciwygChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWyciwygChannel::GetContentType(nsACString &aContentType)
{
  aContentType = NS_LITERAL_CSTRING(wyciwyg_TYPE);
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::SetContentType(const nsACString &aContentType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWyciwygChannel::GetContentCharset(nsACString &aContentCharset)
{
  aContentCharset.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::SetContentCharset(const nsACString &aContentCharset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWyciwygChannel::GetContentLength(PRInt32 *aContentLength)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWyciwygChannel::SetContentLength(PRInt32 aContentLength)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWyciwygChannel::Open(nsIInputStream ** aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWyciwygChannel::AsyncOpen(nsIStreamListener * aListener, nsISupports  * aContext)
{
  LOG(("nsWyciwygChannel::AsyncOpen [this=%x]\n", this));

  NS_ENSURE_ARG_POINTER(aListener);
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);

  //XXX Should I worry about Port safety? 

  mIsPending = PR_TRUE;
  mListener = aListener;
  mListenerContext = aContext;

  // add ourselves to the load group. From this point forward, we'll report
  // all failures asynchronously.
  if (mLoadGroup)
    mLoadGroup->AddRequest(this, nsnull);

  nsresult rv = Connect(PR_TRUE);
  if (NS_FAILED(rv)) {
    LOG(("nsWyciwygChannel::AsyncOpen Connect failed [rv=%x]\n", rv));
    CloseCacheEntry();
    AsyncAbort(rv);
  }
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////////
// nsIWyciwygChannel
//////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsWyciwygChannel::WriteToCacheEntry(const nsACString &aScript)
{
  nsresult rv;

  if (!mCacheEntry) {
    nsCAutoString spec;
    rv = mURI->GetAsciiSpec(spec);
    if (NS_FAILED(rv)) return rv;
    rv = OpenCacheEntry(spec.get(), nsICache::ACCESS_WRITE);
    if (NS_FAILED(rv)) return rv;
  }

  if (!mCacheOutputStream) {
    //Get the transport from cache
    rv = mCacheEntry->GetTransport(getter_AddRefs(mCacheTransport)); 
    if (NS_FAILED(rv)) return rv;
  
    // Get the outputstream from the transport.
    rv = mCacheTransport->OpenOutputStream(0, PRUint32(-1), 0, getter_AddRefs(mCacheOutputStream));    
    if (NS_FAILED(rv)) return rv;
  }

  PRUint32 out;
  return mCacheOutputStream->Write(PromiseFlatCString(aScript).get(), aScript.Length(), &out);
}


NS_IMETHODIMP
nsWyciwygChannel::CloseCacheEntry()
{
  nsresult rv = NS_OK;
  if (mCacheEntry) {
    LOG(("nsWyciwygChannel::CloseCacheEntry [this=%x ]", this));
    // make sure the cache transport isn't holding a reference back to us
    if (mCacheTransport)
      mCacheTransport->SetNotificationCallbacks(nsnull, 0);
    mCacheReadRequest = 0;
    mCacheTransport = 0;
    mCacheOutputStream = 0;
    mCacheEntry = 0;
  }
  return rv;
}

//////////////////////////////////////////////////////////////////////////////
// nsICachelistener
//////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsWyciwygChannel::OnCacheEntryAvailable(nsICacheEntryDescriptor * aCacheEntry, nsCacheAccessMode aMode, nsresult aStatus)
{
  LOG(("nsWyciwygChannel::OnCacheEntryAvailable [this=%x entry=%x "
       "access=%x status=%x]\n", this, aCacheEntry, aMode, aStatus));

  // if the channel's already fired onStopRequest, 
  // then we should ignore this event.
  if (!mIsPending)
    return NS_OK;

  // otherwise, we have to handle this event.
  if (NS_SUCCEEDED(aStatus)) {
    mCacheEntry = aCacheEntry;        
  }

  nsresult rv;

  if (NS_FAILED(mStatus)) {
    LOG(("channel was canceled [this=%x status=%x]\n", this, mStatus));
    rv = mStatus;
  }
  else // advance to the next state...
    rv = Connect(PR_FALSE);

  // a failure from Connect means that we have to abort the channel.
  if (NS_FAILED(rv)) {
    CloseCacheEntry();
    AsyncAbort(rv);
  }

  return rv;
}

//-----------------------------------------------------------------------------
// nsWyciwygChannel::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsWyciwygChannel::OnDataAvailable(nsIRequest *aRequest, nsISupports *aCtxt,
                               nsIInputStream *aInput,
                               PRUint32 aOffset, PRUint32 aCount)
{
  LOG(("nsWyciwygChannel::OnDataAvailable [this=%x request=%x offset=%u count=%u]\n",
        this, aRequest, aOffset, aCount));

  // if the request is for something we no longer reference, then simply 
  // drop this event.
  if (aRequest != mCacheReadRequest) {
    NS_WARNING("nsWyciwygChannel::OnDataAvailable got stale request... why wasn't it cancelled?");
    return NS_BASE_STREAM_CLOSED;
  }

  if (mListener)
    return mListener->OnDataAvailable((nsIRequest *)this, mListenerContext, aInput, aOffset, aCount);

  return NS_BASE_STREAM_CLOSED;
}

//////////////////////////////////////////////////////////////////////////////
// nsIRequestObserver
//////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsWyciwygChannel::OnStartRequest(nsIRequest *aRequest, nsISupports *aCtxt)
{
  nsresult rv = NS_ERROR_FAILURE;
  LOG(("nsWyciwygChannel::OnStartRequest [this=%x request=%x\n",
        this, aRequest));

  // capture the request's status, so our consumers will know ASAP of any
  // connection failures, etc.
  aRequest->GetStatus(&mStatus);
  if (mListener)
     rv = mListener->OnStartRequest(this, mListenerContext);
  return rv;
}


NS_IMETHODIMP
nsWyciwygChannel::OnStopRequest(nsIRequest *aRequest, nsISupports *aCtxt, nsresult aStatus)
{
  LOG(("nsWyciwygChannel::OnStopRequest [this=%x request=%x status=%d\n",
        this, aRequest, (PRUint32)aStatus));
  
  mIsPending = PR_FALSE;
  mStatus = aStatus;
  CloseCacheEntry();
  if (mListener) {
     mListener->OnStopRequest(this, mListenerContext, aStatus);
     mListener = 0;
     mListenerContext = 0;
  }
  
  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nsnull, aStatus);

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////////
// nsIProgressEventSink
//////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsWyciwygChannel::OnStatus(nsIRequest *aRequest, nsISupports *aContext, nsresult aStatus,
                        const PRUnichar *aStatusText)
{
  if (mProgressSink)
    mProgressSink->OnStatus(this, mListenerContext, aStatus, aStatusText);

  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygChannel::OnProgress(nsIRequest *aRequest, nsISupports *aContext,
                          PRUint32 aProgress, PRUint32 aProgressMax)
{
  if (mProgressSink)
    mProgressSink->OnProgress(this, mListenerContext, aProgress, aProgressMax);

  return NS_OK;
}


//////////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////////
nsresult
nsWyciwygChannel::OpenCacheEntry(const char * aCacheKey, nsCacheAccessMode aAccessMode, PRBool * aDelayFlag )
{
  nsresult rv = NS_ERROR_FAILURE;
  // Get cache service
  nsCOMPtr<nsICacheService>  cacheService(do_GetService(NS_CACHESERVICE_CONTRACTID, &rv));

  if (NS_SUCCEEDED(rv) && cacheService) {
    nsXPIDLCString spec;    
    nsAutoString newURIString;    
    nsCOMPtr<nsICacheSession> cacheSession;

    // honor security settings
    nsCacheStoragePolicy storagePolicy;
    if (mLoadFlags & INHIBIT_PERSISTENT_CACHING)
      storagePolicy = nsICache::STORE_IN_MEMORY;
    else
      storagePolicy = nsICache::STORE_ANYWHERE;
 
    // Open a stream based cache session.
    rv = cacheService->CreateSession("wyciwyg", storagePolicy, PR_TRUE, getter_AddRefs(cacheSession));
    if (!cacheSession) 
      return NS_ERROR_FAILURE;

    /* we'll try to synchronously open the cache entry... however, it may be
     * in use and not yet validated, in which case we'll try asynchronously
     * opening the cache entry.
     */
    
    rv = cacheSession->OpenCacheEntry(aCacheKey, aAccessMode, PR_FALSE,
                                 getter_AddRefs(mCacheEntry));

    if (rv == NS_ERROR_CACHE_WAIT_FOR_VALIDATION) {
      // access to the cache entry has been denied. Let's try
      // opening it async. 
      rv = cacheSession->AsyncOpenCacheEntry(aCacheKey, aAccessMode, this);
      if (NS_FAILED(rv)) 
        return rv;
      if (aDelayFlag)
        *aDelayFlag = PR_TRUE;
    }
    else if (rv == NS_OK) {
      LOG(("nsWyciwygChannel::OpenCacheEntry got cache entry \n"));
    }
  } 
  return rv;
}

nsresult
nsWyciwygChannel::Connect(PRBool aFirstTime)
{
  nsresult rv = NS_ERROR_FAILURE;

  LOG(("nsWyciwygChannel::Connect [this=%x]\n", this));

  // true when called from AsyncOpen
  if (aFirstTime) {
    PRBool delayed = PR_FALSE;

    nsCAutoString spec;
    mURI->GetSpec(spec);
    // open a cache entry for this channel...
    rv = OpenCacheEntry(spec.get(), nsICache::ACCESS_READ, &delayed);        

    if (NS_FAILED(rv)) {
      LOG(("nsWyciwygChannel::Connect OpenCacheEntry failed [rv=%x]\n", rv));
      return rv;
    }
 
    if (NS_SUCCEEDED(rv) && delayed)
      return NS_OK;
  }

  // Read the script from cache. 
  if (mCacheEntry) 
    return ReadFromCache();
  return rv;
}

nsresult
nsWyciwygChannel::ReadFromCache()
{
  nsresult rv;

  NS_ENSURE_TRUE(mCacheEntry, NS_ERROR_FAILURE);
  LOG(("nsWyciwygChannel::ReadFromCache [this=%x] ", this));

  // Get a transport to the cached data...
  rv = mCacheEntry->GetTransport(getter_AddRefs(mCacheTransport));
  if (NS_FAILED(rv) || !mCacheTransport) 
    return rv;

  // Hookup the notification callbacks interface to the new transport...
  mCacheTransport->SetNotificationCallbacks(this, 
                                  ((mLoadFlags & nsIRequest::LOAD_BACKGROUND) 
                                    ? nsITransport::DONT_REPORT_PROGRESS 
                                    : 0));

  // Pump the cache data downstream
  return mCacheTransport->AsyncRead(this, nsnull,
                                    0, PRUint32(-1), 0,
                                    getter_AddRefs(mCacheReadRequest));
}




// called when Connect fails
nsresult
nsWyciwygChannel::AsyncAbort(nsresult aStatus)
{
  LOG(("nsWyciwygChannel::AsyncAbort [this=%x status=%x]\n", this, aStatus));

  mStatus = aStatus;
  mIsPending = PR_FALSE;

  // Remove ourselves from the load group.
  if (mLoadGroup)
    mLoadGroup->RemoveRequest((nsIRequest *)this, nsnull, aStatus);

  return NS_OK;
}
