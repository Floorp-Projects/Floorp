/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi/TransceiverImpl.h"
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
#include "mozilla/dom/TransceiverImplBinding.h"
#include "RTCDtlsTransport.h"
#include "RTCRtpReceiver.h"
#include "RTCDTMFSender.h"
#include "libwebrtcglue/WebrtcGmpVideoCodec.h"

namespace mozilla {

using namespace dom;

namespace {
struct ConduitControlState : public AudioConduitControlInterface,
                             public VideoConduitControlInterface {
  ConduitControlState(TransceiverImpl* aTransceiver, RTCDTMFSender* aDtmf,
                      RTCRtpReceiver* aReceiver)
      : mTransceiver(new nsMainThreadPtrHolder<TransceiverImpl>(
            "ConduitControlState::mTransceiver", aTransceiver, false)),
        mDtmf(new nsMainThreadPtrHolder<dom::RTCDTMFSender>(
            "ConduitControlState::mDtmf", aDtmf, false)),
        mReceiver(new nsMainThreadPtrHolder<dom::RTCRtpReceiver>(
            "ConduitControlState::mReceiver", aReceiver, false)) {}

  const nsMainThreadPtrHandle<TransceiverImpl> mTransceiver;
  const nsMainThreadPtrHandle<RTCDTMFSender> mDtmf;
  const nsMainThreadPtrHandle<RTCRtpReceiver> mReceiver;

  // MediaConduitControlInterface
  AbstractCanonical<bool>* CanonicalReceiving() override {
    return mTransceiver->CanonicalReceiving();
  }
  AbstractCanonical<bool>* CanonicalTransmitting() override {
    return mTransceiver->CanonicalTransmitting();
  }
  AbstractCanonical<Ssrcs>* CanonicalLocalSsrcs() override {
    return mTransceiver->CanonicalLocalSsrcs();
  }
  AbstractCanonical<std::string>* CanonicalLocalCname() override {
    return mTransceiver->CanonicalLocalCname();
  }
  AbstractCanonical<std::string>* CanonicalLocalMid() override {
    return mTransceiver->CanonicalLocalMid();
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
    return mTransceiver->CanonicalLocalSendRtpExtensions();
  }

  // AudioConduitControlInterface
  AbstractCanonical<Maybe<AudioCodecConfig>>* CanonicalAudioSendCodec()
      override {
    return mTransceiver->CanonicalAudioSendCodec();
  }
  AbstractCanonical<std::vector<AudioCodecConfig>>* CanonicalAudioRecvCodecs()
      override {
    return mReceiver->CanonicalAudioCodecs();
  }
  MediaEventSource<DtmfEvent>& OnDtmfEvent() override {
    return mDtmf->OnDtmfEvent();
  }

  // VideoConduitControlInterface
  AbstractCanonical<Ssrcs>* CanonicalLocalVideoRtxSsrcs() override {
    return mTransceiver->CanonicalLocalVideoRtxSsrcs();
  }
  AbstractCanonical<Ssrc>* CanonicalRemoteVideoRtxSsrc() override {
    return mReceiver->CanonicalVideoRtxSsrc();
  }
  AbstractCanonical<Maybe<VideoCodecConfig>>* CanonicalVideoSendCodec()
      override {
    return mTransceiver->CanonicalVideoSendCodec();
  }
  AbstractCanonical<Maybe<RtpRtcpConfig>>* CanonicalVideoSendRtpRtcpConfig()
      override {
    return mTransceiver->CanonicalVideoSendRtpRtcpConfig();
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
    return mTransceiver->CanonicalVideoCodecMode();
  }
};
}  // namespace

MOZ_MTLOG_MODULE("transceiverimpl")

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TransceiverImpl, mWindow, mSendTrack,
                                      mReceiver, mDtmf, mDtlsTransport,
                                      mLastStableDtlsTransport)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TransceiverImpl)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TransceiverImpl)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TransceiverImpl)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

#define INIT_CANONICAL(name, val)         \
  name(AbstractThread::MainThread(), val, \
       "TransceiverImpl::" #name " (Canonical)")

TransceiverImpl::TransceiverImpl(
    nsPIDOMWindowInner* aWindow, bool aPrivacyNeeded,
    const std::string& aPCHandle, MediaTransportHandler* aTransportHandler,
    JsepTransceiver* aJsepTransceiver, nsISerialEventTarget* aMainThread,
    nsISerialEventTarget* aStsThread, dom::MediaStreamTrack* aSendTrack,
    WebrtcCallWrapper* aCallWrapper)
    : mWindow(aWindow),
      mPCHandle(aPCHandle),
      mTransportHandler(aTransportHandler),
      mJsepTransceiver(aJsepTransceiver),
      mHaveSetupTransport(false),
      mMainThread(aMainThread),
      mStsThread(aStsThread),
      mSendTrack(aSendTrack),
      mCallWrapper(aCallWrapper),
      INIT_CANONICAL(mReceiving, false),
      INIT_CANONICAL(mTransmitting, false),
      INIT_CANONICAL(mLocalSsrcs, Ssrcs()),
      INIT_CANONICAL(mLocalVideoRtxSsrcs, Ssrcs()),
      INIT_CANONICAL(mLocalCname, std::string()),
      INIT_CANONICAL(mLocalMid, std::string()),
      INIT_CANONICAL(mSyncGroup, std::string()),
      INIT_CANONICAL(mLocalSendRtpExtensions, RtpExtList()),
      INIT_CANONICAL(mAudioSendCodec, Nothing()),
      INIT_CANONICAL(mVideoSendCodec, Nothing()),
      INIT_CANONICAL(mVideoSendRtpRtcpConfig, Nothing()),
      INIT_CANONICAL(mVideoCodecMode, webrtc::VideoCodecMode::kRealtimeVideo) {
  if (IsVideo()) {
    InitVideo();
  } else {
    InitAudio();
  }

  if (!IsValid()) {
    return;
  }

  mReceiver = new RTCRtpReceiver(
      aWindow, aPrivacyNeeded, aPCHandle, aTransportHandler, aJsepTransceiver,
      aMainThread, mCallWrapper->mCallThread, aStsThread, mConduit, this);

  if (!IsVideo()) {
    mDtmf = new RTCDTMFSender(aWindow, this);
  }

  mTransmitPipeline = new MediaPipelineTransmit(
      mPCHandle, mTransportHandler, mMainThread.get(),
      mCallWrapper->mCallThread.get(), mStsThread.get(), IsVideo(), mConduit);

  mTransmitPipeline->SetTrack(mSendTrack);

  if (mConduit) {
    InitConduitControl();
  }

  auto self = nsMainThreadPtrHandle<TransceiverImpl>(
      new nsMainThreadPtrHolder<TransceiverImpl>(
          "TransceiverImpl::TransceiverImpl::self", this, false));
  mStsThread->Dispatch(
      NS_NewRunnableFunction("TransceiverImpl::TransceiverImpl", [self] {
        self->mTransportHandler->SignalStateChange.connect(
            self.get(), &TransceiverImpl::UpdateDtlsTransportState);
        self->mTransportHandler->SignalRtcpStateChange.connect(
            self.get(), &TransceiverImpl::UpdateDtlsTransportState);
      }));
}

#undef INIT_CANONICAL

TransceiverImpl::~TransceiverImpl() = default;

void TransceiverImpl::SetDtlsTransport(dom::RTCDtlsTransport* aDtlsTransport,
                                       bool aStable) {
  mDtlsTransport = aDtlsTransport;
  if (aStable) {
    mLastStableDtlsTransport = mDtlsTransport;
  }
}

void TransceiverImpl::RollbackToStableDtlsTransport() {
  mDtlsTransport = mLastStableDtlsTransport;
}

void TransceiverImpl::UpdateDtlsTransportState(const std::string& aTransportId,
                                               TransportLayer::State aState) {
  if (!mMainThread->IsOnCurrentThread()) {
    mMainThread->Dispatch(
        WrapRunnable(this, &TransceiverImpl::UpdateDtlsTransportState,
                     aTransportId, aState),
        NS_DISPATCH_NORMAL);
    return;
  }

  if (!mDtlsTransport) {
    return;
  }

  mDtlsTransport->UpdateState(aState);
}

void TransceiverImpl::InitAudio() {
  mConduit = AudioSessionConduit::Create(mCallWrapper, mStsThread);

  if (!mConduit) {
    MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mLocalMid.Ref()
                                  << "]: " << __FUNCTION__
                                  << ": Failed to create AudioSessionConduit");
    // TODO(bug 1422897): We need a way to record this when it happens in the
    // wild.
  }
}

void TransceiverImpl::InitVideo() {
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
                                         std::move(options), mPCHandle);

  if (!mConduit) {
    MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mLocalMid.Ref()
                                  << "]: " << __FUNCTION__
                                  << ": Failed to create VideoSessionConduit");
    // TODO(bug 1422897): We need a way to record this when it happens in the
    // wild.
  }
}

void TransceiverImpl::InitConduitControl() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mConduit);
  ConduitControlState control(this, mDtmf, mReceiver);
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

nsresult TransceiverImpl::UpdateSinkIdentity(
    const dom::MediaStreamTrack* aTrack, nsIPrincipal* aPrincipal,
    const PeerIdentity* aSinkIdentity) {
  if (mJsepTransceiver->IsStopped()) {
    return NS_OK;
  }

  mTransmitPipeline->UpdateSinkIdentity_m(aTrack, aPrincipal, aSinkIdentity);
  return NS_OK;
}

void TransceiverImpl::Shutdown_m() {
  // Called via PCImpl::Close -> PCImpl::CloseInt -> PCImpl::ShutdownMedia ->
  // PCMedia::SelfDestruct.  Satisfies step 7 of
  // https://w3c.github.io/webrtc-pc/#dom-rtcpeerconnection-close
  if (mDtlsTransport) {
    mDtlsTransport->UpdateState(TransportLayer::TS_CLOSED);
  }
  Stop();
  mTransmitPipeline = nullptr;
  auto self = nsMainThreadPtrHandle<TransceiverImpl>(
      new nsMainThreadPtrHolder<TransceiverImpl>(
          "TransceiverImpl::Shutdown_m::self", this, false));
  mStsThread->Dispatch(NS_NewRunnableFunction(__func__, [self] {
    self->disconnect_all();
    self->mTransportHandler = nullptr;
  }));
}

nsresult TransceiverImpl::UpdateSendTrack(dom::MediaStreamTrack* aSendTrack) {
  if (mJsepTransceiver->IsStopped()) {
    return NS_ERROR_UNEXPECTED;
  }

  MOZ_MTLOG(ML_DEBUG, mPCHandle << "[" << mLocalMid.Ref() << "]: "
                                << __FUNCTION__ << "(" << aSendTrack << ")");
  mSendTrack = aSendTrack;
  return mTransmitPipeline->SetTrack(mSendTrack);
}

nsresult TransceiverImpl::UpdateTransport() {
  if (!mJsepTransceiver->HasLevel() || mJsepTransceiver->IsStopped()) {
    return NS_OK;
  }

  mReceiver->UpdateTransport();

  if (!mHaveSetupTransport) {
    mTransmitPipeline->SetLevel(mJsepTransceiver->GetLevel());
    mHaveSetupTransport = true;
  }

  mTransmitPipeline->UpdateTransport_m(
      mJsepTransceiver->mTransport.mTransportId, nullptr);
  return NS_OK;
}

nsresult TransceiverImpl::UpdateConduit() {
  if (mJsepTransceiver->IsStopped()) {
    return NS_OK;
  }

  if (mJsepTransceiver->IsAssociated()) {
    mLocalMid = mJsepTransceiver->GetMid();
  } else {
    mLocalMid = std::string();
  }

  mReceiving = false;
  mReceiver->Stop();

  mTransmitting = false;
  mTransmitPipeline->Stop();

  // NOTE(pkerr) - the Call API requires the both local_ssrc and remote_ssrc be
  // set to a non-zero value or the CreateVideo...Stream call will fail.
  if (mJsepTransceiver->mSendTrack.GetSsrcs().empty()) {
    MOZ_MTLOG(ML_ERROR,
              mPCHandle << "[" << mLocalMid.Ref() << "]: " << __FUNCTION__
                        << " No local SSRC set! (Should be set regardless of "
                           "whether we're sending RTP; we need a local SSRC in "
                           "all cases)");
    return NS_ERROR_FAILURE;
  }

  mLocalSsrcs = mJsepTransceiver->mSendTrack.GetSsrcs();
  mLocalVideoRtxSsrcs = mJsepTransceiver->mSendTrack.GetRtxSsrcs();
  mLocalCname = mJsepTransceiver->mSendTrack.GetCNAME();

  if (nsresult rv = mReceiver->UpdateConduit(); NS_FAILED(rv)) {
    return rv;
  }

  // TODO(bug 1616937): Move this stuff into RTCRtpSender.
  if (nsresult rv = IsVideo() ? UpdateVideoConduit() : UpdateAudioConduit();
      NS_FAILED(rv)) {
    return rv;
  }

  if ((mReceiving = mJsepTransceiver->mRecvTrack.GetActive())) {
    mReceiver->Start();
  }

  if ((mTransmitting = mJsepTransceiver->mSendTrack.GetActive())) {
    if (!mSendTrack) {
      MOZ_MTLOG(ML_WARNING,
                mPCHandle << "[" << mLocalMid.Ref() << "]: " << __FUNCTION__
                          << " Starting transmit conduit without send track!");
    }

    mTransmitPipeline->Start();
  }

  return NS_OK;
}

void TransceiverImpl::ResetSync() { mSyncGroup = std::string(); }

nsresult TransceiverImpl::SyncWithMatchingVideoConduits(
    std::vector<RefPtr<TransceiverImpl>>& transceivers) {
  if (mJsepTransceiver->IsStopped()) {
    return NS_OK;
  }

  if (IsVideo()) {
    MOZ_MTLOG(ML_ERROR, mPCHandle << "[" << mLocalMid.Ref()
                                  << "]: " << __FUNCTION__
                                  << " called when transceiver is not "
                                     "video! This should never happen.");
    MOZ_CRASH();
    return NS_ERROR_UNEXPECTED;
  }

  std::set<std::string> myReceiveStreamIds;
  myReceiveStreamIds.insert(mJsepTransceiver->mRecvTrack.GetStreamIds().begin(),
                            mJsepTransceiver->mRecvTrack.GetStreamIds().end());

  for (RefPtr<TransceiverImpl>& transceiver : transceivers) {
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

        MOZ_MTLOG(ML_DEBUG, mPCHandle << "[" << mLocalMid.Ref()
                                      << "]: " << __FUNCTION__ << " Syncing "
                                      << mConduit.get() << " to "
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

bool TransceiverImpl::ConduitHasPluginID(uint64_t aPluginID) {
  return mConduit && mConduit->HasCodecPluginID(aPluginID);
}

bool TransceiverImpl::HasSendTrack(
    const dom::MediaStreamTrack* aSendTrack) const {
  if (!mSendTrack) {
    return false;
  }

  if (!aSendTrack) {
    return true;
  }

  return mSendTrack.get() == aSendTrack;
}

void TransceiverImpl::SyncWithJS(dom::RTCRtpTransceiver& aJsTransceiver,
                                 ErrorResult& aRv) {
  MOZ_MTLOG(ML_DEBUG, mPCHandle << "[" << mLocalMid.Ref()
                                << "]: " << __FUNCTION__
                                << " Syncing with JS transceiver");

  if (!mTransmitPipeline) {
    // Shutdown_m has already been called, probably due to pc.close(). Just
    // nod and smile.
    return;
  }

  // Update stopped, both ways, since either JSEP or JS can stop these
  if (mJsepTransceiver->IsStopped()) {
    // We don't call RTCRtpTransceiver::Stop(), because that causes another sync
    aJsTransceiver.SetStopped(aRv);
    Stop();
  } else if (aJsTransceiver.GetStopped(aRv)) {
    mJsepTransceiver->Stop();
    Stop();
  }

  // Lots of this in here for simple getters that should never fail. Lame.
  // Just propagate the exception and let JS log it.
  if (aRv.Failed()) {
    return;
  }

  // Update direction from JS only
  dom::RTCRtpTransceiverDirection direction = aJsTransceiver.GetDirection(aRv);

  if (aRv.Failed()) {
    return;
  }

  switch (direction) {
    case dom::RTCRtpTransceiverDirection::Sendrecv:
      mJsepTransceiver->mJsDirection =
          SdpDirectionAttribute::Direction::kSendrecv;
      break;
    case dom::RTCRtpTransceiverDirection::Sendonly:
      mJsepTransceiver->mJsDirection =
          SdpDirectionAttribute::Direction::kSendonly;
      break;
    case dom::RTCRtpTransceiverDirection::Recvonly:
      mJsepTransceiver->mJsDirection =
          SdpDirectionAttribute::Direction::kRecvonly;
      break;
    case dom::RTCRtpTransceiverDirection::Inactive:
      mJsepTransceiver->mJsDirection =
          SdpDirectionAttribute::Direction::kInactive;
      break;
    default:
      MOZ_ASSERT(false);
      aRv = NS_ERROR_INVALID_ARG;
      return;
  }

  // Update send track ids in JSEP
  RefPtr<dom::RTCRtpSender> sender = aJsTransceiver.GetSender(aRv);
  if (aRv.Failed()) {
    return;
  }

  nsTArray<RefPtr<DOMMediaStream>> streams;
  sender->GetStreams(streams, aRv);
  if (aRv.Failed()) {
    return;
  }

  std::vector<std::string> streamIds;
  for (const auto& stream : streams) {
    nsString wideStreamId;
    stream->GetId(wideStreamId);
    std::string streamId = NS_ConvertUTF16toUTF8(wideStreamId).get();
    MOZ_ASSERT(!streamId.empty());
    streamIds.push_back(streamId);
  }

  mJsepTransceiver->mSendTrack.UpdateStreamIds(streamIds);

  // Update RTCRtpParameters
  // TODO: Both ways for things like ssrc, codecs, header extensions, etc

  dom::RTCRtpParameters parameters;
  sender->GetParameters(parameters, aRv);

  if (aRv.Failed()) {
    return;
  }

  std::vector<JsepTrack::JsConstraints> constraints;

  if (parameters.mEncodings.WasPassed()) {
    for (auto& encoding : parameters.mEncodings.Value()) {
      JsepTrack::JsConstraints constraint;
      if (encoding.mRid.WasPassed()) {
        // TODO: Either turn on the RID RTP header extension in JsepSession, or
        // just leave that extension on all the time?
        constraint.rid = NS_ConvertUTF16toUTF8(encoding.mRid.Value()).get();
      }
      if (encoding.mMaxBitrate.WasPassed()) {
        constraint.constraints.maxBr = encoding.mMaxBitrate.Value();
      }
      constraint.constraints.scaleDownBy = encoding.mScaleResolutionDownBy;
      constraints.push_back(constraint);
    }
  }

  if (mJsepTransceiver->mSendTrack.SetJsConstraints(constraints)) {
    if (mTransmitPipeline->Transmitting()) {
      DebugOnly<nsresult> rv = UpdateConduit();
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  // If a SRD has unset the receive bit, stop the receive pipeline so incoming
  // RTP does not unmute the receive track.
  if (!mJsepTransceiver->mRecvTrack.GetRemoteSetSendBit() ||
      !mJsepTransceiver->mRecvTrack.GetActive()) {
    mReceiver->Stop();
  }

  // mid from JSEP
  if (mJsepTransceiver->IsAssociated()) {
    aJsTransceiver.SetMid(
        NS_ConvertUTF8toUTF16(mJsepTransceiver->GetMid().c_str()), aRv);
  } else {
    aJsTransceiver.UnsetMid(aRv);
  }

  if (aRv.Failed()) {
    return;
  }

  // currentDirection from JSEP, but not if "this transceiver has never been
  // represented in an offer/answer exchange"
  if (mJsepTransceiver->HasLevel() && mJsepTransceiver->IsNegotiated()) {
    if (IsReceiving()) {
      if (IsSending()) {
        aJsTransceiver.SetCurrentDirection(
            dom::RTCRtpTransceiverDirection::Sendrecv, aRv);
      } else {
        aJsTransceiver.SetCurrentDirection(
            dom::RTCRtpTransceiverDirection::Recvonly, aRv);
      }
    } else {
      if (IsSending()) {
        aJsTransceiver.SetCurrentDirection(
            dom::RTCRtpTransceiverDirection::Sendonly, aRv);
      } else {
        aJsTransceiver.SetCurrentDirection(
            dom::RTCRtpTransceiverDirection::Inactive, aRv);
      }
    }

    if (aRv.Failed()) {
      return;
    }
  }

  // AddTrack magic from JS
  if (aJsTransceiver.GetAddTrackMagic(aRv)) {
    mJsepTransceiver->SetAddTrackMagic();
  }

  if (aRv.Failed()) {
    return;
  }

  if (mJsepTransceiver->IsRemoved()) {
    aJsTransceiver.SetShouldRemove(true, aRv);
  }
}

bool TransceiverImpl::CanSendDTMF() const {
  // Spec says: "If connection's RTCPeerConnectionState is not "connected"
  // return false." We don't support that right now. This is supposed to be
  // true once ICE is complete, and _all_ DTLS handshakes are also complete. We
  // don't really have access to the state of _all_ of our DTLS states either.
  // Our pipeline _does_ know whether SRTP/SRTCP is ready, which happens
  // immediately after our transport finishes DTLS (unless there was an error),
  // so this is pretty close.
  // TODO (bug 1265827): Base this on RTCPeerConnectionState instead.
  // TODO (bug 1623193): Tighten this up
  if (!IsSending() || !mSendTrack) {
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

JSObject* TransceiverImpl::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return dom::TransceiverImpl_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner* TransceiverImpl::GetParentObject() const { return mWindow; }

RefPtr<MediaPipelineTransmit> TransceiverImpl::GetSendPipeline() {
  return mTransmitPipeline;
}

static nsresult JsepCodecDescToAudioCodecConfig(
    const JsepCodecDescription& aCodec, Maybe<AudioCodecConfig>* aConfig) {
  MOZ_ASSERT(aCodec.mType == SdpMediaSection::kAudio);
  if (aCodec.mType != SdpMediaSection::kAudio) return NS_ERROR_INVALID_ARG;

  const JsepAudioCodecDescription& desc =
      static_cast<const JsepAudioCodecDescription&>(aCodec);

  uint16_t pt;

  if (!desc.GetPtAsInt(&pt)) {
    MOZ_MTLOG(ML_ERROR, "Invalid payload type: " << desc.mDefaultPt);
    return NS_ERROR_INVALID_ARG;
  }

  *aConfig = Some(AudioCodecConfig(pt, desc.mName, desc.mClock,
                                   desc.mForceMono ? 1 : desc.mChannels,
                                   desc.mFECEnabled));
  (*aConfig)->mMaxPlaybackRate = desc.mMaxPlaybackRate;
  (*aConfig)->mDtmfEnabled = desc.mDtmfEnabled;
  (*aConfig)->mDTXEnabled = desc.mDTXEnabled;
  (*aConfig)->mMaxAverageBitrate = desc.mMaxAverageBitrate;
  (*aConfig)->mFrameSizeMs = desc.mFrameSizeMs;
  (*aConfig)->mMinFrameSizeMs = desc.mMinFrameSizeMs;
  (*aConfig)->mMaxFrameSizeMs = desc.mMaxFrameSizeMs;
  (*aConfig)->mCbrEnabled = desc.mCbrEnabled;

  return NS_OK;
}

// TODO: Maybe move this someplace else?
/*static*/
nsresult TransceiverImpl::NegotiatedDetailsToAudioCodecConfigs(
    const JsepTrackNegotiatedDetails& aDetails,
    std::vector<AudioCodecConfig>* aConfigs) {
  Maybe<AudioCodecConfig> telephoneEvent;

  if (aDetails.GetEncodingCount()) {
    for (const auto& codec : aDetails.GetEncoding(0).GetCodecs()) {
      Maybe<AudioCodecConfig> config;
      if (NS_FAILED(JsepCodecDescToAudioCodecConfig(*codec, &config))) {
        return NS_ERROR_INVALID_ARG;
      }
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

  if (aConfigs->empty()) {
    MOZ_MTLOG(ML_ERROR, "Can't set up a conduit with 0 codecs");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult TransceiverImpl::UpdateAudioConduit() {
  MOZ_ASSERT(IsValid());

  if (mJsepTransceiver->mSendTrack.GetNegotiatedDetails() &&
      mJsepTransceiver->mSendTrack.GetActive()) {
    const auto& details(*mJsepTransceiver->mSendTrack.GetNegotiatedDetails());
    std::vector<AudioCodecConfig> configs;
    nsresult rv = TransceiverImpl::NegotiatedDetailsToAudioCodecConfigs(
        details, &configs);

    if (NS_FAILED(rv)) {
      MOZ_MTLOG(ML_ERROR, mPCHandle
                              << "[" << mLocalMid.Ref() << "]: " << __FUNCTION__
                              << " Failed to convert JsepCodecDescriptions to "
                                 "AudioCodecConfigs (send).");
      return rv;
    }

    std::vector<AudioCodecConfig> dtmfConfigs;
    std::copy_if(
        configs.begin(), configs.end(), std::back_inserter(dtmfConfigs),
        [](const auto& value) { return value.mName == "telephone-event"; });

    const AudioCodecConfig& sendCodec = configs[0];

    if (!dtmfConfigs.empty()) {
      // There is at least one telephone-event codec.
      // We primarily choose the codec whose frequency matches the send codec.
      // Secondarily we choose the one with the lowest frequency.
      auto dtmfIterator =
          std::find_if(dtmfConfigs.begin(), dtmfConfigs.end(),
                       [&sendCodec](const auto& dtmfCodec) {
                         return dtmfCodec.mFreq == sendCodec.mFreq;
                       });
      if (dtmfIterator == dtmfConfigs.end()) {
        dtmfIterator = std::min_element(
            dtmfConfigs.begin(), dtmfConfigs.end(),
            [](const auto& a, const auto& b) { return a.mFreq < b.mFreq; });
      }
      MOZ_ASSERT(dtmfIterator != dtmfConfigs.end());
      mDtmf->SetPayloadType(dtmfIterator->mType, dtmfIterator->mFreq);
    }

    mAudioSendCodec = Some(sendCodec);
    {
      std::vector<webrtc::RtpExtension> extmaps;
      // @@NG read extmap from track
      details.ForEachRTPHeaderExtension(
          [&extmaps](const SdpExtmapAttributeList::Extmap& extmap) {
            extmaps.emplace_back(extmap.extensionname, extmap.entry);
          });
      mLocalSendRtpExtensions = extmaps;
    }
  }

  return NS_OK;
}

static nsresult JsepCodecDescToVideoCodecConfig(
    const JsepCodecDescription& aCodec, Maybe<VideoCodecConfig>* aConfig) {
  MOZ_ASSERT(aCodec.mType == SdpMediaSection::kVideo);
  if (aCodec.mType != SdpMediaSection::kVideo) {
    MOZ_ASSERT(false, "JsepCodecDescription has wrong type");
    return NS_ERROR_INVALID_ARG;
  }

  const JsepVideoCodecDescription& desc =
      static_cast<const JsepVideoCodecDescription&>(aCodec);

  uint16_t pt;

  if (!desc.GetPtAsInt(&pt)) {
    MOZ_MTLOG(ML_ERROR, "Invalid payload type: " << desc.mDefaultPt);
    return NS_ERROR_INVALID_ARG;
  }

  UniquePtr<VideoCodecConfigH264> h264Config;

  if (desc.mName == "H264") {
    h264Config = MakeUnique<VideoCodecConfigH264>();
    size_t spropSize = sizeof(h264Config->sprop_parameter_sets);
    strncpy(h264Config->sprop_parameter_sets, desc.mSpropParameterSets.c_str(),
            spropSize);
    h264Config->sprop_parameter_sets[spropSize - 1] = '\0';
    h264Config->packetization_mode = desc.mPacketizationMode;
    h264Config->profile_level_id = desc.mProfileLevelId;
    h264Config->tias_bw = 0;  // TODO(bug 1403206)
  }

  *aConfig = Some(
      VideoCodecConfig(pt, desc.mName, desc.mConstraints, h264Config.get()));

  (*aConfig)->mAckFbTypes = desc.mAckFbTypes;
  (*aConfig)->mNackFbTypes = desc.mNackFbTypes;
  (*aConfig)->mCcmFbTypes = desc.mCcmFbTypes;
  (*aConfig)->mRembFbSet = desc.RtcpFbRembIsSet();
  (*aConfig)->mFECFbSet = desc.mFECEnabled;
  (*aConfig)->mTransportCCFbSet = desc.RtcpFbTransportCCIsSet();
  if (desc.mFECEnabled) {
    uint16_t pt;
    if (SdpHelper::GetPtAsInt(desc.mREDPayloadType, &pt)) {
      (*aConfig)->mREDPayloadType = pt;
    }
    if (SdpHelper::GetPtAsInt(desc.mULPFECPayloadType, &pt)) {
      (*aConfig)->mULPFECPayloadType = pt;
    }
  }
  if (desc.mRtxEnabled) {
    uint16_t pt;
    if (SdpHelper::GetPtAsInt(desc.mRtxPayloadType, &pt)) {
      (*aConfig)->mRTXPayloadType = pt;
    }
  }

  return NS_OK;
}

// TODO: Maybe move this someplace else?
/*static*/
nsresult TransceiverImpl::NegotiatedDetailsToVideoCodecConfigs(
    const JsepTrackNegotiatedDetails& aDetails,
    std::vector<VideoCodecConfig>* aConfigs) {
  if (aDetails.GetEncodingCount()) {
    for (const auto& codec : aDetails.GetEncoding(0).GetCodecs()) {
      Maybe<VideoCodecConfig> config;
      if (NS_FAILED(JsepCodecDescToVideoCodecConfig(*codec, &config))) {
        return NS_ERROR_INVALID_ARG;
      }

      config->mTias = aDetails.GetTias();

      for (size_t i = 0; i < aDetails.GetEncodingCount(); ++i) {
        const JsepTrackEncoding& jsepEncoding(aDetails.GetEncoding(i));
        if (jsepEncoding.HasFormat(codec->mDefaultPt)) {
          VideoCodecConfig::Encoding encoding;
          encoding.rid = jsepEncoding.mRid;
          encoding.constraints = jsepEncoding.mConstraints;
          config->mEncodings.push_back(encoding);
        }
      }

      aConfigs->push_back(std::move(*config));
    }
  }

  return NS_OK;
}

nsresult TransceiverImpl::UpdateVideoConduit() {
  MOZ_ASSERT(IsValid());

  // It is possible for SDP to signal that there is a send track, but there not
  // actually be a send track, according to the specification; all that needs to
  // happen is for the transceiver to be configured to send...
  if (mJsepTransceiver->mSendTrack.GetNegotiatedDetails() &&
      mJsepTransceiver->mSendTrack.GetActive()) {
    const auto& details(*mJsepTransceiver->mSendTrack.GetNegotiatedDetails());

    {
      std::vector<webrtc::RtpExtension> extmaps;
      // @@NG read extmap from track
      details.ForEachRTPHeaderExtension(
          [&extmaps](const SdpExtmapAttributeList::Extmap& extmap) {
            extmaps.emplace_back(extmap.extensionname, extmap.entry);
          });
      mLocalSendRtpExtensions = extmaps;
    }

    nsresult rv = ConfigureVideoCodecMode();
    if (NS_FAILED(rv)) {
      return rv;
    }

    std::vector<VideoCodecConfig> configs;
    rv = TransceiverImpl::NegotiatedDetailsToVideoCodecConfigs(details,
                                                               &configs);
    if (NS_FAILED(rv)) {
      MOZ_MTLOG(ML_ERROR, mPCHandle
                              << "[" << mLocalMid.Ref() << "]: " << __FUNCTION__
                              << " Failed to convert JsepCodecDescriptions to "
                                 "VideoCodecConfigs (send).");
      return rv;
    }

    if (configs.empty()) {
      MOZ_MTLOG(ML_INFO, mPCHandle << "[" << mLocalMid.Ref()
                                   << "]: " << __FUNCTION__
                                   << " No codecs were negotiated (send).");
      return NS_OK;
    }

    mVideoSendCodec = Some(configs[0]);
    mVideoSendRtpRtcpConfig = Some(details.GetRtpRtcpConfig());
  }

  return NS_OK;
}

nsresult TransceiverImpl::ConfigureVideoCodecMode() {
  if (!mSendTrack) {
    // Nothing to do
    return NS_OK;
  }

  RefPtr<mozilla::dom::VideoStreamTrack> videotrack =
      mSendTrack->AsVideoStreamTrack();

  if (!videotrack) {
    MOZ_MTLOG(
        ML_ERROR, mPCHandle
                      << "[" << mLocalMid.Ref() << "]: " << __FUNCTION__
                      << " mSendTrack is not video! This should never happen!");
    MOZ_CRASH();
    return NS_ERROR_FAILURE;
  }

  dom::MediaSourceEnum source = videotrack->GetSource().GetMediaSource();
  webrtc::VideoCodecMode mode = webrtc::VideoCodecMode::kRealtimeVideo;
  switch (source) {
    case dom::MediaSourceEnum::Browser:
    case dom::MediaSourceEnum::Screen:
    case dom::MediaSourceEnum::Window:
      mode = webrtc::VideoCodecMode::kScreensharing;
      break;

    case dom::MediaSourceEnum::Camera:
    default:
      mode = webrtc::VideoCodecMode::kRealtimeVideo;
      break;
  }

  mVideoCodecMode = mode;

  return NS_OK;
}

void TransceiverImpl::Stop() {
  mTransmitPipeline->Stop();
  mReceiver->Stop();
  // Make sure that stats queries stop working on this transceiver.
  UpdateSendTrack(nullptr);

  if (mDtmf) {
    mDtmf->StopPlayout();
  }

  if (mCallWrapper) {
    auto conduit = std::move(mConduit);
    (conduit ? conduit->Shutdown()
             : GenericPromise::CreateAndResolve(true, __func__))
        ->Then(mMainThread, __func__,
               [pipeline = mTransmitPipeline, receiver = mReceiver]() mutable {
                 // Shutdown pipelines when conduits are guaranteed shut down,
                 // so that all packets sent from conduits can be delivered.
                 pipeline->Shutdown();
                 receiver->Shutdown();
               });
    mCallWrapper = nullptr;
  }
}

bool TransceiverImpl::IsVideo() const {
  return mJsepTransceiver->GetMediaType() == SdpMediaSection::MediaType::kVideo;
}

}  // namespace mozilla
