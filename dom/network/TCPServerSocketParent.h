/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/PNeckoParent.h"
#include "mozilla/net/PTCPServerSocketParent.h"
#include "nsITCPSocketParent.h"
#include "nsITCPServerSocketParent.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsIDOMTCPSocket.h"

namespace mozilla {
namespace dom {

class TCPServerSocketParent : public mozilla::net::PTCPServerSocketParent
                            , public nsITCPServerSocketParent
{
public:
  NS_DECL_CYCLE_COLLECTION_CLASS(TCPServerSocketParent)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSITCPSERVERSOCKETPARENT

  TCPServerSocketParent() : mNeckoParent(nullptr), mIPCOpen(false) {}

  bool Init(PNeckoParent* neckoParent, const uint16_t& aLocalPort, const uint16_t& aBacklog,
            const nsString& aBinaryType);

  virtual bool RecvClose() override;
  virtual bool RecvRequestDelete() override;

  uint32_t GetAppId();
  bool GetInBrowser();

  void AddIPDLReference();
  void ReleaseIPDLReference();

private:
  ~TCPServerSocketParent() {}

  virtual void ActorDestroy(ActorDestroyReason why) override;

  PNeckoParent* mNeckoParent;
  nsCOMPtr<nsITCPSocketIntermediary> mIntermediary;
  nsCOMPtr<nsIDOMTCPServerSocket> mServerSocket;
  bool mIPCOpen;
};

} // namespace dom
} // namespace mozilla
