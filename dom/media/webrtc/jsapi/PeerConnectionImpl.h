/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _PEER_CONNECTION_IMPL_H_
#define _PEER_CONNECTION_IMPL_H_

#include <string>
#include <vector>
#include <map>
#include <cmath>

#include "prlock.h"
#include "mozilla/RefPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIUUIDGenerator.h"
#include "nsIThread.h"
#include "mozilla/Mutex.h"

// Work around nasty macro in webrtc/voice_engine/voice_engine_defines.h
#ifdef GetLastError
#  undef GetLastError
#endif

#include "jsep/JsepSession.h"
#include "jsep/JsepSessionImpl.h"
#include "sdp/SdpMediaSection.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/RTCPeerConnectionBinding.h"  // mozPacketDumpType, maybe move?
#include "mozilla/dom/RTCRtpTransceiverBinding.h"
#include "mozilla/dom/RTCConfigurationBinding.h"
#include "PrincipalChangeObserver.h"

#include "mozilla/TimeStamp.h"
#include "mozilla/net/DataChannel.h"
#include "VideoUtils.h"
#include "VideoSegment.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "mozilla/PeerIdentity.h"
#include "RTCStatsIdGenerator.h"
#include "RTCStatsReport.h"

namespace test {
#ifdef USE_FAKE_PCOBSERVER
class AFakePCObserver;
#endif
}  // namespace test

class nsDOMDataChannel;

namespace mozilla {
struct CandidateInfo;
class DataChannel;
class DtlsIdentity;
class MediaPipeline;
class MediaPipelineReceive;
class MediaPipelineTransmit;
class TransceiverImpl;

namespace dom {
class RTCCertificate;
struct RTCConfiguration;
struct RTCRtpSourceEntry;
struct RTCIceServer;
struct RTCOfferOptions;
struct RTCRtpParameters;
class RTCRtpSender;
class MediaStreamTrack;

#ifdef USE_FAKE_PCOBSERVER
typedef test::AFakePCObserver PeerConnectionObserver;
typedef const char* PCObserverString;
#else
class PeerConnectionObserver;
typedef NS_ConvertUTF8toUTF16 PCObserverString;
#endif
}  // namespace dom
}  // namespace mozilla

#if defined(__cplusplus) && __cplusplus >= 201103L
typedef struct Timecard Timecard;
#else
#  include "common/time_profiling/timecard.h"
#endif

// To preserve blame, convert nsresult to ErrorResult with wrappers. These
// macros help declare wrappers w/function being wrapped when there are no
// differences.

#define NS_IMETHODIMP_TO_ERRORRESULT(func, rv, ...) \
  NS_IMETHODIMP func(__VA_ARGS__);                  \
  void func(__VA_ARGS__, rv)

#define NS_IMETHODIMP_TO_ERRORRESULT_RETREF(resulttype, func, rv, ...) \
  NS_IMETHODIMP func(__VA_ARGS__, resulttype** result);                \
  already_AddRefed<resulttype> func(__VA_ARGS__, rv)

namespace mozilla {

using mozilla::DtlsIdentity;
using mozilla::ErrorResult;
using mozilla::PeerIdentity;
using mozilla::dom::PeerConnectionObserver;
using mozilla::dom::RTCConfiguration;
using mozilla::dom::RTCIceServer;
using mozilla::dom::RTCOfferOptions;

class PeerConnectionWrapper;
class PeerConnectionMedia;
class RemoteSourceStreamInfo;

// Uuid Generator
class PCUuidGenerator : public mozilla::JsepUuidGenerator {
 public:
  virtual bool Generate(std::string* idp) override;

 private:
  nsCOMPtr<nsIUUIDGenerator> mGenerator;
};

// This is a variation of Telemetry::AutoTimer that keeps a reference
// count and records the elapsed time when the count falls to zero. The
// elapsed time is recorded in seconds.
struct PeerConnectionAutoTimer {
  PeerConnectionAutoTimer()
      : mRefCnt(0), mStart(TimeStamp::Now()), mUsedAV(false){};
  void RegisterConnection();
  void UnregisterConnection(bool aContainedAV);
  bool IsStopped();

 private:
  int64_t mRefCnt;
  TimeStamp mStart;
  bool mUsedAV;
};

// Enter an API call and check that the state is OK,
// the PC isn't closed, etc.
#define PC_AUTO_ENTER_API_CALL(assert_ice_ready)             \
  do {                                                       \
    /* do/while prevents res from conflicting with locals */ \
    nsresult res = CheckApiState(assert_ice_ready);          \
    if (NS_FAILED(res)) return res;                          \
  } while (0)
#define PC_AUTO_ENTER_API_CALL_VOID_RETURN(assert_ice_ready) \
  do {                                                       \
    /* do/while prevents res from conflicting with locals */ \
    nsresult res = CheckApiState(assert_ice_ready);          \
    if (NS_FAILED(res)) return;                              \
  } while (0)
#define PC_AUTO_ENTER_API_CALL_NO_CHECK() CheckThread()

class PeerConnectionImpl final
    : public nsISupports,
      public mozilla::DataChannelConnection::DataConnectionListener,
      public dom::PrincipalChangeObserver<dom::MediaStreamTrack> {
  struct Internal;  // Avoid exposing c includes to bindings

 public:
  explicit PeerConnectionImpl(
      const mozilla::dom::GlobalObject* aGlobal = nullptr);

  NS_DECL_THREADSAFE_ISUPPORTS

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aReflector);

  static already_AddRefed<PeerConnectionImpl> Constructor(
      const mozilla::dom::GlobalObject& aGlobal);

  // DataConnection observers
  void NotifyDataChannel(already_AddRefed<mozilla::DataChannel> aChannel)
      // PeerConnectionImpl only inherits from mozilla::DataChannelConnection
      // inside libxul.
      override;

  const RefPtr<MediaTransportHandler> GetTransportHandler() const;

  // Get the media object
  const RefPtr<PeerConnectionMedia>& media() const {
    PC_AUTO_ENTER_API_CALL_NO_CHECK();
    return mMedia;
  }

  // Handle system to allow weak references to be passed through C code
  virtual const std::string& GetHandle();

  // Name suitable for exposing to content
  virtual const std::string& GetName();

  // ICE events
  void IceConnectionStateChange(dom::RTCIceConnectionState state);
  void IceGatheringStateChange(dom::RTCIceGatheringState state);
  void OnCandidateFound(const std::string& aTransportId,
                        const CandidateInfo& aCandidateInfo);
  void UpdateDefaultCandidate(const std::string& defaultAddr,
                              uint16_t defaultPort,
                              const std::string& defaultRtcpAddr,
                              uint16_t defaultRtcpPort,
                              const std::string& transportId);

  static void ListenThread(void* aData);
  static void ConnectThread(void* aData);

  // Get the main thread
  nsCOMPtr<nsIThread> GetMainThread() { return mThread; }

  // Get the STS thread
  nsISerialEventTarget* GetSTSThread() {
    PC_AUTO_ENTER_API_CALL_NO_CHECK();
    return mSTSThread;
  }

  nsPIDOMWindowInner* GetWindow() const {
    PC_AUTO_ENTER_API_CALL_NO_CHECK();
    return mWindow;
  }

  nsresult Initialize(PeerConnectionObserver& aObserver,
                      nsGlobalWindowInner* aWindow,
                      const RTCConfiguration& aConfiguration,
                      nsISupports* aThread);

  // Initialize PeerConnection from an RTCConfiguration object (JS entrypoint)
  void Initialize(PeerConnectionObserver& aObserver,
                  nsGlobalWindowInner& aWindow,
                  const RTCConfiguration& aConfiguration, nsISupports* aThread,
                  ErrorResult& rv);

  void SetCertificate(mozilla::dom::RTCCertificate& aCertificate);
  const RefPtr<mozilla::dom::RTCCertificate>& Certificate() const;
  // This is a hack to support external linkage.
  RefPtr<DtlsIdentity> Identity() const;

  NS_IMETHODIMP_TO_ERRORRESULT(CreateOffer, ErrorResult& rv,
                               const RTCOfferOptions& aOptions) {
    rv = CreateOffer(aOptions);
  }

  NS_IMETHODIMP CreateAnswer();
  void CreateAnswer(ErrorResult& rv) { rv = CreateAnswer(); }

  NS_IMETHODIMP CreateOffer(const mozilla::JsepOfferOptions& aConstraints);

  NS_IMETHODIMP SetLocalDescription(int32_t aAction, const char* aSDP);

  void SetLocalDescription(int32_t aAction, const nsAString& aSDP,
                           ErrorResult& rv) {
    rv = SetLocalDescription(aAction, NS_ConvertUTF16toUTF8(aSDP).get());
  }

  NS_IMETHODIMP SetRemoteDescription(int32_t aAction, const char* aSDP);

  void SetRemoteDescription(int32_t aAction, const nsAString& aSDP,
                            ErrorResult& rv) {
    rv = SetRemoteDescription(aAction, NS_ConvertUTF16toUTF8(aSDP).get());
  }

  already_AddRefed<dom::Promise> GetStats(dom::MediaStreamTrack* aSelector);

  void GetRemoteStreams(nsTArray<RefPtr<DOMMediaStream>>& aStreamsOut) const;

  NS_IMETHODIMP AddIceCandidate(const char* aCandidate, const char* aMid,
                                const char* aUfrag,
                                const dom::Nullable<unsigned short>& aLevel);

  void AddIceCandidate(const nsAString& aCandidate, const nsAString& aMid,
                       const nsAString& aUfrag,
                       const dom::Nullable<unsigned short>& aLevel,
                       ErrorResult& rv) {
    rv = AddIceCandidate(NS_ConvertUTF16toUTF8(aCandidate).get(),
                         NS_ConvertUTF16toUTF8(aMid).get(),
                         NS_ConvertUTF16toUTF8(aUfrag).get(), aLevel);
  }

  void UpdateNetworkState(bool online);

  NS_IMETHODIMP CloseStreams();

  void CloseStreams(ErrorResult& rv) { rv = CloseStreams(); }

  already_AddRefed<TransceiverImpl> CreateTransceiverImpl(
      const nsAString& aKind, dom::MediaStreamTrack* aSendTrack,
      ErrorResult& rv);

  bool CheckNegotiationNeeded(ErrorResult& rv);

  NS_IMETHODIMP_TO_ERRORRESULT(ReplaceTrackNoRenegotiation, ErrorResult& rv,
                               TransceiverImpl& aTransceiver,
                               mozilla::dom::MediaStreamTrack* aWithTrack) {
    rv = ReplaceTrackNoRenegotiation(aTransceiver, aWithTrack);
  }

  // test-only
  NS_IMETHODIMP_TO_ERRORRESULT(EnablePacketDump, ErrorResult& rv,
                               unsigned long level, dom::mozPacketDumpType type,
                               bool sending) {
    rv = EnablePacketDump(level, type, sending);
  }

  // test-only
  NS_IMETHODIMP_TO_ERRORRESULT(DisablePacketDump, ErrorResult& rv,
                               unsigned long level, dom::mozPacketDumpType type,
                               bool sending) {
    rv = DisablePacketDump(level, type, sending);
  }

  void GetPeerIdentity(nsAString& peerIdentity) {
    if (mPeerIdentity) {
      peerIdentity = mPeerIdentity->ToString();
      return;
    }

    peerIdentity.SetIsVoid(true);
  }

  const PeerIdentity* GetPeerIdentity() const { return mPeerIdentity; }
  NS_IMETHODIMP_TO_ERRORRESULT(SetPeerIdentity, ErrorResult& rv,
                               const nsAString& peerIdentity) {
    rv = SetPeerIdentity(peerIdentity);
  }

  const std::string& GetIdAsAscii() const { return mName; }

  void GetId(nsAString& id) { id = NS_ConvertASCIItoUTF16(mName.c_str()); }

  void SetId(const nsAString& id) { mName = NS_ConvertUTF16toUTF8(id).get(); }

  // this method checks to see if we've made a promise to protect media.
  bool PrivacyRequested() const {
    return mPrivacyRequested.isSome() && *mPrivacyRequested;
  }

  bool PrivacyNeeded() const {
    return mPrivacyRequested.isSome() && *mPrivacyRequested;
  }

  NS_IMETHODIMP GetFingerprint(char** fingerprint);
  void GetFingerprint(nsAString& fingerprint) {
    char* tmp;
    nsresult rv = GetFingerprint(&tmp);
    NS_ENSURE_SUCCESS_VOID(rv);
    fingerprint.AssignASCII(tmp);
    delete[] tmp;
  }

  void GetCurrentLocalDescription(nsAString& aSDP) const;
  void GetPendingLocalDescription(nsAString& aSDP) const;

  void GetCurrentRemoteDescription(nsAString& aSDP) const;
  void GetPendingRemoteDescription(nsAString& aSDP) const;

  dom::Nullable<bool> GetCurrentOfferer() const;
  dom::Nullable<bool> GetPendingOfferer() const;

  NS_IMETHODIMP SignalingState(mozilla::dom::RTCSignalingState* aState);

  mozilla::dom::RTCSignalingState SignalingState() {
    mozilla::dom::RTCSignalingState state;
    SignalingState(&state);
    return state;
  }

  NS_IMETHODIMP IceConnectionState(mozilla::dom::RTCIceConnectionState* aState);

  mozilla::dom::RTCIceConnectionState IceConnectionState() {
    mozilla::dom::RTCIceConnectionState state;
    IceConnectionState(&state);
    return state;
  }

  NS_IMETHODIMP IceGatheringState(mozilla::dom::RTCIceGatheringState* aState);

  mozilla::dom::RTCIceGatheringState IceGatheringState() {
    return mIceGatheringState;
  }

  NS_IMETHODIMP Close();

  void Close(ErrorResult& rv) { rv = Close(); }

  bool PluginCrash(uint32_t aPluginID, const nsAString& aPluginName);

  void RecordEndOfCallTelemetry();

  nsresult InitializeDataChannel();

  NS_IMETHODIMP_TO_ERRORRESULT_RETREF(nsDOMDataChannel, CreateDataChannel,
                                      ErrorResult& rv, const nsAString& aLabel,
                                      const nsAString& aProtocol,
                                      uint16_t aType, bool outOfOrderAllowed,
                                      uint16_t aMaxTime, uint16_t aMaxNum,
                                      bool aExternalNegotiated,
                                      uint16_t aStream);

  // Gets the RTC Signaling State of the JSEP session
  dom::RTCSignalingState GetSignalingState() const;

  void OnSetDescriptionSuccess(JsepSdpType sdpType, bool remote);

  bool IsClosed() const;
  // called when DTLS connects; we only need this once
  nsresult OnAlpnNegotiated(bool aPrivacyRequested);

  bool HasMedia() const;

  // initialize telemetry for when calls start
  void StartCallTelem();

  RefPtr<dom::RTCStatsReportPromise> GetStats(dom::MediaStreamTrack* aSelector,
                                              bool aInternalStats);

  void CollectConduitTelemetryData();

  // for monitoring changes in track ownership
  // PeerConnectionMedia can't do it because it doesn't know about principals
  virtual void PrincipalChanged(dom::MediaStreamTrack* aTrack) override;

  void OnMediaError(const std::string& aError);

  bool ShouldDumpPacket(size_t level, dom::mozPacketDumpType type,
                        bool sending) const;

  void DumpPacket_m(size_t level, dom::mozPacketDumpType type, bool sending,
                    UniquePtr<uint8_t[]>& packet, size_t size);

  const dom::RTCStatsTimestampMaker& GetTimestampMaker() const {
    return mTimestampMaker;
  }

  // Utility function, given a string pref and an URI, returns whether or not
  // the URI occurs in the pref. Wildcards are supported (e.g. *.example.com)
  // and multiple hostnames can be present, separated by commas.
  static bool HostnameInPref(const char* aPrefList, nsIURI* aDocURI);

  void StampTimecard(const char* aEvent);

  bool RelayOnly() const {
    return mJsConfiguration.mIceTransportPolicy.WasPassed() &&
           mJsConfiguration.mIceTransportPolicy.Value() ==
               dom::RTCIceTransportPolicy::Relay;
  }

 private:
  virtual ~PeerConnectionImpl();
  PeerConnectionImpl(const PeerConnectionImpl& rhs);
  PeerConnectionImpl& operator=(PeerConnectionImpl);

  nsTArray<RefPtr<dom::RTCStatsPromise>> GetSenderStats(
      const RefPtr<MediaPipelineTransmit>& aPipeline);
  RefPtr<dom::RTCStatsPromise> GetDataChannelStats(
      const RefPtr<DataChannelConnection>& aDataChannelConnection,
      const DOMHighResTimeStamp aTimestamp);
  nsresult CalculateFingerprint(const std::string& algorithm,
                                std::vector<uint8_t>* fingerprint) const;
  nsresult ConfigureJsepSessionCodecs();

  NS_IMETHODIMP EnsureDataConnection(uint16_t aLocalPort, uint16_t aNumstreams,
                                     uint32_t aMaxMessageSize, bool aMMSSet);

  nsresult CloseInt();
  nsresult CheckApiState(bool assert_ice_ready) const;
  void CheckThread() const { MOZ_ASSERT(CheckThreadInt(), "Wrong thread"); }
  bool CheckThreadInt() const {
    bool on;
    NS_ENSURE_SUCCESS(mThread->IsOnCurrentThread(&on), false);
    NS_ENSURE_TRUE(on, false);
    return true;
  }

  // test-only: called from AddRIDExtension and AddRIDFilter
  // for simulcast mochitests.
  RefPtr<MediaPipeline> GetMediaPipelineForTrack(
      dom::MediaStreamTrack& aRecvTrack);

  // Shut down media - called on main thread only
  void ShutdownMedia();

  void CandidateReady(const std::string& candidate,
                      const std::string& transportId, const std::string& ufrag);
  void SendLocalIceCandidateToContent(uint16_t level, const std::string& mid,
                                      const std::string& candidate,
                                      const std::string& ufrag);

  nsresult GetDatachannelParameters(uint32_t* channels, uint16_t* localport,
                                    uint16_t* remoteport,
                                    uint32_t* maxmessagesize, bool* mmsset,
                                    std::string* transportId,
                                    bool* client) const;

  nsresult AddRtpTransceiverToJsepSession(RefPtr<JsepTransceiver>& transceiver);
  already_AddRefed<TransceiverImpl> CreateTransceiverImpl(
      JsepTransceiver* aJsepTransceiver, dom::MediaStreamTrack* aSendTrack,
      ErrorResult& aRv);

  // When ICE completes, we record a bunch of statistics that outlive the
  // PeerConnection. This is just telemetry right now, but this can also
  // include things like dumping the RLogConnector somewhere, saving away
  // an RTCStatsReport somewhere so it can be inspected after the call is over,
  // or other things.
  void RecordLongtermICEStatistics();

  void RecordIceRestartStatistics(JsepSdpType type);

  void StoreConfigurationForAboutWebrtc(const RTCConfiguration& aConfig);

  dom::Sequence<dom::RTCSdpParsingErrorInternal> GetLastSdpParsingErrors()
      const;
  // Timecard used to measure processing time. This should be the first class
  // attribute so that we accurately measure the time required to instantiate
  // any other attributes of this class.
  Timecard* mTimeCard;

  // Configuration used to initialize the PeerConnection
  dom::RTCConfigurationInternal mJsConfiguration;

  mozilla::dom::RTCSignalingState mSignalingState;

  // ICE State
  mozilla::dom::RTCIceConnectionState mIceConnectionState;
  mozilla::dom::RTCIceGatheringState mIceGatheringState;

  nsCOMPtr<nsIThread> mThread;
  RefPtr<PeerConnectionObserver> mPCObserver;

  nsCOMPtr<nsPIDOMWindowInner> mWindow;

  // The SDP sent in from JS
  std::string mLocalRequestedSDP;
  std::string mRemoteRequestedSDP;
  // Only accessed from main
  mozilla::dom::Sequence<mozilla::dom::RTCSdpHistoryEntryInternal> mSdpHistory;
  std::string mPendingLocalDescription;
  std::string mPendingRemoteDescription;
  std::string mCurrentLocalDescription;
  std::string mCurrentRemoteDescription;
  Maybe<bool> mPendingOfferer;
  Maybe<bool> mCurrentOfferer;

  // DTLS fingerprint
  std::string mFingerprint;
  std::string mRemoteFingerprint;

  // identity-related fields
  // The entity on the other end of the peer-to-peer connection;
  // void if they are not yet identified, and no identity setting has been set
  RefPtr<PeerIdentity> mPeerIdentity;
  // The certificate we are using.
  RefPtr<mozilla::dom::RTCCertificate> mCertificate;
  // Whether an app should be prevented from accessing media produced by the PC
  // If this is true, then media will not be sent until mPeerIdentity matches
  // local streams PeerIdentity; and remote streams are protected from content
  //
  // This can be false if mPeerIdentity is set, in the case where identity is
  // provided, but the media is not protected from the app on either side
  Maybe<bool> mPrivacyRequested;

  // A handle to refer to this PC with
  std::string mHandle;

  // A name for this PC that we are willing to expose to content.
  std::string mName;

  // The target to run stuff on
  nsCOMPtr<nsISerialEventTarget> mSTSThread;

  // DataConnection that's used to get all the DataChannels
  RefPtr<mozilla::DataChannelConnection> mDataConnection;

  bool mForceIceTcp;
  RefPtr<PeerConnectionMedia> mMedia;
  RefPtr<MediaTransportHandler> mTransportHandler;

  // The JSEP negotiation session.
  mozilla::UniquePtr<PCUuidGenerator> mUuidGen;
  mozilla::UniquePtr<mozilla::JsepSession> mJsepSession;
  unsigned long mIceRestartCount;
  unsigned long mIceRollbackCount;

  // The following are used for Telemetry:
  bool mCallTelemStarted = false;
  bool mCallTelemEnded = false;

  // Start time of ICE.
  mozilla::TimeStamp mIceStartTime;
  // Hold PeerConnectionAutoTimer instances for each window.
  static std::map<uint64_t, PeerConnectionAutoTimer> sCallDurationTimers;

  bool mHaveConfiguredCodecs;

  bool mTrickle;

  bool mPrivateWindow;

  // Whether this PeerConnection is being counted as active by mWindow
  bool mActiveOnWindow;

  // storage for Telemetry data
  uint16_t mMaxReceiving[SdpMediaSection::kMediaTypes];
  uint16_t mMaxSending[SdpMediaSection::kMediaTypes];

  std::vector<unsigned> mSendPacketDumpFlags;
  std::vector<unsigned> mRecvPacketDumpFlags;
  Atomic<bool> mPacketDumpEnabled;
  mutable Mutex mPacketDumpFlagsMutex;

  // used to store the raw trickle candidate string for display
  // on the about:webrtc raw candidates table.
  std::vector<std::string> mRawTrickledCandidates;

  dom::RTCStatsTimestampMaker mTimestampMaker;

  RefPtr<RTCStatsIdGenerator> mIdGenerator;
  // Ordinarily, I would use a std::map here, but this used to be a JS Map
  // which iterates in insertion order, and I want to avoid changing this.
  nsTArray<RefPtr<DOMMediaStream>> mReceiveStreams;

  DOMMediaStream* GetReceiveStream(const std::string& aId) const;
  DOMMediaStream* CreateReceiveStream(const std::string& aId);

  // See Bug 1642419, this can be removed when all sites are working with RTX.
  bool mRtxIsAllowed = true;

 public:
  // these are temporary until the DataChannel Listen/Connect API is removed
  unsigned short listenPort;
  unsigned short connectPort;
  char* connectStr;  // XXX ownership/free
};

// This is what is returned when you acquire on a handle
class PeerConnectionWrapper {
 public:
  explicit PeerConnectionWrapper(const std::string& handle);

  PeerConnectionImpl* impl() { return impl_; }

 private:
  RefPtr<PeerConnectionImpl> impl_;
};

}  // namespace mozilla

#undef NS_IMETHODIMP_TO_ERRORRESULT
#undef NS_IMETHODIMP_TO_ERRORRESULT_RETREF
#endif  // _PEER_CONNECTION_IMPL_H_
