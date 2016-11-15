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
bool
DeserializeArrayBuffer(JSContext* cx,
                       const InfallibleTArray<uint8_t>& aBuffer,
                       JS::MutableHandle<JS::Value> aVal);
}

namespace mozilla {
namespace dom {

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

class TCPSocketChild : public mozilla::net::PTCPSocketChild
                     , public TCPSocketChildBase
{
public:
  NS_IMETHOD_(MozExternalRefCountType) Release() override;

  TCPSocketChild(const nsAString& aHost, const uint16_t& aPort);
  ~TCPSocketChild();

  void SendOpen(nsITCPSocketCallback* aSocket, bool aUseSSL, bool aUseArrayBuffers);
  void SendWindowlessOpenBind(nsITCPSocketCallback* aSocket,
                              const nsACString& aRemoteHost, uint16_t aRemotePort,
                              const nsACString& aLocalHost, uint16_t aLocalPort,
                              bool aUseSSL);
  NS_IMETHOD SendSendArray(nsTArray<uint8_t>& aArray,
                           uint32_t aTrackingNumber);
  void SendSend(const nsACString& aData, uint32_t aTrackingNumber);
  nsresult SendSend(const ArrayBuffer& aData,
                    uint32_t aByteOffset,
                    uint32_t aByteLength,
                    uint32_t aTrackingNumber);
  void SendSendArray(nsTArray<uint8_t>* arr,
                     uint32_t trackingNumber);
  void SetSocket(TCPSocket* aSocket);

  void GetHost(nsAString& aHost);
  void GetPort(uint16_t* aPort);

  virtual mozilla::ipc::IPCResult RecvCallback(const nsString& aType,
                                               const CallbackData& aData,
                                               const uint32_t& aReadyState) override;
  virtual mozilla::ipc::IPCResult RecvRequestDelete() override;
  virtual mozilla::ipc::IPCResult RecvUpdateBufferedAmount(const uint32_t& aBufferred,
                                                           const uint32_t& aTrackingNumber) override;
  nsresult SetFilterName(const nsACString& aFilterName);
private:
  nsString mHost;
  uint16_t mPort;
  nsCString mFilterName;
};

} // namespace dom
} // namespace mozilla

#endif
