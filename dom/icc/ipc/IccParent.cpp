/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/icc/IccParent.h"
#include "nsIIccService.h"
#include "IccInfo.h"

using mozilla::dom::IccInfo;

namespace mozilla {
namespace dom {
namespace icc {

namespace {

static void
GetIccInfoDataFromIccInfo(nsIIccInfo* aInInfo, IccInfoData& aOutData) {
  aInInfo->GetIccType(aOutData.iccType());
  aInInfo->GetIccid(aOutData.iccid());
  aInInfo->GetMcc(aOutData.mcc());
  aInInfo->GetMnc(aOutData.mnc());
  aInInfo->GetSpn(aOutData.spn());
  aInInfo->GetIsDisplayNetworkNameRequired(
    &aOutData.isDisplayNetworkNameRequired());
  aInInfo->GetIsDisplaySpnRequired(
    &aOutData.isDisplaySpnRequired());

  nsCOMPtr<nsIGsmIccInfo> gsmIccInfo(do_QueryInterface(aInInfo));
  if (gsmIccInfo) {
    gsmIccInfo->GetMsisdn(aOutData.phoneNumber());
  }

  nsCOMPtr<nsICdmaIccInfo> cdmaIccInfo(do_QueryInterface(aInInfo));
  if (cdmaIccInfo) {
    cdmaIccInfo->GetMdn(aOutData.phoneNumber());
    cdmaIccInfo->GetPrlVersion(&aOutData.prlVersion());
  }
}

} // anonymous namespace

/**
 * PIccParent Implementation.
 */

IccParent::IccParent(uint32_t aServiceId)
{
  MOZ_COUNT_CTOR(IccParent);

  nsCOMPtr<nsIIccService> service =
    do_GetService(ICC_SERVICE_CONTRACTID);

  NS_ASSERTION(service, "Failed to get IccService!");

  service->GetIccByServiceId(aServiceId, getter_AddRefs(mIcc));

  NS_ASSERTION(mIcc, "Failed to get Icc with specified serviceId.");

  mIcc->RegisterListener(this);
}

void
IccParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mIcc) {
    mIcc->UnregisterListener(this);
    mIcc = nullptr;
  }
}

bool
IccParent::RecvInit(OptionalIccInfoData* aInfoData,
                    uint32_t* aCardState)
{
  NS_ENSURE_TRUE(mIcc, false);

  nsresult rv = mIcc->GetCardState(aCardState);
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsIIccInfo> iccInfo;
  rv = mIcc->GetIccInfo(getter_AddRefs(iccInfo));
  NS_ENSURE_SUCCESS(rv, false);

  if (iccInfo) {
    IccInfoData data;
    GetIccInfoDataFromIccInfo(iccInfo, data);
    *aInfoData = OptionalIccInfoData(data);

    return true;
  }

  *aInfoData = OptionalIccInfoData(void_t());

  return true;
}

PIccRequestParent*
IccParent::AllocPIccRequestParent(const IccRequest& aRequest)
{
  NS_ASSERTION(mIcc, "AllocPIccRequestParent after actor was destroyed!");

  IccRequestParent* actor = new IccRequestParent(mIcc);
  // Add an extra ref for IPDL. Will be released in
  // IccParent::DeallocPIccRequestParent().
  actor->AddRef();
  return actor;
}

bool
IccParent::DeallocPIccRequestParent(PIccRequestParent* aActor)
{
  // IccRequestParent is refcounted, must not be freed manually.
  static_cast<IccRequestParent*>(aActor)->Release();
  return true;
}

bool
IccParent::RecvPIccRequestConstructor(PIccRequestParent* aActor,
                                      const IccRequest& aRequest)
{
  NS_ASSERTION(mIcc, "RecvPIccRequestConstructor after actor was destroyed!");

  IccRequestParent* actor = static_cast<IccRequestParent*>(aActor);

  switch (aRequest.type()) {
    case IccRequest::TGetCardLockEnabledRequest:
      return actor->DoRequest(aRequest.get_GetCardLockEnabledRequest());
    case IccRequest::TUnlockCardLockRequest:
      return actor->DoRequest(aRequest.get_UnlockCardLockRequest());
    case IccRequest::TSetCardLockEnabledRequest:
      return actor->DoRequest(aRequest.get_SetCardLockEnabledRequest());
    case IccRequest::TChangeCardLockPasswordRequest:
      return actor->DoRequest(aRequest.get_ChangeCardLockPasswordRequest());
    case IccRequest::TGetCardLockRetryCountRequest:
      return actor->DoRequest(aRequest.get_GetCardLockRetryCountRequest());
    case IccRequest::TMatchMvnoRequest:
      return actor->DoRequest(aRequest.get_MatchMvnoRequest());
    case IccRequest::TGetServiceStateEnabledRequest:
      return actor->DoRequest(aRequest.get_GetServiceStateEnabledRequest());
    default:
      MOZ_CRASH("Received invalid request type!");
  }

  return true;
}

/**
 * nsIIccListener Implementation.
 */

NS_IMPL_ISUPPORTS(IccParent, nsIIccListener)

NS_IMETHODIMP
IccParent::NotifyStkCommand(const nsAString & aMessage)
{
  // Bug 1114938 - [B2G][ICC] Refactor STK in MozIcc.webidl with IPDL.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IccParent::NotifyStkSessionEnd()
{
  // Bug 1114938 - [B2G][ICC] Refactor STK in MozIcc.webidl with IPDL.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IccParent::NotifyCardStateChanged()
{
  NS_ENSURE_TRUE(mIcc, NS_ERROR_FAILURE);

  uint32_t cardState;
  nsresult rv = mIcc->GetCardState(&cardState);
  NS_ENSURE_SUCCESS(rv, rv);

  return SendNotifyCardStateChanged(cardState) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
IccParent::NotifyIccInfoChanged()
{
  NS_ENSURE_TRUE(mIcc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIIccInfo> iccInfo;
  nsresult rv = mIcc->GetIccInfo(getter_AddRefs(iccInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!iccInfo) {
    return SendNotifyIccInfoChanged(OptionalIccInfoData(void_t()))
      ? NS_OK : NS_ERROR_FAILURE;
  }

  IccInfoData data;
  GetIccInfoDataFromIccInfo(iccInfo, data);

  return SendNotifyIccInfoChanged(OptionalIccInfoData(data))
    ? NS_OK : NS_ERROR_FAILURE;
}

/**
 * PIccRequestParent Implementation.
 */

IccRequestParent::IccRequestParent(nsIIcc* aIcc)
  : mIcc(aIcc)
{
  MOZ_COUNT_CTOR(IccRequestParent);
}

void
IccRequestParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mIcc = nullptr;
}

bool
IccRequestParent::DoRequest(const GetCardLockEnabledRequest& aRequest)
{
  return NS_SUCCEEDED(mIcc->GetCardLockEnabled(aRequest.lockType(),
                                               this));
}

bool
IccRequestParent::DoRequest(const UnlockCardLockRequest& aRequest)
{
  return NS_SUCCEEDED(mIcc->UnlockCardLock(aRequest.lockType(),
                                           aRequest.password(),
                                           aRequest.newPin(),
                                           this));
}

bool
IccRequestParent::DoRequest(const SetCardLockEnabledRequest& aRequest)
{
  return NS_SUCCEEDED(mIcc->SetCardLockEnabled(aRequest.lockType(),
                                               aRequest.password(),
                                               aRequest.enabled(),
                                               this));
}

bool
IccRequestParent::DoRequest(const ChangeCardLockPasswordRequest& aRequest)
{
  return NS_SUCCEEDED(mIcc->ChangeCardLockPassword(aRequest.lockType(),
                                                   aRequest.password(),
                                                   aRequest.newPassword(),
                                                   this));
}

bool
IccRequestParent::DoRequest(const GetCardLockRetryCountRequest& aRequest)
{
  return NS_SUCCEEDED(mIcc->GetCardLockRetryCount(aRequest.lockType(),
                                                  this));
}

bool
IccRequestParent::DoRequest(const MatchMvnoRequest& aRequest)
{
  return NS_SUCCEEDED(mIcc->MatchMvno(aRequest.mvnoType(),
                                      aRequest.mvnoData(),
                                      this));
}

bool
IccRequestParent::DoRequest(const GetServiceStateEnabledRequest& aRequest)
{
  return NS_SUCCEEDED(mIcc->GetServiceStateEnabled(aRequest.service(),
                                                   this));
}

nsresult
IccRequestParent::SendReply(const IccReply& aReply)
{
  NS_ENSURE_TRUE(mIcc, NS_ERROR_FAILURE);

  return Send__delete__(this, aReply) ? NS_OK : NS_ERROR_FAILURE;
}

/**
 * nsIIccCallback Implementation.
 */

NS_IMPL_ISUPPORTS(IccRequestParent, nsIIccCallback)

NS_IMETHODIMP
IccRequestParent::NotifySuccess()
{
  return SendReply(IccReplySuccess());
}

NS_IMETHODIMP
IccRequestParent::NotifySuccessWithBoolean(bool aResult)
{
  return SendReply(IccReplySuccessWithBoolean(aResult));
}

NS_IMETHODIMP
IccRequestParent::NotifyGetCardLockRetryCount(int32_t aCount)
{
  return SendReply(IccReplyCardLockRetryCount(aCount));
}

NS_IMETHODIMP
IccRequestParent::NotifyError(const nsAString & aErrorMsg)
{
  return SendReply(IccReplyError(nsString(aErrorMsg)));
}

NS_IMETHODIMP
IccRequestParent::NotifyCardLockError(const nsAString & aErrorMsg,
                                      int32_t aRetryCount)
{
  return SendReply(IccReplyCardLockError(aRetryCount, nsString(aErrorMsg)));
}

} // namespace icc
} // namespace dom
} // namespace mozilla