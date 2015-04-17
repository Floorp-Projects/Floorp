/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/icc/IccChild.h"
#include "IccInfo.h"

using mozilla::dom::IccInfo;

namespace mozilla {
namespace dom {
namespace icc {

/**
 * PIccChild Implementation.
 */

IccChild::IccChild()
  : mCardState(nsIIcc::CARD_STATE_UNKNOWN)
  , mIsAlive(true)
{
  MOZ_COUNT_CTOR(IccChild);
}

IccChild::~IccChild()
{
  MOZ_COUNT_DTOR(IccChild);
}

void
IccChild::Init()
{
  OptionalIccInfoData infoData;

  bool rv = SendInit(&infoData, &mCardState);
  NS_ENSURE_TRUE_VOID(rv);

  UpdateIccInfo(infoData);
}

void
IccChild::Shutdown(){
  if (mIsAlive) {
    mIsAlive = false;
    Send__delete__(this);
  }

  mListeners.Clear();
  mIccInfo = nullptr;
  mCardState = nsIIcc::CARD_STATE_UNKNOWN;
}

void
IccChild::ActorDestroy(ActorDestroyReason why)
{
  mIsAlive = false;
}

bool
IccChild::RecvNotifyCardStateChanged(const uint32_t& aCardState)
{
  mCardState = aCardState;

  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyCardStateChanged();
  }

  return true;
}

bool
IccChild::RecvNotifyIccInfoChanged(const OptionalIccInfoData& aInfoData)
{
  UpdateIccInfo(aInfoData);

  for (int32_t i = 0; i < mListeners.Count(); i++) {
    mListeners[i]->NotifyIccInfoChanged();
  }

  return true;
}

PIccRequestChild*
IccChild::AllocPIccRequestChild(const IccRequest& aRequest)
{
  MOZ_CRASH("Caller is supposed to manually construct a request!");
}

bool
IccChild::DeallocPIccRequestChild(PIccRequestChild* aActor)
{
  delete aActor;
  return true;
}

bool
IccChild::SendRequest(const IccRequest& aRequest, nsIIccCallback* aRequestReply)
{
  NS_ENSURE_TRUE(mIsAlive, false);

  // Deallocated in IccChild::DeallocPIccRequestChild().
  IccRequestChild* actor = new IccRequestChild(aRequestReply);
  SendPIccRequestConstructor(actor, aRequest);

  return true;
}

void
IccChild::UpdateIccInfo(const OptionalIccInfoData& aInfoData) {
  if (aInfoData.type() == OptionalIccInfoData::Tvoid_t) {
    mIccInfo = nullptr;
    return;
  }

  NS_ENSURE_TRUE_VOID(aInfoData.type() == OptionalIccInfoData::TIccInfoData);

  nsRefPtr<IccInfo> iccInfo;
  const IccInfoData& infoData = aInfoData.get_IccInfoData();
  if (infoData.iccType().EqualsLiteral("sim")
      || infoData.iccType().EqualsLiteral("usim")) {
    iccInfo = new GsmIccInfo(infoData);
  } else if (infoData.iccType().EqualsLiteral("ruim")
             || infoData.iccType().EqualsLiteral("csim")){
    iccInfo = new CdmaIccInfo(infoData);
  } else {
    iccInfo = new IccInfo(infoData);
  }

  // We update the orignal one instead of replacing with a new one
  // if the IccType is the same.
  if (mIccInfo) {
    nsString oldIccType;
    nsString newIccType;
    mIccInfo->GetIccType(oldIccType);
    iccInfo->GetIccType(newIccType);

    if (oldIccType.Equals(newIccType)) {
      mIccInfo->Update(iccInfo);
      return;
    }
  }

  mIccInfo = iccInfo;
}

/**
 * nsIIcc Implementation.
 */

NS_IMPL_ISUPPORTS(IccChild, nsIIcc)

NS_IMETHODIMP
IccChild::RegisterListener(nsIIccListener *aListener)
{
  NS_ENSURE_TRUE(!mListeners.Contains(aListener), NS_ERROR_UNEXPECTED);

  mListeners.AppendObject(aListener);
  return NS_OK;
}

NS_IMETHODIMP
IccChild::UnregisterListener(nsIIccListener *aListener)
{
  NS_ENSURE_TRUE(mListeners.Contains(aListener), NS_ERROR_UNEXPECTED);

  mListeners.RemoveObject(aListener);
  return NS_OK;
}

NS_IMETHODIMP
IccChild::GetIccInfo(nsIIccInfo** aIccInfo)
{
  nsCOMPtr<nsIIccInfo> info(mIccInfo);
  info.forget(aIccInfo);
  return NS_OK;
}

NS_IMETHODIMP
IccChild::GetCardState(uint32_t* aCardState)
{
  *aCardState = mCardState;
  return NS_OK;
}

NS_IMETHODIMP
IccChild::GetImsi(nsAString & aImsi)
{
  NS_WARNING("IMSI shall not directly be fetched in child process.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IccChild::GetCardLockEnabled(uint32_t aLockType,
                             nsIIccCallback* aRequestReply)
{
  return SendRequest(GetCardLockEnabledRequest(aLockType), aRequestReply)
    ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
IccChild::UnlockCardLock(uint32_t aLockType,
                         const nsAString& aPassword,
                         const nsAString& aNewPin,
                         nsIIccCallback* aRequestReply)
{
  return SendRequest(UnlockCardLockRequest(aLockType,
                                           nsString(aPassword),
                                           nsString(aNewPin)),
                     aRequestReply)
    ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
IccChild::SetCardLockEnabled(uint32_t aLockType,
                             const nsAString& aPassword,
                             bool aEnabled,
                             nsIIccCallback* aRequestReply)
{
  return SendRequest(SetCardLockEnabledRequest(aLockType,
                                               nsString(aPassword),
                                               aEnabled),
                     aRequestReply)
    ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
IccChild::ChangeCardLockPassword(uint32_t aLockType,
                                 const nsAString& aPassword,
                                 const nsAString& aNewPassword,
                                 nsIIccCallback* aRequestReply)
{
  return SendRequest(ChangeCardLockPasswordRequest(aLockType,
                                                   nsString(aPassword),
                                                   nsString(aNewPassword)),
                     aRequestReply)
    ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
IccChild::GetCardLockRetryCount(uint32_t aLockType,
                                nsIIccCallback* aRequestReply)
{
  return SendRequest(GetCardLockRetryCountRequest(aLockType), aRequestReply)
    ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
IccChild::MatchMvno(uint32_t aMvnoType,
                    const nsAString& aMvnoData,
                    nsIIccCallback* aRequestReply)
{
  return SendRequest(MatchMvnoRequest(aMvnoType, nsString(aMvnoData)),
                     aRequestReply)
    ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
IccChild::GetServiceStateEnabled(uint32_t aService,
                                 nsIIccCallback* aRequestReply)
{
  return SendRequest(GetServiceStateEnabledRequest(aService),
                     aRequestReply)
    ? NS_OK : NS_ERROR_FAILURE;
}

/**
 * PIccRequestChild Implementation.
 */

IccRequestChild::IccRequestChild(nsIIccCallback* aRequestReply)
  : mRequestReply(aRequestReply)
{
  MOZ_COUNT_CTOR(IccRequestChild);
  MOZ_ASSERT(aRequestReply);
}

bool
IccRequestChild::Recv__delete__(const IccReply& aResponse)
{
  MOZ_ASSERT(mRequestReply);

  switch(aResponse.type()) {
    case IccReply::TIccReplySuccess:
      return NS_SUCCEEDED(mRequestReply->NotifySuccess());
    case IccReply::TIccReplySuccessWithBoolean: {
      const IccReplySuccessWithBoolean& resultWithBoolean
        = aResponse.get_IccReplySuccessWithBoolean();
      return NS_SUCCEEDED(
        mRequestReply->NotifySuccessWithBoolean(resultWithBoolean.result()));
    }
    case IccReply::TIccReplyCardLockRetryCount: {
      const IccReplyCardLockRetryCount& retryCount
        = aResponse.get_IccReplyCardLockRetryCount();
      return NS_SUCCEEDED(
        mRequestReply->NotifyGetCardLockRetryCount(retryCount.count()));
    }
    case IccReply::TIccReplyError: {
      const IccReplyError& error = aResponse.get_IccReplyError();
      return NS_SUCCEEDED(mRequestReply->NotifyError(error.message()));
    }
    case IccReply::TIccReplyCardLockError: {
      const IccReplyCardLockError& error
        = aResponse.get_IccReplyCardLockError();
      return NS_SUCCEEDED(
        mRequestReply->NotifyCardLockError(error.message(),
                                           error.retryCount()));
    }
    default:
      MOZ_CRASH("Received invalid response type!");
  }

  return true;
}

} // namespace icc
} // namespace dom
} // namespace mozilla