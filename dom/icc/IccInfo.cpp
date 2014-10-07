/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/IccInfo.h"

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

// IccInfo

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(IccInfo, mWindow, mIccInfo)

NS_IMPL_CYCLE_COLLECTING_ADDREF(IccInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IccInfo)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IccInfo)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

IccInfo::IccInfo(nsPIDOMWindow* aWindow)
  : mWindow(aWindow)
{
}

void
IccInfo::Update(nsIIccInfo* aInfo)
{
  mIccInfo = aInfo;
}

JSObject*
IccInfo::WrapObject(JSContext* aCx)
{
  return MozIccInfoBinding::Wrap(aCx, this);
}

Nullable<IccType>
IccInfo::GetIccType() const
{
  if (!mIccInfo) {
    return Nullable<IccType>();
  }

  nsAutoString type;
  Nullable<IccType> iccType;

  mIccInfo->GetIccType(type);
  CONVERT_STRING_TO_NULLABLE_ENUM(type, IccType, iccType);

  return iccType;
}

void
IccInfo::GetIccid(nsAString& aIccId) const
{
  if (!mIccInfo) {
    aIccId.SetIsVoid(true);
    return;
  }

  mIccInfo->GetIccid(aIccId);
}

void
IccInfo::GetMcc(nsAString& aMcc) const
{
  if (!mIccInfo) {
    aMcc.SetIsVoid(true);
    return;
  }

  mIccInfo->GetMcc(aMcc);
}

void
IccInfo::GetMnc(nsAString& aMnc) const
{
  if (!mIccInfo) {
    aMnc.SetIsVoid(true);
    return;
  }

  mIccInfo->GetMnc(aMnc);
}

void
IccInfo::GetSpn(nsAString& aSpn) const
{
  if (!mIccInfo) {
    aSpn.SetIsVoid(true);
    return;
  }

  mIccInfo->GetSpn(aSpn);
}

bool
IccInfo::IsDisplayNetworkNameRequired() const
{
  if (!mIccInfo) {
    return false;
  }

  bool isDisplayNetworkNameRequired;
  mIccInfo->GetIsDisplayNetworkNameRequired(&isDisplayNetworkNameRequired);

  return isDisplayNetworkNameRequired;
}

bool
IccInfo::IsDisplaySpnRequired() const
{
  if (!mIccInfo) {
    return false;
  }

  bool isDisplaySpnRequired;
  mIccInfo->GetIsDisplaySpnRequired(&isDisplaySpnRequired);

  return isDisplaySpnRequired;
}

// GsmIccInfo

NS_IMPL_CYCLE_COLLECTION_INHERITED(GsmIccInfo, IccInfo, mGsmIccInfo)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(GsmIccInfo)
NS_INTERFACE_MAP_END_INHERITING(IccInfo)

NS_IMPL_ADDREF_INHERITED(GsmIccInfo, IccInfo)
NS_IMPL_RELEASE_INHERITED(GsmIccInfo, IccInfo)

GsmIccInfo::GsmIccInfo(nsPIDOMWindow* aWindow)
  : IccInfo(aWindow)
{
}

void
GsmIccInfo::Update(nsIGsmIccInfo* aInfo)
{
  IccInfo::Update(aInfo);
  mGsmIccInfo = aInfo;
}

JSObject*
GsmIccInfo::WrapObject(JSContext* aCx)
{
  return MozGsmIccInfoBinding::Wrap(aCx, this);
}

void
GsmIccInfo::GetMsisdn(nsAString& aMsisdn) const
{
  if (!mGsmIccInfo) {
    aMsisdn.SetIsVoid(true);
    return;
  }

  mGsmIccInfo->GetMsisdn(aMsisdn);
}

// CdmaIccInfo

NS_IMPL_CYCLE_COLLECTION_INHERITED(CdmaIccInfo, IccInfo, mCdmaIccInfo)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(CdmaIccInfo)
NS_INTERFACE_MAP_END_INHERITING(IccInfo)

NS_IMPL_ADDREF_INHERITED(CdmaIccInfo, IccInfo)
NS_IMPL_RELEASE_INHERITED(CdmaIccInfo, IccInfo)

CdmaIccInfo::CdmaIccInfo(nsPIDOMWindow* aWindow)
  : IccInfo(aWindow)
{
}

void
CdmaIccInfo::Update(nsICdmaIccInfo* aInfo)
{
  IccInfo::Update(aInfo);
  mCdmaIccInfo = aInfo;
}

JSObject*
CdmaIccInfo::WrapObject(JSContext* aCx)
{
  return MozCdmaIccInfoBinding::Wrap(aCx, this);
}

void
CdmaIccInfo::GetMdn(nsAString& aMdn) const
{
  if (!mCdmaIccInfo) {
    aMdn.SetIsVoid(true);
    return;
  }

  mCdmaIccInfo->GetMdn(aMdn);
}

int32_t
CdmaIccInfo::PrlVersion() const
{
  if (!mCdmaIccInfo) {
    return 0;
  }

  int32_t prlVersion;
  mCdmaIccInfo->GetPrlVersion(&prlVersion);

  return prlVersion;
}
