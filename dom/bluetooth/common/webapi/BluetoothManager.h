/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothManager_h
#define mozilla_dom_bluetooth_BluetoothManager_h

#include "BluetoothCommon.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BluetoothAdapterEvent.h"
#include "mozilla/dom/BluetoothAttributeEvent.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Observer.h"
#include "nsISupportsImpl.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothAdapter;
class BluetoothValue;

class BluetoothManager final : public DOMEventTargetHelper
                             , public BluetoothSignalObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BluetoothManager,
                                           DOMEventTargetHelper)

  /****************************************************************************
   * Event Handlers
   ***************************************************************************/
  IMPL_EVENT_HANDLER(attributechanged);
  IMPL_EVENT_HANDLER(adapteradded);
  IMPL_EVENT_HANDLER(adapterremoved);

  /****************************************************************************
   * Methods (Web API Implementation)
   ***************************************************************************/
  /**
   * Return default adapter if it exists, nullptr otherwise. The function is
   * called when applications access property BluetoothManager.defaultAdapter
   */
  BluetoothAdapter* GetDefaultAdapter();

  /**
   * Return adapters array. The function is called when applications call
   * method BluetoothManager.getAdapters()
   *
   * @param aAdapters [out] Adapters array to return
   */
  void GetAdapters(nsTArray<RefPtr<BluetoothAdapter> >& aAdapters);

  /****************************************************************************
   * Others
   ***************************************************************************/
  // Never returns null
  static already_AddRefed<BluetoothManager> Create(nsPIDOMWindowInner* aWindow);
  static bool CheckPermission(nsPIDOMWindowInner* aWindow);

  void Notify(const BluetoothSignal& aData); // BluetoothSignalObserver
  nsPIDOMWindowInner* GetParentObject() const
  {
    return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  virtual void DisconnectFromOwner() override;

  /**
   * Create a BluetoothAdapter object based on properties array
   * and append it into adapters array.
   *
   * @param aValue [in] Properties array to create BluetoothAdapter object
   */
  void AppendAdapter(const BluetoothValue& aValue);

  /**
   * Check whether B2G only GATT client API is enabled (true) or W3C
   * WebBluetooth API is enabled (false).
   */
  static bool B2GGattClientEnabled(JSContext* cx, JSObject* aGlobal);

private:
  BluetoothManager(nsPIDOMWindowInner* aWindow);
  ~BluetoothManager();

  /**
   * Check whether default adapter exists.
   */
  bool DefaultAdapterExists()
  {
    return (mDefaultAdapterIndex >= 0);
  }

  /**
   * Handle "AdapterAdded" bluetooth signal.
   *
   * @param aValue [in] Properties array of the added adapter
   */
  void HandleAdapterAdded(const BluetoothValue& aValue);

  /**
   * Handle "AdapterRemoved" bluetooth signal.
   *
   * @param aValue [in] Address of the removed adapter
   */
  void HandleAdapterRemoved(const BluetoothValue& aValue);

  /**
   * Re-select default adapter from adapters array. The function is called
   * when an adapter is added/removed.
   */
  void ReselectDefaultAdapter();

  /**
   * Fire BluetoothAdapterEvent to trigger
   * onadapteradded/onadapterremoved event handler.
   *
   * @param aType [in] Event type to fire
   * @param aInit [in] Event initialization value
   */
  void DispatchAdapterEvent(const nsAString& aType,
                            const BluetoothAdapterEventInit& aInit);

  /**
   * Fire BluetoothAttributeEvent to trigger onattributechanged event handler.
   */
  void DispatchAttributeEvent();

  /****************************************************************************
   * Variables
   ***************************************************************************/
  /**
   * The index of default adapter in the adapters array.
   */
  int mDefaultAdapterIndex;

  /**
   * The adapters array.
   */
  nsTArray<RefPtr<BluetoothAdapter> > mAdapters;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothManager_h
