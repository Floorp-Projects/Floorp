/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"

#include "mozilla/dom/BluetoothAttributeEvent.h"
#include "mozilla/dom/BluetoothDevice2Binding.h"
#include "mozilla/dom/bluetooth/BluetoothClassOfDevice.h"
#include "mozilla/dom/bluetooth/BluetoothDevice.h"
#include "mozilla/dom/bluetooth/BluetoothGatt.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/dom/Promise.h"

using namespace mozilla;
using namespace mozilla::dom;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothDevice)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BluetoothDevice,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCod)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGatt)

  /**
   * Unregister the bluetooth signal handler after unlinked.
   *
   * This is needed to avoid ending up with exposing a deleted object to JS or
   * accessing deleted objects while receiving signals from parent process
   * after unlinked. Please see Bug 1138267 for detail informations.
   */
  UnregisterBluetoothSignalHandler(tmp->mAddress, tmp);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BluetoothDevice,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCod)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGatt)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothDevice)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BluetoothDevice, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BluetoothDevice, DOMEventTargetHelper)

class FetchUuidsTask final : public BluetoothReplyRunnable
{
public:
  FetchUuidsTask(Promise* aPromise,
                 BluetoothDevice* aDevice)
    : BluetoothReplyRunnable(nullptr, aPromise)
    , mDevice(aDevice)
  {
    MOZ_ASSERT(aPromise);
    MOZ_ASSERT(aDevice);
  }

  bool ParseSuccessfulReply(JS::MutableHandle<JS::Value> aValue)
  {
    aValue.setUndefined();

    const BluetoothValue& v = mReply->get_BluetoothReplySuccess().value();
    NS_ENSURE_TRUE(v.type() == BluetoothValue::TArrayOfnsString, false);
    const InfallibleTArray<nsString>& uuids = v.get_ArrayOfnsString();

    AutoJSAPI jsapi;
    NS_ENSURE_TRUE(jsapi.Init(mDevice->GetParentObject()), false);

    JSContext* cx = jsapi.cx();
    if (!ToJSValue(cx, uuids, aValue)) {
      BT_WARNING("Cannot create JS array!");
      JS_ClearPendingException(cx);
      return false;
    }

    return true;
  }

  virtual void ReleaseMembers() override
  {
    BluetoothReplyRunnable::ReleaseMembers();
    mDevice = nullptr;
  }

private:
  nsRefPtr<BluetoothDevice> mDevice;
};

BluetoothDevice::BluetoothDevice(nsPIDOMWindow* aWindow,
                                 const BluetoothValue& aValue)
  : DOMEventTargetHelper(aWindow)
  , mPaired(false)
  , mType(BluetoothDeviceType::Unknown)
{
  MOZ_ASSERT(aWindow);

  mCod = BluetoothClassOfDevice::Create(aWindow);

  const InfallibleTArray<BluetoothNamedValue>& values =
    aValue.get_ArrayOfBluetoothNamedValue();
  for (uint32_t i = 0; i < values.Length(); ++i) {
    SetPropertyByValue(values[i]);
  }

  RegisterBluetoothSignalHandler(mAddress, this);
}

BluetoothDevice::~BluetoothDevice()
{
  UnregisterBluetoothSignalHandler(mAddress, this);
}

void
BluetoothDevice::DisconnectFromOwner()
{
  DOMEventTargetHelper::DisconnectFromOwner();
  UnregisterBluetoothSignalHandler(mAddress, this);
}

BluetoothDeviceType
BluetoothDevice::ConvertUint32ToDeviceType(const uint32_t aValue)
{
  static const BluetoothDeviceType sDeviceType[] = {
    CONVERT(TYPE_OF_DEVICE_BREDR, BluetoothDeviceType::Classic),
    CONVERT(TYPE_OF_DEVICE_BLE, BluetoothDeviceType::Le),
    CONVERT(TYPE_OF_DEVICE_DUAL, BluetoothDeviceType::Dual),
  };

  BluetoothTypeOfDevice type = static_cast<BluetoothTypeOfDevice>(aValue);
  if (type >= MOZ_ARRAY_LENGTH(sDeviceType)) {
    return BluetoothDeviceType::Unknown;
  }
  return sDeviceType[type];
}

void
BluetoothDevice::SetPropertyByValue(const BluetoothNamedValue& aValue)
{
  const nsString& name = aValue.name();
  const BluetoothValue& value = aValue.value();
  if (name.EqualsLiteral("Name")) {
    mName = value.get_nsString();
  } else if (name.EqualsLiteral("Address")) {
    mAddress = value.get_nsString();
  } else if (name.EqualsLiteral("Cod")) {
    mCod->Update(value.get_uint32_t());
  } else if (name.EqualsLiteral("Paired")) {
    mPaired = value.get_bool();
  } else if (name.EqualsLiteral("UUIDs")) {
    // We assume the received uuids array is sorted without duplicate items.
    // If it's not, we require additional processing before assigning it
    // directly.
    mUuids = value.get_ArrayOfnsString();
    BluetoothDeviceBinding::ClearCachedUuidsValue(this);
  } else if (name.EqualsLiteral("Type")) {
    mType = ConvertUint32ToDeviceType(value.get_uint32_t());
  } else if (name.EqualsLiteral("GattAdv")) {
    MOZ_ASSERT(value.type() == BluetoothValue::TArrayOfuint8_t);
    nsTArray<uint8_t> advData;
    advData = value.get_ArrayOfuint8_t();
    UpdatePropertiesFromAdvData(advData);
  } else {
    BT_WARNING("Not handling device property: %s",
               NS_ConvertUTF16toUTF8(name).get());
  }
}

already_AddRefed<Promise>
BluetoothDevice::FetchUuids(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  // Ensure BluetoothService is available
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  BT_ENSURE_TRUE_REJECT(
    NS_SUCCEEDED(
      bs->FetchUuidsInternal(mAddress, new FetchUuidsTask(promise, this))),
    promise, NS_ERROR_DOM_OPERATION_ERR);

  return promise.forget();
}

// static
already_AddRefed<BluetoothDevice>
BluetoothDevice::Create(nsPIDOMWindow* aWindow,
                        const BluetoothValue& aValue)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  nsRefPtr<BluetoothDevice> device = new BluetoothDevice(aWindow, aValue);
  return device.forget();
}

void
BluetoothDevice::Notify(const BluetoothSignal& aData)
{
  BT_LOGD("[D] %s", NS_ConvertUTF16toUTF8(aData.name()).get());
  NS_ENSURE_TRUE_VOID(mSignalRegistered);

  BluetoothValue v = aData.value();
  if (aData.name().EqualsLiteral("PropertyChanged")) {
    HandlePropertyChanged(v);
  } else {
    BT_WARNING("Not handling device signal: %s",
               NS_ConvertUTF16toUTF8(aData.name()).get());
  }
}

BluetoothDeviceAttribute
BluetoothDevice::ConvertStringToDeviceAttribute(const nsAString& aString)
{
  using namespace
    mozilla::dom::BluetoothDeviceAttributeValues;

  for (size_t index = 0; index < ArrayLength(strings) - 1; index++) {
    if (aString.LowerCaseEqualsASCII(strings[index].value,
                                     strings[index].length)) {
      return static_cast<BluetoothDeviceAttribute>(index);
    }
  }

  return BluetoothDeviceAttribute::Unknown;
}

bool
BluetoothDevice::IsDeviceAttributeChanged(BluetoothDeviceAttribute aType,
                                          const BluetoothValue& aValue)
{
  switch (aType) {
    case BluetoothDeviceAttribute::Cod:
      MOZ_ASSERT(aValue.type() == BluetoothValue::Tuint32_t);
      return !mCod->Equals(aValue.get_uint32_t());
    case BluetoothDeviceAttribute::Name:
      MOZ_ASSERT(aValue.type() == BluetoothValue::TnsString);
      return !mName.Equals(aValue.get_nsString());
    case BluetoothDeviceAttribute::Paired:
      MOZ_ASSERT(aValue.type() == BluetoothValue::Tbool);
      return mPaired != aValue.get_bool();
    case BluetoothDeviceAttribute::Uuids: {
      MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfnsString);
      const InfallibleTArray<nsString>& uuids = aValue.get_ArrayOfnsString();
      // We assume the received uuids array is sorted without duplicate items.
      // If it's not, we require additional processing before comparing it
      // directly.
      return mUuids != uuids;
    }
    default:
      BT_WARNING("Type %d is not handled", uint32_t(aType));
      return false;
  }
}

void
BluetoothDevice::HandlePropertyChanged(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  Sequence<nsString> types;
  for (uint32_t i = 0, propCount = arr.Length(); i < propCount; ++i) {
    BluetoothDeviceAttribute type =
      ConvertStringToDeviceAttribute(arr[i].name());

    // Non-BluetoothDeviceAttribute properties
    if (type == BluetoothDeviceAttribute::Unknown) {
      SetPropertyByValue(arr[i]);
      continue;
    }

    // BluetoothDeviceAttribute properties
    if (IsDeviceAttributeChanged(type, arr[i].value())) {
      SetPropertyByValue(arr[i]);
      BT_APPEND_ENUM_STRING_FALLIBLE(types, BluetoothDeviceAttribute, type);
    }
  }

  if (types.IsEmpty()) {
    // No device attribute changed
    return;
  }

  DispatchAttributeEvent(types);
}

void
BluetoothDevice::DispatchAttributeEvent(const Sequence<nsString>& aTypes)
{
  MOZ_ASSERT(!aTypes.IsEmpty());

  BluetoothAttributeEventInit init;
  init.mAttrs = aTypes;
  nsRefPtr<BluetoothAttributeEvent> event =
    BluetoothAttributeEvent::Constructor(
      this, NS_LITERAL_STRING(ATTRIBUTE_CHANGED_ID), init);

  DispatchTrustedEvent(event);
}

BluetoothGatt*
BluetoothDevice::GetGatt()
{
  NS_ENSURE_TRUE(mType == BluetoothDeviceType::Le ||
                 mType == BluetoothDeviceType::Dual,
                 nullptr);
  if (!mGatt) {
    mGatt = new BluetoothGatt(GetOwner(), mAddress);
  }

  return mGatt;
}

void
BluetoothDevice::UpdatePropertiesFromAdvData(const nsTArray<uint8_t>& aAdvData)
{
  // According to BT Core Spec. Vol 3 - Ch 11, advertisement data consists of a
  // significant part and a non-significant part.
  // The significant part contains a sequence of AD structures. Each AD
  // structure shall have a Length field of one octet, which contains the
  // Length value, and a Data field of Length octets.
  unsigned int offset = 0;
  while (offset < aAdvData.Length()) {
    int dataFieldLength = aAdvData[offset++];

    // According to BT Core Spec, it only occurs to allow an early termination
    // of the Advertising data.
    if (dataFieldLength <= 0) {
      break;
    }

    // Length of the data field which is composed by AD type (1 byte) and
    // AD data (dataFieldLength -1 bytes)
    int dataLength = dataFieldLength - 1;
    if (offset + dataLength >= aAdvData.Length()) {
      break;
    }

    // Update UUIDs and name of BluetoothDevice.
    int type = aAdvData[offset++];
    switch (type) {
      case GAP_INCOMPLETE_UUID16:
      case GAP_COMPLETE_UUID16:
      case GAP_INCOMPLETE_UUID32:
      case GAP_COMPLETE_UUID32:
      case GAP_INCOMPLETE_UUID128:
      case GAP_COMPLETE_UUID128: {
        mUuids.Clear();

        // The length of uint16_t UUID array
        uint8_t len = 0;
        if (GAP_INCOMPLETE_UUID16 && GAP_COMPLETE_UUID16) {
          len = 1;
        } else if (GAP_INCOMPLETE_UUID32 && GAP_COMPLETE_UUID32) {
          len = 2;
        } else {
          len = 8;
        }
        uint16_t uuid[len];

        while (dataLength > 0) {
          // Read (len * 2) bytes from the data buffer and compose a 16-bits
          // UUID array.
          for (uint8_t i = 0; i < len; ++i) {
            uuid[i] = aAdvData[offset++];
            uuid[i] += (aAdvData[offset++] << 8);
            dataLength -= 2;
          }

          char uuidStr[37]; // one more char to be null-terminated
          if (type == GAP_INCOMPLETE_UUID16 || type == GAP_COMPLETE_UUID16) {
            // Convert 16-bits UUID into string.
            snprintf(uuidStr, sizeof(uuidStr),
                     "0000%04x-0000-1000-8000-00805f9b34fb", uuid[0]);
          } else if (type == GAP_INCOMPLETE_UUID32 ||
                     type == GAP_COMPLETE_UUID32) {
            // Convert 32-bits UUID into string.
            snprintf(uuidStr, sizeof(uuidStr),
                     "%04x%04x-0000-1000-8000-00805f9b34fb", uuid[1], uuid[0]);
          } else if (type == GAP_INCOMPLETE_UUID128 ||
                     type == GAP_COMPLETE_UUID128) {
            // Convert 128-bits UUID into string.
            snprintf(uuidStr, sizeof(uuidStr),
                     "%04x%04x-%04x-%04x-%04x-%04x%04x%04x",
                     uuid[7], uuid[6], uuid[5], uuid[4],
                     uuid[3], uuid[2], uuid[1], uuid[0]);
          }
          nsString uuidNsString;
          uuidNsString.AssignLiteral(uuidStr);

          mUuids.AppendElement(uuidNsString);
        }

        BluetoothDeviceBinding::ClearCachedUuidsValue(this);
        break;
      }
      case GAP_SHORTENED_NAME:
        if (!mName.IsEmpty()) break;
      case GAP_COMPLETE_NAME: {
        // Read device name from data buffer.
        char deviceName[dataLength];
        for (int i = 0; i < dataLength; ++i) {
          deviceName[i] = aAdvData[offset++];
        }

        mName.AssignASCII(deviceName, dataLength);
        break;
      }
      default:
        offset += dataLength;
        break;
    }
  }
}

JSObject*
BluetoothDevice::WrapObject(JSContext* aContext,
                            JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothDeviceBinding::Wrap(aContext, this, aGivenProto);
}
