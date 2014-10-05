/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Response.h"
#include "nsDOMString.h"
#include "nsPIDOMWindow.h"
#include "nsIURI.h"
#include "nsISupportsImpl.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/Promise.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(Response)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Response)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Response, mOwner)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Response)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Response::Response(nsISupports* aOwner)
  : mOwner(aOwner)
  , mHeaders(new Headers(aOwner))
{
  SetIsDOMBinding();
}

Response::~Response()
{
}

/* static */ already_AddRefed<Response>
Response::Error(const GlobalObject& aGlobal)
{
  ErrorResult result;
  ResponseInit init;
  init.mStatus = 0;
  Optional<ArrayBufferOrArrayBufferViewOrScalarValueStringOrURLSearchParams> body;
  nsRefPtr<Response> r = Response::Constructor(aGlobal, body, init, result);
  return r.forget();
}

/* static */ already_AddRefed<Response>
Response::Redirect(const GlobalObject& aGlobal, const nsAString& aUrl,
                   uint16_t aStatus)
{
  ErrorResult result;
  ResponseInit init;
  Optional<ArrayBufferOrArrayBufferViewOrScalarValueStringOrURLSearchParams> body;
  nsRefPtr<Response> r = Response::Constructor(aGlobal, body, init, result);
  return r.forget();
}

/*static*/ already_AddRefed<Response>
Response::Constructor(const GlobalObject& global,
                      const Optional<ArrayBufferOrArrayBufferViewOrScalarValueStringOrURLSearchParams>& aBody,
                      const ResponseInit& aInit, ErrorResult& rv)
{
  nsRefPtr<Response> response = new Response(global.GetAsSupports());
  return response.forget();
}

already_AddRefed<Response>
Response::Clone()
{
  nsRefPtr<Response> response = new Response(mOwner);
  return response.forget();
}

already_AddRefed<Promise>
Response::ArrayBuffer(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  MOZ_ASSERT(global);
  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
  return promise.forget();
}

already_AddRefed<Promise>
Response::Blob(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  MOZ_ASSERT(global);
  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
  return promise.forget();
}

already_AddRefed<Promise>
Response::Json(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  MOZ_ASSERT(global);
  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
  return promise.forget();
}

already_AddRefed<Promise>
Response::Text(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  MOZ_ASSERT(global);
  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
  return promise.forget();
}

bool
Response::BodyUsed()
{
  return false;
}
} // namespace dom
} // namespace mozilla
