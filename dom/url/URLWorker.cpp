/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URLWorker.h"

#include "mozilla/dom/Blob.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/Unused.h"
#include "nsProxyRelease.h"
#include "nsStandardURL.h"
#include "nsURLHelper.h"

namespace mozilla {

using net::nsStandardURL;

namespace dom {

// This class creates an URL from a DOM Blob on the main thread.
class CreateURLRunnable : public WorkerMainThreadRunnable
{
private:
  BlobImpl* mBlobImpl;
  nsAString& mURL;

public:
  CreateURLRunnable(WorkerPrivate* aWorkerPrivate, BlobImpl* aBlobImpl,
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
  MainThreadRun() override
  {
    using namespace mozilla::ipc;

    AssertIsOnMainThread();

    DebugOnly<bool> isMutable;
    MOZ_ASSERT(NS_SUCCEEDED(mBlobImpl->GetMutable(&isMutable)));
    MOZ_ASSERT(!isMutable);

    nsCOMPtr<nsIPrincipal> principal = mWorkerPrivate->GetPrincipal();

    nsAutoCString url;
    nsresult rv =
      BlobURLProtocolHandler::AddDataEntry(mBlobImpl, principal, url);

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
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    NS_ConvertUTF16toUTF8 url(mURL);

    nsIPrincipal* urlPrincipal =
      BlobURLProtocolHandler::GetDataEntryPrincipal(url);

    nsCOMPtr<nsIPrincipal> principal = mWorkerPrivate->GetPrincipal();

    bool subsumes;
    if (urlPrincipal &&
        NS_SUCCEEDED(principal->Subsumes(urlPrincipal, &subsumes)) &&
        subsumes) {
      BlobURLProtocolHandler::RemoveDataEntry(url);
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

// This class checks if an URL is valid on the main thread.
class IsValidURLRunnable : public WorkerMainThreadRunnable
{
private:
  const nsString mURL;
  bool mValid;

public:
  IsValidURLRunnable(WorkerPrivate* aWorkerPrivate,
                     const nsAString& aURL)
  : WorkerMainThreadRunnable(aWorkerPrivate,
                             NS_LITERAL_CSTRING("URL :: IsValidURL"))
  , mURL(aURL)
  , mValid(false)
  {}

  bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    NS_ConvertUTF16toUTF8 url(mURL);
    mValid = BlobURLProtocolHandler::HasDataEntry(url);

    return true;
  }

  bool
  IsValidURL() const
  {
    return mValid;
  }
};

// This class creates a URL object on the main thread.
class ConstructorRunnable : public WorkerMainThreadRunnable
{
private:
  const nsString mURL;

  nsString mBase; // IsVoid() if we have no base URI string.

  nsCOMPtr<nsIURI> mRetval;

public:
  ConstructorRunnable(WorkerPrivate* aWorkerPrivate,
                      const nsAString& aURL, const Optional<nsAString>& aBase)
  : WorkerMainThreadRunnable(aWorkerPrivate,
                             NS_LITERAL_CSTRING("URL :: Constructor"))
  , mURL(aURL)
  {
    if (aBase.WasPassed()) {
      mBase = aBase.Value();
    } else {
      mBase.SetIsVoid(true);
    }
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    nsCOMPtr<nsIURI> baseUri;
    if (!mBase.IsVoid()) {
      nsresult rv = NS_NewURI(getter_AddRefs(baseUri), mBase, nullptr, nullptr,
                              nsContentUtils::GetIOService());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return true;
      }
    }

    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), mURL, nullptr, baseUri,
                            nsContentUtils::GetIOService());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return true;
    }

    mRetval = std::move(uri);
    return true;
  }

  nsIURI*
  GetURI(ErrorResult& aRv) const
  {
    MOZ_ASSERT(mWorkerPrivate);
    mWorkerPrivate->AssertIsOnWorkerThread();

    if (!mRetval) {
      aRv.ThrowTypeError<MSG_INVALID_URL>(mURL);
    }

    return mRetval;
  }
};

class OriginGetterRunnable : public WorkerMainThreadRunnable
{
public:
  OriginGetterRunnable(WorkerPrivate* aWorkerPrivate,
                       nsAString& aValue,
                       nsIURI* aURI)
  : WorkerMainThreadRunnable(aWorkerPrivate,
                             // We can have telemetry keys for each getter when
                             // needed.
                             NS_LITERAL_CSTRING("URL :: getter"))
  , mValue(aValue)
  , mURI(aURI)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();
    ErrorResult rv;
    nsContentUtils::GetUTFOrigin(mURI, mValue);
    return true;
  }

  void
  Dispatch(ErrorResult& aRv)
  {
    WorkerMainThreadRunnable::Dispatch(Canceling, aRv);
  }

private:
  nsAString& mValue;
  nsCOMPtr<nsIURI> mURI;
};

class ProtocolSetterRunnable : public WorkerMainThreadRunnable
{
public:
  ProtocolSetterRunnable(WorkerPrivate* aWorkerPrivate,
                         const nsACString& aValue,
                         nsIURI* aURI)
  : WorkerMainThreadRunnable(aWorkerPrivate,
                             NS_LITERAL_CSTRING("ProtocolSetterRunnable"))
  , mValue(aValue)
  , mURI(aURI)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();

    nsCOMPtr<nsIURI> clone;
    nsresult rv = NS_MutateURI(mURI)
                    .SetScheme(mValue)
                    .Finalize(clone);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return true;
    }

    nsAutoCString href;
    rv = clone->GetSpec(href);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return true;
    }

    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), href);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return true;
    }

    mRetval = std::move(uri);
    return true;
  }

  void
  Dispatch(ErrorResult& aRv)
  {
    WorkerMainThreadRunnable::Dispatch(Canceling, aRv);
  }

  nsIURI*
  GetRetval() const
  {
    return mRetval;
  }

private:
  const nsCString mValue;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIURI> mRetval;
};

/* static */ already_AddRefed<URLWorker>
URLWorker::Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
                       const Optional<nsAString>& aBase, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  RefPtr<URLWorker> url = new URLWorker(workerPrivate);
  url->Init(aURL, aBase, aRv);

  return aRv.Failed() ? nullptr : url.forget();
}

/* static */ already_AddRefed<URLWorker>
URLWorker::Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
                       const nsAString& aBase, ErrorResult& aRv)
{
  Optional<nsAString> base;
  base = &aBase;

  return Constructor(aGlobal, aURL, base, aRv);
}

/* static */ void
URLWorker::CreateObjectURL(const GlobalObject& aGlobal, Blob& aBlob,
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
    new CreateURLRunnable(workerPrivate, blobImpl, aResult);

  runnable->Dispatch(Canceling, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (workerPrivate->IsSharedWorker() || workerPrivate->IsServiceWorker()) {
    WorkerGlobalScope* scope = workerPrivate->GlobalScope();
    MOZ_ASSERT(scope);

    scope->RegisterHostObjectURI(NS_ConvertUTF16toUTF8(aResult));
  }
}

/* static */ void
URLWorker::RevokeObjectURL(const GlobalObject& aGlobal, const nsAString& aUrl,
                           ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  RefPtr<RevokeURLRunnable> runnable =
    new RevokeURLRunnable(workerPrivate, aUrl);

  runnable->Dispatch(Canceling, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (workerPrivate->IsSharedWorker() || workerPrivate->IsServiceWorker()) {
    WorkerGlobalScope* scope = workerPrivate->GlobalScope();
    MOZ_ASSERT(scope);

    scope->UnregisterHostObjectURI(NS_ConvertUTF16toUTF8(aUrl));
  }
}

/* static */ bool
URLWorker::IsValidURL(const GlobalObject& aGlobal, const nsAString& aUrl,
                      ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  RefPtr<IsValidURLRunnable> runnable =
    new IsValidURLRunnable(workerPrivate, aUrl);

  runnable->Dispatch(Canceling, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return false;
  }

  return runnable->IsValidURL();
}

URLWorker::URLWorker(WorkerPrivate* aWorkerPrivate)
  : URL(nullptr)
  , mWorkerPrivate(aWorkerPrivate)
{}

void
URLWorker::Init(const nsAString& aURL, const Optional<nsAString>& aBase,
                ErrorResult& aRv)
{
  nsAutoCString scheme;
  nsresult rv = net_ExtractURLScheme(NS_ConvertUTF16toUTF8(aURL), scheme);
  if (NS_FAILED(rv)) {
    // this may be a relative URL, check baseURL
    if (!aBase.WasPassed()) {
      aRv.ThrowTypeError<MSG_INVALID_URL>(aURL);
      return;
    }
    rv = net_ExtractURLScheme(NS_ConvertUTF16toUTF8(aBase.Value()), scheme);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.ThrowTypeError<MSG_INVALID_URL>(aURL);
      return;
    }
  }

  // create url proxy
  RefPtr<ConstructorRunnable> runnable =
    new ConstructorRunnable(mWorkerPrivate, aURL, aBase);
  runnable->Dispatch(Canceling, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsCOMPtr<nsIURI> uri = runnable->GetURI(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  SetURI(uri.forget());
}

URLWorker::~URLWorker() = default;

void
URLWorker::SetHref(const nsAString& aHref, ErrorResult& aRv)
{
  nsAutoCString scheme;
  nsresult rv = net_ExtractURLScheme(NS_ConvertUTF16toUTF8(aHref), scheme);
  if (NS_FAILED(rv)) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(aHref);
    return;
  }

  RefPtr<ConstructorRunnable> runnable =
    new ConstructorRunnable(mWorkerPrivate, aHref, Optional<nsAString>());
  runnable->Dispatch(Canceling, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsCOMPtr<nsIURI> uri = runnable->GetURI(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  SetURI(uri.forget());
  UpdateURLSearchParams();
}

void
URLWorker::GetOrigin(nsAString& aOrigin, ErrorResult& aRv) const
{
  RefPtr<OriginGetterRunnable> runnable =
    new OriginGetterRunnable(mWorkerPrivate, aOrigin, GetURI());

  runnable->Dispatch(aRv);
}

void
URLWorker::SetProtocol(const nsAString& aProtocol, ErrorResult& aRv)
{
  nsAString::const_iterator start, end;
  aProtocol.BeginReading(start);
  aProtocol.EndReading(end);
  nsAString::const_iterator iter(start);

  FindCharInReadable(':', iter, end);
  NS_ConvertUTF16toUTF8 scheme(Substring(start, iter));

  RefPtr<ProtocolSetterRunnable> runnable =
    new ProtocolSetterRunnable(mWorkerPrivate, scheme, GetURI());
  runnable->Dispatch(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsCOMPtr<nsIURI> uri = runnable->GetRetval();
  if (NS_WARN_IF(!uri)) {
    return;
  }

  SetURI(uri.forget());
}

} // namespace dom
} // namespace mozilla
