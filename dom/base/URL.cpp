/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URL.h"

#include "nsGlobalWindow.h"
#include "nsDOMFile.h"
#include "DOMMediaStream.h"
#include "mozilla/dom/MediaSource.h"
#include "mozilla/dom/URLBinding.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsServiceManagerUtils.h"
#include "nsIIOService.h"
#include "nsEscape.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIURL.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(URL)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(URL)
  if (tmp->mSearchParams) {
    tmp->mSearchParams->RemoveObserver(tmp);
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mSearchParams)
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(URL)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSearchParams)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(URL)
NS_IMPL_CYCLE_COLLECTING_RELEASE(URL)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(URL)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

URL::URL(nsIURI* aURI)
  : mURI(aURI)
{
}

JSObject*
URL::WrapObject(JSContext* aCx)
{
  return URLBinding::Wrap(aCx, this);
}

/* static */ already_AddRefed<URL>
URL::Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
                 URL& aBase, ErrorResult& aRv)
{
  nsresult rv;
  nsCOMPtr<nsIIOService> ioService(do_GetService(NS_IOSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri;
  rv = ioService->NewURI(NS_ConvertUTF16toUTF8(aUrl), nullptr, aBase.GetURI(),
                         getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    nsAutoString label(aUrl);
    aRv.ThrowTypeError(MSG_INVALID_URL, &label);
    return nullptr;
  }

  nsRefPtr<URL> url = new URL(uri);
  return url.forget();
}

/* static */ already_AddRefed<URL>
URL::Constructor(const GlobalObject& aGlobal, const nsAString& aUrl,
                  const nsAString& aBase, ErrorResult& aRv)
{
  nsresult rv;
  nsCOMPtr<nsIIOService> ioService(do_GetService(NS_IOSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  nsCOMPtr<nsIURI> baseUri;
  rv = ioService->NewURI(NS_ConvertUTF16toUTF8(aBase), nullptr, nullptr,
                         getter_AddRefs(baseUri));
  if (NS_FAILED(rv)) {
    nsAutoString label(aBase);
    aRv.ThrowTypeError(MSG_INVALID_URL, &label);
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri;
  rv = ioService->NewURI(NS_ConvertUTF16toUTF8(aUrl), nullptr, baseUri,
                         getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    nsAutoString label(aUrl);
    aRv.ThrowTypeError(MSG_INVALID_URL, &label);
    return nullptr;
  }

  nsRefPtr<URL> url = new URL(uri);
  return url.forget();
}

void
URL::CreateObjectURL(const GlobalObject& aGlobal,
                     nsIDOMBlob* aBlob,
                     const objectURLOptions& aOptions,
                     nsString& aResult,
                     ErrorResult& aError)
{
  DOMFile* blob = static_cast<DOMFile*>(aBlob);
  MOZ_ASSERT(blob);

  CreateObjectURLInternal(aGlobal, blob->Impl(),
                          NS_LITERAL_CSTRING(BLOBURI_SCHEME), aOptions, aResult,
                          aError);
}

void
URL::CreateObjectURL(const GlobalObject& aGlobal, DOMMediaStream& aStream,
                     const mozilla::dom::objectURLOptions& aOptions,
                     nsString& aResult,
                     ErrorResult& aError)
{
  CreateObjectURLInternal(aGlobal, &aStream,
                          NS_LITERAL_CSTRING(MEDIASTREAMURI_SCHEME), aOptions,
                          aResult, aError);
}

void
URL::CreateObjectURL(const GlobalObject& aGlobal, MediaSource& aSource,
                     const objectURLOptions& aOptions,
                     nsString& aResult,
                     ErrorResult& aError)
{
  CreateObjectURLInternal(aGlobal, &aSource,
                          NS_LITERAL_CSTRING(MEDIASOURCEURI_SCHEME), aOptions,
                          aResult, aError);
}

void
URL::CreateObjectURLInternal(const GlobalObject& aGlobal, nsISupports* aObject,
                             const nsACString& aScheme,
                             const objectURLOptions& aOptions,
                             nsString& aResult, ErrorResult& aError)
{
  nsCOMPtr<nsIPrincipal> principal = nsContentUtils::ObjectPrincipal(aGlobal.Get());

  nsCString url;
  nsresult rv = nsHostObjectProtocolHandler::AddDataEntry(aScheme, aObject,
                                                          principal, url);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return;
  }

  nsCOMPtr<nsPIDOMWindow> w = do_QueryInterface(aGlobal.GetAsSupports());
  nsGlobalWindow* window = static_cast<nsGlobalWindow*>(w.get());

  if (window) {
    NS_PRECONDITION(window->IsInnerWindow(), "Should be inner window");

    if (!window->GetExtantDoc()) {
      aError.Throw(NS_ERROR_INVALID_POINTER);
      return;
    }

    nsIDocument* doc = window->GetExtantDoc();
    if (doc) {
      doc->RegisterHostObjectUri(url);
    }
  }

  CopyASCIItoUTF16(url, aResult);
}

void
URL::RevokeObjectURL(const GlobalObject& aGlobal, const nsAString& aURL)
{
  nsIPrincipal* principal = nsContentUtils::ObjectPrincipal(aGlobal.Get());

  NS_LossyConvertUTF16toASCII asciiurl(aURL);

  nsIPrincipal* urlPrincipal =
    nsHostObjectProtocolHandler::GetDataEntryPrincipal(asciiurl);

  if (urlPrincipal && principal->Subsumes(urlPrincipal)) {
    nsCOMPtr<nsPIDOMWindow> w = do_QueryInterface(aGlobal.GetAsSupports());
    nsGlobalWindow* window = static_cast<nsGlobalWindow*>(w.get());

    if (window && window->GetExtantDoc()) {
      window->GetExtantDoc()->UnregisterHostObjectUri(asciiurl);
    }
    nsHostObjectProtocolHandler::RemoveDataEntry(asciiurl);
  }
}

void
URL::GetHref(nsString& aHref) const
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
  nsCString href = NS_ConvertUTF16toUTF8(aHref);

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
URL::GetOrigin(nsString& aOrigin) const
{
  nsContentUtils::GetUTFNonNullOrigin(mURI, aOrigin);
}

void
URL::GetProtocol(nsString& aProtocol) const
{
  nsCString protocol;
  if (NS_SUCCEEDED(mURI->GetScheme(protocol))) {
    aProtocol.Truncate();
  }

  CopyASCIItoUTF16(protocol, aProtocol);
  aProtocol.Append(char16_t(':'));
}

void
URL::SetProtocol(const nsAString& aProtocol)
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
URL::GetUsername(nsString& aUsername) const
{
  URL_GETTER(aUsername, GetUsername);
}

void
URL::SetUsername(const nsAString& aUsername)
{
  mURI->SetUsername(NS_ConvertUTF16toUTF8(aUsername));
}

void
URL::GetPassword(nsString& aPassword) const
{
  URL_GETTER(aPassword, GetPassword);
}

void
URL::SetPassword(const nsAString& aPassword)
{
  mURI->SetPassword(NS_ConvertUTF16toUTF8(aPassword));
}

void
URL::GetHost(nsString& aHost) const
{
  URL_GETTER(aHost, GetHostPort);
}

void
URL::SetHost(const nsAString& aHost)
{
  mURI->SetHostPort(NS_ConvertUTF16toUTF8(aHost));
}

void
URL::URLSearchParamsUpdated()
{
  MOZ_ASSERT(mSearchParams);

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

  mSearchParams->ParseInput(search, this);
}

void
URL::GetHostname(nsString& aHostname) const
{
  aHostname.Truncate();
  nsAutoCString tmp;
  nsresult rv = mURI->GetHost(tmp);
  if (NS_SUCCEEDED(rv)) {
    if (tmp.FindChar(':') != -1) { // Escape IPv6 address
      MOZ_ASSERT(!tmp.Length() ||
        (tmp[0] !='[' && tmp[tmp.Length() - 1] != ']'));
      tmp.Insert('[', 0);
      tmp.Append(']');
    }
    CopyUTF8toUTF16(tmp, aHostname);
  }
}

void
URL::SetHostname(const nsAString& aHostname)
{
  // nsStandardURL returns NS_ERROR_UNEXPECTED for an empty hostname
  // The return code is silently ignored
  mURI->SetHost(NS_ConvertUTF16toUTF8(aHostname));
}

void
URL::GetPort(nsString& aPort) const
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
URL::SetPort(const nsAString& aPort)
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
URL::GetPathname(nsString& aPathname) const
{
  aPathname.Truncate();

  nsCOMPtr<nsIURL> url(do_QueryInterface(mURI));
  if (!url) {
    // Do not throw!  Not having a valid URI or URL should result in an empty
    // string.
    return;
  }

  nsAutoCString file;
  nsresult rv = url->GetFilePath(file);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(file, aPathname);
  }
}

void
URL::SetPathname(const nsAString& aPathname)
{
  nsCOMPtr<nsIURL> url(do_QueryInterface(mURI));
  if (!url) {
    // Ignore failures to be compatible with NS4.
    return;
  }

  url->SetFilePath(NS_ConvertUTF16toUTF8(aPathname));
}

void
URL::GetSearch(nsString& aSearch) const
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
URL::SetSearch(const nsAString& aSearch)
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
URL::SetSearchParams(URLSearchParams& aSearchParams)
{
  if (mSearchParams) {
    mSearchParams->RemoveObserver(this);
  }

  // the observer will be cleared using the cycle collector.
  mSearchParams = &aSearchParams;
  mSearchParams->AddObserver(this);

  nsAutoString search;
  mSearchParams->Serialize(search);
  SetSearchInternal(search);
}

void
URL::GetHash(nsString& aHash) const
{
  aHash.Truncate();

  nsAutoCString ref;
  nsresult rv = mURI->GetRef(ref);
  if (NS_SUCCEEDED(rv) && !ref.IsEmpty()) {
    NS_UnescapeURL(ref); // XXX may result in random non-ASCII bytes!
    aHash.Assign(char16_t('#'));
    AppendUTF8toUTF16(ref, aHash);
  }
}

void
URL::SetHash(const nsAString& aHash)
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
    mSearchParams = new URLSearchParams();
    mSearchParams->AddObserver(this);
    UpdateURLSearchParams();
  }
}

}
}
