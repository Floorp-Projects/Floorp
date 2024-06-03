/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RTCRtpReceiver.h"

#include <stdint.h>

#include <vector>
#include <string>
#include <set>

#include "call/call.h"
#include "call/audio_receive_stream.h"
#include "call/video_receive_stream.h"
#include "api/rtp_parameters.h"
#include "api/units/timestamp.h"
#include "api/units/time_delta.h"
#include "system_wrappers/include/clock.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

#include "RTCRtpTransceiver.h"
#include "PeerConnectionImpl.h"
#include "RTCStatsReport.h"
#include "mozilla/dom/RTCRtpReceiverBinding.h"
#include "mozilla/dom/RTCRtpSourcesBinding.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "jsep/JsepTransceiver.h"
#include "libwebrtcglue/MediaConduitControl.h"
#include "libwebrtcglue/MediaConduitInterface.h"
#include "transportbridge/MediaPipeline.h"
#include "sdp/SdpEnum.h"
#include "sdp/SdpAttribute.h"
#include "MediaTransportHandler.h"
#include "RemoteTrackSource.h"

#include "mozilla/dom/RTCRtpCapabilitiesBinding.h"
#include "mozilla/dom/MediaStreamTrack.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/AudioStreamTrack.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "mozilla/dom/RTCRtpScriptTransform.h"

#include "nsPIDOMWindow.h"
#include "PrincipalHandle.h"
#include "nsIPrincipal.h"
#include "MediaTrackGraph.h"
#include "nsStringFwd.h"
#include "MediaSegment.h"
#include "nsLiteralString.h"
#include "nsTArray.h"
#include "nsDOMNavigationTiming.h"
#include "MainThreadUtils.h"
#include "ErrorList.h"
#include "nsWrapperCache.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"
#include "nsThreadUtils.h"
#include "PerformanceRecorder.h"

#include "mozilla/NullPrincipal.h"
#include "mozilla/Preferences.h"
#include "mozilla/StateMirroring.h"
#include "mozilla/Logging.h"
#include "mozilla/RefPtr.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/StateWatching.h"
#include "mozilla/Maybe.h"
#include "mozilla/Assertions.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/MozPromise.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/fallible.h"
#include "mozilla/mozalloc_oom.h"
#include "mozilla/ErrorResult.h"
#include "js/RootingAPI.h"

namespace mozilla::dom {

LazyLogModule gReceiverLog("RTCRtpReceiver");

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(RTCRtpReceiver)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(RTCRtpReceiver)
  tmp->Unlink();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(RTCRtpReceiver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow, mPc, mTransceiver, mTransform,
                                    mTrack, mTrackSource)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(RTCRtpReceiver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(RTCRtpReceiver)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RTCRtpReceiver)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

static PrincipalHandle GetPrincipalHandle(nsPIDOMWindowInner* aWindow,
                                          PrincipalPrivacy aPrivacy) {
  // Set the principal used for creating the tracks. This makes the track
  // data (audio/video samples) accessible to the receiving page. We're
  // only certain that privacy hasn't been requested if we're connected.
  nsCOMPtr<nsIScriptObjectPrincipal> winPrincipal = do_QueryInterface(aWindow);
  RefPtr<nsIPrincipal> principal = winPrincipal->GetPrincipal();
  if (NS_WARN_IF(!principal)) {
    principal = NullPrincipal::CreateWithoutOriginAttributes();
  } else if (aPrivacy == PrincipalPrivacy::Private) {
    principal = NullPrincipal::CreateWithInheritedAttributes(principal);
  }
  return MakePrincipalHandle(principal);
}

#define INIT_CANONICAL(name, val)         \
  name(AbstractThread::MainThread(), val, \
       "RTCRtpReceiver::" #name " (Canonical)")

RTCRtpReceiver::RTCRtpReceiver(
    nsPIDOMWindowInner* aWindow, PrincipalPrivacy aPrivacy,
    PeerConnectionImpl* aPc, MediaTransportHandler* aTransportHandler,
    AbstractThread* aCallThread, nsISerialEventTarget* aStsThread,
    MediaSessionConduit* aConduit, RTCRtpTransceiver* aTransceiver,
    const TrackingId& aTrackingId)
    : mWatchManager(this, AbstractThread::MainThread()),
      mWindow(aWindow),
      mPc(aPc),
      mCallThread(aCallThread),
      mStsThread(aStsThread),
      mTransportHandler(aTransportHandler),
      mTransceiver(aTransceiver),
      INIT_CANONICAL(mSsrc, 0),
      INIT_CANONICAL(mVideoRtxSsrc, 0),
      INIT_CANONICAL(mLocalRtpExtensions, RtpExtList()),
      INIT_CANONICAL(mAudioCodecs, std::vector<AudioCodecConfig>()),
      INIT_CANONICAL(mVideoCodecs, std::vector<VideoCodecConfig>()),
      INIT_CANONICAL(mVideoRtpRtcpConfig, Nothing()),
      INIT_CANONICAL(mReceiving, false),
      INIT_CANONICAL(mFrameTransformerProxy, nullptr) {
  PrincipalHandle principalHandle = GetPrincipalHandle(aWindow, aPrivacy);
  const bool isAudio = aConduit->type() == MediaSessionConduit::AUDIO;

  MediaTrackGraph* graph = MediaTrackGraph::GetInstance(
      isAudio ? MediaTrackGraph::AUDIO_THREAD_DRIVER
              : MediaTrackGraph::SYSTEM_THREAD_DRIVER,
      aWindow, MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      MediaTrackGraph::DEFAULT_OUTPUT_DEVICE);

  if (isAudio) {
    auto* source = graph->CreateSourceTrack(MediaSegment::AUDIO);
    mTrackSource = MakeAndAddRef<RemoteTrackSource>(
        source, this, principalHandle, u"remote audio"_ns, aTrackingId);
    mTrack = MakeAndAddRef<AudioStreamTrack>(aWindow, source, mTrackSource);
    mPipeline = MakeAndAddRef<MediaPipelineReceiveAudio>(
        mPc->GetHandle(), aTransportHandler, aCallThread, mStsThread.get(),
        *aConduit->AsAudioSessionConduit(), mTrackSource->Stream(), aTrackingId,
        principalHandle, aPrivacy);
  } else {
    auto* source = graph->CreateSourceTrack(MediaSegment::VIDEO);
    mTrackSource = MakeAndAddRef<RemoteTrackSource>(
        source, this, principalHandle, u"remote video"_ns, aTrackingId);
    mTrack = MakeAndAddRef<VideoStreamTrack>(aWindow, source, mTrackSource);
    mPipeline = MakeAndAddRef<MediaPipelineReceiveVideo>(
        mPc->GetHandle(), aTransportHandler, aCallThread, mStsThread.get(),
        *aConduit->AsVideoSessionConduit(), mTrackSource->Stream(), aTrackingId,
        principalHandle, aPrivacy);
  }

  mPipeline->InitControl(this);

  // Spec says remote tracks start out muted.
  mTrackSource->SetMuted(true);

  // Until Bug 1232234 is fixed, we'll get extra RTCP BYES during renegotiation,
  // so we'll disable muting on RTCP BYE and timeout for now.
  if (Preferences::GetBool("media.peerconnection.mute_on_bye_or_timeout",
                           false)) {
    mRtcpByeListener = aConduit->RtcpByeEvent().Connect(
        GetMainThreadSerialEventTarget(), this, &RTCRtpReceiver::OnRtcpBye);
    mRtcpTimeoutListener = aConduit->RtcpTimeoutEvent().Connect(
        GetMainThreadSerialEventTarget(), this, &RTCRtpReceiver::OnRtcpTimeout);
  }

  mWatchManager.Watch(mReceiveTrackMute,
                      &RTCRtpReceiver::UpdateReceiveTrackMute);

  mParameters.mCodecs.Construct();
}

#undef INIT_CANONICAL

RTCRtpReceiver::~RTCRtpReceiver() { MOZ_ASSERT(!mPipeline); }

JSObject* RTCRtpReceiver::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return RTCRtpReceiver_Binding::Wrap(aCx, this, aGivenProto);
}

RTCDtlsTransport* RTCRtpReceiver::GetTransport() const {
  if (!mTransceiver) {
    return nullptr;
  }
  return mTransceiver->GetDtlsTransport();
}

void RTCRtpReceiver::GetCapabilities(
    const GlobalObject&, const nsAString& aKind,
    Nullable<dom::RTCRtpCapabilities>& aResult) {
  PeerConnectionImpl::GetCapabilities(aKind, aResult, sdp::Direction::kRecv);
}

void RTCRtpReceiver::GetParameters(RTCRtpReceiveParameters& aParameters) const {
  aParameters = mParameters;
}

already_AddRefed<Promise> RTCRtpReceiver::GetStats(ErrorResult& aError) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mWindow);
  RefPtr<Promise> promise = Promise::Create(global, aError);
  if (NS_WARN_IF(aError.Failed())) {
    return nullptr;
  }

  if (NS_WARN_IF(!mTransceiver)) {
    // TODO(bug 1056433): When we stop nulling this out when the PC is closed
    // (or when the transceiver is stopped), we can remove this code. We
    // resolve instead of reject in order to make this eventual change in
    // behavior a little smaller.
    promise->MaybeResolve(new RTCStatsReport(mWindow));
    return promise.forget();
  }

  mTransceiver->ChainToDomPromiseWithCodecStats(GetStatsInternal(), promise);
  return promise.forget();
}

nsTArray<RefPtr<RTCStatsPromise>> RTCRtpReceiver::GetStatsInternal(
    bool aSkipIceStats) {
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

            Maybe<uint32_t> ssrc = pipeline->mConduit->GetRemoteSSRC();

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
                  ssrc.apply([&](uint32_t aSsrc) { aRemote.mSsrc = aSsrc; });
                  aRemote.mKind = kind;
                  aRemote.mMediaType.Construct(
                      kind);  // mediaType is the old name for kind.
                  aRemote.mLocalId.Construct(localId);
                };

            auto constructCommonInboundRtpStats =
                [&](RTCInboundRtpStreamStats& aLocal) {
                  aLocal.mTrackIdentifier = recvTrackId;
                  aLocal.mTimestamp.Construct(
                      pipeline->GetTimestampMaker().GetNow().ToDom());
                  aLocal.mId.Construct(localId);
                  aLocal.mType.Construct(RTCStatsType::Inbound_rtp);
                  ssrc.apply([&](uint32_t aSsrc) { aLocal.mSsrc = aSsrc; });
                  aLocal.mKind = kind;
                  aLocal.mMediaType.Construct(
                      kind);  // mediaType is the old name for kind.
                  if (remoteId.Length()) {
                    aLocal.mRemoteId.Construct(remoteId);
                  }
                };

            asAudio.apply([&](auto& aConduit) {
              Maybe<webrtc::AudioReceiveStreamInterface::Stats> audioStats =
                  aConduit->GetReceiverStats();
              if (audioStats.isNothing()) {
                return;
              }

              if (!audioStats->last_packet_received.has_value()) {
                // By spec: "The lifetime of all RTP monitored objects starts
                // when the RTP stream is first used: When the first RTP packet
                // is sent or received on the SSRC it represents"
                return;
              }

              // First, fill in remote stat with rtcp sender data, if present.
              if (audioStats->last_sender_report_timestamp_ms) {
                RTCRemoteOutboundRtpStreamStats remote;
                constructCommonRemoteOutboundRtpStats(
                    remote,
                    RTCStatsTimestamp::FromNtp(
                        aConduit->GetTimestampMaker(),
                        webrtc::Timestamp::Millis(
                            *audioStats->last_sender_report_timestamp_ms) +
                            webrtc::TimeDelta::Seconds(webrtc::kNtpJan1970))
                        .ToDom());
                remote.mPacketsSent.Construct(
                    audioStats->sender_reports_packets_sent);
                remote.mBytesSent.Construct(
                    audioStats->sender_reports_bytes_sent);
                remote.mRemoteTimestamp.Construct(
                    *audioStats->last_sender_report_remote_timestamp_ms);
                if (!report->mRemoteOutboundRtpStreamStats.AppendElement(
                        std::move(remote), fallible)) {
                  mozalloc_handle_oom(0);
                }
              }

              // Then, fill in local side (with cross-link to remote only if
              // present)
              RTCInboundRtpStreamStats local;
              constructCommonInboundRtpStats(local);
              local.mJitter.Construct(audioStats->jitter_ms / 1000.0);
              local.mPacketsLost.Construct(audioStats->packets_lost);
              local.mPacketsReceived.Construct(audioStats->packets_received);
              local.mPacketsDiscarded.Construct(audioStats->packets_discarded);
              local.mBytesReceived.Construct(
                  audioStats->payload_bytes_received);
              // Always missing from libwebrtc stats
              // if (audioStats->estimated_playout_ntp_timestamp_ms) {
              //   local.mEstimatedPlayoutTimestamp.Construct(
              //       RTCStatsTimestamp::FromNtp(
              //           aConduit->GetTimestampMaker(),
              //           webrtc::Timestamp::Millis(
              //               *audioStats->estimated_playout_ntp_timestamp_ms))
              //           .ToDom());
              // }
              local.mJitterBufferDelay.Construct(
                  audioStats->jitter_buffer_delay_seconds);
              local.mJitterBufferEmittedCount.Construct(
                  audioStats->jitter_buffer_emitted_count);
              local.mTotalSamplesReceived.Construct(
                  audioStats->total_samples_received);
              local.mConcealedSamples.Construct(audioStats->concealed_samples);
              local.mSilentConcealedSamples.Construct(
                  audioStats->silent_concealed_samples);
              if (audioStats->last_packet_received.has_value()) {
                local.mLastPacketReceivedTimestamp.Construct(
                    RTCStatsTimestamp::FromNtp(
                        aConduit->GetTimestampMaker(),
                        webrtc::Timestamp::Millis(
                            audioStats->last_packet_received->ms()) +
                            webrtc::TimeDelta::Seconds(webrtc::kNtpJan1970))
                        .ToDom());
              }
              local.mHeaderBytesReceived.Construct(
                  audioStats->header_and_padding_bytes_received);
              local.mFecPacketsReceived.Construct(
                  audioStats->fec_packets_received);
              local.mFecPacketsDiscarded.Construct(
                  audioStats->fec_packets_discarded);
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

              if (!report->mInboundRtpStreamStats.AppendElement(
                      std::move(local), fallible)) {
                mozalloc_handle_oom(0);
              }
            });

            asVideo.apply([&](auto& aConduit) {
              Maybe<webrtc::VideoReceiveStreamInterface::Stats> videoStats =
                  aConduit->GetReceiverStats();
              if (videoStats.isNothing()) {
                return;
              }

              if (!videoStats->rtp_stats.last_packet_received.has_value()) {
                // By spec: "The lifetime of all RTP monitored objects starts
                // when the RTP stream is first used: When the first RTP packet
                // is sent or received on the SSRC it represents"
                return;
              }

              // First, fill in remote stat with rtcp sender data, if present.
              if (videoStats->rtcp_sender_ntp_timestamp_ms) {
                RTCRemoteOutboundRtpStreamStats remote;
                constructCommonRemoteOutboundRtpStats(
                    remote, RTCStatsTimestamp::FromNtp(
                                aConduit->GetTimestampMaker(),
                                webrtc::Timestamp::Millis(
                                    videoStats->rtcp_sender_ntp_timestamp_ms))
                                .ToDom());
                remote.mPacketsSent.Construct(
                    videoStats->rtcp_sender_packets_sent);
                remote.mBytesSent.Construct(
                    videoStats->rtcp_sender_octets_sent);
                remote.mRemoteTimestamp.Construct(
                    (webrtc::TimeDelta::Millis(
                         videoStats->rtcp_sender_remote_ntp_timestamp_ms) -
                     webrtc::TimeDelta::Seconds(webrtc::kNtpJan1970))
                        .ms());
                if (!report->mRemoteOutboundRtpStreamStats.AppendElement(
                        std::move(remote), fallible)) {
                  mozalloc_handle_oom(0);
                }
              }

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
              local.mPacketsDiscarded.Construct(videoStats->packets_discarded);
              local.mDiscardedPackets.Construct(videoStats->packets_discarded);
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

              local.mFramesPerSecond.Construct(videoStats->decode_frame_rate);
              local.mFrameWidth.Construct(videoStats->width);
              local.mFrameHeight.Construct(videoStats->height);
              // XXX: key_frames + delta_frames may undercount frames because
              // they were dropped in FrameBuffer::InsertFrame. (bug 1766553)
              local.mFramesReceived.Construct(
                  videoStats->frame_counts.key_frames +
                  videoStats->frame_counts.delta_frames);
              local.mJitterBufferDelay.Construct(
                  videoStats->jitter_buffer_delay.seconds<double>());
              local.mJitterBufferEmittedCount.Construct(
                  videoStats->jitter_buffer_emitted_count);

              if (videoStats->qp_sum) {
                local.mQpSum.Construct(videoStats->qp_sum.value());
              }
              local.mTotalDecodeTime.Construct(
                  double(videoStats->total_decode_time.ms()) / 1000);
              local.mTotalInterFrameDelay.Construct(
                  videoStats->total_inter_frame_delay);
              local.mTotalSquaredInterFrameDelay.Construct(
                  videoStats->total_squared_inter_frame_delay);
              if (videoStats->rtp_stats.last_packet_received.has_value()) {
                local.mLastPacketReceivedTimestamp.Construct(
                    RTCStatsTimestamp::FromNtp(
                        aConduit->GetTimestampMaker(),
                        webrtc::Timestamp::Millis(
                            videoStats->rtp_stats.last_packet_received->ms()) +
                            webrtc::TimeDelta::Seconds(webrtc::kNtpJan1970))
                        .ToDom());
              }
              local.mHeaderBytesReceived.Construct(
                  videoStats->rtp_stats.packet_counter.header_bytes +
                  videoStats->rtp_stats.packet_counter.padding_bytes);
              local.mTotalProcessingDelay.Construct(
                  videoStats->total_processing_delay.seconds<double>());
              /*
               * Potential new stats that are now available upstream
                   .if (videoStats->estimated_playout_ntp_timestamp_ms) {
                local.mEstimatedPlayoutTimestamp.Construct(
                    RTCStatsTimestamp::FromNtp(
                        aConduit->GetTimestampMaker(),
                        webrtc::Timestamp::Millis(
                            *videoStats->estimated_playout_ntp_timestamp_ms))
                        .ToDom());
              }
               */
              // Not including frames dropped in the rendering pipe, which
              // is not of webrtc's concern anyway?!
              local.mFramesDropped.Construct(videoStats->frames_dropped);
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

  if (!aSkipIceStats && GetJsepTransceiver().mTransport.mComponents) {
    promises.AppendElement(mTransportHandler->GetIceStats(
        GetJsepTransceiver().mTransport.mTransportId,
        mPipeline->GetTimestampMaker().GetNow().ToDom()));
  }

  return promises;
}

void RTCRtpReceiver::SetJitterBufferTarget(
    const Nullable<DOMHighResTimeStamp>& aTargetMs, ErrorResult& aError) {
  // Spec says jitter buffer target cannot be negative or larger than 4000
  // milliseconds and to throw RangeError if it is. If an invalid value is
  // received we return early to preserve the current JitterBufferTarget
  // internal slot and jitter buffer values.
  if (mPipeline && mPipeline->mConduit) {
    if (!aTargetMs.IsNull() &&
        (aTargetMs.Value() < 0.0 || aTargetMs.Value() > 4000.0)) {
      aError.ThrowRangeError<MSG_VALUE_OUT_OF_RANGE>("jitterBufferTarget");
      return;
    }

    mJitterBufferTarget.reset();

    if (!aTargetMs.IsNull()) {
      mJitterBufferTarget = Some(aTargetMs.Value());
    }
    // If aJitterBufferTarget is null then we are resetting the jitter buffer so
    // pass the default target of 0.0.
    mPipeline->mConduit->SetJitterBufferTarget(
        mJitterBufferTarget.valueOr(0.0));
  }
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
  MOZ_ASSERT(NS_IsMainThread());
  mWatchManager.Shutdown();
  if (mPipeline) {
    mPipeline->Shutdown();
    mPipeline = nullptr;
  }
  if (mTrackSource) {
    mTrackSource->Destroy();
  }
  mCallThread = nullptr;
  mRtcpByeListener.DisconnectIfExists();
  mRtcpTimeoutListener.DisconnectIfExists();
  mUnmuteListener.DisconnectIfExists();
  if (mTransform) {
    mTransform->GetProxy().SetReceiver(nullptr);
  }
}

void RTCRtpReceiver::BreakCycles() {
  mWindow = nullptr;
  mPc = nullptr;
  mTrack = nullptr;
  mTrackSource = nullptr;
}

void RTCRtpReceiver::Unlink() {
  if (mTransceiver) {
    mTransceiver->Unlink();
  }
}

void RTCRtpReceiver::UpdateTransport() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mHaveSetupTransport) {
    mPipeline->SetLevel(GetJsepTransceiver().GetLevel());
    mHaveSetupTransport = true;
  }

  UniquePtr<MediaPipelineFilter> filter;

  auto const& details = GetJsepTransceiver().mRecvTrack.GetNegotiatedDetails();
  if (GetJsepTransceiver().HasBundleLevel() && details) {
    std::vector<webrtc::RtpExtension> extmaps;
    details->ForEachRTPHeaderExtension(
        [&extmaps](const SdpExtmapAttributeList::Extmap& extmap) {
          extmaps.emplace_back(extmap.extensionname, extmap.entry);
        });
    filter = MakeUnique<MediaPipelineFilter>(extmaps);

    // Add remote SSRCs so we can distinguish which RTP packets actually
    // belong to this pipeline (also RTCP sender reports).
    for (uint32_t ssrc : GetJsepTransceiver().mRecvTrack.GetSsrcs()) {
      filter->AddRemoteSSRC(ssrc);
    }
    for (uint32_t ssrc : GetJsepTransceiver().mRecvTrack.GetRtxSsrcs()) {
      filter->AddRemoteSSRC(ssrc);
    }
    auto mid = Maybe<std::string>();
    if (GetMid() != "") {
      mid = Some(GetMid());
    }
    filter->SetRemoteMediaStreamId(mid);

    // Add unique payload types as a last-ditch fallback
    auto uniquePts = GetJsepTransceiver()
                         .mRecvTrack.GetNegotiatedDetails()
                         ->GetUniqueReceivePayloadTypes();
    for (unsigned char& uniquePt : uniquePts) {
      filter->AddUniqueReceivePT(uniquePt);
    }
  }

  mPipeline->UpdateTransport_m(GetJsepTransceiver().mTransport.mTransportId,
                               std::move(filter));
}

void RTCRtpReceiver::UpdateConduit() {
  if (mPipeline->mConduit->type() == MediaSessionConduit::VIDEO) {
    UpdateVideoConduit();
  } else {
    UpdateAudioConduit();
  }

  if ((mReceiving = mTransceiver->IsReceiving())) {
    mHaveStartedReceiving = true;
  }
}

void RTCRtpReceiver::UpdateVideoConduit() {
  RefPtr<VideoSessionConduit> conduit =
      *mPipeline->mConduit->AsVideoSessionConduit();

  // NOTE(pkerr) - this is new behavior. Needed because the
  // CreateVideoReceiveStream method of the Call API will assert (in debug)
  // and fail if a value is not provided for the remote_ssrc that will be used
  // by the far-end sender.
  if (!GetJsepTransceiver().mRecvTrack.GetSsrcs().empty()) {
    MOZ_LOG(gReceiverLog, LogLevel::Debug,
            ("%s[%s]: %s Setting remote SSRC %u", mPc->GetHandle().c_str(),
             GetMid().c_str(), __FUNCTION__,
             GetJsepTransceiver().mRecvTrack.GetSsrcs().front()));
    uint32_t rtxSsrc =
        GetJsepTransceiver().mRecvTrack.GetRtxSsrcs().empty()
            ? 0
            : GetJsepTransceiver().mRecvTrack.GetRtxSsrcs().front();
    mSsrc = GetJsepTransceiver().mRecvTrack.GetSsrcs().front();
    mVideoRtxSsrc = rtxSsrc;

    // TODO (bug 1423041) once we pay attention to receiving MID's in RTP
    // packets (see bug 1405495) we could make this depending on the presence of
    // MID in the RTP packets instead of relying on the signaling.
    // In any case, do not disable SSRC changes if no SSRCs were negotiated
    if (GetJsepTransceiver().HasBundleLevel() &&
        (!GetJsepTransceiver().mRecvTrack.GetNegotiatedDetails() ||
         !GetJsepTransceiver().mRecvTrack.GetNegotiatedDetails()->GetExt(
             webrtc::RtpExtension::kMidUri))) {
      mCallThread->Dispatch(
          NewRunnableMethod("VideoSessionConduit::DisableSsrcChanges", conduit,
                            &VideoSessionConduit::DisableSsrcChanges));
    }
  }

  if (GetJsepTransceiver().mRecvTrack.GetNegotiatedDetails() &&
      GetJsepTransceiver().mRecvTrack.GetActive()) {
    const auto& details(
        *GetJsepTransceiver().mRecvTrack.GetNegotiatedDetails());

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
    RTCRtpTransceiver::NegotiatedDetailsToVideoCodecConfigs(details, &configs);
    if (configs.empty()) {
      // TODO: Are we supposed to plumb this error back to JS? This does not
      // seem like a failure to set an answer, it just means that codec
      // negotiation failed. For now, we're just doing the same thing we do
      // if negotiation as a whole failed.
      MOZ_LOG(gReceiverLog, LogLevel::Error,
              ("%s[%s]: %s  No video codecs were negotiated (recv).",
               mPc->GetHandle().c_str(), GetMid().c_str(), __FUNCTION__));
      return;
    }

    mVideoCodecs = configs;
    mVideoRtpRtcpConfig = Some(details.GetRtpRtcpConfig());
  }
}

void RTCRtpReceiver::UpdateAudioConduit() {
  RefPtr<AudioSessionConduit> conduit =
      *mPipeline->mConduit->AsAudioSessionConduit();

  if (!GetJsepTransceiver().mRecvTrack.GetSsrcs().empty()) {
    MOZ_LOG(gReceiverLog, LogLevel::Debug,
            ("%s[%s]: %s Setting remote SSRC %u", mPc->GetHandle().c_str(),
             GetMid().c_str(), __FUNCTION__,
             GetJsepTransceiver().mRecvTrack.GetSsrcs().front()));
    mSsrc = GetJsepTransceiver().mRecvTrack.GetSsrcs().front();

    // TODO (bug 1423041) once we pay attention to receiving MID's in RTP
    // packets (see bug 1405495) we could make this depending on the presence of
    // MID in the RTP packets instead of relying on the signaling.
    // In any case, do not disable SSRC changes if no SSRCs were negotiated
    if (GetJsepTransceiver().HasBundleLevel() &&
        (!GetJsepTransceiver().mRecvTrack.GetNegotiatedDetails() ||
         !GetJsepTransceiver().mRecvTrack.GetNegotiatedDetails()->GetExt(
             webrtc::RtpExtension::kMidUri))) {
      mCallThread->Dispatch(
          NewRunnableMethod("AudioSessionConduit::DisableSsrcChanges", conduit,
                            &AudioSessionConduit::DisableSsrcChanges));
    }
  }

  if (GetJsepTransceiver().mRecvTrack.GetNegotiatedDetails() &&
      GetJsepTransceiver().mRecvTrack.GetActive()) {
    const auto& details(
        *GetJsepTransceiver().mRecvTrack.GetNegotiatedDetails());
    std::vector<AudioCodecConfig> configs;
    RTCRtpTransceiver::NegotiatedDetailsToAudioCodecConfigs(details, &configs);
    if (configs.empty()) {
      // TODO: Are we supposed to plumb this error back to JS? This does not
      // seem like a failure to set an answer, it just means that codec
      // negotiation failed. For now, we're just doing the same thing we do
      // if negotiation as a whole failed.
      MOZ_LOG(gReceiverLog, LogLevel::Error,
              ("%s[%s]: %s No audio codecs were negotiated (recv)",
               mPc->GetHandle().c_str(), GetMid().c_str(), __FUNCTION__));
      return;
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
}

void RTCRtpReceiver::Stop() {
  MOZ_ASSERT(mTransceiver->Stopped() || mTransceiver->Stopping());
  mReceiving = false;
  GetMainThreadSerialEventTarget()->Dispatch(NS_NewRunnableFunction(
      __func__, [trackSource = mTrackSource] { trackSource->ForceEnded(); }));
}

bool RTCRtpReceiver::HasTrack(const dom::MediaStreamTrack* aTrack) const {
  return !aTrack || (mTrack == aTrack);
}

void RTCRtpReceiver::SyncFromJsep(const JsepTransceiver& aJsepTransceiver) {
  if (!mPipeline) {
    return;
  }

  if (GetJsepTransceiver().mRecvTrack.GetNegotiatedDetails()) {
    const auto& details(
        *GetJsepTransceiver().mRecvTrack.GetNegotiatedDetails());
    mParameters.mCodecs.Reset();
    mParameters.mCodecs.Construct();
    if (details.GetEncodingCount()) {
      for (const auto& jsepCodec : details.GetEncoding(0).GetCodecs()) {
        RTCRtpCodecParameters codec;
        RTCRtpTransceiver::ToDomRtpCodecParameters(*jsepCodec, &codec);
        Unused << mParameters.mCodecs.Value().AppendElement(codec, fallible);
        if (jsepCodec->Type() == SdpMediaSection::kVideo) {
          const JsepVideoCodecDescription& videoJsepCodec =
              static_cast<JsepVideoCodecDescription&>(*jsepCodec);
          if (videoJsepCodec.mRtxEnabled) {
            RTCRtpCodecParameters rtx;
            RTCRtpTransceiver::ToDomRtpCodecParametersRtx(videoJsepCodec, &rtx);
            Unused << mParameters.mCodecs.Value().AppendElement(rtx, fallible);
          }
        }
      }
    }
  }

  // Spec says we set [[Receptive]] to true on sLD(sendrecv/recvonly), and to
  // false on sRD(recvonly/inactive), sLD(sendonly/inactive), or when stop()
  // is called.
  bool wasReceptive = mReceptive;
  mReceptive = aJsepTransceiver.mRecvTrack.GetReceptive();
  if (!wasReceptive && mReceptive) {
    mUnmuteListener = mPipeline->mConduit->RtpPacketEvent().Connect(
        GetMainThreadSerialEventTarget(), this, &RTCRtpReceiver::OnRtpPacket);
  } else if (wasReceptive && !mReceptive) {
    mUnmuteListener.DisconnectIfExists();
  }
}

void RTCRtpReceiver::SyncToJsep(JsepTransceiver& aJsepTransceiver) const {
  if (!mTransceiver->GetPreferredCodecs().empty()) {
    aJsepTransceiver.mRecvTrack.PopulateCodecs(
        mTransceiver->GetPreferredCodecs(),
        mTransceiver->GetPreferredCodecsInUse());
  }
}

void RTCRtpReceiver::UpdateStreams(StreamAssociationChanges* aChanges) {
  // We don't sort and use set_difference, because we need to report the
  // added/removed streams in the order that they appear in the SDP.
  std::set<std::string> newIds(
      GetJsepTransceiver().mRecvTrack.GetStreamIds().begin(),
      GetJsepTransceiver().mRecvTrack.GetStreamIds().end());
  MOZ_ASSERT(GetJsepTransceiver().mRecvTrack.GetRemoteSetSendBit() ||
             newIds.empty());
  bool needsTrackEvent = false;
  for (const auto& id : mStreamIds) {
    if (!newIds.count(id)) {
      aChanges->mStreamAssociationsRemoved.push_back({mTrack, id});
    }
  }

  std::set<std::string> oldIds(mStreamIds.begin(), mStreamIds.end());
  for (const auto& id : GetJsepTransceiver().mRecvTrack.GetStreamIds()) {
    if (!oldIds.count(id)) {
      needsTrackEvent = true;
      aChanges->mStreamAssociationsAdded.push_back({mTrack, id});
    }
  }

  mStreamIds = GetJsepTransceiver().mRecvTrack.GetStreamIds();

  if (mRemoteSetSendBit !=
      GetJsepTransceiver().mRecvTrack.GetRemoteSetSendBit()) {
    mRemoteSetSendBit = GetJsepTransceiver().mRecvTrack.GetRemoteSetSendBit();
    if (mRemoteSetSendBit) {
      needsTrackEvent = true;
    } else {
      aChanges->mReceiversToMute.push_back(this);
    }
  }

  if (needsTrackEvent) {
    aChanges->mTrackEvents.push_back({this, mStreamIds});
  }
}

void RTCRtpReceiver::UpdatePrincipalPrivacy(PrincipalPrivacy aPrivacy) {
  if (!mPipeline) {
    return;
  }

  if (aPrivacy != PrincipalPrivacy::Private) {
    return;
  }

  mPipeline->SetPrivatePrincipal(GetPrincipalHandle(mWindow, aPrivacy));
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

void RTCRtpReceiver::OnRtcpBye() { mReceiveTrackMute = true; }

void RTCRtpReceiver::OnRtcpTimeout() { mReceiveTrackMute = true; }

void RTCRtpReceiver::SetTrackMuteFromRemoteSdp() {
  MOZ_ASSERT(!mReceptive,
             "PeerConnectionImpl should have blocked unmute events prior to "
             "firing mute");
  mReceiveTrackMute = true;
  // Set the mute state (and fire the mute event) synchronously. Unmute is
  // handled asynchronously after receiving RTP packets.
  UpdateReceiveTrackMute();
  MOZ_ASSERT(mTrack->Muted(), "Muted state was indeed set synchronously");
}

void RTCRtpReceiver::OnRtpPacket() {
  MOZ_ASSERT(mReceptive, "We should not be registered unless this is set!");
  // We should be registered since we're currently getting a callback.
  mUnmuteListener.Disconnect();
  if (mReceptive) {
    mReceiveTrackMute = false;
  }
}

void RTCRtpReceiver::UpdateReceiveTrackMute() {
  if (!mTrack) {
    return;
  }
  if (!mTrackSource) {
    return;
  }
  // This sets the muted state for mTrack and all its clones.
  // Idempotent -- only reacts to changes.
  mTrackSource->SetMuted(mReceiveTrackMute);
}

std::string RTCRtpReceiver::GetMid() const {
  return mTransceiver->GetMidAscii();
}

JsepTransceiver& RTCRtpReceiver::GetJsepTransceiver() {
  MOZ_ASSERT(mTransceiver);
  return mTransceiver->GetJsepTransceiver();
}

const JsepTransceiver& RTCRtpReceiver::GetJsepTransceiver() const {
  MOZ_ASSERT(mTransceiver);
  return mTransceiver->GetJsepTransceiver();
}

void RTCRtpReceiver::SetTransform(RTCRtpScriptTransform* aTransform,
                                  ErrorResult& aError) {
  if (aTransform == mTransform.get()) {
    // Ok... smile and nod
    // TODO: Depending on spec, this might throw
    // https://github.com/w3c/webrtc-encoded-transform/issues/189
    return;
  }

  if (aTransform && aTransform->IsClaimed()) {
    aError.ThrowInvalidStateError("transform has already been used elsewhere");
    return;
  }

  if (aTransform) {
    mFrameTransformerProxy = &aTransform->GetProxy();
  } else {
    mFrameTransformerProxy = nullptr;
  }

  if (mTransform) {
    mTransform->GetProxy().SetReceiver(nullptr);
  }

  mTransform = const_cast<RTCRtpScriptTransform*>(aTransform);

  if (mTransform) {
    mTransform->GetProxy().SetReceiver(this);
    mTransform->SetClaimed();
  }
}

void RTCRtpReceiver::RequestKeyFrame() {
  if (!mTransform || !mPipeline) {
    return;
  }

  mPipeline->mConduit->AsVideoSessionConduit().apply([&](const auto& conduit) {
    conduit->RequestKeyFrame(&mTransform->GetProxy());
  });
}

}  // namespace mozilla::dom

#undef LOGTAG
