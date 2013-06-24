/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TCPServerSocketParent_h
#define mozilla_dom_TCPServerSocketParent_h

#include "mozilla/net/PNeckoParent.h"
#include "mozilla/net/PTCPServerSocketParent.h"
#include "nsITCPSocketParent.h"
#include "nsITCPServerSocketParent.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"

class nsITCPServerSocketInternal;

namespace mozilla {
namespace dom {

class PBrowserParent;

class TCPServerSocketParent : public mozilla::net::PTCPServerSocketParent
                            , public nsITCPServerSocketParent
{
public:
  NS_DECL_CYCLE_COLLECTION_CLASS(TCPServerSocketParent)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSITCPSERVERSOCKETPARENT

  TCPServerSocketParent();

  bool Init(PNeckoParent* neckoParent, const uint16_t& aLocalPort, const uint16_t& aBacklog,
            const nsString& aBinaryType);

  virtual bool RecvClose() MOZ_OVERRIDE;
  virtual bool RecvRequestDelete() MOZ_OVERRIDE;

  uint32_t GetAppId();
  bool GetInBrowser();

  void AddIPDLReference();
  void ReleaseIPDLReference();

private:
  ~TCPServerSocketParent();

  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

  PNeckoParent* mNeckoParent;
  nsCOMPtr<nsITCPSocketIntermediary> mIntermediary;
  nsCOMPtr<nsITCPServerSocketInternal> mServerSocket;
  bool mIPCOpen;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TCPServerSocketParent_h
