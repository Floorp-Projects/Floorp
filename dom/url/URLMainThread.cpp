/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URLMainThread.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Blob.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h"
#include "nsIURL.h"
#include "nsIURIMutator.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

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

  RefPtr<URLMainThread> url = new URLMainThread(aParent);
  url->SetURI(uri.forget());
  return url.forget();
}

/* static */ void
URLMainThread::CreateObjectURL(const GlobalObject& aGlobal, Blob& aBlob,
                               nsAString& aResult, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsIPrincipal> principal =
    nsContentUtils::ObjectPrincipal(aGlobal.Get());

  nsAutoCString url;
  aRv = BlobURLProtocolHandler::AddDataEntry(aBlob.Impl(), principal, url);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  global->RegisterHostObjectURI(url);
  CopyASCIItoUTF16(url, aResult);
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
  aRv = BlobURLProtocolHandler::AddDataEntry(&aSource, principal, url);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsCOMPtr<nsIRunnable> revocation =
    NS_NewRunnableFunction("dom::URLMainThread::CreateObjectURL", [url] {
      BlobURLProtocolHandler::RemoveDataEntry(url);
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
    BlobURLProtocolHandler::GetDataEntryPrincipal(asciiurl);

  if (urlPrincipal && principal->Subsumes(urlPrincipal)) {
    global->UnregisterHostObjectURI(asciiurl);
    BlobURLProtocolHandler::RemoveDataEntry(asciiurl);
  }
}

URLMainThread::URLMainThread(nsISupports* aParent)
  : URL(aParent)
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
  return BlobURLProtocolHandler::HasDataEntry(asciiurl);
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

  SetURI(uri.forget());
  UpdateURLSearchParams();
}

void
URLMainThread::GetOrigin(nsAString& aOrigin, ErrorResult& aRv) const
{
  nsContentUtils::GetUTFOrigin(GetURI(), aOrigin);
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

  SetURI(uri.forget());
}

} // namespace dom
} // namespace mozilla
