/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/bluetooth/BluetoothLeDeviceEvent.h"

#include "js/GCAPI.h"
#include "jsfriendapi.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/BluetoothLeDeviceEventBinding.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/PrimitiveConversions.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/bluetooth/BluetoothDevice.h"

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothLeDeviceEvent)

NS_IMPL_ADDREF_INHERITED(BluetoothLeDeviceEvent, Event)
NS_IMPL_RELEASE_INHERITED(BluetoothLeDeviceEvent, Event)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BluetoothLeDeviceEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDevice)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(BluetoothLeDeviceEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mScanRecord)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BluetoothLeDeviceEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDevice)
  tmp->mScanRecord = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothLeDeviceEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

BluetoothLeDeviceEvent::BluetoothLeDeviceEvent(
  mozilla::dom::EventTarget* aOwner)
  : Event(aOwner, nullptr, nullptr)
{
  mozilla::HoldJSObjects(this);
}

BluetoothLeDeviceEvent::~BluetoothLeDeviceEvent()
{
  mScanRecord = nullptr;
  mozilla::DropJSObjects(this);
}

JSObject*
BluetoothLeDeviceEvent::WrapObjectInternal(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothLeDeviceEventBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<BluetoothLeDeviceEvent>
BluetoothLeDeviceEvent::Constructor(
  mozilla::dom::EventTarget* aOwner,
  const nsAString& aType,
  BluetoothDevice* const aDevice,
  const int16_t aRssi,
  const nsTArray<uint8_t>& aScanRecord)
{
  nsRefPtr<BluetoothLeDeviceEvent> e = new BluetoothLeDeviceEvent(aOwner);
  bool trusted = e->Init(aOwner);
  e->InitEvent(aType, false, false);
  e->mDevice = aDevice;
  e->mRssi = aRssi;
  e->mRawScanRecord = aScanRecord;

  e->SetTrusted(trusted);
  return e.forget();
}

already_AddRefed<BluetoothLeDeviceEvent>
BluetoothLeDeviceEvent::Constructor(
  const GlobalObject& aGlobal,
  const nsAString& aType,
  const BluetoothLeDeviceEventInit& aEventInitDict,
  ErrorResult& aRv)
{
  nsCOMPtr<mozilla::dom::EventTarget> owner =
    do_QueryInterface(aGlobal.GetAsSupports());

  nsRefPtr<BluetoothLeDeviceEvent> e = new BluetoothLeDeviceEvent(owner);
  bool trusted = e->Init(owner);
  e->InitEvent(aType, aEventInitDict.mBubbles, aEventInitDict.mCancelable);
  e->mDevice = aEventInitDict.mDevice;
  e->mRssi = aEventInitDict.mRssi;

  if (!aEventInitDict.mScanRecord.IsNull()) {
    const auto& scanRecord = aEventInitDict.mScanRecord.Value();
    scanRecord.ComputeLengthAndData();
    e->mScanRecord = ArrayBuffer::Create(aGlobal.Context(),
                                         scanRecord.Length(),
                                         scanRecord.Data());
    if (!e->mScanRecord) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
  }

  e->SetTrusted(trusted);
  return e.forget();
}

BluetoothDevice*
BluetoothLeDeviceEvent::GetDevice() const
{
  return mDevice;
}

int16_t
BluetoothLeDeviceEvent::Rssi() const
{
  return mRssi;
}

void
BluetoothLeDeviceEvent::GetScanRecord(
  JSContext* cx,
  JS::MutableHandle<JSObject*> aScanRecord,
  ErrorResult& aRv)
{
  if (!mScanRecord) {
    mScanRecord = ArrayBuffer::Create(cx,
                                      mRawScanRecord.Length(),
                                      mRawScanRecord.Elements());
    if (!mScanRecord) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    mRawScanRecord.Clear();
  }
  JS::ExposeObjectToActiveJS(mScanRecord);
  aScanRecord.set(mScanRecord);

  return;
}
