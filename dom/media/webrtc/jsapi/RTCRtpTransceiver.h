/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _TRANSCEIVERIMPL_H_
#define _TRANSCEIVERIMPL_H_

#include <string>
#include "mozilla/StateMirroring.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsISerialEventTarget.h"
#include "nsTArray.h"
#include "mozilla/dom/MediaStreamTrack.h"
#include "ErrorList.h"
#include "jsep/JsepSession.h"
#include "transport/transportlayer.h"  // For TransportLayer::State
#include "mozilla/dom/RTCRtpTransceiverBinding.h"
#include "RTCStatsReport.h"

class nsIPrincipal;

namespace mozilla {
class PeerIdentity;
class MediaSessionConduit;
class VideoSessionConduit;
class AudioSessionConduit;
struct AudioCodecConfig;
class VideoCodecConfig;  // Why is this a class, but AudioCodecConfig a struct?
class MediaPipelineTransmit;
class MediaPipeline;
class MediaPipelineFilter;
class MediaTransportHandler;
class RTCStatsIdGenerator;
class WebrtcCallWrapper;
class JsepTrackNegotiatedDetails;
class PeerConnectionImpl;
enum class PrincipalPrivacy : uint8_t;

namespace dom {
class RTCDtlsTransport;
class RTCDTMFSender;
class RTCRtpTransceiver;
struct RTCRtpSourceEntry;
class RTCRtpReceiver;
class RTCRtpSender;

/**
 * This is what ties all the various pieces that make up a transceiver
 * together. This includes:
 * MediaStreamTrack for rendering and capture
 * MediaTransportHandler for RTP transmission/reception
 * Audio/VideoConduit for feeding RTP/RTCP into webrtc.org for decoding, and
 * feeding audio/video frames into webrtc.org for encoding into RTP/RTCP.
 */
class RTCRtpTransceiver : public nsISupports, public nsWrapperCache {
 public:
  /**
   * |aSendTrack| might or might not be set.
   */
  RTCRtpTransceiver(
      nsPIDOMWindowInner* aWindow, bool aPrivacyNeeded, PeerConnectionImpl* aPc,
      MediaTransportHandler* aTransportHandler, JsepSession* aJsepSession,
      const std::string& aTransceiverId, bool aIsVideo,
      nsISerialEventTarget* aStsThread, MediaStreamTrack* aSendTrack,
      WebrtcCallWrapper* aCallWrapper, RTCStatsIdGenerator* aIdGenerator);

  void Init(const RTCRtpTransceiverInit& aInit, ErrorResult& aRv);

  bool IsValid() const { return !!mConduit; }

  nsresult UpdateTransport();

  nsresult UpdateConduit();

  void UpdatePrincipalPrivacy(PrincipalPrivacy aPrivacy);

  void ResetSync();

  nsresult SyncWithMatchingVideoConduits(
      nsTArray<RefPtr<RTCRtpTransceiver>>& transceivers);

  void Close();

  void BreakCycles();
  void Unlink();

  bool ConduitHasPluginID(uint64_t aPluginID);

  // for webidl
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  nsPIDOMWindowInner* GetParentObject() const;
  RTCRtpReceiver* Receiver() const { return mReceiver; }
  RTCRtpSender* Sender() const { return mSender; }
  RTCDtlsTransport* GetDtlsTransport() const { return mDtlsTransport; }
  void GetKind(nsAString& aKind) const;
  void GetMid(nsAString& aMid) const;
  RTCRtpTransceiverDirection Direction() const {
    if (mStopping) {
      return RTCRtpTransceiverDirection::Stopped;
    }
    return mDirection;
  }
  void SetDirection(RTCRtpTransceiverDirection aDirection, ErrorResult& aRv);
  Nullable<RTCRtpTransceiverDirection> GetCurrentDirection() {
    if (mStopped) {
      return RTCRtpTransceiverDirection::Stopped;
    }
    return mCurrentDirection;
  }
  void Stop(ErrorResult& aRv);
  void SetDirectionInternal(RTCRtpTransceiverDirection aDirection);
  bool HasBeenUsedToSend() const { return mHasBeenUsedToSend; }

  bool CanSendDTMF() const;
  bool Stopped() const { return mStopped; }
  bool Stopping() const { return mStopping; }
  void SyncToJsep(JsepSession& aSession) const;
  void SyncFromJsep(const JsepSession& aSession);
  std::string GetMidAscii() const;

  void SetDtlsTransport(RTCDtlsTransport* aDtlsTransport, bool aStable);
  void RollbackToStableDtlsTransport();

  std::string GetTransportId() const {
    return mJsepTransceiver.mTransport.mTransportId;
  }

  JsepTransceiver& GetJsepTransceiver() { return mJsepTransceiver; }

  bool IsVideo() const;

  bool IsSending() const;

  bool IsReceiving() const;

  bool ShouldRemove() const;

  Maybe<const std::vector<UniquePtr<JsepCodecDescription>>&>
  GetNegotiatedSendCodecs() const;

  Maybe<const std::vector<UniquePtr<JsepCodecDescription>>&>
  GetNegotiatedRecvCodecs() const;

  struct PayloadTypes {
    Maybe<int> mSendPayloadType;
    Maybe<int> mRecvPayloadType;
  };
  using ActivePayloadTypesPromise = MozPromise<PayloadTypes, nsresult, true>;
  RefPtr<ActivePayloadTypesPromise> GetActivePayloadTypes() const;

  MediaSessionConduit* GetConduit() const { return mConduit; }

  const std::string& GetJsepTransceiverId() const { return mTransceiverId; }

  void SetRemovedFromPc() { mHandlingUnlink = true; }

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(RTCRtpTransceiver)

  static void NegotiatedDetailsToAudioCodecConfigs(
      const JsepTrackNegotiatedDetails& aDetails,
      std::vector<AudioCodecConfig>* aConfigs);

  static void NegotiatedDetailsToVideoCodecConfigs(
      const JsepTrackNegotiatedDetails& aDetails,
      std::vector<VideoCodecConfig>* aConfigs);

  /* Returns a promise that will contain the stats in aStats, along with the
   * codec stats (which is a PC-wide thing) */
  void ChainToDomPromiseWithCodecStats(nsTArray<RefPtr<RTCStatsPromise>> aStats,
                                       const RefPtr<Promise>& aDomPromise);

  /**
   * Takes a set of codec stats (per-peerconnection) and a set of
   * transceiver/transceiver-stats-promise tuples. Filters out all referenced
   * codec stats based on the transceiver's transport and rtp stream stats.
   * Finally returns the flattened stats containing the filtered codec stats and
   * all given per-transceiver-stats.
   */
  static RefPtr<RTCStatsPromise> ApplyCodecStats(
      nsTArray<RTCCodecStats> aCodecStats,
      nsTArray<std::tuple<RTCRtpTransceiver*,
                          RefPtr<RTCStatsPromise::AllPromiseType>>>
          aTransceiverStatsPromises);

  Canonical<std::string>& CanonicalMid() { return mMid; }
  Canonical<std::string>& CanonicalSyncGroup() { return mSyncGroup; }

 private:
  virtual ~RTCRtpTransceiver();
  void InitAudio();
  void InitVideo(const TrackingId& aRecvTrackingId);
  void InitConduitControl();
  void StopImpl();
  void StopTransceiving();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<PeerConnectionImpl> mPc;
  RefPtr<MediaTransportHandler> mTransportHandler;
  const std::string mTransceiverId;
  // Copy of latest from the JSEP engine.
  JsepTransceiver mJsepTransceiver;
  nsCOMPtr<nsISerialEventTarget> mStsThread;
  // state for webrtc.org that is shared between all transceivers
  RefPtr<WebrtcCallWrapper> mCallWrapper;
  RefPtr<MediaStreamTrack> mSendTrack;
  RefPtr<RTCStatsIdGenerator> mIdGenerator;
  RefPtr<MediaSessionConduit> mConduit;
  // The spec says both RTCRtpReceiver and RTCRtpSender have a slot for
  // an RTCDtlsTransport.  They are always the same, so we'll store it
  // here.
  RefPtr<RTCDtlsTransport> mDtlsTransport;
  // The spec says both RTCRtpReceiver and RTCRtpSender have a slot for
  // a last stable state RTCDtlsTransport.  They are always the same, so
  // we'll store it here.
  RefPtr<RTCDtlsTransport> mLastStableDtlsTransport;
  RefPtr<RTCRtpReceiver> mReceiver;
  RefPtr<RTCRtpSender> mSender;
  RTCRtpTransceiverDirection mDirection = RTCRtpTransceiverDirection::Sendrecv;
  Nullable<RTCRtpTransceiverDirection> mCurrentDirection;
  bool mStopped = false;
  bool mStopping = false;
  bool mShutdown = false;
  bool mHasBeenUsedToSend = false;
  PrincipalPrivacy mPrincipalPrivacy;
  bool mShouldRemove = false;
  bool mHasTransport = false;
  bool mIsVideo;
  // This is really nasty. Most of the time, PeerConnectionImpl needs to be in
  // charge of unlinking each RTCRtpTransceiver, because it needs to perform
  // stats queries on its way out, which requires all of the RTCRtpTransceivers
  // (and their transitive dependencies) to stick around until those stats
  // queries are finished. However, when an RTCRtpTransceiver is removed from
  // the PeerConnectionImpl due to negotiation, the PeerConnectionImpl
  // releases its reference, which means the PeerConnectionImpl cannot be in
  // charge of the unlink anymore. We cannot do the unlink when this reference
  // is released either, because RTCRtpTransceiver might have some work it needs
  // to do first. Also, JS may be maintaining a reference to the
  // RTCRtpTransceiver (or one of its dependencies), which means it must remain
  // fully functional after it is removed (meaning it cannot release any of its
  // dependencies, or vice versa).
  bool mHandlingUnlink = false;
  std::string mTransportId;

  Canonical<std::string> mMid;
  Canonical<std::string> mSyncGroup;
};

}  // namespace dom

}  // namespace mozilla

#endif  // _TRANSCEIVERIMPL_H_
