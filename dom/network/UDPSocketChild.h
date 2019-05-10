/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_UDPSocketChild_h__
#define mozilla_dom_UDPSocketChild_h__

#include "mozilla/net/PUDPSocketChild.h"
#include "nsIUDPSocketChild.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"

#define UDPSOCKETCHILD_CID                           \
  {                                                  \
    0xb47e5a0f, 0xd384, 0x48ef, {                    \
      0x88, 0x85, 0x42, 0x59, 0x79, 0x3d, 0x9c, 0xf0 \
    }                                                \
  }

namespace mozilla {
namespace dom {

class UDPSocketChildBase : public nsISupports {
 public:
  NS_DECL_ISUPPORTS

  void AddIPDLReference();
  void ReleaseIPDLReference();

 protected:
  UDPSocketChildBase();
  virtual ~UDPSocketChildBase();
  nsCOMPtr<nsIUDPSocketInternal> mSocket;
  bool mIPCOpen;
};

class UDPSocketChild : public mozilla::net::PUDPSocketChild,
                       public UDPSocketChildBase {
 public:
  NS_IMETHOD_(MozExternalRefCountType) Release() override;

  UDPSocketChild();
  virtual ~UDPSocketChild();

  uint16_t LocalPort() const { return mLocalPort; }
  // Local address as UTF-8.
  const nsACString& LocalAddress() const { return mLocalAddress; }

  nsresult SetFilterName(const nsACString& aFilterName);

  // Allow hosting this over PBackground instead of PNecko
  nsresult SetBackgroundSpinsEvents();

  // Tell the chrome process to bind the UDP socket to a given local host and
  // port
  nsresult Bind(nsIUDPSocketInternal* aSocket, nsIPrincipal* aPrincipal,
                const nsACString& aHost, uint16_t aPort, bool aAddressReuse,
                bool aLoopback, uint32_t recvBufferSize,
                uint32_t sendBufferSize,
                nsIEventTarget* aMainThreadEventTarget);

  // Tell the chrome process to connect the UDP socket to a given remote host
  // and port
  void Connect(nsIUDPSocketInternal* aSocket, const nsACString& aHost,
               uint16_t aPort);

  // Send the given data to the given address.
  nsresult SendWithAddress(const NetAddr* aAddr, const uint8_t* aData,
                           uint32_t aByteLength);

  // Send input stream. This must be a buffered stream implementation.
  nsresult SendBinaryStream(const nsACString& aHost, uint16_t aPort,
                            nsIInputStream* aStream);

  void Close();

  // Address and interface are both UTF-8.
  void JoinMulticast(const nsACString& aMulticastAddress,
                     const nsACString& aInterface);
  void LeaveMulticast(const nsACString& aMulticastAddress,
                      const nsACString& aInterface);

  mozilla::ipc::IPCResult RecvCallbackOpened(
      const UDPAddressInfo& aAddressInfo);
  mozilla::ipc::IPCResult RecvCallbackConnected(
      const UDPAddressInfo& aAddressInfo);
  mozilla::ipc::IPCResult RecvCallbackClosed();
  mozilla::ipc::IPCResult RecvCallbackReceivedData(
      const UDPAddressInfo& aAddressInfo, InfallibleTArray<uint8_t>&& aData);
  mozilla::ipc::IPCResult RecvCallbackError(const nsCString& aMessage,
                                            const nsCString& aFilename,
                                            const uint32_t& aLineNumber);

 private:
  nsresult SendDataInternal(const UDPSocketAddr& aAddr, const uint8_t* aData,
                            const uint32_t aByteLength);

  mozilla::ipc::PBackgroundChild* mBackgroundManager;
  uint16_t mLocalPort;
  nsCString mLocalAddress;
  nsCString mFilterName;
};

}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_UDPSocketChild_h__)
