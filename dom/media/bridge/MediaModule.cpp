/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsIClassInfoImpl.h"

#ifdef MOZ_WEBRTC

#include "PeerConnectionImpl.h"

#define PEERCONNECTION_CID \
{0xb93af7a1, 0x3411, 0x44a8, {0xbd, 0x0a, 0x8a, 0xf3, 0xdd, 0xe4, 0xd8, 0xd8}}

#define PEERCONNECTION_CONTRACTID "@mozilla.org/peerconnection;1"

#include "stun_socket_filter.h"

NS_DEFINE_NAMED_CID(NS_STUN_UDP_SOCKET_FILTER_HANDLER_CID)
NS_DEFINE_NAMED_CID(NS_STUN_TCP_SOCKET_FILTER_HANDLER_CID)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsStunUDPSocketFilterHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsStunTCPSocketFilterHandler)


namespace mozilla
{
// Factory defined in mozilla::, defines mozilla::PeerConnectionImplConstructor
NS_GENERIC_FACTORY_CONSTRUCTOR(PeerConnectionImpl)
}

// Defines kPEERCONNECTION_CID
NS_DEFINE_NAMED_CID(PEERCONNECTION_CID);

static const mozilla::Module::CIDEntry kCIDs[] = {
  { &kPEERCONNECTION_CID, false, nullptr, mozilla::PeerConnectionImplConstructor },
  { &kNS_STUN_UDP_SOCKET_FILTER_HANDLER_CID, false, nullptr, nsStunUDPSocketFilterHandlerConstructor },
  { &kNS_STUN_TCP_SOCKET_FILTER_HANDLER_CID, false, nullptr, nsStunTCPSocketFilterHandlerConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kContracts[] = {
  { PEERCONNECTION_CONTRACTID, &kPEERCONNECTION_CID },
  { NS_STUN_UDP_SOCKET_FILTER_HANDLER_CONTRACTID, &kNS_STUN_UDP_SOCKET_FILTER_HANDLER_CID },
  { NS_STUN_TCP_SOCKET_FILTER_HANDLER_CONTRACTID, &kNS_STUN_TCP_SOCKET_FILTER_HANDLER_CID },
  { nullptr }
};

static const mozilla::Module kModule = {
  mozilla::Module::kVersion,
  kCIDs,
  kContracts
};

NSMODULE_DEFN(peerconnection) = &kModule;

#endif
