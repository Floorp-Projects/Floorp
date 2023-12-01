/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RTCRtpSender.h"

#include <stdint.h>

#include <vector>
#include <string>
#include <algorithm>
#include <utility>
#include <iterator>
#include <set>
#include <sstream>

#include "system_wrappers/include/clock.h"
#include "call/call.h"
#include "api/rtp_parameters.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video_codecs/video_codec.h"
#include "api/video/video_codec_constants.h"
#include "call/audio_send_stream.h"
#include "call/video_send_stream.h"
#include "modules/rtp_rtcp/include/report_block_data.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

#include "nsPIDOMWindow.h"
#include "nsString.h"
#include "MainThreadUtils.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"
#include "nsISupports.h"
#include "nsLiteralString.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsWrapperCache.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/fallible.h"
#include "mozilla/Logging.h"
#include "mozilla/mozalloc_oom.h"
#include "mozilla/MozPromise.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StateWatching.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/StateMirroring.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/MediaStreamTrack.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/RTCRtpScriptTransform.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "mozilla/dom/RTCRtpSenderBinding.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/RTCRtpParametersBinding.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "mozilla/glean/GleanMetrics.h"
#include "js/RootingAPI.h"
#include "jsep/JsepTransceiver.h"
#include "RTCStatsReport.h"
#include "RTCRtpTransceiver.h"
#include "PeerConnectionImpl.h"
#include "libwebrtcglue/CodecConfig.h"
#include "libwebrtcglue/MediaConduitControl.h"
#include "libwebrtcglue/MediaConduitInterface.h"
#include "sdp/SdpAttribute.h"
#include "sdp/SdpEnum.h"

namespace mozilla::dom {

LazyLogModule gSenderLog("RTCRtpSender");

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(RTCRtpSender)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(RTCRtpSender)
  tmp->Unlink();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(RTCRtpSender)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow, mPc, mSenderTrack, mTransceiver,
                                    mStreams, mTransform, mDtmf)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(RTCRtpSender)
NS_IMPL_CYCLE_COLLECTING_RELEASE(RTCRtpSender)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RTCRtpSender)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

#define INIT_CANONICAL(name, val) \
  name(AbstractThread::MainThread(), val, "RTCRtpSender::" #name " (Canonical)")

RTCRtpSender::RTCRtpSender(nsPIDOMWindowInner* aWindow, PeerConnectionImpl* aPc,
                           MediaTransportHandler* aTransportHandler,
                           AbstractThread* aCallThread,
                           nsISerialEventTarget* aStsThread,
                           MediaSessionConduit* aConduit,
                           dom::MediaStreamTrack* aTrack,
                           const Sequence<RTCRtpEncodingParameters>& aEncodings,
                           RTCRtpTransceiver* aTransceiver)
    : mWatchManager(this, AbstractThread::MainThread()),
      mWindow(aWindow),
      mPc(aPc),
      mSenderTrack(aTrack),
      mTransportHandler(aTransportHandler),
      mTransceiver(aTransceiver),
      INIT_CANONICAL(mSsrcs, Ssrcs()),
      INIT_CANONICAL(mVideoRtxSsrcs, Ssrcs()),
      INIT_CANONICAL(mLocalRtpExtensions, RtpExtList()),
      INIT_CANONICAL(mAudioCodec, Nothing()),
      INIT_CANONICAL(mVideoCodec, Nothing()),
      INIT_CANONICAL(mVideoRtpRtcpConfig, Nothing()),
      INIT_CANONICAL(mVideoCodecMode, webrtc::VideoCodecMode::kRealtimeVideo),
      INIT_CANONICAL(mCname, std::string()),
      INIT_CANONICAL(mTransmitting, false),
      INIT_CANONICAL(mFrameTransformerProxy, nullptr) {
  mPipeline = MediaPipelineTransmit::Create(
      mPc->GetHandle(), aTransportHandler, aCallThread, aStsThread,
      aConduit->type() == MediaSessionConduit::VIDEO, aConduit);
  mPipeline->InitControl(this);

  if (aConduit->type() == MediaSessionConduit::AUDIO) {
    mDtmf = new RTCDTMFSender(aWindow, mTransceiver);
  }
  mPipeline->SetTrack(mSenderTrack);

  mozilla::glean::rtcrtpsender::count.Add(1);

  if (mPc->ShouldAllowOldSetParameters()) {
    mAllowOldSetParameters = true;
    mozilla::glean::rtcrtpsender::count_setparameters_compat.Add(1);
  }

  if (aEncodings.Length()) {
    // This sender was created by addTransceiver with sendEncodings.
    mParameters.mEncodings = aEncodings;
    mSimulcastEnvelopeSet = true;
    mozilla::glean::rtcrtpsender::used_sendencodings.AddToNumerator(1);
  } else {
    // This sender was created by addTrack, sRD(offer), or addTransceiver
    // without sendEncodings.
    RTCRtpEncodingParameters defaultEncoding;
    defaultEncoding.mActive = true;
    if (aConduit->type() == MediaSessionConduit::VIDEO) {
      defaultEncoding.mScaleResolutionDownBy.Construct(1.0f);
    }
    Unused << mParameters.mEncodings.AppendElement(defaultEncoding, fallible);
    UpdateRestorableEncodings(mParameters.mEncodings);
    MaybeGetJsepRids();
  }

  if (mDtmf) {
    mWatchManager.Watch(mTransmitting, &RTCRtpSender::UpdateDtmfSender);
  }
}

#undef INIT_CANONICAL

RTCRtpSender::~RTCRtpSender() = default;

JSObject* RTCRtpSender::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return RTCRtpSender_Binding::Wrap(aCx, this, aGivenProto);
}

RTCDtlsTransport* RTCRtpSender::GetTransport() const {
  if (!mTransceiver) {
    return nullptr;
  }
  return mTransceiver->GetDtlsTransport();
}

RTCDTMFSender* RTCRtpSender::GetDtmf() const { return mDtmf; }

already_AddRefed<Promise> RTCRtpSender::GetStats(ErrorResult& aError) {
  RefPtr<Promise> promise = MakePromise(aError);
  if (aError.Failed()) {
    return nullptr;
  }
  if (NS_WARN_IF(!mPipeline)) {
    // TODO(bug 1056433): When we stop nulling this out when the PC is closed
    // (or when the transceiver is stopped), we can remove this code. We
    // resolve instead of reject in order to make this eventual change in
    // behavior a little smaller.
    promise->MaybeResolve(new RTCStatsReport(mWindow));
    return promise.forget();
  }

  if (!mSenderTrack) {
    promise->MaybeResolve(new RTCStatsReport(mWindow));
    return promise.forget();
  }

  mTransceiver->ChainToDomPromiseWithCodecStats(GetStatsInternal(), promise);
  return promise.forget();
}

nsTArray<RefPtr<dom::RTCStatsPromise>> RTCRtpSender::GetStatsInternal(
    bool aSkipIceStats) {
  MOZ_ASSERT(NS_IsMainThread());
  nsTArray<RefPtr<RTCStatsPromise>> promises(2);
  if (!mSenderTrack || !mPipeline) {
    return promises;
  }

  nsAutoString trackName;
  if (auto track = mPipeline->GetTrack()) {
    track->GetId(trackName);
  }

  {
    // Add bandwidth estimation stats
    promises.AppendElement(InvokeAsync(
        mPipeline->mCallThread, __func__,
        [conduit = mPipeline->mConduit, trackName]() mutable {
          auto report = MakeUnique<dom::RTCStatsCollection>();
          Maybe<webrtc::Call::Stats> stats = conduit->GetCallStats();
          stats.apply([&](const auto aStats) {
            dom::RTCBandwidthEstimationInternal bw;
            bw.mTrackIdentifier = trackName;
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

  promises.AppendElement(InvokeAsync(
      mPipeline->mCallThread, __func__, [pipeline = mPipeline, trackName] {
        auto report = MakeUnique<dom::RTCStatsCollection>();
        auto asAudio = pipeline->mConduit->AsAudioSessionConduit();
        auto asVideo = pipeline->mConduit->AsVideoSessionConduit();

        nsString kind = asVideo.isNothing() ? u"audio"_ns : u"video"_ns;
        nsString idstr = kind + u"_"_ns;
        idstr.AppendInt(static_cast<uint32_t>(pipeline->Level()));

        for (uint32_t ssrc : pipeline->mConduit->GetLocalSSRCs()) {
          nsString localId = u"outbound_rtp_"_ns + idstr + u"_"_ns;
          localId.AppendInt(ssrc);
          nsString remoteId;
          Maybe<uint16_t> base_seq =
              pipeline->mConduit->RtpSendBaseSeqFor(ssrc);

          auto constructCommonRemoteInboundRtpStats =
              [&](RTCRemoteInboundRtpStreamStats& aRemote,
                  const webrtc::ReportBlockData& aRtcpData) {
                remoteId = u"outbound_rtcp_"_ns + idstr + u"_"_ns;
                remoteId.AppendInt(ssrc);
                aRemote.mTimestamp.Construct(
                    RTCStatsTimestamp::FromNtp(
                        pipeline->GetTimestampMaker(),
                        webrtc::Timestamp::Micros(
                            aRtcpData.report_block_timestamp_utc().us()) +
                            webrtc::TimeDelta::Seconds(webrtc::kNtpJan1970))
                        .ToDom());
                aRemote.mId.Construct(remoteId);
                aRemote.mType.Construct(RTCStatsType::Remote_inbound_rtp);
                aRemote.mSsrc = ssrc;
                aRemote.mKind = kind;
                aRemote.mMediaType.Construct(
                    kind);  // mediaType is the old name for kind.
                aRemote.mLocalId.Construct(localId);
                if (base_seq) {
                  if (aRtcpData.extended_highest_sequence_number() <
                      *base_seq) {
                    aRemote.mPacketsReceived.Construct(0);
                  } else {
                    aRemote.mPacketsReceived.Construct(
                        aRtcpData.extended_highest_sequence_number() -
                        aRtcpData.cumulative_lost() - *base_seq + 1);
                  }
                }
              };

          auto constructCommonOutboundRtpStats =
              [&](RTCOutboundRtpStreamStats& aLocal) {
                aLocal.mSsrc = ssrc;
                aLocal.mTimestamp.Construct(
                    pipeline->GetTimestampMaker().GetNow().ToDom());
                aLocal.mId.Construct(localId);
                aLocal.mType.Construct(RTCStatsType::Outbound_rtp);
                aLocal.mKind = kind;
                aLocal.mMediaType.Construct(
                    kind);  // mediaType is the old name for kind.
                if (remoteId.Length()) {
                  aLocal.mRemoteId.Construct(remoteId);
                }
              };

          auto constructCommonMediaSourceStats =
              [&](RTCMediaSourceStats& aStats) {
                nsString id = u"mediasource_"_ns + idstr + trackName;
                aStats.mTimestamp.Construct(
                    pipeline->GetTimestampMaker().GetNow().ToDom());
                aStats.mId.Construct(id);
                aStats.mType.Construct(RTCStatsType::Media_source);
                aStats.mTrackIdentifier = trackName;
                aStats.mKind = kind;
              };

          asAudio.apply([&](auto& aConduit) {
            Maybe<webrtc::AudioSendStream::Stats> audioStats =
                aConduit->GetSenderStats();
            if (audioStats.isNothing()) {
              return;
            }

            if (audioStats->packets_sent == 0) {
              // By spec: "The lifetime of all RTP monitored objects starts
              // when the RTP stream is first used: When the first RTP packet
              // is sent or received on the SSRC it represents"
              return;
            }

            // First, fill in remote stat with rtcp receiver data, if present.
            // ReceiverReports have less information than SenderReports, so fill
            // in what we can.
            Maybe<webrtc::ReportBlockData> reportBlockData;
            {
              if (const auto remoteSsrc = aConduit->GetRemoteSSRC();
                  remoteSsrc) {
                for (auto& data : audioStats->report_block_datas) {
                  if (data.source_ssrc() == ssrc &&
                      data.sender_ssrc() == *remoteSsrc) {
                    reportBlockData.emplace(data);
                    break;
                  }
                }
              }
            }
            reportBlockData.apply([&](auto& aReportBlockData) {
              RTCRemoteInboundRtpStreamStats remote;
              constructCommonRemoteInboundRtpStats(remote, aReportBlockData);
              if (audioStats->jitter_ms >= 0) {
                remote.mJitter.Construct(audioStats->jitter_ms / 1000.0);
              }
              if (audioStats->packets_lost >= 0) {
                remote.mPacketsLost.Construct(audioStats->packets_lost);
              }
              if (audioStats->rtt_ms >= 0) {
                remote.mRoundTripTime.Construct(
                    static_cast<double>(audioStats->rtt_ms) / 1000.0);
              }
              remote.mFractionLost.Construct(audioStats->fraction_lost);
              remote.mTotalRoundTripTime.Construct(
                  double(aReportBlockData.sum_rtts().ms()) / 1000);
              remote.mRoundTripTimeMeasurements.Construct(
                  aReportBlockData.num_rtts());
              if (!report->mRemoteInboundRtpStreamStats.AppendElement(
                      std::move(remote), fallible)) {
                mozalloc_handle_oom(0);
              }
            });

            // Then, fill in local side (with cross-link to remote only if
            // present)
            RTCOutboundRtpStreamStats local;
            constructCommonOutboundRtpStats(local);
            local.mPacketsSent.Construct(audioStats->packets_sent);
            local.mBytesSent.Construct(audioStats->payload_bytes_sent);
            local.mNackCount.Construct(
                audioStats->rtcp_packet_type_counts.nack_packets);
            local.mHeaderBytesSent.Construct(
                audioStats->header_and_padding_bytes_sent);
            local.mRetransmittedPacketsSent.Construct(
                audioStats->retransmitted_packets_sent);
            local.mRetransmittedBytesSent.Construct(
                audioStats->retransmitted_bytes_sent);
            /*
             * Potential new stats that are now available upstream.
             * Note: when we last tried exposing this we were getting
             * targetBitrate for audio was ending up as 0. We did not
             * investigate why.
            local.mTargetBitrate.Construct(audioStats->target_bitrate_bps);
             */
            if (!report->mOutboundRtpStreamStats.AppendElement(std::move(local),
                                                               fallible)) {
              mozalloc_handle_oom(0);
            }

            // TODO(bug 1804678): Use RTCAudioSourceStats
            RTCMediaSourceStats mediaSourceStats;
            constructCommonMediaSourceStats(mediaSourceStats);
            if (!report->mMediaSourceStats.AppendElement(
                    std::move(mediaSourceStats), fallible)) {
              mozalloc_handle_oom(0);
            }
          });

          asVideo.apply([&](auto& aConduit) {
            Maybe<webrtc::VideoSendStream::Stats> videoStats =
                aConduit->GetSenderStats();
            if (videoStats.isNothing()) {
              return;
            }

            Maybe<webrtc::VideoSendStream::StreamStats> streamStats;
            auto kv = videoStats->substreams.find(ssrc);
            if (kv != videoStats->substreams.end()) {
              streamStats = Some(kv->second);
            }

            if (!streamStats) {
              // By spec: "The lifetime of all RTP monitored objects starts
              // when the RTP stream is first used: When the first RTP packet
              // is sent or received on the SSRC it represents"
              return;
            }

            aConduit->GetAssociatedLocalRtxSSRC(ssrc).apply(
                [&](const auto rtxSsrc) {
                  auto kv = videoStats->substreams.find(rtxSsrc);
                  if (kv != videoStats->substreams.end()) {
                    streamStats->rtp_stats.Add(kv->second.rtp_stats);
                  }
                });

            if (streamStats->rtp_stats.first_packet_time ==
                webrtc::Timestamp::PlusInfinity()) {
              return;
            }

            // First, fill in remote stat with rtcp receiver data, if present.
            // ReceiverReports have less information than SenderReports, so fill
            // in what we can.
            if (streamStats->report_block_data) {
              const webrtc::ReportBlockData& rtcpReportData =
                  *streamStats->report_block_data;
              RTCRemoteInboundRtpStreamStats remote;
              remote.mJitter.Construct(
                  static_cast<double>(rtcpReportData.jitter()) /
                  webrtc::kVideoPayloadTypeFrequency);
              remote.mPacketsLost.Construct(rtcpReportData.cumulative_lost());
              if (rtcpReportData.has_rtt()) {
                remote.mRoundTripTime.Construct(
                    static_cast<double>(rtcpReportData.last_rtt().ms()) /
                    1000.0);
              }
              constructCommonRemoteInboundRtpStats(remote, rtcpReportData);
              remote.mTotalRoundTripTime.Construct(
                  streamStats->report_block_data->sum_rtts().ms() / 1000.0);
              remote.mFractionLost.Construct(
                  static_cast<float>(rtcpReportData.fraction_lost_raw()) /
                  (1 << 8));
              remote.mRoundTripTimeMeasurements.Construct(
                  streamStats->report_block_data->num_rtts());
              if (!report->mRemoteInboundRtpStreamStats.AppendElement(
                      std::move(remote), fallible)) {
                mozalloc_handle_oom(0);
              }
            }

            // Then, fill in local side (with cross-link to remote only if
            // present)
            RTCOutboundRtpStreamStats local;
            constructCommonOutboundRtpStats(local);
            local.mPacketsSent.Construct(
                streamStats->rtp_stats.transmitted.packets);
            local.mBytesSent.Construct(
                streamStats->rtp_stats.transmitted.payload_bytes);
            local.mNackCount.Construct(
                streamStats->rtcp_packet_type_counts.nack_packets);
            local.mFirCount.Construct(
                streamStats->rtcp_packet_type_counts.fir_packets);
            local.mPliCount.Construct(
                streamStats->rtcp_packet_type_counts.pli_packets);
            local.mFramesEncoded.Construct(streamStats->frames_encoded);
            if (streamStats->qp_sum) {
              local.mQpSum.Construct(*streamStats->qp_sum);
            }
            local.mHeaderBytesSent.Construct(
                streamStats->rtp_stats.transmitted.header_bytes +
                streamStats->rtp_stats.transmitted.padding_bytes);
            local.mRetransmittedPacketsSent.Construct(
                streamStats->rtp_stats.retransmitted.packets);
            local.mRetransmittedBytesSent.Construct(
                streamStats->rtp_stats.retransmitted.payload_bytes);
            local.mTotalEncodedBytesTarget.Construct(
                videoStats->total_encoded_bytes_target);
            local.mFrameWidth.Construct(streamStats->width);
            local.mFrameHeight.Construct(streamStats->height);
            local.mFramesPerSecond.Construct(streamStats->encode_frame_rate);
            local.mFramesSent.Construct(streamStats->frames_encoded);
            local.mHugeFramesSent.Construct(streamStats->huge_frames_sent);
            local.mTotalEncodeTime.Construct(
                double(streamStats->total_encode_time_ms) / 1000.);
            /*
             * Potential new stats that are now available upstream.
            local.mTargetBitrate.Construct(videoStats->target_media_bitrate_bps);
             */
            if (!report->mOutboundRtpStreamStats.AppendElement(std::move(local),
                                                               fallible)) {
              mozalloc_handle_oom(0);
            }

            RTCVideoSourceStats videoSourceStats;
            constructCommonMediaSourceStats(videoSourceStats);
            // webrtc::VideoSendStream::Stats does not have width/height. We
            // might be able to get this somewhere else?
            // videoStats->frames is not documented, but looking at the
            // implementation it appears to be the number of frames inputted to
            // the encoder, which ought to work.
            videoSourceStats.mFrames.Construct(videoStats->frames);
            videoSourceStats.mFramesPerSecond.Construct(
                videoStats->input_frame_rate);
            auto resolution = aConduit->GetLastResolution();
            resolution.apply(
                [&](const VideoSessionConduit::Resolution& aResolution) {
                  videoSourceStats.mWidth.Construct(aResolution.width);
                  videoSourceStats.mHeight.Construct(aResolution.height);
                });
            if (!report->mVideoSourceStats.AppendElement(
                    std::move(videoSourceStats), fallible)) {
              mozalloc_handle_oom(0);
            }
          });
        }

        return RTCStatsPromise::CreateAndResolve(std::move(report), __func__);
      }));

  if (!aSkipIceStats && GetJsepTransceiver().mTransport.mComponents) {
    promises.AppendElement(mTransportHandler->GetIceStats(
        GetJsepTransceiver().mTransport.mTransportId,
        mPipeline->GetTimestampMaker().GetNow().ToDom()));
  }

  return promises;
}

void RTCRtpSender::GetCapabilities(const GlobalObject&, const nsAString& aKind,
                                   Nullable<dom::RTCRtpCapabilities>& aResult) {
  PeerConnectionImpl::GetCapabilities(aKind, aResult, sdp::Direction::kSend);
}

void RTCRtpSender::WarnAboutBadSetParameters(const nsCString& aError) {
  nsCString warning(
      "WARNING! Invalid setParameters call detected! The good news? Firefox "
      "supports sendEncodings in addTransceiver now, so we ask that you switch "
      "over to using the parameters code you use for other browsers. Thank you "
      "for your patience and support. The specific error was: ");
  warning += aError;
  mPc->SendWarningToConsole(warning);
}

nsCString RTCRtpSender::GetEffectiveTLDPlus1() const {
  return mPc->GetEffectiveTLDPlus1();
}

already_AddRefed<Promise> RTCRtpSender::SetParameters(
    const dom::RTCRtpSendParameters& aParameters, ErrorResult& aError) {
  dom::RTCRtpSendParameters paramsCopy(aParameters);
  // When the setParameters method is called, the user agent MUST run the
  // following steps:
  // Let parameters be the method's first argument.
  // Let sender be the RTCRtpSender object on which setParameters is invoked.
  // Let transceiver be the RTCRtpTransceiver object associated with sender
  // (i.e.sender is transceiver.[[Sender]]).

  RefPtr<dom::Promise> p = MakePromise(aError);
  if (aError.Failed()) {
    return nullptr;
  }

  if (mPc->IsClosed()) {
    p->MaybeRejectWithInvalidStateError("Peer connection is closed");
    return p.forget();
  }

  // If transceiver.[[Stopping]] is true, return a promise rejected with a newly
  // created InvalidStateError.
  if (mTransceiver->Stopping()) {
    p->MaybeRejectWithInvalidStateError(
        "This sender's transceiver is stopping/stopped");
    return p.forget();
  }

  // If sender.[[LastReturnedParameters]] is null, return a promise rejected
  // with a newly created InvalidStateError.
  if (!mLastReturnedParameters.isSome()) {
    nsCString error(
        "Cannot call setParameters without first calling getParameters");
    if (mAllowOldSetParameters) {
      if (!mHaveWarnedBecauseNoGetParameters) {
        mHaveWarnedBecauseNoGetParameters = true;
        mozilla::glean::rtcrtpsender_setparameters::warn_no_getparameters
            .AddToNumerator(1);
#ifdef EARLY_BETA_OR_EARLIER
        mozilla::glean::rtcrtpsender_setparameters::blame_no_getparameters
            .Get(GetEffectiveTLDPlus1())
            .Add(1);
#endif
      }
      WarnAboutBadSetParameters(error);
    } else {
      if (!mHaveFailedBecauseNoGetParameters) {
        mHaveFailedBecauseNoGetParameters = true;
        mozilla::glean::rtcrtpsender_setparameters::fail_no_getparameters
            .AddToNumerator(1);
      }
      p->MaybeRejectWithInvalidStateError(error);
      return p.forget();
    }
  }

  // According to the spec, our consistency checking is based on
  // [[LastReturnedParameters]], but if we're letting
  // [[LastReturnedParameters]]==null slide, we still want to do
  // consistency checking on _something_ so we can warn implementers if they
  // are messing that up also. Just find something, _anything_, to do that
  // checking with.
  // TODO(bug 1803388): Remove this stuff once it is no longer needed.
  // TODO(bug 1803389): Remove the glean errors once they are no longer needed.
  Maybe<RTCRtpSendParameters> oldParams;
  if (mAllowOldSetParameters) {
    if (mLastReturnedParameters.isSome()) {
      oldParams = mLastReturnedParameters;
    } else if (mPendingParameters.isSome()) {
      oldParams = mPendingParameters;
    } else {
      oldParams = Some(mParameters);
    }
    MOZ_ASSERT(oldParams.isSome());
  } else {
    oldParams = mLastReturnedParameters;
  }
  MOZ_ASSERT(oldParams.isSome());

  // Validate parameters by running the following steps:
  // Let encodings be parameters.encodings.
  // Let codecs be parameters.codecs.
  // Let N be the number of RTCRtpEncodingParameters stored in
  // sender.[[SendEncodings]].
  // If any of the following conditions are met,
  // return a promise rejected with a newly created InvalidModificationError:

  bool pendingRidChangeFromCompatMode = false;
  // encodings.length is different from N.
  if (paramsCopy.mEncodings.Length() != oldParams->mEncodings.Length()) {
    nsCString error("Cannot change the number of encodings with setParameters");
    if (!mAllowOldSetParameters) {
      if (!mHaveFailedBecauseEncodingCountChange) {
        mHaveFailedBecauseEncodingCountChange = true;
        mozilla::glean::rtcrtpsender_setparameters::fail_length_changed
            .AddToNumerator(1);
      }
      p->MaybeRejectWithInvalidModificationError(error);
      return p.forget();
    }
    // Make sure we don't use the old rids in SyncToJsep while we wait for the
    // queued task below to update mParameters.
    pendingRidChangeFromCompatMode = true;
    mSimulcastEnvelopeSet = true;
    if (!mHaveWarnedBecauseEncodingCountChange) {
      mHaveWarnedBecauseEncodingCountChange = true;
      mozilla::glean::rtcrtpsender_setparameters::warn_length_changed
          .AddToNumerator(1);
#ifdef EARLY_BETA_OR_EARLIER
      mozilla::glean::rtcrtpsender_setparameters::blame_length_changed
          .Get(GetEffectiveTLDPlus1())
          .Add(1);
#endif
    }
    WarnAboutBadSetParameters(error);
  } else {
    // encodings has been re-ordered.
    for (size_t i = 0; i < paramsCopy.mEncodings.Length(); ++i) {
      const auto& oldEncoding = oldParams->mEncodings[i];
      const auto& newEncoding = paramsCopy.mEncodings[i];
      if (oldEncoding.mRid != newEncoding.mRid) {
        nsCString error("Cannot change rid, or reorder encodings");
        if (!mHaveFailedBecauseRidChange) {
          mHaveFailedBecauseRidChange = true;
          mozilla::glean::rtcrtpsender_setparameters::fail_rid_changed
              .AddToNumerator(1);
        }
        p->MaybeRejectWithInvalidModificationError(error);
        return p.forget();
      }
    }
  }

  // TODO(bug 1803388): Handle this in webidl, once we stop allowing the old
  // setParameters style.
  if (!paramsCopy.mTransactionId.WasPassed()) {
    nsCString error("transactionId is not set!");
    if (!mAllowOldSetParameters) {
      if (!mHaveFailedBecauseNoTransactionId) {
        mHaveFailedBecauseNoTransactionId = true;
        mozilla::glean::rtcrtpsender_setparameters::fail_no_transactionid
            .AddToNumerator(1);
      }
      p->MaybeRejectWithTypeError(error);
      return p.forget();
    }
    if (!mHaveWarnedBecauseNoTransactionId) {
      mHaveWarnedBecauseNoTransactionId = true;
      mozilla::glean::rtcrtpsender_setparameters::warn_no_transactionid
          .AddToNumerator(1);
#ifdef EARLY_BETA_OR_EARLIER
      mozilla::glean::rtcrtpsender_setparameters::blame_no_transactionid
          .Get(GetEffectiveTLDPlus1())
          .Add(1);
#endif
    }
    WarnAboutBadSetParameters(error);
  } else if (oldParams->mTransactionId != paramsCopy.mTransactionId) {
    // Any parameter in parameters is marked as a Read-only parameter (such as
    // RID) and has a value that is different from the corresponding parameter
    // value in sender.[[LastReturnedParameters]]. Note that this also applies
    // to transactionId.
    nsCString error(
        "Cannot change transaction id: call getParameters, modify the result, "
        "and then call setParameters");
    if (!mHaveFailedBecauseStaleTransactionId) {
      mHaveFailedBecauseStaleTransactionId = true;
      mozilla::glean::rtcrtpsender_setparameters::fail_stale_transactionid
          .AddToNumerator(1);
    }
    p->MaybeRejectWithInvalidModificationError(error);
    return p.forget();
  }

  // This could conceivably happen if we are allowing the old setParameters
  // behavior.
  if (!paramsCopy.mEncodings.Length()) {
    nsCString error("Cannot set an empty encodings array");
    if (!mAllowOldSetParameters) {
      if (!mHaveFailedBecauseNoEncodings) {
        mHaveFailedBecauseNoEncodings = true;
        mozilla::glean::rtcrtpsender_setparameters::fail_no_encodings
            .AddToNumerator(1);
      }

      p->MaybeRejectWithInvalidModificationError(error);
      return p.forget();
    }
    // TODO: Add some warning telemetry here
    WarnAboutBadSetParameters(error);
    // Just don't do this; it's stupid.
    paramsCopy.mEncodings = oldParams->mEncodings;
  }

  // TODO: Verify remaining read-only parameters
  // headerExtensions (bug 1765851)
  // rtcp (bug 1765852)
  // codecs (bug 1534687)

  // CheckAndRectifyEncodings handles the following steps:
  // If transceiver kind is "audio", remove the scaleResolutionDownBy member
  // from all encodings that contain one.
  //
  // If transceiver kind is "video", and any encoding in encodings contains a
  // scaleResolutionDownBy member whose value is less than 1.0, return a
  // promise rejected with a newly created RangeError.
  //
  // Verify that each encoding in encodings has a maxFramerate member whose
  // value is greater than or equal to 0.0. If one of the maxFramerate values
  // does not meet this requirement, return a promise rejected with a newly
  // created RangeError.
  ErrorResult rv;
  CheckAndRectifyEncodings(paramsCopy.mEncodings, mTransceiver->IsVideo(), rv);
  if (rv.Failed()) {
    if (!mHaveFailedBecauseOtherError) {
      mHaveFailedBecauseOtherError = true;
      mozilla::glean::rtcrtpsender_setparameters::fail_other.AddToNumerator(1);
    }
    p->MaybeReject(std::move(rv));
    return p.forget();
  }

  // If transceiver kind is "video", then for each encoding in encodings that
  // doesn't contain a scaleResolutionDownBy member, add a
  // scaleResolutionDownBy member with the value 1.0.
  if (mTransceiver->IsVideo()) {
    for (auto& encoding : paramsCopy.mEncodings) {
      if (!encoding.mScaleResolutionDownBy.WasPassed()) {
        encoding.mScaleResolutionDownBy.Construct(1.0);
      }
    }
  }

  // Let p be a new promise. (see above)

  // In parallel, configure the media stack to use parameters to transmit
  // sender.[[SenderTrack]].
  // Right now this is infallible. That may change someday.

  // We need to put this in a member variable, since MaybeUpdateConduit needs it
  // This also allows PeerConnectionImpl to detect when there is a pending
  // setParameters, which has implcations for the handling of
  // setRemoteDescription.
  mPendingRidChangeFromCompatMode = pendingRidChangeFromCompatMode;
  mPendingParameters = Some(paramsCopy);
  uint32_t serialNumber = ++mNumSetParametersCalls;
  MaybeUpdateConduit();

  // If the media stack is successfully configured with parameters,
  // queue a task to run the following steps:
  GetMainThreadSerialEventTarget()->Dispatch(NS_NewRunnableFunction(
      __func__,
      [this, self = RefPtr<RTCRtpSender>(this), p, paramsCopy, serialNumber] {
        // Set sender.[[LastReturnedParameters]] to null.
        mLastReturnedParameters = Nothing();
        // Set sender.[[SendEncodings]] to parameters.encodings.
        mParameters = paramsCopy;
        UpdateRestorableEncodings(mParameters.mEncodings);
        // Only clear mPendingParameters if it matches; there could have been
        // back-to-back calls to setParameters, and we only want to clear this
        // if no subsequent setParameters is pending.
        if (serialNumber == mNumSetParametersCalls) {
          mPendingParameters = Nothing();
          // Ok, nothing has called SyncToJsep while this async task was
          // pending. No need for special handling anymore.
          mPendingRidChangeFromCompatMode = false;
        }
        MOZ_ASSERT(mParameters.mEncodings.Length());
        // Resolve p with undefined.
        p->MaybeResolveWithUndefined();
      }));

  // Return p.
  return p.forget();
}

// static
void RTCRtpSender::CheckAndRectifyEncodings(
    Sequence<RTCRtpEncodingParameters>& aEncodings, bool aVideo,
    ErrorResult& aRv) {
  // If any encoding contains a rid member whose value does not conform to the
  // grammar requirements specified in Section 10 of [RFC8851], throw a
  // TypeError.
  for (const auto& encoding : aEncodings) {
    if (encoding.mRid.WasPassed()) {
      std::string utf8Rid = NS_ConvertUTF16toUTF8(encoding.mRid.Value()).get();
      std::string error;
      if (!SdpRidAttributeList::CheckRidValidity(utf8Rid, &error)) {
        aRv.ThrowTypeError(nsCString(error));
        return;
      }
      if (utf8Rid.size() > SdpRidAttributeList::kMaxRidLength) {
        std::ostringstream ss;
        ss << "Rid can be at most " << SdpRidAttributeList::kMaxRidLength
           << " characters long (due to internal limitations)";
        aRv.ThrowTypeError(nsCString(ss.str()));
        return;
      }
    }
  }

  if (aEncodings.Length() > 1) {
    // If some but not all encodings contain a rid member, throw a TypeError.
    // rid must be set if there is more than one encoding
    // NOTE: Since rid is read-only, and the number of encodings cannot grow,
    // this should never happen in setParameters.
    for (const auto& encoding : aEncodings) {
      if (!encoding.mRid.WasPassed()) {
        aRv.ThrowTypeError("Missing rid");
        return;
      }
    }

    // If any encoding contains a rid member whose value is the same as that of
    // a rid contained in another encoding in sendEncodings, throw a TypeError.
    // NOTE: Since rid is read-only, and the number of encodings cannot grow,
    // this should never happen in setParameters.
    std::set<nsString> uniqueRids;
    for (const auto& encoding : aEncodings) {
      if (uniqueRids.count(encoding.mRid.Value())) {
        aRv.ThrowTypeError("Duplicate rid");
        return;
      }
      uniqueRids.insert(encoding.mRid.Value());
    }
  }
  // TODO: ptime/adaptivePtime validation (bug 1733647)

  // If kind is "audio", remove the scaleResolutionDownBy member from all
  // encodings that contain one.
  if (!aVideo) {
    for (auto& encoding : aEncodings) {
      if (encoding.mScaleResolutionDownBy.WasPassed()) {
        encoding.mScaleResolutionDownBy.Reset();
      }
      if (encoding.mMaxFramerate.WasPassed()) {
        encoding.mMaxFramerate.Reset();
      }
    }
  }

  // If any encoding contains a scaleResolutionDownBy member whose value is
  // less than 1.0, throw a RangeError.
  for (const auto& encoding : aEncodings) {
    if (encoding.mScaleResolutionDownBy.WasPassed()) {
      if (encoding.mScaleResolutionDownBy.Value() < 1.0f) {
        aRv.ThrowRangeError("scaleResolutionDownBy must be >= 1.0");
        return;
      }
    }
  }

  // Verify that the value of each maxFramerate member in sendEncodings that is
  // defined is greater than 0.0. If one of the maxFramerate values does not
  // meet this requirement, throw a RangeError.
  for (const auto& encoding : aEncodings) {
    if (encoding.mMaxFramerate.WasPassed()) {
      if (encoding.mMaxFramerate.Value() < 0.0f) {
        aRv.ThrowRangeError("maxFramerate must be non-negative");
        return;
      }
    }
  }
}

void RTCRtpSender::GetParameters(RTCRtpSendParameters& aParameters) {
  MOZ_ASSERT(mParameters.mEncodings.Length());
  // If sender.[[LastReturnedParameters]] is not null, return
  // sender.[[LastReturnedParameters]], and abort these steps.
  if (mLastReturnedParameters.isSome()) {
    aParameters = *mLastReturnedParameters;
    return;
  }

  // Let result be a new RTCRtpSendParameters dictionary constructed as follows:

  // transactionId is set to a new unique identifier
  aParameters.mTransactionId.Construct(mPc->GenerateUUID());

  // encodings is set to the value of the [[SendEncodings]] internal slot.
  aParameters.mEncodings = mParameters.mEncodings;

  // The headerExtensions sequence is populated based on the header extensions
  // that have been negotiated for sending
  // TODO(bug 1765851): We do not support this yet
  // aParameters.mHeaderExtensions.Construct();

  // codecs is set to the value of the [[SendCodecs]] internal slot
  // TODO(bug 1534687): We do not support this yet

  // rtcp.cname is set to the CNAME of the associated RTCPeerConnection.
  // rtcp.reducedSize is set to true if reduced-size RTCP has been negotiated
  // for sending, and false otherwise.
  // TODO(bug 1765852): We do not support this yet
  aParameters.mRtcp.Construct();
  aParameters.mRtcp.Value().mCname.Construct();
  aParameters.mRtcp.Value().mReducedSize.Construct(false);
  aParameters.mHeaderExtensions.Construct();
  aParameters.mCodecs.Construct();

  // Set sender.[[LastReturnedParameters]] to result.
  mLastReturnedParameters = Some(aParameters);

  // Queue a task that sets sender.[[LastReturnedParameters]] to null.
  GetMainThreadSerialEventTarget()->Dispatch(NS_NewRunnableFunction(
      __func__, [this, self = RefPtr<RTCRtpSender>(this)] {
        mLastReturnedParameters = Nothing();
      }));
}

bool operator==(const RTCRtpEncodingParameters& a1,
                const RTCRtpEncodingParameters& a2) {
  // webidl does not generate types that are equality comparable
  return a1.mActive == a2.mActive && a1.mFec == a2.mFec &&
         a1.mMaxBitrate == a2.mMaxBitrate &&
         a1.mMaxFramerate == a2.mMaxFramerate && a1.mPriority == a2.mPriority &&
         a1.mRid == a2.mRid && a1.mRtx == a2.mRtx &&
         a1.mScaleResolutionDownBy == a2.mScaleResolutionDownBy &&
         a1.mSsrc == a2.mSsrc;
}

// static
void RTCRtpSender::ApplyJsEncodingToConduitEncoding(
    const RTCRtpEncodingParameters& aJsEncoding,
    VideoCodecConfig::Encoding* aConduitEncoding) {
  aConduitEncoding->active = aJsEncoding.mActive;
  if (aJsEncoding.mMaxBitrate.WasPassed()) {
    aConduitEncoding->constraints.maxBr = aJsEncoding.mMaxBitrate.Value();
  }
  if (aJsEncoding.mMaxFramerate.WasPassed()) {
    aConduitEncoding->constraints.maxFps =
        Some(aJsEncoding.mMaxFramerate.Value());
  }
  if (aJsEncoding.mScaleResolutionDownBy.WasPassed()) {
    // Optional does not have a valueOr, despite being based on Maybe
    // :(
    aConduitEncoding->constraints.scaleDownBy =
        aJsEncoding.mScaleResolutionDownBy.Value();
  } else {
    aConduitEncoding->constraints.scaleDownBy = 1.0f;
  }
}

void RTCRtpSender::UpdateRestorableEncodings(
    const Sequence<RTCRtpEncodingParameters>& aEncodings) {
  MOZ_ASSERT(aEncodings.Length());

  if (GetJsepTransceiver().mSendTrack.GetNegotiatedDetails()) {
    // Once initial negotiation completes, we are no longer allowed to restore
    // the unicast encoding.
    mUnicastEncoding.reset();
  } else if (mParameters.mEncodings.Length() == 1 &&
             !mParameters.mEncodings[0].mRid.WasPassed()) {
    // If we have not completed the initial negotiation, and we currently are
    // ridless unicast, we need to save our unicast encoding in case a
    // rollback occurs.
    mUnicastEncoding = Some(mParameters.mEncodings[0]);
  }
}

Sequence<RTCRtpEncodingParameters> RTCRtpSender::ToSendEncodings(
    const std::vector<std::string>& aRids) const {
  MOZ_ASSERT(!aRids.empty());

  Sequence<RTCRtpEncodingParameters> result;
  // If sendEncodings is given as input to this algorithm, and is non-empty,
  // set the [[SendEncodings]] slot to sendEncodings.
  for (const auto& rid : aRids) {
    MOZ_ASSERT(!rid.empty());
    RTCRtpEncodingParameters encoding;
    encoding.mActive = true;
    encoding.mRid.Construct(NS_ConvertUTF8toUTF16(rid.c_str()));
    Unused << result.AppendElement(encoding, fallible);
  }

  // If sendEncodings is non-empty, set each encoding's scaleResolutionDownBy
  // to 2^(length of sendEncodings - encoding index - 1).
  if (mTransceiver->IsVideo()) {
    double scale = 1.0f;
    for (auto it = result.rbegin(); it != result.rend(); ++it) {
      it->mScaleResolutionDownBy.Construct(scale);
      scale *= 2;
    }
  }

  return result;
}

void RTCRtpSender::MaybeGetJsepRids() {
  MOZ_ASSERT(!mSimulcastEnvelopeSet);
  MOZ_ASSERT(mParameters.mEncodings.Length());

  auto jsepRids = GetJsepTransceiver().mSendTrack.GetRids();
  if (!jsepRids.empty()) {
    UpdateRestorableEncodings(mParameters.mEncodings);
    if (jsepRids.size() != 1 || !jsepRids[0].empty()) {
      // JSEP is using at least one rid. Stomp our single ridless encoding
      mParameters.mEncodings = ToSendEncodings(jsepRids);
    }
    mSimulcastEnvelopeSet = true;
    mSimulcastEnvelopeSetByJSEP = true;
  }
}

Sequence<RTCRtpEncodingParameters> RTCRtpSender::GetMatchingEncodings(
    const std::vector<std::string>& aRids) const {
  Sequence<RTCRtpEncodingParameters> result;

  if (!aRids.empty() && !aRids[0].empty()) {
    // Simulcast, or unicast with rid
    for (const auto& encoding : mParameters.mEncodings) {
      for (const auto& rid : aRids) {
        auto utf16Rid = NS_ConvertUTF8toUTF16(rid.c_str());
        if (!encoding.mRid.WasPassed() || (utf16Rid == encoding.mRid.Value())) {
          auto encodingCopy(encoding);
          if (!encodingCopy.mRid.WasPassed()) {
            encodingCopy.mRid.Construct(NS_ConvertUTF8toUTF16(rid.c_str()));
          }
          Unused << result.AppendElement(encodingCopy, fallible);
          break;
        }
      }
    }
  }

  // If we're allowing the old setParameters behavior, we _might_ be able to
  // get into this situation even if there were rids above. Be extra careful.
  // Under normal circumstances, this just handles the ridless case.
  if (!result.Length()) {
    // Unicast with no specified rid. Restore mUnicastEncoding, if
    // it exists, otherwise pick the first encoding.
    if (mUnicastEncoding.isSome()) {
      Unused << result.AppendElement(*mUnicastEncoding, fallible);
    } else {
      Unused << result.AppendElement(mParameters.mEncodings[0], fallible);
    }
  }

  return result;
}

void RTCRtpSender::SetStreams(
    const Sequence<OwningNonNull<DOMMediaStream>>& aStreams, ErrorResult& aRv) {
  if (mPc->IsClosed()) {
    aRv.ThrowInvalidStateError(
        "Cannot call setStreams if the peer connection is closed");
    return;
  }

  SetStreamsImpl(aStreams);
  mPc->UpdateNegotiationNeeded();
}

void RTCRtpSender::SetStreamsImpl(
    const Sequence<OwningNonNull<DOMMediaStream>>& aStreams) {
  mStreams.Clear();
  std::set<nsString> ids;
  for (const auto& stream : aStreams) {
    nsString id;
    stream->GetId(id);
    if (!ids.count(id)) {
      ids.insert(id);
      mStreams.AppendElement(stream);
    }
  }
}

void RTCRtpSender::GetStreams(nsTArray<RefPtr<DOMMediaStream>>& aStreams) {
  aStreams = mStreams.Clone();
}

class ReplaceTrackOperation final : public PeerConnectionImpl::Operation {
 public:
  ReplaceTrackOperation(PeerConnectionImpl* aPc,
                        const RefPtr<RTCRtpTransceiver>& aTransceiver,
                        const RefPtr<MediaStreamTrack>& aTrack,
                        ErrorResult& aError);
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ReplaceTrackOperation,
                                           PeerConnectionImpl::Operation)

 private:
  MOZ_CAN_RUN_SCRIPT
  RefPtr<dom::Promise> CallImpl(ErrorResult& aError) override;
  ~ReplaceTrackOperation() = default;
  RefPtr<RTCRtpTransceiver> mTransceiver;
  RefPtr<MediaStreamTrack> mNewTrack;
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(ReplaceTrackOperation,
                                   PeerConnectionImpl::Operation, mTransceiver,
                                   mNewTrack)

NS_IMPL_ADDREF_INHERITED(ReplaceTrackOperation, PeerConnectionImpl::Operation)
NS_IMPL_RELEASE_INHERITED(ReplaceTrackOperation, PeerConnectionImpl::Operation)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReplaceTrackOperation)
NS_INTERFACE_MAP_END_INHERITING(PeerConnectionImpl::Operation)

ReplaceTrackOperation::ReplaceTrackOperation(
    PeerConnectionImpl* aPc, const RefPtr<RTCRtpTransceiver>& aTransceiver,
    const RefPtr<MediaStreamTrack>& aTrack, ErrorResult& aError)
    : PeerConnectionImpl::Operation(aPc, aError),
      mTransceiver(aTransceiver),
      mNewTrack(aTrack) {}

RefPtr<dom::Promise> ReplaceTrackOperation::CallImpl(ErrorResult& aError) {
  RefPtr<RTCRtpSender> sender = mTransceiver->Sender();
  // If transceiver.[[Stopping]] is true, return a promise rejected with a
  // newly created InvalidStateError.
  if (mTransceiver->Stopped() || mTransceiver->Stopping()) {
    RefPtr<dom::Promise> error = sender->MakePromise(aError);
    if (aError.Failed()) {
      return nullptr;
    }
    MOZ_LOG(gSenderLog, LogLevel::Debug,
            ("%s Cannot call replaceTrack when transceiver is stopping",
             __FUNCTION__));
    error->MaybeRejectWithInvalidStateError(
        "Cannot call replaceTrack when transceiver is stopping");
    return error;
  }

  // Let p be a new promise.
  RefPtr<dom::Promise> p = sender->MakePromise(aError);
  if (aError.Failed()) {
    return nullptr;
  }

  if (!sender->SeamlessTrackSwitch(mNewTrack)) {
    MOZ_LOG(gSenderLog, LogLevel::Info,
            ("%s Could not seamlessly replace track", __FUNCTION__));
    p->MaybeRejectWithInvalidModificationError(
        "Could not seamlessly replace track");
    return p;
  }

  // Queue a task that runs the following steps:
  GetMainThreadSerialEventTarget()->Dispatch(NS_NewRunnableFunction(
      __func__, [p, sender, track = mNewTrack]() MOZ_CAN_RUN_SCRIPT_BOUNDARY {
        // If connection.[[IsClosed]] is true, abort these steps.
        // Set sender.[[SenderTrack]] to withTrack.
        if (sender->SetSenderTrackWithClosedCheck(track)) {
          // Resolve p with undefined.
          p->MaybeResolveWithUndefined();
        }
      }));

  // Return p.
  return p;
}

already_AddRefed<dom::Promise> RTCRtpSender::ReplaceTrack(
    dom::MediaStreamTrack* aWithTrack, ErrorResult& aError) {
  // If withTrack is non-null and withTrack.kind differs from the transceiver
  // kind of transceiver, return a promise rejected with a newly created
  // TypeError.
  if (aWithTrack) {
    nsString newKind;
    aWithTrack->GetKind(newKind);
    nsString oldKind;
    mTransceiver->GetKind(oldKind);
    if (newKind != oldKind) {
      RefPtr<dom::Promise> error = MakePromise(aError);
      if (aError.Failed()) {
        return nullptr;
      }
      error->MaybeRejectWithTypeError(
          "Cannot replaceTrack with a different kind!");
      return error.forget();
    }
  }

  MOZ_LOG(gSenderLog, LogLevel::Debug,
          ("%s[%s]: %s (%p to %p)", mPc->GetHandle().c_str(), GetMid().c_str(),
           __FUNCTION__, mSenderTrack.get(), aWithTrack));

  // Return the result of chaining the following steps to connection's
  // operations chain:
  RefPtr<PeerConnectionImpl::Operation> op =
      new ReplaceTrackOperation(mPc, mTransceiver, aWithTrack, aError);
  if (aError.Failed()) {
    return nullptr;
  }
  // Static analysis forces us to use a temporary.
  auto pc = mPc;
  return pc->Chain(op, aError);
}

nsPIDOMWindowInner* RTCRtpSender::GetParentObject() const { return mWindow; }

already_AddRefed<dom::Promise> RTCRtpSender::MakePromise(
    ErrorResult& aError) const {
  return mPc->MakePromise(aError);
}

bool RTCRtpSender::SeamlessTrackSwitch(
    const RefPtr<MediaStreamTrack>& aWithTrack) {
  // We do not actually update mSenderTrack here! Spec says that happens in a
  // queued task after this is done (this happens in
  // SetSenderTrackWithClosedCheck).

  mPipeline->SetTrack(aWithTrack);

  MaybeUpdateConduit();

  // There may eventually be cases where a renegotiation is necessary to switch.
  return true;
}

void RTCRtpSender::SetTrack(const RefPtr<MediaStreamTrack>& aTrack) {
  // Used for RTCPeerConnection.removeTrack and RTCPeerConnection.addTrack
  if (mTransceiver->Stopping()) {
    return;
  }
  mSenderTrack = aTrack;
  SeamlessTrackSwitch(aTrack);
  if (aTrack) {
    // RFC says (in the section on remote rollback):
    // However, an RtpTransceiver MUST NOT be removed if a track was attached
    // to the RtpTransceiver via the addTrack method.
    mSenderTrackSetByAddTrack = true;
  }
}

bool RTCRtpSender::SetSenderTrackWithClosedCheck(
    const RefPtr<MediaStreamTrack>& aTrack) {
  if (!mPc->IsClosed()) {
    mSenderTrack = aTrack;
    return true;
  }

  return false;
}

void RTCRtpSender::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  mWatchManager.Shutdown();
  mPipeline->Shutdown();
  mPipeline = nullptr;
  if (mTransform) {
    mTransform->GetProxy().SetSender(nullptr);
  }
}

void RTCRtpSender::BreakCycles() {
  mWindow = nullptr;
  mPc = nullptr;
  mSenderTrack = nullptr;
  mTransceiver = nullptr;
  mStreams.Clear();
  mDtmf = nullptr;
}

void RTCRtpSender::Unlink() {
  if (mTransceiver) {
    mTransceiver->Unlink();
  }
}

void RTCRtpSender::UpdateTransport() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mHaveSetupTransport) {
    mPipeline->SetLevel(GetJsepTransceiver().GetLevel());
    mHaveSetupTransport = true;
  }

  mPipeline->UpdateTransport_m(GetJsepTransceiver().mTransport.mTransportId,
                               nullptr);
}

void RTCRtpSender::MaybeUpdateConduit() {
  // NOTE(pkerr) - the Call API requires the both local_ssrc and remote_ssrc be
  // set to a non-zero value or the CreateVideo...Stream call will fail.
  if (NS_WARN_IF(GetJsepTransceiver().mSendTrack.GetSsrcs().empty())) {
    MOZ_ASSERT(
        false,
        "No local ssrcs! This is a bug in the jsep engine, and should never "
        "happen!");
    return;
  }

  if (!mPipeline) {
    return;
  }

  bool wasTransmitting = mTransmitting;

  if (mPipeline->mConduit->type() == MediaSessionConduit::VIDEO) {
    Maybe<VideoConfig> newConfig = GetNewVideoConfig();
    if (newConfig.isSome()) {
      ApplyVideoConfig(*newConfig);
    }
  } else {
    Maybe<AudioConfig> newConfig = GetNewAudioConfig();
    if (newConfig.isSome()) {
      ApplyAudioConfig(*newConfig);
    }
  }

  if (!mSenderTrack && !wasTransmitting && mTransmitting) {
    MOZ_LOG(gSenderLog, LogLevel::Debug,
            ("%s[%s]: %s Starting transmit conduit without send track!",
             mPc->GetHandle().c_str(), GetMid().c_str(), __FUNCTION__));
  }
}

void RTCRtpSender::SyncFromJsep(const JsepTransceiver& aJsepTransceiver) {
  if (!mSimulcastEnvelopeSet) {
    // JSEP is establishing the simulcast envelope for the first time, right now
    // This is the addTrack (or addTransceiver without sendEncodings) case.
    MaybeGetJsepRids();
  } else if (!aJsepTransceiver.mSendTrack.GetNegotiatedDetails() ||
             !aJsepTransceiver.mSendTrack.IsInHaveRemote()) {
    // Spec says that we do not update our encodings until we're in stable,
    // _unless_ this is the first negotiation.
    std::vector<std::string> rids = aJsepTransceiver.mSendTrack.GetRids();
    if (mSimulcastEnvelopeSetByJSEP && rids.empty()) {
      // JSEP previously set the simulcast envelope, but now it has no opinion
      // regarding unicast/simulcast. This can only happen on rollback of the
      // initial remote offer.
      mParameters.mEncodings = GetMatchingEncodings(rids);
      MOZ_ASSERT(mParameters.mEncodings.Length());
      mSimulcastEnvelopeSetByJSEP = false;
      mSimulcastEnvelopeSet = false;
    } else if (!rids.empty()) {
      // JSEP has an opinion on the simulcast envelope, which trumps anything
      // we have already.
      mParameters.mEncodings = GetMatchingEncodings(rids);
      MOZ_ASSERT(mParameters.mEncodings.Length());
    }
  }

  MaybeUpdateConduit();
}

void RTCRtpSender::SyncToJsep(JsepTransceiver& aJsepTransceiver) const {
  std::vector<std::string> streamIds;
  for (const auto& stream : mStreams) {
    nsString wideStreamId;
    stream->GetId(wideStreamId);
    std::string streamId = NS_ConvertUTF16toUTF8(wideStreamId).get();
    MOZ_ASSERT(!streamId.empty());
    streamIds.push_back(streamId);
  }

  aJsepTransceiver.mSendTrack.UpdateStreamIds(streamIds);

  if (mSimulcastEnvelopeSet) {
    std::vector<std::string> rids;
    Maybe<RTCRtpSendParameters> parameters;
    if (mPendingRidChangeFromCompatMode) {
      // *sigh* If we have just let a setParameters change our rids, but we have
      // not yet updated mParameters because the queued task hasn't run yet,
      // we want to set the _new_ rids on the JsepTrack. So, we are forced to
      // grab them from mPendingParameters.
      parameters = mPendingParameters;
    } else {
      parameters = Some(mParameters);
    }
    for (const auto& encoding : parameters->mEncodings) {
      if (encoding.mRid.WasPassed()) {
        rids.push_back(NS_ConvertUTF16toUTF8(encoding.mRid.Value()).get());
      } else {
        rids.push_back("");
      }
    }

    aJsepTransceiver.mSendTrack.SetRids(rids);
  }

  if (mTransceiver->IsVideo()) {
    aJsepTransceiver.mSendTrack.SetMaxEncodings(webrtc::kMaxSimulcastStreams);
  } else {
    aJsepTransceiver.mSendTrack.SetMaxEncodings(1);
  }

  if (mSenderTrackSetByAddTrack) {
    aJsepTransceiver.SetOnlyExistsBecauseOfSetRemote(false);
  }
}

Maybe<RTCRtpSender::VideoConfig> RTCRtpSender::GetNewVideoConfig() {
  // It is possible for SDP to signal that there is a send track, but there not
  // actually be a send track, according to the specification; all that needs to
  // happen is for the transceiver to be configured to send...
  if (!GetJsepTransceiver().mSendTrack.GetNegotiatedDetails()) {
    return Nothing();
  }

  VideoConfig oldConfig;
  oldConfig.mSsrcs = mSsrcs;
  oldConfig.mLocalRtpExtensions = mLocalRtpExtensions;
  oldConfig.mCname = mCname;
  oldConfig.mTransmitting = mTransmitting;
  oldConfig.mVideoRtxSsrcs = mVideoRtxSsrcs;
  oldConfig.mVideoCodec = mVideoCodec;
  oldConfig.mVideoRtpRtcpConfig = mVideoRtpRtcpConfig;
  oldConfig.mVideoCodecMode = mVideoCodecMode;

  VideoConfig newConfig(oldConfig);

  UpdateBaseConfig(&newConfig);

  newConfig.mVideoRtxSsrcs = GetJsepTransceiver().mSendTrack.GetRtxSsrcs();

  const JsepTrackNegotiatedDetails details(
      *GetJsepTransceiver().mSendTrack.GetNegotiatedDetails());

  if (mSenderTrack) {
    RefPtr<mozilla::dom::VideoStreamTrack> videotrack =
        mSenderTrack->AsVideoStreamTrack();

    if (!videotrack) {
      MOZ_CRASH(
          "In ConfigureVideoCodecMode, mSenderTrack is not video! This should "
          "never happen!");
    }

    dom::MediaSourceEnum source = videotrack->GetSource().GetMediaSource();
    switch (source) {
      case dom::MediaSourceEnum::Browser:
      case dom::MediaSourceEnum::Screen:
      case dom::MediaSourceEnum::Window:
      case dom::MediaSourceEnum::Application:
        newConfig.mVideoCodecMode = webrtc::VideoCodecMode::kScreensharing;
        break;

      case dom::MediaSourceEnum::Camera:
      case dom::MediaSourceEnum::Other:
        // Other is used by canvas capture, which we treat as realtime video.
        // This seems debatable, but we've been doing it this way for a long
        // time, so this is likely fine.
        newConfig.mVideoCodecMode = webrtc::VideoCodecMode::kRealtimeVideo;
        break;

      case dom::MediaSourceEnum::Microphone:
      case dom::MediaSourceEnum::AudioCapture:
      case dom::MediaSourceEnum::EndGuard_:
        MOZ_ASSERT(false);
        break;
    }
  }

  std::vector<VideoCodecConfig> configs;
  RTCRtpTransceiver::NegotiatedDetailsToVideoCodecConfigs(details, &configs);

  if (configs.empty()) {
    // TODO: Are we supposed to plumb this error back to JS? This does not
    // seem like a failure to set an answer, it just means that codec
    // negotiation failed. For now, we're just doing the same thing we do
    // if negotiation as a whole failed.
    MOZ_LOG(gSenderLog, LogLevel::Error,
            ("%s[%s]: %s  No video codecs were negotiated (send).",
             mPc->GetHandle().c_str(), GetMid().c_str(), __FUNCTION__));
    return Nothing();
  }

  newConfig.mVideoCodec = Some(configs[0]);
  // Spec says that we start using new parameters right away, _before_ we
  // update the parameters that are visible to JS (ie; mParameters).
  const RTCRtpSendParameters& parameters =
      mPendingParameters.isSome() ? *mPendingParameters : mParameters;
  for (VideoCodecConfig::Encoding& conduitEncoding :
       newConfig.mVideoCodec->mEncodings) {
    for (const RTCRtpEncodingParameters& jsEncoding : parameters.mEncodings) {
      std::string rid;
      if (jsEncoding.mRid.WasPassed()) {
        rid = NS_ConvertUTF16toUTF8(jsEncoding.mRid.Value()).get();
      }
      if (conduitEncoding.rid == rid) {
        ApplyJsEncodingToConduitEncoding(jsEncoding, &conduitEncoding);
        break;
      }
    }
  }

  if (!mHaveLoggedUlpfecInfo) {
    bool ulpfecNegotiated = false;
    for (const auto& codec : configs) {
      if (nsCRT::strcasestr(codec.mName.c_str(), "ulpfec")) {
        ulpfecNegotiated = true;
      }
    }
    mozilla::glean::codec_stats::ulpfec_negotiated
        .Get(ulpfecNegotiated ? "negotiated"_ns : "not_negotiated"_ns)
        .Add(1);
    mHaveLoggedUlpfecInfo = true;
  }

  // Log codec information we are tracking
  if (!mHaveLoggedOtherFec &&
      !GetJsepTransceiver().mSendTrack.GetFecCodecName().empty()) {
    mozilla::glean::codec_stats::other_fec_signaled
        .Get(nsDependentCString(
            GetJsepTransceiver().mSendTrack.GetFecCodecName().c_str()))
        .Add(1);
    mHaveLoggedOtherFec = true;
  }
  if (!mHaveLoggedVideoPreferredCodec &&
      !GetJsepTransceiver().mSendTrack.GetVideoPreferredCodec().empty()) {
    mozilla::glean::codec_stats::video_preferred_codec
        .Get(nsDependentCString(
            GetJsepTransceiver().mSendTrack.GetVideoPreferredCodec().c_str()))
        .Add(1);
    mHaveLoggedVideoPreferredCodec = true;
  }

  newConfig.mVideoRtpRtcpConfig = Some(details.GetRtpRtcpConfig());

  if (newConfig == oldConfig) {
    MOZ_LOG(gSenderLog, LogLevel::Debug,
            ("%s[%s]: %s  No change in video config", mPc->GetHandle().c_str(),
             GetMid().c_str(), __FUNCTION__));
    return Nothing();
  }

  if (newConfig.mVideoCodec.isSome()) {
    MOZ_ASSERT(newConfig.mSsrcs.size() ==
               newConfig.mVideoCodec->mEncodings.size());
  }
  return Some(newConfig);
}

Maybe<RTCRtpSender::AudioConfig> RTCRtpSender::GetNewAudioConfig() {
  AudioConfig oldConfig;
  oldConfig.mSsrcs = mSsrcs;
  oldConfig.mLocalRtpExtensions = mLocalRtpExtensions;
  oldConfig.mCname = mCname;
  oldConfig.mTransmitting = mTransmitting;
  oldConfig.mAudioCodec = mAudioCodec;

  AudioConfig newConfig(oldConfig);

  UpdateBaseConfig(&newConfig);

  if (GetJsepTransceiver().mSendTrack.GetNegotiatedDetails() &&
      GetJsepTransceiver().mSendTrack.GetActive()) {
    const auto& details(
        *GetJsepTransceiver().mSendTrack.GetNegotiatedDetails());

    std::vector<AudioCodecConfig> configs;
    RTCRtpTransceiver::NegotiatedDetailsToAudioCodecConfigs(details, &configs);
    if (configs.empty()) {
      // TODO: Are we supposed to plumb this error back to JS? This does not
      // seem like a failure to set an answer, it just means that codec
      // negotiation failed. For now, we're just doing the same thing we do
      // if negotiation as a whole failed.
      MOZ_LOG(gSenderLog, LogLevel::Error,
              ("%s[%s]: %s No audio codecs were negotiated (send)",
               mPc->GetHandle().c_str(), GetMid().c_str(), __FUNCTION__));
      return Nothing();
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
      newConfig.mDtmfPt = dtmfIterator->mType;
      newConfig.mDtmfFreq = dtmfIterator->mFreq;
    }

    newConfig.mAudioCodec = Some(sendCodec);
  }

  if (!mHaveLoggedAudioPreferredCodec &&
      !GetJsepTransceiver().mSendTrack.GetAudioPreferredCodec().empty()) {
    mozilla::glean::codec_stats::audio_preferred_codec
        .Get(nsDependentCString(
            GetJsepTransceiver().mSendTrack.GetAudioPreferredCodec().c_str()))
        .Add(1);
    mHaveLoggedAudioPreferredCodec = true;
  }

  if (newConfig == oldConfig) {
    MOZ_LOG(gSenderLog, LogLevel::Debug,
            ("%s[%s]: %s  No change in audio config", mPc->GetHandle().c_str(),
             GetMid().c_str(), __FUNCTION__));
    return Nothing();
  }

  return Some(newConfig);
}

void RTCRtpSender::UpdateBaseConfig(BaseConfig* aConfig) {
  aConfig->mSsrcs = GetJsepTransceiver().mSendTrack.GetSsrcs();
  aConfig->mCname = GetJsepTransceiver().mSendTrack.GetCNAME();

  if (GetJsepTransceiver().mSendTrack.GetNegotiatedDetails() &&
      GetJsepTransceiver().mSendTrack.GetActive()) {
    const auto& details(
        *GetJsepTransceiver().mSendTrack.GetNegotiatedDetails());
    {
      std::vector<webrtc::RtpExtension> extmaps;
      // @@NG read extmap from track
      details.ForEachRTPHeaderExtension(
          [&extmaps](const SdpExtmapAttributeList::Extmap& extmap) {
            extmaps.emplace_back(extmap.extensionname, extmap.entry);
          });
      aConfig->mLocalRtpExtensions = extmaps;
    }
  }
  // RTCRtpTransceiver::IsSending is updated after negotiation completes, in a
  // queued task (which we may be in right now). Don't use
  // JsepTrack::GetActive, because that updates before the queued task, which
  // is too early for some of the things we interact with here (eg;
  // RTCDTMFSender).
  aConfig->mTransmitting = mTransceiver->IsSending();
}

void RTCRtpSender::ApplyVideoConfig(const VideoConfig& aConfig) {
  if (aConfig.mVideoCodec.isSome()) {
    MOZ_ASSERT(aConfig.mSsrcs.size() == aConfig.mVideoCodec->mEncodings.size());
  }

  mSsrcs = aConfig.mSsrcs;
  mCname = aConfig.mCname;
  mLocalRtpExtensions = aConfig.mLocalRtpExtensions;

  mVideoRtxSsrcs = aConfig.mVideoRtxSsrcs;
  mVideoCodec = aConfig.mVideoCodec;
  mVideoRtpRtcpConfig = aConfig.mVideoRtpRtcpConfig;
  mVideoCodecMode = aConfig.mVideoCodecMode;

  mTransmitting = aConfig.mTransmitting;
}

void RTCRtpSender::ApplyAudioConfig(const AudioConfig& aConfig) {
  mSsrcs = aConfig.mSsrcs;
  mCname = aConfig.mCname;
  mLocalRtpExtensions = aConfig.mLocalRtpExtensions;

  mAudioCodec = aConfig.mAudioCodec;

  if (aConfig.mDtmfPt >= 0) {
    mDtmf->SetPayloadType(aConfig.mDtmfPt, aConfig.mDtmfFreq);
  }

  mTransmitting = aConfig.mTransmitting;
}

void RTCRtpSender::Stop() {
  MOZ_ASSERT(mTransceiver->Stopping());
  mTransmitting = false;
}

bool RTCRtpSender::HasTrack(const dom::MediaStreamTrack* aTrack) const {
  if (!mSenderTrack) {
    return false;
  }

  if (!aTrack) {
    return true;
  }

  return mSenderTrack.get() == aTrack;
}

RefPtr<MediaPipelineTransmit> RTCRtpSender::GetPipeline() const {
  return mPipeline;
}

std::string RTCRtpSender::GetMid() const { return mTransceiver->GetMidAscii(); }

JsepTransceiver& RTCRtpSender::GetJsepTransceiver() {
  return mTransceiver->GetJsepTransceiver();
}

void RTCRtpSender::UpdateDtmfSender() {
  if (!mDtmf) {
    return;
  }

  if (mTransmitting) {
    return;
  }

  mDtmf->StopPlayout();
}

void RTCRtpSender::SetTransform(RTCRtpScriptTransform* aTransform,
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

  // Seamless switch for frames
  if (aTransform) {
    mFrameTransformerProxy = &aTransform->GetProxy();
  } else {
    mFrameTransformerProxy = nullptr;
  }

  if (mTransform) {
    mTransform->GetProxy().SetSender(nullptr);
  }

  mTransform = const_cast<RTCRtpScriptTransform*>(aTransform);

  if (mTransform) {
    mTransform->GetProxy().SetSender(this);
    mTransform->SetClaimed();
  }
}

bool RTCRtpSender::GenerateKeyFrame(const Maybe<std::string>& aRid) {
  if (!mTransform || !mPipeline || !mSenderTrack) {
    return false;
  }

  mPipeline->mConduit->AsVideoSessionConduit().apply([&](const auto& conduit) {
    conduit->GenerateKeyFrame(aRid, &mTransform->GetProxy());
  });
  return true;
}

}  // namespace mozilla::dom

#undef LOGTAG
