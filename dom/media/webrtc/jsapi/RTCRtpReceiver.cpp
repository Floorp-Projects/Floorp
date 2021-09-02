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

#define INIT_CANONICAL(name, val)         \
  name(AbstractThread::MainThread(), val, \
       "RTCRtpReceiver::" #name " (Canonical)")

RTCRtpReceiver::RTCRtpReceiver(
    nsPIDOMWindowInner* aWindow, bool aPrivacyNeeded,
    const std::string& aPCHandle, MediaTransportHandler* aTransportHandler,
    JsepTransceiver* aJsepTransceiver, nsISerialEventTarget* aMainThread,
    AbstractThread* aCallThread, nsISerialEventTarget* aStsThread,
    MediaSessionConduit* aConduit, TransceiverImpl* aTransceiverImpl)
    : mWindow(aWindow),
      mPCHandle(aPCHandle),
      mJsepTransceiver(aJsepTransceiver),
      mMainThread(aMainThread),
      mCallThread(aCallThread),
      mStsThread(aStsThread),
      mTransportHandler(aTransportHandler),
      mTransceiverImpl(aTransceiverImpl),
      INIT_CANONICAL(mSsrc, 0),
      INIT_CANONICAL(mVideoRtxSsrc, 0),
      INIT_CANONICAL(mLocalRtpExtensions, RtpExtList()),
      INIT_CANONICAL(mAudioCodecs, std::vector<AudioCodecConfig>()),
      INIT_CANONICAL(mVideoCodecs, std::vector<VideoCodecConfig>()),
      INIT_CANONICAL(mVideoRtpRtcpConfig, Nothing()) {
  PrincipalHandle principalHandle = GetPrincipalHandle(aWindow, aPrivacyNeeded);
  mTrack = CreateTrack(aWindow, aConduit->type() == MediaSessionConduit::AUDIO,
                       principalHandle.get());
  // Until Bug 1232234 is fixed, we'll get extra RTCP BYES during renegotiation,
  // so we'll disable muting on RTCP BYE and timeout for now.
  if (Preferences::GetBool("media.peerconnection.mute_on_bye_or_timeout",
                           false)) {
    mRtcpByeListener = aConduit->RtcpByeEvent().Connect(
        mMainThread, this, &RTCRtpReceiver::OnRtcpBye);
    mRtcpTimeoutListener = aConduit->RtcpTimeoutEvent().Connect(
        mMainThread, this, &RTCRtpReceiver::OnRtcpTimeout);
  }
  if (aConduit->type() == MediaSessionConduit::AUDIO) {
    mPipeline = new MediaPipelineReceiveAudio(
        mPCHandle, aTransportHandler, mMainThread.get(), aCallThread,
        mStsThread.get(), *aConduit->AsAudioSessionConduit(), mTrack,
        principalHandle);
  } else {
    mPipeline = new MediaPipelineReceiveVideo(
        mPCHandle, aTransportHandler, mMainThread.get(), aCallThread,
        mStsThread.get(), *aConduit->AsVideoSessionConduit(), mTrack,
        principalHandle);
  }
}

#undef INIT_CANONICAL

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

nsTArray<RefPtr<RTCStatsPromise>> RTCRtpReceiver::GetStatsInternal() {
  MOZ_ASSERT(NS_IsMainThread());
  nsTArray<RefPtr<RTCStatsPromise>> promises(3);

  if (!mPipeline) {
    return promises;
  }

  if (!mHaveStartedReceiving) {
    return promises;
  }

  nsString recvTrackId;
  MOZ_ASSERT(mTrack);
  if (mTrack) {
    mTrack->GetId(recvTrackId);
  }

  {
    // Add bandwidth estimation stats
    promises.AppendElement(InvokeAsync(
        mCallThread, __func__,
        [conduit = mPipeline->mConduit, recvTrackId]() mutable {
          auto report = MakeUnique<dom::RTCStatsCollection>();
          const Maybe<webrtc::Call::Stats> stats = conduit->GetCallStats();
          stats.apply([&](const auto& aStats) {
            dom::RTCBandwidthEstimationInternal bw;
            bw.mTrackIdentifier = recvTrackId;
            bw.mSendBandwidthBps.Construct(aStats.send_bandwidth_bps / 8);
            bw.mMaxPaddingBps.Construct(aStats.max_padding_bitrate_bps / 8);
            bw.mReceiveBandwidthBps.Construct(aStats.recv_bandwidth_bps / 8);
            bw.mPacerDelayMs.Construct(aStats.pacer_delay_ms);
            if (aStats.rtt_ms >= 0) {
              bw.mRttMs.Construct(aStats.rtt_ms);
            }
            if (!report->mBandwidthEstimations.AppendElement(std::move(bw),
                                                             fallible)) {
              mozalloc_handle_oom(0);
            }
          });
          return RTCStatsPromise::CreateAndResolve(std::move(report), __func__);
        }));
  }

  promises.AppendElement(
      InvokeAsync(
          mCallThread, __func__,
          [pipeline = mPipeline, recvTrackId] {
            auto report = MakeUnique<dom::RTCStatsCollection>();
            auto asAudio = pipeline->mConduit->AsAudioSessionConduit();
            auto asVideo = pipeline->mConduit->AsVideoSessionConduit();

            nsString kind = asVideo.isNothing() ? u"audio"_ns : u"video"_ns;
            nsString idstr = kind + u"_"_ns;
            idstr.AppendInt(static_cast<uint32_t>(pipeline->Level()));

            Maybe<uint32_t> ssrc;
            unsigned int ssrcval;
            if (pipeline->mConduit->GetRemoteSSRC(&ssrcval)) {
              ssrc = Some(ssrcval);
            }

            // Add frame history
            asVideo.apply([&](const auto& conduit) {
              if (conduit->AddFrameHistory(&report->mVideoFrameHistories)) {
                auto& history = report->mVideoFrameHistories.LastElement();
                history.mTrackIdentifier = recvTrackId;
              }
            });

            // TODO(@@NG):ssrcs handle Conduits having multiple stats at the
            // same level.
            // This is pending spec work.
            // Gather pipeline stats.
            nsString localId = u"inbound_rtp_"_ns + idstr;
            nsString remoteId;

            auto constructCommonRemoteOutboundRtpStats =
                [&](RTCRemoteOutboundRtpStreamStats& aRemote,
                    const DOMHighResTimeStamp& aTimestamp) {
                  remoteId = u"inbound_rtcp_"_ns + idstr;
                  aRemote.mTimestamp.Construct(aTimestamp);
                  aRemote.mId.Construct(remoteId);
                  aRemote.mType.Construct(RTCStatsType::Remote_outbound_rtp);
                  ssrc.apply(
                      [&](uint32_t aSsrc) { aRemote.mSsrc.Construct(aSsrc); });
                  aRemote.mMediaType.Construct(
                      kind);  // mediaType is the old name for kind.
                  aRemote.mKind.Construct(kind);
                  aRemote.mLocalId.Construct(localId);
                };

            auto constructCommonInboundRtpStats =
                [&](RTCInboundRtpStreamStats& aLocal) {
                  // TODO(bug 1496533): Should we use the time of the
                  // most-recently received RTP packet? If so, what do we use if
                  // we haven't received any RTP? Now?
                  aLocal.mTimestamp.Construct(pipeline->GetNow());
                  aLocal.mId.Construct(localId);
                  aLocal.mType.Construct(RTCStatsType::Inbound_rtp);
                  ssrc.apply(
                      [&](uint32_t aSsrc) { aLocal.mSsrc.Construct(aSsrc); });
                  aLocal.mMediaType.Construct(
                      kind);  // mediaType is the old name for kind.
                  aLocal.mKind.Construct(kind);
                  if (remoteId.Length()) {
                    aLocal.mRemoteId.Construct(remoteId);
                  }
                };

            asAudio.apply([&](auto& aConduit) {
              Maybe<webrtc::AudioReceiveStream::Stats> audioStats =
                  aConduit->GetReceiverStats();
              if (audioStats.isNothing()) {
                return;
              }

              // First, fill in remote stat with rtcp sender data, if present.
              aConduit->LastRtcpReceived().apply([&](auto& aTimestamp) {
                RTCRemoteOutboundRtpStreamStats remote;
                constructCommonRemoteOutboundRtpStats(remote, aTimestamp);
                remote.mPacketsSent.Construct(
                    audioStats->rtcp_sender_packets_sent);
                remote.mBytesSent.Construct(
                    audioStats->rtcp_sender_octets_sent);
                remote.mRemoteTimestamp.Construct(
                    audioStats->rtcp_sender_ntp_timestamp_ms);
                if (!report->mRemoteOutboundRtpStreamStats.AppendElement(
                        std::move(remote), fallible)) {
                  mozalloc_handle_oom(0);
                }
              });

              // Then, fill in local side (with cross-link to remote only if
              // present)
              RTCInboundRtpStreamStats local;
              constructCommonInboundRtpStats(local);
              local.mJitter.Construct(audioStats->jitter_ms / 1000.0);
              local.mPacketsLost.Construct(audioStats->packets_lost);
              local.mPacketsReceived.Construct(audioStats->packets_rcvd);
              local.mBytesReceived.Construct(audioStats->payload_bytes_rcvd);
              /*
               * Potential new stats that are now available upstream.
              if (audioStats->last_packet_received_timestamp_ms) {
                local.mLastPacketReceivedTimestamp.Construct(
                    *audioStats->last_packet_received_timestamp_ms);
              }
              local.mHeaderBytesReceived.Construct(
                  audioStats->header_and_padding_bytes_rcvd);
              local.mFecPacketsReceived.Construct(
                  audioStats->fec_packets_received);
              local.mFecPacketsDiscarded.Construct(
                  audioStats->fec_packets_discarded);
              if (audioStats->estimated_playout_ntp_timestamp_ms) {
                local.mEstimatedPlayoutTimestamp.Construct(
                    *audioStats->estimated_playout_ntp_timestamp_ms);
              }
              local.mJitterBufferDelay.Construct(
                  audioStats->jitter_buffer_delay_seconds);
              local.mJitterBufferEmittedCount.Construct(
                  audioStats->jitter_buffer_emitted_count);
              local.mTotalSamplesReceived.Construct(
                  audioStats->total_samples_received);
              local.mConcealedSamples.Construct(audioStats->concealed_samples);
              local.mSilentConcealedSamples.Construct(
                  audioStats->silent_concealed_samples);
              local.mConcealmentEvents.Construct(
                  audioStats->concealment_events);
              local.mInsertedSamplesForDeceleration.Construct(
                  audioStats->inserted_samples_for_deceleration);
              local.mRemovedSamplesForAcceleration.Construct(
                  audioStats->removed_samples_for_acceleration);
              if (audioStats->audio_level >= 0 &&
                  audioStats->audio_level <= 32767) {
                local.mAudioLevel.Construct(audioStats->audio_level / 32767.0);
              }
              local.mTotalAudioEnergy.Construct(
                  audioStats->total_output_energy);
              local.mTotalSamplesDuration.Construct(
                  audioStats->total_output_duration);
               */
              if (!report->mInboundRtpStreamStats.AppendElement(
                      std::move(local), fallible)) {
                mozalloc_handle_oom(0);
              }
            });

            asVideo.apply([&](auto& aConduit) {
              Maybe<webrtc::VideoReceiveStream::Stats> videoStats =
                  aConduit->GetReceiverStats();
              if (videoStats.isNothing()) {
                return;
              }

              // First, fill in remote stat with rtcp sender data, if present.
              aConduit->LastRtcpReceived().apply([&](auto& aTimestamp) {
                RTCRemoteOutboundRtpStreamStats remote;
                constructCommonRemoteOutboundRtpStats(remote, aTimestamp);
                remote.mPacketsSent.Construct(
                    videoStats->rtcp_sender_packets_sent);
                remote.mBytesSent.Construct(
                    videoStats->rtcp_sender_octets_sent);
                remote.mRemoteTimestamp.Construct(
                    videoStats->rtcp_sender_ntp_timestamp_ms);
                if (!report->mRemoteOutboundRtpStreamStats.AppendElement(
                        std::move(remote), fallible)) {
                  mozalloc_handle_oom(0);
                }
              });

              // Then, fill in local side (with cross-link to remote only if
              // present)
              RTCInboundRtpStreamStats local;
              constructCommonInboundRtpStats(local);
              local.mJitter.Construct(
                  static_cast<double>(videoStats->rtp_stats.jitter) /
                  webrtc::kVideoPayloadTypeFrequency);
              local.mPacketsLost.Construct(videoStats->rtp_stats.packets_lost);
              local.mPacketsReceived.Construct(
                  videoStats->rtp_stats.packet_counter.packets);
              local.mBytesReceived.Construct(
                  videoStats->rtp_stats.packet_counter.payload_bytes);

              // Fill in packet type statistics
              local.mNackCount.Construct(
                  videoStats->rtcp_packet_type_counts.nack_packets);
              local.mFirCount.Construct(
                  videoStats->rtcp_packet_type_counts.fir_packets);
              local.mPliCount.Construct(
                  videoStats->rtcp_packet_type_counts.pli_packets);

              // Lastly, fill in video decoder stats
              local.mFramesDecoded.Construct(videoStats->frames_decoded);

              /*
               * Potential new stats that are now available upstream.
              local.mFrameWidth.Construct(videoStats->width);
              local.mFrameheight.Construct(videoStats->height);
              local.mFramesPerSecond.Construct(videoStats->decode_frame_rate);
              if (videoStats->qp_sum) {
                local.mQpSum.Construct(*videoStats->qp_sum.value);
              }
              local.mTotalDecodeTime.Construct(
                  double(videoStats->total_decode_time_ms) / 1000);
              local.mTotalInterFrameDelay.Construct(
                  videoStats->total_inter_frame_delay);
              local.mTotalSquaredInterFrameDelay.Construct(
                  videoStats->total_squared_inter_frame_delay);
              if (videoStats->rtp_stats.last_packet_received_timestamp_ms) {
                local.mLastPacketReceiveTimestamp.Construct(
                    *videoStats->rtp_stats.last_packet_received_timestamp_ms);
              }
              local.mHeaderBytesReceived.Construct(
                  videoStats->rtp_stats.packet_counter.header_bytes +
                  videoStats->rtp_stats.packet_counter.padding_bytes);
              if (videoStats->estimated_playout_ntp_timestamp_ms) {
                local.mEstimatedPlayoutTimestamp.Construct(
                    *videoStats->estimated_playout_ntp_timestamp_ms);
              }
              local.mJitterBufferDelay.Construct(
                  videoStats->jitter_buffer_delay_seconds);
              local.mJitterBufferEmittedCount.Construct(
                  videoStats->jitter_buffer_emitted_count);
              local.mFramesReceived.Construct(
                  videoStats->frame_counts.key_frames +
                  videoStats->frame_counts.delta_frames);
              // Not including frames dropped in the rendering pipe, which is
              // not of webrtc's concern anyway?!
              local.mFramesDropped.Construct(videoStats->frames_dropped);
               */
              if (!report->mInboundRtpStreamStats.AppendElement(
                      std::move(local), fallible)) {
                mozalloc_handle_oom(0);
              }
            });
            return RTCStatsPromise::CreateAndResolve(std::move(report),
                                                     __func__);
          })
          ->Then(
              mStsThread, __func__,
              [pipeline = mPipeline](UniquePtr<RTCStatsCollection> aReport) {
                // Fill in Contributing Source statistics
                if (!aReport->mInboundRtpStreamStats.IsEmpty() &&
                    aReport->mInboundRtpStreamStats[0].mId.WasPassed()) {
                  pipeline->GetContributingSourceStats(
                      aReport->mInboundRtpStreamStats[0].mId.Value(),
                      aReport->mRtpContributingSourceStats);
                }
                return RTCStatsPromise::CreateAndResolve(std::move(aReport),
                                                         __func__);
              },
              [] {
                MOZ_CRASH("Unexpected reject");
                return RTCStatsPromise::CreateAndReject(NS_ERROR_UNEXPECTED,
                                                        __func__);
              }));

  if (mJsepTransceiver->mTransport.mComponents) {
    promises.AppendElement(mTransportHandler->GetIceStats(
        mJsepTransceiver->mTransport.mTransportId, mPipeline->GetNow()));
  }

  return promises;
}

void RTCRtpReceiver::GetContributingSources(
    nsTArray<RTCRtpContributingSource>& aSources) {
  // Duplicate code...
  if (mPipeline && mPipeline->mConduit) {
    nsTArray<dom::RTCRtpSourceEntry> sources;
    mPipeline->mConduit->GetRtpSources(sources);
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
  if (mPipeline && mPipeline->mConduit) {
    nsTArray<dom::RTCRtpSourceEntry> sources;
    mPipeline->mConduit->GetRtpSources(sources);
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
    mPipeline->Shutdown();
    mPipeline = nullptr;
  }
  mTransceiverImpl = nullptr;
  mCallThread = nullptr;
  mRtcpByeListener.DisconnectIfExists();
  mRtcpTimeoutListener.DisconnectIfExists();
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
  if (mPipeline->mConduit->type() == MediaSessionConduit::VIDEO) {
    return UpdateVideoConduit();
  }
  return UpdateAudioConduit();
}

nsresult RTCRtpReceiver::UpdateVideoConduit() {
  RefPtr<VideoSessionConduit> conduit =
      *mPipeline->mConduit->AsVideoSessionConduit();

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
    mSsrc = mJsepTransceiver->mRecvTrack.GetSsrcs().front();
    mVideoRtxSsrc = rtxSsrc;
  }

  // TODO (bug 1423041) once we pay attention to receiving MID's in RTP
  // packets (see bug 1405495) we could make this depending on the presence of
  // MID in the RTP packets instead of relying on the signaling.
  if (mJsepTransceiver->HasBundleLevel() &&
      (!mJsepTransceiver->mRecvTrack.GetNegotiatedDetails() ||
       !mJsepTransceiver->mRecvTrack.GetNegotiatedDetails()->GetExt(
           webrtc::RtpExtension::kMidUri))) {
    mCallThread->Dispatch(
        NewRunnableMethod("VideoSessionConduit::DisableSsrcChanges", conduit,
                          &VideoSessionConduit::DisableSsrcChanges));
  }

  if (mJsepTransceiver->mRecvTrack.GetNegotiatedDetails() &&
      mJsepTransceiver->mRecvTrack.GetActive()) {
    const auto& details(*mJsepTransceiver->mRecvTrack.GetNegotiatedDetails());

    {
      std::vector<webrtc::RtpExtension> extmaps;
      // @@NG read extmap from track
      details.ForEachRTPHeaderExtension(
          [&extmaps](const SdpExtmapAttributeList::Extmap& extmap) {
            extmaps.emplace_back(extmap.extensionname, extmap.entry);
          });
      mLocalRtpExtensions = extmaps;
    }

    std::vector<VideoCodecConfig> configs;
    nsresult rv = TransceiverImpl::NegotiatedDetailsToVideoCodecConfigs(
        details, &configs);

    if (NS_FAILED(rv)) {
      MOZ_LOG(gReceiverLog, LogLevel::Error,
              ("%s[%s]: %s Failed to convert "
               "JsepCodecDescriptions to VideoCodecConfigs (recv).",
               mPCHandle.c_str(), GetMid().c_str(), __FUNCTION__));
      return rv;
    }

    mVideoCodecs = configs;
    mVideoRtpRtcpConfig = Some(details.GetRtpRtcpConfig());
  }

  return NS_OK;
}

nsresult RTCRtpReceiver::UpdateAudioConduit() {
  RefPtr<AudioSessionConduit> conduit =
      *mPipeline->mConduit->AsAudioSessionConduit();

  if (!mJsepTransceiver->mRecvTrack.GetSsrcs().empty()) {
    MOZ_LOG(gReceiverLog, LogLevel::Debug,
            ("%s[%s]: %s Setting remote SSRC %u", mPCHandle.c_str(),
             GetMid().c_str(), __FUNCTION__,
             mJsepTransceiver->mRecvTrack.GetSsrcs().front()));
    mSsrc = mJsepTransceiver->mRecvTrack.GetSsrcs().front();
  }

  if (mJsepTransceiver->mRecvTrack.GetNegotiatedDetails() &&
      mJsepTransceiver->mRecvTrack.GetActive()) {
    const auto& details(*mJsepTransceiver->mRecvTrack.GetNegotiatedDetails());
    std::vector<AudioCodecConfig> configs;
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
    {
      std::vector<webrtc::RtpExtension> extmaps;
      // @@NG read extmap from track
      details.ForEachRTPHeaderExtension(
          [&extmaps](const SdpExtmapAttributeList::Extmap& extmap) {
            extmaps.emplace_back(extmap.extensionname, extmap.entry);
          });
      mLocalRtpExtensions = extmaps;
    }

    mAudioCodecs = configs;
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
  if (!mPipeline || mPipeline->IsVideo() || !mPipeline->mConduit) {
    return;
  }
  mPipeline->mConduit->InsertAudioLevelForContributingSource(
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
