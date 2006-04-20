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
 *
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
#include "nsReadableUtils.h"
#include "nsPrintfCString.h"
#include "nsCRT.h"
#include "nsIURI.h"
#include "nsILoadGroup.h"
#include "nsNetUtil.h"
#include "nsIUploadChannel.h"
#include "nsIDOMSerializer.h"
#include "nsXPCOM.h"
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
#include "nsIDOMWindow.h"
#include "nsIVariant.h"
#include "nsIParser.h"
#include "nsLoadListenerProxy.h"
#include "nsIWindowWatcher.h"
#include "nsIAuthPrompt.h"

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
  mCrossSiteAccessEnabled = PR_FALSE;
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
  NS_INTERFACE_MAP_ENTRY(nsIHttpEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_EXTERNAL_DOM_CLASSINFO(XMLHttpRequest)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsXMLHttpRequest)
NS_IMPL_RELEASE(nsXMLHttpRequest)


/* void addEventListener (in string type, in nsIDOMEventListener
   listener); */
NS_IMETHODIMP
nsXMLHttpRequest::AddEventListener(const nsAString& type,
                                   nsIDOMEventListener *listener,
                                   PRBool useCapture)
{
  NS_ENSURE_ARG(listener);
  nsresult rv;

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
  GetCurrentContext(getter_AddRefs(mScriptContext));

  return NS_OK;
}

/* void removeEventListener (in string type, in nsIDOMEventListener
   listener); */
NS_IMETHODIMP 
nsXMLHttpRequest::RemoveEventListener(const nsAString & type,
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

/* boolean dispatchEvent (in nsIDOMEvent evt); */
NS_IMETHODIMP
nsXMLHttpRequest::DispatchEvent(nsIDOMEvent *evt, PRBool *_retval)
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
nsXMLHttpRequest::DetectCharset(nsAString& aCharset)
{
  aCharset.Truncate();
  nsresult rv;
  nsCAutoString charsetVal;
  rv = mChannel->GetContentCharset(charsetVal);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID,&rv));
    if(NS_SUCCEEDED(rv) && calias) {
      nsAutoString preferred;
      rv = calias->GetPreferred(NS_ConvertASCIItoUCS2(charsetVal), preferred);
      if(NS_SUCCEEDED(rv)) {
        aCharset.Assign(preferred);        
      }
    }
  }
  return rv;
}

nsresult
nsXMLHttpRequest::ConvertBodyToText(PRUnichar **aOutBuffer)
{
  // This code here is basically a copy of a similar thing in
  // nsScanner::Append(const char* aBuffer, PRUint32 aLen).
  // If we get illegal characters in the input we replace 
  // them and don't just fail.

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
      // MS documentation states UTF-8 is default for responseText
      dataCharset.Assign(NS_LITERAL_STRING("UTF-8"));
    }
  }

  if (dataCharset.Equals(NS_LITERAL_STRING("ASCII"))) {
    *aOutBuffer = ToNewUnicode(nsDependentCString(mResponseBody.get(),dataLen));
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

  const char * inBuffer = mResponseBody.get();
  PRInt32 outBufferLength;
  rv = decoder->GetMaxLength(inBuffer, dataLen, &outBufferLength);
  if (NS_FAILED(rv))
    return rv;

  PRUnichar * outBuffer = 
    NS_STATIC_CAST(PRUnichar*, nsMemory::Alloc((outBufferLength + 1) *
                                               sizeof(PRUnichar)));
  if (!outBuffer) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRInt32 totalChars = 0,
          outBufferIndex = 0,
          outLen = outBufferLength;

  do {
    PRInt32 inBufferLength = dataLen;
    rv = decoder->Convert(inBuffer, 
                          &inBufferLength, 
                          &outBuffer[outBufferIndex], 
                          &outLen);
    totalChars += outLen;
    if (NS_FAILED(rv)) {
      // We consume one byte, replace it with U+FFFD
      // and try the conversion again.
      outBuffer[outBufferIndex + outLen++] = (PRUnichar)0xFFFD;
      outBufferIndex += outLen;
      outLen = outBufferLength - (++totalChars);

      decoder->Reset();

      if((inBufferLength + 1) > dataLen) {
        inBufferLength = dataLen;
      } else {
        inBufferLength++;
      }

      inBuffer = &inBuffer[inBufferLength];
      dataLen -= inBufferLength;
    }
  } while ( NS_FAILED(rv) && (dataLen > 0) );

  outBuffer[totalChars] = '\0';
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

  *aStatusText = nsnull;

  if (httpChannel) {
    nsCAutoString text;
    nsresult rv = httpChannel->GetResponseStatusText(text);
    if (NS_FAILED(rv)) return rv;
    *aStatusText = ToNewCString(text);
    return *aStatusText ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  }
  
  return NS_OK;
}

/* void abort (); */
NS_IMETHODIMP 
nsXMLHttpRequest::Abort()
{
  if (mReadRequest) {
    mReadRequest->Cancel(NS_BINDING_ABORTED);
  }
  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
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
  if (httpChannel) {
    nsCAutoString buf;
    nsresult rv = httpChannel->GetResponseHeader(nsDependentCString(header), buf);
    if (NS_FAILED(rv)) return rv;
    *_retval = ToNewCString(buf);
    return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  }
  
  return NS_OK;
}

nsresult 
nsXMLHttpRequest::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
  NS_ENSURE_ARG_POINTER(aLoadGroup);
  *aLoadGroup = nsnull;

  if (!mScriptContext) {
    GetCurrentContext(getter_AddRefs(mScriptContext));
  }

  if (mScriptContext) {
    nsCOMPtr<nsIScriptGlobalObject> global;
    mScriptContext->GetGlobalObject(getter_AddRefs(global));
    nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(global);
    if (window) {
      nsCOMPtr<nsIDOMDocument> domdoc;
      window->GetDocument(getter_AddRefs(domdoc));
      nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
      if (doc) {
        doc->GetDocumentLoadGroup(aLoadGroup);
      }
    }
  }

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
    nsCAutoString userpass;
    userpass.Assign(user);
    if (password) {
      userpass.Append(":");
      userpass.Append(password);
    }
    uri->SetUserPass(userpass);
    authp = PR_TRUE;
  }

  // When we are called from JS we can find the load group for the page,
  // and add ourselves to it. This way any pending requests
  // will be automatically aborted if the user leaves the page.
  nsCOMPtr<nsILoadGroup> loadGroup;
  GetLoadGroup(getter_AddRefs(loadGroup));

  // nsIRequest::LOAD_BACKGROUND prevents throbber from becoming active,
  // which in turn keeps STOP button from becoming active
  rv = NS_NewChannel(getter_AddRefs(mChannel), uri, nsnull, loadGroup, 
                     nsnull, nsIRequest::LOAD_BACKGROUND);
  if (NS_FAILED(rv)) return rv;
  
  //mChannel->SetAuthTriedWithPrehost(authp);

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));
  if (httpChannel) {
    rv = httpChannel->SetRequestMethod(nsDependentCString(method));
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

    // Find out if UniversalBrowserRead privileges are enabled
    // we will need this in case of a redirect
    PRBool crossSiteAccessEnabled;
    rv = secMan->IsCapabilityEnabled("UniversalBrowserRead",
                                     &crossSiteAccessEnabled);
    if (NS_FAILED(rv)) return rv;
    mCrossSiteAccessEnabled = crossSiteAccessEnabled;

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
  
  charsetStr.Assign(NS_LITERAL_STRING("UTF-8"));
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
  nsCAutoString header;
  if( NS_FAILED(httpChannel->GetRequestHeader(NS_LITERAL_CSTRING("Content-Type"), header)) )  
    httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Content-Type"),
                                  NS_LITERAL_CSTRING("text/xml") );

  // set the content length header
  httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Content-Length"), nsPrintfCString("%d", charLength) );

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

  nsresult rv = NS_OK;

  if (xmlHttpRequest->mParseResponseBody) {
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
  mParseResponseBody = PR_TRUE;
  ChangeState(XML_HTTP_REQUEST_LOADED);
  if (mOverrideMimeType.IsEmpty()) {
    // If we are not overriding the mime type, we can gain a huge performance
    // win by not even trying to parse non-XML data. This also protects us
    // from the situation where we have an XML document and sink, but HTML (or other)
    // parser, which can produce unreliable results.
    nsCAutoString type;
    mChannel->GetContentType(type);
    nsACString::const_iterator start, end;
    type.BeginReading(start);
    type.EndReading(end);
    if (!FindInReadable(NS_LITERAL_CSTRING("xml"), start, end)) {
      mParseResponseBody = PR_FALSE;
    }
  } else {
    nsresult status;
    request->GetStatus(&status);
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    if (channel && NS_SUCCEEDED(status)) {
      channel->SetContentType(mOverrideMimeType);
    }
  }

  return mParseResponseBody ? mXMLParserStreamListener->OnStartRequest(request,ctxt) : NS_OK;
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP 
nsXMLHttpRequest::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult status)
{
  nsCOMPtr<nsIParser> parser(do_QueryInterface(mXMLParserStreamListener));
  NS_ABORT_IF_FALSE(parser, "stream listener was expected to be a parser");

  nsresult rv = mParseResponseBody ? mXMLParserStreamListener->OnStopRequest(request,ctxt,status) : NS_OK;
  mXMLParserStreamListener = nsnull;
  mReadRequest = nsnull;
  mContext = nsnull;

  if (NS_FAILED(status)) {
    // This can happen if the user leaves the page while the request was still
    // active. This might also happen if request was active during the regular
    // page load and the user was able to hit STOP button. If XMLHttpRequest is
    // the only active network connection on the page the throbber nor the STOP
    // button are active.
    Abort();
  } else if (parser && parser->IsParserEnabled()) {
    // The parser needs to be enabled for us to safely call RequestCompleted().
    // If the parser is not enabled, it means it was blocked, by xml-stylesheet PI
    // for example, and is still building the document. RequestCompleted() must be
    // called later, when we get the load event from the document.
    RequestCompleted();
  } else {
    ChangeState(XML_HTTP_REQUEST_STOPPED, PR_FALSE);
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

/* void send (in nsIVariant aBody); */
NS_IMETHODIMP 
nsXMLHttpRequest::Send(nsIVariant *aBody)
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
  nsCAutoString method;
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));

  if (httpChannel) {
    httpChannel->GetRequestMethod(method); // If GET, method name will be uppercase
  }

  if (aBody && httpChannel && !method.Equals(NS_LITERAL_CSTRING("GET"))) {
    nsXPIDLString serial;
    nsCOMPtr<nsIInputStream> postDataStream;

    PRUint16 dataType;
    rv = aBody->GetDataType(&dataType);
    if (NS_FAILED(rv)) 
      return NS_ERROR_FAILURE;

    switch (dataType) {
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
      {
        nsCOMPtr<nsISupports> supports;
        nsID *iid;
        rv = aBody->GetAsInterface(&iid, getter_AddRefs(supports));
        if (NS_FAILED(rv)) 
          return NS_ERROR_FAILURE;
        if (iid) 
          nsMemory::Free(iid);

        // document?
        nsCOMPtr<nsIDOMDocument> doc(do_QueryInterface(supports));
        if (doc) {
          nsCOMPtr<nsIDOMSerializer> serializer(do_CreateInstance(NS_XMLSERIALIZER_CONTRACTID, &rv));
          if (NS_FAILED(rv)) return NS_ERROR_FAILURE;  
      
          rv = serializer->SerializeToString(doc, getter_Copies(serial));
          if (NS_FAILED(rv)) 
            return NS_ERROR_FAILURE;
        } else {
          // nsISupportsString?
          nsCOMPtr<nsISupportsString> wstr(do_QueryInterface(supports));
          if (wstr) {
            wstr->GetData(serial);
          } else {
            // stream?
            nsCOMPtr<nsIInputStream> stream(do_QueryInterface(supports));
            if (stream) {
              postDataStream = stream;
            }
          }
        }
      }
      break;
    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_EMPTY:
      // Makes us act as if !aBody, don't upload anything
      break;
    case nsIDataType::VTYPE_EMPTY_ARRAY:
    case nsIDataType::VTYPE_ARRAY:
      // IE6 throws error here, so we do that as well
      return NS_ERROR_INVALID_ARG;
    default:
      // try variant string
      rv = aBody->GetAsWString(getter_Copies(serial));    
      if (NS_FAILED(rv)) 
        return rv;
      break;
    }

    if (serial) {
      // Convert to a byte stream
      rv = GetStreamForWString(serial.get(), 
                               nsCRT::strlen(serial.get()),
                               getter_AddRefs(postDataStream));
      if (NS_FAILED(rv)) 
        return rv;
    }

    if (postDataStream) {
      nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(httpChannel));
      NS_ASSERTION(uploadChannel, "http must support nsIUploadChannel");

      rv = uploadChannel->SetUploadStream(postDataStream, NS_LITERAL_CSTRING(""), -1);
      // Reset the method to its original value
      if (httpChannel) {
          httpChannel->SetRequestMethod(method);
      }
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

  if (!mScriptContext) {
    // We need a context to check if redirect (if any) is allowed
    GetCurrentContext(getter_AddRefs(mScriptContext));
  }

  nsCOMPtr<nsILoadGroup> loadGroup;
  mChannel->GetLoadGroup(getter_AddRefs(loadGroup));

  rv = document->StartDocumentLoad(kLoadAsData, mChannel, 
                                   loadGroup, nsnull, 
                                   getter_AddRefs(listener),
                                   PR_TRUE);

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

  // Hook us up to listen to redirects and the like
  mChannel->SetNotificationCallbacks(this);

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

  if (httpChannel) {
    // We need to set, not add to, the header. Using empty value will
    // clear existing header.
    nsresult rv = httpChannel->SetRequestHeader(nsDependentCString(header),
                                                nsCString());
    if (NS_FAILED(rv))
      return rv;
    // Now set it for real
    return httpChannel->SetRequestHeader(nsDependentCString(header),
                                         nsDependentCString(value));
  }
  
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

/* void   overrideMimeType(in string mimetype); */
NS_IMETHODIMP
nsXMLHttpRequest::OverrideMimeType(const char* aMimeType)
{
  // XXX Should we do some validation here?
  mOverrideMimeType.Assign(aMimeType);
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
  if (mReadRequest) {
    mReadRequest->Cancel(NS_BINDING_ABORTED);
  }
  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
  }
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

/////////////////////////////////////////////////////
// nsIHttpEventSink methods:
//
NS_IMETHODIMP
nsXMLHttpRequest::OnRedirect(nsIHttpChannel *aHttpChannel, nsIChannel *aNewChannel)
{
  NS_ENSURE_ARG_POINTER(aNewChannel);

  if (mScriptContext && !mCrossSiteAccessEnabled) {
    nsresult rv = NS_ERROR_FAILURE;

    nsCOMPtr<nsIJSContextStack> stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1", & rv));
    if (NS_FAILED(rv))
      return rv;

    JSContext *cx = (JSContext *)mScriptContext->GetNativeContext();
    if (!cx)
      return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIScriptSecurityManager> secMan = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIURI> newURI;
    rv = aNewChannel->GetURI(getter_AddRefs(newURI)); // The redirected URI
    if (NS_FAILED(rv)) 
      return rv;

    stack->Push(cx);

    rv = secMan->CheckSameOrigin(cx, newURI);

    stack->Pop(&cx);
    
    if (NS_FAILED(rv))
      return rv;
  }

  mChannel = aNewChannel;

  return NS_OK;
}

/////////////////////////////////////////////////////
// nsIInterfaceRequestor methods:
//
NS_IMETHODIMP
nsXMLHttpRequest::GetInterface(const nsIID & aIID, void **aResult)
{
  if (aIID.Equals(NS_GET_IID(nsIAuthPrompt))) {
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nsnull;

    nsresult rv;
    nsCOMPtr<nsIWindowWatcher> ww(do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIAuthPrompt> prompt;
    rv = ww->GetNewAuthPrompter(nsnull, getter_AddRefs(prompt));
    if (NS_FAILED(rv))
      return rv;

    nsIAuthPrompt *p = prompt.get();
    NS_ADDREF(p);
    *aResult = p;
    return NS_OK;
  }

  return QueryInterface(aIID, aResult);
}


NS_IMPL_ISUPPORTS1(nsXMLHttpRequest::nsHeaderVisitor, nsIHttpHeaderVisitor)

NS_IMETHODIMP nsXMLHttpRequest::
nsHeaderVisitor::VisitHeader(const nsACString &header, const nsACString &value)
{
    mHeaders.Append(header);
    mHeaders.Append(": ");
    mHeaders.Append(value);
    mHeaders.Append('\n');
    return NS_OK;
}

