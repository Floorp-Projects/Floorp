/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_BluetoothDaemonSetupInterface_h
#define mozilla_dom_bluetooth_bluedroid_BluetoothDaemonSetupInterface_h

#include "BluetoothDaemonHelpers.h"
#include "BluetoothInterface.h"
#include "mozilla/ipc/DaemonRunnables.h"

BEGIN_BLUETOOTH_NAMESPACE

using mozilla::ipc::DaemonSocketPDU;
using mozilla::ipc::DaemonSocketPDUHeader;
using mozilla::ipc::DaemonSocketResultHandler;

class BluetoothDaemonSetupModule
{
public:
  enum {
    SERVICE_ID = 0x00
  };

  enum {
    OPCODE_ERROR = 0x00,
    OPCODE_REGISTER_MODULE = 0x01,
    OPCODE_UNREGISTER_MODULE = 0x02,
    OPCODE_CONFIGURATION = 0x03
  };

  virtual nsresult Send(DaemonSocketPDU* aPDU,
                        DaemonSocketResultHandler* aRes) = 0;

  // Commands
  //

  nsresult RegisterModuleCmd(BluetoothSetupServiceId aId, uint8_t aMode,
                             uint32_t aMaxNumClients,
                             BluetoothSetupResultHandler* aRes);

  nsresult UnregisterModuleCmd(BluetoothSetupServiceId aId,
                               BluetoothSetupResultHandler* aRes);

  nsresult ConfigurationCmd(const BluetoothConfigurationParameter* aParam,
                            uint8_t aLen, BluetoothSetupResultHandler* aRes);

protected:

  void HandleSvc(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU, DaemonSocketResultHandler* aRes);

private:

  // Responses
  //

  typedef mozilla::ipc::DaemonResultRunnable0<
    BluetoothSetupResultHandler, void>
    ResultRunnable;

  typedef mozilla::ipc::DaemonResultRunnable1<
    BluetoothSetupResultHandler, void, BluetoothStatus, BluetoothStatus>
    ErrorRunnable;

  void
  ErrorRsp(const DaemonSocketPDUHeader& aHeader,
           DaemonSocketPDU& aPDU,
           BluetoothSetupResultHandler* aRes);

  void
  RegisterModuleRsp(const DaemonSocketPDUHeader& aHeader,
                    DaemonSocketPDU& aPDU,
                    BluetoothSetupResultHandler* aRes);

  void
  UnregisterModuleRsp(const DaemonSocketPDUHeader& aHeader,
                      DaemonSocketPDU& aPDU,
                      BluetoothSetupResultHandler* aRes);

  void
  ConfigurationRsp(const DaemonSocketPDUHeader& aHeader,
                   DaemonSocketPDU& aPDU,
                   BluetoothSetupResultHandler* aRes);
};

class BluetoothDaemonSetupInterface final
  : public BluetoothSetupInterface
{
public:
  BluetoothDaemonSetupInterface(BluetoothDaemonSetupModule* aModule);
  ~BluetoothDaemonSetupInterface();

  void RegisterModule(BluetoothSetupServiceId aId, uint8_t aMode,
                      uint32_t aMaxNumClients,
                      BluetoothSetupResultHandler* aRes) override;

  void UnregisterModule(BluetoothSetupServiceId aId,
                        BluetoothSetupResultHandler* aRes) override;

  void Configuration(const BluetoothConfigurationParameter* aParam,
                     uint8_t aLen,
                     BluetoothSetupResultHandler* aRes) override;

private:
  void DispatchError(BluetoothSetupResultHandler* aRes,
                     BluetoothStatus aStatus);
  void DispatchError(BluetoothSetupResultHandler* aRes, nsresult aRv);

  BluetoothDaemonSetupModule* mModule;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluedroid_BluetoothDaemonSetupInterface_h
