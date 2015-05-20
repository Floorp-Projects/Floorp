/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothDaemonInterface.h"
#include <cutils/properties.h>
#include <fcntl.h>
#include <stdlib.h>
#include "BluetoothDaemonA2dpInterface.h"
#include "BluetoothDaemonAvrcpInterface.h"
#include "BluetoothDaemonConnector.h"
#include "BluetoothDaemonHandsfreeInterface.h"
#include "BluetoothDaemonHelpers.h"
#include "BluetoothDaemonSetupInterface.h"
#include "BluetoothDaemonSocketInterface.h"
#include "BluetoothInterfaceHelpers.h"
#include "mozilla/ipc/ListenSocket.h"
#include "mozilla/unused.h"
#include "prrng.h"

using namespace mozilla::ipc;

BEGIN_BLUETOOTH_NAMESPACE

static const int sRetryInterval = 100; // ms

//
// Protocol initialization and setup
//

class BluetoothDaemonSetupModule
{
public:
  virtual nsresult Send(BluetoothDaemonPDU* aPDU, void* aUserData) = 0;

  // Commands
  //

  nsresult RegisterModuleCmd(uint8_t aId, uint8_t aMode,
                             uint32_t aMaxNumClients,
                             BluetoothSetupResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x00, 0x01, 0));

#if ANDROID_VERSION >= 21
    nsresult rv = PackPDU(aId, aMode, aMaxNumClients, *pdu);
#else
    nsresult rv = PackPDU(aId, aMode, *pdu);
#endif
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult UnregisterModuleCmd(uint8_t aId,
                               BluetoothSetupResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x00, 0x02, 0));

    nsresult rv = PackPDU(aId, *pdu);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult ConfigurationCmd(const BluetoothConfigurationParameter* aParam,
                            uint8_t aLen, BluetoothSetupResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x00, 0x03, 0));

    nsresult rv = PackPDU(
      aLen, PackArray<BluetoothConfigurationParameter>(aParam, aLen), *pdu);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

protected:

  // Called to handle PDUs with Service field equal to 0x00, which
  // contains internal operations for setup and configuration.
  void HandleSvc(const BluetoothDaemonPDUHeader& aHeader,
                 BluetoothDaemonPDU& aPDU, void* aUserData)
  {
    static void (BluetoothDaemonSetupModule::* const HandleRsp[])(
      const BluetoothDaemonPDUHeader&,
      BluetoothDaemonPDU&,
      BluetoothSetupResultHandler*) = {
      INIT_ARRAY_AT(0x00, &BluetoothDaemonSetupModule::ErrorRsp),
      INIT_ARRAY_AT(0x01, &BluetoothDaemonSetupModule::RegisterModuleRsp),
      INIT_ARRAY_AT(0x02, &BluetoothDaemonSetupModule::UnregisterModuleRsp),
      INIT_ARRAY_AT(0x03, &BluetoothDaemonSetupModule::ConfigurationRsp)
    };

    if (NS_WARN_IF(aHeader.mOpcode >= MOZ_ARRAY_LENGTH(HandleRsp)) ||
        NS_WARN_IF(!HandleRsp[aHeader.mOpcode])) {
      return;
    }

    nsRefPtr<BluetoothSetupResultHandler> res =
      already_AddRefed<BluetoothSetupResultHandler>(
        static_cast<BluetoothSetupResultHandler*>(aUserData));

    if (!res) {
      return; // Return early if no result handler has been set
    }

    (this->*(HandleRsp[aHeader.mOpcode]))(aHeader, aPDU, res);
  }

  nsresult Send(BluetoothDaemonPDU* aPDU, BluetoothSetupResultHandler* aRes)
  {
    aRes->AddRef(); // Keep reference for response
    return Send(aPDU, static_cast<void*>(aRes));
  }

private:

  // Responses
  //

  typedef BluetoothResultRunnable0<BluetoothSetupResultHandler, void>
    ResultRunnable;

  typedef BluetoothResultRunnable1<BluetoothSetupResultHandler, void,
                                   BluetoothStatus, BluetoothStatus>
    ErrorRunnable;

  void
  ErrorRsp(const BluetoothDaemonPDUHeader& aHeader,
           BluetoothDaemonPDU& aPDU,
           BluetoothSetupResultHandler* aRes)
  {
    ErrorRunnable::Dispatch(
      aRes, &BluetoothSetupResultHandler::OnError, UnpackPDUInitOp(aPDU));
  }

  void
  RegisterModuleRsp(const BluetoothDaemonPDUHeader& aHeader,
                    BluetoothDaemonPDU& aPDU,
                    BluetoothSetupResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothSetupResultHandler::RegisterModule,
      UnpackPDUInitOp(aPDU));
  }

  void
  UnregisterModuleRsp(const BluetoothDaemonPDUHeader& aHeader,
                      BluetoothDaemonPDU& aPDU,
                      BluetoothSetupResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothSetupResultHandler::UnregisterModule,
      UnpackPDUInitOp(aPDU));
  }

  void
  ConfigurationRsp(const BluetoothDaemonPDUHeader& aHeader,
                   BluetoothDaemonPDU& aPDU,
                   BluetoothSetupResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothSetupResultHandler::Configuration,
      UnpackPDUInitOp(aPDU));
  }
};

//
// Core module
//

static BluetoothNotificationHandler* sNotificationHandler;

class BluetoothDaemonCoreModule
{
public:

  static const int MAX_NUM_CLIENTS;

  virtual nsresult Send(BluetoothDaemonPDU* aPDU, void* aUserData) = 0;

  nsresult EnableCmd(BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x01, 0));

    nsresult rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult DisableCmd(BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x02, 0));

    nsresult rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult GetAdapterPropertiesCmd(BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x03, 0));

    nsresult rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult GetAdapterPropertyCmd(const nsAString& aName,
                                 BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x04, 0));

    nsresult rv = PackPDU(
      PackConversion<const nsAString, BluetoothPropertyType>(aName), *pdu);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult SetAdapterPropertyCmd(const BluetoothNamedValue& aProperty,
                                 BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x05, 0));

    nsresult rv = PackPDU(aProperty, *pdu);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult GetRemoteDevicePropertiesCmd(const nsAString& aRemoteAddr,
                                        BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x06, 0));

    nsresult rv = PackPDU(
      PackConversion<nsAString, BluetoothAddress>(aRemoteAddr), *pdu);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult GetRemoteDevicePropertyCmd(const nsAString& aRemoteAddr,
                                      const nsAString& aName,
                                      BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x07, 0));

    nsresult rv = PackPDU(
      PackConversion<nsAString, BluetoothAddress>(aRemoteAddr),
      PackConversion<nsAString, BluetoothPropertyType>(aName), *pdu);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult SetRemoteDevicePropertyCmd(const nsAString& aRemoteAddr,
                                      const BluetoothNamedValue& aProperty,
                                      BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x08, 0));

    nsresult rv = PackPDU(
      PackConversion<nsAString, BluetoothAddress>(aRemoteAddr),
      aProperty, *pdu);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult GetRemoteServiceRecordCmd(const nsAString& aRemoteAddr,
                                     const uint8_t aUuid[16],
                                     BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x09, 0));

    nsresult rv = PackPDU(
      PackConversion<nsAString, BluetoothAddress>(aRemoteAddr),
      PackArray<uint8_t>(aUuid, 16), *pdu);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult GetRemoteServicesCmd(const nsAString& aRemoteAddr,
                                BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x0a, 0));

    nsresult rv = PackPDU(
      PackConversion<nsAString, BluetoothAddress>(aRemoteAddr), *pdu);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult StartDiscoveryCmd(BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x0b, 0));

    nsresult rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult CancelDiscoveryCmd(BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x0c, 0));

    nsresult rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult CreateBondCmd(const nsAString& aBdAddr,
                         BluetoothTransport aTransport,
                         BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x0d, 0));

#if ANDROID_VERSION >= 21
    nsresult rv = PackPDU(
      PackConversion<nsAString, BluetoothAddress>(aBdAddr), aTransport, *pdu);
#else
    nsresult rv = PackPDU(
      PackConversion<nsAString, BluetoothAddress>(aBdAddr), *pdu);
#endif
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult RemoveBondCmd(const nsAString& aBdAddr,
                         BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x0e, 0));

    nsresult rv = PackPDU(
      PackConversion<nsAString, BluetoothAddress>(aBdAddr), *pdu);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult CancelBondCmd(const nsAString& aBdAddr,
                         BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x0f, 0));

    nsresult rv = PackPDU(
      PackConversion<nsAString, BluetoothAddress>(aBdAddr), *pdu);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult PinReplyCmd(const nsAString& aBdAddr, bool aAccept,
                       const nsAString& aPinCode,
                       BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x10, 0));

    nsresult rv = PackPDU(
      PackConversion<nsAString, BluetoothAddress>(aBdAddr),
      aAccept,
      PackConversion<nsAString, BluetoothPinCode>(aPinCode), *pdu);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult SspReplyCmd(const nsAString& aBdAddr, BluetoothSspVariant aVariant,
                       bool aAccept, uint32_t aPasskey,
                       BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x11, 0));

    nsresult rv = PackPDU(
      PackConversion<nsAString, BluetoothAddress>(aBdAddr),
      aVariant, aAccept, aPasskey, *pdu);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult DutModeConfigureCmd(bool aEnable, BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x12, 0));

    nsresult rv = PackPDU(aEnable, *pdu);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult DutModeSendCmd(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                          BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x13, 0));

    nsresult rv = PackPDU(aOpcode, aLen, PackArray<uint8_t>(aBuf, aLen),
                          *pdu);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

  nsresult LeTestModeCmd(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                         BluetoothResultHandler* aRes)
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoPtr<BluetoothDaemonPDU> pdu(new BluetoothDaemonPDU(0x01, 0x14, 0));

    nsresult rv = PackPDU(aOpcode, aLen, PackArray<uint8_t>(aBuf, aLen),
                          *pdu);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = Send(pdu, aRes);
    if (NS_FAILED(rv)) {
      return rv;
    }
    unused << pdu.forget();
    return rv;
  }

protected:

  void HandleSvc(const BluetoothDaemonPDUHeader& aHeader,
                 BluetoothDaemonPDU& aPDU, void* aUserData)
  {
    static void (BluetoothDaemonCoreModule::* const HandleOp[])(
      const BluetoothDaemonPDUHeader&, BluetoothDaemonPDU&, void*) = {
      INIT_ARRAY_AT(0, &BluetoothDaemonCoreModule::HandleRsp),
      INIT_ARRAY_AT(1, &BluetoothDaemonCoreModule::HandleNtf),
    };

    MOZ_ASSERT(!NS_IsMainThread());

    (this->*(HandleOp[!!(aHeader.mOpcode & 0x80)]))(aHeader, aPDU, aUserData);
  }

  nsresult Send(BluetoothDaemonPDU* aPDU, BluetoothResultHandler* aRes)
  {
    aRes->AddRef(); // Keep reference for response
    return Send(aPDU, static_cast<void*>(aRes));
  }

private:

  // Responses
  //

  typedef BluetoothResultRunnable0<BluetoothResultHandler, void>
    ResultRunnable;

  typedef BluetoothResultRunnable1<BluetoothResultHandler, void,
                                   BluetoothStatus, BluetoothStatus>
    ErrorRunnable;

  void ErrorRsp(const BluetoothDaemonPDUHeader& aHeader,
                BluetoothDaemonPDU& aPDU,
                BluetoothResultHandler* aRes)
  {
    ErrorRunnable::Dispatch(
      aRes, &BluetoothResultHandler::OnError, UnpackPDUInitOp(aPDU));
  }

  void EnableRsp(const BluetoothDaemonPDUHeader& aHeader,
                 BluetoothDaemonPDU& aPDU,
                 BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::Enable, UnpackPDUInitOp(aPDU));
  }

  void DisableRsp(const BluetoothDaemonPDUHeader& aHeader,
                  BluetoothDaemonPDU& aPDU,
                  BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::Disable, UnpackPDUInitOp(aPDU));
  }

  void GetAdapterPropertiesRsp(const BluetoothDaemonPDUHeader& aHeader,
                               BluetoothDaemonPDU& aPDU,
                               BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::GetAdapterProperties,
      UnpackPDUInitOp(aPDU));
  }

  void GetAdapterPropertyRsp(const BluetoothDaemonPDUHeader& aHeader,
                             BluetoothDaemonPDU& aPDU,
                             BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::GetAdapterProperty,
      UnpackPDUInitOp(aPDU));
  }

  void SetAdapterPropertyRsp(const BluetoothDaemonPDUHeader& aHeader,
                             BluetoothDaemonPDU& aPDU,
                             BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::SetAdapterProperty,
      UnpackPDUInitOp(aPDU));
  }

  void GetRemoteDevicePropertiesRsp(const BluetoothDaemonPDUHeader& aHeader,
                                    BluetoothDaemonPDU& aPDU,
                                    BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::GetRemoteDeviceProperties,
      UnpackPDUInitOp(aPDU));
  }

  void
  GetRemoteDevicePropertyRsp(const BluetoothDaemonPDUHeader& aHeader,
                             BluetoothDaemonPDU& aPDU,
                             BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::GetRemoteDeviceProperty,
      UnpackPDUInitOp(aPDU));
  }

  void SetRemoteDevicePropertyRsp(const BluetoothDaemonPDUHeader& aHeader,
                                  BluetoothDaemonPDU& aPDU,
                                  BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::SetRemoteDeviceProperty,
      UnpackPDUInitOp(aPDU));
  }

  void GetRemoteServiceRecordRsp(const BluetoothDaemonPDUHeader& aHeader,
                                 BluetoothDaemonPDU& aPDU,
                                 BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::GetRemoteServiceRecord,
      UnpackPDUInitOp(aPDU));
  }

  void GetRemoteServicesRsp(const BluetoothDaemonPDUHeader& aHeader,
                            BluetoothDaemonPDU& aPDU,
                            BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::GetRemoteServices,
      UnpackPDUInitOp(aPDU));
  }

  void StartDiscoveryRsp(const BluetoothDaemonPDUHeader& aHeader,
                         BluetoothDaemonPDU& aPDU,
                         BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::StartDiscovery,
      UnpackPDUInitOp(aPDU));
  }

  void CancelDiscoveryRsp(const BluetoothDaemonPDUHeader& aHeader,
                          BluetoothDaemonPDU& aPDU,
                          BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::CancelDiscovery,
      UnpackPDUInitOp(aPDU));
  }

  void CreateBondRsp(const BluetoothDaemonPDUHeader& aHeader,
                     BluetoothDaemonPDU& aPDU,
                     BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::CreateBond,
      UnpackPDUInitOp(aPDU));
  }

  void RemoveBondRsp(const BluetoothDaemonPDUHeader& aHeader,
                     BluetoothDaemonPDU& aPDU,
                     BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::RemoveBond,
      UnpackPDUInitOp(aPDU));
  }

  void CancelBondRsp(const BluetoothDaemonPDUHeader& aHeader,
                     BluetoothDaemonPDU& aPDU,
                     BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::CancelBond,
      UnpackPDUInitOp(aPDU));
  }

  void PinReplyRsp(const BluetoothDaemonPDUHeader& aHeader,
                   BluetoothDaemonPDU& aPDU,
                   BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::PinReply,
      UnpackPDUInitOp(aPDU));
  }

  void SspReplyRsp(const BluetoothDaemonPDUHeader& aHeader,
                   BluetoothDaemonPDU& aPDU,
                   BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::SspReply,
      UnpackPDUInitOp(aPDU));
  }

  void DutModeConfigureRsp(const BluetoothDaemonPDUHeader& aHeader,
                           BluetoothDaemonPDU& aPDU,
                           BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::DutModeConfigure,
      UnpackPDUInitOp(aPDU));
  }

  void DutModeSendRsp(const BluetoothDaemonPDUHeader& aHeader,
                      BluetoothDaemonPDU& aPDU,
                      BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::DutModeSend,
      UnpackPDUInitOp(aPDU));
  }

  void LeTestModeRsp(const BluetoothDaemonPDUHeader& aHeader,
                     BluetoothDaemonPDU& aPDU,
                     BluetoothResultHandler* aRes)
  {
    ResultRunnable::Dispatch(
      aRes, &BluetoothResultHandler::LeTestMode,
      UnpackPDUInitOp(aPDU));
  }

  void HandleRsp(const BluetoothDaemonPDUHeader& aHeader,
                 BluetoothDaemonPDU& aPDU, void* aUserData)
  {
    static void (BluetoothDaemonCoreModule::* const HandleRsp[])(
      const BluetoothDaemonPDUHeader&,
      BluetoothDaemonPDU&,
      BluetoothResultHandler*) = {
      INIT_ARRAY_AT(0x00, &BluetoothDaemonCoreModule::ErrorRsp),
      INIT_ARRAY_AT(0x01, &BluetoothDaemonCoreModule::EnableRsp),
      INIT_ARRAY_AT(0x02, &BluetoothDaemonCoreModule::DisableRsp),
      INIT_ARRAY_AT(0x03, &BluetoothDaemonCoreModule::GetAdapterPropertiesRsp),
      INIT_ARRAY_AT(0x04, &BluetoothDaemonCoreModule::GetAdapterPropertyRsp),
      INIT_ARRAY_AT(0x05, &BluetoothDaemonCoreModule::SetAdapterPropertyRsp),
      INIT_ARRAY_AT(0x06,
        &BluetoothDaemonCoreModule::GetRemoteDevicePropertiesRsp),
      INIT_ARRAY_AT(0x07,
        &BluetoothDaemonCoreModule::GetRemoteDevicePropertyRsp),
      INIT_ARRAY_AT(0x08,
        &BluetoothDaemonCoreModule::SetRemoteDevicePropertyRsp),
      INIT_ARRAY_AT(0x09,
        &BluetoothDaemonCoreModule::GetRemoteServiceRecordRsp),
      INIT_ARRAY_AT(0x0a, &BluetoothDaemonCoreModule::GetRemoteServicesRsp),
      INIT_ARRAY_AT(0x0b, &BluetoothDaemonCoreModule::StartDiscoveryRsp),
      INIT_ARRAY_AT(0x0c, &BluetoothDaemonCoreModule::CancelDiscoveryRsp),
      INIT_ARRAY_AT(0x0d, &BluetoothDaemonCoreModule::CreateBondRsp),
      INIT_ARRAY_AT(0x0e, &BluetoothDaemonCoreModule::RemoveBondRsp),
      INIT_ARRAY_AT(0x0f, &BluetoothDaemonCoreModule::CancelBondRsp),
      INIT_ARRAY_AT(0x10, &BluetoothDaemonCoreModule::PinReplyRsp),
      INIT_ARRAY_AT(0x11, &BluetoothDaemonCoreModule::SspReplyRsp),
      INIT_ARRAY_AT(0x12, &BluetoothDaemonCoreModule::DutModeConfigureRsp),
      INIT_ARRAY_AT(0x13, &BluetoothDaemonCoreModule::DutModeSendRsp),
      INIT_ARRAY_AT(0x14, &BluetoothDaemonCoreModule::LeTestModeRsp),
    };

    MOZ_ASSERT(!NS_IsMainThread());

    if (NS_WARN_IF(!(aHeader.mOpcode < MOZ_ARRAY_LENGTH(HandleRsp))) ||
        NS_WARN_IF(!HandleRsp[aHeader.mOpcode])) {
      return;
    }

    nsRefPtr<BluetoothResultHandler> res =
      already_AddRefed<BluetoothResultHandler>(
        static_cast<BluetoothResultHandler*>(aUserData));

    if (!res) {
      return; // Return early if no result handler has been set for response
    }

    (this->*(HandleRsp[aHeader.mOpcode]))(aHeader, aPDU, res);
  }

  // Notifications
  //

  class NotificationHandlerWrapper
  {
  public:
    typedef BluetoothNotificationHandler ObjectType;

    static ObjectType* GetInstance()
    {
      MOZ_ASSERT(NS_IsMainThread());

      return sNotificationHandler;
    }
  };

  typedef BluetoothNotificationRunnable1<NotificationHandlerWrapper, void,
                                         bool>
    AdapterStateChangedNotification;

  typedef BluetoothNotificationRunnable3<NotificationHandlerWrapper, void,
                                         BluetoothStatus, int,
                                         nsAutoArrayPtr<BluetoothProperty>,
                                         BluetoothStatus, int,
                                         const BluetoothProperty*>
    AdapterPropertiesNotification;

  typedef BluetoothNotificationRunnable4<NotificationHandlerWrapper, void,
                                         BluetoothStatus, nsString, int,
                                         nsAutoArrayPtr<BluetoothProperty>,
                                         BluetoothStatus, const nsAString&,
                                         int, const BluetoothProperty*>
    RemoteDevicePropertiesNotification;

  typedef BluetoothNotificationRunnable2<NotificationHandlerWrapper, void,
                                         int,
                                         nsAutoArrayPtr<BluetoothProperty>,
                                         int, const BluetoothProperty*>
    DeviceFoundNotification;

  typedef BluetoothNotificationRunnable1<NotificationHandlerWrapper, void,
                                         bool>
    DiscoveryStateChangedNotification;

  typedef BluetoothNotificationRunnable3<NotificationHandlerWrapper, void,
                                         nsString, nsString, uint32_t,
                                         const nsAString&, const nsAString&>
    PinRequestNotification;

  typedef BluetoothNotificationRunnable5<NotificationHandlerWrapper, void,
                                         nsString, nsString, uint32_t,
                                         BluetoothSspVariant, uint32_t,
                                         const nsAString&, const nsAString&>
    SspRequestNotification;

  typedef BluetoothNotificationRunnable3<NotificationHandlerWrapper, void,
                                         BluetoothStatus, nsString,
                                         BluetoothBondState,
                                         BluetoothStatus, const nsAString&>
    BondStateChangedNotification;

  typedef BluetoothNotificationRunnable3<NotificationHandlerWrapper, void,
                                         BluetoothStatus, nsString, bool,
                                         BluetoothStatus, const nsAString&>
    AclStateChangedNotification;

  typedef BluetoothNotificationRunnable3<NotificationHandlerWrapper, void,
                                         uint16_t, nsAutoArrayPtr<uint8_t>,
                                         uint8_t, uint16_t, const uint8_t*>
    DutModeRecvNotification;

  typedef BluetoothNotificationRunnable2<NotificationHandlerWrapper, void,
                                         BluetoothStatus, uint16_t>
    LeTestModeNotification;

  void AdapterStateChangedNtf(const BluetoothDaemonPDUHeader& aHeader,
                              BluetoothDaemonPDU& aPDU)
  {
    AdapterStateChangedNotification::Dispatch(
      &BluetoothNotificationHandler::AdapterStateChangedNotification,
      UnpackPDUInitOp(aPDU));
  }

  // Init operator class for AdapterPropertiesNotification
  class AdapterPropertiesInitOp final : private PDUInitOp
  {
  public:
    AdapterPropertiesInitOp(BluetoothDaemonPDU& aPDU)
    : PDUInitOp(aPDU)
    { }

    nsresult
    operator () (BluetoothStatus& aArg1, int& aArg2,
                 nsAutoArrayPtr<BluetoothProperty>& aArg3) const
    {
      BluetoothDaemonPDU& pdu = GetPDU();

      /* Read status */
      nsresult rv = UnpackPDU(pdu, aArg1);
      if (NS_FAILED(rv)) {
        return rv;
      }

      /* Read number of properties */
      uint8_t numProperties;
      rv = UnpackPDU(pdu, numProperties);
      if (NS_FAILED(rv)) {
        return rv;
      }
      aArg2 = numProperties;

      /* Read properties array */
      UnpackArray<BluetoothProperty> properties(aArg3, aArg2);
      rv = UnpackPDU(pdu, properties);
      if (NS_FAILED(rv)) {
        return rv;
      }
      WarnAboutTrailingData();
      return NS_OK;
    }
  };

  void AdapterPropertiesNtf(const BluetoothDaemonPDUHeader& aHeader,
                            BluetoothDaemonPDU& aPDU)
  {
    AdapterPropertiesNotification::Dispatch(
      &BluetoothNotificationHandler::AdapterPropertiesNotification,
      AdapterPropertiesInitOp(aPDU));
  }

  // Init operator class for RemoteDevicePropertiesNotification
  class RemoteDevicePropertiesInitOp final : private PDUInitOp
  {
  public:
    RemoteDevicePropertiesInitOp(BluetoothDaemonPDU& aPDU)
    : PDUInitOp(aPDU)
    { }

    nsresult
    operator () (BluetoothStatus& aArg1, nsString& aArg2, int& aArg3,
                 nsAutoArrayPtr<BluetoothProperty>& aArg4) const
    {
      BluetoothDaemonPDU& pdu = GetPDU();

      /* Read status */
      nsresult rv = UnpackPDU(pdu, aArg1);
      if (NS_FAILED(rv)) {
        return rv;
      }

      /* Read address */
      rv = UnpackPDU(
        pdu, UnpackConversion<BluetoothAddress, nsAString>(aArg2));
      if (NS_FAILED(rv)) {
        return rv;
      }

      /* Read number of properties */
      uint8_t numProperties;
      rv = UnpackPDU(pdu, numProperties);
      if (NS_FAILED(rv)) {
        return rv;
      }
      aArg3 = numProperties;

      /* Read properties array */
      UnpackArray<BluetoothProperty> properties(aArg4, aArg3);
      rv = UnpackPDU(pdu, properties);
      if (NS_FAILED(rv)) {
        return rv;
      }
      WarnAboutTrailingData();
      return NS_OK;
    }
  };

  void RemoteDevicePropertiesNtf(const BluetoothDaemonPDUHeader& aHeader,
                                 BluetoothDaemonPDU& aPDU)
  {
    RemoteDevicePropertiesNotification::Dispatch(
      &BluetoothNotificationHandler::RemoteDevicePropertiesNotification,
      RemoteDevicePropertiesInitOp(aPDU));
  }

  // Init operator class for DeviceFoundNotification
  class DeviceFoundInitOp final : private PDUInitOp
  {
  public:
    DeviceFoundInitOp(BluetoothDaemonPDU& aPDU)
    : PDUInitOp(aPDU)
    { }

    nsresult
    operator () (int& aArg1, nsAutoArrayPtr<BluetoothProperty>& aArg2) const
    {
      BluetoothDaemonPDU& pdu = GetPDU();

      /* Read number of properties */
      uint8_t numProperties;
      nsresult rv = UnpackPDU(pdu, numProperties);
      if (NS_FAILED(rv)) {
        return rv;
      }
      aArg1 = numProperties;

      /* Read properties array */
      UnpackArray<BluetoothProperty> properties(aArg2, aArg1);
      rv = UnpackPDU(pdu, properties);
      if (NS_FAILED(rv)) {
        return rv;
      }
      WarnAboutTrailingData();
      return NS_OK;
    }
  };

  void DeviceFoundNtf(const BluetoothDaemonPDUHeader& aHeader,
                      BluetoothDaemonPDU& aPDU)
  {
    DeviceFoundNotification::Dispatch(
      &BluetoothNotificationHandler::DeviceFoundNotification,
      DeviceFoundInitOp(aPDU));
  }

  void DiscoveryStateChangedNtf(const BluetoothDaemonPDUHeader& aHeader,
                                BluetoothDaemonPDU& aPDU)
  {
    DiscoveryStateChangedNotification::Dispatch(
      &BluetoothNotificationHandler::DiscoveryStateChangedNotification,
      UnpackPDUInitOp(aPDU));
  }

  // Init operator class for PinRequestNotification
  class PinRequestInitOp final : private PDUInitOp
  {
  public:
    PinRequestInitOp(BluetoothDaemonPDU& aPDU)
    : PDUInitOp(aPDU)
    { }

    nsresult
    operator () (nsString& aArg1, nsString& aArg2, uint32_t& aArg3) const
    {
      BluetoothDaemonPDU& pdu = GetPDU();

      /* Read remote address */
      nsresult rv = UnpackPDU(
        pdu, UnpackConversion<BluetoothAddress, nsAString>(aArg1));
      if (NS_FAILED(rv)) {
        return rv;
      }

      /* Read remote name */
      rv = UnpackPDU(
        pdu, UnpackConversion<BluetoothRemoteName, nsAString>(aArg2));
      if (NS_FAILED(rv)) {
        return rv;
      }

      /* Read CoD */
      rv = UnpackPDU(pdu, aArg3);
      if (NS_FAILED(rv)) {
        return rv;
      }
      WarnAboutTrailingData();
      return NS_OK;
    }
  };

  void PinRequestNtf(const BluetoothDaemonPDUHeader& aHeader,
                     BluetoothDaemonPDU& aPDU)
  {
    PinRequestNotification::Dispatch(
      &BluetoothNotificationHandler::PinRequestNotification,
      PinRequestInitOp(aPDU));
  }

  // Init operator class for SspRequestNotification
  class SspRequestInitOp final : private PDUInitOp
  {
  public:
    SspRequestInitOp(BluetoothDaemonPDU& aPDU)
    : PDUInitOp(aPDU)
    { }

    nsresult
    operator () (nsString& aArg1, nsString& aArg2, uint32_t& aArg3,
                 BluetoothSspVariant& aArg4, uint32_t& aArg5) const
    {
      BluetoothDaemonPDU& pdu = GetPDU();

      /* Read remote address */
      nsresult rv = UnpackPDU(
        pdu, UnpackConversion<BluetoothAddress, nsAString>(aArg1));
      if (NS_FAILED(rv)) {
        return rv;
      }

      /* Read remote name */
      rv = UnpackPDU(
        pdu, UnpackConversion<BluetoothRemoteName, nsAString>(aArg2));
      if (NS_FAILED(rv)) {
        return rv;
      }

      /* Read CoD */
      rv = UnpackPDU(pdu, aArg3);
      if (NS_FAILED(rv)) {
        return rv;
      }

      /* Read pairing variant */
      rv = UnpackPDU(pdu, aArg4);
      if (NS_FAILED(rv)) {
        return rv;
      }

      /* Read passkey */
      rv = UnpackPDU(pdu, aArg5);
      if (NS_FAILED(rv)) {
        return rv;
      }
      WarnAboutTrailingData();
      return NS_OK;
    }
  };

  void SspRequestNtf(const BluetoothDaemonPDUHeader& aHeader,
                     BluetoothDaemonPDU& aPDU)
  {
    SspRequestNotification::Dispatch(
      &BluetoothNotificationHandler::SspRequestNotification,
      SspRequestInitOp(aPDU));
  }

  // Init operator class for BondStateChangedNotification
  class BondStateChangedInitOp final : private PDUInitOp
  {
  public:
    BondStateChangedInitOp(BluetoothDaemonPDU& aPDU)
    : PDUInitOp(aPDU)
    { }

    nsresult
    operator () (BluetoothStatus& aArg1, nsString& aArg2,
                 BluetoothBondState& aArg3) const
    {
      BluetoothDaemonPDU& pdu = GetPDU();

      /* Read status */
      nsresult rv = UnpackPDU(pdu, aArg1);
      if (NS_FAILED(rv)) {
        return rv;
      }

      /* Read remote address */
      rv = UnpackPDU(
        pdu, UnpackConversion<BluetoothAddress, nsAString>(aArg2));
      if (NS_FAILED(rv)) {
        return rv;
      }

      /* Read bond state */
      rv = UnpackPDU(pdu, aArg3);
      if (NS_FAILED(rv)) {
        return rv;
      }
      WarnAboutTrailingData();
      return NS_OK;
    }
  };

  void BondStateChangedNtf(const BluetoothDaemonPDUHeader& aHeader,
                           BluetoothDaemonPDU& aPDU)
  {
    BondStateChangedNotification::Dispatch(
      &BluetoothNotificationHandler::BondStateChangedNotification,
      BondStateChangedInitOp(aPDU));
  }

  // Init operator class for AclStateChangedNotification
  class AclStateChangedInitOp final : private PDUInitOp
  {
  public:
    AclStateChangedInitOp(BluetoothDaemonPDU& aPDU)
    : PDUInitOp(aPDU)
    { }

    nsresult
    operator () (BluetoothStatus& aArg1, nsString& aArg2, bool& aArg3) const
    {
      BluetoothDaemonPDU& pdu = GetPDU();

      /* Read status */
      nsresult rv = UnpackPDU(pdu, aArg1);
      if (NS_FAILED(rv)) {
        return rv;
      }

      /* Read remote address */
      rv = UnpackPDU(
        pdu, UnpackConversion<BluetoothAddress, nsAString>(aArg2));
      if (NS_FAILED(rv)) {
        return rv;
      }

      /* Read ACL state */
      rv = UnpackPDU(
        pdu, UnpackConversion<BluetoothAclState, bool>(aArg3));
      if (NS_FAILED(rv)) {
        return rv;
      }
      WarnAboutTrailingData();
      return NS_OK;
    }
  };

  void AclStateChangedNtf(const BluetoothDaemonPDUHeader& aHeader,
                          BluetoothDaemonPDU& aPDU)
  {
    AclStateChangedNotification::Dispatch(
      &BluetoothNotificationHandler::AclStateChangedNotification,
      AclStateChangedInitOp(aPDU));
  }

  // Init operator class for DutModeRecvNotification
  class DutModeRecvInitOp final : private PDUInitOp
  {
  public:
    DutModeRecvInitOp(BluetoothDaemonPDU& aPDU)
    : PDUInitOp(aPDU)
    { }

    nsresult
    operator () (uint16_t& aArg1, nsAutoArrayPtr<uint8_t>& aArg2,
                 uint8_t& aArg3) const
    {
      BluetoothDaemonPDU& pdu = GetPDU();

      /* Read opcode */
      nsresult rv = UnpackPDU(pdu, aArg1);
      if (NS_FAILED(rv)) {
        return rv;
      }

      /* Read length */
      rv = UnpackPDU(pdu, aArg3);
      if (NS_FAILED(rv)) {
        return rv;
      }

      /* Read data */
      rv = UnpackPDU(pdu, UnpackArray<uint8_t>(aArg2, aArg3));
      if (NS_FAILED(rv)) {
        return rv;
      }
      WarnAboutTrailingData();
      return NS_OK;
    }
  };

  void DutModeRecvNtf(const BluetoothDaemonPDUHeader& aHeader,
                      BluetoothDaemonPDU& aPDU)
  {
    DutModeRecvNotification::Dispatch(
      &BluetoothNotificationHandler::DutModeRecvNotification,
      DutModeRecvInitOp(aPDU));
  }

  void LeTestModeNtf(const BluetoothDaemonPDUHeader& aHeader,
                     BluetoothDaemonPDU& aPDU)
  {
    LeTestModeNotification::Dispatch(
      &BluetoothNotificationHandler::LeTestModeNotification,
      UnpackPDUInitOp(aPDU));
  }

  void HandleNtf(const BluetoothDaemonPDUHeader& aHeader,
                 BluetoothDaemonPDU& aPDU, void* aUserData)
  {
    static void (BluetoothDaemonCoreModule::* const HandleNtf[])(
      const BluetoothDaemonPDUHeader&, BluetoothDaemonPDU&) = {
      INIT_ARRAY_AT(0, &BluetoothDaemonCoreModule::AdapterStateChangedNtf),
      INIT_ARRAY_AT(1, &BluetoothDaemonCoreModule::AdapterPropertiesNtf),
      INIT_ARRAY_AT(2, &BluetoothDaemonCoreModule::RemoteDevicePropertiesNtf),
      INIT_ARRAY_AT(3, &BluetoothDaemonCoreModule::DeviceFoundNtf),
      INIT_ARRAY_AT(4, &BluetoothDaemonCoreModule::DiscoveryStateChangedNtf),
      INIT_ARRAY_AT(5, &BluetoothDaemonCoreModule::PinRequestNtf),
      INIT_ARRAY_AT(6, &BluetoothDaemonCoreModule::SspRequestNtf),
      INIT_ARRAY_AT(7, &BluetoothDaemonCoreModule::BondStateChangedNtf),
      INIT_ARRAY_AT(8, &BluetoothDaemonCoreModule::AclStateChangedNtf),
      INIT_ARRAY_AT(9, &BluetoothDaemonCoreModule::DutModeRecvNtf),
      INIT_ARRAY_AT(10, &BluetoothDaemonCoreModule::LeTestModeNtf)
    };

    MOZ_ASSERT(!NS_IsMainThread());

    uint8_t index = aHeader.mOpcode - 0x81;

    if (NS_WARN_IF(!(index < MOZ_ARRAY_LENGTH(HandleNtf))) ||
        NS_WARN_IF(!HandleNtf[index])) {
      return;
    }

    (this->*(HandleNtf[index]))(aHeader, aPDU);
  }

};

//
// Protocol handling
//

// |BluetoothDaemonProtocol| is the central class for communicating
// with the Bluetooth daemon. It maintains both socket connections
// to the external daemon and implements the complete HAL protocol
// by inheriting from base-class modules.
//
// Each |BluetoothDaemon*Module| class implements an individual
// module of the HAL protocol. Each class contains the abstract
// methods
//
//  - |Send|,
//  - |RegisterModule|, and
//  - |UnregisterModule|.
//
// Module classes use |Send| to send out command PDUs. The socket
// in |BluetoothDaemonProtocol| is required for sending. The abstract
// method hides all these internal details from the modules.
//
// |RegisterModule| is required during module initialization, when
// modules must register themselves at the daemon. The register command
// is not part of the module itself, but contained in the Setup module
// (id of 0x00). The abstract method |RegisterModule| allows modules to
// call into the Setup module for generating the register command.
//
// |UnregisterModule| works like |RegisterModule|, but for cleanups.
//
// |BluetoothDaemonProtocol| also handles PDU receiving. It implements
// the method |Handle| from |BluetoothDaemonPDUConsumer|. The socket
// connections of type |BluetoothDaemonConnection| invoke this method
// to forward received PDUs for processing by higher layers. The
// implementation of |Handle| checks the service id of the PDU and
// forwards it to the correct module class using the module's method
// |HandleSvc|. Further PDU processing is module-dependent.
//
// To summarize the interface between |BluetoothDaemonProtocol| and
// modules; the former implements the abstract methods
//
//  - |Send|,
//  - |RegisterModule|, and
//  - |UnregisterModule|,
//
// which allow modules to send out data. Each module implements the
// method
//
//  - |HandleSvc|,
//
const int BluetoothDaemonCoreModule::MAX_NUM_CLIENTS = 1;

// which is called by |BluetoothDaemonProtcol| to hand over received
// PDUs into a module.
//
class BluetoothDaemonProtocol final
  : public BluetoothDaemonPDUConsumer
  , public BluetoothDaemonSetupModule
  , public BluetoothDaemonCoreModule
  , public BluetoothDaemonSocketModule
  , public BluetoothDaemonHandsfreeModule
  , public BluetoothDaemonA2dpModule
  , public BluetoothDaemonAvrcpModule
{
public:
  BluetoothDaemonProtocol();

  void SetConnection(BluetoothDaemonConnection* aConnection);

  nsresult RegisterModule(uint8_t aId, uint8_t aMode, uint32_t aMaxNumClients,
                          BluetoothSetupResultHandler* aRes) override;

  nsresult UnregisterModule(uint8_t aId,
                            BluetoothSetupResultHandler* aRes) override;

  // Outgoing PDUs
  //

  nsresult Send(BluetoothDaemonPDU* aPDU, void* aUserData) override;

  void StoreUserData(const BluetoothDaemonPDU& aPDU) override;

  // Incoming PUDs
  //

  void Handle(BluetoothDaemonPDU& aPDU) override;

  void* FetchUserData(const BluetoothDaemonPDUHeader& aHeader);

private:
  void HandleSetupSvc(const BluetoothDaemonPDUHeader& aHeader,
                      BluetoothDaemonPDU& aPDU, void* aUserData);
  void HandleCoreSvc(const BluetoothDaemonPDUHeader& aHeader,
                     BluetoothDaemonPDU& aPDU, void* aUserData);
  void HandleSocketSvc(const BluetoothDaemonPDUHeader& aHeader,
                       BluetoothDaemonPDU& aPDU, void* aUserData);
  void HandleHandsfreeSvc(const BluetoothDaemonPDUHeader& aHeader,
                          BluetoothDaemonPDU& aPDU, void* aUserData);
  void HandleA2dpSvc(const BluetoothDaemonPDUHeader& aHeader,
                     BluetoothDaemonPDU& aPDU, void* aUserData);
  void HandleAvrcpSvc(const BluetoothDaemonPDUHeader& aHeader,
                      BluetoothDaemonPDU& aPDU, void* aUserData);

  BluetoothDaemonConnection* mConnection;
  nsTArray<void*> mUserDataQ;
};

BluetoothDaemonProtocol::BluetoothDaemonProtocol()
{ }

void
BluetoothDaemonProtocol::SetConnection(BluetoothDaemonConnection* aConnection)
{
  mConnection = aConnection;
}

nsresult
BluetoothDaemonProtocol::RegisterModule(uint8_t aId, uint8_t aMode,
                                        uint32_t aMaxNumClients,
                                        BluetoothSetupResultHandler* aRes)
{
  return BluetoothDaemonSetupModule::RegisterModuleCmd(aId, aMode,
                                                       aMaxNumClients, aRes);
}

nsresult
BluetoothDaemonProtocol::UnregisterModule(uint8_t aId,
                                          BluetoothSetupResultHandler* aRes)
{
  return BluetoothDaemonSetupModule::UnregisterModuleCmd(aId, aRes);
}

nsresult
BluetoothDaemonProtocol::Send(BluetoothDaemonPDU* aPDU, void* aUserData)
{
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(aPDU);

  aPDU->SetConsumer(this);
  aPDU->SetUserData(aUserData);
  aPDU->UpdateHeader();

  if (mConnection->GetConnectionStatus() == SOCKET_DISCONNECTED) {
    BT_LOGR("Connection to Bluetooth daemon is closed.");
    return NS_ERROR_FAILURE;
  }

  mConnection->SendSocketData(aPDU); // Forward PDU to command channel

  return NS_OK;
}

void
BluetoothDaemonProtocol::HandleSetupSvc(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  void* aUserData)
{
  BluetoothDaemonSetupModule::HandleSvc(aHeader, aPDU, aUserData);
}

void
BluetoothDaemonProtocol::HandleCoreSvc(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  void* aUserData)
{
  BluetoothDaemonCoreModule::HandleSvc(aHeader, aPDU, aUserData);
}

void
BluetoothDaemonProtocol::HandleSocketSvc(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  void* aUserData)
{
  BluetoothDaemonSocketModule::HandleSvc(aHeader, aPDU, aUserData);
}

void
BluetoothDaemonProtocol::HandleHandsfreeSvc(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  void* aUserData)
{
  BluetoothDaemonHandsfreeModule::HandleSvc(aHeader, aPDU, aUserData);
}

void
BluetoothDaemonProtocol::HandleA2dpSvc(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  void* aUserData)
{
  BluetoothDaemonA2dpModule::HandleSvc(aHeader, aPDU, aUserData);
}

void
BluetoothDaemonProtocol::HandleAvrcpSvc(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  void* aUserData)
{
  BluetoothDaemonAvrcpModule::HandleSvc(aHeader, aPDU, aUserData);
}

void
BluetoothDaemonProtocol::Handle(BluetoothDaemonPDU& aPDU)
{
  static void (BluetoothDaemonProtocol::* const HandleSvc[])(
    const BluetoothDaemonPDUHeader&, BluetoothDaemonPDU&, void*) = {
    INIT_ARRAY_AT(0x00, &BluetoothDaemonProtocol::HandleSetupSvc),
    INIT_ARRAY_AT(0x01, &BluetoothDaemonProtocol::HandleCoreSvc),
    INIT_ARRAY_AT(0x02, &BluetoothDaemonProtocol::HandleSocketSvc),
    INIT_ARRAY_AT(0x03, nullptr), // HID host
    INIT_ARRAY_AT(0x04, nullptr), // PAN
    INIT_ARRAY_AT(BluetoothDaemonHandsfreeModule::SERVICE_ID,
      &BluetoothDaemonProtocol::HandleHandsfreeSvc),
    INIT_ARRAY_AT(BluetoothDaemonA2dpModule::SERVICE_ID,
      &BluetoothDaemonProtocol::HandleA2dpSvc),
    INIT_ARRAY_AT(0x07, nullptr), // Health
    INIT_ARRAY_AT(BluetoothDaemonAvrcpModule::SERVICE_ID,
      &BluetoothDaemonProtocol::HandleAvrcpSvc)
  };

  BluetoothDaemonPDUHeader header;

  if (NS_FAILED(UnpackPDU(aPDU, header)) ||
      NS_WARN_IF(!(header.mService < MOZ_ARRAY_LENGTH(HandleSvc))) ||
      NS_WARN_IF(!(HandleSvc[header.mService]))) {
    return;
  }

  (this->*(HandleSvc[header.mService]))(header, aPDU, FetchUserData(header));
}

void
BluetoothDaemonProtocol::StoreUserData(const BluetoothDaemonPDU& aPDU)
{
  MOZ_ASSERT(!NS_IsMainThread());

  mUserDataQ.AppendElement(aPDU.GetUserData());
}

void*
BluetoothDaemonProtocol::FetchUserData(const BluetoothDaemonPDUHeader& aHeader)
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (aHeader.mOpcode & 0x80) {
    return nullptr; // Ignore notifications
  }

  void* userData = mUserDataQ.ElementAt(0);
  mUserDataQ.RemoveElementAt(0);

  return userData;
}

//
// Listen socket
//

class BluetoothDaemonListenSocket final : public ipc::ListenSocket
{
public:
  BluetoothDaemonListenSocket(BluetoothDaemonInterface* aInterface);

  // Connection state
  //

  void OnConnectSuccess() override;
  void OnConnectError() override;
  void OnDisconnect() override;

private:
  BluetoothDaemonInterface* mInterface;
};

BluetoothDaemonListenSocket::BluetoothDaemonListenSocket(
  BluetoothDaemonInterface* aInterface)
  : mInterface(aInterface)
{ }

void
BluetoothDaemonListenSocket::OnConnectSuccess()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInterface);

  mInterface->OnConnectSuccess(BluetoothDaemonInterface::LISTEN_SOCKET);
}

void
BluetoothDaemonListenSocket::OnConnectError()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInterface);

  mInterface->OnConnectError(BluetoothDaemonInterface::LISTEN_SOCKET);
}

void
BluetoothDaemonListenSocket::OnDisconnect()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInterface);

  mInterface->OnDisconnect(BluetoothDaemonInterface::LISTEN_SOCKET);
}

//
// Channels
//

class BluetoothDaemonChannel final : public BluetoothDaemonConnection
{
public:
  BluetoothDaemonChannel(BluetoothDaemonInterface* aInterface,
                         BluetoothDaemonInterface::Channel aChannel,
                         BluetoothDaemonPDUConsumer* aConsumer);

  // SocketBase
  //

  void OnConnectSuccess() override;
  void OnConnectError() override;
  void OnDisconnect() override;

  // ConnectionOrientedSocket
  //

  ConnectionOrientedSocketIO* GetIO() override;

private:
  BluetoothDaemonInterface* mInterface;
  BluetoothDaemonInterface::Channel mChannel;
  BluetoothDaemonPDUConsumer* mConsumer;
};

BluetoothDaemonChannel::BluetoothDaemonChannel(
  BluetoothDaemonInterface* aInterface,
  BluetoothDaemonInterface::Channel aChannel,
  BluetoothDaemonPDUConsumer* aConsumer)
  : mInterface(aInterface)
  , mChannel(aChannel)
  , mConsumer(aConsumer)
{ }

void
BluetoothDaemonChannel::OnConnectSuccess()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInterface);

  mInterface->OnConnectSuccess(mChannel);
}

void
BluetoothDaemonChannel::OnConnectError()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInterface);

  mInterface->OnConnectError(mChannel);
}

void
BluetoothDaemonChannel::OnDisconnect()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInterface);

  mInterface->OnDisconnect(mChannel);
}

ConnectionOrientedSocketIO*
BluetoothDaemonChannel::GetIO()
{
  return PrepareAccept(mConsumer);
}

//
// Interface
//

/* returns the container structure of a variable; _t is the container's
 * type, _v the name of the variable, and _m is _v's field within _t
 */
#define container(_t, _v, _m) \
  ( (_t*)( ((const unsigned char*)(_v)) - offsetof(_t, _m) ) )


static bool
IsDaemonRunning()
{
  char value[PROPERTY_VALUE_MAX];
  NS_WARN_IF(property_get("init.svc.bluetoothd", value, "") < 0);
  if (strcmp(value, "running")) {
    BT_LOGR("[RESTART] Bluetooth daemon state <%s>", value);
    return false;
  }

  return true;
}

BluetoothDaemonInterface*
BluetoothDaemonInterface::GetInstance()
{
  static BluetoothDaemonInterface* sBluetoothInterface;

  if (sBluetoothInterface) {
    return sBluetoothInterface;
  }

  sBluetoothInterface = new BluetoothDaemonInterface();

  return sBluetoothInterface;
}

BluetoothDaemonInterface::BluetoothDaemonInterface()
{ }

BluetoothDaemonInterface::~BluetoothDaemonInterface()
{ }

class BluetoothDaemonInterface::StartDaemonTask final : public Task
{
public:
  StartDaemonTask(BluetoothDaemonInterface* aInterface,
                  const nsACString& aCommand)
    : mInterface(aInterface)
    , mCommand(aCommand)
  {
    MOZ_ASSERT(mInterface);
  }

  void Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    BT_LOGR("Start Daemon Task");
    // Start Bluetooth daemon again
    if (NS_WARN_IF(property_set("ctl.start", mCommand.get()) < 0)) {
      mInterface->OnConnectError(CMD_CHANNEL);
    }

    // We're done if Bluetooth daemon is already running
    if (IsDaemonRunning()) {
      return;
    }

    // Otherwise try again later
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
      new StartDaemonTask(mInterface, mCommand), sRetryInterval);
  }

private:
  BluetoothDaemonInterface* mInterface;
  nsCString mCommand;
};

class BluetoothDaemonInterface::InitResultHandler final
  : public BluetoothSetupResultHandler
{
public:
  InitResultHandler(BluetoothDaemonInterface* aInterface,
                    BluetoothResultHandler* aRes)
    : mInterface(aInterface)
    , mRes(aRes)
    , mRegisteredSocketModule(false)
  {
    MOZ_ASSERT(mInterface);
  }

  // We need to call methods from the |BluetoothResultHandler|. Since
  // we're already on the main thread and returned from Init, we don't
  // need to dispatch a new runnable.

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (mRes) {
      mRes->OnError(aStatus);
    }
  }

  void RegisterModule() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mInterface->mProtocol);

    if (!mRegisteredSocketModule) {
      mRegisteredSocketModule = true;
      // Init, step 5: Register Socket module
      mInterface->mProtocol->RegisterModuleCmd(0x02, 0x00,
        BluetoothDaemonSocketModule::MAX_NUM_CLIENTS, this);
    } else if (mRes) {
      // Init, step 6: Signal success to caller
      mRes->Init();
    }
  }

private:
  BluetoothDaemonInterface* mInterface;
  nsRefPtr<BluetoothResultHandler> mRes;
  bool mRegisteredSocketModule;
};

void
BluetoothDaemonInterface::OnConnectSuccess(enum Channel aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mResultHandlerQ.IsEmpty());

  switch (aChannel) {
    case LISTEN_SOCKET: {
        // Init, step 2: Start Bluetooth daemon */
        nsCString value("bluetoothd:-a ");
        value.Append(mListenSocketName);
        if (NS_WARN_IF(property_set("ctl.start", value.get()) < 0)) {
          OnConnectError(CMD_CHANNEL);
        }

        /*
         * If Bluetooth daemon is not running, retry to start it later.
         *
         * This condition happens when when we restart Bluetooth daemon
         * immediately after it crashed, as the daemon state remains 'stopping'
         * instead of 'stopped'. Due to the limitation of property service,
         * hereby add delay. See Bug 1143925 Comment 41.
         */
        if (!IsDaemonRunning()) {
          MessageLoop::current()->PostDelayedTask(FROM_HERE,
              new StartDaemonTask(this, value), sRetryInterval);
        }
      }
      break;
    case CMD_CHANNEL:
      // Init, step 3: Listen for notification channel...
      if (!mNtfChannel) {
        mNtfChannel = new BluetoothDaemonChannel(this, NTF_CHANNEL, mProtocol);
      } else if (
        NS_WARN_IF(mNtfChannel->GetConnectionStatus() == SOCKET_CONNECTED)) {
        /* Notification channel should not be open; let's close it. */
        mNtfChannel->Close();
      }
      if (NS_FAILED(mListenSocket->Listen(mNtfChannel))) {
        OnConnectError(NTF_CHANNEL);
      }
      break;
    case NTF_CHANNEL: {
        nsRefPtr<BluetoothResultHandler> res = mResultHandlerQ.ElementAt(0);
        mResultHandlerQ.RemoveElementAt(0);

        // Init, step 4: Register Core module
        nsresult rv = mProtocol->RegisterModuleCmd(
          0x01, 0x00, BluetoothDaemonCoreModule::MAX_NUM_CLIENTS,
          new InitResultHandler(this, res));
        if (NS_FAILED(rv) && res) {
          DispatchError(res, STATUS_FAIL);
        }
      }
      break;
  }
}

void
BluetoothDaemonInterface::OnConnectError(enum Channel aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mResultHandlerQ.IsEmpty());

  switch (aChannel) {
    case NTF_CHANNEL:
      // Close command channel
      mCmdChannel->Close();
    case CMD_CHANNEL:
      // Stop daemon and close listen socket
      unused << NS_WARN_IF(property_set("ctl.stop", "bluetoothd"));
      mListenSocket->Close();
    case LISTEN_SOCKET:
      if (!mResultHandlerQ.IsEmpty()) {
        // Signal error to caller
        nsRefPtr<BluetoothResultHandler> res = mResultHandlerQ.ElementAt(0);
        mResultHandlerQ.RemoveElementAt(0);

        if (res) {
          DispatchError(res, STATUS_FAIL);
        }
      }
      break;
  }
}

/*
 * Three cases for restarting:
 * a) during startup
 * b) during regular service
 * c) during shutdown
 * For (a)/(c) cases, mResultHandlerQ contains an element, but case (b)
 * mResultHandlerQ shall be empty. The following procedure to recover from crashed
 * consists of several steps for case (b).
 * 1) Close listen socket.
 * 2) Wait for all sockets disconnected and inform BluetoothServiceBluedroid to
 * perform the regular stop bluetooth procedure.
 * 3) When stop bluetooth procedures complete, fire
 * AdapterStateChangedNotification to cleanup all necessary data members and
 * deinit ProfileManagers.
 * 4) After all resources cleanup, call |StartBluetooth|
 */
void
BluetoothDaemonInterface::OnDisconnect(enum Channel aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());

  switch (aChannel) {
    case CMD_CHANNEL:
      // We don't have to do anything here. Step 4 is triggered
      // by the daemon.
      break;
    case NTF_CHANNEL:
      // Cleanup, step 4 (Recovery, step 1): Close listen socket
      mListenSocket->Close();
      break;
    case LISTEN_SOCKET:
      if (!mResultHandlerQ.IsEmpty()) {
        nsRefPtr<BluetoothResultHandler> res = mResultHandlerQ.ElementAt(0);
        mResultHandlerQ.RemoveElementAt(0);
        // Cleanup, step 5: Signal success to caller
        if (res) {
          res->Cleanup();
        }
      }
      break;
  }

  /* For recovery make sure all sockets disconnected, in order to avoid
   * the remaining disconnects interfere with the restart procedure.
   */
  if (sNotificationHandler && mResultHandlerQ.IsEmpty()) {
    if (mListenSocket->GetConnectionStatus() == SOCKET_DISCONNECTED &&
        mCmdChannel->GetConnectionStatus() == SOCKET_DISCONNECTED &&
        mNtfChannel->GetConnectionStatus() == SOCKET_DISCONNECTED) {
      // Assume daemon crashed during regular service; notify
      // BluetoothServiceBluedroid to prepare restart-daemon procedure
      sNotificationHandler->BackendErrorNotification(true);
      sNotificationHandler = nullptr;
    }
  }
}

nsresult
BluetoothDaemonInterface::CreateRandomAddressString(
  const nsACString& aPrefix, unsigned long aPostfixLength,
  nsACString& aAddress)
{
  static const char sHexChar[16] = {
    [0x0] = '0', [0x1] = '1', [0x2] = '2', [0x3] = '3',
    [0x4] = '4', [0x5] = '5', [0x6] = '6', [0x7] = '7',
    [0x8] = '8', [0x9] = '9', [0xa] = 'a', [0xb] = 'b',
    [0xc] = 'c', [0xd] = 'd', [0xe] = 'e', [0xf] = 'f'
  };

  unsigned short seed[3];

  if (NS_WARN_IF(!PR_GetRandomNoise(seed, sizeof(seed)))) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  aAddress = aPrefix;
  aAddress.Append('-');

  while (aPostfixLength) {

    // Android doesn't provide rand_r, so we use nrand48 here,
    // even though it's deprecated.
    long value = nrand48(seed);

    size_t bits = sizeof(value) * CHAR_BIT;

    while ((bits > 4) && aPostfixLength) {
      aAddress.Append(sHexChar[value&0xf]);
      bits -= 4;
      value >>= 4;
      --aPostfixLength;
    }
  }

  return NS_OK;
}

/*
 * The init procedure consists of several steps.
 *
 *  (1) Start listening for the command channel's socket connection: We
 *      do this before anything else, so that we don't miss connection
 *      requests from the Bluetooth daemon. This step will create a
 *      listen socket.
 *
 *  (2) Start the Bluetooth daemon: When the daemon starts up it will
 *      open two socket connections to Gecko and thus create the command
 *      and notification channels. Gecko already opened the listen socket
 *      in step (1). Step (2) ends with the creation of the command channel.
 *
 *  (3) Start listening for the notification channel's socket connection:
 *      At the end of step (2), the command channel was opened by the
 *      daemon. In step (3), the daemon immediately tries to open the
 *      next socket for the notification channel. Gecko will accept the
 *      incoming connection request for the notification channel. The
 *      listen socket remained open after step (2), so there's no race
 *      condition between Gecko and the Bluetooth daemon.
 *
 *  (4)(5) Register Core and Socket modules: The Core and Socket modules
 *      are always available and have to be registered after opening the
 *      socket connections during the initialization.
 *
 *  (6) Signal success to the caller.
 *
 * If any step fails, we roll-back the procedure and signal an error to the
 * caller.
 */
void
BluetoothDaemonInterface::Init(
  BluetoothNotificationHandler* aNotificationHandler,
  BluetoothResultHandler* aRes)
{
#define BASE_SOCKET_NAME "bluetoothd"
  static unsigned long POSTFIX_LENGTH = 16;

  // If we could not cleanup properly before and an old
  // instance of the daemon is still running, we kill it
  // here.
  unused << NS_WARN_IF(property_set("ctl.stop", "bluetoothd"));

  sNotificationHandler = aNotificationHandler;

  mResultHandlerQ.AppendElement(aRes);

  if (!mProtocol) {
    mProtocol = new BluetoothDaemonProtocol();
  }

  if (!mListenSocket) {
    mListenSocket = new BluetoothDaemonListenSocket(this);
  }

  // Init, step 1: Listen for command channel... */

  if (!mCmdChannel) {
    mCmdChannel = new BluetoothDaemonChannel(this, CMD_CHANNEL, mProtocol);
  } else if (
    NS_WARN_IF(mCmdChannel->GetConnectionStatus() == SOCKET_CONNECTED)) {
    // Command channel should not be open; let's close it.
    mCmdChannel->Close();
  }

  // The listen socket's name is generated with a random postfix. This
  // avoids naming collisions if we still have a listen socket from a
  // previously failed cleanup. It also makes it hard for malicious
  // external programs to capture the socket name or connect before
  // the daemon can do so. If no random postfix can be generated, we
  // simply use the base name as-is.
  nsresult rv = CreateRandomAddressString(NS_LITERAL_CSTRING(BASE_SOCKET_NAME),
                                          POSTFIX_LENGTH,
                                          mListenSocketName);
  if (NS_FAILED(rv)) {
    mListenSocketName.AssignLiteral(BASE_SOCKET_NAME);
  }

  rv = mListenSocket->Listen(new BluetoothDaemonConnector(mListenSocketName),
                             mCmdChannel);
  if (NS_FAILED(rv)) {
    OnConnectError(CMD_CHANNEL);
    return;
  }

  // The protocol implementation needs a command channel for
  // sending commands to the daemon. We set it here, because
  // this is the earliest time when it's available.
  mProtocol->SetConnection(mCmdChannel);
}

class BluetoothDaemonInterface::CleanupResultHandler final
  : public BluetoothSetupResultHandler
{
public:
  CleanupResultHandler(BluetoothDaemonInterface* aInterface)
    : mInterface(aInterface)
    , mUnregisteredCoreModule(false)
  {
    MOZ_ASSERT(mInterface);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    Proceed();
  }

  void UnregisterModule() override
  {
    Proceed();
  }

private:
  void Proceed()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mInterface->mProtocol);

    if (!mUnregisteredCoreModule) {
      mUnregisteredCoreModule = true;
      // Cleanup, step 2: Unregister Core module
      mInterface->mProtocol->UnregisterModuleCmd(0x01, this);
    } else {
      // Cleanup, step 3: Close command channel
      mInterface->mCmdChannel->Close();
    }
  }

  BluetoothDaemonInterface* mInterface;
  bool mUnregisteredCoreModule;
};

/*
 * Cleaning up is inverse to initialization, except for the shutdown
 * of the socket connections in step (3)
 *
 *  (1)(2) Unregister Socket and Core modules: These modules have been
 *      registered during initialization and need to be unregistered
 *      here. We assume that all other modules are already unregistered.
 *
 *  (3) Close command socket: We only close the command socket. The
 *      daemon will then send any final notifications and close the
 *      notification socket on its side. Once we see the notification
 *      socket's disconnect, we continue with the cleanup.
 *
 *  (4) Close listen socket: The listen socket is not active any longer
 *      and we simply close it.
 *
 *  (5) Signal success to the caller.
 *
 * We don't have to stop the daemon explicitly. It will cleanup and quit
 * after it closed the notification socket.
 *
 * Rolling-back half-completed cleanups is not possible. In the case of
 * an error, we simply push forward and try to recover during the next
 * initialization.
 */
void
BluetoothDaemonInterface::Cleanup(BluetoothResultHandler* aRes)
{

  sNotificationHandler = nullptr;

  // Cleanup, step 1: Unregister Socket module
  nsresult rv = mProtocol->UnregisterModuleCmd(
    0x02, new CleanupResultHandler(this));
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
    return;
  }

  mResultHandlerQ.AppendElement(aRes);
}

void
BluetoothDaemonInterface::Enable(BluetoothResultHandler* aRes)
{
  nsresult rv =
    static_cast<BluetoothDaemonCoreModule*>(mProtocol)->EnableCmd(aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonInterface::Disable(BluetoothResultHandler* aRes)
{
  nsresult rv =
    static_cast<BluetoothDaemonCoreModule*>(mProtocol)->DisableCmd(aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Adapter Properties */

void
BluetoothDaemonInterface::GetAdapterProperties(BluetoothResultHandler* aRes)
{
  nsresult rv = static_cast<BluetoothDaemonCoreModule*>
    (mProtocol)->GetAdapterPropertiesCmd(aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonInterface::GetAdapterProperty(const nsAString& aName,
                                             BluetoothResultHandler* aRes)
{
  nsresult rv = static_cast<BluetoothDaemonCoreModule*>
    (mProtocol)->GetAdapterPropertyCmd(aName, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonInterface::SetAdapterProperty(
  const BluetoothNamedValue& aProperty, BluetoothResultHandler* aRes)
{
  nsresult rv = static_cast<BluetoothDaemonCoreModule*>
    (mProtocol)->SetAdapterPropertyCmd(aProperty, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Remote Device Properties */

void
BluetoothDaemonInterface::GetRemoteDeviceProperties(
  const nsAString& aRemoteAddr, BluetoothResultHandler* aRes)
{
  nsresult rv = static_cast<BluetoothDaemonCoreModule*>
    (mProtocol)->GetRemoteDevicePropertiesCmd(aRemoteAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonInterface::GetRemoteDeviceProperty(
  const nsAString& aRemoteAddr, const nsAString& aName,
  BluetoothResultHandler* aRes)
{
  nsresult rv = static_cast<BluetoothDaemonCoreModule*>
    (mProtocol)->GetRemoteDevicePropertyCmd(aRemoteAddr, aName, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonInterface::SetRemoteDeviceProperty(
  const nsAString& aRemoteAddr, const BluetoothNamedValue& aProperty,
  BluetoothResultHandler* aRes)
{
  nsresult rv = static_cast<BluetoothDaemonCoreModule*>
    (mProtocol)->SetRemoteDevicePropertyCmd(aRemoteAddr, aProperty, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Remote Services */

void
BluetoothDaemonInterface::GetRemoteServiceRecord(const nsAString& aRemoteAddr,
                                                 const uint8_t aUuid[16],
                                                 BluetoothResultHandler* aRes)
{
  nsresult rv = static_cast<BluetoothDaemonCoreModule*>
    (mProtocol)->GetRemoteServiceRecordCmd(aRemoteAddr, aUuid, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonInterface::GetRemoteServices(const nsAString& aRemoteAddr,
                                            BluetoothResultHandler* aRes)
{
  nsresult rv = static_cast<BluetoothDaemonCoreModule*>
    (mProtocol)->GetRemoteServicesCmd(aRemoteAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Discovery */

void
BluetoothDaemonInterface::StartDiscovery(BluetoothResultHandler* aRes)
{
  nsresult rv = static_cast<BluetoothDaemonCoreModule*>
    (mProtocol)->StartDiscoveryCmd(aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonInterface::CancelDiscovery(BluetoothResultHandler* aRes)
{
  nsresult rv = static_cast<BluetoothDaemonCoreModule*>
    (mProtocol)->CancelDiscoveryCmd(aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Bonds */

void
BluetoothDaemonInterface::CreateBond(const nsAString& aBdAddr,
                                     BluetoothTransport aTransport,
                                     BluetoothResultHandler* aRes)
{
  nsresult rv = static_cast<BluetoothDaemonCoreModule*>
    (mProtocol)->CreateBondCmd(aBdAddr, aTransport, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonInterface::RemoveBond(const nsAString& aBdAddr,
                                     BluetoothResultHandler* aRes)
{
  nsresult rv = static_cast<BluetoothDaemonCoreModule*>
    (mProtocol)->RemoveBondCmd(aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonInterface::CancelBond(const nsAString& aBdAddr,
                                     BluetoothResultHandler* aRes)
{
  nsresult rv = static_cast<BluetoothDaemonCoreModule*>
    (mProtocol)->CancelBondCmd(aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Connection */

void
BluetoothDaemonInterface::GetConnectionState(const nsAString& aBdAddr,
                                             BluetoothResultHandler* aRes)
{
  // NO-OP: no corresponding interface of current BlueZ
}

/* Authentication */

void
BluetoothDaemonInterface::PinReply(const nsAString& aBdAddr, bool aAccept,
                                   const nsAString& aPinCode,
                                   BluetoothResultHandler* aRes)
{
  nsresult rv = static_cast<BluetoothDaemonCoreModule*>
    (mProtocol)->PinReplyCmd(aBdAddr, aAccept, aPinCode, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonInterface::SspReply(const nsAString& aBdAddr,
                                   BluetoothSspVariant aVariant,
                                   bool aAccept, uint32_t aPasskey,
                                   BluetoothResultHandler* aRes)
{
  nsresult rv = static_cast<BluetoothDaemonCoreModule*>
    (mProtocol)->SspReplyCmd(aBdAddr, aVariant, aAccept, aPasskey, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* DUT Mode */

void
BluetoothDaemonInterface::DutModeConfigure(bool aEnable,
                                           BluetoothResultHandler* aRes)
{
  nsresult rv = static_cast<BluetoothDaemonCoreModule*>
    (mProtocol)->DutModeConfigureCmd(aEnable, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonInterface::DutModeSend(uint16_t aOpcode, uint8_t* aBuf,
                                      uint8_t aLen,
                                      BluetoothResultHandler* aRes)
{
  nsresult rv = static_cast<BluetoothDaemonCoreModule*>
    (mProtocol)->DutModeSendCmd(aOpcode, aBuf, aLen, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* LE Mode */

void
BluetoothDaemonInterface::LeTestMode(uint16_t aOpcode, uint8_t* aBuf,
                                     uint8_t aLen,
                                     BluetoothResultHandler* aRes)
{
  nsresult rv = static_cast<BluetoothDaemonCoreModule*>
    (mProtocol)->LeTestModeCmd(aOpcode, aBuf, aLen, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Energy Information */

void
BluetoothDaemonInterface::ReadEnergyInfo(BluetoothResultHandler* aRes)
{
  // NO-OP: no corresponding interface of current BlueZ
}

void
BluetoothDaemonInterface::DispatchError(BluetoothResultHandler* aRes,
                                        BluetoothStatus aStatus)
{
  BluetoothResultRunnable1<
    BluetoothResultHandler, void, BluetoothStatus, BluetoothStatus>::Dispatch(
    aRes, &BluetoothResultHandler::OnError,
    ConstantInitOp1<BluetoothStatus>(aStatus));
}

void
BluetoothDaemonInterface::DispatchError(BluetoothResultHandler* aRes,
                                        nsresult aRv)
{
  BluetoothStatus status;

  if (NS_WARN_IF(NS_FAILED(Convert(aRv, status)))) {
    status = STATUS_FAIL;
  }
  DispatchError(aRes, status);
}

// Profile Interfaces
//

BluetoothSocketInterface*
BluetoothDaemonInterface::GetBluetoothSocketInterface()
{
  if (mSocketInterface) {
    return mSocketInterface;
  }

  mSocketInterface = new BluetoothDaemonSocketInterface(mProtocol);

  return mSocketInterface;
}

BluetoothHandsfreeInterface*
BluetoothDaemonInterface::GetBluetoothHandsfreeInterface()
{
  if (mHandsfreeInterface) {
    return mHandsfreeInterface;
  }

  mHandsfreeInterface = new BluetoothDaemonHandsfreeInterface(mProtocol);

  return mHandsfreeInterface;
}

BluetoothA2dpInterface*
BluetoothDaemonInterface::GetBluetoothA2dpInterface()
{
  if (mA2dpInterface) {
    return mA2dpInterface;
  }

  mA2dpInterface = new BluetoothDaemonA2dpInterface(mProtocol);

  return mA2dpInterface;
}

BluetoothAvrcpInterface*
BluetoothDaemonInterface::GetBluetoothAvrcpInterface()
{
  if (mAvrcpInterface) {
    return mAvrcpInterface;
  }

  mAvrcpInterface = new BluetoothDaemonAvrcpInterface(mProtocol);

  return mAvrcpInterface;
}

BluetoothGattInterface*
BluetoothDaemonInterface::GetBluetoothGattInterface()
{
  return nullptr;
}

END_BLUETOOTH_NAMESPACE
