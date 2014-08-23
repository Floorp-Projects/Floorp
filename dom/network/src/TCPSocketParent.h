/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TCPSocketParent_h
#define mozilla_dom_TCPSocketParent_h

#include "mozilla/net/PTCPSocketParent.h"
#include "nsITCPSocketParent.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsIDOMTCPSocket.h"
#include "js/TypeDecls.h"
#include "mozilla/net/OfflineObserver.h"

#define TCPSOCKETPARENT_CID \
  { 0x4e7246c6, 0xa8b3, 0x426d, { 0x9c, 0x17, 0x76, 0xda, 0xb1, 0xe1, 0xe1, 0x4a } }

namespace mozilla {
namespace dom {

class PBrowserParent;

class TCPSocketParentBase : public nsITCPSocketParent
                          , public mozilla::net::DisconnectableParent
{
public:
  NS_DECL_CYCLE_COLLECTION_CLASS(TCPSocketParentBase)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  void AddIPDLReference();
  void ReleaseIPDLReference();

protected:
  TCPSocketParentBase();
  virtual ~TCPSocketParentBase();

  nsCOMPtr<nsITCPSocketIntermediary> mIntermediary;
  nsCOMPtr<nsIDOMTCPSocket> mSocket;
  nsRefPtr<mozilla::net::OfflineObserver> mObserver;
  bool mIPCOpen;
};

class TCPSocketParent : public mozilla::net::PTCPSocketParent
                      , public TCPSocketParentBase
{
public:
  NS_DECL_NSITCPSOCKETPARENT
  NS_IMETHOD_(MozExternalRefCountType) Release() MOZ_OVERRIDE;

  TCPSocketParent() : mIntermediaryObj(nullptr) {}

  virtual bool RecvOpen(const nsString& aHost, const uint16_t& aPort,
                        const bool& useSSL, const nsString& aBinaryType);

  virtual bool RecvStartTLS() MOZ_OVERRIDE;
  virtual bool RecvSuspend() MOZ_OVERRIDE;
  virtual bool RecvResume() MOZ_OVERRIDE;
  virtual bool RecvClose() MOZ_OVERRIDE;
  virtual bool RecvData(const SendableData& aData,
                        const uint32_t& aTrackingNumber) MOZ_OVERRIDE;
  virtual bool RecvRequestDelete() MOZ_OVERRIDE;

  virtual nsresult OfflineNotification(nsISupports *) MOZ_OVERRIDE;
  virtual uint32_t GetAppId() MOZ_OVERRIDE;
private:
  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

  JSObject* mIntermediaryObj;
};

} // namespace dom
} // namespace mozilla

#endif
