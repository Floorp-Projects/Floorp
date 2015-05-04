/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CellBroadcastMessage.h"
#include "mozilla/dom/MozCellBroadcastMessageBinding.h"
#include "nsPIDOMWindow.h"
#include "nsICellBroadcastService.h"

namespace mozilla {
namespace dom {

/**
 * Converter for XPIDL Constants to WebIDL Enumerations
 */

template<class T> struct EnumConverter {};

template<class T>
struct StaticEnumConverter
{
  typedef T WebidlEnumType;
  typedef uint32_t XpidlEnumType;

  static MOZ_CONSTEXPR WebidlEnumType
  x2w(XpidlEnumType aXpidlEnum) { return static_cast<WebidlEnumType>(aXpidlEnum); }
};

template<class T>
MOZ_CONSTEXPR T
ToWebidlEnum(uint32_t aXpidlEnum) { return EnumConverter<T>::x2w(aXpidlEnum); }

// Declare converters here:
template <>
struct EnumConverter<CellBroadcastGsmGeographicalScope> :
  public StaticEnumConverter<CellBroadcastGsmGeographicalScope> {};
template <>
struct EnumConverter<CellBroadcastMessageClass> :
  public StaticEnumConverter<CellBroadcastMessageClass> {};
template <>
struct EnumConverter<CellBroadcastEtwsWarningType> :
  public StaticEnumConverter<CellBroadcastEtwsWarningType> {};

/**
 * CellBroadcastMessage Implementation.
 */

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CellBroadcastMessage, mWindow, mEtwsInfo)

NS_IMPL_CYCLE_COLLECTING_ADDREF(CellBroadcastMessage)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CellBroadcastMessage)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CellBroadcastMessage)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

CellBroadcastMessage::CellBroadcastMessage(nsPIDOMWindow* aWindow,
                                           uint32_t aServiceId,
                                           uint32_t aGsmGeographicalScope,
                                           uint16_t aMessageCode,
                                           uint16_t aMessageId,
                                           const nsAString& aLanguage,
                                           const nsAString& aBody,
                                           uint32_t aMessageClass,
                                           uint64_t aTimestamp,
                                           uint32_t aCdmaServiceCategory,
                                           bool aHasEtwsInfo,
                                           uint32_t aEtwsWarningType,
                                           bool aEtwsEmergencyUserAlert,
                                           bool aEtwsPopup)
  : mWindow(aWindow)
  , mServiceId(aServiceId)
  , mMessageCode(aMessageCode)
  , mMessageId(aMessageId)
  , mLanguage(aLanguage)
  , mBody(aBody)
  , mTimestamp(aTimestamp)
  , mEtwsInfo(aHasEtwsInfo ? new CellBroadcastEtwsInfo(aWindow,
                                                       aEtwsWarningType,
                                                       aEtwsEmergencyUserAlert,
                                                       aEtwsPopup)
                           : nullptr)
{
  if (aGsmGeographicalScope <
      static_cast<uint32_t>(CellBroadcastGsmGeographicalScope::EndGuard_)) {
    mGsmGeographicalScope.SetValue(
      ToWebidlEnum<CellBroadcastGsmGeographicalScope>(aGsmGeographicalScope));
  }

  if (aMessageClass <
      static_cast<uint32_t>(CellBroadcastMessageClass::EndGuard_)) {
    mMessageClass.SetValue(
      ToWebidlEnum<CellBroadcastMessageClass>(aMessageClass));
  }

  // CdmaServiceCategory represents a 16bit unsigned value.
  if (aCdmaServiceCategory <= 0xFFFFU) {
    mCdmaServiceCategory.SetValue(static_cast<uint16_t>(aCdmaServiceCategory));
  }
}

JSObject*
CellBroadcastMessage::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MozCellBroadcastMessageBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<CellBroadcastEtwsInfo>
CellBroadcastMessage::GetEtws() const
{
  nsRefPtr<CellBroadcastEtwsInfo> etwsInfo = mEtwsInfo;
  return etwsInfo.forget();
}

/**
 * CellBroadcastEtwsInfo Implementation.
 */

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CellBroadcastEtwsInfo, mWindow)

NS_IMPL_CYCLE_COLLECTING_ADDREF(CellBroadcastEtwsInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CellBroadcastEtwsInfo)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CellBroadcastEtwsInfo)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

CellBroadcastEtwsInfo::CellBroadcastEtwsInfo(nsPIDOMWindow* aWindow,
                                             uint32_t aWarningType,
                                             bool aEmergencyUserAlert,
                                             bool aPopup)
  : mWindow(aWindow)
  , mEmergencyUserAlert(aEmergencyUserAlert)
  , mPopup(aPopup)
{
  if (aWarningType <
      static_cast<uint32_t>(CellBroadcastEtwsWarningType::EndGuard_)) {
    mWarningType.SetValue(
      ToWebidlEnum<CellBroadcastEtwsWarningType>(aWarningType));
  }
}

JSObject*
CellBroadcastEtwsInfo::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MozCellBroadcastEtwsInfoBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
