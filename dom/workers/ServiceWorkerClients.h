/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef mozilla_dom_workers_serviceworkerclients_h
#define mozilla_dom_workers_serviceworkerclients_h

#include "nsAutoPtr.h"
#include "nsWrapperCache.h"

namespace mozilla {

class ErrorResult;

namespace dom {

class Promise;

namespace workers {

class ServiceWorkerGlobalScope;

class ServiceWorkerClients MOZ_FINAL : public nsISupports,
                                       public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ServiceWorkerClients)

  ServiceWorkerClients(ServiceWorkerGlobalScope* aWorkerScope);

  already_AddRefed<Promise> GetServiced(ErrorResult& aRv);
  already_AddRefed<Promise> ReloadAll(ErrorResult& aRv);

  JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  ServiceWorkerGlobalScope* GetParentObject() const
  {
    return mWorkerScope;
  }

private:
  ~ServiceWorkerClients()
  {
  }

  nsRefPtr<ServiceWorkerGlobalScope> mWorkerScope;
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_serviceworkerclients_h
