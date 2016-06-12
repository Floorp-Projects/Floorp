/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cellbroadcast_CellBroadcastMessage_h
#define mozilla_dom_cellbroadcast_CellBroadcastMessage_h

#include "mozilla/dom/MozCellBroadcastMessageBinding.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class CellBroadcastEtwsInfo;

class CellBroadcastMessage final : public nsISupports
                                 , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CellBroadcastMessage)

  CellBroadcastMessage(nsPIDOMWindowInner* aWindow,
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
                       bool aEtwsPopup);

  nsPIDOMWindowInner*
  GetParentObject() const { return mWindow; }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL interface

  uint32_t ServiceId() const { return mServiceId; }

  const Nullable<CellBroadcastGsmGeographicalScope>&
  GetGsmGeographicalScope() { return mGsmGeographicalScope; }

  uint16_t MessageCode() const { return mMessageCode; }

  uint16_t MessageId() const { return mMessageId; }

  void GetLanguage(nsString& aLanguage) const { aLanguage = mLanguage; }

  void GetBody(nsString& aBody) const { aBody = mBody; }

  const Nullable<CellBroadcastMessageClass>&
  GetMessageClass() { return mMessageClass; }

  uint64_t Timestamp() const { return mTimestamp; }

  // Mark this as resultNotAddRefed to return raw pointers
  already_AddRefed<CellBroadcastEtwsInfo> GetEtws() const;

  const Nullable<uint16_t>& GetCdmaServiceCategory() { return mCdmaServiceCategory; };

private:
  // final suppresses -Werror,-Wdelete-non-virtual-dtor
  ~CellBroadcastMessage() {};

  // Don't try to use the default constructor.
  CellBroadcastMessage();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  uint32_t mServiceId;
  Nullable<CellBroadcastGsmGeographicalScope> mGsmGeographicalScope;
  uint16_t mMessageCode;
  uint16_t mMessageId;
  nsString mLanguage;
  nsString mBody;
  Nullable<CellBroadcastMessageClass> mMessageClass;
  uint64_t mTimestamp;
  Nullable<uint16_t> mCdmaServiceCategory;
  RefPtr<CellBroadcastEtwsInfo> mEtwsInfo;
};

class CellBroadcastEtwsInfo final : public nsISupports
                                  , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CellBroadcastEtwsInfo)

  CellBroadcastEtwsInfo(nsPIDOMWindowInner* aWindow,
                        uint32_t aWarningType,
                        bool aEmergencyUserAlert,
                        bool aPopup);

  nsPIDOMWindowInner*
  GetParentObject() const { return mWindow; }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL interface

  const Nullable<CellBroadcastEtwsWarningType>&
  GetWarningType()  { return mWarningType; }

  bool EmergencyUserAlert() const { return mEmergencyUserAlert; }

  bool Popup() const { return mPopup; }

private:
  // final suppresses -Werror,-Wdelete-non-virtual-dtor
  ~CellBroadcastEtwsInfo() {};

  // Don't try to use the default constructor.
  CellBroadcastEtwsInfo();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  Nullable<CellBroadcastEtwsWarningType> mWarningType;
  bool mEmergencyUserAlert;
  bool mPopup;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cellbroadcast_CellBroadcastMessage_h
