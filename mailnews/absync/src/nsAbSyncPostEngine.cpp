/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h" // for pre-compiled headers
#include "nsCOMPtr.h"
#include <stdio.h>
#include "nscore.h"
#include "prprf.h"
#include "nsEscape.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "comi18n.h"
#include "prmem.h"
#include "plstr.h"
#include "nsIPref.h"
#include "nsIComponentManager.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsAbSyncPostEngine.h"
#include "nsIIOService.h"
#include "nsIChannel.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"
#include "nsIHttpChannel.h"
#include "nsIUploadChannel.h"
#include "nsTextFormatter.h"
#include "nsICookieService.h"
#include "nsIAbSync.h"
#include "nsAbSyncCID.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCookieServiceCID, NS_COOKIESERVICE_CID);
static NS_DEFINE_CID(kCAbSyncMojoCID, NS_AB_SYNC_MOJO_CID);
static NS_DEFINE_CID(kAbSync, NS_ABSYNC_SERVICE_CID);

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
   NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
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
  mContentType = nsnull;
  mCharset = nsnull;

  mListenerArray = nsnull;
  mListenerArrayCount = 0;

  mPostEngineState =  nsIAbSyncPostEngineState::nsIAbSyncPostIdle;
  mTransactionID = 0;
  mMessageSize = 0;
  mAuthenticationRunning = PR_TRUE;
  mCookie = nsnull;
  mUser = nsnull;
  mSyncProtocolRequest = nsnull;
  mSyncProtocolRequestPrefix = nsnull;
  mChannel = nsnull;
  mMojoSyncSpec = nsnull;
}

nsAbSyncPostEngine::~nsAbSyncPostEngine()
{
  mStillRunning = PR_FALSE;
  PR_FREEIF(mContentType);
  PR_FREEIF(mCharset);

  PR_FREEIF(mSyncProtocolRequest);
  PR_FREEIF(mSyncProtocolRequestPrefix);
  PR_FREEIF(mCookie);
  PR_FREEIF(mUser);
  PR_FREEIF(mMojoSyncSpec);
  DeleteListeners();
}

PRInt32 Base64Decode_int(const char *in_str, unsigned char *out_str,
                                PRUint32& decoded_len);
/* ==================================================================
 * Base64Encode
 *
 * Returns number of bytes that were encoded.
 *
 *   >0   -> OK
 *   -1   -> BAD (output buffer not big enough).
 *
 * ==================================================================
 */
PRInt32 Base64Encode(const unsigned char *in_str, PRInt32 in_len, char *out_str,
                               PRInt32 out_len)
{
    static unsigned char base64[] =
    {  
        /*   0    1    2    3    4    5    6    7   */
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', /* 0 */
        'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', /* 1 */
        'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', /* 2 */
        'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', /* 3 */
        'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', /* 4 */
        'o', 'p', 'q', 'r', 's', 't', 'u', 'v', /* 5 */
        'w', 'x', 'y', 'z', '0', '1', '2', '3', /* 6 */
        '4', '5', '6', '7', '8', '9', '+', '/'  /* 7 */
    };
    PRInt32 curr_out_len = 0;

    PRInt32 i = 0;
    unsigned char a, b, c;

    out_str[0] = '\0';

    if (in_len > 0)
    {

        while (i < in_len)
        {
            a = in_str[i];
            b = (i + 1 >= in_len) ? 0 : in_str[i + 1];
            c = (i + 2 >= in_len) ? 0 : in_str[i + 2];

            if (i + 2 < in_len)
            {
                out_str[curr_out_len++] = (base64[(a >> 2) & 0x3F]);
                out_str[curr_out_len++] = (base64[((a << 4) & 0x30)
                                                 + ((b >> 4) & 0xf)]);
                out_str[curr_out_len++] = (base64[((b << 2) & 0x3c)
                                                 + ((c >> 6) & 0x3)]);
                out_str[curr_out_len++] = (base64[c & 0x3F]);
            }
            else if (i + 1 < in_len)
            {
                out_str[curr_out_len++] = (base64[(a >> 2) & 0x3F]);
                out_str[curr_out_len++] = (base64[((a << 4) & 0x30)
                                                 + ((b >> 4) & 0xf)]);
                out_str[curr_out_len++] = (base64[((b << 2) & 0x3c)
                                                 + ((c >> 6) & 0x3)]);
                out_str[curr_out_len++] = '=';
            }
            else
            {
                out_str[curr_out_len++] = (base64[(a >> 2) & 0x3F]);
                out_str[curr_out_len++] = (base64[((a << 4) & 0x30)
                                                 + ((b >> 4) & 0xf)]);
                out_str[curr_out_len++] = '=';
                out_str[curr_out_len++] = '=';
            }

            i += 3;

            if((curr_out_len + 4) > out_len)
            {
                return(-1);
            }
				
        }
        out_str[curr_out_len] = '\0';
    }

    return curr_out_len;
}

/*
 * This routine decodes base64 string to a buffer.
 * Populates 'out_str' with b64 decoded data.  
 *
 * Returns number of bytes that were decoded.
 *   >0   -> OK
 *   -1   -> BAD (output buffer not big enough).
 */
PRInt32 Base64Decode(const char *in_str, unsigned char *out_str,
                                PRUint32* decoded_len)
{
  return Base64Decode_int(in_str, out_str, *decoded_len);
}

PRInt32 Base64Decode_int(const char *in_str, unsigned char *out_str,
                                PRUint32& decoded_len)
{
    PRInt32 in_len = strlen (/*(char *)*/ in_str);
    PRInt32 ii = 0;
    PRInt32 a = 0;
    char ch;
    PRInt32 b1 = 0;
    long b4 = 0;
    PRInt32 nn = 0;

    /* Decode remainder of base 64 string */

    while (ii < in_len)
    {
        ch = in_str[ii++];
        if (ch >= 'A' && ch <= 'Z') b1 = (ch - 'A');
        else if (ch >= 'a' && ch <= 'z') b1 = 26 + (ch - 'a');
        else if (ch >= '0' && ch <= '9') b1 = 52 + (ch - '0');
        else if (ch == '+') b1 = 62;
        else if (ch == '/') b1 = 63;
        else if (ch == '\r' || ch == '\n') continue;
        else
        {
            if (ch == '=')
            {
                if (nn == 3)
                {
                    if ((a + 2) > (PRInt32) decoded_len) 
                        return (-1); /* Bail.  Buffer overflow */
                    b4 = (b4 << 6);
                    out_str[a++] = (char) (0xff & (b4 >> 16));
                    out_str[a++] = (char) (0xff & (b4 >> 8));
                }
                else if (nn == 2)
                {
                    if ((a + 1) > (PRInt32) decoded_len)
                    {
                        return (-1); /* Bail.  Buffer overflow */
                    }
                    b4 = (b4 << 12);
                    out_str[a++] = (char) (0xff & (b4 >> 16));
                }
            }
            break;
        }
        b4 = (b4 << 6) | (long) b1;
        nn++;
        if (nn == 4)
        {
            if ((a + 3) > (PRInt32) decoded_len)
            {
                return (-1); /* Bail.  Buffer overflow */
            }
            out_str[a++] = (char) (0xff & (b4 >> 16));
            out_str[a++] = (char) (0xff & (b4 >> 8));
            out_str[a++] = (char) (0xff & (b4));
            nn = 0;
        }
    }

    out_str[a] = '\0';
    decoded_len = a;

    return (a);
}

NS_IMETHODIMP nsAbSyncPostEngine::GetInterface(const nsIID & aIID, void * *aInstancePtr)
{
   NS_ENSURE_ARG_POINTER(aInstancePtr);
   return QueryInterface(aIID, aInstancePtr);
}

// nsIURIContentListener support
NS_IMETHODIMP 
nsAbSyncPostEngine::OnStartURIOpen(nsIURI* aURI, PRBool* aAbortOpen)
{
   return NS_OK;
}

NS_IMETHODIMP 
nsAbSyncPostEngine::IsPreferred(const char * aContentType,
                                nsURILoadCommand aCommand,
                                char ** aDesiredContentType,
                                PRBool * aCanHandleContent)

{
  return CanHandleContent(aContentType, aCommand, aDesiredContentType,
                          aCanHandleContent);
}

NS_IMETHODIMP 
nsAbSyncPostEngine::CanHandleContent(const char * aContentType,
                                nsURILoadCommand aCommand,
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
                      nsIRequest *request,
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
nsAbSyncPostEngine::OnDataAvailable(nsIRequest *request, nsISupports * ctxt, nsIInputStream *aIStream, 
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
  mProtocolResponse.Append(NS_ConvertASCIItoUCS2(buf, readLen));
  PR_FREEIF(buf);
  mTotalWritten += readLen;

  if (!mAuthenticationRunning)
    NotifyListenersOnProgress(mTransactionID, mTotalWritten, 0);
  return NS_OK;
}


// Methods for nsIRequestObserver 
nsresult
nsAbSyncPostEngine::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  if (mAuthenticationRunning)
    NotifyListenersOnStartAuthOperation();
  else
    NotifyListenersOnStartSending(mTransactionID, mMessageSize);
  return NS_OK;
}

nsresult
nsAbSyncPostEngine::OnStopRequest(nsIRequest *request, nsISupports * /* ctxt */, nsresult aStatus)
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
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  if (channel)
  {
    char    *contentType = nsnull;
    char    *charset = nsnull;

    if (NS_SUCCEEDED(channel->GetContentType(&contentType)) && contentType)
    {
      if (PL_strcasecmp(contentType, UNKNOWN_CONTENT_TYPE))
      {
        mContentType = contentType;
      }
    }
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);
    if (httpChannel)
    {
      if (NS_SUCCEEDED(httpChannel->GetCharset(&charset)) && charset)
      {
        mCharset = charset;
      }
    }
  }  

  // set the state...
  mPostEngineState = nsIAbSyncPostEngineState::nsIAbSyncPostIdle;

  if (mAuthenticationRunning)
  {
    nsresult  rv;
    if (mSyncMojo)
      rv = mSyncMojo->GetAbSyncMojoResults(&mUser, &mCookie, &mMojoSyncSpec, &mMojoSyncPort);

    if (NS_SUCCEEDED(rv))
    {
      // Before we really get started...lets let sync know who is doing this...
	    nsCOMPtr<nsIAbSync> sync(do_GetService(kAbSync, &rv)); 
	    if (NS_SUCCEEDED(rv) || sync) 
        sync->SetAbSyncUser(mUser);

      // Base64 encode then url encode it...
      //
      char    tUser[256] = "";

      if (Base64Encode((unsigned char *)mUser, nsCRT::strlen(mUser), tUser, sizeof(tUser)) < 0)
      {
        rv = NS_ERROR_FAILURE;
        NotifyListenersOnStopAuthOperation(rv, tProtResponse);
        NotifyListenersOnStopSending(mTransactionID, rv, nsnull);
      }
      else 
      {
        char *tUser2 = nsEscape(tUser, url_Path);
        if (!tUser2)
        {
          rv = NS_ERROR_FAILURE;
          NotifyListenersOnStopAuthOperation(rv, tProtResponse);
          NotifyListenersOnStopSending(mTransactionID, rv, nsnull);
        }
        else
        {
          mSyncProtocolRequestPrefix = PR_smprintf("cn=%s&cc=%s&", tUser2, mCookie);
          PR_FREEIF(tUser2);
          NotifyListenersOnStopAuthOperation(aStatus, tProtResponse);
          KickTheSyncOperation();
        }
      }

      // RICHIE - Special here to show the server we are hitting!
      // RICHIE - REMOVE THIS BEFORE SHIPPING!!!!
#ifdef DEBUG
      PRUnichar *msgValue = nsnull;
      msgValue = nsTextFormatter::smprintf(nsString(NS_ConvertASCIItoUCS2("Server: %s - port %d")).get(),
                                           mMojoSyncSpec, mMojoSyncPort);
      NotifyListenersOnStatus(mTransactionID, msgValue);
      PR_FREEIF(msgValue);
      // RICHIE 
#endif
    }
    else
    {
      NotifyListenersOnStopAuthOperation(rv, tProtResponse);
      NotifyListenersOnStopSending(mTransactionID, rv, nsnull);
    }

    mSyncMojo = nsnull;
  }
  else
  {
    tProtResponse = mProtocolResponse.ToNewCString();
    NotifyListenersOnStopSending(mTransactionID, aStatus, tProtResponse);
  }

  PR_FREEIF(tProtResponse);

  // Time to return...
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
nsAbSyncPostEngine::NotifyListenersOnStopAuthOperation(nsresult aStatus, const char *aCookie)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnStopAuthOperation(aStatus, nsnull, aCookie);
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
                                                 char *aProtocolResponse)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnStopOperation(aTransactionID, aStatus, nsnull, aProtocolResponse);

  return NS_OK;
}

// Utility to create a nsIURI object...
extern "C" nsresult 
nsEngineNewURI(nsIURI** aInstancePtrResult, const char *aSpec, nsIURI *aBase)
{  
  nsresult  res;

  if (nsnull == aInstancePtrResult) 
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIIOService> pService(do_GetService(kIOServiceCID, &res));
  if (NS_FAILED(res)) 
    return NS_ERROR_FACTORY_NOT_REGISTERED;

  return pService->NewURI(aSpec, aBase, aInstancePtrResult);
}

nsresult
nsAbSyncPostEngine::FireURLRequest(nsIURI *aURL, const char *postData)
{
  nsresult rv;
  nsCOMPtr<nsIInputStream> postStream;

  if (!postData)
    return NS_ERROR_INVALID_ARG;

  NS_ENSURE_SUCCESS(NS_OpenURI(getter_AddRefs(mChannel), aURL, nsnull), NS_ERROR_FAILURE);

  // Tag the post stream onto the channel...but never seemed to work...so putting it
  // directly on the URL spec
  //
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
  if (!httpChannel)
    return NS_ERROR_FAILURE;

  if (NS_SUCCEEDED(rv = NS_NewPostDataStream(getter_AddRefs(postStream), PR_FALSE, postData, 0))){
    nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(httpChannel));
    NS_ASSERTION(uploadChannel, "http must support nsIUploadChannel");
    uploadChannel->SetUploadStream(postStream, nsnull, -1);
  }
  httpChannel->AsyncOpen(this, nsnull);

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
NS_IMETHODIMP nsAbSyncPostEngine::BuildMojoString(nsIDocShell *aRootDocShell, char **aID)
{
  nsresult        rv;

  if (!aID)
    return NS_ERROR_FAILURE;

  // Now, get the COMPtr to the Mojo!
  if (!mSyncMojo)
  {
    rv = nsComponentManager::CreateInstance(kCAbSyncMojoCID, NULL, NS_GET_IID(nsIAbSyncMojo), getter_AddRefs(mSyncMojo));
    if ( NS_FAILED(rv) || (!mSyncMojo) )
      return NS_ERROR_FAILURE;
  }

  rv = mSyncMojo->BuildMojoString(aRootDocShell, aID);
  return rv;
}

NS_IMETHODIMP nsAbSyncPostEngine::SendAbRequest(const char *aSpec, PRInt32 aPort, const char *aProtocolRequest, PRInt32 aTransactionID,
                                                nsIDocShell *aDocShell, const char *aUser)
{
  nsresult      rv;
  char          *mojoUser = nsnull;
  char          *mojoSnack = nsnull;

  // Only try if we are not currently busy!
  if (mPostEngineState != nsIAbSyncPostEngineState::nsIAbSyncPostIdle)
    return NS_ERROR_FAILURE;

  // Now, get the COMPtr to the Mojo!
  if (!mSyncMojo)
  {
    rv = nsComponentManager::CreateInstance(kCAbSyncMojoCID, NULL, NS_GET_IID(nsIAbSyncMojo), getter_AddRefs(mSyncMojo));
    if ( NS_FAILED(rv) || (!mSyncMojo) )
      return NS_ERROR_FAILURE;
  }

  if (aUser)
    mUser = nsCRT::strdup(aUser);
  if (NS_FAILED(mSyncMojo->StartAbSyncMojo(this, aDocShell, mUser)))
    return NS_ERROR_FAILURE;  

  // Set transaction ID and save/init Sync info...
  mTransactionID = aTransactionID;

  // Init stuff we need....
  mSyncProtocolRequest = nsCRT::strdup(aProtocolRequest);
  mProtocolResponse = NS_ConvertASCIItoUCS2("");
  mTotalWritten = 0;

  // The first thing we need to do is authentication so do it!
  mAuthenticationRunning = PR_TRUE;
  mPostEngineState = nsIAbSyncPostEngineState::nsIAbSyncAuthenticationRunning;
  return NS_OK;
}

NS_IMETHODIMP 
nsAbSyncPostEngine::KickTheSyncOperation(void)
{
  nsresult  rv;
  nsIURI    *workURI = nsnull;
  char      *protString = nsnull;

  // The first thing we need to do is authentication so do it!
  mAuthenticationRunning = PR_FALSE;
  mProtocolResponse = NS_ConvertASCIItoUCS2("");
  mPostEngineState = nsIAbSyncPostEngineState::nsIAbSyncPostRunning;

  char    *postHeader = "Content-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nCookie: %s\r\n\r\n%s";
  protString = PR_smprintf("%s%s", mSyncProtocolRequestPrefix, mSyncProtocolRequest);
  if (protString)
    mMessageSize = nsCRT::strlen(protString);
  else
    mMessageSize = 0;

  char *tCommand = PR_smprintf(postHeader, mMessageSize, mCookie, protString);
  PR_FREEIF(protString);

#ifdef DEBUG_rhp
  printf("COMMAND = %s\n", tCommand);
#endif

  if (!tCommand)
  {
    rv = NS_ERROR_OUT_OF_MEMORY; // we couldn't allocate the string 
    goto GetOuttaHere;
  }

  rv = nsEngineNewURI(&workURI, mMojoSyncSpec, nsnull);
  if (NS_FAILED(rv) || (!workURI))
  {
    rv = NS_ERROR_FAILURE;  // we couldn't allocate the string 
    goto GetOuttaHere;
  }

  if (mMojoSyncPort > 0)
    workURI->SetPort(mMojoSyncPort);

  rv = FireURLRequest(workURI, tCommand);

  if (NS_SUCCEEDED(rv))
    NotifyListenersOnStartSending(mTransactionID, mMessageSize);

GetOuttaHere:
  NS_IF_RELEASE(workURI);
  PR_FREEIF(tCommand);
  mPostEngineState = nsIAbSyncPostEngineState::nsIAbSyncPostRunning;
  return rv;
}

NS_IMETHODIMP 
nsAbSyncPostEngine::CancelAbSync()
{
  nsresult      rv = NS_ERROR_FAILURE;

  if (mSyncMojo)
  {
    rv = mSyncMojo->CancelTheMojo();
  }
  else if (mChannel)
  {
    rv = mChannel->Cancel(NS_BINDING_ABORTED);
  }

  return rv;
}

NS_IMETHODIMP 
nsAbSyncPostEngine::GetMojoUserAndSnack(char **aMojoUser, char **aMojoSnack)
{
  if ( (!mUser) || (!mCookie) )
    return NS_ERROR_FAILURE;

  *aMojoUser = nsCRT::strdup(mUser);
  *aMojoSnack = nsCRT::strdup(mCookie);

  if ( (!*aMojoUser) || (!*aMojoSnack) )
    return NS_ERROR_FAILURE;
  else
    return NS_OK;
}
