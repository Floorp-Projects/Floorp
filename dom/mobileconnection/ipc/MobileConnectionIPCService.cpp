/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileConnectionIPCService.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::mobileconnection;

NS_IMPL_ISUPPORTS(MobileConnectionIPCService, nsIMobileConnectionService)

MobileConnectionIPCService::MobileConnectionIPCService()
{
  int32_t numRil = Preferences::GetInt("ril.numRadioInterfaces", 1);
  mItems.SetLength(numRil);
}

MobileConnectionIPCService::~MobileConnectionIPCService()
{
  uint32_t count = mItems.Length();
  for (uint32_t i = 0; i < count; i++) {
    if (mItems[i]) {
      mItems[i]->Shutdown();
    }
  }
}

// nsIMobileConnectionService

NS_IMETHODIMP
MobileConnectionIPCService::GetNumItems(uint32_t* aNumItems)
{
  *aNumItems = mItems.Length();
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionIPCService::GetItemByServiceId(uint32_t aServiceId,
                                               nsIMobileConnection** aItem)
{
  NS_ENSURE_TRUE(aServiceId < mItems.Length(), NS_ERROR_INVALID_ARG);

  if (!mItems[aServiceId]) {
    nsRefPtr<MobileConnectionChild> child = new MobileConnectionChild(aServiceId);

    // |SendPMobileConnectionConstructor| adds another reference to the child
    // actor and removes in |DeallocPMobileConnectionChild|.
    ContentChild::GetSingleton()->SendPMobileConnectionConstructor(child,
                                                                   aServiceId);
    child->Init();

    mItems[aServiceId] = child;
  }

  nsRefPtr<nsIMobileConnection> item(mItems[aServiceId]);
  item.forget(aItem);

  return NS_OK;
}
