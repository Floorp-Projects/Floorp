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
#include "nsIServiceManager.h"
#include "nsURLFetcher.h"
#include "nsIIOService.h"
#include "nsIChannel.h"

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

// The following macros actually implement addref, release and 
// query interface for our component. 
NS_IMPL_ISUPPORTS1(nsURLFetcher, nsIStreamListener)

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
}

nsURLFetcher::~nsURLFetcher()
{
  mStillRunning = PR_FALSE;
}

nsresult
nsURLFetcher::StillRunning(PRBool *running)
{
  *running = mStillRunning;
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
nsURLFetcher::OnDataAvailable(nsIRequest *request, nsISupports * ctxt, nsIInputStream *aIStream, 
                              PRUint32 sourceOffset, PRUint32 aLength)
{
  nsresult        rc = NS_OK;
  PRUint32        readLen = aLength;

  if (!mOutStream)
    return NS_ERROR_FAILURE;

  char *buf = (char *)PR_Malloc(aLength);
  if (!buf)
    return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */

  // read the data from the input stram...
  nsresult rv = aIStream->Read(buf, aLength, &readLen);
  if (NS_FAILED(rv)) return rv;

  // write to the output file...
  mTotalWritten += mOutStream->write(buf, readLen);

  PR_FREEIF(buf);
  return rc;
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
nsURLFetcher::OnStartRequest(nsIRequest *request, nsISupports * ctxt)
{
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
nsURLFetcher::OnStopRequest(nsIRequest *request, nsISupports * /* ctxt */, nsresult aStatus, const PRUnichar* aMsg)
{
#ifdef NS_DEBUG_rhp
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
    mCallback (mURL, aStatus, mTotalWritten, aMsg, mTagData);

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

  NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIChannel> channel;
  rv = service->NewChannelFromURI(aURL, getter_AddRefs(channel));
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIRequest> request;
  rv = channel->AsyncRead(this, nsnull, 0, -1, getter_AddRefs(request));
  if (NS_FAILED(rv)) return rv;

  mURL = dont_QueryInterface(aURL);
  mOutStream = fOut;
  mCallback = cb;
  mTagData = tagData;
  NS_ADDREF(this);
  return NS_OK;
}
