/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_Ril_h
#define mozilla_ipc_Ril_h 1

#include <mozilla/dom/workers/Workers.h>
#include <mozilla/ipc/StreamSocket.h>
#include <mozilla/ipc/StreamSocketConsumer.h>

namespace mozilla {
namespace ipc {

class RilConsumer final : public StreamSocketConsumer
{
public:
  static nsresult Register(
    unsigned int aClientId,
    mozilla::dom::workers::WorkerCrossThreadDispatcher* aDispatcher);
  static void Shutdown();

  void Send(UnixSocketRawData* aRawData);

private:
  RilConsumer(unsigned long aClientId,
              mozilla::dom::workers::WorkerCrossThreadDispatcher* aDispatcher);

  void Close();

  // Methods for |StreamSocketConsumer|
  //

  void ReceiveSocketData(int aIndex,
                         nsAutoPtr<UnixSocketBuffer>& aBuffer) override;
  void OnConnectSuccess(int aIndex) override;
  void OnConnectError(int aIndex) override;
  void OnDisconnect(int aIndex) override;

  nsRefPtr<StreamSocket> mSocket;
  nsRefPtr<mozilla::dom::workers::WorkerCrossThreadDispatcher> mDispatcher;
  nsCString mAddress;
  bool mShutdown;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_Ril_h
