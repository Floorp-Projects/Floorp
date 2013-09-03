/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/PTCPServerSocketChild.h"
#include "nsITCPServerSocketChild.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"

#define TCPSERVERSOCKETCHILD_CID \
  { 0x41a77ec8, 0xfd86, 0x409e, { 0xae, 0xa9, 0xaf, 0x2c, 0xa4, 0x07, 0xef, 0x8e } }

class nsITCPServerSocketInternal;

namespace mozilla {
namespace dom {

class TCPServerSocketChildBase : public nsITCPServerSocketChild {
public:
  NS_DECL_CYCLE_COLLECTION_CLASS(TCPServerSocketChildBase)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  void AddIPDLReference();
  void ReleaseIPDLReference();

protected:
  TCPServerSocketChildBase();
  virtual ~TCPServerSocketChildBase();

  nsCOMPtr<nsITCPServerSocketInternal> mServerSocket;
  bool mIPCOpen;
};

class TCPServerSocketChild : public mozilla::net::PTCPServerSocketChild
                           , public TCPServerSocketChildBase
{
public:
  NS_DECL_NSITCPSERVERSOCKETCHILD
  NS_IMETHOD_(nsrefcnt) Release() MOZ_OVERRIDE;

  TCPServerSocketChild();
  ~TCPServerSocketChild();

  virtual bool RecvCallbackAccept(PTCPSocketChild *socket)  MOZ_OVERRIDE;
  virtual bool RecvCallbackError(const nsString& aMessage,
                                 const nsString& aFilename,
                                 const uint32_t& aLineNumber,
                                 const uint32_t& aColumnNumber) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla
