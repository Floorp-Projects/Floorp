/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothGattServer_h
#define mozilla_dom_bluetooth_BluetoothGattServer_h

#include "mozilla/dom/BluetoothGattServerBinding.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "nsCOMPtr.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothGattServer final : public nsISupports
                                , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(BluetoothGattServer)

  /****************************************************************************
   * Attribute Getters
   ***************************************************************************/

  /****************************************************************************
   * Event Handlers
   ***************************************************************************/

  /****************************************************************************
   * Methods (Web API Implementation)
   ***************************************************************************/

  /****************************************************************************
   * Others
   ***************************************************************************/
  nsPIDOMWindow* GetParentObject() const
  {
     return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

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

  bool mValid;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothGattServer_h
