/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXMLHttpRequest.h"

#ifndef XP_WIN
#include <unistd.h>
#endif
#include "mozilla/ArrayUtils.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/BlobSet.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FetchUtil.h"
#include "mozilla/dom/XMLHttpRequestUploadBinding.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/MemoryReporting.h"
#include "nsIDOMDocument.h"
#include "mozilla/dom/ProgressEvent.h"
#include "nsIJARChannel.h"
#include "nsIJARURI.h"
#include "nsLayoutCID.h"
#include "nsReadableUtils.h"

#include "nsIURI.h"
#include "nsILoadGroup.h"
#include "nsNetUtil.h"
#include "nsStringStream.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPrompt2.h"
#include "nsIOutputStream.h"
#include "nsISupportsPrimitives.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "nsIUploadChannel.h"
#include "nsIUploadChannel2.h"
#include "nsIDOMSerializer.h"
#include "nsXPCOM.h"
#include "nsIDOMEventListener.h"
#include "nsIScriptSecurityManager.h"
#include "nsIDOMWindow.h"
#include "nsIVariant.h"
#include "nsVariant.h"
#include "nsIScriptError.h"
#include "nsIStreamConverterService.h"
#include "nsICachingChannel.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsError.h"
#include "nsCORSListenerProxy.h"
#include "nsIHTMLDocument.h"
#include "nsIStorageStream.h"
#include "nsIPromptFactory.h"
#include "nsIWindowWatcher.h"
#include "nsIConsoleService.h"
#include "nsIContentSecurityPolicy.h"
#include "nsAsyncRedirectVerifyHelper.h"
#include "nsStringBuffer.h"
#include "nsIFileChannel.h"
#include "mozilla/Telemetry.h"
#include "jsfriendapi.h"
#include "GeckoProfiler.h"
#include "mozilla/dom/EncodingUtils.h"
#include "nsIUnicodeDecoder.h"
#include "mozilla/dom/XMLHttpRequestBinding.h"
#include "mozilla/Attributes.h"
#include "nsIPermissionManager.h"
#include "nsMimeTypes.h"
#include "nsIHttpChannelInternal.h"
#include "nsIClassOfService.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsFormData.h"
#include "nsStreamListenerWrapper.h"
#include "xpcjsid.h"
#include "nsITimedChannel.h"
#include "nsWrapperCacheInlines.h"
#include "nsZipArchive.h"
#include "mozilla/Preferences.h"
#include "private/pprio.h"

using namespace mozilla;
using namespace mozilla::dom;

// Maximum size that we'll grow an ArrayBuffer instead of doubling,
// once doubling reaches this threshold
#define XML_HTTP_REQUEST_ARRAYBUFFER_MAX_GROWTH (32*1024*1024)
// start at 32k to avoid lots of doubling right at the start
#define XML_HTTP_REQUEST_ARRAYBUFFER_MIN_SIZE (32*1024)
// the maximum Content-Length that we'll preallocate.  1GB.  Must fit
// in an int32_t!
#define XML_HTTP_REQUEST_MAX_CONTENT_LENGTH_PREALLOCATE (1*1024*1024*1024LL)

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
#define XML_HTTP_REQUEST_SENT             (1 << 5) // Internal, corresponds to
                                                   // "OPENED and the send()
                                                   // flag is set" in spec
                                                   // terms.
// The above states are mutually exclusive, change with ChangeState() only.
// The states below can be combined.
#define XML_HTTP_REQUEST_ABORTED        (1 << 7)  // Internal
#define XML_HTTP_REQUEST_ASYNC          (1 << 8)  // Internal
#define XML_HTTP_REQUEST_PARSEBODY      (1 << 9)  // Internal
#define XML_HTTP_REQUEST_SYNCLOOPING    (1 << 10) // Internal
#define XML_HTTP_REQUEST_BACKGROUND     (1 << 13) // Internal
#define XML_HTTP_REQUEST_USE_XSITE_AC   (1 << 14) // Internal
#define XML_HTTP_REQUEST_NEED_AC_PREFLIGHT (1 << 15) // Internal
#define XML_HTTP_REQUEST_AC_WITH_CREDENTIALS (1 << 16) // Internal
#define XML_HTTP_REQUEST_TIMED_OUT (1 << 17) // Internal
#define XML_HTTP_REQUEST_DELETED (1 << 18) // Internal

#define XML_HTTP_REQUEST_LOADSTATES         \
  (XML_HTTP_REQUEST_UNSENT |                \
   XML_HTTP_REQUEST_OPENED |                \
   XML_HTTP_REQUEST_HEADERS_RECEIVED |      \
   XML_HTTP_REQUEST_LOADING |               \
   XML_HTTP_REQUEST_DONE |                  \
   XML_HTTP_REQUEST_SENT)

#define NS_BADCERTHANDLER_CONTRACTID \
  "@mozilla.org/content/xmlhttprequest-bad-cert-handler;1"

#define NS_PROGRESS_EVENT_INTERVAL 50

#define IMPL_CSTRING_GETTER(_name)                                              \
  NS_IMETHODIMP                                                                 \
  nsXMLHttpRequest::_name(nsACString& aOut)                                     \
  {                                                                             \
    nsCString tmp;                                                              \
    _name(tmp);                                                                 \
    aOut = tmp;                                                                 \
    return NS_OK;                                                               \
  }

NS_IMPL_ISUPPORTS(nsXHRParseEndListener, nsIDOMEventListener)

class nsResumeTimeoutsEvent : public nsRunnable
{
public:
  explicit nsResumeTimeoutsEvent(nsPIDOMWindow* aWindow) : mWindow(aWindow) {}

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

//-----------------------------------------------------------------------------
// XMLHttpRequestAuthPrompt
//-----------------------------------------------------------------------------

class XMLHttpRequestAuthPrompt : public nsIAuthPrompt
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTHPROMPT

  XMLHttpRequestAuthPrompt();

protected:
  virtual ~XMLHttpRequestAuthPrompt();
};

NS_IMPL_ISUPPORTS(XMLHttpRequestAuthPrompt, nsIAuthPrompt)

XMLHttpRequestAuthPrompt::XMLHttpRequestAuthPrompt()
{
  MOZ_COUNT_CTOR(XMLHttpRequestAuthPrompt);
}

XMLHttpRequestAuthPrompt::~XMLHttpRequestAuthPrompt()
{
  MOZ_COUNT_DTOR(XMLHttpRequestAuthPrompt);
}

NS_IMETHODIMP
XMLHttpRequestAuthPrompt::Prompt(const char16_t* aDialogTitle,
                                 const char16_t* aText,
                                 const char16_t* aPasswordRealm,
                                 uint32_t aSavePassword,
                                 const char16_t* aDefaultText,
                                 char16_t** aResult,
                                 bool* aRetval)
{
  *aRetval = false;
  return NS_OK;
}

NS_IMETHODIMP
XMLHttpRequestAuthPrompt::PromptUsernameAndPassword(const char16_t* aDialogTitle,
                                                    const char16_t* aDialogText,
                                                    const char16_t* aPasswordRealm,
                                                    uint32_t aSavePassword,
                                                    char16_t** aUser,
                                                    char16_t** aPwd,
                                                    bool* aRetval)
{
  *aRetval = false;
  return NS_OK;
}

NS_IMETHODIMP
XMLHttpRequestAuthPrompt::PromptPassword(const char16_t* aDialogTitle,
                                         const char16_t* aText,
                                         const char16_t* aPasswordRealm,
                                         uint32_t aSavePassword,
                                         char16_t** aPwd,
                                         bool* aRetval)
{
  *aRetval = false;
  return NS_OK;
}

/////////////////////////////////////////////

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXHREventTarget)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsXHREventTarget,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsXHREventTarget,
                                                DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsXHREventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIXMLHttpRequestEventTarget)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(nsXHREventTarget, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsXHREventTarget, DOMEventTargetHelper)

void
nsXHREventTarget::DisconnectFromOwner()
{
  DOMEventTargetHelper::DisconnectFromOwner();
}

/////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN(nsXMLHttpRequestUpload)
  NS_INTERFACE_MAP_ENTRY(nsIXMLHttpRequestUpload)
NS_INTERFACE_MAP_END_INHERITING(nsXHREventTarget)

NS_IMPL_ADDREF_INHERITED(nsXMLHttpRequestUpload, nsXHREventTarget)
NS_IMPL_RELEASE_INHERITED(nsXMLHttpRequestUpload, nsXHREventTarget)

/* virtual */ JSObject*
nsXMLHttpRequestUpload::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return XMLHttpRequestUploadBinding::Wrap(aCx, this, aGivenProto);
}

/////////////////////////////////////////////
//
//
/////////////////////////////////////////////

bool
nsXMLHttpRequest::sDontWarnAboutSyncXHR = false;

nsXMLHttpRequest::nsXMLHttpRequest()
  : mResponseBodyDecodedPos(0),
    mResponseType(XML_HTTP_RESPONSE_TYPE_DEFAULT),
    mRequestObserver(nullptr),
    mState(XML_HTTP_REQUEST_UNSENT | XML_HTTP_REQUEST_ASYNC),
    mUploadTransferred(0), mUploadTotal(0), mUploadComplete(true),
    mProgressSinceLastProgressEvent(false),
    mRequestSentTime(0), mTimeoutMilliseconds(0),
    mErrorLoad(false), mWaitingForOnStopRequest(false),
    mProgressTimerIsActive(false),
    mIsHtml(false),
    mWarnAboutSyncHtml(false),
    mLoadLengthComputable(false), mLoadTotal(0),
    mIsSystem(false),
    mIsAnon(false),
    mFirstStartRequestSeen(false),
    mInLoadProgressEvent(false),
    mResultJSON(JS::UndefinedValue()),
    mResultArrayBuffer(nullptr),
    mIsMappedArrayBuffer(false),
    mXPCOMifier(nullptr)
{
#ifdef DEBUG
  StaticAssertions();
#endif
}

nsXMLHttpRequest::~nsXMLHttpRequest()
{
  mState |= XML_HTTP_REQUEST_DELETED;

  if (mState & (XML_HTTP_REQUEST_SENT |
                XML_HTTP_REQUEST_LOADING)) {
    Abort();
  }

  MOZ_ASSERT(!(mState & XML_HTTP_REQUEST_SYNCLOOPING), "we rather crash than hang");
  mState &= ~XML_HTTP_REQUEST_SYNCLOOPING;

  mResultJSON.setUndefined();
  mResultArrayBuffer = nullptr;
  mozilla::DropJSObjects(this);
}

void
nsXMLHttpRequest::RootJSResultObjects()
{
  mozilla::HoldJSObjects(this);
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

  // Instead of grabbing some random global from the context stack,
  // let's use the default one (junk scope) for now.
  // We should move away from this Init...
  Construct(subjectPrincipal, xpc::NativeGlobal(xpc::PrivilegedJunkScope()));
  return NS_OK;
}

/**
 * This Init method should only be called by C++ consumers.
 */
NS_IMETHODIMP
nsXMLHttpRequest::Init(nsIPrincipal* aPrincipal,
                       nsIScriptContext* aScriptContext,
                       nsIGlobalObject* aGlobalObject,
                       nsIURI* aBaseURI,
                       nsILoadGroup* aLoadGroup)
{
  NS_ENSURE_ARG_POINTER(aPrincipal);

  if (nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aGlobalObject)) {
    if (win->IsOuterWindow()) {
      // Must be bound to inner window, innerize if necessary.
      nsCOMPtr<nsIGlobalObject> inner = do_QueryInterface(
        win->GetCurrentInnerWindow());
      aGlobalObject = inner.get();
    }
  }

  Construct(aPrincipal, aGlobalObject, aBaseURI, aLoadGroup);
  return NS_OK;
}

void
nsXMLHttpRequest::InitParameters(bool aAnon, bool aSystem)
{
  if (!aAnon && !aSystem) {
    return;
  }

  // Check for permissions.
  // Chrome is always allowed access, so do the permission check only
  // for non-chrome pages.
  if (!IsSystemXHR() && aSystem) {
    nsIGlobalObject* global = GetOwnerGlobal();
    if (NS_WARN_IF(!global)) {
      SetParameters(aAnon, false);
      return;
    }

    nsIPrincipal* principal = global->PrincipalOrNull();
    if (NS_WARN_IF(!principal)) {
      SetParameters(aAnon, false);
      return;
    }

    nsCOMPtr<nsIPermissionManager> permMgr =
      services::GetPermissionManager();
    if (NS_WARN_IF(!permMgr)) {
      SetParameters(aAnon, false);
      return;
    }

    uint32_t permission;
    nsresult rv =
      permMgr->TestPermissionFromPrincipal(principal, "systemXHR", &permission);
    if (NS_FAILED(rv) || permission != nsIPermissionManager::ALLOW_ACTION) {
      SetParameters(aAnon, false);
      return;
    }
  }

  SetParameters(aAnon, aSystem);
}

void
nsXMLHttpRequest::ResetResponse()
{
  mResponseXML = nullptr;
  mResponseBody.Truncate();
  mResponseText.Truncate();
  mResponseBlob = nullptr;
  mDOMBlob = nullptr;
  mBlobSet = nullptr;
  mResultArrayBuffer = nullptr;
  mArrayBufferBuilder.reset();
  mResultJSON.setUndefined();
  mDataAvailable = 0;
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
      tmp->mListenerManager->MarkForCC();
    }
    if (!isBlack && tmp->PreservingWrapper()) {
      // This marks the wrapper black.
      tmp->GetWrapper();
    }
    return true;
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsXMLHttpRequest)
  return tmp->
    IsBlackAndDoesNotNeedTracing(static_cast<DOMEventTargetHelper*>(tmp));
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsXMLHttpRequest)
  return tmp->IsBlack();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsXMLHttpRequest,
                                                  nsXHREventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChannel)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResponseXML)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mXMLParserStreamListener)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResponseBlob)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDOMBlob)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNotificationCallbacks)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChannelEventSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mProgressEventSink)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mUpload)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsXMLHttpRequest,
                                                nsXHREventTarget)
  tmp->mResultArrayBuffer = nullptr;
  tmp->mArrayBufferBuilder.reset();
  tmp->mResultJSON.setUndefined();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChannel)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mResponseXML)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mXMLParserStreamListener)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mResponseBlob)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDOMBlob)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNotificationCallbacks)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChannelEventSink)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mProgressEventSink)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mUpload)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(nsXMLHttpRequest,
                                               nsXHREventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mResultArrayBuffer)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mResultJSON)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

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
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsISizeOfEventTarget)
NS_INTERFACE_MAP_END_INHERITING(nsXHREventTarget)

NS_IMPL_ADDREF_INHERITED(nsXMLHttpRequest, nsXHREventTarget)
NS_IMPL_RELEASE_INHERITED(nsXMLHttpRequest, nsXHREventTarget)

NS_IMPL_EVENT_HANDLER(nsXMLHttpRequest, readystatechange)

void
nsXMLHttpRequest::DisconnectFromOwner()
{
  nsXHREventTarget::DisconnectFromOwner();
  Abort();
}

size_t
nsXMLHttpRequest::SizeOfEventTargetIncludingThis(
  MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += mResponseBody.SizeOfExcludingThisIfUnshared(aMallocSizeOf);

  // Why is this safe?  Because no-one else will report this string.  The
  // other possible sharers of this string are as follows.
  //
  // - The JS engine could hold copies if the JS code holds references, e.g.
  //   |var text = XHR.responseText|.  However, those references will be via JS
  //   external strings, for which the JS memory reporter does *not* report the
  //   chars.
  //
  // - Binary extensions, but they're *extremely* unlikely to do any memory
  //   reporting.
  //
  n += mResponseText.SizeOfExcludingThisEvenIfShared(aMallocSizeOf);

  return n;

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - lots
}

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
    doc = aWindow->GetExtantDoc();
  }
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("DOM"), doc,
                                  nsContentUtils::eDOM_PROPERTIES,
                                  aWarning);
}

NS_IMETHODIMP
nsXMLHttpRequest::GetResponseXML(nsIDOMDocument **aResponseXML)
{
  ErrorResult rv;
  nsIDocument* responseXML = GetResponseXML(rv);
  if (rv.Failed()) {
    return rv.StealNSResult();
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
  if (mWarnAboutSyncHtml) {
    mWarnAboutSyncHtml = false;
    LogMessage("HTMLSyncXHRWarning", GetOwner());
  }
  return (XML_HTTP_REQUEST_DONE & mState) ? mResponseXML : nullptr;
}

/*
 * This piece copied from XMLDocument, we try to get the charset
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

  nsAutoCString charsetVal;
  bool ok = mChannel &&
            NS_SUCCEEDED(mChannel->GetContentCharset(charsetVal)) &&
            EncodingUtils::FindEncodingForLabel(charsetVal, mResponseCharset);
  if (!ok || mResponseCharset.IsEmpty()) {
    // MS documentation states UTF-8 is default for responseText
    mResponseCharset.AssignLiteral("UTF-8");
  }

  if (mResponseType == XML_HTTP_RESPONSE_TYPE_JSON &&
      !mResponseCharset.EqualsLiteral("UTF-8")) {
    // The XHR spec says only UTF-8 is supported for responseType == "json"
    LogMessage("JSONCharsetWarning", GetOwner());
    mResponseCharset.AssignLiteral("UTF-8");
  }

  mDecoder = EncodingUtils::DecoderForEncoding(mResponseCharset);

  return NS_OK;
}

nsresult
nsXMLHttpRequest::AppendToResponseText(const char * aSrcBuffer,
                                       uint32_t aSrcBufferLen)
{
  NS_ENSURE_STATE(mDecoder);

  int32_t destBufferLen;
  nsresult rv = mDecoder->GetMaxLength(aSrcBuffer, aSrcBufferLen,
                                       &destBufferLen);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t size = mResponseText.Length() + destBufferLen;
  if (size < (uint32_t)destBufferLen) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!mResponseText.SetCapacity(size, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  char16_t* destBuffer = mResponseText.BeginWriting() + mResponseText.Length();

  CheckedInt32 totalChars = mResponseText.Length();

  // This code here is basically a copy of a similar thing in
  // nsScanner::Append(const char* aBuffer, uint32_t aLen).
  int32_t srclen = (int32_t)aSrcBufferLen;
  int32_t destlen = (int32_t)destBufferLen;
  rv = mDecoder->Convert(aSrcBuffer,
                         &srclen,
                         destBuffer,
                         &destlen);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  totalChars += destlen;
  if (!totalChars.isValid()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mResponseText.SetLength(totalChars.value());
  return NS_OK;
}

NS_IMETHODIMP
nsXMLHttpRequest::GetResponseText(nsAString& aResponseText)
{
  ErrorResult rv;
  nsString responseText;
  GetResponseText(responseText, rv);
  aResponseText = responseText;
  return rv.StealNSResult();
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
    mDecoder = EncodingUtils::DecoderForEncoding(mResponseCharset);
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
  JS::Rooted<JS::Value> value(aCx);
  if (!JS_ParseJSON(aCx,
                    static_cast<const char16_t*>(mResponseText.get()), mResponseText.Length(),
                    &value)) {
    return NS_ERROR_FAILURE;
  }

  mResultJSON = value;
  return NS_OK;
}

void
nsXMLHttpRequest::CreatePartialBlob()
{
  if (mDOMBlob) {
    // Use progress info to determine whether load is complete, but use
    // mDataAvailable to ensure a slice is created based on the uncompressed
    // data count.
    if (mLoadTotal == mLoadTransferred) {
      mResponseBlob = mDOMBlob;
    } else {
      ErrorResult rv;
      mResponseBlob = mDOMBlob->CreateSlice(0, mDataAvailable,
                                            EmptyString(), rv);
    }
    return;
  }

  // mBlobSet can be null if the request has been canceled
  if (!mBlobSet) {
    return;
  }

  nsAutoCString contentType;
  if (mLoadTotal == mLoadTransferred) {
    mChannel->GetContentType(contentType);
  }

  mResponseBlob = mBlobSet->GetBlobInternal(GetOwner(), contentType);
}

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
  static_assert(\
    static_cast<int>(XMLHttpRequestResponseType::_lc)  \
    == XML_HTTP_RESPONSE_TYPE_ ## _uc, \
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

NS_IMETHODIMP nsXMLHttpRequest::SetResponseType(const nsAString& aResponseType)
{
  nsXMLHttpRequest::ResponseTypeEnum responseType;
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
  return rv.StealNSResult();
}

void
nsXMLHttpRequest::SetResponseType(XMLHttpRequestResponseType aType,
                                  ErrorResult& aRv)
{
  SetResponseType(ResponseTypeEnum(static_cast<int>(aType)), aRv);
}

void
nsXMLHttpRequest::SetResponseType(nsXMLHttpRequest::ResponseTypeEnum aResponseType,
                                  ErrorResult& aRv)
{
  // If the state is LOADING or DONE raise an INVALID_STATE_ERR exception
  // and terminate these steps.
  if ((mState & (XML_HTTP_REQUEST_LOADING | XML_HTTP_REQUEST_DONE))) {
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

}

NS_IMETHODIMP
nsXMLHttpRequest::GetResponse(JSContext *aCx, JS::MutableHandle<JS::Value> aResult)
{
  ErrorResult rv;
  GetResponse(aCx, aResult, rv);
  return rv.StealNSResult();
}

void
nsXMLHttpRequest::GetResponse(JSContext* aCx,
                              JS::MutableHandle<JS::Value> aResponse,
                              ErrorResult& aRv)
{
  switch (mResponseType) {
  case XML_HTTP_RESPONSE_TYPE_DEFAULT:
  case XML_HTTP_RESPONSE_TYPE_TEXT:
  case XML_HTTP_RESPONSE_TYPE_CHUNKED_TEXT:
  {
    nsString str;
    aRv = GetResponseText(str);
    if (aRv.Failed()) {
      return;
    }
    if (!xpc::StringToJsval(aCx, str, aResponse)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    }
    return;
  }

  case XML_HTTP_RESPONSE_TYPE_ARRAYBUFFER:
  case XML_HTTP_RESPONSE_TYPE_CHUNKED_ARRAYBUFFER:
  {
    if (!(mResponseType == XML_HTTP_RESPONSE_TYPE_ARRAYBUFFER &&
          mState & XML_HTTP_REQUEST_DONE) &&
        !(mResponseType == XML_HTTP_RESPONSE_TYPE_CHUNKED_ARRAYBUFFER &&
          mInLoadProgressEvent)) {
      aResponse.setNull();
      return;
    }

    if (!mResultArrayBuffer) {
      RootJSResultObjects();

      mResultArrayBuffer = mArrayBufferBuilder.getArrayBuffer(aCx);
      if (!mResultArrayBuffer) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return;
      }
    }
    JS::ExposeObjectToActiveJS(mResultArrayBuffer);
    aResponse.setObject(*mResultArrayBuffer);
    return;
  }
  case XML_HTTP_RESPONSE_TYPE_BLOB:
  case XML_HTTP_RESPONSE_TYPE_MOZ_BLOB:
  {
    if (!(mState & XML_HTTP_REQUEST_DONE)) {
      if (mResponseType != XML_HTTP_RESPONSE_TYPE_MOZ_BLOB) {
        aResponse.setNull();
        return;
      }

      if (!mResponseBlob) {
        CreatePartialBlob();
      }
    }

    if (!mResponseBlob) {
      aResponse.setNull();
      return;
    }

    GetOrCreateDOMReflector(aCx, mResponseBlob, aResponse);
    return;
  }
  case XML_HTTP_RESPONSE_TYPE_DOCUMENT:
  {
    if (!(mState & XML_HTTP_REQUEST_DONE) || !mResponseXML) {
      aResponse.setNull();
      return;
    }

    aRv = nsContentUtils::WrapNative(aCx, mResponseXML, aResponse);
    return;
  }
  case XML_HTTP_RESPONSE_TYPE_JSON:
  {
    if (!(mState & XML_HTTP_REQUEST_DONE)) {
      aResponse.setNull();
      return;
    }

    if (mResultJSON.isUndefined()) {
      aRv = CreateResponseParsedJSON(aCx);
      mResponseText.Truncate();
      if (aRv.Failed()) {
        // Per spec, errors aren't propagated. null is returned instead.
        aRv = NS_OK;
        // It would be nice to log the error to the console. That's hard to
        // do without calling window.onerror as a side effect, though.
        JS_ClearPendingException(aCx);
        mResultJSON.setNull();
      }
    }
    JS::ExposeValueToActiveJS(mResultJSON);
    aResponse.set(mResultJSON);
    return;
  }
  default:
    NS_ERROR("Should not happen");
  }

  aResponse.setNull();
}

bool
nsXMLHttpRequest::IsDeniedCrossSiteRequest()
{
  if ((mState & XML_HTTP_REQUEST_USE_XSITE_AC) && mChannel) {
    nsresult rv;
    mChannel->GetStatus(&rv);
    if (NS_FAILED(rv)) {
      return true;
    }
  }
  return false;
}

void
nsXMLHttpRequest::GetResponseURL(nsAString& aUrl)
{
  aUrl.Truncate();

  uint16_t readyState = ReadyState();
  if ((readyState == UNSENT || readyState == OPENED) || !mChannel) {
    return;
  }

  // Make sure we don't leak responseURL information from denied cross-site
  // requests.
  if (IsDeniedCrossSiteRequest()) {
    return;
  }

  nsCOMPtr<nsIURI> responseUrl;
  mChannel->GetURI(getter_AddRefs(responseUrl));

  if (!responseUrl) {
    return;
  }

  nsAutoCString temp;
  responseUrl->GetSpecIgnoringRef(temp);
  CopyUTF8toUTF16(temp, aUrl);
}

NS_IMETHODIMP
nsXMLHttpRequest::GetStatus(uint32_t *aStatus)
{
  *aStatus = Status();
  return NS_OK;
}

uint32_t
nsXMLHttpRequest::Status()
{
  // Make sure we don't leak status information from denied cross-site
  // requests.
  if (IsDeniedCrossSiteRequest()) {
    return 0;
  }

  uint16_t readyState = ReadyState();
  if (readyState == UNSENT || readyState == OPENED) {
    return 0;
  }

  if (mErrorLoad) {
    // Let's simulate the http protocol for jar/app requests:
    nsCOMPtr<nsIJARChannel> jarChannel = GetCurrentJARChannel();
    if (jarChannel) {
      nsresult status;
      mChannel->GetStatus(&status);

      if (status == NS_ERROR_FILE_NOT_FOUND) {
        return 404; // Not Found
      } else {
        return 500; // Internal Error
      }
    }

    return 0;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = GetCurrentHttpChannel();
  if (!httpChannel) {
    // Pretend like we got a 200 response, since our load was successful
    return 200;
  }

  uint32_t status;
  nsresult rv = httpChannel->GetResponseStatus(&status);
  if (NS_FAILED(rv)) {
    status = 0;
  }

  return status;
}

IMPL_CSTRING_GETTER(GetStatusText)
void
nsXMLHttpRequest::GetStatusText(nsCString& aStatusText)
{
  // Return an empty status text on all error loads.
  aStatusText.Truncate();

  // Make sure we don't leak status information from denied cross-site
  // requests.
  if (IsDeniedCrossSiteRequest()) {
    return;
  }

  // Check the current XHR state to see if it is valid to obtain the statusText
  // value.  This check is to prevent the status text for redirects from being
  // available before all the redirects have been followed and HTTP headers have
  // been received.
  uint16_t readyState = ReadyState();
  if (readyState == UNSENT || readyState == OPENED) {
    return;
  }

  if (mErrorLoad) {
    return;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = GetCurrentHttpChannel();
  if (httpChannel) {
    httpChannel->GetResponseStatusText(aStatusText);
  } else {
    aStatusText.AssignLiteral("OK");
  }
}

void
nsXMLHttpRequest::CloseRequestWithError(const nsAString& aType,
                                        const uint32_t aFlag)
{
  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
  }
  if (mTimeoutTimer) {
    mTimeoutTimer->Cancel();
  }
  uint32_t responseLength = mResponseBody.Length();
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

/*Method that checks if it is safe to expose a header value to the client.
It is used to check what headers are exposed for CORS requests.*/
bool
nsXMLHttpRequest::IsSafeHeader(const nsACString& header, nsIHttpChannel* httpChannel)
{
  // See bug #380418. Hide "Set-Cookie" headers from non-chrome scripts.
  if (!IsSystemXHR() && nsContentUtils::IsForbiddenResponseHeader(header)) {
    NS_WARNING("blocked access to response header");
    return false;
  }
  // if this is not a CORS call all headers are safe
  if (!(mState & XML_HTTP_REQUEST_USE_XSITE_AC)){
    return true;
  }
  // Check for dangerous headers
  // Make sure we don't leak header information from denied cross-site
  // requests.
  if (mChannel) {
    nsresult status;
    mChannel->GetStatus(&status);
    if (NS_FAILED(status)) {
      return false;
    }
  }  
  const char* kCrossOriginSafeHeaders[] = {
    "cache-control", "content-language", "content-type", "expires",
    "last-modified", "pragma"
  };
  for (uint32_t i = 0; i < ArrayLength(kCrossOriginSafeHeaders); ++i) {
    if (header.LowerCaseEqualsASCII(kCrossOriginSafeHeaders[i])) {
      return true;
    }
  }
  nsAutoCString headerVal;
  // The "Access-Control-Expose-Headers" header contains a comma separated
  // list of method names.
  httpChannel->
      GetResponseHeader(NS_LITERAL_CSTRING("Access-Control-Expose-Headers"),
                        headerVal);
  nsCCharSeparatedTokenizer exposeTokens(headerVal, ',');
  bool isSafe = false;
  while (exposeTokens.hasMoreTokens()) {
    const nsDependentCSubstring& token = exposeTokens.nextToken();
    if (token.IsEmpty()) {
      continue;
    }
    if (!NS_IsValidHTTPToken(token)) {
      return false;
    }
    if (header.Equals(token, nsCaseInsensitiveCStringComparator())) {
      isSafe = true;
    }
  }
  return isSafe;
}

IMPL_CSTRING_GETTER(GetAllResponseHeaders)
void
nsXMLHttpRequest::GetAllResponseHeaders(nsCString& aResponseHeaders)
{
  aResponseHeaders.Truncate();

  // If the state is UNSENT or OPENED,
  // return the empty string and terminate these steps.
  if (mState & (XML_HTTP_REQUEST_UNSENT |
                XML_HTTP_REQUEST_OPENED | XML_HTTP_REQUEST_SENT)) {
    return;
  }

  if (nsCOMPtr<nsIHttpChannel> httpChannel = GetCurrentHttpChannel()) {
    nsRefPtr<nsHeaderVisitor> visitor = new nsHeaderVisitor(this, httpChannel);
    if (NS_SUCCEEDED(httpChannel->VisitResponseHeaders(visitor))) {
      aResponseHeaders = visitor->Headers();
    }
    return;
  }

  if (!mChannel) {
    return;
  }

  // Even non-http channels supply content type.
  nsAutoCString value;
  if (NS_SUCCEEDED(mChannel->GetContentType(value))) {
    aResponseHeaders.AppendLiteral("Content-Type: ");
    aResponseHeaders.Append(value);
    if (NS_SUCCEEDED(mChannel->GetContentCharset(value)) && !value.IsEmpty()) {
      aResponseHeaders.AppendLiteral(";charset=");
      aResponseHeaders.Append(value);
    }
    aResponseHeaders.AppendLiteral("\r\n");
  }

  int64_t length;
  if (NS_SUCCEEDED(mChannel->GetContentLength(&length))) {
    aResponseHeaders.AppendLiteral("Content-Length: ");
    aResponseHeaders.AppendInt(length);
    aResponseHeaders.AppendLiteral("\r\n");
  }
}

NS_IMETHODIMP
nsXMLHttpRequest::GetResponseHeader(const nsACString& aHeader,
                                    nsACString& aResult)
{
  ErrorResult rv;
  GetResponseHeader(aHeader, aResult, rv);
  return rv.StealNSResult();
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

    // Even non-http channels supply content type and content length.
    // Remember we don't leak header information from denied cross-site
    // requests.
    nsresult status;
    if (!mChannel ||
        NS_FAILED(mChannel->GetStatus(&status)) ||
        NS_FAILED(status)) {
      return;
    }

    // Content Type:
    if (header.LowerCaseEqualsASCII("content-type")) {
      if (NS_FAILED(mChannel->GetContentType(_retval))) {
        // Means no content type
        _retval.SetIsVoid(true);
        return;
      }

      nsCString value;
      if (NS_SUCCEEDED(mChannel->GetContentCharset(value)) &&
          !value.IsEmpty()) {
        _retval.AppendLiteral(";charset=");
        _retval.Append(value);
      }
    }

    // Content Length:
    else if (header.LowerCaseEqualsASCII("content-length")) {
      int64_t length;
      if (NS_SUCCEEDED(mChannel->GetContentLength(&length))) {
        _retval.AppendInt(length);
      }
    }

    return;
  }

  // Check for dangerous headers
  if (!IsSafeHeader(header, httpChannel)) {
    return;
  }

  aRv = httpChannel->GetResponseHeader(header, _retval);
  if (aRv.ErrorCodeIs(NS_ERROR_NOT_AVAILABLE)) {
    // Means no header
    _retval.SetIsVoid(true);
    aRv.SuppressException();
  }
}

already_AddRefed<nsILoadGroup>
nsXMLHttpRequest::GetLoadGroup() const
{
  if (mState & XML_HTTP_REQUEST_BACKGROUND) {                 
    return nullptr;
  }

  if (mLoadGroup) {
    nsCOMPtr<nsILoadGroup> ref = mLoadGroup;
    return ref.forget();
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
  nsRefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);
  event.forget(aDOMEvent);

  (*aDOMEvent)->InitEvent(NS_LITERAL_STRING(READYSTATE_STR),
                          false, false);

  // We assume anyone who managed to call CreateReadystatechangeEvent is trusted
  (*aDOMEvent)->SetTrusted(true);

  return NS_OK;
}

void
nsXMLHttpRequest::DispatchProgressEvent(DOMEventTargetHelper* aTarget,
                                        const nsAString& aType,
                                        bool aLengthComputable,
                                        int64_t aLoaded, int64_t aTotal)
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

  ProgressEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mLengthComputable = aLengthComputable;
  init.mLoaded = aLoaded;
  init.mTotal = (aTotal == -1) ? 0 : aTotal;

  nsRefPtr<ProgressEvent> event =
    ProgressEvent::Constructor(aTarget, aType, init);
  event->SetTrusted(true);

  aTarget->DispatchDOMEvent(nullptr, event, nullptr, nullptr);

  if (dispatchLoadend) {
    DispatchProgressEvent(aTarget, NS_LITERAL_STRING(LOADEND_STR),
                          aLengthComputable, aLoaded, aTotal);
  }
}
                                          
already_AddRefed<nsIHttpChannel>
nsXMLHttpRequest::GetCurrentHttpChannel()
{
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
  return httpChannel.forget();
}

already_AddRefed<nsIJARChannel>
nsXMLHttpRequest::GetCurrentJARChannel()
{
  nsCOMPtr<nsIJARChannel> appChannel = do_QueryInterface(mChannel);
  return appChannel.forget();
}

bool
nsXMLHttpRequest::IsSystemXHR()
{
  return mIsSystem || nsContentUtils::IsSystemPrincipal(mPrincipal);
}

nsresult
nsXMLHttpRequest::CheckChannelForCrossSiteRequest(nsIChannel* aChannel)
{
  // A system XHR (chrome code or a web app with the right permission) can
  // always perform cross-site requests. In the web app case, however, we
  // must still check for protected URIs like file:///.
  if (IsSystemXHR()) {
    if (!nsContentUtils::IsSystemPrincipal(mPrincipal)) {
      nsIScriptSecurityManager *secMan = nsContentUtils::GetSecurityManager();
      nsCOMPtr<nsIURI> uri;
      aChannel->GetOriginalURI(getter_AddRefs(uri));
      return secMan->CheckLoadURIWithPrincipal(
        mPrincipal, uri, nsIScriptSecurityManager::STANDARD);
    }
    return NS_OK;
  }

  // If this is a same-origin request or the channel's URI inherits
  // its principal, it's allowed.
  if (nsContentUtils::CheckMayLoad(mPrincipal, aChannel, true)) {
    return NS_OK;
  }

  // This is a cross-site request
  mState |= XML_HTTP_REQUEST_USE_XSITE_AC;

  // Check if we need to do a preflight request.
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  NS_ENSURE_TRUE(httpChannel, NS_ERROR_DOM_BAD_URI);

  nsAutoCString method;
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
                       const nsAString& password, uint8_t optional_argc)
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
nsXMLHttpRequest::Open(const nsACString& inMethod, const nsACString& url,
                       bool async, const Optional<nsAString>& user,
                       const Optional<nsAString>& password)
{
  NS_ENSURE_ARG(!inMethod.IsEmpty());

  if (!async && !DontWarnAboutSyncXHR() && GetOwner() &&
      GetOwner()->GetExtantDoc()) {
    GetOwner()->GetExtantDoc()->WarnOnceAbout(nsIDocument::eSyncXMLHttpRequest);
  }

  Telemetry::Accumulate(Telemetry::XMLHTTPREQUEST_ASYNC_OR_SYNC,
                        async ? 0 : 1);

  NS_ENSURE_TRUE(mPrincipal, NS_ERROR_NOT_INITIALIZED);

  nsAutoCString method;
  nsresult rv = FetchUtil::GetValidRequestMethod(inMethod, method);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
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

  nsCOMPtr<nsIURI> uri;

  if (mState & (XML_HTTP_REQUEST_OPENED |
                XML_HTTP_REQUEST_HEADERS_RECEIVED |
                XML_HTTP_REQUEST_LOADING |
                XML_HTTP_REQUEST_SENT)) {
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
  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST,
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
    nsAutoCString userpass;
    CopyUTF16toUTF8(user.Value(), userpass);
    if (password.WasPassed() && !password.Value().IsEmpty()) {
      userpass.Append(':');
      AppendUTF16toUTF8(password.Value(), userpass);
    }
    uri->SetUserPass(userpass);
  }

  // Clear our record of previously set headers so future header set
  // operations will merge/override correctly.
  mAlreadySetHeaders.Clear();

  // When we are called from JS we can find the load group for the page,
  // and add ourselves to it. This way any pending requests
  // will be automatically aborted if the user leaves the page.
  nsCOMPtr<nsILoadGroup> loadGroup = GetLoadGroup();

  nsSecurityFlags secFlags = nsILoadInfo::SEC_NORMAL;
  nsLoadFlags loadFlags = nsIRequest::LOAD_BACKGROUND;
  if (IsSystemXHR()) {
    // Don't give this document the system principal.  We need to keep track of
    // mPrincipal being system because we use it for various security checks
    // that should be passing, but the document data shouldn't get a system
    // principal.  Hence we set the sandbox flag in loadinfo, so that 
    // GetChannelResultPrincipal will give us the nullprincipal.
    secFlags |= nsILoadInfo::SEC_SANDBOXED;

    //For a XHR, disable interception
    loadFlags |= nsIChannel::LOAD_BYPASS_SERVICE_WORKER;
  } else {
    secFlags |= nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL;
  }

  // If we have the document, use it
  if (doc) {
    rv = NS_NewChannel(getter_AddRefs(mChannel),
                       uri,
                       doc,
                       secFlags,
                       nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST,
                       loadGroup,
                       nullptr,   // aCallbacks
                       loadFlags);
  } else {
    //otherwise use the principal
    rv = NS_NewChannel(getter_AddRefs(mChannel),
                       uri,
                       mPrincipal,
                       secFlags,
                       nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST,
                       loadGroup,
                       nullptr,   // aCallbacks
                       loadFlags);
  }

  if (NS_FAILED(rv)) return rv;

  mState &= ~(XML_HTTP_REQUEST_USE_XSITE_AC |
              XML_HTTP_REQUEST_NEED_AC_PREFLIGHT);

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));
  if (httpChannel) {
    rv = httpChannel->SetRequestMethod(method);
    NS_ENSURE_SUCCESS(rv, rv);

    // Set the initiator type
    nsCOMPtr<nsITimedChannel> timedChannel(do_QueryInterface(httpChannel));
    if (timedChannel) {
      timedChannel->SetInitiatorType(NS_LITERAL_STRING("xmlhttprequest"));
    }
  }

  ChangeState(XML_HTTP_REQUEST_OPENED);

  return rv;
}

void
nsXMLHttpRequest::PopulateNetworkInterfaceId()
{
  if (mNetworkInterfaceId.IsEmpty()) {
    return;
  }
  nsCOMPtr<nsIHttpChannelInternal> channel(do_QueryInterface(mChannel));
  if (!channel) {
    return;
  }
  channel->SetNetworkInterfaceId(mNetworkInterfaceId);
}

/*
 * "Copy" from a stream.
 */
NS_METHOD
nsXMLHttpRequest::StreamReaderFunc(nsIInputStream* in,
                                   void* closure,
                                   const char* fromRawSegment,
                                   uint32_t toOffset,
                                   uint32_t count,
                                   uint32_t *writeCount)
{
  nsXMLHttpRequest* xmlHttpRequest = static_cast<nsXMLHttpRequest*>(closure);
  if (!xmlHttpRequest || !writeCount) {
    NS_WARNING("XMLHttpRequest cannot read from stream: no closure or writeCount");
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_OK;

  if (xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_BLOB ||
      xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_MOZ_BLOB) {
    if (!xmlHttpRequest->mDOMBlob) {
      if (!xmlHttpRequest->mBlobSet) {
        xmlHttpRequest->mBlobSet = new BlobSet();
      }
      rv = xmlHttpRequest->mBlobSet->AppendVoidPtr(fromRawSegment, count);
    }
    // Clear the cache so that the blob size is updated.
    if (xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_MOZ_BLOB) {
      xmlHttpRequest->mResponseBlob = nullptr;
    }
  } else if ((xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_ARRAYBUFFER &&
              !xmlHttpRequest->mIsMappedArrayBuffer) ||
             xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_CHUNKED_ARRAYBUFFER) {
    // get the initial capacity to something reasonable to avoid a bunch of reallocs right
    // at the start
    if (xmlHttpRequest->mArrayBufferBuilder.capacity() == 0)
      xmlHttpRequest->mArrayBufferBuilder.setCapacity(PR_MAX(count, XML_HTTP_REQUEST_ARRAYBUFFER_MIN_SIZE));

    xmlHttpRequest->mArrayBufferBuilder.append(reinterpret_cast<const uint8_t*>(fromRawSegment), count,
                                               XML_HTTP_REQUEST_ARRAYBUFFER_MAX_GROWTH);
  } else if (xmlHttpRequest->mResponseType == XML_HTTP_RESPONSE_TYPE_DEFAULT &&
             xmlHttpRequest->mResponseXML) {
    // Copy for our own use
    if (!xmlHttpRequest->mResponseBody.Append(fromRawSegment, count, fallible)) {
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
                                  ->OnDataAvailable(xmlHttpRequest->mChannel,
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

bool nsXMLHttpRequest::CreateDOMBlob(nsIRequest *request)
{
  nsCOMPtr<nsIFile> file;
  nsCOMPtr<nsIFileChannel> fc = do_QueryInterface(request);
  if (fc) {
    fc->GetFile(getter_AddRefs(file));
  }

  if (!file)
    return false;

  nsAutoCString contentType;
  mChannel->GetContentType(contentType);

  mDOMBlob = File::CreateFromFile(GetOwner(), file, EmptyString(),
                                  NS_ConvertASCIItoUTF16(contentType));

  mBlobSet = nullptr;
  NS_ASSERTION(mResponseBody.IsEmpty(), "mResponseBody should be empty");
  return true;
}

NS_IMETHODIMP
nsXMLHttpRequest::OnDataAvailable(nsIRequest *request,
                                  nsISupports *ctxt,
                                  nsIInputStream *inStr,
                                  uint64_t sourceOffset,
                                  uint32_t count)
{
  NS_ENSURE_ARG_POINTER(inStr);

  MOZ_ASSERT(mContext.get() == ctxt,"start context different from OnDataAvailable context");

  mProgressSinceLastProgressEvent = true;

  bool cancelable = false;
  if ((mResponseType == XML_HTTP_RESPONSE_TYPE_BLOB ||
       mResponseType == XML_HTTP_RESPONSE_TYPE_MOZ_BLOB) && !mDOMBlob) {
    cancelable = CreateDOMBlob(request);
    // The nsIStreamListener contract mandates us
    // to read from the stream before returning.
  }

  uint32_t totalRead;
  nsresult rv = inStr->ReadSegments(nsXMLHttpRequest::StreamReaderFunc,
                                    (void*)this, count, &totalRead);
  NS_ENSURE_SUCCESS(rv, rv);

  if (cancelable) {
    // We don't have to read from the local file for the blob response
    ErrorResult error;
    mDataAvailable = mDOMBlob->GetSize(error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }

    ChangeState(XML_HTTP_REQUEST_LOADING);
    return request->Cancel(NS_OK);
  }

  mDataAvailable += totalRead;

  ChangeState(XML_HTTP_REQUEST_LOADING);
  
  MaybeDispatchProgressEvents(false);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLHttpRequest::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  PROFILER_LABEL("nsXMLHttpRequest", "OnStartRequest",
    js::ProfileEntry::Category::NETWORK);

  nsresult rv = NS_OK;
  if (!mFirstStartRequestSeen && mRequestObserver) {
    mFirstStartRequestSeen = true;
    mRequestObserver->OnStartRequest(request, ctxt);
  }

  if (request != mChannel) {
    // Can this still happen?
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

  nsresult status;
  request->GetStatus(&status);
  mErrorLoad = mErrorLoad || NS_FAILED(status);

  if (mUpload && !mUploadComplete && !mErrorLoad &&
      (mState & XML_HTTP_REQUEST_ASYNC)) {
    if (mProgressTimerIsActive) {
      mProgressTimerIsActive = false;
      mProgressNotifier->Cancel();
    }
    if (mUploadTransferred < mUploadTotal) {
      mUploadTransferred = mUploadTotal;
      mProgressSinceLastProgressEvent = true;
      mUploadLengthComputable = true;
      MaybeDispatchProgressEvents(true);
    }
    mUploadComplete = true;
    DispatchProgressEvent(mUpload, NS_LITERAL_STRING(LOAD_STR),
                          true, mUploadTotal, mUploadTotal);
  }

  mContext = ctxt;
  mState |= XML_HTTP_REQUEST_PARSEBODY;
  ChangeState(XML_HTTP_REQUEST_HEADERS_RECEIVED);

  ResetResponse();

  if (!mOverrideMimeType.IsEmpty()) {
    channel->SetContentType(NS_ConvertUTF16toUTF8(mOverrideMimeType));
  }

  DetectCharset();

  // Set up arraybuffer
  if (mResponseType == XML_HTTP_RESPONSE_TYPE_ARRAYBUFFER && NS_SUCCEEDED(status)) {
    if (mIsMappedArrayBuffer) {
      nsCOMPtr<nsIJARChannel> jarChannel = do_QueryInterface(channel);
      if (jarChannel) {
        nsCOMPtr<nsIURI> uri;
        rv = channel->GetURI(getter_AddRefs(uri));
        if (NS_SUCCEEDED(rv)) {
          nsAutoCString file;
          nsAutoCString scheme;
          uri->GetScheme(scheme);
          if (scheme.LowerCaseEqualsLiteral("app")) {
            uri->GetPath(file);
            // The actual file inside zip package has no leading slash.
            file.Trim("/", true, false, false);
          } else if (scheme.LowerCaseEqualsLiteral("jar")) {
            nsCOMPtr<nsIJARURI> jarURI = do_QueryInterface(uri);
            if (jarURI) {
              jarURI->GetJAREntry(file);
            }
          }
          nsCOMPtr<nsIFile> jarFile;
          jarChannel->GetJarFile(getter_AddRefs(jarFile));
          if (!jarFile) {
            mIsMappedArrayBuffer = false;
          } else {
            rv = mArrayBufferBuilder.mapToFileInPackage(file, jarFile);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              mIsMappedArrayBuffer = false;
            } else {
              channel->SetContentType(NS_LITERAL_CSTRING("application/mem-mapped"));
            }
          }
        }
      }
    }
    // If memory mapping failed, mIsMappedArrayBuffer would be set to false,
    // and we want it fallback to the malloc way.
    if (!mIsMappedArrayBuffer) {
      int64_t contentLength;
      rv = channel->GetContentLength(&contentLength);
      if (NS_SUCCEEDED(rv) &&
          contentLength > 0 &&
          contentLength < XML_HTTP_REQUEST_MAX_CONTENT_LENGTH_PREALLOCATE) {
        mArrayBufferBuilder.setCapacity(static_cast<int32_t>(contentLength));
      }
    }
  }

  // Set up responseXML
  bool parseBody = mResponseType == XML_HTTP_RESPONSE_TYPE_DEFAULT ||
                     mResponseType == XML_HTTP_RESPONSE_TYPE_DOCUMENT;
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));
  if (parseBody && httpChannel) {
    nsAutoCString method;
    httpChannel->GetRequestMethod(method);
    parseBody = !method.EqualsLiteral("HEAD");
  }

  mIsHtml = false;
  mWarnAboutSyncHtml = false;
  if (parseBody && NS_SUCCEEDED(status)) {
    // We can gain a huge performance win by not even trying to
    // parse non-XML data. This also protects us from the situation
    // where we have an XML document and sink, but HTML (or other)
    // parser, which can produce unreliable results.
    nsAutoCString type;
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
    rv = mChannel->GetURI(getter_AddRefs(docURI));
    NS_ENSURE_SUCCESS(rv, rv);
    baseURI = docURI;

    nsIScriptContext* sc = GetContextForEventHandlers(&rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIDocument> doc =
      nsContentUtils::GetDocumentFromScriptContext(sc);
    nsCOMPtr<nsIURI> chromeXHRDocURI, chromeXHRDocBaseURI;
    if (doc) {
      chromeXHRDocURI = doc->GetDocumentURI();
      chromeXHRDocBaseURI = doc->GetBaseURI();
    }

    // Create an empty document from it.
    const nsAString& emptyStr = EmptyString();
    nsCOMPtr<nsIDOMDocument> responseDoc;
    nsIGlobalObject* global = DOMEventTargetHelper::GetParentObject();

    nsCOMPtr<nsIPrincipal> requestingPrincipal;
    rv = nsContentUtils::GetSecurityManager()->
       GetChannelResultPrincipal(channel, getter_AddRefs(requestingPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_NewDOMDocument(getter_AddRefs(responseDoc),
                           emptyStr, emptyStr, nullptr, docURI,
                           baseURI, requestingPrincipal, true, global,
                           mIsHtml ? DocumentFlavorHTML :
                                     DocumentFlavorLegacyGuess);
    NS_ENSURE_SUCCESS(rv, rv);
    mResponseXML = do_QueryInterface(responseDoc);
    mResponseXML->SetChromeXHRDocURI(chromeXHRDocURI);
    mResponseXML->SetChromeXHRDocBaseURI(chromeXHRDocBaseURI);

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
      HasListenersFor(nsGkAtoms::onprogress)) {
    StartProgressEventTimer();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXMLHttpRequest::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult status)
{
  PROFILER_LABEL("nsXMLHttpRequest", "OnStopRequest",
    js::ProfileEntry::Category::NETWORK);

  if (request != mChannel) {
    // Can this still happen?
    return NS_OK;
  }

  mWaitingForOnStopRequest = false;

  if (mRequestObserver) {
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
    if (!mDOMBlob) {
      CreateDOMBlob(request);
    }
    if (mDOMBlob) {
      mResponseBlob = mDOMBlob;
      mDOMBlob = nullptr;
    } else {
      // mBlobSet can be null if the channel is non-file non-cacheable
      // and if the response length is zero.
      if (!mBlobSet) {
        mBlobSet = new BlobSet();
      }
      // Smaller files may be written in cache map instead of separate files.
      // Also, no-store response cannot be written in persistent cache.
      nsAutoCString contentType;
      mChannel->GetContentType(contentType);
      mResponseBlob = mBlobSet->GetBlobInternal(GetOwner(), contentType);
      mBlobSet = nullptr;
    }
    NS_ASSERTION(mResponseBody.IsEmpty(), "mResponseBody should be empty");
    NS_ASSERTION(mResponseText.IsEmpty(), "mResponseText should be empty");
  } else if (NS_SUCCEEDED(status) &&
             ((mResponseType == XML_HTTP_RESPONSE_TYPE_ARRAYBUFFER &&
               !mIsMappedArrayBuffer) ||
              mResponseType == XML_HTTP_RESPONSE_TYPE_CHUNKED_ARRAYBUFFER)) {
    // set the capacity down to the actual length, to realloc back
    // down to the actual size
    if (!mArrayBufferBuilder.setCapacity(mArrayBufferBuilder.length())) {
      // this should never happen!
      status = NS_ERROR_UNEXPECTED;
    }
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
    nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(mResponseXML);
    EventListenerManager* manager =
      eventTarget->GetOrCreateListenerManager();
    manager->AddEventListenerByType(new nsXHRParseEndListener(this),
                                    NS_LITERAL_STRING("DOMContentLoaded"),
                                    TrustedEventsAtSystemGroupBubble());
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
  }
}

static nsresult
GetRequestBody(nsIDOMDocument* aDoc, nsIInputStream** aResult,
               uint64_t* aContentLength, nsACString& aContentType,
               nsACString& aCharset)
{
  aContentType.AssignLiteral("application/xml");
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDoc));
  NS_ENSURE_STATE(doc);
  aCharset.AssignLiteral("UTF-8");

  nsresult rv;
  nsCOMPtr<nsIDOMSerializer> serializer =
    do_CreateInstance(NS_XMLSERIALIZER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStorageStream> storStream;
  rv = NS_NewStorageStream(4096, UINT32_MAX, getter_AddRefs(storStream));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIOutputStream> output;
  rv = storStream->GetOutputStream(0, getter_AddRefs(output));
  NS_ENSURE_SUCCESS(rv, rv);

  // Make sure to use the encoding we'll send
  rv = serializer->SerializeToStream(aDoc, output, aCharset);
  NS_ENSURE_SUCCESS(rv, rv);

  output->Close();

  uint32_t length;
  rv = storStream->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);
  *aContentLength = length;

  return storStream->NewInputStream(0, aResult);
}

static nsresult
GetRequestBody(const nsAString& aString, nsIInputStream** aResult,
               uint64_t* aContentLength, nsACString& aContentType,
               nsACString& aCharset)
{
  aContentType.AssignLiteral("text/plain");
  aCharset.AssignLiteral("UTF-8");

  nsCString converted = NS_ConvertUTF16toUTF8(aString);
  *aContentLength = converted.Length();
  return NS_NewCStringInputStream(aResult, converted);
}

static nsresult
GetRequestBody(nsIInputStream* aStream, nsIInputStream** aResult,
               uint64_t* aContentLength, nsACString& aContentType,
               nsACString& aCharset)
{
  aContentType.AssignLiteral("text/plain");
  aCharset.Truncate();

  nsresult rv = aStream->Available(aContentLength);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*aResult = aStream);

  return NS_OK;
}

static nsresult
GetRequestBody(nsIXHRSendable* aSendable, nsIInputStream** aResult, uint64_t* aContentLength,
               nsACString& aContentType, nsACString& aCharset)
{
  return aSendable->GetSendInfo(aResult, aContentLength, aContentType, aCharset);
}

// Used for array buffers and array buffer views
static nsresult
GetRequestBody(const uint8_t* aData, uint32_t aDataLength,
               nsIInputStream** aResult, uint64_t* aContentLength,
               nsACString& aContentType, nsACString& aCharset)
{
  aContentType.SetIsVoid(true);
  aCharset.Truncate();

  *aContentLength = aDataLength;
  const char* data = reinterpret_cast<const char*>(aData);

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream), data, aDataLength,
                                      NS_ASSIGNMENT_COPY);
  NS_ENSURE_SUCCESS(rv, rv);

  stream.forget(aResult);

  return NS_OK;
}

static nsresult
GetRequestBody(nsIVariant* aBody, nsIInputStream** aResult, uint64_t* aContentLength,
               nsACString& aContentType, nsACString& aCharset)
{
  *aResult = nullptr;

  uint16_t dataType;
  nsresult rv = aBody->GetDataType(&dataType);
  NS_ENSURE_SUCCESS(rv, rv);

  if (dataType == nsIDataType::VTYPE_INTERFACE ||
      dataType == nsIDataType::VTYPE_INTERFACE_IS) {
    nsCOMPtr<nsISupports> supports;
    nsID *iid;
    rv = aBody->GetAsInterface(&iid, getter_AddRefs(supports));
    NS_ENSURE_SUCCESS(rv, rv);

    free(iid);

    // document?
    nsCOMPtr<nsIDOMDocument> doc = do_QueryInterface(supports);
    if (doc) {
      return GetRequestBody(doc, aResult, aContentLength, aContentType, aCharset);
    }

    // nsISupportsString?
    nsCOMPtr<nsISupportsString> wstr = do_QueryInterface(supports);
    if (wstr) {
      nsAutoString string;
      wstr->GetData(string);

      return GetRequestBody(string, aResult, aContentLength, aContentType, aCharset);
    }

    // nsIInputStream?
    nsCOMPtr<nsIInputStream> stream = do_QueryInterface(supports);
    if (stream) {
      return GetRequestBody(stream, aResult, aContentLength, aContentType, aCharset);
    }

    // nsIXHRSendable?
    nsCOMPtr<nsIXHRSendable> sendable = do_QueryInterface(supports);
    if (sendable) {
      return GetRequestBody(sendable, aResult, aContentLength, aContentType, aCharset);
    }

    // ArrayBuffer?
    AutoSafeJSContext cx;
    JS::Rooted<JS::Value> realVal(cx);

    nsresult rv = aBody->GetAsJSVal(&realVal);
    if (NS_SUCCEEDED(rv) && !realVal.isPrimitive()) {
      JS::Rooted<JSObject*> obj(cx, realVal.toObjectOrNull());
      ArrayBuffer buf;
      if (buf.Init(obj)) {
          buf.ComputeLengthAndData();
          return GetRequestBody(buf.Data(), buf.Length(), aResult,
                                aContentLength, aContentType, aCharset);
      }
    }
  }
  else if (dataType == nsIDataType::VTYPE_VOID ||
           dataType == nsIDataType::VTYPE_EMPTY) {
    // Makes us act as if !aBody, don't upload anything
    aContentType.AssignLiteral("text/plain");
    aCharset.AssignLiteral("UTF-8");
    *aContentLength = 0;

    return NS_OK;
  }

  char16_t* data = nullptr;
  uint32_t len = 0;
  rv = aBody->GetAsWStringWithSize(&len, &data);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString string;
  string.Adopt(data, len);

  return GetRequestBody(string, aResult, aContentLength, aContentType, aCharset);
}

/* static */
nsresult
nsXMLHttpRequest::GetRequestBody(nsIVariant* aVariant,
                                 const Nullable<RequestBody>& aBody,
                                 nsIInputStream** aResult,
                                 uint64_t* aContentLength,
                                 nsACString& aContentType, nsACString& aCharset)
{
  if (aVariant) {
    return ::GetRequestBody(aVariant, aResult, aContentLength, aContentType, aCharset);
  }

  const RequestBody& body = aBody.Value();
  RequestBody::Value value = body.GetValue();
  switch (body.GetType()) {
    case nsXMLHttpRequest::RequestBody::ArrayBuffer:
    {
      const ArrayBuffer* buffer = value.mArrayBuffer;
      buffer->ComputeLengthAndData();
      return ::GetRequestBody(buffer->Data(), buffer->Length(), aResult,
                              aContentLength, aContentType, aCharset);
    }
    case nsXMLHttpRequest::RequestBody::ArrayBufferView:
    {
      const ArrayBufferView* view = value.mArrayBufferView;
      view->ComputeLengthAndData();
      return ::GetRequestBody(view->Data(), view->Length(), aResult,
                              aContentLength, aContentType, aCharset);
    }
    case nsXMLHttpRequest::RequestBody::Blob:
    {
      nsresult rv;
      nsCOMPtr<nsIDOMBlob> blob = value.mBlob;
      nsCOMPtr<nsIXHRSendable> sendable = do_QueryInterface(blob, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      return ::GetRequestBody(sendable, aResult, aContentLength, aContentType, aCharset);
    }
    case nsXMLHttpRequest::RequestBody::Document:
    {
      nsCOMPtr<nsIDOMDocument> document = do_QueryInterface(value.mDocument);
      return ::GetRequestBody(document, aResult, aContentLength, aContentType, aCharset);
    }
    case nsXMLHttpRequest::RequestBody::DOMString:
    {
      return ::GetRequestBody(*value.mString, aResult, aContentLength,
                              aContentType, aCharset);
    }
    case nsXMLHttpRequest::RequestBody::FormData:
    {
      MOZ_ASSERT(value.mFormData);
      return ::GetRequestBody(value.mFormData, aResult, aContentLength,
                              aContentType, aCharset);
    }
    case nsXMLHttpRequest::RequestBody::InputStream:
    {
      return ::GetRequestBody(value.mStream, aResult, aContentLength,
                              aContentType, aCharset);
    }
    default:
    {
      return NS_ERROR_FAILURE;
    }
  }

  NS_NOTREACHED("Default cases exist for a reason");
  return NS_OK;
}

NS_IMETHODIMP
nsXMLHttpRequest::Send(nsIVariant *aBody)
{
  return Send(aBody, Nullable<RequestBody>());
}

nsresult
nsXMLHttpRequest::Send(nsIVariant* aVariant, const Nullable<RequestBody>& aBody)
{
  NS_ENSURE_TRUE(mPrincipal, NS_ERROR_NOT_INITIALIZED);

  PopulateNetworkInterfaceId();

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
  if (HasListenersFor(nsGkAtoms::onprogress) ||
      (mUpload && mUpload->HasListenersFor(nsGkAtoms::onprogress))) {
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
  nsAutoCString method;
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));

  if (httpChannel) {
    httpChannel->GetRequestMethod(method); // If GET, method name will be uppercase

    if (!IsSystemXHR()) {
      nsCOMPtr<nsPIDOMWindow> owner = GetOwner();
      nsCOMPtr<nsIDocument> doc = owner ? owner->GetExtantDoc() : nullptr;
      nsContentUtils::SetFetchReferrerURIWithPolicy(mPrincipal, doc,
                                                    httpChannel);
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
  if ((aVariant || !aBody.IsNull()) && httpChannel &&
      !method.LowerCaseEqualsLiteral("get") &&
      !method.LowerCaseEqualsLiteral("head")) {

    nsAutoCString charset;
    nsAutoCString defaultContentType;
    nsCOMPtr<nsIInputStream> postDataStream;

    uint64_t size_u64;
    rv = GetRequestBody(aVariant, aBody, getter_AddRefs(postDataStream),
                        &size_u64, defaultContentType, charset);
    NS_ENSURE_SUCCESS(rv, rv);

    // make sure it fits within js MAX_SAFE_INTEGER
    mUploadTotal =
      net::InScriptableRange(size_u64) ? static_cast<int64_t>(size_u64) : -1;

    if (postDataStream) {
      // If no content type header was set by the client, we set it to
      // application/xml.
      nsAutoCString contentType;
      if (NS_FAILED(httpChannel->
                      GetRequestHeader(NS_LITERAL_CSTRING("Content-Type"),
                                       contentType))) {
        contentType = defaultContentType;

        if (!charset.IsEmpty()) {
          // If we are providing the default content type, then we also need to
          // provide a charset declaration.
          contentType.Append(NS_LITERAL_CSTRING(";charset="));
          contentType.Append(charset);
        }
      }

      // We don't want to set a charset for streams.
      if (!charset.IsEmpty()) {
        nsAutoCString specifiedCharset;
        bool haveCharset;
        int32_t charsetStart, charsetEnd;
        rv = NS_ExtractCharsetFromContentType(contentType, specifiedCharset,
                                              &haveCharset, &charsetStart,
                                              &charsetEnd);
        while (NS_SUCCEEDED(rv) && haveCharset) {
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
            // Find the start of the actual charset declaration, skipping the
            // "; charset=" to avoid modifying whitespace.
            int32_t charIdx =
              Substring(contentType, charsetStart,
                        charsetEnd - charsetStart).FindChar('=') + 1;
            MOZ_ASSERT(charIdx != -1);

            contentType.Replace(charsetStart + charIdx,
                                charsetEnd - charsetStart - charIdx,
                                charset);
          }

          // Look for another charset declaration in the string, limiting the
          // search to only look for charsets before the current charset, to
          // prevent finding the same charset twice.
          nsDependentCSubstring interestingSection =
            Substring(contentType, 0, charsetStart);
          rv = NS_ExtractCharsetFromContentType(interestingSection,
                                                specifiedCharset, &haveCharset,
                                                &charsetStart, &charsetEnd);
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

      // We want to use a newer version of the upload channel that won't
      // ignore the necessary headers for an empty Content-Type.
      nsCOMPtr<nsIUploadChannel2> uploadChannel2(do_QueryInterface(httpChannel));
      // This assertion will fire if buggy extensions are installed
      NS_ASSERTION(uploadChannel2, "http must support nsIUploadChannel2");
      if (uploadChannel2) {
          uploadChannel2->ExplicitSetUploadStream(postDataStream, contentType,
                                                 mUploadTotal, method, false);
      }
      else {
        // http channel doesn't support the new nsIUploadChannel2. Emulate
        // as best we can using nsIUploadChannel
        if (contentType.IsEmpty()) {
          contentType.AssignLiteral("application/octet-stream");
        }
        nsCOMPtr<nsIUploadChannel> uploadChannel =
          do_QueryInterface(httpChannel);
        uploadChannel->SetUploadStream(postDataStream, contentType, mUploadTotal);
        // Reset the method to its original value
        httpChannel->SetRequestMethod(method);
      }
    }
  }

  if (httpChannel) {
    nsAutoCString contentTypeHeader;
    rv = httpChannel->GetRequestHeader(NS_LITERAL_CSTRING("Content-Type"),
                                       contentTypeHeader);
    if (NS_SUCCEEDED(rv)) {
      if (!nsContentUtils::IsAllowedNonCorsContentType(contentTypeHeader)) {
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

  // Blocking gets are common enough out of XHR that we should mark
  // the channel slow by default for pipeline purposes
  AddLoadFlags(mChannel, nsIRequest::INHIBIT_PIPELINE);

  nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(mChannel));
  if (cos) {
    // we never let XHR be blocked by head CSS/JS loads to avoid
    // potential deadlock where server generation of CSS/JS requires
    // an XHR signal.
    cos->AddClassFlags(nsIClassOfService::Unblocked);
  }

  nsCOMPtr<nsIHttpChannelInternal>
    internalHttpChannel(do_QueryInterface(mChannel));
  if (internalHttpChannel) {
    // Disable Necko-internal response timeouts.
    internalHttpChannel->SetResponseTimeoutEnabled(false);
  }

  nsCOMPtr<nsIStreamListener> listener = this;
  if (!IsSystemXHR()) {
    // Always create a nsCORSListenerProxy here even if it's
    // a same-origin request right now, since it could be redirected.
    nsRefPtr<nsCORSListenerProxy> corsListener =
      new nsCORSListenerProxy(listener, mPrincipal, withCredentials);
    rv = corsListener->Init(mChannel, DataURIHandling::Allow);
    NS_ENSURE_SUCCESS(rv, rv);
    listener = corsListener;
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
  else {
    AddLoadFlags(mChannel, nsIChannel::LOAD_EXPLICIT_CREDENTIALS);
  }

  NS_ASSERTION(listener != this,
               "Using an object as a listener that can't be exposed to JS");

  // When we are sync loading, we need to bypass the local cache when it would
  // otherwise block us waiting for exclusive access to the cache.  If we don't
  // do this, then we could dead lock in some cases (see bug 309424).
  //
  // Also don't block on the cache entry on async if it is busy - favoring parallelism
  // over cache hit rate for xhr. This does not disable the cache everywhere -
  // only in cases where more than one channel for the same URI is accessed
  // simultanously.

  AddLoadFlags(mChannel,
               nsICachingChannel::LOAD_BYPASS_LOCAL_CACHE_IF_BUSY);

  // While it would be optimal to bypass the cache in case of POST requests
  // since they are never cached, our ServiceWorker interception implementation
  // on single-process systems is implemented via the HTTP cache, so DO NOT
  // bypass the cache based on method!

  // Since we expect XML data, set the type hint accordingly
  // if the channel doesn't know any content type.
  // This means that we always try to parse local files as XML
  // ignoring return value, as this is not critical
  nsAutoCString contentType;
  if (NS_FAILED(mChannel->GetContentType(contentType)) ||
      contentType.IsEmpty() ||
      contentType.Equals(UNKNOWN_CONTENT_TYPE)) {
    mChannel->SetContentType(NS_LITERAL_CSTRING("application/xml"));
  }

  // We're about to send the request.  Start our timeout.
  mRequestSentTime = PR_Now();
  StartTimeoutTimer();

  // Set up the preflight if needed
  if (mState & XML_HTTP_REQUEST_NEED_AC_PREFLIGHT) {
    // Check to see if this initial OPTIONS request has already been cached
    // in our special Access Control Cache.

    rv = internalHttpChannel->SetCorsPreflightParameters(mCORSUnsafeHeaders,
                                                         withCredentials, mPrincipal);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mIsMappedArrayBuffer = false;
  if (mResponseType == XML_HTTP_RESPONSE_TYPE_ARRAYBUFFER &&
      Preferences::GetBool("dom.mapped_arraybuffer.enabled", false)) {
    nsCOMPtr<nsIURI> uri;
    nsAutoCString scheme;

    rv = mChannel->GetURI(getter_AddRefs(uri));
    if (NS_SUCCEEDED(rv)) {
      uri->GetScheme(scheme);
      if (scheme.LowerCaseEqualsLiteral("app") ||
          scheme.LowerCaseEqualsLiteral("jar")) {
        mIsMappedArrayBuffer = true;
      }
    }
  }

  // Start reading from the channel
  rv = mChannel->AsyncOpen(listener, nullptr);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Drop our ref to the channel to avoid cycles
    mChannel = nullptr;
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
          suspendedDoc = suspendedWindow->GetExtantDoc();
          if (suspendedDoc) {
            suspendedDoc->SuppressEventHandling(nsIDocument::eEvents);
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
      suspendedDoc->UnsuppressEventHandlingAndFireEvents(nsIDocument::eEvents,
                                                         true);
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
    if (mUpload && mUpload->HasListenersFor(nsGkAtoms::onprogress)) {
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
  if (!NS_IsValidHTTPToken(header)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  if (!mChannel)             // open() initializes mChannel, and open()
    return NS_ERROR_FAILURE; // must be called before first setRequestHeader()

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
  if (!httpChannel) {
    return NS_OK;
  }

  // We will merge XHR headers, per the spec (secion 4.6.2) unless:
  // 1 - The caller is privileged and setting an invalid header,
  // or
  // 2 - we have not yet explicitly set that header; this allows web
  //     content to override default headers the first time they set them.
  bool mergeHeaders = true;

  if (!IsSystemXHR()) {
    // Step 5: Check for dangerous headers.
    // Prevent modification to certain HTTP headers (see bug 302263), unless
    // the executing script is privileged.
    if (nsContentUtils::IsForbiddenRequestHeader(header)) {
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
      for (uint32_t i = 0; i < ArrayLength(kCrossOriginSafeHeaders); ++i) {
        if (header.LowerCaseEqualsASCII(kCrossOriginSafeHeaders[i])) {
          safeHeader = true;
          break;
        }
      }
    }

    if (!safeHeader) {
      if (!mCORSUnsafeHeaders.Contains(header)) {
        mCORSUnsafeHeaders.AppendElement(header);
      }
    }
  } else {
    // Case 1 above
    if (nsContentUtils::IsForbiddenSystemRequestHeader(header)) {
      mergeHeaders = false;
    }
  }

  if (!mAlreadySetHeaders.Contains(header)) {
    // Case 2 above
    mergeHeaders = false;
  }

  nsresult rv;
  if (value.IsEmpty()) {
    rv = httpChannel->SetEmptyRequestHeader(header);
  } else {
    // Merge headers depending on what we decided above.
    rv = httpChannel->SetRequestHeader(header, value, mergeHeaders);
  }
  if (rv == NS_ERROR_INVALID_ARG) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }
  if (NS_SUCCEEDED(rv)) {
    // Remember that we've set this header, so subsequent set operations will merge values.
    mAlreadySetHeaders.PutEntry(nsCString(header));

    // We'll want to duplicate this header for any replacement channels (eg. on redirect)
    RequestHeader reqHeader = {
      nsCString(header), nsCString(value)
    };
    mModifiedRequestHeaders.AppendElement(reqHeader);
  }
  return rv;
}

NS_IMETHODIMP
nsXMLHttpRequest::GetTimeout(uint32_t *aTimeout)
{
  *aTimeout = Timeout();
  return NS_OK;
}

NS_IMETHODIMP
nsXMLHttpRequest::SetTimeout(uint32_t aTimeout)
{
  ErrorResult rv;
  SetTimeout(aTimeout, rv);
  return rv.StealNSResult();
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
  MOZ_ASSERT(mRequestSentTime,
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
  uint32_t elapsed =
    (uint32_t)((PR_Now() - mRequestSentTime) / PR_USEC_PER_MSEC);
  mTimeoutTimer->InitWithCallback(
    this,
    mTimeoutMilliseconds > elapsed ? mTimeoutMilliseconds - elapsed : 0,
    nsITimer::TYPE_ONE_SHOT
  );
}

NS_IMETHODIMP
nsXMLHttpRequest::GetReadyState(uint16_t *aState)
{
  *aState = ReadyState();
  return NS_OK;
}

uint16_t
nsXMLHttpRequest::ReadyState()
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
  if (mState & XML_HTTP_REQUEST_LOADING) {
    return LOADING;
  }
  MOZ_ASSERT(mState & XML_HTTP_REQUEST_DONE);
  return DONE;
}

NS_IMETHODIMP
nsXMLHttpRequest::SlowOverrideMimeType(const nsAString& aMimeType)
{
  OverrideMimeType(aMimeType);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLHttpRequest::GetMozBackgroundRequest(bool *_retval)
{
  *_retval = MozBackgroundRequest();
  return NS_OK;
}

bool
nsXMLHttpRequest::MozBackgroundRequest()
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
  if (!IsSystemXHR()) {
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

NS_IMETHODIMP
nsXMLHttpRequest::GetWithCredentials(bool *_retval)
{
  *_retval = WithCredentials();
  return NS_OK;
}

bool
nsXMLHttpRequest::WithCredentials()
{
  return !!(mState & XML_HTTP_REQUEST_AC_WITH_CREDENTIALS);
}

NS_IMETHODIMP
nsXMLHttpRequest::SetWithCredentials(bool aWithCredentials)
{
  ErrorResult rv;
  SetWithCredentials(aWithCredentials, rv);
  return rv.StealNSResult();
}

void
nsXMLHttpRequest::SetWithCredentials(bool aWithCredentials, ErrorResult& aRv)
{
  // Return error if we're already processing a request.  Note that we can't use
  // ReadyState() here, because it can't differentiate between "opened" and
  // "sent", so we use mState directly.
  if (!(mState & XML_HTTP_REQUEST_UNSENT) &&
      !(mState & XML_HTTP_REQUEST_OPENED)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // sync request is not allowed setting withCredentials in window context
  if (HasOrHasHadOwner() &&
      !(mState & (XML_HTTP_REQUEST_UNSENT | XML_HTTP_REQUEST_ASYNC))) {
    LogMessage("WithCredentialsSyncXHRWarning", GetOwner());
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  if (aWithCredentials) {
    mState |= XML_HTTP_REQUEST_AC_WITH_CREDENTIALS;
  } else {
    mState &= ~XML_HTTP_REQUEST_AC_WITH_CREDENTIALS;
  }
}

nsresult
nsXMLHttpRequest::ChangeState(uint32_t aState, bool aBroadcast)
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

  if ((aState & XML_HTTP_REQUEST_LOADSTATES) &&  // Broadcast load states only
      aState != XML_HTTP_REQUEST_SENT && // And not internal ones
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
class AsyncVerifyRedirectCallbackForwarder final : public nsIAsyncVerifyRedirectCallback
{
public:
  explicit AsyncVerifyRedirectCallbackForwarder(nsXMLHttpRequest* xhr)
    : mXHR(xhr)
  {
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(AsyncVerifyRedirectCallbackForwarder)

  // nsIAsyncVerifyRedirectCallback implementation
  NS_IMETHOD OnRedirectVerifyCallback(nsresult result) override
  {
    mXHR->OnRedirectVerifyCallback(result);

    return NS_OK;
  }

private:
  ~AsyncVerifyRedirectCallbackForwarder() {}

  nsRefPtr<nsXMLHttpRequest> mXHR;
};

NS_IMPL_CYCLE_COLLECTION(AsyncVerifyRedirectCallbackForwarder, mXHR)

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
                                         uint32_t    aFlags,
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
      for (uint32_t i = mModifiedRequestHeaders.Length(); i > 0; ) {
        --i;
        if (mModifiedRequestHeaders[i].value.IsEmpty()) {
          httpChannel->SetEmptyRequestHeader(mModifiedRequestHeaders[i].header);
        } else {
          httpChannel->SetRequestHeader(mModifiedRequestHeaders[i].header,
                                        mModifiedRequestHeaders[i].value,
                                        false);
        }
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
    if (mUpload && !mUploadComplete) {
      DispatchProgressEvent(mUpload, NS_LITERAL_STRING(PROGRESS_STR),
                            mUploadLengthComputable, mUploadTransferred,
                            mUploadTotal);
    }
  } else {
    if (aFinalProgress) {
      mLoadTotal = mLoadTransferred;
    }
    mInLoadProgressEvent = true;
    DispatchProgressEvent(this, NS_LITERAL_STRING(PROGRESS_STR),
                          mLoadLengthComputable, mLoadTransferred,
                          mLoadTotal);
    mInLoadProgressEvent = false;
    if (mResponseType == XML_HTTP_RESPONSE_TYPE_CHUNKED_TEXT ||
        mResponseType == XML_HTTP_RESPONSE_TYPE_CHUNKED_ARRAYBUFFER) {
      mResponseBody.Truncate();
      mResponseText.Truncate();
      mResultArrayBuffer = nullptr;
      mArrayBufferBuilder.reset();
    }
  }

  mProgressSinceLastProgressEvent = false;
}

NS_IMETHODIMP
nsXMLHttpRequest::OnProgress(nsIRequest *aRequest, nsISupports *aContext, int64_t aProgress, int64_t aProgressMax)
{
  // We're uploading if our state is XML_HTTP_REQUEST_OPENED or
  // XML_HTTP_REQUEST_SENT
  bool upload = !!((XML_HTTP_REQUEST_OPENED | XML_HTTP_REQUEST_SENT) & mState);
  // When uploading, OnProgress reports also headers in aProgress and aProgressMax.
  // So, try to remove the headers, if possible.
  bool lengthComputable = (aProgressMax != -1);
  if (upload) {
    int64_t loaded = aProgress;
    if (lengthComputable) {
      int64_t headerSize = aProgressMax - mUploadTotal;
      loaded -= headerSize;
    }
    mUploadLengthComputable = lengthComputable;
    mUploadTransferred = loaded;
    mProgressSinceLastProgressEvent = true;

    MaybeDispatchProgressEvents((mUploadTransferred == mUploadTotal));
  } else {
    mLoadLengthComputable = lengthComputable;
    mLoadTotal = lengthComputable ? aProgressMax : 0;
    mLoadTransferred = aProgress;
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
nsXMLHttpRequest::OnStatus(nsIRequest *aRequest, nsISupports *aContext, nsresult aStatus, const char16_t *aStatusArg)
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
    *aResult = static_cast<nsIChannelEventSink*>(EnsureXPCOMifier().take());
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsIProgressEventSink))) {
    mProgressEventSink = do_GetInterface(mNotificationCallbacks);
    *aResult = static_cast<nsIProgressEventSink*>(EnsureXPCOMifier().take());
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

    nsCOMPtr<nsIURI> uri;
    rv = mChannel->GetURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    // Verify that it's ok to prompt for credentials here, per spec
    // http://xhr.spec.whatwg.org/#the-send%28%29-method
    bool showPrompt = true;

    // If authentication fails, XMLHttpRequest origin and
    // the request URL are same origin, ...
    /* Disabled - bug: 799540
    if (mState & XML_HTTP_REQUEST_USE_XSITE_AC) {
      showPrompt = false;
    }
    */

    // ... Authorization is not in the list of author request headers, ...
    if (showPrompt) {
      for (uint32_t i = 0, len = mModifiedRequestHeaders.Length(); i < len; ++i) {
        if (mModifiedRequestHeaders[i].header.
              LowerCaseEqualsLiteral("authorization")) {
          showPrompt = false;
          break;
        }
      }
    }

    // ... request username is null, and request password is null,
    if (showPrompt) {

      nsCString username;
      rv = uri->GetUsername(username);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCString password;
      rv = uri->GetPassword(password);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!username.IsEmpty() || !password.IsEmpty()) {
        showPrompt = false;
      }
    }

    // ... user agents should prompt the end user for their username and password.
    if (!showPrompt) {
      nsRefPtr<XMLHttpRequestAuthPrompt> prompt = new XMLHttpRequestAuthPrompt();
      if (!prompt)
        return NS_ERROR_OUT_OF_MEMORY;

      return prompt->QueryInterface(aIID, aResult);
    }

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
  // Now check for the various XHR non-DOM interfaces, except
  // nsIProgressEventSink and nsIChannelEventSink which we already
  // handled above.
  else if (aIID.Equals(NS_GET_IID(nsIStreamListener))) {
    *aResult = static_cast<nsIStreamListener*>(EnsureXPCOMifier().take());
    return NS_OK;
  }
  else if (aIID.Equals(NS_GET_IID(nsIRequestObserver))) {
    *aResult = static_cast<nsIRequestObserver*>(EnsureXPCOMifier().take());
    return NS_OK;
  }
  else if (aIID.Equals(NS_GET_IID(nsITimerCallback))) {
    *aResult = static_cast<nsITimerCallback*>(EnsureXPCOMifier().take());
    return NS_OK;
  }

  return QueryInterface(aIID, aResult);
}

void
nsXMLHttpRequest::GetInterface(JSContext* aCx, nsIJSID* aIID,
                               JS::MutableHandle<JS::Value> aRetval,
                               ErrorResult& aRv)
{
  dom::GetInterface(aCx, this, aIID, aRetval, aRv);
}

nsXMLHttpRequestUpload*
nsXMLHttpRequest::Upload()
{
  if (!mUpload) {
    mUpload = new nsXMLHttpRequestUpload(this);
  }
  return mUpload;
}

NS_IMETHODIMP
nsXMLHttpRequest::GetUpload(nsIXMLHttpRequestUpload** aUpload)
{
  nsRefPtr<nsXMLHttpRequestUpload> upload = Upload();
  upload.forget(aUpload);
  return NS_OK;
}

bool
nsXMLHttpRequest::MozAnon()
{
  return mIsAnon;
}

NS_IMETHODIMP
nsXMLHttpRequest::GetMozAnon(bool* aAnon)
{
  *aAnon = MozAnon();
  return NS_OK;
}

bool
nsXMLHttpRequest::MozSystem()
{
  return IsSystemXHR();
}

NS_IMETHODIMP
nsXMLHttpRequest::GetMozSystem(bool* aSystem)
{
  *aSystem = MozSystem();
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
  MaybeDispatchProgressEvents(false);
}

void
nsXMLHttpRequest::StartProgressEventTimer()
{
  if (!mProgressNotifier) {
    mProgressNotifier = do_CreateInstance(NS_TIMER_CONTRACTID);
  }
  if (mProgressNotifier) {
    mProgressTimerIsActive = true;
    mProgressNotifier->Cancel();
    mProgressNotifier->InitWithCallback(this, NS_PROGRESS_EVENT_INTERVAL,
                                        nsITimer::TYPE_ONE_SHOT);
  }
}

already_AddRefed<nsXMLHttpRequestXPCOMifier>
nsXMLHttpRequest::EnsureXPCOMifier()
{
  if (!mXPCOMifier) {
    mXPCOMifier = new nsXMLHttpRequestXPCOMifier(this);
  }
  nsRefPtr<nsXMLHttpRequestXPCOMifier> newRef(mXPCOMifier);
  return newRef.forget();
}

NS_IMPL_ISUPPORTS(nsXMLHttpRequest::nsHeaderVisitor, nsIHttpHeaderVisitor)

NS_IMETHODIMP nsXMLHttpRequest::
nsHeaderVisitor::VisitHeader(const nsACString &header, const nsACString &value)
{
  if (mXHR->IsSafeHeader(header, mHttpChannel)) {
    mHeaders.Append(header);
    mHeaders.AppendLiteral(": ");
    mHeaders.Append(value);
    mHeaders.AppendLiteral("\r\n");
  }
  return NS_OK;
}

// nsXMLHttpRequestXPCOMifier implementation
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXMLHttpRequestXPCOMifier)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIChannelEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXMLHttpRequestXPCOMifier)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXMLHttpRequestXPCOMifier)

// Can't NS_IMPL_CYCLE_COLLECTION( because mXHR has ambiguous
// inheritance from nsISupports.
NS_IMPL_CYCLE_COLLECTION_CLASS(nsXMLHttpRequestXPCOMifier)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXMLHttpRequestXPCOMifier)
if (tmp->mXHR) {
  tmp->mXHR->mXPCOMifier = nullptr;
}
NS_IMPL_CYCLE_COLLECTION_UNLINK(mXHR)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXMLHttpRequestXPCOMifier)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mXHR)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMETHODIMP
nsXMLHttpRequestXPCOMifier::GetInterface(const nsIID & aIID, void **aResult)
{
  // Return ourselves for the things we implement (except
  // nsIInterfaceRequestor) and the XHR for the rest.
  if (!aIID.Equals(NS_GET_IID(nsIInterfaceRequestor))) {
    nsresult rv = QueryInterface(aIID, aResult);
    if (NS_SUCCEEDED(rv)) {
      return rv;
    }
  }

  return mXHR->GetInterface(aIID, aResult);
}

namespace mozilla {

ArrayBufferBuilder::ArrayBufferBuilder()
  : mDataPtr(nullptr),
    mCapacity(0),
    mLength(0),
    mMapPtr(nullptr)
{
}

ArrayBufferBuilder::~ArrayBufferBuilder()
{
  reset();
}

void
ArrayBufferBuilder::reset()
{
  if (mDataPtr) {
    JS_free(nullptr, mDataPtr);
  }

  if (mMapPtr) {
    JS_ReleaseMappedArrayBufferContents(mMapPtr, mLength);
    mMapPtr = nullptr;
  }

  mDataPtr = nullptr;
  mCapacity = mLength = 0;
}

bool
ArrayBufferBuilder::setCapacity(uint32_t aNewCap)
{
  MOZ_ASSERT(!mMapPtr);

  // To ensure that realloc won't free mDataPtr, use a size of 1
  // instead of 0.
  uint8_t* newdata = (uint8_t *) js_realloc(mDataPtr, aNewCap ? aNewCap : 1);

  if (!newdata) {
    return false;
  }

  if (aNewCap > mCapacity) {
    memset(newdata + mCapacity, 0, aNewCap - mCapacity);
  }

  mDataPtr = newdata;
  mCapacity = aNewCap;
  if (mLength > aNewCap) {
    mLength = aNewCap;
  }

  return true;
}

bool
ArrayBufferBuilder::append(const uint8_t *aNewData, uint32_t aDataLen,
                           uint32_t aMaxGrowth)
{
  MOZ_ASSERT(!mMapPtr);

  CheckedUint32 neededCapacity = mLength;
  neededCapacity += aDataLen;
  if (!neededCapacity.isValid()) {
    return false;
  }
  if (mLength + aDataLen > mCapacity) {
    CheckedUint32 newcap = mCapacity;
    // Double while under aMaxGrowth or if not specified.
    if (!aMaxGrowth || mCapacity < aMaxGrowth) {
      newcap *= 2;
    } else {
      newcap += aMaxGrowth;
    }

    if (!newcap.isValid()) {
      return false;
    }

    // But make sure there's always enough to satisfy our request.
    if (newcap.value() < neededCapacity.value()) {
      newcap = neededCapacity;
    }

    if (!setCapacity(newcap.value())) {
      return false;
    }
  }

  // Assert that the region isn't overlapping so we can memcpy.
  MOZ_ASSERT(!areOverlappingRegions(aNewData, aDataLen, mDataPtr + mLength,
                                    aDataLen));

  memcpy(mDataPtr + mLength, aNewData, aDataLen);
  mLength += aDataLen;

  return true;
}

JSObject*
ArrayBufferBuilder::getArrayBuffer(JSContext* aCx)
{
  if (mMapPtr) {
    JSObject* obj = JS_NewMappedArrayBufferWithContents(aCx, mLength, mMapPtr);
    if (!obj) {
      JS_ReleaseMappedArrayBufferContents(mMapPtr, mLength);
    }
    mMapPtr = nullptr;

    // The memory-mapped contents will be released when obj been finalized(GCed
    // or neutered).
    return obj;
  }

  // we need to check for mLength == 0, because nothing may have been
  // added
  if (mCapacity > mLength || mLength == 0) {
    if (!setCapacity(mLength)) {
      return nullptr;
    }
  }

  JSObject* obj = JS_NewArrayBufferWithContents(aCx, mLength, mDataPtr);
  mLength = mCapacity = 0;
  if (!obj) {
    js_free(mDataPtr);
  }
  mDataPtr = nullptr;
  return obj;
}

nsresult
ArrayBufferBuilder::mapToFileInPackage(const nsCString& aFile,
                                       nsIFile* aJarFile)
{
#ifdef XP_WIN
  // TODO: Bug 988813 - Support memory mapped array buffer for Windows platform.
  MOZ_CRASH("Not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  nsresult rv;

  // Open Jar file to get related attributes of target file.
  nsRefPtr<nsZipArchive> zip = new nsZipArchive();
  rv = zip->OpenArchive(aJarFile);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsZipItem* zipItem = zip->GetItem(aFile.get());
  if (!zipItem) {
    return NS_ERROR_FILE_TARGET_DOES_NOT_EXIST;
  }

  // If file was added to the package as stored(uncompressed), map to the
  // offset of file in zip package.
  if (!zipItem->Compression()) {
    uint32_t offset = zip->GetDataOffset(zipItem);
    uint32_t size = zipItem->RealSize();
    mozilla::AutoFDClose pr_fd;
    rv = aJarFile->OpenNSPRFileDesc(PR_RDONLY, 0, &pr_fd.rwget());
    if (NS_FAILED(rv)) {
      return rv;
    }
    mMapPtr = JS_CreateMappedArrayBufferContents(PR_FileDesc2NativeHandle(pr_fd),
                                                 offset, size);
    if (mMapPtr) {
      mLength = size;
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
#endif
}

/* static */ bool
ArrayBufferBuilder::areOverlappingRegions(const uint8_t* aStart1,
                                          uint32_t aLength1,
                                          const uint8_t* aStart2,
                                          uint32_t aLength2)
{
  const uint8_t* end1 = aStart1 + aLength1;
  const uint8_t* end2 = aStart2 + aLength2;

  const uint8_t* max_start = aStart1 > aStart2 ? aStart1 : aStart2;
  const uint8_t* min_end   = end1 < end2 ? end1 : end2;

  return max_start < min_end;
}

} // namespace mozilla
