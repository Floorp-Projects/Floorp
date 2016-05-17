/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URL.h"

#include "nsIIOService.h"
#include "nsPIDOMWindow.h"

#include "mozilla/dom/File.h"
#include "mozilla/dom/URL.h"
#include "mozilla/dom/URLBinding.h"
#include "mozilla/dom/URLSearchParams.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/ipc/nsIRemoteBlob.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "nsGlobalWindow.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"

BEGIN_WORKERS_NAMESPACE
using mozilla::dom::GlobalObject;

class URLProxy final
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(URLProxy)

  explicit URLProxy(already_AddRefed<mozilla::dom::URL> aURL)
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

  RefPtr<mozilla::dom::URL> mURL;
};

// This class creates an URL from a DOM Blob on the main thread.
class CreateURLRunnable : public WorkerMainThreadRunnable
{
private:
  BlobImpl* mBlobImpl;
  nsAString& mURL;

public:
  CreateURLRunnable(WorkerPrivate* aWorkerPrivate, BlobImpl* aBlobImpl,
                    const mozilla::dom::objectURLOptions& aOptions,
                    nsAString& aURL)
  : WorkerMainThreadRunnable(aWorkerPrivate,
                             NS_LITERAL_CSTRING("URL :: CreateURL"))
  , mBlobImpl(aBlobImpl)
  , mURL(aURL)
  {
    MOZ_ASSERT(aBlobImpl);

    DebugOnly<bool> isMutable;
    MOZ_ASSERT(NS_SUCCEEDED(aBlobImpl->GetMutable(&isMutable)));
    MOZ_ASSERT(!isMutable);
  }

  bool
  MainThreadRun()
  {
    using namespace mozilla::ipc;

    AssertIsOnMainThread();

    RefPtr<BlobImpl> newBlobImplHolder;

    if (nsCOMPtr<nsIRemoteBlob> remoteBlob = do_QueryInterface(mBlobImpl)) {
      if (BlobChild* blobChild = remoteBlob->GetBlobChild()) {
        if (PBackgroundChild* blobManager = blobChild->GetBackgroundManager()) {
          PBackgroundChild* backgroundManager =
            BackgroundChild::GetForCurrentThread();
          MOZ_ASSERT(backgroundManager);

          if (blobManager != backgroundManager) {
            // Always make sure we have a blob from an actor we can use on this
            // thread.
            blobChild = BlobChild::GetOrCreate(backgroundManager, mBlobImpl);
            MOZ_ASSERT(blobChild);

            newBlobImplHolder = blobChild->GetBlobImpl();
            MOZ_ASSERT(newBlobImplHolder);

            mBlobImpl = newBlobImplHolder;
          }
        }
      }
    }

    DebugOnly<bool> isMutable;
    MOZ_ASSERT(NS_SUCCEEDED(mBlobImpl->GetMutable(&isMutable)));
    MOZ_ASSERT(!isMutable);

    nsCOMPtr<nsIPrincipal> principal = mWorkerPrivate->GetPrincipal();

    nsAutoCString url;
    nsresult rv = nsHostObjectProtocolHandler::AddDataEntry(
        NS_LITERAL_CSTRING(BLOBURI_SCHEME),
        mBlobImpl, principal, mWorkerPrivate->IsInPrivateBrowsing(), url);

    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to add data entry for the blob!");
      SetDOMStringToNull(mURL);
      return false;
    }

    if (!mWorkerPrivate->IsSharedWorker() &&
        !mWorkerPrivate->IsServiceWorker()) {
      // Walk up to top worker object.
      WorkerPrivate* wp = mWorkerPrivate;
      while (WorkerPrivate* parent = wp->GetParent()) {
        wp = parent;
      }

      nsCOMPtr<nsIScriptContext> sc = wp->GetScriptContext();
      // We could not have a ScriptContext in JSM code. In this case, we leak.
      if (sc) {
        nsCOMPtr<nsIGlobalObject> global = sc->GetGlobalObject();
        MOZ_ASSERT(global);

        global->RegisterHostObjectURI(url);
      }
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
  : WorkerMainThreadRunnable(aWorkerPrivate,
                             NS_LITERAL_CSTRING("URL :: RevokeURL"))
  , mURL(aURL)
  {}

  bool
  MainThreadRun()
  {
    AssertIsOnMainThread();

    NS_ConvertUTF16toUTF8 url(mURL);

    nsIPrincipal* urlPrincipal =
      nsHostObjectProtocolHandler::GetDataEntryPrincipal(url);

    nsCOMPtr<nsIPrincipal> principal = mWorkerPrivate->GetPrincipal();

    bool subsumes;
    if (urlPrincipal &&
        NS_SUCCEEDED(principal->Subsumes(urlPrincipal, &subsumes)) &&
        subsumes) {
      nsHostObjectProtocolHandler::RemoveDataEntry(url);
    }

    if (!mWorkerPrivate->IsSharedWorker() &&
        !mWorkerPrivate->IsServiceWorker()) {
      // Walk up to top worker object.
      WorkerPrivate* wp = mWorkerPrivate;
      while (WorkerPrivate* parent = wp->GetParent()) {
        wp = parent;
      }

      nsCOMPtr<nsIScriptContext> sc = wp->GetScriptContext();
      // We could not have a ScriptContext in JSM code. In this case, we leak.
      if (sc) {
        nsCOMPtr<nsIGlobalObject> global = sc->GetGlobalObject();
        MOZ_ASSERT(global);

        global->UnregisterHostObjectURI(url);
      }
    }

    return true;
  }
};

// This class creates a URL object on the main thread.
class ConstructorRunnable : public WorkerMainThreadRunnable
{
private:
  const nsString mURL;

  nsString mBase; // IsVoid() if we have no base URI string.
  RefPtr<URLProxy> mBaseProxy;
  mozilla::ErrorResult& mRv;

  RefPtr<URLProxy> mRetval;

public:
  ConstructorRunnable(WorkerPrivate* aWorkerPrivate,
                      const nsAString& aURL, const Optional<nsAString>& aBase,
                      mozilla::ErrorResult& aRv)
  : WorkerMainThreadRunnable(aWorkerPrivate,
                             NS_LITERAL_CSTRING("URL :: Constructor"))
  , mURL(aURL)
  , mRv(aRv)
  {
    if (aBase.WasPassed()) {
      mBase = aBase.Value();
    } else {
      mBase.SetIsVoid(true);
    }
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  ConstructorRunnable(WorkerPrivate* aWorkerPrivate,
                      const nsAString& aURL, URLProxy* aBaseProxy,
                      mozilla::ErrorResult& aRv)
  : WorkerMainThreadRunnable(aWorkerPrivate,
                             NS_LITERAL_CSTRING("URL :: Constructor with BaseURL"))
  , mURL(aURL)
  , mBaseProxy(aBaseProxy)
  , mRv(aRv)
  {
    mBase.SetIsVoid(true);
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool
  MainThreadRun()
  {
    AssertIsOnMainThread();

    RefPtr<mozilla::dom::URL> url;
    if (mBaseProxy) {
      url = mozilla::dom::URL::Constructor(nullptr, mURL, mBaseProxy->URI(),
                                           mRv);
    } else if (!mBase.IsVoid()) {
      url = mozilla::dom::URL::Constructor(nullptr, mURL, mBase, mRv);
    } else {
      url = mozilla::dom::URL::Constructor(nullptr, mURL, nullptr, mRv);
    }

    if (mRv.Failed()) {
      return true;
    }

    mRetval = new URLProxy(url.forget());
    return true;
  }

  URLProxy*
  GetURLProxy()
  {
    return mRetval;
  }
};

class TeardownURLRunnable : public Runnable
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
  RefPtr<URLProxy> mURLProxy;
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
                 GetterType aType, nsAString& aValue,
                 URLProxy* aURLProxy)
  : WorkerMainThreadRunnable(aWorkerPrivate,
                             // We can have telemetry keys for each getter when
                             // needed.
                             NS_LITERAL_CSTRING("URL :: getter"))
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

    return true;
  }

private:
  nsAString& mValue;
  GetterType mType;
  RefPtr<URLProxy> mURLProxy;
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
                 URLProxy* aURLProxy)
  : WorkerMainThreadRunnable(aWorkerPrivate,
                             // We can have telemetry keys for each setter when
                             // needed.
                             NS_LITERAL_CSTRING("URL :: setter"))
  , mValue(aValue)
  , mType(aType)
  , mURLProxy(aURLProxy)
  , mFailed(false)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool
  MainThreadRun()
  {
    AssertIsOnMainThread();

    switch (mType) {
      case SetterHref: {
        ErrorResult rv;
        mURLProxy->URL()->SetHref(mValue, rv);
        if (NS_WARN_IF(rv.Failed())) {
          rv.SuppressException();
          mFailed = true;
        }

        break;
      }

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

    return true;
  }

  bool Failed() const
  {
    return mFailed;
  }

private:
  const nsString mValue;
  SetterType mType;
  RefPtr<URLProxy> mURLProxy;
  bool mFailed;
};

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(URL, mSearchParams)

// The reason for using worker::URL is to have different refcnt logging than
// for main thread URL.
NS_IMPL_CYCLE_COLLECTING_ADDREF(workers::URL)
NS_IMPL_CYCLE_COLLECTING_RELEASE(workers::URL)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(URL)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// static
already_AddRefed<URL>
URL::Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
                 URL& aBase, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  RefPtr<ConstructorRunnable> runnable =
    new ConstructorRunnable(workerPrivate, aUrl, aBase.GetURLProxy(), aRv);

  return FinishConstructor(cx, workerPrivate, runnable, aRv);
}

// static
already_AddRefed<URL>
URL::Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
                 const Optional<nsAString>& aBase, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  RefPtr<ConstructorRunnable> runnable =
    new ConstructorRunnable(workerPrivate, aUrl, aBase, aRv);

  return FinishConstructor(cx, workerPrivate, runnable, aRv);
}

// static
already_AddRefed<URL>
URL::Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
                 const nsAString& aBase, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  Optional<nsAString> base;
  base = &aBase;
  RefPtr<ConstructorRunnable> runnable =
    new ConstructorRunnable(workerPrivate, aUrl, base, aRv);

  return FinishConstructor(cx, workerPrivate, runnable, aRv);
}

// static
already_AddRefed<URL>
URL::FinishConstructor(JSContext* aCx, WorkerPrivate* aPrivate,
                       ConstructorRunnable* aRunnable, ErrorResult& aRv)
{
  aRunnable->Dispatch(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<URLProxy> proxy = aRunnable->GetURLProxy();
  if (!proxy) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return nullptr;
  }

  RefPtr<URL> url = new URL(aPrivate, proxy);
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
    RefPtr<TeardownURLRunnable> runnable =
      new TeardownURLRunnable(mURLProxy);
    mURLProxy = nullptr;

    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      NS_ERROR("Failed to dispatch teardown runnable!");
    }
  }
}

JSObject*
URL::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return URLBinding_workers::Wrap(aCx, this, aGivenProto);
}

void
URL::GetHref(nsAString& aHref, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterHref, aHref,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URL::SetHref(const nsAString& aHref, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterHref, aHref,
                       mURLProxy);

  runnable->Dispatch(aRv);
  if (aRv.Failed()) {
    return;
  }

  if (runnable->Failed()) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(aHref);
    return;
  }

  UpdateURLSearchParams();
}

void
URL::GetOrigin(nsAString& aOrigin, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterOrigin, aOrigin,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URL::GetProtocol(nsAString& aProtocol, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterProtocol, aProtocol,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URL::SetProtocol(const nsAString& aProtocol, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterProtocol,
                       aProtocol, mURLProxy);

  runnable->Dispatch(aRv);

  MOZ_ASSERT(!runnable->Failed());
}

void
URL::GetUsername(nsAString& aUsername, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterUsername, aUsername,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URL::SetUsername(const nsAString& aUsername, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterUsername,
                       aUsername, mURLProxy);

  runnable->Dispatch(aRv);

  MOZ_ASSERT(!runnable->Failed());
}

void
URL::GetPassword(nsAString& aPassword, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterPassword, aPassword,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URL::SetPassword(const nsAString& aPassword, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterPassword,
                       aPassword, mURLProxy);

  runnable->Dispatch(aRv);

  MOZ_ASSERT(!runnable->Failed());
}

void
URL::GetHost(nsAString& aHost, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterHost, aHost,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URL::SetHost(const nsAString& aHost, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterHost,
                       aHost, mURLProxy);

  runnable->Dispatch(aRv);

  MOZ_ASSERT(!runnable->Failed());
}

void
URL::GetHostname(nsAString& aHostname, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterHostname, aHostname,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URL::SetHostname(const nsAString& aHostname, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterHostname,
                       aHostname, mURLProxy);

  runnable->Dispatch(aRv);

  MOZ_ASSERT(!runnable->Failed());
}

void
URL::GetPort(nsAString& aPort, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterPort, aPort,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URL::SetPort(const nsAString& aPort, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterPort,
                       aPort, mURLProxy);

  runnable->Dispatch(aRv);

  MOZ_ASSERT(!runnable->Failed());
}

void
URL::GetPathname(nsAString& aPathname, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterPathname, aPathname,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URL::SetPathname(const nsAString& aPathname, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterPathname,
                       aPathname, mURLProxy);

  runnable->Dispatch(aRv);

  MOZ_ASSERT(!runnable->Failed());
}

void
URL::GetSearch(nsAString& aSearch, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterSearch, aSearch,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URL::SetSearch(const nsAString& aSearch, ErrorResult& aRv)
{
  SetSearchInternal(aSearch, aRv);
  UpdateURLSearchParams();
}

void
URL::SetSearchInternal(const nsAString& aSearch, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterSearch,
                       aSearch, mURLProxy);

  runnable->Dispatch(aRv);

  MOZ_ASSERT(!runnable->Failed());
}

mozilla::dom::URLSearchParams*
URL::SearchParams()
{
  CreateSearchParamsIfNeeded();
  return mSearchParams;
}

void
URL::GetHash(nsAString& aHash, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterHash, aHash,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URL::SetHash(const nsAString& aHash, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterHash,
                       aHash, mURLProxy);

  runnable->Dispatch(aRv);

  MOZ_ASSERT(!runnable->Failed());
}

// static
void
URL::CreateObjectURL(const GlobalObject& aGlobal, Blob& aBlob,
                     const mozilla::dom::objectURLOptions& aOptions,
                     nsAString& aResult, mozilla::ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  RefPtr<BlobImpl> blobImpl = aBlob.Impl();
  MOZ_ASSERT(blobImpl);

  aRv = blobImpl->SetMutable(false);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  RefPtr<CreateURLRunnable> runnable =
    new CreateURLRunnable(workerPrivate, blobImpl, aOptions, aResult);

  runnable->Dispatch(aRv);
  if (aRv.Failed()) {
    return;
  }

  if (workerPrivate->IsSharedWorker() || workerPrivate->IsServiceWorker()) {
    WorkerGlobalScope* scope = workerPrivate->GlobalScope();
    MOZ_ASSERT(scope);

    scope->RegisterHostObjectURI(NS_ConvertUTF16toUTF8(aResult));
  }
}

// static
void
URL::RevokeObjectURL(const GlobalObject& aGlobal, const nsAString& aUrl,
                     ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  RefPtr<RevokeURLRunnable> runnable =
    new RevokeURLRunnable(workerPrivate, aUrl);

  runnable->Dispatch(aRv);
  if (aRv.Failed()) {
    return;
  }

  if (workerPrivate->IsSharedWorker() || workerPrivate->IsServiceWorker()) {
    WorkerGlobalScope* scope = workerPrivate->GlobalScope();
    MOZ_ASSERT(scope);

    scope->UnregisterHostObjectURI(NS_ConvertUTF16toUTF8(aUrl));
  }
}

void
URL::URLSearchParamsUpdated(URLSearchParams* aSearchParams)
{
  MOZ_ASSERT(mSearchParams);
  MOZ_ASSERT(mSearchParams == aSearchParams);

  nsAutoString search;
  mSearchParams->Serialize(search);
  ErrorResult rv;
  SetSearchInternal(search, rv);
  // XXXbz and now what?  We're supposed to stop everything if rv failed!
  rv.SuppressException();
}

void
URL::UpdateURLSearchParams()
{
  if (mSearchParams) {
    nsAutoString search;
    ErrorResult rv;
    GetSearch(search, rv);
    // XXXbz and now what?  We're supposed to stop everything if rv failed!
    rv.SuppressException();
    mSearchParams->ParseInput(NS_ConvertUTF16toUTF8(Substring(search, 1)));
  }
}

void
URL::CreateSearchParamsIfNeeded()
{
  if (!mSearchParams) {
    mSearchParams = new URLSearchParams(nullptr, this);
    UpdateURLSearchParams();
  }
}

END_WORKERS_NAMESPACE
