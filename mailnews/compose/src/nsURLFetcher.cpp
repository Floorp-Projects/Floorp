/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsCOMPtr.h"
#include "stdio.h"
#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "comi18n.h"
#include "prmem.h"
#include "plstr.h"
#include "nsRepository.h"
#include "nsIURL.h"
#include "nsString.h"
#include "nsINetService.h"
#include "nsIServiceManager.h"
#include "nsURLFetcher.h"

// netlib definitions....
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);

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
			return obj->QueryInterface(nsCOMTypeInfo<nsIStreamListener>::GetIID(), (void**) aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; // we couldn't allocate the object 
	}
	else
		return NS_ERROR_NULL_POINTER; // aInstancePtrResult was NULL....
}

// The following macros actually implement addref, release and 
// query interface for our component. 
NS_IMPL_ISUPPORTS(nsURLFetcher, nsCOMTypeInfo<nsIStreamListener>::GetIID());

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
  mURL = nsnull;
  mStillRunning = PR_TRUE;
  mCallback = nsnull;
  mNetService = nsnull;
  mContentType = nsnull;
}

nsURLFetcher::~nsURLFetcher()
{
  mStillRunning = PR_FALSE;
  if (mNetService)
  {
    nsServiceManager::ReleaseService(kNetServiceCID, mNetService);
  }

  PR_FREEIF(mContentType);
  NS_IF_RELEASE(mURL);
}

nsresult
nsURLFetcher::StillRunning(PRBool *running)
{
  *running = mStillRunning;
  return NS_OK;
}


// Methods for nsIStreamListener...
//
// Return information regarding the current URL load.<BR>
// The info structure that is passed in is filled out and returned
// to the caller. 
// 
//This method is currently not called.  
//
nsresult
nsURLFetcher::GetBindInfo(nsIURI* aURL, nsStreamBindingInfo* aInfo)
{
#ifdef NS_DEBUG_richie
  printf("nsURLFetcher::GetBindInfo()\n");
#endif

  return NS_OK;
}

/**
* Notify the client that data is available in the input stream.  This
* method is called whenver data is written into the input stream by the
* networking library...<BR><BR>
* 
* @param pIStream  The input stream containing the data.  This stream can
* be either a blocking or non-blocking stream.
* @param length    The amount of data that was just pushed into the stream.
* @return The return value is currently ignored.
*/
nsresult
nsURLFetcher::OnDataAvailable(nsIURI* aURL, nsIInputStream *aIStream, 
                                   PRUint32 aLength)
{
  PRUint32        readLen = aLength;
  PRUint32        wroteIt;

  if (!mOutStream)
    return NS_ERROR_INVALID_ARG;

  char *buf = (char *)PR_Malloc(aLength);
  if (!buf)
    return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */

  // read the data from the input stram...
  aIStream->Read(buf, aLength, &readLen);

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
/**
* Notify the observer that the URL has started to load.  This method is
* called only once, at the beginning of a URL load.<BR><BR>
*
* @return The return value is currently ignored.  In the future it may be
* used to cancel the URL load..
*/
nsresult
nsURLFetcher::OnStartRequest(nsIURI* aURL, const char *aContentType)
{
#ifdef NS_DEBUG_richie
  printf("nsURLFetcher::OnStartRequest() for Content-Type: %s\n", aContentType);
#endif

  if (aContentType)
    mContentType = PL_strdup(aContentType);

  return NS_OK;
}

/**
* Notify the observer that progress as occurred for the URL load.<BR>
*/
nsresult
nsURLFetcher::OnProgress(nsIURI* aURL, PRUint32 aProgress, PRUint32 aProgressMax)
{
#ifdef NS_DEBUG_richie
  printf("nsURLFetcher::OnProgress() - %d bytes\n", aProgress);
#endif

  return NS_OK;
}

/**
* Notify the observer with a status message for the URL load.<BR>
*/
nsresult
nsURLFetcher::OnStatus(nsIURI* aURL, const PRUnichar* aMsg)
{
#ifdef NS_DEBUG_richie
  nsString  tmp(aMsg);
  char      *msg = tmp.ToNewCString();
  printf("nsURLFetcher::OnStatus(): %s\n", msg);
  PR_FREEIF(msg);
#endif

  return NS_OK;
}

/**
* Notify the observer that the URL has finished loading.  This method is 
* called once when the networking library has finished processing the 
* URL transaction initiatied via the nsINetService::Open(...) call.<BR><BR>
* 
* This method is called regardless of whether the URL loaded successfully.<BR><BR>
* 
* @param status    Status code for the URL load.
* @param msg   A text string describing the error.
* @return The return value is currently ignored.
*/
nsresult
nsURLFetcher::OnStopRequest(nsIURI* aURL, nsresult aStatus, const PRUnichar* aMsg)
{
#ifdef NS_DEBUG_richie
  printf("nsURLFetcher::OnStopRequest()\n");
#endif

  //
  // Now complete the stream!
  //
  mStillRunning = PR_FALSE;

  // First close the output stream...
  if (mOutStream)
    mOutStream->close();

  // Now if there is a callback, we need to call it...
  if (mCallback)
    mCallback (mURL, aStatus, mContentType, mTotalWritten, aMsg, mTagData);

  // Time to return...
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

  rv = nsServiceManager::GetService(kNetServiceCID, nsCOMTypeInfo<nsINetService>::GetIID(),
                                             (nsISupports **)&mNetService);
  if ((rv != NS_OK)  || (!mNetService))
  {
    return NS_ERROR_FACTORY_NOT_LOADED;
  }

  if (NS_FAILED(mNetService->OpenStream(aURL, this)))
  {
    nsServiceManager::ReleaseService(kNetServiceCID, mNetService);  
    return NS_ERROR_UNEXPECTED;
  }

  mURL = aURL;
  mOutStream = fOut;
  mCallback = cb;
  mTagData = tagData;
  NS_ADDREF(mURL);
  NS_ADDREF(this);
  return NS_OK;
}
