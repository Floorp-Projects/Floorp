/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothService.h"
#include "BluetoothUtils.h"
#include "mozilla/dom/BluetoothGattDescriptorBinding.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/dom/bluetooth/BluetoothGattCharacteristic.h"
#include "mozilla/dom/bluetooth/BluetoothGattDescriptor.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"

using namespace mozilla;
using namespace mozilla::dom;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(
  BluetoothGattDescriptor, mOwner, mCharacteristic)
NS_IMPL_CYCLE_COLLECTING_ADDREF(BluetoothGattDescriptor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BluetoothGattDescriptor)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BluetoothGattDescriptor)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

BluetoothGattDescriptor::BluetoothGattDescriptor(
  nsPIDOMWindow* aOwner,
  BluetoothGattCharacteristic* aCharacteristic,
  const BluetoothGattId& aDescriptorId)
  : mOwner(aOwner)
  , mCharacteristic(aCharacteristic)
  , mDescriptorId(aDescriptorId)
{
  MOZ_ASSERT(aOwner);
  MOZ_ASSERT(aCharacteristic);

  // Generate a string representation to provide uuid of this descriptor to
  // applications
  ReversedUuidToString(aDescriptorId.mUuid, mUuidStr);
}

BluetoothGattDescriptor::~BluetoothGattDescriptor()
{
}

JSObject*
BluetoothGattDescriptor::WrapObject(JSContext* aContext,
                                    JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothGattDescriptorBinding::Wrap(aContext, this, aGivenProto);
}
