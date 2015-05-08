/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_UnixSocketWatcher_h
#define mozilla_ipc_UnixSocketWatcher_h

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#ifdef MOZ_B2G_BT_BLUEZ
#include <bluetooth/bluetooth.h>
#include <bluetooth/sco.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/rfcomm.h>
#endif
#include "UnixFdWatcher.h"

namespace mozilla {
namespace ipc {

union sockaddr_any {
  sockaddr_storage storage; // address-family only
  sockaddr_un un;
  sockaddr_in in;
  sockaddr_in6 in6;
#ifdef MOZ_B2G_BT_BLUEZ
  sockaddr_sco sco;
  sockaddr_rc rc;
  sockaddr_l2 l2;
#endif
  // ... others
};

class UnixSocketWatcher : public UnixFdWatcher
{
public:
  enum ConnectionStatus {
    SOCKET_IS_DISCONNECTED = 0,
    SOCKET_IS_LISTENING,
    SOCKET_IS_CONNECTING,
    SOCKET_IS_CONNECTED
  };

  virtual ~UnixSocketWatcher();

  virtual void Close() override;

  ConnectionStatus GetConnectionStatus() const
  {
    return mConnectionStatus;
  }

  // Connect to a peer
  nsresult Connect(const struct sockaddr* aAddr, socklen_t aAddrLen);

  // Listen on socket for incoming connection requests
  nsresult Listen(const struct sockaddr* aAddr, socklen_t aAddrLen);

  // Callback method for successful connection requests
  virtual void OnConnected() {};

  // Callback method for successful listen requests
  virtual void OnListening() {};

  // Callback method for accepting from a listening socket
  virtual void OnSocketCanAcceptWithoutBlocking() {};

  // Callback method for receiving from socket
  virtual void OnSocketCanReceiveWithoutBlocking() {};

  // Callback method for sending on socket
  virtual void OnSocketCanSendWithoutBlocking() {};

protected:
  UnixSocketWatcher(MessageLoop* aIOLoop);
  UnixSocketWatcher(MessageLoop* aIOLoop, int aFd,
                    ConnectionStatus aConnectionStatus);
  void SetSocket(int aFd, ConnectionStatus aConnectionStatus);

private:
  void OnFileCanReadWithoutBlocking(int aFd) override;
  void OnFileCanWriteWithoutBlocking(int aFd) override;

  ConnectionStatus mConnectionStatus;
};

}
}

#endif
