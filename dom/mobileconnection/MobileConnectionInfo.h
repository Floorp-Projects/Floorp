/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MobileConnectionInfo_h
#define mozilla_dom_MobileConnectionInfo_h

#include "mozilla/dom/MobileCellInfo.h"
#include "mozilla/dom/MobileNetworkInfo.h"
#include "mozilla/dom/MozMobileConnectionInfoBinding.h"
#include "nsIMobileConnectionInfo.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class MobileConnectionInfo final : public nsIMobileConnectionInfo
                                 , public nsWrapperCache
{
public:
  NS_DECL_NSIMOBILECONNECTIONINFO
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MobileConnectionInfo)

  explicit MobileConnectionInfo(nsPIDOMWindow* aWindow);

  MobileConnectionInfo(const nsAString& aState, bool aConnected,
                       bool aEmergencyCallsOnly, bool aRoaming,
                       nsIMobileNetworkInfo* aNetworkInfo,
                       const nsAString& aType,
                       const Nullable<int32_t>& aSignalStrength,
                       const Nullable<uint16_t>& aRelSignalStrength,
                       nsIMobileCellInfo* aCellInfo);

  void
  Update(nsIMobileConnectionInfo* aInfo);

  nsPIDOMWindow*
  GetParentObject() const
  {
    return mWindow;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL interface
  bool
  Connected() const
  {
    return mConnected;
  }

  bool
  EmergencyCallsOnly() const
  {
    return mEmergencyCallsOnly;
  }

  bool
  Roaming() const
  {
    return mRoaming;
  }

  Nullable<MobileConnectionState>
  GetState() const
  {
    return mState;
  }

  Nullable<MobileConnectionType>
  GetType() const
  {
    return mType;
  }

  MobileNetworkInfo*
  GetNetwork() const
  {
    return mNetworkInfo;
  }

  Nullable<int32_t>
  GetSignalStrength() const
  {
    return mSignalStrength;
  }

  Nullable<uint16_t>
  GetRelSignalStrength() const
  {
    return mRelSignalStrength;
  }

  MobileCellInfo*
  GetCell() const
  {
    return mCellInfo;
  }

private:
  ~MobileConnectionInfo() {}

private:
  bool mConnected;
  bool mEmergencyCallsOnly;
  bool mRoaming;
  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsRefPtr<MobileNetworkInfo> mNetworkInfo;
  nsRefPtr<MobileCellInfo> mCellInfo;
  Nullable<MobileConnectionState> mState;
  Nullable<MobileConnectionType> mType;
  Nullable<int32_t> mSignalStrength;
  Nullable<uint16_t> mRelSignalStrength;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MobileConnectionInfo_h
