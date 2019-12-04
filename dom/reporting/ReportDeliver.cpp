/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/EndpointForReportChild.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ReportDeliver.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/RequestBinding.h"
#include "mozilla/dom/Response.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsGlobalWindowInner.h"
#include "nsIGlobalObject.h"
#include "nsNetUtil.h"
#include "nsStringStream.h"

namespace mozilla {
namespace dom {

namespace {

StaticRefPtr<ReportDeliver> gReportDeliver;

class ReportFetchHandler final : public PromiseNativeHandler {
 public:
  NS_DECL_ISUPPORTS

  explicit ReportFetchHandler(const ReportDeliver::ReportData& aReportData)
      : mReportData(aReportData) {}

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    if (!gReportDeliver) {
      return;
    }

    if (NS_WARN_IF(!aValue.isObject())) {
      return;
    }

    JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
    MOZ_ASSERT(obj);

    {
      Response* response = nullptr;
      if (NS_WARN_IF(NS_FAILED(UNWRAP_OBJECT(Response, &obj, response)))) {
        return;
      }

      if (response->Status() == 410) {
        mozilla::ipc::PBackgroundChild* actorChild =
            mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();

        mozilla::ipc::PrincipalInfo principalInfo;
        nsresult rv =
            PrincipalToPrincipalInfo(mReportData.mPrincipal, &principalInfo);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return;
        }

        actorChild->SendRemoveEndpoint(mReportData.mGroupName,
                                       mReportData.mEndpointURL, principalInfo);
      }
    }
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    if (gReportDeliver) {
      ++mReportData.mFailures;
      gReportDeliver->AppendReportData(mReportData);
    }
  }

 private:
  ~ReportFetchHandler() = default;

  ReportDeliver::ReportData mReportData;
};

NS_IMPL_ISUPPORTS0(ReportFetchHandler)

// This RAII class keeps a list of sandboxed globals for the delivering of
// reports. In this way, if we have to deliver more than 1 report to the same
// origin, we reuse the sandbox.
class MOZ_RAII SandboxGlobalHolder final {
 public:
  nsIGlobalObject* GetOrCreateSandboxGlobalObject(nsIPrincipal* aPrincipal) {
    MOZ_ASSERT(aPrincipal);

    nsAutoCString origin;
    nsresult rv = aPrincipal->GetOrigin(origin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    nsCOMPtr<nsIGlobalObject> globalObject = mGlobals.Get(origin);
    if (globalObject) {
      return globalObject;
    }

    nsIXPConnect* xpc = nsContentUtils::XPConnect();
    MOZ_ASSERT(xpc, "This should never be null!");

    AutoJSAPI jsapi;
    jsapi.Init();

    JSContext* cx = jsapi.cx();
    JS::Rooted<JSObject*> sandbox(cx);
    rv = xpc->CreateSandbox(cx, aPrincipal, sandbox.address());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    // The JSContext is not in a realm, so CreateSandbox returned an unwrapped
    // global.
    MOZ_ASSERT(JS_IsGlobalObject(sandbox));

    globalObject = xpc::NativeGlobal(sandbox);
    if (NS_WARN_IF(!globalObject)) {
      return nullptr;
    }

    if (NS_WARN_IF(!mGlobals.Put(origin, globalObject, fallible))) {
      return nullptr;
    }

    return globalObject;
  }

 private:
  nsInterfaceHashtable<nsCStringHashKey, nsIGlobalObject> mGlobals;
};

struct StringWriteFunc final : public JSONWriteFunc {
  nsACString&
      mBuffer;  // The lifetime of the struct must be bound to the buffer
  explicit StringWriteFunc(nsACString& aBuffer) : mBuffer(aBuffer) {}

  void Write(const char* aStr) override { mBuffer.Append(aStr); }
};

class ReportJSONWriter final : public JSONWriter {
 public:
  explicit ReportJSONWriter(nsACString& aOutput)
      : JSONWriter(MakeUnique<StringWriteFunc>(aOutput)) {}

  void JSONProperty(const char* aProperty, const char* aJSON) {
    Separator();
    PropertyNameAndColon(aProperty);
    mWriter->Write(aJSON);
  }
};

void SendReport(ReportDeliver::ReportData& aReportData,
                SandboxGlobalHolder& aHolder) {
  nsCOMPtr<nsIGlobalObject> globalObject =
      aHolder.GetOrCreateSandboxGlobalObject(aReportData.mPrincipal);
  if (NS_WARN_IF(!globalObject)) {
    return;
  }

  // The body
  nsAutoCString body;
  ReportJSONWriter w(body);

  w.Start();

  w.IntProperty(
      "age", (TimeStamp::Now() - aReportData.mCreationTime).ToMilliseconds());
  w.StringProperty("type", NS_ConvertUTF16toUTF8(aReportData.mType).get());
  w.StringProperty("url", NS_ConvertUTF16toUTF8(aReportData.mURL).get());
  w.StringProperty("user_agent",
                   NS_ConvertUTF16toUTF8(aReportData.mUserAgent).get());
  w.JSONProperty("body", aReportData.mReportBodyJSON.get());
  w.End();

  // The body as stream
  nsCOMPtr<nsIInputStream> streamBody;
  nsresult rv = NS_NewCStringInputStream(getter_AddRefs(streamBody), body);

  // Headers
  IgnoredErrorResult error;
  RefPtr<InternalHeaders> internalHeaders =
      new InternalHeaders(HeadersGuardEnum::Request);
  internalHeaders->Set(NS_LITERAL_CSTRING("Content-Type"),
                       NS_LITERAL_CSTRING("application/reports+json"), error);
  if (NS_WARN_IF(error.Failed())) {
    return;
  }

  // URL and fragments
  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), aReportData.mEndpointURL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsIURI> uriClone;
  rv = NS_GetURIWithoutRef(uri, getter_AddRefs(uriClone));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsAutoCString uriSpec;
  rv = uriClone->GetSpec(uriSpec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsAutoCString uriFragment;
  rv = uri->GetRef(uriFragment);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  RefPtr<InternalRequest> internalRequest =
      new InternalRequest(uriSpec, uriFragment);

  internalRequest->SetMethod(NS_LITERAL_CSTRING("POST"));
  internalRequest->SetBody(streamBody, body.Length());
  internalRequest->SetHeaders(internalHeaders);
  internalRequest->SetSkipServiceWorker();
  // TODO: internalRequest->SetContentPolicyType(TYPE_REPORT);
  internalRequest->SetMode(RequestMode::Cors);
  internalRequest->SetCredentialsMode(RequestCredentials::Include);

  RefPtr<Request> request = new Request(globalObject, internalRequest, nullptr);

  RequestOrUSVString fetchInput;
  fetchInput.SetAsRequest() = request;

  RefPtr<Promise> promise = FetchRequest(
      globalObject, fetchInput, RequestInit(), CallerType::NonSystem, error);
  if (error.Failed()) {
    ++aReportData.mFailures;
    if (gReportDeliver) {
      gReportDeliver->AppendReportData(aReportData);
    }
    return;
  }

  RefPtr<ReportFetchHandler> handler = new ReportFetchHandler(aReportData);
  promise->AppendNativeHandler(handler);
}

}  // namespace

/* static */
void ReportDeliver::Record(nsPIDOMWindowInner* aWindow, const nsAString& aType,
                           const nsAString& aGroupName, const nsAString& aURL,
                           ReportBody* aBody) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aBody);

  nsAutoCString reportBodyJSON;
  ReportJSONWriter w(reportBodyJSON);

  w.Start();
  aBody->ToJSON(w);
  w.End();

  nsCOMPtr<nsIPrincipal> principal =
      nsGlobalWindowInner::Cast(aWindow)->GetPrincipal();
  if (NS_WARN_IF(!principal)) {
    return;
  }

  mozilla::ipc::PrincipalInfo principalInfo;
  nsresult rv = PrincipalToPrincipalInfo(principal, &principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  mozilla::ipc::PBackgroundChild* actorChild =
      mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();

  PEndpointForReportChild* actor =
      actorChild->SendPEndpointForReportConstructor(nsString(aGroupName),
                                                    principalInfo);
  if (NS_WARN_IF(!actor)) {
    return;
  }

  ReportData data;
  data.mType = aType;
  data.mGroupName = aGroupName;
  data.mURL = aURL;
  data.mCreationTime = TimeStamp::Now();
  data.mReportBodyJSON = reportBodyJSON;
  data.mPrincipal = principal;
  data.mFailures = 0;

  Navigator* navigator = aWindow->Navigator();
  MOZ_ASSERT(navigator);

  IgnoredErrorResult error;
  navigator->GetUserAgent(data.mUserAgent, CallerType::NonSystem, error);
  if (NS_WARN_IF(error.Failed())) {
    return;
  }

  static_cast<EndpointForReportChild*>(actor)->Initialize(data);
}

/* static */
void ReportDeliver::Fetch(const ReportData& aReportData) {
  if (!gReportDeliver) {
    RefPtr<ReportDeliver> rd = new ReportDeliver();

    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (NS_WARN_IF(!obs)) {
      return;
    }

    obs->AddObserver(rd, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
    gReportDeliver = rd;
  }

  gReportDeliver->AppendReportData(aReportData);
}

void ReportDeliver::AppendReportData(const ReportData& aReportData) {
  if (aReportData.mFailures >
      StaticPrefs::dom_reporting_delivering_maxFailures()) {
    return;
  }

  if (NS_WARN_IF(!mReportQueue.AppendElement(aReportData, fallible))) {
    return;
  }

  while (mReportQueue.Length() >
         StaticPrefs::dom_reporting_delivering_maxReports()) {
    mReportQueue.RemoveElementAt(0);
  }

  if (!mTimer) {
    uint32_t timeout = StaticPrefs::dom_reporting_delivering_timeout() * 1000;
    nsresult rv = NS_NewTimerWithCallback(
        getter_AddRefs(mTimer), this, timeout, nsITimer::TYPE_ONE_SHOT,
        SystemGroup::EventTargetFor(TaskCategory::Other));
    Unused << NS_WARN_IF(NS_FAILED(rv));
  }
}

NS_IMETHODIMP
ReportDeliver::Notify(nsITimer* aTimer) {
  mTimer = nullptr;

  nsTArray<ReportData> reports;
  reports.SwapElements(mReportQueue);

  SandboxGlobalHolder holder;

  for (ReportData& report : reports) {
    SendReport(report, holder);
  }

  return NS_OK;
}

NS_IMETHODIMP
ReportDeliver::Observe(nsISupports* aSubject, const char* aTopic,
                       const char16_t* aData) {
  MOZ_ASSERT(!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID));

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return NS_OK;
  }

  obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  gReportDeliver = nullptr;
  return NS_OK;
}

ReportDeliver::ReportDeliver() = default;

ReportDeliver::~ReportDeliver() = default;

NS_INTERFACE_MAP_BEGIN(ReportDeliver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(ReportDeliver)
NS_IMPL_RELEASE(ReportDeliver)

}  // namespace dom
}  // namespace mozilla
