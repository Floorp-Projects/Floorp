/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothpropertyobject_h__
#define mozilla_dom_bluetooth_bluetoothpropertyobject_h__

#include "BluetoothCommon.h"
#include "BluetoothReplyRunnable.h"

class nsIDOMDOMRequest;
class nsIDOMWindow;

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothNamedValue;

class BluetoothPropertyContainer
{
public:
  nsresult GetProperties();
  nsresult SetProperty(nsIDOMWindow* aOwner,
                       const BluetoothNamedValue& aProperty,
                       nsIDOMDOMRequest** aRequest);
  virtual void SetPropertyByValue(const BluetoothNamedValue& aValue) = 0;
  nsString GetPath()
  {
    return mPath;
  }

  // Compatibility with nsRefPtr to make sure we don't hold a weakptr to
  // ourselves
  virtual nsrefcnt AddRef() = 0;
  virtual nsrefcnt Release() = 0;

protected:
  BluetoothPropertyContainer(BluetoothObjectType aType) :
    mObjectType(aType)
  {}

  ~BluetoothPropertyContainer()
  {}
  
  class GetPropertiesTask : public BluetoothReplyRunnable
  {
  public:
    GetPropertiesTask(BluetoothPropertyContainer* aPropObj, nsIDOMDOMRequest* aReq) :
      BluetoothReplyRunnable(aReq),
      mPropObjPtr(aPropObj)
    {
      MOZ_ASSERT(aReq && aPropObj);
    }

    virtual bool ParseSuccessfulReply(jsval* aValue);
    
    void
    ReleaseMembers()
    {
      BluetoothReplyRunnable::ReleaseMembers();
      mPropObjPtr = nullptr;
    }
    
  private:
    BluetoothPropertyContainer* mPropObjPtr;    
  };

  nsString mPath;
  BluetoothObjectType mObjectType;
};

END_BLUETOOTH_NAMESPACE

#endif
