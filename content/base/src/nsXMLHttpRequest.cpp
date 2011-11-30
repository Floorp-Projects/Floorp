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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "mozilla/Util.h"

#include "nsXMLHttpRequest.h"
#include "nsISimpleEnumerator.h"
#include "nsIXPConnect.h"
#include "nsICharsetConverterManager.h"
#include "nsLayoutCID.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIURI.h"
#include "nsILoadGroup.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "nsIUploadChannel.h"
#include "nsIUploadChannel2.h"
#include "nsIDOMSerializer.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsGUIEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "prprf.h"
#include "nsIDOMEventListener.h"
#include "nsIJSContextStack.h"
#include "nsIScriptSecurityManager.h"
#include "nsWeakPtr.h"
#include "nsICharsetAlias.h"
#include "nsIScriptGlobalObject.h"
#include "nsDOMClassInfoID.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "nsIVariant.h"
#include "xpcprivate.h"
#include "XPCQuickStubs.h"
#include "nsStringStream.h"
#include "nsIStreamConverterService.h"
#include "nsICachingChannel.h"
#include "nsContentUtils.h"
#include "nsEventDispatcher.h"
#include "nsDOMJSUtils.h"
#include "nsCOMArray.h"
#include "nsIScriptableUConv.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsContentErrors.h"
#include "nsLayoutStatics.h"
#include "nsCrossSiteListenerProxy.h"
#include "nsDOMError.h"
#include "nsIHTMLDocument.h"
#include "nsIMultiPartChannel.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIStorageStream.h"
#include "nsIPromptFactory.h"
#include "nsIWindowWatcher.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsIConsoleService.h"
#include "nsIChannelPolicy.h"
#include "nsChannelPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsAsyncRedirectVerifyHelper.h"
#include "jstypedarray.h"
#include "nsStringBuffer.h"
#include "nsDOMFile.h"
#include "nsIFileChannel.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;

#define LOAD_STR "load"
#define ERROR_STR "error"
#define ABORT_STR "abort"
#define LOADSTART_STR "loadstart"
#define PROGRESS_STR "progress"
#define UPLOADPROGRESS_STR "uploadprogress"
#define READYSTATE_STR "readystatechange"
#define LOADEND_STR "loadend"

// CIDs

// State
#define XML_HTTP_REQUEST_UNSENT           (1 << 0) // 0 UNSENT
#define XML_HTTP_REQUEST_OPENED           (1 << 1) // 1 OPENED
#define XML_HTTP_REQUEST_HEADERS_RECEIVED (1 << 2) // 2 HEADERS_RECEIVED
#define XML_HTTP_REQUEST_LOADING          (1 << 3) // 3 LOADING
#define XML_HTTP_REQUEST_DONE             (1 << 4) // 4 DONE
#define XML_HTTP_REQUEST_SENT             (1 << 5) // Internal, OPENED in IE and external view
#define XML_HTTP_REQUEST_STOPPED          (1 << 6) // Internal, LOADING in IE and external view
// The above states are mutually exclusive, change with ChangeState() only.
// The states below can be combined.
#define XML_HTTP_REQUEST_ABORTED        (1 << 7)  // Internal
#define XML_HTTP_REQUEST_ASYNC          (1 << 8)  // Internal
#define XML_HTTP_REQUEST_PARSEBODY      (1 << 9)  // Internal
#define XML_HTTP_REQUEST_SYNCLOOPING    (1 << 10) // Internal
#define XML_HTTP_REQUEST_MULTIPART      (1 << 11) // Internal
#define XML_HTTP_REQUEST_GOT_FINAL_STOP (1 << 12) // Internal
#define XML_HTTP_REQUEST_BACKGROUND     (1 << 13) // Internal
// This is set when we've got the headers for a multipart XMLHttpRequest,
// but haven't yet started to process the first part.
#define XML_HTTP_REQUEST_MPART_HEADERS  (1 << 14) // Internal
#define XML_HTTP_REQUEST_USE_XSITE_AC   (1 << 15) // Internal
#define XML_HTTP_REQUEST_NEED_AC_PREFLIGHT (1 << 16) // Internal
#define XML_HTTP_REQUEST_AC_WITH_CREDENTIALS (1 << 17) // Internal

#define XML_HTTP_REQUEST_LOADSTATES         \
  (XML_HTTP_REQUEST_UNSENT |                \
   XML_HTTP_REQUEST_OPENED |                \
   XML_HTTP_REQUEST_HEADERS_RECEIVED |      \
   XML_HTTP_REQUEST_LOADING |               \
   XML_HTTP_REQUEST_DONE |                  \
   XML_HTTP_REQUEST_SENT |                  \
   XML_HTTP_REQUEST_STOPPED)

#define NS_BADCERTHANDLER_CONTRACTID \
  "@mozilla.org/content/xmlhttprequest-bad-cert-handler;1"

#define NS_PROGRESS_EVENT_INTERVAL 50

NS_IMPL_ISUPPORTS1(nsXHRParseEndListener, nsIDOMEventListener)

class nsResumeTimeoutsEvent : public nsRunnable
{
public:
  nsResumeTimeoutsEvent(nsPIDOMWindow* aWindow) : mWindow(aWindow) {}

  NS_IMETHOD Run()
  {
    mWindow->ResumeTimeouts(false);
    return NS_OK;
  }

private:
  nsCOMPtr<nsPIDOMWindow> mWindow;
};


// This helper function adds the given load flags to the request's existing
// load flags.
static void AddLoadFlags(nsIRequest *request, nsLoadFlags newFlags)
{
  nsLoadFlags flags;
  request->GetLoadFlags(&flags);
  flags |= newFlags;
  request->SetLoadFlags(flags);
}

static nsresult IsCapabilityEnabled(const char *capability, bool *enabled)
{
  nsIScriptSecurityManager *secMan = nsContentUtils::GetSecurityManager();
  if (!secMan)
    return NS_ERROR_FAILURE;

  return secMan->IsCapabilityEnabled(capability, enabled);
}

// Helper proxy class to be used when expecting an
// multipart/x-mixed-replace stream of XML documents.

class nsMultipartProxyListener : public nsIStreamListener
{
public:
  nsMultipartProxyListener(nsIStreamListener *dest);
  virtual ~nsMultipartProxyListener();

  /* additional members */
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

private:
  nsCOMPtr<nsIStreamListener> mDestListener;
};


nsMultipartProxyListener::nsMultipartProxyListener(nsIStreamListener *dest)
  : mDestListener(dest)
{
}

nsMultipartProxyListener::~nsMultipartProxyListener()
{
}

NS_IMPL_ISUPPORTS2(nsMultipartProxyListener, nsIStreamListener,
                   nsIRequestObserver)

/** nsIRequestObserver methods **/

NS_IMETHODIMP
nsMultipartProxyListener::OnStartRequest(nsIRequest *aRequest,
                                         nsISupports *ctxt)
{
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  NS_ENSURE_TRUE(channel, NS_ERROR_UNEXPECTED);

  nsCAutoString contentType;
  nsresult rv = channel->GetContentType(contentType);

  if (!contentType.EqualsLiteral("multipart/x-mixed-replace")) {
    return NS_ERROR_INVALID_ARG;
  }

  // If multipart/x-mixed-replace content, we'll insert a MIME
  // decoder in the pipeline to handle the content and pass it along
  // to our original listener.

  nsCOMPtr<nsIXMLHttpRequest> xhr = do_QueryInterface(mDestListener);

  nsCOMPtr<nsIStreamConverterService> convServ =
    do_GetService("@mozilla.org/streamConverters;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIStreamListener> toListener(mDestListener);
    nsCOMPtr<nsIStreamListener> fromListener;

    rv = convServ->AsyncConvertData("multipart/x-mixed-replace",
                                    "*/*",
                                    toListener,
                                    nsnull,
                                    getter_AddRefs(fromListener));
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && fromListener, NS_ERROR_UNEXPECTED);

    mDestListener = fromListener;
  }

  if (xhr) {
    static_cast<nsXMLHttpRequest*>(xhr.get())->mState |=
      XML_HTTP_REQUEST_MPART_HEADERS;
   }

  return mDestListener->OnStartRequest(aRequest, ctxt);
}

NS_IMETHODIMP
nsMultipartProxyListener::OnStopRequest(nsIRequest *aRequest,
                                        nsISupports *ctxt,
                                        nsresult status)
{
  return mDestListener->OnStopRequest(aRequest, ctxt, status);
}

/** nsIStreamListener methods **/

NS_IMETHODIMP
nsMultipartProxyListener::OnDataAvailable(nsIRequest *aRequest,
                                          nsISupports *ctxt,
                                          nsIInputStream *inStr,
                                          PRUint32 sourceOffset,
                                          PRUint32 count)
{
  return mDestListener->OnDataAvailable(aRequest, ctxt, inStr, sourceOffset,
                                        count);
}

/////////////////////////////////////////////

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXHREventTarget)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsXHREventTarget,
                                                  nsDOMEventTargetWrapperCache)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnLoadListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnErrorListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnAbortListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnLoadStartListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnProgressListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnLoadendListener)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsXHREventTarget,
                                                nsDOMEventTargetWrapperCache)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnLoadListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnErrorListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnAbortListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnLoadStartListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnProgressListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnLoadendListener)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsXHREventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIXMLHttpRequestEventTarget)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetWrapperCache)

NS_IMPL_ADDREF_INHERITED(nsXHREventTarget, nsDOMEventTargetWrapperCache)
NS_IMPL_RELEASE_INHERITED(nsXHREventTarget, nsDOMEventTargetWrapperCache)

NS_IMETHODIMP
nsXHREventTarget::GetOnload(nsIDOMEventListener** aOnLoad)
{
  return GetInnerEventListener(mOnLoadListener, aOnLoad);
}

NS_IMETHODIMP
nsXHREventTarget::SetOnload(nsIDOMEventListener* aOnLoad)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(LOAD_STR),
                                mOnLoadListener, aOnLoad);
}

NS_IMETHODIMP
nsXHREventTarget::GetOnerror(nsIDOMEventListener** aOnerror)
{
  return GetInnerEventListener(mOnErrorListener, aOnerror);
}

NS_IMETHODIMP
nsXHREventTarget::SetOnerror(nsIDOMEventListener* aOnerror)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(ERROR_STR),
                                mOnErrorListener, aOnerror);
}

NS_IMETHODIMP
nsXHREventTarget::GetOnabort(nsIDOMEventListener** aOnabort)
{
  return GetInnerEventListener(mOnAbortListener, aOnabort);
}

NS_IMETHODIMP
nsXHREventTarget::SetOnabort(nsIDOMEventListener* aOnabort)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(ABORT_STR),
                                mOnAbortListener, aOnabort);
}

NS_IMETHODIMP
nsXHREventTarget::GetOnloadstart(nsIDOMEventListener** aOnloadstart)
{
  return GetInnerEventListener(mOnLoadStartListener, aOnloadstart);
}

NS_IMETHODIMP
nsXHREventTarget::SetOnloadstart(nsIDOMEventListener* aOnloadstart)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(LOADSTART_STR),
                                mOnLoadStartListener, aOnloadstart);
}

NS_IMETHODIMP
nsXHREventTarget::GetOnprogress(nsIDOMEventListener** aOnprogress)
{
  return GetInnerEventListener(mOnProgressListener, aOnprogress);
}

NS_IMETHODIMP
nsXHREventTarget::SetOnprogress(nsIDOMEventListener* aOnprogress)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(PROGRESS_STR),
                                mOnProgressListener, aOnprogress);
}

NS_IMETHODIMP
nsXHREventTarget::GetOnloadend(nsIDOMEventListener** aOnLoadend)
{
  return GetInnerEventListener(mOnLoadendListener, aOnLoadend);
}

NS_IMETHODIMP
nsXHREventTarget::SetOnloadend(nsIDOMEventListener* aOnLoadend)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(LOADEND_STR),
                                mOnLoadendListener, aOnLoadend);
}

/////////////////////////////////////////////

nsXMLHttpRequestUpload::~nsXMLHttpRequestUpload()
{
  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
}

DOMCI_DATA(XMLHttpRequestUpload, nsXMLHttpRequestUpload)

NS_INTERFACE_MAP_BEGIN(nsXMLHttpRequestUpload)
  NS_INTERFACE_MAP_ENTRY(nsIXMLHttpRequestUpload)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(XMLHttpRequestUpload)
NS_INTERFACE_MAP_END_INHERITING(nsXHREventTarget)

NS_IMPL_ADDREF_INHERITED(nsXMLHttpRequestUpload, nsXHREventTarget)
NS_IMPL_RELEASE_INHERITED(nsXMLHttpRequestUpload, nsXHREventTarget)

/////////////////////////////////////////////
//
//
/////////////////////////////////////////////

nsXMLHttpRequest::nsXMLHttpRequest()
  : mResponseBodyDecodedPos(0),
    mResponseType(XML_HTTP_RESPONSE_TYPE_DEFAULT),
    mRequestObserver(nsnull), mState(XML_HTTP_REQUEST_UNSENT),
    mUploadTransferred(0), mUploadTotal(0), mUploadComplete(true),
    mProgressSinceLastProgressEvent(false),
    mUploadProgress(0), mUploadProgressMax(0),
    mErrorLoad(false), mTimerIsActive(false),
    mProgressEventWasDelayed(false),
    mLoadLengthComputable(false),
    mIsHtml(false),
    mWarnAboutMultipartHtml(false),
    mWarnAboutSyncHtml(false),
    mLoadTotal(0),
    mFirstStartRequestSeen(false),
    mInLoadProgressEvent(false),
    mResultJSON(JSVAL_VOID),
    mResultArrayBuffer(nsnull)
{
  nsLayoutStatics::AddRef();
}

nsXMLHttpRequest::~nsXMLHttpRequest()
{
  if (mListenerManager) {
    mListenerManager->Disconnect();
  }

  if (mState & (XML_HTTP_REQUEST_STOPPED |
                XML_HTTP_REQUEST_SENT |
                XML_HTTP_REQUEST_LOADING)) {
    Abort();
  }

  NS_ABORT_IF_FALSE(!(mState & XML_HTTP_REQUEST_SYNCLOOPING), "we rather crash than hang");
  mState &= ~XML_HTTP_REQUEST_SYNCLOOPING;

  // This can happen if the XHR was only used by C++ (and so never created a JS
  // wrapper) that also made an ArrayBuffer.
  if (PreservingWrapper()) {
    nsContentUtils::ReleaseWrapper(
      static_cast<nsIDOMEventTarget*>(
        static_cast<nsDOMEventTargetHelper*>(this)), this);
  }

  nsLayoutStatics::Release();
}

void
nsXMLHttpRequest::RootResultArrayBuffer()
{
  nsContentUtils::PreserveWrapper(
    static_cast<nsIDOMEventTarget*>(
      static_cast<nsDOMEventTargetHelper*>(this)), this);
}

/**
 * This Init method is called from the factory constructor.
 */
nsresult
nsXMLHttpRequest::Init()
{
  // Set the original mScriptContext and mPrincipal, if available.
  // Get JSContext from stack.
  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1");

  if (!stack) {
    return NS_OK;
  }

  JSContext *cx;

  if (NS_FAILED(stack->Peek(&cx)) || !cx) {
    return NS_OK;
  }

  nsIScriptSecurityManager *secMan = nsContentUtils::GetSecurityManager();
  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  if (secMan) {
    nsresult rv = secMan->GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  NS_ENSURE_STATE(subjectPrincipal);
  mPrincipal = subjectPrincipal;

  nsIScriptContext* context = GetScriptContextFromJSContext(cx);
  if (context) {
    mScriptContext = context;
    nsCOMPtr<nsPIDOMWindow> window =
      do_QueryInterface(context->GetGlobalObject());
    if (window) {
      mOwner = window->GetCurrentInnerWindow();
    }
  }

  return NS_OK;
}
/**
 * This Init method should only be called by C++ consumers.
 */
NS_IMETHODIMP
nsXMLHttpRequest::Init(nsIPrincipal* aPrincipal,
                       nsIScriptContext* aScriptContext,
                       nsPIDOMWindow* aOwnerWindow,
                       nsIURI* aBaseURI)
{
  NS_ENSURE_ARG_POINTER(aPrincipal);

  // This object may have already been initialized in the other Init call above
  // if JS was on the stack. Clear the old values for mScriptContext and mOwner
  // if new ones are not supplied here.

  mPrincipal = aPrincipal;
  mScriptContext = aScriptContext;
  if (aOwnerWindow) {
    mOwner = aOwnerWindow->GetCurrentInnerWindow();
  }
  else {
    mOwner = nsnull;
  }
  mBaseURI = aBaseURI;

  return NS_OK;
}

/**
 * This Initialize method is called from XPConnect via nsIJSNativeInitializer.
 */
NS_IMETHODIMP
nsXMLHttpRequest::Initialize(nsISupports* aOwner, JSContext* cx, JSObject* obj,
                             PRUint32 argc, jsval *argv)
{
  mOwner = do_QueryInterface(aOwner);
  if (!mOwner) {
    NS_WARNING("Unexpected nsIJSNativeInitializer owner");
    return NS_OK;
  }

  // This XHR object is bound to a |window|,
  // so re-set principal and script context.
  nsCOMPtr<nsIScriptObjectPrincipal> scriptPrincipal = do_QueryInterface(aOwner);
  NS_ENSURE_STATE(scriptPrincipal);
  mPrincipal = scriptPrincipal->GetPrincipal();
  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aOwner);
  NS_ENSURE_STATE(sgo);
  mScriptContext = sgo->GetContext();
  NS_ENSURE_STATE(mScriptContext);
  return NS_OK; 
}

void
nsXMLHttpRequest::ResetResponse()
{
  mResponseXML = nsnull;
  mResponseBody.Truncate();
  mResponseText.Truncate();
  mResponseBlob = nsnull;
  mResultArrayBuffer = nsnull;
  mResultJSON = JSVAL_VOID;
  mLoadTransferred = 0;
  mResponseBodyDecodedPos = 0;
}

void
nsXMLHttpRequest::SetRequestObserver(nsIRequestObserver* aObserver)
{
  mRequestObserver = aObserver;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXMLHttpRequest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsXMLHttpRequest,
                                                  nsXHREventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mChannel)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mReadRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mResponseXML)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCORSPreflightChannel)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnUploadProgressListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnReadystatechangeListener)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mXMLParserStreamListener)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mChannelEventSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mProgressEventSink)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mUpload,
                                                       nsIXMLHttpRequestUpload)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsXMLHttpRequest,
                                                nsXHREventTarget)
  tmp->mResultArrayBuffer = nsnull;
  tmp->mResultJSON = JSVAL_VOID;
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mChannel)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mReadRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mResponseXML)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCORSPreflightChannel)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnUploadProgressListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnReadystatechangeListener)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mXMLParserStreamListener)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mChannelEventSink)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mProgressEventSink)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mUpload)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(nsXMLHttpRequest,
                                               nsXHREventTarget)
  if(tmp->mResultArrayBuffer) {
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_CALLBACK(tmp->mResultArrayBuffer,
                                               "mResultArrayBuffer")
  }
  if (JSVAL_IS_GCTHING(tmp->mResultJSON)) {
    void *gcThing = JSVAL_TO_GCTHING(tmp->mResultJSON);
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_CALLBACK(gcThing, "mResultJSON")
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

DOMCI_DATA(XMLHttpRequest, nsXMLHttpRequest)

// QueryInterface implementation for nsXMLHttpRequest
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsXMLHttpRequest)
  NS_INTERFACE_MAP_ENTRY(nsIXMLHttpRequest)
  NS_INTERFACE_MAP_ENTRY(nsIJSXMLHttpRequest)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIChannelEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(XMLHttpRequest)
NS_INTERFACE_MAP_END_INHERITING(nsXHREventTarget)

NS_IMPL_ADDREF_INHERITED(nsXMLHttpRequest, nsXHREventTarget)
NS_IMPL_RELEASE_INHERITED(nsXMLHttpRequest, nsXHREventTarget)

NS_IMETHODIMP
nsXMLHttpRequest::GetOnreadystatechange(nsIDOMEventListener * *aOnreadystatechange)
{
  return
    nsXHREventTarget::GetInnerEventListener(mOnReadystatechangeListener,
                                            aOnreadystatechange);
}

NS_IMETHODIMP
nsXMLHttpRequest::SetOnreadystatechange(nsIDOMEventListener * aOnreadystatechange)
{
  return
    nsXHREventTarget::RemoveAddEventListener(NS_LITERAL_STRING(READYSTATE_STR),
                                             mOnReadystatechangeListener,
                                             aOnreadystatechange);
}

NS_IMETHODIMP
nsXMLHttpRequest::GetOnuploadprogress(nsIDOMEventListener * *aOnuploadprogress)
{
  return
    nsXHREventTarget::GetInnerEventListener(mOnUploadProgressListener,
                                            aOnuploadprogress);
}

NS_IMETHODIMP
nsXMLHttpRequest::SetOnuploadprogress(nsIDOMEventListener * aOnuploadprogress)
{
  return
    nsXHREventTarget::RemoveAddEventListener(NS_LITERAL_STRING(UPLOADPROGRESS_STR),
                                             mOnUploadProgressListener,
                                             aOnuploadprogress);
}

/* readonly attribute nsIChannel channel; */
NS_IMETHODIMP
nsXMLHttpRequest::GetChannel(nsIChannel **aChannel)
{
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_IF_ADDREF(*aChannel = mChannel);

  return NS_OK;
}

/* readonly attribute nsIDOMDocument responseXML; */
NS_IMETHODIMP
nsXMLHttpRequest::GetResponseXML(nsIDOMDocument **aResponseXML)
{
  NS_ENSURE_ARG_POINTER(aResponseXML);
  *aResponseXML = nsnull;
  if (mResponseType != XML_HTTP_RESPONSE_TYPE_DEFAULT &&
      mResponseType != XML_HTTP_RESPONSE_TYPE_DOCUMENT) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }
  if ((XML_HTTP_REQUEST_DONE & mState) && mResponseXML) {
    *aResponseXML = mResponseXML;
    NS_ADDREF(*aResponseXML);
  }
  if (mWarnAboutMultipartHtml) {
    mWarnAboutMultipartHtml = false;
    nsContentUtils::ReportToConsole(nsContentUtils::eDOM_PROPERTIES,
                                    "HTMLMultipartXHRWarning",
                                    nsnull,
                                    0,
                                    nsnull, // Response URL not kept around
                                    EmptyString(),
                                    0,
                                    0,
                                    nsIScriptError::warningFlag,
                                    "DOM",
                                    mOwner->WindowID());
  }
  if (mWarnAboutSyncHtml) {
    mWarnAboutSyncHtml = false;
    nsContentUtils::ReportToConsole(nsContentUtils::eDOM_PROPERTIES,
                                    "HTMLSyncXHRWarning",
                                    nsnull,
                                    0,
                                    nsnull, // Response URL not kept around
                                    EmptyString(),
                                    0,
                                    0,
                                    nsIScriptError::warningFlag,
                                    "DOM",
                                    mOwner->WindowID());
  }
  return NS_OK;
}

/*
 * This piece copied from nsXMLDocument, we try to get the charset
 * from HTTP headers.
 */
nsresult
nsXMLHttpRequest::DetectCharset()
{
  mResponseCharset.Truncate();
  mDecoder = nsnull;

  if (mResponseType != XML_HTTP_RESPONSE_TYPE_DEFAULT &&
      mResponseType != XML_HTTP_RESPONSE_TYPE_TEXT &&
      mResponseType != XML_HTTP_RESPONSE_TYPE_JSON &&
      mResponseType != XML_HTTP_RESPONSE_TYPE_CHUNKED_TEXT) {
    return NS_OK;
  }

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(mReadRequest);
  if (!channel) {
    channel = mChannel;
  }

  nsCAutoString charsetVal;
  nsresult rv = channel ? channel->GetContentCharset(charsetVal) :
                NS_ERROR_FAILURE;
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsICharsetAlias> calias =
      do_GetService(NS_CHARSETALIAS_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv) && calias) {
      rv = calias->GetPreferred(charsetVal, mResponseCharset);
    }
  }

  if (NS_FAILED(rv) || mResponseCharset.IsEmpty()) {
    // MS documentation states UTF-8 is default for responseText
    mResponseCharset.AssignLiteral("UTF-8");
  }

  nsCOMPtr<nsICharsetConverterManager> ccm =
    do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return ccm->GetUnicodeDecoderRaw(mResponseCharset.get(),
                                   getter_AddRefs(mDecoder));
}

nsresult
nsXMLHttpRequest::AppendToResponseText(const char * aSrcBuffer,
                                       PRUint32 aSrcBufferLen)
{
  NS_ENSURE_STATE(mDecoder);

  PRInt32 destBufferLen;
  nsresult rv = mDecoder->GetMaxLength(aSrcBuffer, aSrcBufferLen,
                                       &destBufferLen);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mResponseText.SetCapacity(mResponseText.Length() + destBufferLen)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRUnichar* destBuffer = mResponseText.BeginWriting() + mResponseText.Length();

  PRInt32 totalChars = mResponseText.Length();

  // This code here is basically a copy of a similar thing in
  // nsScanner::Append(const char* aBuffer, PRUint32 aLen).
  // If we get illegal characters in the input we replace
  // them and don't just fail.
  do {
    PRInt32 srclen = (PRInt32)aSrcBufferLen;
    PRInt32 destlen = (PRInt32)destBufferLen;
    rv = mDecoder->Convert(aSrcBuffer,
                           &srclen,
                           destBuffer,
                           &destlen);
    if (NS_FAILED(rv)) {
      // We consume one byte, replace it with U+FFFD
      // and try the conversion again.

      destBuffer[destlen] = (PRUnichar)0xFFFD; // add replacement character
      destlen++; // skip written replacement character
      destBuffer += destlen;
      destBufferLen -= destlen;

      if (srclen < (PRInt32)aSrcBufferLen) {
        srclen++; // Consume the invalid character
      }
      aSrcBuffer += srclen;
      aSrcBufferLen -= srclen;

      mDecoder->Reset();
    }

    totalChars += destlen;

  } while (NS_FAILED(rv) && aSrcBufferLen > 0);

  mResponseText.SetLength(totalChars);

  return NS_OK;
}

/* readonly attribute AString responseText; */
NS_IMETHODIMP nsXMLHttpRequest::GetResponseText(nsAString& aResponseText)
{
  aResponseText.Truncate();

  if (mResponseType != XML_HTTP_RESPONSE_TYPE_DEFAULT &&
      mResponseType != XML_HTTP_RESPONSE_TYPE_TEXT &&
      mResponseType != XML_HTTP_RESPONSE_TYPE_CHUNKED_TEXT) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (mResponseType == XML_HTTP_RESPONSE_TYPE_CHUNKED_TEXT &&
      !mInLoadProgressEvent) {
    aResponseText.SetIsVoid(true);
    return NS_OK;
  }

  if (!(mState & (XML_HTTP_REQUEST_DONE | XML_HTTP_REQUEST_LOADING))) {
    return NS_OK;
  }

  // We only decode text lazily if we're also parsing to a doc.
  // Also, if we've decoded all current data already, then no need to decode
  // more.
  if (!mResponseXML ||
      mResponseBodyDecodedPos == mResponseBody.Length()) {
    aResponseText = mResponseText;
    return NS_OK;
  }

  nsresult rv;

  nsCOMPtr<nsIDocument> document = do_QueryInterface(mResponseXML);
  if (mResponseCharset != document->GetDocumentCharacterSet()) {
    mResponseCharset = document->GetDocumentCharacterSet();
    mResponseText.Truncate();
    mResponseBodyDecodedPos = 0;

    nsCOMPtr<nsICharsetConverterManager> ccm =
      do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ccm->GetUnicodeDecoderRaw(mResponseCharset.get(),
                                   getter_AddRefs(mDecoder));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ASSERTION(mResponseBodyDecodedPos < mResponseBody.Length(),
               "Unexpected mResponseBodyDecodedPos");
  rv = AppendToResponseText(mResponseBody.get() + mResponseBodyDecodedPos,
                            mResponseBody.Length() - mResponseBodyDecodedPos);
  NS_ENSURE_SUCCESS(rv, rv);

  mResponseBodyDecodedPos = mResponseBody.Length();
  
  if (mState & XML_HTTP_REQUEST_DONE) {
    // Free memory buffer which we no longer need
    mResponseBody.Truncate();
    mResponseBodyDecodedPos = 0;
  }

  aResponseText = mResponseText;

  return NS_OK;
}

nsresult
nsXMLHttpRequest::CreateResponseParsedJSON(JSContext* aCx)
{
  if (!aCx) {
    return NS_ERROR_FAILURE;
  }

  if (!JS_ParseJSON(aCx,
                    (jschar*)mResponseText.get(),
                    mResponseText.Length(), &mResultJSON)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsXMLHttpRequest::CreateResponseArrayBuffer(JSContext *aCx)
{
  if (!aCx){
    return NS_ERROR_FAILURE;
  }

  PRInt32 dataLen = mResponseBody.Length();
  RootResultArrayBuffer();
  mResultArrayBuffer = js_CreateArrayBuffer(aCx, dataLen);
  if (!mResultArrayBuffer) {
    return NS_ERROR_FAILURE;
  }

  if (dataLen > 0) {
    JSObject *abuf = js::ArrayBuffer::getArrayBuffer(mResultArrayBuffer);
    NS_ASSERTION(abuf, "What happened?");
    memcpy(JS_GetArrayBufferData(abuf), mResponseBody.BeginReading(), dataLen);
  }

  return NS_OK;
}

/* attribute AString responseType; */
NS_IMETHODIMP nsXMLHttpRequest::GetResponseType(nsAString& aResponseType)
{
  switch (mResponseType) {
  case XML_HTTP_RESPONSE_TYPE_DEFAULT:
    aResponseType.Truncate();
    break;
  case XML_HTTP_RESPONSE_TYPE_ARRAYBUFFER:
    aResponseType.AssignLiteral("arraybuffer");
    break;
  case XML_HTTP_RESPONSE_TYPE_BLOB:
    aResponseType.AssignLiteral("blob");
    break;
  case XML_HTTP_RESPONSE_TYPE_DOCUMENT:
    aResponseType.AssignLiteral("document");
    break;
  case XML_HTTP_RESPONSE_TYPE_TEXT:
    aResponseType.AssignLiteral("text");
    break;
  case XML_HTTP_RESPONSE_TYPE_JSON:
    aResponseType.AssignLiteral("moz-json");
    break;
  case XML_HTTP_RESPONSE_TYPE_CHUNKED_TEXT:
    aResponseType.AssignLiteral("moz-chunked-text");
    break;
  case XML_HTTP_RESPONSE_TYPE_CHUNKED_ARRAYBUFFER:
    aResponseType.AssignLiteral("moz-chunked-arraybuffer");
    break;
  default:
    NS_ERROR("Should not happen");
  }

  return NS_OK;
}

/* attribute AString responseType; */
NS_IMETHODIMP nsXMLHttpRequest::SetResponseType(const nsAString& aResponseType)
{
  // If the state is not OPENED or HEADERS_RECEIVED raise an
  // INVALID_STATE_ERR exception and terminate these steps.
  if (!(mState & (XML_HTTP_REQUEST_OPENED | XML_HTTP_REQUEST_SENT |
                  XML_HTTP_REQUEST_HEADERS_RECEIVED)))
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  // Set the responseType attribute's value to the given value.
  if (aResponseType.IsEmpty()) {
    mResponseType = XML_HTTP_RESPONSE_TYPE_DEFAULT;
  } else if (aResponseType.EqualsLiteral("arraybuffer")) {
    mResponseType = XML_HTTP_RESPONSE_TYPE_ARRAYBUFFER;
  } else if (aResponseType.EqualsLiteral("blob")) {
    mResponseType = XML_HTTP_RESPONSE_TYPE_BLOB;
  } else if (aResponseType.EqualsLiteral("document")) {
    mResponseType = XML_HTTP_RESPONSE_TYPE_DOCUMENT;
  } else if (aResponseType.EqualsLiteral("text")) {
    mResponseType = XML_HTTP_RESPONSE_TYPE_TEXT;
  } else if (aResponseType.EqualsLiteral("moz-json")) {
    mResponseType = XML_HTTP_RESPONSE_TYPE_JSON;
  } else if (aResponseType.EqualsLiteral("moz-chunked-text")) {
    if (!(mState & XML_HTTP_REQUEST_ASYNC)) {
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    }
    mResponseType = XML_HTTP_RESPONSE_TYPE_CHUNKED_TEXT;
  } else if (aResponseType.EqualsLiteral("moz-chunked-arraybuffer")) {
    if (!(mState & XML_HTTP_REQUEST_ASYNC)) {
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    }
    mResponseType = XML_HTTP_RESPONSE_TYPE_CHUNKED_ARRAYBUFFER;
  }
  // If the given value is not the empty string, "arraybuffer",
  // "blob", "document", or "text" terminate these steps.

  // If the state is OPENED, SetCacheAsFile would have no effect here
  // because the channel hasn't initialized the cache entry yet.
  // SetCacheAsFile will be called from OnStartRequest.
  // If the state is HEADERS_RECEIVED, however, we need to call
  // it immediately because OnStartRequest is already dispatched.
  if (mState & XML_HTTP_REQUEST_HEADERS_RECEIVED) {
    nsCOMPtr<nsICachingChannel> cc(do_QueryInterface(mChannel));
    if (cc) {
      cc->SetCacheAsFile(mResponseType == XML_HTTP_RESPONSE_TYPE_BLOB);
    }
  }

  return NS_OK;
}

/* readonly attribute jsval response; */
NS_IMETHODIMP nsXMLHttpRequest::GetResponse(JSContext *aCx, jsval *aResult)
{
  nsresult rv = NS_OK;

  switch (mResponseType) {
  case XML_HTTP_RESPONSE_TYPE_DEFAULT:
  case XML_HTTP_RESPONSE_TYPE_TEXT:
  case XML_HTTP_RESPONSE_TYPE_CHUNKED_TEXT:
    {
      nsString str;
      rv = GetResponseText(str);
      if (NS_FAILED(rv)) return rv;
      NS_ENSURE_TRUE(xpc_qsStringToJsval(aCx, str, aResult),
                     NS_ERROR_OUT_OF_MEMORY);
    }
    break;

  case XML_HTTP_RESPONSE_TYPE_ARRAYBUFFER:
  case XML_HTTP_RESPONSE_TYPE_CHUNKED_ARRAYBUFFER:
    if ((mResponseType == XML_HTTP_RESPONSE_TYPE_ARRAYBUFFER &&
         mState & XML_HTTP_REQUEST_DONE) ||
        (mResponseType == XML_HTTP_RESPONSE_TYPE_CHUNKED_ARRAYBUFFER &&
         mInLoadProgressEvent)) {
      if (!mResultArrayBuffer) {
         rv = CreateResponseArrayBuffer(aCx);
         NS_ENSURE_SUCCESS(rv, rv);
      }
      *aResult = OBJECT_TO_JSVAL(mResultArrayBuffer);
    } else {
      *aResult = JSVAL_NULL;
    }
    break;

  case XML_HTTP_RESPONSE_TYPE_BLOB:
    if (mState & XML_HTTP_REQUEST_DONE && mResponseBlob) {
      JSObject* scope = JS_GetGlobalForScopeChain(aCx);
      rv = nsContentUtils::WrapNative(aCx, scope, mResponseBlob, aResult,
                                      nsnull, true);
    } else {
      *aResult = JSVAL_NULL;
    }
    break;

  case XML_HTTP_RESPONSE_TYPE_DOCUMENT:
    if (mState & XML_HTTP_REQUEST_DONE && mResponseXML) {
      JSObject* scope = JS_GetGlobalForScopeChain(aCx);
      rv = nsContentUtils::WrapNative(aCx, scope, mResponseXML, aResult,
                                      nsnull, true);
    } else {
      *aResult = JSVAL_NULL;
    }
    break;

  case XML_HTTP_RESPONSE_TYPE_JSON:
    if (mState & XML_HTTP_REQUEST_DONE) {
      if (mResultJSON == JSVAL_VOID) {
        rv = CreateResponseParsedJSON(aCx);
        NS_ENSURE_SUCCESS(rv, rv);

        mResponseText.Truncate();
      }
      *aResult = mResultJSON;
    } else {
      *aResult = JSVAL_NULL;
    }
    break;

  default:
    NS_ERROR("Should not happen");
  }

  return rv;
}

/* readonly attribute unsigned long status; */
NS_IMETHODIMP
nsXMLHttpRequest::GetStatus(PRUint32 *aStatus)
{
  *aStatus = 0;

  if (mState & XML_HTTP_REQUEST_USE_XSITE_AC) {
    // Make sure we don't leak status information from denied cross-site
    // requests.
    if (mChannel) {
      nsresult status;
      mChannel->GetStatus(&status);
      if (NS_FAILED(status)) {
        return NS_OK;
      }
    }
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = GetCurrentHttpChannel();

  if (httpChannel) {
    nsresult rv = httpChannel->GetResponseStatus(aStatus);
    if (rv == NS_ERROR_NOT_AVAILABLE) {
      // Someone's calling this before we got a response... Check our
      // ReadyState.  If we're at 3 or 4, then this means the connection
      // errored before we got any data; return 0 in that case.
      PRUint16 readyState;
      GetReadyState(&readyState);
      if (readyState >= LOADING) {
        *aStatus = 0;
        return NS_OK;
      }
    }

    return rv;
  }

  return NS_OK;
}

/* readonly attribute AUTF8String statusText; */
NS_IMETHODIMP
nsXMLHttpRequest::GetStatusText(nsACString& aStatusText)
{
  nsCOMPtr<nsIHttpChannel> httpChannel = GetCurrentHttpChannel();

  aStatusText.Truncate();

  if (httpChannel) {
    if (mState & XML_HTTP_REQUEST_USE_XSITE_AC) {
      // Make sure we don't leak status information from denied cross-site
      // requests.
      if (mChannel) {
        nsresult status;
        mChannel->GetStatus(&status);
        if (NS_FAILED(status)) {
          return NS_OK;
        }
      }
    }

    httpChannel->GetResponseStatusText(aStatusText);
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
  if (mCORSPreflightChannel) {
    mCORSPreflightChannel->Cancel(NS_BINDING_ABORTED);
  }
  PRUint32 responseLength = mResponseBody.Length();
  ResetResponse();
  mState |= XML_HTTP_REQUEST_ABORTED;
  
  if (!(mState & (XML_HTTP_REQUEST_UNSENT |
                  XML_HTTP_REQUEST_OPENED |
                  XML_HTTP_REQUEST_DONE))) {
    ChangeState(XML_HTTP_REQUEST_DONE, true);

    if (!(mState & XML_HTTP_REQUEST_SYNCLOOPING)) {
      NS_NAMED_LITERAL_STRING(abortStr, ABORT_STR);
      DispatchProgressEvent(this, abortStr, mLoadLengthComputable, responseLength,
                            mLoadTotal);
      if (mUpload && !mUploadComplete) {
        mUploadComplete = true;
        DispatchProgressEvent(mUpload, abortStr, true, mUploadTransferred,
                              mUploadTotal);
      }
    }
  }

  // The ChangeState call above calls onreadystatechange handlers which
  // if they load a new url will cause nsXMLHttpRequest::Open to clear
  // the abort state bit. If this occurs we're not uninitialized (bug 361773).
  if (mState & XML_HTTP_REQUEST_ABORTED) {
    ChangeState(XML_HTTP_REQUEST_UNSENT, false);  // IE seems to do it
  }

  mState &= ~XML_HTTP_REQUEST_SYNCLOOPING;

  return NS_OK;
}

/* string getAllResponseHeaders (); */
NS_IMETHODIMP
nsXMLHttpRequest::GetAllResponseHeaders(char **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  if (mState & XML_HTTP_REQUEST_USE_XSITE_AC) {
    *_retval = ToNewCString(EmptyString());
    return NS_OK;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = GetCurrentHttpChannel();

  if (httpChannel) {
    nsRefPtr<nsHeaderVisitor> visitor = new nsHeaderVisitor();
    nsresult rv = httpChannel->VisitResponseHeaders(visitor);
    if (NS_SUCCEEDED(rv))
      *_retval = ToNewCString(visitor->Headers());
  }
 
  if (!*_retval) {
    *_retval = ToNewCString(EmptyString());
  }

  return NS_OK;
}

/* ACString getResponseHeader (in AUTF8String header); */
NS_IMETHODIMP
nsXMLHttpRequest::GetResponseHeader(const nsACString& header,
                                    nsACString& _retval)
{
  nsresult rv = NS_OK;
  _retval.SetIsVoid(true);

  nsCOMPtr<nsIHttpChannel> httpChannel = GetCurrentHttpChannel();

  if (!httpChannel) {
    return NS_OK;
  }

  // See bug #380418. Hide "Set-Cookie" headers from non-chrome scripts.
  bool chrome = false; // default to false in case IsCapabilityEnabled fails
  IsCapabilityEnabled("UniversalXPConnect", &chrome);
  if (!chrome &&
       (header.LowerCaseEqualsASCII("set-cookie") ||
        header.LowerCaseEqualsASCII("set-cookie2"))) {
    NS_WARNING("blocked access to response header");
    return NS_OK;
  }

  // Check for dangerous headers
  if (mState & XML_HTTP_REQUEST_USE_XSITE_AC) {
    // Make sure we don't leak header information from denied cross-site
    // requests.
    if (mChannel) {
      nsresult status;
      mChannel->GetStatus(&status);
      if (NS_FAILED(status)) {
        return NS_OK;
      }
    }

    const char *kCrossOriginSafeHeaders[] = {
      "cache-control", "content-language", "content-type", "expires",
      "last-modified", "pragma"
    };
    bool safeHeader = false;
    PRUint32 i;
    for (i = 0; i < ArrayLength(kCrossOriginSafeHeaders); ++i) {
      if (header.LowerCaseEqualsASCII(kCrossOriginSafeHeaders[i])) {
        safeHeader = true;
        break;
      }
    }

    if (!safeHeader) {
      nsCAutoString headerVal;
      // The "Access-Control-Expose-Headers" header contains a comma separated
      // list of method names.
      httpChannel->
        GetResponseHeader(NS_LITERAL_CSTRING("Access-Control-Expose-Headers"),
                          headerVal);
      nsCCharSeparatedTokenizer exposeTokens(headerVal, ',');
      while(exposeTokens.hasMoreTokens()) {
        const nsDependentCSubstring& token = exposeTokens.nextToken();
        if (token.IsEmpty()) {
          continue;
        }
        if (!IsValidHTTPToken(token)) {
          return NS_OK;
        }
        if (header.Equals(token, nsCaseInsensitiveCStringComparator())) {
          safeHeader = true;
        }
      }
    }

    if (!safeHeader) {
      return NS_OK;
    }
  }

  rv = httpChannel->GetResponseHeader(header, _retval);
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    // Means no header
    _retval.SetIsVoid(true);
    rv = NS_OK;
  }

  return rv;
}

already_AddRefed<nsILoadGroup>
nsXMLHttpRequest::GetLoadGroup() const
{
  if (mState & XML_HTTP_REQUEST_BACKGROUND) {
    return nsnull;
  }

  nsCOMPtr<nsIDocument> doc =
    nsContentUtils::GetDocumentFromScriptContext(mScriptContext);
  if (doc) {
    return doc->GetDocumentLoadGroup();
  }

  return nsnull;
}

nsresult
nsXMLHttpRequest::CreateReadystatechangeEvent(nsIDOMEvent** aDOMEvent)
{
  nsresult rv = nsEventDispatcher::CreateEvent(nsnull, nsnull,
                                               NS_LITERAL_STRING("Events"),
                                               aDOMEvent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIPrivateDOMEvent> privevent(do_QueryInterface(*aDOMEvent));
  if (!privevent) {
    NS_IF_RELEASE(*aDOMEvent);
    return NS_ERROR_FAILURE;
  }

  (*aDOMEvent)->InitEvent(NS_LITERAL_STRING(READYSTATE_STR),
                          false, false);

  // We assume anyone who managed to call CreateReadystatechangeEvent is trusted
  privevent->SetTrusted(true);

  return NS_OK;
}

void
nsXMLHttpRequest::DispatchProgressEvent(nsDOMEventTargetHelper* aTarget,
                                        const nsAString& aType,
                                        bool aUseLSEventWrapper,
                                        bool aLengthComputable,
                                        PRUint64 aLoaded, PRUint64 aTotal,
                                        PRUint64 aPosition, PRUint64 aTotalSize)
{
  NS_ASSERTION(aTarget, "null target");
  NS_ASSERTION(!aType.IsEmpty(), "missing event type");

  if (NS_FAILED(CheckInnerWindowCorrectness()) ||
      (!AllowUploadProgress() &&
       (aTarget == mUpload || aType.EqualsLiteral(UPLOADPROGRESS_STR)))) {
    return;
  }

  bool dispatchLoadend = aType.EqualsLiteral(LOAD_STR) ||
                           aType.EqualsLiteral(ERROR_STR) ||
                           aType.EqualsLiteral(ABORT_STR);
  
  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = nsEventDispatcher::CreateEvent(nsnull, nsnull,
                                               NS_LITERAL_STRING("ProgressEvent"),
                                               getter_AddRefs(event));
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIPrivateDOMEvent> privevent(do_QueryInterface(event));
  if (!privevent) {
    return;
  }
  privevent->SetTrusted(true);

  nsCOMPtr<nsIDOMProgressEvent> progress = do_QueryInterface(event);
  if (!progress) {
    return;
  }

  progress->InitProgressEvent(aType, false, false, aLengthComputable,
                              aLoaded, (aTotal == LL_MAXUINT) ? 0 : aTotal);

  if (aUseLSEventWrapper) {
    nsCOMPtr<nsIDOMProgressEvent> xhrprogressEvent =
      new nsXMLHttpProgressEvent(progress, aPosition, aTotalSize, mOwner);
    event = xhrprogressEvent;
  }
  aTarget->DispatchDOMEvent(nsnull, event, nsnull, nsnull);
  
  if (dispatchLoadend) {
    DispatchProgressEvent(aTarget, NS_LITERAL_STRING(LOADEND_STR),
                          aUseLSEventWrapper, aLengthComputable,
                          aLoaded, aTotal, aPosition, aTotalSize);
  }
}
                                          
already_AddRefed<nsIHttpChannel>
nsXMLHttpRequest::GetCurrentHttpChannel()
{
  nsIHttpChannel *httpChannel = nsnull;

  if (mReadRequest) {
    CallQueryInterface(mReadRequest, &httpChannel);
  }

  if (!httpChannel && mChannel) {
    CallQueryInterface(mChannel, &httpChannel);
  }

  return httpChannel;
}

bool
nsXMLHttpRequest::IsSystemXHR()
{
  return !!nsContentUtils::IsSystemPrincipal(mPrincipal);
}

nsresult
nsXMLHttpRequest::CheckChannelForCrossSiteRequest(nsIChannel* aChannel)
{
  // First check if cross-site requests are enabled...
  if (IsSystemXHR()) {
    return NS_OK;
  }

  // ...or if this is a same-origin request.
  if (nsContentUtils::CheckMayLoad(mPrincipal, aChannel)) {
    return NS_OK;
  }

  // This is a cross-site request
  mState |= XML_HTTP_REQUEST_USE_XSITE_AC;

  // Check if we need to do a preflight request.
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  NS_ENSURE_TRUE(httpChannel, NS_ERROR_DOM_BAD_URI);
    
  nsCAutoString method;
  httpChannel->GetRequestMethod(method);
  if (!mCORSUnsafeHeaders.IsEmpty() ||
      HasListenersFor(NS_LITERAL_STRING(UPLOADPROGRESS_STR)) ||
      (mUpload && mUpload->HasListeners()) ||
      (!method.LowerCaseEqualsLiteral("get") &&
       !method.LowerCaseEqualsLiteral("post") &&
       !method.LowerCaseEqualsLiteral("head"))) {
    mState |= XML_HTTP_REQUEST_NEED_AC_PREFLIGHT;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXMLHttpRequest::Open(const nsACString& method, const nsACString& url,
                       bool async, const nsAString& user,
                       const nsAString& password, PRUint8 optional_argc)
{
  NS_ENSURE_ARG(!method.IsEmpty());

  if (!optional_argc) {
    // No optional arguments were passed in. Default async to true.
    async = true;
  }
  Telemetry::Accumulate(Telemetry::XMLHTTPREQUEST_ASYNC_OR_SYNC,
                        async ? 0 : 1);

  NS_ENSURE_TRUE(mPrincipal, NS_ERROR_NOT_INITIALIZED);

  // Disallow HTTP/1.1 TRACE method (see bug 302489)
  // and MS IIS equivalent TRACK (see bug 381264)
  if (method.LowerCaseEqualsLiteral("trace") ||
      method.LowerCaseEqualsLiteral("track")) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  nsCOMPtr<nsIURI> uri;
  bool authp = false;

  if (mState & (XML_HTTP_REQUEST_OPENED |
                XML_HTTP_REQUEST_HEADERS_RECEIVED |
                XML_HTTP_REQUEST_LOADING |
                XML_HTTP_REQUEST_SENT |
                XML_HTTP_REQUEST_STOPPED)) {
    // IE aborts as well
    Abort();

    // XXX We should probably send a warning to the JS console
    //     that load was aborted and event listeners were cleared
    //     since this looks like a situation that could happen
    //     by accident and you could spend a lot of time wondering
    //     why things didn't work.
  }

  if (mState & XML_HTTP_REQUEST_ABORTED) {
    // Something caused this request to abort (e.g the current request
    // was caceled, channels closed etc), most likely the abort()
    // function was called by script. Unset our aborted state, and
    // proceed as normal

    mState &= ~XML_HTTP_REQUEST_ABORTED;
  }

  if (async) {
    mState |= XML_HTTP_REQUEST_ASYNC;
  } else {
    mState &= ~XML_HTTP_REQUEST_ASYNC;
  }

  mState &= ~XML_HTTP_REQUEST_MPART_HEADERS;

  nsCOMPtr<nsIDocument> doc =
    nsContentUtils::GetDocumentFromScriptContext(mScriptContext);
  
  nsCOMPtr<nsIURI> baseURI;
  if (mBaseURI) {
    baseURI = mBaseURI;
  }
  else if (doc) {
    baseURI = doc->GetBaseURI();
  }

  rv = NS_NewURI(getter_AddRefs(uri), url, nsnull, baseURI);
  if (NS_FAILED(rv)) return rv;

  // mScriptContext should be initialized because of GetBaseURI() above.
  // Still need to consider the case that doc is nsnull however.
  rv = CheckInnerWindowCorrectness();
  NS_ENSURE_SUCCESS(rv, rv);
  PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_XMLHTTPREQUEST,
                                 uri,
                                 mPrincipal,
                                 doc,
                                 EmptyCString(), //mime guess
                                 nsnull,         //extra
                                 &shouldLoad,
                                 nsContentUtils::GetContentPolicy(),
                                 nsContentUtils::GetSecurityManager());
  if (NS_FAILED(rv)) return rv;
  if (NS_CP_REJECTED(shouldLoad)) {
    // Disallowed by content policy
    return NS_ERROR_CONTENT_BLOCKED;
  }

  if (!user.IsEmpty()) {
    nsCAutoString userpass;
    CopyUTF16toUTF8(user, userpass);
    if (!password.IsEmpty()) {
      userpass.Append(':');
      AppendUTF16toUTF8(password, userpass);
    }
    uri->SetUserPass(userpass);
    authp = true;
  }

  // When we are called from JS we can find the load group for the page,
  // and add ourselves to it. This way any pending requests
  // will be automatically aborted if the user leaves the page.
  nsCOMPtr<nsILoadGroup> loadGroup = GetLoadGroup();

  // get Content Security Policy from principal to pass into channel
  nsCOMPtr<nsIChannelPolicy> channelPolicy;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = mPrincipal->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_SUCCESS(rv, rv);
  if (csp) {
    channelPolicy = do_CreateInstance("@mozilla.org/nschannelpolicy;1");
    channelPolicy->SetContentSecurityPolicy(csp);
    channelPolicy->SetLoadType(nsIContentPolicy::TYPE_XMLHTTPREQUEST);
  }
  rv = NS_NewChannel(getter_AddRefs(mChannel),
                     uri,
                     nsnull,                    // ioService
                     loadGroup,
                     nsnull,                    // callbacks
                     nsIRequest::LOAD_BACKGROUND,
                     channelPolicy);
  if (NS_FAILED(rv)) return rv;

  mState &= ~(XML_HTTP_REQUEST_USE_XSITE_AC |
              XML_HTTP_REQUEST_NEED_AC_PREFLIGHT);

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));
  if (httpChannel) {
    rv = httpChannel->SetRequestMethod(method);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  ChangeState(XML_HTTP_REQUEST_OPENED);

  return rv;
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
  nsXMLHttpRequest* xmlHttpRequest = static_cast<nsXMLHttpRequest*>(closure);
  if (!xmlHttpRequest || !writeCount) {
    NS_WARNING("XMLHttpRequest cannot read from stream: no closure or writeCount");
    return NS_ERROR_FAILURE;
  }

  if (xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_BLOB &&
      xmlHttpRequest->mResponseBlob) {
    *writeCount = count;
    return NS_OK;
  }

  if ((xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_DEFAULT &&
       xmlHttpRequest->mResponseXML) ||
      xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_ARRAYBUFFER ||
      xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_BLOB ||
      xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_CHUNKED_ARRAYBUFFER) {
    // Copy for our own use
    PRUint32 previousLength = xmlHttpRequest->mResponseBody.Length();
    xmlHttpRequest->mResponseBody.Append(fromRawSegment,count);
    if (count > 0 && xmlHttpRequest->mResponseBody.Length() == previousLength) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else if (xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_DEFAULT ||
             xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_TEXT ||
             xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_JSON ||
             xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_CHUNKED_TEXT) {
    NS_ASSERTION(!xmlHttpRequest->mResponseXML,
                 "We shouldn't be parsing a doc here");
    xmlHttpRequest->AppendToResponseText(fromRawSegment, count);
  }

  nsresult rv = NS_OK;

  if (xmlHttpRequest->mState & XML_HTTP_REQUEST_PARSEBODY) {
    // Give the same data to the parser.

    // We need to wrap the data in a new lightweight stream and pass that
    // to the parser, because calling ReadSegments() recursively on the same
    // stream is not supported.
    nsCOMPtr<nsIInputStream> copyStream;
    rv = NS_NewByteInputStream(getter_AddRefs(copyStream), fromRawSegment, count);

    if (NS_SUCCEEDED(rv) && xmlHttpRequest->mXMLParserStreamListener) {
      NS_ASSERTION(copyStream, "NS_NewByteInputStream lied");
      nsresult parsingResult = xmlHttpRequest->mXMLParserStreamListener
                                  ->OnDataAvailable(xmlHttpRequest->mReadRequest,
                                                    xmlHttpRequest->mContext,
                                                    copyStream, toOffset, count);

      // No use to continue parsing if we failed here, but we
      // should still finish reading the stream
      if (NS_FAILED(parsingResult)) {
        xmlHttpRequest->mState &= ~XML_HTTP_REQUEST_PARSEBODY;
      }
    }
  }

  if (NS_SUCCEEDED(rv)) {
    *writeCount = count;
  } else {
    *writeCount = 0;
  }

  return rv;
}

bool nsXMLHttpRequest::CreateResponseBlob(nsIRequest *request)
{
  nsCOMPtr<nsIFile> file;
  nsCOMPtr<nsICachingChannel> cc(do_QueryInterface(request));
  if (cc) {
    cc->GetCacheFile(getter_AddRefs(file));
  } else {
    nsCOMPtr<nsIFileChannel> fc = do_QueryInterface(request);
    if (fc) {
      fc->GetFile(getter_AddRefs(file));
    }
  }
  bool fromFile = false;
  if (file) {
    nsCAutoString contentType;
    mChannel->GetContentType(contentType);
    nsCOMPtr<nsISupports> cacheToken;
    if (cc) {
      cc->GetCacheToken(getter_AddRefs(cacheToken));
      // We need to call IsFromCache to determine whether the response is
      // fully cached (i.e. whether we can skip reading the response).
      cc->IsFromCache(&fromFile);
    } else {
      // If the response is coming from the local resource, we can skip
      // reading the response unconditionally.
      fromFile = true;
    }

    mResponseBlob =
      new nsDOMFileFile(file, NS_ConvertASCIItoUTF16(contentType), cacheToken);
    mResponseBody.Truncate();
  }
  return fromFile;
}

NS_IMETHODIMP
nsXMLHttpRequest::OnDataAvailable(nsIRequest *request,
                                  nsISupports *ctxt,
                                  nsIInputStream *inStr,
                                  PRUint32 sourceOffset,
                                  PRUint32 count)
{
  NS_ENSURE_ARG_POINTER(inStr);

  NS_ABORT_IF_FALSE(mContext.get() == ctxt,"start context different from OnDataAvailable context");

  mProgressSinceLastProgressEvent = true;

  bool cancelable = false;
  if (mResponseType == XML_HTTP_RESPONSE_TYPE_BLOB && !mResponseBlob) {
    cancelable = CreateResponseBlob(request);
    // The nsIStreamListener contract mandates us
    // to read from the stream before returning.
  }

  PRUint32 totalRead;
  nsresult rv = inStr->ReadSegments(nsXMLHttpRequest::StreamReaderFunc,
                                    (void*)this, count, &totalRead);
  NS_ENSURE_SUCCESS(rv, rv);

  if (cancelable) {
    // We don't have to read from the local file for the blob response
    mResponseBlob->GetSize(&mLoadTransferred);
    ChangeState(XML_HTTP_REQUEST_LOADING);
    return request->Cancel(NS_OK);
  }

  mLoadTransferred += totalRead;

  ChangeState(XML_HTTP_REQUEST_LOADING);
  
  MaybeDispatchProgressEvents(false);

  return NS_OK;
}

bool
IsSameOrBaseChannel(nsIRequest* aPossibleBase, nsIChannel* aChannel)
{
  nsCOMPtr<nsIMultiPartChannel> mpChannel = do_QueryInterface(aPossibleBase);
  if (mpChannel) {
    nsCOMPtr<nsIChannel> baseChannel;
    nsresult rv = mpChannel->GetBaseChannel(getter_AddRefs(baseChannel));
    NS_ENSURE_SUCCESS(rv, false);
    
    return baseChannel == aChannel;
  }

  return aPossibleBase == aChannel;
}

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP
nsXMLHttpRequest::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  nsresult rv = NS_OK;
  if (!mFirstStartRequestSeen && mRequestObserver) {
    mFirstStartRequestSeen = true;
    mRequestObserver->OnStartRequest(request, ctxt);
  }

  if (!IsSameOrBaseChannel(request, mChannel)) {
    return NS_OK;
  }

  // Don't do anything if we have been aborted
  if (mState & XML_HTTP_REQUEST_UNSENT)
    return NS_OK;

  if (mState & XML_HTTP_REQUEST_ABORTED) {
    NS_ERROR("Ugh, still getting data on an aborted XMLHttpRequest!");

    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIChannel> channel(do_QueryInterface(request));
  NS_ENSURE_TRUE(channel, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIPrincipal> documentPrincipal;
  if (IsSystemXHR()) {
    // Don't give this document the system principal.  We need to keep track of
    // mPrincipal being system because we use it for various security checks
    // that should be passing, but the document data shouldn't get a system
    // principal.
    nsresult rv;
    documentPrincipal = do_CreateInstance("@mozilla.org/nullprincipal;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    documentPrincipal = mPrincipal;
  }

  channel->SetOwner(documentPrincipal);

  nsresult status;
  request->GetStatus(&status);
  mErrorLoad = mErrorLoad || NS_FAILED(status);

  if (mUpload && !mUploadComplete && !mErrorLoad &&
      (mState & XML_HTTP_REQUEST_ASYNC)) {
    if (mTimerIsActive) {
      mTimerIsActive = false;
      mProgressNotifier->Cancel();
    }
    MaybeDispatchProgressEvents(true);
    mUploadComplete = true;
    DispatchProgressEvent(mUpload, NS_LITERAL_STRING(LOAD_STR),
                          true, mUploadTotal, mUploadTotal);
  }

  mReadRequest = request;
  mContext = ctxt;
  mState |= XML_HTTP_REQUEST_PARSEBODY;
  mState &= ~XML_HTTP_REQUEST_MPART_HEADERS;
  ChangeState(XML_HTTP_REQUEST_HEADERS_RECEIVED);

  if (mResponseType == XML_HTTP_RESPONSE_TYPE_BLOB) {
    nsCOMPtr<nsICachingChannel> cc(do_QueryInterface(mChannel));
    if (cc) {
      cc->SetCacheAsFile(true);
    }
  }

  ResetResponse();

  if (!mOverrideMimeType.IsEmpty()) {
    channel->SetContentType(mOverrideMimeType);
  }

  DetectCharset();

  // Set up responseXML
  bool parseBody = mResponseType == XML_HTTP_RESPONSE_TYPE_DEFAULT ||
                     mResponseType == XML_HTTP_RESPONSE_TYPE_DOCUMENT;
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));
  if (parseBody && httpChannel) {
    nsCAutoString method;
    httpChannel->GetRequestMethod(method);
    parseBody = !method.EqualsLiteral("HEAD");
  }

  mIsHtml = false;
  mWarnAboutMultipartHtml = false;
  mWarnAboutSyncHtml = false;
  if (parseBody && NS_SUCCEEDED(status)) {
    // We can gain a huge performance win by not even trying to
    // parse non-XML data. This also protects us from the situation
    // where we have an XML document and sink, but HTML (or other)
    // parser, which can produce unreliable results.
    nsCAutoString type;
    channel->GetContentType(type);

    if ((mResponseType == XML_HTTP_RESPONSE_TYPE_DOCUMENT) &&
        type.EqualsLiteral("text/html")) {
      // HTML parsing is only supported for responseType == "document" to
      // avoid running the parser and, worse, populating responseXML for
      // legacy users of XHR who use responseType == "" for retrieving the
      // responseText of text/html resources. This legacy case is so common
      // that it's not useful to emit a warning about it.
      if (!(mState & XML_HTTP_REQUEST_ASYNC)) {
        // We don't make cool new features available in the bad synchronous
        // mode. The synchronous mode is for legacy only.
        mWarnAboutSyncHtml = true;
        mState &= ~XML_HTTP_REQUEST_PARSEBODY;
      } else if (mState & XML_HTTP_REQUEST_MULTIPART) {
        // HTML parsing is supported only for non-multipart responses. The
        // multipart implementation assumes that it's OK to start the next part
        // immediately after the last part. That doesn't work with the HTML
        // parser, because when OnStopRequest for one part has fired, the
        // parser thread still hasn't posted back the runnables that make the
        // parsing appear finished.
        //
        // On the other hand, multipart support seems to be a legacy feature,
        // so it isn't clear that use cases justify adding support for deferring
        // the multipart stream events between parts to accommodate the
        // asynchronous nature of the HTML parser.
        mWarnAboutMultipartHtml = true;
        mState &= ~XML_HTTP_REQUEST_PARSEBODY;
      } else {
        mIsHtml = true;
      }
    } else if (type.Find("xml") == kNotFound) {
      mState &= ~XML_HTTP_REQUEST_PARSEBODY;
    }
  } else {
    // The request failed, so we shouldn't be parsing anyway
    mState &= ~XML_HTTP_REQUEST_PARSEBODY;
  }

  if (mState & XML_HTTP_REQUEST_PARSEBODY) {
    nsCOMPtr<nsIURI> baseURI, docURI;
    nsCOMPtr<nsIDocument> doc =
      nsContentUtils::GetDocumentFromScriptContext(mScriptContext);

    if (doc) {
      docURI = doc->GetDocumentURI();
      baseURI = doc->GetBaseURI();
    }

    // Create an empty document from it.  Here we have to cheat a little bit...
    // Setting the base URI to |baseURI| won't work if the document has a null
    // principal, so use mPrincipal when creating the document, then reset the
    // principal.
    const nsAString& emptyStr = EmptyString();
    nsCOMPtr<nsIScriptGlobalObject> global = do_QueryInterface(mOwner);
    rv = nsContentUtils::CreateDocument(emptyStr, emptyStr, nsnull, docURI,
                                        baseURI, mPrincipal, global,
                                        mIsHtml ? DocumentFlavorHTML :
                                                  DocumentFlavorLegacyGuess,
                                        getter_AddRefs(mResponseXML));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIDocument> responseDoc = do_QueryInterface(mResponseXML);
    responseDoc->SetPrincipal(documentPrincipal);

    if (IsSystemXHR()) {
      responseDoc->ForceEnableXULXBL();
    }

    if (mState & XML_HTTP_REQUEST_USE_XSITE_AC) {
      nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(mResponseXML);
      if (htmlDoc) {
        htmlDoc->DisableCookieAccess();
      }
    }

    nsCOMPtr<nsIStreamListener> listener;
    nsCOMPtr<nsILoadGroup> loadGroup;
    channel->GetLoadGroup(getter_AddRefs(loadGroup));

    rv = responseDoc->StartDocumentLoad(kLoadAsData, channel, loadGroup,
                                        nsnull, getter_AddRefs(listener),
                                        !(mState & XML_HTTP_REQUEST_USE_XSITE_AC));
    NS_ENSURE_SUCCESS(rv, rv);

    mXMLParserStreamListener = listener;
    rv = mXMLParserStreamListener->OnStartRequest(request, ctxt);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // We won't get any progress events anyway if we didn't have progress
  // events when starting the request - so maybe no need to start timer here.
  if (NS_SUCCEEDED(rv) &&
      (mState & XML_HTTP_REQUEST_ASYNC) &&
      HasListenersFor(NS_LITERAL_STRING(PROGRESS_STR))) {
    StartProgressEventTimer();
  }

  return NS_OK;
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP
nsXMLHttpRequest::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult status)
{
  if (!IsSameOrBaseChannel(request, mChannel)) {
    return NS_OK;
  }

  nsresult rv = NS_OK;

  // If we're loading a multipart stream of XML documents, we'll get
  // an OnStopRequest() for the last part in the stream, and then
  // another one for the end of the initiating
  // "multipart/x-mixed-replace" stream too. So we must check that we
  // still have an xml parser stream listener before accessing it
  // here.
  nsCOMPtr<nsIMultiPartChannel> mpChannel = do_QueryInterface(request);
  if (mpChannel) {
    bool last;
    rv = mpChannel->GetIsLastPart(&last);
    NS_ENSURE_SUCCESS(rv, rv);
    if (last) {
      mState |= XML_HTTP_REQUEST_GOT_FINAL_STOP;
    }
  }
  else {
    mState |= XML_HTTP_REQUEST_GOT_FINAL_STOP;
  }

  if (mRequestObserver && mState & XML_HTTP_REQUEST_GOT_FINAL_STOP) {
    NS_ASSERTION(mFirstStartRequestSeen, "Inconsistent state!");
    mFirstStartRequestSeen = false;
    mRequestObserver->OnStopRequest(request, ctxt, status);
  }

  // make sure to notify the listener if we were aborted
  // XXX in fact, why don't we do the cleanup below in this case??
  if (mState & XML_HTTP_REQUEST_UNSENT) {
    if (mXMLParserStreamListener)
      (void) mXMLParserStreamListener->OnStopRequest(request, ctxt, status);
    return NS_OK;
  }

  // Is this good enough here?
  if (mState & XML_HTTP_REQUEST_PARSEBODY && mXMLParserStreamListener) {
    mXMLParserStreamListener->OnStopRequest(request, ctxt, status);
  }

  mXMLParserStreamListener = nsnull;
  mReadRequest = nsnull;
  mContext = nsnull;

  // If we're received data since the last progress event, make sure to fire
  // an event for it, except in the HTML case, defer the last progress event
  // until the parser is done.
  if (!mIsHtml) {
    MaybeDispatchProgressEvents(true);
  }

  nsCOMPtr<nsIChannel> channel(do_QueryInterface(request));
  NS_ENSURE_TRUE(channel, NS_ERROR_UNEXPECTED);

  if (NS_SUCCEEDED(status) && mResponseType == XML_HTTP_RESPONSE_TYPE_BLOB) {
    if (!mResponseBlob) {
      CreateResponseBlob(request);
    }
    if (!mResponseBlob) {
      // Smaller files may be written in cache map instead of separate files.
      // Also, no-store response cannot be written in persistent cache.
      nsCAutoString contentType;
      mChannel->GetContentType(contentType);
      // XXX We should change mResponseBody to be a raw malloc'ed buffer
      //     to avoid copying the data.
      PRUint32 blobLen = mResponseBody.Length();
      void *blobData = PR_Malloc(blobLen);
      if (blobData) {
        memcpy(blobData, mResponseBody.BeginReading(), blobLen);

        mResponseBlob =
          new nsDOMMemoryFile(blobData, blobLen,
                              NS_ConvertASCIItoUTF16(contentType));
        mResponseBody.Truncate();
      }
      NS_ASSERTION(mResponseText.IsEmpty(), "mResponseText should be empty");
    }
  }

  channel->SetNotificationCallbacks(nsnull);
  mNotificationCallbacks = nsnull;
  mChannelEventSink = nsnull;
  mProgressEventSink = nsnull;

  mState &= ~XML_HTTP_REQUEST_SYNCLOOPING;

  if (NS_FAILED(status)) {
    // This can happen if the server is unreachable. Other possible
    // reasons are that the user leaves the page or hits the ESC key.

    mErrorLoad = true;
    mResponseXML = nsnull;
  }

  // If we're uninitialized at this point, we encountered an error
  // earlier and listeners have already been notified. Also we do
  // not want to do this if we already completed.
  if (mState & (XML_HTTP_REQUEST_UNSENT |
                XML_HTTP_REQUEST_DONE)) {
    return NS_OK;
  }

  if (!mResponseXML) {
    ChangeStateToDone();
    return NS_OK;
  }
  if (mIsHtml) {
    NS_ASSERTION(!(mState & XML_HTTP_REQUEST_SYNCLOOPING),
      "We weren't supposed to support HTML parsing with XHR!");
    nsCOMPtr<nsIDOMEventTarget> eventTarget = do_QueryInterface(mResponseXML);
    nsEventListenerManager* manager = eventTarget->GetListenerManager(true);
    manager->AddEventListenerByType(new nsXHRParseEndListener(this),
                                    NS_LITERAL_STRING("DOMContentLoaded"),
                                    NS_EVENT_FLAG_BUBBLE |
                                    NS_EVENT_FLAG_SYSTEM_EVENT);
    return NS_OK;
  }
  // We might have been sent non-XML data. If that was the case,
  // we should null out the document member. The idea in this
  // check here is that if there is no document element it is not
  // an XML document. We might need a fancier check...
  nsCOMPtr<nsIDOMElement> root;
  mResponseXML->GetDocumentElement(getter_AddRefs(root));
  if (!root) {
    mResponseXML = nsnull;
  }
  ChangeStateToDone();
  return NS_OK;
}

void
nsXMLHttpRequest::ChangeStateToDone()
{
  if (mIsHtml) {
    // In the HTML case, this has to be deferred, because the parser doesn't
    // do it's job synchronously.
    MaybeDispatchProgressEvents(true);
  }
  ChangeState(XML_HTTP_REQUEST_DONE, true);

  NS_NAMED_LITERAL_STRING(errorStr, ERROR_STR);
  NS_NAMED_LITERAL_STRING(loadStr, LOAD_STR);
  DispatchProgressEvent(this,
                        mErrorLoad ? errorStr : loadStr,
                        !mErrorLoad,
                        mLoadTransferred,
                        mErrorLoad ? 0 : mLoadTransferred);
  if (mErrorLoad && mUpload && !mUploadComplete) {
    DispatchProgressEvent(mUpload, errorStr, true,
                          mUploadTransferred, mUploadTotal);
  }

  if (mErrorLoad) {
    // By nulling out channel here we make it so that Send() can test
    // for that and throw. Also calling the various status
    // methods/members will not throw.
    // This matches what IE does.
    mChannel = nsnull;
  }
  else if (!(mState & XML_HTTP_REQUEST_GOT_FINAL_STOP)) {
    // We're a multipart request, so we're not done. Reset to opened.
    ChangeState(XML_HTTP_REQUEST_OPENED);
  }
}

NS_IMETHODIMP
nsXMLHttpRequest::SendAsBinary(const nsAString &aBody)
{
  char *data = static_cast<char*>(NS_Alloc(aBody.Length() + 1));
  if (!data)
    return NS_ERROR_OUT_OF_MEMORY;

  nsAString::const_iterator iter, end;
  aBody.BeginReading(iter);
  aBody.EndReading(end);
  char *p = data;
  while (iter != end) {
    if (*iter & 0xFF00) {
      NS_Free(data);
      return NS_ERROR_DOM_INVALID_CHARACTER_ERR;
    }
    *p++ = static_cast<char>(*iter++);
  }
  *p = '\0';

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream), data,
                                      aBody.Length(), NS_ASSIGNMENT_ADOPT);
  if (NS_FAILED(rv))
    NS_Free(data);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIWritableVariant> variant = new nsVariant();
  if (!variant) return NS_ERROR_OUT_OF_MEMORY;

  rv = variant->SetAsISupports(stream);
  NS_ENSURE_SUCCESS(rv, rv);

  return Send(variant);
}

static nsresult
GetRequestBody(nsIVariant* aBody, nsIInputStream** aResult,
               nsACString& aContentType, nsACString& aCharset)
{
  *aResult = nsnull;
  aContentType.AssignLiteral("text/plain");
  aCharset.AssignLiteral("UTF-8");

  PRUint16 dataType;
  nsresult rv = aBody->GetDataType(&dataType);
  NS_ENSURE_SUCCESS(rv, rv);

  if (dataType == nsIDataType::VTYPE_INTERFACE ||
      dataType == nsIDataType::VTYPE_INTERFACE_IS) {
    nsCOMPtr<nsISupports> supports;
    nsID *iid;
    rv = aBody->GetAsInterface(&iid, getter_AddRefs(supports));
    NS_ENSURE_SUCCESS(rv, rv);

    nsMemory::Free(iid);

    // document?
    nsCOMPtr<nsIDOMDocument> doc = do_QueryInterface(supports);
    if (doc) {
      aContentType.AssignLiteral("application/xml");
      nsAutoString inputEncoding;
      doc->GetInputEncoding(inputEncoding);
      if (!DOMStringIsNull(inputEncoding)) {
        CopyUTF16toUTF8(inputEncoding, aCharset);
      }

      // Serialize to a stream so that the encoding used will
      // match the document's.
      nsCOMPtr<nsIDOMSerializer> serializer =
        do_CreateInstance(NS_XMLSERIALIZER_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIStorageStream> storStream;
      rv = NS_NewStorageStream(4096, PR_UINT32_MAX,
                               getter_AddRefs(storStream));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIOutputStream> output;
      rv = storStream->GetOutputStream(0, getter_AddRefs(output));
      NS_ENSURE_SUCCESS(rv, rv);

      // Make sure to use the encoding we'll send
      rv = serializer->SerializeToStream(doc, output, aCharset);
      NS_ENSURE_SUCCESS(rv, rv);

      output->Close();

      return storStream->NewInputStream(0, aResult);
    }

    // nsISupportsString?
    nsCOMPtr<nsISupportsString> wstr = do_QueryInterface(supports);
    if (wstr) {
      nsAutoString string;
      wstr->GetData(string);

      return NS_NewCStringInputStream(aResult,
                                      NS_ConvertUTF16toUTF8(string));
    }

    // nsIInputStream?
    nsCOMPtr<nsIInputStream> stream = do_QueryInterface(supports);
    if (stream) {
      stream.forget(aResult);
      aCharset.Truncate();

      return NS_OK;
    }

    // nsIXHRSendable?
    nsCOMPtr<nsIXHRSendable> sendable = do_QueryInterface(supports);
    if (sendable) {
      return sendable->GetSendInfo(aResult, aContentType, aCharset);
    }

    // ArrayBuffer?
    jsval realVal;
    JSObject* obj;
    nsresult rv = aBody->GetAsJSVal(&realVal);
    if (NS_SUCCEEDED(rv) && !JSVAL_IS_PRIMITIVE(realVal) &&
        (obj = JSVAL_TO_OBJECT(realVal)) &&
        (js_IsArrayBuffer(obj))) {

      aContentType.SetIsVoid(true);
      PRInt32 abLength = JS_GetArrayBufferByteLength(obj);
      char* data = (char*)JS_GetArrayBufferData(obj);

      nsCOMPtr<nsIInputStream> stream;
      nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream), data,
                                          abLength, NS_ASSIGNMENT_COPY);
      NS_ENSURE_SUCCESS(rv, rv);
      stream.forget(aResult);
      aCharset.Truncate();

      return NS_OK;
    }
  }
  else if (dataType == nsIDataType::VTYPE_VOID ||
           dataType == nsIDataType::VTYPE_EMPTY) {
    // Makes us act as if !aBody, don't upload anything
    return NS_OK;
  }

  PRUnichar* data = nsnull;
  PRUint32 len = 0;
  rv = aBody->GetAsWStringWithSize(&len, &data);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString string;
  string.Adopt(data, len);

  return NS_NewCStringInputStream(aResult, NS_ConvertUTF16toUTF8(string));
}

/* void send (in nsIVariant aBody); */
NS_IMETHODIMP
nsXMLHttpRequest::Send(nsIVariant *aBody)
{
  NS_ENSURE_TRUE(mPrincipal, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = CheckInnerWindowCorrectness();
  NS_ENSURE_SUCCESS(rv, rv);

  // Return error if we're already processing a request
  if (XML_HTTP_REQUEST_SENT & mState) {
    return NS_ERROR_FAILURE;
  }

  // Make sure we've been opened
  if (!mChannel || !(XML_HTTP_REQUEST_OPENED & mState)) {
    return NS_ERROR_NOT_INITIALIZED;
  }


  // nsIRequest::LOAD_BACKGROUND prevents throbber from becoming active, which
  // in turn keeps STOP button from becoming active.  If the consumer passed in
  // a progress event handler we must load with nsIRequest::LOAD_NORMAL or
  // necko won't generate any progress notifications.
  if (HasListenersFor(NS_LITERAL_STRING(PROGRESS_STR)) ||
      HasListenersFor(NS_LITERAL_STRING(UPLOADPROGRESS_STR)) ||
      (mUpload && mUpload->HasListenersFor(NS_LITERAL_STRING(PROGRESS_STR)))) {
    nsLoadFlags loadFlags;
    mChannel->GetLoadFlags(&loadFlags);
    loadFlags &= ~nsIRequest::LOAD_BACKGROUND;
    loadFlags |= nsIRequest::LOAD_NORMAL;
    mChannel->SetLoadFlags(loadFlags);
  }

  // XXX We should probably send a warning to the JS console
  //     if there are no event listeners set and we are doing
  //     an asynchronous call.

  // Ignore argument if method is GET, there is no point in trying to
  // upload anything
  nsCAutoString method;
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));

  if (httpChannel) {
    httpChannel->GetRequestMethod(method); // If GET, method name will be uppercase

    if (!IsSystemXHR()) {
      // Get the referrer for the request.
      //
      // If it weren't for history.push/replaceState, we could just use the
      // principal's URI here.  But since we want changes to the URI effected
      // by push/replaceState to be reflected in the XHR referrer, we have to
      // be more clever.
      //
      // If the document's original URI (before any push/replaceStates) matches
      // our principal, then we use the document's current URI (after
      // push/replaceStates).  Otherwise (if the document is, say, a data:
      // URI), we just use the principal's URI.

      nsCOMPtr<nsIURI> principalURI;
      mPrincipal->GetURI(getter_AddRefs(principalURI));

      nsCOMPtr<nsIDocument> doc =
        nsContentUtils::GetDocumentFromScriptContext(mScriptContext);

      nsCOMPtr<nsIURI> docCurURI;
      nsCOMPtr<nsIURI> docOrigURI;
      if (doc) {
        docCurURI = doc->GetDocumentURI();
        docOrigURI = doc->GetOriginalURI();
      }

      nsCOMPtr<nsIURI> referrerURI;

      if (principalURI && docCurURI && docOrigURI) {
        bool equal = false;
        principalURI->Equals(docOrigURI, &equal);
        if (equal) {
          referrerURI = docCurURI;
        }
      }

      if (!referrerURI)
        referrerURI = principalURI;

      httpChannel->SetReferrer(referrerURI);
    }

    // Some extensions override the http protocol handler and provide their own
    // implementation. The channels returned from that implementation doesn't
    // seem to always implement the nsIUploadChannel2 interface, presumably
    // because it's a new interface.
    // Eventually we should remove this and simply require that http channels
    // implement the new interface.
    // See bug 529041
    nsCOMPtr<nsIUploadChannel2> uploadChannel2 =
      do_QueryInterface(httpChannel);
    if (!uploadChannel2) {
      nsCOMPtr<nsIConsoleService> consoleService =
        do_GetService(NS_CONSOLESERVICE_CONTRACTID);
      if (consoleService) {
        consoleService->LogStringMessage(NS_LITERAL_STRING(
          "Http channel implementation doesn't support nsIUploadChannel2. An extension has supplied a non-functional http protocol handler. This will break behavior and in future releases not work at all."
                                                           ).get());
      }
    }
  }

  mUploadTransferred = 0;
  mUploadTotal = 0;
  // By default we don't have any upload, so mark upload complete.
  mUploadComplete = true;
  mErrorLoad = false;
  mLoadLengthComputable = false;
  mLoadTotal = 0;
  mUploadProgress = 0;
  mUploadProgressMax = 0;
  if (aBody && httpChannel && !method.EqualsLiteral("GET")) {

    nsCAutoString charset;
    nsCAutoString defaultContentType;
    nsCOMPtr<nsIInputStream> postDataStream;

    rv = GetRequestBody(aBody, getter_AddRefs(postDataStream),
                        defaultContentType, charset);
    NS_ENSURE_SUCCESS(rv, rv);

    if (postDataStream) {
      // If no content type header was set by the client, we set it to
      // application/xml.
      nsCAutoString contentType;
      if (NS_FAILED(httpChannel->
                      GetRequestHeader(NS_LITERAL_CSTRING("Content-Type"),
                                       contentType)) ||
          contentType.IsEmpty()) {
        contentType = defaultContentType;
      }

      // We don't want to set a charset for streams.
      if (!charset.IsEmpty()) {
        nsCAutoString specifiedCharset;
        bool haveCharset;
        PRInt32 charsetStart, charsetEnd;
        rv = NS_ExtractCharsetFromContentType(contentType, specifiedCharset,
                                              &haveCharset, &charsetStart,
                                              &charsetEnd);
        if (NS_SUCCEEDED(rv)) {
          // special case: the extracted charset is quoted with single quotes
          // -- for the purpose of preserving what was set we want to handle
          // them as delimiters (although they aren't really)
          if (specifiedCharset.Length() >= 2 &&
              specifiedCharset.First() == '\'' &&
              specifiedCharset.Last() == '\'') {
            specifiedCharset = Substring(specifiedCharset, 1,
                                         specifiedCharset.Length() - 2);
          }

          // If the content-type the page set already has a charset parameter,
          // and it's the same charset, up to case, as |charset|, just send the
          // page-set content-type header.  Apparently at least
          // google-web-toolkit is broken and relies on the exact case of its
          // charset parameter, which makes things break if we use |charset|
          // (which is always a fully resolved charset per our charset alias
          // table, hence might be differently cased).
          if (!specifiedCharset.Equals(charset,
                                       nsCaseInsensitiveCStringComparator())) {
            nsCAutoString newCharset("; charset=");
            newCharset.Append(charset);
            contentType.Replace(charsetStart, charsetEnd - charsetStart,
                                newCharset);
          }
        }
      }

      // If necessary, wrap the stream in a buffered stream so as to guarantee
      // support for our upload when calling ExplicitSetUploadStream.
      if (!NS_InputStreamIsBuffered(postDataStream)) {
        nsCOMPtr<nsIInputStream> bufferedStream;
        rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream),
                                       postDataStream, 
                                       4096);
        NS_ENSURE_SUCCESS(rv, rv);

        postDataStream = bufferedStream;
      }

      mUploadComplete = false;
      PRUint32 uploadTotal = 0;
      postDataStream->Available(&uploadTotal);
      mUploadTotal = uploadTotal;

      // We want to use a newer version of the upload channel that won't
      // ignore the necessary headers for an empty Content-Type.
      nsCOMPtr<nsIUploadChannel2> uploadChannel2(do_QueryInterface(httpChannel));
      // This assertion will fire if buggy extensions are installed
      NS_ASSERTION(uploadChannel2, "http must support nsIUploadChannel2");
      if (uploadChannel2) {
          uploadChannel2->ExplicitSetUploadStream(postDataStream, contentType,
                                                 -1, method, false);
      }
      else {
        // http channel doesn't support the new nsIUploadChannel2. Emulate
        // as best we can using nsIUploadChannel
        if (contentType.IsEmpty()) {
          contentType.AssignLiteral("application/octet-stream");
        }
        nsCOMPtr<nsIUploadChannel> uploadChannel =
          do_QueryInterface(httpChannel);
        uploadChannel->SetUploadStream(postDataStream, contentType, -1);
        // Reset the method to its original value
        httpChannel->SetRequestMethod(method);
      }
    }
  }

  if (httpChannel) {
    nsCAutoString contentTypeHeader;
    rv = httpChannel->GetRequestHeader(NS_LITERAL_CSTRING("Content-Type"),
                                       contentTypeHeader);
    if (NS_SUCCEEDED(rv)) {
      nsCAutoString contentType, charset;
      rv = NS_ParseContentType(contentTypeHeader, contentType, charset);
      NS_ENSURE_SUCCESS(rv, rv);
  
      if (!contentType.LowerCaseEqualsLiteral("text/plain") &&
          !contentType.LowerCaseEqualsLiteral("application/x-www-form-urlencoded") &&
          !contentType.LowerCaseEqualsLiteral("multipart/form-data")) {
        mCORSUnsafeHeaders.AppendElement(NS_LITERAL_CSTRING("Content-Type"));
      }
    }
  }

  ResetResponse();

  rv = CheckChannelForCrossSiteRequest(mChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  bool withCredentials = !!(mState & XML_HTTP_REQUEST_AC_WITH_CREDENTIALS);

  // Hook us up to listen to redirects and the like
  mChannel->GetNotificationCallbacks(getter_AddRefs(mNotificationCallbacks));
  mChannel->SetNotificationCallbacks(this);

  // Create our listener
  nsCOMPtr<nsIStreamListener> listener = this;
  if (mState & XML_HTTP_REQUEST_MULTIPART) {
    listener = new nsMultipartProxyListener(listener);
    if (!listener) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (!IsSystemXHR()) {
    // Always create a nsCORSListenerProxy here even if it's
    // a same-origin request right now, since it could be redirected.
    listener = new nsCORSListenerProxy(listener, mPrincipal, mChannel,
                                       withCredentials, &rv);
    NS_ENSURE_TRUE(listener, NS_ERROR_OUT_OF_MEMORY);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Bypass the network cache in cases where it makes no sense:
  // 1) Multipart responses are very large and would likely be doomed by the
  //    cache once they grow too large, so they are not worth caching.
  // 2) POST responses are always unique, and we provide no API that would
  //    allow our consumers to specify a "cache key" to access old POST
  //    responses, so they are not worth caching.
  if ((mState & XML_HTTP_REQUEST_MULTIPART) || method.EqualsLiteral("POST")) {
    AddLoadFlags(mChannel,
        nsIRequest::LOAD_BYPASS_CACHE | nsIRequest::INHIBIT_CACHING);
  }
  // When we are sync loading, we need to bypass the local cache when it would
  // otherwise block us waiting for exclusive access to the cache.  If we don't
  // do this, then we could dead lock in some cases (see bug 309424).
  else if (!(mState & XML_HTTP_REQUEST_ASYNC)) {
    AddLoadFlags(mChannel,
        nsICachingChannel::LOAD_BYPASS_LOCAL_CACHE_IF_BUSY);
  }

  // Since we expect XML data, set the type hint accordingly
  // This means that we always try to parse local files as XML
  // ignoring return value, as this is not critical
  mChannel->SetContentType(NS_LITERAL_CSTRING("application/xml"));

  // Set up the preflight if needed
  if (mState & XML_HTTP_REQUEST_NEED_AC_PREFLIGHT) {
    // Check to see if this initial OPTIONS request has already been cached
    // in our special Access Control Cache.

    rv = NS_StartCORSPreflight(mChannel, listener,
                               mPrincipal, withCredentials,
                               mCORSUnsafeHeaders,
                               getter_AddRefs(mCORSPreflightChannel));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // Start reading from the channel
    rv = mChannel->AsyncOpen(listener, nsnull);
  }

  if (NS_FAILED(rv)) {
    // Drop our ref to the channel to avoid cycles
    mChannel = nsnull;
    mCORSPreflightChannel = nsnull;
    return rv;
  }

  // If we're synchronous, spin an event loop here and wait
  if (!(mState & XML_HTTP_REQUEST_ASYNC)) {
    mState |= XML_HTTP_REQUEST_SYNCLOOPING;

    nsCOMPtr<nsIDocument> suspendedDoc;
    nsCOMPtr<nsIRunnable> resumeTimeoutRunnable;
    if (mOwner) {
      nsCOMPtr<nsIDOMWindow> topWindow;
      if (NS_SUCCEEDED(mOwner->GetTop(getter_AddRefs(topWindow)))) {
        nsCOMPtr<nsPIDOMWindow> suspendedWindow(do_QueryInterface(topWindow));
        if (suspendedWindow &&
            (suspendedWindow = suspendedWindow->GetCurrentInnerWindow())) {
          suspendedDoc = do_QueryInterface(suspendedWindow->GetExtantDocument());
          if (suspendedDoc) {
            suspendedDoc->SuppressEventHandling();
          }
          suspendedWindow->SuspendTimeouts(1, false);
          resumeTimeoutRunnable = new nsResumeTimeoutsEvent(suspendedWindow);
        }
      }
    }

    ChangeState(XML_HTTP_REQUEST_SENT);
    // Note, calling ChangeState may have cleared
    // XML_HTTP_REQUEST_SYNCLOOPING flag.
    nsIThread *thread = NS_GetCurrentThread();
    while (mState & XML_HTTP_REQUEST_SYNCLOOPING) {
      if (!NS_ProcessNextEvent(thread)) {
        rv = NS_ERROR_UNEXPECTED;
        break;
      }
    }

    if (suspendedDoc) {
      suspendedDoc->UnsuppressEventHandlingAndFireEvents(true);
    }

    if (resumeTimeoutRunnable) {
      NS_DispatchToCurrentThread(resumeTimeoutRunnable);
    }
  } else {
    // Now that we've successfully opened the channel, we can change state.  Note
    // that this needs to come after the AsyncOpen() and rv check, because this
    // can run script that would try to restart this request, and that could end
    // up doing our AsyncOpen on a null channel if the reentered AsyncOpen fails.
    ChangeState(XML_HTTP_REQUEST_SENT);
    if (!mUploadComplete &&
        HasListenersFor(NS_LITERAL_STRING(UPLOADPROGRESS_STR)) ||
        (mUpload && mUpload->HasListenersFor(NS_LITERAL_STRING(PROGRESS_STR)))) {
      StartProgressEventTimer();
    }
    DispatchProgressEvent(this, NS_LITERAL_STRING(LOADSTART_STR), false,
                          0, 0);
    if (mUpload && !mUploadComplete) {
      DispatchProgressEvent(mUpload, NS_LITERAL_STRING(LOADSTART_STR), true,
                            0, mUploadTotal);
    }
  }

  if (!mChannel) {
    return NS_ERROR_FAILURE;
  }

  return rv;
}

/* void setRequestHeader (in AUTF8String header, in AUTF8String value); */
NS_IMETHODIMP
nsXMLHttpRequest::SetRequestHeader(const nsACString& header,
                                   const nsACString& value)
{
  nsresult rv;

  // Make sure we don't store an invalid header name in mCORSUnsafeHeaders
  if (!IsValidHTTPToken(header)) {
    return NS_ERROR_FAILURE;
  }

  // Check that we haven't already opened the channel. We can't rely on
  // the channel throwing from mChannel->SetRequestHeader since we might
  // still be waiting for mCORSPreflightChannel to actually open mChannel
  if (mCORSPreflightChannel) {
    bool pending;
    rv = mCORSPreflightChannel->IsPending(&pending);
    NS_ENSURE_SUCCESS(rv, rv);
    
    if (pending) {
      return NS_ERROR_IN_PROGRESS;
    }
  }

  if (!(mState & XML_HTTP_REQUEST_OPENED))
    return NS_ERROR_IN_PROGRESS;

  if (!mChannel)             // open() initializes mChannel, and open()
    return NS_ERROR_FAILURE; // must be called before first setRequestHeader()

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));
  if (!httpChannel) {
    return NS_OK;
  }

  // Prevent modification to certain HTTP headers (see bug 302263), unless
  // the executing script has UniversalBrowserWrite permission.

  bool privileged;
  rv = IsCapabilityEnabled("UniversalBrowserWrite", &privileged);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  if (!privileged) {
    // Check for dangerous headers
    const char *kInvalidHeaders[] = {
      "accept-charset", "accept-encoding", "access-control-request-headers",
      "access-control-request-method", "connection", "content-length",
      "cookie", "cookie2", "content-transfer-encoding", "date", "expect",
      "host", "keep-alive", "origin", "referer", "te", "trailer",
      "transfer-encoding", "upgrade", "user-agent", "via"
    };
    PRUint32 i;
    for (i = 0; i < ArrayLength(kInvalidHeaders); ++i) {
      if (header.LowerCaseEqualsASCII(kInvalidHeaders[i])) {
        NS_WARNING("refusing to set request header");
        return NS_OK;
      }
    }
    if (StringBeginsWith(header, NS_LITERAL_CSTRING("proxy-"),
                         nsCaseInsensitiveCStringComparator()) ||
        StringBeginsWith(header, NS_LITERAL_CSTRING("sec-"),
                         nsCaseInsensitiveCStringComparator())) {
      NS_WARNING("refusing to set request header");
      return NS_OK;
    }

    // Check for dangerous cross-site headers
    bool safeHeader = IsSystemXHR();
    if (!safeHeader) {
      // Content-Type isn't always safe, but we'll deal with it in Send()
      const char *kCrossOriginSafeHeaders[] = {
        "accept", "accept-language", "content-language", "content-type",
        "last-event-id"
      };
      for (i = 0; i < ArrayLength(kCrossOriginSafeHeaders); ++i) {
        if (header.LowerCaseEqualsASCII(kCrossOriginSafeHeaders[i])) {
          safeHeader = true;
          break;
        }
      }
    }

    if (!safeHeader) {
      mCORSUnsafeHeaders.AppendElement(header);
    }
  }

  // We need to set, not add to, the header.
  rv = httpChannel->SetRequestHeader(header, value, false);
  if (NS_SUCCEEDED(rv)) {
    // We'll want to duplicate this header for any replacement channels (eg. on redirect)
    RequestHeader reqHeader = {
      nsCString(header), nsCString(value)
    };
    mModifiedRequestHeaders.AppendElement(reqHeader);
  }

  return rv;
}

/* readonly attribute long readyState; */
NS_IMETHODIMP
nsXMLHttpRequest::GetReadyState(PRUint16 *aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  // Translate some of our internal states for external consumers
  if (mState & XML_HTTP_REQUEST_UNSENT) {
    *aState = UNSENT;
  } else  if (mState & (XML_HTTP_REQUEST_OPENED | XML_HTTP_REQUEST_SENT)) {
    *aState = OPENED;
  } else if (mState & XML_HTTP_REQUEST_HEADERS_RECEIVED) {
    *aState = HEADERS_RECEIVED;
  } else if (mState & (XML_HTTP_REQUEST_LOADING | XML_HTTP_REQUEST_STOPPED)) {
    *aState = LOADING;
  } else if (mState & XML_HTTP_REQUEST_DONE) {
    *aState = DONE;
  } else {
    NS_ERROR("Should not happen");
  }

  return NS_OK;
}

/* void   overrideMimeType(in AUTF8String mimetype); */
NS_IMETHODIMP
nsXMLHttpRequest::OverrideMimeType(const nsACString& aMimeType)
{
  // XXX Should we do some validation here?
  mOverrideMimeType.Assign(aMimeType);
  return NS_OK;
}


/* attribute boolean multipart; */
NS_IMETHODIMP
nsXMLHttpRequest::GetMultipart(bool *_retval)
{
  *_retval = !!(mState & XML_HTTP_REQUEST_MULTIPART);

  return NS_OK;
}

/* attribute boolean multipart; */
NS_IMETHODIMP
nsXMLHttpRequest::SetMultipart(bool aMultipart)
{
  if (!(mState & XML_HTTP_REQUEST_UNSENT)) {
    // Can't change this while we're in the middle of something.
    return NS_ERROR_IN_PROGRESS;
  }

  if (aMultipart) {
    mState |= XML_HTTP_REQUEST_MULTIPART;
  } else {
    mState &= ~XML_HTTP_REQUEST_MULTIPART;
  }

  return NS_OK;
}

/* attribute boolean mozBackgroundRequest; */
NS_IMETHODIMP
nsXMLHttpRequest::GetMozBackgroundRequest(bool *_retval)
{
  *_retval = !!(mState & XML_HTTP_REQUEST_BACKGROUND);

  return NS_OK;
}

/* attribute boolean mozBackgroundRequest; */
NS_IMETHODIMP
nsXMLHttpRequest::SetMozBackgroundRequest(bool aMozBackgroundRequest)
{
  bool privileged;

  nsresult rv = IsCapabilityEnabled("UniversalXPConnect", &privileged);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!privileged)
    return NS_ERROR_DOM_SECURITY_ERR;

  if (!(mState & XML_HTTP_REQUEST_UNSENT)) {
    // Can't change this while we're in the middle of something.
    return NS_ERROR_IN_PROGRESS;
  }

  if (aMozBackgroundRequest) {
    mState |= XML_HTTP_REQUEST_BACKGROUND;
  } else {
    mState &= ~XML_HTTP_REQUEST_BACKGROUND;
  }

  return NS_OK;
}

/* attribute boolean withCredentials; */
NS_IMETHODIMP
nsXMLHttpRequest::GetWithCredentials(bool *_retval)
{
  *_retval = !!(mState & XML_HTTP_REQUEST_AC_WITH_CREDENTIALS);

  return NS_OK;
}

/* attribute boolean withCredentials; */
NS_IMETHODIMP
nsXMLHttpRequest::SetWithCredentials(bool aWithCredentials)
{
  // Return error if we're already processing a request
  if (XML_HTTP_REQUEST_SENT & mState) {
    return NS_ERROR_FAILURE;
  }
  
  if (aWithCredentials) {
    mState |= XML_HTTP_REQUEST_AC_WITH_CREDENTIALS;
  }
  else {
    mState &= ~XML_HTTP_REQUEST_AC_WITH_CREDENTIALS;
  }
  return NS_OK;
}

nsresult
nsXMLHttpRequest::ChangeState(PRUint32 aState, bool aBroadcast)
{
  // If we are setting one of the mutually exclusive states,
  // unset those state bits first.
  if (aState & XML_HTTP_REQUEST_LOADSTATES) {
    mState &= ~XML_HTTP_REQUEST_LOADSTATES;
  }
  mState |= aState;
  nsresult rv = NS_OK;

  if (mProgressNotifier &&
      !(aState & (XML_HTTP_REQUEST_HEADERS_RECEIVED | XML_HTTP_REQUEST_LOADING))) {
    mTimerIsActive = false;
    mProgressNotifier->Cancel();
  }

  if ((aState & XML_HTTP_REQUEST_LOADSTATES) && // Broadcast load states only
      aBroadcast &&
      (mState & XML_HTTP_REQUEST_ASYNC ||
       aState & XML_HTTP_REQUEST_OPENED ||
       aState & XML_HTTP_REQUEST_DONE)) {
    nsCOMPtr<nsIDOMEvent> event;
    rv = CreateReadystatechangeEvent(getter_AddRefs(event));
    NS_ENSURE_SUCCESS(rv, rv);

    DispatchDOMEvent(nsnull, event, nsnull, nsnull);
  }

  return rv;
}

/*
 * Simple helper class that just forwards the redirect callback back
 * to the nsXMLHttpRequest.
 */
class AsyncVerifyRedirectCallbackForwarder : public nsIAsyncVerifyRedirectCallback
{
public:
  AsyncVerifyRedirectCallbackForwarder(nsXMLHttpRequest *xhr)
    : mXHR(xhr)
  {
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(AsyncVerifyRedirectCallbackForwarder)

  // nsIAsyncVerifyRedirectCallback implementation
  NS_IMETHOD OnRedirectVerifyCallback(nsresult result)
  {
    mXHR->OnRedirectVerifyCallback(result);

    return NS_OK;
  }

private:
  nsRefPtr<nsXMLHttpRequest> mXHR;
};

NS_IMPL_CYCLE_COLLECTION_CLASS(AsyncVerifyRedirectCallbackForwarder)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AsyncVerifyRedirectCallbackForwarder)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mXHR, nsIXMLHttpRequest)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AsyncVerifyRedirectCallbackForwarder)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mXHR)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AsyncVerifyRedirectCallbackForwarder)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncVerifyRedirectCallback)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(AsyncVerifyRedirectCallbackForwarder)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AsyncVerifyRedirectCallbackForwarder)


/////////////////////////////////////////////////////
// nsIChannelEventSink methods:
//
NS_IMETHODIMP
nsXMLHttpRequest::AsyncOnChannelRedirect(nsIChannel *aOldChannel,
                                         nsIChannel *aNewChannel,
                                         PRUint32    aFlags,
                                         nsIAsyncVerifyRedirectCallback *callback)
{
  NS_PRECONDITION(aNewChannel, "Redirect without a channel?");

  nsresult rv;

  if (!NS_IsInternalSameURIRedirect(aOldChannel, aNewChannel, aFlags)) {
    rv = CheckChannelForCrossSiteRequest(aNewChannel);
    if (NS_FAILED(rv)) {
      NS_WARNING("nsXMLHttpRequest::OnChannelRedirect: "
                 "CheckChannelForCrossSiteRequest returned failure");
      return rv;
    }

    // Disable redirects for preflighted cross-site requests entirely for now
    // Note, do this after the call to CheckChannelForCrossSiteRequest
    // to make sure that XML_HTTP_REQUEST_USE_XSITE_AC is up-to-date
    if ((mState & XML_HTTP_REQUEST_NEED_AC_PREFLIGHT)) {
       return NS_ERROR_DOM_BAD_URI;
    }
  }

  // Prepare to receive callback
  mRedirectCallback = callback;
  mNewRedirectChannel = aNewChannel;

  if (mChannelEventSink) {
    nsRefPtr<AsyncVerifyRedirectCallbackForwarder> fwd =
      new AsyncVerifyRedirectCallbackForwarder(this);

    rv = mChannelEventSink->AsyncOnChannelRedirect(aOldChannel,
                                                   aNewChannel,
                                                   aFlags, fwd);
    if (NS_FAILED(rv)) {
        mRedirectCallback = nsnull;
        mNewRedirectChannel = nsnull;
    }
    return rv;
  }
  OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

void
nsXMLHttpRequest::OnRedirectVerifyCallback(nsresult result)
{
  NS_ASSERTION(mRedirectCallback, "mRedirectCallback not set in callback");
  NS_ASSERTION(mNewRedirectChannel, "mNewRedirectChannel not set in callback");

  if (NS_SUCCEEDED(result)) {
    mChannel = mNewRedirectChannel;

    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));
    if (httpChannel) {
      // Ensure all original headers are duplicated for the new channel (bug #553888)
      for (PRUint32 i = mModifiedRequestHeaders.Length(); i > 0; ) {
        --i;
        httpChannel->SetRequestHeader(mModifiedRequestHeaders[i].header,
                                      mModifiedRequestHeaders[i].value,
                                      false);
      }
    }
  } else {
    mErrorLoad = true;
  }

  mNewRedirectChannel = nsnull;

  mRedirectCallback->OnRedirectVerifyCallback(result);
  mRedirectCallback = nsnull;
}

/////////////////////////////////////////////////////
// nsIProgressEventSink methods:
//

void
nsXMLHttpRequest::MaybeDispatchProgressEvents(bool aFinalProgress)
{
  if (aFinalProgress && mTimerIsActive) {
    mTimerIsActive = false;
    mProgressNotifier->Cancel();
  }

  if (mTimerIsActive ||
      !mProgressSinceLastProgressEvent ||
      mErrorLoad ||
      !(mState & XML_HTTP_REQUEST_ASYNC)) {
    return;
  }

  if (!aFinalProgress) {
    StartProgressEventTimer();
  }

  // We're uploading if our state is XML_HTTP_REQUEST_OPENED or
  // XML_HTTP_REQUEST_SENT
  if ((XML_HTTP_REQUEST_OPENED | XML_HTTP_REQUEST_SENT) & mState) {
    if (aFinalProgress) {
      mUploadTotal = mUploadTransferred;
      mUploadProgressMax = mUploadProgress;
      mUploadLengthComputable = true;
    }
    DispatchProgressEvent(this, NS_LITERAL_STRING(UPLOADPROGRESS_STR),
                          true, mUploadLengthComputable, mUploadTransferred,
                          mUploadTotal, mUploadProgress,
                          mUploadProgressMax);
    if (mUpload && !mUploadComplete) {
      DispatchProgressEvent(mUpload, NS_LITERAL_STRING(PROGRESS_STR),
                            true, mUploadLengthComputable, mUploadTransferred,
                            mUploadTotal, mUploadProgress,
                            mUploadProgressMax);
    }
  } else {
    if (aFinalProgress) {
      mLoadTotal = mLoadTransferred;
      mLoadLengthComputable = true;
    }
    mInLoadProgressEvent = true;
    DispatchProgressEvent(this, NS_LITERAL_STRING(PROGRESS_STR),
                          true, mLoadLengthComputable, mLoadTransferred,
                          mLoadTotal, mLoadTransferred, mLoadTotal);
    mInLoadProgressEvent = false;
    if (mResponseType == XML_HTTP_RESPONSE_TYPE_CHUNKED_TEXT ||
        mResponseType == XML_HTTP_RESPONSE_TYPE_CHUNKED_ARRAYBUFFER) {
      mResponseBody.Truncate();
      mResponseText.Truncate();
      mResultArrayBuffer = nsnull;
    }
  }

  mProgressSinceLastProgressEvent = false;
}

NS_IMETHODIMP
nsXMLHttpRequest::OnProgress(nsIRequest *aRequest, nsISupports *aContext, PRUint64 aProgress, PRUint64 aProgressMax)
{
  // We're in middle of processing multipart headers and we don't want to report
  // any progress because upload's 'load' is dispatched when we start to load
  // the first response.
  if (XML_HTTP_REQUEST_MPART_HEADERS & mState) {
    return NS_OK;
  }

  // We're uploading if our state is XML_HTTP_REQUEST_OPENED or
  // XML_HTTP_REQUEST_SENT
  bool upload = !!((XML_HTTP_REQUEST_OPENED | XML_HTTP_REQUEST_SENT) & mState);
  // When uploading, OnProgress reports also headers in aProgress and aProgressMax.
  // So, try to remove the headers, if possible.
  bool lengthComputable = (aProgressMax != LL_MAXUINT);
  if (upload) {
    PRUint64 loaded = aProgress;
    PRUint64 total = aProgressMax;
    if (lengthComputable) {
      PRUint64 headerSize = aProgressMax - mUploadTotal;
      loaded -= headerSize;
      total -= headerSize;
    }
    mUploadLengthComputable = lengthComputable;
    mUploadTransferred = loaded;
    mUploadProgress = aProgress;
    mUploadProgressMax = aProgressMax;
    mProgressSinceLastProgressEvent = true;

    MaybeDispatchProgressEvents(false);
  } else {
    mLoadLengthComputable = lengthComputable;
    mLoadTotal = lengthComputable ? aProgressMax : 0;
    
    // Don't dispatch progress events here. OnDataAvailable will take care
    // of that.
  }

  if (mProgressEventSink) {
    mProgressEventSink->OnProgress(aRequest, aContext, aProgress,
                                   aProgressMax);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXMLHttpRequest::OnStatus(nsIRequest *aRequest, nsISupports *aContext, nsresult aStatus, const PRUnichar *aStatusArg)
{
  if (mProgressEventSink) {
    mProgressEventSink->OnStatus(aRequest, aContext, aStatus, aStatusArg);
  }

  return NS_OK;
}

bool
nsXMLHttpRequest::AllowUploadProgress()
{
  return !(mState & XML_HTTP_REQUEST_USE_XSITE_AC) ||
    (mState & XML_HTTP_REQUEST_NEED_AC_PREFLIGHT);
}

/////////////////////////////////////////////////////
// nsIInterfaceRequestor methods:
//
NS_IMETHODIMP
nsXMLHttpRequest::GetInterface(const nsIID & aIID, void **aResult)
{
  nsresult rv;

  // Make sure to return ourselves for the channel event sink interface and
  // progress event sink interface, no matter what.  We can forward these to
  // mNotificationCallbacks if it wants to get notifications for them.  But we
  // need to see these notifications for proper functioning.
  if (aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
    mChannelEventSink = do_GetInterface(mNotificationCallbacks);
    *aResult = static_cast<nsIChannelEventSink*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsIProgressEventSink))) {
    mProgressEventSink = do_GetInterface(mNotificationCallbacks);
    *aResult = static_cast<nsIProgressEventSink*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  // Now give mNotificationCallbacks (if non-null) a chance to return the
  // desired interface.
  if (mNotificationCallbacks) {
    rv = mNotificationCallbacks->GetInterface(aIID, aResult);
    if (NS_SUCCEEDED(rv)) {
      NS_ASSERTION(*aResult, "Lying nsIInterfaceRequestor implementation!");
      return rv;
    }
  }

  if (mState & XML_HTTP_REQUEST_BACKGROUND) {
    nsCOMPtr<nsIInterfaceRequestor> badCertHandler(do_CreateInstance(NS_BADCERTHANDLER_CONTRACTID, &rv));

    // Ignore failure to get component, we may not have all its dependencies
    // available
    if (NS_SUCCEEDED(rv)) {
      rv = badCertHandler->GetInterface(aIID, aResult);
      if (NS_SUCCEEDED(rv))
        return rv;
    }
  }
  else if (aIID.Equals(NS_GET_IID(nsIAuthPrompt)) ||
           aIID.Equals(NS_GET_IID(nsIAuthPrompt2))) {
    nsCOMPtr<nsIPromptFactory> wwatch =
      do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the an auth prompter for our window so that the parenting
    // of the dialogs works as it should when using tabs.

    nsCOMPtr<nsIDOMWindow> window;
    if (mOwner) {
      window = mOwner->GetOuterWindow();
    }

    return wwatch->GetPrompt(window, aIID,
                             reinterpret_cast<void**>(aResult));

  }

  return QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
nsXMLHttpRequest::GetUpload(nsIXMLHttpRequestUpload** aUpload)
{
  *aUpload = nsnull;

  nsresult rv;
  nsIScriptContext* scriptContext =
    GetContextForEventHandlers(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!mUpload) {
    mUpload = new nsXMLHttpRequestUpload(mOwner, scriptContext);
    NS_ENSURE_TRUE(mUpload, NS_ERROR_OUT_OF_MEMORY);
  }
  NS_ADDREF(*aUpload = mUpload);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLHttpRequest::Notify(nsITimer* aTimer)
{
  mTimerIsActive = false;
  if (!(XML_HTTP_REQUEST_MPART_HEADERS & mState)) {
    MaybeDispatchProgressEvents(false);
  }

  return NS_OK;
}

void
nsXMLHttpRequest::StartProgressEventTimer()
{
  if (!mProgressNotifier) {
    mProgressNotifier = do_CreateInstance(NS_TIMER_CONTRACTID);
  }
  if (mProgressNotifier) {
    mProgressEventWasDelayed = false;
    mTimerIsActive = true;
    mProgressNotifier->Cancel();
    mProgressNotifier->InitWithCallback(this, NS_PROGRESS_EVENT_INTERVAL,
                                        nsITimer::TYPE_ONE_SHOT);
  }
}

NS_IMPL_ISUPPORTS1(nsXMLHttpRequest::nsHeaderVisitor, nsIHttpHeaderVisitor)

NS_IMETHODIMP nsXMLHttpRequest::
nsHeaderVisitor::VisitHeader(const nsACString &header, const nsACString &value)
{
    // See bug #380418. Hide "Set-Cookie" headers from non-chrome scripts.
    bool chrome = false; // default to false in case IsCapabilityEnabled fails
    IsCapabilityEnabled("UniversalXPConnect", &chrome);
    if (!chrome &&
         (header.LowerCaseEqualsASCII("set-cookie") ||
          header.LowerCaseEqualsASCII("set-cookie2"))) {
        NS_WARNING("blocked access to response header");
    } else {
        mHeaders.Append(header);
        mHeaders.Append(": ");
        mHeaders.Append(value);
        mHeaders.Append('\n');
    }
    return NS_OK;
}

// DOM event class to handle progress notifications
nsXMLHttpProgressEvent::nsXMLHttpProgressEvent(nsIDOMProgressEvent* aInner,
                                               PRUint64 aCurrentProgress,
                                               PRUint64 aMaxProgress,
                                               nsPIDOMWindow* aWindow)
  : mWindow(aWindow)
{
  mInner = static_cast<nsDOMProgressEvent*>(aInner);
  mCurProgress = aCurrentProgress;
  mMaxProgress = aMaxProgress;
}

nsXMLHttpProgressEvent::~nsXMLHttpProgressEvent()
{}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXMLHttpProgressEvent)

DOMCI_DATA(XMLHttpProgressEvent, nsXMLHttpProgressEvent)

// QueryInterface implementation for nsXMLHttpProgressEvent
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXMLHttpProgressEvent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMProgressEvent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEvent, nsIDOMProgressEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSEvent)
  NS_INTERFACE_MAP_ENTRY(nsIPrivateDOMEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMProgressEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLSProgressEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(XMLHttpProgressEvent)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXMLHttpProgressEvent)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXMLHttpProgressEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXMLHttpProgressEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mInner);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mWindow);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXMLHttpProgressEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mInner,
                                                       nsIDOMProgressEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mWindow);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMETHODIMP nsXMLHttpProgressEvent::GetInput(nsIDOMLSInput * *aInput)
{
  *aInput = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
nsXMLHttpProgressEvent::WarnAboutLSProgressEvent(nsIDocument::DeprecatedOperations aOperation)
{
  if (!mWindow) {
    return;
  }
  nsCOMPtr<nsIDocument> document =
    do_QueryInterface(mWindow->GetExtantDocument());
  if (!document) {
    return;
  }
  document->WarnOnceAbout(aOperation);
}

NS_IMETHODIMP nsXMLHttpProgressEvent::GetPosition(PRUint32 *aPosition)
{
  WarnAboutLSProgressEvent(nsIDocument::ePosition);
  // XXX can we change the iface?
  LL_L2UI(*aPosition, mCurProgress);
  return NS_OK;
}

NS_IMETHODIMP nsXMLHttpProgressEvent::GetTotalSize(PRUint32 *aTotalSize)
{
  WarnAboutLSProgressEvent(nsIDocument::eTotalSize);
  // XXX can we change the iface?
  LL_L2UI(*aTotalSize, mMaxProgress);
  return NS_OK;
}
