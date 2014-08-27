/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URL.h"

#include "nsDOMFile.h"
#include "nsIDocument.h"
#include "nsIIOService.h"
#include "nsPIDOMWindow.h"

#include "mozilla/dom/URL.h"
#include "mozilla/dom/URLBinding.h"
#include "mozilla/dom/URLSearchParams.h"
#include "nsGlobalWindow.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

#include "File.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"

BEGIN_WORKERS_NAMESPACE
using mozilla::dom::GlobalObject;

class URLProxy MOZ_FINAL
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(URLProxy)

  explicit URLProxy(mozilla::dom::URL* aURL)
    : mURL(aURL)
  {
    AssertIsOnMainThread();
  }

  mozilla::dom::URL* URL()
  {
    return mURL;
  }

  nsIURI* URI()
  {
    return mURL->GetURI();
  }

  void ReleaseURI()
  {
    AssertIsOnMainThread();
    mURL = nullptr;
  }

private:
  // Private destructor, to discourage deletion outside of Release():
  ~URLProxy()
  {
     MOZ_ASSERT(!mURL);
  }

  nsRefPtr<mozilla::dom::URL> mURL;
};

// This class creates an URL from a DOM Blob on the main thread.
class CreateURLRunnable : public WorkerMainThreadRunnable
{
private:
  DOMFileImpl* mBlobImpl;
  nsString& mURL;

public:
  CreateURLRunnable(WorkerPrivate* aWorkerPrivate, DOMFileImpl* aBlobImpl,
                    const mozilla::dom::objectURLOptions& aOptions,
                    nsString& aURL)
  : WorkerMainThreadRunnable(aWorkerPrivate),
    mBlobImpl(aBlobImpl),
    mURL(aURL)
  {
    MOZ_ASSERT(aBlobImpl);
  }

  bool
  MainThreadRun()
  {
    AssertIsOnMainThread();

    nsCOMPtr<nsIPrincipal> principal;
    nsIDocument* doc = nullptr;

    nsCOMPtr<nsPIDOMWindow> window = mWorkerPrivate->GetWindow();
    if (window) {
      doc = window->GetExtantDoc();
      if (!doc) {
        SetDOMStringToNull(mURL);
        return false;
      }

      principal = doc->NodePrincipal();
    } else {
      MOZ_ASSERT_IF(!mWorkerPrivate->GetParent(), mWorkerPrivate->IsChromeWorker());
      principal = mWorkerPrivate->GetPrincipal();
    }

    nsCString url;
    nsresult rv = nsHostObjectProtocolHandler::AddDataEntry(
        NS_LITERAL_CSTRING(BLOBURI_SCHEME),
        mBlobImpl, principal, url);

    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to add data entry for the blob!");
      SetDOMStringToNull(mURL);
      return false;
    }

    if (doc) {
      doc->RegisterHostObjectUri(url);
    } else {
      mWorkerPrivate->RegisterHostObjectURI(url);
    }

    mURL = NS_ConvertUTF8toUTF16(url);
    return true;
  }
};

// This class revokes an URL on the main thread.
class RevokeURLRunnable : public WorkerMainThreadRunnable
{
private:
  const nsString mURL;

public:
  RevokeURLRunnable(WorkerPrivate* aWorkerPrivate,
                    const nsAString& aURL)
  : WorkerMainThreadRunnable(aWorkerPrivate),
    mURL(aURL)
  {}

  bool
  MainThreadRun()
  {
    AssertIsOnMainThread();

    nsCOMPtr<nsIPrincipal> principal;
    nsIDocument* doc = nullptr;

    nsCOMPtr<nsPIDOMWindow> window = mWorkerPrivate->GetWindow();
    if (window) {
      doc = window->GetExtantDoc();
      if (!doc) {
        return false;
      }

      principal = doc->NodePrincipal();
    } else {
      MOZ_ASSERT_IF(!mWorkerPrivate->GetParent(), mWorkerPrivate->IsChromeWorker());
      principal = mWorkerPrivate->GetPrincipal();
    }

    NS_ConvertUTF16toUTF8 url(mURL);

    nsIPrincipal* urlPrincipal =
      nsHostObjectProtocolHandler::GetDataEntryPrincipal(url);

    bool subsumes;
    if (urlPrincipal &&
        NS_SUCCEEDED(principal->Subsumes(urlPrincipal, &subsumes)) &&
        subsumes) {
      if (doc) {
        doc->UnregisterHostObjectUri(url);
      }

      nsHostObjectProtocolHandler::RemoveDataEntry(url);
    }

    if (!window) {
      mWorkerPrivate->UnregisterHostObjectURI(url);
    }

    return true;
  }
};

// This class creates a URL object on the main thread.
class ConstructorRunnable : public WorkerMainThreadRunnable
{
private:
  const nsString mURL;

  const nsString mBase;
  nsRefPtr<URLProxy> mBaseProxy;
  mozilla::ErrorResult& mRv;

  nsRefPtr<URLProxy> mRetval;

public:
  ConstructorRunnable(WorkerPrivate* aWorkerPrivate,
                      const nsAString& aURL, const nsAString& aBase,
                      mozilla::ErrorResult& aRv)
  : WorkerMainThreadRunnable(aWorkerPrivate)
  , mURL(aURL)
  , mBase(aBase)
  , mRv(aRv)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  ConstructorRunnable(WorkerPrivate* aWorkerPrivate,
                      const nsAString& aURL, URLProxy* aBaseProxy,
                      mozilla::ErrorResult& aRv)
  : WorkerMainThreadRunnable(aWorkerPrivate)
  , mURL(aURL)
  , mBaseProxy(aBaseProxy)
  , mRv(aRv)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool
  MainThreadRun()
  {
    AssertIsOnMainThread();

    nsresult rv;
    nsCOMPtr<nsIIOService> ioService(do_GetService(NS_IOSERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv)) {
      mRv.Throw(rv);
      return true;
    }

    nsCOMPtr<nsIURI> baseURL;

    if (!mBaseProxy) {
      rv = ioService->NewURI(NS_ConvertUTF16toUTF8(mBase), nullptr, nullptr,
                             getter_AddRefs(baseURL));
      if (NS_FAILED(rv)) {
        mRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
        return true;
      }
    } else {
      baseURL = mBaseProxy->URI();
    }

    nsCOMPtr<nsIURI> url;
    rv = ioService->NewURI(NS_ConvertUTF16toUTF8(mURL), nullptr, baseURL,
                           getter_AddRefs(url));
    if (NS_FAILED(rv)) {
      mRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return true;
    }

    mRetval = new URLProxy(new mozilla::dom::URL(url));
    return true;
  }

  URLProxy*
  GetURLProxy()
  {
    return mRetval;
  }
};

class TeardownURLRunnable : public nsRunnable
{
public:
  explicit TeardownURLRunnable(URLProxy* aURLProxy)
    : mURLProxy(aURLProxy)
  {
  }

  NS_IMETHOD Run()
  {
    AssertIsOnMainThread();

    mURLProxy->ReleaseURI();
    mURLProxy = nullptr;

    return NS_OK;
  }

private:
  nsRefPtr<URLProxy> mURLProxy;
};

// This class is the generic getter for any URL property.
class GetterRunnable : public WorkerMainThreadRunnable
{
public:
  enum GetterType {
    GetterHref,
    GetterOrigin,
    GetterProtocol,
    GetterUsername,
    GetterPassword,
    GetterHost,
    GetterHostname,
    GetterPort,
    GetterPathname,
    GetterSearch,
    GetterHash,
  };

  GetterRunnable(WorkerPrivate* aWorkerPrivate,
                 GetterType aType, nsString& aValue,
                 URLProxy* aURLProxy)
  : WorkerMainThreadRunnable(aWorkerPrivate)
  , mValue(aValue)
  , mType(aType)
  , mURLProxy(aURLProxy)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool
  MainThreadRun()
  {
    AssertIsOnMainThread();

    ErrorResult rv;
    switch (mType) {
      case GetterHref:
        mURLProxy->URL()->GetHref(mValue, rv);
        break;

      case GetterOrigin:
        mURLProxy->URL()->GetOrigin(mValue, rv);
        break;

      case GetterProtocol:
        mURLProxy->URL()->GetProtocol(mValue, rv);
        break;

      case GetterUsername:
        mURLProxy->URL()->GetUsername(mValue, rv);
        break;

      case GetterPassword:
        mURLProxy->URL()->GetPassword(mValue, rv);
        break;

      case GetterHost:
        mURLProxy->URL()->GetHost(mValue, rv);
        break;

      case GetterHostname:
        mURLProxy->URL()->GetHostname(mValue, rv);
        break;

      case GetterPort:
        mURLProxy->URL()->GetPort(mValue, rv);
        break;

      case GetterPathname:
        mURLProxy->URL()->GetPathname(mValue, rv);
        break;

      case GetterSearch:
        mURLProxy->URL()->GetSearch(mValue, rv);
        break;

      case GetterHash:
        mURLProxy->URL()->GetHash(mValue, rv);
        break;
    }

    MOZ_ASSERT(!rv.Failed());
    return true;
  }

private:
  nsString& mValue;
  GetterType mType;
  nsRefPtr<URLProxy> mURLProxy;
};

// This class is the generic setter for any URL property.
class SetterRunnable : public WorkerMainThreadRunnable
{
public:
  enum SetterType {
    SetterHref,
    SetterProtocol,
    SetterUsername,
    SetterPassword,
    SetterHost,
    SetterHostname,
    SetterPort,
    SetterPathname,
    SetterSearch,
    SetterHash,
  };

  SetterRunnable(WorkerPrivate* aWorkerPrivate,
                 SetterType aType, const nsAString& aValue,
                 URLProxy* aURLProxy, mozilla::ErrorResult& aRv)
  : WorkerMainThreadRunnable(aWorkerPrivate)
  , mValue(aValue)
  , mType(aType)
  , mURLProxy(aURLProxy)
  , mRv(aRv)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool
  MainThreadRun()
  {
    AssertIsOnMainThread();

    switch (mType) {
      case SetterHref:
        mURLProxy->URL()->SetHref(mValue, mRv);
        break;

      case SetterProtocol:
        mURLProxy->URL()->SetProtocol(mValue, mRv);
        break;

      case SetterUsername:
        mURLProxy->URL()->SetUsername(mValue, mRv);
        break;

      case SetterPassword:
        mURLProxy->URL()->SetPassword(mValue, mRv);
        break;

      case SetterHost:
        mURLProxy->URL()->SetHost(mValue, mRv);
        break;

      case SetterHostname:
        mURLProxy->URL()->SetHostname(mValue, mRv);
        break;

      case SetterPort:
        mURLProxy->URL()->SetPort(mValue, mRv);
        break;

      case SetterPathname:
        mURLProxy->URL()->SetPathname(mValue, mRv);
        break;

      case SetterSearch:
        mURLProxy->URL()->SetSearch(mValue, mRv);
        break;

      case SetterHash:
        mURLProxy->URL()->SetHash(mValue, mRv);
        break;
    }

    return true;
  }

private:
  const nsString mValue;
  SetterType mType;
  nsRefPtr<URLProxy> mURLProxy;
  mozilla::ErrorResult& mRv;
};

NS_IMPL_CYCLE_COLLECTION(URL, mSearchParams)

// The reason for using worker::URL is to have different refcnt logging than
// for main thread URL.
NS_IMPL_CYCLE_COLLECTING_ADDREF(workers::URL)
NS_IMPL_CYCLE_COLLECTING_RELEASE(workers::URL)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(URL)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// static
already_AddRefed<URL>
URL::Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
                 URL& aBase, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  nsRefPtr<ConstructorRunnable> runnable =
    new ConstructorRunnable(workerPrivate, aUrl, aBase.GetURLProxy(), aRv);

  if (!runnable->Dispatch(cx)) {
    JS_ReportPendingException(cx);
  }

  nsRefPtr<URLProxy> proxy = runnable->GetURLProxy();
  if (!proxy) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return nullptr;
  }

  nsRefPtr<URL> url = new URL(workerPrivate, proxy);
  return url.forget();
}

// static
already_AddRefed<URL>
URL::Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
                 const nsAString& aBase, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  nsRefPtr<ConstructorRunnable> runnable =
    new ConstructorRunnable(workerPrivate, aUrl, aBase, aRv);

  if (!runnable->Dispatch(cx)) {
    JS_ReportPendingException(cx);
  }

  nsRefPtr<URLProxy> proxy = runnable->GetURLProxy();
  if (!proxy) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return nullptr;
  }

  nsRefPtr<URL> url = new URL(workerPrivate, proxy);
  return url.forget();
}

URL::URL(WorkerPrivate* aWorkerPrivate, URLProxy* aURLProxy)
  : mWorkerPrivate(aWorkerPrivate)
  , mURLProxy(aURLProxy)
{
  MOZ_COUNT_CTOR(workers::URL);
}

URL::~URL()
{
  MOZ_COUNT_DTOR(workers::URL);

  if (mURLProxy) {
    nsRefPtr<TeardownURLRunnable> runnable =
      new TeardownURLRunnable(mURLProxy);
    mURLProxy = nullptr;

    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      NS_ERROR("Failed to dispatch teardown runnable!");
    }
  }
}

JSObject*
URL::WrapObject(JSContext* aCx)
{
  return URLBinding_workers::Wrap(aCx, this);
}

void
URL::GetHref(nsString& aHref, ErrorResult& aRv) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterHref, aHref,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::SetHref(const nsAString& aHref, ErrorResult& aRv)
{
  nsRefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterHref, aHref,
                       mURLProxy, aRv);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }

  UpdateURLSearchParams();
}

void
URL::GetOrigin(nsString& aOrigin, ErrorResult& aRv) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterOrigin, aOrigin,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::GetProtocol(nsString& aProtocol, ErrorResult& aRv) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterProtocol, aProtocol,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::SetProtocol(const nsAString& aProtocol, ErrorResult& aRv)
{
  ErrorResult rv;
  nsRefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterProtocol,
                       aProtocol, mURLProxy, rv);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::GetUsername(nsString& aUsername, ErrorResult& aRv) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterUsername, aUsername,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::SetUsername(const nsAString& aUsername, ErrorResult& aRv)
{
  ErrorResult rv;
  nsRefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterUsername,
                       aUsername, mURLProxy, rv);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::GetPassword(nsString& aPassword, ErrorResult& aRv) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterPassword, aPassword,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::SetPassword(const nsAString& aPassword, ErrorResult& aRv)
{
  ErrorResult rv;
  nsRefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterPassword,
                       aPassword, mURLProxy, rv);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::GetHost(nsString& aHost, ErrorResult& aRv) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterHost, aHost,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::SetHost(const nsAString& aHost, ErrorResult& aRv)
{
  ErrorResult rv;
  nsRefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterHost,
                       aHost, mURLProxy, rv);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::GetHostname(nsString& aHostname, ErrorResult& aRv) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterHostname, aHostname,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::SetHostname(const nsAString& aHostname, ErrorResult& aRv)
{
  ErrorResult rv;
  nsRefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterHostname,
                       aHostname, mURLProxy, rv);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::GetPort(nsString& aPort, ErrorResult& aRv) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterPort, aPort,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::SetPort(const nsAString& aPort, ErrorResult& aRv)
{
  ErrorResult rv;
  nsRefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterPort,
                       aPort, mURLProxy, rv);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::GetPathname(nsString& aPathname, ErrorResult& aRv) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterPathname, aPathname,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::SetPathname(const nsAString& aPathname, ErrorResult& aRv)
{
  ErrorResult rv;
  nsRefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterPathname,
                       aPathname, mURLProxy, rv);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::GetSearch(nsString& aSearch, ErrorResult& aRv) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterSearch, aSearch,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::SetSearch(const nsAString& aSearch, ErrorResult& aRv)
{
  SetSearchInternal(aSearch);
  UpdateURLSearchParams();
}

void
URL::SetSearchInternal(const nsAString& aSearch)
{
  ErrorResult rv;
  nsRefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterSearch,
                       aSearch, mURLProxy, rv);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

mozilla::dom::URLSearchParams*
URL::SearchParams()
{
  CreateSearchParamsIfNeeded();
  return mSearchParams;
}

void
URL::SetSearchParams(URLSearchParams& aSearchParams)
{
  if (mSearchParams) {
    mSearchParams->RemoveObserver(this);
  }

  mSearchParams = &aSearchParams;
  mSearchParams->AddObserver(this);

  nsString search;
  mSearchParams->Serialize(search);
  SetSearchInternal(search);
}

void
URL::GetHash(nsString& aHash, ErrorResult& aRv) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterHash, aHash,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::SetHash(const nsAString& aHash, ErrorResult& aRv)
{
  ErrorResult rv;
  nsRefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterHash,
                       aHash, mURLProxy, rv);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

// static
void
URL::CreateObjectURL(const GlobalObject& aGlobal, JSObject* aBlob,
                     const mozilla::dom::objectURLOptions& aOptions,
                     nsString& aResult, mozilla::ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  nsCOMPtr<nsIDOMBlob> blob = file::GetDOMBlobFromJSObject(aBlob);
  if (!blob) {
    SetDOMStringToNull(aResult);

    NS_NAMED_LITERAL_STRING(argStr, "Argument 1 of URL.createObjectURL");
    NS_NAMED_LITERAL_STRING(blobStr, "Blob");
    aRv.ThrowTypeError(MSG_DOES_NOT_IMPLEMENT_INTERFACE, &argStr, &blobStr);
    return;
  }

  DOMFile* domBlob = static_cast<DOMFile*>(blob.get());

  nsRefPtr<CreateURLRunnable> runnable =
    new CreateURLRunnable(workerPrivate, domBlob->Impl(), aOptions, aResult);

  if (!runnable->Dispatch(cx)) {
    JS_ReportPendingException(cx);
  }
}

// static
void
URL::CreateObjectURL(const GlobalObject& aGlobal, JSObject& aBlob,
                     const mozilla::dom::objectURLOptions& aOptions,
                     nsString& aResult, mozilla::ErrorResult& aRv)
{
  return CreateObjectURL(aGlobal, &aBlob, aOptions, aResult, aRv);
}

// static
void
URL::RevokeObjectURL(const GlobalObject& aGlobal, const nsAString& aUrl)
{
  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  nsRefPtr<RevokeURLRunnable> runnable =
    new RevokeURLRunnable(workerPrivate, aUrl);

  if (!runnable->Dispatch(cx)) {
    JS_ReportPendingException(cx);
  }
}

void
URL::URLSearchParamsUpdated(URLSearchParams* aSearchParams)
{
  MOZ_ASSERT(mSearchParams);
  MOZ_ASSERT(mSearchParams == aSearchParams);

  nsString search;
  mSearchParams->Serialize(search);
  SetSearchInternal(search);
}

void
URL::UpdateURLSearchParams()
{
  if (mSearchParams) {
    nsString search;
    ErrorResult rv;
    GetSearch(search, rv);
    mSearchParams->ParseInput(NS_ConvertUTF16toUTF8(Substring(search, 1)), this);
  }
}

void
URL::CreateSearchParamsIfNeeded()
{
  if (!mSearchParams) {
    mSearchParams = new URLSearchParams();
    mSearchParams->AddObserver(this);
    UpdateURLSearchParams();
  }
}

END_WORKERS_NAMESPACE
