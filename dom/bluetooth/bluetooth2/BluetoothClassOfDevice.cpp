/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothClassOfDevice.h"

#include "mozilla/dom/BluetoothClassOfDeviceBinding.h"
#include "nsThreadUtils.h"

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(BluetoothClassOfDevice, mOwnerWindow)
NS_IMPL_CYCLE_COLLECTING_ADDREF(BluetoothClassOfDevice)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BluetoothClassOfDevice)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BluetoothClassOfDevice)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/*
 * Class of Device(CoD): 32-bit unsigned integer
 *
 *  31   24  23    13 12     8 7      2 1 0
 * |       | Major   | Major  | Minor  |   |
 * |       | service | device | device |   |
 * |       | class   | class  | class  |   |
 * |       |<- 11  ->|<- 5  ->|<- 6  ->|   |
 *
 * https://www.bluetooth.org/en-us/specification/assigned-numbers/baseband
 */

// Bit 23 ~ Bit 13: Major service class
#define GET_MAJOR_SERVICE_CLASS(cod) (((cod) & 0xffe000) >> 13)

// Bit 12 ~ Bit 8: Major device class
#define GET_MAJOR_DEVICE_CLASS(cod)  (((cod) & 0x1f00) >> 8)

// Bit 7 ~ Bit 2: Minor device class
#define GET_MINOR_DEVICE_CLASS(cod)  (((cod) & 0xfc) >> 2)

BluetoothClassOfDevice::BluetoothClassOfDevice(nsPIDOMWindow* aOwner)
  : mOwnerWindow(aOwner)
{
  MOZ_ASSERT(aOwner);

  Reset();
}

BluetoothClassOfDevice::~BluetoothClassOfDevice()
{}

void
BluetoothClassOfDevice::Reset()
{
  mMajorServiceClass = 0x1; // LIMITED_DISCOVERABILITY
  mMajorDeviceClass = 0x1F; // UNCATEGORIZED
  mMinorDeviceClass = 0;
}

bool
BluetoothClassOfDevice::Equals(const uint32_t aValue)
{
  return (mMajorServiceClass == GET_MAJOR_SERVICE_CLASS(aValue) &&
          mMajorDeviceClass == GET_MAJOR_DEVICE_CLASS(aValue) &&
          mMinorDeviceClass == GET_MINOR_DEVICE_CLASS(aValue));
}

uint32_t
BluetoothClassOfDevice::ToUint32()
{
  return (mMajorServiceClass & 0x7ff) << 13 |
         (mMajorDeviceClass & 0x1f) << 8 |
         (mMinorDeviceClass & 0x3f) << 2;
}

void
BluetoothClassOfDevice::Update(const uint32_t aValue)
{
  mMajorServiceClass = GET_MAJOR_SERVICE_CLASS(aValue);
  mMajorDeviceClass = GET_MAJOR_DEVICE_CLASS(aValue);
  mMinorDeviceClass = GET_MINOR_DEVICE_CLASS(aValue);
}

// static
already_AddRefed<BluetoothClassOfDevice>
BluetoothClassOfDevice::Create(nsPIDOMWindow* aOwner)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aOwner);

  nsRefPtr<BluetoothClassOfDevice> cod = new BluetoothClassOfDevice(aOwner);
  return cod.forget();
}

JSObject*
BluetoothClassOfDevice::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothClassOfDeviceBinding::Wrap(aCx, this, aGivenProto);
}
