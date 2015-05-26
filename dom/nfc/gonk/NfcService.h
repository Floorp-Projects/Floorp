/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NfcService_h
#define NfcService_h

#include <mozilla/ipc/ListenSocket.h>
#include "mozilla/ipc/ListenSocketConsumer.h"
#include <mozilla/ipc/StreamSocket.h>
#include "mozilla/ipc/StreamSocketConsumer.h"
#include "nsCOMPtr.h"
#include "nsINfcService.h"
#include "NfcMessageHandler.h"

class nsIThread;

namespace mozilla {
namespace dom {
class NfcEventOptions;
} // namespace dom

class NfcService final
  : public nsINfcService
  , public mozilla::ipc::StreamSocketConsumer
  , public mozilla::ipc::ListenSocketConsumer
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINFCSERVICE

  static already_AddRefed<NfcService> FactoryCreate();

  void DispatchNfcEvent(const mozilla::dom::NfcEventOptions& aOptions);

  bool PostToNfcDaemon(const uint8_t* aData, size_t aSize);

  nsCOMPtr<nsIThread> GetThread() {
    return mThread;
  }

  // Methods for |StreamSocketConsumer| and |ListenSocketConsumer|
  //

  void ReceiveSocketData(
    int aIndex, nsAutoPtr<mozilla::ipc::UnixSocketBuffer>& aBuffer) override;
  void OnConnectSuccess(int aIndex) override;
  void OnConnectError(int aIndex) override;
  void OnDisconnect(int aIndex) override;

private:
  enum SocketType {
    LISTEN_SOCKET,
    STREAM_SOCKET
  };

  NfcService();
  ~NfcService();

  nsCOMPtr<nsIThread> mThread;
  nsCOMPtr<nsINfcGonkEventListener> mListener;
  nsRefPtr<mozilla::ipc::ListenSocket> mListenSocket;
  nsRefPtr<mozilla::ipc::StreamSocket> mStreamSocket;
  nsAutoPtr<NfcMessageHandler> mHandler;
  nsCString mListenSocketName;
};

} // namespace mozilla

#endif // NfcService_h
