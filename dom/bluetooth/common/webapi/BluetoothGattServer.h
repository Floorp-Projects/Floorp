/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothGattServer_h
#define mozilla_dom_bluetooth_BluetoothGattServer_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/BluetoothGattServerBinding.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "nsCOMPtr.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {
class Promise;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSignal;

class BluetoothGattServer final : public DOMEventTargetHelper
                                , public BluetoothSignalObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BluetoothGattServer,
                                           DOMEventTargetHelper)

  /****************************************************************************
   * Attribute Getters
   ***************************************************************************/

  /****************************************************************************
   * Event Handlers
   ***************************************************************************/
  IMPL_EVENT_HANDLER(connectionstatechanged);

  /****************************************************************************
   * Methods (Web API Implementation)
   ***************************************************************************/
  already_AddRefed<Promise> Connect(
    const nsAString& aAddress, ErrorResult& aRv);
  already_AddRefed<Promise> Disconnect(
    const nsAString& aAddress, ErrorResult& aRv);

  /****************************************************************************
   * Others
   ***************************************************************************/
  void Notify(const BluetoothSignal& aData); // BluetoothSignalObserver

  nsPIDOMWindow* GetParentObject() const
  {
     return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  virtual void DisconnectFromOwner() override;
  BluetoothGattServer(nsPIDOMWindow* aOwner);

  /* Invalidate the GATT server.
   * If the BluetoothAdapter turns off, existing BluetoothGattServer instances
   * should stop working till the end of life.
   */
  void Invalidate();

private:
  ~BluetoothGattServer();

  /****************************************************************************
   * Variables
   ***************************************************************************/
  nsCOMPtr<nsPIDOMWindow> mOwner;

  /**
   * Random generated UUID of this GATT client.
   */
  nsString mAppUuid;

  /**
   * Id of the GATT server interface given by bluetooth stack.
   * 0 if the interface is not registered yet, nonzero otherwise.
   */
  int mServerIf;

  bool mValid;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothGattServer_h
