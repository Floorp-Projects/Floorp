/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsXMLHttpRequest.h"
#include "nsISimpleEnumerator.h"
#include "nsIXPConnect.h"
#include "nsIByteArrayInputStream.h"
#include "nsIUnicodeEncoder.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsLayoutCID.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIPrivateDOMImplementation.h"
#include "nsXPIDLString.h"
#include "nsIURI.h"
#include "nsILoadGroup.h"
#include "nsNetUtil.h"
#include "nsIUploadChannel.h"
#include "nsIDOMSerializer.h"
#include "nsISupportsPrimitives.h"
#include "nsIDOMEventReceiver.h"
#include "nsIEventListenerManager.h"
#include "nsGUIEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "prprf.h"
#include "nsIDOMEventListener.h"
#include "nsIJSContextStack.h"
#include "nsIScriptSecurityManager.h"
#include "nsICodebasePrincipal.h"
#include "nsWeakPtr.h"
#include "nsICharsetAlias.h"
#ifdef IMPLEMENT_SYNC_LOAD
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIEventQueueService.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#endif
#include "nsIDOMClassInfo.h"
#include "nsIDOMElement.h"
#include "nsIParser.h"
#include "nsLoadListenerProxy.h"

static const char* kLoadAsData = "loadAsData";
#define LOADSTR NS_LITERAL_STRING("load")
#define ERRORSTR NS_LITERAL_STRING("error")

// CIDs
static NS_DEFINE_CID(kIDOMDOMImplementationCID, NS_DOM_IMPLEMENTATION_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
#ifdef IMPLEMENT_SYNC_LOAD
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
#endif

static void
GetCurrentContext(nsIScriptContext **aScriptContext)
{
  *aScriptContext = nsnull;

  // Get JSContext from stack.
  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1");

  if (!stack) {
    return;
  }

  JSContext *cx;

  if (NS_FAILED(stack->Peek(&cx))) {
    return;
  }

  if (!cx) {
    return;
  }

  nsISupports *priv = (nsISupports *)::JS_GetContextPrivate(cx);

  if (!priv) {
    return;
  }

  CallQueryInterface(priv, aScriptContext);

  return;
}


/////////////////////////////////////////////
//
//
/////////////////////////////////////////////

nsXMLHttpRequest::nsXMLHttpRequest()
{
  NS_INIT_ISUPPORTS();
  ChangeState(XML_HTTP_REQUEST_UNINITIALIZED,PR_FALSE);
  mAsync = PR_TRUE;
}

nsXMLHttpRequest::~nsXMLHttpRequest()
{
  if (XML_HTTP_REQUEST_SENT == mStatus) {
    Abort();
  }    
#ifdef IMPLEMENT_SYNC_LOAD
  if (mChromeWindow) {
    mChromeWindow->ExitModalEventLoop(NS_OK);
  }
#endif
}


// QueryInterface implementation for nsXMLHttpRequest
NS_INTERFACE_MAP_BEGIN(nsXMLHttpRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXMLHttpRequest)
  NS_INTERFACE_MAP_ENTRY(nsIXMLHttpRequest)
  NS_INTERFACE_MAP_ENTRY(nsIJSXMLHttpRequest)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLoadListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_DOM_CLASSINFO(XMLHttpRequest)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsXMLHttpRequest)
NS_IMPL_RELEASE(nsXMLHttpRequest)


/* void addEventListener (in string type, in nsIDOMEventListener
   listener); */
NS_IMETHODIMP
nsXMLHttpRequest::AddEventListener(const nsAReadableString& type,
                                   nsIDOMEventListener *listener,
                                   PRBool useCapture)
{
  NS_ENSURE_ARG(listener);
  nsresult rv;

  // I know, I know - strcmp's. But it's only for a couple of 
  // cases...and they are short strings. :-)
  if (type.Equals(LOADSTR)) {
    if (!mLoadEventListeners) {
      rv = NS_NewISupportsArray(getter_AddRefs(mLoadEventListeners));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    mLoadEventListeners->AppendElement(listener);
  }
  else if (type.Equals(ERRORSTR)) {
    if (!mErrorEventListeners) {
      rv = NS_NewISupportsArray(getter_AddRefs(mErrorEventListeners));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    mErrorEventListeners->AppendElement(listener);
  }
  else {
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

/* void removeEventListener (in string type, in nsIDOMEventListener
   listener); */
NS_IMETHODIMP 
nsXMLHttpRequest::RemoveEventListener(const nsAReadableString & type,
                                      nsIDOMEventListener *listener,
                                      PRBool useCapture)
{
  NS_ENSURE_ARG(listener);

  if (type.Equals(LOADSTR)) {
    if (mLoadEventListeners) {
      mLoadEventListeners->RemoveElement(listener);
    }
  }
  else if (type.Equals(ERRORSTR)) {
    if (mErrorEventListeners) {
      mErrorEventListeners->RemoveElement(listener);
    }
  }
  else {
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

/* void dispatchEvent (in nsIDOMEvent evt); */
NS_IMETHODIMP
nsXMLHttpRequest::DispatchEvent(nsIDOMEvent *evt)
{
  // Ignored

  return NS_OK;
}

/* attribute nsIOnReadystatechangeHandler onreadystatechange; */
NS_IMETHODIMP 
nsXMLHttpRequest::GetOnreadystatechange(nsIOnReadystatechangeHandler * *aOnreadystatechange)
{
  NS_ENSURE_ARG_POINTER(aOnreadystatechange);

  *aOnreadystatechange = mOnReadystatechangeListener;

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLHttpRequest::SetOnreadystatechange(nsIOnReadystatechangeHandler * aOnreadystatechange)
{
  mOnReadystatechangeListener = aOnreadystatechange;

  GetCurrentContext(getter_AddRefs(mScriptContext));

  return NS_OK;
}


/* attribute nsIDOMEventListener onload; */
NS_IMETHODIMP 
nsXMLHttpRequest::GetOnload(nsIDOMEventListener * *aOnLoad)
{
  NS_ENSURE_ARG_POINTER(aOnLoad);

  *aOnLoad = mOnLoadListener;

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLHttpRequest::SetOnload(nsIDOMEventListener * aOnLoad)
{
  mOnLoadListener = aOnLoad;

  GetCurrentContext(getter_AddRefs(mScriptContext));

  return NS_OK;
}

/* attribute nsIDOMEventListener onerror; */
NS_IMETHODIMP 
nsXMLHttpRequest::GetOnerror(nsIDOMEventListener * *aOnerror)
{
  NS_ENSURE_ARG_POINTER(aOnerror);

  *aOnerror = mOnErrorListener;

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLHttpRequest::SetOnerror(nsIDOMEventListener * aOnerror)
{
  mOnErrorListener = aOnerror;

  GetCurrentContext(getter_AddRefs(mScriptContext));

  return NS_OK;
}

/* readonly attribute nsIChannel channel; */
NS_IMETHODIMP nsXMLHttpRequest::GetChannel(nsIChannel **aChannel)
{
  NS_ENSURE_ARG_POINTER(aChannel);
  *aChannel = mChannel;
  NS_IF_ADDREF(*aChannel);

  return NS_OK;
}

/* readonly attribute nsIDOMDocument responseXML; */
NS_IMETHODIMP nsXMLHttpRequest::GetResponseXML(nsIDOMDocument **aResponseXML)
{
  NS_ENSURE_ARG_POINTER(aResponseXML);
  *aResponseXML = nsnull;
  if ((XML_HTTP_REQUEST_COMPLETED == mStatus) && mDocument) {
    *aResponseXML = mDocument;
    NS_ADDREF(*aResponseXML);
  }

  return NS_OK;
}

/*
 * This piece copied from nsXMLDocument, we try to get the charset
 * from HTTP headers.
 */
nsresult
nsXMLHttpRequest::DetectCharset(nsAWritableString& aCharset)
{
  aCharset.Truncate();
  nsresult rv;
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel,&rv));
  if(httpChannel) {
    nsXPIDLCString contenttypeheader;
    rv = httpChannel->GetResponseHeader("content-type", getter_Copies(contenttypeheader));
    if (NS_SUCCEEDED(rv)) {
      nsAutoString contentType;
      contentType.AssignWithConversion( contenttypeheader.get() );
      PRInt32 start = contentType.RFind("charset=", PR_TRUE ) ;
      if(start>=0) {
        start += 8; // 8 = "charset=".length
        PRInt32 end = 0;
        if(PRUnichar('"') == contentType.CharAt(start)) {
          start++;
          end = contentType.FindCharInSet("\"", start  );
          if(end<0)
            end = contentType.Length();
        } else {
          end = contentType.FindCharInSet(";\n\r ", start  );
          if(end<0)
            end = contentType.Length();
        }
        nsAutoString theCharset;
        contentType.Mid(theCharset, start, end - start);
        nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID,&rv));
        if(NS_SUCCEEDED(rv) && calias) {
          nsAutoString preferred;
          rv = calias->GetPreferred(theCharset, preferred);
          if(NS_SUCCEEDED(rv)) {
            aCharset.Assign(preferred);
          }
        }
      }
    }
  }
  return rv;
}

nsresult
nsXMLHttpRequest::ConvertBodyToText(PRUnichar **aOutBuffer)
{
  *aOutBuffer = nsnull;

  PRInt32 dataLen = mResponseBody.Length();
  if (!dataLen)
    return NS_OK;

  nsresult rv = NS_OK;

  nsAutoString dataCharset;
  nsCOMPtr<nsIDocument> document(do_QueryInterface(mDocument));
  if (document) {
    rv = document->GetDocumentCharacterSet(dataCharset);
    if (NS_FAILED(rv))
      return rv;
  } else {
    if (NS_FAILED(DetectCharset(dataCharset)) || dataCharset.IsEmpty()) {
      dataCharset.Assign(NS_LITERAL_STRING("ISO-8859-1")); // Parser assumes ISO-8859-1
    }
  }

  if (dataCharset.Equals(NS_LITERAL_STRING("ASCII"))) {
    // XXX There is no ASCII->Unicode decoder?
    // XXX How to do this without double allocation/copy?
    *aOutBuffer = NS_ConvertASCIItoUCS2(mResponseBody.get(),dataLen).ToNewUnicode();
    if (!*aOutBuffer)
      return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
  }

  nsCOMPtr<nsICharsetConverterManager> ccm(do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID,&rv));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIUnicodeDecoder> decoder;
  rv = ccm->GetUnicodeDecoder(&dataCharset,getter_AddRefs(decoder));
  if (NS_FAILED(rv))
    return rv;

  // This loop here is basically a copy of a similar thing in nsScanner.
  // It will exit on first iteration in normal cases, but if we get illegal
  // characters in the input we replace them and then continue.
  const char * inBuffer = mResponseBody.get();
  PRUnichar * outBuffer = nsnull;
  PRInt32 outBufferIndex = 0, inBufferLength = dataLen;
  PRInt32 outLen;
  for(;;) {
    rv = decoder->GetMaxLength(inBuffer,inBufferLength,&outLen);
    if (NS_FAILED(rv))
      return rv;

    PRUnichar * newOutBuffer = NS_STATIC_CAST(PRUnichar*,nsMemory::Realloc(outBuffer,(outLen+outBufferIndex+1) * sizeof(PRUnichar)));
    if (!newOutBuffer) {
      nsMemory::Free(outBuffer);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    outBuffer = newOutBuffer;

    rv = decoder->Convert(inBuffer,&inBufferLength,&outBuffer[outBufferIndex],&outLen);
    if (rv == NS_OK) {
      break;
    } else if (rv == NS_ERROR_ILLEGAL_INPUT) {
      // We consume one byte, replace it with U+FFFD
      // and try the conversion again.
      outBuffer[outBufferIndex + outLen] = (PRUnichar)0xFFFD;
      outLen++;
      outBufferIndex += outLen;
      inBuffer = &inBuffer[outLen];
      inBufferLength -= outLen;
    } else if (NS_FAILED(rv)) {
      nsMemory::Free(outBuffer);
      return rv; // We bail out on other critical errors
    } else /*if (rv != NS_OK)*/ {
      NS_ABORT_IF_FALSE(0,"This should not happen, nsIUnicodeDecoder::Convert() succeeded partially");
      nsMemory::Free(outBuffer);
      return NS_ERROR_FAILURE;
    }
  }
  outBuffer[outBufferIndex + outLen] = '\0';
  *aOutBuffer = outBuffer;

  return NS_OK;
}

/* readonly attribute wstring responseText; */
NS_IMETHODIMP nsXMLHttpRequest::GetResponseText(PRUnichar **aResponseText)
{
  NS_ENSURE_ARG_POINTER(aResponseText);
  *aResponseText = nsnull;
  
  if ((XML_HTTP_REQUEST_COMPLETED == mStatus) ||
      (XML_HTTP_REQUEST_INTERACTIVE == mStatus)) {
    // First check if we can represent the data as a string - if it contains
    // nulls we won't try. 
    if (mResponseBody.FindChar('\0') >= 0)
      return NS_OK;

    nsresult rv = ConvertBodyToText(aResponseText);
    if (NS_FAILED(rv))
      return rv;
  }

  return NS_OK;
}

/* readonly attribute unsigned long status; */
NS_IMETHODIMP 
nsXMLHttpRequest::GetStatus(PRUint32 *aStatus)
{
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));

  if (httpChannel) {
    return httpChannel->GetResponseStatus(aStatus);
  } 
  *aStatus = 0;

  return NS_OK;
}

/* readonly attribute string statusText; */
NS_IMETHODIMP 
nsXMLHttpRequest::GetStatusText(char * *aStatusText)
{
  NS_ENSURE_ARG_POINTER(aStatusText);
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));

  if (httpChannel) {
    return httpChannel->GetResponseStatusText(aStatusText);
  }
  *aStatusText = nsnull;
  
  return NS_OK;
}

/* void abort (); */
NS_IMETHODIMP 
nsXMLHttpRequest::Abort()
{
  if (mReadRequest) {
    return mReadRequest->Cancel(NS_BINDING_ABORTED);
  }
  
  return NS_OK;
}

/* string getAllResponseHeaders (); */
NS_IMETHODIMP 
nsXMLHttpRequest::GetAllResponseHeaders(char **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));
  if (httpChannel) {
    nsHeaderVisitor *visitor = nsnull;
    NS_NEWXPCOM(visitor, nsHeaderVisitor);
    if (!visitor)
      return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(visitor);

    nsresult rv = httpChannel->VisitResponseHeaders(visitor);
    if (NS_SUCCEEDED(rv))
      *_retval = ToNewCString(visitor->Headers());

    NS_RELEASE(visitor);
    return rv;
  }
  
  return NS_OK;
}

/* string getResponseHeader (in string header); */
NS_IMETHODIMP 
nsXMLHttpRequest::GetResponseHeader(const char *header, char **_retval)
{
  NS_ENSURE_ARG(header);
  NS_ENSURE_ARG_POINTER(_retval);
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));

  *_retval = nsnull;
  if (httpChannel)
    return httpChannel->GetResponseHeader(header, _retval);
  
  return NS_OK;
}

/* noscript void openRequest (in string method, in string url, in boolean async, in string user, in string password); */
NS_IMETHODIMP 
nsXMLHttpRequest::OpenRequest(const char *method, 
                              const char *url, 
                              PRBool async, 
                              const char *user, 
                              const char *password)
{
  NS_ENSURE_ARG(method);
  NS_ENSURE_ARG(url);
  
  nsresult rv;
  nsCOMPtr<nsIURI> uri; 
  PRBool authp = PR_FALSE;

  // Return error if we're already processing a request
  if (XML_HTTP_REQUEST_SENT == mStatus) {
    return NS_ERROR_FAILURE;
  }

  mAsync = async;

  rv = NS_NewURI(getter_AddRefs(uri), url, mBaseURI);
  if (NS_FAILED(rv)) return rv;

  if (user) {
    nsCAutoString prehost;
    prehost.Assign(user);
    if (password) {
      prehost.Append(":");
      prehost.Append(password);
    }
    uri->SetPreHost(prehost.get());
    authp = PR_TRUE;
  }

  rv = NS_OpenURI(getter_AddRefs(mChannel), uri, nsnull, nsnull);
  if (NS_FAILED(rv)) return rv;
  
  //mChannel->SetAuthTriedWithPrehost(authp);

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));
  if (httpChannel) {
    rv = httpChannel->SetRequestMethod(method);
  }

  ChangeState(XML_HTTP_REQUEST_OPENED);

  return rv;
}

/* void open (in string method, in string url); */
NS_IMETHODIMP 
nsXMLHttpRequest::Open(const char *method, const char *url)
{
  nsresult rv;
  PRBool async = PR_TRUE;
  char* user = nsnull;
  char* password = nsnull;

  nsCOMPtr<nsIXPCNativeCallContext> cc;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
  if(NS_SUCCEEDED(rv)) {
    rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
  }

  if (NS_SUCCEEDED(rv) && cc) {
    PRUint32 argc;
    rv = cc->GetArgc(&argc);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    jsval* argv;
    rv = cc->GetArgvPtr(&argv);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    JSContext* cx;
    rv = cc->GetJSContext(&cx);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIScriptSecurityManager> secMan = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIPrincipal> principal;
    rv = secMan->GetSubjectPrincipal(getter_AddRefs(principal));
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsICodebasePrincipal> codebase = do_QueryInterface(principal);
      if (codebase) {
        codebase->GetURI(getter_AddRefs(mBaseURI));
      }
    }

    nsCOMPtr<nsIURI> targetURI;
    rv = NS_NewURI(getter_AddRefs(targetURI), url, mBaseURI);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    rv = secMan->CheckConnect(cx, targetURI, "XMLHttpRequest","open");
    if (NS_FAILED(rv))
    {
      // Security check failed.
      return NS_OK;
    }

    if (argc > 2) {
      JSBool asyncBool;
      JS_ValueToBoolean(cx, argv[2], &asyncBool);
      async = (PRBool)asyncBool;

      if (argc > 3) {
        JSString* userStr;

        userStr = JS_ValueToString(cx, argv[3]);
        if (userStr) {
          user = JS_GetStringBytes(userStr);
        }

        if (argc > 4) {
          JSString* passwordStr;

          passwordStr = JS_ValueToString(cx, argv[4]);
          if (passwordStr) {
            password = JS_GetStringBytes(passwordStr);
          }
        }
      }
    }
  }

  return OpenRequest(method, url, async, user, password);
}

nsresult
nsXMLHttpRequest::GetStreamForWString(const PRUnichar* aStr,
                                      PRInt32 aLength,
                                      nsIInputStream** aStream)
{
  nsresult rv;
  nsCOMPtr<nsIUnicodeEncoder> encoder;
  nsAutoString charsetStr;
  char* postData;

  // We want to encode the string as utf-8, so get the right encoder
  nsCOMPtr<nsICharsetConverterManager> charsetConv = 
           do_GetService(kCharsetConverterManagerCID, &rv);
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
  

  // Allocate extra space for the trailing and leading CRLF
  postData = (char*)nsMemory::Alloc(charLength + 5);
  if (!postData) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  rv = encoder->Convert(unicodeBuf, 
                        &unicodeLength, postData+2, &charLength);
  if (NS_FAILED(rv)) {
    nsMemory::Free(postData);
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));
  if (!httpChannel) {
    return NS_ERROR_FAILURE;
  }
  
  // If no content type header was set by the client, we set it to text/xml.
  nsXPIDLCString header;
  if( NS_OK != httpChannel->GetRequestHeader("Content-Type", getter_Copies(header)) )  
    httpChannel->SetRequestHeader("Content-Type", "text/xml" );

  // set the content length header
  char charLengthBuf [32];
  PR_snprintf(charLengthBuf, sizeof(charLengthBuf), "%d", charLength);
  httpChannel->SetRequestHeader("Content-Length", charLengthBuf );

  // Shove in the trailing and leading CRLF
  postData[0] = nsCRT::CR;
  postData[1] = nsCRT::LF;
  postData[2+charLength] = nsCRT::CR;
  postData[2+charLength+1] = nsCRT::LF;
  postData[2+charLength+2] = '\0';

  // The new stream takes ownership of the buffer
  rv = NS_NewByteArrayInputStream((nsIByteArrayInputStream**)aStream, 
                                  postData, 
                                  charLength+4);
  if (NS_FAILED(rv)) {
    nsMemory::Free(postData);
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}


/*
 * "Copy" from a stream.
 */
NS_METHOD
nsXMLHttpRequest::StreamReaderFunc(nsIInputStream* in,
                void* closure,
                const char* fromRawSegment,
                PRUint32 toOffset,
                PRUint32 count,
                PRUint32 *writeCount)
{
  nsXMLHttpRequest* xmlHttpRequest = NS_STATIC_CAST(nsXMLHttpRequest*, closure);
  if (!xmlHttpRequest || !writeCount) {
    NS_WARNING("XMLHttpRequest cannot read from stream: no closure or writeCount");
    return NS_ERROR_FAILURE;
  }

  // Copy for our own use
  xmlHttpRequest->mResponseBody.Append(fromRawSegment,count);

  nsresult rv;

  // Give the same data to the parser.

  // We need to wrap the data in a new lightweight stream and pass that
  // to the parser, because calling ReadSegments() recursively on the same
  // stream is not supported.
  nsCOMPtr<nsISupports> supportsStream;
  rv = NS_NewByteInputStream(getter_AddRefs(supportsStream),fromRawSegment,count);

  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIInputStream> copyStream(do_QueryInterface(supportsStream));
    if (copyStream) {
      rv = xmlHttpRequest->mXMLParserStreamListener->OnDataAvailable(xmlHttpRequest->mReadRequest,xmlHttpRequest->mContext,copyStream,toOffset,count);
    } else {
      NS_ERROR("NS_NewByteInputStream did not give out nsIInputStream!");
      rv = NS_ERROR_UNEXPECTED;
    }
  }

  xmlHttpRequest->ChangeState(XML_HTTP_REQUEST_INTERACTIVE);

  if (NS_SUCCEEDED(rv)) {
    *writeCount = count;
  } else {
    *writeCount = 0;
  }

  return rv;
}

/* void onDataAvailable (in nsIRequest request, in nsISupports ctxt, in nsIInputStream inStr, in unsigned long sourceOffset, in unsigned long count); */
NS_IMETHODIMP 
nsXMLHttpRequest::OnDataAvailable(nsIRequest *request, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
  NS_ENSURE_ARG_POINTER(inStr);

  NS_ABORT_IF_FALSE(mContext.get() == ctxt,"start context different from OnDataAvailable context");

  PRUint32 totalRead;
  return inStr->ReadSegments(nsXMLHttpRequest::StreamReaderFunc, (void*)this, count, &totalRead);
}

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP 
nsXMLHttpRequest::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  mReadRequest = request;
  mContext = ctxt;
  ChangeState(XML_HTTP_REQUEST_LOADED);
  return mXMLParserStreamListener->OnStartRequest(request,ctxt);
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP 
nsXMLHttpRequest::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult status)
{
  nsCOMPtr<nsIParser> parser(do_QueryInterface(mXMLParserStreamListener));
  NS_ABORT_IF_FALSE(parser, "stream listener was expected to be a parser");

  nsresult rv = mXMLParserStreamListener->OnStopRequest(request,ctxt,status);
  mXMLParserStreamListener = nsnull;
  mReadRequest = nsnull;
  mContext = nsnull;

  // The parser needs to be enabled for us to safely call RequestCompleted().
  // If the parser is not enabled, it means it was blocked, by xml-stylesheet PI
  // for example, and is still building the document. RequestCompleted() must be
  // called later, when we get the load event from the document.
  if (parser && parser->IsParserEnabled()) {
    RequestCompleted();
  } else {
    ChangeState(XML_HTTP_REQUEST_STOPPED,PR_FALSE);
  }

  return rv;
}

nsresult 
nsXMLHttpRequest::RequestCompleted()
{
  nsresult rv = NS_OK;

  // If we're uninitialized at this point, we encountered an error
  // earlier and listeners have already been notified. Also we do
  // not want to do this if we already completed.
  if ((mStatus == XML_HTTP_REQUEST_UNINITIALIZED) ||
      (mStatus == XML_HTTP_REQUEST_COMPLETED)) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMEvent> domevent;
  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(mDocument));
  if (!receiver) {
    return NS_ERROR_FAILURE;
  }
  
  nsCOMPtr<nsIEventListenerManager> manager;
  rv = receiver->GetListenerManager(getter_AddRefs(manager));
  if (!manager) {
    return NS_ERROR_FAILURE;
  }

  nsEvent event;
  event.eventStructType = NS_EVENT;
  event.message = NS_PAGE_LOAD;
  rv = manager->CreateEvent(nsnull, &event,
                            NS_LITERAL_STRING("HTMLEvents"), 
                            getter_AddRefs(domevent));
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  nsCOMPtr<nsIPrivateDOMEvent> privevent(do_QueryInterface(domevent));
  if (!privevent) {
    return NS_ERROR_FAILURE;
  }
  privevent->SetTarget(this);

  // We might have been sent non-XML data. If that was the case,
  // we should null out the document member. The idea in this
  // check here is that if there is no document element it is not
  // an XML document. We might need a fancier check...
  if (mDocument) {
    nsCOMPtr<nsIDOMElement> root;
    mDocument->GetDocumentElement(getter_AddRefs(root));
    if (!root) {
      mDocument = nsnull;
    }
  }

  ChangeState(XML_HTTP_REQUEST_COMPLETED);

#ifdef IMPLEMENT_SYNC_LOAD
  if (mChromeWindow) {
    mChromeWindow->ExitModalEventLoop(NS_OK);
    mChromeWindow = 0;
  }
#endif

  nsCOMPtr<nsIJSContextStack> stack;
  JSContext *cx = nsnull;

  if (mScriptContext) {
    stack = do_GetService("@mozilla.org/js/xpc/ContextStack;1");

    if (stack) {
      cx = (JSContext *)mScriptContext->GetNativeContext();

      if (cx) {
        stack->Push(cx);
      }
    }
  }

  if (mOnLoadListener) {
    mOnLoadListener->HandleEvent(domevent);
  }

  if (mLoadEventListeners) {
    PRUint32 index, count;

    mLoadEventListeners->Count(&count);
    for (index = 0; index < count; index++) {
      nsCOMPtr<nsIDOMEventListener> listener;

      mLoadEventListeners->QueryElementAt(index,
                                          NS_GET_IID(nsIDOMEventListener),
                                          getter_AddRefs(listener));

      if (listener) {
        listener->HandleEvent(domevent);
      }
    }
  }

  if (cx) {
    stack->Pop(&cx);
  }

  return rv;
}

/* void send (in nsISupports body); */
NS_IMETHODIMP 
nsXMLHttpRequest::Send(nsISupports *body)
{
  nsresult rv;
  
  // Return error if we're already processing a request
  if (XML_HTTP_REQUEST_SENT == mStatus) {
    return NS_ERROR_FAILURE;
  }
  
  // Make sure we've been opened
  if (!mChannel || XML_HTTP_REQUEST_OPENED != mStatus) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Ignore argument if method is GET, there is no point in trying to upload anything
  nsXPIDLCString method;
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));

  if (httpChannel) {
    httpChannel->GetRequestMethod(getter_Copies(method)); // If GET, method name will be uppercase
  }

  if (body && httpChannel && nsCRT::strcmp("GET",method.get()) != 0) {
    nsCOMPtr<nsIInputStream> postDataStream;

    nsCOMPtr<nsIDOMDocument> doc(do_QueryInterface(body));
    if (doc) {
      // Get an XML serializer
      nsCOMPtr<nsIDOMSerializer> serializer(do_CreateInstance(NS_XMLSERIALIZER_CONTRACTID, &rv));
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;  
      
      // Serialize the current document to string
      nsXPIDLString serial;
      rv = serializer->SerializeToString(doc, getter_Copies(serial));
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
      
      // Convert to a byte stream
      rv = GetStreamForWString(serial.get(), 
                               nsCRT::strlen(serial.get()),
                               getter_AddRefs(postDataStream));
      if (NS_FAILED(rv)) return rv;
    }
    else {
      nsCOMPtr<nsIInputStream> stream(do_QueryInterface(body));
      if (stream) {
        postDataStream = stream;
      }
      else {
        nsCOMPtr<nsISupportsWString> wstr(do_QueryInterface(body));
        if (wstr) {
          nsXPIDLString holder;
          wstr->GetData(getter_Copies(holder));
          rv = GetStreamForWString(holder.get(), 
                                   nsCRT::strlen(holder.get()),
                                   getter_AddRefs(postDataStream));
          if (NS_FAILED(rv)) return rv;
        }
      }
    }

    if (postDataStream) {
      nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(httpChannel));
      NS_ASSERTION(uploadChannel, "http must support nsIUploadChannel");

      rv = uploadChannel->SetUploadStream(postDataStream, nsnull, -1);
    }
  }

  // Get and initialize a DOMImplementation
  nsCOMPtr<nsIDOMDOMImplementation> implementation(do_CreateInstance(kIDOMDOMImplementationCID, &rv));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  
  if (mBaseURI) {
    nsCOMPtr<nsIPrivateDOMImplementation> privImpl(do_QueryInterface(implementation));
    if (privImpl) {
      privImpl->Init(mBaseURI);
    }
  }

  // Create an empty document from it (resets current document as well)
  nsAutoString emptyStr;
  rv = implementation->CreateDocument(emptyStr, 
                                      emptyStr, 
                                      nsnull, 
                                      getter_AddRefs(mDocument));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  // Reset responseBody
  mResponseBody.Truncate();

  // Register as a load listener on the document
  nsCOMPtr<nsIDOMEventReceiver> target(do_QueryInterface(mDocument));
  if (target) {
    nsWeakPtr requestWeak(getter_AddRefs(NS_GetWeakReference(NS_STATIC_CAST(nsIXMLHttpRequest*, this))));
    nsLoadListenerProxy* proxy = new nsLoadListenerProxy(requestWeak);
    if (!proxy) return NS_ERROR_OUT_OF_MEMORY;

    // This will addref the proxy
    rv = target->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMEventListener*, 
                                                      proxy), 
                                       NS_GET_IID(nsIDOMLoadListener));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  }

  // Tell the document to start loading
  nsCOMPtr<nsIStreamListener> listener;
  nsCOMPtr<nsIDocument> document(do_QueryInterface(mDocument));
  if (!document) {
    return NS_ERROR_FAILURE;
  }

#ifdef IMPLEMENT_SYNC_LOAD
  nsCOMPtr<nsIEventQueue> modalEventQueue;
  nsCOMPtr<nsIEventQueueService> eventQService;
  
  if (!mAsync) { 
    nsCOMPtr<nsIXPCNativeCallContext> cc;
    nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
    if(NS_SUCCEEDED(rv)) {
      rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
    }

    if (NS_SUCCEEDED(rv) && cc) {
      JSContext* cx;
      rv = cc->GetJSContext(&cx);
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

      // We can only do this if we're called from a DOM script context
      nsIScriptContext* scriptCX = (nsIScriptContext*)JS_GetContextPrivate(cx);
      if (!scriptCX) return NS_OK;

      // Get the nsIDocShellTreeOwner associated with the window
      // containing this script context
      // XXX Need to find a better way to do this rather than
      // chaining through a bunch of getters and QIs
      nsCOMPtr<nsIScriptGlobalObject> global;
      scriptCX->GetGlobalObject(getter_AddRefs(global));
      if (!global) return NS_ERROR_FAILURE;

      nsCOMPtr<nsIDocShell> docshell;
      rv = global->GetDocShell(getter_AddRefs(docshell));
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
      
      nsCOMPtr<nsIDocShellTreeItem> item = do_QueryInterface(docshell);
      if (!item) return NS_ERROR_FAILURE;

      nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
      rv = item->GetTreeOwner(getter_AddRefs(treeOwner));
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

      nsCOMPtr<nsIInterfaceRequestor> treeRequestor(do_GetInterface(treeOwner));
      if (!treeRequestor) return NS_ERROR_FAILURE;

      treeRequestor->GetInterface(NS_GET_IID(nsIWebBrowserChrome), getter_AddRefs(mChromeWindow));
      if (!mChromeWindow) return NS_ERROR_FAILURE;

      eventQService = do_GetService(kEventQueueServiceCID);
      if(!eventQService || 
         NS_FAILED(eventQService->PushThreadEventQueue(getter_AddRefs(modalEventQueue)))) {
        return NS_ERROR_FAILURE;
      }
    }
  }
#endif

  rv = document->StartDocumentLoad(kLoadAsData, mChannel, 
                                   nsnull, nsnull, 
                                   getter_AddRefs(listener),
                                   PR_FALSE);

#ifdef IMPLEMENT_SYNC_LOAD
  if (NS_FAILED(rv)) {
    if (modalEventQueue) {
      eventQService->PopThreadEventQueue(modalEventQueue);
    }
    return NS_ERROR_FAILURE;
  }
#else
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
#endif

  // Start reading from the channel
  ChangeState(XML_HTTP_REQUEST_SENT);
  mXMLParserStreamListener = listener;
  rv = mChannel->AsyncOpen(this, nsnull);

#ifdef IMPLEMENT_SYNC_LOAD
  if (NS_FAILED(rv)) {
    if (modalEventQueue) {
      eventQService->PopThreadEventQueue(modalEventQueue);
    }
    return NS_ERROR_FAILURE;
  }  
#else
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
#endif

#ifdef IMPLEMENT_SYNC_LOAD
  // If we're synchronous, spin an event loop here and wait
  if (!mAsync && mChromeWindow) {
    rv = mChromeWindow->ShowAsModal();
    
    eventQService->PopThreadEventQueue(modalEventQueue);
    
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;      
  }
#endif
  
  return NS_OK;
}

/* void setRequestHeader (in string header, in string value); */
NS_IMETHODIMP 
nsXMLHttpRequest::SetRequestHeader(const char *header, const char *value)
{
  if (!mChannel)             // open() initializes mChannel, and open()
    return NS_ERROR_FAILURE; // must be called before first setRequestHeader()

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));

  if (httpChannel)
    return httpChannel->SetRequestHeader(header, value);
  
  return NS_OK;
}

/* readonly attribute long readyState; */
NS_IMETHODIMP 
nsXMLHttpRequest::GetReadyState(PRInt32 *aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  // Translate some of our internal states for external consumers
  switch (mStatus) {
    case XML_HTTP_REQUEST_SENT:
      *aState = XML_HTTP_REQUEST_OPENED;
      break;
    case XML_HTTP_REQUEST_STOPPED:
      *aState = XML_HTTP_REQUEST_INTERACTIVE;
      break;
    default:
      *aState = mStatus;
      break;
  }
  return NS_OK;
}

// nsIDOMEventListener
nsresult
nsXMLHttpRequest::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}


// nsIDOMLoadListener
nsresult
nsXMLHttpRequest::Load(nsIDOMEvent* aEvent)
{
  // If we had an XML error in the data, the parser terminated and
  // we received the load event, even though we might still be
  // loading data into responseBody/responseText. We will delay
  // sending the load event until OnStopRequest(). In normal case
  // there is no harm done, we will get OnStopRequest() immediately
  // after the load event.
  //
  // However, if the data we were loading caused the parser to stop,
  // for example when loading external stylesheets, we can receive
  // the OnStopRequest() call before the parser has finished building
  // the document. In that case, we obviously should not fire the event
  // in OnStopRequest(). For those documents, we must wait for the load
  // event from the document to fire our RequestCompleted().
  if (mStatus == XML_HTTP_REQUEST_STOPPED) {
    RequestCompleted();
  }
  return NS_OK;
}

nsresult
nsXMLHttpRequest::Unload(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

nsresult
nsXMLHttpRequest::Abort(nsIDOMEvent* aEvent)
{
  mDocument = nsnull;
  ChangeState(XML_HTTP_REQUEST_UNINITIALIZED);
#ifdef IMPLEMENT_SYNC_LOAD
  if (mChromeWindow) {
    mChromeWindow->ExitModalEventLoop(NS_OK);
    mChromeWindow = 0;
  }
#endif

  return NS_OK;
}

nsresult
nsXMLHttpRequest::Error(nsIDOMEvent* aEvent)
{
  mDocument = nsnull;
  ChangeState(XML_HTTP_REQUEST_UNINITIALIZED);
#ifdef IMPLEMENT_SYNC_LOAD
  if (mChromeWindow) {
    mChromeWindow->ExitModalEventLoop(NS_OK);
    mChromeWindow = 0;
  }
#endif

  nsCOMPtr<nsIJSContextStack> stack;
  JSContext *cx = nsnull;

  if (mScriptContext) {
    stack = do_GetService("@mozilla.org/js/xpc/ContextStack;1");

    if (stack) {
      cx = (JSContext *)mScriptContext->GetNativeContext();

      if (cx) {
        stack->Push(cx);
      }
    }
  }

  if (mOnErrorListener) {
    mOnErrorListener->HandleEvent(aEvent);
  }

  if (mErrorEventListeners) {
    PRUint32 index, count;

    mErrorEventListeners->Count(&count);
    for (index = 0; index < count; index++) {
      nsCOMPtr<nsIDOMEventListener> listener;

      mErrorEventListeners->QueryElementAt(index,
                                           NS_GET_IID(nsIDOMEventListener),
                                           getter_AddRefs(listener));

      if (listener) {
        listener->HandleEvent(aEvent);
      }
    }
  }

  if (cx) {
    stack->Pop(&cx);
  }

  return NS_OK;
}

nsresult
nsXMLHttpRequest::ChangeState(nsXMLHttpRequestState aState, PRBool aBroadcast)
{
  mStatus = aState;
  nsresult rv = NS_OK;
  if (mAsync && aBroadcast && mOnReadystatechangeListener) {
    nsCOMPtr<nsIJSContextStack> stack;
    JSContext *cx = nsnull;

    if (mScriptContext) {
      stack = do_GetService("@mozilla.org/js/xpc/ContextStack;1");

      if (stack) {
        cx = (JSContext *)mScriptContext->GetNativeContext();

        if (cx) {
          stack->Push(cx);
        }
      }
    }

    rv = mOnReadystatechangeListener->HandleEvent();

    if (cx) {
      stack->Pop(&cx);
    }
  }

  return rv;
}

NS_IMPL_ISUPPORTS1(nsXMLHttpRequest::nsHeaderVisitor, nsIHttpHeaderVisitor)

NS_IMETHODIMP nsXMLHttpRequest::
nsHeaderVisitor::VisitHeader(const char *header, const char *value)
{
    mHeaders.Append(header);
    mHeaders.Append(": ");
    mHeaders.Append(value);
    mHeaders.Append('\n');
    return NS_OK;
}

