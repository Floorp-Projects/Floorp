/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsURLFetcher.h"

#include "msgCore.h" // for pre-compiled headers
#include "nsCOMPtr.h"
#include <stdio.h>
#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "comi18n.h"
#include "prmem.h"
#include "plstr.h"
#include "nsIComponentManager.h"
#include "nsString.h"
#include "nsIIOService.h"
#include "nsIChannel.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"
#include "nsIHttpChannel.h"
#include "nsIWebProgress.h"
#include "nsMsgAttachmentHandler.h"
#include "nsMsgSend.h"
#include "nsIStreamConverterService.h"

NS_IMPL_ISUPPORTS6(nsURLFetcher, nsIURLFetcher, nsIStreamListener, nsIURIContentListener, nsIInterfaceRequestor, nsIWebProgressListener, nsISupportsWeakReference)


/* 
 * Inherited methods for nsMimeConverter
 */
nsURLFetcher::nsURLFetcher()
{
#if defined(DEBUG_ducarroz)
  printf("CREATE nsURLFetcher: %x\n", this);
#endif

  // Init member variables...
  mTotalWritten = 0;
  mBuffer = nsnull;
  mBufferSize = 0;
  mStillRunning = PR_TRUE;
  mCallback = nsnull;
  mOnStopRequestProcessed = PR_FALSE;
  mIsFile=PR_FALSE;
  nsURLFetcherStreamConsumer *consumer = new nsURLFetcherStreamConsumer(this);
  mConverter = do_QueryInterface(consumer);
}

nsURLFetcher::~nsURLFetcher()
{
#if defined(DEBUG_ducarroz)
  printf("DISPOSE nsURLFetcher: %x\n", this);
#endif
  mStillRunning = PR_FALSE;
  
  PR_FREEIF(mBuffer);
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
nsURLFetcher::OnStartURIOpen(nsIURI* aURI, PRBool* aAbortOpen)
{
   return NS_OK;
}

NS_IMETHODIMP 
nsURLFetcher::IsPreferred(const char * aContentType,
                                char ** aDesiredContentType,
                                PRBool * aCanHandleContent)

{
  return CanHandleContent(aContentType, PR_TRUE, aDesiredContentType,
                          aCanHandleContent);
}

NS_IMETHODIMP 
nsURLFetcher::CanHandleContent(const char * aContentType,
                                PRBool aIsContentPreferred,
                                char ** aDesiredContentType,
                                PRBool * aCanHandleContent)

{
    if (!mIsFile && nsCRT::strcasecmp(aContentType, MESSAGE_RFC822) == 0)
      *aDesiredContentType = nsCRT::strdup("text/html");

    // since we explicilty loaded the url, we always want to handle it!
    *aCanHandleContent = PR_TRUE;
  return NS_OK;
} 

NS_IMETHODIMP 
nsURLFetcher::DoContent(const char * aContentType,
                      PRBool aIsContentPreferred,
                      nsIRequest *request,
                      nsIStreamListener ** aContentHandler,
                      PRBool * aAbortProcess)
{
  nsresult rv = NS_OK;

  if (aAbortProcess)
    *aAbortProcess = PR_FALSE;
  QueryInterface(NS_GET_IID(nsIStreamListener), (void **) aContentHandler);

  /*
    Check the content-type to see if we need to insert a converter
  */
  if (nsCRT::strcasecmp(aContentType, UNKNOWN_CONTENT_TYPE) == 0 ||
        nsCRT::strcasecmp(aContentType, MULTIPART_MIXED_REPLACE) == 0 ||
        nsCRT::strcasecmp(aContentType, MULTIPART_MIXED) == 0 ||
        nsCRT::strcasecmp(aContentType, MULTIPART_BYTERANGES) == 0)
  {
    rv = InsertConverter(aContentType);
    if (NS_SUCCEEDED(rv))
      mConverterContentType.Adopt(nsCRT::strdup(aContentType));
  }

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
      webProgress->AddProgressListener(this, nsIWebProgress::NOTIFY_STATE_ALL);
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
nsURLFetcher::OnDataAvailable(nsIRequest *request, nsISupports * ctxt, nsIInputStream *aIStream, 
                              PRUint32 sourceOffset, PRUint32 aLength)
{
  /* let our converter or consumer process the data */
  if (!mConverter)
    return NS_ERROR_FAILURE;

  return mConverter->OnDataAvailable(request, ctxt, aIStream, sourceOffset, aLength);
}


// Methods for nsIStreamObserver 
nsresult
nsURLFetcher::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  /* check if the user has canceld the operation */
  nsMsgAttachmentHandler *attachmentHdl = (nsMsgAttachmentHandler *)mTagData;
  if (attachmentHdl)
  {
    nsCOMPtr<nsIMsgSend> sendPtr;
    attachmentHdl->GetMimeDeliveryState(getter_AddRefs(sendPtr));
    if (sendPtr)
    {
      nsCOMPtr<nsIMsgProgress> progress;
      sendPtr->GetProgress(getter_AddRefs(progress));
      if (progress)
      {
        PRBool cancel = PR_FALSE;
        progress->GetProcessCanceledByUser(&cancel);
        if (cancel)
          return request->Cancel(NS_ERROR_ABORT);
      }
    }
    attachmentHdl->mRequest = request;
  }

  /* call our converter or consumer */
  if (mConverter)
    return mConverter->OnStartRequest(request, ctxt);

  return NS_OK;
}

NS_IMETHODIMP
nsURLFetcher::OnStopRequest(nsIRequest *request, nsISupports * ctxt, nsresult aStatus)
{
#if defined(DEBUG_ducarroz)
  printf("nsURLFetcher::OnStopRequest()\n");
#endif

  nsresult rv = NS_OK;

  // it's possible we could get in here from the channel calling us with an OnStopRequest and from our
  // onStatusChange method (in the case of an error). So we should protect against this to make sure we
  // don't process the on stop request twice...

  if (mOnStopRequestProcessed)
    return NS_OK;
  mOnStopRequestProcessed = PR_TRUE;
  
  /* first, call our converter or consumer */
  if (mConverter)
    rv = mConverter->OnStopRequest(request, ctxt, aStatus);

  nsMsgAttachmentHandler *attachmentHdl = (nsMsgAttachmentHandler *)mTagData;
  if (attachmentHdl)
    attachmentHdl->mRequest = nsnull;

  //
  // Now complete the stream!
  //
  mStillRunning = PR_FALSE;

  // time to close the output stream...
  if (mOutStream)
  {
    mOutStream->Close();
    mOutStream = nsnull;
  
    /* In case of multipart/x-mixed-replace, we need to truncate the file to the current part size */
    if (PL_strcasecmp(mConverterContentType, MULTIPART_MIXED_REPLACE) == 0)
  {
      PRInt64 fileSize;
      LL_I2L(fileSize, mTotalWritten);
      mLocalFile->SetFileSize(fileSize);
    }
  }

  // Now if there is a callback, we need to call it...
  if (mCallback)
    mCallback (aStatus, (const char *)mContentType, (const char *)mCharset, mTotalWritten, nsnull, mTagData);

  // Time to return...
  return NS_OK;
}

nsresult 
nsURLFetcher::Initialize(nsILocalFile *localFile, 
                         nsIFileOutputStream *outputStream,
                         nsAttachSaveCompletionCallback cb, 
                         void *tagData)
{
  if (!outputStream || !localFile)
    return NS_ERROR_INVALID_ARG;

  mOutStream = outputStream;
  mLocalFile = localFile;
  mCallback = cb;     //JFD: Please, no more callback, use a listener...
  mTagData = tagData; //JFD: TODO, WE SHOULD USE A NSCOMPTR to hold this stuff!!!
  return NS_OK;
}

nsresult
nsURLFetcher::FireURLRequest(nsIURI *aURL, nsILocalFile *localFile, nsIFileOutputStream *outputStream, 
                             nsAttachSaveCompletionCallback cb, void *tagData)
{
  nsresult rv;

  rv = Initialize(localFile, outputStream, cb, tagData);
  NS_ENSURE_SUCCESS(rv, rv);

  //check to see if aURL is a local file or not
  aURL->SchemeIs("file", &mIsFile);
  
  // we're about to fire a new url request so make sure the on stop request flag is cleared...
  mOnStopRequestProcessed = PR_FALSE;

  // let's try uri dispatching...
  nsCOMPtr<nsIURILoader> pURILoader (do_GetService(NS_URI_LOADER_CONTRACTID));
  NS_ENSURE_TRUE(pURILoader, NS_ERROR_FAILURE);

  nsCOMPtr<nsIChannel> channel;
  nsCOMPtr<nsILoadGroup> loadGroup;
  pURILoader->GetLoadGroupForContext(this, getter_AddRefs(loadGroup));
  NS_ENSURE_SUCCESS(NS_NewChannel(getter_AddRefs(channel), aURL, nsnull, loadGroup, this), NS_ERROR_FAILURE);
 
  return pURILoader->OpenURI(channel, PR_FALSE, this);
}

nsresult
nsURLFetcher::InsertConverter(const char * aContentType)
{
  nsresult rv;

  nsCOMPtr<nsIStreamConverterService> convServ(do_GetService("@mozilla.org/streamConverters;1", &rv));
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIStreamListener> toListener(mConverter);
    nsCOMPtr<nsIStreamListener> fromListener;
    nsAutoString contentType;
    
    contentType.AssignWithConversion(aContentType);
    rv = convServ->AsyncConvertData(contentType.get(),
                                    NS_LITERAL_STRING("*/*").get(),
                                    toListener,
                                    nsnull,
                                    getter_AddRefs(fromListener));
    if (NS_SUCCEEDED(rv))
      mConverter = fromListener;
  }

  return rv;
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
                          PRUint32 aStateFlags, nsresult aStatus)
{
  // all we care about is the case where an error occurred (as in we were unable to locate the
  // the url....

  if (NS_FAILED(aStatus))
    OnStopRequest(aRequest, nsnull, aStatus);

  return NS_OK;
}

NS_IMETHODIMP
nsURLFetcher::OnLocationChange(nsIWebProgress* aWebProgress,
                               nsIRequest* aRequest,
                               nsIURI *aURI)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP 
nsURLFetcher::OnStatusChange(nsIWebProgress* aWebProgress,
                             nsIRequest* aRequest,
                             nsresult aStatus,
                             const PRUnichar* aMessage)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP 
nsURLFetcher::OnSecurityChange(nsIWebProgress *aWebProgress, 
                               nsIRequest *aRequest, 
                               PRUint32 state)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}


/**
 * Stream consumer used for handling special content type like multipart/x-mixed-replace
 */

NS_IMPL_ISUPPORTS2(nsURLFetcherStreamConsumer, nsIStreamListener, nsIRequestObserver)

nsURLFetcherStreamConsumer::nsURLFetcherStreamConsumer(nsURLFetcher* urlFetcher) :
  mURLFetcher(urlFetcher)
{
#if defined(DEBUG_ducarroz)
  printf("CREATE nsURLFetcherStreamConsumer: %x\n", this);
#endif
}

nsURLFetcherStreamConsumer::~nsURLFetcherStreamConsumer()
{
#if defined(DEBUG_ducarroz)
  printf("DISPOSE nsURLFetcherStreamConsumer: %x\n", this);
#endif
}

/** nsIRequestObserver methods **/

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP nsURLFetcherStreamConsumer::OnStartRequest(nsIRequest *aRequest, nsISupports *ctxt)
{
  if (!mURLFetcher || !mURLFetcher->mOutStream)
    return NS_ERROR_FAILURE;

  /* In case of multipart/x-mixed-replace, we need to erase the output file content */
  if (PL_strcasecmp(mURLFetcher->mConverterContentType, MULTIPART_MIXED_REPLACE) == 0)
  {
    nsCOMPtr<nsISeekableStream> seekStream = do_QueryInterface(mURLFetcher->mOutStream);
    if (seekStream)
      seekStream->Seek(nsISeekableStream::NS_SEEK_SET, 0);
    mURLFetcher->mTotalWritten = 0;
  }

  return NS_OK;
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt, in nsresult status); */
NS_IMETHODIMP nsURLFetcherStreamConsumer::OnStopRequest(nsIRequest *aRequest, nsISupports *ctxt, nsresult status)
{
  if (!mURLFetcher)
    return NS_ERROR_FAILURE;

  // Check the content type!
  nsCAutoString contentType;
  nsCAutoString charset;

  nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(aRequest);
  if(!aChannel) return NS_ERROR_FAILURE;

  if (NS_SUCCEEDED(aChannel->GetContentType(contentType)) &&
      !contentType.EqualsLiteral(UNKNOWN_CONTENT_TYPE))
  {
    mURLFetcher->mContentType = contentType;
  }

  if (NS_SUCCEEDED(aChannel->GetContentCharset(charset)) && !charset.IsEmpty())
  {
    mURLFetcher->mCharset = charset;
  }

  return NS_OK;
}

/** nsIStreamListener methods **/

/* void onDataAvailable (in nsIRequest request, in nsISupports ctxt, in nsIInputStream inStr, in unsigned long sourceOffset, in unsigned long count); */
NS_IMETHODIMP nsURLFetcherStreamConsumer::OnDataAvailable(nsIRequest *aRequest, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
  PRUint32        readLen = count;
  PRUint32        wroteIt;

  if (!mURLFetcher)
    return NS_ERROR_FAILURE;

  if (!mURLFetcher->mOutStream)
    return NS_ERROR_INVALID_ARG;

  if (mURLFetcher->mBufferSize < count)
  {
    PR_FREEIF(mURLFetcher->mBuffer);

    if (count > 0x1000)
      mURLFetcher->mBufferSize = count;
    else
      mURLFetcher->mBufferSize = 0x1000;

    mURLFetcher->mBuffer = (char *)PR_Malloc(mURLFetcher->mBufferSize);
    if (!mURLFetcher->mBuffer)
      return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
  }

  // read the data from the input stram...
  nsresult rv = inStr->Read(mURLFetcher->mBuffer, count, &readLen);
  if (NS_FAILED(rv))
    return rv;

  // write to the output file...
  mURLFetcher->mOutStream->Write(mURLFetcher->mBuffer, readLen, &wroteIt);

  if (wroteIt != readLen)
    return NS_ERROR_FAILURE;
  else
  {
    mURLFetcher->mTotalWritten += wroteIt;
    return NS_OK;
  }
}
