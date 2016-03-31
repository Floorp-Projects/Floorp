/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * The registry interface gives yo access to the Sensors daemon's Registry
 * service. The purpose of the service is to register and setup all other
 * services, and make them available.
 *
 * All public methods and callback methods run on the main thread.
 */

#ifndef hal_gonk_GonkSensorsRegistryInterface_h
#define hal_gonk_GonkSensorsRegistryInterface_h

#include <mozilla/ipc/DaemonRunnables.h>
#include <mozilla/ipc/DaemonSocketMessageHandlers.h>
#include "SensorsTypes.h"

namespace mozilla {
namespace ipc {

class DaemonSocketPDU;
class DaemonSocketPDUHeader;

}
}

namespace mozilla {
namespace hal {

class SensorsInterface;

using mozilla::ipc::DaemonSocketPDU;
using mozilla::ipc::DaemonSocketPDUHeader;
using mozilla::ipc::DaemonSocketResultHandler;

/**
 * This class is the result-handler interface for the Sensors
 * Registry interface. Methods always run on the main thread.
 */
class GonkSensorsRegistryResultHandler : public DaemonSocketResultHandler
{
public:

  /**
   * Called if a registry command failed.
   *
   * @param aError The error code.
   */
  virtual void OnError(SensorsError aError);

  /**
   * The callback method for |GonkSensorsRegistryInterface::RegisterModule|.
   *
   * @param aProtocolVersion The daemon's protocol version. Make sure it's
   *                         compatible with Gecko's implementation.
   */
  virtual void RegisterModule(uint32_t aProtocolVersion);

  /**
   * The callback method for |SensorsRegsitryInterface::UnregisterModule|.
   */
  virtual void UnregisterModule();

protected:
  virtual ~GonkSensorsRegistryResultHandler();
};

/**
 * This is the module class for the Sensors registry component. It handles
 * PDU packing and unpacking. Methods are either executed on the main thread
 * or the I/O thread.
 *
 * This is an internal class, use |GonkSensorsRegistryInterface| instead.
 */
class GonkSensorsRegistryModule
{
public:
  enum {
    SERVICE_ID = 0x00
  };

  enum {
    OPCODE_ERROR = 0x00,
    OPCODE_REGISTER_MODULE = 0x01,
    OPCODE_UNREGISTER_MODULE = 0x02
  };

  virtual nsresult Send(DaemonSocketPDU* aPDU,
                        DaemonSocketResultHandler* aRes) = 0;

  //
  // Commands
  //

  nsresult RegisterModuleCmd(uint8_t aId,
                             GonkSensorsRegistryResultHandler* aRes);

  nsresult UnregisterModuleCmd(uint8_t aId,
                               GonkSensorsRegistryResultHandler* aRes);

protected:
  virtual ~GonkSensorsRegistryModule();

  void HandleSvc(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU, DaemonSocketResultHandler* aRes);

  //
  // Responses
  //

  typedef mozilla::ipc::DaemonResultRunnable0<
    GonkSensorsRegistryResultHandler, void>
    ResultRunnable;

  typedef mozilla::ipc::DaemonResultRunnable1<
    GonkSensorsRegistryResultHandler, void, uint32_t, uint32_t>
    Uint32ResultRunnable;

  typedef mozilla::ipc::DaemonResultRunnable1<
    GonkSensorsRegistryResultHandler, void, SensorsError, SensorsError>
    ErrorRunnable;

  void ErrorRsp(const DaemonSocketPDUHeader& aHeader,
                DaemonSocketPDU& aPDU,
                GonkSensorsRegistryResultHandler* aRes);

  void RegisterModuleRsp(const DaemonSocketPDUHeader& aHeader,
                         DaemonSocketPDU& aPDU,
                         GonkSensorsRegistryResultHandler* aRes);

  void UnregisterModuleRsp(const DaemonSocketPDUHeader& aHeader,
                           DaemonSocketPDU& aPDU,
                           GonkSensorsRegistryResultHandler* aRes);
};

/**
 * This class implements the public interface to the Sensors Registry
 * component. Use |SensorsInterface::GetRegistryInterface| to retrieve
 * an instance. All methods run on the main thread.
 */
class GonkSensorsRegistryInterface final
{
public:
  GonkSensorsRegistryInterface(GonkSensorsRegistryModule* aModule);
  ~GonkSensorsRegistryInterface();

  /**
   * Sends a RegisterModule command to the Sensors daemon. When the
   * result handler's |RegisterModule| method gets called, the service
   * has been registered successfully and can be used.
   *
   * @param aId The id of the service that is to be registered.
   * @param aRes The result handler.
   */
  void RegisterModule(uint8_t aId, GonkSensorsRegistryResultHandler* aRes);

  /**
   * Sends an UnregisterModule command to the Sensors daemon. The service
   * should not be used afterwards until it has been registered again.
   *
   * @param aId The id of the service that is to be unregistered.
   * @param aRes The result handler.
   */
  void UnregisterModule(uint8_t aId, GonkSensorsRegistryResultHandler* aRes);

private:
  void DispatchError(GonkSensorsRegistryResultHandler* aRes,
                     SensorsError aError);
  void DispatchError(GonkSensorsRegistryResultHandler* aRes,
                     nsresult aRv);

  GonkSensorsRegistryModule* mModule;
};

} // namespace hal
} // namespace mozilla

#endif // hal_gonk_GonkSensorsRegistryInterface_h
