/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "prprf.h"
#include "nsIDOMEventListener.h"
#include "nsIJSContextStack.h"
#include "nsIScriptSecurityManager.h"
#include "nsWeakPtr.h"
#include "nsCharsetAlias.h"
#include "nsIScriptGlobalObject.h"
#include "nsDOMClassInfoID.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "nsIVariant.h"
#include "nsVariant.h"
#include "nsIScriptError.h"
#include "xpcpublic.h"
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
#include "nsStringBuffer.h"
#include "nsDOMFile.h"
#include "nsIFileChannel.h"
#include "mozilla/Telemetry.h"
#include "jsfriendapi.h"
#include "sampler.h"
#include "mozilla/dom/XMLHttpRequestBinding.h"
#include "nsIDOMFormData.h"
#include "DictionaryHelpers.h"
#include "mozilla/Attributes.h"

#include "nsWrapperCacheInlines.h"
#include "nsStreamListenerWrapper.h"

using namespace mozilla;
using namespace mozilla::dom;

#define LOAD_STR "load"
#define ERROR_STR "error"
#define ABORT_STR "abort"
#define TIMEOUT_STR "timeout"
#define LOADSTART_STR "loadstart"
#define PROGRESS_STR "progress"
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
#define XML_HTTP_REQUEST_TIMED_OUT (1 << 18) // Internal
#define XML_HTTP_REQUEST_DELETED (1 << 19) // Internal

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

#define IMPL_STRING_GETTER(_name)                                               \
  NS_IMETHODIMP                                                                 \
  nsXMLHttpRequest::_name(nsAString& aOut)                                      \
  {                                                                             \
    nsString tmp;                                                               \
    _name(tmp);                                                                 \
    aOut = tmp;                                                                 \
    return NS_OK;                                                               \
  }

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
                                    nullptr,
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
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnLoadListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnErrorListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnAbortListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnLoadStartListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnProgressListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnLoadendListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnTimeoutListener)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsXHREventTarget,
                                                nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnLoadListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnErrorListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnAbortListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnLoadStartListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnProgressListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnLoadendListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnTimeoutListener)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsXHREventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIXMLHttpRequestEventTarget)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(nsXHREventTarget, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsXHREventTarget, nsDOMEventTargetHelper)

void
nsXHREventTarget::DisconnectFromOwner()
{
  nsDOMEventTargetHelper::DisconnectFromOwner();
  NS_DISCONNECT_EVENT_HANDLER(Load)
  NS_DISCONNECT_EVENT_HANDLER(Error)
  NS_DISCONNECT_EVENT_HANDLER(Abort)
  NS_DISCONNECT_EVENT_HANDLER(Load)
  NS_DISCONNECT_EVENT_HANDLER(Progress)
  NS_DISCONNECT_EVENT_HANDLER(Loadend)
  NS_DISCONNECT_EVENT_HANDLER(Timeout)
}

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

/* attribute nsIDOMEventListener ontimeout; */
NS_IMETHODIMP
nsXHREventTarget::GetOntimeout(nsIDOMEventListener * *aOntimeout)
{
  return GetInnerEventListener(mOnTimeoutListener, aOntimeout);
}
NS_IMETHODIMP
nsXHREventTarget::SetOntimeout(nsIDOMEventListener *aOntimeout)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(TIMEOUT_STR),
                                mOnTimeoutListener, aOntimeout);
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
    mRequestObserver(nullptr), mState(XML_HTTP_REQUEST_UNSENT),
    mUploadTransferred(0), mUploadTotal(0), mUploadComplete(true),
    mProgressSinceLastProgressEvent(false),
    mUploadProgress(0), mUploadProgressMax(0),
    mRequestSentTime(0), mTimeoutMilliseconds(0),
    mErrorLoad(false), mWaitingForOnStopRequest(false),
    mProgressTimerIsActive(false), mProgressEventWasDelayed(false),
    mIsHtml(false),
    mWarnAboutMultipartHtml(false),
    mWarnAboutSyncHtml(false),
    mLoadLengthComputable(false), mLoadTotal(0),
    mIsSystem(false),
    mIsAnon(false),
    mFirstStartRequestSeen(false),
    mInLoadProgressEvent(false),
    mResultJSON(JSVAL_VOID),
    mResultArrayBuffer(nullptr)
{
  nsLayoutStatics::AddRef();

  SetIsDOMBinding();
#ifdef DEBUG
  StaticAssertions();
#endif
}

nsXMLHttpRequest::~nsXMLHttpRequest()
{
  mState |= XML_HTTP_REQUEST_DELETED;

  if (mState & (XML_HTTP_REQUEST_STOPPED |
                XML_HTTP_REQUEST_SENT |
                XML_HTTP_REQUEST_LOADING)) {
    Abort();
  }

  NS_ABORT_IF_FALSE(!(mState & XML_HTTP_REQUEST_SYNCLOOPING), "we rather crash than hang");
  mState &= ~XML_HTTP_REQUEST_SYNCLOOPING;

  nsLayoutStatics::Release();
}

void
nsXMLHttpRequest::RootJSResultObjects()
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
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  if (secMan) {
    secMan->GetSystemPrincipal(getter_AddRefs(subjectPrincipal));
  }
  NS_ENSURE_STATE(subjectPrincipal);
  Construct(subjectPrincipal, nullptr);
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
  Construct(aPrincipal,
            aOwnerWindow ? aOwnerWindow->GetCurrentInnerWindow() : nullptr,
            aBaseURI);
  return NS_OK;
}

/**
 * This Initialize method is called from XPConnect via nsIJSNativeInitializer.
 */
NS_IMETHODIMP
nsXMLHttpRequest::Initialize(nsISupports* aOwner, JSContext* cx, JSObject* obj,
                             PRUint32 argc, jsval *argv)
{
  nsCOMPtr<nsPIDOMWindow> owner = do_QueryInterface(aOwner);
  if (!owner) {
    NS_WARNING("Unexpected nsIJSNativeInitializer owner");
    return NS_OK;
  }

  // This XHR object is bound to a |window|,
  // so re-set principal and script context.
  nsCOMPtr<nsIScriptObjectPrincipal> scriptPrincipal = do_QueryInterface(aOwner);
  NS_ENSURE_STATE(scriptPrincipal);

  Construct(scriptPrincipal->GetPrincipal(), owner);
  if (argc) {
    nsresult rv = InitParameters(cx, argv);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult
nsXMLHttpRequest::InitParameters(JSContext* aCx, const jsval* aParams)
{
  XMLHttpRequestParameters params;
  nsresult rv = params.Init(aCx, aParams);
  NS_ENSURE_SUCCESS(rv, rv);

  InitParameters(params.mozAnon, params.mozSystem);
  return NS_OK;
}

void
nsXMLHttpRequest::InitParameters(bool aAnon, bool aSystem)
{
  // Check for permissions.
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(GetOwner());
  if (!window || !window->GetDocShell()) {
    return;
  }

  // Chrome is always allowed access, so do the permission check only
  // for non-chrome pages.
  if (!nsContentUtils::IsCallerChrome()) {
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(window->GetExtantDocument());
    if (!doc) {
      return;
    }

    nsCOMPtr<nsIURI> uri;
    doc->NodePrincipal()->GetURI(getter_AddRefs(uri));
    if (!nsContentUtils::URIIsChromeOrInPref(uri, "dom.systemXHR.whitelist")) {
      return;
    }
  }

  mIsAnon = aAnon;
  mIsSystem = aSystem;
}

void
nsXMLHttpRequest::ResetResponse()
{
  mResponseXML = nullptr;
  mResponseBody.Truncate();
  mResponseText.Truncate();
  mResponseBlob = nullptr;
  mDOMFile = nullptr;
  mBuilder = nullptr;
  mResultArrayBuffer = nullptr;
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

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsXMLHttpRequest)
  bool isBlack = tmp->IsBlack();
  if (isBlack || tmp->mWaitingForOnStopRequest) {
    if (tmp->mListenerManager) {
      tmp->mListenerManager->UnmarkGrayJSListeners();
      NS_UNMARK_LISTENER_WRAPPER(Load)
      NS_UNMARK_LISTENER_WRAPPER(Error)
      NS_UNMARK_LISTENER_WRAPPER(Abort)
      NS_UNMARK_LISTENER_WRAPPER(LoadStart)
      NS_UNMARK_LISTENER_WRAPPER(Progress)
      NS_UNMARK_LISTENER_WRAPPER(Loadend)
      NS_UNMARK_LISTENER_WRAPPER(Readystatechange)
    }
    if (!isBlack && tmp->PreservingWrapper()) {
      xpc_UnmarkGrayObject(tmp->GetWrapperPreserveColor());
    }
    return true;
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsXMLHttpRequest)
  return tmp->IsBlack();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsXMLHttpRequest)
  return tmp->IsBlack();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsXMLHttpRequest,
                                                  nsXHREventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mChannel)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mReadRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mResponseXML)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCORSPreflightChannel)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnReadystatechangeListener)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mXMLParserStreamListener)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mChannelEventSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mProgressEventSink)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mUpload,
                                                       nsIXMLHttpRequestUpload)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsXMLHttpRequest,
                                                nsXHREventTarget)
  tmp->mResultArrayBuffer = nullptr;
  tmp->mResultJSON = JSVAL_VOID;
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mChannel)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mReadRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mResponseXML)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCORSPreflightChannel)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnReadystatechangeListener)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mXMLParserStreamListener)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mChannelEventSink)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mProgressEventSink)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mUpload)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(nsXMLHttpRequest,
                                               nsXHREventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mResultArrayBuffer)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mResultJSON)
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

void
nsXMLHttpRequest::DisconnectFromOwner()
{
  nsXHREventTarget::DisconnectFromOwner();
  NS_DISCONNECT_EVENT_HANDLER(Readystatechange)
  Abort();
}

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

/* readonly attribute nsIChannel channel; */
NS_IMETHODIMP
nsXMLHttpRequest::GetChannel(nsIChannel **aChannel)
{
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_IF_ADDREF(*aChannel = mChannel);

  return NS_OK;
}

static void LogMessage(const char* aWarning, nsPIDOMWindow* aWindow)
{
  nsCOMPtr<nsIDocument> doc;
  if (aWindow) {
    doc = do_QueryInterface(aWindow->GetExtantDocument());
  }
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  "DOM", doc,
                                  nsContentUtils::eDOM_PROPERTIES,
                                  aWarning);
}

/* readonly attribute nsIDOMDocument responseXML; */
NS_IMETHODIMP
nsXMLHttpRequest::GetResponseXML(nsIDOMDocument **aResponseXML)
{
  ErrorResult rv;
  nsIDocument* responseXML = GetResponseXML(rv);
  if (rv.Failed()) {
    return rv.ErrorCode();
  }

  if (!responseXML) {
    *aResponseXML = nullptr;
    return NS_OK;
  }

  return CallQueryInterface(responseXML, aResponseXML);
}

nsIDocument*
nsXMLHttpRequest::GetResponseXML(ErrorResult& aRv)
{
  if (mResponseType != XML_HTTP_RESPONSE_TYPE_DEFAULT &&
      mResponseType != XML_HTTP_RESPONSE_TYPE_DOCUMENT) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  if (mWarnAboutMultipartHtml) {
    mWarnAboutMultipartHtml = false;
    LogMessage("HTMLMultipartXHRWarning", GetOwner());
  }
  if (mWarnAboutSyncHtml) {
    mWarnAboutSyncHtml = false;
    LogMessage("HTMLSyncXHRWarning", GetOwner());
  }
  return (XML_HTTP_REQUEST_DONE & mState) ? mResponseXML : nullptr;
}

/*
 * This piece copied from nsXMLDocument, we try to get the charset
 * from HTTP headers.
 */
nsresult
nsXMLHttpRequest::DetectCharset()
{
  mResponseCharset.Truncate();
  mDecoder = nullptr;

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
    rv = nsCharsetAlias::GetPreferred(charsetVal, mResponseCharset);
  }

  if (NS_FAILED(rv) || mResponseCharset.IsEmpty()) {
    // MS documentation states UTF-8 is default for responseText
    mResponseCharset.AssignLiteral("UTF-8");
  }

  if (mResponseType == XML_HTTP_RESPONSE_TYPE_JSON &&
      !mResponseCharset.EqualsLiteral("UTF-8")) {
    // The XHR spec says only UTF-8 is supported for responseType == "json"
    LogMessage("JSONCharsetWarning", GetOwner());
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

  if (!mResponseText.SetCapacity(mResponseText.Length() + destBufferLen, fallible_t())) {
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
NS_IMETHODIMP
nsXMLHttpRequest::GetResponseText(nsAString& aResponseText)
{
  ErrorResult rv;
  nsString responseText;
  GetResponseText(responseText, rv);
  aResponseText = responseText;
  return rv.ErrorCode();
}

void
nsXMLHttpRequest::GetResponseText(nsString& aResponseText, ErrorResult& aRv)
{
  aResponseText.Truncate();

  if (mResponseType != XML_HTTP_RESPONSE_TYPE_DEFAULT &&
      mResponseType != XML_HTTP_RESPONSE_TYPE_TEXT &&
      mResponseType != XML_HTTP_RESPONSE_TYPE_CHUNKED_TEXT) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (mResponseType == XML_HTTP_RESPONSE_TYPE_CHUNKED_TEXT &&
      !mInLoadProgressEvent) {
    aResponseText.SetIsVoid(true);
    return;
  }

  if (!(mState & (XML_HTTP_REQUEST_DONE | XML_HTTP_REQUEST_LOADING))) {
    return;
  }

  // We only decode text lazily if we're also parsing to a doc.
  // Also, if we've decoded all current data already, then no need to decode
  // more.
  if (!mResponseXML ||
      mResponseBodyDecodedPos == mResponseBody.Length()) {
    aResponseText = mResponseText;
    return;
  }

  if (mResponseCharset != mResponseXML->GetDocumentCharacterSet()) {
    mResponseCharset = mResponseXML->GetDocumentCharacterSet();
    mResponseText.Truncate();
    mResponseBodyDecodedPos = 0;

    nsresult rv;
    nsCOMPtr<nsICharsetConverterManager> ccm =
      do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }

    aRv = ccm->GetUnicodeDecoderRaw(mResponseCharset.get(),
                                    getter_AddRefs(mDecoder));
    if (aRv.Failed()) {
      return;
    }
  }

  NS_ASSERTION(mResponseBodyDecodedPos < mResponseBody.Length(),
               "Unexpected mResponseBodyDecodedPos");
  aRv = AppendToResponseText(mResponseBody.get() + mResponseBodyDecodedPos,
                             mResponseBody.Length() - mResponseBodyDecodedPos);
  if (aRv.Failed()) {
    return;
  }

  mResponseBodyDecodedPos = mResponseBody.Length();
  
  if (mState & XML_HTTP_REQUEST_DONE) {
    // Free memory buffer which we no longer need
    mResponseBody.Truncate();
    mResponseBodyDecodedPos = 0;
  }

  aResponseText = mResponseText;
}

nsresult
nsXMLHttpRequest::CreateResponseParsedJSON(JSContext* aCx)
{
  if (!aCx) {
    return NS_ERROR_FAILURE;
  }
  RootJSResultObjects();

  // The Unicode converter has already zapped the BOM if there was one
  if (!JS_ParseJSON(aCx,
                    static_cast<const jschar*>(mResponseText.get()),
                    mResponseText.Length(), &mResultJSON)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsXMLHttpRequest::CreatePartialBlob()
{
  if (mDOMFile) {
    if (mLoadTotal == mLoadTransferred) {
      mResponseBlob = mDOMFile;
    } else {
      mResponseBlob =
        mDOMFile->CreateSlice(0, mLoadTransferred, EmptyString());
    }
    return NS_OK;
  }

  // mBuilder can be null if the request has been canceled
  if (!mBuilder) {
    return NS_OK;
  }

  nsCAutoString contentType;
  if (mLoadTotal == mLoadTransferred) {
    mChannel->GetContentType(contentType);
  }

  return mBuilder->GetBlobInternal(NS_ConvertASCIItoUTF16(contentType),
                                   false, getter_AddRefs(mResponseBlob));
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
    aResponseType.AssignLiteral("json");
    break;
  case XML_HTTP_RESPONSE_TYPE_CHUNKED_TEXT:
    aResponseType.AssignLiteral("moz-chunked-text");
    break;
  case XML_HTTP_RESPONSE_TYPE_CHUNKED_ARRAYBUFFER:
    aResponseType.AssignLiteral("moz-chunked-arraybuffer");
    break;
  case XML_HTTP_RESPONSE_TYPE_MOZ_BLOB:
    aResponseType.AssignLiteral("moz-blob");
    break;
  default:
    NS_ERROR("Should not happen");
  }

  return NS_OK;
}

#ifdef DEBUG
void
nsXMLHttpRequest::StaticAssertions()
{
#define ASSERT_ENUM_EQUAL(_lc, _uc) \
  MOZ_STATIC_ASSERT(\
    XMLHttpRequestResponseTypeValues::_lc                \
    == XMLHttpRequestResponseType(XML_HTTP_RESPONSE_TYPE_ ## _uc), \
    #_uc " should match")

  ASSERT_ENUM_EQUAL(_empty, DEFAULT);
  ASSERT_ENUM_EQUAL(Arraybuffer, ARRAYBUFFER);
  ASSERT_ENUM_EQUAL(Blob, BLOB);
  ASSERT_ENUM_EQUAL(Document, DOCUMENT);
  ASSERT_ENUM_EQUAL(Json, JSON);
  ASSERT_ENUM_EQUAL(Text, TEXT);
  ASSERT_ENUM_EQUAL(Moz_chunked_text, CHUNKED_TEXT);
  ASSERT_ENUM_EQUAL(Moz_chunked_arraybuffer, CHUNKED_ARRAYBUFFER);
  ASSERT_ENUM_EQUAL(Moz_blob, MOZ_BLOB);
#undef ASSERT_ENUM_EQUAL
}
#endif

/* attribute AString responseType; */
NS_IMETHODIMP nsXMLHttpRequest::SetResponseType(const nsAString& aResponseType)
{
  nsXMLHttpRequest::ResponseType responseType;
  if (aResponseType.IsEmpty()) {
    responseType = XML_HTTP_RESPONSE_TYPE_DEFAULT;
  } else if (aResponseType.EqualsLiteral("arraybuffer")) {
    responseType = XML_HTTP_RESPONSE_TYPE_ARRAYBUFFER;
  } else if (aResponseType.EqualsLiteral("blob")) {
    responseType = XML_HTTP_RESPONSE_TYPE_BLOB;
  } else if (aResponseType.EqualsLiteral("document")) {
    responseType = XML_HTTP_RESPONSE_TYPE_DOCUMENT;
  } else if (aResponseType.EqualsLiteral("text")) {
    responseType = XML_HTTP_RESPONSE_TYPE_TEXT;
  } else if (aResponseType.EqualsLiteral("json")) {
    responseType = XML_HTTP_RESPONSE_TYPE_JSON;
  } else if (aResponseType.EqualsLiteral("moz-chunked-text")) {
    responseType = XML_HTTP_RESPONSE_TYPE_CHUNKED_TEXT;
  } else if (aResponseType.EqualsLiteral("moz-chunked-arraybuffer")) {
    responseType = XML_HTTP_RESPONSE_TYPE_CHUNKED_ARRAYBUFFER;
  } else if (aResponseType.EqualsLiteral("moz-blob")) {
    responseType = XML_HTTP_RESPONSE_TYPE_MOZ_BLOB;
  } else {
    return NS_OK;
  }

  ErrorResult rv;
  SetResponseType(responseType, rv);
  return rv.ErrorCode();
}

void
nsXMLHttpRequest::SetResponseType(XMLHttpRequestResponseType aType,
                                  ErrorResult& aRv)
{
  SetResponseType(ResponseType(aType), aRv);
}

void
nsXMLHttpRequest::SetResponseType(nsXMLHttpRequest::ResponseType aResponseType,
                                  ErrorResult& aRv)
{
  // If the state is not OPENED or HEADERS_RECEIVED raise an
  // INVALID_STATE_ERR exception and terminate these steps.
  if (!(mState & (XML_HTTP_REQUEST_OPENED | XML_HTTP_REQUEST_SENT |
                  XML_HTTP_REQUEST_HEADERS_RECEIVED))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // sync request is not allowed setting responseType in window context
  if (HasOrHasHadOwner() &&
      !(mState & (XML_HTTP_REQUEST_UNSENT | XML_HTTP_REQUEST_ASYNC))) {
    LogMessage("ResponseTypeSyncXHRWarning", GetOwner());
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  if (!(mState & XML_HTTP_REQUEST_ASYNC) &&
      (aResponseType == XML_HTTP_RESPONSE_TYPE_CHUNKED_TEXT ||
       aResponseType == XML_HTTP_RESPONSE_TYPE_CHUNKED_ARRAYBUFFER)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // Set the responseType attribute's value to the given value.
  mResponseType = aResponseType;

  // If the state is OPENED, SetCacheAsFile would have no effect here
  // because the channel hasn't initialized the cache entry yet.
  // SetCacheAsFile will be called from OnStartRequest.
  // If the state is HEADERS_RECEIVED, however, we need to call
  // it immediately because OnStartRequest is already dispatched.
  if (mState & XML_HTTP_REQUEST_HEADERS_RECEIVED) {
    nsCOMPtr<nsICachingChannel> cc(do_QueryInterface(mChannel));
    if (cc) {
      cc->SetCacheAsFile(mResponseType == XML_HTTP_RESPONSE_TYPE_BLOB ||
                         mResponseType == XML_HTTP_RESPONSE_TYPE_MOZ_BLOB);
    }
  }
}

/* readonly attribute jsval response; */
NS_IMETHODIMP
nsXMLHttpRequest::GetResponse(JSContext *aCx, jsval *aResult)
{
  ErrorResult rv;
  *aResult = GetResponse(aCx, rv);
  return rv.ErrorCode();
}

JS::Value
nsXMLHttpRequest::GetResponse(JSContext* aCx, ErrorResult& aRv)
{
  switch (mResponseType) {
  case XML_HTTP_RESPONSE_TYPE_DEFAULT:
  case XML_HTTP_RESPONSE_TYPE_TEXT:
  case XML_HTTP_RESPONSE_TYPE_CHUNKED_TEXT:
  {
    nsString str;
    aRv = GetResponseText(str);
    if (aRv.Failed()) {
      return JSVAL_NULL;
    }
    JS::Value result;
    if (!xpc::StringToJsval(aCx, str, &result)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return JSVAL_NULL;
    }
    return result;
  }

  case XML_HTTP_RESPONSE_TYPE_ARRAYBUFFER:
  case XML_HTTP_RESPONSE_TYPE_CHUNKED_ARRAYBUFFER:
  {
    if (!(mResponseType == XML_HTTP_RESPONSE_TYPE_ARRAYBUFFER &&
          mState & XML_HTTP_REQUEST_DONE) &&
        !(mResponseType == XML_HTTP_RESPONSE_TYPE_CHUNKED_ARRAYBUFFER &&
          mInLoadProgressEvent)) {
      return JSVAL_NULL;
    }

    if (!mResultArrayBuffer) {
      RootJSResultObjects();
      aRv = nsContentUtils::CreateArrayBuffer(aCx, mResponseBody,
                                              &mResultArrayBuffer);
      if (aRv.Failed()) {
        return JSVAL_NULL;
      }
    }
    return OBJECT_TO_JSVAL(mResultArrayBuffer);
  }
  case XML_HTTP_RESPONSE_TYPE_BLOB:
  case XML_HTTP_RESPONSE_TYPE_MOZ_BLOB:
  {
    if (!(mState & XML_HTTP_REQUEST_DONE)) {
      if (mResponseType != XML_HTTP_RESPONSE_TYPE_MOZ_BLOB) {
        return JSVAL_NULL;
      }

      if (!mResponseBlob) {
        aRv = CreatePartialBlob();
        if (aRv.Failed()) {
          return JSVAL_NULL;
        }
      }
    }

    if (!mResponseBlob) {
      return JSVAL_NULL;
    }

    JS::Value result = JSVAL_NULL;
    JSObject* scope = JS_GetGlobalForScopeChain(aCx);
    aRv = nsContentUtils::WrapNative(aCx, scope, mResponseBlob, &result,
                                     nullptr, true);
    return result;
  }
  case XML_HTTP_RESPONSE_TYPE_DOCUMENT:
  {
    if (!(mState & XML_HTTP_REQUEST_DONE) || !mResponseXML) {
      return JSVAL_NULL;
    }

    JSObject* scope = JS_GetGlobalForScopeChain(aCx);
    JS::Value result = JSVAL_NULL;
    aRv = nsContentUtils::WrapNative(aCx, scope, mResponseXML, &result,
                                     nullptr, true);
    return result;
  }
  case XML_HTTP_RESPONSE_TYPE_JSON:
  {
    if (!(mState & XML_HTTP_REQUEST_DONE)) {
      return JSVAL_NULL;
    }

    if (mResultJSON == JSVAL_VOID) {
      aRv = CreateResponseParsedJSON(aCx);
      mResponseText.Truncate();
      if (aRv.Failed()) {
        // Per spec, errors aren't propagated. null is returned instead.
        aRv = NS_OK;
        // It would be nice to log the error to the console. That's hard to
        // do without calling window.onerror as a side effect, though.
        JS_ClearPendingException(aCx);
        mResultJSON = JSVAL_NULL;
      }
    }
    return mResultJSON;
  }
  default:
    NS_ERROR("Should not happen");
  }

  return JSVAL_NULL;
}

/* readonly attribute unsigned long status; */
NS_IMETHODIMP
nsXMLHttpRequest::GetStatus(PRUint32 *aStatus)
{
  *aStatus = GetStatus();
  return NS_OK;
}

uint32_t
nsXMLHttpRequest::GetStatus()
{
  if (mState & XML_HTTP_REQUEST_USE_XSITE_AC) {
    // Make sure we don't leak status information from denied cross-site
    // requests.
    if (mChannel) {
      nsresult status;
      mChannel->GetStatus(&status);
      if (NS_FAILED(status)) {
        return 0;
      }
    }
  }

  PRUint16 readyState;
  GetReadyState(&readyState);
  if (readyState == UNSENT || readyState == OPENED || mErrorLoad) {
    return 0;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = GetCurrentHttpChannel();
  if (!httpChannel) {
    return 0;
  }

  PRUint32 status;
  nsresult rv = httpChannel->GetResponseStatus(&status);
  if (NS_FAILED(rv)) {
    status = 0;
  }

  return status;
}

IMPL_STRING_GETTER(GetStatusText)
void
nsXMLHttpRequest::GetStatusText(nsString& aStatusText)
{
  nsCOMPtr<nsIHttpChannel> httpChannel = GetCurrentHttpChannel();

  aStatusText.Truncate();

  if (!httpChannel) {
    return;
  }

  if (mState & XML_HTTP_REQUEST_USE_XSITE_AC) {
    // Make sure we don't leak status information from denied cross-site
    // requests.
    if (mChannel) {
      nsresult status;
      mChannel->GetStatus(&status);
      if (NS_FAILED(status)) {
        return;
      }
    }
  }

  nsCString statusText;
  httpChannel->GetResponseStatusText(statusText);
  if (statusText.IsVoid()) {
    aStatusText.SetIsVoid(true);
  } else {
    // We use UTF8ToNewUnicode here because it truncates after invalid UTF-8
    // characters, CopyUTF8toUTF16 just doesn't copy in that case.
    PRUint32 length;
    PRUnichar* chars = UTF8ToNewUnicode(statusText, &length);
    aStatusText.Adopt(chars, length);
  }
}

void
nsXMLHttpRequest::CloseRequestWithError(const nsAString& aType,
                                        const PRUint32 aFlag)
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
  if (mTimeoutTimer) {
    mTimeoutTimer->Cancel();
  }
  PRUint32 responseLength = mResponseBody.Length();
  ResetResponse();
  mState |= aFlag;

  // If we're in the destructor, don't risk dispatching an event.
  if (mState & XML_HTTP_REQUEST_DELETED) {
    mState &= ~XML_HTTP_REQUEST_SYNCLOOPING;
    return;
  }

  if (!(mState & (XML_HTTP_REQUEST_UNSENT |
                  XML_HTTP_REQUEST_OPENED |
                  XML_HTTP_REQUEST_DONE))) {
    ChangeState(XML_HTTP_REQUEST_DONE, true);

    if (!(mState & XML_HTTP_REQUEST_SYNCLOOPING)) {
      DispatchProgressEvent(this, aType, mLoadLengthComputable, responseLength,
                            mLoadTotal);
      if (mUpload && !mUploadComplete) {
        mUploadComplete = true;
        DispatchProgressEvent(mUpload, aType, true, mUploadTransferred,
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
}

/* void abort (); */
void
nsXMLHttpRequest::Abort()
{
  CloseRequestWithError(NS_LITERAL_STRING(ABORT_STR), XML_HTTP_REQUEST_ABORTED);
}

NS_IMETHODIMP
nsXMLHttpRequest::SlowAbort()
{
  Abort();
  return NS_OK;
}

/* DOMString getAllResponseHeaders(); */
IMPL_STRING_GETTER(GetAllResponseHeaders)
void
nsXMLHttpRequest::GetAllResponseHeaders(nsString& aResponseHeaders)
{
  aResponseHeaders.Truncate();

  // If the state is UNSENT or OPENED,
  // return the empty string and terminate these steps.
  if (mState & (XML_HTTP_REQUEST_UNSENT |
                XML_HTTP_REQUEST_OPENED | XML_HTTP_REQUEST_SENT)) {
    return;
  }

  if (mState & XML_HTTP_REQUEST_USE_XSITE_AC) {
    return;
  }

  if (nsCOMPtr<nsIHttpChannel> httpChannel = GetCurrentHttpChannel()) {
    nsRefPtr<nsHeaderVisitor> visitor = new nsHeaderVisitor();
    if (NS_SUCCEEDED(httpChannel->VisitResponseHeaders(visitor))) {
      CopyASCIItoUTF16(visitor->Headers(), aResponseHeaders);
    }
    return;
  }

  if (!mChannel) {
    return;
  }

  // Even non-http channels supply content type.
  nsCAutoString value;
  if (NS_SUCCEEDED(mChannel->GetContentType(value))) {
    aResponseHeaders.AppendLiteral("Content-Type: ");
    AppendASCIItoUTF16(value, aResponseHeaders);
    if (NS_SUCCEEDED(mChannel->GetContentCharset(value)) && !value.IsEmpty()) {
      aResponseHeaders.AppendLiteral(";charset=");
      AppendASCIItoUTF16(value, aResponseHeaders);
    }
    aResponseHeaders.AppendLiteral("\r\n");
  }
}

NS_IMETHODIMP
nsXMLHttpRequest::GetResponseHeader(const nsACString& aHeader,
                                    nsACString& aResult)
{
  ErrorResult rv;
  GetResponseHeader(aHeader, aResult, rv);
  return rv.ErrorCode();
}

void
nsXMLHttpRequest::GetResponseHeader(const nsACString& header,
                                    nsACString& _retval, ErrorResult& aRv)
{
  _retval.SetIsVoid(true);

  nsCOMPtr<nsIHttpChannel> httpChannel = GetCurrentHttpChannel();

  if (!httpChannel) {
    // If the state is UNSENT or OPENED,
    // return null and terminate these steps.
    if (mState & (XML_HTTP_REQUEST_UNSENT |
                  XML_HTTP_REQUEST_OPENED | XML_HTTP_REQUEST_SENT)) {
      return;
    }

    // Even non-http channels supply content type.
    // Remember we don't leak header information from denied cross-site
    // requests.
    nsresult status;
    if (!mChannel ||
        NS_FAILED(mChannel->GetStatus(&status)) ||
        NS_FAILED(status) ||
        !header.LowerCaseEqualsASCII("content-type")) {
      return;
    }

    if (NS_FAILED(mChannel->GetContentType(_retval))) {
      // Means no content type
      _retval.SetIsVoid(true);
      return;
    }

    nsCString value;
    if (NS_SUCCEEDED(mChannel->GetContentCharset(value)) &&
        !value.IsEmpty()) {
      _retval.Append(";charset=");
      _retval.Append(value);
    }

    return;
  }

  // See bug #380418. Hide "Set-Cookie" headers from non-chrome scripts.
  bool chrome = false; // default to false in case IsCapabilityEnabled fails
  IsCapabilityEnabled("UniversalXPConnect", &chrome);
  if (!chrome &&
       (header.LowerCaseEqualsASCII("set-cookie") ||
        header.LowerCaseEqualsASCII("set-cookie2"))) {
    NS_WARNING("blocked access to response header");
    return;
  }

  // Check for dangerous headers
  if (mState & XML_HTTP_REQUEST_USE_XSITE_AC) {
    // Make sure we don't leak header information from denied cross-site
    // requests.
    if (mChannel) {
      nsresult status;
      mChannel->GetStatus(&status);
      if (NS_FAILED(status)) {
        return;
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
          return;
        }
        if (header.Equals(token, nsCaseInsensitiveCStringComparator())) {
          safeHeader = true;
        }
      }
    }

    if (!safeHeader) {
      return;
    }
  }

  aRv = httpChannel->GetResponseHeader(header, _retval);
  if (aRv.ErrorCode() == NS_ERROR_NOT_AVAILABLE) {
    // Means no header
    _retval.SetIsVoid(true);
    aRv = NS_OK;
  }
}

already_AddRefed<nsILoadGroup>
nsXMLHttpRequest::GetLoadGroup() const
{
  if (mState & XML_HTTP_REQUEST_BACKGROUND) {                 
    return nullptr;
  }

  nsresult rv = NS_ERROR_FAILURE;
  nsIScriptContext* sc =
    const_cast<nsXMLHttpRequest*>(this)->GetContextForEventHandlers(&rv);
  nsCOMPtr<nsIDocument> doc =
    nsContentUtils::GetDocumentFromScriptContext(sc);
  if (doc) {
    return doc->GetDocumentLoadGroup();
  }

  return nullptr;
}

nsresult
nsXMLHttpRequest::CreateReadystatechangeEvent(nsIDOMEvent** aDOMEvent)
{
  nsresult rv = nsEventDispatcher::CreateEvent(nullptr, nullptr,
                                               NS_LITERAL_STRING("Events"),
                                               aDOMEvent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  (*aDOMEvent)->InitEvent(NS_LITERAL_STRING(READYSTATE_STR),
                          false, false);

  // We assume anyone who managed to call CreateReadystatechangeEvent is trusted
  (*aDOMEvent)->SetTrusted(true);

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
      (!AllowUploadProgress() && aTarget == mUpload)) {
    return;
  }

  bool dispatchLoadend = aType.EqualsLiteral(LOAD_STR) ||
                           aType.EqualsLiteral(ERROR_STR) ||
                           aType.EqualsLiteral(TIMEOUT_STR) ||
                           aType.EqualsLiteral(ABORT_STR);
  
  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = nsEventDispatcher::CreateEvent(nullptr, nullptr,
                                               NS_LITERAL_STRING("ProgressEvent"),
                                               getter_AddRefs(event));
  if (NS_FAILED(rv)) {
    return;
  }

  event->SetTrusted(true);

  nsCOMPtr<nsIDOMProgressEvent> progress = do_QueryInterface(event);
  if (!progress) {
    return;
  }

  progress->InitProgressEvent(aType, false, false, aLengthComputable,
                              aLoaded, (aTotal == LL_MAXUINT) ? 0 : aTotal);

  if (aUseLSEventWrapper) {
    nsCOMPtr<nsIDOMProgressEvent> xhrprogressEvent =
      new nsXMLHttpProgressEvent(progress, aPosition, aTotalSize, GetOwner());
    event = xhrprogressEvent;
  }
  aTarget->DispatchDOMEvent(nullptr, event, nullptr, nullptr);
  
  if (dispatchLoadend) {
    DispatchProgressEvent(aTarget, NS_LITERAL_STRING(LOADEND_STR),
                          aUseLSEventWrapper, aLengthComputable,
                          aLoaded, aTotal, aPosition, aTotalSize);
  }
}
                                          
already_AddRefed<nsIHttpChannel>
nsXMLHttpRequest::GetCurrentHttpChannel()
{
  nsIHttpChannel *httpChannel = nullptr;

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
  return mIsSystem || nsContentUtils::IsSystemPrincipal(mPrincipal);
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

  // exempt data URIs from the same origin check.
  nsCOMPtr<nsIURI> channelURI;
  bool dataScheme = false;
  if (NS_SUCCEEDED(NS_GetFinalChannelURI(aChannel,
                                         getter_AddRefs(channelURI))) &&
      NS_SUCCEEDED(channelURI->SchemeIs("data", &dataScheme)) &&
      dataScheme) {
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
  if (!optional_argc) {
    // No optional arguments were passed in. Default async to true.
    async = true;
  }
  Optional<nsAString> realUser;
  if (optional_argc > 1) {
    realUser = &user;
  }
  Optional<nsAString> realPassword;
  if (optional_argc > 2) {
    realPassword = &password;
  }
  return Open(method, url, async, realUser, realPassword);
}

nsresult
nsXMLHttpRequest::Open(const nsACString& method, const nsACString& url,
                       bool async, const Optional<nsAString>& user,
                       const Optional<nsAString>& password)
{
  NS_ENSURE_ARG(!method.IsEmpty());

  Telemetry::Accumulate(Telemetry::XMLHTTPREQUEST_ASYNC_OR_SYNC,
                        async ? 0 : 1);

  NS_ENSURE_TRUE(mPrincipal, NS_ERROR_NOT_INITIALIZED);

  // Disallow HTTP/1.1 TRACE method (see bug 302489)
  // and MS IIS equivalent TRACK (see bug 381264)
  if (method.LowerCaseEqualsLiteral("trace") ||
      method.LowerCaseEqualsLiteral("track")) {
    return NS_ERROR_INVALID_ARG;
  }

  // sync request is not allowed using withCredential or responseType
  // in window context
  if (!async && HasOrHasHadOwner() &&
      (mState & XML_HTTP_REQUEST_AC_WITH_CREDENTIALS ||
       mTimeoutMilliseconds ||
       mResponseType != XML_HTTP_RESPONSE_TYPE_DEFAULT)) {
    if (mState & XML_HTTP_REQUEST_AC_WITH_CREDENTIALS) {
      LogMessage("WithCredentialsSyncXHRWarning", GetOwner());
    }
    if (mTimeoutMilliseconds) {
      LogMessage("TimeoutSyncXHRWarning", GetOwner());
    }
    if (mResponseType != XML_HTTP_RESPONSE_TYPE_DEFAULT) {
      LogMessage("ResponseTypeSyncXHRWarning", GetOwner());
    }
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  nsresult rv;
  nsCOMPtr<nsIURI> uri;

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

  // Unset any pre-existing aborted and timed-out states.
  mState &= ~XML_HTTP_REQUEST_ABORTED & ~XML_HTTP_REQUEST_TIMED_OUT;

  if (async) {
    mState |= XML_HTTP_REQUEST_ASYNC;
  } else {
    mState &= ~XML_HTTP_REQUEST_ASYNC;
  }

  mState &= ~XML_HTTP_REQUEST_MPART_HEADERS;

  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDocument> doc =
    nsContentUtils::GetDocumentFromScriptContext(sc);
  
  nsCOMPtr<nsIURI> baseURI;
  if (mBaseURI) {
    baseURI = mBaseURI;
  }
  else if (doc) {
    baseURI = doc->GetBaseURI();
  }

  rv = NS_NewURI(getter_AddRefs(uri), url, nullptr, baseURI);
  if (NS_FAILED(rv)) return rv;

  rv = CheckInnerWindowCorrectness();
  NS_ENSURE_SUCCESS(rv, rv);
  PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_XMLHTTPREQUEST,
                                 uri,
                                 mPrincipal,
                                 doc,
                                 EmptyCString(), //mime guess
                                 nullptr,         //extra
                                 &shouldLoad,
                                 nsContentUtils::GetContentPolicy(),
                                 nsContentUtils::GetSecurityManager());
  if (NS_FAILED(rv)) return rv;
  if (NS_CP_REJECTED(shouldLoad)) {
    // Disallowed by content policy
    return NS_ERROR_CONTENT_BLOCKED;
  }

  // XXXbz this is wrong: we should only be looking at whether
  // user/password were passed, not at the values!  See bug 759624.
  if (user.WasPassed() && !user.Value().IsEmpty()) {
    nsCAutoString userpass;
    CopyUTF16toUTF8(user.Value(), userpass);
    if (password.WasPassed() && !password.Value().IsEmpty()) {
      userpass.Append(':');
      AppendUTF16toUTF8(password.Value(), userpass);
    }
    uri->SetUserPass(userpass);
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
                     nullptr,                    // ioService
                     loadGroup,
                     nullptr,                    // callbacks
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

  nsresult rv = NS_OK;

  if (xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_BLOB ||
      xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_MOZ_BLOB) {
    if (!xmlHttpRequest->mDOMFile) {
      if (!xmlHttpRequest->mBuilder) {
        xmlHttpRequest->mBuilder = new nsDOMBlobBuilder();
      }
      rv = xmlHttpRequest->mBuilder->AppendVoidPtr(fromRawSegment, count);
    }
    // Clear the cache so that the blob size is updated.
    if (xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_MOZ_BLOB) {
      xmlHttpRequest->mResponseBlob = nullptr;
    }
    if (NS_SUCCEEDED(rv)) {
      *writeCount = count;
    }
    return rv;
  }

  if ((xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_DEFAULT &&
       xmlHttpRequest->mResponseXML) ||
      xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_ARRAYBUFFER ||
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

bool nsXMLHttpRequest::CreateDOMFile(nsIRequest *request)
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

    mDOMFile =
      new nsDOMFileFile(file, NS_ConvertASCIItoUTF16(contentType), cacheToken);
    mBuilder = nullptr;
    NS_ASSERTION(mResponseBody.IsEmpty(), "mResponseBody should be empty");
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
  if ((mResponseType == XML_HTTP_RESPONSE_TYPE_BLOB ||
       mResponseType == XML_HTTP_RESPONSE_TYPE_MOZ_BLOB) && !mDOMFile) {
    cancelable = CreateDOMFile(request);
    // The nsIStreamListener contract mandates us
    // to read from the stream before returning.
  }

  PRUint32 totalRead;
  nsresult rv = inStr->ReadSegments(nsXMLHttpRequest::StreamReaderFunc,
                                    (void*)this, count, &totalRead);
  NS_ENSURE_SUCCESS(rv, rv);

  if (cancelable) {
    // We don't have to read from the local file for the blob response
    mDOMFile->GetSize(&mLoadTransferred);
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
  SAMPLE_LABEL("nsXMLHttpRequest", "OnStartRequest");
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

  /* Apparently, Abort() should set XML_HTTP_REQUEST_UNSENT.  See bug 361773.
     XHR2 spec says this is correct. */
  if (mState & XML_HTTP_REQUEST_ABORTED) {
    NS_ERROR("Ugh, still getting data on an aborted XMLHttpRequest!");

    return NS_ERROR_UNEXPECTED;
  }

  // Don't do anything if we have timed out.
  if (mState & XML_HTTP_REQUEST_TIMED_OUT) {
    return NS_OK;
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
    if (mProgressTimerIsActive) {
      mProgressTimerIsActive = false;
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

  if (mResponseType == XML_HTTP_RESPONSE_TYPE_BLOB ||
      mResponseType == XML_HTTP_RESPONSE_TYPE_MOZ_BLOB) {
    nsCOMPtr<nsICachingChannel> cc(do_QueryInterface(mChannel));
    if (cc) {
      cc->SetCacheAsFile(true);
    }
  }

  ResetResponse();

  if (!mOverrideMimeType.IsEmpty()) {
    channel->SetContentType(NS_ConvertUTF16toUTF8(mOverrideMimeType));
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
    nsIScriptContext* sc = GetContextForEventHandlers(&rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIDocument> doc =
      nsContentUtils::GetDocumentFromScriptContext(sc);

    if (doc) {
      docURI = doc->GetDocumentURI();
      baseURI = doc->GetBaseURI();
    }

    // Create an empty document from it.  Here we have to cheat a little bit...
    // Setting the base URI to |baseURI| won't work if the document has a null
    // principal, so use mPrincipal when creating the document, then reset the
    // principal.
    const nsAString& emptyStr = EmptyString();
    nsCOMPtr<nsIScriptGlobalObject> global = do_QueryInterface(GetOwner());
    nsCOMPtr<nsIDOMDocument> responseDoc;
    rv = nsContentUtils::CreateDocument(emptyStr, emptyStr, nullptr, docURI,
                                        baseURI, mPrincipal, global,
                                        mIsHtml ? DocumentFlavorHTML :
                                                  DocumentFlavorLegacyGuess,
                                        getter_AddRefs(responseDoc));
    NS_ENSURE_SUCCESS(rv, rv);
    mResponseXML = do_QueryInterface(responseDoc);
    mResponseXML->SetPrincipal(documentPrincipal);

    if (nsContentUtils::IsSystemPrincipal(mPrincipal)) {
      mResponseXML->ForceEnableXULXBL();
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

    rv = mResponseXML->StartDocumentLoad(kLoadAsData, channel, loadGroup,
                                         nullptr, getter_AddRefs(listener),
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
  SAMPLE_LABEL("content", "nsXMLHttpRequest::OnStopRequest");
  if (!IsSameOrBaseChannel(request, mChannel)) {
    return NS_OK;
  }

  mWaitingForOnStopRequest = false;

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
  // XML_HTTP_REQUEST_UNSENT is for abort calls.  See OnStartRequest above.
  if ((mState & XML_HTTP_REQUEST_UNSENT) ||
      (mState & XML_HTTP_REQUEST_TIMED_OUT)) {
    if (mXMLParserStreamListener)
      (void) mXMLParserStreamListener->OnStopRequest(request, ctxt, status);
    return NS_OK;
  }

  // Is this good enough here?
  if (mState & XML_HTTP_REQUEST_PARSEBODY && mXMLParserStreamListener) {
    mXMLParserStreamListener->OnStopRequest(request, ctxt, status);
  }

  mXMLParserStreamListener = nullptr;
  mReadRequest = nullptr;
  mContext = nullptr;

  // If we're received data since the last progress event, make sure to fire
  // an event for it, except in the HTML case, defer the last progress event
  // until the parser is done.
  if (!mIsHtml) {
    MaybeDispatchProgressEvents(true);
  }

  if (NS_SUCCEEDED(status) &&
      (mResponseType == XML_HTTP_RESPONSE_TYPE_BLOB ||
       mResponseType == XML_HTTP_RESPONSE_TYPE_MOZ_BLOB)) {
    if (!mDOMFile) {
      CreateDOMFile(request);
    }
    if (mDOMFile) {
      mResponseBlob = mDOMFile;
      mDOMFile = nullptr;
    } else {
      // Smaller files may be written in cache map instead of separate files.
      // Also, no-store response cannot be written in persistent cache.
      nsCAutoString contentType;
      mChannel->GetContentType(contentType);
      mBuilder->GetBlobInternal(NS_ConvertASCIItoUTF16(contentType),
                                false, getter_AddRefs(mResponseBlob));
      mBuilder = nullptr;
    }
    NS_ASSERTION(mResponseBody.IsEmpty(), "mResponseBody should be empty");
    NS_ASSERTION(mResponseText.IsEmpty(), "mResponseText should be empty");
  }

  nsCOMPtr<nsIChannel> channel(do_QueryInterface(request));
  NS_ENSURE_TRUE(channel, NS_ERROR_UNEXPECTED);

  channel->SetNotificationCallbacks(nullptr);
  mNotificationCallbacks = nullptr;
  mChannelEventSink = nullptr;
  mProgressEventSink = nullptr;

  mState &= ~XML_HTTP_REQUEST_SYNCLOOPING;

  if (NS_FAILED(status)) {
    // This can happen if the server is unreachable. Other possible
    // reasons are that the user leaves the page or hits the ESC key.

    mErrorLoad = true;
    mResponseXML = nullptr;
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
  if (!mResponseXML->GetRootElement()) {
    mResponseXML = nullptr;
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
  if (mTimeoutTimer) {
    mTimeoutTimer->Cancel();
  }

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
    mChannel = nullptr;
    mCORSPreflightChannel = nullptr;
  }
  else if (!(mState & XML_HTTP_REQUEST_GOT_FINAL_STOP)) {
    // We're a multipart request, so we're not done. Reset to opened.
    ChangeState(XML_HTTP_REQUEST_OPENED);
  }
}

NS_IMETHODIMP
nsXMLHttpRequest::SendAsBinary(const nsAString &aBody)
{
  ErrorResult rv;
  SendAsBinary(aBody, rv);
  return rv.ErrorCode();
}

void
nsXMLHttpRequest::SendAsBinary(const nsAString &aBody,
                               ErrorResult& aRv)
{
  char *data = static_cast<char*>(NS_Alloc(aBody.Length() + 1));
  if (!data) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  nsAString::const_iterator iter, end;
  aBody.BeginReading(iter);
  aBody.EndReading(end);
  char *p = data;
  while (iter != end) {
    if (*iter & 0xFF00) {
      NS_Free(data);
      aRv.Throw(NS_ERROR_DOM_INVALID_CHARACTER_ERR);
      return;
    }
    *p++ = static_cast<char>(*iter++);
  }
  *p = '\0';

  nsCOMPtr<nsIInputStream> stream;
  aRv = NS_NewByteInputStream(getter_AddRefs(stream), data, aBody.Length(),
                              NS_ASSIGNMENT_ADOPT);
  if (aRv.Failed()) {
    NS_Free(data);
    return;
  }

  nsCOMPtr<nsIWritableVariant> variant = new nsVariant();

  aRv = variant->SetAsISupports(stream);
  if (aRv.Failed()) {
    return;
  }

  aRv = Send(variant);
}

static nsresult
GetRequestBody(nsIDOMDocument* aDoc, nsIInputStream** aResult,
               nsACString& aContentType, nsACString& aCharset)
{
  aContentType.AssignLiteral("application/xml");
  nsAutoString inputEncoding;
  aDoc->GetInputEncoding(inputEncoding);
  if (!DOMStringIsNull(inputEncoding)) {
    CopyUTF16toUTF8(inputEncoding, aCharset);
  }
  else {
    aCharset.AssignLiteral("UTF-8");
  }

  // Serialize to a stream so that the encoding used will
  // match the document's.
  nsresult rv;
  nsCOMPtr<nsIDOMSerializer> serializer =
    do_CreateInstance(NS_XMLSERIALIZER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStorageStream> storStream;
  rv = NS_NewStorageStream(4096, PR_UINT32_MAX, getter_AddRefs(storStream));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIOutputStream> output;
  rv = storStream->GetOutputStream(0, getter_AddRefs(output));
  NS_ENSURE_SUCCESS(rv, rv);

  // Make sure to use the encoding we'll send
  rv = serializer->SerializeToStream(aDoc, output, aCharset);
  NS_ENSURE_SUCCESS(rv, rv);

  output->Close();

  return storStream->NewInputStream(0, aResult);
}

static nsresult
GetRequestBody(const nsAString& aString, nsIInputStream** aResult,
               nsACString& aContentType, nsACString& aCharset)
{
  aContentType.AssignLiteral("text/plain");
  aCharset.AssignLiteral("UTF-8");

  return NS_NewCStringInputStream(aResult, NS_ConvertUTF16toUTF8(aString));
}

static nsresult
GetRequestBody(nsIInputStream* aStream, nsIInputStream** aResult,
               nsACString& aContentType, nsACString& aCharset)
{
  aContentType.AssignLiteral("text/plain");
  aCharset.Truncate();

  NS_ADDREF(*aResult = aStream);

  return NS_OK;
}

static nsresult
GetRequestBody(nsIXHRSendable* aSendable, nsIInputStream** aResult,
               nsACString& aContentType, nsACString& aCharset)
{
  return aSendable->GetSendInfo(aResult, aContentType, aCharset);
}

static nsresult
GetRequestBody(ArrayBuffer* aArrayBuffer, nsIInputStream** aResult,
               nsACString& aContentType, nsACString& aCharset)
{
  aContentType.SetIsVoid(true);
  aCharset.Truncate();

  PRInt32 length = aArrayBuffer->Length();
  char* data = reinterpret_cast<char*>(aArrayBuffer->Data());

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream), data, length,
                                      NS_ASSIGNMENT_COPY);
  NS_ENSURE_SUCCESS(rv, rv);

  stream.forget(aResult);

  return NS_OK;
}

static nsresult
GetRequestBody(nsIVariant* aBody, nsIInputStream** aResult,
               nsACString& aContentType, nsACString& aCharset)
{
  *aResult = nullptr;

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
      return GetRequestBody(doc, aResult, aContentType, aCharset);
    }

    // nsISupportsString?
    nsCOMPtr<nsISupportsString> wstr = do_QueryInterface(supports);
    if (wstr) {
      nsAutoString string;
      wstr->GetData(string);

      return GetRequestBody(string, aResult, aContentType, aCharset);
    }

    // nsIInputStream?
    nsCOMPtr<nsIInputStream> stream = do_QueryInterface(supports);
    if (stream) {
      return GetRequestBody(stream, aResult, aContentType, aCharset);
    }

    // nsIXHRSendable?
    nsCOMPtr<nsIXHRSendable> sendable = do_QueryInterface(supports);
    if (sendable) {
      return GetRequestBody(sendable, aResult, aContentType, aCharset);
    }

    // ArrayBuffer?
    jsval realVal;
    nsCxPusher pusher;
    JSAutoEnterCompartment ac;
    JSObject* obj;

    // If there's a context on the stack, we can just use it. Otherwise, we need
    // to use the safe js context (and push it into the stack, so that it's
    // visible to cx-less functions that we might call here).
    JSContext* cx = nsContentUtils::GetCurrentJSContext();
    if (!cx) {
      cx = nsContentUtils::GetSafeJSContext();
      if (!pusher.Push(cx)) {
        return NS_ERROR_FAILURE;
      }
    }

    nsresult rv = aBody->GetAsJSVal(&realVal);
    if (NS_SUCCEEDED(rv) && !JSVAL_IS_PRIMITIVE(realVal) &&
        (obj = JSVAL_TO_OBJECT(realVal)) &&
        ac.enter(cx, obj) &&
        (JS_IsArrayBufferObject(obj, cx))) {
      ArrayBuffer buf(cx, obj);
      return GetRequestBody(&buf, aResult, aContentType, aCharset);
    }
  }
  else if (dataType == nsIDataType::VTYPE_VOID ||
           dataType == nsIDataType::VTYPE_EMPTY) {
    // Makes us act as if !aBody, don't upload anything
    aContentType.AssignLiteral("text/plain");
    aCharset.AssignLiteral("UTF-8");

    return NS_OK;
  }

  PRUnichar* data = nullptr;
  PRUint32 len = 0;
  rv = aBody->GetAsWStringWithSize(&len, &data);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString string;
  string.Adopt(data, len);

  return GetRequestBody(string, aResult, aContentType, aCharset);
}

/* static */
nsresult
nsXMLHttpRequest::GetRequestBody(nsIVariant* aVariant,
                                 const Nullable<RequestBody>& aBody,
                                 nsIInputStream** aResult,
                                 nsACString& aContentType, nsACString& aCharset)
{
  if (aVariant) {
    return ::GetRequestBody(aVariant, aResult, aContentType, aCharset);
  }

  const RequestBody& body = aBody.Value();
  RequestBody::Value value = body.GetValue();
  switch (body.GetType()) {
    case nsXMLHttpRequest::RequestBody::ArrayBuffer:
    {
      return ::GetRequestBody(value.mArrayBuffer, aResult, aContentType, aCharset);
    }
    case nsXMLHttpRequest::RequestBody::Blob:
    {
      nsresult rv;
      nsCOMPtr<nsIXHRSendable> sendable = do_QueryInterface(value.mBlob, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      return ::GetRequestBody(sendable, aResult, aContentType, aCharset);
    }
    case nsXMLHttpRequest::RequestBody::Document:
    {
      nsCOMPtr<nsIDOMDocument> document = do_QueryInterface(value.mDocument);
      return ::GetRequestBody(document, aResult, aContentType, aCharset);
    }
    case nsXMLHttpRequest::RequestBody::DOMString:
    {
      return ::GetRequestBody(*value.mString, aResult, aContentType, aCharset);
    }
    case nsXMLHttpRequest::RequestBody::FormData:
    {
      nsresult rv;
      nsCOMPtr<nsIXHRSendable> sendable = do_QueryInterface(value.mFormData, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      return ::GetRequestBody(sendable, aResult, aContentType, aCharset);
    }
    case nsXMLHttpRequest::RequestBody::InputStream:
    {
      return ::GetRequestBody(value.mStream, aResult, aContentType, aCharset);
    }
    default:
    {
      return NS_ERROR_FAILURE;
    }
  }

  NS_NOTREACHED("Default cases exist for a reason");
  return NS_OK;
}

/* void send (in nsIVariant aBody); */
NS_IMETHODIMP
nsXMLHttpRequest::Send(nsIVariant *aBody)
{
  return Send(aBody, Nullable<RequestBody>());
}

nsresult
nsXMLHttpRequest::Send(nsIVariant* aVariant, const Nullable<RequestBody>& aBody)
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

      nsIScriptContext* sc = GetContextForEventHandlers(&rv);
      NS_ENSURE_SUCCESS(rv, rv);
      nsCOMPtr<nsIDocument> doc =
        nsContentUtils::GetDocumentFromScriptContext(sc);

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
  if ((aVariant || !aBody.IsNull()) && httpChannel &&
      !method.EqualsLiteral("GET")) {

    nsCAutoString charset;
    nsCAutoString defaultContentType;
    nsCOMPtr<nsIInputStream> postDataStream;

    rv = GetRequestBody(aVariant, aBody, getter_AddRefs(postDataStream),
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
    Telemetry::Accumulate(Telemetry::MULTIPART_XHR_RESPONSE, 1);
    listener = new nsMultipartProxyListener(listener);
  } else {
    Telemetry::Accumulate(Telemetry::MULTIPART_XHR_RESPONSE, 0);
  }

  // Blocking gets are common enough out of XHR that we should mark
  // the channel slow by default for pipeline purposes
  AddLoadFlags(mChannel, nsIRequest::INHIBIT_PIPELINE);

  if (!IsSystemXHR()) {
    // Always create a nsCORSListenerProxy here even if it's
    // a same-origin request right now, since it could be redirected.
    listener = new nsCORSListenerProxy(listener, mPrincipal, mChannel,
                                       withCredentials, true, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // Because of bug 682305, we can't let listener be the XHR object itself
    // because JS wouldn't be able to use it. So if we haven't otherwise
    // created a listener around 'this', do so now.

    listener = new nsStreamListenerWrapper(listener);
  }

  if (mIsAnon) {
    AddLoadFlags(mChannel, nsIRequest::LOAD_ANONYMOUS);
  }

  NS_ASSERTION(listener != this,
               "Using an object as a listener that can't be exposed to JS");

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

  // We're about to send the request.  Start our timeout.
  mRequestSentTime = PR_Now();
  StartTimeoutTimer();

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
    rv = mChannel->AsyncOpen(listener, nullptr);
  }

  if (NS_FAILED(rv)) {
    // Drop our ref to the channel to avoid cycles
    mChannel = nullptr;
    mCORSPreflightChannel = nullptr;
    return rv;
  }

  // Either AsyncOpen was called, or CORS will open the channel later.
  mWaitingForOnStopRequest = true;

  // If we're synchronous, spin an event loop here and wait
  if (!(mState & XML_HTTP_REQUEST_ASYNC)) {
    mState |= XML_HTTP_REQUEST_SYNCLOOPING;

    nsCOMPtr<nsIDocument> suspendedDoc;
    nsCOMPtr<nsIRunnable> resumeTimeoutRunnable;
    if (GetOwner()) {
      nsCOMPtr<nsIDOMWindow> topWindow;
      if (NS_SUCCEEDED(GetOwner()->GetTop(getter_AddRefs(topWindow)))) {
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

    {
      nsAutoSyncOperation sync(suspendedDoc);
      // Note, calling ChangeState may have cleared
      // XML_HTTP_REQUEST_SYNCLOOPING flag.
      nsIThread *thread = NS_GetCurrentThread();
      while (mState & XML_HTTP_REQUEST_SYNCLOOPING) {
        if (!NS_ProcessNextEvent(thread)) {
          rv = NS_ERROR_UNEXPECTED;
          break;
        }
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
    if (mUpload && mUpload->HasListenersFor(NS_LITERAL_STRING(PROGRESS_STR))) {
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
// http://dvcs.w3.org/hg/xhr/raw-file/tip/Overview.html#dom-xmlhttprequest-setrequestheader
NS_IMETHODIMP
nsXMLHttpRequest::SetRequestHeader(const nsACString& header,
                                   const nsACString& value)
{
  // Step 1 and 2
  if (!(mState & XML_HTTP_REQUEST_OPENED)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }
  NS_ASSERTION(mChannel, "mChannel must be valid if we're OPENED.");

  // Step 3
  // Make sure we don't store an invalid header name in mCORSUnsafeHeaders
  if (!IsValidHTTPToken(header)) { // XXX nsHttp::IsValidToken?
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  // Check that we haven't already opened the channel. We can't rely on
  // the channel throwing from mChannel->SetRequestHeader since we might
  // still be waiting for mCORSPreflightChannel to actually open mChannel
  if (mCORSPreflightChannel) {
    bool pending;
    nsresult rv = mCORSPreflightChannel->IsPending(&pending);
    NS_ENSURE_SUCCESS(rv, rv);
    
    if (pending) {
      return NS_ERROR_IN_PROGRESS;
    }
  }

  if (!mChannel)             // open() initializes mChannel, and open()
    return NS_ERROR_FAILURE; // must be called before first setRequestHeader()

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
  if (!httpChannel) {
    return NS_OK;
  }

  // Prevent modification to certain HTTP headers (see bug 302263), unless
  // the executing script has UniversalXPConnect.

  bool privileged;
  if (NS_FAILED(IsCapabilityEnabled("UniversalXPConnect", &privileged)))
    return NS_ERROR_FAILURE;

  if (!privileged) {
    // Step 5: Check for dangerous headers.
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
  nsresult rv = httpChannel->SetRequestHeader(header, value, false);
  if (rv == NS_ERROR_INVALID_ARG) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }
  if (NS_SUCCEEDED(rv)) {
    // We'll want to duplicate this header for any replacement channels (eg. on redirect)
    RequestHeader reqHeader = {
      nsCString(header), nsCString(value)
    };
    mModifiedRequestHeaders.AppendElement(reqHeader);
  }
  return rv;
}

/* attribute unsigned long timeout; */
NS_IMETHODIMP
nsXMLHttpRequest::GetTimeout(PRUint32 *aTimeout)
{
  *aTimeout = GetTimeout();
  return NS_OK;
}

NS_IMETHODIMP
nsXMLHttpRequest::SetTimeout(PRUint32 aTimeout)
{
  ErrorResult rv;
  SetTimeout(aTimeout, rv);
  return rv.ErrorCode();
}

void
nsXMLHttpRequest::SetTimeout(uint32_t aTimeout, ErrorResult& aRv)
{
  if (!(mState & (XML_HTTP_REQUEST_ASYNC | XML_HTTP_REQUEST_UNSENT)) &&
      HasOrHasHadOwner()) {
    /* Timeout is not supported for synchronous requests with an owning window,
       per XHR2 spec. */
    LogMessage("TimeoutSyncXHRWarning", GetOwner());
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  mTimeoutMilliseconds = aTimeout;
  if (mRequestSentTime) {
    StartTimeoutTimer();
  }
}

void
nsXMLHttpRequest::StartTimeoutTimer()
{
  NS_ABORT_IF_FALSE(mRequestSentTime,
                    "StartTimeoutTimer mustn't be called before the request was sent!");
  if (mState & XML_HTTP_REQUEST_DONE) {
    // do nothing!
    return;
  }

  if (mTimeoutTimer) {
    mTimeoutTimer->Cancel();
  }

  if (!mTimeoutMilliseconds) {
    return;
  }

  if (!mTimeoutTimer) {
    mTimeoutTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  }
  PRUint32 elapsed =
    (PRUint32)((PR_Now() - mRequestSentTime) / PR_USEC_PER_MSEC);
  mTimeoutTimer->InitWithCallback(
    this,
    mTimeoutMilliseconds > elapsed ? mTimeoutMilliseconds - elapsed : 0,
    nsITimer::TYPE_ONE_SHOT
  );
}

/* readonly attribute unsigned short readyState; */
NS_IMETHODIMP
nsXMLHttpRequest::GetReadyState(PRUint16 *aState)
{
  *aState = GetReadyState();
  return NS_OK;
}

uint16_t
nsXMLHttpRequest::GetReadyState()
{
  // Translate some of our internal states for external consumers
  if (mState & XML_HTTP_REQUEST_UNSENT) {
    return UNSENT;
  }
  if (mState & (XML_HTTP_REQUEST_OPENED | XML_HTTP_REQUEST_SENT)) {
    return OPENED;
  }
  if (mState & XML_HTTP_REQUEST_HEADERS_RECEIVED) {
    return HEADERS_RECEIVED;
  }
  if (mState & (XML_HTTP_REQUEST_LOADING | XML_HTTP_REQUEST_STOPPED)) {
    return LOADING;
  }
  MOZ_ASSERT(mState & XML_HTTP_REQUEST_DONE);
  return DONE;
}

/* void overrideMimeType(in DOMString mimetype); */
NS_IMETHODIMP
nsXMLHttpRequest::SlowOverrideMimeType(const nsAString& aMimeType)
{
  OverrideMimeType(aMimeType);
  return NS_OK;
}

/* attribute boolean multipart; */
NS_IMETHODIMP
nsXMLHttpRequest::GetMultipart(bool *_retval)
{
  *_retval = GetMultipart();
  return NS_OK;
}

bool
nsXMLHttpRequest::GetMultipart()
{
  return !!(mState & XML_HTTP_REQUEST_MULTIPART);
}

NS_IMETHODIMP
nsXMLHttpRequest::SetMultipart(bool aMultipart)
{
  nsresult rv = NS_OK;
  SetMultipart(aMultipart, rv);
  return rv;
}

void
nsXMLHttpRequest::SetMultipart(bool aMultipart, nsresult& aRv)
{
  if (!(mState & XML_HTTP_REQUEST_UNSENT)) {
    // Can't change this while we're in the middle of something.
    aRv = NS_ERROR_IN_PROGRESS;
    return;
  }

  if (aMultipart) {
    mState |= XML_HTTP_REQUEST_MULTIPART;
  } else {
    mState &= ~XML_HTTP_REQUEST_MULTIPART;
  }
}

/* attribute boolean mozBackgroundRequest; */
NS_IMETHODIMP
nsXMLHttpRequest::GetMozBackgroundRequest(bool *_retval)
{
  *_retval = GetMozBackgroundRequest();
  return NS_OK;
}

bool
nsXMLHttpRequest::GetMozBackgroundRequest()
{
  return !!(mState & XML_HTTP_REQUEST_BACKGROUND);
}

NS_IMETHODIMP
nsXMLHttpRequest::SetMozBackgroundRequest(bool aMozBackgroundRequest)
{
  nsresult rv = NS_OK;
  SetMozBackgroundRequest(aMozBackgroundRequest, rv);
  return rv;
}

void
nsXMLHttpRequest::SetMozBackgroundRequest(bool aMozBackgroundRequest, nsresult& aRv)
{
  bool privileged;
  aRv = IsCapabilityEnabled("UniversalXPConnect", &privileged);
  if (NS_FAILED(aRv)) {
    return;
  }

  if (!privileged) {
    aRv = NS_ERROR_DOM_SECURITY_ERR;
    return;
  }

  if (!(mState & XML_HTTP_REQUEST_UNSENT)) {
    // Can't change this while we're in the middle of something.
    aRv = NS_ERROR_IN_PROGRESS;
    return;
  }

  if (aMozBackgroundRequest) {
    mState |= XML_HTTP_REQUEST_BACKGROUND;
  } else {
    mState &= ~XML_HTTP_REQUEST_BACKGROUND;
  }
}

/* attribute boolean withCredentials; */
NS_IMETHODIMP
nsXMLHttpRequest::GetWithCredentials(bool *_retval)
{
  *_retval = GetWithCredentials();
  return NS_OK;
}

bool
nsXMLHttpRequest::GetWithCredentials()
{
  return !!(mState & XML_HTTP_REQUEST_AC_WITH_CREDENTIALS);
}

NS_IMETHODIMP
nsXMLHttpRequest::SetWithCredentials(bool aWithCredentials)
{
  nsresult rv = NS_OK;
  SetWithCredentials(aWithCredentials, rv);
  return rv;
}

void
nsXMLHttpRequest::SetWithCredentials(bool aWithCredentials, nsresult& aRv)
{
  // Return error if we're already processing a request
  if (XML_HTTP_REQUEST_SENT & mState) {
    aRv = NS_ERROR_FAILURE;
    return;
  }

  // sync request is not allowed setting withCredentials in window context
  if (HasOrHasHadOwner() &&
      !(mState & (XML_HTTP_REQUEST_UNSENT | XML_HTTP_REQUEST_ASYNC))) {
    LogMessage("WithCredentialsSyncXHRWarning", GetOwner());
    aRv = NS_ERROR_DOM_INVALID_ACCESS_ERR;
    return;
  }

  if (aWithCredentials) {
    mState |= XML_HTTP_REQUEST_AC_WITH_CREDENTIALS;
  } else {
    mState &= ~XML_HTTP_REQUEST_AC_WITH_CREDENTIALS;
  }
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
    mProgressTimerIsActive = false;
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

    DispatchDOMEvent(nullptr, event, nullptr, nullptr);
  }

  return rv;
}

/*
 * Simple helper class that just forwards the redirect callback back
 * to the nsXMLHttpRequest.
 */
class AsyncVerifyRedirectCallbackForwarder MOZ_FINAL : public nsIAsyncVerifyRedirectCallback
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
        mRedirectCallback = nullptr;
        mNewRedirectChannel = nullptr;
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

  mNewRedirectChannel = nullptr;

  mRedirectCallback->OnRedirectVerifyCallback(result);
  mRedirectCallback = nullptr;
}

/////////////////////////////////////////////////////
// nsIProgressEventSink methods:
//

void
nsXMLHttpRequest::MaybeDispatchProgressEvents(bool aFinalProgress)
{
  if (aFinalProgress && mProgressTimerIsActive) {
    mProgressTimerIsActive = false;
    mProgressNotifier->Cancel();
  }

  if (mProgressTimerIsActive ||
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
    }
    if (mUpload && !mUploadComplete) {
      DispatchProgressEvent(mUpload, NS_LITERAL_STRING(PROGRESS_STR),
                            true, mUploadLengthComputable, mUploadTransferred,
                            mUploadTotal, mUploadProgress,
                            mUploadProgressMax);
    }
  } else {
    if (aFinalProgress) {
      mLoadTotal = mLoadTransferred;
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
      mResultArrayBuffer = nullptr;
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
    if (GetOwner()) {
      window = GetOwner()->GetOuterWindow();
    }

    return wwatch->GetPrompt(window, aIID,
                             reinterpret_cast<void**>(aResult));

  }

  return QueryInterface(aIID, aResult);
}

JS::Value
nsXMLHttpRequest::GetInterface(JSContext* aCx, nsIJSIID* aIID, ErrorResult& aRv)
{
  const nsID* iid = aIID->GetID();
  nsCOMPtr<nsISupports> result;
  JS::Value v = JSVAL_NULL;
  aRv = GetInterface(*iid, getter_AddRefs(result));
  NS_ENSURE_FALSE(aRv.Failed(), JSVAL_NULL);

  JSObject* global = JS_GetGlobalForObject(aCx, GetWrapper());
  aRv = nsContentUtils::WrapNative(aCx, global, result, iid, &v);
  return aRv.Failed() ? JSVAL_NULL : v;
}

nsXMLHttpRequestUpload*
nsXMLHttpRequest::GetUpload()
{
  if (!mUpload) {
    mUpload = new nsXMLHttpRequestUpload(this);
  }
  return mUpload;
}

NS_IMETHODIMP
nsXMLHttpRequest::GetUpload(nsIXMLHttpRequestUpload** aUpload)
{
  nsRefPtr<nsXMLHttpRequestUpload> upload = GetUpload();
  upload.forget(aUpload);
  return NS_OK;
}

bool
nsXMLHttpRequest::GetMozAnon()
{
  return mIsAnon;
}

NS_IMETHODIMP
nsXMLHttpRequest::GetMozAnon(bool* aAnon)
{
  *aAnon = GetMozAnon();
  return NS_OK;
}

bool
nsXMLHttpRequest::GetMozSystem()
{
  return IsSystemXHR();
}

NS_IMETHODIMP
nsXMLHttpRequest::GetMozSystem(bool* aSystem)
{
  *aSystem = GetMozSystem();
  return NS_OK;
}

void
nsXMLHttpRequest::HandleTimeoutCallback()
{
  if (mState & XML_HTTP_REQUEST_DONE) {
    NS_NOTREACHED("nsXMLHttpRequest::HandleTimeoutCallback with completed request");
    // do nothing!
    return;
  }

  CloseRequestWithError(NS_LITERAL_STRING(TIMEOUT_STR),
                        XML_HTTP_REQUEST_TIMED_OUT);
}

NS_IMETHODIMP
nsXMLHttpRequest::Notify(nsITimer* aTimer)
{
  if (mProgressNotifier == aTimer) {
    HandleProgressTimerCallback();
    return NS_OK;
  }

  if (mTimeoutTimer == aTimer) {
    HandleTimeoutCallback();
    return NS_OK;
  }

  // Just in case some JS user wants to QI to nsITimerCallback and play with us...
  NS_WARNING("Unexpected timer!");
  return NS_ERROR_INVALID_POINTER;
}

void
nsXMLHttpRequest::HandleProgressTimerCallback()
{
  mProgressTimerIsActive = false;
  if (!(XML_HTTP_REQUEST_MPART_HEADERS & mState)) {
    MaybeDispatchProgressEvents(false);
  }
}

void
nsXMLHttpRequest::StartProgressEventTimer()
{
  if (!mProgressNotifier) {
    mProgressNotifier = do_CreateInstance(NS_TIMER_CONTRACTID);
  }
  if (mProgressNotifier) {
    mProgressEventWasDelayed = false;
    mProgressTimerIsActive = true;
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
        mHeaders.Append("\r\n");
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
  *aInput = nullptr;
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
