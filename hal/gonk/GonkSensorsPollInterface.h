/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * The poll interface gives yo access to the Sensors daemon's Poll service,
 * which handles sensors. The poll service will inform you when sensors are
 * detected or removed from the system. You can activate (or deactivate)
 * existing sensors and poll will deliver the sensors' events.
 *
 * All public methods and callback methods run on the main thread.
 */

#ifndef hal_gonk_GonkSensorsPollInterface_h
#define hal_gonk_GonkSensorsPollInterface_h

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
 * Poll interface. Methods always run on the main thread.
 */
class GonkSensorsPollResultHandler : public DaemonSocketResultHandler
{
public:

  /**
   * Called if a poll command failed.
   *
   * @param aError The error code.
   */
  virtual void OnError(SensorsError aError);

  /**
   * The callback method for |GonkSensorsPollInterface::EnableSensor|.
   */
  virtual void EnableSensor();

  /**
   * The callback method for |GonkSensorsPollInterface::DisableSensor|.
   */
  virtual void DisableSensor();

  /**
   * The callback method for |GonkSensorsPollInterface::SetPeriod|.
   */
  virtual void SetPeriod();

protected:
  virtual ~GonkSensorsPollResultHandler();
};

/**
 * This is the notification-handler interface. Implement this classes
 * methods to handle event and notifications from the sensors daemon.
 */
class GonkSensorsPollNotificationHandler
{
public:

  /**
   * The notification handler for errors. You'll receive this call if
   * there's been a critical error in the daemon. Either try to handle
   * the error, or restart the daemon.
   *
   * @param aError The error code.
   */
  virtual void ErrorNotification(SensorsError aError);

  /**
   * This methods gets call when a new sensor has been detected.
   *
   * @param aId The sensor's id.
   * @param aType The sensor's type.
   * @param aRange The sensor's maximum value.
   * @param aResolution The minimum difference between two consecutive values.
   * @param aPower The sensor's power consumption (in mA).
   * @param aMinPeriod The minimum time between two events (in ns).
   * @param aMaxPeriod The maximum time between two events (in ns).
   * @param aTriggerMode The sensor's mode for triggering events.
   * @param aDeliveryMode The sensor's urgency for event delivery.
   */
  virtual void SensorDetectedNotification(int32_t aId, SensorsType aType,
                                          float aRange, float aResolution,
                                          float aPower, int32_t aMinPeriod,
                                          int32_t aMaxPeriod,
                                          SensorsTriggerMode aTriggerMode,
                                          SensorsDeliveryMode aDeliveryMode);

  /**
   * This methods gets call when an existing sensor has been removed.
   *
   * @param aId The sensor's id.
   */
  virtual void SensorLostNotification(int32_t aId);

  /**
   * This is the callback methods for sensor events. Only activated sensors
   * generate events. All sensors are disabled by default. The actual data
   * of the event depends on the sensor type.
   *
   * @param aId The sensor's id.
   * @param aEvent The event's data.
   */
  virtual void EventNotification(int32_t aId, const SensorsEvent& aEvent);

protected:
  virtual ~GonkSensorsPollNotificationHandler();
};

/**
 * This is the module class for the Sensors poll component. It handles PDU
 * packing and unpacking. Methods are either executed on the main thread or
 * the I/O thread.
 *
 * This is an internal class, use |GonkSensorsPollInterface| instead.
 */
class GonkSensorsPollModule
{
public:
  class NotificationHandlerWrapper;

  enum {
    SERVICE_ID = 0x01
  };

  enum {
    OPCODE_ERROR = 0x00,
    OPCODE_ENABLE_SENSOR = 0x01,
    OPCODE_DISABLE_SENSOR = 0x02,
    OPCODE_SET_PERIOD = 0x03
  };

  enum {
    MIN_PROTOCOL_VERSION = 1,
    MAX_PROTOCOL_VERSION = 1
  };

  virtual nsresult Send(DaemonSocketPDU* aPDU,
                        DaemonSocketResultHandler* aRes) = 0;

  nsresult SetProtocolVersion(unsigned long aProtocolVersion);

  //
  // Commands
  //

  nsresult EnableSensorCmd(int32_t aId,
                           GonkSensorsPollResultHandler* aRes);

  nsresult DisableSensorCmd(int32_t aId,
                            GonkSensorsPollResultHandler* aRes);

  nsresult SetPeriodCmd(int32_t aId, uint64_t aPeriod,
                        GonkSensorsPollResultHandler* aRes);

protected:
  GonkSensorsPollModule();
  virtual ~GonkSensorsPollModule();

  void HandleSvc(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU,
                 DaemonSocketResultHandler* aRes);

private:

  //
  // Responses
  //

  typedef mozilla::ipc::DaemonResultRunnable0<
    GonkSensorsPollResultHandler, void>
    ResultRunnable;

  typedef mozilla::ipc::DaemonResultRunnable1<
    GonkSensorsPollResultHandler, void, SensorsError, SensorsError>
    ErrorRunnable;

  void ErrorRsp(const DaemonSocketPDUHeader& aHeader,
                DaemonSocketPDU& aPDU,
                GonkSensorsPollResultHandler* aRes);

  void EnableSensorRsp(const DaemonSocketPDUHeader& aHeader,
                       DaemonSocketPDU& aPDU,
                       GonkSensorsPollResultHandler* aRes);

  void DisableSensorRsp(const DaemonSocketPDUHeader& aHeader,
                        DaemonSocketPDU& aPDU,
                        GonkSensorsPollResultHandler* aRes);

  void SetPeriodRsp(const DaemonSocketPDUHeader& aHeader,
                    DaemonSocketPDU& aPDU,
                    GonkSensorsPollResultHandler* aRes);

  void HandleRsp(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU,
                 DaemonSocketResultHandler* aRes);

  //
  // Notifications
  //

  typedef mozilla::ipc::DaemonNotificationRunnable1<
    NotificationHandlerWrapper, void, SensorsError>
    ErrorNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable9<
    NotificationHandlerWrapper, void, int32_t, SensorsType,
    float, float, float, int32_t, int32_t, SensorsTriggerMode,
    SensorsDeliveryMode>
    SensorDetectedNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable1<
    NotificationHandlerWrapper, void, int32_t>
    SensorLostNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void, int32_t, SensorsEvent, int32_t,
    const SensorsEvent&>
    EventNotification;

  class SensorDetectedInitOp;
  class SensorLostInitOp;
  class EventInitOp;

  void ErrorNtf(const DaemonSocketPDUHeader& aHeader,
                DaemonSocketPDU& aPDU);

  void SensorDetectedNtf(const DaemonSocketPDUHeader& aHeader,
                         DaemonSocketPDU& aPDU);

  void SensorLostNtf(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU);

  void EventNtf(const DaemonSocketPDUHeader& aHeader,
                DaemonSocketPDU& aPDU);

  void HandleNtf(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU,
                 DaemonSocketResultHandler* aRes);

private:
  unsigned long mProtocolVersion;
};

/**
 * This class implements the public interface to the Sensors poll
 * component. Use |SensorsInterface::GetPollInterface| to retrieve
 * an instance. All methods run on the main thread.
 */
class GonkSensorsPollInterface final
{
public:
  GonkSensorsPollInterface(GonkSensorsPollModule* aModule);
  ~GonkSensorsPollInterface();

  /**
   * This method sets the notification handler for poll notifications. Call
   * this method immediately after registering the module. Otherwise you won't
   * be able able to receive poll notifications. You may not free the handler
   * class while the poll component is regsitered.
   *
   * @param aNotificationHandler An instance of a poll notification handler.
   */
  void SetNotificationHandler(
    GonkSensorsPollNotificationHandler* aNotificationHandler);

  /**
   * This method sets the protocol version. You should set it to the
   * value that has been returned from the backend when registering the
   * Poll service. You cannot send or receive messages before setting
   * the protocol version.
   *
   * @param aProtocolVersion
   * @return NS_OK for supported versions, or an XPCOM error code otherwise.
   */
  nsresult SetProtocolVersion(unsigned long aProtocolVersion);

  /**
   * Enables an existing sensor. The sensor id will have been delivered in
   * a SensorDetectedNotification.
   *
   * @param aId The sensor's id.
   * @param aRes The result handler.
   */
  void EnableSensor(int32_t aId, GonkSensorsPollResultHandler* aRes);

  /**
   * Disables an existing sensor. The sensor id will have been delivered in
   * a SensorDetectedNotification.
   *
   * @param aId The sensor's id.
   * @param aRes The result handler.
   */
  void DisableSensor(int32_t aId, GonkSensorsPollResultHandler* aRes);

  /**
   * Sets the period for a sensor. The sensor id will have been delivered in
   * a SensorDetectedNotification. The value for the period should be between
   * the sensor's minimum and maximum period.
   *
   * @param aId The sensor's id.
   * @param aPeriod The sensor's new period.
   * @param aRes The result handler.
   */
  void SetPeriod(int32_t aId, uint64_t aPeriod, GonkSensorsPollResultHandler* aRes);

private:
  void DispatchError(GonkSensorsPollResultHandler* aRes, SensorsError aError);
  void DispatchError(GonkSensorsPollResultHandler* aRes, nsresult aRv);

  GonkSensorsPollModule* mModule;
};

} // hal
} // namespace mozilla

#endif // hal_gonk_GonkSensorsPollInterface_h
