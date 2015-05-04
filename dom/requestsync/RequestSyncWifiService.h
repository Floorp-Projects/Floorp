/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RequestSyncWifiService_h
#define mozilla_dom_RequestSyncWifiService_h

#include "mozilla/dom/network/Types.h"
#include "mozilla/Hal.h"
#include "nsIObserver.h"

namespace mozilla {
namespace dom {

class RequestSyncWifiService final : public nsISupports
                                   , public NetworkObserver
{
public:
  NS_DECL_ISUPPORTS

  static void Init();

  static already_AddRefed<RequestSyncWifiService> GetInstance();

  void Notify(const hal::NetworkInformation& aNetworkInfo) override;

private:
  RequestSyncWifiService()
    : mIsWifi(false)
  {}

  ~RequestSyncWifiService()
  {}

  bool mIsWifi;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_RequestSyncWifiService_h
