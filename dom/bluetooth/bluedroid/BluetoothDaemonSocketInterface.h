/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_bluetoothdaemonsocketinterface_h__
#define mozilla_dom_bluetooth_bluedroid_bluetoothdaemonsocketinterface_h__

#include "BluetoothDaemonHelpers.h"
#include "BluetoothInterface.h"
#include "BluetoothInterfaceHelpers.h"

BEGIN_BLUETOOTH_NAMESPACE

using namespace mozilla::ipc;

class BluetoothDaemonSocketModule
{
public:
  static const int MAX_NUM_CLIENTS;

  virtual nsresult Send(BluetoothDaemonPDU* aPDU, void* aUserData) = 0;

  // Commands
  //

  nsresult ListenCmd(BluetoothSocketType aType,
                     const nsAString& aServiceName,
                     const uint8_t aServiceUuid[16],
                     int aChannel, bool aEncrypt, bool aAuth,
                     BluetoothSocketResultHandler* aRes);

  nsresult ConnectCmd(const nsAString& aBdAddr,
                      BluetoothSocketType aType,
                      const uint8_t aUuid[16],
                      int aChannel, bool aEncrypt, bool aAuth,
                      BluetoothSocketResultHandler* aRes);

  nsresult AcceptCmd(int aFd, BluetoothSocketResultHandler* aRes);

  nsresult CloseCmd(BluetoothSocketResultHandler* aRes);

protected:

  void HandleSvc(const BluetoothDaemonPDUHeader& aHeader,
                 BluetoothDaemonPDU& aPDU, void* aUserData);

  nsresult Send(BluetoothDaemonPDU* aPDU, BluetoothSocketResultHandler* aRes);

private:
  class AcceptWatcher;
  class ConnectWatcher;
  class ListenInitOp;

  uint8_t SocketFlags(bool aEncrypt, bool aAuth);

  // Responses
  //

  typedef BluetoothResultRunnable0<BluetoothSocketResultHandler, void>
    ResultRunnable;

  typedef BluetoothResultRunnable1<BluetoothSocketResultHandler, void,
                                   int, int>
    IntResultRunnable;

  typedef BluetoothResultRunnable1<BluetoothSocketResultHandler, void,
                                   BluetoothStatus, BluetoothStatus>
    ErrorRunnable;

  typedef BluetoothResultRunnable3<BluetoothSocketResultHandler, void,
                                   int, nsString, int,
                                   int, const nsAString_internal&, int>
    IntStringIntResultRunnable;

  void ErrorRsp(const BluetoothDaemonPDUHeader& aHeader,
                BluetoothDaemonPDU& aPDU,
                BluetoothSocketResultHandler* aRes);

  void ListenRsp(const BluetoothDaemonPDUHeader& aHeader,
                 BluetoothDaemonPDU& aPDU,
                 BluetoothSocketResultHandler* aRes);

  void ConnectRsp(const BluetoothDaemonPDUHeader& aHeader,
                  BluetoothDaemonPDU& aPDU,
                  BluetoothSocketResultHandler* aRes);
};

class BluetoothDaemonSocketInterface final
  : public BluetoothSocketInterface
{
public:
  BluetoothDaemonSocketInterface(BluetoothDaemonSocketModule* aModule);
  ~BluetoothDaemonSocketInterface();

  void Listen(BluetoothSocketType aType,
              const nsAString& aServiceName,
              const uint8_t aServiceUuid[16],
              int aChannel, bool aEncrypt, bool aAuth,
              BluetoothSocketResultHandler* aRes);

  void Connect(const nsAString& aBdAddr,
               BluetoothSocketType aType,
               const uint8_t aUuid[16],
               int aChannel, bool aEncrypt, bool aAuth,
               BluetoothSocketResultHandler* aRes);

  void Accept(int aFd, BluetoothSocketResultHandler* aRes);

  void Close(BluetoothSocketResultHandler* aRes);

private:
  void DispatchError(BluetoothSocketResultHandler* aRes,
                     BluetoothStatus aStatus);
  void DispatchError(BluetoothSocketResultHandler* aRes, nsresult aRv);

  BluetoothDaemonSocketModule* mModule;
};

END_BLUETOOTH_NAMESPACE

#endif
