/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RequestSyncWifiService.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsIObserverService.h"

namespace mozilla {
namespace dom {

using namespace hal;

NS_IMPL_ISUPPORTS0(RequestSyncWifiService)

namespace {

StaticRefPtr<RequestSyncWifiService> sService;

} // anonymous namespace

/* static */ void
RequestSyncWifiService::Init()
{
  nsRefPtr<RequestSyncWifiService> service = GetInstance();
  if (!service) {
    NS_WARNING("Failed to initialize RequestSyncWifiService.");
  }
}

/* static */ already_AddRefed<RequestSyncWifiService>
RequestSyncWifiService::GetInstance()
{
  if (!sService) {
    sService = new RequestSyncWifiService();
    hal::RegisterNetworkObserver(sService);
    ClearOnShutdown(&sService);
  }

  nsRefPtr<RequestSyncWifiService> service = sService.get();
  return service.forget();
}

void
RequestSyncWifiService::Notify(const hal::NetworkInformation& aNetworkInfo)
{
  bool isWifi = aNetworkInfo.isWifi();
  if (isWifi == mIsWifi) {
    return;
  }

  mIsWifi = isWifi;

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "wifi-state-changed",
                         mIsWifi ? MOZ_UTF16("enabled") :
                                   MOZ_UTF16("disabled"));
  }
}

} // dom namespace
} // mozilla namespace
