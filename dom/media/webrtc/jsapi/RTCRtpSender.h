/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _RTCRtpSender_h_
#define _RTCRtpSender_h_

#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StateMirroring.h"
#include "mozilla/Maybe.h"
#include "js/RootingAPI.h"
#include "libwebrtcglue/RtpRtcpConfig.h"
#include "nsTArray.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "mozilla/dom/RTCRtpCapabilitiesBinding.h"
#include "mozilla/dom/RTCRtpParametersBinding.h"
#include "RTCStatsReport.h"
#include "jsep/JsepTrack.h"
#include "transportbridge/MediaPipeline.h"

class nsPIDOMWindowInner;

namespace mozilla {
class MediaSessionConduit;
class MediaTransportHandler;
class JsepTransceiver;
class PeerConnectionImpl;
class DOMMediaStream;

namespace dom {
class MediaStreamTrack;
class Promise;
class RTCDtlsTransport;
class RTCDTMFSender;
struct RTCRtpCapabilities;
class RTCRtpTransceiver;
class RTCRtpScriptTransform;

class RTCRtpSender : public nsISupports,
                     public nsWrapperCache,
                     public MediaPipelineTransmitControlInterface {
 public:
  RTCRtpSender(nsPIDOMWindowInner* aWindow, PeerConnectionImpl* aPc,
               MediaTransportHandler* aTransportHandler,
               AbstractThread* aCallThread, nsISerialEventTarget* aStsThread,
               MediaSessionConduit* aConduit, dom::MediaStreamTrack* aTrack,
               const Sequence<RTCRtpEncodingParameters>& aEncodings,
               RTCRtpTransceiver* aTransceiver);

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(RTCRtpSender)

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // webidl
  MediaStreamTrack* GetTrack() const { return mSenderTrack; }
  RTCDtlsTransport* GetTransport() const;
  RTCDTMFSender* GetDtmf() const;
  MOZ_CAN_RUN_SCRIPT
  already_AddRefed<Promise> ReplaceTrack(MediaStreamTrack* aWithTrack,
                                         ErrorResult& aError);
  already_AddRefed<Promise> GetStats(ErrorResult& aError);
  static void GetCapabilities(const GlobalObject&, const nsAString& kind,
                              Nullable<dom::RTCRtpCapabilities>& result);
  already_AddRefed<Promise> SetParameters(
      const dom::RTCRtpSendParameters& aParameters, ErrorResult& aError);
  // Not a simple getter, so not const
  // See https://w3c.github.io/webrtc-pc/#dom-rtcrtpsender-getparameters
  void GetParameters(RTCRtpSendParameters& aParameters);

  static void CheckAndRectifyEncodings(
      Sequence<RTCRtpEncodingParameters>& aEncodings, bool aVideo,
      ErrorResult& aRv);

  RTCRtpScriptTransform* GetTransform() const { return mTransform; }

  void SetTransform(RTCRtpScriptTransform* aTransform, ErrorResult& aError);
  bool GenerateKeyFrame(const Maybe<std::string>& aRid);

  nsPIDOMWindowInner* GetParentObject() const;
  nsTArray<RefPtr<RTCStatsPromise>> GetStatsInternal(
      bool aSkipIceStats = false);

  void SetStreams(const Sequence<OwningNonNull<DOMMediaStream>>& aStreams,
                  ErrorResult& aRv);
  // ChromeOnly webidl
  void GetStreams(nsTArray<RefPtr<DOMMediaStream>>& aStreams);
  // ChromeOnly webidl
  void SetStreamsImpl(const Sequence<OwningNonNull<DOMMediaStream>>& aStreams);
  // ChromeOnly webidl
  void SetTrack(const RefPtr<MediaStreamTrack>& aTrack);
  void Shutdown();
  void BreakCycles();
  void Unlink();
  // Terminal state, reached through stopping RTCRtpTransceiver.
  void Stop();
  bool HasTrack(const dom::MediaStreamTrack* aTrack) const;
  bool IsMyPc(const PeerConnectionImpl* aPc) const { return mPc.get() == aPc; }
  RefPtr<MediaPipelineTransmit> GetPipeline() const;
  already_AddRefed<dom::Promise> MakePromise(ErrorResult& aError) const;
  bool SeamlessTrackSwitch(const RefPtr<MediaStreamTrack>& aWithTrack);
  bool SetSenderTrackWithClosedCheck(const RefPtr<MediaStreamTrack>& aTrack);

  // This is called when we set an answer (ie; when the transport is finalized).
  void UpdateTransport();
  void SyncToJsep(JsepTransceiver& aJsepTransceiver) const;
  void SyncFromJsep(const JsepTransceiver& aJsepTransceiver);
  void MaybeUpdateConduit();

  Canonical<Ssrcs>& CanonicalSsrcs() { return mSsrcs; }
  Canonical<Ssrcs>& CanonicalVideoRtxSsrcs() { return mVideoRtxSsrcs; }
  Canonical<RtpExtList>& CanonicalLocalRtpExtensions() {
    return mLocalRtpExtensions;
  }

  Canonical<Maybe<AudioCodecConfig>>& CanonicalAudioCodec() {
    return mAudioCodec;
  }

  Canonical<Maybe<VideoCodecConfig>>& CanonicalVideoCodec() {
    return mVideoCodec;
  }
  Canonical<Maybe<RtpRtcpConfig>>& CanonicalVideoRtpRtcpConfig() {
    return mVideoRtpRtcpConfig;
  }
  Canonical<webrtc::VideoCodecMode>& CanonicalVideoCodecMode() {
    return mVideoCodecMode;
  }
  Canonical<std::string>& CanonicalCname() { return mCname; }
  Canonical<bool>& CanonicalTransmitting() override { return mTransmitting; }

  Canonical<RefPtr<FrameTransformerProxy>>& CanonicalFrameTransformerProxy() {
    return mFrameTransformerProxy;
  }

  bool HasPendingSetParameters() const { return mPendingParameters.isSome(); }
  void InvalidateLastReturnedParameters() {
    mLastReturnedParameters = Nothing();
  }

 private:
  virtual ~RTCRtpSender();

  std::string GetMid() const;
  JsepTransceiver& GetJsepTransceiver();
  static void ApplyJsEncodingToConduitEncoding(
      const RTCRtpEncodingParameters& aJsEncoding,
      VideoCodecConfig::Encoding* aConduitEncoding);
  void UpdateRestorableEncodings(
      const Sequence<RTCRtpEncodingParameters>& aEncodings);
  Sequence<RTCRtpEncodingParameters> GetMatchingEncodings(
      const std::vector<std::string>& aRids) const;
  Sequence<RTCRtpEncodingParameters> ToSendEncodings(
      const std::vector<std::string>& aRids) const;
  void MaybeGetJsepRids();
  void UpdateDtmfSender();

  void WarnAboutBadSetParameters(const nsCString& aError);
  nsCString GetEffectiveTLDPlus1() const;

  WatchManager<RTCRtpSender> mWatchManager;
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<PeerConnectionImpl> mPc;
  RefPtr<dom::MediaStreamTrack> mSenderTrack;
  bool mSenderTrackSetByAddTrack = false;
  RTCRtpSendParameters mParameters;
  Maybe<RTCRtpSendParameters> mPendingParameters;
  uint32_t mNumSetParametersCalls = 0;
  // When JSEP goes from simulcast to unicast without a rid, and we started out
  // as unicast without a rid, we are supposed to restore that unicast encoding
  // from before.
  Maybe<RTCRtpEncodingParameters> mUnicastEncoding;
  bool mSimulcastEnvelopeSet = false;
  bool mSimulcastEnvelopeSetByJSEP = false;
  bool mPendingRidChangeFromCompatMode = false;
  Maybe<RTCRtpSendParameters> mLastReturnedParameters;
  RefPtr<MediaPipelineTransmit> mPipeline;
  RefPtr<MediaTransportHandler> mTransportHandler;
  RefPtr<RTCRtpTransceiver> mTransceiver;
  nsTArray<RefPtr<DOMMediaStream>> mStreams;
  RefPtr<RTCRtpScriptTransform> mTransform;
  bool mHaveSetupTransport = false;
  // TODO(bug 1803388): Remove this stuff once it is no longer needed.
  bool mAllowOldSetParameters = false;

  // TODO(bug 1803388): Remove the glean warnings once they are no longer needed
  bool mHaveWarnedBecauseNoGetParameters = false;
  bool mHaveWarnedBecauseEncodingCountChange = false;
  bool mHaveWarnedBecauseNoTransactionId = false;
  // TODO(bug 1803389): Remove the glean errors once they are no longer needed.
  bool mHaveFailedBecauseNoGetParameters = false;
  bool mHaveFailedBecauseEncodingCountChange = false;
  bool mHaveFailedBecauseRidChange = false;
  bool mHaveFailedBecauseNoTransactionId = false;
  bool mHaveFailedBecauseStaleTransactionId = false;
  bool mHaveFailedBecauseNoEncodings = false;
  bool mHaveFailedBecauseOtherError = false;

  // Limits logging of codec information
  bool mHaveLoggedUlpfecInfo = false;
  bool mHaveLoggedOtherFec = false;
  bool mHaveLoggedVideoPreferredCodec = false;
  bool mHaveLoggedAudioPreferredCodec = false;

  RefPtr<dom::RTCDTMFSender> mDtmf;

  class BaseConfig {
   public:
    // TODO(bug 1744116): Use = default here
    bool operator==(const BaseConfig& aOther) const {
      return mSsrcs == aOther.mSsrcs &&
             mLocalRtpExtensions == aOther.mLocalRtpExtensions &&
             mCname == aOther.mCname && mTransmitting == aOther.mTransmitting;
    }
    Ssrcs mSsrcs;
    RtpExtList mLocalRtpExtensions;
    std::string mCname;
    bool mTransmitting = false;
  };

  class VideoConfig : public BaseConfig {
   public:
    // TODO(bug 1744116): Use = default here
    bool operator==(const VideoConfig& aOther) const {
      return BaseConfig::operator==(aOther) &&
             mVideoRtxSsrcs == aOther.mVideoRtxSsrcs &&
             mVideoCodec == aOther.mVideoCodec &&
             mVideoRtpRtcpConfig == aOther.mVideoRtpRtcpConfig &&
             mVideoCodecMode == aOther.mVideoCodecMode;
    }
    Ssrcs mVideoRtxSsrcs;
    Maybe<VideoCodecConfig> mVideoCodec;
    Maybe<RtpRtcpConfig> mVideoRtpRtcpConfig;
    webrtc::VideoCodecMode mVideoCodecMode =
        webrtc::VideoCodecMode::kRealtimeVideo;
  };

  class AudioConfig : public BaseConfig {
   public:
    // TODO(bug 1744116): Use = default here
    bool operator==(const AudioConfig& aOther) const {
      return BaseConfig::operator==(aOther) &&
             mAudioCodec == aOther.mAudioCodec && mDtmfPt == aOther.mDtmfPt &&
             mDtmfFreq == aOther.mDtmfFreq;
    }
    Maybe<AudioCodecConfig> mAudioCodec;
    int32_t mDtmfPt = -1;
    int32_t mDtmfFreq = 0;
  };

  Maybe<VideoConfig> GetNewVideoConfig();
  Maybe<AudioConfig> GetNewAudioConfig();
  void UpdateBaseConfig(BaseConfig* aConfig);
  void ApplyVideoConfig(const VideoConfig& aConfig);
  void ApplyAudioConfig(const AudioConfig& aConfig);

  Canonical<Ssrcs> mSsrcs;
  Canonical<Ssrcs> mVideoRtxSsrcs;
  Canonical<RtpExtList> mLocalRtpExtensions;

  Canonical<Maybe<AudioCodecConfig>> mAudioCodec;
  Canonical<Maybe<VideoCodecConfig>> mVideoCodec;
  Canonical<Maybe<RtpRtcpConfig>> mVideoRtpRtcpConfig;
  Canonical<webrtc::VideoCodecMode> mVideoCodecMode;
  Canonical<std::string> mCname;
  Canonical<bool> mTransmitting;
  Canonical<RefPtr<FrameTransformerProxy>> mFrameTransformerProxy;
};

}  // namespace dom
}  // namespace mozilla
#endif  // _RTCRtpSender_h_
