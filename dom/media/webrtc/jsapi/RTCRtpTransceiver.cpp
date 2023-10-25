/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi/RTCRtpTransceiver.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>
#include <utility>
#include <set>
#include <string>
#include <tuple>

#include "api/video_codecs/video_codec.h"

#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"
#include "nsISerialEventTarget.h"
#include "nsISupports.h"
#include "nsProxyRelease.h"
#include "nsStringFwd.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsWrapperCache.h"
#include "PrincipalHandle.h"
#include "ErrorList.h"
#include "MainThreadUtils.h"
#include "MediaEventSource.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/fallible.h"
#include "mozilla/Maybe.h"
#include "mozilla/mozalloc_oom.h"
#include "mozilla/MozPromise.h"
#include "mozilla/Preferences.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/StateMirroring.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "mozilla/dom/RTCRtpReceiverBinding.h"
#include "mozilla/dom/RTCRtpSenderBinding.h"
#include "mozilla/dom/RTCRtpTransceiverBinding.h"
#include "mozilla/dom/Promise.h"
#include "utils/PerformanceRecorder.h"
#include "systemservices/MediaUtils.h"
#include "MediaTrackGraph.h"
#include "js/RootingAPI.h"
#include "libwebrtcglue/AudioConduit.h"
#include "libwebrtcglue/VideoConduit.h"
#include "transportbridge/MediaPipeline.h"
#include "jsep/JsepTrack.h"
#include "sdp/SdpHelper.h"
#include "transport/logging.h"
#include "RemoteTrackSource.h"
#include "libwebrtcglue/RtpRtcpConfig.h"
#include "MediaTransportHandler.h"
#include "RTCDtlsTransport.h"
#include "RTCRtpReceiver.h"
#include "RTCRtpSender.h"
#include "RTCDTMFSender.h"
#include "PeerConnectionImpl.h"
#include "RTCStatsIdGenerator.h"
#include "libwebrtcglue/WebrtcCallWrapper.h"
#include "libwebrtcglue/FrameTransformerProxy.h"
#include "jsep/JsepCodecDescription.h"
#include "jsep/JsepSession.h"
#include "jsep/JsepTrackEncoding.h"
#include "libwebrtcglue/CodecConfig.h"
#include "libwebrtcglue/MediaConduitControl.h"
#include "libwebrtcglue/MediaConduitInterface.h"
#include "RTCStatsReport.h"
#include "sdp/SdpAttribute.h"
#include "sdp/SdpEnum.h"
#include "sdp/SdpMediaSection.h"
#include "transport/transportlayer.h"

namespace mozilla {

using namespace dom;

namespace {
struct ConduitControlState : public AudioConduitControlInterface,
                             public VideoConduitControlInterface {
  ConduitControlState(RTCRtpTransceiver* aTransceiver, RTCRtpSender* aSender,
                      RTCRtpReceiver* aReceiver)
      : mTransceiver(new nsMainThreadPtrHolder<RTCRtpTransceiver>(
            "ConduitControlState::mTransceiver", aTransceiver, false)),
        mSender(new nsMainThreadPtrHolder<dom::RTCRtpSender>(
            "ConduitControlState::mSender", aSender, false)),
        mReceiver(new nsMainThreadPtrHolder<dom::RTCRtpReceiver>(
            "ConduitControlState::mReceiver", aReceiver, false)) {}

  const nsMainThreadPtrHandle<RTCRtpTransceiver> mTransceiver;
  const nsMainThreadPtrHandle<RTCRtpSender> mSender;
  const nsMainThreadPtrHandle<RTCRtpReceiver> mReceiver;

  // MediaConduitControlInterface
  Canonical<bool>& CanonicalReceiving() override {
    return mReceiver->CanonicalReceiving();
  }
  Canonical<bool>& CanonicalTransmitting() override {
    return mSender->CanonicalTransmitting();
  }
  Canonical<Ssrcs>& CanonicalLocalSsrcs() override {
    return mSender->CanonicalSsrcs();
  }
  Canonical<std::string>& CanonicalLocalCname() override {
    return mSender->CanonicalCname();
  }
  Canonical<std::string>& CanonicalMid() override {
    return mTransceiver->CanonicalMid();
  }
  Canonical<Ssrc>& CanonicalRemoteSsrc() override {
    return mReceiver->CanonicalSsrc();
  }
  Canonical<std::string>& CanonicalSyncGroup() override {
    return mTransceiver->CanonicalSyncGroup();
  }
  Canonical<RtpExtList>& CanonicalLocalRecvRtpExtensions() override {
    return mReceiver->CanonicalLocalRtpExtensions();
  }
  Canonical<RtpExtList>& CanonicalLocalSendRtpExtensions() override {
    return mSender->CanonicalLocalRtpExtensions();
  }

  // AudioConduitControlInterface
  Canonical<Maybe<AudioCodecConfig>>& CanonicalAudioSendCodec() override {
    return mSender->CanonicalAudioCodec();
  }
  Canonical<std::vector<AudioCodecConfig>>& CanonicalAudioRecvCodecs()
      override {
    return mReceiver->CanonicalAudioCodecs();
  }
  MediaEventSource<DtmfEvent>& OnDtmfEvent() override {
    return mSender->GetDtmf()->OnDtmfEvent();
  }

  // VideoConduitControlInterface
  Canonical<Ssrcs>& CanonicalLocalVideoRtxSsrcs() override {
    return mSender->CanonicalVideoRtxSsrcs();
  }
  Canonical<Ssrc>& CanonicalRemoteVideoRtxSsrc() override {
    return mReceiver->CanonicalVideoRtxSsrc();
  }
  Canonical<Maybe<VideoCodecConfig>>& CanonicalVideoSendCodec() override {
    return mSender->CanonicalVideoCodec();
  }
  Canonical<Maybe<RtpRtcpConfig>>& CanonicalVideoSendRtpRtcpConfig() override {
    return mSender->CanonicalVideoRtpRtcpConfig();
  }
  Canonical<std::vector<VideoCodecConfig>>& CanonicalVideoRecvCodecs()
      override {
    return mReceiver->CanonicalVideoCodecs();
  }
  Canonical<Maybe<RtpRtcpConfig>>& CanonicalVideoRecvRtpRtcpConfig() override {
    return mReceiver->CanonicalVideoRtpRtcpConfig();
  }
  Canonical<webrtc::VideoCodecMode>& CanonicalVideoCodecMode() override {
    return mSender->CanonicalVideoCodecMode();
  }
  Canonical<RefPtr<FrameTransformerProxy>>& CanonicalFrameTransformerProxySend()
      override {
    return mSender->CanonicalFrameTransformerProxy();
  }

  Canonical<RefPtr<FrameTransformerProxy>>& CanonicalFrameTransformerProxyRecv()
      override {
    return mReceiver->CanonicalFrameTransformerProxy();
  }
};
}  // namespace

MOZ_MTLOG_MODULE("RTCRtpTransceiver")

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(RTCRtpTransceiver)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(RTCRtpTransceiver)
  tmp->Unlink();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(RTCRtpTransceiver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow, mPc, mSendTrack, mReceiver,
                                    mSender, mDtlsTransport,
                                    mLastStableDtlsTransport)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(RTCRtpTransceiver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(RTCRtpTransceiver)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RTCRtpTransceiver)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

#define INIT_CANONICAL(name, val)         \
  name(AbstractThread::MainThread(), val, \
       "RTCRtpTransceiver::" #name " (Canonical)")

RTCRtpTransceiver::RTCRtpTransceiver(
    nsPIDOMWindowInner* aWindow, bool aPrivacyNeeded, PeerConnectionImpl* aPc,
    MediaTransportHandler* aTransportHandler, JsepSession* aJsepSession,
    const std::string& aTransceiverId, bool aIsVideo,
    nsISerialEventTarget* aStsThread, dom::MediaStreamTrack* aSendTrack,
    WebrtcCallWrapper* aCallWrapper, RTCStatsIdGenerator* aIdGenerator)
    : mWindow(aWindow),
      mPc(aPc),
      mTransportHandler(aTransportHandler),
      mTransceiverId(aTransceiverId),
      mJsepTransceiver(*aJsepSession->GetTransceiver(mTransceiverId)),
      mStsThread(aStsThread),
      mCallWrapper(aCallWrapper),
      mSendTrack(aSendTrack),
      mIdGenerator(aIdGenerator),
      mPrincipalPrivacy(aPrivacyNeeded ? PrincipalPrivacy::Private
                                       : PrincipalPrivacy::NonPrivate),
      mIsVideo(aIsVideo),
      INIT_CANONICAL(mMid, std::string()),
      INIT_CANONICAL(mSyncGroup, std::string()) {}

#undef INIT_CANONICAL

RTCRtpTransceiver::~RTCRtpTransceiver() = default;

SdpDirectionAttribute::Direction ToSdpDirection(
    RTCRtpTransceiverDirection aDirection) {
  switch (aDirection) {
    case dom::RTCRtpTransceiverDirection::Sendrecv:
      return SdpDirectionAttribute::Direction::kSendrecv;
    case dom::RTCRtpTransceiverDirection::Sendonly:
      return SdpDirectionAttribute::Direction::kSendonly;
    case dom::RTCRtpTransceiverDirection::Recvonly:
      return SdpDirectionAttribute::Direction::kRecvonly;
    case dom::RTCRtpTransceiverDirection::Inactive:
    case dom::RTCRtpTransceiverDirection::Stopped:
      return SdpDirectionAttribute::Direction::kInactive;
    case dom::RTCRtpTransceiverDirection::EndGuard_:;
  }
  MOZ_CRASH("Invalid transceiver direction!");
}

static uint32_t sRemoteSourceId = 0;

// TODO(bug 1401592): Once we implement the sendEncodings stuff, there will
// need to be validation code in here.
void RTCRtpTransceiver::Init(const RTCRtpTransceiverInit& aInit,
                             ErrorResult& aRv) {
  TrackingId trackingId(TrackingId::Source::RTCRtpReceiver, sRemoteSourceId++,
                        TrackingId::TrackAcrossProcesses::Yes);
  if (IsVideo()) {
    InitVideo(trackingId);
  } else {
    InitAudio();
  }

  if (!IsValid()) {
    aRv = NS_ERROR_UNEXPECTED;
    return;
  }

  mReceiver = new RTCRtpReceiver(mWindow, mPrincipalPrivacy, mPc,
                                 mTransportHandler, mCallWrapper->mCallThread,
                                 mStsThread, mConduit, this, trackingId);

  mSender = new RTCRtpSender(mWindow, mPc, mTransportHandler,
                             mCallWrapper->mCallThread, mStsThread, mConduit,
                             mSendTrack, aInit.mSendEncodings, this);

  if (mConduit) {
    InitConduitControl();
  }

  mSender->SetStreamsImpl(aInit.mStreams);
  mDirection = aInit.mDirection;
}

void RTCRtpTransceiver::SetDtlsTransport(dom::RTCDtlsTransport* aDtlsTransport,
                                         bool aStable) {
  mDtlsTransport = aDtlsTransport;
  if (aStable) {
    mLastStableDtlsTransport = mDtlsTransport;
  }
}

void RTCRtpTransceiver::RollbackToStableDtlsTransport() {
  mDtlsTransport = mLastStableDtlsTransport;
}

void RTCRtpTransceiver::InitAudio() {
  mConduit = AudioSessionConduit::Create(mCallWrapper, mStsThread);

  if (!mConduit) {
    MOZ_MTLOG(ML_ERROR, mPc->GetHandle()
                            << "[" << mMid.Ref() << "]: " << __FUNCTION__
                            << ": Failed to create AudioSessionConduit");
    // TODO(bug 1422897): We need a way to record this when it happens in the
    // wild.
  }
}

void RTCRtpTransceiver::InitVideo(const TrackingId& aRecvTrackingId) {
  VideoSessionConduit::Options options;
  options.mVideoLatencyTestEnable =
      Preferences::GetBool("media.video.test_latency", false);
  options.mMinBitrate = std::max(
      0,
      Preferences::GetInt("media.peerconnection.video.min_bitrate", 0) * 1000);
  options.mStartBitrate = std::max(
      0, Preferences::GetInt("media.peerconnection.video.start_bitrate", 0) *
             1000);
  options.mPrefMaxBitrate = std::max(
      0,
      Preferences::GetInt("media.peerconnection.video.max_bitrate", 0) * 1000);
  if (options.mMinBitrate != 0 &&
      options.mMinBitrate < kViEMinCodecBitrate_bps) {
    options.mMinBitrate = kViEMinCodecBitrate_bps;
  }
  if (options.mStartBitrate < options.mMinBitrate) {
    options.mStartBitrate = options.mMinBitrate;
  }
  if (options.mPrefMaxBitrate &&
      options.mStartBitrate > options.mPrefMaxBitrate) {
    options.mStartBitrate = options.mPrefMaxBitrate;
  }
  // XXX We'd love if this was a live param for testing adaptation/etc
  // in automation
  options.mMinBitrateEstimate =
      std::max(0, Preferences::GetInt(
                      "media.peerconnection.video.min_bitrate_estimate", 0) *
                      1000);
  options.mSpatialLayers = std::max(
      1, Preferences::GetInt("media.peerconnection.video.svc.spatial", 0));
  options.mTemporalLayers = std::max(
      1, Preferences::GetInt("media.peerconnection.video.svc.temporal", 0));
  options.mDenoising =
      Preferences::GetBool("media.peerconnection.video.denoising", false);
  options.mLockScaling =
      Preferences::GetBool("media.peerconnection.video.lock_scaling", false);

  mConduit =
      VideoSessionConduit::Create(mCallWrapper, mStsThread, std::move(options),
                                  mPc->GetHandle(), aRecvTrackingId);

  if (!mConduit) {
    MOZ_MTLOG(ML_ERROR, mPc->GetHandle()
                            << "[" << mMid.Ref() << "]: " << __FUNCTION__
                            << ": Failed to create VideoSessionConduit");
    // TODO(bug 1422897): We need a way to record this when it happens in the
    // wild.
  }
}

void RTCRtpTransceiver::InitConduitControl() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mConduit);
  ConduitControlState control(this, mSender, mReceiver);
  mConduit->AsVideoSessionConduit().apply(
      [&](auto aConduit) { aConduit->InitControl(&control); });
  mConduit->AsAudioSessionConduit().apply(
      [&](auto aConduit) { aConduit->InitControl(&control); });
}

void RTCRtpTransceiver::Close() {
  // Called via PCImpl::Close -> PCImpl::CloseInt -> PCImpl::ShutdownMedia ->
  // PCMedia::SelfDestruct.  Satisfies step 7 of
  // https://w3c.github.io/webrtc-pc/#dom-rtcpeerconnection-close
  mShutdown = true;
  if (mDtlsTransport) {
    mDtlsTransport->UpdateStateNoEvent(TransportLayer::TS_CLOSED);
  }
  StopImpl();
}

void RTCRtpTransceiver::BreakCycles() {
  mSender->BreakCycles();
  mReceiver->BreakCycles();
  mWindow = nullptr;
  mSendTrack = nullptr;
  mSender = nullptr;
  mReceiver = nullptr;
  mDtlsTransport = nullptr;
  mLastStableDtlsTransport = nullptr;
  mPc = nullptr;
}

void RTCRtpTransceiver::Unlink() {
  if (mHandlingUnlink) {
    BreakCycles();
    mHandlingUnlink = false;
  } else if (mPc) {
    mPc->Close();
    mPc->BreakCycles();
  }
}

// TODO: Only called from one place in PeerConnectionImpl, synchronously, when
// the JSEP engine has successfully completed an offer/answer exchange. This is
// a bit squirrely, since identity validation happens asynchronously in
// PeerConnection.jsm. This probably needs to happen once all the "in parallel"
// steps have succeeded, but before we queue the task for JS observable state
// updates.
nsresult RTCRtpTransceiver::UpdateTransport() {
  if (!mHasTransport) {
    return NS_OK;
  }

  mReceiver->UpdateTransport();
  mSender->UpdateTransport();
  return NS_OK;
}

nsresult RTCRtpTransceiver::UpdateConduit() {
  if (mStopped) {
    return NS_OK;
  }

  mReceiver->UpdateConduit();
  mSender->MaybeUpdateConduit();

  return NS_OK;
}

void RTCRtpTransceiver::UpdatePrincipalPrivacy(PrincipalPrivacy aPrivacy) {
  if (mPrincipalPrivacy == aPrivacy) {
    return;
  }

  mPrincipalPrivacy = aPrivacy;
  mReceiver->UpdatePrincipalPrivacy(mPrincipalPrivacy);
}

void RTCRtpTransceiver::ResetSync() { mSyncGroup = std::string(); }

// TODO: Only called from one place in PeerConnectionImpl, synchronously, when
// the JSEP engine has successfully completed an offer/answer exchange. This is
// a bit squirrely, since identity validation happens asynchronously in
// PeerConnection.jsm. This probably needs to happen once all the "in parallel"
// steps have succeeded, but before we queue the task for JS observable state
// updates.
nsresult RTCRtpTransceiver::SyncWithMatchingVideoConduits(
    nsTArray<RefPtr<RTCRtpTransceiver>>& transceivers) {
  if (mStopped) {
    return NS_OK;
  }

  if (IsVideo()) {
    MOZ_MTLOG(ML_ERROR, mPc->GetHandle()
                            << "[" << mMid.Ref() << "]: " << __FUNCTION__
                            << " called when transceiver is not "
                               "video! This should never happen.");
    MOZ_CRASH();
    return NS_ERROR_UNEXPECTED;
  }

  std::set<std::string> myReceiveStreamIds;
  myReceiveStreamIds.insert(mReceiver->GetStreamIds().begin(),
                            mReceiver->GetStreamIds().end());

  for (RefPtr<RTCRtpTransceiver>& transceiver : transceivers) {
    if (!transceiver->IsValid()) {
      continue;
    }

    if (!transceiver->IsVideo()) {
      // |this| is an audio transceiver, so we skip other audio transceivers
      continue;
    }

    // Maybe could make this more efficient by cacheing this set, but probably
    // not worth it.
    for (const std::string& streamId :
         transceiver->Receiver()->GetStreamIds()) {
      if (myReceiveStreamIds.count(streamId)) {
        // Ok, we have one video, one non-video - cross the streams!
        mSyncGroup = streamId;
        transceiver->mSyncGroup = streamId;

        MOZ_MTLOG(ML_DEBUG, mPc->GetHandle()
                                << "[" << mMid.Ref() << "]: " << __FUNCTION__
                                << " Syncing " << mConduit.get() << " to "
                                << transceiver->mConduit.get());

        // The sync code in call.cc only permits sync between audio stream and
        // one video stream. They take the first match, so there's no point in
        // continuing here. If we want to change the default, we should sort
        // video streams here and only call SetSyncGroup on the chosen stream.
        break;
      }
    }
  }

  return NS_OK;
}

bool RTCRtpTransceiver::ConduitHasPluginID(uint64_t aPluginID) {
  return mConduit && mConduit->HasCodecPluginID(aPluginID);
}

void RTCRtpTransceiver::SyncFromJsep(const JsepSession& aSession) {
  MOZ_MTLOG(ML_DEBUG, mPc->GetHandle()
                          << "[" << mMid.Ref() << "]: " << __FUNCTION__
                          << " Syncing from JSEP transceiver");
  if (mShutdown) {
    // Shutdown_m has already been called, probably due to pc.close(). Just
    // nod and smile.
    return;
  }

  mJsepTransceiver = *aSession.GetTransceiver(mTransceiverId);

  // Transceivers can stop due to sRD, so we need to check that
  if (!mStopped && mJsepTransceiver.IsStopped()) {
    MOZ_MTLOG(ML_DEBUG, mPc->GetHandle()
                            << "[" << mMid.Ref() << "]: " << __FUNCTION__
                            << " JSEP transceiver is stopped");
    StopImpl();
  }

  mReceiver->SyncFromJsep(mJsepTransceiver);
  mSender->SyncFromJsep(mJsepTransceiver);

  // mid from JSEP
  if (mJsepTransceiver.IsAssociated()) {
    mMid = mJsepTransceiver.GetMid();
  } else {
    mMid = std::string();
  }

  // currentDirection from JSEP, but not if "this transceiver has never been
  // represented in an offer/answer exchange"
  if (mJsepTransceiver.HasLevel() && mJsepTransceiver.IsNegotiated()) {
    if (mJsepTransceiver.mRecvTrack.GetActive()) {
      if (mJsepTransceiver.mSendTrack.GetActive()) {
        mCurrentDirection.SetValue(dom::RTCRtpTransceiverDirection::Sendrecv);
        mHasBeenUsedToSend = true;
      } else {
        mCurrentDirection.SetValue(dom::RTCRtpTransceiverDirection::Recvonly);
      }
    } else {
      if (mJsepTransceiver.mSendTrack.GetActive()) {
        mCurrentDirection.SetValue(dom::RTCRtpTransceiverDirection::Sendonly);
        mHasBeenUsedToSend = true;
      } else {
        mCurrentDirection.SetValue(dom::RTCRtpTransceiverDirection::Inactive);
      }
    }
  }

  mShouldRemove = mJsepTransceiver.IsRemoved();
  mHasTransport = !mStopped && mJsepTransceiver.mTransport.mComponents;
}

void RTCRtpTransceiver::SyncToJsep(JsepSession& aSession) const {
  MOZ_MTLOG(ML_DEBUG, mPc->GetHandle()
                          << "[" << mMid.Ref() << "]: " << __FUNCTION__
                          << " Syncing to JSEP transceiver");

  aSession.ApplyToTransceiver(
      mTransceiverId, [this, self = RefPtr<const RTCRtpTransceiver>(this)](
                          JsepTransceiver& aTransceiver) {
        mReceiver->SyncToJsep(aTransceiver);
        mSender->SyncToJsep(aTransceiver);
        aTransceiver.mJsDirection = ToSdpDirection(mDirection);
        if (mStopping || mStopped) {
          aTransceiver.Stop();
        }
      });
}

void RTCRtpTransceiver::GetKind(nsAString& aKind) const {
  // The transceiver kind of an RTCRtpTransceiver is defined by the kind of the
  // associated RTCRtpReceiver's MediaStreamTrack object.
  MOZ_ASSERT(mReceiver && mReceiver->Track());
  mReceiver->Track()->GetKind(aKind);
}

void RTCRtpTransceiver::GetMid(nsAString& aMid) const {
  if (!mMid.Ref().empty()) {
    aMid = NS_ConvertUTF8toUTF16(mMid.Ref());
  } else {
    aMid.SetIsVoid(true);
  }
}

std::string RTCRtpTransceiver::GetMidAscii() const {
  if (mMid.Ref().empty()) {
    return std::string();
  }

  return mMid.Ref();
}

void RTCRtpTransceiver::SetDirection(RTCRtpTransceiverDirection aDirection,
                                     ErrorResult& aRv) {
  // If transceiver.[[Stopping]] is true, throw an InvalidStateError.
  if (mStopping) {
    aRv.ThrowInvalidStateError("Transceiver is stopping/stopped!");
    return;
  }

  // If newDirection is equal to transceiver.[[Direction]], abort these steps.
  if (aDirection == mDirection) {
    return;
  }

  // If newDirection is equal to "stopped", throw a TypeError.
  if (aDirection == RTCRtpTransceiverDirection::Stopped) {
    aRv.ThrowTypeError("Cannot use \"stopped\" in setDirection!");
    return;
  }

  // Set transceiver.[[Direction]] to newDirection.
  SetDirectionInternal(aDirection);

  // Update the negotiation-needed flag for connection.
  mPc->UpdateNegotiationNeeded();
}

void RTCRtpTransceiver::SetDirectionInternal(
    RTCRtpTransceiverDirection aDirection) {
  // We do not update the direction on the JsepTransceiver until sync
  mDirection = aDirection;
}

bool RTCRtpTransceiver::ShouldRemove() const { return mShouldRemove; }

bool RTCRtpTransceiver::CanSendDTMF() const {
  // Spec says: "If connection's RTCPeerConnectionState is not "connected"
  // return false." We don't support that right now. This is supposed to be
  // true once ICE is complete, and _all_ DTLS handshakes are also complete. We
  // don't really have access to the state of _all_ of our DTLS states either.
  // Our pipeline _does_ know whether SRTP/SRTCP is ready, which happens
  // immediately after our transport finishes DTLS (unless there was an error),
  // so this is pretty close.
  // TODO (bug 1265827): Base this on RTCPeerConnectionState instead.
  // TODO (bug 1623193): Tighten this up
  if (!IsSending() || !mSender->GetTrack() || Stopping()) {
    return false;
  }

  // Ok, it looks like the connection is up and sending. Did we negotiate
  // telephone-event?
  const JsepTrackNegotiatedDetails* details =
      mJsepTransceiver.mSendTrack.GetNegotiatedDetails();
  if (NS_WARN_IF(!details || !details->GetEncodingCount())) {
    // What?
    return false;
  }

  for (size_t i = 0; i < details->GetEncodingCount(); ++i) {
    const auto& encoding = details->GetEncoding(i);
    for (const auto& codec : encoding.GetCodecs()) {
      if (codec->mName == "telephone-event") {
        return true;
      }
    }
  }

  return false;
}

JSObject* RTCRtpTransceiver::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return dom::RTCRtpTransceiver_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner* RTCRtpTransceiver::GetParentObject() const {
  return mWindow;
}

static void JsepCodecDescToAudioCodecConfig(
    const JsepAudioCodecDescription& aCodec, Maybe<AudioCodecConfig>* aConfig) {
  uint16_t pt;

  // TODO(bug 1761272): Getting the pt for a JsepAudioCodecDescription should be
  // infallible.
  if (NS_WARN_IF(!aCodec.GetPtAsInt(&pt))) {
    MOZ_MTLOG(ML_ERROR, "Invalid payload type: " << aCodec.mDefaultPt);
    MOZ_ASSERT(false);
    return;
  }

  // libwebrtc crashes if we attempt to configure a mono recv codec
  bool sendMono = aCodec.mForceMono && aCodec.mDirection == sdp::kSend;

  *aConfig = Some(AudioCodecConfig(
      pt, aCodec.mName, static_cast<int>(aCodec.mClock),
      sendMono ? 1 : static_cast<int>(aCodec.mChannels), aCodec.mFECEnabled));
  (*aConfig)->mMaxPlaybackRate = static_cast<int>(aCodec.mMaxPlaybackRate);
  (*aConfig)->mDtmfEnabled = aCodec.mDtmfEnabled;
  (*aConfig)->mDTXEnabled = aCodec.mDTXEnabled;
  (*aConfig)->mMaxAverageBitrate = aCodec.mMaxAverageBitrate;
  (*aConfig)->mFrameSizeMs = aCodec.mFrameSizeMs;
  (*aConfig)->mMinFrameSizeMs = aCodec.mMinFrameSizeMs;
  (*aConfig)->mMaxFrameSizeMs = aCodec.mMaxFrameSizeMs;
  (*aConfig)->mCbrEnabled = aCodec.mCbrEnabled;
}

// TODO: This and the next function probably should move to JsepTransceiver
Maybe<const std::vector<UniquePtr<JsepCodecDescription>>&>
RTCRtpTransceiver::GetNegotiatedSendCodecs() const {
  if (!mJsepTransceiver.mSendTrack.GetActive()) {
    return Nothing();
  }

  const auto* details = mJsepTransceiver.mSendTrack.GetNegotiatedDetails();
  if (!details) {
    return Nothing();
  }

  if (details->GetEncodingCount() == 0) {
    return Nothing();
  }

  return SomeRef(details->GetEncoding(0).GetCodecs());
}

Maybe<const std::vector<UniquePtr<JsepCodecDescription>>&>
RTCRtpTransceiver::GetNegotiatedRecvCodecs() const {
  if (!mJsepTransceiver.mRecvTrack.GetActive()) {
    return Nothing();
  }

  const auto* details = mJsepTransceiver.mRecvTrack.GetNegotiatedDetails();
  if (!details) {
    return Nothing();
  }

  if (details->GetEncodingCount() == 0) {
    return Nothing();
  }

  return SomeRef(details->GetEncoding(0).GetCodecs());
}

// TODO: Maybe move this someplace else?
/*static*/
void RTCRtpTransceiver::NegotiatedDetailsToAudioCodecConfigs(
    const JsepTrackNegotiatedDetails& aDetails,
    std::vector<AudioCodecConfig>* aConfigs) {
  Maybe<AudioCodecConfig> telephoneEvent;

  if (aDetails.GetEncodingCount()) {
    for (const auto& codec : aDetails.GetEncoding(0).GetCodecs()) {
      if (NS_WARN_IF(codec->Type() != SdpMediaSection::kAudio)) {
        MOZ_ASSERT(false, "Codec is not audio! This is a JSEP bug.");
        return;
      }
      Maybe<AudioCodecConfig> config;
      const JsepAudioCodecDescription& audio =
          static_cast<const JsepAudioCodecDescription&>(*codec);
      JsepCodecDescToAudioCodecConfig(audio, &config);
      if (config->mName == "telephone-event") {
        telephoneEvent = std::move(config);
      } else {
        aConfigs->push_back(std::move(*config));
      }
    }
  }

  // Put telephone event at the back, because webrtc.org crashes if we don't
  // If we need to do even more sorting, we should use std::sort.
  if (telephoneEvent) {
    aConfigs->push_back(std::move(*telephoneEvent));
  }
}

auto RTCRtpTransceiver::GetActivePayloadTypes() const
    -> RefPtr<ActivePayloadTypesPromise> {
  if (!mConduit) {
    return ActivePayloadTypesPromise::CreateAndResolve(PayloadTypes(),
                                                       __func__);
  }

  if (!mCallWrapper) {
    return ActivePayloadTypesPromise::CreateAndResolve(PayloadTypes(),
                                                       __func__);
  }

  return InvokeAsync(mCallWrapper->mCallThread, __func__,
                     [conduit = mConduit]() {
                       PayloadTypes pts;
                       pts.mSendPayloadType = conduit->ActiveSendPayloadType();
                       pts.mRecvPayloadType = conduit->ActiveRecvPayloadType();
                       return ActivePayloadTypesPromise::CreateAndResolve(
                           std::move(pts), __func__);
                     });
}

static void JsepCodecDescToVideoCodecConfig(
    const JsepVideoCodecDescription& aCodec, Maybe<VideoCodecConfig>* aConfig) {
  uint16_t pt;

  // TODO(bug 1761272): Getting the pt for a JsepVideoCodecDescription should be
  // infallible.
  if (NS_WARN_IF(!aCodec.GetPtAsInt(&pt))) {
    MOZ_MTLOG(ML_ERROR, "Invalid payload type: " << aCodec.mDefaultPt);
    MOZ_ASSERT(false);
    return;
  }

  UniquePtr<VideoCodecConfigH264> h264Config;

  if (aCodec.mName == "H264") {
    h264Config = MakeUnique<VideoCodecConfigH264>();
    size_t spropSize = sizeof(h264Config->sprop_parameter_sets);
    strncpy(h264Config->sprop_parameter_sets,
            aCodec.mSpropParameterSets.c_str(), spropSize);
    h264Config->sprop_parameter_sets[spropSize - 1] = '\0';
    h264Config->packetization_mode =
        static_cast<int>(aCodec.mPacketizationMode);
    h264Config->profile_level_id = static_cast<int>(aCodec.mProfileLevelId);
    h264Config->tias_bw = 0;  // TODO(bug 1403206)
  }

  *aConfig = Some(VideoCodecConfig(pt, aCodec.mName, aCodec.mConstraints,
                                   h264Config.get()));

  (*aConfig)->mAckFbTypes = aCodec.mAckFbTypes;
  (*aConfig)->mNackFbTypes = aCodec.mNackFbTypes;
  (*aConfig)->mCcmFbTypes = aCodec.mCcmFbTypes;
  (*aConfig)->mRembFbSet = aCodec.RtcpFbRembIsSet();
  (*aConfig)->mFECFbSet = aCodec.mFECEnabled;
  (*aConfig)->mTransportCCFbSet = aCodec.RtcpFbTransportCCIsSet();
  if (aCodec.mFECEnabled) {
    uint16_t pt;
    if (SdpHelper::GetPtAsInt(aCodec.mREDPayloadType, &pt)) {
      (*aConfig)->mREDPayloadType = pt;
    }
    if (SdpHelper::GetPtAsInt(aCodec.mULPFECPayloadType, &pt)) {
      (*aConfig)->mULPFECPayloadType = pt;
    }
    if (SdpHelper::GetPtAsInt(aCodec.mREDRTXPayloadType, &pt)) {
      (*aConfig)->mREDRTXPayloadType = pt;
    }
  }
  if (aCodec.mRtxEnabled) {
    uint16_t pt;
    if (SdpHelper::GetPtAsInt(aCodec.mRtxPayloadType, &pt)) {
      (*aConfig)->mRTXPayloadType = pt;
    }
  }
}

// TODO: Maybe move this someplace else?
/*static*/
void RTCRtpTransceiver::NegotiatedDetailsToVideoCodecConfigs(
    const JsepTrackNegotiatedDetails& aDetails,
    std::vector<VideoCodecConfig>* aConfigs) {
  if (aDetails.GetEncodingCount()) {
    for (const auto& codec : aDetails.GetEncoding(0).GetCodecs()) {
      if (NS_WARN_IF(codec->Type() != SdpMediaSection::kVideo)) {
        MOZ_ASSERT(false, "Codec is not video! This is a JSEP bug.");
        return;
      }
      Maybe<VideoCodecConfig> config;
      const JsepVideoCodecDescription& video =
          static_cast<const JsepVideoCodecDescription&>(*codec);

      JsepCodecDescToVideoCodecConfig(video, &config);

      config->mTias = aDetails.GetTias();

      for (size_t i = 0; i < aDetails.GetEncodingCount(); ++i) {
        const JsepTrackEncoding& jsepEncoding(aDetails.GetEncoding(i));
        if (jsepEncoding.HasFormat(video.mDefaultPt)) {
          VideoCodecConfig::Encoding encoding;
          encoding.rid = jsepEncoding.mRid;
          config->mEncodings.push_back(encoding);
        }
      }

      aConfigs->push_back(std::move(*config));
    }
  }
}

void RTCRtpTransceiver::Stop(ErrorResult& aRv) {
  if (mPc->IsClosed()) {
    aRv.ThrowInvalidStateError("Peer connection is closed");
    return;
  }

  if (mStopping) {
    return;
  }

  StopTransceiving();
  mPc->UpdateNegotiationNeeded();
}

void RTCRtpTransceiver::StopTransceiving() {
  if (mStopping) {
    MOZ_ASSERT(false);
    return;
  }
  mStopping = true;
  // This is the "Stop sending and receiving" algorithm from webrtc-pc
  mSender->Stop();
  mReceiver->Stop();
  mDirection = RTCRtpTransceiverDirection::Inactive;
}

void RTCRtpTransceiver::StopImpl() {
  // This is the "stop the RTCRtpTransceiver" algorithm from webrtc-pc
  if (!mStopping) {
    StopTransceiving();
  }

  if (mCallWrapper) {
    auto conduit = std::move(mConduit);
    (conduit ? conduit->Shutdown()
             : GenericPromise::CreateAndResolve(true, __func__))
        ->Then(GetMainThreadSerialEventTarget(), __func__,
               [sender = mSender, receiver = mReceiver]() mutable {
                 // Shutdown pipelines when conduits are guaranteed shut down,
                 // so that all packets sent from conduits can be delivered.
                 sender->Shutdown();
                 receiver->Shutdown();
               });
    mCallWrapper = nullptr;
  }
  mStopped = true;
  mCurrentDirection.SetNull();

  mSender->Stop();
  mReceiver->Stop();

  mHasTransport = false;

  auto self = nsMainThreadPtrHandle<RTCRtpTransceiver>(
      new nsMainThreadPtrHolder<RTCRtpTransceiver>(
          "RTCRtpTransceiver::StopImpl::self", this, false));
  mStsThread->Dispatch(NS_NewRunnableFunction(
      __func__, [self] { self->mTransportHandler = nullptr; }));
}

bool RTCRtpTransceiver::IsVideo() const { return mIsVideo; }

bool RTCRtpTransceiver::IsSending() const {
  return mCurrentDirection == Nullable(RTCRtpTransceiverDirection::Sendonly) ||
         mCurrentDirection == Nullable(RTCRtpTransceiverDirection::Sendrecv);
}

bool RTCRtpTransceiver::IsReceiving() const {
  return mCurrentDirection == Nullable(RTCRtpTransceiverDirection::Recvonly) ||
         mCurrentDirection == Nullable(RTCRtpTransceiverDirection::Sendrecv);
}

void RTCRtpTransceiver::ChainToDomPromiseWithCodecStats(
    nsTArray<RefPtr<RTCStatsPromise>> aStats,
    const RefPtr<dom::Promise>& aDomPromise) {
  nsTArray<RTCCodecStats> codecStats =
      mPc->GetCodecStats(mPc->GetTimestampMaker().GetNow().ToDom());

  AutoTArray<
      std::tuple<RTCRtpTransceiver*, RefPtr<RTCStatsPromise::AllPromiseType>>,
      1>
      statsPromises;
  statsPromises.AppendElement(std::make_tuple(
      this, RTCStatsPromise::All(GetMainThreadSerialEventTarget(), aStats)));

  ApplyCodecStats(std::move(codecStats), std::move(statsPromises))
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [aDomPromise, window = mWindow,
           idGen = mIdGenerator](UniquePtr<RTCStatsCollection> aStats) mutable {
            // Rewrite ids and merge stats collections into the final report.
            AutoTArray<UniquePtr<RTCStatsCollection>, 1> stats;
            stats.AppendElement(std::move(aStats));

            RTCStatsCollection opaqueStats;
            idGen->RewriteIds(std::move(stats), &opaqueStats);

            RefPtr<RTCStatsReport> report(new RTCStatsReport(window));
            report->Incorporate(opaqueStats);

            aDomPromise->MaybeResolve(std::move(report));
          },
          [aDomPromise](nsresult aError) {
            aDomPromise->MaybeReject(NS_ERROR_FAILURE);
          });
}

RefPtr<RTCStatsPromise> RTCRtpTransceiver::ApplyCodecStats(
    nsTArray<RTCCodecStats> aCodecStats,
    nsTArray<
        std::tuple<RTCRtpTransceiver*, RefPtr<RTCStatsPromise::AllPromiseType>>>
        aTransceiverStatsPromises) {
  MOZ_ASSERT(NS_IsMainThread());
  // The process here is roughly:
  // - Gather all inputs to the codec filtering process, including:
  //   - Each transceiver's transportIds
  //   - Each transceiver's active payload types (resolved)
  //   - Each transceiver's resolved stats
  //
  //   Waiting (async) for multiple promises of different types is not supported
  //   by the MozPromise API (bug 1752318), so we are a bit finicky here. We
  //   create media::Refcountables of the types we want to resolve, and let
  //   these be shared across Then-functions through RefPtrs.
  //
  // - For each active payload type in a transceiver:
  //   - Register the codec stats for this payload type and transport if we
  //     haven't already done so
  //   - If it was a send payload type, assign the codec stats id for this
  //     payload type and transport to the transceiver's outbound-rtp and
  //     remote-inbound-rtp stats as codecId
  //   - If it was a recv payload type, assign the codec stats id for this
  //     payload type and transport to the transceiver's inbound-rtp and
  //     remote-outbound-rtp stats as codecId
  //
  // - Flatten all transceiver stats collections into one, and set the
  //   registered codec stats on it

  // Wrap codec stats in a Refcountable<> to allow sharing across promise
  // handlers.
  auto codecStats = MakeRefPtr<media::Refcountable<nsTArray<RTCCodecStats>>>();
  *codecStats = std::move(aCodecStats);

  struct IdComparator {
    bool operator()(const RTCCodecStats& aA, const RTCCodecStats& aB) const {
      return aA.mId.Value() < aB.mId.Value();
    }
  };

  // Stores distinct codec stats by id; to avoid dupes within a transport.
  auto finalCodecStats =
      MakeRefPtr<media::Refcountable<std::set<RTCCodecStats, IdComparator>>>();

  // All the transceiver rtp stream stats in a single array. These stats will,
  // when resolved, contain codecIds.
  nsTArray<RefPtr<RTCStatsPromise>> promises(
      aTransceiverStatsPromises.Length());

  for (const auto& [transceiver, allPromise] : aTransceiverStatsPromises) {
    // Per transceiver, gather up what we need to assign codecId to this
    // transceiver's rtp stream stats. Register codec stats while we're at it.
    auto payloadTypes =
        MakeRefPtr<media::Refcountable<RTCRtpTransceiver::PayloadTypes>>();
    promises.AppendElement(
        transceiver->GetActivePayloadTypes()
            ->Then(
                GetMainThreadSerialEventTarget(), __func__,
                [payloadTypes, allPromise = allPromise](
                    RTCRtpTransceiver::PayloadTypes aPayloadTypes) {
                  // Forward active payload types to the next Then-handler.
                  *payloadTypes = std::move(aPayloadTypes);
                  return allPromise;
                },
                [] {
                  MOZ_CRASH("Unexpected reject");
                  return RTCStatsPromise::AllPromiseType::CreateAndReject(
                      NS_ERROR_UNEXPECTED, __func__);
                })
            ->Then(
                GetMainThreadSerialEventTarget(), __func__,
                [codecStats, finalCodecStats, payloadTypes,
                 transportId =
                     NS_ConvertASCIItoUTF16(transceiver->GetTransportId())](
                    nsTArray<UniquePtr<RTCStatsCollection>>
                        aTransceiverStats) mutable {
                  // We have all the data we need to register codec stats and
                  // assign codecIds for this transceiver's rtp stream stats.

                  auto report = MakeUnique<RTCStatsCollection>();
                  FlattenStats(std::move(aTransceiverStats), report.get());

                  // Find the codec stats we are looking for, based on the
                  // transportId and the active payload types.
                  Maybe<RTCCodecStats&> sendCodec;
                  Maybe<RTCCodecStats&> recvCodec;
                  for (auto& codec : *codecStats) {
                    if (payloadTypes->mSendPayloadType.isSome() ==
                            sendCodec.isSome() &&
                        payloadTypes->mRecvPayloadType.isSome() ==
                            recvCodec.isSome()) {
                      // We have found all the codec stats we were looking for.
                      break;
                    }
                    if (codec.mTransportId != transportId) {
                      continue;
                    }
                    if (payloadTypes->mSendPayloadType &&
                        *payloadTypes->mSendPayloadType ==
                            static_cast<int>(codec.mPayloadType) &&
                        (!codec.mCodecType.WasPassed() ||
                         codec.mCodecType.Value() == RTCCodecType::Encode)) {
                      MOZ_ASSERT(!sendCodec,
                                 "At most one send codec stat per transceiver");
                      sendCodec = SomeRef(codec);
                    }
                    if (payloadTypes->mRecvPayloadType &&
                        *payloadTypes->mRecvPayloadType ==
                            static_cast<int>(codec.mPayloadType) &&
                        (!codec.mCodecType.WasPassed() ||
                         codec.mCodecType.Value() == RTCCodecType::Decode)) {
                      MOZ_ASSERT(!recvCodec,
                                 "At most one recv codec stat per transceiver");
                      recvCodec = SomeRef(codec);
                    }
                  }

                  // Register and assign codecIds for the found codec stats.
                  if (sendCodec) {
                    finalCodecStats->insert(*sendCodec);
                    for (auto& stat : report->mOutboundRtpStreamStats) {
                      stat.mCodecId.Construct(sendCodec->mId.Value());
                    }
                    for (auto& stat : report->mRemoteInboundRtpStreamStats) {
                      stat.mCodecId.Construct(sendCodec->mId.Value());
                    }
                  }
                  if (recvCodec) {
                    finalCodecStats->insert(*recvCodec);
                    for (auto& stat : report->mInboundRtpStreamStats) {
                      stat.mCodecId.Construct(recvCodec->mId.Value());
                    }
                    for (auto& stat : report->mRemoteOutboundRtpStreamStats) {
                      stat.mCodecId.Construct(recvCodec->mId.Value());
                    }
                  }

                  return RTCStatsPromise::CreateAndResolve(std::move(report),
                                                           __func__);
                },
                [] {
                  MOZ_CRASH("Unexpected reject");
                  return RTCStatsPromise::CreateAndReject(NS_ERROR_UNEXPECTED,
                                                          __func__);
                }));
  }

  return RTCStatsPromise::All(GetMainThreadSerialEventTarget(), promises)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [finalCodecStats = std::move(finalCodecStats)](
              nsTArray<UniquePtr<RTCStatsCollection>> aStats) mutable {
            auto finalStats = MakeUnique<RTCStatsCollection>();
            FlattenStats(std::move(aStats), finalStats.get());
            MOZ_ASSERT(finalStats->mCodecStats.IsEmpty());
            if (!finalStats->mCodecStats.SetCapacity(finalCodecStats->size(),
                                                     fallible)) {
              mozalloc_handle_oom(0);
            }
            while (!finalCodecStats->empty()) {
              auto node = finalCodecStats->extract(finalCodecStats->begin());
              if (!finalStats->mCodecStats.AppendElement(
                      std::move(node.value()), fallible)) {
                mozalloc_handle_oom(0);
              }
            }
            return RTCStatsPromise::CreateAndResolve(std::move(finalStats),
                                                     __func__);
          },
          [] {
            MOZ_CRASH("Unexpected reject");
            return RTCStatsPromise::CreateAndReject(NS_ERROR_UNEXPECTED,
                                                    __func__);
          });
}

}  // namespace mozilla
