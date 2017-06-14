/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerEvents.h"

#include "nsAutoPtr.h"
#include "nsIConsoleReportCollector.h"
#include "nsIHttpChannelInternal.h"
#include "nsINetworkInterceptController.h"
#include "nsIOutputStream.h"
#include "nsIScriptError.h"
#include "nsITimedChannel.h"
#include "mozilla/Encoding.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsStreamUtils.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsSerializationHelper.h"
#include "nsQueryObject.h"
#include "ServiceWorkerClient.h"
#include "ServiceWorkerManager.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/BodyUtil.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/dom/FetchEventBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/PushEventBinding.h"
#include "mozilla/dom/PushMessageDataBinding.h"
#include "mozilla/dom/PushUtil.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/workers/bindings/ServiceWorker.h"

#include "js/Conversions.h"
#include "js/TypeDecls.h"
#include "WorkerPrivate.h"
#include "xpcpublic.h"

using namespace mozilla::dom;
using namespace mozilla::dom::workers;

namespace {

void
AsyncLog(nsIInterceptedChannel *aInterceptedChannel,
         const nsACString& aRespondWithScriptSpec,
         uint32_t aRespondWithLineNumber, uint32_t aRespondWithColumnNumber,
         const nsACString& aMessageName, const nsTArray<nsString>& aParams)
{
  MOZ_ASSERT(aInterceptedChannel);
  nsCOMPtr<nsIConsoleReportCollector> reporter =
    aInterceptedChannel->GetConsoleReportCollector();
  if (reporter) {
    reporter->AddConsoleReport(nsIScriptError::errorFlag,
                               NS_LITERAL_CSTRING("Service Worker Interception"),
                               nsContentUtils::eDOM_PROPERTIES,
                               aRespondWithScriptSpec,
                               aRespondWithLineNumber,
                               aRespondWithColumnNumber,
                               aMessageName, aParams);
  }
}

template<typename... Params>
void
AsyncLog(nsIInterceptedChannel* aInterceptedChannel,
         const nsACString& aRespondWithScriptSpec,
         uint32_t aRespondWithLineNumber, uint32_t aRespondWithColumnNumber,
         // We have to list one explicit string so that calls with an
         // nsTArray of params won't end up in here.
         const nsACString& aMessageName, const nsAString& aFirstParam,
         Params&&... aParams)
{
  nsTArray<nsString> paramsList(sizeof...(Params) + 1);
  StringArrayAppender::Append(paramsList, sizeof...(Params) + 1,
                              aFirstParam, Forward<Params>(aParams)...);
  AsyncLog(aInterceptedChannel, aRespondWithScriptSpec, aRespondWithLineNumber,
           aRespondWithColumnNumber, aMessageName, paramsList);
}

} // anonymous namespace

BEGIN_WORKERS_NAMESPACE

CancelChannelRunnable::CancelChannelRunnable(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                                             nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration,
                                             nsresult aStatus)
  : mChannel(aChannel)
  , mRegistration(aRegistration)
  , mStatus(aStatus)
{
}

NS_IMETHODIMP
CancelChannelRunnable::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  // TODO: When bug 1204254 is implemented, this time marker should be moved to
  // the point where the body of the network request is complete.
  mChannel->SetHandleFetchEventEnd(TimeStamp::Now());
  mChannel->SaveTimeStamps();

  mChannel->Cancel(mStatus);
  mRegistration->MaybeScheduleUpdate();
  return NS_OK;
}

FetchEvent::FetchEvent(EventTarget* aOwner)
  : ExtendableEvent(aOwner)
  , mPreventDefaultLineNumber(0)
  , mPreventDefaultColumnNumber(0)
  , mIsReload(false)
  , mWaitToRespond(false)
{
}

FetchEvent::~FetchEvent()
{
}

void
FetchEvent::PostInit(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                     nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration,
                     const nsACString& aScriptSpec)
{
  mChannel = aChannel;
  mRegistration = aRegistration;
  mScriptSpec.Assign(aScriptSpec);
}

/*static*/ already_AddRefed<FetchEvent>
FetchEvent::Constructor(const GlobalObject& aGlobal,
                        const nsAString& aType,
                        const FetchEventInit& aOptions,
                        ErrorResult& aRv)
{
  RefPtr<EventTarget> owner = do_QueryObject(aGlobal.GetAsSupports());
  MOZ_ASSERT(owner);
  RefPtr<FetchEvent> e = new FetchEvent(owner);
  bool trusted = e->Init(owner);
  e->InitEvent(aType, aOptions.mBubbles, aOptions.mCancelable);
  e->SetTrusted(trusted);
  e->SetComposed(aOptions.mComposed);
  e->mRequest = aOptions.mRequest;
  e->mClientId = aOptions.mClientId;
  e->mIsReload = aOptions.mIsReload;
  return e.forget();
}

namespace {

class FinishResponse final : public Runnable
{
  nsMainThreadPtrHandle<nsIInterceptedChannel> mChannel;
  RefPtr<InternalResponse> mInternalResponse;
  ChannelInfo mWorkerChannelInfo;
  const nsCString mScriptSpec;
  const nsCString mResponseURLSpec;

public:
  FinishResponse(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                 InternalResponse* aInternalResponse,
                 const ChannelInfo& aWorkerChannelInfo,
                 const nsACString& aScriptSpec,
                 const nsACString& aResponseURLSpec)
    : mChannel(aChannel)
    , mInternalResponse(aInternalResponse)
    , mWorkerChannelInfo(aWorkerChannelInfo)
    , mScriptSpec(aScriptSpec)
    , mResponseURLSpec(aResponseURLSpec)
  {
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();

    nsCOMPtr<nsIChannel> underlyingChannel;
    nsresult rv = mChannel->GetChannel(getter_AddRefs(underlyingChannel));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(underlyingChannel, NS_ERROR_UNEXPECTED);
    nsCOMPtr<nsILoadInfo> loadInfo = underlyingChannel->GetLoadInfo();

    if (!loadInfo || !CSPPermitsResponse(loadInfo)) {
      mChannel->Cancel(NS_ERROR_CONTENT_BLOCKED);
      return NS_OK;
    }

    ChannelInfo channelInfo;
    if (mInternalResponse->GetChannelInfo().IsInitialized()) {
      channelInfo = mInternalResponse->GetChannelInfo();
    } else {
      // We are dealing with a synthesized response here, so fall back to the
      // channel info for the worker script.
      channelInfo = mWorkerChannelInfo;
    }
    rv = mChannel->SetChannelInfo(&channelInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mChannel->Cancel(NS_ERROR_INTERCEPTION_FAILED);
      return NS_OK;
    }

    rv = mChannel->SynthesizeStatus(mInternalResponse->GetUnfilteredStatus(),
                                    mInternalResponse->GetUnfilteredStatusText());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mChannel->Cancel(NS_ERROR_INTERCEPTION_FAILED);
      return NS_OK;
    }

    AutoTArray<InternalHeaders::Entry, 5> entries;
    mInternalResponse->UnfilteredHeaders()->GetEntries(entries);
    for (uint32_t i = 0; i < entries.Length(); ++i) {
       mChannel->SynthesizeHeader(entries[i].mName, entries[i].mValue);
    }

    auto castLoadInfo = static_cast<LoadInfo*>(loadInfo.get());
    castLoadInfo->SynthesizeServiceWorkerTainting(mInternalResponse->GetTainting());

    rv = mChannel->FinishSynthesizedResponse(mResponseURLSpec);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mChannel->Cancel(NS_ERROR_INTERCEPTION_FAILED);
      return NS_OK;
    }

    TimeStamp timeStamp = TimeStamp::Now();
    mChannel->SetHandleFetchEventEnd(timeStamp);
    mChannel->SetFinishSynthesizedResponseEnd(timeStamp);
    mChannel->SaveTimeStamps();

    nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
    if (obsService) {
      obsService->NotifyObservers(underlyingChannel, "service-worker-synthesized-response", nullptr);
    }

    return rv;
  }
  bool CSPPermitsResponse(nsILoadInfo* aLoadInfo)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aLoadInfo);
    nsresult rv;
    nsCOMPtr<nsIURI> uri;
    nsCString url = mInternalResponse->GetUnfilteredURL();
    if (url.IsEmpty()) {
      // Synthetic response. The buck stops at the worker script.
      url = mScriptSpec;
    }
    rv = NS_NewURI(getter_AddRefs(uri), url, nullptr, nullptr);
    NS_ENSURE_SUCCESS(rv, false);
    int16_t decision = nsIContentPolicy::ACCEPT;
    rv = NS_CheckContentLoadPolicy(aLoadInfo->InternalContentPolicyType(), uri,
                                   aLoadInfo->LoadingPrincipal(),
                                   aLoadInfo->LoadingNode(), EmptyCString(),
                                   nullptr, &decision);
    NS_ENSURE_SUCCESS(rv, false);
    return decision == nsIContentPolicy::ACCEPT;
  }
};

class RespondWithHandler final : public PromiseNativeHandler
{
  nsMainThreadPtrHandle<nsIInterceptedChannel> mInterceptedChannel;
  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> mRegistration;
  const RequestMode mRequestMode;
  const RequestRedirect mRequestRedirectMode;
#ifdef DEBUG
  const bool mIsClientRequest;
#endif
  const nsCString mScriptSpec;
  const nsString mRequestURL;
  const nsCString mRespondWithScriptSpec;
  const uint32_t mRespondWithLineNumber;
  const uint32_t mRespondWithColumnNumber;
  bool mRequestWasHandled;
public:
  NS_DECL_ISUPPORTS

  RespondWithHandler(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                     nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration,
                     RequestMode aRequestMode, bool aIsClientRequest,
                     RequestRedirect aRedirectMode,
                     const nsACString& aScriptSpec,
                     const nsAString& aRequestURL,
                     const nsACString& aRespondWithScriptSpec,
                     uint32_t aRespondWithLineNumber,
                     uint32_t aRespondWithColumnNumber)
    : mInterceptedChannel(aChannel)
    , mRegistration(aRegistration)
    , mRequestMode(aRequestMode)
    , mRequestRedirectMode(aRedirectMode)
#ifdef DEBUG
    , mIsClientRequest(aIsClientRequest)
#endif
    , mScriptSpec(aScriptSpec)
    , mRequestURL(aRequestURL)
    , mRespondWithScriptSpec(aRespondWithScriptSpec)
    , mRespondWithLineNumber(aRespondWithLineNumber)
    , mRespondWithColumnNumber(aRespondWithColumnNumber)
    , mRequestWasHandled(false)
  {
  }

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  void CancelRequest(nsresult aStatus);

  void AsyncLog(const nsACString& aMessageName, const nsTArray<nsString>& aParams)
  {
    ::AsyncLog(mInterceptedChannel, mRespondWithScriptSpec, mRespondWithLineNumber,
               mRespondWithColumnNumber, aMessageName, aParams);
  }

  void AsyncLog(const nsACString& aSourceSpec, uint32_t aLine, uint32_t aColumn,
                const nsACString& aMessageName, const nsTArray<nsString>& aParams)
  {
    ::AsyncLog(mInterceptedChannel, aSourceSpec, aLine, aColumn, aMessageName,
               aParams);
  }

private:
  ~RespondWithHandler()
  {
    if (!mRequestWasHandled) {
      ::AsyncLog(mInterceptedChannel, mRespondWithScriptSpec,
                 mRespondWithLineNumber, mRespondWithColumnNumber,
                 NS_LITERAL_CSTRING("InterceptionFailedWithURL"), mRequestURL);
      CancelRequest(NS_ERROR_INTERCEPTION_FAILED);
    }
  }
};

struct RespondWithClosure
{
  nsMainThreadPtrHandle<nsIInterceptedChannel> mInterceptedChannel;
  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> mRegistration;
  RefPtr<InternalResponse> mInternalResponse;
  ChannelInfo mWorkerChannelInfo;
  const nsCString mScriptSpec;
  const nsCString mResponseURLSpec;
  const nsString mRequestURL;
  const nsCString mRespondWithScriptSpec;
  const uint32_t mRespondWithLineNumber;
  const uint32_t mRespondWithColumnNumber;

  RespondWithClosure(nsMainThreadPtrHandle<nsIInterceptedChannel>& aChannel,
                     nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration,
                     InternalResponse* aInternalResponse,
                     const ChannelInfo& aWorkerChannelInfo,
                     const nsCString& aScriptSpec,
                     const nsACString& aResponseURLSpec,
                     const nsAString& aRequestURL,
                     const nsACString& aRespondWithScriptSpec,
                     uint32_t aRespondWithLineNumber,
                     uint32_t aRespondWithColumnNumber)
    : mInterceptedChannel(aChannel)
    , mRegistration(aRegistration)
    , mInternalResponse(aInternalResponse)
    , mWorkerChannelInfo(aWorkerChannelInfo)
    , mScriptSpec(aScriptSpec)
    , mResponseURLSpec(aResponseURLSpec)
    , mRequestURL(aRequestURL)
    , mRespondWithScriptSpec(aRespondWithScriptSpec)
    , mRespondWithLineNumber(aRespondWithLineNumber)
    , mRespondWithColumnNumber(aRespondWithColumnNumber)
  {
  }
};

void RespondWithCopyComplete(void* aClosure, nsresult aStatus)
{
  nsAutoPtr<RespondWithClosure> data(static_cast<RespondWithClosure*>(aClosure));
  nsCOMPtr<nsIRunnable> event;
  if (NS_WARN_IF(NS_FAILED(aStatus))) {
    AsyncLog(data->mInterceptedChannel, data->mRespondWithScriptSpec,
             data->mRespondWithLineNumber, data->mRespondWithColumnNumber,
             NS_LITERAL_CSTRING("InterceptionFailedWithURL"),
             data->mRequestURL);
    event = new CancelChannelRunnable(data->mInterceptedChannel,
                                      data->mRegistration,
                                      NS_ERROR_INTERCEPTION_FAILED);
  } else {
    event = new FinishResponse(data->mInterceptedChannel,
                               data->mInternalResponse,
                               data->mWorkerChannelInfo,
                               data->mScriptSpec,
                               data->mResponseURLSpec);
  }
  // In theory this can happen after the worker thread is terminated.
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  if (worker) {
    MOZ_ALWAYS_SUCCEEDS(worker->DispatchToMainThread(event.forget()));
  } else {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(event.forget()));
  }
}

namespace {

void
ExtractErrorValues(JSContext* aCx, JS::Handle<JS::Value> aValue,
                  nsACString& aSourceSpecOut, uint32_t *aLineOut,
                  uint32_t *aColumnOut, nsString& aMessageOut)
{
  MOZ_ASSERT(aLineOut);
  MOZ_ASSERT(aColumnOut);

  if (aValue.isObject()) {
    JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
    RefPtr<DOMException> domException;

    // Try to process as an Error object.  Use the file/line/column values
    // from the Error as they will be more specific to the root cause of
    // the problem.
    JSErrorReport* err = obj ? JS_ErrorFromException(aCx, obj) : nullptr;
    if (err) {
      // Use xpc to extract the error message only.  We don't actually send
      // this report anywhere.
      RefPtr<xpc::ErrorReport> report = new xpc::ErrorReport();
      report->Init(err,
                   "<unknown>", // toString result
                   false,       // chrome
                   0);          // window ID

      if (!report->mFileName.IsEmpty()) {
        CopyUTF16toUTF8(report->mFileName, aSourceSpecOut);
        *aLineOut = report->mLineNumber;
        *aColumnOut = report->mColumn;
      }
      aMessageOut.Assign(report->mErrorMsg);
    }

    // Next, try to unwrap the rejection value as a DOMException.
    else if(NS_SUCCEEDED(UNWRAP_OBJECT(DOMException, obj, domException))) {

      nsAutoString filename;
      domException->GetFilename(aCx, filename);
      if (!filename.IsEmpty()) {
        CopyUTF16toUTF8(filename, aSourceSpecOut);
        *aLineOut = domException->LineNumber(aCx);
        *aColumnOut = domException->ColumnNumber();
      }

      domException->GetName(aMessageOut);
      aMessageOut.AppendLiteral(": ");

      nsAutoString message;
      domException->GetMessageMoz(message);
      aMessageOut.Append(message);
    }
  }

  // If we could not unwrap a specific error type, then perform default safe
  // string conversions on primitives.  Objects will result in "[Object]"
  // unfortunately.
  if (aMessageOut.IsEmpty()) {
    nsAutoJSString jsString;
    if (jsString.init(aCx, aValue)) {
      aMessageOut = jsString;
    } else {
      JS_ClearPendingException(aCx);
    }
  }
}

} // anonymous namespace

class MOZ_STACK_CLASS AutoCancel
{
  RefPtr<RespondWithHandler> mOwner;
  nsCString mSourceSpec;
  uint32_t mLine;
  uint32_t mColumn;
  nsCString mMessageName;
  nsTArray<nsString> mParams;

public:
  AutoCancel(RespondWithHandler* aOwner, const nsString& aRequestURL)
    : mOwner(aOwner)
    , mLine(0)
    , mColumn(0)
    , mMessageName(NS_LITERAL_CSTRING("InterceptionFailedWithURL"))
  {
    mParams.AppendElement(aRequestURL);
  }

  ~AutoCancel()
  {
    if (mOwner) {
      if (mSourceSpec.IsEmpty()) {
        mOwner->AsyncLog(mMessageName, mParams);
      } else {
        mOwner->AsyncLog(mSourceSpec, mLine, mColumn, mMessageName, mParams);
      }
      mOwner->CancelRequest(NS_ERROR_INTERCEPTION_FAILED);
    }
  }

  template<typename... Params>
  void SetCancelMessage(const nsACString& aMessageName, Params&&... aParams)
  {
    MOZ_ASSERT(mOwner);
    MOZ_ASSERT(mMessageName.EqualsLiteral("InterceptionFailedWithURL"));
    MOZ_ASSERT(mParams.Length() == 1);
    mMessageName = aMessageName;
    mParams.Clear();
    StringArrayAppender::Append(mParams, sizeof...(Params),
                                Forward<Params>(aParams)...);
  }

  template<typename... Params>
  void SetCancelMessageAndLocation(const nsACString& aSourceSpec,
                                   uint32_t aLine, uint32_t aColumn,
                                   const nsACString& aMessageName,
                                   Params&&... aParams)
  {
    MOZ_ASSERT(mOwner);
    MOZ_ASSERT(mMessageName.EqualsLiteral("InterceptionFailedWithURL"));
    MOZ_ASSERT(mParams.Length() == 1);

    mSourceSpec = aSourceSpec;
    mLine = aLine;
    mColumn = aColumn;

    mMessageName = aMessageName;
    mParams.Clear();
    StringArrayAppender::Append(mParams, sizeof...(Params),
                                Forward<Params>(aParams)...);
  }

  void Reset()
  {
    mOwner = nullptr;
  }
};

NS_IMPL_ISUPPORTS0(RespondWithHandler)

void
RespondWithHandler::ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  AutoCancel autoCancel(this, mRequestURL);
  mInterceptedChannel->SetFinishResponseStart(TimeStamp::Now());

  if (!aValue.isObject()) {
    NS_WARNING("FetchEvent::RespondWith was passed a promise resolved to a non-Object value");

    nsCString sourceSpec;
    uint32_t line = 0;
    uint32_t column = 0;
    nsString valueString;
    ExtractErrorValues(aCx, aValue, sourceSpec, &line, &column, valueString);

    autoCancel.SetCancelMessageAndLocation(sourceSpec, line, column,
                                           NS_LITERAL_CSTRING("InterceptedNonResponseWithURL"),
                                           mRequestURL, valueString);
    return;
  }

  RefPtr<Response> response;
  nsresult rv = UNWRAP_OBJECT(Response, &aValue.toObject(), response);
  if (NS_FAILED(rv)) {
    nsCString sourceSpec;
    uint32_t line = 0;
    uint32_t column = 0;
    nsString valueString;
    ExtractErrorValues(aCx, aValue, sourceSpec, &line, &column, valueString);

    autoCancel.SetCancelMessageAndLocation(sourceSpec, line, column,
                                           NS_LITERAL_CSTRING("InterceptedNonResponseWithURL"),
                                           mRequestURL, valueString);
    return;
  }

  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  // Section "HTTP Fetch", step 3.3:
  //  If one of the following conditions is true, return a network error:
  //    * response's type is "error".
  //    * request's mode is not "no-cors" and response's type is "opaque".
  //    * request's redirect mode is not "manual" and response's type is
  //      "opaqueredirect".
  //    * request's redirect mode is not "follow" and response's url list
  //      has more than one item.

  if (response->Type() == ResponseType::Error) {
    autoCancel.SetCancelMessage(
      NS_LITERAL_CSTRING("InterceptedErrorResponseWithURL"), mRequestURL);
    return;
  }

  MOZ_ASSERT_IF(mIsClientRequest, mRequestMode == RequestMode::Same_origin ||
                                  mRequestMode == RequestMode::Navigate);

  if (response->Type() == ResponseType::Opaque && mRequestMode != RequestMode::No_cors) {
    uint32_t mode = static_cast<uint32_t>(mRequestMode);
    NS_ConvertASCIItoUTF16 modeString(RequestModeValues::strings[mode].value,
                                      RequestModeValues::strings[mode].length);

    autoCancel.SetCancelMessage(
      NS_LITERAL_CSTRING("BadOpaqueInterceptionRequestModeWithURL"),
      mRequestURL, modeString);
    return;
  }

  if (mRequestRedirectMode != RequestRedirect::Manual &&
      response->Type() == ResponseType::Opaqueredirect) {
    autoCancel.SetCancelMessage(
      NS_LITERAL_CSTRING("BadOpaqueRedirectInterceptionWithURL"), mRequestURL);
    return;
  }

  if (mRequestRedirectMode != RequestRedirect::Follow && response->Redirected()) {
    autoCancel.SetCancelMessage(
      NS_LITERAL_CSTRING("BadRedirectModeInterceptionWithURL"), mRequestURL);
    return;
  }

  if (NS_WARN_IF(response->BodyUsed())) {
    autoCancel.SetCancelMessage(
      NS_LITERAL_CSTRING("InterceptedUsedResponseWithURL"), mRequestURL);
    return;
  }

  RefPtr<InternalResponse> ir = response->GetInternalResponse();
  if (NS_WARN_IF(!ir)) {
    return;
  }
  // When an opaque response is encountered, we need the original channel's principal
  // to reflect the final URL. Non-opaque responses are either same-origin or CORS-enabled
  // cross-origin responses, which are treated as same-origin by consumers.
  nsCString responseURL;
  if (response->Type() == ResponseType::Opaque) {
    responseURL = ir->GetUnfilteredURL();
    if (NS_WARN_IF(responseURL.IsEmpty())) {
      return;
    }
  }
  nsAutoPtr<RespondWithClosure> closure(new RespondWithClosure(mInterceptedChannel,
                                                               mRegistration, ir,
                                                               worker->GetChannelInfo(),
                                                               mScriptSpec,
                                                               responseURL,
                                                               mRequestURL,
                                                               mRespondWithScriptSpec,
                                                               mRespondWithLineNumber,
                                                               mRespondWithColumnNumber));
  nsCOMPtr<nsIInputStream> body;
  ir->GetUnfilteredBody(getter_AddRefs(body));
  // Errors and redirects may not have a body.
  if (body) {
    response->SetBodyUsed();

    nsCOMPtr<nsIOutputStream> responseBody;
    rv = mInterceptedChannel->GetResponseBody(getter_AddRefs(responseBody));
    if (NS_WARN_IF(NS_FAILED(rv)) || !responseBody) {
      return;
    }

    const uint32_t kCopySegmentSize = 4096;

    // Depending on how the Response passed to .respondWith() was created, we may
    // get a non-buffered input stream.  In addition, in some configurations the
    // destination channel's output stream can be unbuffered.  We wrap the output
    // stream side here so that NS_AsyncCopy() works.  Wrapping the output side
    // provides the most consistent operation since there are fewer stream types
    // we are writing to.  The input stream can be a wide variety of concrete
    // objects which may or many not play well with NS_InputStreamIsBuffered().
    if (!NS_OutputStreamIsBuffered(responseBody)) {
      nsCOMPtr<nsIOutputStream> buffered;
      rv = NS_NewBufferedOutputStream(getter_AddRefs(buffered), responseBody,
           kCopySegmentSize);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return;
      }
      responseBody = buffered;
    }

    nsCOMPtr<nsIEventTarget> stsThread = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
    if (NS_WARN_IF(!stsThread)) {
      return;
    }

    // XXXnsm, Fix for Bug 1141332 means that if we decide to make this
    // streaming at some point, we'll need a different solution to that bug.
    rv = NS_AsyncCopy(body, responseBody, stsThread, NS_ASYNCCOPY_VIA_WRITESEGMENTS,
                      kCopySegmentSize, RespondWithCopyComplete, closure.forget());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  } else {
    RespondWithCopyComplete(closure.forget(), NS_OK);
  }

  MOZ_ASSERT(!closure);
  autoCancel.Reset();
  mRequestWasHandled = true;
}

void
RespondWithHandler::RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  nsCString sourceSpec = mRespondWithScriptSpec;
  uint32_t line = mRespondWithLineNumber;
  uint32_t column = mRespondWithColumnNumber;
  nsString valueString;

  mInterceptedChannel->SetFinishResponseStart(TimeStamp::Now());

  ExtractErrorValues(aCx, aValue, sourceSpec, &line, &column, valueString);

  ::AsyncLog(mInterceptedChannel, sourceSpec, line, column,
             NS_LITERAL_CSTRING("InterceptionRejectedResponseWithURL"),
             mRequestURL, valueString);

  CancelRequest(NS_ERROR_INTERCEPTION_FAILED);
}

void
RespondWithHandler::CancelRequest(nsresult aStatus)
{
  nsCOMPtr<nsIRunnable> runnable =
    new CancelChannelRunnable(mInterceptedChannel, mRegistration, aStatus);
  // Note, this may run off the worker thread during worker termination.
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  if (worker) {
    MOZ_ALWAYS_SUCCEEDS(worker->DispatchToMainThread(runnable.forget()));
  } else {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable.forget()));
  }
  mRequestWasHandled = true;
}

} // namespace

void
FetchEvent::RespondWith(JSContext* aCx, Promise& aArg, ErrorResult& aRv)
{
  if (EventPhase() == nsIDOMEvent::NONE || mWaitToRespond) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }


  // Record where respondWith() was called in the script so we can include the
  // information in any error reporting.  We should be guaranteed not to get
  // a file:// string here because service workers require http/https.
  nsCString spec;
  uint32_t line = 0;
  uint32_t column = 0;
  nsJSUtils::GetCallingLocation(aCx, spec, &line, &column);

  RefPtr<InternalRequest> ir = mRequest->GetInternalRequest();

  nsAutoCString requestURL;
  ir->GetURL(requestURL);

  StopImmediatePropagation();
  mWaitToRespond = true;
  RefPtr<RespondWithHandler> handler =
    new RespondWithHandler(mChannel, mRegistration, mRequest->Mode(),
                           ir->IsClientRequest(), mRequest->Redirect(),
                           mScriptSpec, NS_ConvertUTF8toUTF16(requestURL),
                           spec, line, column);
  aArg.AppendNativeHandler(handler);

  if (!WaitOnPromise(aArg)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

void
FetchEvent::PreventDefault(JSContext* aCx, CallerType aCallerType)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aCallerType != CallerType::System,
             "Since when do we support system-principal service workers?");

  if (mPreventDefaultScriptSpec.IsEmpty()) {
    // Note when the FetchEvent might have been canceled by script, but don't
    // actually log the location until we are sure it matters.  This is
    // determined in ServiceWorkerPrivate.cpp.  We only remember the first
    // call to preventDefault() as its the most likely to have actually canceled
    // the event.
    nsJSUtils::GetCallingLocation(aCx, mPreventDefaultScriptSpec,
                                  &mPreventDefaultLineNumber,
                                  &mPreventDefaultColumnNumber);
  }

  Event::PreventDefault(aCx, aCallerType);
}

void
FetchEvent::ReportCanceled()
{
  MOZ_ASSERT(!mPreventDefaultScriptSpec.IsEmpty());

  RefPtr<InternalRequest> ir = mRequest->GetInternalRequest();
  nsAutoCString url;
  ir->GetURL(url);

  // The variadic template provided by StringArrayAppender requires exactly
  // an nsString.
  NS_ConvertUTF8toUTF16 requestURL(url);
  //nsString requestURL;
  //CopyUTF8toUTF16(url, requestURL);

  ::AsyncLog(mChannel.get(), mPreventDefaultScriptSpec,
             mPreventDefaultLineNumber, mPreventDefaultColumnNumber,
             NS_LITERAL_CSTRING("InterceptionCanceledWithURL"), requestURL);
}

namespace {

class WaitUntilHandler final : public PromiseNativeHandler
{
  WorkerPrivate* mWorkerPrivate;
  const nsCString mScope;
  nsCString mSourceSpec;
  uint32_t mLine;
  uint32_t mColumn;
  nsString mRejectValue;

  ~WaitUntilHandler()
  {
  }

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  WaitUntilHandler(WorkerPrivate* aWorkerPrivate, JSContext* aCx)
    : mWorkerPrivate(aWorkerPrivate)
    , mScope(mWorkerPrivate->ServiceWorkerScope())
    , mLine(0)
    , mColumn(0)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();

    // Save the location of the waitUntil() call itself as a fallback
    // in case the rejection value does not contain any location info.
    nsJSUtils::GetCallingLocation(aCx, mSourceSpec, &mLine, &mColumn);
  }

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    // do nothing, we are only here to report errors
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    mWorkerPrivate->AssertIsOnWorkerThread();

    nsCString spec;
    uint32_t line = 0;
    uint32_t column = 0;
    ExtractErrorValues(aCx, aValue, spec, &line, &column, mRejectValue);

    // only use the extracted location if we found one
    if (!spec.IsEmpty()) {
      mSourceSpec = spec;
      mLine = line;
      mColumn = column;
    }

    MOZ_ALWAYS_SUCCEEDS(mWorkerPrivate->DispatchToMainThread(
        NewRunnableMethod(this, &WaitUntilHandler::ReportOnMainThread)));
  }

  void
  ReportOnMainThread()
  {
    AssertIsOnMainThread();
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (!swm) {
      // browser shutdown
      return;
    }

    // TODO: Make the error message a localized string. (bug 1222720)
    nsString message;
    message.AppendLiteral("Service worker event waitUntil() was passed a "
                          "promise that rejected with '");
    message.Append(mRejectValue);
    message.AppendLiteral("'.");

    // Note, there is a corner case where this won't report to the window
    // that triggered the error.  Consider a navigation fetch event that
    // rejects waitUntil() without holding respondWith() open.  In this case
    // there is no controlling document yet, the window did call .register()
    // because there is no documeny yet, and the navigation is no longer
    // being intercepted.

    swm->ReportToAllClients(mScope, message, NS_ConvertUTF8toUTF16(mSourceSpec),
                            EmptyString(), mLine, mColumn,
                            nsIScriptError::errorFlag);
  }
};

NS_IMPL_ISUPPORTS0(WaitUntilHandler)

} // anonymous namespace

NS_IMPL_ADDREF_INHERITED(FetchEvent, ExtendableEvent)
NS_IMPL_RELEASE_INHERITED(FetchEvent, ExtendableEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FetchEvent)
NS_INTERFACE_MAP_END_INHERITING(ExtendableEvent)

NS_IMPL_CYCLE_COLLECTION_INHERITED(FetchEvent, ExtendableEvent, mRequest)

ExtendableEvent::ExtendableEvent(EventTarget* aOwner)
  : Event(aOwner, nullptr, nullptr)
{
}

bool
ExtendableEvent::WaitOnPromise(Promise& aPromise)
{
  if (!mExtensionsHandler) {
    return false;
  }
  return mExtensionsHandler->WaitOnPromise(aPromise);
}

void
ExtendableEvent::SetKeepAliveHandler(ExtensionsHandler* aExtensionsHandler)
{
  MOZ_ASSERT(!mExtensionsHandler);
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();
  mExtensionsHandler = aExtensionsHandler;
}

void
ExtendableEvent::WaitUntil(JSContext* aCx, Promise& aPromise, ErrorResult& aRv)
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (!WaitOnPromise(aPromise)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // Append our handler to each waitUntil promise separately so we
  // can record the location in script where waitUntil was called.
  RefPtr<WaitUntilHandler> handler =
    new WaitUntilHandler(GetCurrentThreadWorkerPrivate(), aCx);
  aPromise.AppendNativeHandler(handler);
}

NS_IMPL_ADDREF_INHERITED(ExtendableEvent, Event)
NS_IMPL_RELEASE_INHERITED(ExtendableEvent, Event)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ExtendableEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

namespace {
nsresult
ExtractBytesFromUSVString(const nsAString& aStr, nsTArray<uint8_t>& aBytes)
{
  MOZ_ASSERT(aBytes.IsEmpty());
  auto encoder = UTF_8_ENCODING->NewEncoder();
  CheckedInt<size_t> needed =
    encoder->MaxBufferLengthFromUTF16WithoutReplacement(aStr.Length());
  if (NS_WARN_IF(!needed.isValid() ||
                 !aBytes.SetLength(needed.value(), fallible))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  uint32_t result;
  size_t read;
  size_t written;
  Tie(result, read, written) =
    encoder->EncodeFromUTF16WithoutReplacement(aStr, aBytes, true);
  MOZ_ASSERT(result == kInputEmpty);
  MOZ_ASSERT(read == aStr.Length());
  aBytes.TruncateLength(written);
  return NS_OK;
}

nsresult
ExtractBytesFromData(const OwningArrayBufferViewOrArrayBufferOrUSVString& aDataInit, nsTArray<uint8_t>& aBytes)
{
  if (aDataInit.IsArrayBufferView()) {
    const ArrayBufferView& view = aDataInit.GetAsArrayBufferView();
    if (NS_WARN_IF(!PushUtil::CopyArrayBufferViewToArray(view, aBytes))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
  }
  if (aDataInit.IsArrayBuffer()) {
    const ArrayBuffer& buffer = aDataInit.GetAsArrayBuffer();
    if (NS_WARN_IF(!PushUtil::CopyArrayBufferToArray(buffer, aBytes))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
  }
  if (aDataInit.IsUSVString()) {
    return ExtractBytesFromUSVString(aDataInit.GetAsUSVString(), aBytes);
  }
  NS_NOTREACHED("Unexpected push message data");
  return NS_ERROR_FAILURE;
}
}

PushMessageData::PushMessageData(nsISupports* aOwner,
                                 nsTArray<uint8_t>&& aBytes)
  : mOwner(aOwner), mBytes(Move(aBytes)) {}

PushMessageData::~PushMessageData()
{
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PushMessageData, mOwner)

NS_IMPL_CYCLE_COLLECTING_ADDREF(PushMessageData)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PushMessageData)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PushMessageData)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
PushMessageData::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::PushMessageDataBinding::Wrap(aCx, this, aGivenProto);
}

void
PushMessageData::Json(JSContext* cx, JS::MutableHandle<JS::Value> aRetval,
                      ErrorResult& aRv)
{
  if (NS_FAILED(EnsureDecodedText())) {
    aRv.Throw(NS_ERROR_DOM_UNKNOWN_ERR);
    return;
  }
  BodyUtil::ConsumeJson(cx, aRetval, mDecodedText, aRv);
}

void
PushMessageData::Text(nsAString& aData)
{
  if (NS_SUCCEEDED(EnsureDecodedText())) {
    aData = mDecodedText;
  }
}

void
PushMessageData::ArrayBuffer(JSContext* cx,
                             JS::MutableHandle<JSObject*> aRetval,
                             ErrorResult& aRv)
{
  uint8_t* data = GetContentsCopy();
  if (data) {
    BodyUtil::ConsumeArrayBuffer(cx, aRetval, mBytes.Length(), data, aRv);
  }
}

already_AddRefed<mozilla::dom::Blob>
PushMessageData::Blob(ErrorResult& aRv)
{
  uint8_t* data = GetContentsCopy();
  if (data) {
    RefPtr<mozilla::dom::Blob> blob = BodyUtil::ConsumeBlob(
      mOwner, EmptyString(), mBytes.Length(), data, aRv);
    if (blob) {
      return blob.forget();
    }
  }
  return nullptr;
}

nsresult
PushMessageData::EnsureDecodedText()
{
  if (mBytes.IsEmpty() || !mDecodedText.IsEmpty()) {
    return NS_OK;
  }
  nsresult rv = BodyUtil::ConsumeText(
    mBytes.Length(),
    reinterpret_cast<uint8_t*>(mBytes.Elements()),
    mDecodedText
  );
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mDecodedText.Truncate();
    return rv;
  }
  return NS_OK;
}

uint8_t*
PushMessageData::GetContentsCopy()
{
  uint32_t length = mBytes.Length();
  void* data = malloc(length);
  if (!data) {
    return nullptr;
  }
  memcpy(data, mBytes.Elements(), length);
  return reinterpret_cast<uint8_t*>(data);
}

PushEvent::PushEvent(EventTarget* aOwner)
  : ExtendableEvent(aOwner)
{
}

already_AddRefed<PushEvent>
PushEvent::Constructor(mozilla::dom::EventTarget* aOwner,
                       const nsAString& aType,
                       const PushEventInit& aOptions,
                       ErrorResult& aRv)
{
  RefPtr<PushEvent> e = new PushEvent(aOwner);
  bool trusted = e->Init(aOwner);
  e->InitEvent(aType, aOptions.mBubbles, aOptions.mCancelable);
  e->SetTrusted(trusted);
  e->SetComposed(aOptions.mComposed);
  if(aOptions.mData.WasPassed()){
    nsTArray<uint8_t> bytes;
    nsresult rv = ExtractBytesFromData(aOptions.mData.Value(), bytes);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }
    e->mData = new PushMessageData(aOwner, Move(bytes));
  }
  return e.forget();
}

NS_IMPL_ADDREF_INHERITED(PushEvent, ExtendableEvent)
NS_IMPL_RELEASE_INHERITED(PushEvent, ExtendableEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PushEvent)
NS_INTERFACE_MAP_END_INHERITING(ExtendableEvent)

NS_IMPL_CYCLE_COLLECTION_INHERITED(PushEvent, ExtendableEvent, mData)

JSObject*
PushEvent::WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::PushEventBinding::Wrap(aCx, this, aGivenProto);
}

ExtendableMessageEvent::ExtendableMessageEvent(EventTarget* aOwner)
  : ExtendableEvent(aOwner)
  , mData(JS::UndefinedValue())
{
  mozilla::HoldJSObjects(this);
}

ExtendableMessageEvent::~ExtendableMessageEvent()
{
  mData.setUndefined();
  DropJSObjects(this);
}

void
ExtendableMessageEvent::GetData(JSContext* aCx,
                                JS::MutableHandle<JS::Value> aData,
                                ErrorResult& aRv)
{
  aData.set(mData);
  if (!JS_WrapValue(aCx, aData)) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
}

void
ExtendableMessageEvent::GetSource(Nullable<OwningClientOrServiceWorkerOrMessagePort>& aValue) const
{
  if (mClient) {
    aValue.SetValue().SetAsClient() = mClient;
  } else if (mServiceWorker) {
    aValue.SetValue().SetAsServiceWorker() = mServiceWorker;
  } else if (mMessagePort) {
    aValue.SetValue().SetAsMessagePort() = mMessagePort;
  } else {
    // nullptr source is possible for manually constructed event
    aValue.SetNull();
  }
}

/* static */ already_AddRefed<ExtendableMessageEvent>
ExtendableMessageEvent::Constructor(const GlobalObject& aGlobal,
                                    const nsAString& aType,
                                    const ExtendableMessageEventInit& aOptions,
                                    ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(t, aType, aOptions, aRv);
}

/* static */ already_AddRefed<ExtendableMessageEvent>
ExtendableMessageEvent::Constructor(mozilla::dom::EventTarget* aEventTarget,
                                    const nsAString& aType,
                                    const ExtendableMessageEventInit& aOptions,
                                    ErrorResult& aRv)
{
  RefPtr<ExtendableMessageEvent> event = new ExtendableMessageEvent(aEventTarget);

  event->InitEvent(aType, aOptions.mBubbles, aOptions.mCancelable);
  bool trusted = event->Init(aEventTarget);
  event->SetTrusted(trusted);

  event->mData = aOptions.mData;
  event->mOrigin = aOptions.mOrigin;
  event->mLastEventId = aOptions.mLastEventId;

  if (!aOptions.mSource.IsNull()) {
    if (aOptions.mSource.Value().IsClient()) {
      event->mClient = aOptions.mSource.Value().GetAsClient();
    } else if (aOptions.mSource.Value().IsServiceWorker()){
      event->mServiceWorker = aOptions.mSource.Value().GetAsServiceWorker();
    } else if (aOptions.mSource.Value().IsMessagePort()){
      event->mMessagePort = aOptions.mSource.Value().GetAsMessagePort();
    }
  }

  event->mPorts.AppendElements(aOptions.mPorts);
  return event.forget();
}

void
ExtendableMessageEvent::GetPorts(nsTArray<RefPtr<MessagePort>>& aPorts)
{
  aPorts = mPorts;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(ExtendableMessageEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ExtendableMessageEvent, Event)
  tmp->mData.setUndefined();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mClient)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mServiceWorker)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessagePort)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPorts)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ExtendableMessageEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mClient)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mServiceWorker)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessagePort)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPorts)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(ExtendableMessageEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mData)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ExtendableMessageEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_ADDREF_INHERITED(ExtendableMessageEvent, Event)
NS_IMPL_RELEASE_INHERITED(ExtendableMessageEvent, Event)

END_WORKERS_NAMESPACE
