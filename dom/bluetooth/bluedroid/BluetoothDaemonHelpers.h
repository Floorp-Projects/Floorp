/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_BluetoothDaemonHelpers_h
#define mozilla_dom_bluetooth_bluedroid_BluetoothDaemonHelpers_h

#include "BluetoothCommon.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/ipc/DaemonSocketPDU.h"
#include "mozilla/ipc/DaemonSocketPDUHelpers.h"

BEGIN_BLUETOOTH_NAMESPACE

using mozilla::ipc::DaemonSocketPDU;
using mozilla::ipc::DaemonSocketPDUHeader;
using mozilla::ipc::DaemonSocketPDUHelpers::Convert;
using mozilla::ipc::DaemonSocketPDUHelpers::PackPDU;
using mozilla::ipc::DaemonSocketPDUHelpers::UnpackPDU;

using namespace mozilla::ipc::DaemonSocketPDUHelpers;

//
// Helper structures
//

struct BluetoothAvrcpAttributeTextPairs {
  BluetoothAvrcpAttributeTextPairs(const uint8_t* aAttr,
                                   const char** aText,
                                   size_t aLength)
    : mAttr(aAttr)
    , mText(aText)
    , mLength(aLength)
  { }

  const uint8_t* mAttr;
  const char** mText;
  size_t mLength;
};

struct BluetoothAvrcpAttributeValuePairs {
  BluetoothAvrcpAttributeValuePairs(const uint8_t* aAttr,
                                    const uint8_t* aValue,
                                    size_t aLength)
    : mAttr(aAttr)
    , mValue(aValue)
    , mLength(aLength)
  { }

  const uint8_t* mAttr;
  const uint8_t* mValue;
  size_t mLength;
};

struct BluetoothAvrcpEventParamPair {
  BluetoothAvrcpEventParamPair(BluetoothAvrcpEvent aEvent,
                               const BluetoothAvrcpNotificationParam& aParam)
    : mEvent(aEvent)
    , mParam(aParam)
  { }

  size_t GetLength()
  {
    size_t size;

    switch(mEvent) {
      case AVRCP_EVENT_PLAY_STATUS_CHANGED:
        /* PackPDU casts ControlPlayStatus to uint8_t */
        size = sizeof(static_cast<uint8_t>(mParam.mPlayStatus));
        break;
      case AVRCP_EVENT_TRACK_CHANGE:
        size = sizeof(mParam.mTrack);
        break;
      case AVRCP_EVENT_TRACK_REACHED_END:
      case AVRCP_EVENT_TRACK_REACHED_START:
        /* no data to pack */
        size = 0;
        break;
      case AVRCP_EVENT_PLAY_POS_CHANGED:
        size = sizeof(mParam.mSongPos);
        break;
      case AVRCP_EVENT_APP_SETTINGS_CHANGED:
        size = (sizeof(mParam.mIds[0]) + sizeof(mParam.mValues[0])) * mParam.mNumAttr;
        break;
      default:
        size = 0;
        break;
    }

    return size;
  }

  BluetoothAvrcpEvent mEvent;
  const BluetoothAvrcpNotificationParam& mParam;
};

//
// Conversion
//

nsresult
Convert(bool aIn, BluetoothScanMode& aOut);

nsresult
Convert(int32_t aIn, BluetoothTypeOfDevice& aOut);

nsresult
Convert(int32_t aIn, BluetoothScanMode& aOut);

nsresult
Convert(uint8_t aIn, BluetoothA2dpAudioState& aOut);

nsresult
Convert(uint8_t aIn, BluetoothA2dpConnectionState& aOut);

nsresult
Convert(uint8_t aIn, BluetoothAclState& aOut);

nsresult
Convert(uint8_t aIn, BluetoothAvrcpEvent& aOut);

nsresult
Convert(uint8_t aIn, BluetoothAvrcpMediaAttribute& aOut);

nsresult
Convert(uint8_t aIn, BluetoothAvrcpPlayerAttribute& aOut);

nsresult
Convert(uint8_t aIn, BluetoothAvrcpRemoteFeatureBits& aOut);

nsresult
Convert(uint8_t aIn, BluetoothHandsfreeAudioState& aOut);

nsresult
Convert(uint8_t aIn, BluetoothHandsfreeCallHoldType& aOut);

nsresult
Convert(uint8_t aIn, BluetoothHandsfreeConnectionState& aOut);

nsresult
Convert(uint8_t aIn, BluetoothHandsfreeNRECState& aOut);

nsresult
Convert(uint8_t aIn, BluetoothHandsfreeVoiceRecognitionState& aOut);

nsresult
Convert(uint8_t aIn, BluetoothHandsfreeVolumeType& aOut);

nsresult
Convert(uint8_t aIn, BluetoothHandsfreeWbsConfig& aOut);

nsresult
Convert(uint8_t aIn, BluetoothBondState& aOut);

nsresult
Convert(uint8_t aIn, BluetoothTypeOfDevice& aOut);

nsresult
Convert(uint8_t aIn, BluetoothPropertyType& aOut);

nsresult
Convert(uint8_t aIn, BluetoothScanMode& aOut);

nsresult
Convert(uint8_t aIn, BluetoothSspVariant& aOut);

nsresult
Convert(uint8_t aIn, BluetoothStatus& aOut);

nsresult
Convert(int32_t aIn, BluetoothAttributeHandle& aOut);

nsresult
Convert(uint8_t aIn, BluetoothHidProtocolMode& aOut);

nsresult
Convert(uint8_t aIn, BluetoothHidConnectionState& aOut);

nsresult
Convert(uint8_t aIn, BluetoothHidStatus& aOut);

nsresult
Convert(int32_t aIn, BluetoothGattStatus& aOut);

nsresult
Convert(const BluetoothAttributeHandle& aIn, int32_t& aOut);

nsresult
Convert(const BluetoothAttributeHandle& aIn, uint16_t& aOut);

nsresult
Convert(BluetoothAvrcpEvent aIn, uint8_t& aOut);

nsresult
Convert(BluetoothAvrcpNotification aIn, uint8_t& aOut);

nsresult
Convert(BluetoothAvrcpPlayerAttribute aIn, uint8_t& aOut);

nsresult
Convert(BluetoothAvrcpRemoteFeatureBits aIn, unsigned long& aOut);

nsresult
Convert(BluetoothAvrcpStatus aIn, uint8_t& aOut);

nsresult
Convert(BluetoothHandsfreeAtResponse aIn, uint8_t& aOut);

nsresult
Convert(BluetoothHandsfreeCallAddressType aIn, uint8_t& aOut);

nsresult
Convert(BluetoothHandsfreeCallDirection aIn, uint8_t& aOut);

nsresult
Convert(BluetoothHandsfreeCallState aIn, uint8_t& aOut);

nsresult
Convert(BluetoothHandsfreeCallMode aIn, uint8_t& aOut);

nsresult
Convert(BluetoothHandsfreeCallMptyType aIn, uint8_t& aOut);

nsresult
Convert(BluetoothHandsfreeNetworkState aIn, uint8_t& aOut);

nsresult
Convert(BluetoothHandsfreeServiceType aIn, uint8_t& aOut);

nsresult
Convert(BluetoothHandsfreeVolumeType aIn, uint8_t& aOut);

nsresult
Convert(BluetoothHandsfreeWbsConfig aIn, uint8_t& aOut);

nsresult
Convert(BluetoothPropertyType aIn, uint8_t& aOut);

nsresult
Convert(BluetoothScanMode aIn, uint8_t& aOut);

nsresult
Convert(BluetoothSetupServiceId aIn, uint8_t& aOut);

nsresult
Convert(BluetoothSocketType aIn, uint8_t& aOut);

nsresult
Convert(BluetoothSspVariant aIn, uint8_t& aOut);

nsresult
Convert(ControlPlayStatus aIn, uint8_t& aOut);

nsresult
Convert(BluetoothGattAuthReq aIn, int32_t& aOut);

nsresult
Convert(BluetoothGattAuthReq aIn, uint8_t& aOut);

nsresult
Convert(BluetoothGattWriteType aIn, int32_t& aOut);

nsresult
Convert(nsresult aIn, BluetoothStatus& aOut);

nsresult
Convert(BluetoothHidProtocolMode aIn, uint8_t& aOut);

nsresult
Convert(BluetoothHidReportType aIn, uint8_t& aOut);

//
// Packing
//

nsresult
PackPDU(const BluetoothAddress& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothAttributeHandle& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothAvrcpAttributeTextPairs& aIn,
        DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothAvrcpAttributeValuePairs& aIn,
        DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothAvrcpElementAttribute& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(BluetoothAvrcpEvent aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothAvrcpEventParamPair& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(BluetoothAvrcpNotification aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(BluetoothAvrcpPlayerAttribute aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(BluetoothAvrcpStatus aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothConfigurationParameter& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothHandsfreeAtResponse& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothHandsfreeCallAddressType& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothHandsfreeCallDirection& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothHandsfreeCallMode& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothHandsfreeCallMptyType& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothHandsfreeCallState& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothHandsfreeNetworkState& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothHandsfreeServiceType& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothHandsfreeVolumeType& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothHandsfreeWbsConfig& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothProperty& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothPinCode& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(BluetoothPropertyType aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothServiceName& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(BluetoothSetupServiceId aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(BluetoothSocketType aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(BluetoothSspVariant aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(BluetoothScanMode aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(ControlPlayStatus aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothUuid& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothGattId& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothGattServiceId& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(BluetoothGattAuthReq aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(BluetoothGattWriteType aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(BluetoothTransport aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothHidInfoParam& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(const BluetoothHidReport& aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(BluetoothHidProtocolMode aIn, DaemonSocketPDU& aPDU);

nsresult
PackPDU(BluetoothHidReportType aIn, DaemonSocketPDU& aPDU);

/* This implementation of |PackPDU| packs |BluetoothUuid| in reversed order.
 * (ex. reversed GATT UUID, see bug 1171866)
 */
inline nsresult
PackPDU(const PackReversed<BluetoothUuid>& aIn, DaemonSocketPDU& aPDU)
{
 return PackPDU(
   PackReversed<PackArray<uint8_t>>(
     PackArray<uint8_t>(aIn.mValue.mUuid, sizeof(aIn.mValue.mUuid))),
   aPDU);
}

//
// Unpacking
//

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothA2dpAudioState& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothA2dpConnectionState& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothAclState& aOut);

inline nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothAddress& aOut)
{
  return aPDU.Read(aOut.mAddr, sizeof(aOut.mAddr));
}

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothAttributeHandle& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothAvrcpEvent& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothAvrcpMediaAttribute& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothAvrcpPlayerAttribute& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothAvrcpPlayerSettings& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothAvrcpRemoteFeatureBits& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothBondState& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothTypeOfDevice& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothHandsfreeAudioState& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothHandsfreeCallHoldType& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothHandsfreeConnectionState& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothHandsfreeNRECState& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothHandsfreeWbsConfig& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU,
          BluetoothHandsfreeVoiceRecognitionState& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothHandsfreeVolumeType& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothRemoteInfo& aOut);

inline nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothRemoteName& aOut)
{
  nsresult rv = aPDU.Read(aOut.mName, sizeof(aOut.mName));
  if (NS_FAILED(rv)) {
    return rv;
  }
  /* The PDU stores one extra byte for the trailing \0 character. We
   * consume the byte, but don't store the character.
   */
  if (!aPDU.Consume(1)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  auto end = std::find(aOut.mName, aOut.mName + sizeof(aOut.mName), '\0');

  aOut.mLength = end - aOut.mName;
  return NS_OK;
}

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothProperty& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothPropertyType& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothScanMode& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothServiceRecord& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothSspVariant& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothStatus& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothGattStatus& aOut);

inline nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothUuid& aOut)
{
  return aPDU.Read(aOut.mUuid, sizeof(aOut.mUuid));
}

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothGattId& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothGattServiceId& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothGattReadParam& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothGattWriteParam& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothGattNotifyParam& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothHidInfoParam& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothHidReport& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothHidProtocolMode& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothHidConnectionState& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, BluetoothHidStatus& aOut);

/* This implementation of |UnpackPDU| unpacks |BluetoothUuid| in reversed
 * order. (ex. reversed GATT UUID, see bug 1171866)
 */
inline nsresult
UnpackPDU(DaemonSocketPDU& aPDU, const UnpackReversed<BluetoothUuid>& aOut)
{
  return UnpackPDU(
    aPDU,
    UnpackReversed<UnpackArray<uint8_t>>(
      UnpackArray<uint8_t>(aOut.mValue->mUuid, sizeof(aOut.mValue->mUuid))));
}

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluedroid_BluetoothDaemonHelpers_h
