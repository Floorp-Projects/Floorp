/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TCPServerSocketChild_h
#define mozilla_dom_TCPServerSocketChild_h

#include "mozilla/net/PTCPServerSocketChild.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"

#define TCPSERVERSOCKETCHILD_CID \
  { 0x41a77ec8, 0xfd86, 0x409e, { 0xae, 0xa9, 0xaf, 0x2c, 0xa4, 0x07, 0xef, 0x8e } }

class nsITCPServerSocketInternal;

namespace mozilla {
namespace dom {

class TCPServerSocket;

class TCPServerSocketChildBase : public nsISupports {
public:
  NS_DECL_CYCLE_COLLECTION_CLASS(TCPServerSocketChildBase)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  void AddIPDLReference();
  void ReleaseIPDLReference();

protected:
  TCPServerSocketChildBase();
  virtual ~TCPServerSocketChildBase();

  RefPtr<TCPServerSocket> mServerSocket;
  bool mIPCOpen;
};

class TCPServerSocketChild : public mozilla::net::PTCPServerSocketChild
                           , public TCPServerSocketChildBase
{
public:
  NS_IMETHOD_(MozExternalRefCountType) Release() override;

  TCPServerSocketChild(TCPServerSocket* aServerSocket, uint16_t aLocalPort,
                       uint16_t aBacklog, bool aUseArrayBuffers);
  ~TCPServerSocketChild();

  void Close();

  virtual bool RecvCallbackAccept(PTCPSocketChild *socket)  override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TCPServerSocketChild_h
