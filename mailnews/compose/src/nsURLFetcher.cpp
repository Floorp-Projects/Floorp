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
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "msgCore.h" // for pre-compiled headers
#include "nsCOMPtr.h"
#include <stdio.h>
#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "comi18n.h"
#include "prmem.h"
#include "plstr.h"
#include "nsRepository.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsURLFetcher.h"
#include "nsIIOService.h"
#include "nsIChannel.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"
#include "nsIHTTPChannel.h"
#include "nsIWebProgress.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

/* 
 * This function will be used by the factory to generate an 
 * mime object class object....
 */
nsresult NS_NewURLFetcher(nsURLFetcher ** aInstancePtrResult)
{
	//nsresult result = NS_OK;
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsURLFetcher *obj = new nsURLFetcher();
		if (obj)
			return obj->QueryInterface(NS_GET_IID(nsIStreamListener), (void**) aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; // we couldn't allocate the object 
	}
	else
		return NS_ERROR_NULL_POINTER; // aInstancePtrResult was NULL....
}

NS_IMPL_ADDREF(nsURLFetcher)
NS_IMPL_RELEASE(nsURLFetcher)

NS_INTERFACE_MAP_BEGIN(nsURLFetcher)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURIContentListener)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIStreamObserver)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
NS_INTERFACE_MAP_END

/* 
 * Inherited methods for nsMimeConverter
 */
nsURLFetcher::nsURLFetcher()
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_REFCNT();

  // Init member variables...
  mOutStream = nsnull;
  mTotalWritten = 0;
  mStillRunning = PR_TRUE;
  mCallback = nsnull;
  mContentType = nsnull;
  mCharset = nsnull;
  mOnStopRequestProcessed = PR_FALSE;
}

nsURLFetcher::~nsURLFetcher()
{
  mStillRunning = PR_FALSE;
  PR_FREEIF(mContentType);
  PR_FREEIF(mCharset);
  // Remove the DocShell as a listener of the old WebProgress...
  if (mLoadCookie) 
  {
    nsCOMPtr<nsIWebProgress> webProgress(do_QueryInterface(mLoadCookie));

    if (webProgress)
      webProgress->RemoveProgressListener(this);
  }
}

NS_IMETHODIMP nsURLFetcher::GetInterface(const nsIID & aIID, void * *aInstancePtr)
{
   NS_ENSURE_ARG_POINTER(aInstancePtr);
   return QueryInterface(aIID, aInstancePtr);
}

// nsIURIContentListener support
NS_IMETHODIMP 
nsURLFetcher::OnStartURIOpen(nsIURI* aURI, 
   const char* aWindowTarget, PRBool* aAbortOpen)
{
   return NS_OK;
}

NS_IMETHODIMP 
nsURLFetcher::GetProtocolHandler(nsIURI *aURI, nsIProtocolHandler **aProtocolHandler)
{
  *aProtocolHandler = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsURLFetcher::IsPreferred(const char * aContentType,
                                nsURILoadCommand aCommand,
                                const char * aWindowTarget,
                                char ** aDesiredContentType,
                                PRBool * aCanHandleContent)

{
  return CanHandleContent(aContentType, aCommand, aWindowTarget, aDesiredContentType,
                          aCanHandleContent);
}

NS_IMETHODIMP 
nsURLFetcher::CanHandleContent(const char * aContentType,
                                nsURILoadCommand aCommand,
                                const char * aWindowTarget,
                                char ** aDesiredContentType,
                                PRBool * aCanHandleContent)

{
  if (nsCRT::strcasecmp(aContentType, MESSAGE_RFC822) == 0)
    *aDesiredContentType = nsCRT::strdup("text/html");

  // since we explicilty loaded the url, we always want to handle it!
  *aCanHandleContent = PR_TRUE;
  return NS_OK;
} 

NS_IMETHODIMP 
nsURLFetcher::DoContent(const char * aContentType,
                      nsURILoadCommand aCommand,
                      const char * aWindowTarget,
                      nsIChannel * aOpenedChannel,
                      nsIStreamListener ** aContentHandler,
                      PRBool * aAbortProcess)
{
  nsresult rv = NS_OK;
  if (aAbortProcess)
    *aAbortProcess = PR_FALSE;
  QueryInterface(NS_GET_IID(nsIStreamListener), (void **) aContentHandler);
  return rv;
}

NS_IMETHODIMP 
nsURLFetcher::GetParentContentListener(nsIURIContentListener** aParent)
{
  *aParent = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsURLFetcher::SetParentContentListener(nsIURIContentListener* aParent)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsURLFetcher::GetLoadCookie(nsISupports ** aLoadCookie)
{
  *aLoadCookie = mLoadCookie;
  NS_IF_ADDREF(*aLoadCookie);
  return NS_OK;
}

NS_IMETHODIMP 
nsURLFetcher::SetLoadCookie(nsISupports * aLoadCookie)
{
  // Remove the DocShell as a listener of the old WebProgress...
  if (mLoadCookie) 
  {
    nsCOMPtr<nsIWebProgress> webProgress(do_QueryInterface(mLoadCookie));

    if (webProgress)
      webProgress->RemoveProgressListener(this);
  }

  mLoadCookie = aLoadCookie;

  // Add the DocShell as a listener to the new WebProgress...
  if (mLoadCookie) 
  {
    nsCOMPtr<nsIWebProgress> webProgress(do_QueryInterface(mLoadCookie));

    if (webProgress) 
      webProgress->AddProgressListener(this);
  }
  return NS_OK;

}

nsresult
nsURLFetcher::StillRunning(PRBool *running)
{
  *running = mStillRunning;
  return NS_OK;
}


// Methods for nsIStreamListener...
nsresult
nsURLFetcher::OnDataAvailable(nsIChannel * aChannel, nsISupports * ctxt, nsIInputStream *aIStream, 
                              PRUint32 sourceOffset, PRUint32 aLength)
{
  PRUint32        readLen = aLength;
  PRUint32        wroteIt;

  if (!mOutStream)
    return NS_ERROR_INVALID_ARG;

  char *buf = (char *)PR_Malloc(aLength);
  if (!buf)
    return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */

  // read the data from the input stram...
  nsresult rv = aIStream->Read(buf, aLength, &readLen);
  if (NS_FAILED(rv)) return rv;

  // write to the output file...
  wroteIt = mOutStream->write(buf, readLen);
  PR_FREEIF(buf);

  if (wroteIt != readLen)
    return NS_ERROR_FAILURE;
  else
  {
    mTotalWritten += wroteIt;
    return NS_OK;
  }
}


// Methods for nsIStreamObserver 
nsresult
nsURLFetcher::OnStartRequest(nsIChannel *aChannel, nsISupports *ctxt)
{
  return NS_OK;
}

nsresult
nsURLFetcher::OnStopRequest(nsIChannel *aChannel, nsISupports * /* ctxt */, nsresult aStatus, const PRUnichar* aMsg)
{
#ifdef NS_DEBUG_rhp
  printf("nsURLFetcher::OnStopRequest()\n");
#endif

  // it's possible we could get in here from the channel calling us with an OnStopRequest and from our
  // onStatusChange method (in the case of an error). So we should protect against this to make sure we
  // don't process the on stop request twice...

  if (mOnStopRequestProcessed) return NS_OK;
  mOnStopRequestProcessed = PR_TRUE;

  //
  // Now complete the stream!
  //
  mStillRunning = PR_FALSE;

  // First close the output stream...
  if (mOutStream)
  {
    mOutStream->close();
    mOutStream = nsnull;
  }

  // Check the content type!
  if (aChannel)
  {
    char    *contentType = nsnull;
    char    *charset = nsnull;

    if (NS_SUCCEEDED(aChannel->GetContentType(&contentType)) && contentType)
    {
      if (PL_strcasecmp(contentType, UNKNOWN_CONTENT_TYPE))
      {
        mContentType = contentType;
      }
    }

    nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface(aChannel);
    if (httpChannel)
    {
      if (NS_SUCCEEDED(httpChannel->GetCharset(&charset)) && charset)
      {
        mCharset = charset;
      }
    }
  }  

  // Now if there is a callback, we need to call it...
  if (mCallback)
    mCallback (mURL, aStatus, mContentType, mCharset, mTotalWritten, aMsg, mTagData);

  // Time to return...
  return NS_OK;
}

nsresult 
nsURLFetcher::Initialize(nsOutputFileStream *fOut,
                         nsAttachSaveCompletionCallback cb, 
                         void *tagData)
{
  if (!fOut)
    return NS_ERROR_INVALID_ARG;

  if (!fOut->is_open())
    return NS_ERROR_FAILURE;

  mOutStream = fOut;
  mCallback = cb;
  mTagData = tagData;
  return NS_OK;
}

nsresult
nsURLFetcher::FireURLRequest(nsIURI *aURL, nsOutputFileStream *fOut, 
                             nsAttachSaveCompletionCallback cb, void *tagData)
{
  nsresult rv;

  if ( (!aURL) || (!fOut) )
  {
    return NS_ERROR_INVALID_ARG;
  }

  if (!fOut->is_open())
  {
    return NS_ERROR_FAILURE;
  }

  // we're about to fire a new url request so make sure the on stop request flag is cleared...
  mOnStopRequestProcessed = PR_FALSE;

  // let's try uri dispatching...
  nsCOMPtr<nsIURILoader> pURILoader (do_GetService(NS_URI_LOADER_CONTRACTID));
  NS_ENSURE_TRUE(pURILoader, NS_ERROR_FAILURE);

  nsCOMPtr<nsISupports> cntListener (do_QueryInterface(NS_STATIC_CAST(nsIStreamListener *, this)));
  nsCOMPtr<nsIChannel> channel;
  nsCOMPtr<nsILoadGroup> loadGroup;
  pURILoader->GetLoadGroupForContext(cntListener, getter_AddRefs(loadGroup));
  NS_ENSURE_SUCCESS(NS_OpenURI(getter_AddRefs(channel), aURL, nsnull, loadGroup), NS_ERROR_FAILURE);
 
  rv = pURILoader->OpenURI(channel, nsIURILoader::viewNormal, nsnull /* window target */, 
                           cntListener);

  mURL = dont_QueryInterface(aURL);
  mOutStream = fOut;
  mCallback = cb;
  mTagData = tagData;
  NS_ADDREF(this);
  return NS_OK;
}


// web progress listener implementation

NS_IMETHODIMP
nsURLFetcher::OnProgressChange(nsIWebProgress *aProgress, nsIRequest *aRequest,
                             PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress,
                             PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
  return NS_OK;
}

NS_IMETHODIMP
nsURLFetcher::OnStateChange(nsIWebProgress *aProgress, nsIRequest *aRequest,
                          PRInt32 aStateFlags, nsresult aStatus)
{
  // all we care about is the case where an error occurred (as in we were unable to locate the
  // the url....

  if (NS_FAILED(aStatus))
  {
    nsCOMPtr<nsIChannel> channel (do_QueryInterface(aRequest));
    OnStopRequest(channel, nsnull, aStatus, nsnull);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsURLFetcher::OnLocationChange(nsIWebProgress* aWebProgress,
                               nsIRequest* aRequest,
                               nsIURI *aURI)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsURLFetcher::OnStatusChange(nsIWebProgress* aWebProgress,
                             nsIRequest* aRequest,
                             nsresult aStatus,
                             const PRUnichar* aMessage)
{
    return NS_OK;
}

