/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_UDPSocketParent_h__
#define mozilla_dom_UDPSocketParent_h__

#include "mozilla/net/PUDPSocketParent.h"
#include "nsCOMPtr.h"
#include "nsIUDPSocket.h"
#include "nsIUDPSocketFilter.h"

namespace mozilla {
namespace dom {

class UDPSocketParent : public mozilla::net::PUDPSocketParent
                      , public nsIUDPSocketListener
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUDPSOCKETLISTENER

  UDPSocketParent(nsIUDPSocketFilter* filter) :
    mIPCOpen(true),
    mFilter(filter) {}

  bool Init(const nsCString& aHost, const uint16_t aPort);

  virtual bool RecvClose() MOZ_OVERRIDE;
  virtual bool RecvData(const InfallibleTArray<uint8_t>& aData,
                        const nsCString& aRemoteAddress,
                        const uint16_t& aPort) MOZ_OVERRIDE;
  virtual bool RecvDataWithAddress( const InfallibleTArray<uint8_t>& data,
                                    const mozilla::net::NetAddr& addr);
  virtual bool RecvRequestDelete() MOZ_OVERRIDE;

private:
  virtual ~UDPSocketParent();

  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

  bool mIPCOpen;
  nsCOMPtr<nsIUDPSocket> mSocket;
  nsCOMPtr<nsIUDPSocketFilter> mFilter;
};

} // namespace dom
} // namespace mozilla

#endif // !defined(mozilla_dom_UDPSocketParent_h__)
