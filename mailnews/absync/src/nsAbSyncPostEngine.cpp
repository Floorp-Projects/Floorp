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
#include "nsAbSyncPostEngine.h"
#include "nsIIOService.h"
#include "nsIChannel.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"
#include "nsIHTTPChannel.h"
#include "nsHTTPEnums.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

/* 
 * This function will be used by the factory to generate an 
 * mime object class object....
 */
nsresult NS_NewPostEngine(nsAbSyncPostEngine ** aInstancePtrResult)
{
	//nsresult result = NS_OK;
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsAbSyncPostEngine *obj = new nsAbSyncPostEngine();
		if (obj)
			return obj->QueryInterface(NS_GET_IID(nsIStreamListener), (void**) aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; // we couldn't allocate the object 
	}
	else
		return NS_ERROR_NULL_POINTER; // aInstancePtrResult was NULL....
}

NS_IMPL_ADDREF(nsAbSyncPostEngine)
NS_IMPL_RELEASE(nsAbSyncPostEngine)

NS_INTERFACE_MAP_BEGIN(nsAbSyncPostEngine)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURIContentListener)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIStreamObserver)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
NS_INTERFACE_MAP_END

/* 
 * Inherited methods for nsMimeConverter
 */
nsAbSyncPostEngine::nsAbSyncPostEngine()
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_REFCNT();

  // Init member variables...
  mTotalWritten = 0;
  mStillRunning = PR_TRUE;
  mCallback = nsnull;
  mContentType = nsnull;
  mCharset = nsnull;

  mListenerArray = nsnull;
  mListenerArrayCount = 0;

  mPostEngineState =  nsIAbSyncPostEngineState::nsIAbSyncPostIdle;
  mTransactionID = 0;
}

nsAbSyncPostEngine::~nsAbSyncPostEngine()
{
  mStillRunning = PR_FALSE;
  PR_FREEIF(mContentType);
  PR_FREEIF(mCharset);

  DeleteListeners();
}

NS_IMETHODIMP nsAbSyncPostEngine::GetInterface(const nsIID & aIID, void * *aInstancePtr)
{
   NS_ENSURE_ARG_POINTER(aInstancePtr);
   return QueryInterface(aIID, aInstancePtr);
}

// nsIURIContentListener support
NS_IMETHODIMP 
nsAbSyncPostEngine::OnStartURIOpen(nsIURI* aURI, 
   const char* aWindowTarget, PRBool* aAbortOpen)
{
   return NS_OK;
}

NS_IMETHODIMP 
nsAbSyncPostEngine::GetProtocolHandler(nsIURI *aURI, nsIProtocolHandler **aProtocolHandler)
{
  *aProtocolHandler = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsAbSyncPostEngine::IsPreferred(const char * aContentType,
                                nsURILoadCommand aCommand,
                                const char * aWindowTarget,
                                char ** aDesiredContentType,
                                PRBool * aCanHandleContent)

{
  return CanHandleContent(aContentType, aCommand, aWindowTarget, aDesiredContentType,
                          aCanHandleContent);
}

NS_IMETHODIMP 
nsAbSyncPostEngine::CanHandleContent(const char * aContentType,
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
nsAbSyncPostEngine::DoContent(const char * aContentType,
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
nsAbSyncPostEngine::GetParentContentListener(nsIURIContentListener** aParent)
{
  *aParent = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsAbSyncPostEngine::SetParentContentListener(nsIURIContentListener* aParent)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsAbSyncPostEngine::GetLoadCookie(nsISupports ** aLoadCookie)
{
  *aLoadCookie = mLoadCookie;
  NS_IF_ADDREF(*aLoadCookie);
  return NS_OK;
}

NS_IMETHODIMP 
nsAbSyncPostEngine::SetLoadCookie(nsISupports * aLoadCookie)
{
  mLoadCookie = aLoadCookie;
  return NS_OK;
}

nsresult
nsAbSyncPostEngine::StillRunning(PRBool *running)
{
  *running = mStillRunning;
  return NS_OK;
}


// Methods for nsIStreamListener...
nsresult
nsAbSyncPostEngine::OnDataAvailable(nsIChannel * aChannel, nsISupports * ctxt, nsIInputStream *aIStream, 
                                    PRUint32 sourceOffset, PRUint32 aLength)
{
  PRUint32        readLen = aLength;

  char *buf = (char *)PR_Malloc(aLength);
  if (!buf)
    return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */

  // read the data from the input stram...
  nsresult rv = aIStream->Read(buf, aLength, &readLen);
  if (NS_FAILED(rv)) return rv;

  // write to the protocol response buffer...
  mProtocolResponse.Append(buf, readLen);
  PR_FREEIF(buf);
  mTotalWritten += readLen;
  NotifyListenersOnProgress(mTransactionID, mTotalWritten, 0);
  return NS_OK;
}


// Methods for nsIStreamObserver 
nsresult
nsAbSyncPostEngine::OnStartRequest(nsIChannel *aChannel, nsISupports *ctxt)
{
  NotifyListenersOnStartSending(mTransactionID, mMessageSize);
  return NS_OK;
}

nsresult
nsAbSyncPostEngine::OnStopRequest(nsIChannel *aChannel, nsISupports * /* ctxt */, nsresult aStatus, const PRUnichar* aMsg)
{
#ifdef NS_DEBUG_rhp
  printf("nsAbSyncPostEngine::OnStopRequest()\n");
#endif

  //
  // Now complete the stream!
  //
  mStillRunning = PR_FALSE;

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

  char  *tProtResponse = mProtocolResponse.ToNewCString();
  NotifyListenersOnStopSending(mTransactionID, aStatus, aMsg, tProtResponse);
  PR_FREEIF(tProtResponse);

  // Time to return...
  return NS_OK;
}

nsresult 
nsAbSyncPostEngine::Initialize(nsOutputFileStream *fOut,
                         nsPostCompletionCallback cb, 
                         void *tagData)
{
  if (!fOut)
    return NS_ERROR_INVALID_ARG;

  if (!fOut->is_open())
    return NS_ERROR_FAILURE;

  mCallback = cb;
  mTagData = tagData;
  return NS_OK;
}

nsresult
nsAbSyncPostEngine::FireURLRequest(nsIURI *aURL, nsPostCompletionCallback  cb, 
                                   void *tagData, const char *postData)
{
  nsresult rv;
  nsIInputStream *postStream = nsnull;

  if (!postData)
    return NS_ERROR_INVALID_ARG;

  rv = NS_NewPostDataStream(&postStream, PR_FALSE, postData, 0);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIChannel> channel;
  NS_ENSURE_SUCCESS(NS_OpenURI(getter_AddRefs(channel), aURL, nsnull), NS_ERROR_FAILURE);
  
  // Tag the post stream onto the channel...
  nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface(channel);
  if (!httpChannel)
    return NS_ERROR_FAILURE;

  mMessageSize = nsCRT::strlen(postData);
  httpChannel->SetRequestMethod(HM_POST);
  httpChannel->SetPostDataStream(postStream);

  // let's try uri dispatching...
  nsCOMPtr<nsIURILoader> pURILoader (do_GetService(NS_URI_LOADER_PROGID));
  NS_ENSURE_TRUE(pURILoader, NS_ERROR_FAILURE);
  nsCOMPtr<nsISupports> openContext;
  nsCOMPtr<nsISupports> cntListener (do_QueryInterface(NS_STATIC_CAST(nsIStreamListener *, this)));
  rv = pURILoader->OpenURI(channel, nsIURILoader::viewNormal, nsnull /* window target */, cntListener);

  mURL = dont_QueryInterface(aURL);
  mCallback = cb;
  mTagData = tagData;
  NS_ADDREF(this);
  return NS_OK;
}

/* void AddSyncListener (in nsIAbSyncPostListener aListener); */
NS_IMETHODIMP nsAbSyncPostEngine::AddPostListener(nsIAbSyncPostListener *aListener)
{
  if ( (mListenerArrayCount > 0) || mListenerArray )
  {
    ++mListenerArrayCount;
    mListenerArray = (nsIAbSyncPostListener **) 
                  PR_Realloc(*mListenerArray, sizeof(nsIAbSyncPostListener *) * mListenerArrayCount);
    if (!mListenerArray)
      return NS_ERROR_OUT_OF_MEMORY;
    else
    {
      mListenerArray[mListenerArrayCount - 1] = aListener;
      return NS_OK;
    }
  }
  else
  {
    mListenerArrayCount = 1;
    mListenerArray = (nsIAbSyncPostListener **) PR_Malloc(sizeof(nsIAbSyncPostListener *) * mListenerArrayCount);
    if (!mListenerArray)
      return NS_ERROR_OUT_OF_MEMORY;

    nsCRT::memset(mListenerArray, 0, (sizeof(nsIAbSyncPostListener *) * mListenerArrayCount));
  
    mListenerArray[0] = aListener;
    NS_ADDREF(mListenerArray[0]);
    return NS_OK;
  }
}

/* void RemoveSyncListener (in nsIAbSyncPostListener aListener); */
NS_IMETHODIMP nsAbSyncPostEngine::RemovePostListener(nsIAbSyncPostListener *aListener)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] == aListener)
    {
      NS_RELEASE(mListenerArray[i]);
      mListenerArray[i] = nsnull;
      return NS_OK;
    }

  return NS_ERROR_INVALID_ARG;
}

nsresult
nsAbSyncPostEngine::DeleteListeners()
{
  if ( (mListenerArray) && (*mListenerArray) )
  {
    PRInt32 i;
    for (i=0; i<mListenerArrayCount; i++)
    {
      NS_RELEASE(mListenerArray[i]);
    }
    
    PR_FREEIF(mListenerArray);
  }

  mListenerArrayCount = 0;
  return NS_OK;
}

nsresult
nsAbSyncPostEngine::NotifyListenersOnStartSending(PRInt32 aTransactionID, PRUint32 aMsgSize)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnStartOperation(aTransactionID, aMsgSize);

  return NS_OK;
}

nsresult
nsAbSyncPostEngine::NotifyListenersOnProgress(PRInt32 aTransactionID, PRUint32 aProgress, PRUint32 aProgressMax)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnProgress(aTransactionID, aProgress, aProgressMax);

  return NS_OK;
}

nsresult
nsAbSyncPostEngine::NotifyListenersOnStatus(PRInt32 aTransactionID, PRUnichar *aMsg)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnStatus(aTransactionID, aMsg);

  return NS_OK;
}

nsresult
nsAbSyncPostEngine::NotifyListenersOnStopSending(PRInt32 aTransactionID, nsresult aStatus, 
                                                 const PRUnichar *aMsg, char *aProtocolResponse)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnStopOperation(aTransactionID, aStatus, aMsg, aProtocolResponse);

  return NS_OK;
}

// Utility to create a nsIURI object...
extern "C" nsresult 
nsMimeNewURI(nsIURI** aInstancePtrResult, const char *aSpec, nsIURI *aBase)
{  
  nsresult  res;

  if (nsnull == aInstancePtrResult) 
    return NS_ERROR_NULL_POINTER;
  
  NS_WITH_SERVICE(nsIIOService, pService, kIOServiceCID, &res);
  if (NS_FAILED(res)) 
    return NS_ERROR_FACTORY_NOT_REGISTERED;

  return pService->NewURI(aSpec, aBase, aInstancePtrResult);
}

static nsresult
PostDoneCallback(nsIURI* aURL, nsresult aStatus, 
                 const char *aContentType,
                 const char *aCharset,
                 PRInt32 totalSize, 
                 const PRUnichar* aMsg, void *tagData)
{
  nsAbSyncPostEngine *postEngine = (nsAbSyncPostEngine *) tagData;
  if (postEngine)
  {
    postEngine->mPostEngineState = nsIAbSyncPostEngineState::nsIAbSyncPostIdle;
  }

  return NS_OK;
}


/* PRInt32 GetCurrentState (); */
NS_IMETHODIMP nsAbSyncPostEngine::GetCurrentState(PRInt32 *_retval)
{
  *_retval = mPostEngineState;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////////
//
// This is the implementation of the actual post driver. 
//
////////////////////////////////////////////////////////////////////////////////////////

/* void SendAbRequest (in string aProtocolRequest, out PRInt32 aTransactionID); */
NS_IMETHODIMP nsAbSyncPostEngine::SendAbRequest(const char *aSpec, const char *aProtocolRequest, PRInt32 aTransactionID)
{
  nsresult  rv;
  nsIURI    *workURI = nsnull;

  // Only try if we are not currently busy!
  if (mPostEngineState != nsIAbSyncPostEngineState::nsIAbSyncPostIdle)
    return NS_ERROR_FAILURE;

  mTransactionID = aTransactionID;
  mProtocolResponse = "";
  mTotalWritten = 0;

  rv = nsMimeNewURI(&workURI, aSpec, nsnull);
  if (NS_FAILED(rv) || (!workURI))
    return NS_ERROR_FAILURE;

  rv = FireURLRequest(workURI, PostDoneCallback, this, aProtocolRequest);
  mPostEngineState = nsIAbSyncPostEngineState::nsIAbSyncPostRunning;
  return rv;
}
