/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URL.h"
#include "File.h"

#include "nsTraceRefcnt.h"

#include "WorkerPrivate.h"
#include "nsThreadUtils.h"

#include "nsPIDOMWindow.h"
#include "nsGlobalWindow.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsServiceManagerUtils.h"

#include "nsIDocument.h"
#include "nsIDOMFile.h"

#include "DOMBindingInlines.h"
#include "mozilla/dom/URL.h"
#include "nsIIOService.h"
#include "nsNetCID.h"

BEGIN_WORKERS_NAMESPACE
using mozilla::dom::GlobalObject;

class URLProxy MOZ_FINAL
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(URLProxy)

  URLProxy(mozilla::dom::URL* aURL)
    : mURL(aURL)
  {
    AssertIsOnMainThread();
  }

  ~URLProxy()
  {
     MOZ_ASSERT(!mURL);
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
  nsRefPtr<mozilla::dom::URL> mURL;
};

// Base class for the URL runnable objects.
class URLRunnable : public nsRunnable
{
protected:
  WorkerPrivate* mWorkerPrivate;
  uint32_t mSyncQueueKey;

private:
  class ResponseRunnable : public WorkerSyncRunnable
  {
    uint32_t mSyncQueueKey;

  public:
    ResponseRunnable(WorkerPrivate* aWorkerPrivate,
                     uint32_t aSyncQueueKey)
    : WorkerSyncRunnable(aWorkerPrivate, aSyncQueueKey, false),
      mSyncQueueKey(aSyncQueueKey)
    {
      NS_ASSERTION(aWorkerPrivate, "Don't hand me a null WorkerPrivate!");
    }

    bool
    WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
    {
      aWorkerPrivate->StopSyncLoop(mSyncQueueKey, true);
      return true;
    }

    bool
    PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
    {
      AssertIsOnMainThread();
      return true;
    }

    void
    PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                 bool aDispatchResult)
    {
      AssertIsOnMainThread();
    }
  };

protected:
  URLRunnable(WorkerPrivate* aWorkerPrivate)
  : mWorkerPrivate(aWorkerPrivate)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

public:
  bool
  Dispatch(JSContext* aCx)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
    AutoSyncLoopHolder syncLoop(mWorkerPrivate);
    mSyncQueueKey = syncLoop.SyncQueueKey();

    if (NS_FAILED(NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL))) {
      JS_ReportError(aCx, "Failed to dispatch to main thread!");
      return false;
    }

    return syncLoop.RunAndForget(aCx);
  }

private:
  NS_IMETHOD Run()
  {
    AssertIsOnMainThread();

    MainThreadRun();

    nsRefPtr<ResponseRunnable> response =
      new ResponseRunnable(mWorkerPrivate, mSyncQueueKey);
    if (!response->Dispatch(nullptr)) {
      NS_WARNING("Failed to dispatch response!");
    }

    return NS_OK;
  }

protected:
  virtual void
  MainThreadRun() = 0;
};

// This class creates an URL from a DOM Blob on the main thread.
class CreateURLRunnable : public URLRunnable
{
private:
  nsIDOMBlob* mBlob;
  nsString& mURL;

public:
  CreateURLRunnable(WorkerPrivate* aWorkerPrivate, nsIDOMBlob* aBlob,
                    const mozilla::dom::objectURLOptions& aOptions,
                    nsString& aURL)
  : URLRunnable(aWorkerPrivate),
    mBlob(aBlob),
    mURL(aURL)
  {
    MOZ_ASSERT(aBlob);
  }

  void
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
        return;
      }

      principal = doc->NodePrincipal();
    } else {
      MOZ_ASSERT_IF(!mWorkerPrivate->GetParent(), mWorkerPrivate->IsChromeWorker());
      principal = mWorkerPrivate->GetPrincipal();
    }

    nsCString url;
    nsresult rv = nsHostObjectProtocolHandler::AddDataEntry(
        NS_LITERAL_CSTRING(BLOBURI_SCHEME),
        mBlob, principal, url);

    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to add data entry for the blob!");
      SetDOMStringToNull(mURL);
      return;
    }

    if (doc) {
      doc->RegisterHostObjectUri(url);
    } else {
      mWorkerPrivate->RegisterHostObjectURI(url);
    }

    mURL = NS_ConvertUTF8toUTF16(url);
  }
};

// This class revokes an URL on the main thread.
class RevokeURLRunnable : public URLRunnable
{
private:
  const nsString mURL;

public:
  RevokeURLRunnable(WorkerPrivate* aWorkerPrivate,
                    const nsAString& aURL)
  : URLRunnable(aWorkerPrivate),
    mURL(aURL)
  {}

  void
  MainThreadRun()
  {
    AssertIsOnMainThread();

    nsCOMPtr<nsIPrincipal> principal;
    nsIDocument* doc = nullptr;

    nsCOMPtr<nsPIDOMWindow> window = mWorkerPrivate->GetWindow();
    if (window) {
      doc = window->GetExtantDoc();
      if (!doc) {
        return;
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
  }
};

// This class creates a URL object on the main thread.
class ConstructorRunnable : public URLRunnable
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
  : URLRunnable(aWorkerPrivate)
  , mURL(aURL)
  , mBase(aBase)
  , mRv(aRv)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  ConstructorRunnable(WorkerPrivate* aWorkerPrivate,
                      const nsAString& aURL, URLProxy* aBaseProxy,
                      mozilla::ErrorResult& aRv)
  : URLRunnable(aWorkerPrivate)
  , mURL(aURL)
  , mBaseProxy(aBaseProxy)
  , mRv(aRv)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  void
  MainThreadRun()
  {
    AssertIsOnMainThread();

    nsresult rv;
    nsCOMPtr<nsIIOService> ioService(do_GetService(NS_IOSERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv)) {
      mRv.Throw(rv);
      return;
    }

    nsCOMPtr<nsIURI> baseURL;

    if (!mBaseProxy) {
      rv = ioService->NewURI(NS_ConvertUTF16toUTF8(mBase), nullptr, nullptr,
                             getter_AddRefs(baseURL));
      if (NS_FAILED(rv)) {
        mRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
        return;
      }
    } else {
      baseURL = mBaseProxy->URI();
    }

    nsCOMPtr<nsIURI> url;
    rv = ioService->NewURI(NS_ConvertUTF16toUTF8(mURL), nullptr, baseURL,
                           getter_AddRefs(url));
    if (NS_FAILED(rv)) {
      mRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return;
    }

    mRetval = new URLProxy(new mozilla::dom::URL(nullptr, url));
  }

  URLProxy*
  GetURLProxy()
  {
    return mRetval;
  }
};

class TeardownRunnable : public nsRunnable
{
public:
  TeardownRunnable(URLProxy* aURLProxy)
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
class GetterRunnable : public URLRunnable
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
  : URLRunnable(aWorkerPrivate)
  , mValue(aValue)
  , mType(aType)
  , mURLProxy(aURLProxy)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  void
  MainThreadRun()
  {
    AssertIsOnMainThread();

    switch (mType) {
      case GetterHref:
        mURLProxy->URL()->GetHref(mValue);
        break;

      case GetterOrigin:
        mURLProxy->URL()->GetOrigin(mValue);
        break;

      case GetterProtocol:
        mURLProxy->URL()->GetProtocol(mValue);
        break;

      case GetterUsername:
        mURLProxy->URL()->GetUsername(mValue);
        break;

      case GetterPassword:
        mURLProxy->URL()->GetPassword(mValue);
        break;

      case GetterHost:
        mURLProxy->URL()->GetHost(mValue);
        break;

      case GetterHostname:
        mURLProxy->URL()->GetHostname(mValue);
        break;

      case GetterPort:
        mURLProxy->URL()->GetPort(mValue);
        break;

      case GetterPathname:
        mURLProxy->URL()->GetPathname(mValue);
        break;

      case GetterSearch:
        mURLProxy->URL()->GetSearch(mValue);
        break;

      case GetterHash:
        mURLProxy->URL()->GetHash(mValue);
        break;
    }
  }

private:
  nsString& mValue;
  GetterType mType;
  nsRefPtr<URLProxy> mURLProxy;
};

// This class is the generic setter for any URL property.
class SetterRunnable : public URLRunnable
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
  : URLRunnable(aWorkerPrivate)
  , mValue(aValue)
  , mType(aType)
  , mURLProxy(aURLProxy)
  , mRv(aRv)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  void
  MainThreadRun()
  {
    AssertIsOnMainThread();

    switch (mType) {
      case SetterHref:
        mURLProxy->URL()->SetHref(mValue, mRv);
        break;

      case SetterProtocol:
        mURLProxy->URL()->SetProtocol(mValue);
        break;

      case SetterUsername:
        mURLProxy->URL()->SetUsername(mValue);
        break;

      case SetterPassword:
        mURLProxy->URL()->SetPassword(mValue);
        break;

      case SetterHost:
        mURLProxy->URL()->SetHost(mValue);
        break;

      case SetterHostname:
        mURLProxy->URL()->SetHostname(mValue);
        break;

      case SetterPort:
        mURLProxy->URL()->SetPort(mValue);
        break;

      case SetterPathname:
        mURLProxy->URL()->SetPathname(mValue);
        break;

      case SetterSearch:
        mURLProxy->URL()->SetSearch(mValue);
        break;

      case SetterHash:
        mURLProxy->URL()->SetHash(mValue);
        break;
    }
  }

private:
  const nsString mValue;
  SetterType mType;
  nsRefPtr<URLProxy> mURLProxy;
  mozilla::ErrorResult& mRv;
};

// static
URL*
URL::Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
                 URL& aBase, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.GetContext();
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

  if (!Wrap(aGlobal.GetContext(), aGlobal.Get(), url)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return url;
}

// static
URL*
URL::Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
                 const nsAString& aBase, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.GetContext();
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

  if (!Wrap(aGlobal.GetContext(), aGlobal.Get(), url)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return url;
}

URL::URL(WorkerPrivate* aWorkerPrivate, URLProxy* aURLProxy)
  : DOMBindingBase(aWorkerPrivate->GetJSContext())
  , mWorkerPrivate(aWorkerPrivate)
  , mURLProxy(aURLProxy)
{
}

URL::~URL()
{
  if (mURLProxy) {
    nsRefPtr<TeardownRunnable> runnable = new TeardownRunnable(mURLProxy);
    mURLProxy = nullptr;

    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      NS_ERROR("Failed to dispatch teardown runnable!");
    }
  }
}

void
URL::_trace(JSTracer* aTrc)
{
  DOMBindingBase::_trace(aTrc);
}

void
URL::_finalize(JSFreeOp* aFop)
{
  DOMBindingBase::_finalize(aFop);
}

void
URL::GetHref(nsString& aHref) const
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
}

void
URL::GetOrigin(nsString& aOrigin) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterOrigin, aOrigin,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::GetProtocol(nsString& aProtocol) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterProtocol, aProtocol,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::SetProtocol(const nsAString& aProtocol)
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
URL::GetUsername(nsString& aUsername) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterUsername, aUsername,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::SetUsername(const nsAString& aUsername)
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
URL::GetPassword(nsString& aPassword) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterPassword, aPassword,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::SetPassword(const nsAString& aPassword)
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
URL::GetHost(nsString& aHost) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterHost, aHost,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::SetHost(const nsAString& aHost)
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
URL::GetHostname(nsString& aHostname) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterHostname, aHostname,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::SetHostname(const nsAString& aHostname)
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
URL::GetPort(nsString& aPort) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterPort, aPort,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::SetPort(const nsAString& aPort)
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
URL::GetPathname(nsString& aPathname) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterPathname, aPathname,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::SetPathname(const nsAString& aPathname)
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
URL::GetSearch(nsString& aSearch) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterSearch, aSearch,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::SetSearch(const nsAString& aSearch)
{
  ErrorResult rv;
  nsRefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterSearch,
                       aSearch, mURLProxy, rv);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::GetHash(nsString& aHash) const
{
  nsRefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterHash, aHash,
                       mURLProxy);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    JS_ReportPendingException(mWorkerPrivate->GetJSContext());
  }
}

void
URL::SetHash(const nsAString& aHash)
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
  JSContext* cx = aGlobal.GetContext();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  nsCOMPtr<nsIDOMBlob> blob = file::GetDOMBlobFromJSObject(aBlob);
  if (!blob) {
    SetDOMStringToNull(aResult);

    NS_NAMED_LITERAL_STRING(argStr, "Argument 1 of URL.createObjectURL");
    NS_NAMED_LITERAL_STRING(blobStr, "Blob");
    aRv.ThrowTypeError(MSG_DOES_NOT_IMPLEMENT_INTERFACE, &argStr, &blobStr);
    return;
  }

  nsRefPtr<CreateURLRunnable> runnable =
    new CreateURLRunnable(workerPrivate, blob, aOptions, aResult);

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
  JSContext* cx = aGlobal.GetContext();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  nsRefPtr<RevokeURLRunnable> runnable =
    new RevokeURLRunnable(workerPrivate, aUrl);

  if (!runnable->Dispatch(cx)) {
    JS_ReportPendingException(cx);
  }
}

END_WORKERS_NAMESPACE
