/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileCellInfo.h"
#include "mozilla/dom/MozMobileCellInfoBinding.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MobileCellInfo, mWindow)

NS_IMPL_CYCLE_COLLECTING_ADDREF(MobileCellInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MobileCellInfo)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MobileCellInfo)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIMobileCellInfo)
NS_INTERFACE_MAP_END

MobileCellInfo::MobileCellInfo(nsPIDOMWindow* aWindow)
  : mWindow(aWindow)
  , mGsmLocationAreaCode(-1)
  , mGsmCellId(-1)
  , mCdmaBaseStationId(-1)
  , mCdmaBaseStationLatitude(-1)
  , mCdmaBaseStationLongitude(-1)
  , mCdmaSystemId(-1)
  , mCdmaNetworkId(-1)
{
  SetIsDOMBinding();
}

void
MobileCellInfo::Update(nsIMobileCellInfo* aInfo)
{
  if (!aInfo) {
    return;
  }

  aInfo->GetGsmLocationAreaCode(&mGsmLocationAreaCode);
  aInfo->GetGsmCellId(&mGsmCellId);
  aInfo->GetCdmaBaseStationId(&mCdmaBaseStationId);
  aInfo->GetCdmaBaseStationLatitude(&mCdmaBaseStationLatitude);
  aInfo->GetCdmaBaseStationLongitude(&mCdmaBaseStationLongitude);
  aInfo->GetCdmaSystemId(&mCdmaSystemId);
  aInfo->GetCdmaNetworkId(&mCdmaNetworkId);
}

JSObject*
MobileCellInfo::WrapObject(JSContext* aCx)
{
  return MozMobileCellInfoBinding::Wrap(aCx, this);
}

// nsIMobileCellInfo

NS_IMETHODIMP
MobileCellInfo::GetGsmLocationAreaCode(int32_t* aGsmLocationAreaCode)
{
  *aGsmLocationAreaCode = GsmLocationAreaCode();
  return NS_OK;
}

NS_IMETHODIMP
MobileCellInfo::GetGsmCellId(int64_t* aGsmCellId)
{
  *aGsmCellId = GsmCellId();
  return NS_OK;
}

NS_IMETHODIMP
MobileCellInfo::GetCdmaBaseStationId(int32_t* aCdmaBaseStationId)
{
  *aCdmaBaseStationId = CdmaBaseStationId();
  return NS_OK;
}

NS_IMETHODIMP
MobileCellInfo::GetCdmaBaseStationLatitude(int32_t* aCdmaBaseStationLatitude)
{
  *aCdmaBaseStationLatitude = CdmaBaseStationLatitude();
  return NS_OK;
}

NS_IMETHODIMP
MobileCellInfo::GetCdmaBaseStationLongitude(int32_t* aCdmaBaseStationLongitude)
{
  *aCdmaBaseStationLongitude = CdmaBaseStationLongitude();
  return NS_OK;
}

NS_IMETHODIMP
MobileCellInfo::GetCdmaSystemId(int32_t* aCdmaSystemId)
{
  *aCdmaSystemId = CdmaSystemId();
  return NS_OK;
}

NS_IMETHODIMP
MobileCellInfo::GetCdmaNetworkId(int32_t* aCdmaNetworkId)
{
  *aCdmaNetworkId = CdmaNetworkId();
  return NS_OK;
}
