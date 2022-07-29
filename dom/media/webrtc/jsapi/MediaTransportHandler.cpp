/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaTransportHandler.h"
#include "MediaTransportHandlerIPC.h"
#include "transport/nricemediastream.h"
#include "transport/nriceresolver.h"
#include "transport/transportflow.h"
#include "transport/transportlayerice.h"
#include "transport/transportlayerdtls.h"
#include "transport/transportlayersrtp.h"

// Config stuff
#include "mozilla/dom/RTCConfigurationBinding.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_network.h"

// Parsing STUN/TURN URIs
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsURLHelper.h"
#include "nsIURLParser.h"

// Logging stuff
#include "common/browser_logging/CSFLog.h"

// For fetching ICE logging
#include "transport/rlogconnector.h"

// DTLS
#include "sdp/SdpAttribute.h"

#include "transport/runnable_utils.h"

#include "mozilla/Algorithm.h"
#include "mozilla/Telemetry.h"

#include "mozilla/dom/RTCStatsReportBinding.h"

#include "nss.h"                // For NSS_NoDB_Init
#include "mozilla/PublicSSL.h"  // For psm::InitializeCipherSuite

#include "nsISocketTransportService.h"
#include "nsDNSService2.h"

#include <string>
#include <vector>
#include <map>

#ifdef MOZ_GECKO_PROFILER
#  include "mozilla/ProfilerMarkers.h"

#  define MEDIA_TRANSPORT_HANDLER_PACKET_RECEIVED(aPacket)              \
    PROFILER_MARKER_TEXT("WebRTC Packet Received", MEDIA_RT, {},        \
                         ProfilerString8View::WrapNullTerminatedString( \
                             PacketTypeToString((aPacket).type())));
#else
#  define MEDIA_TRANSPORT_HANDLER_PACKET_RECEIVED(aPacket)
#endif

namespace mozilla {

static const char* mthLogTag = "MediaTransportHandler";
#ifdef LOGTAG
#  undef LOGTAG
#endif
#define LOGTAG mthLogTag

class MediaTransportHandlerSTS : public MediaTransportHandler,
                                 public sigslot::has_slots<> {
 public:
  explicit MediaTransportHandlerSTS(nsISerialEventTarget* aCallbackThread);

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
                                  const std::string& aUfrag,
                                  const std::string& aPwd,
                                  int aComponentCount) override;

  void SetTargetForDefaultLocalAddressLookup(const std::string& aTargetIp,
                                             uint16_t aTargetPort) override;

  // We set default-route-only as late as possible because it depends on what
  // capture permissions have been granted on the window, which could easily
  // change between Init (ie; when the PC is created) and StartIceGathering
  // (ie; when we set the local description).
  void StartIceGathering(bool aDefaultRouteOnly, bool aObfuscateHostAddresses,
                         // This will go away once mtransport moves to its
                         // own process, because we won't need to get this
                         // via IPC anymore
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

  void AddIceCandidate(const std::string& aTransportId,
                       const std::string& aCandidate, const std::string& aUfrag,
                       const std::string& aObfuscatedAddress) override;

  void UpdateNetworkState(bool aOnline) override;

  void SendPacket(const std::string& aTransportId,
                  MediaPacket&& aPacket) override;

  RefPtr<dom::RTCStatsPromise> GetIceStats(const std::string& aTransportId,
                                           DOMHighResTimeStamp aNow) override;

  void Shutdown();

 private:
  void Destroy() override;
  void Destroy_s();
  void DestroyFinal();
  void Shutdown_s();
  RefPtr<TransportFlow> CreateTransportFlow(
      const std::string& aTransportId, bool aIsRtcp,
      const RefPtr<DtlsIdentity>& aDtlsIdentity, bool aDtlsClient,
      const DtlsDigestList& aDigests, bool aPrivacyRequested);

  struct Transport {
    RefPtr<TransportFlow> mFlow;
    RefPtr<TransportFlow> mRtcpFlow;
  };

  using MediaTransportHandler::OnAlpnNegotiated;
  using MediaTransportHandler::OnCandidate;
  using MediaTransportHandler::OnConnectionStateChange;
  using MediaTransportHandler::OnEncryptedSending;
  using MediaTransportHandler::OnGatheringStateChange;
  using MediaTransportHandler::OnPacketReceived;
  using MediaTransportHandler::OnRtcpStateChange;
  using MediaTransportHandler::OnStateChange;

  void OnGatheringStateChange(NrIceCtx* aIceCtx,
                              NrIceCtx::GatheringState aState);
  void OnConnectionStateChange(NrIceCtx* aIceCtx,
                               NrIceCtx::ConnectionState aState);
  void OnCandidateFound(NrIceMediaStream* aStream,
                        const std::string& aCandidate,
                        const std::string& aUfrag, const std::string& aMDNSAddr,
                        const std::string& aActualAddr);
  void OnStateChange(TransportLayer* aLayer, TransportLayer::State);
  void OnRtcpStateChange(TransportLayer* aLayer, TransportLayer::State);
  void PacketReceived(TransportLayer* aLayer, MediaPacket& aPacket);
  void EncryptedPacketSending(TransportLayer* aLayer, MediaPacket& aPacket);
  RefPtr<TransportFlow> GetTransportFlow(const std::string& aTransportId,
                                         bool aIsRtcp) const;
  void GetIceStats(const NrIceMediaStream& aStream, DOMHighResTimeStamp aNow,
                   dom::RTCStatsCollection* aStats) const;

  virtual ~MediaTransportHandlerSTS() = default;
  nsCOMPtr<nsISerialEventTarget> mStsThread;
  RefPtr<NrIceCtx> mIceCtx;
  RefPtr<NrIceResolver> mDNSResolver;
  std::map<std::string, Transport> mTransports;
  bool mObfuscateHostAddresses = false;
  bool mTurnDisabled = false;
  uint32_t mMinDtlsVersion = 0;
  uint32_t mMaxDtlsVersion = 0;
  bool mForceNoHost = false;
  Maybe<NrIceCtx::NatSimulatorConfig> mNatConfig;

  std::set<std::string> mSignaledAddresses;

  // Init can only be done on main, but we want this to be usable on any thread
  using InitPromise = MozPromise<bool, std::string, false>;
  RefPtr<InitPromise> mInitPromise;
};

/* static */
already_AddRefed<MediaTransportHandler> MediaTransportHandler::Create(
    nsISerialEventTarget* aCallbackThread) {
  RefPtr<MediaTransportHandler> result;
  if (XRE_IsContentProcess() &&
      Preferences::GetBool("media.peerconnection.mtransport_process") &&
      StaticPrefs::network_process_enabled()) {
    result = new MediaTransportHandlerIPC(aCallbackThread);
  } else {
    result = new MediaTransportHandlerSTS(aCallbackThread);
  }
  return result.forget();
}

class STSShutdownHandler : public nsISTSShutdownObserver {
 public:
  NS_DECL_ISUPPORTS

  // Lazy singleton
  static RefPtr<STSShutdownHandler>& Instance() {
    MOZ_ASSERT(NS_IsMainThread());
    static RefPtr<STSShutdownHandler> sHandler(new STSShutdownHandler);
    return sHandler;
  }

  void Shutdown() {
    MOZ_ASSERT(NS_IsMainThread());
    for (const auto& handler : mHandlers) {
      handler->Shutdown();
    }
    mHandlers.clear();
  }

  STSShutdownHandler() {
    CSFLogDebug(LOGTAG, "%s", __func__);
    nsresult res;
    nsCOMPtr<nsISocketTransportService> sts =
        do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &res);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(res));
    MOZ_RELEASE_ASSERT(sts);
    sts->AddShutdownObserver(this);
  }

  NS_IMETHOD Observe() override {
    CSFLogDebug(LOGTAG, "%s", __func__);
    Shutdown();
    nsresult res;
    nsCOMPtr<nsISocketTransportService> sts =
        do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &res);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(res));
    MOZ_RELEASE_ASSERT(sts);
    sts->RemoveShutdownObserver(this);
    Instance() = nullptr;
    return NS_OK;
  }

  void Register(MediaTransportHandlerSTS* aHandler) {
    MOZ_ASSERT(NS_IsMainThread());
    mHandlers.insert(aHandler);
  }

  void Deregister(MediaTransportHandlerSTS* aHandler) {
    MOZ_ASSERT(NS_IsMainThread());
    mHandlers.erase(aHandler);
  }

 private:
  virtual ~STSShutdownHandler() = default;

  // Raw ptrs, registered on init, deregistered on destruction, all on main
  std::set<MediaTransportHandlerSTS*> mHandlers;
};

NS_IMPL_ISUPPORTS(STSShutdownHandler, nsISTSShutdownObserver);

MediaTransportHandlerSTS::MediaTransportHandlerSTS(
    nsISerialEventTarget* aCallbackThread)
    : MediaTransportHandler(aCallbackThread) {
  nsresult rv;
  mStsThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  if (!mStsThread) {
    MOZ_CRASH();
  }

  RLogConnector::CreateInstance();

  CSFLogDebug(LOGTAG, "%s done %p", __func__, this);

  // We do not set up mDNSService here, because we are not running on main (we
  // use PBackground), and the DNS service asserts.
}

static NrIceCtx::Policy toNrIcePolicy(dom::RTCIceTransportPolicy aPolicy) {
  switch (aPolicy) {
    case dom::RTCIceTransportPolicy::Relay:
      return NrIceCtx::ICE_POLICY_RELAY;
    case dom::RTCIceTransportPolicy::All:
      return NrIceCtx::ICE_POLICY_ALL;
    default:
      MOZ_CRASH();
  }
  return NrIceCtx::ICE_POLICY_ALL;
}

// list of known acceptable ports for webrtc
int16_t gGoodWebrtcPortList[] = {
    53,    // Some deplyoments use DNS port to punch through overzealous NATs
    3478,  // stun or turn
    5349,  // stuns or turns
    0,     // Sentinel value: This MUST be zero
};

static nsresult addNrIceServer(const nsString& aIceUrl,
                               const dom::RTCIceServer& aIceServer,
                               std::vector<NrIceStunServer>* aStunServersOut,
                               std::vector<NrIceTurnServer>* aTurnServersOut) {
  // Without STUN/TURN handlers, NS_NewURI returns nsSimpleURI rather than
  // nsStandardURL. To parse STUN/TURN URI's to spec
  // http://tools.ietf.org/html/draft-nandakumar-rtcweb-stun-uri-02#section-3
  // http://tools.ietf.org/html/draft-petithuguenin-behave-turn-uri-03#section-3
  // we parse out the query-string, and use ParseAuthority() on the rest
  RefPtr<nsIURI> url;
  nsresult rv = NS_NewURI(getter_AddRefs(url), aIceUrl);
  NS_ENSURE_SUCCESS(rv, rv);
  bool isStun = url->SchemeIs("stun");
  bool isStuns = url->SchemeIs("stuns");
  bool isTurn = url->SchemeIs("turn");
  bool isTurns = url->SchemeIs("turns");
  if (!(isStun || isStuns || isTurn || isTurns)) {
    return NS_ERROR_FAILURE;
  }
  if (isStuns) {
    return NS_OK;  // TODO: Support STUNS (Bug 1056934)
  }

  nsAutoCString spec;
  rv = url->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  // TODO(jib@mozilla.com): Revisit once nsURI supports STUN/TURN (Bug 833509)
  int32_t port;
  nsAutoCString host;
  nsAutoCString transport;
  {
    uint32_t hostPos;
    int32_t hostLen;
    nsAutoCString path;
    rv = url->GetPathQueryRef(path);
    NS_ENSURE_SUCCESS(rv, rv);

    // Tolerate query-string + parse 'transport=[udp|tcp]' by hand.
    int32_t questionmark = path.FindChar('?');
    if (questionmark >= 0) {
      const nsCString match = "transport="_ns;

      for (int32_t i = questionmark, endPos; i >= 0; i = endPos) {
        endPos = path.FindCharInSet("&", i + 1);
        const nsDependentCSubstring fieldvaluepair =
            Substring(path, i + 1, endPos);
        if (StringBeginsWith(fieldvaluepair, match)) {
          transport = Substring(fieldvaluepair, match.Length());
          ToLowerCase(transport);
        }
      }
      path.SetLength(questionmark);
    }

    rv = net_GetAuthURLParser()->ParseAuthority(
        path.get(), static_cast<int>(path.Length()), nullptr, nullptr, nullptr,
        nullptr, &hostPos, &hostLen, &port);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!hostLen) {
      return NS_ERROR_FAILURE;
    }
    if (hostPos > 1) {
      /* The username was removed */
      return NS_ERROR_FAILURE;
    }
    path.Mid(host, hostPos, hostLen);
    // Strip off brackets around IPv6 literals
    host.Trim("[]");
  }
  if (port == -1) port = (isStuns || isTurns) ? 5349 : 3478;

  // First check the known good ports for webrtc
  bool goodPort = false;
  for (int i = 0; !goodPort && gGoodWebrtcPortList[i]; i++) {
    if (port == gGoodWebrtcPortList[i]) {
      goodPort = true;
    }
  }

  // if not in the list of known good ports for webrtc, check
  // the generic block list using NS_CheckPortSafety.
  if (!goodPort) {
    rv = NS_CheckPortSafety(port, nullptr);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (isStuns || isTurns) {
    // Should we barf if transport is set to udp or something?
    transport = kNrIceTransportTls;
  }

  if (transport.IsEmpty()) {
    transport = kNrIceTransportUdp;
  }

  if (isTurn || isTurns) {
    std::string pwd(
        NS_ConvertUTF16toUTF8(aIceServer.mCredential.Value()).get());
    std::string username(
        NS_ConvertUTF16toUTF8(aIceServer.mUsername.Value()).get());

    std::vector<unsigned char> password(pwd.begin(), pwd.end());

    UniquePtr<NrIceTurnServer> server(NrIceTurnServer::Create(
        host.get(), port, username, password, transport.get()));
    if (!server) {
      return NS_ERROR_FAILURE;
    }
    if (server->HasFqdn()) {
      // Add an IPv4 entry, then an IPv6 entry
      aTurnServersOut->push_back(*server);
      server->SetUseIPv6IfFqdn();
    }
    aTurnServersOut->emplace_back(std::move(*server));
  } else {
    UniquePtr<NrIceStunServer> server(
        NrIceStunServer::Create(host.get(), port, transport.get()));
    if (!server) {
      return NS_ERROR_FAILURE;
    }
    if (server->HasFqdn()) {
      // Add an IPv4 entry, then an IPv6 entry
      aStunServersOut->push_back(*server);
      server->SetUseIPv6IfFqdn();
    }
    aStunServersOut->emplace_back(std::move(*server));
  }
  return NS_OK;
}

/* static */
nsresult MediaTransportHandler::ConvertIceServers(
    const nsTArray<dom::RTCIceServer>& aIceServers,
    std::vector<NrIceStunServer>* aStunServers,
    std::vector<NrIceTurnServer>* aTurnServers) {
  for (const auto& iceServer : aIceServers) {
    NS_ENSURE_STATE(iceServer.mUrls.WasPassed());
    NS_ENSURE_STATE(iceServer.mUrls.Value().IsStringSequence());
    for (const auto& iceUrl : iceServer.mUrls.Value().GetAsStringSequence()) {
      nsresult rv =
          addNrIceServer(iceUrl, iceServer, aStunServers, aTurnServers);
      if (NS_FAILED(rv)) {
        CSFLogError(LOGTAG, "%s: invalid STUN/TURN server: %s", __FUNCTION__,
                    NS_ConvertUTF16toUTF8(iceUrl).get());
        return rv;
      }
    }
  }

  return NS_OK;
}

static NrIceCtx::GlobalConfig GetGlobalConfig() {
  NrIceCtx::GlobalConfig config;
  config.mAllowLinkLocal =
      Preferences::GetBool("media.peerconnection.ice.link_local", false);
  config.mAllowLoopback =
      Preferences::GetBool("media.peerconnection.ice.loopback", false);
  config.mTcpEnabled =
      Preferences::GetBool("media.peerconnection.ice.tcp", false);
  config.mStunClientMaxTransmits = Preferences::GetInt(
      "media.peerconnection.ice.stun_client_maximum_transmits",
      config.mStunClientMaxTransmits);
  config.mTrickleIceGracePeriod =
      Preferences::GetInt("media.peerconnection.ice.trickle_grace_period",
                          config.mTrickleIceGracePeriod);
  config.mIceTcpSoSockCount = Preferences::GetInt(
      "media.peerconnection.ice.tcp_so_sock_count", config.mIceTcpSoSockCount);
  config.mIceTcpListenBacklog =
      Preferences::GetInt("media.peerconnection.ice.tcp_listen_backlog",
                          config.mIceTcpListenBacklog);
  (void)Preferences::GetCString("media.peerconnection.ice.force_interface",
                                config.mForceNetInterface);
  return config;
}

static Maybe<NrIceCtx::NatSimulatorConfig> GetNatConfig() {
  bool block_tcp = Preferences::GetBool(
      "media.peerconnection.nat_simulator.block_tcp", false);
  bool block_udp = Preferences::GetBool(
      "media.peerconnection.nat_simulator.block_udp", false);
  bool block_tls = Preferences::GetBool(
      "media.peerconnection.nat_simulator.block_tls", false);
  int error_code_for_drop = Preferences::GetInt(
      "media.peerconnection.nat_simulator.error_code_for_drop", 0);
  nsAutoCString mapping_type;
  (void)Preferences::GetCString(
      "media.peerconnection.nat_simulator.mapping_type", mapping_type);
  nsAutoCString filtering_type;
  (void)Preferences::GetCString(
      "media.peerconnection.nat_simulator.filtering_type", filtering_type);
  nsAutoCString redirect_address;
  (void)Preferences::GetCString(
      "media.peerconnection.nat_simulator.redirect_address", redirect_address);
  nsAutoCString redirect_targets;
  (void)Preferences::GetCString(
      "media.peerconnection.nat_simulator.redirect_targets", redirect_targets);

  if (block_udp || block_tcp || block_tls || !mapping_type.IsEmpty() ||
      !filtering_type.IsEmpty() || !redirect_address.IsEmpty()) {
    CSFLogDebug(LOGTAG, "NAT filtering type: %s", filtering_type.get());
    CSFLogDebug(LOGTAG, "NAT mapping type: %s", mapping_type.get());
    NrIceCtx::NatSimulatorConfig natConfig;
    natConfig.mBlockUdp = block_udp;
    natConfig.mBlockTcp = block_tcp;
    natConfig.mBlockTls = block_tls;
    natConfig.mErrorCodeForDrop = error_code_for_drop;
    natConfig.mFilteringType = filtering_type;
    natConfig.mMappingType = mapping_type;
    if (redirect_address.Length()) {
      CSFLogDebug(LOGTAG, "Redirect address: %s", redirect_address.get());
      CSFLogDebug(LOGTAG, "Redirect targets: %s", redirect_targets.get());
      natConfig.mRedirectAddress = redirect_address;
      std::stringstream str(redirect_targets.Data());
      std::string target;
      while (getline(str, target, ',')) {
        CSFLogDebug(LOGTAG, "Adding target: %s", target.c_str());
        natConfig.mRedirectTargets.AppendElement(target);
      }
    }
    return Some(natConfig);
  }
  return Nothing();
}

void MediaTransportHandlerSTS::CreateIceCtx(const std::string& aName) {
  mInitPromise = InvokeAsync(
      GetMainThreadSerialEventTarget(), __func__,
      [=, self = RefPtr<MediaTransportHandlerSTS>(this)]() {
        CSFLogDebug(LOGTAG, "%s starting", __func__);
        if (!NSS_IsInitialized()) {
          if (NSS_NoDB_Init(nullptr) != SECSuccess) {
            MOZ_CRASH();
            return InitPromise::CreateAndReject("NSS_NoDB_Init failed",
                                                __func__);
          }

          if (NS_FAILED(mozilla::psm::InitializeCipherSuite())) {
            MOZ_CRASH();
            return InitPromise::CreateAndReject("InitializeCipherSuite failed",
                                                __func__);
          }

          mozilla::psm::DisableMD5();
        }

        static bool globalInitDone = false;
        if (!globalInitDone) {
          // Ensure the DNS service is initted for the first time on main
          DebugOnly<RefPtr<nsIDNSService>> dnsService =
              RefPtr<nsIDNSService>(nsDNSService::GetXPCOMSingleton());
          MOZ_ASSERT(dnsService.value);
          mStsThread->Dispatch(
              WrapRunnableNM(&NrIceCtx::InitializeGlobals, GetGlobalConfig()),
              NS_DISPATCH_NORMAL);
          globalInitDone = true;
        }

        // Give us a way to globally turn off TURN support
        mTurnDisabled =
            Preferences::GetBool("media.peerconnection.turn.disable", false);
        // We are reading these here, because when we setup the DTLS transport
        // we are on the wrong thread to read prefs
        mMinDtlsVersion =
            Preferences::GetUint("media.peerconnection.dtls.version.min");
        mMaxDtlsVersion =
            Preferences::GetUint("media.peerconnection.dtls.version.max");
        mForceNoHost =
            Preferences::GetBool("media.peerconnection.ice.no_host", false);
        mNatConfig = GetNatConfig();

        MOZ_RELEASE_ASSERT(STSShutdownHandler::Instance());
        STSShutdownHandler::Instance()->Register(this);

        return InvokeAsync(
            mStsThread, __func__,
            [=, self = RefPtr<MediaTransportHandlerSTS>(this)]() {
              mIceCtx = NrIceCtx::Create(aName);
              if (!mIceCtx) {
                return InitPromise::CreateAndReject("NrIceCtx::Create failed",
                                                    __func__);
              }

              mIceCtx->SignalGatheringStateChange.connect(
                  this, &MediaTransportHandlerSTS::OnGatheringStateChange);
              mIceCtx->SignalConnectionStateChange.connect(
                  this, &MediaTransportHandlerSTS::OnConnectionStateChange);

              mDNSResolver = new NrIceResolver;
              nsresult rv;
              if (NS_FAILED(rv = mDNSResolver->Init())) {
                CSFLogError(LOGTAG, "%s: Failed to initialize dns resolver",
                            __FUNCTION__);
                return InitPromise::CreateAndReject(
                    "Failed to initialize dns resolver", __func__);
              }
              if (NS_FAILED(rv = mIceCtx->SetResolver(
                                mDNSResolver->AllocateResolver()))) {
                CSFLogError(LOGTAG, "%s: Failed to get dns resolver",
                            __FUNCTION__);
                return InitPromise::CreateAndReject(
                    "Failed to get dns resolver", __func__);
              }

              CSFLogDebug(LOGTAG, "%s done", __func__);
              return InitPromise::CreateAndResolve(true, __func__);
            });
      });
}

nsresult MediaTransportHandlerSTS::SetIceConfig(
    const nsTArray<dom::RTCIceServer>& aIceServers,
    dom::RTCIceTransportPolicy aIcePolicy) {
  // We rely on getting an error when this happens, so do it up front.
  std::vector<NrIceStunServer> stunServers;
  std::vector<NrIceTurnServer> turnServers;
  nsresult rv = ConvertIceServers(aIceServers, &stunServers, &turnServers);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MOZ_RELEASE_ASSERT(mInitPromise);

  mInitPromise->Then(
      mStsThread, __func__,
      [=, self = RefPtr<MediaTransportHandlerSTS>(this)]() {
        if (!mIceCtx) {
          CSFLogError(LOGTAG, "%s: mIceCtx is null", __FUNCTION__);
          return;
        }
        NrIceCtx::Config config;
        config.mPolicy = toNrIcePolicy(aIcePolicy);
        if (config.mPolicy == NrIceCtx::ICE_POLICY_ALL && mForceNoHost) {
          config.mPolicy = NrIceCtx::ICE_POLICY_NO_HOST;
        }
        config.mNatSimulatorConfig = mNatConfig;

        nsresult rv;

        if (NS_FAILED(rv = mIceCtx->SetStunServers(stunServers))) {
          CSFLogError(LOGTAG, "%s: Failed to set stun servers", __FUNCTION__);
          return;
        }
        if (!mTurnDisabled) {
          if (NS_FAILED(rv = mIceCtx->SetTurnServers(turnServers))) {
            CSFLogError(LOGTAG, "%s: Failed to set turn servers", __FUNCTION__);
            return;
          }
        } else if (!turnServers.empty()) {
          CSFLogError(LOGTAG, "%s: Setting turn servers disabled",
                      __FUNCTION__);
        }
        if (NS_FAILED(rv = mIceCtx->SetIceConfig(config))) {
          CSFLogError(LOGTAG, "%s: Failed to set config", __FUNCTION__);
        }
      });

  return NS_OK;
}

void MediaTransportHandlerSTS::Shutdown() {
  CSFLogDebug(LOGTAG, "%s", __func__);
  MOZ_ASSERT(NS_IsMainThread());
  mStsThread->Dispatch(NewNonOwningRunnableMethod(
      __func__, this, &MediaTransportHandlerSTS::Shutdown_s));
}

void MediaTransportHandlerSTS::Shutdown_s() {
  CSFLogDebug(LOGTAG, "%s", __func__);
  disconnect_all();
  // Clear the transports before destroying the ice ctx so that
  // the close_notify alerts have a chance to be sent as the
  // TransportFlow destructors execute.
  mTransports.clear();
  if (mIceCtx) {
    NrIceStats stats = mIceCtx->Destroy();
    CSFLogDebug(LOGTAG,
                "Ice Telemetry: stun (retransmits: %d)"
                "   turn (401s: %d   403s: %d   438s: %d)",
                stats.stun_retransmits, stats.turn_401s, stats.turn_403s,
                stats.turn_438s);
  }
  mIceCtx = nullptr;
  mDNSResolver = nullptr;
}

void MediaTransportHandlerSTS::Destroy() {
  CSFLogDebug(LOGTAG, "%s %p", __func__, this);
  // Our "destruction tour" starts on main, because we need to deregister.
  if (!NS_IsMainThread()) {
    GetMainThreadEventTarget()->Dispatch(NewNonOwningRunnableMethod(
        __func__, this, &MediaTransportHandlerSTS::Destroy));
    return;
  }

  MOZ_ASSERT(NS_IsMainThread());
  if (STSShutdownHandler::Instance()) {
    STSShutdownHandler::Instance()->Deregister(this);
    Shutdown();
  }

  // mIceCtx still has a reference to us via sigslot! We must dispach to STS,
  // and clean up there. However, by the time _that_ happens, we may have
  // dispatched a signal callback to mCallbackThread, so we have to dispatch
  // the final destruction to mCallbackThread.
  nsresult rv = mStsThread->Dispatch(NewNonOwningRunnableMethod(
      __func__, this, &MediaTransportHandlerSTS::Destroy_s));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    CSFLogError(LOGTAG,
                "Unable to dispatch to STS: why has the XPCOM shutdown handler "
                "not been invoked?");
    delete this;
  }
}

void MediaTransportHandlerSTS::Destroy_s() {
  if (mCallbackThread && !mCallbackThread->IsOnCurrentThread()) {
    nsresult rv = mCallbackThread->Dispatch(NewNonOwningRunnableMethod(
        __func__, this, &MediaTransportHandlerSTS::DestroyFinal));
    if (NS_SUCCEEDED(rv)) {
      return;
    }
  }

  DestroyFinal();
}

void MediaTransportHandlerSTS::DestroyFinal() { delete this; }

void MediaTransportHandlerSTS::SetProxyConfig(
    NrSocketProxyConfig&& aProxyConfig) {
  MOZ_RELEASE_ASSERT(mInitPromise);

  mInitPromise->Then(
      mStsThread, __func__,
      [this, self = RefPtr<MediaTransportHandlerSTS>(this),
       aProxyConfig = std::move(aProxyConfig)]() mutable {
        if (!mIceCtx) {
          return;  // Probably due to XPCOM shutdown
        }

        mIceCtx->SetProxyConfig(std::move(aProxyConfig));
      },
      [](const std::string& aError) {});
}

void MediaTransportHandlerSTS::EnsureProvisionalTransport(
    const std::string& aTransportId, const std::string& aUfrag,
    const std::string& aPwd, int aComponentCount) {
  MOZ_RELEASE_ASSERT(mInitPromise);

  mInitPromise->Then(
      mStsThread, __func__,
      [=, self = RefPtr<MediaTransportHandlerSTS>(this)]() {
        if (!mIceCtx) {
          return;  // Probably due to XPCOM shutdown
        }

        RefPtr<NrIceMediaStream> stream(mIceCtx->GetStream(aTransportId));
        if (!stream) {
          CSFLogDebug(LOGTAG, "%s: Creating ICE media stream=%s components=%d",
                      mIceCtx->name().c_str(), aTransportId.c_str(),
                      aComponentCount);

          std::ostringstream os;
          os << mIceCtx->name() << " transport-id=" << aTransportId;
          stream =
              mIceCtx->CreateStream(aTransportId, os.str(), aComponentCount);

          if (!stream) {
            CSFLogError(LOGTAG, "Failed to create ICE stream.");
            return;
          }

          stream->SignalCandidate.connect(
              this, &MediaTransportHandlerSTS::OnCandidateFound);
        }

        // Begins an ICE restart if this stream has a different ufrag/pwd
        stream->SetIceCredentials(aUfrag, aPwd);

        // Make sure there's an entry in mTransports
        mTransports[aTransportId];
      },
      [](const std::string& aError) {});
}

void MediaTransportHandlerSTS::ActivateTransport(
    const std::string& aTransportId, const std::string& aLocalUfrag,
    const std::string& aLocalPwd, size_t aComponentCount,
    const std::string& aUfrag, const std::string& aPassword,
    const nsTArray<uint8_t>& aKeyDer, const nsTArray<uint8_t>& aCertDer,
    SSLKEAType aAuthType, bool aDtlsClient, const DtlsDigestList& aDigests,
    bool aPrivacyRequested) {
  MOZ_RELEASE_ASSERT(mInitPromise);

  mInitPromise->Then(
      mStsThread, __func__,
      [=, keyDer = aKeyDer.Clone(), certDer = aCertDer.Clone(),
       self = RefPtr<MediaTransportHandlerSTS>(this)]() {
        if (!mIceCtx) {
          return;  // Probably due to XPCOM shutdown
        }

        MOZ_ASSERT(aComponentCount);
        RefPtr<DtlsIdentity> dtlsIdentity(
            DtlsIdentity::Deserialize(keyDer, certDer, aAuthType));
        if (!dtlsIdentity) {
          MOZ_ASSERT(false);
          return;
        }

        RefPtr<NrIceMediaStream> stream(mIceCtx->GetStream(aTransportId));
        if (!stream) {
          MOZ_ASSERT(false);
          return;
        }

        CSFLogDebug(LOGTAG, "%s: Activating ICE media stream=%s components=%u",
                    mIceCtx->name().c_str(), aTransportId.c_str(),
                    static_cast<unsigned>(aComponentCount));

        std::vector<std::string> attrs;
        attrs.reserve(2 /* ufrag + pwd */);
        attrs.push_back("ice-ufrag:" + aUfrag);
        attrs.push_back("ice-pwd:" + aPassword);

        // If we started an ICE restart in EnsureProvisionalTransport, this is
        // where we decide whether to commit or rollback.
        nsresult rv = stream->ConnectToPeer(aLocalUfrag, aLocalPwd, attrs);
        if (NS_FAILED(rv)) {
          CSFLogError(LOGTAG, "Couldn't parse ICE attributes, rv=%u",
                      static_cast<unsigned>(rv));
          MOZ_ASSERT(false);
          return;
        }

        Transport transport = mTransports[aTransportId];
        if (!transport.mFlow) {
          transport.mFlow =
              CreateTransportFlow(aTransportId, false, dtlsIdentity,
                                  aDtlsClient, aDigests, aPrivacyRequested);
          if (!transport.mFlow) {
            return;
          }
          TransportLayer* dtls =
              transport.mFlow->GetLayer(TransportLayerDtls::ID());
          dtls->SignalStateChange.connect(
              this, &MediaTransportHandlerSTS::OnStateChange);
          if (aComponentCount < 2) {
            dtls->SignalStateChange.connect(
                this, &MediaTransportHandlerSTS::OnRtcpStateChange);
          }
        }

        if (aComponentCount == 2) {
          if (!transport.mRtcpFlow) {
            transport.mRtcpFlow =
                CreateTransportFlow(aTransportId, true, dtlsIdentity,
                                    aDtlsClient, aDigests, aPrivacyRequested);
            if (!transport.mRtcpFlow) {
              return;
            }
            TransportLayer* dtls =
                transport.mRtcpFlow->GetLayer(TransportLayerDtls::ID());
            dtls->SignalStateChange.connect(
                this, &MediaTransportHandlerSTS::OnRtcpStateChange);
          }
        } else {
          transport.mRtcpFlow = nullptr;
          // components are 1-indexed
          stream->DisableComponent(2);
        }

        mTransports[aTransportId] = transport;
      },
      [](const std::string& aError) {});
}

void MediaTransportHandlerSTS::SetTargetForDefaultLocalAddressLookup(
    const std::string& aTargetIp, uint16_t aTargetPort) {
  MOZ_RELEASE_ASSERT(mInitPromise);

  mInitPromise->Then(
      mStsThread, __func__,
      [=, self = RefPtr<MediaTransportHandlerSTS>(this)]() {
        if (!mIceCtx) {
          return;  // Probably due to XPCOM shutdown
        }

        mIceCtx->SetTargetForDefaultLocalAddressLookup(aTargetIp, aTargetPort);
      },
      [](const std::string& aError) {});
}

void MediaTransportHandlerSTS::StartIceGathering(
    bool aDefaultRouteOnly, bool aObfuscateHostAddresses,
    const nsTArray<NrIceStunAddr>& aStunAddrs) {
  MOZ_RELEASE_ASSERT(mInitPromise);

  mInitPromise->Then(
      mStsThread, __func__,
      [=, stunAddrs = aStunAddrs.Clone(),
       self = RefPtr<MediaTransportHandlerSTS>(this)]() {
        if (!mIceCtx) {
          return;  // Probably due to XPCOM shutdown
        }

        mObfuscateHostAddresses = aObfuscateHostAddresses;

        // Belt and suspenders - in e10s mode, the call below to SetStunAddrs
        // needs to have the proper flags set on ice ctx.  For non-e10s,
        // setting those flags happens in StartGathering.  We could probably
        // just set them here, and only do it here.
        mIceCtx->SetCtxFlags(aDefaultRouteOnly);

        if (stunAddrs.Length()) {
          mIceCtx->SetStunAddrs(stunAddrs);
        }

        // Start gathering, but only if there are streams
        if (!mIceCtx->GetStreams().empty()) {
          mIceCtx->StartGathering(aDefaultRouteOnly, aObfuscateHostAddresses);
        }
      },
      [](const std::string& aError) {});
}

void MediaTransportHandlerSTS::StartIceChecks(
    bool aIsControlling, const std::vector<std::string>& aIceOptions) {
  MOZ_RELEASE_ASSERT(mInitPromise);

  mInitPromise->Then(
      mStsThread, __func__,
      [=, self = RefPtr<MediaTransportHandlerSTS>(this)]() {
        if (!mIceCtx) {
          return;  // Probably due to XPCOM shutdown
        }

        nsresult rv = mIceCtx->ParseGlobalAttributes(aIceOptions);
        if (NS_FAILED(rv)) {
          CSFLogError(LOGTAG, "%s: couldn't parse global parameters",
                      __FUNCTION__);
          return;
        }

        rv = mIceCtx->SetControlling(aIsControlling ? NrIceCtx::ICE_CONTROLLING
                                                    : NrIceCtx::ICE_CONTROLLED);
        if (NS_FAILED(rv)) {
          CSFLogError(LOGTAG, "%s: couldn't set controlling to %d",
                      __FUNCTION__, aIsControlling);
          return;
        }

        rv = mIceCtx->StartChecks();
        if (NS_FAILED(rv)) {
          CSFLogError(LOGTAG, "%s: couldn't start checks", __FUNCTION__);
          return;
        }
      },
      [](const std::string& aError) {});
}

void TokenizeCandidate(const std::string& aCandidate,
                       std::vector<std::string>& aTokens) {
  aTokens.clear();

  std::istringstream iss(aCandidate);
  std::string token;
  while (std::getline(iss, token, ' ')) {
    aTokens.push_back(token);
  }
}

void MediaTransportHandlerSTS::AddIceCandidate(
    const std::string& aTransportId, const std::string& aCandidate,
    const std::string& aUfrag, const std::string& aObfuscatedAddress) {
  MOZ_RELEASE_ASSERT(mInitPromise);

  mInitPromise->Then(
      mStsThread, __func__,
      [=, self = RefPtr<MediaTransportHandlerSTS>(this)]() {
        if (!mIceCtx) {
          return;  // Probably due to XPCOM shutdown
        }

        std::vector<std::string> tokens;
        TokenizeCandidate(aCandidate, tokens);

        RefPtr<NrIceMediaStream> stream(mIceCtx->GetStream(aTransportId));
        if (!stream) {
          CSFLogError(LOGTAG,
                      "No ICE stream for candidate with transport id %s: %s",
                      aTransportId.c_str(), aCandidate.c_str());
          return;
        }

        nsresult rv = stream->ParseTrickleCandidate(aCandidate, aUfrag,
                                                    aObfuscatedAddress);
        if (NS_SUCCEEDED(rv)) {
          // If the address is not obfuscated, we want to track it as
          // explicitly signaled so that we know it is fine to reveal
          // the address later on.
          if (mObfuscateHostAddresses && tokens.size() > 4 &&
              aObfuscatedAddress.empty()) {
            mSignaledAddresses.insert(tokens[4]);
          }
        } else {
          CSFLogError(LOGTAG,
                      "Couldn't process ICE candidate with transport id %s: "
                      "%s",
                      aTransportId.c_str(), aCandidate.c_str());
        }
      },
      [](const std::string& aError) {});
}

void MediaTransportHandlerSTS::UpdateNetworkState(bool aOnline) {
  MOZ_RELEASE_ASSERT(mInitPromise);

  mInitPromise->Then(
      mStsThread, __func__,
      [=, self = RefPtr<MediaTransportHandlerSTS>(this)]() {
        if (!mIceCtx) {
          return;  // Probably due to XPCOM shutdown
        }

        mIceCtx->UpdateNetworkState(aOnline);
      },
      [](const std::string& aError) {});
}

void MediaTransportHandlerSTS::RemoveTransportsExcept(
    const std::set<std::string>& aTransportIds) {
  MOZ_RELEASE_ASSERT(mInitPromise);

  mInitPromise->Then(
      mStsThread, __func__,
      [=, self = RefPtr<MediaTransportHandlerSTS>(this)]() {
        if (!mIceCtx) {
          return;  // Probably due to XPCOM shutdown
        }

        for (auto it = mTransports.begin(); it != mTransports.end();) {
          const std::string transportId(it->first);
          if (!aTransportIds.count(transportId)) {
            if (it->second.mFlow) {
              OnStateChange(transportId, TransportLayer::TS_NONE);
              OnRtcpStateChange(transportId, TransportLayer::TS_NONE);
            }
            // Erase the transport before destroying the ice stream so that
            // the close_notify alerts have a chance to be sent as the
            // TransportFlow destructors execute.
            it = mTransports.erase(it);
            // We're already on the STS thread, but the TransportFlow
            // destructor executed when mTransports.erase(it) is called
            // above dispatches the call to DestroyFinal to the STS thread. If
            // we don't also dispatch the call to destroy the NrIceMediaStream
            // to the STS thread, it will tear down the NrIceMediaStream
            // before the TransportFlow is destroyed.  Without a valid
            // NrIceMediaStream the close_notify alert cannot be sent.
            mStsThread->Dispatch(NS_NewRunnableFunction(
                __func__, [iceCtx = RefPtr<NrIceCtx>(mIceCtx), transportId] {
                  iceCtx->DestroyStream(transportId);
                }));
          } else {
            MOZ_ASSERT(it->second.mFlow);
            ++it;
          }
        }
      },
      [](const std::string& aError) {});
}

void MediaTransportHandlerSTS::SendPacket(const std::string& aTransportId,
                                          MediaPacket&& aPacket) {
  MOZ_RELEASE_ASSERT(mInitPromise);

  mInitPromise->Then(
      mStsThread, __func__,
      [this, self = RefPtr<MediaTransportHandlerSTS>(this), aTransportId,
       aPacket = std::move(aPacket)]() mutable {
        if (!mIceCtx) {
          return;  // Probably due to XPCOM shutdown
        }

        MOZ_ASSERT(aPacket.type() != MediaPacket::UNCLASSIFIED);
        RefPtr<TransportFlow> flow =
            GetTransportFlow(aTransportId, aPacket.type() == MediaPacket::RTCP);

        if (!flow) {
          CSFLogError(LOGTAG,
                      "%s: No such transport flow (%s) for outgoing packet",
                      mIceCtx->name().c_str(), aTransportId.c_str());
          return;
        }

        TransportLayer* layer = nullptr;
        switch (aPacket.type()) {
          case MediaPacket::SCTP:
            layer = flow->GetLayer(TransportLayerDtls::ID());
            break;
          case MediaPacket::RTP:
          case MediaPacket::RTCP:
            layer = flow->GetLayer(TransportLayerSrtp::ID());
            break;
          default:
            // Maybe it would be useful to allow the injection of other packet
            // types for testing?
            MOZ_ASSERT(false);
            return;
        }

        MOZ_ASSERT(layer);

        if (layer->SendPacket(aPacket) < 0) {
          CSFLogError(LOGTAG, "%s: Transport flow (%s) failed to send packet",
                      mIceCtx->name().c_str(), aTransportId.c_str());
        }
      },
      [](const std::string& aError) {});
}

TransportLayer::State MediaTransportHandler::GetState(
    const std::string& aTransportId, bool aRtcp) const {
  // TODO Bug 1520692: we should allow Datachannel to connect without
  // DTLS SRTP keys
  if (mCallbackThread) {
    MOZ_ASSERT(mCallbackThread->IsOnCurrentThread());
  }

  const std::map<std::string, TransportLayer::State>* cache = nullptr;
  if (aRtcp) {
    cache = &mRtcpStateCache;
  } else {
    cache = &mStateCache;
  }

  auto it = cache->find(aTransportId);
  if (it != cache->end()) {
    return it->second;
  }
  return TransportLayer::TS_NONE;
}

void MediaTransportHandler::OnCandidate(const std::string& aTransportId,
                                        const CandidateInfo& aCandidateInfo) {
  if (mCallbackThread && !mCallbackThread->IsOnCurrentThread()) {
    mCallbackThread->Dispatch(
        // This is being called from sigslot, which does not hold a strong ref.
        WrapRunnable(this, &MediaTransportHandler::OnCandidate, aTransportId,
                     aCandidateInfo),
        NS_DISPATCH_NORMAL);
    return;
  }

  SignalCandidate(aTransportId, aCandidateInfo);
}

void MediaTransportHandler::OnAlpnNegotiated(const std::string& aAlpn) {
  if (mCallbackThread && !mCallbackThread->IsOnCurrentThread()) {
    mCallbackThread->Dispatch(
        // This is being called from sigslot, which does not hold a strong ref.
        WrapRunnable(this, &MediaTransportHandler::OnAlpnNegotiated, aAlpn),
        NS_DISPATCH_NORMAL);
    return;
  }

  const bool privacyRequested = aAlpn == "c-webrtc";
  SignalAlpnNegotiated(aAlpn, privacyRequested);
}

void MediaTransportHandler::OnGatheringStateChange(
    dom::RTCIceGatheringState aState) {
  if (mCallbackThread && !mCallbackThread->IsOnCurrentThread()) {
    mCallbackThread->Dispatch(
        // This is being called from sigslot, which does not hold a strong ref.
        WrapRunnable(this, &MediaTransportHandler::OnGatheringStateChange,
                     aState),
        NS_DISPATCH_NORMAL);
    return;
  }

  SignalGatheringStateChange(aState);
}

void MediaTransportHandler::OnConnectionStateChange(
    dom::RTCIceConnectionState aState) {
  if (mCallbackThread && !mCallbackThread->IsOnCurrentThread()) {
    mCallbackThread->Dispatch(
        // This is being called from sigslot, which does not hold a strong ref.
        WrapRunnable(this, &MediaTransportHandler::OnConnectionStateChange,
                     aState),
        NS_DISPATCH_NORMAL);
    return;
  }

  SignalConnectionStateChange(aState);
}

void MediaTransportHandler::OnPacketReceived(const std::string& aTransportId,
                                             const MediaPacket& aPacket) {
  if (mCallbackThread && !mCallbackThread->IsOnCurrentThread()) {
    mCallbackThread->Dispatch(
        // This is being called from sigslot, which does not hold a strong ref.
        WrapRunnable(this, &MediaTransportHandler::OnPacketReceived,
                     aTransportId, aPacket.Clone()),
        NS_DISPATCH_NORMAL);
    return;
  }

  SignalPacketReceived(aTransportId, aPacket);
}

void MediaTransportHandler::OnEncryptedSending(const std::string& aTransportId,
                                               const MediaPacket& aPacket) {
  if (mCallbackThread && !mCallbackThread->IsOnCurrentThread()) {
    mCallbackThread->Dispatch(
        // This is being called from sigslot, which does not hold a strong ref.
        WrapRunnable(this, &MediaTransportHandler::OnEncryptedSending,
                     aTransportId, aPacket.Clone()),
        NS_DISPATCH_NORMAL);
    return;
  }

  SignalEncryptedSending(aTransportId, aPacket);
}

void MediaTransportHandler::OnStateChange(const std::string& aTransportId,
                                          TransportLayer::State aState) {
  if (mCallbackThread && !mCallbackThread->IsOnCurrentThread()) {
    mCallbackThread->Dispatch(
        // This is being called from sigslot, which does not hold a strong ref.
        WrapRunnable(this, &MediaTransportHandler::OnStateChange, aTransportId,
                     aState),
        NS_DISPATCH_NORMAL);
    return;
  }

  if (aState == TransportLayer::TS_NONE) {
    mStateCache.erase(aTransportId);
  } else {
    mStateCache[aTransportId] = aState;
  }
  SignalStateChange(aTransportId, aState);
}

void MediaTransportHandler::OnRtcpStateChange(const std::string& aTransportId,
                                              TransportLayer::State aState) {
  if (mCallbackThread && !mCallbackThread->IsOnCurrentThread()) {
    mCallbackThread->Dispatch(
        // This is being called from sigslot, which does not hold a strong ref.
        WrapRunnable(this, &MediaTransportHandler::OnRtcpStateChange,
                     aTransportId, aState),
        NS_DISPATCH_NORMAL);
    return;
  }

  if (aState == TransportLayer::TS_NONE) {
    mRtcpStateCache.erase(aTransportId);
  } else {
    mRtcpStateCache[aTransportId] = aState;
  }
  SignalRtcpStateChange(aTransportId, aState);
}

RefPtr<dom::RTCStatsPromise> MediaTransportHandlerSTS::GetIceStats(
    const std::string& aTransportId, DOMHighResTimeStamp aNow) {
  MOZ_RELEASE_ASSERT(mInitPromise);

  return mInitPromise->Then(
      mStsThread, __func__,
      [=, self = RefPtr<MediaTransportHandlerSTS>(this)]() {
        UniquePtr<dom::RTCStatsCollection> stats(new dom::RTCStatsCollection);
        if (mIceCtx) {
          for (const auto& stream : mIceCtx->GetStreams()) {
            if (aTransportId.empty() || aTransportId == stream->GetId()) {
              GetIceStats(*stream, aNow, stats.get());
            }
          }
        }
        return dom::RTCStatsPromise::CreateAndResolve(std::move(stats),
                                                      __func__);
      });
}

RefPtr<MediaTransportHandler::IceLogPromise>
MediaTransportHandlerSTS::GetIceLog(const nsCString& aPattern) {
  return InvokeAsync(
      mStsThread, __func__, [=, self = RefPtr<MediaTransportHandlerSTS>(this)] {
        dom::Sequence<nsString> converted;
        RLogConnector* logs = RLogConnector::GetInstance();
        std::deque<std::string> result;
        // Might not exist yet.
        if (logs) {
          logs->Filter(aPattern.get(), 0, &result);
        }
        /// XXX(Bug 1631386) Check if we should reject the promise instead of
        /// crashing in an OOM situation.
        if (!converted.SetCapacity(result.size(), fallible)) {
          mozalloc_handle_oom(sizeof(nsString) * result.size());
        }
        for (auto& line : result) {
          // Cannot fail, SetCapacity was called before.
          (void)converted.AppendElement(NS_ConvertUTF8toUTF16(line.c_str()),
                                        fallible);
        }
        return IceLogPromise::CreateAndResolve(std::move(converted), __func__);
      });
}

void MediaTransportHandlerSTS::ClearIceLog() {
  if (!mStsThread->IsOnCurrentThread()) {
    mStsThread->Dispatch(WrapRunnable(RefPtr<MediaTransportHandlerSTS>(this),
                                      &MediaTransportHandlerSTS::ClearIceLog),
                         NS_DISPATCH_NORMAL);
    return;
  }

  RLogConnector* logs = RLogConnector::GetInstance();
  if (logs) {
    logs->Clear();
  }
}

void MediaTransportHandlerSTS::EnterPrivateMode() {
  if (!mStsThread->IsOnCurrentThread()) {
    mStsThread->Dispatch(
        WrapRunnable(RefPtr<MediaTransportHandlerSTS>(this),
                     &MediaTransportHandlerSTS::EnterPrivateMode),
        NS_DISPATCH_NORMAL);
    return;
  }

  RLogConnector::GetInstance()->EnterPrivateMode();
}

void MediaTransportHandlerSTS::ExitPrivateMode() {
  if (!mStsThread->IsOnCurrentThread()) {
    mStsThread->Dispatch(
        WrapRunnable(RefPtr<MediaTransportHandlerSTS>(this),
                     &MediaTransportHandlerSTS::ExitPrivateMode),
        NS_DISPATCH_NORMAL);
    return;
  }

  auto* log = RLogConnector::GetInstance();
  MOZ_ASSERT(log);
  if (log) {
    log->ExitPrivateMode();
  }
}

static void ToRTCIceCandidateStats(
    const std::vector<NrIceCandidate>& candidates,
    dom::RTCStatsType candidateType, const nsString& transportId,
    DOMHighResTimeStamp now, dom::RTCStatsCollection* stats,
    bool obfuscateHostAddresses,
    const std::set<std::string>& signaledAddresses) {
  MOZ_ASSERT(stats);
  for (const auto& candidate : candidates) {
    dom::RTCIceCandidateStats cand;
    cand.mType.Construct(candidateType);
    NS_ConvertASCIItoUTF16 codeword(candidate.codeword.c_str());
    cand.mTransportId.Construct(transportId);
    cand.mId.Construct(codeword);
    cand.mTimestamp.Construct(now);
    cand.mCandidateType.Construct(dom::RTCIceCandidateType(candidate.type));
    cand.mPriority.Construct(candidate.priority);
    // https://tools.ietf.org/html/draft-ietf-rtcweb-mdns-ice-candidates-03#section-3.3.1
    // This obfuscates the address with the mDNS address if one exists
    if (!candidate.mdns_addr.empty()) {
      cand.mAddress.Construct(
          NS_ConvertASCIItoUTF16(candidate.mdns_addr.c_str()));
    } else if (obfuscateHostAddresses &&
               candidate.type == NrIceCandidate::ICE_PEER_REFLEXIVE &&
               signaledAddresses.find(candidate.cand_addr.host) ==
                   signaledAddresses.end()) {
      cand.mAddress.Construct(NS_ConvertASCIItoUTF16("(redacted)"));
    } else {
      cand.mAddress.Construct(
          NS_ConvertASCIItoUTF16(candidate.cand_addr.host.c_str()));
    }
    cand.mPort.Construct(candidate.cand_addr.port);
    cand.mProtocol.Construct(
        NS_ConvertASCIItoUTF16(candidate.cand_addr.transport.c_str()));
    if (candidateType == dom::RTCStatsType::Local_candidate &&
        dom::RTCIceCandidateType(candidate.type) ==
            dom::RTCIceCandidateType::Relay) {
      cand.mRelayProtocol.Construct(
          NS_ConvertASCIItoUTF16(candidate.local_addr.transport.c_str()));
    }
    cand.mProxied.Construct(NS_ConvertASCIItoUTF16(
        candidate.is_proxied ? "proxied" : "non-proxied"));
    if (!stats->mIceCandidateStats.AppendElement(cand, fallible)) {
      // XXX(Bug 1632090) Instead of extending the array 1-by-1 (which might
      // involve multiple reallocations) and potentially crashing here,
      // SetCapacity could be called outside the loop once.
      mozalloc_handle_oom(0);
    }
    if (candidate.trickled) {
      if (!stats->mTrickledIceCandidateStats.AppendElement(cand, fallible)) {
        mozalloc_handle_oom(0);
      }
    }
  }
}

void MediaTransportHandlerSTS::GetIceStats(
    const NrIceMediaStream& aStream, DOMHighResTimeStamp aNow,
    dom::RTCStatsCollection* aStats) const {
  MOZ_ASSERT(mStsThread->IsOnCurrentThread());

  NS_ConvertASCIItoUTF16 transportId(aStream.GetId().c_str());

  std::vector<NrIceCandidatePair> candPairs;
  nsresult res = aStream.GetCandidatePairs(&candPairs);
  if (NS_FAILED(res)) {
    CSFLogError(LOGTAG,
                "%s: Error getting candidate pairs for transport id \"%s\"",
                __FUNCTION__, aStream.GetId().c_str());
    return;
  }

  for (auto& candPair : candPairs) {
    NS_ConvertASCIItoUTF16 codeword(candPair.codeword.c_str());
    NS_ConvertASCIItoUTF16 localCodeword(candPair.local.codeword.c_str());
    NS_ConvertASCIItoUTF16 remoteCodeword(candPair.remote.codeword.c_str());
    // Only expose candidate-pair statistics to chrome, until we've thought
    // through the implications of exposing it to content.

    dom::RTCIceCandidatePairStats s;
    s.mId.Construct(codeword);
    s.mTransportId.Construct(transportId);
    s.mTimestamp.Construct(aNow);
    s.mType.Construct(dom::RTCStatsType::Candidate_pair);
    s.mLocalCandidateId.Construct(localCodeword);
    s.mRemoteCandidateId.Construct(remoteCodeword);
    s.mNominated.Construct(candPair.nominated);
    s.mWritable.Construct(candPair.writable);
    s.mReadable.Construct(candPair.readable);
    s.mPriority.Construct(candPair.priority);
    s.mSelected.Construct(candPair.selected);
    s.mBytesSent.Construct(candPair.bytes_sent);
    s.mBytesReceived.Construct(candPair.bytes_recvd);
    s.mLastPacketSentTimestamp.Construct(candPair.ms_since_last_send);
    s.mLastPacketReceivedTimestamp.Construct(candPair.ms_since_last_recv);
    s.mState.Construct(dom::RTCStatsIceCandidatePairState(candPair.state));
    s.mComponentId.Construct(candPair.component_id);
    if (!aStats->mIceCandidatePairStats.AppendElement(s, fallible)) {
      // XXX(Bug 1632090) Instead of extending the array 1-by-1 (which might
      // involve multiple reallocations) and potentially crashing here,
      // SetCapacity could be called outside the loop once.
      mozalloc_handle_oom(0);
    }
  }

  std::vector<NrIceCandidate> candidates;
  if (NS_SUCCEEDED(aStream.GetLocalCandidates(&candidates))) {
    ToRTCIceCandidateStats(candidates, dom::RTCStatsType::Local_candidate,
                           transportId, aNow, aStats, mObfuscateHostAddresses,
                           mSignaledAddresses);
    // add the local candidates unparsed string to a sequence
    for (const auto& candidate : candidates) {
      if (!aStats->mRawLocalCandidates.AppendElement(
              NS_ConvertASCIItoUTF16(candidate.label.c_str()), fallible)) {
        // XXX(Bug 1632090) Instead of extending the array 1-by-1 (which might
        // involve multiple reallocations) and potentially crashing here,
        // SetCapacity could be called outside the loop once.
        mozalloc_handle_oom(0);
      }
    }
  }
  candidates.clear();

  if (NS_SUCCEEDED(aStream.GetRemoteCandidates(&candidates))) {
    ToRTCIceCandidateStats(candidates, dom::RTCStatsType::Remote_candidate,
                           transportId, aNow, aStats, mObfuscateHostAddresses,
                           mSignaledAddresses);
    // add the remote candidates unparsed string to a sequence
    for (const auto& candidate : candidates) {
      if (!aStats->mRawRemoteCandidates.AppendElement(
              NS_ConvertASCIItoUTF16(candidate.label.c_str()), fallible)) {
        // XXX(Bug 1632090) Instead of extending the array 1-by-1 (which might
        // involve multiple reallocations) and potentially crashing here,
        // SetCapacity could be called outside the loop once.
        mozalloc_handle_oom(0);
      }
    }
  }
}

RefPtr<TransportFlow> MediaTransportHandlerSTS::GetTransportFlow(
    const std::string& aTransportId, bool aIsRtcp) const {
  auto it = mTransports.find(aTransportId);
  if (it == mTransports.end()) {
    return nullptr;
  }

  if (aIsRtcp) {
    return it->second.mRtcpFlow ? it->second.mRtcpFlow : it->second.mFlow;
    ;
  }

  return it->second.mFlow;
}

RefPtr<TransportFlow> MediaTransportHandlerSTS::CreateTransportFlow(
    const std::string& aTransportId, bool aIsRtcp,
    const RefPtr<DtlsIdentity>& aDtlsIdentity, bool aDtlsClient,
    const DtlsDigestList& aDigests, bool aPrivacyRequested) {
  nsresult rv;
  RefPtr<TransportFlow> flow = new TransportFlow(aTransportId);

  // The media streams are made on STS so we need to defer setup.
  auto ice = MakeUnique<TransportLayerIce>();
  auto dtls = MakeUnique<TransportLayerDtls>();
  auto srtp = MakeUnique<TransportLayerSrtp>(*dtls);
  dtls->SetRole(aDtlsClient ? TransportLayerDtls::CLIENT
                            : TransportLayerDtls::SERVER);

  dtls->SetIdentity(aDtlsIdentity);

  dtls->SetMinMaxVersion(
      static_cast<TransportLayerDtls::Version>(mMinDtlsVersion),
      static_cast<TransportLayerDtls::Version>(mMaxDtlsVersion));

  for (const auto& digest : aDigests) {
    rv = dtls->SetVerificationDigest(digest);
    if (NS_FAILED(rv)) {
      CSFLogError(LOGTAG, "Could not set fingerprint");
      return nullptr;
    }
  }

  std::vector<uint16_t> srtpCiphers =
      TransportLayerDtls::GetDefaultSrtpCiphers();

  rv = dtls->SetSrtpCiphers(srtpCiphers);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "Couldn't set SRTP ciphers");
    return nullptr;
  }

  // Always permits negotiation of the confidential mode.
  // Only allow non-confidential (which is an allowed default),
  // if we aren't confidential.
  std::set<std::string> alpn = {"c-webrtc"};
  std::string alpnDefault;
  if (!aPrivacyRequested) {
    alpnDefault = "webrtc";
    alpn.insert(alpnDefault);
  }
  rv = dtls->SetAlpn(alpn, alpnDefault);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "Couldn't set ALPN");
    return nullptr;
  }

  ice->SetParameters(mIceCtx->GetStream(aTransportId), aIsRtcp ? 2 : 1);
  NS_ENSURE_SUCCESS(ice->Init(), nullptr);
  NS_ENSURE_SUCCESS(dtls->Init(), nullptr);
  NS_ENSURE_SUCCESS(srtp->Init(), nullptr);
  dtls->Chain(ice.get());
  srtp->Chain(ice.get());

  dtls->SignalPacketReceived.connect(this,
                                     &MediaTransportHandlerSTS::PacketReceived);
  srtp->SignalPacketReceived.connect(this,
                                     &MediaTransportHandlerSTS::PacketReceived);
  ice->SignalPacketSending.connect(
      this, &MediaTransportHandlerSTS::EncryptedPacketSending);
  flow->PushLayer(ice.release());
  flow->PushLayer(dtls.release());
  flow->PushLayer(srtp.release());
  return flow;
}

static mozilla::dom::RTCIceGatheringState toDomIceGatheringState(
    NrIceCtx::GatheringState aState) {
  switch (aState) {
    case NrIceCtx::ICE_CTX_GATHER_INIT:
      return dom::RTCIceGatheringState::New;
    case NrIceCtx::ICE_CTX_GATHER_STARTED:
      return dom::RTCIceGatheringState::Gathering;
    case NrIceCtx::ICE_CTX_GATHER_COMPLETE:
      return dom::RTCIceGatheringState::Complete;
  }
  MOZ_CRASH();
}

void MediaTransportHandlerSTS::OnGatheringStateChange(
    NrIceCtx* aIceCtx, NrIceCtx::GatheringState aState) {
  OnGatheringStateChange(toDomIceGatheringState(aState));
}

static mozilla::dom::RTCIceConnectionState toDomIceConnectionState(
    NrIceCtx::ConnectionState aState) {
  switch (aState) {
    case NrIceCtx::ICE_CTX_INIT:
      return dom::RTCIceConnectionState::New;
    case NrIceCtx::ICE_CTX_CHECKING:
      return dom::RTCIceConnectionState::Checking;
    case NrIceCtx::ICE_CTX_CONNECTED:
      return dom::RTCIceConnectionState::Connected;
    case NrIceCtx::ICE_CTX_COMPLETED:
      return dom::RTCIceConnectionState::Completed;
    case NrIceCtx::ICE_CTX_FAILED:
      return dom::RTCIceConnectionState::Failed;
    case NrIceCtx::ICE_CTX_DISCONNECTED:
      return dom::RTCIceConnectionState::Disconnected;
    case NrIceCtx::ICE_CTX_CLOSED:
      return dom::RTCIceConnectionState::Closed;
  }
  MOZ_CRASH();
}

void MediaTransportHandlerSTS::OnConnectionStateChange(
    NrIceCtx* aIceCtx, NrIceCtx::ConnectionState aState) {
  OnConnectionStateChange(toDomIceConnectionState(aState));
}

// The stuff below here will eventually go into the MediaTransportChild class
void MediaTransportHandlerSTS::OnCandidateFound(
    NrIceMediaStream* aStream, const std::string& aCandidate,
    const std::string& aUfrag, const std::string& aMDNSAddr,
    const std::string& aActualAddr) {
  CandidateInfo info;
  info.mCandidate = aCandidate;
  MOZ_ASSERT(!aUfrag.empty());
  info.mUfrag = aUfrag;
  NrIceCandidate defaultRtpCandidate;
  NrIceCandidate defaultRtcpCandidate;
  nsresult rv = aStream->GetDefaultCandidate(1, &defaultRtpCandidate);
  if (NS_SUCCEEDED(rv)) {
    if (!defaultRtpCandidate.mdns_addr.empty()) {
      info.mDefaultHostRtp = "0.0.0.0";
      info.mDefaultPortRtp = 9;
    } else {
      info.mDefaultHostRtp = defaultRtpCandidate.cand_addr.host;
      info.mDefaultPortRtp = defaultRtpCandidate.cand_addr.port;
    }
  } else {
    CSFLogError(LOGTAG,
                "%s: GetDefaultCandidates failed for transport id %s, "
                "res=%u",
                __FUNCTION__, aStream->GetId().c_str(),
                static_cast<unsigned>(rv));
  }

  // Optional; component won't exist if doing rtcp-mux
  if (NS_SUCCEEDED(aStream->GetDefaultCandidate(2, &defaultRtcpCandidate))) {
    if (!defaultRtcpCandidate.mdns_addr.empty()) {
      info.mDefaultHostRtcp = defaultRtcpCandidate.mdns_addr;
    } else {
      info.mDefaultHostRtcp = defaultRtcpCandidate.cand_addr.host;
    }
    info.mDefaultPortRtcp = defaultRtcpCandidate.cand_addr.port;
  }

  info.mMDNSAddress = aMDNSAddr;
  info.mActualAddress = aActualAddr;

  OnCandidate(aStream->GetId(), info);
}

void MediaTransportHandlerSTS::OnStateChange(TransportLayer* aLayer,
                                             TransportLayer::State aState) {
  if (aState == TransportLayer::TS_OPEN) {
    MOZ_ASSERT(aLayer->id() == TransportLayerDtls::ID());
    TransportLayerDtls* dtlsLayer = static_cast<TransportLayerDtls*>(aLayer);
    OnAlpnNegotiated(dtlsLayer->GetNegotiatedAlpn());
  }

  // DTLS state indicates the readiness of the transport as a whole, because
  // SRTP uses the keys from the DTLS handshake.
  MediaTransportHandler::OnStateChange(aLayer->flow_id(), aState);
}

void MediaTransportHandlerSTS::OnRtcpStateChange(TransportLayer* aLayer,
                                                 TransportLayer::State aState) {
  MediaTransportHandler::OnRtcpStateChange(aLayer->flow_id(), aState);
}

constexpr static const char* PacketTypeToString(MediaPacket::Type type) {
  switch (type) {
    case MediaPacket::Type::UNCLASSIFIED:
      return "UNCLASSIFIED";
    case MediaPacket::Type::SRTP:
      return "SRTP";
    case MediaPacket::Type::SRTCP:
      return "SRTCP";
    case MediaPacket::Type::DTLS:
      return "DTLS";
    case MediaPacket::Type::RTP:
      return "RTP";
    case MediaPacket::Type::RTCP:
      return "RTCP";
    case MediaPacket::Type::SCTP:
      return "SCTP";
    default:
      MOZ_ASSERT(false, "unreached");
      return "";
  }
}

void MediaTransportHandlerSTS::PacketReceived(TransportLayer* aLayer,
                                              MediaPacket& aPacket) {
  MEDIA_TRANSPORT_HANDLER_PACKET_RECEIVED(aPacket);
  OnPacketReceived(aLayer->flow_id(), aPacket);
}

void MediaTransportHandlerSTS::EncryptedPacketSending(TransportLayer* aLayer,
                                                      MediaPacket& aPacket) {
  OnEncryptedSending(aLayer->flow_id(), aPacket);
}

}  // namespace mozilla

#undef MEDIA_TRANSPORT_HANDLER_PACKET_RECEIVED
