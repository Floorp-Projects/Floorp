/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothGattAttributeEvent_h
#define mozilla_dom_bluetooth_BluetoothGattAttributeEvent_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/BluetoothGattAttributeEventBinding.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/dom/Event.h"

struct JSContext;
BEGIN_BLUETOOTH_NAMESPACE

class BluetoothGattAttributeEvent final : public Event
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
    BluetoothGattAttributeEvent, Event)
protected:
  virtual ~BluetoothGattAttributeEvent();
  explicit BluetoothGattAttributeEvent(EventTarget* aOwner);

  nsString mAddress;
  int32_t mRequestId;
  nsRefPtr<BluetoothGattCharacteristic> mCharacteristic;
  nsRefPtr<BluetoothGattDescriptor> mDescriptor;
  JS::Heap<JSObject*> mValue;
  bool mNeedResponse;

public:
  virtual
  JSObject* WrapObjectInternal(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<BluetoothGattAttributeEvent>
  Constructor(EventTarget* aOwner,
              const nsAString& aType,
              const nsAString& aAddress,
              int32_t aRequestId,
              BluetoothGattCharacteristic* aCharacteristic,
              BluetoothGattDescriptor* aDescriptor,
              const nsTArray<uint8_t>* aValue,
              bool aNeedResponse,
              bool aBubbles,
              bool aCancelable);

  static already_AddRefed<BluetoothGattAttributeEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const BluetoothGattAttributeEventInit& aEventInitDict,
              ErrorResult& aRv);

  void GetAddress(nsString& aRetVal) const;

  int32_t RequestId() const;

  BluetoothGattCharacteristic* GetCharacteristic() const;

  BluetoothGattDescriptor* GetDescriptor() const;

  void
  GetValue(JSContext* cx,
           JS::MutableHandle<JSObject*> aValue,
           ErrorResult& aRv);

  bool NeedResponse() const;

private:
  nsTArray<uint8_t> mRawValue;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_BluetoothGattAttributeEvent_h
