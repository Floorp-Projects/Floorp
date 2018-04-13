/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URLWorker.h"

#include "mozilla/dom/Blob.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/Unused.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsProxyRelease.h"
#include "nsStandardURL.h"
#include "nsURLHelper.h"

namespace mozilla {

using net::nsStandardURL;

namespace dom {

// Proxy class to forward all the requests to a URLMainThread object.
class URLWorker::URLProxy final
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(URLProxy)

  explicit URLProxy(already_AddRefed<URLMainThread> aURL)
    : mURL(aURL)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  URLMainThread* URL()
  {
    return mURL;
  }

  nsIURI* URI()
  {
    MOZ_ASSERT(NS_IsMainThread());
    return mURL->GetURI();
  }

  void ReleaseURI()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mURL = nullptr;
  }

private:
  // Private destructor, to discourage deletion outside of Release():
  ~URLProxy()
  {
     MOZ_ASSERT(!mURL);
  }

  RefPtr<URLMainThread> mURL;
};

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
      nsHostObjectProtocolHandler::AddDataEntry(mBlobImpl, principal, url);

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
    mValid = nsHostObjectProtocolHandler::HasDataEntry(url);

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

  RefPtr<URLWorker::URLProxy> mRetval;

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

    ErrorResult rv;
    RefPtr<URLMainThread> url;
    if (!mBase.IsVoid()) {
      url = URLMainThread::Constructor(nullptr, mURL, mBase, rv);
    } else {
      url = URLMainThread::Constructor(nullptr, mURL, nullptr, rv);
    }

    if (rv.Failed()) {
      rv.SuppressException();
      return true;
    }

    mRetval = new URLWorker::URLProxy(url.forget());
    return true;
  }

  URLWorker::URLProxy*
  GetURLProxy(ErrorResult& aRv) const
  {
    MOZ_ASSERT(mWorkerPrivate);
    mWorkerPrivate->AssertIsOnWorkerThread();

    if (!mRetval) {
      aRv.ThrowTypeError<MSG_INVALID_URL>(mURL);
    }

    return mRetval;
  }
};

class TeardownURLRunnable : public Runnable
{
public:
  explicit TeardownURLRunnable(URLWorker::URLProxy* aURLProxy)
    : Runnable("dom::TeardownURLRunnable")
    , mURLProxy(aURLProxy)
  {
  }

  NS_IMETHOD Run() override
  {
    AssertIsOnMainThread();

    mURLProxy->ReleaseURI();
    mURLProxy = nullptr;

    return NS_OK;
  }

private:
  RefPtr<URLWorker::URLProxy> mURLProxy;
};

class OriginGetterRunnable : public WorkerMainThreadRunnable
{
public:
  OriginGetterRunnable(WorkerPrivate* aWorkerPrivate,
                       nsAString& aValue,
                       URLWorker::URLProxy* aURLProxy)
  : WorkerMainThreadRunnable(aWorkerPrivate,
                             // We can have telemetry keys for each getter when
                             // needed.
                             NS_LITERAL_CSTRING("URL :: getter"))
  , mValue(aValue)
  , mURLProxy(aURLProxy)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool
  MainThreadRun() override
  {
    AssertIsOnMainThread();
    ErrorResult rv;
    mURLProxy->URL()->GetOrigin(mValue, rv);
    MOZ_ASSERT(!rv.Failed(), "Main-thread getters do not fail.");
    return true;
  }

  void
  Dispatch(ErrorResult& aRv)
  {
    WorkerMainThreadRunnable::Dispatch(Terminating, aRv);
  }

private:
  nsAString& mValue;
  RefPtr<URLWorker::URLProxy> mURLProxy;
};

class SetterRunnable : public WorkerMainThreadRunnable
{
public:
  enum SetterType {
    SetterHref,
    SetterProtocol,
  };

  SetterRunnable(WorkerPrivate* aWorkerPrivate,
                 SetterType aType, const nsAString& aValue,
                 URLWorker::URLProxy* aURLProxy)
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
  MainThreadRun() override
  {
    AssertIsOnMainThread();
    ErrorResult rv;

    switch (mType) {
      case SetterHref: {
        mURLProxy->URL()->SetHref(mValue, rv);
        break;
      }

      case SetterProtocol:
        mURLProxy->URL()->SetProtocol(mValue, rv);
        break;
    }

    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      mFailed = true;
    }

    return true;
  }

  bool Failed() const
  {
    return mFailed;
  }

  void
  Dispatch(ErrorResult& aRv)
  {
    WorkerMainThreadRunnable::Dispatch(Terminating, aRv);
  }

private:
  const nsString mValue;
  SetterType mType;
  RefPtr<URLWorker::URLProxy> mURLProxy;
  bool mFailed;
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

  runnable->Dispatch(Terminating, aRv);
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

  runnable->Dispatch(Terminating, aRv);
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

  runnable->Dispatch(Terminating, aRv);
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

  if (scheme.EqualsLiteral("http") || scheme.EqualsLiteral("https")) {
    nsCOMPtr<nsIURI> baseURL;
    if (aBase.WasPassed()) {
      // XXXcatalinb: SetSpec only writes a warning to the console on urls
      // without a valid scheme. I can't fix that because we've come to rely
      // on that behaviour in a bunch of different places.
      nsresult rv = NS_MutateURI(new nsStandardURL::Mutator())
        .SetSpec(NS_ConvertUTF16toUTF8(aBase.Value()))
        .Finalize(baseURL);
      nsAutoCString baseScheme;
      if (baseURL) {
        baseURL->GetScheme(baseScheme);
      }
      if (NS_WARN_IF(NS_FAILED(rv)) || baseScheme.IsEmpty()) {
        aRv.ThrowTypeError<MSG_INVALID_URL>(aBase.Value());
        return;
      }
    }
    nsCOMPtr<nsIURI> uri;
    aRv = NS_MutateURI(new nsStandardURL::Mutator())
            .Apply(NS_MutatorMethod(&nsIStandardURLMutator::Init,
                                    nsIStandardURL::URLTYPE_STANDARD,
                                    -1, NS_ConvertUTF16toUTF8(aURL),
                                    nullptr, baseURL, nullptr))
            .Finalize(mStdURL);
    return;
  }

  // create url proxy
  RefPtr<ConstructorRunnable> runnable =
    new ConstructorRunnable(mWorkerPrivate, aURL, aBase);
  runnable->Dispatch(Terminating, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  mURLProxy = runnable->GetURLProxy(aRv);
}

URLWorker::~URLWorker()
{
  if (mURLProxy) {
    mWorkerPrivate->AssertIsOnWorkerThread();

    RefPtr<TeardownURLRunnable> runnable =
      new TeardownURLRunnable(mURLProxy);
    mURLProxy = nullptr;

    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      NS_ERROR("Failed to dispatch teardown runnable!");
    }
  }
}

void
URLWorker::GetHref(nsAString& aHref) const
{
  aHref.Truncate();
  if (mStdURL) {
    nsAutoCString href;
    nsresult rv = mStdURL->GetSpec(href);
    if (NS_SUCCEEDED(rv)) {
      CopyUTF8toUTF16(href, aHref);
    }
    return;
  }

  MOZ_ASSERT(mURLProxy);
  mURLProxy->URL()->GetHref(aHref);
}

void
URLWorker::SetHref(const nsAString& aHref, ErrorResult& aRv)
{
  SetHrefInternal(aHref, eUseProxyIfNeeded, aRv);
}

void
URLWorker::SetHrefInternal(const nsAString& aHref, Strategy aStrategy,
                           ErrorResult& aRv)
{
  nsAutoCString scheme;
  nsresult rv = net_ExtractURLScheme(NS_ConvertUTF16toUTF8(aHref), scheme);
  if (NS_FAILED(rv)) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(aHref);
    return;
  }

  if (aStrategy == eUseProxyIfNeeded &&
      (scheme.EqualsLiteral("http") || scheme.EqualsLiteral("https"))) {
    nsCOMPtr<nsIURI> uri;
    aRv = NS_MutateURI(new nsStandardURL::Mutator())
            .SetSpec(NS_ConvertUTF16toUTF8(aHref))
            .Finalize(mStdURL);
    if (mURLProxy) {
      mWorkerPrivate->AssertIsOnWorkerThread();

      RefPtr<TeardownURLRunnable> runnable =
        new TeardownURLRunnable(mURLProxy);
      mURLProxy = nullptr;

      if (NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(runnable)))) {
        return;
      }
    }

    UpdateURLSearchParams();
    return;
  }

  mStdURL = nullptr;
  // fallback to using a main thread url proxy
  if (mURLProxy) {
    RefPtr<SetterRunnable> runnable =
      new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterHref, aHref,
                         mURLProxy);

    runnable->Dispatch(aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    UpdateURLSearchParams();
    return;
  }

  // create the proxy now
  RefPtr<ConstructorRunnable> runnable =
    new ConstructorRunnable(mWorkerPrivate, aHref, Optional<nsAString>());
  runnable->Dispatch(Terminating, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  mURLProxy = runnable->GetURLProxy(aRv);

  UpdateURLSearchParams();
}

void
URLWorker::GetOrigin(nsAString& aOrigin, ErrorResult& aRv) const
{
  if (mStdURL) {
    nsContentUtils::GetUTFOrigin(mStdURL, aOrigin);
    return;
  }

  MOZ_ASSERT(mURLProxy);
  RefPtr<OriginGetterRunnable> runnable =
    new OriginGetterRunnable(mWorkerPrivate, aOrigin, mURLProxy);

  runnable->Dispatch(aRv);
}

void
URLWorker::GetProtocol(nsAString& aProtocol) const
{
  aProtocol.Truncate();
  nsAutoCString protocol;
  if (mStdURL) {
    if (NS_SUCCEEDED(mStdURL->GetScheme(protocol))) {
      CopyASCIItoUTF16(protocol, aProtocol);
      aProtocol.Append(char16_t(':'));
    }

    return;
  }

  MOZ_ASSERT(mURLProxy);
  mURLProxy->URL()->GetProtocol(aProtocol);
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

  // If we are using nsStandardURL on the owning thread, we can continue only if
  // the scheme is http or https.
  if (mStdURL &&
      (scheme.EqualsLiteral("http") || scheme.EqualsLiteral("https"))) {
    Unused << NS_MutateURI(mStdURL)
                .SetScheme(scheme)
                .Finalize(mStdURL);
    return;
  }

  // If we are using mStandardURL but the new scheme is not http nor https, we
  // have to migrate to the URL proxy.
  if (mStdURL) {
    nsAutoCString href;
    nsresult rv = mStdURL->GetSpec(href);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    SetHrefInternal(NS_ConvertUTF8toUTF16(href), eAlwaysUseProxy, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    // We want a proxy here.
    MOZ_ASSERT(!mStdURL);
    MOZ_ASSERT(mURLProxy);

    // Now we can restart setting the protocol.
  }

  MOZ_ASSERT(mURLProxy);
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterProtocol,
                       aProtocol, mURLProxy);

  runnable->Dispatch(aRv);

  MOZ_ASSERT(!runnable->Failed());
}

#define STDURL_GETTER(value, method)    \
  if (mStdURL) {                        \
    value.Truncate();                   \
    nsAutoCString tmp;                  \
    nsresult rv = mStdURL->method(tmp); \
    if (NS_SUCCEEDED(rv)) {             \
      CopyUTF8toUTF16(tmp, value);      \
    }                                   \
    return;                             \
  }

#define STDURL_SETTER(value, method)                     \
  if (mStdURL) {                                         \
    Unused << NS_MutateURI(mStdURL)                      \
              .method(NS_ConvertUTF16toUTF8(value))      \
              .Finalize(mStdURL);                        \
    return;                                              \
  }

void
URLWorker::GetUsername(nsAString& aUsername) const
{
  STDURL_GETTER(aUsername, GetUsername);

  MOZ_ASSERT(mURLProxy);
  mURLProxy->URL()->GetUsername(aUsername);
}

void
URLWorker::SetUsername(const nsAString& aUsername)
{
  STDURL_SETTER(aUsername, SetUsername);

  MOZ_ASSERT(mURLProxy);
  mURLProxy->URL()->SetUsername(aUsername);
}

void
URLWorker::GetPassword(nsAString& aPassword) const
{
  STDURL_GETTER(aPassword, GetPassword);

  MOZ_ASSERT(mURLProxy);
  mURLProxy->URL()->GetPassword(aPassword);
}

void
URLWorker::SetPassword(const nsAString& aPassword)
{
  STDURL_SETTER(aPassword, SetPassword);

  MOZ_ASSERT(mURLProxy);
  mURLProxy->URL()->SetPassword(aPassword);
}

void
URLWorker::GetHost(nsAString& aHost) const
{
  STDURL_GETTER(aHost, GetHostPort);

  MOZ_ASSERT(mURLProxy);
  mURLProxy->URL()->GetHost(aHost);
}

void
URLWorker::SetHost(const nsAString& aHost)
{
  STDURL_SETTER(aHost, SetHostPort);

  MOZ_ASSERT(mURLProxy);
  mURLProxy->URL()->SetHost(aHost);
}

void
URLWorker::GetHostname(nsAString& aHostname) const
{
  aHostname.Truncate();
  if (mStdURL) {
    nsresult rv = nsContentUtils::GetHostOrIPv6WithBrackets(mStdURL, aHostname);
    if (NS_FAILED(rv)) {
      aHostname.Truncate();
    }
    return;
  }

  MOZ_ASSERT(mURLProxy);
  mURLProxy->URL()->GetHostname(aHostname);
}

void
URLWorker::SetHostname(const nsAString& aHostname)
{
  STDURL_SETTER(aHostname, SetHost);

  MOZ_ASSERT(mURLProxy);
  mURLProxy->URL()->SetHostname(aHostname);
}

void
URLWorker::GetPort(nsAString& aPort) const
{
  aPort.Truncate();

  if (mStdURL) {
    int32_t port;
    nsresult rv = mStdURL->GetPort(&port);
    if (NS_SUCCEEDED(rv) && port != -1) {
      nsAutoString portStr;
      portStr.AppendInt(port, 10);
      aPort.Assign(portStr);
    }
    return;
  }

  MOZ_ASSERT(mURLProxy);
  mURLProxy->URL()->GetPort(aPort);
}

void
URLWorker::SetPort(const nsAString& aPort)
{
  if (mStdURL) {
    nsresult rv;
    nsAutoString portStr(aPort);
    int32_t port = -1;

    // nsIURI uses -1 as default value.
    if (!portStr.IsEmpty()) {
      port = portStr.ToInteger(&rv);
      if (NS_FAILED(rv)) {
        return;
      }
    }

    Unused << NS_MutateURI(mStdURL)
                .SetPort(port)
                .Finalize(mStdURL);
    return;
  }

  MOZ_ASSERT(mURLProxy);
  mURLProxy->URL()->SetPort(aPort);
}

void
URLWorker::GetPathname(nsAString& aPathname) const
{
  aPathname.Truncate();

  if (mStdURL) {
    nsAutoCString file;
    nsresult rv = mStdURL->GetFilePath(file);
    if (NS_SUCCEEDED(rv)) {
      CopyUTF8toUTF16(file, aPathname);
    }
    return;
  }

  MOZ_ASSERT(mURLProxy);
  mURLProxy->URL()->GetPathname(aPathname);
}

void
URLWorker::SetPathname(const nsAString& aPathname)
{
  STDURL_SETTER(aPathname, SetFilePath);

  MOZ_ASSERT(mURLProxy);
  mURLProxy->URL()->SetPathname(aPathname);
}

void
URLWorker::GetSearch(nsAString& aSearch) const
{
  aSearch.Truncate();

  if (mStdURL) {
    nsAutoCString search;
    nsresult rv;

    rv = mStdURL->GetQuery(search);
    if (NS_SUCCEEDED(rv) && !search.IsEmpty()) {
      aSearch.Assign(u'?');
      AppendUTF8toUTF16(search, aSearch);
    }
    return;
  }

  MOZ_ASSERT(mURLProxy);
  mURLProxy->URL()->GetSearch(aSearch);
}

void
URLWorker::GetHash(nsAString& aHash) const
{
  aHash.Truncate();
  if (mStdURL) {
    nsAutoCString ref;
    nsresult rv = mStdURL->GetRef(ref);
    if (NS_SUCCEEDED(rv) && !ref.IsEmpty()) {
      aHash.Assign(char16_t('#'));
      AppendUTF8toUTF16(ref, aHash);
    }
    return;
  }

  MOZ_ASSERT(mURLProxy);
  mURLProxy->URL()->GetHash(aHash);
}

void
URLWorker::SetHash(const nsAString& aHash)
{
  STDURL_SETTER(aHash, SetRef);

  MOZ_ASSERT(mURLProxy);
  mURLProxy->URL()->SetHash(aHash);
}

void
URLWorker::SetSearchInternal(const nsAString& aSearch)
{
  if (mStdURL) {
    // URLMainThread ignores failures here.
    Unused << NS_MutateURI(mStdURL)
                .SetQuery(NS_ConvertUTF16toUTF8(aSearch))
                .Finalize(mStdURL);
    return;
  }

  MOZ_ASSERT(mURLProxy);
  mURLProxy->URL()->SetSearch(aSearch);
}

void
URLWorker::UpdateURLSearchParams()
{
  if (mSearchParams) {
    nsAutoString search;
    GetSearch(search);
    mSearchParams->ParseInput(NS_ConvertUTF16toUTF8(Substring(search, 1)));
  }
}

} // namespace dom
} // namespace mozilla
