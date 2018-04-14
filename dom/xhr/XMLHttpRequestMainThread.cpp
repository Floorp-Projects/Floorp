/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XMLHttpRequestMainThread.h"

#include <algorithm>
#ifndef XP_WIN
#include <unistd.h>
#endif
#include "mozilla/ArrayUtils.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/BlobSet.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/DOMString.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileBinding.h"
#include "mozilla/dom/FileCreatorHelper.h"
#include "mozilla/dom/FetchUtil.h"
#include "mozilla/dom/FormData.h"
#include "mozilla/dom/MutableBlobStorage.h"
#include "mozilla/dom/XMLDocument.h"
#include "mozilla/dom/URLSearchParams.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/Encoding.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/LoadContext.h"
#include "mozilla/MemoryReporting.h"
#include "nsIDOMDocument.h"
#include "mozilla/dom/ProgressEvent.h"
#include "nsIJARChannel.h"
#include "nsIJARURI.h"
#include "nsLayoutCID.h"
#include "nsReadableUtils.h"

#include "nsIURI.h"
#include "nsIURIMutator.h"
#include "nsILoadGroup.h"
#include "nsNetUtil.h"
#include "nsStringStream.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPrompt2.h"
#include "nsIClassOfService.h"
#include "nsIOutputStream.h"
#include "nsISupportsPrimitives.h"
#include "nsISupportsPriority.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "nsIUploadChannel.h"
#include "nsIUploadChannel2.h"
#include "nsXPCOM.h"
#include "nsIDOMEventListener.h"
#include "nsIScriptSecurityManager.h"
#include "nsIVariant.h"
#include "nsVariant.h"
#include "nsIScriptError.h"
#include "nsIStreamConverterService.h"
#include "nsICachingChannel.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsError.h"
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
#include "mozilla/dom/XMLHttpRequestBinding.h"
#include "mozilla/Attributes.h"
#include "MultipartBlobImpl.h"
#include "nsIPermissionManager.h"
#include "nsMimeTypes.h"
#include "nsIHttpChannelInternal.h"
#include "nsIClassOfService.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsStreamListenerWrapper.h"
#include "xpcjsid.h"
#include "nsITimedChannel.h"
#include "nsWrapperCacheInlines.h"
#include "nsZipArchive.h"
#include "mozilla/Preferences.h"
#include "private/pprio.h"
#include "XMLHttpRequestUpload.h"

// Undefine the macro of CreateFile to avoid FileCreatorHelper#CreateFile being
// replaced by FileCreatorHelper#CreateFileW.
#ifdef CreateFile
#undef CreateFile
#endif

using namespace mozilla::net;

namespace mozilla {
namespace dom {

// Maximum size that we'll grow an ArrayBuffer instead of doubling,
// once doubling reaches this threshold
const uint32_t XML_HTTP_REQUEST_ARRAYBUFFER_MAX_GROWTH = 32*1024*1024;
// start at 32k to avoid lots of doubling right at the start
const uint32_t XML_HTTP_REQUEST_ARRAYBUFFER_MIN_SIZE = 32*1024;
// the maximum Content-Length that we'll preallocate.  1GB.  Must fit
// in an int32_t!
const int32_t XML_HTTP_REQUEST_MAX_CONTENT_LENGTH_PREALLOCATE = 1*1024*1024*1024LL;

namespace {
  const nsLiteralString ProgressEventTypeStrings[] = {
    NS_LITERAL_STRING("loadstart"),
    NS_LITERAL_STRING("progress"),
    NS_LITERAL_STRING("error"),
    NS_LITERAL_STRING("abort"),
    NS_LITERAL_STRING("timeout"),
    NS_LITERAL_STRING("load"),
    NS_LITERAL_STRING("loadend")
  };
  static_assert(MOZ_ARRAY_LENGTH(ProgressEventTypeStrings) ==
                  size_t(XMLHttpRequestMainThread::ProgressEventType::ENUM_MAX),
                "Mismatched lengths for ProgressEventTypeStrings and ProgressEventType enums");

  const nsString kLiteralString_readystatechange = NS_LITERAL_STRING("readystatechange");
  const nsString kLiteralString_xmlhttprequest = NS_LITERAL_STRING("xmlhttprequest");
  const nsString kLiteralString_DOMContentLoaded = NS_LITERAL_STRING("DOMContentLoaded");
}

// CIDs
#define NS_BADCERTHANDLER_CONTRACTID \
  "@mozilla.org/content/xmlhttprequest-bad-cert-handler;1"

#define NS_PROGRESS_EVENT_INTERVAL 50
#define MAX_SYNC_TIMEOUT_WHEN_UNLOADING 10000 /* 10 secs */

NS_IMPL_ISUPPORTS(nsXHRParseEndListener, nsIDOMEventListener)

class nsResumeTimeoutsEvent : public Runnable
{
public:
  explicit nsResumeTimeoutsEvent(nsPIDOMWindowInner* aWindow)
    : Runnable("dom::nsResumeTimeoutsEvent")
    , mWindow(aWindow)
  {
  }

  NS_IMETHOD Run() override
  {
    mWindow->Resume();
    return NS_OK;
  }

private:
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
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

// We are in a sync event loop.
#define NOT_CALLABLE_IN_SYNC_SEND                              \
  if (mFlagSyncLooping) {                                      \
    return NS_ERROR_DOM_INVALID_STATE_XHR_HAS_INVALID_CONTEXT; \
  }

#define NOT_CALLABLE_IN_SYNC_SEND_RV                               \
  if (mFlagSyncLooping) {                                          \
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_XHR_HAS_INVALID_CONTEXT); \
    return;                                                        \
  }

/////////////////////////////////////////////
//
//
/////////////////////////////////////////////

bool
XMLHttpRequestMainThread::sDontWarnAboutSyncXHR = false;

XMLHttpRequestMainThread::XMLHttpRequestMainThread()
  : mResponseBodyDecodedPos(0),
    mResponseCharset(nullptr),
    mResponseType(XMLHttpRequestResponseType::_empty),
    mRequestObserver(nullptr),
    mState(XMLHttpRequestBinding::UNSENT),
    mFlagSynchronous(false), mFlagAborted(false), mFlagParseBody(false),
    mFlagSyncLooping(false), mFlagBackgroundRequest(false),
    mFlagHadUploadListenersOnSend(false), mFlagACwithCredentials(false),
    mFlagTimedOut(false), mFlagDeleted(false), mFlagSend(false),
    mUploadTransferred(0), mUploadTotal(0), mUploadComplete(true),
    mProgressSinceLastProgressEvent(false),
    mRequestSentTime(0), mTimeoutMilliseconds(0),
    mErrorLoad(ErrorType::eOK), mErrorParsingXML(false),
    mWaitingForOnStopRequest(false),
    mProgressTimerIsActive(false),
    mIsHtml(false),
    mWarnAboutSyncHtml(false),
    mLoadTotal(-1),
    mIsSystem(false),
    mIsAnon(false),
    mFirstStartRequestSeen(false),
    mInLoadProgressEvent(false),
    mResultJSON(JS::UndefinedValue()),
    mResultArrayBuffer(nullptr),
    mIsMappedArrayBuffer(false),
    mXPCOMifier(nullptr),
    mEventDispatchingSuspended(false)
{
  mozilla::HoldJSObjects(this);
}

XMLHttpRequestMainThread::~XMLHttpRequestMainThread()
{
  mFlagDeleted = true;

  if ((mState == XMLHttpRequestBinding::OPENED && mFlagSend) ||
      mState == XMLHttpRequestBinding::LOADING) {
    Abort();
  }

  if (mParseEndListener) {
    mParseEndListener->SetIsStale();
    mParseEndListener = nullptr;
  }

  MOZ_ASSERT(!mFlagSyncLooping, "we rather crash than hang");
  mFlagSyncLooping = false;

  mResultJSON.setUndefined();
  mResultArrayBuffer = nullptr;
  mozilla::DropJSObjects(this);
}

void
XMLHttpRequestMainThread::InitParameters(bool aAnon, bool aSystem)
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
XMLHttpRequestMainThread::SetClientInfoAndController(const ClientInfo& aClientInfo,
                                                     const Maybe<ServiceWorkerDescriptor>& aController)
{
  mClientInfo.emplace(aClientInfo);
  mController = aController;
}

void
XMLHttpRequestMainThread::ResetResponse()
{
  mResponseXML = nullptr;
  mResponseBody.Truncate();
  TruncateResponseText();
  mResponseBlob = nullptr;
  mBlobStorage = nullptr;
  mResultArrayBuffer = nullptr;
  mArrayBufferBuilder.reset();
  mResultJSON.setUndefined();
  mLoadTransferred = 0;
  mResponseBodyDecodedPos = 0;
}

void
XMLHttpRequestMainThread::SetRequestObserver(nsIRequestObserver* aObserver)
{
  mRequestObserver = aObserver;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(XMLHttpRequestMainThread)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(XMLHttpRequestMainThread,
                                                  XMLHttpRequestEventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChannel)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResponseXML)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mXMLParserStreamListener)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResponseBlob)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNotificationCallbacks)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChannelEventSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mProgressEventSink)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mUpload)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(XMLHttpRequestMainThread,
                                                XMLHttpRequestEventTarget)
  tmp->mResultArrayBuffer = nullptr;
  tmp->mArrayBufferBuilder.reset();
  tmp->mResultJSON.setUndefined();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChannel)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mResponseXML)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mXMLParserStreamListener)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mResponseBlob)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNotificationCallbacks)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChannelEventSink)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mProgressEventSink)

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mUpload)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(XMLHttpRequestMainThread,
                                               XMLHttpRequestEventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mResultArrayBuffer)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mResultJSON)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

bool
XMLHttpRequestMainThread::IsCertainlyAliveForCC() const
{
  return mWaitingForOnStopRequest;
}

// QueryInterface implementation for XMLHttpRequestMainThread
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(XMLHttpRequestMainThread)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIChannelEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
  NS_INTERFACE_MAP_ENTRY(nsISizeOfEventTarget)
NS_INTERFACE_MAP_END_INHERITING(XMLHttpRequestEventTarget)

NS_IMPL_ADDREF_INHERITED(XMLHttpRequestMainThread, XMLHttpRequestEventTarget)
NS_IMPL_RELEASE_INHERITED(XMLHttpRequestMainThread, XMLHttpRequestEventTarget)

void
XMLHttpRequestMainThread::DisconnectFromOwner()
{
  XMLHttpRequestEventTarget::DisconnectFromOwner();
  Abort();
}

size_t
XMLHttpRequestMainThread::SizeOfEventTargetIncludingThis(
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
  n += mResponseText.SizeOfThis(aMallocSizeOf);

  return n;

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - lots
}

static void LogMessage(const char* aWarning, nsPIDOMWindowInner* aWindow,
                       const char16_t** aParams=nullptr, uint32_t aParamCount=0)
{
  nsCOMPtr<nsIDocument> doc;
  if (aWindow) {
    doc = aWindow->GetExtantDoc();
  }
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("DOM"), doc,
                                  nsContentUtils::eDOM_PROPERTIES,
                                  aWarning, aParams, aParamCount);
}

nsIDocument*
XMLHttpRequestMainThread::GetResponseXML(ErrorResult& aRv)
{
  if (mResponseType != XMLHttpRequestResponseType::_empty &&
      mResponseType != XMLHttpRequestResponseType::Document) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_XHR_HAS_WRONG_RESPONSETYPE_FOR_RESPONSEXML);
    return nullptr;
  }
  if (mWarnAboutSyncHtml) {
    mWarnAboutSyncHtml = false;
    LogMessage("HTMLSyncXHRWarning", GetOwner());
  }
  if (mState != XMLHttpRequestBinding::DONE) {
    return nullptr;
  }
  return mResponseXML;
}

/*
 * This piece copied from XMLDocument, we try to get the charset
 * from HTTP headers.
 */
nsresult
XMLHttpRequestMainThread::DetectCharset()
{
  mResponseCharset = nullptr;
  mDecoder = nullptr;

  if (mResponseType != XMLHttpRequestResponseType::_empty &&
      mResponseType != XMLHttpRequestResponseType::Text &&
      mResponseType != XMLHttpRequestResponseType::Json) {
    return NS_OK;
  }

  nsAutoCString charsetVal;
  const Encoding* encoding;
  bool ok = mChannel &&
            NS_SUCCEEDED(mChannel->GetContentCharset(charsetVal)) &&
            (encoding = Encoding::ForLabel(charsetVal));
  if (!ok) {
    // MS documentation states UTF-8 is default for responseText
    encoding = UTF_8_ENCODING;
  }

  if (mResponseType == XMLHttpRequestResponseType::Json &&
      encoding != UTF_8_ENCODING) {
    // The XHR spec says only UTF-8 is supported for responseType == "json"
    LogMessage("JSONCharsetWarning", GetOwner());
    encoding = UTF_8_ENCODING;
  }

  mResponseCharset = encoding;
  mDecoder = encoding->NewDecoderWithBOMRemoval();

  return NS_OK;
}

nsresult
XMLHttpRequestMainThread::AppendToResponseText(const char * aSrcBuffer,
                                               uint32_t aSrcBufferLen)
{
  NS_ENSURE_STATE(mDecoder);

  CheckedInt<size_t> destBufferLen =
    mDecoder->MaxUTF16BufferLength(aSrcBufferLen);
  if (!destBufferLen.isValid()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  CheckedInt32 size = mResponseText.Length();
  size += destBufferLen.value();
  if (!size.isValid()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  XMLHttpRequestStringWriterHelper helper(mResponseText);

  if (!helper.AddCapacity(destBufferLen.value())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // XXX there's no handling for incomplete byte sequences on EOF!
  uint32_t result;
  size_t read;
  size_t written;
  bool hadErrors;
  Tie(result, read, written, hadErrors) = mDecoder->DecodeToUTF16(
    AsBytes(MakeSpan(aSrcBuffer, aSrcBufferLen)),
    MakeSpan(helper.EndOfExistingData(), destBufferLen.value()),
    false);
  MOZ_ASSERT(result == kInputEmpty);
  MOZ_ASSERT(read == aSrcBufferLen);
  MOZ_ASSERT(written <= destBufferLen.value());
  Unused << hadErrors;
  helper.AddLength(written);
  return NS_OK;
}

void
XMLHttpRequestMainThread::GetResponseText(DOMString& aResponseText,
                                          ErrorResult& aRv)
{
  XMLHttpRequestStringSnapshot snapshot;
  GetResponseText(snapshot, aRv);
  if (aRv.Failed()) {
    return;
  }

  if (!snapshot.GetAsString(aResponseText)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
}

void
XMLHttpRequestMainThread::GetResponseText(XMLHttpRequestStringSnapshot& aSnapshot,
                                          ErrorResult& aRv)
{
  aSnapshot.Reset();

  if (mResponseType != XMLHttpRequestResponseType::_empty &&
      mResponseType != XMLHttpRequestResponseType::Text) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_XHR_HAS_WRONG_RESPONSETYPE_FOR_RESPONSETEXT);
    return;
  }

  if (mState != XMLHttpRequestBinding::LOADING &&
      mState != XMLHttpRequestBinding::DONE) {
    return;
  }

  // Main Fetch step 18 requires to ignore body for head/connect methods.
  if (mRequestMethod.EqualsLiteral("HEAD") ||
      mRequestMethod.EqualsLiteral("CONNECT")) {
    return;
  }

  // We only decode text lazily if we're also parsing to a doc.
  // Also, if we've decoded all current data already, then no need to decode
  // more.
  if ((!mResponseXML && !mErrorParsingXML) ||
      mResponseBodyDecodedPos == mResponseBody.Length()) {
    mResponseText.CreateSnapshot(aSnapshot);
    return;
  }

  MatchCharsetAndDecoderToResponseDocument();

  NS_ASSERTION(mResponseBodyDecodedPos < mResponseBody.Length(),
               "Unexpected mResponseBodyDecodedPos");
  aRv = AppendToResponseText(mResponseBody.get() + mResponseBodyDecodedPos,
                             mResponseBody.Length() - mResponseBodyDecodedPos);
  if (aRv.Failed()) {
    return;
  }

  mResponseBodyDecodedPos = mResponseBody.Length();

  if (mState == XMLHttpRequestBinding::DONE) {
    // Free memory buffer which we no longer need
    mResponseBody.Truncate();
    mResponseBodyDecodedPos = 0;
  }

  mResponseText.CreateSnapshot(aSnapshot);
}

nsresult
XMLHttpRequestMainThread::CreateResponseParsedJSON(JSContext* aCx)
{
  if (!aCx) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString string;
  if (!mResponseText.GetAsString(string)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // The Unicode converter has already zapped the BOM if there was one
  JS::Rooted<JS::Value> value(aCx);
  if (!JS_ParseJSON(aCx, string.BeginReading(), string.Length(), &value)) {
    return NS_ERROR_FAILURE;
  }

  mResultJSON = value;
  return NS_OK;
}

void
XMLHttpRequestMainThread::SetResponseType(XMLHttpRequestResponseType aResponseType,
                                          ErrorResult& aRv)
{
  NOT_CALLABLE_IN_SYNC_SEND_RV

  if (mState == XMLHttpRequestBinding::LOADING ||
      mState == XMLHttpRequestBinding::DONE) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_XHR_MUST_NOT_BE_LOADING_OR_DONE);
    return;
  }

  // sync request is not allowed setting responseType in window context
  if (HasOrHasHadOwner() &&
      mState != XMLHttpRequestBinding::UNSENT && mFlagSynchronous) {
    LogMessage("ResponseTypeSyncXHRWarning", GetOwner());
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_XHR_TIMEOUT_AND_RESPONSETYPE_UNSUPPORTED_FOR_SYNC);
    return;
  }

  if (mFlagSynchronous &&
      aResponseType == XMLHttpRequestResponseType::Moz_chunked_arraybuffer) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_XHR_CHUNKED_RESPONSETYPES_UNSUPPORTED_FOR_SYNC);
    return;
  }

  // We want to get rid of this moz-only types. Bug 1335365.
  if (aResponseType == XMLHttpRequestResponseType::Moz_chunked_arraybuffer) {
    Telemetry::Accumulate(Telemetry::MOZ_CHUNKED_ARRAYBUFFER_IN_XHR, 1);
  }

  // Set the responseType attribute's value to the given value.
  mResponseType = aResponseType;
}

void
XMLHttpRequestMainThread::GetResponse(JSContext* aCx,
                                      JS::MutableHandle<JS::Value> aResponse,
                                      ErrorResult& aRv)
{
  switch (mResponseType) {
  case XMLHttpRequestResponseType::_empty:
  case XMLHttpRequestResponseType::Text:
  {
    DOMString str;
    GetResponseText(str, aRv);
    if (aRv.Failed()) {
      return;
    }
    if (!xpc::StringToJsval(aCx, str, aResponse)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    }
    return;
  }

  case XMLHttpRequestResponseType::Arraybuffer:
  case XMLHttpRequestResponseType::Moz_chunked_arraybuffer:
  {
    if (!(mResponseType == XMLHttpRequestResponseType::Arraybuffer &&
          mState == XMLHttpRequestBinding::DONE) &&
        !(mResponseType == XMLHttpRequestResponseType::Moz_chunked_arraybuffer &&
          mInLoadProgressEvent)) {
      aResponse.setNull();
      return;
    }

    if (!mResultArrayBuffer) {
      mResultArrayBuffer = mArrayBufferBuilder.getArrayBuffer(aCx);
      if (!mResultArrayBuffer) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return;
      }
    }
    aResponse.setObject(*mResultArrayBuffer);
    return;
  }
  case XMLHttpRequestResponseType::Blob:
  {
    if (mState != XMLHttpRequestBinding::DONE) {
      aResponse.setNull();
      return;
    }

    if (!mResponseBlob) {
      aResponse.setNull();
      return;
    }

    GetOrCreateDOMReflector(aCx, mResponseBlob, aResponse);
    return;
  }
  case XMLHttpRequestResponseType::Document:
  {
    if (!mResponseXML || mState != XMLHttpRequestBinding::DONE) {
      aResponse.setNull();
      return;
    }

    aRv = nsContentUtils::WrapNative(aCx, mResponseXML, aResponse);
    return;
  }
  case XMLHttpRequestResponseType::Json:
  {
    if (mState != XMLHttpRequestBinding::DONE) {
      aResponse.setNull();
      return;
    }

    if (mResultJSON.isUndefined()) {
      aRv = CreateResponseParsedJSON(aCx);
      TruncateResponseText();
      if (aRv.Failed()) {
        // Per spec, errors aren't propagated. null is returned instead.
        aRv = NS_OK;
        // It would be nice to log the error to the console. That's hard to
        // do without calling window.onerror as a side effect, though.
        JS_ClearPendingException(aCx);
        mResultJSON.setNull();
      }
    }
    aResponse.set(mResultJSON);
    return;
  }
  default:
    NS_ERROR("Should not happen");
  }

  aResponse.setNull();
}

bool
XMLHttpRequestMainThread::IsCrossSiteCORSRequest() const
{
  if (!mChannel) {
    return false;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = mChannel->GetLoadInfo();
  MOZ_ASSERT(loadInfo);

  if (!loadInfo) {
    return false;
  }

  return loadInfo->GetTainting() == LoadTainting::CORS;
}

bool
XMLHttpRequestMainThread::IsDeniedCrossSiteCORSRequest()
{
  if (IsCrossSiteCORSRequest()) {
    nsresult rv;
    mChannel->GetStatus(&rv);
    if (NS_FAILED(rv)) {
      return true;
    }
  }
  return false;
}

void
XMLHttpRequestMainThread::GetResponseURL(nsAString& aUrl)
{
  aUrl.Truncate();

  if ((mState == XMLHttpRequestBinding::UNSENT ||
       mState == XMLHttpRequestBinding::OPENED) || !mChannel) {
    return;
  }

  // Make sure we don't leak responseURL information from denied cross-site
  // requests.
  if (IsDeniedCrossSiteCORSRequest()) {
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

uint32_t
XMLHttpRequestMainThread::GetStatus(ErrorResult& aRv)
{
  // Make sure we don't leak status information from denied cross-site
  // requests.
  if (IsDeniedCrossSiteCORSRequest()) {
    return 0;
  }

  if (mState == XMLHttpRequestBinding::UNSENT ||
      mState == XMLHttpRequestBinding::OPENED) {
    return 0;
  }

  if (mErrorLoad != ErrorType::eOK) {
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

void
XMLHttpRequestMainThread::GetStatusText(nsACString& aStatusText,
                                        ErrorResult& aRv)
{
  // Return an empty status text on all error loads.
  aStatusText.Truncate();

  // Make sure we don't leak status information from denied cross-site
  // requests.
  if (IsDeniedCrossSiteCORSRequest()) {
    return;
  }

  // Check the current XHR state to see if it is valid to obtain the statusText
  // value.  This check is to prevent the status text for redirects from being
  // available before all the redirects have been followed and HTTP headers have
  // been received.
  if (mState == XMLHttpRequestBinding::UNSENT ||
      mState == XMLHttpRequestBinding::OPENED) {
    return;
  }

  if (mErrorLoad != ErrorType::eOK) {
    return;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = GetCurrentHttpChannel();
  if (httpChannel) {
    Unused << httpChannel->GetResponseStatusText(aStatusText);
  } else {
    aStatusText.AssignLiteral("OK");
  }
}

void
XMLHttpRequestMainThread::TerminateOngoingFetch() {
  if ((mState == XMLHttpRequestBinding::OPENED && mFlagSend) ||
      mState == XMLHttpRequestBinding::HEADERS_RECEIVED ||
      mState == XMLHttpRequestBinding::LOADING) {
    CloseRequest();
  }
}

void
XMLHttpRequestMainThread::CloseRequest()
{
  mWaitingForOnStopRequest = false;
  mErrorLoad = ErrorType::eTerminated;
  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
  }
  if (mTimeoutTimer) {
    mTimeoutTimer->Cancel();
  }
}

void
XMLHttpRequestMainThread::CloseRequestWithError(const ProgressEventType aType)
{
  CloseRequest();

  ResetResponse();

  // If we're in the destructor, don't risk dispatching an event.
  if (mFlagDeleted) {
    mFlagSyncLooping = false;
    return;
  }

  if (mState != XMLHttpRequestBinding::UNSENT &&
      !(mState == XMLHttpRequestBinding::OPENED && !mFlagSend) &&
      mState != XMLHttpRequestBinding::DONE) {
    ChangeState(XMLHttpRequestBinding::DONE, true);

    if (!mFlagSyncLooping) {
      if (mUpload && !mUploadComplete) {
        mUploadComplete = true;
        DispatchProgressEvent(mUpload, aType, 0, -1);
      }
      DispatchProgressEvent(this, aType, 0, -1);
    }
  }

  // The ChangeState call above calls onreadystatechange handlers which
  // if they load a new url will cause XMLHttpRequestMainThread::Open to clear
  // the abort state bit. If this occurs we're not uninitialized (bug 361773).
  if (mFlagAborted) {
    ChangeState(XMLHttpRequestBinding::UNSENT, false);  // IE seems to do it
  }

  mFlagSyncLooping = false;
}

void
XMLHttpRequestMainThread::RequestErrorSteps(const ProgressEventType aEventType,
                                            const nsresult aOptionalException,
                                            ErrorResult& aRv)
{
  // Step 1
  mState = XMLHttpRequestBinding::DONE;

  StopProgressEventTimer();

  // Step 2
  mFlagSend = false;

  // Step 3
  ResetResponse();

  // If we're in the destructor, don't risk dispatching an event.
  if (mFlagDeleted) {
    mFlagSyncLooping = false;
    return;
  }

  // Step 4
  if (mFlagSynchronous && NS_FAILED(aOptionalException)) {
    aRv.Throw(aOptionalException);
    return;
  }

  // Step 5
  FireReadystatechangeEvent();

  // Step 6
  if (mUpload && !mUploadComplete) {

    // Step 6-1
    mUploadComplete = true;

    // Step 6-2
    if (mFlagHadUploadListenersOnSend) {

      // Steps 6-3, 6-4 (loadend is fired for us)
      DispatchProgressEvent(mUpload, aEventType, 0, -1);
    }
  }

  // Steps 7 and 8 (loadend is fired for us)
  DispatchProgressEvent(this, aEventType, 0, -1);
}

void
XMLHttpRequestMainThread::Abort(ErrorResult& aRv)
{
  NOT_CALLABLE_IN_SYNC_SEND_RV
  AbortInternal(aRv);
}

void
XMLHttpRequestMainThread::AbortInternal(ErrorResult& aRv)
{
  mFlagAborted = true;

  // Step 1
  TerminateOngoingFetch();

  // Step 2
  if ((mState == XMLHttpRequestBinding::OPENED && mFlagSend) ||
       mState == XMLHttpRequestBinding::HEADERS_RECEIVED ||
       mState == XMLHttpRequestBinding::LOADING) {
    RequestErrorSteps(ProgressEventType::abort, NS_OK, aRv);
  }

  // Step 3
  if (mState == XMLHttpRequestBinding::DONE) {
    ChangeState(XMLHttpRequestBinding::UNSENT, false); // no ReadystateChange event
  }

  mFlagSyncLooping = false;
}

/*Method that checks if it is safe to expose a header value to the client.
It is used to check what headers are exposed for CORS requests.*/
bool
XMLHttpRequestMainThread::IsSafeHeader(const nsACString& aHeader,
                                       NotNull<nsIHttpChannel*> aHttpChannel) const
{
  // See bug #380418. Hide "Set-Cookie" headers from non-chrome scripts.
  if (!IsSystemXHR() && nsContentUtils::IsForbiddenResponseHeader(aHeader)) {
    NS_WARNING("blocked access to response header");
    return false;
  }
  // if this is not a CORS call all headers are safe
  if (!IsCrossSiteCORSRequest()) {
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
    if (aHeader.LowerCaseEqualsASCII(kCrossOriginSafeHeaders[i])) {
      return true;
    }
  }
  nsAutoCString headerVal;
  // The "Access-Control-Expose-Headers" header contains a comma separated
  // list of method names.
  Unused << aHttpChannel->
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
    if (aHeader.Equals(token, nsCaseInsensitiveCStringComparator())) {
      isSafe = true;
    }
  }
  return isSafe;
}

void
XMLHttpRequestMainThread::GetAllResponseHeaders(nsACString& aResponseHeaders,
                                                ErrorResult& aRv)
{
  NOT_CALLABLE_IN_SYNC_SEND_RV

  aResponseHeaders.Truncate();

  // If the state is UNSENT or OPENED,
  // return the empty string and terminate these steps.
  if (mState == XMLHttpRequestBinding::UNSENT ||
      mState == XMLHttpRequestBinding::OPENED) {
    return;
  }

  if (mErrorLoad != ErrorType::eOK) {
    return;
  }

  if (nsCOMPtr<nsIHttpChannel> httpChannel = GetCurrentHttpChannel()) {
    RefPtr<nsHeaderVisitor> visitor =
      new nsHeaderVisitor(*this, WrapNotNull(httpChannel));
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

  // Don't provide Content-Length for data URIs
  nsCOMPtr<nsIURI> uri;
  bool isDataURI;
  if (NS_FAILED(mChannel->GetURI(getter_AddRefs(uri))) ||
      NS_FAILED(uri->SchemeIs("data", &isDataURI)) ||
      !isDataURI) {
    int64_t length;
    if (NS_SUCCEEDED(mChannel->GetContentLength(&length))) {
      aResponseHeaders.AppendLiteral("Content-Length: ");
      aResponseHeaders.AppendInt(length);
      aResponseHeaders.AppendLiteral("\r\n");
    }
  }
}

void
XMLHttpRequestMainThread::GetResponseHeader(const nsACString& header,
                                            nsACString& _retval, ErrorResult& aRv)
{
  NOT_CALLABLE_IN_SYNC_SEND_RV

  _retval.SetIsVoid(true);

  nsCOMPtr<nsIHttpChannel> httpChannel = GetCurrentHttpChannel();

  if (!httpChannel) {
    // If the state is UNSENT or OPENED,
    // return null and terminate these steps.
    if (mState == XMLHttpRequestBinding::UNSENT ||
        mState == XMLHttpRequestBinding::OPENED) {
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
  if (!IsSafeHeader(header, WrapNotNull(httpChannel))) {
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
XMLHttpRequestMainThread::GetLoadGroup() const
{
  if (mFlagBackgroundRequest) {
    return nullptr;
  }

  if (mLoadGroup) {
    nsCOMPtr<nsILoadGroup> ref = mLoadGroup;
    return ref.forget();
  }

  nsIDocument* doc = GetDocumentIfCurrent();
  if (doc) {
    return doc->GetDocumentLoadGroup();
  }

  return nullptr;
}

nsresult
XMLHttpRequestMainThread::FireReadystatechangeEvent()
{
  MOZ_ASSERT(mState != XMLHttpRequestBinding::UNSENT);
  RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);
  event->InitEvent(kLiteralString_readystatechange, false, false);
  // We assume anyone who managed to call CreateReadystatechangeEvent is trusted
  event->SetTrusted(true);
  DispatchOrStoreEvent(this, event);
  return NS_OK;
}

void
XMLHttpRequestMainThread::DispatchProgressEvent(DOMEventTargetHelper* aTarget,
                                                const ProgressEventType aType,
                                                int64_t aLoaded, int64_t aTotal)
{
  NS_ASSERTION(aTarget, "null target");

  if (NS_FAILED(CheckInnerWindowCorrectness()) ||
      (!AllowUploadProgress() && aTarget == mUpload)) {
    return;
  }

  // If blocked by CORS, zero-out the stats on progress events
  // and never fire "progress" or "load" events at all.
  if (IsDeniedCrossSiteCORSRequest()) {
    if (aType == ProgressEventType::progress ||
        aType == ProgressEventType::load) {
      return;
    }
    aLoaded = 0;
    aTotal = -1;
  }

  if (aType == ProgressEventType::progress) {
    mInLoadProgressEvent = true;
  }

  ProgressEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mLengthComputable = aTotal != -1; // XHR spec step 6.1
  init.mLoaded = aLoaded;
  init.mTotal = (aTotal == -1) ? 0 : aTotal;

  const nsAString& typeString = ProgressEventTypeStrings[(uint8_t)aType];
  RefPtr<ProgressEvent> event =
    ProgressEvent::Constructor(aTarget, typeString, init);
  event->SetTrusted(true);

  DispatchOrStoreEvent(aTarget, event);

  if (aType == ProgressEventType::progress) {
    mInLoadProgressEvent = false;

    // clear chunked responses after every progress event
    if (mResponseType == XMLHttpRequestResponseType::Moz_chunked_arraybuffer) {
      mResponseBody.Truncate();
      TruncateResponseText();
      mResultArrayBuffer = nullptr;
      mArrayBufferBuilder.reset();
    }
  }

  // If we're sending a load, error, timeout or abort event, then
  // also dispatch the subsequent loadend event.
  if (aType == ProgressEventType::load || aType == ProgressEventType::error ||
      aType == ProgressEventType::timeout || aType == ProgressEventType::abort) {
    DispatchProgressEvent(aTarget, ProgressEventType::loadend, aLoaded, aTotal);
  }
}

void
XMLHttpRequestMainThread::DispatchOrStoreEvent(DOMEventTargetHelper* aTarget,
                                               Event* aEvent)
{
  MOZ_ASSERT(aTarget);
  MOZ_ASSERT(aEvent);

  if (NS_FAILED(CheckInnerWindowCorrectness())) {
    return;
  }

  if (mEventDispatchingSuspended) {
    PendingEvent* event = mPendingEvents.AppendElement();
    event->mTarget = aTarget;
    event->mEvent = aEvent;
    return;
  }

  aTarget->DispatchEvent(*aEvent);
}

void
XMLHttpRequestMainThread::SuspendEventDispatching()
{
  MOZ_ASSERT(!mEventDispatchingSuspended);
  mEventDispatchingSuspended = true;
}

void
XMLHttpRequestMainThread::ResumeEventDispatching()
{
  MOZ_ASSERT(mEventDispatchingSuspended);
  mEventDispatchingSuspended = false;

  nsTArray<PendingEvent> pendingEvents;
  pendingEvents.SwapElements(mPendingEvents);

  if (NS_FAILED(CheckInnerWindowCorrectness())) {
    return;
  }

  for (uint32_t i = 0; i < pendingEvents.Length(); ++i) {
    pendingEvents[i].mTarget->DispatchEvent(*pendingEvents[i].mEvent);
  }
}

already_AddRefed<nsIHttpChannel>
XMLHttpRequestMainThread::GetCurrentHttpChannel()
{
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
  return httpChannel.forget();
}

already_AddRefed<nsIJARChannel>
XMLHttpRequestMainThread::GetCurrentJARChannel()
{
  nsCOMPtr<nsIJARChannel> appChannel = do_QueryInterface(mChannel);
  return appChannel.forget();
}

bool
XMLHttpRequestMainThread::IsSystemXHR() const
{
  return mIsSystem || nsContentUtils::IsSystemPrincipal(mPrincipal);
}

bool
XMLHttpRequestMainThread::InUploadPhase() const
{
  // We're in the upload phase while our state is OPENED.
  return mState == XMLHttpRequestBinding::OPENED;
}

// This case is hit when the async parameter is outright omitted, which
// should set it to true (and the username and password to null).
void
XMLHttpRequestMainThread::Open(const nsACString& aMethod, const nsAString& aUrl,
                               ErrorResult& aRv)
{
  Open(aMethod, aUrl, true, VoidString(), VoidString(), aRv);
}

// This case is hit when the async parameter is specified, even if the
// JS value was "undefined" (which due to legacy reasons should be
// treated as true, which is how it will already be passed in here).
void
XMLHttpRequestMainThread::Open(const nsACString& aMethod,
                               const nsAString& aUrl,
                               bool aAsync,
                               const nsAString& aUsername,
                               const nsAString& aPassword,
                               ErrorResult& aRv)
{
  nsresult rv = Open(aMethod, NS_ConvertUTF16toUTF8(aUrl), aAsync, aUsername, aPassword);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

nsresult
XMLHttpRequestMainThread::Open(const nsACString& aMethod,
                               const nsACString& aUrl,
                               bool aAsync,
                               const nsAString& aUsername,
                               const nsAString& aPassword)
{
  NOT_CALLABLE_IN_SYNC_SEND

  // Gecko-specific
  if (!aAsync && !DontWarnAboutSyncXHR() && GetOwner() &&
      GetOwner()->GetExtantDoc()) {
    GetOwner()->GetExtantDoc()->WarnOnceAbout(nsIDocument::eSyncXMLHttpRequest);
  }

  Telemetry::Accumulate(Telemetry::XMLHTTPREQUEST_ASYNC_OR_SYNC, aAsync ? 0 : 1);

  // Step 1
  nsCOMPtr<nsIDocument> responsibleDocument = GetDocumentIfCurrent();
  if (!responsibleDocument) {
    // This could be because we're no longer current or because we're in some
    // non-window context...
    nsresult rv = CheckInnerWindowCorrectness();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_ERROR_DOM_INVALID_STATE_XHR_HAS_INVALID_CONTEXT;
    }
  }
  NS_ENSURE_TRUE(mPrincipal, NS_ERROR_NOT_INITIALIZED);

  // Steps 2-4
  nsAutoCString method;
  nsresult rv = FetchUtil::GetValidRequestMethod(aMethod, method);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Steps 5-6
  nsCOMPtr<nsIURI> baseURI;
  if (mBaseURI) {
    baseURI = mBaseURI;
  } else if (responsibleDocument) {
    baseURI = responsibleDocument->GetBaseURI();
  }

  // Use the responsible document's encoding for the URL if we have one,
  // except for dedicated workers. Use UTF-8 otherwise.
  NotNull<const Encoding*> originCharset = UTF_8_ENCODING;
  if (responsibleDocument &&
      responsibleDocument->NodePrincipal() == mPrincipal) {
    originCharset = responsibleDocument->GetDocumentCharacterSet();
  }

  nsCOMPtr<nsIURI> parsedURL;
  rv = NS_NewURI(getter_AddRefs(parsedURL), aUrl, originCharset, baseURI);
  if (NS_FAILED(rv)) {
    if (rv ==  NS_ERROR_MALFORMED_URI) {
      return NS_ERROR_DOM_MALFORMED_URI;
    }
    return rv;
  }
  if (NS_WARN_IF(NS_FAILED(CheckInnerWindowCorrectness()))) {
    return NS_ERROR_DOM_INVALID_STATE_XHR_HAS_INVALID_CONTEXT;
  }

  // Step 7
  // This is already handled by the other Open() method, which passes
  // username and password in as NullStrings.

  // Step 8
  nsAutoCString host;
  parsedURL->GetHost(host);
  if (!host.IsEmpty()) {
    if (!aUsername.IsVoid() || !aPassword.IsVoid()) {
      Unused << NS_MutateURI(parsedURL)
                  .SetUsername(NS_ConvertUTF16toUTF8(aUsername))
                  .SetPassword(NS_ConvertUTF16toUTF8(aPassword))
                  .Finalize(parsedURL);
    }
  }

  // Step 9
  if (!aAsync && HasOrHasHadOwner() && (mTimeoutMilliseconds ||
       mResponseType != XMLHttpRequestResponseType::_empty)) {
    if (mTimeoutMilliseconds) {
      LogMessage("TimeoutSyncXHRWarning", GetOwner());
    }
    if (mResponseType != XMLHttpRequestResponseType::_empty) {
      LogMessage("ResponseTypeSyncXHRWarning", GetOwner());
    }
    return NS_ERROR_DOM_INVALID_ACCESS_XHR_TIMEOUT_AND_RESPONSETYPE_UNSUPPORTED_FOR_SYNC;
  }

  // Step 10
  TerminateOngoingFetch();

  // Step 11
  // timeouts are handled without a flag
  mFlagSend = false;
  mRequestMethod.Assign(method);
  mRequestURL = parsedURL;
  mFlagSynchronous = !aAsync;
  mAuthorRequestHeaders.Clear();
  ResetResponse();

  // Gecko-specific
  mFlagHadUploadListenersOnSend = false;
  mFlagAborted = false;
  mFlagTimedOut = false;

  // Per spec we should only create the channel on send(), but we have internal
  // code that relies on the channel being created now, and that code is not
  // always IsSystemXHR(). However, we're not supposed to throw channel-creation
  // errors during open(), so we silently ignore those here.
  CreateChannel();

  // Step 12
  if (mState != XMLHttpRequestBinding::OPENED) {
    mState = XMLHttpRequestBinding::OPENED;
    FireReadystatechangeEvent();
  }

  return NS_OK;
}

void
XMLHttpRequestMainThread::SetOriginAttributes(const OriginAttributesDictionary& aAttrs)
{
  MOZ_ASSERT((mState == XMLHttpRequestBinding::OPENED) && !mFlagSend);

  OriginAttributes attrs(aAttrs);

  nsCOMPtr<nsILoadInfo> loadInfo = mChannel->GetLoadInfo();
  MOZ_ASSERT(loadInfo);
  if (loadInfo) {
    loadInfo->SetOriginAttributes(attrs);
  }
}

void
XMLHttpRequestMainThread::PopulateNetworkInterfaceId()
{
  if (mNetworkInterfaceId.IsEmpty()) {
    return;
  }
  nsCOMPtr<nsIHttpChannelInternal> channel(do_QueryInterface(mChannel));
  if (!channel) {
    return;
  }
  DebugOnly<nsresult> rv = channel->SetNetworkInterfaceId(mNetworkInterfaceId);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

/*
 * "Copy" from a stream.
 */
nsresult
XMLHttpRequestMainThread::StreamReaderFunc(nsIInputStream* in,
                                           void* closure,
                                           const char* fromRawSegment,
                                           uint32_t toOffset,
                                           uint32_t count,
                                           uint32_t *writeCount)
{
  XMLHttpRequestMainThread* xmlHttpRequest = static_cast<XMLHttpRequestMainThread*>(closure);
  if (!xmlHttpRequest || !writeCount) {
    NS_WARNING("XMLHttpRequest cannot read from stream: no closure or writeCount");
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_OK;

  if (xmlHttpRequest->mResponseType == XMLHttpRequestResponseType::Blob) {
    xmlHttpRequest->MaybeCreateBlobStorage();
    rv = xmlHttpRequest->mBlobStorage->Append(fromRawSegment, count);
  } else if ((xmlHttpRequest->mResponseType == XMLHttpRequestResponseType::Arraybuffer &&
              !xmlHttpRequest->mIsMappedArrayBuffer) ||
             xmlHttpRequest->mResponseType == XMLHttpRequestResponseType::Moz_chunked_arraybuffer) {
    // get the initial capacity to something reasonable to avoid a bunch of reallocs right
    // at the start
    if (xmlHttpRequest->mArrayBufferBuilder.capacity() == 0)
      xmlHttpRequest->mArrayBufferBuilder.setCapacity(std::max(count, XML_HTTP_REQUEST_ARRAYBUFFER_MIN_SIZE));

    if (NS_WARN_IF(!xmlHttpRequest->mArrayBufferBuilder.append(
                      reinterpret_cast<const uint8_t*>(fromRawSegment),
                      count,
                      XML_HTTP_REQUEST_ARRAYBUFFER_MAX_GROWTH))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

  } else if (xmlHttpRequest->mResponseType == XMLHttpRequestResponseType::_empty &&
             xmlHttpRequest->mResponseXML) {
    // Copy for our own use
    if (!xmlHttpRequest->mResponseBody.Append(fromRawSegment, count, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else if (xmlHttpRequest->mResponseType == XMLHttpRequestResponseType::_empty ||
             xmlHttpRequest->mResponseType == XMLHttpRequestResponseType::Text ||
             xmlHttpRequest->mResponseType == XMLHttpRequestResponseType::Json) {
    NS_ASSERTION(!xmlHttpRequest->mResponseXML,
                 "We shouldn't be parsing a doc here");
    rv = xmlHttpRequest->AppendToResponseText(fromRawSegment, count);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (xmlHttpRequest->mFlagParseBody) {
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
        xmlHttpRequest->mFlagParseBody = false;
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

namespace {

nsresult
GetLocalFileFromChannel(nsIRequest* aRequest, nsIFile** aFile)
{
  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(aFile);

  *aFile = nullptr;

  nsCOMPtr<nsIFileChannel> fc = do_QueryInterface(aRequest);
  if (!fc) {
    return NS_OK;
  }

  nsCOMPtr<nsIFile> file;
  nsresult rv = fc->GetFile(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  file.forget(aFile);
  return NS_OK;
}

nsresult
DummyStreamReaderFunc(nsIInputStream* aInputStream,
                      void* aClosure,
                      const char* aFromRawSegment,
                      uint32_t aToOffset,
                      uint32_t aCount,
                      uint32_t* aWriteCount)
{
  *aWriteCount = aCount;
  return NS_OK;
}

class FileCreationHandler final : public PromiseNativeHandler
{
public:
  NS_DECL_ISUPPORTS

  static void
  Create(Promise* aPromise, XMLHttpRequestMainThread* aXHR)
  {
    MOZ_ASSERT(aPromise);

    RefPtr<FileCreationHandler> handler = new FileCreationHandler(aXHR);
    aPromise->AppendNativeHandler(handler);
  }

  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    if (NS_WARN_IF(!aValue.isObject())) {
      mXHR->LocalFileToBlobCompleted(nullptr);
      return;
    }

    RefPtr<Blob> blob;
    if (NS_WARN_IF(NS_FAILED(UNWRAP_OBJECT(Blob, &aValue.toObject(), blob)))) {
      mXHR->LocalFileToBlobCompleted(nullptr);
      return;
    }

    mXHR->LocalFileToBlobCompleted(blob);
  }

  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    mXHR->LocalFileToBlobCompleted(nullptr);
  }

private:
  explicit FileCreationHandler(XMLHttpRequestMainThread* aXHR)
    : mXHR(aXHR)
  {
    MOZ_ASSERT(aXHR);
  }

  ~FileCreationHandler() = default;

  RefPtr<XMLHttpRequestMainThread> mXHR;
};

NS_IMPL_ISUPPORTS0(FileCreationHandler)

} // namespace

void
XMLHttpRequestMainThread::LocalFileToBlobCompleted(Blob* aBlob)
{
  MOZ_ASSERT(mState != XMLHttpRequestBinding::DONE);

  mResponseBlob = aBlob;
  mBlobStorage = nullptr;
  NS_ASSERTION(mResponseBody.IsEmpty(), "mResponseBody should be empty");

  ChangeStateToDone();
}

NS_IMETHODIMP
XMLHttpRequestMainThread::OnDataAvailable(nsIRequest *request,
                                          nsISupports *ctxt,
                                          nsIInputStream *inStr,
                                          uint64_t sourceOffset,
                                          uint32_t count)
{
  NS_ENSURE_ARG_POINTER(inStr);

  MOZ_ASSERT(mContext.get() == ctxt,"start context different from OnDataAvailable context");

  mProgressSinceLastProgressEvent = true;
  XMLHttpRequestBinding::ClearCachedResponseTextValue(this);

  nsresult rv;

  nsCOMPtr<nsIFile> localFile;
  if (mResponseType == XMLHttpRequestResponseType::Blob) {
    rv = GetLocalFileFromChannel(request, getter_AddRefs(localFile));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (localFile) {
      mBlobStorage = nullptr;
      NS_ASSERTION(mResponseBody.IsEmpty(), "mResponseBody should be empty");

      // The nsIStreamListener contract mandates us to read from the stream
      // before returning.
      uint32_t totalRead;
      rv =
        inStr->ReadSegments(DummyStreamReaderFunc, nullptr, count, &totalRead);
      NS_ENSURE_SUCCESS(rv, rv);

      ChangeState(XMLHttpRequestBinding::LOADING);

      // Cancel() must be called with an error. We use
      // NS_ERROR_FILE_ALREADY_EXISTS to know that we've aborted the operation
      // just because we can retrieve the File from the channel directly.
      return request->Cancel(NS_ERROR_FILE_ALREADY_EXISTS);
    }
  }

  uint32_t totalRead;
  rv = inStr->ReadSegments(XMLHttpRequestMainThread::StreamReaderFunc,
                           (void*)this, count, &totalRead);
  NS_ENSURE_SUCCESS(rv, rv);

  // Fire the first progress event/loading state change
  if (mState == XMLHttpRequestBinding::HEADERS_RECEIVED) {
    ChangeState(XMLHttpRequestBinding::LOADING);
    if (!mFlagSynchronous) {
      DispatchProgressEvent(this, ProgressEventType::progress,
                            mLoadTransferred, mLoadTotal);
    }
    mProgressSinceLastProgressEvent = false;
  }

  if (!mFlagSynchronous && !mProgressTimerIsActive) {
    StartProgressEventTimer();
  }

  return NS_OK;
}

NS_IMETHODIMP
XMLHttpRequestMainThread::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  AUTO_PROFILER_LABEL("XMLHttpRequestMainThread::OnStartRequest", NETWORK);

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
  if (mState == XMLHttpRequestBinding::UNSENT) {
    return NS_OK;
  }

  /* Apparently, Abort() should set UNSENT.  See bug 361773.
     XHR2 spec says this is correct. */
  if (mFlagAborted) {
    NS_ERROR("Ugh, still getting data on an aborted XMLHttpRequest!");

    return NS_ERROR_UNEXPECTED;
  }

  // Don't do anything if we have timed out.
  if (mFlagTimedOut) {
    return NS_OK;
  }

  nsCOMPtr<nsIChannel> channel(do_QueryInterface(request));
  NS_ENSURE_TRUE(channel, NS_ERROR_UNEXPECTED);

  nsresult status;
  request->GetStatus(&status);
  if (mErrorLoad == ErrorType::eOK && NS_FAILED(status)) {
    mErrorLoad = ErrorType::eRequest;
  }

  // Upload phase is now over. If we were uploading anything,
  // stop the timer and fire any final progress events.
  if (mUpload && !mUploadComplete && mErrorLoad == ErrorType::eOK && !mFlagSynchronous) {
    StopProgressEventTimer();

    mUploadTransferred = mUploadTotal;

    if (mProgressSinceLastProgressEvent) {
      DispatchProgressEvent(mUpload, ProgressEventType::progress,
                            mUploadTransferred, mUploadTotal);
      mProgressSinceLastProgressEvent = false;
    }

    mUploadComplete = true;
    DispatchProgressEvent(mUpload, ProgressEventType::load,
                          mUploadTotal, mUploadTotal);
  }

  mContext = ctxt;
  mFlagParseBody = true;
  ChangeState(XMLHttpRequestBinding::HEADERS_RECEIVED);

  ResetResponse();

  if (!mOverrideMimeType.IsEmpty()) {
    channel->SetContentType(NS_ConvertUTF16toUTF8(mOverrideMimeType));
  }

  // Fallback to 'application/octet-stream'
  nsAutoCString type;
  channel->GetContentType(type);
  if (type.EqualsLiteral(UNKNOWN_CONTENT_TYPE)) {
    channel->SetContentType(NS_LITERAL_CSTRING(APPLICATION_OCTET_STREAM));
  }

  DetectCharset();

  // Set up arraybuffer
  if (mResponseType == XMLHttpRequestResponseType::Arraybuffer &&
      NS_SUCCEEDED(status)) {
    if (mIsMappedArrayBuffer) {
      nsCOMPtr<nsIJARChannel> jarChannel = do_QueryInterface(channel);
      if (jarChannel) {
        nsCOMPtr<nsIURI> uri;
        rv = channel->GetURI(getter_AddRefs(uri));
        if (NS_SUCCEEDED(rv)) {
          nsAutoCString file;
          nsAutoCString scheme;
          uri->GetScheme(scheme);
          if (scheme.LowerCaseEqualsLiteral("jar")) {
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
            // This can happen legitimately if there are compressed files
            // in the jarFile. See bug #1357219. No need to warn on the error.
            if (NS_FAILED(rv)) {
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
  // Note: Main Fetch step 18 requires to ignore body for head/connect methods.
  bool parseBody = (mResponseType == XMLHttpRequestResponseType::_empty ||
                    mResponseType == XMLHttpRequestResponseType::Document) &&
                   !(mRequestMethod.EqualsLiteral("HEAD") ||
                     mRequestMethod.EqualsLiteral("CONNECT"));

  mIsHtml = false;
  mWarnAboutSyncHtml = false;
  if (parseBody && NS_SUCCEEDED(status)) {
    // We can gain a huge performance win by not even trying to
    // parse non-XML data. This also protects us from the situation
    // where we have an XML document and sink, but HTML (or other)
    // parser, which can produce unreliable results.
    nsAutoCString type;
    channel->GetContentType(type);

    if ((mResponseType == XMLHttpRequestResponseType::Document) &&
        type.EqualsLiteral("text/html")) {
      // HTML parsing is only supported for responseType == "document" to
      // avoid running the parser and, worse, populating responseXML for
      // legacy users of XHR who use responseType == "" for retrieving the
      // responseText of text/html resources. This legacy case is so common
      // that it's not useful to emit a warning about it.
      if (mFlagSynchronous) {
        // We don't make cool new features available in the bad synchronous
        // mode. The synchronous mode is for legacy only.
        mWarnAboutSyncHtml = true;
        mFlagParseBody = false;
      } else {
        mIsHtml = true;
      }
    } else if (!(type.EqualsLiteral("text/xml") ||
                 type.EqualsLiteral("application/xml") ||
                 type.RFind("+xml", true, -1, 4) != kNotFound)) {
      // Follow https://xhr.spec.whatwg.org/
      // If final MIME type is not null, text/html, text/xml, application/xml,
      // or does not end in +xml, return null.
      mFlagParseBody = false;
    }
  } else {
    // The request failed, so we shouldn't be parsing anyway
    mFlagParseBody = false;
  }

  if (mFlagParseBody) {
    nsCOMPtr<nsIURI> baseURI, docURI;
    rv = mChannel->GetURI(getter_AddRefs(docURI));
    NS_ENSURE_SUCCESS(rv, rv);
    baseURI = docURI;

    nsCOMPtr<nsIDocument> doc = GetDocumentIfCurrent();
    nsCOMPtr<nsIURI> chromeXHRDocURI, chromeXHRDocBaseURI;
    if (doc) {
      chromeXHRDocURI = doc->GetDocumentURI();
      chromeXHRDocBaseURI = doc->GetBaseURI();
    } else {
      // If we're no longer current, just kill the load, though it really should
      // have been killed already.
      if (NS_WARN_IF(NS_FAILED(CheckInnerWindowCorrectness()))) {
        return NS_ERROR_DOM_INVALID_STATE_XHR_HAS_INVALID_CONTEXT;
      }
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

    // suppress parsing failure messages to console for statuses which
    // can have empty bodies (see bug 884693).
    IgnoredErrorResult rv2;
    uint32_t responseStatus = GetStatus(rv2);
    if (!rv2.Failed() &&
        (responseStatus == 201 || responseStatus == 202 ||
         responseStatus == 204 || responseStatus == 205 ||
         responseStatus == 304)) {
      mResponseXML->SetSuppressParserErrorConsoleMessages(true);
    }

    if (nsContentUtils::IsSystemPrincipal(mPrincipal)) {
      mResponseXML->ForceEnableXULXBL();
    }

    nsCOMPtr<nsILoadInfo> loadInfo = mChannel->GetLoadInfo();
    MOZ_ASSERT(loadInfo);
    bool isCrossSite = false;
    if (loadInfo) {
      isCrossSite = loadInfo->GetTainting() != LoadTainting::Basic;
    }

    if (isCrossSite) {
      nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(mResponseXML);
      if (htmlDoc) {
        htmlDoc->DisableCookieAccess();
      }
    }

    nsCOMPtr<nsIStreamListener> listener;
    nsCOMPtr<nsILoadGroup> loadGroup;
    channel->GetLoadGroup(getter_AddRefs(loadGroup));

    // suppress <parsererror> nodes on XML document parse failure, but only
    // for non-privileged code (including Web Extensions). See bug 289714.
    if (!IsSystemXHR()) {
      mResponseXML->SetSuppressParserErrorElement(true);
    }

    rv = mResponseXML->StartDocumentLoad(kLoadAsData, channel, loadGroup,
                                         nullptr, getter_AddRefs(listener),
                                         !isCrossSite);
    NS_ENSURE_SUCCESS(rv, rv);

    // the spec requires the response document.referrer to be the empty string
    mResponseXML->SetReferrer(NS_LITERAL_CSTRING(""));

    mXMLParserStreamListener = listener;
    rv = mXMLParserStreamListener->OnStartRequest(request, ctxt);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Download phase beginning; start the progress event timer if necessary.
  if (NS_SUCCEEDED(rv) && HasListenersFor(nsGkAtoms::onprogress)) {
    StartProgressEventTimer();
  }

  return NS_OK;
}

NS_IMETHODIMP
XMLHttpRequestMainThread::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult status)
{
  AUTO_PROFILER_LABEL("XMLHttpRequestMainThread::OnStopRequest", NETWORK);

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
  // UNSENT is for abort calls.  See OnStartRequest above.
  if (mState == XMLHttpRequestBinding::UNSENT || mFlagTimedOut) {
    if (mXMLParserStreamListener)
      (void) mXMLParserStreamListener->OnStopRequest(request, ctxt, status);
    return NS_OK;
  }

  // Is this good enough here?
  if (mXMLParserStreamListener && mFlagParseBody) {
    mXMLParserStreamListener->OnStopRequest(request, ctxt, status);
  }

  mXMLParserStreamListener = nullptr;
  mContext = nullptr;

  bool waitingForBlobCreation = false;

  // If we have this error, we have to deal with a file: URL + responseType =
  // blob. We have this error because we canceled the channel. The status will
  // be set to NS_OK.
  if (status == NS_ERROR_FILE_ALREADY_EXISTS &&
      mResponseType == XMLHttpRequestResponseType::Blob) {
    nsCOMPtr<nsIFile> file;
    nsresult rv = GetLocalFileFromChannel(request, getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (file) {
      nsAutoCString contentType;
      rv = mChannel->GetContentType(contentType);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      ChromeFilePropertyBag bag;
      bag.mType = NS_ConvertUTF8toUTF16(contentType);

      nsCOMPtr<nsIGlobalObject> global = GetOwnerGlobal();

      ErrorResult error;
      RefPtr<Promise> promise =
        FileCreatorHelper::CreateFile(global, file, bag, true, error);
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }

      FileCreationHandler::Create(promise, this);
      waitingForBlobCreation = true;
      status = NS_OK;

      NS_ASSERTION(mResponseBody.IsEmpty(), "mResponseBody should be empty");
      NS_ASSERTION(mResponseText.IsEmpty(), "mResponseText should be empty");
    }
  }

  if (NS_SUCCEEDED(status) &&
      mResponseType == XMLHttpRequestResponseType::Blob &&
      !waitingForBlobCreation) {
    // Smaller files may be written in cache map instead of separate files.
    // Also, no-store response cannot be written in persistent cache.
    nsAutoCString contentType;
    mChannel->GetContentType(contentType);

    // mBlobStorage can be null if the channel is non-file non-cacheable
    // and if the response length is zero.
    MaybeCreateBlobStorage();
    mBlobStorage->GetBlobWhenReady(GetOwner(), contentType, this);
    waitingForBlobCreation = true;

    NS_ASSERTION(mResponseBody.IsEmpty(), "mResponseBody should be empty");
    NS_ASSERTION(mResponseText.IsEmpty(), "mResponseText should be empty");
  } else if (NS_SUCCEEDED(status) &&
             ((mResponseType == XMLHttpRequestResponseType::Arraybuffer &&
               !mIsMappedArrayBuffer) ||
              mResponseType == XMLHttpRequestResponseType::Moz_chunked_arraybuffer)) {
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

  mFlagSyncLooping = false;
  mRequestSentTime = 0;

  // update our charset and decoder to match mResponseXML,
  // before it is possibly nulled out
  MatchCharsetAndDecoderToResponseDocument();

  if (NS_FAILED(status)) {
    // This can happen if the server is unreachable. Other possible
    // reasons are that the user leaves the page or hits the ESC key.

    mErrorLoad = ErrorType::eUnreachable;
    mResponseXML = nullptr;
  }

  // If we're uninitialized at this point, we encountered an error
  // earlier and listeners have already been notified. Also we do
  // not want to do this if we already completed.
  if (mState == XMLHttpRequestBinding::UNSENT ||
      mState == XMLHttpRequestBinding::DONE) {
    return NS_OK;
  }

  if (!mResponseXML) {
    mFlagParseBody = false;

    //We postpone the 'done' until the creation of the Blob is completed.
    if (!waitingForBlobCreation) {
      ChangeStateToDone();
    }

    return NS_OK;
  }

  if (mIsHtml) {
    NS_ASSERTION(!mFlagSyncLooping,
      "We weren't supposed to support HTML parsing with XHR!");
    mParseEndListener = new nsXHRParseEndListener(this);
    nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(mResponseXML);
    EventListenerManager* manager =
      eventTarget->GetOrCreateListenerManager();
    manager->AddEventListenerByType(mParseEndListener,
                                    kLiteralString_DOMContentLoaded,
                                    TrustedEventsAtSystemGroupBubble());
    return NS_OK;
  } else {
    mFlagParseBody = false;
  }

  // We might have been sent non-XML data. If that was the case,
  // we should null out the document member. The idea in this
  // check here is that if there is no document element it is not
  // an XML document. We might need a fancier check...
  if (!mResponseXML->GetRootElement()) {
    mErrorParsingXML = true;
    mResponseXML = nullptr;
  }
  ChangeStateToDone();
  return NS_OK;
}

void
XMLHttpRequestMainThread::OnBodyParseEnd()
{
  mFlagParseBody = false;
  mParseEndListener = nullptr;
  ChangeStateToDone();
}

void
XMLHttpRequestMainThread::MatchCharsetAndDecoderToResponseDocument()
{
  if (mResponseXML && mResponseCharset != mResponseXML->GetDocumentCharacterSet()) {
    mResponseCharset = mResponseXML->GetDocumentCharacterSet();
    TruncateResponseText();
    mResponseBodyDecodedPos = 0;
    mDecoder = mResponseCharset->NewDecoderWithBOMRemoval();
  }
}

void
XMLHttpRequestMainThread::ChangeStateToDone()
{
  StopProgressEventTimer();

  MOZ_ASSERT(!mFlagParseBody,
             "ChangeStateToDone() called before async HTML parsing is done.");

  mFlagSend = false;

  if (mTimeoutTimer) {
    mTimeoutTimer->Cancel();
  }

  // Per spec, fire the last download progress event, if any,
  // before readystatechange=4/done. (Note that 0-sized responses
  // will have not sent a progress event yet, so one must be sent here).
  if (!mFlagSynchronous &&
      (!mLoadTransferred || mProgressSinceLastProgressEvent)) {
    DispatchProgressEvent(this, ProgressEventType::progress,
                          mLoadTransferred, mLoadTotal);
    mProgressSinceLastProgressEvent = false;
  }

  // Per spec, fire readystatechange=4/done before final error events.
  ChangeState(XMLHttpRequestBinding::DONE, true);

  // Per spec, if we failed in the upload phase, fire a final error
  // and loadend events for the upload after readystatechange=4/done.
  if (!mFlagSynchronous && mUpload && !mUploadComplete) {
    DispatchProgressEvent(mUpload, ProgressEventType::error, 0, -1);
  }

  // Per spec, fire download's load/error and loadend events after
  // readystatechange=4/done (and of course all upload events).
  if (mErrorLoad != ErrorType::eOK) {
    DispatchProgressEvent(this, ProgressEventType::error, 0, -1);
  } else {
    DispatchProgressEvent(this, ProgressEventType::load,
                          mLoadTransferred, mLoadTotal);
  }

  if (mErrorLoad != ErrorType::eOK) {
    // By nulling out channel here we make it so that Send() can test
    // for that and throw. Also calling the various status
    // methods/members will not throw.
    // This matches what IE does.
    mChannel = nullptr;
  }
}

nsresult
XMLHttpRequestMainThread::CreateChannel()
{
  // When we are called from JS we can find the load group for the page,
  // and add ourselves to it. This way any pending requests
  // will be automatically aborted if the user leaves the page.
  nsCOMPtr<nsILoadGroup> loadGroup = GetLoadGroup();

  nsSecurityFlags secFlags;
  nsLoadFlags loadFlags = nsIRequest::LOAD_BACKGROUND |
                          nsIChannel::LOAD_CLASSIFY_URI;
  if (nsContentUtils::IsSystemPrincipal(mPrincipal)) {
    // When chrome is loading we want to make sure to sandbox any potential
    // result document. We also want to allow cross-origin loads.
    secFlags = nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL |
               nsILoadInfo::SEC_SANDBOXED;
  } else if (IsSystemXHR()) {
    // For pages that have appropriate permissions, we want to still allow
    // cross-origin loads, but make sure that the any potential result
    // documents get the same principal as the loader.
    secFlags = nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS |
               nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL;
    loadFlags |= nsIChannel::LOAD_BYPASS_SERVICE_WORKER;
  } else {
    // Otherwise use CORS. Again, make sure that potential result documents
    // use the same principal as the loader.
    secFlags = nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS |
               nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL;
  }

  if (mIsAnon) {
    secFlags |= nsILoadInfo::SEC_COOKIES_OMIT;
  }

  // Use the responsibleDocument if we have it, except for dedicated workers
  // where it will be the parent document, which is not the one we want to use.
  nsresult rv;
  nsCOMPtr<nsIDocument> responsibleDocument = GetDocumentIfCurrent();
  if (responsibleDocument && responsibleDocument->NodePrincipal() == mPrincipal) {
    rv = NS_NewChannel(getter_AddRefs(mChannel),
                       mRequestURL,
                       responsibleDocument,
                       secFlags,
                       nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST,
                       nullptr, // aPerformanceStorage
                       loadGroup,
                       nullptr,   // aCallbacks
                       loadFlags);
  } else if (mClientInfo.isSome()) {
    rv = NS_NewChannel(getter_AddRefs(mChannel),
                       mRequestURL,
                       mPrincipal,
                       mClientInfo.ref(),
                       mController,
                       secFlags,
                       nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST,
                       mPerformanceStorage, // aPerformanceStorage
                       loadGroup,
                       nullptr,   // aCallbacks
                       loadFlags);
  } else {
    // Otherwise use the principal.
    rv = NS_NewChannel(getter_AddRefs(mChannel),
                       mRequestURL,
                       mPrincipal,
                       secFlags,
                       nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST,
                       mPerformanceStorage, // aPerformanceStorage
                       loadGroup,
                       nullptr,   // aCallbacks
                       loadFlags);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));
  if (httpChannel) {
    rv = httpChannel->SetRequestMethod(mRequestMethod);
    NS_ENSURE_SUCCESS(rv, rv);

    // Set the initiator type
    nsCOMPtr<nsITimedChannel> timedChannel(do_QueryInterface(httpChannel));
    if (timedChannel) {
      timedChannel->SetInitiatorType(NS_LITERAL_STRING("xmlhttprequest"));
    }
  }

  return NS_OK;
}

void
XMLHttpRequestMainThread::MaybeLowerChannelPriority()
{
  nsCOMPtr<nsIDocument> doc = GetDocumentIfCurrent();
  if (!doc) {
    return;
  }

  AutoJSAPI jsapi;
  if (!jsapi.Init(GetOwnerGlobal())) {
    return;
  }

  JSContext* cx = jsapi.cx();
  nsAutoCString fileNameString;
  if (!nsJSUtils::GetCallingLocation(cx, fileNameString)) {
    return;
  }

  if (!doc->IsScriptTracking(fileNameString)) {
    return;
  }

  if (nsContentUtils::IsTailingEnabled()) {
    nsCOMPtr<nsIClassOfService> cos = do_QueryInterface(mChannel);
    if (cos) {
      // Adding TailAllowed to overrule the Unblocked flag, but to preserve
      // the effect of Unblocked when tailing is off.
      cos->AddClassFlags(nsIClassOfService::Throttleable |
                         nsIClassOfService::Tail |
                         nsIClassOfService::TailAllowed);
    }
  }

  nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(mChannel);
  if (p) {
    p->SetPriority(nsISupportsPriority::PRIORITY_LOWEST);
  }
}

nsresult
XMLHttpRequestMainThread::InitiateFetch(already_AddRefed<nsIInputStream> aUploadStream,
                                        int64_t aUploadLength,
                                        nsACString& aUploadContentType)
{
  nsresult rv;
  nsCOMPtr<nsIInputStream> uploadStream = Move(aUploadStream);

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

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));
  if (httpChannel) {
    // If the user hasn't overridden the Accept header, set it to */* per spec.
    if (!mAuthorRequestHeaders.Has("accept")) {
      mAuthorRequestHeaders.Set("accept", NS_LITERAL_CSTRING("*/*"));
    }

    mAuthorRequestHeaders.ApplyToChannel(httpChannel);

    if (!IsSystemXHR()) {
      nsCOMPtr<nsPIDOMWindowInner> owner = GetOwner();
      nsCOMPtr<nsIDocument> doc = owner ? owner->GetExtantDoc() : nullptr;
      mozilla::net::ReferrerPolicy referrerPolicy = doc ?
        doc->GetReferrerPolicy() : mozilla::net::RP_Unset;
      nsContentUtils::SetFetchReferrerURIWithPolicy(mPrincipal, doc,
                                                    httpChannel, referrerPolicy);
    }

    // Some extensions override the http protocol handler and provide their own
    // implementation. The channels returned from that implementation don't
    // always seem to implement the nsIUploadChannel2 interface, presumably
    // because it's a new interface. Eventually we should remove this and simply
    // require that http channels implement the new interface (see bug 529041).
    nsCOMPtr<nsIUploadChannel2> uploadChannel2 = do_QueryInterface(httpChannel);
    if (!uploadChannel2) {
      nsCOMPtr<nsIConsoleService> consoleService =
        do_GetService(NS_CONSOLESERVICE_CONTRACTID);
      if (consoleService) {
        consoleService->LogStringMessage(
          u"Http channel implementation doesn't support nsIUploadChannel2. "
          "An extension has supplied a non-functional http protocol handler. "
          "This will break behavior and in future releases not work at all.");
      }
    }

    if (uploadStream) {
      // If necessary, wrap the stream in a buffered stream so as to guarantee
      // support for our upload when calling ExplicitSetUploadStream.
      if (!NS_InputStreamIsBuffered(uploadStream)) {
        nsCOMPtr<nsIInputStream> bufferedStream;
        rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream),
                                       uploadStream.forget(), 4096);
        NS_ENSURE_SUCCESS(rv, rv);

        uploadStream = bufferedStream;
      }

      // We want to use a newer version of the upload channel that won't
      // ignore the necessary headers for an empty Content-Type.
      nsCOMPtr<nsIUploadChannel2> uploadChannel2(do_QueryInterface(httpChannel));
      // This assertion will fire if buggy extensions are installed
      NS_ASSERTION(uploadChannel2, "http must support nsIUploadChannel2");
      if (uploadChannel2) {
          uploadChannel2->ExplicitSetUploadStream(uploadStream,
                                                  aUploadContentType,
                                                  mUploadTotal, mRequestMethod,
                                                  false);
      } else {
        // The http channel doesn't support the new nsIUploadChannel2.
        // Emulate it as best we can using nsIUploadChannel.
        if (aUploadContentType.IsEmpty()) {
          aUploadContentType.AssignLiteral("application/octet-stream");
        }
        nsCOMPtr<nsIUploadChannel> uploadChannel =
          do_QueryInterface(httpChannel);
        uploadChannel->SetUploadStream(uploadStream, aUploadContentType,
                                       mUploadTotal);
        // Reset the method to its original value
        rv = httpChannel->SetRequestMethod(mRequestMethod);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
      }
    }
  }

  // Due to the chrome-only XHR.channel API, we need a hacky way to set the
  // SEC_COOKIES_INCLUDE *after* the channel has been has been created, since
  // .withCredentials can be called after open() is called.
  // Not doing this for privileged system XHRs since those don't use CORS.
  if (!IsSystemXHR() && !mIsAnon && mFlagACwithCredentials) {
    nsCOMPtr<nsILoadInfo> loadInfo = mChannel->GetLoadInfo();
    if (loadInfo) {
      static_cast<net::LoadInfo*>(loadInfo.get())->SetIncludeCookiesSecFlag();
    }
  }

  // We never let XHR be blocked by head CSS/JS loads to avoid potential
  // deadlock where server generation of CSS/JS requires an XHR signal.
  nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(mChannel));
  if (cos) {
    cos->AddClassFlags(nsIClassOfService::Unblocked);

    // Mark channel as urgent-start if the XHR is triggered by user input
    // events.
    if (EventStateManager::IsHandlingUserInput()) {
      cos->AddClassFlags(nsIClassOfService::UrgentStart);
    }
  }

  // Disable Necko-internal response timeouts.
  nsCOMPtr<nsIHttpChannelInternal>
    internalHttpChannel(do_QueryInterface(mChannel));
  if (internalHttpChannel) {
    rv = internalHttpChannel->SetResponseTimeoutEnabled(false);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  if (!mIsAnon) {
    AddLoadFlags(mChannel, nsIChannel::LOAD_EXPLICIT_CREDENTIALS);
  }

  // Bypass the network cache in cases where it makes no sense:
  // POST responses are always unique, and we provide no API that would
  // allow our consumers to specify a "cache key" to access old POST
  // responses, so they are not worth caching.
  if (mRequestMethod.EqualsLiteral("POST")) {
    AddLoadFlags(mChannel,
                 nsICachingChannel::LOAD_BYPASS_LOCAL_CACHE |
                 nsIRequest::INHIBIT_CACHING);
  } else {
    // When we are sync loading, we need to bypass the local cache when it would
    // otherwise block us waiting for exclusive access to the cache.  If we don't
    // do this, then we could dead lock in some cases (see bug 309424).
    //
    // Also don't block on the cache entry on async if it is busy - favoring parallelism
    // over cache hit rate for xhr. This does not disable the cache everywhere -
    // only in cases where more than one channel for the same URI is accessed
    // simultanously.
    AddLoadFlags(mChannel, nsICachingChannel::LOAD_BYPASS_LOCAL_CACHE_IF_BUSY);
  }

  // Since we expect XML data, set the type hint accordingly
  // if the channel doesn't know any content type.
  // This means that we always try to parse local files as XML
  // ignoring return value, as this is not critical. Use text/xml as fallback
  // MIME type.
  nsAutoCString contentType;
  if (NS_FAILED(mChannel->GetContentType(contentType)) ||
      contentType.IsEmpty() ||
      contentType.EqualsLiteral(UNKNOWN_CONTENT_TYPE)) {
    mChannel->SetContentType(NS_LITERAL_CSTRING("text/xml"));
  }

  // Set up the preflight if needed
  if (!IsSystemXHR()) {
    nsTArray<nsCString> CORSUnsafeHeaders;
    mAuthorRequestHeaders.GetCORSUnsafeHeaders(CORSUnsafeHeaders);
    nsCOMPtr<nsILoadInfo> loadInfo = mChannel->GetLoadInfo();
    if (loadInfo) {
      loadInfo->SetCorsPreflightInfo(CORSUnsafeHeaders,
                                     mFlagHadUploadListenersOnSend);
    }
  }

  // Hook us up to listen to redirects and the like. Only do this very late
  // since this creates a cycle between the channel and us. This cycle has
  // to be manually broken if anything below fails.
  mChannel->GetNotificationCallbacks(getter_AddRefs(mNotificationCallbacks));
  mChannel->SetNotificationCallbacks(this);

  if (internalHttpChannel) {
    internalHttpChannel->SetBlockAuthPrompt(ShouldBlockAuthPrompt());
  }

  // Because of bug 682305, we can't let listener be the XHR object itself
  // because JS wouldn't be able to use it. So create a listener around 'this'.
  // Make sure to hold a strong reference so that we don't leak the wrapper.
  nsCOMPtr<nsIStreamListener> listener = new net::nsStreamListenerWrapper(this);

  // Check if this XHR is created from a tracking script.
  // If yes, lower the channel's priority.
  if (nsContentUtils::IsLowerNetworkPriority()) {
    MaybeLowerChannelPriority();
  }

  // Start reading from the channel
  rv = mChannel->AsyncOpen2(listener);
  listener = nullptr;
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Drop our ref to the channel to avoid cycles. Also drop channel's
    // ref to us to be extra safe.
    mChannel->SetNotificationCallbacks(mNotificationCallbacks);
    mChannel = nullptr;

    mErrorLoad = ErrorType::eChannelOpen;

    // Per spec, we throw on sync errors, but not async.
    if (mFlagSynchronous) {
      mState = XMLHttpRequestBinding::DONE;
      return NS_ERROR_DOM_NETWORK_ERR;
    }
  }

  return NS_OK;
}

void
XMLHttpRequestMainThread::UnsuppressEventHandlingAndResume()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mFlagSynchronous);

  if (mSuspendedDoc) {
    mSuspendedDoc->UnsuppressEventHandlingAndFireEvents(true);
    mSuspendedDoc = nullptr;
  }

  if (mResumeTimeoutRunnable) {
    DispatchToMainThread(mResumeTimeoutRunnable.forget());
    mResumeTimeoutRunnable = nullptr;
  }
}

void
XMLHttpRequestMainThread::Send(JSContext* aCx,
                               const Nullable<DocumentOrBlobOrArrayBufferViewOrArrayBufferOrFormDataOrURLSearchParamsOrUSVString>& aData,
                               ErrorResult& aRv)
{
  NOT_CALLABLE_IN_SYNC_SEND_RV

  if (aData.IsNull()) {
    aRv = SendInternal(nullptr);
    return;
  }


  if (aData.Value().IsDocument()) {
    BodyExtractor<nsIDocument> body(&aData.Value().GetAsDocument());
    aRv = SendInternal(&body);
    return;
  }

  if (aData.Value().IsBlob()) {
    BodyExtractor<const Blob> body(&aData.Value().GetAsBlob());
    aRv = SendInternal(&body);
    return;
  }

  if (aData.Value().IsArrayBuffer()) {
    BodyExtractor<const ArrayBuffer> body(&aData.Value().GetAsArrayBuffer());
    aRv = SendInternal(&body);
    return;
  }

  if (aData.Value().IsArrayBufferView()) {
    BodyExtractor<const ArrayBufferView>
      body(&aData.Value().GetAsArrayBufferView());
    aRv = SendInternal(&body);
    return;
  }

  if (aData.Value().IsFormData()) {
    BodyExtractor<const FormData> body(&aData.Value().GetAsFormData());
    aRv = SendInternal(&body);
    return;
  }

  if (aData.Value().IsURLSearchParams()) {
    BodyExtractor<const URLSearchParams> body(&aData.Value().GetAsURLSearchParams());
    aRv = SendInternal(&body);
    return;
  }

  if (aData.Value().IsUSVString()) {
    BodyExtractor<const nsAString> body(&aData.Value().GetAsUSVString());
    aRv = SendInternal(&body);
    return;
  }
}

nsresult
XMLHttpRequestMainThread::MaybeSilentSendFailure(nsresult aRv)
{
  // Per spec, silently fail on async request failures; throw for sync.
  if (mFlagSynchronous) {
    mState = XMLHttpRequestBinding::DONE;
    return NS_ERROR_DOM_NETWORK_ERR;
  }

  // Defer the actual sending of async events just in case listeners
  // are attached after the send() method is called.
  Unused << NS_WARN_IF(NS_FAILED(
    DispatchToMainThread(NewRunnableMethod<ProgressEventType>(
      "dom::XMLHttpRequestMainThread::CloseRequestWithError",
      this,
      &XMLHttpRequestMainThread::CloseRequestWithError,
      ProgressEventType::error))));
  return NS_OK;
}

nsresult
XMLHttpRequestMainThread::SendInternal(const BodyExtractorBase* aBody)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE(mPrincipal, NS_ERROR_NOT_INITIALIZED);

  // Step 1
  if (mState != XMLHttpRequestBinding::OPENED) {
    return NS_ERROR_DOM_INVALID_STATE_XHR_MUST_BE_OPENED;
  }

  // Step 2
  if (mFlagSend) {
    return NS_ERROR_DOM_INVALID_STATE_XHR_MUST_NOT_BE_SENDING;
  }

  nsresult rv = CheckInnerWindowCorrectness();
  if (NS_FAILED(rv)) {
    return NS_ERROR_DOM_INVALID_STATE_XHR_HAS_INVALID_CONTEXT;
  }

  // If open() failed to create the channel, then throw a network error
  // as per spec. We really should create the channel here in send(), but
  // we have internal code relying on the channel being created in open().
  if (!mChannel) {
    mFlagSend = true; // so CloseRequestWithError sets us to DONE.
    return MaybeSilentSendFailure(NS_ERROR_DOM_NETWORK_ERR);
  }

  PopulateNetworkInterfaceId();

  // XXX We should probably send a warning to the JS console
  //     if there are no event listeners set and we are doing
  //     an asynchronous call.

  mUploadTransferred = 0;
  mUploadTotal = 0;
  // By default we don't have any upload, so mark upload complete.
  mUploadComplete = true;
  mErrorLoad = ErrorType::eOK;
  mLoadTotal = -1;
  nsCOMPtr<nsIInputStream> uploadStream;
  nsAutoCString uploadContentType;
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));
  if (aBody && httpChannel &&
      !mRequestMethod.EqualsLiteral("GET") &&
      !mRequestMethod.EqualsLiteral("HEAD")) {

    nsAutoCString charset;
    nsAutoCString defaultContentType;
    uint64_t size_u64;
    rv = aBody->GetAsStream(getter_AddRefs(uploadStream),
                            &size_u64, defaultContentType, charset);
    NS_ENSURE_SUCCESS(rv, rv);

    // make sure it fits within js MAX_SAFE_INTEGER
    mUploadTotal =
      net::InScriptableRange(size_u64) ? static_cast<int64_t>(size_u64) : -1;

    if (uploadStream) {
      // If author set no Content-Type, use the default from GetAsStream().
      mAuthorRequestHeaders.Get("content-type", uploadContentType);
      if (uploadContentType.IsVoid()) {
        uploadContentType = defaultContentType;
      }

      // We don't want to set a charset for streams.
      if (!charset.IsEmpty()) {
        // Replace all case-insensitive matches of the charset in the
        // content-type with the correct case.
        RequestHeaders::CharsetIterator iter(uploadContentType);
        const nsCaseInsensitiveCStringComparator cmp;
        while (iter.Next()) {
          if (!iter.Equals(charset, cmp)) {
            iter.Replace(charset);
          }
        }
      }

      mUploadComplete = false;
    }
  }

  ResetResponse();

  // Check if we should enable cross-origin upload listeners.
  if (mUpload && mUpload->HasListeners()) {
    mFlagHadUploadListenersOnSend = true;
  }

  mIsMappedArrayBuffer = false;
  if (mResponseType == XMLHttpRequestResponseType::Arraybuffer &&
      IsMappedArrayBufferEnabled()) {
    nsCOMPtr<nsIURI> uri;
    nsAutoCString scheme;

    rv = mChannel->GetURI(getter_AddRefs(uri));
    if (NS_SUCCEEDED(rv)) {
      uri->GetScheme(scheme);
      if (scheme.LowerCaseEqualsLiteral("jar")) {
        mIsMappedArrayBuffer = true;
      }
    }
  }

  rv = InitiateFetch(uploadStream.forget(), mUploadTotal, uploadContentType);
  NS_ENSURE_SUCCESS(rv, rv);

  // Start our timeout
  mRequestSentTime = PR_Now();
  StartTimeoutTimer();

  mWaitingForOnStopRequest = true;

  // Step 8
  mFlagSend = true;

  // If we're synchronous, spin an event loop here and wait
  if (mFlagSynchronous) {
    mFlagSyncLooping = true;

    if (GetOwner()) {
      if (nsCOMPtr<nsPIDOMWindowOuter> topWindow = GetOwner()->GetOuterWindow()->GetTop()) {
        if (nsCOMPtr<nsPIDOMWindowInner> topInner = topWindow->GetCurrentInnerWindow()) {
          mSuspendedDoc = topWindow->GetExtantDoc();
          if (mSuspendedDoc) {
            mSuspendedDoc->SuppressEventHandling();
          }
          topInner->Suspend();
          mResumeTimeoutRunnable = new nsResumeTimeoutsEvent(topInner);
        }
      }
    }

    SuspendEventDispatching();
    StopProgressEventTimer();

    SyncTimeoutType syncTimeoutType = MaybeStartSyncTimeoutTimer();
    if (syncTimeoutType == eErrorOrExpired) {
      Abort();
      rv = NS_ERROR_DOM_NETWORK_ERR;
    }

    if (NS_SUCCEEDED(rv)) {
      nsAutoSyncOperation sync(mSuspendedDoc);
      if (!SpinEventLoopUntil([&]() { return !mFlagSyncLooping; })) {
        rv = NS_ERROR_UNEXPECTED;
      }

      // Time expired... We should throw.
      if (syncTimeoutType == eTimerStarted && !mSyncTimeoutTimer) {
        rv = NS_ERROR_DOM_NETWORK_ERR;
      }

      CancelSyncTimeoutTimer();
    }

    UnsuppressEventHandlingAndResume();
    ResumeEventDispatching();
  } else {
    // Now that we've successfully opened the channel, we can change state.  Note
    // that this needs to come after the AsyncOpen() and rv check, because this
    // can run script that would try to restart this request, and that could end
    // up doing our AsyncOpen on a null channel if the reentered AsyncOpen fails.
    StopProgressEventTimer();

    // Upload phase beginning; start the progress event timer if necessary.
    if (mUpload && mUpload->HasListenersFor(nsGkAtoms::onprogress)) {
      StartProgressEventTimer();
    }
    // Dispatch loadstart events
    DispatchProgressEvent(this, ProgressEventType::loadstart, 0, -1);
    if (mUpload && !mUploadComplete) {
      DispatchProgressEvent(mUpload, ProgressEventType::loadstart,
                            0, mUploadTotal);
    }
  }

  if (!mChannel) {
    return MaybeSilentSendFailure(NS_ERROR_DOM_NETWORK_ERR);
  }

  return rv;
}

/* static */
bool
XMLHttpRequestMainThread::IsMappedArrayBufferEnabled()
{
  static bool sMappedArrayBufferAdded = false;
  static bool sIsMappedArrayBufferEnabled;

  if (!sMappedArrayBufferAdded) {
    Preferences::AddBoolVarCache(&sIsMappedArrayBufferEnabled,
                                 "dom.mapped_arraybuffer.enabled",
                                 true);
    sMappedArrayBufferAdded = true;
  }

  return sIsMappedArrayBufferEnabled;
}

/* static */
bool
XMLHttpRequestMainThread::IsLowercaseResponseHeader()
{
  static bool sLowercaseResponseHeaderAdded = false;
  static bool sIsLowercaseResponseHeaderEnabled;

  if (!sLowercaseResponseHeaderAdded) {
    Preferences::AddBoolVarCache(&sIsLowercaseResponseHeaderEnabled,
                                 "dom.xhr.lowercase_header.enabled",
                                 false);
    sLowercaseResponseHeaderAdded = true;
  }

  return sIsLowercaseResponseHeaderEnabled;
}

// http://dvcs.w3.org/hg/xhr/raw-file/tip/Overview.html#dom-xmlhttprequest-setrequestheader
void
XMLHttpRequestMainThread::SetRequestHeader(const nsACString& aName,
                                           const nsACString& aValue,
                                           ErrorResult& aRv)
{
  NOT_CALLABLE_IN_SYNC_SEND_RV

  // Step 1
  if (mState != XMLHttpRequestBinding::OPENED) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_XHR_MUST_BE_OPENED);
    return;
  }

  // Step 2
  if (mFlagSend) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_XHR_MUST_NOT_BE_SENDING);
    return;
  }

  // Step 3
  nsAutoCString value;
  NS_TrimHTTPWhitespace(aValue, value);

  // Step 4
  if (!NS_IsValidHTTPToken(aName) || !NS_IsReasonableHTTPHeaderValue(value)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_HEADER_NAME);
    return;
  }

  // Step 5
  bool isPrivilegedCaller = IsSystemXHR();
  bool isForbiddenHeader = nsContentUtils::IsForbiddenRequestHeader(aName);
  if (!isPrivilegedCaller && isForbiddenHeader) {
    NS_ConvertUTF8toUTF16 name(aName);
    const char16_t* params[] = { name.get() };
    LogMessage("ForbiddenHeaderWarning", GetOwner(), params, ArrayLength(params));
    return;
  }

  // Step 6.1
  // Skipping for now, as normalizing the case of header names may not be
  // web-compatible. See bug 1285036.

  // Step 6.2-6.3
  // Gecko-specific: invalid headers can be set by privileged
  //                 callers, but will not merge.
  if (isPrivilegedCaller && isForbiddenHeader) {
    mAuthorRequestHeaders.Set(aName, value);
  } else {
    mAuthorRequestHeaders.MergeOrSet(aName, value);
  }
}

void
XMLHttpRequestMainThread::SetTimeout(uint32_t aTimeout, ErrorResult& aRv)
{
  NOT_CALLABLE_IN_SYNC_SEND_RV

  if (mFlagSynchronous &&
      mState != XMLHttpRequestBinding::UNSENT && HasOrHasHadOwner()) {
    /* Timeout is not supported for synchronous requests with an owning window,
       per XHR2 spec. */
    LogMessage("TimeoutSyncXHRWarning", GetOwner());
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_XHR_TIMEOUT_AND_RESPONSETYPE_UNSUPPORTED_FOR_SYNC);
    return;
  }

  mTimeoutMilliseconds = aTimeout;
  if (mRequestSentTime) {
    StartTimeoutTimer();
  }
}

void
XMLHttpRequestMainThread::SetTimerEventTarget(nsITimer* aTimer)
{
  if (nsCOMPtr<nsIGlobalObject> global = GetOwnerGlobal()) {
    nsCOMPtr<nsIEventTarget> target = global->EventTargetFor(TaskCategory::Other);
    aTimer->SetTarget(target);
  }
}

nsresult
XMLHttpRequestMainThread::DispatchToMainThread(already_AddRefed<nsIRunnable> aRunnable)
{
  if (nsCOMPtr<nsIGlobalObject> global = GetOwnerGlobal()) {
    nsCOMPtr<nsIEventTarget> target = global->EventTargetFor(TaskCategory::Other);
    MOZ_ASSERT(target);

    return target->Dispatch(Move(aRunnable), NS_DISPATCH_NORMAL);
  }

  return NS_DispatchToMainThread(Move(aRunnable));
}

void
XMLHttpRequestMainThread::StartTimeoutTimer()
{
  MOZ_ASSERT(mRequestSentTime,
             "StartTimeoutTimer mustn't be called before the request was sent!");
  if (mState == XMLHttpRequestBinding::DONE) {
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
    mTimeoutTimer = NS_NewTimer();
    SetTimerEventTarget(mTimeoutTimer);
  }
  uint32_t elapsed =
    (uint32_t)((PR_Now() - mRequestSentTime) / PR_USEC_PER_MSEC);
  mTimeoutTimer->InitWithCallback(
    this,
    mTimeoutMilliseconds > elapsed ? mTimeoutMilliseconds - elapsed : 0,
    nsITimer::TYPE_ONE_SHOT
  );
}

uint16_t
XMLHttpRequestMainThread::ReadyState() const
{
  return mState;
}

void
XMLHttpRequestMainThread::OverrideMimeType(const nsAString& aMimeType,
                                           ErrorResult& aRv)
{
  NOT_CALLABLE_IN_SYNC_SEND_RV

  if (mState == XMLHttpRequestBinding::LOADING ||
      mState == XMLHttpRequestBinding::DONE) {
    ResetResponse();
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_XHR_MUST_NOT_BE_LOADING_OR_DONE);
    return;
  }

  mOverrideMimeType = aMimeType;
}

bool
XMLHttpRequestMainThread::MozBackgroundRequest() const
{
  return mFlagBackgroundRequest;
}

nsresult
XMLHttpRequestMainThread::SetMozBackgroundRequest(bool aMozBackgroundRequest)
{
  if (!IsSystemXHR()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (mState != XMLHttpRequestBinding::UNSENT) {
    // Can't change this while we're in the middle of something.
    return NS_ERROR_DOM_INVALID_STATE_XHR_MUST_NOT_BE_SENDING;
  }

  mFlagBackgroundRequest = aMozBackgroundRequest;

  return NS_OK;
}

void
XMLHttpRequestMainThread::SetMozBackgroundRequest(bool aMozBackgroundRequest,
                                                  ErrorResult& aRv)
{
  // No errors for this webIDL method on main-thread.
  SetMozBackgroundRequest(aMozBackgroundRequest);
}

bool
XMLHttpRequestMainThread::WithCredentials() const
{
  return mFlagACwithCredentials;
}

void
XMLHttpRequestMainThread::SetWithCredentials(bool aWithCredentials, ErrorResult& aRv)
{
  NOT_CALLABLE_IN_SYNC_SEND_RV

  // Return error if we're already processing a request.  Note that we can't use
  // ReadyState() here, because it can't differentiate between "opened" and
  // "sent", so we use mState directly.

  if ((mState != XMLHttpRequestBinding::UNSENT &&
       mState != XMLHttpRequestBinding::OPENED) ||
      mFlagSend || mIsAnon) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_XHR_MUST_NOT_BE_SENDING);
    return;
  }

  mFlagACwithCredentials = aWithCredentials;
}

nsresult
XMLHttpRequestMainThread::ChangeState(uint16_t aState, bool aBroadcast)
{
  mState = aState;
  nsresult rv = NS_OK;

  if (aState != XMLHttpRequestBinding::HEADERS_RECEIVED &&
      aState != XMLHttpRequestBinding::LOADING) {
    StopProgressEventTimer();
  }


  if (aBroadcast && (!mFlagSynchronous ||
                     aState == XMLHttpRequestBinding::OPENED ||
                     aState == XMLHttpRequestBinding::DONE)) {
    rv = FireReadystatechangeEvent();
  }

  return rv;
}

/////////////////////////////////////////////////////
// nsIChannelEventSink methods:
//
NS_IMETHODIMP
XMLHttpRequestMainThread::AsyncOnChannelRedirect(nsIChannel *aOldChannel,
                                                 nsIChannel *aNewChannel,
                                                 uint32_t    aFlags,
                                                 nsIAsyncVerifyRedirectCallback *callback)
{
  NS_PRECONDITION(aNewChannel, "Redirect without a channel?");

  // Prepare to receive callback
  mRedirectCallback = callback;
  mNewRedirectChannel = aNewChannel;

  if (mChannelEventSink) {
    nsCOMPtr<nsIAsyncVerifyRedirectCallback> fwd =
      EnsureXPCOMifier();

    nsresult rv = mChannelEventSink->AsyncOnChannelRedirect(aOldChannel,
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

nsresult
XMLHttpRequestMainThread::OnRedirectVerifyCallback(nsresult result)
{
  NS_ASSERTION(mRedirectCallback, "mRedirectCallback not set in callback");
  NS_ASSERTION(mNewRedirectChannel, "mNewRedirectChannel not set in callback");

  if (NS_SUCCEEDED(result)) {
    mChannel = mNewRedirectChannel;

    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));
    if (httpChannel) {
      // Ensure all original headers are duplicated for the new channel (bug #553888)
      mAuthorRequestHeaders.ApplyToChannel(httpChannel);
    }
  } else {
    mErrorLoad = ErrorType::eRedirect;
  }

  mNewRedirectChannel = nullptr;

  mRedirectCallback->OnRedirectVerifyCallback(result);
  mRedirectCallback = nullptr;

  // It's important that we return success here. If we return the result code
  // that we were passed, JavaScript callers who cancel the redirect will wind
  // up throwing an exception in the process.
  return NS_OK;
}

/////////////////////////////////////////////////////
// nsIProgressEventSink methods:
//

NS_IMETHODIMP
XMLHttpRequestMainThread::OnProgress(nsIRequest *aRequest, nsISupports *aContext, int64_t aProgress, int64_t aProgressMax)
{
  // When uploading, OnProgress reports also headers in aProgress and aProgressMax.
  // So, try to remove the headers, if possible.
  bool lengthComputable = (aProgressMax != -1);
  if (InUploadPhase()) {
    int64_t loaded = aProgress;
    if (lengthComputable) {
      int64_t headerSize = aProgressMax - mUploadTotal;
      loaded -= headerSize;
    }
    mUploadTransferred = loaded;
    mProgressSinceLastProgressEvent = true;

    if (!mFlagSynchronous && !mProgressTimerIsActive) {
      StartProgressEventTimer();
    }
  } else {
    mLoadTotal = aProgressMax;
    mLoadTransferred = aProgress;
    // OnDataAvailable() handles mProgressSinceLastProgressEvent
    // for the download phase.
  }

  if (mProgressEventSink) {
    mProgressEventSink->OnProgress(aRequest, aContext, aProgress,
                                   aProgressMax);
  }

  return NS_OK;
}

NS_IMETHODIMP
XMLHttpRequestMainThread::OnStatus(nsIRequest *aRequest, nsISupports *aContext, nsresult aStatus, const char16_t *aStatusArg)
{
  if (mProgressEventSink) {
    mProgressEventSink->OnStatus(aRequest, aContext, aStatus, aStatusArg);
  }

  return NS_OK;
}

bool
XMLHttpRequestMainThread::AllowUploadProgress()
{
  return !IsCrossSiteCORSRequest() ||
         mFlagHadUploadListenersOnSend;
}

/////////////////////////////////////////////////////
// nsIInterfaceRequestor methods:
//
NS_IMETHODIMP
XMLHttpRequestMainThread::GetInterface(const nsIID & aIID, void **aResult)
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

  if (mFlagBackgroundRequest) {
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

    nsCOMPtr<nsPIDOMWindowOuter> window;
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
XMLHttpRequestMainThread::GetInterface(JSContext* aCx, nsIJSID* aIID,
                                       JS::MutableHandle<JS::Value> aRetval,
                                       ErrorResult& aRv)
{
  dom::GetInterface(aCx, this, aIID, aRetval, aRv);
}

XMLHttpRequestUpload*
XMLHttpRequestMainThread::GetUpload(ErrorResult& aRv)
{
  if (!mUpload) {
    mUpload = new XMLHttpRequestUpload(this);
  }
  return mUpload;
}

bool
XMLHttpRequestMainThread::MozAnon() const
{
  return mIsAnon;
}

bool
XMLHttpRequestMainThread::MozSystem() const
{
  return IsSystemXHR();
}

void
XMLHttpRequestMainThread::HandleTimeoutCallback()
{
  if (mState == XMLHttpRequestBinding::DONE) {
    NS_NOTREACHED("XMLHttpRequestMainThread::HandleTimeoutCallback with completed request");
    // do nothing!
    return;
  }

  mFlagTimedOut = true;
  CloseRequestWithError(ProgressEventType::timeout);
}

NS_IMETHODIMP
XMLHttpRequestMainThread::Notify(nsITimer* aTimer)
{
  if (mProgressNotifier == aTimer) {
    HandleProgressTimerCallback();
    return NS_OK;
  }

  if (mTimeoutTimer == aTimer) {
    HandleTimeoutCallback();
    return NS_OK;
  }

  if (mSyncTimeoutTimer == aTimer) {
    HandleSyncTimeoutTimer();
    return NS_OK;
  }

  // Just in case some JS user wants to QI to nsITimerCallback and play with us...
  NS_WARNING("Unexpected timer!");
  return NS_ERROR_INVALID_POINTER;
}

void
XMLHttpRequestMainThread::HandleProgressTimerCallback()
{
  // Don't fire the progress event if mLoadTotal is 0, see XHR spec step 6.1
  if (!mLoadTotal && mLoadTransferred) {
    return;
  }

  mProgressTimerIsActive = false;

  if (!mProgressSinceLastProgressEvent || mErrorLoad != ErrorType::eOK) {
    return;
  }

  if (InUploadPhase()) {
    if (mUpload && !mUploadComplete && mFlagHadUploadListenersOnSend) {
      DispatchProgressEvent(mUpload, ProgressEventType::progress,
                            mUploadTransferred, mUploadTotal);
    }
  } else {
    FireReadystatechangeEvent();
    DispatchProgressEvent(this, ProgressEventType::progress,
                          mLoadTransferred, mLoadTotal);
  }

  mProgressSinceLastProgressEvent = false;

  StartProgressEventTimer();
}

void
XMLHttpRequestMainThread::StopProgressEventTimer()
{
  if (mProgressNotifier) {
    mProgressTimerIsActive = false;
    mProgressNotifier->Cancel();
  }
}

void
XMLHttpRequestMainThread::StartProgressEventTimer()
{
  if (!mProgressNotifier) {
    mProgressNotifier = NS_NewTimer();
    SetTimerEventTarget(mProgressNotifier);
  }
  if (mProgressNotifier) {
    mProgressTimerIsActive = true;
    mProgressNotifier->Cancel();
    mProgressNotifier->InitWithCallback(this, NS_PROGRESS_EVENT_INTERVAL,
                                        nsITimer::TYPE_ONE_SHOT);
  }
}

XMLHttpRequestMainThread::SyncTimeoutType
XMLHttpRequestMainThread::MaybeStartSyncTimeoutTimer()
{
  MOZ_ASSERT(mFlagSynchronous);

  nsIDocument* doc = GetDocumentIfCurrent();
  if (!doc || !doc->GetPageUnloadingEventTimeStamp()) {
    return eNoTimerNeeded;
  }

  // If we are in a beforeunload or a unload event, we must force a timeout.
  TimeDuration diff = (TimeStamp::NowLoRes() - doc->GetPageUnloadingEventTimeStamp());
  if (diff.ToMilliseconds() > MAX_SYNC_TIMEOUT_WHEN_UNLOADING) {
    return eErrorOrExpired;
  }

  mSyncTimeoutTimer = NS_NewTimer();
  SetTimerEventTarget(mSyncTimeoutTimer);
  if (!mSyncTimeoutTimer) {
    return eErrorOrExpired;
  }

  uint32_t timeout = MAX_SYNC_TIMEOUT_WHEN_UNLOADING - diff.ToMilliseconds();
  nsresult rv = mSyncTimeoutTimer->InitWithCallback(this, timeout,
                                                    nsITimer::TYPE_ONE_SHOT);
  return NS_FAILED(rv) ? eErrorOrExpired : eTimerStarted;
}

void
XMLHttpRequestMainThread::HandleSyncTimeoutTimer()
{
  MOZ_ASSERT(mSyncTimeoutTimer);
  MOZ_ASSERT(mFlagSyncLooping);

  CancelSyncTimeoutTimer();
  Abort();
}

void
XMLHttpRequestMainThread::CancelSyncTimeoutTimer()
{
  if (mSyncTimeoutTimer) {
    mSyncTimeoutTimer->Cancel();
    mSyncTimeoutTimer = nullptr;
  }
}

already_AddRefed<nsXMLHttpRequestXPCOMifier>
XMLHttpRequestMainThread::EnsureXPCOMifier()
{
  if (!mXPCOMifier) {
    mXPCOMifier = new nsXMLHttpRequestXPCOMifier(this);
  }
  RefPtr<nsXMLHttpRequestXPCOMifier> newRef(mXPCOMifier);
  return newRef.forget();
}

bool
XMLHttpRequestMainThread::ShouldBlockAuthPrompt()
{
  // Verify that it's ok to prompt for credentials here, per spec
  // http://xhr.spec.whatwg.org/#the-send%28%29-method

  if (mAuthorRequestHeaders.Has("authorization")) {
    return true;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = mChannel->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  // Also skip if a username and/or password is provided in the URI.
  nsCString username;
  rv = uri->GetUsername(username);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  nsCString password;
  rv = uri->GetPassword(password);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  if (!username.IsEmpty() || !password.IsEmpty()) {
    return true;
  }

  return false;
}

void
XMLHttpRequestMainThread::TruncateResponseText()
{
  mResponseText.Truncate();
  XMLHttpRequestBinding::ClearCachedResponseTextValue(this);
}

NS_IMPL_ISUPPORTS(XMLHttpRequestMainThread::nsHeaderVisitor, nsIHttpHeaderVisitor)

NS_IMETHODIMP XMLHttpRequestMainThread::
nsHeaderVisitor::VisitHeader(const nsACString &header, const nsACString &value)
{
  if (mXHR.IsSafeHeader(header, mHttpChannel)) {
    if (!IsLowercaseResponseHeader()) {
      if(!mHeaderList.InsertElementSorted(HeaderEntry(header, value),
                                          fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      return NS_OK;
    }

    nsAutoCString lowerHeader(header);
    ToLowerCase(lowerHeader);
    if (!mHeaderList.InsertElementSorted(HeaderEntry(lowerHeader, value),
                                         fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return NS_OK;
}

void
XMLHttpRequestMainThread::MaybeCreateBlobStorage()
{
  MOZ_ASSERT(mResponseType == XMLHttpRequestResponseType::Blob);

  if (mBlobStorage) {
    return;
  }

  MutableBlobStorage::MutableBlobStorageType storageType =
    BasePrincipal::Cast(mPrincipal)->PrivateBrowsingId() == 0
      ? MutableBlobStorage::eCouldBeInTemporaryFile
      : MutableBlobStorage::eOnlyInMemory;

  nsCOMPtr<nsIEventTarget> eventTarget;
  if (nsCOMPtr<nsIGlobalObject> global = GetOwnerGlobal()) {
    eventTarget = global->EventTargetFor(TaskCategory::Other);
  }

  mBlobStorage = new MutableBlobStorage(storageType, eventTarget);
}

void
XMLHttpRequestMainThread::BlobStoreCompleted(MutableBlobStorage* aBlobStorage,
                                             Blob* aBlob, nsresult aRv)
{
  // Ok, the state is changed...
  if (mBlobStorage != aBlobStorage || NS_FAILED(aRv)) {
    return;
  }

  MOZ_ASSERT(mState != XMLHttpRequestBinding::DONE);

  mResponseBlob = aBlob;
  mBlobStorage = nullptr;

  ChangeStateToDone();
}

NS_IMETHODIMP
XMLHttpRequestMainThread::GetName(nsACString& aName)
{
  aName.AssignLiteral("XMLHttpRequest");
  return NS_OK;
}

// nsXMLHttpRequestXPCOMifier implementation
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXMLHttpRequestXPCOMifier)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIChannelEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncVerifyRedirectCallback)
  NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
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

    // The memory-mapped contents will be released when the ArrayBuffer becomes
    // detached or is GC'd.
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
  nsresult rv;

  // Open Jar file to get related attributes of target file.
  RefPtr<nsZipArchive> zip = new nsZipArchive();
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

RequestHeaders::RequestHeader*
RequestHeaders::Find(const nsACString& aName)
{
  const nsCaseInsensitiveCStringComparator ignoreCase;
  for (RequestHeaders::RequestHeader& header : mHeaders) {
    if (header.mName.Equals(aName, ignoreCase)) {
      return &header;
    }
  }
  return nullptr;
}

bool
RequestHeaders::Has(const char* aName)
{
  return Has(nsDependentCString(aName));
}

bool
RequestHeaders::Has(const nsACString& aName)
{
  return !!Find(aName);
}

void
RequestHeaders::Get(const char* aName, nsACString& aValue)
{
  Get(nsDependentCString(aName), aValue);
}

void
RequestHeaders::Get(const nsACString& aName, nsACString& aValue)
{
  RequestHeader* header = Find(aName);
  if (header) {
    aValue = header->mValue;
  } else {
    aValue.SetIsVoid(true);
  }
}

void
RequestHeaders::Set(const char* aName, const nsACString& aValue)
{
  Set(nsDependentCString(aName), aValue);
}

void
RequestHeaders::Set(const nsACString& aName, const nsACString& aValue)
{
  RequestHeader* header = Find(aName);
  if (header) {
    header->mValue.Assign(aValue);
  } else {
    RequestHeader newHeader = {
      nsCString(aName), nsCString(aValue)
    };
    mHeaders.AppendElement(newHeader);
  }
}

void
RequestHeaders::MergeOrSet(const char* aName, const nsACString& aValue)
{
  MergeOrSet(nsDependentCString(aName), aValue);
}

void
RequestHeaders::MergeOrSet(const nsACString& aName, const nsACString& aValue)
{
  RequestHeader* header = Find(aName);
  if (header) {
    header->mValue.AppendLiteral(", ");
    header->mValue.Append(aValue);
  } else {
    RequestHeader newHeader = {
      nsCString(aName), nsCString(aValue)
    };
    mHeaders.AppendElement(newHeader);
  }
}

void
RequestHeaders::Clear()
{
  mHeaders.Clear();
}

void
RequestHeaders::ApplyToChannel(nsIHttpChannel* aHttpChannel) const
{
  for (const RequestHeader& header : mHeaders) {
    if (header.mValue.IsEmpty()) {
      DebugOnly<nsresult> rv =
        aHttpChannel->SetEmptyRequestHeader(header.mName);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    } else {
      DebugOnly<nsresult> rv =
        aHttpChannel->SetRequestHeader(header.mName, header.mValue, false);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }
}

void
RequestHeaders::GetCORSUnsafeHeaders(nsTArray<nsCString>& aArray) const
{
  static const char *kCrossOriginSafeHeaders[] = {
    "accept", "accept-language", "content-language", "content-type",
    "last-event-id"
  };
  const uint32_t kCrossOriginSafeHeadersLength =
    ArrayLength(kCrossOriginSafeHeaders);
  for (const RequestHeader& header : mHeaders) {
    bool safe = false;
    for (uint32_t i = 0; i < kCrossOriginSafeHeadersLength; ++i) {
      if (header.mName.LowerCaseEqualsASCII(kCrossOriginSafeHeaders[i])) {
        safe = true;
        break;
      }
    }
    if (!safe) {
      aArray.AppendElement(header.mName);
    }
  }
}

RequestHeaders::CharsetIterator::CharsetIterator(nsACString& aSource) :
  mValid(false),
  mCurPos(-1),
  mCurLen(-1),
  mCutoff(aSource.Length()),
  mSource(aSource)
{
}

bool
RequestHeaders::CharsetIterator::Equals(const nsACString& aOther,
                                        const nsCStringComparator& aCmp) const
{
  if (mValid) {
    return Substring(mSource, mCurPos, mCurLen).Equals(aOther, aCmp);
  } else {
    return false;
  }
}

void
RequestHeaders::CharsetIterator::Replace(const nsACString& aReplacement)
{
  if (mValid) {
    mSource.Replace(mCurPos, mCurLen, aReplacement);
    mCurLen = aReplacement.Length();
  }
}

bool
RequestHeaders::CharsetIterator::Next()
{
  int32_t start, end;
  nsAutoCString charset;

  // Look for another charset declaration in the string, limiting the
  // search to only the characters before the parts we've already searched
  // (before mCutoff), so that we don't find the same charset twice.
  NS_ExtractCharsetFromContentType(Substring(mSource, 0, mCutoff),
                                   charset, &mValid, &start, &end);

  if (!mValid) {
    return false;
  }

  // Everything after the = sign is the part of the charset we want.
  mCurPos = mSource.FindChar('=', start) + 1;
  mCurLen = end - mCurPos;

  // Special case: the extracted charset is quoted with single quotes.
  // For the purpose of preserving what was set we want to handle them
  // as delimiters (although they aren't really).
  if (charset.Length() >= 2 &&
      charset.First() == '\'' &&
      charset.Last() == '\'') {
    ++mCurPos;
    mCurLen -= 2;
  }

  mCutoff = start;

  return true;
}

} // dom namespace
} // mozilla namespace
