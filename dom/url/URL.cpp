/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URL.h"
#include "URLMainThread.h"
#include "URLWorker.h"

#include "nsASCIIMask.h"
#include "MainThreadUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/URLBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Document.h"
#include "nsIURIMutator.h"
#include "nsNetUtil.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(URL, mParent, mSearchParams)

NS_IMPL_CYCLE_COLLECTING_ADDREF(URL)
NS_IMPL_CYCLE_COLLECTING_RELEASE(URL)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(URL)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* URL::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return URL_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<URL> URL::Constructor(const GlobalObject& aGlobal,
                                       const nsACString& aURL,
                                       const Optional<nsACString>& aBase,
                                       ErrorResult& aRv) {
  if (aBase.WasPassed()) {
    return Constructor(aGlobal.GetAsSupports(), aURL, aBase.Value(), aRv);
  }

  return Constructor(aGlobal.GetAsSupports(), aURL, nullptr, aRv);
}

/* static */
already_AddRefed<URL> URL::Constructor(nsISupports* aParent,
                                       const nsACString& aURL,
                                       const nsACString& aBase,
                                       ErrorResult& aRv) {
  nsCOMPtr<nsIURI> baseUri;
  nsresult rv = NS_NewURI(getter_AddRefs(baseUri), aBase);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(aBase);
    return nullptr;
  }

  return Constructor(aParent, aURL, baseUri, aRv);
}

/* static */
already_AddRefed<URL> URL::Constructor(nsISupports* aParent,
                                       const nsACString& aURL, nsIURI* aBase,
                                       ErrorResult& aRv) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL, nullptr, aBase);
  if (NS_FAILED(rv)) {
    // No need to warn in this case. It's common to use the URL constructor
    // to determine if a URL is valid and an exception will be propagated.
    aRv.ThrowTypeError<MSG_INVALID_URL>(aURL);
    return nullptr;
  }

  return MakeAndAddRef<URL>(aParent, std::move(uri));
}

already_AddRefed<URL> URL::FromURI(GlobalObject& aGlobal, nsIURI* aURI) {
  return MakeAndAddRef<URL>(aGlobal.GetAsSupports(), aURI);
}

void URL::CreateObjectURL(const GlobalObject& aGlobal, Blob& aBlob,
                          nsACString& aResult, ErrorResult& aRv) {
  if (NS_IsMainThread()) {
    URLMainThread::CreateObjectURL(aGlobal, aBlob, aResult, aRv);
  } else {
    URLWorker::CreateObjectURL(aGlobal, aBlob, aResult, aRv);
  }
}

void URL::CreateObjectURL(const GlobalObject& aGlobal, MediaSource& aSource,
                          nsACString& aResult, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  URLMainThread::CreateObjectURL(aGlobal, aSource, aResult, aRv);
}

void URL::RevokeObjectURL(const GlobalObject& aGlobal, const nsACString& aURL,
                          ErrorResult& aRv) {
  if (aURL.Contains('#')) {
    // Don't revoke URLs that contain fragments.
    return;
  }

  if (NS_IsMainThread()) {
    URLMainThread::RevokeObjectURL(aGlobal, aURL, aRv);
  } else {
    URLWorker::RevokeObjectURL(aGlobal, aURL, aRv);
  }
}

bool URL::IsValidObjectURL(const GlobalObject& aGlobal, const nsACString& aURL,
                           ErrorResult& aRv) {
  if (NS_IsMainThread()) {
    return URLMainThread::IsValidObjectURL(aGlobal, aURL, aRv);
  }
  return URLWorker::IsValidObjectURL(aGlobal, aURL, aRv);
}

already_AddRefed<nsIURI> URL::ParseURI(const nsACString& aURL,
                                       const Optional<nsACString>& aBase) {
  nsCOMPtr<nsIURI> baseUri;
  nsCOMPtr<nsIURI> uri;

  if (aBase.WasPassed()) {
    nsresult rv = NS_NewURI(getter_AddRefs(baseUri), aBase.Value());
    if (NS_FAILED(rv)) {
      return nullptr;
    }
  }

  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL, nullptr, baseUri);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return uri.forget();
};

already_AddRefed<URL> URL::Parse(const GlobalObject& aGlobal,
                                 const nsACString& aURL,
                                 const Optional<nsACString>& aBase) {
  nsCOMPtr<nsIURI> uri = ParseURI(aURL, aBase);
  if (!uri) {
    return nullptr;
  }
  return MakeAndAddRef<URL>(aGlobal.GetAsSupports(), std::move(uri));
}

bool URL::CanParse(const GlobalObject& aGlobal, const nsACString& aURL,
                   const Optional<nsACString>& aBase) {
  nsCOMPtr<nsIURI> uri = ParseURI(aURL, aBase);
  return !!uri;
}

URLSearchParams* URL::SearchParams() {
  CreateSearchParamsIfNeeded();
  return mSearchParams;
}

bool IsChromeURI(nsIURI* aURI) { return aURI->SchemeIs("chrome"); }

void URL::CreateSearchParamsIfNeeded() {
  if (!mSearchParams) {
    mSearchParams = new URLSearchParams(mParent, this);
    UpdateURLSearchParams();
  }
}

void URL::SetSearch(const nsACString& aSearch) {
  SetSearchInternal(aSearch);
  UpdateURLSearchParams();
}

void URL::URLSearchParamsUpdated(URLSearchParams* aSearchParams) {
  MOZ_ASSERT(mSearchParams);
  MOZ_ASSERT(mSearchParams == aSearchParams);

  nsAutoCString search;
  mSearchParams->Serialize(search);
  SetSearchInternal(search);
}

#define URL_GETTER(value, func) \
  MOZ_ASSERT(mURI);             \
  mURI->func(value);

void URL::GetHref(nsACString& aHref) const { URL_GETTER(aHref, GetSpec); }

void URL::SetHref(const nsACString& aHref, ErrorResult& aRv) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aHref);
  if (NS_FAILED(rv)) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(aHref);
    return;
  }

  mURI = std::move(uri);
  UpdateURLSearchParams();
}

void URL::GetOrigin(nsACString& aOrigin) const {
  nsresult rv =
      nsContentUtils::GetWebExposedOriginSerialization(URI(), aOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aOrigin.Truncate();
  }
}

void URL::GetProtocol(nsACString& aProtocol) const {
  URL_GETTER(aProtocol, GetScheme);
  aProtocol.Append(char16_t(':'));
}

void URL::SetProtocol(const nsACString& aProtocol) {
  nsCOMPtr<nsIURI> uri(URI());
  if (!uri) {
    return;
  }
  uri = net::TryChangeProtocol(uri, aProtocol);
  if (!uri) {
    return;
  }
  mURI = std::move(uri);
}

void URL::GetUsername(nsACString& aUsername) const {
  URL_GETTER(aUsername, GetUsername);
}

void URL::SetUsername(const nsACString& aUsername) {
  MOZ_ASSERT(mURI);
  Unused << NS_MutateURI(mURI).SetUsername(aUsername).Finalize(mURI);
}

void URL::GetPassword(nsACString& aPassword) const {
  URL_GETTER(aPassword, GetPassword);
}

void URL::SetPassword(const nsACString& aPassword) {
  MOZ_ASSERT(mURI);

  Unused << NS_MutateURI(mURI).SetPassword(aPassword).Finalize(mURI);
}

void URL::GetHost(nsACString& aHost) const { URL_GETTER(aHost, GetHostPort); }

void URL::SetHost(const nsACString& aHost) {
  MOZ_ASSERT(mURI);
  Unused << NS_MutateURI(mURI).SetHostPort(aHost).Finalize(mURI);
}

void URL::GetHostname(nsACString& aHostname) const {
  MOZ_ASSERT(mURI);
  aHostname.Truncate();
  nsContentUtils::GetHostOrIPv6WithBrackets(mURI, aHostname);
}

void URL::SetHostname(const nsACString& aHostname) {
  MOZ_ASSERT(mURI);

  // nsStandardURL returns NS_ERROR_UNEXPECTED for an empty hostname
  // The return code is silently ignored
  Unused << NS_MutateURI(mURI).SetHost(aHostname).Finalize(mURI);
}

void URL::GetPort(nsACString& aPort) const {
  MOZ_ASSERT(mURI);
  aPort.Truncate();

  int32_t port;
  nsresult rv = mURI->GetPort(&port);
  if (NS_SUCCEEDED(rv) && port != -1) {
    aPort.AppendInt(port, 10);
  }
}

void URL::SetPort(const nsACString& aPort) {
  nsresult rv;
  nsAutoCString portStr(aPort);
  int32_t port = -1;

  // nsIURI uses -1 as default value.
  portStr.StripTaggedASCII(ASCIIMask::MaskCRLFTab());
  if (!portStr.IsEmpty()) {
    // To be valid, the port must start with an ASCII digit.
    // (nsACString::ToInteger ignores leading junk, so check before calling.)
    if (!IsAsciiDigit(portStr[0])) {
      return;
    }
    port = portStr.ToInteger(&rv);
    if (NS_FAILED(rv)) {
      return;
    }
  }

  Unused << NS_MutateURI(mURI).SetPort(port).Finalize(mURI);
}

void URL::GetPathname(nsACString& aPathname) const {
  MOZ_ASSERT(mURI);
  // Do not throw!  Not having a valid URI or URL should result in an empty
  // string.
  mURI->GetFilePath(aPathname);
}

void URL::SetPathname(const nsACString& aPathname) {
  MOZ_ASSERT(mURI);

  // Do not throw!
  Unused << NS_MutateURI(mURI).SetFilePath(aPathname).Finalize(mURI);
}

void URL::GetSearch(nsACString& aSearch) const {
  MOZ_ASSERT(mURI);

  aSearch.Truncate();

  // Do not throw!  Not having a valid URI or URL should result in an empty
  // string.

  nsresult rv;
  rv = mURI->GetQuery(aSearch);
  if (NS_SUCCEEDED(rv) && !aSearch.IsEmpty()) {
    aSearch.Insert('?', 0);
  }
}

void URL::GetHash(nsACString& aHash) const {
  MOZ_ASSERT(mURI);
  aHash.Truncate();
  nsresult rv = mURI->GetRef(aHash);
  if (NS_SUCCEEDED(rv) && !aHash.IsEmpty()) {
    aHash.Insert('#', 0);
  }
}

void URL::SetHash(const nsACString& aHash) {
  MOZ_ASSERT(mURI);

  Unused << NS_MutateURI(mURI).SetRef(aHash).Finalize(mURI);
}

void URL::SetSearchInternal(const nsACString& aSearch) {
  MOZ_ASSERT(mURI);

  // Ignore failures to be compatible with NS4.
  Unused << NS_MutateURI(mURI).SetQuery(aSearch).Finalize(mURI);
}

void URL::UpdateURLSearchParams() {
  if (!mSearchParams) {
    return;
  }

  nsAutoCString search;
  nsresult rv = URI()->GetQuery(search);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    search.Truncate();
  }

  mSearchParams->ParseInput(search);
}

nsIURI* URL::URI() const {
  MOZ_ASSERT(mURI);
  return mURI;
}

}  // namespace mozilla::dom
