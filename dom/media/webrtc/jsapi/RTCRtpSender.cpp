/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RTCRtpSender.h"
#include "transport/logging.h"
#include "mozilla/dom/MediaStreamTrack.h"
#include "mozilla/dom/Promise.h"
#include "transportbridge/MediaPipeline.h"
#include "nsPIDOMWindow.h"
#include "nsString.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "jsep/JsepTransceiver.h"
#include "mozilla/dom/RTCRtpSenderBinding.h"
#include "RTCStatsReport.h"
#include "mozilla/Preferences.h"
#include "RTCRtpTransceiver.h"
#include "PeerConnectionImpl.h"
#include "libwebrtcglue/AudioConduit.h"
#include <vector>

namespace mozilla::dom {

LazyLogModule gSenderLog("RTCRtpSender");

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(RTCRtpSender)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(RTCRtpSender)
  // We do not do anything here, we wait for BreakCycles to be called
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(RTCRtpSender)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow, mPc, mSenderTrack, mTransceiver,
                                    mStreams, mDtmf)
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
                           RTCRtpTransceiver* aTransceiver)
    : mWindow(aWindow),
      mPc(aPc),
      mSenderTrack(aTrack),
      mTransceiver(aTransceiver),
      INIT_CANONICAL(mSsrcs, Ssrcs()),
      INIT_CANONICAL(mVideoRtxSsrcs, Ssrcs()),
      INIT_CANONICAL(mLocalRtpExtensions, RtpExtList()),
      INIT_CANONICAL(mAudioCodec, Nothing()),
      INIT_CANONICAL(mVideoCodec, Nothing()),
      INIT_CANONICAL(mVideoRtpRtcpConfig, Nothing()),
      INIT_CANONICAL(mVideoCodecMode, webrtc::VideoCodecMode::kRealtimeVideo),
      INIT_CANONICAL(mCname, std::string()),
      INIT_CANONICAL(mTransmitting, false) {
  mPipeline = new MediaPipelineTransmit(
      mPc->GetHandle(), aTransportHandler, aCallThread, aStsThread,
      aConduit->type() == MediaSessionConduit::VIDEO, aConduit);

  if (aConduit->type() == MediaSessionConduit::AUDIO) {
    mDtmf = new RTCDTMFSender(aWindow, mTransceiver);
  }
  mPipeline->SetTrack(mSenderTrack);
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

nsTArray<RefPtr<dom::RTCStatsPromise>> RTCRtpSender::GetStatsInternal() {
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

  promises.AppendElement(
      InvokeAsync(mPipeline->mCallThread, __func__, [pipeline = mPipeline] {
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
                    pipeline->GetTimestampMaker().ConvertNtpToDomTime(
                        webrtc::Timestamp::Micros(
                            aRtcpData.report_block_timestamp_utc_us()) +
                        webrtc::TimeDelta::Seconds(webrtc::kNtpJan1970)));
                aRemote.mId.Construct(remoteId);
                aRemote.mType.Construct(RTCStatsType::Remote_inbound_rtp);
                aRemote.mSsrc.Construct(ssrc);
                aRemote.mMediaType.Construct(
                    kind);  // mediaType is the old name for kind.
                aRemote.mKind.Construct(kind);
                aRemote.mLocalId.Construct(localId);
                if (base_seq) {
                  if (aRtcpData.report_block()
                          .extended_highest_sequence_number < *base_seq) {
                    aRemote.mPacketsReceived.Construct(0);
                  } else {
                    aRemote.mPacketsReceived.Construct(
                        aRtcpData.report_block()
                            .extended_highest_sequence_number -
                        aRtcpData.report_block().packets_lost - *base_seq + 1);
                  }
                }
              };

          auto constructCommonOutboundRtpStats =
              [&](RTCOutboundRtpStreamStats& aLocal) {
                aLocal.mSsrc.Construct(ssrc);
                aLocal.mTimestamp.Construct(
                    pipeline->GetTimestampMaker().GetNow());
                aLocal.mId.Construct(localId);
                aLocal.mType.Construct(RTCStatsType::Outbound_rtp);
                aLocal.mMediaType.Construct(
                    kind);  // mediaType is the old name for kind.
                aLocal.mKind.Construct(kind);
                if (remoteId.Length()) {
                  aLocal.mRemoteId.Construct(remoteId);
                }
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
                  if (data.report_block().source_ssrc == ssrc &&
                      data.report_block().sender_ssrc == *remoteSsrc) {
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
              /*
               * Potential new stats that are now available upstream.
              remote.mFractionLost.Construct(audioStats->fraction_lost);
              remote.mTotalRoundTripTime.Construct(
                  double(aReportBlockData.sum_rtt_ms()) / 1000);
              remote.mRoundTripTimeMeasurements.Construct(
                  aReportBlockData.num_rtts());
               */
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

            if (!streamStats ||
                streamStats->rtp_stats.first_packet_time_ms == -1) {
              // By spec: "The lifetime of all RTP monitored objects starts
              // when the RTP stream is first used: When the first RTP packet
              // is sent or received on the SSRC it represents"
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
                  static_cast<double>(rtcpReportData.report_block().jitter) /
                  webrtc::kVideoPayloadTypeFrequency);
              remote.mPacketsLost.Construct(
                  rtcpReportData.report_block().packets_lost);
              if (rtcpReportData.has_rtt()) {
                remote.mRoundTripTime.Construct(
                    static_cast<double>(rtcpReportData.last_rtt_ms()) / 1000.0);
              }
              constructCommonRemoteInboundRtpStats(remote, rtcpReportData);
              /*
               * Potential new stats that are now available upstream.
              remote.mTotalRoundTripTime.Construct(
                  streamStats->report_block_data->sum_rtt_ms() / 1000.0);
              remote.mFractionLost.Construct(
                  static_cast<float>(streamStats->rtcp_stats.fraction_lost) /
                  (1 << 8));
              remote.mRoundTripTimeMeasurements.Construct(
                  streamStats->report_block_data.num_rtts());
               */
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
          });
        }
        return RTCStatsPromise::CreateAndResolve(std::move(report), __func__);
      }));

  return promises;
}

already_AddRefed<Promise> RTCRtpSender::SetParameters(
    const dom::RTCRtpParameters& aParameters, ErrorResult& aError) {
  // TODO(bug 1401592): transaction ids and other spec fixes
  RefPtr<dom::Promise> p = MakePromise(aError);
  if (aError.Failed()) {
    return nullptr;
  }
  if (mPc->IsClosed()) {
    p->MaybeRejectWithInvalidStateError("Peer connection is closed");
    return p.forget();
  }

  if (mTransceiver->Stopped()) {
    p->MaybeRejectWithInvalidStateError("This sender's transceiver is stopped");
    return p.forget();
  }

  if (!Preferences::GetBool("media.peerconnection.simulcast", false)) {
    p->MaybeResolveWithUndefined();
    return p.forget();
  }

  dom::RTCRtpParameters parameters(aParameters);

  if (!parameters.mEncodings.WasPassed()) {
    parameters.mEncodings.Construct();
  }

  std::set<nsString> uniqueRids;
  for (const auto& encoding : parameters.mEncodings.Value()) {
    if (encoding.mScaleResolutionDownBy < 1.0f) {
      p->MaybeRejectWithRangeError("scaleResolutionDownBy must be >= 1.0");
      return p.forget();
    }
    if (parameters.mEncodings.Value().Length() > 1 &&
        !encoding.mRid.WasPassed()) {
      p->MaybeRejectWithTypeError("Missing rid");
      return p.forget();
    }
    if (encoding.mRid.WasPassed()) {
      if (uniqueRids.count(encoding.mRid.Value())) {
        p->MaybeRejectWithTypeError("Duplicate rid");
        return p.forget();
      }
      uniqueRids.insert(encoding.mRid.Value());
    }

    if (encoding.mMaxFramerate.WasPassed()) {
      if (encoding.mMaxFramerate.Value() < 0.0f) {
        p->MaybeRejectWithRangeError("maxFramerate must be non-negative");
        return p.forget();
      }
    }
  }

  // TODO(bug 1401592): transaction ids, timing changes

  GetMainThreadEventTarget()->Dispatch(NS_NewRunnableFunction(
      __func__, [this, self = RefPtr<RTCRtpSender>(this), p, parameters] {
        // p never resolves if the pc is closed. That's what the spec wants.
        if (!mPc->IsClosed()) {
          ApplyParameters(parameters);
          p->MaybeResolveWithUndefined();
        }
      }));

  return p.forget();
}

void RTCRtpSender::GetParameters(RTCRtpParameters& aParameters) const {
  // TODO(bug 1401592): transaction ids and other spec fixes
  aParameters = mParameters;
}

void RTCRtpSender::ApplyParameters(const RTCRtpParameters& aParameters) {
  mParameters = aParameters;
  std::vector<JsepTrack::JsConstraints> constraints;

  if (aParameters.mEncodings.WasPassed()) {
    for (const auto& encoding : aParameters.mEncodings.Value()) {
      JsepTrack::JsConstraints constraint;
      if (encoding.mRid.WasPassed()) {
        // TODO: Either turn on the RID RTP header extension in JsepSession, or
        // just leave that extension on all the time?
        constraint.rid = NS_ConvertUTF16toUTF8(encoding.mRid.Value()).get();
      }
      if (encoding.mMaxBitrate.WasPassed()) {
        constraint.constraints.maxBr = encoding.mMaxBitrate.Value();
      }
      if (encoding.mMaxFramerate.WasPassed()) {
        constraint.constraints.maxFps = Some(encoding.mMaxFramerate.Value());
      }
      constraint.constraints.scaleDownBy = encoding.mScaleResolutionDownBy;
      constraints.push_back(constraint);
    }
  }

  if (GetJsepTransceiver().mSendTrack.SetJsConstraints(constraints)) {
    if (mPipeline->Transmitting()) {
      UpdateConduit();
    }
  }
}

void RTCRtpSender::SetStreams(
    const Sequence<OwningNonNull<DOMMediaStream>>& aStreams) {
  mStreams.Clear();
  for (const auto& stream : aStreams) {
    mStreams.AppendElement(stream);
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
  // If transceiver.[[Stopped]] is true, return a promise rejected with a newly
  // created InvalidStateError.
  if (mTransceiver->Stopped()) {
    RefPtr<dom::Promise> error = sender->MakePromise(aError);
    if (aError.Failed()) {
      return nullptr;
    }
    MOZ_LOG(gSenderLog, LogLevel::Debug,
            ("%s Cannot call replaceTrack when transceiver is stopped",
             __FUNCTION__));
    error->MaybeRejectWithInvalidStateError(
        "Cannot call replaceTrack when transceiver is stopped");
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
  GetMainThreadEventTarget()->Dispatch(NS_NewRunnableFunction(
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

  // Let sending be true if transceiver.[[CurrentDirection]] is "sendrecv" or
  // "sendonly", and false otherwise.
  bool sending = mTransceiver->IsSending();
  if (sending && !aWithTrack) {
    // If sending is true, and withTrack is null, have the sender stop sending.
    Stop();
  }

  mPipeline->SetTrack(aWithTrack);

  if (sending && aWithTrack) {
    // If sending is true, and withTrack is not null, determine if withTrack can
    // be sent immediately by the sender without violating the sender's
    // already-negotiated envelope, and if it cannot, then reject p with a newly
    // created InvalidModificationError, and abort these steps.

    if (mTransceiver->IsVideo()) {
      // We update the media conduits here so we can apply different codec
      // settings for different sources (e.g. screensharing as opposed to
      // camera.)
      Maybe<MediaSourceEnum> oldType;
      Maybe<MediaSourceEnum> newType;
      if (mSenderTrack) {
        oldType = Some(mSenderTrack->GetSource().GetMediaSource());
      }
      if (aWithTrack) {
        newType = Some(aWithTrack->GetSource().GetMediaSource());
      }
      if (oldType != newType) {
        UpdateConduit();
      }
    } else if (!mSenderTrack != !aWithTrack) {
      UpdateConduit();
    }
  }

  // There may eventually be cases where a renegotiation is necessary to switch.
  return true;
}

void RTCRtpSender::SetTrack(const RefPtr<MediaStreamTrack>& aTrack) {
  // Used for RTCPeerConnection.removeTrack and RTCPeerConnection.addTrack
  mSenderTrack = aTrack;
  SeamlessTrackSwitch(aTrack);
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
  mPipeline->Shutdown();
  mPipeline = nullptr;
}

void RTCRtpSender::BreakCycles() {
  mWindow = nullptr;
  mPc = nullptr;
  mSenderTrack = nullptr;
  mTransceiver = nullptr;
  mStreams.Clear();
  mDtmf = nullptr;
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

void RTCRtpSender::UpdateConduit() {
  // NOTE(pkerr) - the Call API requires the both local_ssrc and remote_ssrc be
  // set to a non-zero value or the CreateVideo...Stream call will fail.
  if (NS_WARN_IF(GetJsepTransceiver().mSendTrack.GetSsrcs().empty())) {
    MOZ_ASSERT(
        false,
        "No local ssrcs! This is a bug in the jsep engine, and should never "
        "happen!");
    return;
  }

  mTransmitting = false;
  Stop();

  mSsrcs = GetJsepTransceiver().mSendTrack.GetSsrcs();
  mVideoRtxSsrcs = GetJsepTransceiver().mSendTrack.GetRtxSsrcs();
  mCname = GetJsepTransceiver().mSendTrack.GetCNAME();

  if (mPipeline->mConduit->type() == MediaSessionConduit::VIDEO) {
    UpdateVideoConduit();
  } else {
    UpdateAudioConduit();
  }
  if ((mTransmitting = GetJsepTransceiver().mSendTrack.GetActive())) {
    Start();
  }
}

void RTCRtpSender::SyncFromJsep(const JsepTransceiver& aJsepTransceiver) {}

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
}

void RTCRtpSender::ConfigureVideoCodecMode() {
  if (!mSenderTrack) {
    // Nothing to do
    return;
  }

  RefPtr<mozilla::dom::VideoStreamTrack> videotrack =
      mSenderTrack->AsVideoStreamTrack();

  if (!videotrack) {
    MOZ_CRASH(
        "In ConfigureVideoCodecMode, mSenderTrack is not video! This should "
        "never happen!");
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
}

void RTCRtpSender::UpdateVideoConduit() {
  // It is possible for SDP to signal that there is a send track, but there not
  // actually be a send track, according to the specification; all that needs to
  // happen is for the transceiver to be configured to send...
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
      mLocalRtpExtensions = extmaps;
    }

    ConfigureVideoCodecMode();

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
      return;
    }

    mVideoCodec = Some(configs[0]);
    mVideoRtpRtcpConfig = Some(details.GetRtpRtcpConfig());
  }
}

void RTCRtpSender::UpdateAudioConduit() {
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
      return;
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

    mAudioCodec = Some(sendCodec);
    {
      std::vector<webrtc::RtpExtension> extmaps;
      // @@NG read extmap from track
      details.ForEachRTPHeaderExtension(
          [&extmaps](const SdpExtmapAttributeList::Extmap& extmap) {
            extmaps.emplace_back(extmap.extensionname, extmap.entry);
          });
      mLocalRtpExtensions = extmaps;
    }
  }
}

void RTCRtpSender::Stop() {
  mPipeline->Stop();
  if (mDtmf) {
    mDtmf->StopPlayout();
  }
}

void RTCRtpSender::Start() {
  if (!mSenderTrack) {
    MOZ_LOG(gSenderLog, LogLevel::Debug,
            ("%s[%s]: %s Starting transmit conduit without send track!",
             mPc->GetHandle().c_str(), GetMid().c_str(), __FUNCTION__));
  }
  mPipeline->Start();
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
  return *mTransceiver->GetJsepTransceiver();
}

}  // namespace mozilla::dom

#undef LOGTAG
