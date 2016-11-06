/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Worklet.h"
#include "mozilla/dom/WorkletBinding.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/Response.h"

namespace mozilla {
namespace dom {

namespace {

class WorkletFetchHandler : public PromiseNativeHandler
{
public:
  NS_DECL_ISUPPORTS

  static already_AddRefed<Promise>
  Fetch(Worklet* aWorklet, const nsAString& aModuleURL, ErrorResult& aRv)
  {
    RefPtr<Promise> promise = Promise::Create(aWorklet->GetParentObject(), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    RequestOrUSVString request;
    request.SetAsUSVString().Rebind(aModuleURL.Data(), aModuleURL.Length());

    RequestInit init;

    RefPtr<Promise> fetchPromise =
      FetchRequest(aWorklet->GetParentObject(), request, init, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      promise->MaybeReject(aRv);
      return promise.forget();
    }

    RefPtr<WorkletFetchHandler> handler =
      new WorkletFetchHandler(aWorklet, promise);
    fetchPromise->AppendNativeHandler(handler);

    return promise.forget();
  }

  virtual void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    if (!aValue.isObject()) {
      mPromise->MaybeReject(NS_ERROR_FAILURE);
      return;
    }

    RefPtr<Response> response;
    nsresult rv = UNWRAP_OBJECT(Response, &aValue.toObject(), response);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPromise->MaybeReject(rv);
      return;
    }

    if (!response->Ok()) {
      mPromise->MaybeReject(NS_ERROR_DOM_NETWORK_ERR);
      return;
    }

    // TODO: do something with this response...

    mPromise->MaybeResolveWithUndefined();
  }

  virtual void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    mPromise->MaybeReject(aCx, aValue);
  }

private:
  WorkletFetchHandler(Worklet* aWorklet, Promise* aPromise)
    : mWorklet(aWorklet)
    , mPromise(aPromise)
  {}

  ~WorkletFetchHandler()
  {}

  RefPtr<Worklet> mWorklet;
  RefPtr<Promise> mPromise;
};

NS_IMPL_ISUPPORTS0(WorkletFetchHandler)

} // anonymous namespace

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Worklet, mGlobal)
NS_IMPL_CYCLE_COLLECTING_ADDREF(Worklet)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Worklet)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Worklet)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
Worklet::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return WorkletBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise>
Worklet::Import(const nsAString& aModuleURL, ErrorResult& aRv)
{
  return WorkletFetchHandler::Fetch(this, aModuleURL, aRv);
}

} // dom namespace
} // mozilla namespace
