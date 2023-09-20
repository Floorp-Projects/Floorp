/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TCPSocketChild_h
#define mozilla_dom_TCPSocketChild_h

#include "mozilla/dom/TypedArray.h"
#include "mozilla/net/PTCPSocketChild.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "js/TypeDecls.h"

class nsITCPSocketCallback;

namespace IPC {
bool DeserializeArrayBuffer(JSContext* cx, const nsTArray<uint8_t>& aBuffer,
                            JS::MutableHandle<JS::Value> aVal);
}

namespace mozilla::dom {

class TCPSocket;

class TCPSocketChildBase : public nsISupports {
 public:
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TCPSocketChildBase)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  void AddIPDLReference();
  void ReleaseIPDLReference();

 protected:
  TCPSocketChildBase();
  virtual ~TCPSocketChildBase();

  nsCOMPtr<nsITCPSocketCallback> mSocket;
  bool mIPCOpen;
};

class TCPSocketChild : public mozilla::net::PTCPSocketChild,
                       public TCPSocketChildBase {
 public:
  NS_IMETHOD_(MozExternalRefCountType) Release() override;

  TCPSocketChild(const nsAString& aHost, const uint16_t& aPort,
                 nsISerialEventTarget* aTarget);
  ~TCPSocketChild();

  void SendOpen(nsITCPSocketCallback* aSocket, bool aUseSSL,
                bool aUseArrayBuffers);
  void SendSend(const nsACString& aData);
  void SendSend(nsTArray<uint8_t>&& aData);
  void SetSocket(TCPSocket* aSocket);

  void GetHost(nsAString& aHost);
  void GetPort(uint16_t* aPort) const;

  mozilla::ipc::IPCResult RecvCallback(const nsString& aType,
                                       const CallbackData& aData,
                                       const uint32_t& aReadyState);
  mozilla::ipc::IPCResult RecvRequestDelete();
  mozilla::ipc::IPCResult RecvUpdateBufferedAmount(
      const uint32_t& aBufferred, const uint32_t& aTrackingNumber);

 private:
  nsString mHost;
  uint16_t mPort;
  nsCOMPtr<nsISerialEventTarget> mIPCEventTarget;
};

}  // namespace mozilla::dom

#endif
