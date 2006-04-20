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
#include "nsIDOMSerializer.h"
#include "nsISupportsPrimitives.h"
#include "nsIDOMEventReceiver.h"
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
#endif
#include "nsIDOMClassInfo.h"

static const char* kLoadAsData = "loadAsData";
#define LOADSTR NS_LITERAL_STRING("load")
#define ERRORSTR NS_LITERAL_STRING("error")

// CIDs
static NS_DEFINE_CID(kIDOMDOMImplementationCID, NS_DOM_IMPLEMENTATION_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
#ifdef IMPLEMENT_SYNC_LOAD
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
#endif

static JSContext*
GetSafeContext()
{
  // Get the "safe" JSContext: our JSContext of last resort
  nsresult rv;
  nsCOMPtr<nsIThreadJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv))
    return nsnull;

  JSContext* cx;
  if (NS_FAILED(stack->GetSafeJSContext(&cx))) {
    return nsnull;
  }
  return cx;
}
 
static JSContext*
GetCurrentContext()
{
  // Get JSContext from stack.
  nsresult rv;
  NS_WITH_SERVICE(nsIJSContextStack, stack, "@mozilla.org/js/xpc/ContextStack;1", 
                  &rv);
  if (NS_FAILED(rv))
    return nsnull;
  JSContext *cx;
  if (NS_FAILED(stack->Peek(&cx)))
    return nsnull;
  return cx;
}

nsXMLHttpRequestScriptListener::nsXMLHttpRequestScriptListener(JSObject* aScopeObj, JSObject* aFunctionObj)
{
  NS_INIT_ISUPPORTS();
  // We don't have to add a GC root for the scope object
  // since we'll go away if it goes away
  mScopeObj = aScopeObj;
  mFunctionObj = aFunctionObj;
  JSContext* cx;
  cx = GetSafeContext();
  if (cx) {
    JS_AddNamedRoot(cx, &mFunctionObj, "nsXMLHttpRequest");
  }
}

nsXMLHttpRequestScriptListener::~nsXMLHttpRequestScriptListener()
{
  JSContext* cx;
  cx = GetSafeContext();
  if (cx) {
    JS_RemoveRoot(cx, &mFunctionObj);
  }
}

NS_IMPL_ISUPPORTS2(nsXMLHttpRequestScriptListener, nsIDOMEventListener, nsIPrivateJSEventListener)

NS_IMETHODIMP
nsXMLHttpRequestScriptListener::HandleEvent(nsIDOMEvent* aEvent)
{
  JSContext* cx;
  cx = GetCurrentContext();
  if (!cx) {
    cx = GetSafeContext();
  }
  if (cx) {
    jsval val;

    // Hmmm...we can't pass along the nsIDOMEvent because
    // we may not have the right type of context (required
    // to get a JSObject from a nsIScriptObjectOwner)
    JS_CallFunctionValue(cx, mScopeObj, 
                         OBJECT_TO_JSVAL(mFunctionObj),
                         0, nsnull, &val);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXMLHttpRequestScriptListener::GetFunctionObj(JSObject** aObj)
{
  NS_ENSURE_ARG_POINTER(aObj);
  
  *aObj = mFunctionObj;
  return NS_OK;
}

/////////////////////////////////////////////
//
// This class exists to prevent a circular reference between
// the loaded document and the nsXMLHttpRequest instance. The
// request owns the document. While the document is loading, 
// the request is a load listener, held onto by the document.
// The proxy class breaks the circularity by filling in as the
// load listener and holding a weak reference to the request
// object.
//
/////////////////////////////////////////////

class nsLoadListenerProxy : public nsIDOMLoadListener {
public:
  nsLoadListenerProxy(nsWeakPtr aParent);
  virtual ~nsLoadListenerProxy();

  NS_DECL_ISUPPORTS

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

  // nsIDOMLoadListener
  NS_IMETHOD Load(nsIDOMEvent* aEvent);
  NS_IMETHOD Unload(nsIDOMEvent* aEvent);
  NS_IMETHOD Abort(nsIDOMEvent* aEvent);
  NS_IMETHOD Error(nsIDOMEvent* aEvent);

protected:
  nsWeakPtr  mParent;
};

nsLoadListenerProxy::nsLoadListenerProxy(nsWeakPtr aParent)
{
  NS_INIT_ISUPPORTS();
  mParent = aParent;
}

nsLoadListenerProxy::~nsLoadListenerProxy()
{
}

NS_IMPL_ISUPPORTS1(nsLoadListenerProxy, nsIDOMLoadListener)

NS_IMETHODIMP
nsLoadListenerProxy::HandleEvent(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMLoadListener> listener(do_QueryReferent(mParent));

  if (listener) {
    return listener->HandleEvent(aEvent);
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsLoadListenerProxy::Load(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMLoadListener> listener(do_QueryReferent(mParent));

  if (listener) {
    return listener->Load(aEvent);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLoadListenerProxy::Unload(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMLoadListener> listener(do_QueryReferent(mParent));

  if (listener) {
    return listener->Unload(aEvent);
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsLoadListenerProxy::Abort(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMLoadListener> listener(do_QueryReferent(mParent));

  if (listener) {
    return listener->Abort(aEvent);
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsLoadListenerProxy::Error(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMLoadListener> listener(do_QueryReferent(mParent));

  if (listener) {
    return listener->Error(aEvent);
  }
  
  return NS_OK;
}

/////////////////////////////////////////////
//
//
/////////////////////////////////////////////

nsXMLHttpRequest::nsXMLHttpRequest()
{
  NS_INIT_ISUPPORTS();
  mStatus = XML_HTTP_REQUEST_INITIALIZED;
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


// XPConnect interface list for nsXMLHttpRequest
NS_CLASSINFO_MAP_BEGIN(XMLHttpRequest)
  NS_CLASSINFO_MAP_ENTRY(nsIXMLHttpRequest)
  NS_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
NS_CLASSINFO_MAP_END


// QueryInterface implementation for nsXMLHttpRequest
NS_INTERFACE_MAP_BEGIN(nsXMLHttpRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXMLHttpRequest)
  NS_INTERFACE_MAP_ENTRY(nsIXMLHttpRequest)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLoadListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsISecurityCheckedComponent)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_DOM_CLASSINFO(XMLHttpRequest)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsXMLHttpRequest)
NS_IMPL_RELEASE(nsXMLHttpRequest)


/* void addEventListener (in string type, in nsIDOMEventListener listener); */
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

/* void removeEventListener (in string type, in nsIDOMEventListener listener); */
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

nsresult
nsXMLHttpRequest::MakeScriptEventListener(nsISupports* aObject,
                                          nsIDOMEventListener** aListener)
{
  nsresult rv;

  *aListener = nsnull;

  nsCOMPtr<nsIXPCNativeCallContext> cc;
  NS_WITH_SERVICE(nsIXPConnect, xpc, nsIXPConnect::GetCID(), &rv);
  if(NS_SUCCEEDED(rv)) {
    rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
  }

  if (NS_SUCCEEDED(rv) && cc) {
    nsCOMPtr<nsIXPConnectJSObjectHolder> jsobjholder(do_QueryInterface(aObject));
    if (jsobjholder) {
      JSObject* funobj;
      rv = jsobjholder->GetJSObject(&funobj);
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

      JSContext* cx;
      rv = cc->GetJSContext(&cx);
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
      
      JSFunction* fun = JS_ValueToFunction(cx, OBJECT_TO_JSVAL(funobj));
      if (!fun) {
        return NS_ERROR_INVALID_ARG;
      }
      
      nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
      rv = cc->GetCalleeWrapper(getter_AddRefs(wrapper));
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

      JSObject* scopeobj;
      rv = wrapper->GetJSObject(&scopeobj);
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

      nsXMLHttpRequestScriptListener* listener = new nsXMLHttpRequestScriptListener(scopeobj, funobj);
      if (!listener) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      *aListener = listener;
      NS_ADDREF(*aListener);
    }
  }
  
  return NS_OK;
}

static PRBool 
CheckForScriptListener(nsISupports* aElement, void *aData)
{
  nsCOMPtr<nsIPrivateJSEventListener> jsel(do_QueryInterface(aElement));
  if (jsel) {
    nsIDOMEventListener** retval = (nsIDOMEventListener**)aData;

    aElement->QueryInterface(NS_GET_IID(nsIDOMEventListener), (void**)retval);
    return PR_FALSE;
  }
  return PR_TRUE;
}

void
nsXMLHttpRequest::GetScriptEventListener(nsISupportsArray* aList, 
                                         nsIDOMEventListener** aListener)
{
  aList->EnumerateForwards(CheckForScriptListener, (void*)aListener);
}

PRBool
nsXMLHttpRequest::StuffReturnValue(nsIDOMEventListener* aListener)
{
  nsresult rv;

  nsCOMPtr<nsIXPCNativeCallContext> cc;
  NS_WITH_SERVICE(nsIXPConnect, xpc, nsIXPConnect::GetCID(), &rv);
  if(NS_SUCCEEDED(rv)) {
    rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
  }

  // If we're being called through JS, stuff the return value
  if (NS_SUCCEEDED(rv) && cc) {
    jsval* val;
    rv = cc->GetRetValPtr(&val);
    if (NS_SUCCEEDED(rv)) {
      JSObject* obj;
      nsCOMPtr<nsIPrivateJSEventListener> jsel(do_QueryInterface(aListener));
      if (jsel) {
        jsel->GetFunctionObj(&obj);
        *val = OBJECT_TO_JSVAL(obj);
        cc->SetReturnValueWasSet(JS_TRUE);
        return PR_TRUE;
      }
    }
  }
  
  return PR_FALSE;
}

/* attribute nsIDOMEventListener onload; */
NS_IMETHODIMP 
nsXMLHttpRequest::GetOnload(nsISupports * *aOnLoad)
{
  NS_ENSURE_ARG_POINTER(aOnLoad);

  *aOnLoad = nsnull;
  if (mLoadEventListeners) {
    nsCOMPtr<nsIDOMEventListener> listener;
    
    GetScriptEventListener(mLoadEventListeners, getter_AddRefs(listener));
    if (listener) {
      StuffReturnValue(listener);
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLHttpRequest::SetOnload(nsISupports * aOnLoad)
{
  NS_ENSURE_ARG(aOnLoad);

  nsresult rv;
  nsCOMPtr<nsIDOMEventListener> listener;

  rv = MakeScriptEventListener(aOnLoad, getter_AddRefs(listener));
  if (NS_FAILED(rv)) return rv;

  if (listener) {
    nsCOMPtr<nsIDOMEventListener> oldListener;
    
    // Remove any old script event listener that exists since
    // we can only have one
    if (mLoadEventListeners) {
      GetScriptEventListener(mLoadEventListeners, getter_AddRefs(oldListener));
      RemoveEventListener(LOADSTR, oldListener, PR_TRUE);
    }
  }
  else {
    // If it's not a script event listener, try to directly QI it to
    // an actual event listener
    listener = do_QueryInterface(aOnLoad);
    if (!listener) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return AddEventListener(LOADSTR, listener, PR_TRUE);
}

/* attribute nsIDOMEventListener onerror; */
NS_IMETHODIMP 
nsXMLHttpRequest::GetOnerror(nsISupports * *aOnerror)
{
  NS_ENSURE_ARG_POINTER(aOnerror);

  *aOnerror = nsnull;
  if (mErrorEventListeners) {
    nsCOMPtr<nsIDOMEventListener> listener;
    
    GetScriptEventListener(mErrorEventListeners, getter_AddRefs(listener));
    if (listener) {
      StuffReturnValue(listener);
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLHttpRequest::SetOnerror(nsISupports * aOnerror)
{
  NS_ENSURE_ARG(aOnerror);

  nsresult rv;
  nsCOMPtr<nsIDOMEventListener> listener;

  rv = MakeScriptEventListener(aOnerror, getter_AddRefs(listener));
  if (NS_FAILED(rv)) return rv;

  if (listener) {
    nsCOMPtr<nsIDOMEventListener> oldListener;
    
    // Remove any old script event listener that exists since
    // we can only have one
    if (mErrorEventListeners) {
      GetScriptEventListener(mErrorEventListeners, 
                             getter_AddRefs(oldListener));
      RemoveEventListener(ERRORSTR, oldListener, PR_TRUE);
    }
  }
  else {
    // If it's not a script event listener, try to directly QI it to
    // an actual event listener
    listener = do_QueryInterface(aOnerror);
    if (!listener) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return AddEventListener(ERRORSTR, listener, PR_TRUE);
}

/* readonly attribute nsIHttpChannel channel; */
NS_IMETHODIMP nsXMLHttpRequest::GetChannel(nsIHttpChannel **aChannel)
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
      if(start<0) {
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
  if ((XML_HTTP_REQUEST_COMPLETED == mStatus)) {
    // First check if we can represent the data as a string - if it contains
    // nulls we won't try. 
    if (mResponseBody.FindChar('\0') >= 0)
      return NS_ERROR_FAILURE;
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
  if (mChannel) {
    return mChannel->GetResponseStatus(aStatus);
  }
  
  return NS_OK;
}

/* readonly attribute string statusText; */
NS_IMETHODIMP 
nsXMLHttpRequest::GetStatusText(char * *aStatusText)
{
  NS_ENSURE_ARG_POINTER(aStatusText);
  *aStatusText = nsnull;
  if (mChannel) {
    return mChannel->GetResponseStatusText(aStatusText);
  }
  
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
  if (mChannel) {
    nsHeaderVisitor *visitor = nsnull;
    NS_NEWXPCOM(visitor, nsHeaderVisitor);
    if (!visitor)
      return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(visitor);

    nsresult rv = mChannel->VisitResponseHeaders(visitor);
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

  *_retval = nsnull;
  if (mChannel)
    return mChannel->GetResponseHeader(header, _retval);
  
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

  // Only http URLs are allowed
  // The following check takes the place of nsScriptSecurityManager::CheckLoadURI
  // since loads of http URLs are always allowed.
  PRBool isHTTP = PR_FALSE;
  uri->SchemeIs("http", &isHTTP);
  if (!isHTTP)
      return NS_ERROR_INVALID_ARG;

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

  nsCOMPtr<nsIChannel> channel;
  rv = NS_OpenURI(getter_AddRefs(channel), uri, nsnull, nsnull);
  if (NS_FAILED(rv)) return rv;
  
  mChannel = do_QueryInterface(channel);
  if (!mChannel) {
    return NS_ERROR_INVALID_ARG;
  }

  //mChannel->SetAuthTriedWithPrehost(authp);

  rv = mChannel->SetRequestMethod(method);

  mStatus = XML_HTTP_REQUEST_OPENED;

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
  NS_WITH_SERVICE(nsIXPConnect, xpc, nsIXPConnect::GetCID(), &rv);
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

    NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                    NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
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
      // Security check failed. The above call set a JS exception. The
      // following lines ensure that the exception is propagated.

      NS_WITH_SERVICE(nsIXPConnect, xpc, nsIXPConnect::GetCID(), &rv);
      nsCOMPtr<nsIXPCNativeCallContext> cc;
      if(NS_SUCCEEDED(rv))
        xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
      if (cc)
        cc->SetExceptionWasThrown(PR_TRUE);
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
  
#define MAX_HEADER_SIZE 128

  // Allocate extra space for the header and trailing CRLF
  postData = (char*)nsMemory::Alloc(MAX_HEADER_SIZE + charLength + 3);
  if (!postData) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  rv = encoder->Convert(unicodeBuf, 
                        &unicodeLength, postData+MAX_HEADER_SIZE, &charLength);
  if (NS_FAILED(rv)) {
    nsMemory::Free(postData);
    return NS_ERROR_FAILURE;
  }

  // Now that we know the real content length we can create the header
  PR_snprintf(postData,
              MAX_HEADER_SIZE,
              "Content-type: text/xml\015\012Content-Length: %d\015\012\015\012",
              charLength);
  PRInt32 headerSize = nsCRT::strlen(postData);

  // Copy the post data to immediately follow the header
  nsCRT::memcpy(postData+headerSize, postData+MAX_HEADER_SIZE, charLength);

  // Shove in the trailing CRLF
  postData[headerSize+charLength] = nsCRT::CR;
  postData[headerSize+charLength+1] = nsCRT::LF;
  postData[headerSize+charLength+2] = '\0';

  // The new stream takes ownership of the buffer
  rv = NS_NewByteArrayInputStream((nsIByteArrayInputStream**)aStream, 
                                  postData, 
                                  headerSize+charLength+2);
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
  nsresult result = inStr->ReadSegments(nsXMLHttpRequest::StreamReaderFunc, (void*)this, count, &totalRead);
  if (NS_FAILED(result)) {
    mResponseBody.Truncate();
  }

  return result;
}

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP 
nsXMLHttpRequest::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  mReadRequest = request;
  mContext = ctxt;
  return mXMLParserStreamListener->OnStartRequest(request,ctxt);
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP 
nsXMLHttpRequest::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult status)
{
  nsresult rv = mXMLParserStreamListener->OnStopRequest(request,ctxt,status);
  mXMLParserStreamListener = nsnull;
  mReadRequest = nsnull;
  mContext = nsnull;
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
  if (!mChannel) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Ignore argument if method is GET, there is no point in trying to upload anything
  nsXPIDLCString method;
  mChannel->GetRequestMethod(getter_Copies(method)); // If GET, method name will be uppercase

  if (body && nsCRT::strcmp("GET",method.get()) != 0) {
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
      rv = mChannel->SetUploadStream(postDataStream);
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
    NS_WITH_SERVICE(nsIXPConnect, xpc, nsIXPConnect::GetCID(), &rv);
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
      global = dont_AddRef(scriptCX->GetGlobalObject());
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
  mStatus = XML_HTTP_REQUEST_SENT;
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
  if (mChannel)
    return mChannel->SetRequestHeader(header, value);
  
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
  mStatus = XML_HTTP_REQUEST_COMPLETED;
#ifdef IMPLEMENT_SYNC_LOAD
  if (mChromeWindow) {
    mChromeWindow->ExitModalEventLoop(NS_OK);
    mChromeWindow = 0;
  }
#endif
  if (mLoadEventListeners) {
    PRUint32 index, count;

    mLoadEventListeners->Count(&count);
    for (index = 0; index < count; index++) {
      nsCOMPtr<nsISupports> current = dont_AddRef(mLoadEventListeners->ElementAt(index));
      if (current) {
        nsCOMPtr<nsIDOMEventListener> listener(do_QueryInterface(current));
        if (listener) {
          listener->HandleEvent(aEvent);
        }
      }
    }
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
  mStatus = XML_HTTP_REQUEST_ABORTED;
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
  mStatus = XML_HTTP_REQUEST_ABORTED;
#ifdef IMPLEMENT_SYNC_LOAD
  if (mChromeWindow) {
    mChromeWindow->ExitModalEventLoop(NS_OK);
    mChromeWindow = 0;
  }
#endif
  if (mErrorEventListeners) {
    PRUint32 index, count;

    mErrorEventListeners->Count(&count);
    for (index = 0; index < count; index++) {
      nsCOMPtr<nsISupports> current = dont_AddRef(mErrorEventListeners->ElementAt(index));
      if (current) {
        nsCOMPtr<nsIDOMEventListener> listener(do_QueryInterface(current));
        if (listener) {
          listener->HandleEvent(aEvent);
        }
      }
    }
  }

  return NS_OK;
}

static const char* kAllAccess = "AllAccess";

/* string canCreateWrapper (in nsIIDPtr iid); */
NS_IMETHODIMP 
nsXMLHttpRequest::CanCreateWrapper(const nsIID * iid, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsIXMLHttpRequest))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canCallMethod (in nsIIDPtr iid, in wstring methodName); */
NS_IMETHODIMP 
nsXMLHttpRequest::CanCallMethod(const nsIID * iid, const PRUnichar *methodName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsIXMLHttpRequest))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsXMLHttpRequest::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsIXMLHttpRequest))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canSetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsXMLHttpRequest::CanSetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsIXMLHttpRequest))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
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
