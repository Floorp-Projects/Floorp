/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URL.h"
#include "URLMainThread.h"
#include "URLWorker.h"

#include "MainThreadUtils.h"
#include "mozilla/dom/URLBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Document.h"
#include "nsIURIMutator.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

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
                                       const nsAString& aURL,
                                       const Optional<nsAString>& aBase,
                                       ErrorResult& aRv) {
  if (aBase.WasPassed()) {
    return Constructor(aGlobal.GetAsSupports(), aURL, aBase.Value(), aRv);
  }

  return Constructor(aGlobal.GetAsSupports(), aURL, nullptr, aRv);
}

/* static */
already_AddRefed<URL> URL::Constructor(nsISupports* aParent,
                                       const nsAString& aURL,
                                       const nsAString& aBase,
                                       ErrorResult& aRv) {
  // Don't use NS_ConvertUTF16toUTF8 because that doesn't let us handle OOM.
  nsAutoCString base;
  if (!AppendUTF16toUTF8(aBase, base, fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  nsCOMPtr<nsIURI> baseUri;
  nsresult rv = NS_NewURI(getter_AddRefs(baseUri), base);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(base);
    return nullptr;
  }

  return Constructor(aParent, aURL, baseUri, aRv);
}

/* static */
already_AddRefed<URL> URL::Constructor(nsISupports* aParent,
                                       const nsAString& aURL, nsIURI* aBase,
                                       ErrorResult& aRv) {
  // Don't use NS_ConvertUTF16toUTF8 because that doesn't let us handle OOM.
  nsAutoCString urlStr;
  if (!AppendUTF16toUTF8(aURL, urlStr, fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), urlStr, nullptr, aBase);
  if (NS_FAILED(rv)) {
    // No need to warn in this case. It's common to use the URL constructor
    // to determine if a URL is valid and an exception will be propagated.
    aRv.ThrowTypeError<MSG_INVALID_URL>(urlStr);
    return nullptr;
  }

  RefPtr<URL> url = new URL(aParent);
  url->SetURI(uri.forget());
  return url.forget();
}

void URL::CreateObjectURL(const GlobalObject& aGlobal, Blob& aBlob,
                          nsAString& aResult, ErrorResult& aRv) {
  if (NS_IsMainThread()) {
    URLMainThread::CreateObjectURL(aGlobal, aBlob, aResult, aRv);
  } else {
    URLWorker::CreateObjectURL(aGlobal, aBlob, aResult, aRv);
  }
}

void URL::CreateObjectURL(const GlobalObject& aGlobal, MediaSource& aSource,
                          nsAString& aResult, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  URLMainThread::CreateObjectURL(aGlobal, aSource, aResult, aRv);
}

void URL::RevokeObjectURL(const GlobalObject& aGlobal, const nsAString& aURL,
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

bool URL::IsValidURL(const GlobalObject& aGlobal, const nsAString& aURL,
                     ErrorResult& aRv) {
  if (NS_IsMainThread()) {
    return URLMainThread::IsValidURL(aGlobal, aURL, aRv);
  }
  return URLWorker::IsValidURL(aGlobal, aURL, aRv);
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

void URL::SetSearch(const nsAString& aSearch) {
  SetSearchInternal(aSearch);
  UpdateURLSearchParams();
}

void URL::URLSearchParamsUpdated(URLSearchParams* aSearchParams) {
  MOZ_ASSERT(mSearchParams);
  MOZ_ASSERT(mSearchParams == aSearchParams);

  nsAutoString search;
  mSearchParams->Serialize(search);

  SetSearchInternal(search);
}

#define URL_GETTER(value, func)  \
  MOZ_ASSERT(mURI);              \
  value.Truncate();              \
  nsAutoCString tmp;             \
  nsresult rv = mURI->func(tmp); \
  if (NS_SUCCEEDED(rv)) {        \
    CopyUTF8toUTF16(tmp, value); \
  }

void URL::GetHref(nsAString& aHref) const { URL_GETTER(aHref, GetSpec); }

void URL::SetHref(const nsAString& aHref, ErrorResult& aRv) {
  // Don't use NS_ConvertUTF16toUTF8 because that doesn't let us handle OOM.
  nsAutoCString href;
  if (!AppendUTF16toUTF8(aHref, href, fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), href);
  if (NS_FAILED(rv)) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(href);
    return;
  }

  mURI = std::move(uri);
  UpdateURLSearchParams();
}

void URL::GetOrigin(nsAString& aOrigin, ErrorResult& aRv) const {
  nsresult rv = nsContentUtils::GetUTFOrigin(GetURI(), aOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aOrigin.Truncate();
  }
}

void URL::GetProtocol(nsAString& aProtocol) const {
  URL_GETTER(aProtocol, GetScheme);
  aProtocol.Append(char16_t(':'));
}

void URL::SetProtocol(const nsAString& aProtocol, ErrorResult& aRv) {
  nsAString::const_iterator start;
  aProtocol.BeginReading(start);

  nsAString::const_iterator end;
  aProtocol.EndReading(end);

  nsAString::const_iterator iter(start);
  FindCharInReadable(':', iter, end);

  // Changing the protocol of a URL, changes the "nature" of the URI
  // implementation. In order to do this properly, we have to serialize the
  // existing URL and reparse it in a new object.
  nsCOMPtr<nsIURI> clone;
  nsresult rv = NS_MutateURI(GetURI())
                    .SetScheme(NS_ConvertUTF16toUTF8(Substring(start, iter)))
                    .Finalize(clone);
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

  mURI = std::move(uri);
}

void URL::GetUsername(nsAString& aUsername) const {
  URL_GETTER(aUsername, GetUsername);
}

void URL::SetUsername(const nsAString& aUsername) {
  MOZ_ASSERT(mURI);

  Unused << NS_MutateURI(mURI)
                .SetUsername(NS_ConvertUTF16toUTF8(aUsername))
                .Finalize(mURI);
}

void URL::GetPassword(nsAString& aPassword) const {
  URL_GETTER(aPassword, GetPassword);
}

void URL::SetPassword(const nsAString& aPassword) {
  MOZ_ASSERT(mURI);

  Unused << NS_MutateURI(mURI)
                .SetPassword(NS_ConvertUTF16toUTF8(aPassword))
                .Finalize(mURI);
}

void URL::GetHost(nsAString& aHost) const { URL_GETTER(aHost, GetHostPort); }

void URL::SetHost(const nsAString& aHost) {
  MOZ_ASSERT(mURI);

  Unused << NS_MutateURI(mURI)
                .SetHostPort(NS_ConvertUTF16toUTF8(aHost))
                .Finalize(mURI);
}

void URL::GetHostname(nsAString& aHostname) const {
  MOZ_ASSERT(mURI);

  aHostname.Truncate();
  nsContentUtils::GetHostOrIPv6WithBrackets(mURI, aHostname);
}

void URL::SetHostname(const nsAString& aHostname) {
  MOZ_ASSERT(mURI);

  // nsStandardURL returns NS_ERROR_UNEXPECTED for an empty hostname
  // The return code is silently ignored
  mozilla::Unused << NS_MutateURI(mURI)
                         .SetHost(NS_ConvertUTF16toUTF8(aHostname))
                         .Finalize(mURI);
}

void URL::GetPort(nsAString& aPort) const {
  MOZ_ASSERT(mURI);

  aPort.Truncate();

  int32_t port;
  nsresult rv = mURI->GetPort(&port);
  if (NS_SUCCEEDED(rv) && port != -1) {
    nsAutoString portStr;
    portStr.AppendInt(port, 10);
    aPort.Assign(portStr);
  }
}

void URL::SetPort(const nsAString& aPort) {
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

  Unused << NS_MutateURI(mURI).SetPort(port).Finalize(mURI);
}

void URL::GetPathname(nsAString& aPathname) const {
  MOZ_ASSERT(mURI);

  aPathname.Truncate();

  // Do not throw!  Not having a valid URI or URL should result in an empty
  // string.

  nsAutoCString file;
  nsresult rv = mURI->GetFilePath(file);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(file, aPathname);
  }
}

void URL::SetPathname(const nsAString& aPathname) {
  MOZ_ASSERT(mURI);

  // Do not throw!

  Unused << NS_MutateURI(mURI)
                .SetFilePath(NS_ConvertUTF16toUTF8(aPathname))
                .Finalize(mURI);
}

void URL::GetSearch(nsAString& aSearch) const {
  MOZ_ASSERT(mURI);

  aSearch.Truncate();

  // Do not throw!  Not having a valid URI or URL should result in an empty
  // string.

  nsAutoCString search;
  nsresult rv;

  rv = mURI->GetQuery(search);
  if (NS_SUCCEEDED(rv) && !search.IsEmpty()) {
    aSearch.Assign(u'?');
    AppendUTF8toUTF16(search, aSearch);
  }
}

void URL::GetHash(nsAString& aHash) const {
  MOZ_ASSERT(mURI);

  aHash.Truncate();

  nsAutoCString ref;
  nsresult rv = mURI->GetRef(ref);
  if (NS_SUCCEEDED(rv) && !ref.IsEmpty()) {
    aHash.Assign(char16_t('#'));
    AppendUTF8toUTF16(ref, aHash);
  }
}

void URL::SetHash(const nsAString& aHash) {
  MOZ_ASSERT(mURI);

  Unused
      << NS_MutateURI(mURI).SetRef(NS_ConvertUTF16toUTF8(aHash)).Finalize(mURI);
}

void URL::SetSearchInternal(const nsAString& aSearch) {
  MOZ_ASSERT(mURI);

  // Ignore failures to be compatible with NS4.

  Unused << NS_MutateURI(mURI)
                .SetQuery(NS_ConvertUTF16toUTF8(aSearch))
                .Finalize(mURI);
}

void URL::UpdateURLSearchParams() {
  if (!mSearchParams) {
    return;
  }

  nsAutoCString search;
  nsresult rv = GetURI()->GetQuery(search);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    search.Truncate();
  }

  mSearchParams->ParseInput(search);
}

void URL::SetURI(already_AddRefed<nsIURI> aURI) {
  mURI = std::move(aURI);
  MOZ_ASSERT(mURI);
}

nsIURI* URL::GetURI() const {
  MOZ_ASSERT(mURI);
  return mURI;
}

}  // namespace dom
}  // namespace mozilla
