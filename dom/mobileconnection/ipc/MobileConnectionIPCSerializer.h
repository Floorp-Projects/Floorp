/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_mobileconnection_src_ipc_MobileConnectionIPCSerialiser_h
#define dom_mobileconnection_src_ipc_MobileConnectionIPCSerialiser_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/MobileCellInfo.h"
#include "mozilla/dom/MobileConnectionInfo.h"
#include "mozilla/dom/MobileNetworkInfo.h"
#include "MozMobileConnectionBinding.h"

using mozilla::AutoSafeJSContext;
using mozilla::dom::MobileNetworkInfo;
using mozilla::dom::MobileCellInfo;
using mozilla::dom::MobileConnectionInfo;

typedef nsIMobileCellInfo* nsMobileCellInfo;
typedef nsIMobileConnectionInfo* nsMobileConnectionInfo;
typedef nsIMobileNetworkInfo* nsMobileNetworkInfo;

namespace IPC {

struct MozCallForwardingOptions : public mozilla::dom::MozCallForwardingOptions
{
  bool operator==(const MozCallForwardingOptions& aOther) const
  {
    return // Compare mActive
           ((!mActive.WasPassed() && !aOther.mActive.WasPassed()) ||
            (mActive.WasPassed() && aOther.mActive.WasPassed() &&
             mActive.Value() == aOther.mActive.Value())) &&
           // Compare mAction
           ((!mAction.WasPassed() && !aOther.mAction.WasPassed()) ||
            (mAction.WasPassed() && aOther.mAction.WasPassed() &&
             mAction.Value() == aOther.mAction.Value())) &&
           // Compare mReason
           ((!mReason.WasPassed() && !aOther.mReason.WasPassed()) ||
            (mReason.WasPassed() && aOther.mReason.WasPassed() &&
             mReason.Value() == aOther.mReason.Value())) &&
           // Compare mNumber
           ((!mNumber.WasPassed() && !aOther.mNumber.WasPassed()) ||
            (mNumber.WasPassed() && aOther.mNumber.WasPassed() &&
             mNumber.Value() == aOther.mNumber.Value())) &&
           // Compare mTimeSeconds
           ((!mTimeSeconds.WasPassed() && !aOther.mTimeSeconds.WasPassed()) ||
            (mTimeSeconds.WasPassed() && aOther.mTimeSeconds.WasPassed() &&
             mTimeSeconds.Value() == aOther.mTimeSeconds.Value())) &&
           // Compare mServiceClass
           ((!mServiceClass.WasPassed() && !aOther.mServiceClass.WasPassed()) ||
            (mServiceClass.WasPassed() && aOther.mServiceClass.WasPassed() &&
             mServiceClass.Value() == aOther.mServiceClass.Value()));
  };
};

struct MozCallBarringOptions : mozilla::dom::MozCallBarringOptions
{
  bool operator==(const MozCallBarringOptions& aOther) const
  {
    return // Compare mEnabled
           ((!mEnabled.WasPassed() && !aOther.mEnabled.WasPassed()) ||
            (mEnabled.WasPassed() && aOther.mEnabled.WasPassed() &&
             mEnabled.Value() == aOther.mEnabled.Value())) &&
           // Compare mPassword
           ((!mPassword.WasPassed() && !aOther.mPassword.WasPassed()) ||
            (mPassword.WasPassed() && aOther.mPassword.WasPassed() &&
             mPassword.Value() == aOther.mPassword.Value())) &&
           // Compare mProgram
           ((!mProgram.WasPassed() && !aOther.mProgram.WasPassed()) ||
            (mProgram.WasPassed() && aOther.mProgram.WasPassed() &&
             mProgram.Value() == aOther.mProgram.Value())) &&
           // Compare mServiceClass
           ((!mServiceClass.WasPassed() && !aOther.mServiceClass.WasPassed()) ||
            (mServiceClass.WasPassed() && aOther.mServiceClass.WasPassed() &&
             mServiceClass.Value() == aOther.mServiceClass.Value()));
  };
};

/**
 * nsIMobileNetworkInfo Serialize/De-serialize.
 */
template <>
struct ParamTraits<nsIMobileNetworkInfo*>
{
  typedef nsIMobileNetworkInfo* paramType;

  // Function to serialize a MobileNetworkInfo.
  static void Write(Message *aMsg, const paramType& aParam)
  {
    bool isNull = !aParam;
    WriteParam(aMsg, isNull);
    // If it is a null object, then we are done.
    if (isNull) {
      return;
    }

    nsString pString;
    aParam->GetShortName(pString);
    WriteParam(aMsg, pString);

    aParam->GetLongName(pString);
    WriteParam(aMsg, pString);

    aParam->GetMcc(pString);
    WriteParam(aMsg, pString);

    aParam->GetMnc(pString);
    WriteParam(aMsg, pString);

    aParam->GetState(pString);
    WriteParam(aMsg, pString);

    // We release the ref here given that ipdl won't handle reference counting.
    aParam->Release();
  }

  // Function to de-serialize a MobileNetworkInfo.
  static bool Read(const Message *aMsg, void **aIter, paramType* aResult)
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

    nsString shortName;
    nsString longName;
    nsString mcc;
    nsString mnc;
    nsString state;

    // It's not important to us where it fails, but rather if it fails
    if (!(ReadParam(aMsg, aIter, &shortName) &&
          ReadParam(aMsg, aIter, &longName) &&
          ReadParam(aMsg, aIter, &mcc) &&
          ReadParam(aMsg, aIter, &mnc) &&
          ReadParam(aMsg, aIter, &state))) {
      return false;
    }

    *aResult = new MobileNetworkInfo(shortName,
                                     longName,
                                     mcc,
                                     mnc,
                                     state);
    // We release this ref after receiver finishes processing.
    NS_ADDREF(*aResult);

    return true;
  }
};

/**
 * nsIMobileCellInfo Serialize/De-serialize.
 */
template <>
struct ParamTraits<nsIMobileCellInfo*>
{
  typedef nsIMobileCellInfo* paramType;

  // Function to serialize a MobileCellInfo.
  static void Write(Message *aMsg, const paramType& aParam)
  {
    bool isNull = !aParam;
    WriteParam(aMsg, isNull);
    // If it is a null object, then we are done.
    if (isNull) {
      return;
    }

    int32_t pLong;
    int64_t pLongLong;

    aParam->GetGsmLocationAreaCode(&pLong);
    WriteParam(aMsg, pLong);

    aParam->GetGsmCellId(&pLongLong);
    WriteParam(aMsg, pLongLong);

    aParam->GetCdmaBaseStationId(&pLong);
    WriteParam(aMsg, pLong);

    aParam->GetCdmaBaseStationLatitude(&pLong);
    WriteParam(aMsg, pLong);

    aParam->GetCdmaBaseStationLongitude(&pLong);
    WriteParam(aMsg, pLong);

    aParam->GetCdmaSystemId(&pLong);
    WriteParam(aMsg, pLong);

    aParam->GetCdmaNetworkId(&pLong);
    WriteParam(aMsg, pLong);

    // We release the ref here given that ipdl won't handle reference counting.
    aParam->Release();
  }

  // Function to de-serialize a MobileCellInfo.
  static bool Read(const Message *aMsg, void **aIter, paramType* aResult)
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

    int32_t gsmLac;
    int64_t gsmCellId;
    int32_t cdmaBsId;
    int32_t cdmaBsLat;
    int32_t cdmaBsLong;
    int32_t cdmaSystemId;
    int32_t cdmaNetworkId;

    // It's not important to us where it fails, but rather if it fails
    if (!(ReadParam(aMsg, aIter, &gsmLac) &&
          ReadParam(aMsg, aIter, &gsmCellId) &&
          ReadParam(aMsg, aIter, &cdmaBsId) &&
          ReadParam(aMsg, aIter, &cdmaBsLat) &&
          ReadParam(aMsg, aIter, &cdmaBsLong) &&
          ReadParam(aMsg, aIter, &cdmaSystemId) &&
          ReadParam(aMsg, aIter, &cdmaNetworkId))) {
      return false;
    }

    *aResult = new MobileCellInfo(gsmLac, gsmCellId, cdmaBsId, cdmaBsLat,
                                  cdmaBsLong, cdmaSystemId, cdmaNetworkId);
    // We release this ref after receiver finishes processing.
    NS_ADDREF(*aResult);

    return true;
  }
};

/**
 * nsIMobileConnectionInfo Serialize/De-serialize.
 */
template <>
struct ParamTraits<nsIMobileConnectionInfo*>
{
  typedef nsIMobileConnectionInfo* paramType;

  // Function to serialize a MobileConnectionInfo.
  static void Write(Message *aMsg, const paramType& aParam)
  {
    bool isNull = !aParam;
    WriteParam(aMsg, isNull);
    // If it is a null object, then we are done.
    if (isNull) {
      return;
    }

    AutoSafeJSContext cx;
    nsString pString;
    bool pBool;
    nsCOMPtr<nsIMobileNetworkInfo> pNetworkInfo;
    nsCOMPtr<nsIMobileCellInfo> pCellInfo;
    JS::Rooted<JS::Value> pJsval(cx);
    int32_t pInt32;

    aParam->GetState(pString);
    WriteParam(aMsg, pString);

    aParam->GetConnected(&pBool);
    WriteParam(aMsg, pBool);

    aParam->GetEmergencyCallsOnly(&pBool);
    WriteParam(aMsg, pBool);

    aParam->GetRoaming(&pBool);
    WriteParam(aMsg, pBool);

    aParam->GetType(pString);
    WriteParam(aMsg, pString);

    aParam->GetNetwork(getter_AddRefs(pNetworkInfo));
    // Release ref when WriteParam is finished.
    WriteParam(aMsg, pNetworkInfo.forget().take());

    aParam->GetCell(getter_AddRefs(pCellInfo));
    // Release ref when WriteParam is finished.
    WriteParam(aMsg, pCellInfo.forget().take());

    // Serialize jsval signalStrength
    aParam->GetSignalStrength(&pJsval);
    isNull = !pJsval.isInt32();
    WriteParam(aMsg, isNull);

    if (!isNull) {
      pInt32 = pJsval.toInt32();
      WriteParam(aMsg, pInt32);
    }

    // Serialize jsval relSignalStrength
    aParam->GetRelSignalStrength(&pJsval);
    isNull = !pJsval.isInt32();
    WriteParam(aMsg, isNull);

    if (!isNull) {
      pInt32 = pJsval.toInt32();
      WriteParam(aMsg, pInt32);
    }

    // We release the ref here given that ipdl won't handle reference counting.
    aParam->Release();
  }

  // Function to de-serialize a MobileConectionInfo.
  static bool Read(const Message* aMsg, void **aIter, paramType* aResult)
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

    AutoSafeJSContext cx;
    nsString state;
    bool connected;
    bool emergencyOnly;
    bool roaming;
    nsString type;
    nsIMobileNetworkInfo* networkInfo = nullptr;
    nsIMobileCellInfo* cellInfo = nullptr;
    Nullable<int32_t> signalStrength;
    Nullable<uint16_t> relSignalStrength;

    // It's not important to us where it fails, but rather if it fails
    if (!(ReadParam(aMsg, aIter, &state) &&
          ReadParam(aMsg, aIter, &connected) &&
          ReadParam(aMsg, aIter, &emergencyOnly) &&
          ReadParam(aMsg, aIter, &roaming) &&
          ReadParam(aMsg, aIter, &type) &&
          ReadParam(aMsg, aIter, &networkInfo) &&
          ReadParam(aMsg, aIter, &cellInfo))) {
      return false;
    }

    // De-serialize jsval signalStrength
    if (!ReadParam(aMsg, aIter, &isNull)) {
      return false;
    }

    if (!isNull) {
      int32_t value;

      if (!ReadParam(aMsg, aIter, &value)) {
        return false;
      }

      signalStrength.SetValue(value);
    }

    // De-serialize jsval relSignalStrength
    if (!ReadParam(aMsg, aIter, &isNull)) {
      return false;
    }

    if (!isNull) {
      int32_t value;

      if (!ReadParam(aMsg, aIter, &value)) {
        return false;
      }

      relSignalStrength.SetValue(uint16_t(value));
    }

    *aResult = new MobileConnectionInfo(state,
                                        connected,
                                        emergencyOnly,
                                        roaming,
                                        networkInfo,
                                        type,
                                        signalStrength,
                                        relSignalStrength,
                                        cellInfo);
    // We release this ref after receiver finishes processing.
    NS_ADDREF(*aResult);
    // We already clone the data into MobileConnectionInfo, so release the ref
    // of networkInfo and cellInfo here.
    NS_IF_RELEASE(networkInfo);
    NS_IF_RELEASE(cellInfo);

    return true;
  }
};

/**
 * MozCallForwardingOptions Serialize/De-serialize.
 */
template <>
struct ParamTraits<MozCallForwardingOptions>
{
  typedef MozCallForwardingOptions paramType;

  // Function to serialize a MozCallForwardingOptions.
  static void Write(Message *aMsg, const paramType& aParam)
  {
    bool wasPassed = false;
    bool isNull = false;

    // Write mActive
    wasPassed = aParam.mActive.WasPassed();
    WriteParam(aMsg, wasPassed);
    if (wasPassed) {
      isNull = aParam.mActive.Value().IsNull();
      WriteParam(aMsg, isNull);
      if (!isNull) {
        WriteParam(aMsg, aParam.mActive.Value().Value());
      }
    }

    // Write mAction
    wasPassed = aParam.mAction.WasPassed();
    WriteParam(aMsg, wasPassed);
    if (wasPassed) {
      isNull = aParam.mAction.Value().IsNull();
      WriteParam(aMsg, isNull);
      if (!isNull) {
        WriteParam(aMsg, aParam.mAction.Value().Value());
      }
    }

    // Write mReason
    wasPassed = aParam.mReason.WasPassed();
    WriteParam(aMsg, wasPassed);
    if (wasPassed) {
      isNull = aParam.mReason.Value().IsNull();
      WriteParam(aMsg, isNull);
      if (!isNull) {
        WriteParam(aMsg, aParam.mReason.Value().Value());
      }
    }

    // Write mNumber
    wasPassed = aParam.mNumber.WasPassed();
    WriteParam(aMsg, wasPassed);
    if (wasPassed) {
      WriteParam(aMsg, aParam.mNumber.Value());
    }

    // Write mTimeSeconds
    wasPassed = aParam.mTimeSeconds.WasPassed();
    WriteParam(aMsg, wasPassed);
    if (wasPassed) {
      isNull = aParam.mTimeSeconds.Value().IsNull();
      WriteParam(aMsg, isNull);
      if (!isNull) {
        WriteParam(aMsg, aParam.mTimeSeconds.Value().Value());
      }
    }

    // Write mServiceClass
    wasPassed = aParam.mServiceClass.WasPassed();
    WriteParam(aMsg, wasPassed);
    if (wasPassed) {
      isNull = aParam.mServiceClass.Value().IsNull();
      WriteParam(aMsg, isNull);
      if (!isNull) {
        WriteParam(aMsg, aParam.mServiceClass.Value().Value());
      }
    }
  }

  // Function to de-serialize a MozCallForwardingOptions.
  static bool Read(const Message *aMsg, void **aIter, paramType* aResult)
  {
    bool wasPassed = false;
    bool isNull = false;

    // Read mActive
    if (!ReadParam(aMsg, aIter, &wasPassed)) {
      return false;
    }
    if (wasPassed) {
      aResult->mActive.Construct();
      if (!ReadParam(aMsg, aIter, &isNull)) {
        return false;
      }

      if (!isNull) {
        if (!ReadParam(aMsg, aIter, &aResult->mActive.Value().SetValue())) {
          return false;
        }
      }
    }

    // Read mAction
    if (!ReadParam(aMsg, aIter, &wasPassed)) {
      return false;
    }
    if (wasPassed) {
      aResult->mAction.Construct();
      if (!ReadParam(aMsg, aIter, &isNull)) {
        return false;
      }

      if (!isNull) {
        if (!ReadParam(aMsg, aIter, &aResult->mAction.Value().SetValue())) {
          return false;
        }
      }
    }

    // Read mReason
    if (!ReadParam(aMsg, aIter, &wasPassed)) {
      return false;
    }
    if (wasPassed) {
      aResult->mReason.Construct();
      if (!ReadParam(aMsg, aIter, &isNull)) {
        return false;
      }

      if (!isNull) {
        if (!ReadParam(aMsg, aIter, &aResult->mReason.Value().SetValue())) {
          return false;
        }
      }
    }

    // Read mNumber
    if (!ReadParam(aMsg, aIter, &wasPassed)) {
      return false;
    }
    if (wasPassed) {
      if (!ReadParam(aMsg, aIter, &aResult->mNumber.Construct())) {
        return false;
      }
    }

    // Read mTimeSeconds
    if (!ReadParam(aMsg, aIter, &wasPassed)) {
      return false;
    }
    if (wasPassed) {
      aResult->mTimeSeconds.Construct();
      if (!ReadParam(aMsg, aIter, &isNull)) {
        return false;
      }

      if (!isNull) {
        if (!ReadParam(aMsg, aIter, &aResult->mTimeSeconds.Value().SetValue())) {
          return false;
        }
      }
    }

    // Read mServiceClass
    if (!ReadParam(aMsg, aIter, &wasPassed)) {
      return false;
    }
    if (wasPassed) {
      aResult->mServiceClass.Construct();
      if (!ReadParam(aMsg, aIter, &isNull)) {
        return false;
      }

      if (!isNull) {
        if (!ReadParam(aMsg, aIter, &aResult->mServiceClass.Value().SetValue())) {
          return false;
        }
      }
    }

    return true;
  }
};

/**
 * MozCallBarringOptions Serialize/De-serialize.
 */
template <>
struct ParamTraits<MozCallBarringOptions>
{
  typedef MozCallBarringOptions paramType;

  // Function to serialize a MozCallBarringOptions.
  static void Write(Message *aMsg, const paramType& aParam)
  {
    bool wasPassed = false;
    bool isNull = false;

    // Write mProgram
    wasPassed = aParam.mProgram.WasPassed();
    WriteParam(aMsg, wasPassed);
    if (wasPassed) {
      isNull = aParam.mProgram.Value().IsNull();
      WriteParam(aMsg, isNull);
      if (!isNull) {
        WriteParam(aMsg, aParam.mProgram.Value().Value());
      }
    }

    // Write mEnabled
    wasPassed = aParam.mEnabled.WasPassed();
    WriteParam(aMsg, wasPassed);
    if (wasPassed) {
      isNull = aParam.mEnabled.Value().IsNull();
      WriteParam(aMsg, isNull);
      if (!isNull) {
        WriteParam(aMsg, aParam.mEnabled.Value().Value());
      }
    }

    // Write mPassword
    wasPassed = aParam.mPassword.WasPassed();
    WriteParam(aMsg, wasPassed);
    if (wasPassed) {
      WriteParam(aMsg, aParam.mPassword.Value());
    }

    // Write mServiceClass
    wasPassed = aParam.mServiceClass.WasPassed();
    WriteParam(aMsg, wasPassed);
    if (wasPassed) {
      isNull = aParam.mServiceClass.Value().IsNull();
      WriteParam(aMsg, isNull);
      if (!isNull) {
        WriteParam(aMsg, aParam.mServiceClass.Value().Value());
      }
    }

    // Write mPin
    wasPassed = aParam.mPin.WasPassed();
    WriteParam(aMsg, wasPassed);
    if (wasPassed) {
      WriteParam(aMsg, aParam.mPin.Value());
    }

    // Write mNewPin
    wasPassed = aParam.mNewPin.WasPassed();
    WriteParam(aMsg, wasPassed);
    if (wasPassed) {
      WriteParam(aMsg, aParam.mNewPin.Value());
    }
  }

  // Function to de-serialize a MozCallBarringOptions.
  static bool Read(const Message *aMsg, void **aIter, paramType* aResult)
  {
    bool wasPassed = false;
    bool isNull = false;

    // Read mProgram
    if (!ReadParam(aMsg, aIter, &wasPassed)) {
      return false;
    }
    if (wasPassed) {
      aResult->mProgram.Construct();
      if (!ReadParam(aMsg, aIter, &isNull)) {
        return false;
      }

      if (!isNull) {
        if (!ReadParam(aMsg, aIter, &aResult->mProgram.Value().SetValue())) {
          return false;
        }
      }
    }

    // Read mEnabled
    if (!ReadParam(aMsg, aIter, &wasPassed)) {
      return false;
    }
    if (wasPassed) {
      aResult->mEnabled.Construct();
      if (!ReadParam(aMsg, aIter, &isNull)) {
        return false;
      }

      if (!isNull) {
        if (!ReadParam(aMsg, aIter, &aResult->mEnabled.Value().SetValue())) {
          return false;
        }
      }
    }

    // Read mPassword
    if (!ReadParam(aMsg, aIter, &wasPassed)) {
      return false;
    }
    if (wasPassed) {
      if (!ReadParam(aMsg, aIter, &aResult->mPassword.Construct())) {
        return false;
      }
    }

    // Read mServiceClass
    if (!ReadParam(aMsg, aIter, &wasPassed)) {
      return false;
    }
    if (wasPassed) {
      aResult->mServiceClass.Construct();
      if (!ReadParam(aMsg, aIter, &isNull)) {
        return false;
      }

      if (!isNull) {
        if (!ReadParam(aMsg, aIter, &aResult->mServiceClass.Value().SetValue())) {
          return false;
        }
      }
    }

    // Read mPin
    if (!ReadParam(aMsg, aIter, &wasPassed)) {
      return false;
    }
    if (wasPassed) {
      if (!ReadParam(aMsg, aIter, &aResult->mPin.Construct())) {
        return false;
      }
    }

    // Read mNewPin
    if (!ReadParam(aMsg, aIter, &wasPassed)) {
      return false;
    }
    if (wasPassed) {
      if (!ReadParam(aMsg, aIter, &aResult->mNewPin.Construct())) {
        return false;
      }
    }

    return true;
  }
};

} // namespace IPC

#endif // dom_mobileconnection_src_ipc_MobileConnectionIPCSerialiser_h
