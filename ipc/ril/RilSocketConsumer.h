/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_RilSocketConsumer_h
#define mozilla_ipc_RilSocketConsumer_h

#include "nsAutoPtr.h"

class JSContext;

namespace mozilla {
namespace ipc {

class UnixSocketBuffer;

/**
 * |RilSocketConsumer| handles socket events and received data.
 */
class RilSocketConsumer
{
public:
  /**
   * Method to be called whenever data is received. RIL-worker only.
   *
   * @param aCx The RIL worker's JS context.
   * @param aIndex The index that has been given to the stream socket.
   * @param aBuffer Data received from the socket.
   */
  virtual void ReceiveSocketData(JSContext* aCx,
                                 int aIndex,
                                 nsAutoPtr<UnixSocketBuffer>& aBuffer) = 0;

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
  virtual ~RilSocketConsumer();
};

}
}

#endif
