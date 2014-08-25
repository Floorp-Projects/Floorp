/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef mozilla_dom_workers_serviceworkerclient_h
#define mozilla_dom_workers_serviceworkerclient_h

#include "nsCOMPtr.h"
#include "nsWrapperCache.h"

namespace mozilla {

class ErrorResult;

namespace dom {

class Promise;
template<typename T> class Optional;
template<typename T> class Sequence;

namespace workers {

class ServiceWorkerClient MOZ_FINAL : public nsISupports,
                                      public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ServiceWorkerClient)

  ServiceWorkerClient(nsISupports* aOwner, uint64_t aId)
    : mOwner(aOwner),
      mId(aId)
  {
    SetIsDOMBinding();
  }

  uint32_t Id() const
  {
    return mId;
  }

  void PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                   const Optional<Sequence<JS::Value>>& aTransferable,
                   ErrorResult& aRv);

  nsISupports* GetParentObject() const
  {
    return mOwner;
  }

  JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

private:
  ~ServiceWorkerClient()
  {
  }

  nsCOMPtr<nsISupports> mOwner;
  uint64_t mId;
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_serviceworkerclient_h
