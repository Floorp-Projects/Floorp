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
#include "nsIPref.h"
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
#include "nsTextFormatter.h"
#include "nsIHTTPHeader.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

/* 
 * This function will be used by the factory to generate an 
 * object class object....
 */
NS_METHOD
nsAbSyncPostEngine::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsAbSyncPostEngine *ph = new nsAbSyncPostEngine();
    if (ph == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return ph->QueryInterface(aIID, aResult);
}

NS_IMPL_ADDREF(nsAbSyncPostEngine)
NS_IMPL_RELEASE(nsAbSyncPostEngine)

NS_INTERFACE_MAP_BEGIN(nsAbSyncPostEngine)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURIContentListener)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIStreamObserver)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
   NS_INTERFACE_MAP_ENTRY(nsIAbSyncPostEngine)
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
  mMessageSize = 0;
  mAuthenticationRunning = PR_TRUE;
}

nsAbSyncPostEngine::~nsAbSyncPostEngine()
{
  mStillRunning = PR_FALSE;
  PR_FREEIF(mContentType);
  PR_FREEIF(mCharset);

  PR_FREEIF(mSyncSpec);
  PR_FREEIF(mSyncProtocolRequest);

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
  mProtocolResponse.Append(NS_ConvertASCIItoUCS2(buf), readLen);
  PR_FREEIF(buf);
  mTotalWritten += readLen;

  if (!mAuthenticationRunning)
    NotifyListenersOnProgress(mTransactionID, mTotalWritten, 0);
  return NS_OK;
}


// Methods for nsIStreamObserver 
nsresult
nsAbSyncPostEngine::OnStartRequest(nsIChannel *aChannel, nsISupports *ctxt)
{
  if (mAuthenticationRunning)
    NotifyListenersOnStartAuthOperation();
  else
    NotifyListenersOnStartSending(mTransactionID, mMessageSize);
  return NS_OK;
}

nsresult
nsAbSyncPostEngine::OnStopRequest(nsIChannel *aChannel, nsISupports * /* ctxt */, nsresult aStatus, const PRUnichar* aMsg)
{
#ifdef NS_DEBUG_rhp
  printf("nsAbSyncPostEngine::OnStopRequest()\n");
#endif

  char  *tProtResponse = nsnull;

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
    mCallback (aStatus, mContentType, mCharset, mTotalWritten, aMsg, mTagData);

  if (mAuthenticationRunning)
  {
    if (aChannel)
    {
      // Dig out the cookie!
      nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface(aChannel);
      if (httpChannel)
      {
        nsresult rv;
        if (httpChannel)
        {
          nsCAutoString			eTagValue, lastModValue, contentLengthValue;
          nsCOMPtr<nsISimpleEnumerator>	enumerator;
          if (NS_SUCCEEDED(rv = httpChannel->GetResponseHeaderEnumerator(getter_AddRefs(enumerator))))
          {
            PRBool			bMoreHeaders;
            
            while (NS_SUCCEEDED(rv = enumerator->HasMoreElements(&bMoreHeaders))
              && (bMoreHeaders == PR_TRUE))
            {
              nsCOMPtr<nsISupports>   item;
              enumerator->GetNext(getter_AddRefs(item));
              nsCOMPtr<nsIHTTPHeader>	header = do_QueryInterface(item);
              if (header)
              {
                nsCOMPtr<nsIAtom>       headerAtom;
                header->GetField(getter_AddRefs(headerAtom));
                nsAutoString		headerStr;
                headerAtom->ToString(headerStr);
                
                char	*val = nsnull;
                if (headerStr.EqualsIgnoreCase("set-cookie"))
                {
                  header->GetValue(&val);
                  if (val)
                  {
                    nsCRT::free(val);
                  }
                }
              }
            }
          }
        }
      }
    }
    
    NotifyListenersOnStopAuthOperation(aStatus, aMsg, tProtResponse);
    KickTheSyncOperation();
  }
  else
  {
    tProtResponse = mProtocolResponse.ToNewCString();
    NotifyListenersOnStopSending(mTransactionID, aStatus, aMsg, tProtResponse);
  }

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
nsAbSyncPostEngine::NotifyListenersOnStartAuthOperation(void)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnStartAuthOperation();
  return NS_OK;
}

nsresult
nsAbSyncPostEngine::NotifyListenersOnStopAuthOperation(nsresult aStatus, const PRUnichar *aMsg, const char *aCookie)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnStopAuthOperation(aStatus, aMsg, aCookie);
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
nsEngineNewURI(nsIURI** aInstancePtrResult, const char *aSpec, nsIURI *aBase)
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
PostDoneCallback(nsresult aStatus, 
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

nsresult
nsAbSyncPostEngine::FireURLRequest(nsIURI *aURL, nsPostCompletionCallback  cb, 
                                   void *tagData, const char *postData)
{
  nsresult rv;
  nsCOMPtr<nsIInputStream> postStream;

  if (!postData)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIChannel> channel;
  NS_ENSURE_SUCCESS(NS_OpenURI(getter_AddRefs(channel), aURL, nsnull), NS_ERROR_FAILURE);

  // Tag the post stream onto the channel...but never seemed to work...so putting it
  // directly on the URL spec
  //
  nsCOMPtr<nsIAtom> method = NS_NewAtom ("POST");
  nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface(channel);
  if (!httpChannel)
    return NS_ERROR_FAILURE;

  httpChannel->SetRequestMethod(method);
  if (NS_SUCCEEDED(rv = NS_NewPostDataStream(getter_AddRefs(postStream), PR_FALSE, postData, 0)))
    httpChannel->SetUploadStream(postStream);

  httpChannel->AsyncRead(this, nsnull);

  mCallback = cb;
  mTagData = tagData;
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
NS_IMETHODIMP nsAbSyncPostEngine::SendAbRequest(const char *aSpec, PRInt32 aPort, const char *aProtocolRequest, PRInt32 aTransactionID)
{
  nsresult    rv;
  nsIURI      *workURI = nsnull;
  PRUnichar   *screenName = nsnull;
  PRUnichar   *passWord = nsnull;
  char        *sName = nsnull;
  char        *pWord = nsnull;
  char        *tCommand = nsnull;
  char        *protocolRequest = nsnull;
  char        *postHeader = nsnull;

  // Only try if we are not currently busy!
  if (mPostEngineState != nsIAbSyncPostEngineState::nsIAbSyncPostIdle)
    return NS_ERROR_FAILURE;

  // Get the prefs for the server and port for Authentication...
  //
  char      *authServer = nsnull;
  char      *authSpec = nsnull;
  PRInt32   authPort;

  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
  if (NS_FAILED(rv) || !prefs) 
    return NS_ERROR_FAILURE;

  prefs->CopyCharPref("mail.absync.auth_server",           &authServer);
  if (NS_FAILED(prefs->GetIntPref("mail.absync.auth_port", &authPort)))
    authPort = 16500;

  authSpec = PR_smprintf("http://%s", authServer);
  if (!authSpec)  
  {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto GetOuttaHere;
  }

  // Set transaction ID and save/init Sync info...
  mTransactionID = aTransactionID;
  mSyncSpec = nsCRT::strdup(aSpec);
  mSyncPort = aPort;
  mSyncProtocolRequest = nsCRT::strdup(aProtocolRequest);
  mProtocolResponse = NS_ConvertASCIItoUCS2("");
  mTotalWritten = 0;

  // The first thing we need to do is authentication so do it!
  mAuthenticationRunning = PR_TRUE;
  mPostEngineState = nsIAbSyncPostEngineState::nsIAbSyncAuthenticationRunning;

  // Now build the auth post to get the length...
  // Got to aim and get the username and password:
  //
    // The interface is nsIAimSession: 
    // GetCurrentScreenName(PRUnichar** aCurrentScreenName); 
    // GetSavedPassword(PRUnichar** aPassword); 
  //

  prefs->CopyCharPref("mail.absync.aim_username_hack", &sName);
  prefs->CopyCharPref("mail.absync.aim_password_hack", &pWord);

  protocolRequest = PR_smprintf("screenname=%s&password=%s", sName, pWord);

  if (protocolRequest)
    mMessageSize = nsCRT::strlen(protocolRequest);
  else
    mMessageSize = 0;

  postHeader = "Content-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\n\r\n%s";
  tCommand = PR_smprintf(postHeader, mMessageSize, protocolRequest);
  if (!tCommand)
  {
    rv = NS_ERROR_OUT_OF_MEMORY; // we couldn't allocate the string 
    goto GetOuttaHere;
  }

  rv = nsEngineNewURI(&workURI, authSpec, nsnull);
  if (NS_FAILED(rv) || (!workURI))
  {
    rv = NS_ERROR_OUT_OF_MEMORY; // we couldn't allocate the string 
    goto GetOuttaHere;
  }

  if (authPort > 0)
    workURI->SetPort(authPort);

  rv = FireURLRequest(workURI, PostDoneCallback, this, tCommand);

GetOuttaHere:
  PR_FREEIF(authSpec);
  NS_IF_RELEASE(workURI);
  PR_FREEIF(tCommand);
  PR_FREEIF(sName);
  PR_FREEIF(pWord);
  mPostEngineState = nsIAbSyncPostEngineState::nsIAbSyncPostRunning;
  return rv;
}

NS_IMETHODIMP 
nsAbSyncPostEngine::KickTheSyncOperation(void)
{
  nsresult  rv;
  nsIURI    *workURI = nsnull;

  // The first thing we need to do is authentication so do it!
  mAuthenticationRunning = PR_FALSE;
  mProtocolResponse = NS_ConvertASCIItoUCS2("");
  mPostEngineState = nsIAbSyncPostEngineState::nsIAbSyncPostRunning;

  char    *postHeader = "Content-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\n\r\n%s";
  if (mSyncProtocolRequest)
    mMessageSize = nsCRT::strlen(mSyncProtocolRequest);
  else
    mMessageSize = 0;

  char *tCommand = PR_smprintf(postHeader, mMessageSize, mSyncProtocolRequest);
  if (!tCommand)
  {
    rv = NS_ERROR_OUT_OF_MEMORY; // we couldn't allocate the string 
    goto GetOuttaHere;
  }

  rv = nsEngineNewURI(&workURI, mSyncSpec, nsnull);
  if (NS_FAILED(rv) || (!workURI))
  {
    rv = NS_ERROR_FAILURE;  // we couldn't allocate the string 
    goto GetOuttaHere;
  }

  if (mSyncPort > 0)
    workURI->SetPort(mSyncPort);

  rv = FireURLRequest(workURI, PostDoneCallback, this, tCommand);

GetOuttaHere:
  NS_IF_RELEASE(workURI);
  PR_FREEIF(tCommand);
  mPostEngineState = nsIAbSyncPostEngineState::nsIAbSyncPostRunning;
  return rv;
}
