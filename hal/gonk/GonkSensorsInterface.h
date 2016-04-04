/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * The sensors interface gives you access to the low-level sensors code
 * in a platform-independent manner. The interfaces in this file allow
 * for starting an stopping the sensors driver. Specific functionality
 * is implemented in sub-interfaces.
 */

#ifndef hal_gonk_GonkSensorsInterface_h
#define hal_gonk_GonkSensorsInterface_h

#include <mozilla/ipc/DaemonSocketConsumer.h>
#include <mozilla/ipc/DaemonSocketMessageHandlers.h>
#include <mozilla/ipc/ListenSocketConsumer.h>
#include <mozilla/UniquePtr.h>
#include "SensorsTypes.h"

namespace mozilla {
namespace ipc {

class DaemonSocket;
class ListenSocket;

}
}

namespace mozilla {
namespace hal {

class GonkSensorsPollInterface;
class GonkSensorsProtocol;
class GonkSensorsRegistryInterface;

/**
 * This class is the result-handler interface for the Sensors
 * interface. Methods always run on the main thread.
 */
class GonkSensorsResultHandler
  : public mozilla::ipc::DaemonSocketResultHandler
{
public:

  /**
   * Called if a command failed.
   *
   * @param aError The error code.
   */
  virtual void OnError(SensorsError aError);

  /**
   * The callback method for |GonkSensorsInterface::Connect|.
   */
  virtual void Connect();

  /**
   * The callback method for |GonkSensorsInterface::Connect|.
   */
  virtual void Disconnect();

protected:
  virtual ~GonkSensorsResultHandler();
};

/**
 * This is the notification-handler interface. Implement this classes
 * methods to handle event and notifications from the sensors daemon.
 * All methods run on the main thread.
 */
class GonkSensorsNotificationHandler
{
public:

  /**
   * This notification is called when the backend code fails
   * unexpectedly. Save state in the high-level code and restart
   * the driver.
   *
   * @param aCrash True is the sensors driver crashed.
   */
  virtual void BackendErrorNotification(bool aCrashed);

protected:
  virtual ~GonkSensorsNotificationHandler();
};

/**
 * This class implements the public interface to the Sensors functionality
 * and driver. Use |GonkSensorsInterface::GetInstance| to retrieve an instance.
 * All methods run on the main thread.
 */
class GonkSensorsInterface final
  : public mozilla::ipc::DaemonSocketConsumer
  , public mozilla::ipc::ListenSocketConsumer
{
public:
  /**
   * Returns an instance of the Sensors backend. This code can return
   * |nullptr| if no Sensors backend is available.
   *
   * @return An instance of |GonkSensorsInterface|.
   */
  static GonkSensorsInterface* GetInstance();

  /**
   * This method sets the notification handler for sensor notifications. Call
   * this method immediately after retreiving an instance of the class, or you
   * won't be able able to receive notifications. You may not free the handler
   * class while the Sensors backend is connected.
   *
   * @param aNotificationHandler An instance of a notification handler.
   */
  void SetNotificationHandler(
    GonkSensorsNotificationHandler* aNotificationHandler);

  /**
   * This method starts the Sensors backend and establishes ad connection
   * with Gecko. This is a multi-step process and errors are signalled by
   * |GonkSensorsNotificationHandler::BackendErrorNotification|. If you see
   * this notification before the connection has been established, it's
   * certainly best to assume the Sensors backend to be not evailable.
   *
   * @param aRes The result handler.
   */
  void Connect(GonkSensorsNotificationHandler* aNotificationHandler,
               GonkSensorsResultHandler* aRes);

  /**
   * This method disconnects Gecko from the Sensors backend and frees
   * the backend's resources. This will invalidate all interfaces and
   * state. Don't use any sensors functionality without reconnecting
   * first.
   *
   * @param aRes The result handler.
   */
  void Disconnect(GonkSensorsResultHandler* aRes);

  /**
   * Returns the Registry interface for the connected Sensors backend.
   *
   * @return An instance of the Sensors Registry interface.
   */
  GonkSensorsRegistryInterface* GetSensorsRegistryInterface();

  /**
   * Returns the Poll interface for the connected Sensors backend.
   *
   * @return An instance of the Sensors Poll interface.
   */
  GonkSensorsPollInterface* GetSensorsPollInterface();

private:
  enum Channel {
    LISTEN_SOCKET,
    DATA_SOCKET
  };

  GonkSensorsInterface();
  ~GonkSensorsInterface();

  void DispatchError(GonkSensorsResultHandler* aRes, SensorsError aError);
  void DispatchError(GonkSensorsResultHandler* aRes, nsresult aRv);

  // Methods for |DaemonSocketConsumer| and |ListenSocketConsumer|
  //

  void OnConnectSuccess(int aIndex) override;
  void OnConnectError(int aIndex) override;
  void OnDisconnect(int aIndex) override;

  nsCString mListenSocketName;
  RefPtr<mozilla::ipc::ListenSocket> mListenSocket;
  RefPtr<mozilla::ipc::DaemonSocket> mDataSocket;
  UniquePtr<GonkSensorsProtocol> mProtocol;

  nsTArray<RefPtr<GonkSensorsResultHandler> > mResultHandlerQ;

  GonkSensorsNotificationHandler* mNotificationHandler;

  UniquePtr<GonkSensorsRegistryInterface> mRegistryInterface;
  UniquePtr<GonkSensorsPollInterface> mPollInterface;
};

} // namespace hal
} // namespace mozilla

#endif // hal_gonk_GonkSensorsInterface_h
