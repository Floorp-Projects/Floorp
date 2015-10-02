/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URL.h"

#include "nsGlobalWindow.h"
#include "DOMMediaStream.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/MediaSource.h"
#include "mozilla/dom/URLBinding.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsServiceManagerUtils.h"
#include "nsIIOService.h"
#include "nsEscape.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIURL.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(URL, mParent, mSearchParams)

NS_IMPL_CYCLE_COLLECTING_ADDREF(URL)
NS_IMPL_CYCLE_COLLECTING_RELEASE(URL)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(URL)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

URL::URL(nsISupports* aParent, already_AddRefed<nsIURI> aURI)
  : mParent(aParent)
  , mURI(aURI)
{
}

JSObject*
URL::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return URLBinding::Wrap(aCx, this, aGivenProto);
}

/* static */ already_AddRefed<URL>
URL::Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
                 URL& aBase, ErrorResult& aRv)
{
  return Constructor(aGlobal.GetAsSupports(), aUrl, aBase.GetURI(), aRv);
}

/* static */ already_AddRefed<URL>
URL::Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
                 const Optional<nsAString>& aBase, ErrorResult& aRv)
{
  if (aBase.WasPassed()) {
    return Constructor(aGlobal.GetAsSupports(), aUrl, aBase.Value(), aRv);
  }

  return Constructor(aGlobal.GetAsSupports(), aUrl, nullptr, aRv);
}

/* static */ already_AddRefed<URL>
URL::Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
                 const nsAString& aBase, ErrorResult& aRv)
{
  return Constructor(aGlobal.GetAsSupports(), aUrl, aBase, aRv);
}

/* static */ already_AddRefed<URL>
URL::Constructor(nsISupports* aParent, const nsAString& aUrl,
                 const nsAString& aBase, ErrorResult& aRv)
{
  nsCOMPtr<nsIURI> baseUri;
  nsresult rv = NS_NewURI(getter_AddRefs(baseUri), aBase, nullptr, nullptr,
                          nsContentUtils::GetIOService());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.ThrowTypeError(MSG_INVALID_URL, &aBase);
    return nullptr;
  }

  return Constructor(aParent, aUrl, baseUri, aRv);
}

/* static */
already_AddRefed<URL>
URL::Constructor(nsISupports* aParent, const nsAString& aUrl, nsIURI* aBase,
                 ErrorResult& aRv)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aUrl, nullptr, aBase,
                          nsContentUtils::GetIOService());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.ThrowTypeError(MSG_INVALID_URL, &aUrl);
    return nullptr;
  }

  nsRefPtr<URL> url = new URL(aParent, uri.forget());
  return url.forget();
}

void
URL::CreateObjectURL(const GlobalObject& aGlobal,
                     Blob& aBlob,
                     const objectURLOptions& aOptions,
                     nsAString& aResult,
                     ErrorResult& aError)
{
  CreateObjectURLInternal(aGlobal, aBlob.Impl(),
                          NS_LITERAL_CSTRING(BLOBURI_SCHEME), aOptions, aResult,
                          aError);
}

void
URL::CreateObjectURL(const GlobalObject& aGlobal, DOMMediaStream& aStream,
                     const mozilla::dom::objectURLOptions& aOptions,
                     nsAString& aResult,
                     ErrorResult& aError)
{
  CreateObjectURLInternal(aGlobal, &aStream,
                          NS_LITERAL_CSTRING(MEDIASTREAMURI_SCHEME), aOptions,
                          aResult, aError);
}

void
URL::CreateObjectURL(const GlobalObject& aGlobal, MediaSource& aSource,
                     const objectURLOptions& aOptions,
                     nsAString& aResult,
                     ErrorResult& aError)
{
  nsCOMPtr<nsIPrincipal> principal = nsContentUtils::ObjectPrincipal(aGlobal.Get());

  nsCString url;
  nsresult rv = nsHostObjectProtocolHandler::
    AddDataEntry(NS_LITERAL_CSTRING(MEDIASOURCEURI_SCHEME),
                 &aSource, principal, url);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return;
  }

  nsCOMPtr<nsIRunnable> revocation = NS_NewRunnableFunction(
    [url] {
      nsHostObjectProtocolHandler::RemoveDataEntry(url);
    });

  nsContentUtils::RunInStableState(revocation.forget());

  CopyASCIItoUTF16(url, aResult);
}

void
URL::CreateObjectURLInternal(const GlobalObject& aGlobal, nsISupports* aObject,
                             const nsACString& aScheme,
                             const objectURLOptions& aOptions,
                             nsAString& aResult, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsIPrincipal> principal = nsContentUtils::ObjectPrincipal(aGlobal.Get());

  nsAutoCString url;
  nsresult rv = nsHostObjectProtocolHandler::AddDataEntry(aScheme, aObject,
                                                          principal, url);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  global->RegisterHostObjectURI(url);
  CopyASCIItoUTF16(url, aResult);
}

void
URL::RevokeObjectURL(const GlobalObject& aGlobal, const nsAString& aURL,
                     ErrorResult& aRv)
{
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
    nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
    global->UnregisterHostObjectURI(asciiurl);
    nsHostObjectProtocolHandler::RemoveDataEntry(asciiurl);
  }
}

void
URL::GetHref(nsAString& aHref, ErrorResult& aRv) const
{
  aHref.Truncate();

  nsAutoCString href;
  nsresult rv = mURI->GetSpec(href);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(href, aHref);
  }
}

void
URL::SetHref(const nsAString& aHref, ErrorResult& aRv)
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
    nsAutoString label(aHref);
    aRv.ThrowTypeError(MSG_INVALID_URL, &label);
    return;
  }

  mURI = uri;
  UpdateURLSearchParams();
}

void
URL::GetOrigin(nsAString& aOrigin, ErrorResult& aRv) const
{
  nsContentUtils::GetUTFOrigin(mURI, aOrigin);
}

void
URL::GetProtocol(nsAString& aProtocol, ErrorResult& aRv) const
{
  nsAutoCString protocol;
  if (NS_SUCCEEDED(mURI->GetScheme(protocol))) {
    aProtocol.Truncate();
  }

  CopyASCIItoUTF16(protocol, aProtocol);
  aProtocol.Append(char16_t(':'));
}

void
URL::SetProtocol(const nsAString& aProtocol, ErrorResult& aRv)
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
URL::GetUsername(nsAString& aUsername, ErrorResult& aRv) const
{
  URL_GETTER(aUsername, GetUsername);
}

void
URL::SetUsername(const nsAString& aUsername, ErrorResult& aRv)
{
  mURI->SetUsername(NS_ConvertUTF16toUTF8(aUsername));
}

void
URL::GetPassword(nsAString& aPassword, ErrorResult& aRv) const
{
  URL_GETTER(aPassword, GetPassword);
}

void
URL::SetPassword(const nsAString& aPassword, ErrorResult& aRv)
{
  mURI->SetPassword(NS_ConvertUTF16toUTF8(aPassword));
}

void
URL::GetHost(nsAString& aHost, ErrorResult& aRv) const
{
  URL_GETTER(aHost, GetHostPort);
}

void
URL::SetHost(const nsAString& aHost, ErrorResult& aRv)
{
  mURI->SetHostPort(NS_ConvertUTF16toUTF8(aHost));
}

void
URL::URLSearchParamsUpdated(URLSearchParams* aSearchParams)
{
  MOZ_ASSERT(mSearchParams);
  MOZ_ASSERT(mSearchParams == aSearchParams);

  nsAutoString search;
  mSearchParams->Serialize(search);
  SetSearchInternal(search);
}

void
URL::UpdateURLSearchParams()
{
  if (!mSearchParams) {
    return;
  }

  nsAutoCString search;
  nsCOMPtr<nsIURL> url(do_QueryInterface(mURI));
  if (url) {
    nsresult rv = url->GetQuery(search);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to get the query from a nsIURL.");
    }
  }

  mSearchParams->ParseInput(search);
}

void
URL::GetHostname(nsAString& aHostname, ErrorResult& aRv) const
{
  aHostname.Truncate();
  nsContentUtils::GetHostOrIPv6WithBrackets(mURI, aHostname);
}

void
URL::SetHostname(const nsAString& aHostname, ErrorResult& aRv)
{
  // nsStandardURL returns NS_ERROR_UNEXPECTED for an empty hostname
  // The return code is silently ignored
  mURI->SetHost(NS_ConvertUTF16toUTF8(aHostname));
}

void
URL::GetPort(nsAString& aPort, ErrorResult& aRv) const
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
URL::SetPort(const nsAString& aPort, ErrorResult& aRv)
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
URL::GetPathname(nsAString& aPathname, ErrorResult& aRv) const
{
  aPathname.Truncate();

  nsCOMPtr<nsIURL> url(do_QueryInterface(mURI));
  if (!url) {
    nsAutoCString path;
    nsresult rv = mURI->GetPath(path);
    if (NS_FAILED(rv)){
      // Do not throw!  Not having a valid URI or URL should result in an empty
      // string.
      return;
    }

    CopyUTF8toUTF16(path, aPathname);
    return;
  }

  nsAutoCString file;
  nsresult rv = url->GetFilePath(file);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(file, aPathname);
  }
}

void
URL::SetPathname(const nsAString& aPathname, ErrorResult& aRv)
{
  nsCOMPtr<nsIURL> url(do_QueryInterface(mURI));
  if (!url) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  url->SetFilePath(NS_ConvertUTF16toUTF8(aPathname));
}

void
URL::GetSearch(nsAString& aSearch, ErrorResult& aRv) const
{
  aSearch.Truncate();

  nsCOMPtr<nsIURL> url(do_QueryInterface(mURI));
  if (!url) {
    // Do not throw!  Not having a valid URI or URL should result in an empty
    // string.
    return;
  }

  nsAutoCString search;
  nsresult rv = url->GetQuery(search);
  if (NS_SUCCEEDED(rv) && !search.IsEmpty()) {
    CopyUTF8toUTF16(NS_LITERAL_CSTRING("?") + search, aSearch);
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
  nsCOMPtr<nsIURL> url(do_QueryInterface(mURI));
  if (!url) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  url->SetQuery(NS_ConvertUTF16toUTF8(aSearch));
}

URLSearchParams*
URL::SearchParams()
{
  CreateSearchParamsIfNeeded();
  return mSearchParams;
}

void
URL::GetHash(nsAString& aHash, ErrorResult& aRv) const
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
URL::SetHash(const nsAString& aHash, ErrorResult& aRv)
{
  mURI->SetRef(NS_ConvertUTF16toUTF8(aHash));
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

} // namespace dom
} // namespace mozilla
