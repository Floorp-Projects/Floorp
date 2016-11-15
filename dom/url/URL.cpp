/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URL.h"

#include "DOMMediaStream.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/MediaSource.h"
#include "mozilla/dom/URLBinding.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/ipc/nsIRemoteBlob.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "nsContentUtils.h"
#include "nsEscape.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsIIOService.h"
#include "nsIURIWithQuery.h"
#include "nsIURL.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"

namespace mozilla {
namespace dom {

///////////////////////////////////////////////////////////////////////////////
// URL for main-thread
///////////////////////////////////////////////////////////////////////////////

namespace {

template<typename T>
void
CreateObjectURLInternal(const GlobalObject& aGlobal, T aObject,
                        nsAString& aResult, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsIPrincipal> principal =
    nsContentUtils::ObjectPrincipal(aGlobal.Get());

  nsAutoCString url;
  aRv = nsHostObjectProtocolHandler::AddDataEntry(aObject, principal, url);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  global->RegisterHostObjectURI(url);
  CopyASCIItoUTF16(url, aResult);
}

// The URL implementation for the main-thread
class URLMainThread final : public URL
{
public:
  static already_AddRefed<URLMainThread>
  Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
              URL& aBase, ErrorResult& aRv);

  static already_AddRefed<URLMainThread>
  Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
              const Optional<nsAString>& aBase, ErrorResult& aRv);

  static already_AddRefed<URLMainThread>
  Constructor(nsISupports* aParent, const nsAString& aURL,
              const nsAString& aBase, ErrorResult& aRv);

  static already_AddRefed<URLMainThread>
  Constructor(nsISupports* aParent, const nsAString& aURL, nsIURI* aBase,
              ErrorResult& aRv);

  static void
  CreateObjectURL(const GlobalObject& aGlobal, Blob& aBlob,
                  const objectURLOptions& aOptions, nsAString& aResult,
                  ErrorResult& aRv)
  {
    MOZ_ASSERT(NS_IsMainThread());
    CreateObjectURLInternal(aGlobal, aBlob.Impl(), aResult, aRv);
  }

  static void
  CreateObjectURL(const GlobalObject& aGlobal, DOMMediaStream& aStream,
                  const objectURLOptions& aOptions, nsAString& aResult,
                  ErrorResult& aRv)
  {
    MOZ_ASSERT(NS_IsMainThread());
    CreateObjectURLInternal(aGlobal, &aStream, aResult, aRv);
  }

  static void
  CreateObjectURL(const GlobalObject& aGlobal, MediaSource& aSource,
                  const objectURLOptions& aOptions, nsAString& aResult,
                  ErrorResult& aRv);

  static void
  RevokeObjectURL(const GlobalObject& aGlobal, const nsAString& aURL,
                  ErrorResult& aRv);

  static bool
  IsValidURL(const GlobalObject& aGlobal, const nsAString& aURL,
             ErrorResult& aRv);

  URLMainThread(nsISupports* aParent, already_AddRefed<nsIURI> aURI)
    : URL(aParent)
    , mURI(aURI)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  virtual void
  GetHref(nsAString& aHref, ErrorResult& aRv) const override;

  virtual void
  SetHref(const nsAString& aHref, ErrorResult& aRv) override;

  virtual void
  GetOrigin(nsAString& aOrigin, ErrorResult& aRv) const override;

  virtual void
  GetProtocol(nsAString& aProtocol, ErrorResult& aRv) const override;

  virtual void
  SetProtocol(const nsAString& aProtocol, ErrorResult& aRv) override;

  virtual void
  GetUsername(nsAString& aUsername, ErrorResult& aRv) const override;

  virtual void
  SetUsername(const nsAString& aUsername, ErrorResult& aRv) override;

  virtual void
  GetPassword(nsAString& aPassword, ErrorResult& aRv) const override;

  virtual void
  SetPassword(const nsAString& aPassword, ErrorResult& aRv) override;

  virtual void
  GetHost(nsAString& aHost, ErrorResult& aRv) const override;

  virtual void
  SetHost(const nsAString& aHost, ErrorResult& aRv) override;

  virtual void
  GetHostname(nsAString& aHostname, ErrorResult& aRv) const override;

  virtual void
  SetHostname(const nsAString& aHostname, ErrorResult& aRv) override;

  virtual void
  GetPort(nsAString& aPort, ErrorResult& aRv) const override;

  virtual void
  SetPort(const nsAString& aPort, ErrorResult& aRv) override;

  virtual void
  GetPathname(nsAString& aPathname, ErrorResult& aRv) const override;

  virtual void
  SetPathname(const nsAString& aPathname, ErrorResult& aRv) override;

  virtual void
  GetSearch(nsAString& aSearch, ErrorResult& aRv) const override;

  virtual void
  GetHash(nsAString& aHost, ErrorResult& aRv) const override;

  virtual void
  SetHash(const nsAString& aHash, ErrorResult& aRv) override;

  virtual void UpdateURLSearchParams() override;

  virtual void
  SetSearchInternal(const nsAString& aSearch, ErrorResult& aRv) override;

  nsIURI*
  GetURI() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    return mURI;
  }

private:
  ~URLMainThread()
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  nsCOMPtr<nsIURI> mURI;
};

/* static */ already_AddRefed<URLMainThread>
URLMainThread::Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
                           URL& aBase, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  URLMainThread& base = static_cast<URLMainThread&>(aBase);
  return Constructor(aGlobal.GetAsSupports(), aURL, base.GetURI(), aRv);
}

/* static */ already_AddRefed<URLMainThread>
URLMainThread::Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
                           const Optional<nsAString>& aBase, ErrorResult& aRv)
{
  if (aBase.WasPassed()) {
    return Constructor(aGlobal.GetAsSupports(), aURL, aBase.Value(), aRv);
  }

  return Constructor(aGlobal.GetAsSupports(), aURL, nullptr, aRv);
}

/* static */ already_AddRefed<URLMainThread>
URLMainThread::Constructor(nsISupports* aParent, const nsAString& aURL,
                           const nsAString& aBase, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIURI> baseUri;
  nsresult rv = NS_NewURI(getter_AddRefs(baseUri), aBase, nullptr, nullptr,
                          nsContentUtils::GetIOService());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(aBase);
    return nullptr;
  }

  return Constructor(aParent, aURL, baseUri, aRv);
}

/* static */ already_AddRefed<URLMainThread>
URLMainThread::Constructor(nsISupports* aParent, const nsAString& aURL,
                           nsIURI* aBase, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL, nullptr, aBase,
                          nsContentUtils::GetIOService());
  if (NS_FAILED(rv)) {
    // No need to warn in this case. It's common to use the URL constructor
    // to determine if a URL is valid and an exception will be propagated.
    aRv.ThrowTypeError<MSG_INVALID_URL>(aURL);
    return nullptr;
  }

  RefPtr<URLMainThread> url = new URLMainThread(aParent, uri.forget());
  return url.forget();
}

/* static */ void
URLMainThread::CreateObjectURL(const GlobalObject& aGlobal,
                               MediaSource& aSource,
                               const objectURLOptions& aOptions,
                               nsAString& aResult, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIPrincipal> principal =
    nsContentUtils::ObjectPrincipal(aGlobal.Get());

  nsAutoCString url;
  aRv = nsHostObjectProtocolHandler::AddDataEntry(&aSource, principal, url);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsCOMPtr<nsIRunnable> revocation = NS_NewRunnableFunction(
    [url] {
      nsHostObjectProtocolHandler::RemoveDataEntry(url);
    });

  nsContentUtils::RunInStableState(revocation.forget());

  CopyASCIItoUTF16(url, aResult);
}

/* static */ void
URLMainThread::RevokeObjectURL(const GlobalObject& aGlobal,
                               const nsAString& aURL, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsIPrincipal* principal = nsContentUtils::ObjectPrincipal(aGlobal.Get());

  NS_LossyConvertUTF16toASCII asciiurl(aURL);

  nsIPrincipal* urlPrincipal =
    nsHostObjectProtocolHandler::GetDataEntryPrincipal(asciiurl);

  if (urlPrincipal && principal->Subsumes(urlPrincipal)) {
    global->UnregisterHostObjectURI(asciiurl);
    nsHostObjectProtocolHandler::RemoveDataEntry(asciiurl);
  }
}

/* static */ bool
URLMainThread::IsValidURL(const GlobalObject& aGlobal, const nsAString& aURL,
                          ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_LossyConvertUTF16toASCII asciiurl(aURL);
  return nsHostObjectProtocolHandler::HasDataEntry(asciiurl);
}

void
URLMainThread::GetHref(nsAString& aHref, ErrorResult& aRv) const
{
  aHref.Truncate();

  nsAutoCString href;
  nsresult rv = mURI->GetSpec(href);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(href, aHref);
  }
}

void
URLMainThread::SetHref(const nsAString& aHref, ErrorResult& aRv)
{
  NS_ConvertUTF16toUTF8 href(aHref);

  nsresult rv;
  nsCOMPtr<nsIIOService> ioService(do_GetService(NS_IOSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  nsCOMPtr<nsIURI> uri;
  rv = ioService->NewURI(href, nullptr, nullptr, getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(aHref);
    return;
  }

  mURI = uri;
  UpdateURLSearchParams();
}

void
URLMainThread::GetOrigin(nsAString& aOrigin, ErrorResult& aRv) const
{
  nsContentUtils::GetUTFOrigin(mURI, aOrigin);
}

void
URLMainThread::GetProtocol(nsAString& aProtocol, ErrorResult& aRv) const
{
  nsAutoCString protocol;
  if (NS_SUCCEEDED(mURI->GetScheme(protocol))) {
    aProtocol.Truncate();
  }

  CopyASCIItoUTF16(protocol, aProtocol);
  aProtocol.Append(char16_t(':'));
}

void
URLMainThread::SetProtocol(const nsAString& aProtocol, ErrorResult& aRv)
{
  nsAString::const_iterator start, end;
  aProtocol.BeginReading(start);
  aProtocol.EndReading(end);
  nsAString::const_iterator iter(start);

  FindCharInReadable(':', iter, end);

  // Changing the protocol of a URL, changes the "nature" of the URI
  // implementation. In order to do this properly, we have to serialize the
  // existing URL and reparse it in a new object.
  nsCOMPtr<nsIURI> clone;
  nsresult rv = mURI->Clone(getter_AddRefs(clone));
  if (NS_WARN_IF(NS_FAILED(rv)) || !clone) {
    return;
  }

  rv = clone->SetScheme(NS_ConvertUTF16toUTF8(Substring(start, iter)));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsAutoCString href;
  rv = clone->GetSpec(href);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), href);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  mURI = uri;
}

#define URL_GETTER( value, func ) \
  value.Truncate();               \
  nsAutoCString tmp;              \
  nsresult rv = mURI->func(tmp);  \
  if (NS_SUCCEEDED(rv)) {         \
    CopyUTF8toUTF16(tmp, value);  \
  }

void
URLMainThread::GetUsername(nsAString& aUsername, ErrorResult& aRv) const
{
  URL_GETTER(aUsername, GetUsername);
}

void
URLMainThread::SetUsername(const nsAString& aUsername, ErrorResult& aRv)
{
  mURI->SetUsername(NS_ConvertUTF16toUTF8(aUsername));
}

void
URLMainThread::GetPassword(nsAString& aPassword, ErrorResult& aRv) const
{
  URL_GETTER(aPassword, GetPassword);
}

void
URLMainThread::SetPassword(const nsAString& aPassword, ErrorResult& aRv)
{
  mURI->SetPassword(NS_ConvertUTF16toUTF8(aPassword));
}

void
URLMainThread::GetHost(nsAString& aHost, ErrorResult& aRv) const
{
  URL_GETTER(aHost, GetHostPort);
}

void
URLMainThread::SetHost(const nsAString& aHost, ErrorResult& aRv)
{
  mURI->SetHostPort(NS_ConvertUTF16toUTF8(aHost));
}

void
URLMainThread::UpdateURLSearchParams()
{
  if (!mSearchParams) {
    return;
  }

  nsAutoCString search;
  nsCOMPtr<nsIURL> url(do_QueryInterface(mURI));
  if (url) {
    nsresult rv = url->GetQuery(search);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      search.Truncate();
    }
  }

  mSearchParams->ParseInput(search);
}

void
URLMainThread::GetHostname(nsAString& aHostname, ErrorResult& aRv) const
{
  aHostname.Truncate();
  nsContentUtils::GetHostOrIPv6WithBrackets(mURI, aHostname);
}

void
URLMainThread::SetHostname(const nsAString& aHostname, ErrorResult& aRv)
{
  // nsStandardURL returns NS_ERROR_UNEXPECTED for an empty hostname
  // The return code is silently ignored
  mURI->SetHost(NS_ConvertUTF16toUTF8(aHostname));
}

void
URLMainThread::GetPort(nsAString& aPort, ErrorResult& aRv) const
{
  aPort.Truncate();

  int32_t port;
  nsresult rv = mURI->GetPort(&port);
  if (NS_SUCCEEDED(rv) && port != -1) {
    nsAutoString portStr;
    portStr.AppendInt(port, 10);
    aPort.Assign(portStr);
  }
}

void
URLMainThread::SetPort(const nsAString& aPort, ErrorResult& aRv)
{
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

  mURI->SetPort(port);
}

void
URLMainThread::GetPathname(nsAString& aPathname, ErrorResult& aRv) const
{
  aPathname.Truncate();

  // Do not throw!  Not having a valid URI or URL should result in an empty
  // string.

  nsCOMPtr<nsIURIWithQuery> url(do_QueryInterface(mURI));
  if (url) {
    nsAutoCString file;
    nsresult rv = url->GetFilePath(file);
    if (NS_SUCCEEDED(rv)) {
      CopyUTF8toUTF16(file, aPathname);
    }

    return;
  }

  nsAutoCString path;
  nsresult rv = mURI->GetPath(path);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(path, aPathname);
  }
}

void
URLMainThread::SetPathname(const nsAString& aPathname, ErrorResult& aRv)
{
  // Do not throw!

  nsCOMPtr<nsIURIWithQuery> url(do_QueryInterface(mURI));
  if (url) {
    url->SetFilePath(NS_ConvertUTF16toUTF8(aPathname));
    return;
  }
}

void
URLMainThread::GetSearch(nsAString& aSearch, ErrorResult& aRv) const
{
  aSearch.Truncate();

  // Do not throw!  Not having a valid URI or URL should result in an empty
  // string.

  nsAutoCString search;
  nsresult rv;

  nsCOMPtr<nsIURIWithQuery> url(do_QueryInterface(mURI));
  if (url) {
    rv = url->GetQuery(search);
    if (NS_SUCCEEDED(rv) && !search.IsEmpty()) {
      CopyUTF8toUTF16(NS_LITERAL_CSTRING("?") + search, aSearch);
    }
    return;
  }
}

void
URLMainThread::GetHash(nsAString& aHash, ErrorResult& aRv) const
{
  aHash.Truncate();

  nsAutoCString ref;
  nsresult rv = mURI->GetRef(ref);
  if (NS_SUCCEEDED(rv) && !ref.IsEmpty()) {
    aHash.Assign(char16_t('#'));
    if (nsContentUtils::GettersDecodeURLHash()) {
      NS_UnescapeURL(ref); // XXX may result in random non-ASCII bytes!
    }
    AppendUTF8toUTF16(ref, aHash);
  }
}

void
URLMainThread::SetHash(const nsAString& aHash, ErrorResult& aRv)
{
  mURI->SetRef(NS_ConvertUTF16toUTF8(aHash));
}

void
URLMainThread::SetSearchInternal(const nsAString& aSearch, ErrorResult& aRv)
{
  // Ignore failures to be compatible with NS4.

  nsCOMPtr<nsIURIWithQuery> uriWithQuery(do_QueryInterface(mURI));
  if (uriWithQuery) {
    uriWithQuery->SetQuery(NS_ConvertUTF16toUTF8(aSearch));
    return;
  }
}

} // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// URL for Workers
///////////////////////////////////////////////////////////////////////////////

namespace {

using namespace workers;

// Proxy class to forward all the requests to a URLMainThread object.
class URLProxy final
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
    MOZ_ASSERT(NS_IsMainThread());
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

// URLWorker implements the URL object in workers.
class URLWorker final : public URL
{
public:
  static already_AddRefed<URLWorker>
  Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
              URL& aBase, ErrorResult& aRv);

  static already_AddRefed<URLWorker>
  Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
              const Optional<nsAString>& aBase, ErrorResult& aRv);

  static already_AddRefed<URLWorker>
  Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
              const nsAString& aBase, ErrorResult& aRv);

  static void
  CreateObjectURL(const GlobalObject& aGlobal, Blob& aBlob,
                  const mozilla::dom::objectURLOptions& aOptions,
                  nsAString& aResult, mozilla::ErrorResult& aRv);

  static void
  RevokeObjectURL(const GlobalObject& aGlobal, const nsAString& aUrl,
                  ErrorResult& aRv);

  static bool
  IsValidURL(const GlobalObject& aGlobal, const nsAString& aUrl,
             ErrorResult& aRv);

  URLWorker(WorkerPrivate* aWorkerPrivate, URLProxy* aURLProxy);

  virtual void
  GetHref(nsAString& aHref, ErrorResult& aRv) const override;

  virtual void
  SetHref(const nsAString& aHref, ErrorResult& aRv) override;

  virtual void
  GetOrigin(nsAString& aOrigin, ErrorResult& aRv) const override;

  virtual void
  GetProtocol(nsAString& aProtocol, ErrorResult& aRv) const override;

  virtual void
  SetProtocol(const nsAString& aProtocol, ErrorResult& aRv) override;

  virtual void
  GetUsername(nsAString& aUsername, ErrorResult& aRv) const override;

  virtual void
  SetUsername(const nsAString& aUsername, ErrorResult& aRv) override;

  virtual void
  GetPassword(nsAString& aPassword, ErrorResult& aRv) const override;

  virtual void
  SetPassword(const nsAString& aPassword, ErrorResult& aRv) override;

  virtual void
  GetHost(nsAString& aHost, ErrorResult& aRv) const override;

  virtual void
  SetHost(const nsAString& aHost, ErrorResult& aRv) override;

  virtual void
  GetHostname(nsAString& aHostname, ErrorResult& aRv) const override;

  virtual void
  SetHostname(const nsAString& aHostname, ErrorResult& aRv) override;

  virtual void
  GetPort(nsAString& aPort, ErrorResult& aRv) const override;

  virtual void
  SetPort(const nsAString& aPort, ErrorResult& aRv) override;

  virtual void
  GetPathname(nsAString& aPathname, ErrorResult& aRv) const override;

  virtual void
  SetPathname(const nsAString& aPathname, ErrorResult& aRv) override;

  virtual void
  GetSearch(nsAString& aSearch, ErrorResult& aRv) const override;

  virtual void
  GetHash(nsAString& aHost, ErrorResult& aRv) const override;

  virtual void
  SetHash(const nsAString& aHash, ErrorResult& aRv) override;

  virtual void UpdateURLSearchParams() override;

  virtual void
  SetSearchInternal(const nsAString& aSearch, ErrorResult& aRv) override;

  URLProxy*
  GetURLProxy() const
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
    return mURLProxy;
  }

private:
  ~URLWorker();

  workers::WorkerPrivate* mWorkerPrivate;
  RefPtr<URLProxy> mURLProxy;
};

// This class creates an URL from a DOM Blob on the main thread.
class CreateURLRunnable : public WorkerMainThreadRunnable
{
private:
  BlobImpl* mBlobImpl;
  nsAString& mURL;

public:
  CreateURLRunnable(WorkerPrivate* aWorkerPrivate, BlobImpl* aBlobImpl,
                    const objectURLOptions& aOptions,
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
  MainThreadRun()
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
  RefPtr<URLProxy> mBaseProxy;

  RefPtr<URLProxy> mRetval;

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

  ConstructorRunnable(WorkerPrivate* aWorkerPrivate,
                      const nsAString& aURL, URLProxy* aBaseProxy)
  : WorkerMainThreadRunnable(aWorkerPrivate,
                             NS_LITERAL_CSTRING("URL :: Constructor with BaseURL"))
  , mURL(aURL)
  , mBaseProxy(aBaseProxy)
  {
    mBase.SetIsVoid(true);
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool
  MainThreadRun()
  {
    AssertIsOnMainThread();

    ErrorResult rv;
    RefPtr<URLMainThread> url;
    if (mBaseProxy) {
      url = URLMainThread::Constructor(nullptr, mURL, mBaseProxy->URI(), rv);
    } else if (!mBase.IsVoid()) {
      url = URLMainThread::Constructor(nullptr, mURL, mBase, rv);
    } else {
      url = URLMainThread::Constructor(nullptr, mURL, nullptr, rv);
    }

    if (rv.Failed()) {
      rv.SuppressException();
      return true;
    }

    mRetval = new URLProxy(url.forget());
    return true;
  }

  URLProxy*
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

    MOZ_ASSERT(!rv.Failed(), "Main-thread getters do not fail.");
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
    ErrorResult rv;

    switch (mType) {
      case SetterHref: {
        mURLProxy->URL()->SetHref(mValue, rv);
        break;
      }

      case SetterProtocol:
        mURLProxy->URL()->SetProtocol(mValue, rv);
        break;

      case SetterUsername:
        mURLProxy->URL()->SetUsername(mValue, rv);
        break;

      case SetterPassword:
        mURLProxy->URL()->SetPassword(mValue, rv);
        break;

      case SetterHost:
        mURLProxy->URL()->SetHost(mValue, rv);
        break;

      case SetterHostname:
        mURLProxy->URL()->SetHostname(mValue, rv);
        break;

      case SetterPort:
        mURLProxy->URL()->SetPort(mValue, rv);
        break;

      case SetterPathname:
        mURLProxy->URL()->SetPathname(mValue, rv);
        break;

      case SetterSearch:
        mURLProxy->URL()->SetSearch(mValue, rv);
        break;

      case SetterHash:
        mURLProxy->URL()->SetHash(mValue, rv);
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

private:
  const nsString mValue;
  SetterType mType;
  RefPtr<URLProxy> mURLProxy;
  bool mFailed;
};

already_AddRefed<URLWorker>
FinishConstructor(JSContext* aCx, WorkerPrivate* aPrivate,
                  ConstructorRunnable* aRunnable, ErrorResult& aRv)
{
  aRunnable->Dispatch(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<URLProxy> proxy = aRunnable->GetURLProxy(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<URLWorker> url = new URLWorker(aPrivate, proxy);
  return url.forget();
}

/* static */ already_AddRefed<URLWorker>
URLWorker::Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
                       URL& aBase, ErrorResult& aRv)
{
  MOZ_ASSERT(!NS_IsMainThread());

  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  URLWorker& base = static_cast<URLWorker&>(aBase);
  RefPtr<ConstructorRunnable> runnable =
    new ConstructorRunnable(workerPrivate, aURL, base.GetURLProxy());

  return FinishConstructor(cx, workerPrivate, runnable, aRv);
}

/* static */ already_AddRefed<URLWorker>
URLWorker::Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
                       const Optional<nsAString>& aBase, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  RefPtr<ConstructorRunnable> runnable =
    new ConstructorRunnable(workerPrivate, aURL, aBase);

  return FinishConstructor(cx, workerPrivate, runnable, aRv);
}

/* static */ already_AddRefed<URLWorker>
URLWorker::Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
                       const nsAString& aBase, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);

  Optional<nsAString> base;
  base = &aBase;

  RefPtr<ConstructorRunnable> runnable =
    new ConstructorRunnable(workerPrivate, aURL, base);

  return FinishConstructor(cx, workerPrivate, runnable, aRv);
}

/* static */ void
URLWorker::CreateObjectURL(const GlobalObject& aGlobal, Blob& aBlob,
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

  runnable->Dispatch(aRv);
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

  runnable->Dispatch(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return false;
  }

  return runnable->IsValidURL();
}

URLWorker::URLWorker(WorkerPrivate* aWorkerPrivate, URLProxy* aURLProxy)
  : URL(nullptr)
  , mWorkerPrivate(aWorkerPrivate)
  , mURLProxy(aURLProxy)
{}

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
URLWorker::GetHref(nsAString& aHref, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterHref, aHref,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URLWorker::SetHref(const nsAString& aHref, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterHref, aHref,
                       mURLProxy);

  runnable->Dispatch(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (runnable->Failed()) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(aHref);
    return;
  }

  UpdateURLSearchParams();
}

void
URLWorker::GetOrigin(nsAString& aOrigin, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterOrigin, aOrigin,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URLWorker::GetProtocol(nsAString& aProtocol, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterProtocol, aProtocol,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URLWorker::SetProtocol(const nsAString& aProtocol, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterProtocol,
                       aProtocol, mURLProxy);

  runnable->Dispatch(aRv);

  MOZ_ASSERT(!runnable->Failed());
}

void
URLWorker::GetUsername(nsAString& aUsername, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterUsername, aUsername,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URLWorker::SetUsername(const nsAString& aUsername, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterUsername,
                       aUsername, mURLProxy);

  runnable->Dispatch(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  MOZ_ASSERT(!runnable->Failed());
}

void
URLWorker::GetPassword(nsAString& aPassword, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterPassword, aPassword,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URLWorker::SetPassword(const nsAString& aPassword, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterPassword,
                       aPassword, mURLProxy);

  runnable->Dispatch(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  MOZ_ASSERT(!runnable->Failed());
}

void
URLWorker::GetHost(nsAString& aHost, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterHost, aHost,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URLWorker::SetHost(const nsAString& aHost, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterHost,
                       aHost, mURLProxy);

  runnable->Dispatch(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  MOZ_ASSERT(!runnable->Failed());
}

void
URLWorker::GetHostname(nsAString& aHostname, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterHostname, aHostname,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URLWorker::SetHostname(const nsAString& aHostname, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterHostname,
                       aHostname, mURLProxy);

  runnable->Dispatch(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  MOZ_ASSERT(!runnable->Failed());
}

void
URLWorker::GetPort(nsAString& aPort, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterPort, aPort,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URLWorker::SetPort(const nsAString& aPort, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterPort,
                       aPort, mURLProxy);

  runnable->Dispatch(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  MOZ_ASSERT(!runnable->Failed());
}

void
URLWorker::GetPathname(nsAString& aPathname, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterPathname,
                       aPathname, mURLProxy);

  runnable->Dispatch(aRv);
}

void
URLWorker::SetPathname(const nsAString& aPathname, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterPathname,
                       aPathname, mURLProxy);

  runnable->Dispatch(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  MOZ_ASSERT(!runnable->Failed());
}

void
URLWorker::GetSearch(nsAString& aSearch, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterSearch, aSearch,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URLWorker::GetHash(nsAString& aHash, ErrorResult& aRv) const
{
  RefPtr<GetterRunnable> runnable =
    new GetterRunnable(mWorkerPrivate, GetterRunnable::GetterHash, aHash,
                       mURLProxy);

  runnable->Dispatch(aRv);
}

void
URLWorker::SetHash(const nsAString& aHash, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterHash,
                       aHash, mURLProxy);

  runnable->Dispatch(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  MOZ_ASSERT(!runnable->Failed());
}

void
URLWorker::SetSearchInternal(const nsAString& aSearch, ErrorResult& aRv)
{
  RefPtr<SetterRunnable> runnable =
    new SetterRunnable(mWorkerPrivate, SetterRunnable::SetterSearch,
                       aSearch, mURLProxy);

  runnable->Dispatch(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  MOZ_ASSERT(!runnable->Failed());
}

void
URLWorker::UpdateURLSearchParams()
{
  if (mSearchParams) {
    nsAutoString search;

    ErrorResult rv;
    GetSearch(search, rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
    }

    mSearchParams->ParseInput(NS_ConvertUTF16toUTF8(Substring(search, 1)));
  }
}

} // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// Base class for URL
///////////////////////////////////////////////////////////////////////////////

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(URL, mParent, mSearchParams)

NS_IMPL_CYCLE_COLLECTING_ADDREF(URL)
NS_IMPL_CYCLE_COLLECTING_RELEASE(URL)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(URL)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
URL::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return URLBinding::Wrap(aCx, this, aGivenProto);
}

/* static */ already_AddRefed<URL>
URL::Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
                 URL& aBase, ErrorResult& aRv)
{
  if (NS_IsMainThread()) {
    return URLMainThread::Constructor(aGlobal, aURL, aBase, aRv);
  }

  return URLWorker::Constructor(aGlobal, aURL, aBase, aRv);
}

/* static */ already_AddRefed<URL>
URL::Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
                 const Optional<nsAString>& aBase, ErrorResult& aRv)
{
  if (NS_IsMainThread()) {
    return URLMainThread::Constructor(aGlobal, aURL, aBase, aRv);
  }

  return URLWorker::Constructor(aGlobal, aURL, aBase, aRv);
}

/* static */ already_AddRefed<URL>
URL::WorkerConstructor(const GlobalObject& aGlobal, const nsAString& aURL,
                       const nsAString& aBase, ErrorResult& aRv)
{
  return URLWorker::Constructor(aGlobal, aURL, aBase, aRv);
}

void
URL::CreateObjectURL(const GlobalObject& aGlobal, Blob& aBlob,
                     const objectURLOptions& aOptions, nsAString& aResult,
                     ErrorResult& aRv)
{
  if (NS_IsMainThread()) {
    URLMainThread::CreateObjectURL(aGlobal, aBlob, aOptions, aResult, aRv);
  } else {
    URLWorker::CreateObjectURL(aGlobal, aBlob, aOptions, aResult, aRv);
  }
}

void
URL::CreateObjectURL(const GlobalObject& aGlobal, DOMMediaStream& aStream,
                     const objectURLOptions& aOptions, nsAString& aResult,
                     ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  URLMainThread::CreateObjectURL(aGlobal, aStream, aOptions, aResult, aRv);
}

void
URL::CreateObjectURL(const GlobalObject& aGlobal, MediaSource& aSource,
                     const objectURLOptions& aOptions,
                     nsAString& aResult,
                     ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  URLMainThread::CreateObjectURL(aGlobal, aSource, aOptions, aResult, aRv);
}

void
URL::RevokeObjectURL(const GlobalObject& aGlobal, const nsAString& aURL,
                     ErrorResult& aRv)
{
  if (NS_IsMainThread()) {
    URLMainThread::RevokeObjectURL(aGlobal, aURL, aRv);
  } else {
    URLWorker::RevokeObjectURL(aGlobal, aURL, aRv);
  }
}

bool
URL::IsValidURL(const GlobalObject& aGlobal, const nsAString& aURL,
                ErrorResult& aRv)
{
  if (NS_IsMainThread()) {
    return URLMainThread::IsValidURL(aGlobal, aURL, aRv);
  }
  return URLWorker::IsValidURL(aGlobal, aURL, aRv);
}

URLSearchParams*
URL::SearchParams()
{
  CreateSearchParamsIfNeeded();
  return mSearchParams;
}

bool IsChromeURI(nsIURI* aURI)
{
  bool isChrome = false;
  if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)))
      return isChrome;
  return false;
}

void
URL::CreateSearchParamsIfNeeded()
{
  if (!mSearchParams) {
    mSearchParams = new URLSearchParams(mParent, this);
    UpdateURLSearchParams();
  }
}

void
URL::SetSearch(const nsAString& aSearch, ErrorResult& aRv)
{
  SetSearchInternal(aSearch, aRv);
  UpdateURLSearchParams();
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
  NS_WARNING_ASSERTION(!rv.Failed(), "SetSearchInternal failed");
  rv.SuppressException();
}

} // namespace dom
} // namespace mozilla
