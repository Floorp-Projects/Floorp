/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MTRANSPORTHANDLER_IPC_H__
#define _MTRANSPORTHANDLER_IPC_H__

#include "jsapi/MediaTransportHandler.h"
#include "mozilla/dom/PMediaTransportChild.h"

namespace mozilla {

class MediaTransportChild;

// Implementation of MediaTransportHandler that uses IPC (PMediaTransport) to
// talk to mtransport on another process.
class MediaTransportHandlerIPC final : public MediaTransportHandler {
 public:
  explicit MediaTransportHandlerIPC(nsISerialEventTarget* aCallbackThread);
  void Initialize() override;
  RefPtr<IceLogPromise> GetIceLog(const nsCString& aPattern) override;
  void ClearIceLog() override;
  void EnterPrivateMode() override;
  void ExitPrivateMode() override;

  void CreateIceCtx(const std::string& aName) override;

  nsresult SetIceConfig(const nsTArray<dom::RTCIceServer>& aIceServers,
                        dom::RTCIceTransportPolicy aIcePolicy) override;

  // We will probably be able to move the proxy lookup stuff into
  // this class once we move mtransport to its own process.
  void SetProxyConfig(NrSocketProxyConfig&& aProxyConfig) override;

  void EnsureProvisionalTransport(const std::string& aTransportId,
                                  const std::string& aLocalUfrag,
                                  const std::string& aLocalPwd,
                                  int aComponentCount) override;

  void SetTargetForDefaultLocalAddressLookup(const std::string& aTargetIp,
                                             uint16_t aTargetPort) override;

  // We set default-route-only as late as possible because it depends on what
  // capture permissions have been granted on the window, which could easily
  // change between Init (ie; when the PC is created) and StartIceGathering
  // (ie; when we set the local description).
  void StartIceGathering(bool aDefaultRouteOnly, bool aObfuscateHostAddresses,
                         // TODO: It probably makes sense to look
                         // this up internally
                         const nsTArray<NrIceStunAddr>& aStunAddrs) override;

  void ActivateTransport(
      const std::string& aTransportId, const std::string& aLocalUfrag,
      const std::string& aLocalPwd, size_t aComponentCount,
      const std::string& aUfrag, const std::string& aPassword,
      const nsTArray<uint8_t>& aKeyDer, const nsTArray<uint8_t>& aCertDer,
      SSLKEAType aAuthType, bool aDtlsClient, const DtlsDigestList& aDigests,
      bool aPrivacyRequested) override;

  void RemoveTransportsExcept(
      const std::set<std::string>& aTransportIds) override;

  void StartIceChecks(bool aIsControlling,
                      const std::vector<std::string>& aIceOptions) override;

  void SendPacket(const std::string& aTransportId,
                  MediaPacket&& aPacket) override;

  void AddIceCandidate(const std::string& aTransportId,
                       const std::string& aCandidate, const std::string& aUfrag,
                       const std::string& aObfuscatedAddress) override;

  void UpdateNetworkState(bool aOnline) override;

  RefPtr<dom::RTCStatsPromise> GetIceStats(const std::string& aTransportId,
                                           DOMHighResTimeStamp aNow) override;

 private:
  friend class MediaTransportChild;
  void Destroy() override;
  virtual ~MediaTransportHandlerIPC();

  RefPtr<MediaTransportChild> mChild;

  // |mChild| can only be initted asynchronously, |mInitPromise| resolves
  // when that happens. The |Then| calls make it convenient to dispatch API
  // calls to main, which is a bonus.
  // Init promise is not exclusive; this lets us call |Then| on it for every
  // API call we get, instead of creating another promise each time.
  typedef MozPromise<bool, nsCString, false> InitPromise;
  RefPtr<InitPromise> mInitPromise;
};

}  // namespace mozilla

#endif  //_MTRANSPORTHANDLER_IPC_H__
