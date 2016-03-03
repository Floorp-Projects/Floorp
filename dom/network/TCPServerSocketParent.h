/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TCPServerSocketParent_h
#define mozilla_dom_TCPServerSocketParent_h

#include "mozilla/net/PNeckoParent.h"
#include "mozilla/net/PTCPServerSocketParent.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {

class TCPServerSocket;
class TCPServerSocketEvent;
class TCPSocketParent;

class TCPServerSocketParent : public mozilla::net::PTCPServerSocketParent
                            , public nsISupports
{
public:
  NS_DECL_CYCLE_COLLECTION_CLASS(TCPServerSocketParent)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  TCPServerSocketParent(PNeckoParent* neckoParent, uint16_t aLocalPort,
                        uint16_t aBacklog, bool aUseArrayBuffers);

  void Init();

  virtual bool RecvClose() override;
  virtual bool RecvRequestDelete() override;

  uint32_t GetAppId();
  bool GetInIsolatedMozBrowser();

  void AddIPDLReference();
  void ReleaseIPDLReference();

  void OnConnect(TCPServerSocketEvent* event);

private:
  ~TCPServerSocketParent();

  nsresult SendCallbackAccept(TCPSocketParent *socket);

  virtual void ActorDestroy(ActorDestroyReason why) override;

  PNeckoParent* mNeckoParent;
  RefPtr<TCPServerSocket> mServerSocket;
  bool mIPCOpen;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TCPServerSocketParent_h
