/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CellBroadcastMessage.h"
#include "mozilla/dom/MozCellBroadcastMessageBinding.h"
#include "nsPIDOMWindow.h"

#define CONVERT_STRING_TO_NULLABLE_ENUM(_string, _enumType, _enum)      \
{                                                                       \
  _enum.SetNull();                                                      \
                                                                        \
  uint32_t i = 0;                                                       \
  for (const EnumEntry* entry = _enumType##Values::strings;             \
       entry->value;                                                    \
       ++entry, ++i) {                                                  \
    if (_string.EqualsASCII(entry->value)) {                            \
      _enum.SetValue(static_cast<_enumType>(i));                        \
    }                                                                   \
  }                                                                     \
}

namespace mozilla {
namespace dom {

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
                                           const nsAString& aGsmGeographicalScope,
                                           uint16_t aMessageCode,
                                           uint16_t aMessageId,
                                           const nsAString& aLanguage,
                                           const nsAString& aBody,
                                           const nsAString& aMessageClass,
                                           uint64_t aTimestamp,
                                           uint32_t aCdmaServiceCategory,
                                           bool aHasEtwsInfo,
                                           const nsAString& aEtwsWarningType,
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
  CONVERT_STRING_TO_NULLABLE_ENUM(aGsmGeographicalScope,
                                  CellBroadcastGsmGeographicalScope,
                                  mGsmGeographicalScope)

  CONVERT_STRING_TO_NULLABLE_ENUM(aMessageClass,
                                  CellBroadcastMessageClass,
                                  mMessageClass)

  // CdmaServiceCategory represents a 16bit unsigned value.
  if (aCdmaServiceCategory > 0xFFFFU) {
    mCdmaServiceCategory.SetNull();
  } else {
    mCdmaServiceCategory.SetValue(static_cast<uint16_t>(aCdmaServiceCategory));
  }

  SetIsDOMBinding();
}

JSObject*
CellBroadcastMessage::WrapObject(JSContext* aCx)
{
  return MozCellBroadcastMessageBinding::Wrap(aCx, this);
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
                                             const nsAString& aWarningType,
                                             bool aEmergencyUserAlert,
                                             bool aPopup)
  : mWindow(aWindow)
  , mEmergencyUserAlert(aEmergencyUserAlert)
  , mPopup(aPopup)
{
  CONVERT_STRING_TO_NULLABLE_ENUM(aWarningType,
                                  CellBroadcastEtwsWarningType,
                                  mWarningType)

  SetIsDOMBinding();
}

JSObject*
CellBroadcastEtwsInfo::WrapObject(JSContext* aCx)
{
  return MozCellBroadcastEtwsInfoBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla
