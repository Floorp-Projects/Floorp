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
#include "nsIDocument.h"

namespace mozilla {
namespace dom {

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
                     nsAString& aResult, ErrorResult& aRv)
{
  if (NS_IsMainThread()) {
    URLMainThread::CreateObjectURL(aGlobal, aBlob, aResult, aRv);
  } else {
    URLWorker::CreateObjectURL(aGlobal, aBlob, aResult, aRv);
  }
}

void
URL::CreateObjectURL(const GlobalObject& aGlobal, DOMMediaStream& aStream,
                     nsAString& aResult, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  DeprecationWarning(aGlobal, nsIDocument::eURLCreateObjectURL_MediaStream);

  URLMainThread::CreateObjectURL(aGlobal, aStream, aResult, aRv);
}

void
URL::CreateObjectURL(const GlobalObject& aGlobal, MediaSource& aSource,
                     nsAString& aResult, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  URLMainThread::CreateObjectURL(aGlobal, aSource, aResult, aRv);
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
URL::SetSearch(const nsAString& aSearch)
{
  SetSearchInternal(aSearch);
  UpdateURLSearchParams();
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

} // namespace dom
} // namespace mozilla
