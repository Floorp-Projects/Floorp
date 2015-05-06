/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_TelephonyIPCSerializer_h
#define mozilla_dom_telephony_TelephonyIPCSerializer_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/telephony/TelephonyCallInfo.h"
#include "nsITelephonyCallInfo.h"

using mozilla::AutoJSContext;
using mozilla::dom::telephony::TelephonyCallInfo;

typedef nsITelephonyCallInfo* nsTelephonyCallInfo;

namespace IPC {

/**
 * nsITelephonyCallInfo Serialize/De-serialize.
 */
template <>
struct ParamTraits<nsITelephonyCallInfo*>
{
  typedef nsITelephonyCallInfo* paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    bool isNull = !aParam;
    WriteParam(aMsg, isNull);
    // If it is a null object, then we are done.
    if (isNull) {
      return;
    }

    uint32_t clientId;
    uint32_t callIndex;
    uint16_t callState;
    nsString number;
    uint16_t numberPresentation;
    nsString name;
    uint16_t namePresentation;
    bool isOutgoing;
    bool isEmergency;
    bool isConference;
    bool isSwitchable;
    bool isMergeable;

    aParam->GetClientId(&clientId);
    aParam->GetCallIndex(&callIndex);
    aParam->GetCallState(&callState);
    aParam->GetNumber(number);
    aParam->GetNumberPresentation(&numberPresentation);
    aParam->GetName(name);
    aParam->GetNamePresentation(&namePresentation);
    aParam->GetIsOutgoing(&isOutgoing);
    aParam->GetIsEmergency(&isEmergency);
    aParam->GetIsConference(&isConference);
    aParam->GetIsSwitchable(&isSwitchable);
    aParam->GetIsMergeable(&isMergeable);

    WriteParam(aMsg, clientId);
    WriteParam(aMsg, callIndex);
    WriteParam(aMsg, callState);
    WriteParam(aMsg, number);
    WriteParam(aMsg, numberPresentation);
    WriteParam(aMsg, name);
    WriteParam(aMsg, namePresentation);
    WriteParam(aMsg, isOutgoing);
    WriteParam(aMsg, isEmergency);
    WriteParam(aMsg, isConference);
    WriteParam(aMsg, isSwitchable);
    WriteParam(aMsg, isMergeable);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    // Check if is the null pointer we have transfered.
    bool isNull;
    if (!ReadParam(aMsg, aIter, &isNull)) {
      return false;
    }

    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    uint32_t clientId;
    uint32_t callIndex;
    uint16_t callState;
    nsString number;
    uint16_t numberPresentation;
    nsString name;
    uint16_t namePresentation;
    bool isOutgoing;
    bool isEmergency;
    bool isConference;
    bool isSwitchable;
    bool isMergeable;

    // It's not important to us where it fails, but rather if it fails
    if (!(ReadParam(aMsg, aIter, &clientId) &&
          ReadParam(aMsg, aIter, &callIndex) &&
          ReadParam(aMsg, aIter, &callState) &&
          ReadParam(aMsg, aIter, &number) &&
          ReadParam(aMsg, aIter, &numberPresentation) &&
          ReadParam(aMsg, aIter, &name) &&
          ReadParam(aMsg, aIter, &namePresentation) &&
          ReadParam(aMsg, aIter, &isOutgoing) &&
          ReadParam(aMsg, aIter, &isEmergency) &&
          ReadParam(aMsg, aIter, &isConference) &&
          ReadParam(aMsg, aIter, &isSwitchable) &&
          ReadParam(aMsg, aIter, &isMergeable))) {
      return false;
    }

    nsCOMPtr<nsITelephonyCallInfo> info =
        new TelephonyCallInfo(clientId, callIndex, callState, number,
                              numberPresentation, name, namePresentation,
                              isOutgoing, isEmergency, isConference,
                              isSwitchable, isMergeable);

    info.forget(aResult);

    return true;
  }
};


} // namespace IPC

#endif // mozilla_dom_telephony_TelephonyIPCSerializer_h
