/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright © 2013, Deutsche Telekom, Inc. */

#ifndef mozilla_ipc_Nfc_h
#define mozilla_ipc_Nfc_h 1

#include <mozilla/ipc/ListenSocket.h>
#include <mozilla/ipc/StreamSocket.h>
#include <mozilla/ipc/UnixSocketConnector.h>

namespace mozilla {
namespace ipc {

class NfcSocketListener
{
public:
  enum SocketType {
    LISTEN_SOCKET,
    STREAM_SOCKET
  };

  virtual void ReceiveSocketData(nsAutoPtr<UnixSocketRawData>& aData) = 0;

  virtual void OnConnectSuccess(enum SocketType aSocketType) = 0;
  virtual void OnConnectError(enum SocketType aSocketType) = 0;
  virtual void OnDisconnect(enum SocketType aSocketType) = 0;
};

class NfcListenSocket MOZ_FINAL : public mozilla::ipc::ListenSocket
{
public:
  NfcListenSocket(NfcSocketListener* aListener);

  void OnConnectSuccess() MOZ_OVERRIDE;
  void OnConnectError() MOZ_OVERRIDE;
  void OnDisconnect() MOZ_OVERRIDE;

private:
  NfcSocketListener* mListener;
};

class NfcConnector MOZ_FINAL : public mozilla::ipc::UnixSocketConnector
{
public:
  NfcConnector()
  { }

  int Create() MOZ_OVERRIDE;
  bool CreateAddr(bool aIsServer,
                  socklen_t& aAddrSize,
                  sockaddr_any& aAddr,
                  const char* aAddress) MOZ_OVERRIDE;
  bool SetUp(int aFd) MOZ_OVERRIDE;
  bool SetUpListenSocket(int aFd) MOZ_OVERRIDE;
  void GetSocketAddr(const sockaddr_any& aAddr,
                     nsAString& aAddrStr) MOZ_OVERRIDE;
};

class NfcConsumer MOZ_FINAL : public mozilla::ipc::StreamSocket
{
public:
  NfcConsumer(NfcSocketListener* aListener);

  void Shutdown();
  bool PostToNfcDaemon(const uint8_t* aData, size_t aSize);

  ConnectionOrientedSocketIO* GetIO() MOZ_OVERRIDE;

private:
  void ReceiveSocketData(
    nsAutoPtr<UnixSocketRawData>& aData) MOZ_OVERRIDE;

  void OnConnectSuccess() MOZ_OVERRIDE;
  void OnConnectError() MOZ_OVERRIDE;
  void OnDisconnect() MOZ_OVERRIDE;

private:
  NfcSocketListener* mListener;
};

} // namespace ipc
} // namepsace mozilla

#endif // mozilla_ipc_Nfc_h
