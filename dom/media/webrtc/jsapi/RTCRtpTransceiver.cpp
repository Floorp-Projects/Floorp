/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi/RTCRtpTransceiver.h"
#include "mozilla/UniquePtr.h"
#include <algorithm>
#include <string>
#include <vector>
#include "libwebrtcglue/AudioConduit.h"
#include "libwebrtcglue/VideoConduit.h"
#include "MediaTrackGraph.h"
#include "transportbridge/MediaPipeline.h"
#include "transportbridge/MediaPipelineFilter.h"
#include "jsep/JsepTrack.h"
#include "sdp/SdpHelper.h"
#include "MediaTrackGraphImpl.h"
#include "transport/logging.h"
#include "MediaEngine.h"
#include "nsIPrincipal.h"
#include "MediaSegment.h"
#include "RemoteTrackSource.h"
#include "libwebrtcglue/RtpRtcpConfig.h"
#include "MediaTransportHandler.h"
#include "mozilla/dom/RTCRtpReceiverBinding.h"
#include "mozilla/dom/RTCRtpSenderBinding.h"
#include "mozilla/dom/RTCRtpTransceiverBinding.h"
#include "mozilla/dom/Promise.h"
#include "RTCDtlsTransport.h"
#include "RTCRtpReceiver.h"
#include "RTCRtpSender.h"
#include "RTCDTMFSender.h"
#include "systemservices/MediaUtils.h"
#include "libwebrtcglue/WebrtcCallWrapper.h"
#include "libwebrtcglue/WebrtcGmpVideoCodec.h"

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
  AbstractCanonical<bool>* CanonicalReceiving() override {
    return mReceiver->CanonicalReceiving();
  }
  AbstractCanonical<bool>* CanonicalTransmitting() override {
    return mSender->CanonicalTransmitting();
  }
  AbstractCanonical<Ssrcs>* CanonicalLocalSsrcs() override {
    return mSender->CanonicalSsrcs();
  }
  AbstractCanonical<std::string>* CanonicalLocalCname() override {
    return mSender->CanonicalCname();
  }
  AbstractCanonical<std::string>* CanonicalMid() override {
    return mTransceiver->CanonicalMid();
  }
  AbstractCanonical<Ssrc>* CanonicalRemoteSsrc() override {
    return mReceiver->CanonicalSsrc();
  }
  AbstractCanonical<std::string>* CanonicalSyncGroup() override {
    return mTransceiver->CanonicalSyncGroup();
  }
  AbstractCanonical<RtpExtList>* CanonicalLocalRecvRtpExtensions() override {
    return mReceiver->CanonicalLocalRtpExtensions();
  }
  AbstractCanonical<RtpExtList>* CanonicalLocalSendRtpExtensions() override {
    return mSender->CanonicalLocalRtpExtensions();
  }

  // AudioConduitControlInterface
  AbstractCanonical<Maybe<AudioCodecConfig>>* CanonicalAudioSendCodec()
      override {
    return mSender->CanonicalAudioCodec();
  }
  AbstractCanonical<std::vector<AudioCodecConfig>>* CanonicalAudioRecvCodecs()
      override {
    return mReceiver->CanonicalAudioCodecs();
  }
  MediaEventSource<DtmfEvent>& OnDtmfEvent() override {
    return mSender->GetDtmf()->OnDtmfEvent();
  }

  // VideoConduitControlInterface
  AbstractCanonical<Ssrcs>* CanonicalLocalVideoRtxSsrcs() override {
    return mSender->CanonicalVideoRtxSsrcs();
  }
  AbstractCanonical<Ssrc>* CanonicalRemoteVideoRtxSsrc() override {
    return mReceiver->CanonicalVideoRtxSsrc();
  }
  AbstractCanonical<Maybe<VideoCodecConfig>>* CanonicalVideoSendCodec()
      override {
    return mSender->CanonicalVideoCodec();
  }
  AbstractCanonical<Maybe<RtpRtcpConfig>>* CanonicalVideoSendRtpRtcpConfig()
      override {
    return mSender->CanonicalVideoRtpRtcpConfig();
  }
  AbstractCanonical<std::vector<VideoCodecConfig>>* CanonicalVideoRecvCodecs()
      override {
    return mReceiver->CanonicalVideoCodecs();
  }
  AbstractCanonical<Maybe<RtpRtcpConfig>>* CanonicalVideoRecvRtpRtcpConfig()
      override {
    return mReceiver->CanonicalVideoRtpRtcpConfig();
  }
  AbstractCanonical<webrtc::VideoCodecMode>* CanonicalVideoCodecMode()
      override {
    return mSender->CanonicalVideoCodecMode();
  }
};
}  // namespace

MOZ_MTLOG_MODULE("RTCRtpTransceiver")

NS_IMPL_CYCLE_COLLECTION_CLASS(RTCRtpTransceiver)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(RTCRtpTransceiver)
  // We do not unlink mPc from here; PeerConnectionImpl invokes the necessary
  // teardown code itself during unlink.
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow, mReceiver, mSendTrack, mSender,
                                  mDtlsTransport, mLastStableDtlsTransport)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(RTCRtpTransceiver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow, mPc, mSendTrack, mReceiver,
                                    mSender, mDtlsTransport,
                                    mLastStableDtlsTransport)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(RTCRtpTransceiver)

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
    MediaTransportHandler* aTransportHandler, JsepTransceiver* aJsepTransceiver,
    nsISerialEventTarget* aStsThread, dom::MediaStreamTrack* aSendTrack,
    WebrtcCallWrapper* aCallWrapper, RTCStatsIdGenerator* aIdGenerator)
    : mWindow(aWindow),
      mPc(aPc),
      mTransportHandler(aTransportHandler),
      mJsepTransceiver(aJsepTransceiver),
      mStsThread(aStsThread),
      mCallWrapper(aCallWrapper),
      mSendTrack(aSendTrack),
      mIdGenerator(aIdGenerator),
      mPrivacyNeeded(aPrivacyNeeded),
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
      return SdpDirectionAttribute::Direction::kInactive;
    case dom::RTCRtpTransceiverDirection::EndGuard_:;
  }
  MOZ_CRASH("Invalid transceiver direction!");
}

// TODO(bug 1401592): Once we implement the sendEncodings stuff, there will
// need to be validation code in here.
void RTCRtpTransceiver::Init(const RTCRtpTransceiverInit& aInit,
                             ErrorResult& aRv) {
  if (IsVideo()) {
    InitVideo();
  } else {
    InitAudio();
  }

  if (!IsValid()) {
    aRv = NS_ERROR_UNEXPECTED;
    return;
  }

  mReceiver = new RTCRtpReceiver(
      mWindow, mPrivacyNeeded, mPc, mTransportHandler, mJsepTransceiver,
      mCallWrapper->mCallThread, mStsThread, mConduit, this);

  mSender = new RTCRtpSender(mWindow, mPc, mTransportHandler, mJsepTransceiver,
                             mCallWrapper->mCallThread, mStsThread, mConduit,
                             mSendTrack, this);

  if (mConduit) {
    InitConduitControl();
  }

  auto self = nsMainThreadPtrHandle<RTCRtpTransceiver>(
      new nsMainThreadPtrHolder<RTCRtpTransceiver>(
          "RTCRtpTransceiver::RTCRtpTransceiver::self", this, false));
  mStsThread->Dispatch(
      NS_NewRunnableFunction("RTCRtpTransceiver::RTCRtpTransceiver", [self] {
        self->mTransportHandler->SignalStateChange.connect(
            self.get(), &RTCRtpTransceiver::UpdateDtlsTransportState);
        self->mTransportHandler->SignalRtcpStateChange.connect(
            self.get(), &RTCRtpTransceiver::UpdateDtlsTransportState);
      }));

  // TODO(bug 1401592): apply aInit.mSendEncodings to mSender
  mSender->SetStreams(aInit.mStreams);
  mDirection = aInit.mDirection;
  mJsepTransceiver->mJsDirection = ToSdpDirection(mDirection);
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

void RTCRtpTransceiver::UpdateDtlsTransportState(
    const std::string& aTransportId, TransportLayer::State aState) {
  if (!GetMainThreadEventTarget()->IsOnCurrentThread()) {
    GetMainThreadEventTarget()->Dispatch(
        WrapRunnable(this, &RTCRtpTransceiver::UpdateDtlsTransportState,
                     aTransportId, aState),
        NS_DISPATCH_NORMAL);
    return;
  }

  if (!mDtlsTransport) {
    return;
  }

  mDtlsTransport->UpdateState(aState);
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

void RTCRtpTransceiver::InitVideo() {
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

  mConduit = VideoSessionConduit::Create(mCallWrapper, mStsThread,
                                         std::move(options), mPc->GetHandle());

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
  mCallWrapper->mCallThread->Dispatch(NS_NewRunnableFunction(
      __func__, [conduit = mConduit, control = std::move(control)]() mutable {
        conduit->AsVideoSessionConduit().apply(
            [&](VideoSessionConduit* aConduit) {
              aConduit->InitControl(&control);
            });
        conduit->AsAudioSessionConduit().apply(
            [&](AudioSessionConduit* aConduit) {
              aConduit->InitControl(&control);
            });
      }));
}

void RTCRtpTransceiver::Shutdown_m() {
  // Called via PCImpl::Close -> PCImpl::CloseInt -> PCImpl::ShutdownMedia ->
  // PCMedia::SelfDestruct.  Satisfies step 7 of
  // https://w3c.github.io/webrtc-pc/#dom-rtcpeerconnection-close
  mShutdown = true;
  if (mDtlsTransport) {
    mDtlsTransport->UpdateState(TransportLayer::TS_CLOSED);
  }
  StopImpl();
}

nsresult RTCRtpTransceiver::UpdateTransport() {
  if (!mJsepTransceiver->HasLevel() || mJsepTransceiver->IsStopped()) {
    return NS_OK;
  }

  mReceiver->UpdateTransport();
  mSender->UpdateTransport();
  return NS_OK;
}

nsresult RTCRtpTransceiver::UpdateConduit() {
  if (mJsepTransceiver->IsStopped()) {
    return NS_OK;
  }

  if (mJsepTransceiver->IsAssociated()) {
    mMid = mJsepTransceiver->GetMid();
  } else {
    mMid = std::string();
  }

  mReceiver->UpdateConduit();
  mSender->UpdateConduit();

  return NS_OK;
}

void RTCRtpTransceiver::ResetSync() { mSyncGroup = std::string(); }

nsresult RTCRtpTransceiver::SyncWithMatchingVideoConduits(
    nsTArray<RefPtr<RTCRtpTransceiver>>& transceivers) {
  if (mJsepTransceiver->IsStopped()) {
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
  myReceiveStreamIds.insert(mJsepTransceiver->mRecvTrack.GetStreamIds().begin(),
                            mJsepTransceiver->mRecvTrack.GetStreamIds().end());

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
         transceiver->mJsepTransceiver->mRecvTrack.GetStreamIds()) {
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

void RTCRtpTransceiver::SyncWithJsep() {
  MOZ_MTLOG(ML_DEBUG, mPc->GetHandle()
                          << "[" << mMid.Ref() << "]: " << __FUNCTION__
                          << " Syncing with JSEP transceiver");

  if (mShutdown) {
    // Shutdown_m has already been called, probably due to pc.close(). Just
    // nod and smile.
    return;
  }

  // Transceivers can stop due to JSEP negotiation, so we need to check that
  if (mJsepTransceiver->IsStopped()) {
    StopImpl();
  }

  // If a SRD has unset the receive bit, stop the receive pipeline so incoming
  // RTP does not unmute the receive track.
  if (!mJsepTransceiver->mRecvTrack.GetRemoteSetSendBit() ||
      !mJsepTransceiver->mRecvTrack.GetActive()) {
    mReceiver->Stop();
  }

  // mid from JSEP
  if (mJsepTransceiver->IsAssociated()) {
    mMid = mJsepTransceiver->GetMid();
  } else {
    mMid = std::string();
  }

  // currentDirection from JSEP, but not if "this transceiver has never been
  // represented in an offer/answer exchange"
  if (mJsepTransceiver->HasLevel() && mJsepTransceiver->IsNegotiated()) {
    if (IsReceiving()) {
      if (IsSending()) {
        mCurrentDirection.SetValue(dom::RTCRtpTransceiverDirection::Sendrecv);
        mHasBeenUsedToSend = true;
      } else {
        mCurrentDirection.SetValue(dom::RTCRtpTransceiverDirection::Recvonly);
      }
    } else {
      if (IsSending()) {
        mCurrentDirection.SetValue(dom::RTCRtpTransceiverDirection::Sendonly);
        mHasBeenUsedToSend = true;
      } else {
        mCurrentDirection.SetValue(dom::RTCRtpTransceiverDirection::Inactive);
      }
    }
  }
}

void RTCRtpTransceiver::GetKind(nsAString& aKind) const {
  // The transceiver kind of an RTCRtpTransceiver is defined by the kind of the
  // associated RTCRtpReceiver's MediaStreamTrack object.
  mReceiver->Track()->GetKind(aKind);
}

void RTCRtpTransceiver::GetMid(nsAString& aMid) const {
  if (!mMid.Ref().empty()) {
    aMid = NS_ConvertUTF8toUTF16(mMid.Ref());
  } else {
    aMid.SetIsVoid(true);
  }
}

void RTCRtpTransceiver::SetDirection(RTCRtpTransceiverDirection aDirection,
                                     ErrorResult& aRv) {
  if (mStopped) {
    aRv.ThrowInvalidStateError("Transceiver is stopped!");
    return;
  }

  if (aDirection == mDirection) {
    return;
  }

  SetDirectionInternal(aDirection);

  mPc->UpdateNegotiationNeeded();
}

void RTCRtpTransceiver::SetDirectionInternal(
    RTCRtpTransceiverDirection aDirection) {
  mJsepTransceiver->mJsDirection = ToSdpDirection(aDirection);
  mDirection = aDirection;
}

void RTCRtpTransceiver::SetAddTrackMagic() {
  // TODO(bug 1767820): Refactor this to only forbid removal, not to set the
  // magic bit
  mJsepTransceiver->SetAddTrackMagic();
}

bool RTCRtpTransceiver::ShouldRemove() const {
  return mJsepTransceiver->IsRemoved();
}

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
  if (!IsSending() || !mSender->GetTrack()) {
    return false;
  }

  // Ok, it looks like the connection is up and sending. Did we negotiate
  // telephone-event?
  JsepTrackNegotiatedDetails* details =
      mJsepTransceiver->mSendTrack.GetNegotiatedDetails();
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

  *aConfig = Some(AudioCodecConfig(
      pt, aCodec.mName, static_cast<int>(aCodec.mClock),
      aCodec.mForceMono ? 1 : static_cast<int>(aCodec.mChannels),
      aCodec.mFECEnabled));
  (*aConfig)->mMaxPlaybackRate = static_cast<int>(aCodec.mMaxPlaybackRate);
  (*aConfig)->mDtmfEnabled = aCodec.mDtmfEnabled;
  (*aConfig)->mDTXEnabled = aCodec.mDTXEnabled;
  (*aConfig)->mMaxAverageBitrate = aCodec.mMaxAverageBitrate;
  (*aConfig)->mFrameSizeMs = aCodec.mFrameSizeMs;
  (*aConfig)->mMinFrameSizeMs = aCodec.mMinFrameSizeMs;
  (*aConfig)->mMaxFrameSizeMs = aCodec.mMaxFrameSizeMs;
  (*aConfig)->mCbrEnabled = aCodec.mCbrEnabled;
}

Maybe<const std::vector<UniquePtr<JsepCodecDescription>>&>
RTCRtpTransceiver::GetNegotiatedSendCodecs() const {
  if (!IsSending()) {
    return Nothing();
  }

  const auto* details = mJsepTransceiver->mSendTrack.GetNegotiatedDetails();
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
  if (!IsReceiving()) {
    return Nothing();
  }

  const auto* details = mJsepTransceiver->mRecvTrack.GetNegotiatedDetails();
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
          encoding.constraints = jsepEncoding.mConstraints;
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

  StopImpl();
  mPc->UpdateNegotiationNeeded();
}

void RTCRtpTransceiver::StopImpl() {
  if (mStopped) {
    return;
  }
  mSender->Stop();
  mReceiver->Stop();

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
  mJsepTransceiver->Stop();
  auto self = nsMainThreadPtrHandle<RTCRtpTransceiver>(
      new nsMainThreadPtrHolder<RTCRtpTransceiver>(
          "RTCRtpTransceiver::StopImpl::self", this, false));
  mStsThread->Dispatch(NS_NewRunnableFunction(__func__, [self] {
    self->disconnect_all();
    self->mTransportHandler = nullptr;
  }));
}

bool RTCRtpTransceiver::IsVideo() const {
  return mJsepTransceiver->GetMediaType() == SdpMediaSection::MediaType::kVideo;
}

void RTCRtpTransceiver::ChainToDomPromiseWithCodecStats(
    nsTArray<RefPtr<RTCStatsPromise>> aStats,
    const RefPtr<dom::Promise>& aDomPromise) {
  nsTArray<RTCCodecStats> codecStats =
      mPc->GetCodecStats(mPc->GetTimestampMaker().GetNow());

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
