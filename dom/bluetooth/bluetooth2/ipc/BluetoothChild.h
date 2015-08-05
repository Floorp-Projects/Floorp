/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_ipc_BluetoothChild_h
#define mozilla_dom_bluetooth_ipc_BluetoothChild_h

#include "mozilla/dom/bluetooth/BluetoothCommon.h"

#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/dom/bluetooth/PBluetoothChild.h"
#include "mozilla/dom/bluetooth/PBluetoothRequestChild.h"

#include "mozilla/Attributes.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace dom {
namespace bluetooth {

class BluetoothServiceChildProcess;

} // namespace bluetooth
} // namespace dom
} // namespace mozilla

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothReplyRunnable;

/*******************************************************************************
 * BluetoothChild
 ******************************************************************************/

class BluetoothChild : public PBluetoothChild
{
  friend class mozilla::dom::bluetooth::BluetoothServiceChildProcess;

  enum ShutdownState
  {
    Running = 0,
    SentStopNotifying,
    ReceivedNotificationsStopped,
    Dead
  };

  ShutdownState mShutdownState;

protected:
  BluetoothChild(BluetoothServiceChildProcess* aBluetoothService);
  virtual ~BluetoothChild();

  void
  BeginShutdown();

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool
  RecvNotify(const BluetoothSignal& aSignal);

  virtual bool
  RecvEnabled(const bool& aEnabled) override;

  virtual bool
  RecvBeginShutdown() override;

  virtual bool
  RecvNotificationsStopped() override;

  virtual PBluetoothRequestChild*
  AllocPBluetoothRequestChild(const Request& aRequest) override;

  virtual bool
  DeallocPBluetoothRequestChild(PBluetoothRequestChild* aActor) override;
};

/*******************************************************************************
 * BluetoothRequestChild
 ******************************************************************************/

class BluetoothRequestChild : public PBluetoothRequestChild
{
  friend class mozilla::dom::bluetooth::BluetoothChild;

  nsRefPtr<BluetoothReplyRunnable> mReplyRunnable;

public:
  BluetoothRequestChild(BluetoothReplyRunnable* aReplyRunnable);

protected:
  virtual ~BluetoothRequestChild();

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool
  Recv__delete__(const BluetoothReply& aReply) override;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_ipc_BluetoothChild_h
