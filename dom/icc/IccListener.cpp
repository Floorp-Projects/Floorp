/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IccListener.h"

#include "Icc.h"
#include "IccManager.h"
#include "nsIDOMClassInfo.h"
#include "nsIIccInfo.h"

using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(IccListener, nsIIccListener)

IccListener::IccListener(IccManager* aIccManager, uint32_t aClientId)
  : mClientId(aClientId)
  , mIccManager(aIccManager)
{
  MOZ_ASSERT(mIccManager);

  nsCOMPtr<nsIIccService> iccService = do_GetService(ICC_SERVICE_CONTRACTID);

  if (!iccService) {
    NS_WARNING("Could not acquire nsIIccService!");
    return;
  }

  iccService->GetIccByServiceId(mClientId, getter_AddRefs(mHandler));
  if (!mHandler) {
    NS_WARNING("Could not acquire nsIIcc!");
    return;
  }

  nsCOMPtr<nsIIccInfo> iccInfo;
  mHandler->GetIccInfo(getter_AddRefs(iccInfo));
  if (iccInfo) {
    nsString iccId;
    iccInfo->GetIccid(iccId);
    if (!iccId.IsEmpty()) {
      mIcc = new Icc(mIccManager->GetOwner(), mHandler, iccInfo);
    }
  }

  DebugOnly<nsresult> rv = mHandler->RegisterListener(this);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Failed registering icc listener with Icc Handler");
}

IccListener::~IccListener()
{
  Shutdown();
}

void
IccListener::Shutdown()
{
  if (mHandler) {
    mHandler->UnregisterListener(this);
    mHandler = nullptr;
  }

  if (mIcc) {
    mIcc->Shutdown();
    mIcc = nullptr;
  }

  mIccManager = nullptr;
}

// nsIIccListener

NS_IMETHODIMP
IccListener::NotifyStkCommand(nsIStkProactiveCmd *aStkProactiveCmd)
{
  if (!mIcc) {
    return NS_OK;
  }

  return mIcc->NotifyStkEvent(NS_LITERAL_STRING("stkcommand"), aStkProactiveCmd);
}

NS_IMETHODIMP
IccListener::NotifyStkSessionEnd()
{
  if (!mIcc) {
    return NS_OK;
  }

  return mIcc->NotifyEvent(NS_LITERAL_STRING("stksessionend"));
}

NS_IMETHODIMP
IccListener::NotifyCardStateChanged()
{
  if (!mIcc) {
    return NS_OK;
  }

  return mIcc->NotifyEvent(NS_LITERAL_STRING("cardstatechange"));
}

NS_IMETHODIMP
IccListener::NotifyIccInfoChanged()
{
  if (!mHandler) {
    return NS_OK;
  }

  nsCOMPtr<nsIIccInfo> iccInfo;
  mHandler->GetIccInfo(getter_AddRefs(iccInfo));

  // Create/delete icc object based on current iccInfo.
  // 1. If the mIcc is nullptr and iccInfo has valid data, create icc object and
  //    notify mIccManager a new icc is added.
  // 2. If the mIcc is not nullptr and iccInfo becomes to null, delete existed
  //    icc object and notify mIccManager the icc is removed.
  if (!mIcc) {
    if (iccInfo) {
      nsString iccId;
      iccInfo->GetIccid(iccId);
      if (!iccId.IsEmpty()) {
        mIcc = new Icc(mIccManager->GetOwner(), mHandler, iccInfo);
        mIccManager->NotifyIccAdd(iccId);
        mIcc->NotifyEvent(NS_LITERAL_STRING("iccinfochange"));
      }
    }
  } else {
    mIcc->UpdateIccInfo(iccInfo);
    mIcc->NotifyEvent(NS_LITERAL_STRING("iccinfochange"));
    if (!iccInfo) {
      nsString iccId = mIcc->GetIccId();
      mIcc->Shutdown();
      mIcc = nullptr;
      mIccManager->NotifyIccRemove(iccId);
    }
  }

  return NS_OK;
}
