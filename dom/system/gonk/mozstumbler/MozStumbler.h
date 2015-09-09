/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_mozstumbler_h__
#define mozilla_system_mozstumbler_h__

#include "nsIDOMEventTarget.h"
#include "nsICellInfo.h"
#include "nsIWifi.h"

#define STUMBLE_INTERVAL_MS 3000

class nsGeoPosition;

void MozStumble(nsGeoPosition* position);

class StumblerInfo final : public nsICellInfoListCallback,
                           public nsIWifiScanResultsReady
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICELLINFOLISTCALLBACK
  NS_DECL_NSIWIFISCANRESULTSREADY

  explicit StumblerInfo(nsGeoPosition* position)
    : mPosition(position), mCellInfoResponsesExpected(0), mCellInfoResponsesReceived(0), mIsWifiInfoResponseReceived(0)
  {}
  void SetWifiInfoResponseReceived();
  void SetCellInfoResponsesExpected(uint8_t count);

private:
  ~StumblerInfo() {}
  void DumpStumblerInfo();
  nsresult LocationInfoToString(nsACString& aLocDesc);
  void CellNetworkInfoToString(nsACString& aCellDesc);
  nsTArray<nsRefPtr<nsICellInfo>> mCellInfo;
  nsCString mWifiDesc;
  nsRefPtr<nsGeoPosition> mPosition;
  int mCellInfoResponsesExpected;
  int mCellInfoResponsesReceived;
  bool mIsWifiInfoResponseReceived;
};
#endif // mozilla_system_mozstumbler_h__

