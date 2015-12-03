/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef mozilla_dom_workers_serviceworkerclients_h
#define mozilla_dom_workers_serviceworkerclients_h

#include "nsAutoPtr.h"
#include "nsWrapperCache.h"

#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ClientsBinding.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {
namespace dom {
namespace workers {

class ServiceWorkerClients final : public nsISupports,
                                   public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ServiceWorkerClients)

  explicit ServiceWorkerClients(ServiceWorkerGlobalScope* aWorkerScope);

  already_AddRefed<Promise>
  Get(const nsAString& aClientId, ErrorResult& aRv);

  already_AddRefed<Promise>
  MatchAll(const ClientQueryOptions& aOptions, ErrorResult& aRv);

  already_AddRefed<Promise>
  OpenWindow(const nsAString& aUrl, ErrorResult& aRv);

  already_AddRefed<Promise>
  Claim(ErrorResult& aRv);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  ServiceWorkerGlobalScope*
  GetParentObject() const
  {
    return mWorkerScope;
  }

private:
  ~ServiceWorkerClients()
  {
  }

  RefPtr<ServiceWorkerGlobalScope> mWorkerScope;
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_serviceworkerclients_h
