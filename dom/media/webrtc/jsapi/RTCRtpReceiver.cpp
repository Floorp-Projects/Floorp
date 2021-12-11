/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RTCRtpReceiver.h"
#include "transport/logging.h"
#include "mozilla/dom/MediaStreamTrack.h"
#include "mozilla/dom/Promise.h"
#include "transportbridge/MediaPipeline.h"
#include "nsPIDOMWindow.h"
#include "PrincipalHandle.h"
#include "nsIPrincipal.h"
#include "mozilla/dom/Document.h"
#include "mozilla/NullPrincipal.h"
#include "MediaTrackGraph.h"
#include "RemoteTrackSource.h"
#include "libwebrtcglue/RtpRtcpConfig.h"
#include "nsString.h"
#include "mozilla/dom/AudioStreamTrack.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "MediaTransportHandler.h"
#include "jsep/JsepTransceiver.h"
#include "mozilla/dom/RTCRtpReceiverBinding.h"
#include "mozilla/dom/RTCRtpSourcesBinding.h"
#include "RTCStatsReport.h"
#include "mozilla/Preferences.h"
#include "TransceiverImpl.h"
#include "libwebrtcglue/AudioConduit.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(RTCRtpReceiver, mWindow, mTrack)
NS_IMPL_CYCLE_COLLECTING_ADDREF(RTCRtpReceiver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(RTCRtpReceiver)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RTCRtpReceiver)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

LazyLogModule gReceiverLog("RTCRtpReceiver");

static PrincipalHandle GetPrincipalHandle(nsPIDOMWindowInner* aWindow,
                                          bool aPrivacyNeeded) {
  // Set the principal used for creating the tracks. This makes the track
  // data (audio/video samples) accessible to the receiving page. We're
  // only certain that privacy hasn't been requested if we're connected.
  nsCOMPtr<nsIScriptObjectPrincipal> winPrincipal = do_QueryInterface(aWindow);
  RefPtr<nsIPrincipal> principal = winPrincipal->GetPrincipal();
  if (NS_WARN_IF(!principal)) {
    principal = NullPrincipal::CreateWithoutOriginAttributes();
  } else if (aPrivacyNeeded) {
    principal = NullPrincipal::CreateWithInheritedAttributes(principal);
  }
  return MakePrincipalHandle(principal);
}

static already_AddRefed<dom::MediaStreamTrack> CreateTrack(
    nsPIDOMWindowInner* aWindow, bool aAudio,
    const nsCOMPtr<nsIPrincipal>& aPrincipal) {
  MediaTrackGraph* graph = MediaTrackGraph::GetInstance(
      aAudio ? MediaTrackGraph::AUDIO_THREAD_DRIVER
             : MediaTrackGraph::SYSTEM_THREAD_DRIVER,
      aWindow, MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      MediaTrackGraph::DEFAULT_OUTPUT_DEVICE);

  RefPtr<MediaStreamTrack> track;
  RefPtr<RemoteTrackSource> trackSource;
  if (aAudio) {
    RefPtr<SourceMediaTrack> source =
        graph->CreateSourceTrack(MediaSegment::AUDIO);
    trackSource = new RemoteTrackSource(source, aPrincipal, u"remote audio"_ns);
    track = new AudioStreamTrack(aWindow, source, trackSource);
  } else {
    RefPtr<SourceMediaTrack> source =
        graph->CreateSourceTrack(MediaSegment::VIDEO);
    trackSource = new RemoteTrackSource(source, aPrincipal, u"remote video"_ns);
    track = new VideoStreamTrack(aWindow, source, trackSource);
  }

  // Spec says remote tracks start out muted.
  trackSource->SetMuted(true);

  return track.forget();
}

RTCRtpReceiver::RTCRtpReceiver(nsPIDOMWindowInner* aWindow, bool aPrivacyNeeded,
                               const std::string& aPCHandle,
                               MediaTransportHandler* aTransportHandler,
                               JsepTransceiver* aJsepTransceiver,
                               nsISerialEventTarget* aMainThread,
                               nsISerialEventTarget* aStsThread,
                               MediaSessionConduit* aConduit,
                               TransceiverImpl* aTransceiverImpl)
    : mWindow(aWindow),
      mPCHandle(aPCHandle),
      mJsepTransceiver(aJsepTransceiver),
      mMainThread(aMainThread),
      mStsThread(aStsThread),
      mTransportHandler(aTransportHandler),
      mTransceiverImpl(aTransceiverImpl) {
  PrincipalHandle principalHandle = GetPrincipalHandle(aWindow, aPrivacyNeeded);
  mTrack = CreateTrack(aWindow, aConduit->type() == MediaSessionConduit::AUDIO,
                       principalHandle.get());
  // Until Bug 1232234 is fixed, we'll get extra RTCP BYES during renegotiation,
  // so we'll disable muting on RTCP BYE and timeout for now.
  if (Preferences::GetBool("media.peerconnection.mute_on_bye_or_timeout",
                           false)) {
    aConduit->SetRtcpEventObserver(this);
  }
  if (aConduit->type() == MediaSessionConduit::AUDIO) {
    mPipeline = new MediaPipelineReceiveAudio(
        mPCHandle, aTransportHandler, mMainThread.get(), mStsThread.get(),
        static_cast<AudioSessionConduit*>(aConduit), mTrack, principalHandle);
  } else {
    mPipeline = new MediaPipelineReceiveVideo(
        mPCHandle, aTransportHandler, mMainThread.get(), mStsThread.get(),
        static_cast<VideoSessionConduit*>(aConduit), mTrack, principalHandle);
  }
}

RTCRtpReceiver::~RTCRtpReceiver() { MOZ_ASSERT(!mPipeline); }

JSObject* RTCRtpReceiver::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return RTCRtpReceiver_Binding::Wrap(aCx, this, aGivenProto);
}

RTCDtlsTransport* RTCRtpReceiver::GetTransport() const {
  if (!mTransceiverImpl) {
    return nullptr;
  }
  return mTransceiverImpl->GetDtlsTransport();
}

already_AddRefed<Promise> RTCRtpReceiver::GetStats() {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mWindow);
  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(global, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.StealNSResult();
    return nullptr;
  }

  // Two promises; one for RTP/RTCP stats, another for ICE stats.
  auto promises = GetStatsInternal();
  RTCStatsPromise::All(mMainThread, promises)
      ->Then(
          mMainThread, __func__,
          [promise, window = mWindow](
              const nsTArray<UniquePtr<RTCStatsCollection>>& aStats) {
            RefPtr<RTCStatsReport> report(new RTCStatsReport(window));
            for (const auto& stats : aStats) {
              report->Incorporate(*stats);
            }
            promise->MaybeResolve(std::move(report));
          },
          [promise](nsresult aError) {
            promise->MaybeReject(NS_ERROR_FAILURE);
          });

  return promise.forget();
}

static UniquePtr<dom::RTCStatsCollection> GetReceiverStats_s(
    const RefPtr<MediaPipelineReceive>& aPipeline,
    const nsString& aRecvTrackId) {
  UniquePtr<dom::RTCStatsCollection> report(new dom::RTCStatsCollection);
  auto asVideo = aPipeline->Conduit()->AsVideoSessionConduit();

  nsString kind = asVideo.isNothing() ? u"audio"_ns : u"video"_ns;
  nsString idstr = kind + u"_"_ns;
  idstr.AppendInt(static_cast<uint32_t>(aPipeline->Level()));

  // Add bandwidth estimation stats
  aPipeline->Conduit()->GetBandwidthEstimation().apply([&](auto& bw) {
    bw.mTrackIdentifier = aRecvTrackId;
    if (!report->mBandwidthEstimations.AppendElement(bw, fallible)) {
      mozalloc_handle_oom(0);
    }
  });

  Maybe<uint32_t> ssrc;
  unsigned int ssrcval;
  if (aPipeline->Conduit()->GetRemoteSSRC(&ssrcval)) {
    ssrc = Some(ssrcval);
  }

  // Add frame history
  asVideo.apply([&](const auto& conduit) {
    if (conduit->AddFrameHistory(&report->mVideoFrameHistories)) {
      auto& history = report->mVideoFrameHistories.LastElement();
      history.mTrackIdentifier = aRecvTrackId;
    }
  });

  // TODO(@@NG):ssrcs handle Conduits having multiple stats at the same level
  // This is pending spec work
  // Gather pipeline stats.
  nsString localId = u"inbound_rtp_"_ns + idstr;
  nsString remoteId;

  // First, fill in remote stat with rtcp sender data, if present.
  uint32_t packetsSent;
  uint64_t bytesSent;
  DOMHighResTimeStamp remoteTimestamp;
  Maybe<DOMHighResTimeStamp> timestamp =
      aPipeline->Conduit()->LastRtcpReceived();
  if (timestamp.isSome() && aPipeline->Conduit()->GetRTCPSenderReport(
                                &packetsSent, &bytesSent, &remoteTimestamp)) {
    RTCRemoteOutboundRtpStreamStats s;
    remoteId = u"inbound_rtcp_"_ns + idstr;
    s.mTimestamp.Construct(*timestamp);
    s.mId.Construct(remoteId);
    s.mType.Construct(RTCStatsType::Remote_outbound_rtp);
    ssrc.apply([&s](uint32_t aSsrc) { s.mSsrc.Construct(aSsrc); });
    s.mMediaType.Construct(kind);  // mediaType is the old name for kind.
    s.mKind.Construct(kind);
    s.mLocalId.Construct(localId);
    s.mPacketsSent.Construct(packetsSent);
    s.mBytesSent.Construct(bytesSent);
    s.mRemoteTimestamp.Construct(remoteTimestamp);
    if (!report->mRemoteOutboundRtpStreamStats.AppendElement(s, fallible)) {
      mozalloc_handle_oom(0);
    }
  }

  // Then, fill in local side (with cross-link to remote only if present)
  RTCInboundRtpStreamStats s;
  // TODO(bug 1496533): Should we use the time of the most-recently received
  // RTP packet? If so, what do we use if we haven't received any RTP? Now?
  s.mTimestamp.Construct(aPipeline->GetNow());
  s.mId.Construct(localId);
  s.mType.Construct(RTCStatsType::Inbound_rtp);
  ssrc.apply([&s](uint32_t aSsrc) { s.mSsrc.Construct(aSsrc); });
  s.mMediaType.Construct(kind);  // mediaType is the old name for kind.
  s.mKind.Construct(kind);
  unsigned int jitterMs, packetsLost;
  if (aPipeline->Conduit()->GetRTPReceiverStats(&jitterMs, &packetsLost)) {
    s.mJitter.Construct(double(jitterMs) / 1000);
    s.mPacketsLost.Construct(packetsLost);
  }
  if (remoteId.Length()) {
    s.mRemoteId.Construct(remoteId);
  }
  s.mPacketsReceived.Construct(aPipeline->RtpPacketsReceived());
  s.mBytesReceived.Construct(aPipeline->RtpBytesReceived());

  // Fill in packet type statistics
  webrtc::RtcpPacketTypeCounter counters;
  if (aPipeline->Conduit()->GetRecvPacketTypeStats(&counters)) {
    s.mNackCount.Construct(counters.nack_packets);
    // Fill in video only packet type stats
    if (asVideo) {
      s.mFirCount.Construct(counters.fir_packets);
      s.mPliCount.Construct(counters.pli_packets);
    }
  }
  // Lastly, fill in video decoder stats if this is video
  asVideo.apply([&s](auto conduit) {
    double framerateMean;
    double framerateStdDev;
    double bitrateMean;
    double bitrateStdDev;
    uint32_t discardedPackets;
    uint32_t framesDecoded;
    if (conduit->GetVideoDecoderStats(&framerateMean, &framerateStdDev,
                                      &bitrateMean, &bitrateStdDev,
                                      &discardedPackets, &framesDecoded)) {
      s.mFramerateMean.Construct(framerateMean);
      s.mFramerateStdDev.Construct(framerateStdDev);
      s.mBitrateMean.Construct(bitrateMean);
      s.mBitrateStdDev.Construct(bitrateStdDev);
      s.mDiscardedPackets.Construct(discardedPackets);
      s.mFramesDecoded.Construct(framesDecoded);
    }
  });
  if (!report->mInboundRtpStreamStats.AppendElement(s, fallible)) {
    mozalloc_handle_oom(0);
  }

  // Fill in Contributing Source statistics
  aPipeline->GetContributingSourceStats(localId,
                                        report->mRtpContributingSourceStats);

  return report;
}

nsTArray<RefPtr<RTCStatsPromise>> RTCRtpReceiver::GetStatsInternal() {
  nsTArray<RefPtr<RTCStatsPromise>> promises;
  if (mPipeline && mHaveStartedReceiving) {
    nsString recvTrackId;
    MOZ_ASSERT(mTrack);
    if (mTrack) {
      mTrack->GetId(recvTrackId);
    }
    promises.AppendElement(InvokeAsync(
        mStsThread, __func__, [pipeline = mPipeline, recvTrackId]() {
          return RTCStatsPromise::CreateAndResolve(
              GetReceiverStats_s(pipeline, recvTrackId), __func__);
        }));

    if (mJsepTransceiver->mTransport.mComponents) {
      promises.AppendElement(mTransportHandler->GetIceStats(
          mJsepTransceiver->mTransport.mTransportId, mPipeline->GetNow()));
    }
  }
  return promises;
}

void RTCRtpReceiver::GetContributingSources(
    nsTArray<RTCRtpContributingSource>& aSources) {
  // Duplicate code...
  if (mPipeline && mPipeline->Conduit()) {
    RefPtr<AudioSessionConduit> conduit(
        static_cast<AudioSessionConduit*>(mPipeline->Conduit()));
    nsTArray<dom::RTCRtpSourceEntry> sources;
    conduit->GetRtpSources(sources);
    sources.RemoveElementsBy([](const dom::RTCRtpSourceEntry& aEntry) {
      return aEntry.mSourceType != dom::RTCRtpSourceEntryType::Contributing;
    });
    aSources.ReplaceElementsAt(0, aSources.Length(), sources.Elements(),
                               sources.Length());
  }
}

void RTCRtpReceiver::GetSynchronizationSources(
    nsTArray<dom::RTCRtpSynchronizationSource>& aSources) {
  // Duplicate code...
  if (mPipeline && mPipeline->Conduit()) {
    RefPtr<AudioSessionConduit> conduit(
        static_cast<AudioSessionConduit*>(mPipeline->Conduit()));
    nsTArray<dom::RTCRtpSourceEntry> sources;
    conduit->GetRtpSources(sources);
    sources.RemoveElementsBy([](const dom::RTCRtpSourceEntry& aEntry) {
      return aEntry.mSourceType != dom::RTCRtpSourceEntryType::Synchronization;
    });
    aSources.ReplaceElementsAt(0, aSources.Length(), sources.Elements(),
                               sources.Length());
  }
}

nsPIDOMWindowInner* RTCRtpReceiver::GetParentObject() const { return mWindow; }

void RTCRtpReceiver::Shutdown() {
  ASSERT_ON_THREAD(mMainThread);
  if (mPipeline) {
    mPipeline->Shutdown_m();
    mPipeline->Conduit()->SetRtcpEventObserver(nullptr);
    mPipeline = nullptr;
  }
  mTransceiverImpl = nullptr;
}

void RTCRtpReceiver::UpdateTransport() {
  ASSERT_ON_THREAD(mMainThread);
  if (!mHaveSetupTransport) {
    mPipeline->SetLevel(mJsepTransceiver->GetLevel());
    mHaveSetupTransport = true;
  }

  UniquePtr<MediaPipelineFilter> filter;

  auto const& details = mJsepTransceiver->mRecvTrack.GetNegotiatedDetails();
  if (mJsepTransceiver->HasBundleLevel() && details) {
    std::vector<webrtc::RtpExtension> extmaps;
    details->ForEachRTPHeaderExtension(
        [&extmaps](const SdpExtmapAttributeList::Extmap& extmap) {
          extmaps.emplace_back(extmap.extensionname, extmap.entry);
        });
    filter = MakeUnique<MediaPipelineFilter>(extmaps);

    // Add remote SSRCs so we can distinguish which RTP packets actually
    // belong to this pipeline (also RTCP sender reports).
    for (uint32_t ssrc : mJsepTransceiver->mRecvTrack.GetSsrcs()) {
      filter->AddRemoteSSRC(ssrc);
    }
    for (uint32_t ssrc : mJsepTransceiver->mRecvTrack.GetRtxSsrcs()) {
      filter->AddRemoteSSRC(ssrc);
    }
    auto mid = Maybe<std::string>();
    if (GetMid() != "") {
      mid = Some(GetMid());
    }
    filter->SetRemoteMediaStreamId(mid);

    // Add unique payload types as a last-ditch fallback
    auto uniquePts = mJsepTransceiver->mRecvTrack.GetNegotiatedDetails()
                         ->GetUniquePayloadTypes();
    for (unsigned char& uniquePt : uniquePts) {
      filter->AddUniquePT(uniquePt);
    }
  }

  mPipeline->UpdateTransport_m(mJsepTransceiver->mTransport.mTransportId,
                               std::move(filter));
}

nsresult RTCRtpReceiver::UpdateConduit() {
  if (mPipeline->Conduit()->type() == MediaSessionConduit::VIDEO) {
    return UpdateVideoConduit();
  }
  return UpdateAudioConduit();
}

nsresult RTCRtpReceiver::UpdateVideoConduit() {
  RefPtr<VideoSessionConduit> conduit =
      static_cast<VideoSessionConduit*>(mPipeline->Conduit());

  // NOTE(pkerr) - this is new behavior. Needed because the
  // CreateVideoReceiveStream method of the Call API will assert (in debug)
  // and fail if a value is not provided for the remote_ssrc that will be used
  // by the far-end sender.
  if (!mJsepTransceiver->mRecvTrack.GetSsrcs().empty()) {
    MOZ_LOG(gReceiverLog, LogLevel::Debug,
            ("%s[%s]: %s Setting remote SSRC %u", mPCHandle.c_str(),
             GetMid().c_str(), __FUNCTION__,
             mJsepTransceiver->mRecvTrack.GetSsrcs().front()));
    uint32_t rtxSsrc = mJsepTransceiver->mRecvTrack.GetRtxSsrcs().empty()
                           ? 0
                           : mJsepTransceiver->mRecvTrack.GetRtxSsrcs().front();
    conduit->SetRemoteSSRC(mJsepTransceiver->mRecvTrack.GetSsrcs().front(),
                           rtxSsrc);
  }

  // TODO (bug 1423041) once we pay attention to receiving MID's in RTP
  // packets (see bug 1405495) we could make this depending on the presence of
  // MID in the RTP packets instead of relying on the signaling.
  if (mJsepTransceiver->HasBundleLevel() &&
      (!mJsepTransceiver->mRecvTrack.GetNegotiatedDetails() ||
       !mJsepTransceiver->mRecvTrack.GetNegotiatedDetails()->GetExt(
           webrtc::RtpExtension::kMIdUri))) {
    mStsThread->Dispatch(
        NewRunnableMethod("VideoSessionConduit::DisableSsrcChanges", conduit,
                          &VideoSessionConduit::DisableSsrcChanges));
  }

  if (mJsepTransceiver->mRecvTrack.GetNegotiatedDetails() &&
      mJsepTransceiver->mRecvTrack.GetActive()) {
    const auto& details(*mJsepTransceiver->mRecvTrack.GetNegotiatedDetails());

    TransceiverImpl::UpdateConduitRtpExtmap(
        *conduit, details, MediaSessionConduitLocalDirection::kRecv);

    std::vector<UniquePtr<VideoCodecConfig>> configs;
    nsresult rv = TransceiverImpl::NegotiatedDetailsToVideoCodecConfigs(
        details, &configs);

    if (NS_FAILED(rv)) {
      MOZ_LOG(gReceiverLog, LogLevel::Error,
              ("%s[%s]: %s Failed to convert "
               "JsepCodecDescriptions to VideoCodecConfigs (recv).",
               mPCHandle.c_str(), GetMid().c_str(), __FUNCTION__));
      return rv;
    }

    auto error =
        conduit->ConfigureRecvMediaCodecs(configs, details.GetRtpRtcpConfig());

    if (error) {
      MOZ_LOG(gReceiverLog, LogLevel::Error,
              ("%s[%s]: %s "
               "ConfigureRecvMediaCodecs failed: %u",
               mPCHandle.c_str(), GetMid().c_str(), __FUNCTION__, error));
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

nsresult RTCRtpReceiver::UpdateAudioConduit() {
  RefPtr<AudioSessionConduit> conduit =
      static_cast<AudioSessionConduit*>(mPipeline->Conduit());

  if (!mJsepTransceiver->mRecvTrack.GetSsrcs().empty()) {
    MOZ_LOG(gReceiverLog, LogLevel::Debug,
            ("%s[%s]: %s Setting remote SSRC %u", mPCHandle.c_str(),
             GetMid().c_str(), __FUNCTION__,
             mJsepTransceiver->mRecvTrack.GetSsrcs().front()));
    uint32_t rtxSsrc = mJsepTransceiver->mRecvTrack.GetRtxSsrcs().empty()
                           ? 0
                           : mJsepTransceiver->mRecvTrack.GetRtxSsrcs().front();
    conduit->SetRemoteSSRC(mJsepTransceiver->mRecvTrack.GetSsrcs().front(),
                           rtxSsrc);
  }

  if (mJsepTransceiver->mRecvTrack.GetNegotiatedDetails() &&
      mJsepTransceiver->mRecvTrack.GetActive()) {
    const auto& details(*mJsepTransceiver->mRecvTrack.GetNegotiatedDetails());
    std::vector<UniquePtr<AudioCodecConfig>> configs;
    nsresult rv = TransceiverImpl::NegotiatedDetailsToAudioCodecConfigs(
        details, &configs);

    if (NS_FAILED(rv)) {
      MOZ_LOG(gReceiverLog, LogLevel::Error,
              ("%s[%s]: %s Failed to convert "
               "JsepCodecDescriptions to AudioCodecConfigs (recv).",
               mPCHandle.c_str(), GetMid().c_str(), __FUNCTION__));
      return rv;
    }

    // Ensure conduit knows about extensions prior to creating streams
    TransceiverImpl::UpdateConduitRtpExtmap(
        *conduit, details, MediaSessionConduitLocalDirection::kRecv);

    auto error = conduit->ConfigureRecvMediaCodecs(configs);

    if (error) {
      MOZ_LOG(gReceiverLog, LogLevel::Error,
              ("%s[%s]: %s "
               "ConfigureRecvMediaCodecs failed: %u",
               mPCHandle.c_str(), GetMid().c_str(), __FUNCTION__, error));
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

void RTCRtpReceiver::Stop() {
  if (mPipeline) {
    mPipeline->Stop();
  }
}

void RTCRtpReceiver::Start() {
  mPipeline->Start();
  mHaveStartedReceiving = true;
}

bool RTCRtpReceiver::HasTrack(const dom::MediaStreamTrack* aTrack) const {
  return !aTrack || (mTrack == aTrack);
}

void RTCRtpReceiver::UpdateStreams(StreamAssociationChanges* aChanges) {
  // We don't sort and use set_difference, because we need to report the
  // added/removed streams in the order that they appear in the SDP.
  std::set<std::string> newIds(
      mJsepTransceiver->mRecvTrack.GetStreamIds().begin(),
      mJsepTransceiver->mRecvTrack.GetStreamIds().end());
  MOZ_ASSERT(mJsepTransceiver->mRecvTrack.GetRemoteSetSendBit() ||
             newIds.empty());
  bool needsTrackEvent = false;
  for (const auto& id : mStreamIds) {
    if (!newIds.count(id)) {
      aChanges->mStreamAssociationsRemoved.push_back({mTrack, id});
    }
  }

  std::set<std::string> oldIds(mStreamIds.begin(), mStreamIds.end());
  for (const auto& id : mJsepTransceiver->mRecvTrack.GetStreamIds()) {
    if (!oldIds.count(id)) {
      needsTrackEvent = true;
      aChanges->mStreamAssociationsAdded.push_back({mTrack, id});
    }
  }

  mStreamIds = mJsepTransceiver->mRecvTrack.GetStreamIds();

  if (mRemoteSetSendBit != mJsepTransceiver->mRecvTrack.GetRemoteSetSendBit()) {
    mRemoteSetSendBit = mJsepTransceiver->mRecvTrack.GetRemoteSetSendBit();
    if (mRemoteSetSendBit) {
      needsTrackEvent = true;
    } else {
      aChanges->mTracksToMute.push_back(mTrack);
    }
  }

  if (needsTrackEvent) {
    aChanges->mTrackEvents.push_back({this, mStreamIds});
  }
}

// test-only: adds fake CSRCs and audio data
void RTCRtpReceiver::MozInsertAudioLevelForContributingSource(
    const uint32_t aSource, const DOMHighResTimeStamp aTimestamp,
    const uint32_t aRtpTimestamp, const bool aHasLevel, const uint8_t aLevel) {
  if (!mPipeline || mPipeline->IsVideo() || !mPipeline->Conduit()) {
    return;
  }
  WebrtcAudioConduit* audio_conduit =
      static_cast<WebrtcAudioConduit*>(mPipeline->Conduit());
  audio_conduit->InsertAudioLevelForContributingSource(
      aSource, aTimestamp, aRtpTimestamp, aHasLevel, aLevel);
}

void RTCRtpReceiver::OnRtcpBye() { SetReceiveTrackMuted(true); }

void RTCRtpReceiver::OnRtcpTimeout() { SetReceiveTrackMuted(true); }

void RTCRtpReceiver::SetReceiveTrackMuted(bool aMuted) {
  if (mTrack) {
    // This sets the muted state for mTrack and all its clones.
    static_cast<RemoteTrackSource&>(mTrack->GetSource()).SetMuted(aMuted);
  }
}

std::string RTCRtpReceiver::GetMid() const {
  if (mJsepTransceiver->IsAssociated()) {
    return mJsepTransceiver->GetMid();
  }
  return std::string();
}

}  // namespace mozilla::dom

#undef LOGTAG
