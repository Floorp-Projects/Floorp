/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothmanager_h__
#define mozilla_dom_bluetooth_bluetoothmanager_h__

#include "mozilla/Attributes.h"
#include "BluetoothCommon.h"
#include "BluetoothPropertyContainer.h"
#include "nsDOMEventTargetHelper.h"
#include "mozilla/Observer.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {
class DOMRequest;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothNamedValue;

class BluetoothManager : public nsDOMEventTargetHelper
                       , public BluetoothSignalObserver
                       , public BluetoothPropertyContainer
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  // Never returns null
  static already_AddRefed<BluetoothManager>
    Create(nsPIDOMWindow* aWindow);
  static bool CheckPermission(nsPIDOMWindow* aWindow);
  void Notify(const BluetoothSignal& aData);
  virtual void SetPropertyByValue(const BluetoothNamedValue& aValue) MOZ_OVERRIDE;

  bool GetEnabled(ErrorResult& aRv);
  bool IsConnected(uint16_t aProfileId, ErrorResult& aRv);

  already_AddRefed<DOMRequest> GetDefaultAdapter(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(enabled);
  IMPL_EVENT_HANDLER(disabled);
  IMPL_EVENT_HANDLER(adapteradded);

  nsPIDOMWindow* GetParentObject() const
  {
     return GetOwner();
  }

  virtual JSObject*
    WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  virtual void DisconnectFromOwner() MOZ_OVERRIDE;

private:
  BluetoothManager(nsPIDOMWindow* aWindow);
  ~BluetoothManager();
};

END_BLUETOOTH_NAMESPACE

#endif
