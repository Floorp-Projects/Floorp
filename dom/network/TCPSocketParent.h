/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TCPSocketParent_h
#define mozilla_dom_TCPSocketParent_h

#include "mozilla/dom/TCPSocketBinding.h"
#include "mozilla/net/PTCPSocketParent.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsISocketFilter.h"
#include "js/TypeDecls.h"

#define TCPSOCKETPARENT_CID \
  { 0x4e7246c6, 0xa8b3, 0x426d, { 0x9c, 0x17, 0x76, 0xda, 0xb1, 0xe1, 0xe1, 0x4a } }

namespace mozilla {
namespace dom {

class TCPSocket;

class TCPSocketParentBase : public nsISupports
{
public:
  NS_DECL_CYCLE_COLLECTION_CLASS(TCPSocketParentBase)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  void AddIPDLReference();
  void ReleaseIPDLReference();

protected:
  TCPSocketParentBase();
  virtual ~TCPSocketParentBase();

  RefPtr<TCPSocket> mSocket;
  bool mIPCOpen;
};

class TCPSocketParent : public mozilla::net::PTCPSocketParent
                      , public TCPSocketParentBase
{
public:
  NS_IMETHOD_(MozExternalRefCountType) Release() override;

  TCPSocketParent() {}

  virtual mozilla::ipc::IPCResult RecvOpen(const nsString& aHost, const uint16_t& aPort,
                                           const bool& useSSL, const bool& aUseArrayBuffers) override;

  virtual mozilla::ipc::IPCResult RecvOpenBind(const nsCString& aRemoteHost,
                                               const uint16_t& aRemotePort,
                                               const nsCString& aLocalAddr,
                                               const uint16_t& aLocalPort,
                                               const bool&     aUseSSL,
                                               const bool&     aReuseAddrPort,
                                               const bool& aUseArrayBuffers,
                                               const nsCString& aFilter) override;

  virtual mozilla::ipc::IPCResult RecvStartTLS() override;
  virtual mozilla::ipc::IPCResult RecvSuspend() override;
  virtual mozilla::ipc::IPCResult RecvResume() override;
  virtual mozilla::ipc::IPCResult RecvClose() override;
  virtual mozilla::ipc::IPCResult RecvData(const SendableData& aData,
                                           const uint32_t& aTrackingNumber) override;
  virtual mozilla::ipc::IPCResult RecvRequestDelete() override;

  void FireErrorEvent(const nsAString& aName, const nsAString& aType, TCPReadyState aReadyState);
  void FireEvent(const nsAString& aType, TCPReadyState aReadyState);
  void FireArrayBufferDataEvent(nsTArray<uint8_t>& aBuffer, TCPReadyState aReadyState);
  void FireStringDataEvent(const nsACString& aData, TCPReadyState aReadyState);

  void SetSocket(TCPSocket *socket);
  nsresult GetHost(nsAString& aHost);
  nsresult GetPort(uint16_t* aPort);

private:
  virtual void ActorDestroy(ActorDestroyReason why) override;
  void SendEvent(const nsAString& aType, CallbackData aData, TCPReadyState aReadyState);
  nsresult SetFilter(const nsCString& aFilter);

  nsCOMPtr<nsISocketFilter> mFilter;
};

} // namespace dom
} // namespace mozilla

#endif
