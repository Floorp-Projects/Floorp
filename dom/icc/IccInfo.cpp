/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/IccInfo.h"

#include "mozilla/dom/icc/PIccTypes.h"
#include "nsPIDOMWindow.h"

#define CONVERT_STRING_TO_NULLABLE_ENUM(_string, _enumType, _enum)      \
{                                                                       \
  uint32_t i = 0;                                                       \
  for (const EnumEntry* entry = _enumType##Values::strings;             \
       entry->value;                                                    \
       ++entry, ++i) {                                                  \
    if (_string.EqualsASCII(entry->value)) {                            \
      _enum.SetValue(static_cast<_enumType>(i));                        \
    }                                                                   \
  }                                                                     \
}

using namespace mozilla::dom;

using mozilla::dom::icc::IccInfoData;

// IccInfo

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(IccInfo, mWindow)

NS_IMPL_CYCLE_COLLECTING_ADDREF(IccInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IccInfo)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IccInfo)
  NS_INTERFACE_MAP_ENTRY(nsIIccInfo)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END

IccInfo::IccInfo(nsPIDOMWindow* aWindow)
  : mWindow(aWindow)
{
  mIccType.SetIsVoid(true);
  mIccid.SetIsVoid(true);
  mMcc.SetIsVoid(true);
  mMnc.SetIsVoid(true);
  mSpn.SetIsVoid(true);
}

IccInfo::IccInfo(const IccInfoData& aData)
{
  mIccType = aData.iccType();
  mIccid = aData.iccid();
  mMcc = aData.mcc();
  mMnc = aData.mnc();
  mSpn = aData.spn();
  mIsDisplayNetworkNameRequired = aData.isDisplayNetworkNameRequired();
  mIsDisplaySpnRequired = aData.isDisplaySpnRequired();
}

// nsIIccInfo

NS_IMETHODIMP
IccInfo::GetIccType(nsAString & aIccType)
{
  aIccType = mIccType;
  return NS_OK;
}

NS_IMETHODIMP
IccInfo::GetIccid(nsAString & aIccid)
{
  aIccid = mIccid;
  return NS_OK;
}

NS_IMETHODIMP
IccInfo::GetMcc(nsAString & aMcc)
{
  aMcc = mMcc;
  return NS_OK;
}

NS_IMETHODIMP
IccInfo::GetMnc(nsAString & aMnc)
{
  aMnc = mMnc;
  return NS_OK;
}

NS_IMETHODIMP
IccInfo::GetSpn(nsAString & aSpn)
{
  aSpn = mSpn;
  return NS_OK;
}

NS_IMETHODIMP
IccInfo::GetIsDisplayNetworkNameRequired(bool *aIsDisplayNetworkNameRequired)
{
  *aIsDisplayNetworkNameRequired = mIsDisplayNetworkNameRequired;
  return NS_OK;
}

NS_IMETHODIMP
IccInfo::GetIsDisplaySpnRequired(bool *aIsDisplaySpnRequired)
{
  *aIsDisplaySpnRequired = mIsDisplaySpnRequired;
  return NS_OK;
}

void
IccInfo::Update(nsIIccInfo* aInfo)
{
  NS_ASSERTION(aInfo, "aInfo is null");

  aInfo->GetIccType(mIccType);
  aInfo->GetIccid(mIccid);
  aInfo->GetMcc(mMcc);
  aInfo->GetMnc(mMnc);
  aInfo->GetSpn(mSpn);
  aInfo->GetIsDisplayNetworkNameRequired(
    &mIsDisplayNetworkNameRequired);
  aInfo->GetIsDisplaySpnRequired(
    &mIsDisplaySpnRequired);
}

// WebIDL implementation

JSObject*
IccInfo::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MozIccInfoBinding::Wrap(aCx, this, aGivenProto);
}

Nullable<IccType>
IccInfo::GetIccType() const
{
  Nullable<IccType> iccType;

  CONVERT_STRING_TO_NULLABLE_ENUM(mIccType, IccType, iccType);

  return iccType;
}

void
IccInfo::GetIccid(nsAString& aIccId) const
{
  aIccId = mIccid;
}

void
IccInfo::GetMcc(nsAString& aMcc) const
{
  aMcc = mMcc;
}

void
IccInfo::GetMnc(nsAString& aMnc) const
{
  aMnc = mMnc;
}

void
IccInfo::GetSpn(nsAString& aSpn) const
{
  aSpn = mSpn;
}

bool
IccInfo::IsDisplayNetworkNameRequired() const
{
  return mIsDisplayNetworkNameRequired;
}

bool
IccInfo::IsDisplaySpnRequired() const
{
  return mIsDisplaySpnRequired;
}

// GsmIccInfo

NS_IMPL_ISUPPORTS_INHERITED(GsmIccInfo, IccInfo, nsIGsmIccInfo)

GsmIccInfo::GsmIccInfo(nsPIDOMWindow* aWindow)
  : IccInfo(aWindow)
{
  mPhoneNumber.SetIsVoid(true);
}

GsmIccInfo::GsmIccInfo(const IccInfoData& aData)
  : IccInfo(aData)
{
  mPhoneNumber = aData.phoneNumber();
}

// nsIGsmIccInfo

NS_IMETHODIMP
GsmIccInfo::GetMsisdn(nsAString & aMsisdn)
{
  aMsisdn = mPhoneNumber;
  return NS_OK;
}

// WebIDL implementation

void
GsmIccInfo::Update(nsIGsmIccInfo* aInfo)
{
  MOZ_ASSERT(aInfo);
  nsCOMPtr<nsIIccInfo> iccInfo = do_QueryInterface(aInfo);
  MOZ_ASSERT(iccInfo);

  IccInfo::Update(iccInfo);

  aInfo->GetMsisdn(mPhoneNumber);
}

JSObject*
GsmIccInfo::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MozGsmIccInfoBinding::Wrap(aCx, this, aGivenProto);
}

void
GsmIccInfo::GetMsisdn(nsAString& aMsisdn) const
{
  aMsisdn = mPhoneNumber;
}

// CdmaIccInfo

NS_IMPL_ISUPPORTS_INHERITED(CdmaIccInfo, IccInfo, nsICdmaIccInfo)

CdmaIccInfo::CdmaIccInfo(nsPIDOMWindow* aWindow)
  : IccInfo(aWindow)
{
  mPhoneNumber.SetIsVoid(true);
}

CdmaIccInfo::CdmaIccInfo(const IccInfoData& aData)
  : IccInfo(aData)
{
  mPhoneNumber = aData.phoneNumber();
  mPrlVersion = aData.prlVersion();
}

// nsICdmaIccInfo

NS_IMETHODIMP
CdmaIccInfo::GetMdn(nsAString & aMdn)
{
  aMdn = mPhoneNumber;
  return NS_OK;
}

NS_IMETHODIMP
CdmaIccInfo::GetPrlVersion(int32_t *aPrlVersion)
{
  *aPrlVersion = mPrlVersion;
  return NS_OK;
}

void
CdmaIccInfo::Update(nsICdmaIccInfo* aInfo)
{
  MOZ_ASSERT(aInfo);
  nsCOMPtr<nsIIccInfo> iccInfo = do_QueryInterface(aInfo);
  MOZ_ASSERT(iccInfo);

  IccInfo::Update(iccInfo);

  aInfo->GetMdn(mPhoneNumber);
  aInfo->GetPrlVersion(&mPrlVersion);
}

// WebIDL implementation

JSObject*
CdmaIccInfo::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MozCdmaIccInfoBinding::Wrap(aCx, this, aGivenProto);
}

void
CdmaIccInfo::GetMdn(nsAString& aMdn) const
{
  aMdn = mPhoneNumber;
}

int32_t
CdmaIccInfo::PrlVersion() const
{
  return mPrlVersion;
}
