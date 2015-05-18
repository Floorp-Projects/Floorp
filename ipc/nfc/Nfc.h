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

namespace mozilla {
namespace ipc {

class NfcSocketListener
{
public:
  enum SocketType {
    LISTEN_SOCKET,
    STREAM_SOCKET
  };

  virtual void ReceiveSocketData(nsAutoPtr<UnixSocketBuffer>& aData) = 0;

  virtual void OnConnectSuccess(enum SocketType aSocketType) = 0;
  virtual void OnConnectError(enum SocketType aSocketType) = 0;
  virtual void OnDisconnect(enum SocketType aSocketType) = 0;
};

class NfcListenSocket final : public mozilla::ipc::ListenSocket
{
public:
  NfcListenSocket(NfcSocketListener* aListener);

  void OnConnectSuccess() override;
  void OnConnectError() override;
  void OnDisconnect() override;

private:
  NfcSocketListener* mListener;
};

class NfcConsumer final : public mozilla::ipc::StreamSocket
{
public:
  NfcConsumer(NfcSocketListener* aListener);

  void Shutdown();
  bool PostToNfcDaemon(const uint8_t* aData, size_t aSize);

  ConnectionOrientedSocketIO* GetIO() override;

private:
  void ReceiveSocketData(nsAutoPtr<UnixSocketBuffer>& aBuffer) override;

  void OnConnectSuccess() override;
  void OnConnectError() override;
  void OnDisconnect() override;

private:
  NfcSocketListener* mListener;
};

} // namespace ipc
} // namepsace mozilla

#endif // mozilla_ipc_Nfc_h
