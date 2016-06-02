/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "nsTHashtable.h"
#include "nsClassHashtable.h"
#include "nsIUUIDGenerator.h"
#include "jsapi.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Logging.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"

#include "mozilla/dom/FlyWebDiscoveryManager.h"
#include "mozilla/dom/FlyWebDiscoveryManagerBinding.h"

namespace mozilla {
namespace dom {

static LazyLogModule gFlyWebDiscoveryManagerLog("FlyWebDiscoveryManager");
#undef LOG_I
#define LOG_I(...) MOZ_LOG(mozilla::dom::gFlyWebDiscoveryManagerLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#undef LOG_E
#define LOG_E(...) MOZ_LOG(mozilla::dom::gFlyWebDiscoveryManagerLog, mozilla::LogLevel::Error, (__VA_ARGS__))


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(FlyWebDiscoveryManager)

NS_IMPL_CYCLE_COLLECTING_ADDREF(FlyWebDiscoveryManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FlyWebDiscoveryManager)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FlyWebDiscoveryManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

FlyWebDiscoveryManager::FlyWebDiscoveryManager(nsISupports* aParent,
                                               FlyWebService* aService)
  : mParent(aParent)
  , mService(aService)
  , mNextId(0)
{
}

FlyWebDiscoveryManager::~FlyWebDiscoveryManager()
{
  mService->UnregisterDiscoveryManager(this);
}

JSObject*
FlyWebDiscoveryManager::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FlyWebDiscoveryManagerBinding::Wrap(aCx, this, aGivenProto);
}

nsISupports*
FlyWebDiscoveryManager::GetParentObject() const
{
  return mParent;
}

/* static */ already_AddRefed<FlyWebDiscoveryManager>
FlyWebDiscoveryManager::Constructor(const GlobalObject& aGlobal, ErrorResult& rv)
{
  RefPtr<FlyWebService> service = FlyWebService::GetOrCreate();
  if (!service) {
    return nullptr;
  }

  RefPtr<FlyWebDiscoveryManager> result = new FlyWebDiscoveryManager(
                    aGlobal.GetAsSupports(), service);
  return result.forget();
}

void
FlyWebDiscoveryManager::ListServices(nsTArray<FlyWebDiscoveredService>& aServices)
{
  return mService->ListDiscoveredServices(aServices);
}

uint32_t
FlyWebDiscoveryManager::StartDiscovery(FlyWebDiscoveryCallback& aCallback)
{
  uint32_t id = GenerateId();
  mCallbackMap.Put(id, &aCallback);
  mService->RegisterDiscoveryManager(this);
  return id;
}

void
FlyWebDiscoveryManager::StopDiscovery(uint32_t aId)
{
  mCallbackMap.Remove(aId);
  if (mCallbackMap.Count() == 0) {
    mService->UnregisterDiscoveryManager(this);
  }
}

void
FlyWebDiscoveryManager::PairWithService(const nsAString& aServiceId,
                                        FlyWebPairingCallback& aCallback)
{
  mService->PairWithService(aServiceId, aCallback);
}

void
FlyWebDiscoveryManager::NotifyDiscoveredServicesChanged()
{
  nsTArray<FlyWebDiscoveredService> services;
  ListServices(services);
  Sequence<FlyWebDiscoveredService> servicesSeq;
  servicesSeq.SwapElements(services);
  for (auto iter = mCallbackMap.Iter(); !iter.Done(); iter.Next()) {
    FlyWebDiscoveryCallback *callback = iter.UserData();
    ErrorResult err;
    callback->OnDiscoveredServicesChanged(servicesSeq, err);
    ENSURE_SUCCESS_VOID(err);
  }
}


} // namespace dom
} // namespace mozilla
