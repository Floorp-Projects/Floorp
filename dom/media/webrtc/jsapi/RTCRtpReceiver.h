/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _RTCRtpReceiver_h_
#define _RTCRtpReceiver_h_

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
#include "RTCStatsReport.h"
#include "libwebrtcglue/RtcpEventObserver.h"
#include <vector>

class nsPIDOMWindowInner;

namespace mozilla {
class MediaPipelineReceive;
class MediaSessionConduit;
class MediaTransportHandler;
class JsepTransceiver;
class TransceiverImpl;

namespace dom {
class MediaStreamTrack;
class Promise;
class RTCDtlsTransport;
struct RTCRtpContributingSource;
struct RTCRtpSynchronizationSource;

class RTCRtpReceiver : public nsISupports,
                       public nsWrapperCache,
                       public RtcpEventObserver {
 public:
  RTCRtpReceiver(nsPIDOMWindowInner* aWindow, bool aPrivacyNeeded,
                 const std::string& aPCHandle,
                 MediaTransportHandler* aTransportHandler,
                 JsepTransceiver* aJsepTransceiver,
                 nsISerialEventTarget* aMainThread, AbstractThread* aCallThread,
                 nsISerialEventTarget* aStsThread,
                 MediaSessionConduit* aConduit,
                 TransceiverImpl* aTransceiverImpl);

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(RTCRtpReceiver)

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // webidl
  MediaStreamTrack* Track() const { return mTrack; }
  RTCDtlsTransport* GetTransport() const;
  already_AddRefed<Promise> GetStats();
  void GetContributingSources(
      nsTArray<dom::RTCRtpContributingSource>& aSources);
  void GetSynchronizationSources(
      nsTArray<dom::RTCRtpSynchronizationSource>& aSources);
  // test-only: insert fake CSRCs and audio levels for testing
  void MozInsertAudioLevelForContributingSource(
      const uint32_t aSource, const DOMHighResTimeStamp aTimestamp,
      const uint32_t aRtpTimestamp, const bool aHasLevel, const uint8_t aLevel);

  nsPIDOMWindowInner* GetParentObject() const;
  nsTArray<RefPtr<RTCStatsPromise>> GetStatsInternal();

  void Shutdown();
  void Stop();
  void Start();
  bool HasTrack(const dom::MediaStreamTrack* aTrack) const;

  struct StreamAssociation {
    RefPtr<MediaStreamTrack> mTrack;
    std::string mStreamId;
  };

  struct TrackEventInfo {
    RefPtr<RTCRtpReceiver> mReceiver;
    std::vector<std::string> mStreamIds;
  };

  struct StreamAssociationChanges {
    std::vector<RefPtr<MediaStreamTrack>> mTracksToMute;
    std::vector<StreamAssociation> mStreamAssociationsRemoved;
    std::vector<StreamAssociation> mStreamAssociationsAdded;
    std::vector<TrackEventInfo> mTrackEvents;
  };

  // This is called when we set an answer (ie; when the transport is finalized).
  void UpdateTransport();
  nsresult UpdateConduit();

  // This is called when we set a remote description; may be an offer or answer.
  void UpdateStreams(StreamAssociationChanges* aChanges);

  void OnRtcpBye() override;

  void OnRtcpTimeout() override;

  void SetReceiveTrackMuted(bool aMuted);

  AbstractCanonical<Ssrc>* CanonicalSsrc() { return &mSsrc; }
  AbstractCanonical<Ssrc>* CanonicalVideoRtxSsrc() { return &mVideoRtxSsrc; }
  AbstractCanonical<RtpExtList>* CanonicalLocalRtpExtensions() {
    return &mLocalRtpExtensions;
  }

  AbstractCanonical<std::vector<AudioCodecConfig>>* CanonicalAudioCodecs() {
    return &mAudioCodecs;
  }

  AbstractCanonical<std::vector<VideoCodecConfig>>* CanonicalVideoCodecs() {
    return &mVideoCodecs;
  }
  AbstractCanonical<Maybe<RtpRtcpConfig>>* CanonicalVideoRtpRtcpConfig() {
    return &mVideoRtpRtcpConfig;
  }

 private:
  virtual ~RTCRtpReceiver();

  nsresult UpdateVideoConduit();
  nsresult UpdateAudioConduit();

  std::string GetMid() const;

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  const std::string mPCHandle;
  RefPtr<JsepTransceiver> mJsepTransceiver;
  bool mHaveStartedReceiving = false;
  bool mHaveSetupTransport = false;
  nsCOMPtr<nsISerialEventTarget> mMainThread;
  RefPtr<AbstractThread> mCallThread;
  nsCOMPtr<nsISerialEventTarget> mStsThread;
  RefPtr<dom::MediaStreamTrack> mTrack;
  RefPtr<MediaPipelineReceive> mPipeline;
  RefPtr<MediaTransportHandler> mTransportHandler;
  RefPtr<TransceiverImpl> mTransceiverImpl;
  // This is [[AssociatedRemoteMediaStreams]], basically. We do not keep the
  // streams themselves here, because that would require this object to know
  // where the stream list for the whole RTCPeerConnection lives..
  std::vector<std::string> mStreamIds;
  bool mRemoteSetSendBit = false;

  Canonical<Ssrc> mSsrc;
  Canonical<Ssrc> mVideoRtxSsrc;
  Canonical<RtpExtList> mLocalRtpExtensions;

  Canonical<std::vector<AudioCodecConfig>> mAudioCodecs;

  Canonical<std::vector<VideoCodecConfig>> mVideoCodecs;
  Canonical<Maybe<RtpRtcpConfig>> mVideoRtpRtcpConfig;
};

}  // namespace dom
}  // namespace mozilla
#endif  // _RTCRtpReceiver_h_
