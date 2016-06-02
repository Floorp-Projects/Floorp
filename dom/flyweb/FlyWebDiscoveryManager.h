/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FlyWebDiscoveryManager_h
#define mozilla_dom_FlyWebDiscoveryManager_h

#include "nsISupportsImpl.h"
#include "mozilla/ErrorResult.h"
#include "nsRefPtrHashtable.h"
#include "nsWrapperCache.h"
#include "FlyWebDiscoveryManagerBinding.h"
#include "FlyWebService.h"

namespace mozilla {
namespace dom {

class FlyWebDiscoveryManager final : public nsISupports
                                   , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(FlyWebDiscoveryManager)

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
  nsISupports* GetParentObject() const;

  static already_AddRefed<FlyWebDiscoveryManager> Constructor(const GlobalObject& aGlobal,
                                                              ErrorResult& rv);

  void ListServices(nsTArray<FlyWebDiscoveredService>& aServices);
  uint32_t StartDiscovery(FlyWebDiscoveryCallback& aCallback);
  void StopDiscovery(uint32_t aId);

  void PairWithService(const nsAString& aServiceId,
                       FlyWebPairingCallback& callback);

  void NotifyDiscoveredServicesChanged();

private:
  FlyWebDiscoveryManager(nsISupports* mParent, FlyWebService* aService);
  ~FlyWebDiscoveryManager();

  uint32_t GenerateId() {
    return ++mNextId;
  }

  nsCOMPtr<nsISupports> mParent;
  RefPtr<FlyWebService> mService;

  uint32_t mNextId;

  nsRefPtrHashtable<nsUint32HashKey, FlyWebDiscoveryCallback> mCallbackMap;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FlyWebDiscoveryManager_h
