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
 */

#include "nsDOMParser.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsIInputStream.h"
#include "nsNetUtil.h"
#include "nsIByteArrayInputStream.h"

#include "nsIXPConnect.h"
#include "nsIUnicodeEncoder.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsLayoutCID.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIPrivateDOMImplementation.h"
#include "nsIJSContextStack.h"
#include "nsIScriptSecurityManager.h"
#include "nsICodebasePrincipal.h"

static const char* kLoadAsData = "loadAsData";

static NS_DEFINE_CID(kIDOMDOMImplementationCID, NS_DOM_IMPLEMENTATION_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

/////////////////////////////////////////////
//
//
/////////////////////////////////////////////

class nsDOMParserChannel : public nsIChannel {
public:
  nsDOMParserChannel(nsIURI* aURI, const char* aContentType);
  virtual ~nsDOMParserChannel();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUEST
  NS_DECL_NSICHANNEL

protected:
  nsCString mContentType;
  nsresult mStatus;
  PRInt32 mContentLength;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsISupports> mOwner;
  nsCOMPtr<nsILoadGroup> mLoadGroup;
};

nsDOMParserChannel::nsDOMParserChannel(nsIURI* aURI, const char* aContentType)
{
  NS_INIT_ISUPPORTS();
  mURI = aURI;
  mContentType.Assign(aContentType);
  mStatus = NS_OK;
  mContentLength = -1;
}

nsDOMParserChannel::~nsDOMParserChannel()
{
}

NS_IMPL_ISUPPORTS2(nsDOMParserChannel, nsIRequest, nsIChannel)

/* boolean isPending (); */
NS_IMETHODIMP nsDOMParserChannel::GetName(PRUnichar* *result)
{
    NS_NOTREACHED("nsDOMParserChannel::GetName");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsDOMParserChannel::IsPending(PRBool *_retval)
{
  NS_ENSURE_ARG(_retval);
  *_retval = PR_FALSE;
  return NS_OK;
}

/* readonly attribute nsresult status; */
NS_IMETHODIMP 
nsDOMParserChannel::GetStatus(nsresult *aStatus)
{
  NS_ENSURE_ARG(aStatus);
  *aStatus = mStatus;
  return NS_OK;
}

/* void cancel (in nsresult status); */
NS_IMETHODIMP 
nsDOMParserChannel::Cancel(nsresult status)
{
  mStatus = status;
  return NS_OK;
}

/* void suspend (); */
NS_IMETHODIMP 
nsDOMParserChannel::Suspend()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void resume (); */
NS_IMETHODIMP 
nsDOMParserChannel::Resume()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsIURI originalURI; */
NS_IMETHODIMP 
nsDOMParserChannel::GetOriginalURI(nsIURI * *aOriginalURI)
{
  NS_ENSURE_ARG_POINTER(aOriginalURI);
  *aOriginalURI = mURI;
  NS_ADDREF(*aOriginalURI);
  return NS_OK;
}
NS_IMETHODIMP nsDOMParserChannel::SetOriginalURI(nsIURI * aOriginalURI)
{
  mURI = aOriginalURI;
  return NS_OK;
}

/* attribute nsIURI URI; */
NS_IMETHODIMP nsDOMParserChannel::GetURI(nsIURI * *aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  *aURI = mURI;
  NS_ADDREF(*aURI);
  return NS_OK;
}
NS_IMETHODIMP nsDOMParserChannel::SetURI(nsIURI * aURI)
{
  mURI = aURI;
  return NS_OK;
}

/* attribute unsigned long transferOffset; */
NS_IMETHODIMP nsDOMParserChannel::GetTransferOffset(PRUint32 *aTransferOffset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsDOMParserChannel::SetTransferOffset(PRUint32 aTransferOffset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute long transferCount; */
NS_IMETHODIMP nsDOMParserChannel::GetTransferCount(PRInt32 *aTransferCount)
{
  NS_ENSURE_ARG(aTransferCount);
  *aTransferCount = -1;
  return NS_OK;
}
NS_IMETHODIMP nsDOMParserChannel::SetTransferCount(PRInt32 aTransferCount)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsLoadFlags loadAttributes; */
NS_IMETHODIMP nsDOMParserChannel::GetLoadAttributes(nsLoadFlags *aLoadAttributes)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsDOMParserChannel::SetLoadAttributes(nsLoadFlags aLoadAttributes)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute string contentType; */
NS_IMETHODIMP nsDOMParserChannel::GetContentType(char * *aContentType)
{
  NS_ENSURE_ARG_POINTER(aContentType);
  *aContentType = mContentType.ToNewCString();
  return NS_OK;
}
NS_IMETHODIMP nsDOMParserChannel::SetContentType(const char * aContentType)
{
  NS_ENSURE_ARG(aContentType);
  mContentType.Assign(aContentType);
  return NS_OK;
}

/* attribute long contentLength; */
NS_IMETHODIMP nsDOMParserChannel::GetContentLength(PRInt32 *aContentLength)
{
  NS_ENSURE_ARG(aContentLength);
  *aContentLength = mContentLength;
  return NS_OK;
}
NS_IMETHODIMP nsDOMParserChannel::SetContentLength(PRInt32 aContentLength)
{
  mContentLength = aContentLength;
  return NS_OK;
}

/* attribute nsISupports owner; */
NS_IMETHODIMP nsDOMParserChannel::GetOwner(nsISupports * *aOwner)
{
  NS_ENSURE_ARG_POINTER(aOwner);
  *aOwner = mOwner;
  NS_IF_ADDREF(*aOwner);
  return NS_OK;
}
NS_IMETHODIMP nsDOMParserChannel::SetOwner(nsISupports * aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

/* attribute nsILoadGroup loadGroup; */
NS_IMETHODIMP nsDOMParserChannel::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
  NS_ENSURE_ARG_POINTER(aLoadGroup);
  *aLoadGroup = mLoadGroup;
  NS_IF_ADDREF(*aLoadGroup);
  return NS_OK;
}
NS_IMETHODIMP nsDOMParserChannel::SetLoadGroup(nsILoadGroup * aLoadGroup)
{
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

/* attribute nsIInterfaceRequestor notificationCallbacks; */
NS_IMETHODIMP nsDOMParserChannel::GetNotificationCallbacks(nsIInterfaceRequestor * *aNotificationCallbacks)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsDOMParserChannel::SetNotificationCallbacks(nsIInterfaceRequestor * aNotificationCallbacks)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsISupports securityInfo; */
NS_IMETHODIMP nsDOMParserChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute unsigned long bufferSegmentSize; */
NS_IMETHODIMP nsDOMParserChannel::GetBufferSegmentSize(PRUint32 *aBufferSegmentSize)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsDOMParserChannel::SetBufferSegmentSize(PRUint32 aBufferSegmentSize)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute unsigned long bufferMaxSize; */
NS_IMETHODIMP nsDOMParserChannel::GetBufferMaxSize(PRUint32 *aBufferMaxSize)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsDOMParserChannel::SetBufferMaxSize(PRUint32 aBufferMaxSize)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIFile localFile; */
NS_IMETHODIMP nsDOMParserChannel::GetLocalFile(nsIFile * *aLocalFile)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute boolean pipeliningAllowed; */
NS_IMETHODIMP nsDOMParserChannel::GetPipeliningAllowed(PRBool *aPipeliningAllowed)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsDOMParserChannel::SetPipeliningAllowed(PRBool aPipeliningAllowed)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIInputStream openInputStream (); */
NS_IMETHODIMP nsDOMParserChannel::OpenInputStream(nsIInputStream **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIOutputStream openOutputStream (); */
NS_IMETHODIMP nsDOMParserChannel::OpenOutputStream(nsIOutputStream **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void asyncRead (in nsIStreamListener listener, in nsISupports ctxt); */
NS_IMETHODIMP nsDOMParserChannel::AsyncRead(nsIStreamListener *listener, nsISupports *ctxt)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void asyncWrite (in nsIInputStream fromStream, in nsIStreamObserver observer, in nsISupports ctxt); */
NS_IMETHODIMP nsDOMParserChannel::AsyncWrite(nsIInputStream *fromStream, nsIStreamObserver *observer, nsISupports *ctxt)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/////////////////////////////////////////////
//
//
/////////////////////////////////////////////

nsDOMParser::nsDOMParser()
{
  NS_INIT_ISUPPORTS();
}

nsDOMParser::~nsDOMParser()
{
}

NS_IMPL_ISUPPORTS2(nsDOMParser, nsIDOMParser, nsISecurityCheckedComponent)

static nsresult
ConvertWStringToStream(const PRUnichar* aStr,
                       PRInt32 aLength,
                       nsIInputStream** aStream,
                       PRInt32* aContentLength)
{
  nsresult rv;
  nsCOMPtr<nsIUnicodeEncoder> encoder;
  nsAutoString charsetStr;
  char* charBuf;

  // We want to encode the string as utf-8, so get the right encoder
  NS_WITH_SERVICE(nsICharsetConverterManager,
                  charsetConv, 
                  kCharsetConverterManagerCID,
                  &rv);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  
  charsetStr.AssignWithConversion("UTF-8");
  rv = charsetConv->GetUnicodeEncoder(&charsetStr,
                                      getter_AddRefs(encoder));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  
  // Convert to utf-8
  PRInt32 charLength;
  const PRUnichar* unicodeBuf = aStr;
  PRInt32 unicodeLength = aLength;
    
  rv = encoder->GetMaxLength(unicodeBuf, unicodeLength, &charLength);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  
  charBuf = (char*)nsMemory::Alloc(charLength + 1);
  if (!charBuf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  rv = encoder->Convert(unicodeBuf, 
                        &unicodeLength, 
                        charBuf, 
                        &charLength);
  if (NS_FAILED(rv)) {
    nsMemory::Free(charBuf);
    return NS_ERROR_FAILURE;
  }

  // The new stream takes ownership of the buffer
  rv = NS_NewByteArrayInputStream((nsIByteArrayInputStream**)aStream, 
                                  charBuf, 
                                  charLength);
  if (NS_FAILED(rv)) {
    nsMemory::Free(charBuf);
    return NS_ERROR_FAILURE;
  }
  
  *aContentLength = charLength;

  return NS_OK;
}

/* nsIDOMDocument parseFromString (in wstring str, in string contentType); */
NS_IMETHODIMP 
nsDOMParser::ParseFromString(const PRUnichar *str, 
                             const char *contentType, 
                             nsIDOMDocument **_retval)
{
  NS_ENSURE_ARG(str);
  NS_ENSURE_ARG(contentType);
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = NS_OK;
  nsCOMPtr<nsIInputStream> stream;
  PRInt32 contentLength;

  rv = ConvertWStringToStream(str, nsCRT::strlen(str), getter_AddRefs(stream), &contentLength);
  if (NS_FAILED(rv)) return rv;

  return ParseFromStream(stream, "UTF-8", contentLength, contentType, _retval);
}

/* nsIDOMDocument parseFromStream (in nsIInputStream stream, in string charset, in string contentType); */
NS_IMETHODIMP 
nsDOMParser::ParseFromStream(nsIInputStream *stream, 
                             const char *charset, 
                             PRInt32 contentLength,
                             const char *contentType, 
                             nsIDOMDocument **_retval)
{
  NS_ENSURE_ARG(stream);
  NS_ENSURE_ARG(charset);
  NS_ENSURE_ARG(contentType);
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv;
  nsCOMPtr<nsIURI> baseURI;
  nsCOMPtr<nsIPrincipal> principal;

  // For now, we can only create XML documents.
  if (nsCRT::strcmp(contentType, "text/xml") != 0) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // First try to find a base URI for the document we're creating
  nsCOMPtr<nsIXPCNativeCallContext> cc;
  NS_WITH_SERVICE(nsIXPConnect, xpc, nsIXPConnect::GetCID(), &rv);
  if(NS_SUCCEEDED(rv)) {
    rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
  }

  if (NS_SUCCEEDED(rv) && cc) {
    JSContext* cx;
    rv = cc->GetJSContext(&cx);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    
    NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
    if (NS_SUCCEEDED(rv)) {
      rv = secMan->GetSubjectPrincipal(getter_AddRefs(principal));
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsICodebasePrincipal> codebase = do_QueryInterface(principal);
        if (codebase) {
          codebase->GetURI(getter_AddRefs(baseURI));
        }
      }
    }
  }

  // Get and initialize a DOMImplementation
  nsCOMPtr<nsIDOMDOMImplementation> implementation = do_CreateInstance(kIDOMDOMImplementationCID, &rv);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  
  if (baseURI) {
    nsCOMPtr<nsIPrivateDOMImplementation> privImpl = do_QueryInterface(implementation);
    if (privImpl) {
      privImpl->Init(baseURI);
    }
  }

  // Create an empty document from it
  nsCOMPtr<nsIDOMDocument> domDocument;
  nsAutoString emptyStr;
  rv = implementation->CreateDocument(emptyStr, 
                                      emptyStr, 
                                      nsnull, 
                                      getter_AddRefs(domDocument));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  // Create a fake channel 
  nsDOMParserChannel* parserChannel = new nsDOMParserChannel(baseURI, contentType);
  if (!parserChannel) return NS_ERROR_OUT_OF_MEMORY;

  // Hold a reference to it in this method
  nsCOMPtr<nsIChannel> channel = NS_STATIC_CAST(nsIChannel*, parserChannel);
  if (principal) {
    channel->SetOwner(principal);
  }

  // Tell the document to start loading
  nsCOMPtr<nsIStreamListener> listener;
  nsCOMPtr<nsIDocument> document = do_QueryInterface(domDocument);

  if (!document) return NS_ERROR_FAILURE;
  rv = document->StartDocumentLoad(kLoadAsData, channel, 
                                   nsnull, nsnull, 
                                   getter_AddRefs(listener),
                                   PR_FALSE);

  if (NS_FAILED(rv) || !listener) return NS_ERROR_FAILURE;

  // Now start pumping data to the listener
  nsresult status;

  rv = listener->OnStartRequest(channel, nsnull);
  channel->GetStatus(&status);

  if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(status)) {
    rv = listener->OnDataAvailable(channel, nsnull, stream, 0, contentLength);
    channel->GetStatus(&status);
  }

  rv = listener->OnStopRequest(channel, nsnull, status, nsnull);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  *_retval = domDocument;
  NS_ADDREF(*_retval);

  return NS_OK;
}

static const char* kAllAccess = "AllAccess";

/* string canCreateWrapper (in nsIIDPtr iid); */
NS_IMETHODIMP 
nsDOMParser::CanCreateWrapper(const nsIID * iid, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsIDOMParser))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canCallMethod (in nsIIDPtr iid, in wstring methodName); */
NS_IMETHODIMP 
nsDOMParser::CanCallMethod(const nsIID * iid, const PRUnichar *methodName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsIDOMParser))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsDOMParser::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsIDOMParser))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canSetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsDOMParser::CanSetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsIDOMParser))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}
