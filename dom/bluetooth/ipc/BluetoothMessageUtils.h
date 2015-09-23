/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_ipc_BluetoothMessageUtils_h
#define mozilla_dom_bluetooth_ipc_BluetoothMessageUtils_h

#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "ipc/IPCMessageUtils.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothObjectType>
  : public ContiguousEnumSerializer<
             mozilla::dom::bluetooth::BluetoothObjectType,
             mozilla::dom::bluetooth::TYPE_MANAGER,
             mozilla::dom::bluetooth::NUM_TYPE>
{ };

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothSspVariant>
  : public ContiguousEnumSerializer<
             mozilla::dom::bluetooth::BluetoothSspVariant,
             mozilla::dom::bluetooth::SSP_VARIANT_PASSKEY_CONFIRMATION,
             mozilla::dom::bluetooth::NUM_SSP_VARIANT>
{ };

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothStatus>
  : public ContiguousEnumSerializer<
             mozilla::dom::bluetooth::BluetoothStatus,
             mozilla::dom::bluetooth::STATUS_SUCCESS,
             mozilla::dom::bluetooth::NUM_STATUS>
{ };

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothGattWriteType>
  : public ContiguousEnumSerializer<
             mozilla::dom::bluetooth::BluetoothGattWriteType,
             mozilla::dom::bluetooth::GATT_WRITE_TYPE_NO_RESPONSE,
             mozilla::dom::bluetooth::GATT_WRITE_TYPE_END_GUARD>
{ };

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothGattAuthReq>
  : public ContiguousEnumSerializer<
             mozilla::dom::bluetooth::BluetoothGattAuthReq,
             mozilla::dom::bluetooth::GATT_AUTH_REQ_NONE,
             mozilla::dom::bluetooth::GATT_AUTH_REQ_END_GUARD>
{ };

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothUuid>
{
  typedef mozilla::dom::bluetooth::BluetoothUuid paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    for (uint8_t i = 0; i < 16; i++) {
      WriteParam(aMsg, aParam.mUuid[i]);
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    for (uint8_t i = 0; i < 16; i++) {
      if (!ReadParam(aMsg, aIter, &(aResult->mUuid[i]))) {
        return false;
      }
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothGattId>
{
  typedef mozilla::dom::bluetooth::BluetoothGattId paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mUuid);
    WriteParam(aMsg, aParam.mInstanceId);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mUuid)) ||
        !ReadParam(aMsg, aIter, &(aResult->mInstanceId))) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothGattServiceId>
{
  typedef mozilla::dom::bluetooth::BluetoothGattServiceId paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mId);
    WriteParam(aMsg, aParam.mIsPrimary);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mId)) ||
        !ReadParam(aMsg, aIter, &(aResult->mIsPrimary))) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothGattCharAttribute>
{
  typedef mozilla::dom::bluetooth::BluetoothGattCharAttribute paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mId);
    WriteParam(aMsg, aParam.mProperties);
    WriteParam(aMsg, aParam.mWriteType);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mId)) ||
        !ReadParam(aMsg, aIter, &(aResult->mProperties)) ||
        !ReadParam(aMsg, aIter, &(aResult->mWriteType))) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothAttributeHandle>
{
  typedef mozilla::dom::bluetooth::BluetoothAttributeHandle paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mHandle);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mHandle))) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::bluetooth::BluetoothGattResponse>
{
  typedef mozilla::dom::bluetooth::BluetoothGattResponse paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mHandle);
    WriteParam(aMsg, aParam.mOffset);
    WriteParam(aMsg, aParam.mLength);
    WriteParam(aMsg, aParam.mAuthReq);
    for (uint16_t i = 0; i < aParam.mLength; i++) {
      WriteParam(aMsg, aParam.mValue[i]);
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mHandle)) ||
        !ReadParam(aMsg, aIter, &(aResult->mOffset)) ||
        !ReadParam(aMsg, aIter, &(aResult->mLength)) ||
        !ReadParam(aMsg, aIter, &(aResult->mAuthReq))) {
      return false;
    }

    for (uint16_t i = 0; i < aResult->mLength; i++) {
      if (!ReadParam(aMsg, aIter, &(aResult->mValue[i]))) {
        return false;
      }
    }

    return true;
  }
};
} // namespace IPC

#endif // mozilla_dom_bluetooth_ipc_BluetoothMessageUtils_h
