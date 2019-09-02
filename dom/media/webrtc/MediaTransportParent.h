/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MTRANSPORTHANDLER_PARENT_H__
#define _MTRANSPORTHANDLER_PARENT_H__

#include "mozilla/dom/PMediaTransportParent.h"
#include <memory>

namespace mozilla {

class MediaTransportParent : public dom::PMediaTransportParent {
 public:
#ifdef MOZ_WEBRTC
  MediaTransportParent();
  virtual ~MediaTransportParent();

  mozilla::ipc::IPCResult RecvGetIceLog(const nsCString& pattern,
                                        GetIceLogResolver&& aResolve);
  mozilla::ipc::IPCResult RecvClearIceLog();
  mozilla::ipc::IPCResult RecvEnterPrivateMode();
  mozilla::ipc::IPCResult RecvExitPrivateMode();
  mozilla::ipc::IPCResult RecvCreateIceCtx(
      const string& name, nsTArray<RTCIceServer>&& iceServers,
      const RTCIceTransportPolicy& icePolicy);
  mozilla::ipc::IPCResult RecvSetProxyServer(const dom::TabId& tabId,
                                             const net::LoadInfoArgs& args,
                                             const nsCString& alpn);
  mozilla::ipc::IPCResult RecvEnsureProvisionalTransport(
      const string& transportId, const string& localUfrag,
      const string& localPwd, const int& componentCount);
  mozilla::ipc::IPCResult RecvSetTargetForDefaultLocalAddressLookup(
      const string& targetIp, uint16_t targetPort);
  mozilla::ipc::IPCResult RecvStartIceGathering(
      const bool& defaultRouteOnly, const bool& obfuscateAddresses,
      const net::NrIceStunAddrArray& stunAddrs);
  mozilla::ipc::IPCResult RecvActivateTransport(
      const string& transportId, const string& localUfrag,
      const string& localPwd, const int& componentCount,
      const string& remoteUfrag, const string& remotePwd,
      nsTArray<uint8_t>&& keyDer, nsTArray<uint8_t>&& certDer,
      const int& authType, const bool& dtlsClient,
      const DtlsDigestList& digests, const bool& privacyRequested);
  mozilla::ipc::IPCResult RecvRemoveTransportsExcept(
      const StringVector& transportIds);
  mozilla::ipc::IPCResult RecvStartIceChecks(const bool& isControlling,
                                             const StringVector& iceOptions);
  mozilla::ipc::IPCResult RecvSendPacket(const string& transportId,
                                         const MediaPacket& packet);
  mozilla::ipc::IPCResult RecvAddIceCandidate(const string& transportId,
                                              const string& candidate,
                                              const string& ufrag);
  mozilla::ipc::IPCResult RecvUpdateNetworkState(const bool& online);
  mozilla::ipc::IPCResult RecvGetIceStats(
      const string& transportId, const double& now,
      const RTCStatsReportInternal& reportIn, GetIceStatsResolver&& aResolve);

  void ActorDestroy(ActorDestroyReason aWhy);

 private:
  // Hide the sigslot/MediaTransportHandler stuff from IPC.
  class Impl;
  std::unique_ptr<Impl> mImpl;
#endif  // MOZ_WEBRTC
};
}  // namespace mozilla
#endif  //_MTRANSPORTHANDLER_PARENT_H__
