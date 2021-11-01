/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _PEER_CONNECTION_MEDIA_H_
#define _PEER_CONNECTION_MEDIA_H_

#include <string>
#include <vector>
#include <map>

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/net/StunAddrsRequestChild.h"
#include "MediaTransportHandler.h"
#include "nsIHttpChannelInternal.h"
#include "RTCDtlsTransport.h"

#include "TransceiverImpl.h"

class nsIPrincipal;

namespace mozilla {
class DataChannel;
class PeerIdentity;
namespace dom {
class MediaStreamTrack;
}  // namespace dom
}  // namespace mozilla

#include "transport/nriceresolver.h"
#include "transport/nricemediastream.h"

namespace mozilla {

class PeerConnectionImpl;
class PeerConnectionMedia;
class PCUuidGenerator;
class MediaPipeline;
class MediaPipelineReceive;
class MediaPipelineTransmit;
class MediaPipelineFilter;
class JsepSession;

// TODO(bug 1402997): If we move the TransceiverImpl stuff out of here, this
// will be a class that handles just the transport stuff, and we can rename it
// to something more explanatory (say, PeerConnectionTransportManager).
class PeerConnectionMedia : public sigslot::has_slots<> {
  ~PeerConnectionMedia();

 public:
  explicit PeerConnectionMedia(PeerConnectionImpl* parent);

  nsresult Init();
  void Shutdown();

  // Ensure ICE transports exist that we might need when offer/answer concludes
  void EnsureTransports(const JsepSession& aSession);

  void UpdateRTCDtlsTransports(bool aMarkAsStable);
  void RollbackRTCDtlsTransports();
  void RemoveRTCDtlsTransportsExcept(
      const std::set<std::string>& aTransportIds);

  // Activate ICE transports at the conclusion of offer/answer,
  // or when rollback occurs.
  nsresult UpdateTransports(const JsepSession& aSession,
                            const bool forceIceTcp);

  void ResetStunAddrsForIceRestart() { mStunAddrs.Clear(); }

  // Start ICE checks.
  void StartIceChecks(const JsepSession& session);

  // Process a trickle ICE candidate.
  void AddIceCandidate(const std::string& candidate,
                       const std::string& aTransportId,
                       const std::string& aUFrag);

  // Handle notifications of network online/offline events.
  void UpdateNetworkState(bool online);

  // Handle complete media pipelines.
  // This updates codec parameters, starts/stops send/receive, and other
  // stuff that doesn't necessarily require negotiation. This can be called at
  // any time, not just when an offer/answer exchange completes.
  // TODO: Let's move this to PeerConnectionImpl
  nsresult UpdateMediaPipelines();

  // TODO: Let's move the TransceiverImpl stuff to PeerConnectionImpl.
  nsresult AddTransceiver(JsepTransceiver* aJsepTransceiver,
                          dom::MediaStreamTrack* aSendTrack,
                          SharedWebrtcState* aSharedWebrtcState,
                          RefPtr<TransceiverImpl>* aTransceiverImpl);

  void GetTransmitPipelinesMatching(
      const dom::MediaStreamTrack* aTrack,
      nsTArray<RefPtr<MediaPipelineTransmit>>* aPipelines);

  std::string GetTransportIdMatchingSendTrack(
      const dom::MediaStreamTrack& aTrack) const;

  // In cases where the peer isn't yet identified, we disable the pipeline (not
  // the stream, that would potentially affect others), so that it sends
  // black/silence.  Once the peer is identified, re-enable those streams.
  // aTrack will be set if this update came from a principal change on aTrack.
  // TODO: Move to PeerConnectionImpl
  void UpdateSinkIdentity_m(const dom::MediaStreamTrack* aTrack,
                            nsIPrincipal* aPrincipal,
                            const PeerIdentity* aSinkIdentity);
  // this determines if any track is peerIdentity constrained
  bool AnyLocalTrackHasPeerIdentity() const;

  bool AnyCodecHasPluginID(uint64_t aPluginID);

  const nsCOMPtr<nsIThread>& GetMainThread() const { return mMainThread; }
  const nsCOMPtr<nsISerialEventTarget>& GetSTSThread() const {
    return mSTSThread;
  }

  // Used by PCImpl in a couple of places. Might be good to move that code in
  // here.
  std::vector<RefPtr<TransceiverImpl>>& GetTransceivers() {
    return mTransceivers;
  }

  nsPIDOMWindowInner* GetWindow() const;
  already_AddRefed<nsIHttpChannelInternal> GetChannel() const;

  void AlpnNegotiated_s(const std::string& aAlpn, bool aPrivacyRequested);
  void AlpnNegotiated_m(bool aPrivacyRequested);

  // TODO: Move to PeerConnectionImpl
  RefPtr<WebrtcCallWrapper> mCall;

  // mtransport objects
  RefPtr<MediaTransportHandler> mTransportHandler;

 private:
  void InitLocalAddrs();  // for stun local address IPC request
  bool ShouldForceProxy() const;
  std::unique_ptr<NrSocketProxyConfig> GetProxyConfig() const;

  class StunAddrsHandler : public net::StunAddrsListener {
   public:
    explicit StunAddrsHandler(PeerConnectionMedia* pcm) : pcm_(pcm) {}

    void OnMDNSQueryComplete(const nsCString& hostname,
                             const Maybe<nsCString>& address) override;

    void OnStunAddrsAvailable(
        const mozilla::net::NrIceStunAddrArray& addrs) override;

   private:
    RefPtr<PeerConnectionMedia> pcm_;
    virtual ~StunAddrsHandler() {}
  };

  // Manage ICE transports.
  void UpdateTransport(const JsepTransceiver& aTransceiver, bool aForceIceTcp);

  void GatherIfReady();
  void FlushIceCtxOperationQueueIfReady();
  void PerformOrEnqueueIceCtxOperation(nsIRunnable* runnable);
  nsresult SetTargetForDefaultLocalAddressLookup();
  void EnsureIceGathering(bool aDefaultRouteOnly, bool aObfuscateHostAddresses);

  bool GetPrefDefaultAddressOnly() const;
  bool GetPrefObfuscateHostAddresses() const;

  void ConnectSignals();

  // ICE events
  void IceGatheringStateChange_s(dom::RTCIceGatheringState aState);
  void IceConnectionStateChange_s(dom::RTCIceConnectionState aState);
  void OnCandidateFound_s(const std::string& aTransportId,
                          const CandidateInfo& aCandidateInfo);

  void IceGatheringStateChange_m(dom::RTCIceGatheringState aState);
  void IceConnectionStateChange_m(dom::RTCIceConnectionState aState);
  void OnCandidateFound_m(const std::string& aTransportId,
                          const CandidateInfo& aCandidateInfo);

  bool IsIceCtxReady() const {
    return mLocalAddrsRequestState == STUN_ADDR_REQUEST_COMPLETE;
  }

  // The parent PC
  PeerConnectionImpl* mParent;
  // and a loose handle on it for event driven stuff
  std::string mParentHandle;
  std::string mParentName;

  std::vector<RefPtr<TransceiverImpl>> mTransceivers;
  std::map<std::string, RefPtr<dom::RTCDtlsTransport>>
      mTransportIdToRTCDtlsTransport;

  // The main thread.
  nsCOMPtr<nsIThread> mMainThread;

  // The STS thread.
  nsCOMPtr<nsISerialEventTarget> mSTSThread;

  // Used whenever we need to dispatch a runnable to STS to tweak something
  // on our ICE ctx, but are not ready to do so at the moment (eg; we are
  // waiting to get a callback with our http proxy config before we start
  // gathering or start checking)
  std::vector<nsCOMPtr<nsIRunnable>> mQueuedIceCtxOperations;

  // Set if prefs dictate that we should force the use of a web proxy.
  bool mForceProxy;

  // Used to cancel incoming stun addrs response
  RefPtr<net::StunAddrsRequestChild> mStunAddrsRequest;

  enum StunAddrRequestState {
    STUN_ADDR_REQUEST_NONE,
    STUN_ADDR_REQUEST_PENDING,
    STUN_ADDR_REQUEST_COMPLETE
  };
  // Used to track the state of the stun addr IPC request
  StunAddrRequestState mLocalAddrsRequestState;

  // Used to store the result of the stun addr IPC request
  nsTArray<NrIceStunAddr> mStunAddrs;

  // Used to ensure the target for default local address lookup is only set
  // once.
  bool mTargetForDefaultLocalAddressLookupIsSet;

  // Set to true when the object is going to be released.
  bool mDestroyed;

  // Keep track of local hostnames to register. Registration is deferred
  // until StartIceChecks has run. Accessed on main thread only.
  std::map<std::string, std::string> mMDNSHostnamesToRegister;
  bool mCanRegisterMDNSHostnamesDirectly = false;

  // Used to store the mDNS hostnames that we have registered
  std::set<std::string> mRegisteredMDNSHostnames;

  // Used to store the mDNS hostnames that we have queried
  struct PendingIceCandidate {
    std::vector<std::string> mTokenizedCandidate;
    std::string mTransportId;
    std::string mUfrag;
  };
  std::map<std::string, std::list<PendingIceCandidate>> mQueriedMDNSHostnames;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PeerConnectionMedia)
};

}  // namespace mozilla

#endif
