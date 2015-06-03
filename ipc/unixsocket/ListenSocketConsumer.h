/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_listensocketconsumer_h
#define mozilla_ipc_listensocketconsumer_h

namespace mozilla {
namespace ipc {

/**
 * |ListenSocketConsumer| handles socket events.
 */
class ListenSocketConsumer
{
public:
  virtual ~ListenSocketConsumer();

  /**
   * Callback for socket success. Consumer-thread only.
   *
   * @param aIndex The index that has been given to the listening socket.
   */
  virtual void OnConnectSuccess(int aIndex) = 0;

  /**
   * Callback for socket errors. Consumer-thread only.
   *
   * @param aIndex The index that has been given to the listening socket.
   */
  virtual void OnConnectError(int aIndex) = 0;

  /**
   * Callback for socket disconnect. Consumer-thread only.
   *
   * @param aIndex The index that has been given to the listeing socket.
   */
  virtual void OnDisconnect(int aIndex) = 0;
};

}
}

#endif
