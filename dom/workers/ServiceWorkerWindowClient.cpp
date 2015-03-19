/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ServiceWorkerWindowClient.h"

#include "mozilla/dom/ClientBinding.h"

using namespace mozilla::dom;
using namespace mozilla::dom::workers;

JSObject*
ServiceWorkerWindowClient::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return WindowClientBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise>
ServiceWorkerWindowClient::Focus() const
{
  ErrorResult result;
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  MOZ_ASSERT(global);

  nsRefPtr<Promise> promise = Promise::Create(global, result);
  if (NS_WARN_IF(result.Failed())) {
    return nullptr;
  }

  promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
  return promise.forget();
}
