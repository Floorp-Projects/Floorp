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
#include "nsIStreamObserver.h"
#include "nsISimpleEnumerator.h"
#include "nsIHTTPHeader.h"
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
#ifdef IMPLEMENT_SYNC_LOAD
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIEventQueueService.h"
#endif

static const char* kLoadAsData = "loadAsData";
static const char* kLoadStr = "load";
static const char* kErrorStr = "error";

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
  NS_WITH_SERVICE(nsIJSContextStack, stack, "@mozilla.org/js/xpc/ContextStack;1", 
                  &rv);
  if (NS_FAILED(rv))
    return nsnull;
  nsCOMPtr<nsIThreadJSContextStack> tcs = do_QueryInterface(stack);
  JSContext* cx;
  if (NS_FAILED(tcs->GetSafeJSContext(&cx))) {
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

nsresult 
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
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent);

  // nsIDOMLoadListener
  virtual nsresult Load(nsIDOMEvent* aEvent);
  virtual nsresult Unload(nsIDOMEvent* aEvent);
  virtual nsresult Abort(nsIDOMEvent* aEvent);
  virtual nsresult Error(nsIDOMEvent* aEvent);

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

nsresult
nsLoadListenerProxy::HandleEvent(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMLoadListener> listener = do_QueryReferent(mParent);

  if (listener) {
    return listener->HandleEvent(aEvent);
  }
  
  return NS_OK;
}

nsresult 
nsLoadListenerProxy::Load(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMLoadListener> listener = do_QueryReferent(mParent);

  if (listener) {
    return listener->Load(aEvent);
  }

  return NS_OK;
}

nsresult 
nsLoadListenerProxy::Unload(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMLoadListener> listener = do_QueryReferent(mParent);

  if (listener) {
    return listener->Unload(aEvent);
  }
  
  return NS_OK;
}

nsresult 
nsLoadListenerProxy::Abort(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMLoadListener> listener = do_QueryReferent(mParent);

  if (listener) {
    return listener->Abort(aEvent);
  }
  
  return NS_OK;
}

nsresult 
nsLoadListenerProxy::Error(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMLoadListener> listener = do_QueryReferent(mParent);

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
  if (mDocShellTreeOwner) {
    mDocShellTreeOwner->ExitModalLoop(NS_OK);
  }
#endif
}

NS_IMPL_ADDREF(nsXMLHttpRequest)
NS_IMPL_RELEASE(nsXMLHttpRequest)

NS_INTERFACE_MAP_BEGIN(nsXMLHttpRequest)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXMLHttpRequest)
   NS_INTERFACE_MAP_ENTRY(nsIXMLHttpRequest)
   NS_INTERFACE_MAP_ENTRY(nsIDOMLoadListener)
   NS_INTERFACE_MAP_ENTRY(nsISecurityCheckedComponent)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END
  
/* noscript void addEventListener (in string type, in nsIDOMEventListener listener); */
NS_IMETHODIMP 
nsXMLHttpRequest::AddEventListener(const char *type, 
                                   nsIDOMEventListener *listener)
{
  NS_ENSURE_ARG(type);
  NS_ENSURE_ARG(listener);
  nsresult rv;

  // I know, I know - strcmp's. But it's only for a couple of 
  // cases...and they are short strings. :-)
  if (nsCRT::strcmp(type, kLoadStr) == 0) {
    if (!mLoadEventListeners) {
      rv = NS_NewISupportsArray(getter_AddRefs(mLoadEventListeners));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    mLoadEventListeners->AppendElement(listener);
  }
  else if (nsCRT::strcmp(type, kErrorStr) == 0) {
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

/* noscript void removeEventListener (in string type, in nsIDOMEventListener listener); */
NS_IMETHODIMP 
nsXMLHttpRequest::RemoveEventListener(const char *type, 
                                      nsIDOMEventListener *listener)
{
  NS_ENSURE_ARG(type);
  NS_ENSURE_ARG(listener);

  if (nsCRT::strcmp(type, kLoadStr) == 0) {
    if (mLoadEventListeners) {
      mLoadEventListeners->RemoveElement(listener);
    }
  }
  else if (nsCRT::strcmp(type, kErrorStr) == 0) {
    if (mErrorEventListeners) {
      mErrorEventListeners->RemoveElement(listener);
    }
  }
  else {
    return NS_ERROR_INVALID_ARG;
  }

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
    nsCOMPtr<nsIXPConnectJSObjectHolder> jsobjholder = do_QueryInterface(aObject);
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
  nsCOMPtr<nsIPrivateJSEventListener> jsel = do_QueryInterface(aElement);
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
      nsCOMPtr<nsIPrivateJSEventListener> jsel = do_QueryInterface(aListener);
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
      RemoveEventListener(kLoadStr, oldListener);
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

  return AddEventListener(kLoadStr, listener);
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
      RemoveEventListener(kErrorStr, oldListener);
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

  return AddEventListener(kErrorStr, listener);
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
    return mChannel->GetResponseString(aStatusText);
  }
  
  return NS_OK;
}

/* void abort (); */
NS_IMETHODIMP 
nsXMLHttpRequest::Abort()
{
  if (mChannel) {
    return mChannel->Cancel(NS_BINDING_ABORTED);
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
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    nsCAutoString headers;

    nsresult rv = mChannel->GetResponseHeaderEnumerator(getter_AddRefs(enumerator));
    if (NS_FAILED(rv)) {
      return rv;
    }
    
    PRBool hasMore;
    while(NS_SUCCEEDED(enumerator->HasMoreElements(&hasMore)) && hasMore) {
      nsCOMPtr<nsISupports> isup;
      
      rv = enumerator->GetNext(getter_AddRefs(isup));
      if (NS_FAILED(rv)) {
        break;
      }
      nsCOMPtr<nsIHTTPHeader> header = do_QueryInterface(isup);
      if (header) {
        nsXPIDLCString name, value;
        header->GetFieldName(getter_Copies(name));
        header->GetValue(getter_Copies(value));
        headers.Append((const char*)name);
        headers.Append(": ");
        headers.Append((const char*)value);
        headers.Append("\n");
      }
    }
    *_retval = headers.ToNewCString();
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
  if (mChannel) {
    nsCOMPtr<nsIAtom> headerAtom = dont_AddRef(NS_NewAtom(header));
    return mChannel->GetResponseHeader(headerAtom, _retval);
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
  nsCOMPtr<nsILoadGroup> loadGroup;
  PRBool authp = PR_FALSE;

  // Return error if we're alreday processing a request
  if (XML_HTTP_REQUEST_SENT == mStatus) {
    return NS_ERROR_FAILURE;
  }

  mAsync = async;

  // If we have a base document, use it for the base URL and loadgroup
  if (mBaseDocument) {
    rv = mBaseDocument->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
    if (NS_FAILED(rv)) return rv;
  }

  rv = NS_NewURI(getter_AddRefs(uri), url, mBaseURI);
  if (NS_FAILED(rv)) return rv;

  // Only http URLs are allowed
  nsXPIDLCString protocol;
  uri->GetScheme(getter_Copies(protocol)); 
  if (nsCRT::strncmp("http", (const char*)protocol, 4) != 0) {
    return NS_ERROR_INVALID_ARG;
  }

  if (user) {
    nsCAutoString prehost;
    prehost.Assign(user);
    if (password) {
      prehost.Append(":");
      prehost.Append(password);
    }
    uri->SetPreHost(prehost.GetBuffer());
    authp = PR_TRUE;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_OpenURI(getter_AddRefs(channel), uri, nsnull, loadGroup);
  if (NS_FAILED(rv)) return rv;
  
  mChannel = do_QueryInterface(channel);
  if (!mChannel) {
    return NS_ERROR_INVALID_ARG;
  }

  mChannel->SetAuthTriedWithPrehost(authp);

  nsCOMPtr<nsIAtom> methodAtom = dont_AddRef(NS_NewAtom(method));
  if (methodAtom) {
    rv = mChannel->SetRequestMethod(methodAtom);
  }

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
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIPrincipal> principal;
      rv = secMan->GetSubjectPrincipal(getter_AddRefs(principal));
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsICodebasePrincipal> codebase = do_QueryInterface(principal);
        if (codebase) {
          codebase->GetURI(getter_AddRefs(mBaseURI));
        }
      }
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

  // Shove in the traling CRLF
  postData[headerSize+charLength] = CR;
  postData[headerSize+charLength+1] = LF;
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

/* void send (in nsISupports body); */
NS_IMETHODIMP 
nsXMLHttpRequest::Send(nsISupports *body)
{
  nsresult rv;
  nsCOMPtr<nsIInputStream> postDataStream;

  // Return error if we're alreday processing a request
  if (XML_HTTP_REQUEST_SENT == mStatus) {
    return NS_ERROR_FAILURE;
  }
  
  // Make sure we've been opened
  if (!mChannel) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (body) {
    nsCOMPtr<nsIDOMDocument> doc = do_QueryInterface(body);
    if (doc) {
      // Get an XML serializer
      nsCOMPtr<nsIDOMSerializer> serializer = do_CreateInstance(NS_XMLSERIALIZER_CONTRACTID, &rv);
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;  
      
      // Serialize the current document to string
      nsXPIDLString serial;
      rv = serializer->SerializeToString(doc, getter_Copies(serial));
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
      
      // Convert to a byte stream
      rv = GetStreamForWString((const PRUnichar*)serial, 
                               nsCRT::strlen((const PRUnichar*)serial),
                               getter_AddRefs(postDataStream));
      if (NS_FAILED(rv)) return rv;
    }
    else {
      nsCOMPtr<nsIInputStream> stream = do_QueryInterface(body);
      if (stream) {
        postDataStream = stream;
      }
      else {
        nsCOMPtr<nsISupportsWString> wstr = do_QueryInterface(body);
        if (wstr) {
          nsXPIDLString holder;
          wstr->GetData(getter_Copies(holder));
          rv = GetStreamForWString((const PRUnichar*)holder, 
                                   nsCRT::strlen((const PRUnichar*)holder),
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
  nsCOMPtr<nsIDOMDOMImplementation> implementation = do_CreateInstance(kIDOMDOMImplementationCID, &rv);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  
  if (mBaseURI) {
    nsCOMPtr<nsIPrivateDOMImplementation> privImpl = do_QueryInterface(implementation);
    if (privImpl) {
      privImpl->Init(mBaseURI);
    }
  }

  // Create an empty document from it
  nsAutoString emptyStr;
  rv = implementation->CreateDocument(emptyStr, 
                                      emptyStr, 
                                      nsnull, 
                                      getter_AddRefs(mDocument));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  // Register as a load listener on the document
  nsCOMPtr<nsIDOMEventReceiver> target = do_QueryInterface(mDocument);
  if (target) {
    nsWeakPtr requestWeak = getter_AddRefs(NS_GetWeakReference(NS_STATIC_CAST(nsIXMLHttpRequest*, this)));
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
  nsCOMPtr<nsIDocument> document = do_QueryInterface(mDocument);
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

      rv = item->GetTreeOwner(getter_AddRefs(mDocShellTreeOwner));
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

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
  rv = mChannel->AsyncRead(listener, nsnull);

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
  if (!mAsync && mDocShellTreeOwner) {
    rv = mDocShellTreeOwner->ShowModal();
    
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
  if (mChannel) {
    nsCOMPtr<nsIAtom> headerAtom = dont_AddRef(NS_NewAtom(header));
    return mChannel->SetRequestHeader(headerAtom, value);
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
  mStatus = XML_HTTP_REQUEST_COMPLETED;
#ifdef IMPLEMENT_SYNC_LOAD
  if (mDocShellTreeOwner) {
    mDocShellTreeOwner->ExitModalLoop(NS_OK);
    mDocShellTreeOwner = 0;
  }
#endif
  if (mLoadEventListeners) {
    PRUint32 index, count;

    mLoadEventListeners->Count(&count);
    for (index = 0; index < count; index++) {
      nsCOMPtr<nsISupports> current = dont_AddRef(mLoadEventListeners->ElementAt(index));
      if (current) {
        nsCOMPtr<nsIDOMEventListener> listener = do_QueryInterface(current);
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
  if (mDocShellTreeOwner) {
    mDocShellTreeOwner->ExitModalLoop(NS_OK);
    mDocShellTreeOwner = 0;
  }
#endif

  return NS_OK;
}

nsresult
nsXMLHttpRequest::Error(nsIDOMEvent* aEvent)
{
  mStatus = XML_HTTP_REQUEST_ABORTED;
#ifdef IMPLEMENT_SYNC_LOAD
  if (mDocShellTreeOwner) {
    mDocShellTreeOwner->ExitModalLoop(NS_OK);
    mDocShellTreeOwner = 0;
  }
#endif
  if (mErrorEventListeners) {
    PRUint32 index, count;

    mErrorEventListeners->Count(&count);
    for (index = 0; index < count; index++) {
      nsCOMPtr<nsISupports> current = dont_AddRef(mErrorEventListeners->ElementAt(index));
      if (current) {
        nsCOMPtr<nsIDOMEventListener> listener = do_QueryInterface(current);
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
