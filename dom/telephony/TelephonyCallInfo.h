/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TelephonyCallInfo_h
#define mozilla_dom_TelephonyCallInfo_h

#include "nsITelephonyCallInfo.h"
#include "nsString.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {
namespace telephony {

class TelephonyCallInfo final : public nsITelephonyCallInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITELEPHONYCALLINFO

  TelephonyCallInfo(uint32_t aClientId,
                    uint32_t aCallIndex,
                    uint16_t aCallState,
                    const nsAString& aDisconnectedReason,

                    const nsAString& aNumber,
                    uint16_t aNumberPresentation,
                    const nsAString& aName,
                    uint16_t aNamePresentation,

                    bool aIsOutgoing,
                    bool aIsEmergency,
                    bool aIsConference,
                    bool aIsSwitchable,
                    bool aIsMergeable);

private:
  // Don't try to use the default constructor.
  TelephonyCallInfo() {}

  ~TelephonyCallInfo() {}

  uint32_t mClientId;
  uint32_t mCallIndex;
  uint16_t mCallState;
  nsString mDisconnectedReason;

  nsString mNumber;
  uint16_t mNumberPresentation;
  nsString mName;
  uint16_t mNamePresentation;

  bool mIsOutgoing;
  bool mIsEmergency;
  bool mIsConference;
  bool mIsSwitchable;
  bool mIsMergeable;
};

} // namespace telephony
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TelephonyCallInfo_h
