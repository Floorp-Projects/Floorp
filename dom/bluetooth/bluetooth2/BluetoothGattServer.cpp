/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothGattServer.h"

using namespace mozilla;
using namespace mozilla::dom;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(BluetoothGattServer,
                                      mOwner)

NS_IMPL_CYCLE_COLLECTING_ADDREF(BluetoothGattServer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BluetoothGattServer)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BluetoothGattServer)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

BluetoothGattServer::BluetoothGattServer(nsPIDOMWindow* aOwner)
  : mOwner(aOwner)
  , mValid(true)
{
}

BluetoothGattServer::~BluetoothGattServer()
{
  Invalidate();
}

JSObject*
BluetoothGattServer::WrapObject(JSContext* aContext,
                                JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothGattServerBinding::Wrap(aContext, this, aGivenProto);
}

void
BluetoothGattServer::Invalidate()
{
  mValid = false;

  /* TODO: add tear down stuff here */

  return;
}