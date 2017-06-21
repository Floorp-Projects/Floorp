/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URLMainThread.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Blob.h"
#include "nsContentUtils.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsIURL.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

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

} // anonymous namespace

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
URLMainThread::CreateObjectURL(const GlobalObject& aGlobal, Blob& aBlob,
                               nsAString& aResult, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  CreateObjectURLInternal(aGlobal, aBlob.Impl(), aResult, aRv);
}

/* static */ void
URLMainThread::CreateObjectURL(const GlobalObject& aGlobal,
                               DOMMediaStream& aStream,
                               nsAString& aResult, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  CreateObjectURLInternal(aGlobal, &aStream, aResult, aRv);
}

/* static */ void
URLMainThread::CreateObjectURL(const GlobalObject& aGlobal,
                               MediaSource& aSource,
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

URLMainThread::URLMainThread(nsISupports* aParent,
                             already_AddRefed<nsIURI> aURI)
  : URL(aParent)
  , mURI(aURI)
{
  MOZ_ASSERT(NS_IsMainThread());
}

URLMainThread::~URLMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
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

  nsAutoCString file;
  nsresult rv = mURI->GetFilePath(file);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(file, aPathname);
  }
}

void
URLMainThread::SetPathname(const nsAString& aPathname, ErrorResult& aRv)
{
  // Do not throw!

  mURI->SetFilePath(NS_ConvertUTF16toUTF8(aPathname));
}

void
URLMainThread::GetSearch(nsAString& aSearch, ErrorResult& aRv) const
{
  aSearch.Truncate();

  // Do not throw!  Not having a valid URI or URL should result in an empty
  // string.

  nsAutoCString search;
  nsresult rv;

  rv = mURI->GetQuery(search);
  if (NS_SUCCEEDED(rv) && !search.IsEmpty()) {
    CopyUTF8toUTF16(NS_LITERAL_CSTRING("?") + search, aSearch);
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

  mURI->SetQuery(NS_ConvertUTF16toUTF8(aSearch));
}

nsIURI*
URLMainThread::GetURI() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mURI;
}

} // namespace dom
} // namespace mozilla
