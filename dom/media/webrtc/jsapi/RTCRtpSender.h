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
#include "libwebrtcglue/MediaConduitInterface.h"
#include "libwebrtcglue/RtpRtcpConfig.h"
#include "nsTArray.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "mozilla/dom/RTCRtpParametersBinding.h"
#include "RTCStatsReport.h"

class nsPIDOMWindowInner;

namespace mozilla {
class MediaPipelineTransmit;
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
class RTCRtpTransceiver;

class RTCRtpSender : public nsISupports, public nsWrapperCache {
 public:
  RTCRtpSender(nsPIDOMWindowInner* aWindow, PeerConnectionImpl* aPc,
               MediaTransportHandler* aTransportHandler,
               AbstractThread* aCallThread, nsISerialEventTarget* aStsThread,
               MediaSessionConduit* aConduit, dom::MediaStreamTrack* aTrack,
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
  already_AddRefed<Promise> SetParameters(
      const dom::RTCRtpParameters& aParameters, ErrorResult& aError);
  void GetParameters(RTCRtpParameters& aParameters) const;

  nsPIDOMWindowInner* GetParentObject() const;
  nsTArray<RefPtr<RTCStatsPromise>> GetStatsInternal();

  // This would just be stream ids, except PeerConnection.jsm uses GetStreams
  // to implement the non-standard RTCPeerConnection.getLocalStreams. We might
  // be able to simplify this later.
  // ChromeOnly webidl
  void SetStreams(const Sequence<OwningNonNull<DOMMediaStream>>& aStreams);
  // ChromeOnly webidl
  void GetStreams(nsTArray<RefPtr<DOMMediaStream>>& aStreams);
  void SetTrack(const RefPtr<MediaStreamTrack>& aTrack);
  void Shutdown();
  void BreakCycles();
  void Stop();
  void Start();
  bool HasTrack(const dom::MediaStreamTrack* aTrack) const;
  bool IsMyPc(const PeerConnectionImpl* aPc) const { return mPc.get() == aPc; }
  RefPtr<MediaPipelineTransmit> GetPipeline() const;
  already_AddRefed<dom::Promise> MakePromise(ErrorResult& aError) const;
  bool SeamlessTrackSwitch(const RefPtr<MediaStreamTrack>& aWithTrack);
  bool SetSenderTrackWithClosedCheck(const RefPtr<MediaStreamTrack>& aTrack);

  // This is called when we set an answer (ie; when the transport is finalized).
  void UpdateTransport();
  void UpdateConduit();
  void SyncToJsep(JsepTransceiver& aJsepTransceiver) const;
  void SyncFromJsep(const JsepTransceiver& aJsepTransceiver);

  AbstractCanonical<Ssrcs>* CanonicalSsrcs() { return &mSsrcs; }
  AbstractCanonical<Ssrcs>* CanonicalVideoRtxSsrcs() { return &mVideoRtxSsrcs; }
  AbstractCanonical<RtpExtList>* CanonicalLocalRtpExtensions() {
    return &mLocalRtpExtensions;
  }

  AbstractCanonical<Maybe<AudioCodecConfig>>* CanonicalAudioCodec() {
    return &mAudioCodec;
  }

  AbstractCanonical<Maybe<VideoCodecConfig>>* CanonicalVideoCodec() {
    return &mVideoCodec;
  }
  AbstractCanonical<Maybe<RtpRtcpConfig>>* CanonicalVideoRtpRtcpConfig() {
    return &mVideoRtpRtcpConfig;
  }
  AbstractCanonical<webrtc::VideoCodecMode>* CanonicalVideoCodecMode() {
    return &mVideoCodecMode;
  }
  AbstractCanonical<std::string>* CanonicalCname() { return &mCname; }
  AbstractCanonical<bool>* CanonicalTransmitting() { return &mTransmitting; }

 private:
  virtual ~RTCRtpSender();

  void UpdateVideoConduit();
  void UpdateAudioConduit();

  std::string GetMid() const;
  JsepTransceiver& GetJsepTransceiver();
  void ApplyParameters(const RTCRtpParameters& aParameters);
  void ConfigureVideoCodecMode();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<PeerConnectionImpl> mPc;
  RefPtr<dom::MediaStreamTrack> mSenderTrack;
  RTCRtpParameters mParameters;
  RefPtr<MediaPipelineTransmit> mPipeline;
  RefPtr<RTCRtpTransceiver> mTransceiver;
  nsTArray<RefPtr<DOMMediaStream>> mStreams;
  bool mHaveSetupTransport = false;

  RefPtr<dom::RTCDTMFSender> mDtmf;

  Canonical<Ssrcs> mSsrcs;
  Canonical<Ssrcs> mVideoRtxSsrcs;
  Canonical<RtpExtList> mLocalRtpExtensions;

  Canonical<Maybe<AudioCodecConfig>> mAudioCodec;
  Canonical<Maybe<VideoCodecConfig>> mVideoCodec;
  Canonical<Maybe<RtpRtcpConfig>> mVideoRtpRtcpConfig;
  Canonical<webrtc::VideoCodecMode> mVideoCodecMode;
  Canonical<std::string> mCname;
  Canonical<bool> mTransmitting;
};

}  // namespace dom
}  // namespace mozilla
#endif  // _RTCRtpSender_h_
