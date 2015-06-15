/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_DaemonSocketConsumer_h
#define mozilla_ipc_DaemonSocketConsumer_h

namespace mozilla {
namespace ipc {

class DaemonSocketPDU;

/**
 * |DaemonSocketIOConsumer| processes incoming PDUs from the
 * HAL daemon. Please note that its method |Handle| runs on a
 * different than the  consumer thread.
 */
class DaemonSocketIOConsumer
{
public:
  virtual ~DaemonSocketIOConsumer();

  virtual void Handle(DaemonSocketPDU& aPDU) = 0;
  virtual void StoreUserData(const DaemonSocketPDU& aPDU) = 0;

protected:
  DaemonSocketIOConsumer();
};

/**
 * |DaemonSocketConsumer| handles socket events.
 */
class DaemonSocketConsumer
{
public:
  /**
   * Callback for socket success. Consumer-thread only.
   *
   * @param aIndex The index that has been given to the stream socket.
   */
  virtual void OnConnectSuccess(int aIndex) = 0;

  /**
   * Callback for socket errors. Consumer-thread only.
   *
   * @param aIndex The index that has been given to the stream socket.
   */
  virtual void OnConnectError(int aIndex) = 0;

  /**
   * Callback for socket disconnect. Consumer-thread only.
   *
   * @param aIndex The index that has been given to the stream socket.
   */
  virtual void OnDisconnect(int aIndex) = 0;

protected:
  DaemonSocketConsumer();
  virtual ~DaemonSocketConsumer();
};

}
}

#endif

