/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include "MediaPipeline.h"

#include <inttypes.h>
#include <math.h>
#include <sstream>

#include "AudioSegment.h"
#include "AudioConverter.h"
#include "DOMMediaStream.h"
#include "ImageContainer.h"
#include "ImageTypes.h"
#include "Layers.h"
#include "MediaEngine.h"
#include "MediaSegment.h"
#include "MediaTrackGraphImpl.h"
#include "MediaTrackListener.h"
#include "MediaStreamTrack.h"
#include "jsapi/RemoteTrackSource.h"
#include "RtpLogger.h"
#include "VideoFrameConverter.h"
#include "VideoSegment.h"
#include "VideoStreamTrack.h"
#include "VideoUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/PeerIdentity.h"
#include "mozilla/Preferences.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Types.h"
#include "nsError.h"
#include "nsThreadUtils.h"
#include "transport/runnable_utils.h"
#include "jsapi/MediaTransportHandler.h"
#include "Tracing.h"
#include "libwebrtcglue/WebrtcImageBuffer.h"
#include "common_video/include/video_frame_buffer.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"

// Max size given stereo is 480*2*2 = 1920 (10ms of 16-bits stereo audio at
// 48KHz)
#define AUDIO_SAMPLE_BUFFER_MAX_BYTES (480 * 2 * 2)
static_assert((WEBRTC_MAX_SAMPLE_RATE / 100) * sizeof(uint16_t) * 2 <=
                  AUDIO_SAMPLE_BUFFER_MAX_BYTES,
              "AUDIO_SAMPLE_BUFFER_MAX_BYTES is not large enough");

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::layers;

mozilla::LazyLogModule gMediaPipelineLog("MediaPipeline");

namespace mozilla {

// An async inserter for audio data, to avoid running audio codec encoders
// on the MTG/input audio thread.  Basically just bounces all the audio
// data to a single audio processing/input queue.  We could if we wanted to
// use multiple threads and a TaskQueue.
class AudioProxyThread {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AudioProxyThread)

  explicit AudioProxyThread(RefPtr<AudioSessionConduit> aConduit)
      : mConduit(std::move(aConduit)),
        mTaskQueue(new TaskQueue(
            GetMediaThreadPool(MediaThreadType::WEBRTC_DECODER), "AudioProxy")),
        mAudioConverter(nullptr) {
    MOZ_ASSERT(mConduit);
    MOZ_COUNT_CTOR(AudioProxyThread);
  }

  // This function is the identity if aInputRate is supported.
  // Else, it returns a rate that is supported, that ensure no loss in audio
  // quality: the sampling rate returned is always greater to the inputed
  // sampling-rate, if they differ..
  uint32_t AppropriateSendingRateForInputRate(uint32_t aInputRate) {
    AudioSessionConduit* conduit =
        static_cast<AudioSessionConduit*>(mConduit.get());
    if (conduit->IsSamplingFreqSupported(aInputRate)) {
      return aInputRate;
    }
    if (aInputRate < 16000) {
      return 16000;
    }
    if (aInputRate < 32000) {
      return 32000;
    }
    if (aInputRate < 44100) {
      return 44100;
    }
    return 48000;
  }

  // From an arbitrary AudioChunk at sampling-rate aRate, process the audio into
  // something the conduit can work with (or send silence if the track is not
  // enabled), and send the audio in 10ms chunks to the conduit.
  void InternalProcessAudioChunk(TrackRate aRate, const AudioChunk& aChunk,
                                 bool aEnabled) {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

    // Convert to interleaved 16-bits integer audio, with a maximum of two
    // channels (since the WebRTC.org code below makes the assumption that the
    // input audio is either mono or stereo), with a sample-rate rate that is
    // 16, 32, 44.1, or 48kHz.
    uint32_t outputChannels = aChunk.ChannelCount() == 1 ? 1 : 2;
    int32_t transmissionRate = AppropriateSendingRateForInputRate(aRate);

    // We take advantage of the fact that the common case (microphone directly
    // to PeerConnection, that is, a normal call), the samples are already
    // 16-bits mono, so the representation in interleaved and planar is the
    // same, and we can just use that.
    if (aEnabled && outputChannels == 1 &&
        aChunk.mBufferFormat == AUDIO_FORMAT_S16 && transmissionRate == aRate) {
      const int16_t* samples = aChunk.ChannelData<int16_t>().Elements()[0];
      PacketizeAndSend(samples, transmissionRate, outputChannels,
                       aChunk.mDuration);
      return;
    }

    uint32_t sampleCount = aChunk.mDuration * outputChannels;
    if (mInterleavedAudio.Length() < sampleCount) {
      mInterleavedAudio.SetLength(sampleCount);
    }

    if (!aEnabled || aChunk.mBufferFormat == AUDIO_FORMAT_SILENCE) {
      PodZero(mInterleavedAudio.Elements(), sampleCount);
    } else if (aChunk.mBufferFormat == AUDIO_FORMAT_FLOAT32) {
      DownmixAndInterleave(aChunk.ChannelData<float>(), aChunk.mDuration,
                           aChunk.mVolume, outputChannels,
                           mInterleavedAudio.Elements());
    } else if (aChunk.mBufferFormat == AUDIO_FORMAT_S16) {
      DownmixAndInterleave(aChunk.ChannelData<int16_t>(), aChunk.mDuration,
                           aChunk.mVolume, outputChannels,
                           mInterleavedAudio.Elements());
    }
    int16_t* inputAudio = mInterleavedAudio.Elements();
    size_t inputAudioFrameCount = aChunk.mDuration;

    AudioConfig inputConfig(AudioConfig::ChannelLayout(outputChannels), aRate,
                            AudioConfig::FORMAT_S16);
    AudioConfig outputConfig(AudioConfig::ChannelLayout(outputChannels),
                             transmissionRate, AudioConfig::FORMAT_S16);
    // Resample to an acceptable sample-rate for the sending side
    if (!mAudioConverter || mAudioConverter->InputConfig() != inputConfig ||
        mAudioConverter->OutputConfig() != outputConfig) {
      mAudioConverter = MakeUnique<AudioConverter>(inputConfig, outputConfig);
    }

    int16_t* processedAudio = nullptr;
    size_t framesProcessed =
        mAudioConverter->Process(inputAudio, inputAudioFrameCount);

    if (framesProcessed == 0) {
      // In place conversion not possible, use a buffer.
      framesProcessed = mAudioConverter->Process(mOutputAudio, inputAudio,
                                                 inputAudioFrameCount);
      processedAudio = mOutputAudio.Data();
    } else {
      processedAudio = inputAudio;
    }

    PacketizeAndSend(processedAudio, transmissionRate, outputChannels,
                     framesProcessed);
  }

  // This packetizes aAudioData in 10ms chunks and sends it.
  // aAudioData is interleaved audio data at a rate and with a channel count
  // that is appropriate to send with the conduit.
  void PacketizeAndSend(const int16_t* aAudioData, uint32_t aRate,
                        uint32_t aChannels, uint32_t aFrameCount) {
    MOZ_ASSERT(AppropriateSendingRateForInputRate(aRate) == aRate);
    MOZ_ASSERT(aChannels == 1 || aChannels == 2);
    MOZ_ASSERT(aAudioData);

    uint32_t audio_10ms = aRate / 100;

    if (!mPacketizer || mPacketizer->mPacketSize != audio_10ms ||
        mPacketizer->mChannels != aChannels) {
      // It's the right thing to drop the bit of audio still in the packetizer:
      // we don't want to send to the conduit audio that has two different
      // rates while telling it that it has a constante rate.
      mPacketizer =
          MakeUnique<AudioPacketizer<int16_t, int16_t>>(audio_10ms, aChannels);
      mPacket = MakeUnique<int16_t[]>(audio_10ms * aChannels);
    }

    mPacketizer->Input(aAudioData, aFrameCount);

    while (mPacketizer->PacketsAvailable()) {
      mPacketizer->Output(mPacket.get());
      auto frame = std::make_unique<webrtc::AudioFrame>();
      // UpdateFrame makes a copy of the audio data.
      frame->UpdateFrame(frame->timestamp_, mPacket.get(),
                         mPacketizer->mPacketSize, aRate, frame->speech_type_,
                         frame->vad_activity_, mPacketizer->mChannels);
      mConduit->SendAudioFrame(std::move(frame));
    }
  }

  void QueueAudioChunk(TrackRate aRate, const AudioChunk& aChunk,
                       bool aEnabled) {
    RefPtr<AudioProxyThread> self = this;
    nsresult rv = mTaskQueue->Dispatch(NS_NewRunnableFunction(
        "AudioProxyThread::QueueAudioChunk", [self, aRate, aChunk, aEnabled]() {
          self->InternalProcessAudioChunk(aRate, aChunk, aEnabled);
        }));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

 protected:
  virtual ~AudioProxyThread() { MOZ_COUNT_DTOR(AudioProxyThread); }

  const RefPtr<AudioSessionConduit> mConduit;
  const RefPtr<TaskQueue> mTaskQueue;
  // Only accessed on mTaskQueue
  UniquePtr<AudioPacketizer<int16_t, int16_t>> mPacketizer;
  // A buffer to hold a single packet of audio.
  UniquePtr<int16_t[]> mPacket;
  nsTArray<int16_t> mInterleavedAudio;
  AlignedShortBuffer mOutputAudio;
  UniquePtr<AudioConverter> mAudioConverter;
};

MediaPipeline::MediaPipeline(const std::string& aPc,
                             RefPtr<MediaTransportHandler> aTransportHandler,
                             DirectionType aDirection,
                             RefPtr<nsISerialEventTarget> aMainThread,
                             RefPtr<AbstractThread> aCallThread,
                             RefPtr<nsISerialEventTarget> aStsThread,
                             RefPtr<MediaSessionConduit> aConduit)
    : mConduit(std::move(aConduit)),
      mDirection(aDirection),
      mMainThread(std::move(aMainThread)),
      mCallThread(std::move(aCallThread)),
      mStsThread(aStsThread),
      mActive(false, "MediaPipeline::mActive"),
      mLevel(0),
      mTransportHandler(std::move(aTransportHandler)),
      mRtpPacketsSent(0),
      mRtcpPacketsSent(0),
      mRtpPacketsReceived(0),
      mRtcpPacketsReceived(0),
      mRtpBytesSent(0),
      mRtpBytesReceived(0),
      mPc(aPc),
      mFilter(),
      mRtpParser(webrtc::RtpHeaderParser::CreateForTest().release()),
      mPacketDumper(new PacketDumper(mPc)) {}

MediaPipeline::~MediaPipeline() {
  MOZ_LOG(gMediaPipelineLog, LogLevel::Info,
          ("Destroying MediaPipeline: %s", mDescription.c_str()));
}

void MediaPipeline::Start() {
  ASSERT_ON_THREAD(mMainThread);
  mActive = true;
}

void MediaPipeline::Stop() {
  ASSERT_ON_THREAD(mMainThread);
  mActive = false;
}

void MediaPipeline::Shutdown() {
  ASSERT_ON_THREAD(mMainThread);
  Stop();

  RUN_ON_THREAD(mStsThread,
                WrapRunnable(RefPtr<MediaPipeline>(this),
                             &MediaPipeline::DetachTransport_s),
                NS_DISPATCH_NORMAL);
}

void MediaPipeline::DetachTransport_s() {
  ASSERT_ON_THREAD(mStsThread);

  MOZ_LOG(gMediaPipelineLog, LogLevel::Info,
          ("%s in %s", mDescription.c_str(), __FUNCTION__));

  disconnect_all();
  mRtpState = TransportLayer::TS_NONE;
  mRtcpState = TransportLayer::TS_NONE;
  mTransportId.clear();
  mConduit->SetTransportActive(false);
  mRtpSendEventListener.DisconnectIfExists();
  mSenderRtcpSendEventListener.DisconnectIfExists();
  mReceiverRtcpSendEventListener.DisconnectIfExists();

  // Make sure any cycles are broken
  mPacketDumper = nullptr;
}

void MediaPipeline::UpdateTransport_m(
    const std::string& aTransportId, UniquePtr<MediaPipelineFilter>&& aFilter) {
  mStsThread->Dispatch(NS_NewRunnableFunction(
      __func__, [aTransportId, filter = std::move(aFilter),
                 self = RefPtr<MediaPipeline>(this)]() mutable {
        self->UpdateTransport_s(aTransportId, std::move(filter));
      }));
}

void MediaPipeline::UpdateTransport_s(
    const std::string& aTransportId, UniquePtr<MediaPipelineFilter>&& aFilter) {
  ASSERT_ON_THREAD(mStsThread);
  if (!mSignalsConnected) {
    mTransportHandler->SignalStateChange.connect(
        this, &MediaPipeline::RtpStateChange);
    mTransportHandler->SignalRtcpStateChange.connect(
        this, &MediaPipeline::RtcpStateChange);
    mTransportHandler->SignalEncryptedSending.connect(
        this, &MediaPipeline::EncryptedPacketSending);
    mTransportHandler->SignalPacketReceived.connect(
        this, &MediaPipeline::PacketReceived);
    mTransportHandler->SignalAlpnNegotiated.connect(
        this, &MediaPipeline::AlpnNegotiated);
    mSignalsConnected = true;
  }

  if (aTransportId != mTransportId) {
    mTransportId = aTransportId;
    mRtpState = mTransportHandler->GetState(mTransportId, false);
    mRtcpState = mTransportHandler->GetState(mTransportId, true);
    CheckTransportStates();
  }

  if (mFilter) {
    for (const auto& extension : mFilter->GetExtmap()) {
      mRtpParser->DeregisterRtpHeaderExtension(extension);
    }
  }
  if (mFilter && aFilter) {
    // Use the new filter, but don't forget any remote SSRCs that we've learned
    // by receiving traffic.
    mFilter->Update(*aFilter);
  } else {
    mFilter = std::move(aFilter);
  }
  if (mFilter) {
    for (const auto& extension : mFilter->GetExtmap()) {
      mRtpParser->RegisterRtpHeaderExtension(extension);
    }
  }
}

void MediaPipeline::GetContributingSourceStats(
    const nsString& aInboundRtpStreamId,
    FallibleTArray<dom::RTCRTPContributingSourceStats>& aArr) const {
  ASSERT_ON_THREAD(mStsThread);
  // Get the expiry from now
  DOMHighResTimeStamp expiry = RtpCSRCStats::GetExpiryFromTime(GetNow());
  for (auto info : mCsrcStats) {
    if (!info.second.Expired(expiry)) {
      RTCRTPContributingSourceStats stats;
      info.second.GetWebidlInstance(stats, aInboundRtpStreamId);
      if (!aArr.AppendElement(stats, fallible)) {
        mozalloc_handle_oom(0);
      }
    }
  }
}

void MediaPipeline::RtpStateChange(const std::string& aTransportId,
                                   TransportLayer::State aState) {
  if (mTransportId != aTransportId) {
    return;
  }
  mRtpState = aState;
  CheckTransportStates();
}

void MediaPipeline::RtcpStateChange(const std::string& aTransportId,
                                    TransportLayer::State aState) {
  if (mTransportId != aTransportId) {
    return;
  }
  mRtcpState = aState;
  CheckTransportStates();
}

void MediaPipeline::CheckTransportStates() {
  ASSERT_ON_THREAD(mStsThread);

  if (mRtpState == TransportLayer::TS_CLOSED ||
      mRtpState == TransportLayer::TS_ERROR ||
      mRtcpState == TransportLayer::TS_CLOSED ||
      mRtcpState == TransportLayer::TS_ERROR) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Warning,
            ("RTP Transport failed for pipeline %p flow %s", this,
             mDescription.c_str()));

    NS_WARNING(
        "MediaPipeline Transport failed. This is not properly cleaned up yet");
    // TODO(ekr@rtfm.com): SECURITY: Figure out how to clean up if the
    // connection was good and now it is bad.
    // TODO(ekr@rtfm.com): Report up so that the PC knows we
    // have experienced an error.
    mConduit->SetTransportActive(false);
    mRtpSendEventListener.DisconnectIfExists();
    mSenderRtcpSendEventListener.DisconnectIfExists();
    mReceiverRtcpSendEventListener.DisconnectIfExists();
    return;
  }

  if (mRtpState == TransportLayer::TS_OPEN) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Info,
            ("RTP Transport ready for pipeline %p flow %s", this,
             mDescription.c_str()));
  }

  if (mRtcpState == TransportLayer::TS_OPEN) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Info,
            ("RTCP Transport ready for pipeline %p flow %s", this,
             mDescription.c_str()));
  }

  if (mRtpState == TransportLayer::TS_OPEN && mRtcpState == mRtpState) {
    if (mDirection == DirectionType::TRANSMIT) {
      mConduit->ConnectSenderRtcpEvent(mSenderRtcpReceiveEvent);
      mRtpSendEventListener = mConduit->SenderRtpSendEvent().Connect(
          mStsThread, this, &MediaPipeline::SendPacket);
      mSenderRtcpSendEventListener = mConduit->SenderRtcpSendEvent().Connect(
          mStsThread, this, &MediaPipeline::SendPacket);
    } else {
      mConduit->ConnectReceiverRtcpEvent(mReceiverRtcpReceiveEvent);
      mConduit->ConnectReceiverRtpEvent(mRtpReceiveEvent);
      mReceiverRtcpSendEventListener =
          mConduit->ReceiverRtcpSendEvent().Connect(mStsThread, this,
                                                    &MediaPipeline::SendPacket);
    }
    mConduit->SetTransportActive(true);
    TransportReady_s();
  }
}

void MediaPipeline::SendPacket(MediaPacket&& aPacket) {
  ASSERT_ON_THREAD(mStsThread);

  const bool isRtp = aPacket.type() == MediaPacket::RTP;

  if (isRtp && mRtpState != TransportLayer::TS_OPEN) {
    return;
  }

  if (!isRtp && mRtcpState != TransportLayer::TS_OPEN) {
    return;
  }

  aPacket.sdp_level() = Some(Level());

  if (RtpLogger::IsPacketLoggingOn()) {
    RtpLogger::LogPacket(aPacket, false, mDescription);
  }

  if (isRtp) {
    mPacketDumper->Dump(Level(), dom::mozPacketDumpType::Rtp, true,
                        aPacket.data(), aPacket.len());
    IncrementRtpPacketsSent(aPacket);
  } else {
    mPacketDumper->Dump(Level(), dom::mozPacketDumpType::Rtcp, true,
                        aPacket.data(), aPacket.len());
    IncrementRtcpPacketsSent();
  }

  MOZ_LOG(
      gMediaPipelineLog, LogLevel::Debug,
      ("%s sending %s packet", mDescription.c_str(), (isRtp ? "RTP" : "RTCP")));

  mTransportHandler->SendPacket(mTransportId, std::move(aPacket));
}

void MediaPipeline::IncrementRtpPacketsSent(const MediaPacket& aPacket) {
  ASSERT_ON_THREAD(mStsThread);
  ++mRtpPacketsSent;
  mRtpBytesSent += aPacket.len();

  if (!(mRtpPacketsSent % 100)) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Info,
            ("RTP sent packet count for %s Pipeline %p: %u (%" PRId64 " bytes)",
             mDescription.c_str(), this, mRtpPacketsSent, mRtpBytesSent));
  }
}

void MediaPipeline::IncrementRtcpPacketsSent() {
  ASSERT_ON_THREAD(mStsThread);
  ++mRtcpPacketsSent;
  if (!(mRtcpPacketsSent % 100)) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Info,
            ("RTCP sent packet count for %s Pipeline %p: %u",
             mDescription.c_str(), this, mRtcpPacketsSent));
  }
}

void MediaPipeline::IncrementRtpPacketsReceived(int32_t aBytes) {
  ASSERT_ON_THREAD(mStsThread);
  ++mRtpPacketsReceived;
  mRtpBytesReceived += aBytes;
  if (!(mRtpPacketsReceived % 100)) {
    MOZ_LOG(
        gMediaPipelineLog, LogLevel::Info,
        ("RTP received packet count for %s Pipeline %p: %u (%" PRId64 " bytes)",
         mDescription.c_str(), this, mRtpPacketsReceived, mRtpBytesReceived));
  }
}

void MediaPipeline::IncrementRtcpPacketsReceived() {
  ASSERT_ON_THREAD(mStsThread);
  ++mRtcpPacketsReceived;
  if (!(mRtcpPacketsReceived % 100)) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Info,
            ("RTCP received packet count for %s Pipeline %p: %u",
             mDescription.c_str(), this, mRtcpPacketsReceived));
  }
}

void MediaPipeline::RtpPacketReceived(const MediaPacket& packet) {
  ASSERT_ON_THREAD(mStsThread);

  if (mDirection == DirectionType::TRANSMIT) {
    return;
  }

  if (!packet.len()) {
    return;
  }

  webrtc::RTPHeader header;
  if (!mRtpParser->Parse(packet.data(), packet.len(), &header, true)) {
    return;
  }

  if (mFilter && !mFilter->Filter(header)) {
    return;
  }

  // Make sure to only get the time once, and only if we need it by
  // using getTimestamp() for access
  DOMHighResTimeStamp now = 0.0;
  bool hasTime = false;

  // Remove expired RtpCSRCStats
  if (!mCsrcStats.empty()) {
    if (!hasTime) {
      now = GetNow();
      hasTime = true;
    }
    auto expiry = RtpCSRCStats::GetExpiryFromTime(now);
    for (auto p = mCsrcStats.begin(); p != mCsrcStats.end();) {
      if (p->second.Expired(expiry)) {
        p = mCsrcStats.erase(p);
        continue;
      }
      p++;
    }
  }

  // Add new RtpCSRCStats
  if (header.numCSRCs) {
    for (auto i = 0; i < header.numCSRCs; i++) {
      if (!hasTime) {
        now = GetNow();
        hasTime = true;
      }
      auto csrcInfo = mCsrcStats.find(header.arrOfCSRCs[i]);
      if (csrcInfo == mCsrcStats.end()) {
        mCsrcStats.insert(std::make_pair(
            header.arrOfCSRCs[i], RtpCSRCStats(header.arrOfCSRCs[i], now)));
      } else {
        csrcInfo->second.SetTimestamp(now);
      }
    }
  }

  MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
          ("%s received RTP packet.", mDescription.c_str()));
  IncrementRtpPacketsReceived(packet.len());
  OnRtpPacketReceived();

  RtpLogger::LogPacket(packet, true, mDescription);

  // Might be nice to pass ownership of the buffer in this case, but it is a
  // small optimization in a rare case.
  mPacketDumper->Dump(mLevel, dom::mozPacketDumpType::Srtp, false,
                      packet.encrypted_data(), packet.encrypted_len());

  mPacketDumper->Dump(mLevel, dom::mozPacketDumpType::Rtp, false, packet.data(),
                      packet.len());

  mRtpReceiveEvent.Notify(packet, header);
}

void MediaPipeline::RtcpPacketReceived(const MediaPacket& packet) {
  ASSERT_ON_THREAD(mStsThread);

  if (!packet.len()) {
    return;
  }

  // We do not filter RTCP. This is because a compound RTCP packet can contain
  // any collection of RTCP packets, and webrtc.org already knows how to filter
  // out what it is interested in, and what it is not. Maybe someday we should
  // have a TransportLayer that breaks up compound RTCP so we can filter them
  // individually, but I doubt that will matter much.

  MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
          ("%s received RTCP packet.", mDescription.c_str()));
  IncrementRtcpPacketsReceived();

  RtpLogger::LogPacket(packet, true, mDescription);

  // Might be nice to pass ownership of the buffer in this case, but it is a
  // small optimization in a rare case.
  mPacketDumper->Dump(mLevel, dom::mozPacketDumpType::Srtcp, false,
                      packet.encrypted_data(), packet.encrypted_len());

  mPacketDumper->Dump(mLevel, dom::mozPacketDumpType::Rtcp, false,
                      packet.data(), packet.len());

  if (StaticPrefs::media_webrtc_net_force_disable_rtcp_reception()) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
            ("%s RTCP packet forced to be dropped", mDescription.c_str()));
    return;
  }

  if (mDirection == DirectionType::TRANSMIT) {
    mSenderRtcpReceiveEvent.Notify(std::move(packet));
  } else {
    mReceiverRtcpReceiveEvent.Notify(std::move(packet));
  }
}

void MediaPipeline::PacketReceived(const std::string& aTransportId,
                                   const MediaPacket& packet) {
  ASSERT_ON_THREAD(mStsThread);

  if (mTransportId != aTransportId) {
    return;
  }

  MOZ_ASSERT(mRtpState == TransportLayer::TS_OPEN);
  MOZ_ASSERT(mRtcpState == mRtpState);

  switch (packet.type()) {
    case MediaPacket::RTP:
      RtpPacketReceived(packet);
      break;
    case MediaPacket::RTCP:
      RtcpPacketReceived(packet);
      break;
    default:;
  }
}

void MediaPipeline::AlpnNegotiated(const std::string& aAlpn,
                                   bool aPrivacyRequested) {
  ASSERT_ON_THREAD(mStsThread);

  if (aPrivacyRequested) {
    MakePrincipalPrivate_s();
  }
}

void MediaPipeline::EncryptedPacketSending(const std::string& aTransportId,
                                           const MediaPacket& aPacket) {
  ASSERT_ON_THREAD(mStsThread);

  if (mTransportId == aTransportId) {
    dom::mozPacketDumpType type;
    if (aPacket.type() == MediaPacket::SRTP) {
      type = dom::mozPacketDumpType::Srtp;
    } else if (aPacket.type() == MediaPacket::SRTCP) {
      type = dom::mozPacketDumpType::Srtcp;
    } else if (aPacket.type() == MediaPacket::DTLS) {
      // TODO(bug 1497936): Implement packet dump for DTLS
      return;
    } else {
      MOZ_ASSERT(false);
      return;
    }
    mPacketDumper->Dump(Level(), type, true, aPacket.data(), aPacket.len());
  }
}

class MediaPipelineTransmit::PipelineListener
    : public DirectMediaTrackListener {
  friend class MediaPipelineTransmit;

 public:
  explicit PipelineListener(RefPtr<MediaSessionConduit> aConduit)
      : mConduit(std::move(aConduit)),
        mActive(false),
        mEnabled(false),
        mDirectConnect(false) {}

  ~PipelineListener() {
    if (mConverter) {
      mConverter->Shutdown();
    }
  }

  void SetActive(bool aActive) {
    mActive = aActive;
    if (mConverter) {
      mConverter->SetActive(aActive);
    }
  }
  void SetEnabled(bool aEnabled) { mEnabled = aEnabled; }

  // These are needed since nested classes don't have access to any particular
  // instance of the parent
  void SetAudioProxy(RefPtr<AudioProxyThread> aProxy) {
    mAudioProcessing = std::move(aProxy);
  }

  void SetVideoFrameConverter(RefPtr<VideoFrameConverter> aConverter) {
    mConverter = std::move(aConverter);
  }

  void OnVideoFrameConverted(const webrtc::VideoFrame& aVideoFrame) {
    MOZ_RELEASE_ASSERT(mConduit->type() == MediaSessionConduit::VIDEO);
    static_cast<VideoSessionConduit*>(mConduit.get())
        ->SendVideoFrame(aVideoFrame);
  }

  // Implement MediaTrackListener
  void NotifyQueuedChanges(MediaTrackGraph* aGraph, TrackTime aOffset,
                           const MediaSegment& aQueuedMedia) override;
  void NotifyEnabledStateChanged(MediaTrackGraph* aGraph,
                                 bool aEnabled) override;

  // Implement DirectMediaTrackListener
  void NotifyRealtimeTrackData(MediaTrackGraph* aGraph, TrackTime aOffset,
                               const MediaSegment& aMedia) override;
  void NotifyDirectListenerInstalled(InstallationResult aResult) override;
  void NotifyDirectListenerUninstalled() override;

 private:
  void NewData(const MediaSegment& aMedia, TrackRate aRate = 0);

  const RefPtr<MediaSessionConduit> mConduit;
  RefPtr<AudioProxyThread> mAudioProcessing;
  RefPtr<VideoFrameConverter> mConverter;

  // active is true if there is a transport to send on
  mozilla::Atomic<bool> mActive;
  // enabled is true if the media access control permits sending
  // actual content; when false you get black/silence
  mozilla::Atomic<bool> mEnabled;

  // Written and read on the MediaTrackGraph thread
  bool mDirectConnect;
};

// Implements VideoConverterListener for MediaPipeline.
//
// We pass converted frames on to MediaPipelineTransmit::PipelineListener
// where they are further forwarded to VideoConduit.
// MediaPipelineTransmit calls Detach() during shutdown to ensure there is
// no cyclic dependencies between us and PipelineListener.
class MediaPipelineTransmit::VideoFrameFeeder : public VideoConverterListener {
 public:
  explicit VideoFrameFeeder(RefPtr<PipelineListener> aListener)
      : mMutex("VideoFrameFeeder"), mListener(std::move(aListener)) {
    MOZ_COUNT_CTOR(VideoFrameFeeder);
  }

  void Detach() {
    MutexAutoLock lock(mMutex);

    mListener = nullptr;
  }

  void OnVideoFrameConverted(const webrtc::VideoFrame& aVideoFrame) override {
    MutexAutoLock lock(mMutex);

    if (!mListener) {
      return;
    }

    mListener->OnVideoFrameConverted(aVideoFrame);
  }

 protected:
  MOZ_COUNTED_DTOR_OVERRIDE(VideoFrameFeeder)

  Mutex mMutex;  // Protects the member below.
  RefPtr<PipelineListener> mListener;
};

MediaPipelineTransmit::MediaPipelineTransmit(
    const std::string& aPc, RefPtr<MediaTransportHandler> aTransportHandler,
    RefPtr<nsISerialEventTarget> aMainThread,
    RefPtr<AbstractThread> aCallThread, RefPtr<nsISerialEventTarget> aStsThread,
    bool aIsVideo, RefPtr<MediaSessionConduit> aConduit)
    : MediaPipeline(aPc, std::move(aTransportHandler), DirectionType::TRANSMIT,
                    std::move(aMainThread), std::move(aCallThread),
                    std::move(aStsThread), std::move(aConduit)),
      mWatchManager(this, AbstractThread::MainThread()),
      mIsVideo(aIsVideo),
      mListener(new PipelineListener(mConduit)),
      mFeeder(aIsVideo ? MakeAndAddRef<VideoFrameFeeder>(mListener)
                       : nullptr),  // For video we send frames to an
                                    // async VideoFrameConverter that
                                    // calls back to a VideoFrameFeeder
                                    // that feeds I420 frames to
                                    // VideoConduit.
      mDomTrack(nullptr, "MediaPipelineTransmit::mDomTrack"),
      mSendTrackOverride(nullptr, "MediaPipelineTransmit::mSendTrackOverride") {
  if (!IsVideo()) {
    mAudioProcessing =
        MakeAndAddRef<AudioProxyThread>(*mConduit->AsAudioSessionConduit());
    mListener->SetAudioProxy(mAudioProcessing);
  } else {  // Video
    mConverter = MakeAndAddRef<VideoFrameConverter>();
    mConverter->AddListener(mFeeder);
    mListener->SetVideoFrameConverter(mConverter);
  }

  mWatchManager.Watch(mActive, &MediaPipelineTransmit::UpdateSendState);
  mWatchManager.Watch(mDomTrack, &MediaPipelineTransmit::UpdateSendState);
  mWatchManager.Watch(mSendTrackOverride,
                      &MediaPipelineTransmit::UpdateSendState);

  mDescription = GenerateDescription();
}

MediaPipelineTransmit::~MediaPipelineTransmit() {
  if (mFeeder) {
    mFeeder->Detach();
  }

  MOZ_ASSERT(!mTransmitting);
  MOZ_ASSERT(!mDomTrack.Ref());
}

void MediaPipelineTransmit::Shutdown() {
  MediaPipeline::Shutdown();
  MOZ_ASSERT(!mActive);
  mWatchManager.Shutdown();
  mDomTrack = nullptr;
  mUnsettingSendTrack = false;
  UpdateSendState();
  MOZ_ASSERT(!mTransmitting);
}

void MediaPipeline::SetDescription_s(const std::string& description) {
  ASSERT_ON_THREAD(mStsThread);
  mDescription = description;
}

std::string MediaPipelineTransmit::GenerateDescription() const {
  ASSERT_ON_THREAD(mMainThread);

  std::stringstream description;
  description << mPc << "| ";
  description << (mIsVideo ? "Transmit video[" : "Transmit audio[");

  if (mDomTrack.Ref()) {
    nsString nsTrackId;
    mDomTrack.Ref()->GetId(nsTrackId);
    description << NS_ConvertUTF16toUTF8(nsTrackId).get();
  } else if (mSendTrackOverride.Ref()) {
    description << "override " << mSendTrackOverride.Ref().get();
  } else {
    description << "no track";
  }

  description << "]";

  return description.str();
}

void MediaPipelineTransmit::UpdateSendState() {
  ASSERT_ON_THREAD(mMainThread);

  // This runs because either mActive, mDomTrack or mSendTrackOverride changed,
  // or because mSendTrack was unset async. Based on these inputs this method
  // is responsible for hooking up mSendTrack to mListener in order to feed data
  // to the conduit.
  //
  // If we are inactive, or if the send track does not match what we want to
  // send (mDomTrack or mSendTrackOverride), we must stop feeding data to the
  // conduit. NB that removing the listener from mSendTrack is async, and we
  // must wait for it to resolve before adding mListener to another track.
  // mUnsettingSendTrack gates us until the listener has been removed from
  // mSendTrack.
  //
  // If we are active and the send track does match what we want to send, we
  // make sure mListener is added to the send track. Either now, or if we're
  // still waiting for another send track to be removed, during a future call to
  // this method.

  if (mUnsettingSendTrack) {
    // We must wait for the send track to be unset before we can set it again,
    // to avoid races. Once unset this function is triggered again.
    return;
  }

  const bool wasTransmitting = mTransmitting;

  const bool haveLiveSendTrack = mSendTrack && !mSendTrack->IsDestroyed();
  const bool haveLiveDomTrack = mDomTrack.Ref() && !mDomTrack.Ref()->Ended();
  const bool haveLiveOverrideTrack =
      mSendTrackOverride.Ref() && !mSendTrackOverride.Ref()->IsDestroyed();
  const bool mustRemoveSendTrack =
      haveLiveSendTrack && !mSendTrackOverride.Ref() &&
      (!haveLiveDomTrack ||
       mDomTrack.Ref()->GetTrack() != mSendPort->GetSource());

  mTransmitting = mActive && (haveLiveDomTrack || haveLiveOverrideTrack) &&
                  !mustRemoveSendTrack;

  MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
          ("MediaPipeline %p UpdateSendState wasTransmitting=%d, active=%d, "
           "sendTrack=%p (%s), domTrack=%p (%s), "
           "sendTrackOverride=%p (%s), mustRemove=%d, mTransmitting=%d",
           this, wasTransmitting, mActive.Ref(), mSendTrack.get(),
           haveLiveSendTrack ? "live" : "ended", mDomTrack.Ref().get(),
           haveLiveDomTrack ? "live" : "ended", mSendTrackOverride.Ref().get(),
           haveLiveOverrideTrack ? "live" : "ended", mustRemoveSendTrack,
           mTransmitting));

  if (!wasTransmitting && mTransmitting) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
            ("Attaching pipeline %p to track %p conduit type=%s", this,
             mDomTrack.Ref().get(), mIsVideo ? "video" : "audio"));
    if (mDescriptionInvalidated) {
      // Only update the description when we attach to a track, as detaching is
      // always a longer async step than updating the description. Updating on
      // detach would cause the wrong track id to be attributed in logs.
      RUN_ON_THREAD(mStsThread,
                    WrapRunnable(RefPtr<MediaPipeline>(this),
                                 &MediaPipelineTransmit::SetDescription_s,
                                 GenerateDescription()),
                    NS_DISPATCH_NORMAL);
      mDescriptionInvalidated = false;
    }
    if (mSendTrackOverride.Ref()) {
      // Special path that allows unittests to avoid mDomTrack and the graph by
      // manually calling SetSendTrack.
      mSendTrack = mSendTrackOverride.Ref();
    } else {
      mSendTrack = mDomTrack.Ref()->Graph()->CreateForwardedInputTrack(
          mDomTrack.Ref()->GetTrack()->mType);
      mSendPort = mSendTrack->AllocateInputPort(mDomTrack.Ref()->GetTrack());
    }
    mSendTrack->QueueSetAutoend(false);
    if (mIsVideo) {
      mSendTrack->AddDirectListener(mListener);
    }
    mSendTrack->AddListener(mListener);
  }

  if (wasTransmitting && !mTransmitting) {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
            ("Detaching pipeline %p from track %p conduit type=%s", this,
             mDomTrack.Ref().get(), mIsVideo ? "video" : "audio"));
    mUnsettingSendTrack = true;
    if (mIsVideo) {
      mSendTrack->RemoveDirectListener(mListener);
    }
    mSendTrack->RemoveListener(mListener)->Then(
        mMainThread, __func__,
        [this, self = RefPtr<MediaPipelineTransmit>(this)] {
          mUnsettingSendTrack = false;
          mSendTrack = nullptr;
          if (!mWatchManager.IsShutdown()) {
            mWatchManager.ManualNotify(&MediaPipelineTransmit::UpdateSendState);
          }
        });
    if (!mSendTrackOverride.Ref()) {
      // If an override is set it may be re-used.
      mSendTrack->Destroy();
      mSendPort->Destroy();
      mSendPort = nullptr;
    }
  }
}

bool MediaPipelineTransmit::Transmitting() const {
  ASSERT_ON_THREAD(mMainThread);

  return mActive;
}

bool MediaPipelineTransmit::IsVideo() const { return mIsVideo; }

void MediaPipelineTransmit::UpdateSinkIdentity_m(
    const MediaStreamTrack* aTrack, nsIPrincipal* aPrincipal,
    const PeerIdentity* aSinkIdentity) {
  ASSERT_ON_THREAD(mMainThread);

  if (aTrack != nullptr && aTrack != mDomTrack.Ref()) {
    // If a track is specified, then it might not be for this pipeline,
    // since we receive notifications for all tracks on the PC.
    // nullptr means that the PeerIdentity has changed and shall be applied
    // to all tracks of the PC.
    return;
  }

  if (!mDomTrack.Ref()) {
    // Nothing to do here
    return;
  }

  bool enableTrack = aPrincipal->Subsumes(mDomTrack.Ref()->GetPrincipal());
  if (!enableTrack) {
    // first try didn't work, but there's a chance that this is still available
    // if our track is bound to a peerIdentity, and the peer connection (our
    // sink) is bound to the same identity, then we can enable the track.
    const PeerIdentity* trackIdentity = mDomTrack.Ref()->GetPeerIdentity();
    if (aSinkIdentity && trackIdentity) {
      enableTrack = (*aSinkIdentity == *trackIdentity);
    }
  }

  mListener->SetEnabled(enableTrack);
}

void MediaPipelineTransmit::TransportReady_s() {
  ASSERT_ON_THREAD(mStsThread);
  // Call base ready function.
  MediaPipeline::TransportReady_s();
  mListener->SetActive(true);
}

nsresult MediaPipelineTransmit::SetTrack(RefPtr<MediaStreamTrack> aDomTrack) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aDomTrack) {
    nsString nsTrackId;
    aDomTrack->GetId(nsTrackId);
    MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
            ("Reattaching pipeline to track %p track %s conduit type: %s",
             aDomTrack.get(), NS_ConvertUTF16toUTF8(nsTrackId).get(),
             mIsVideo ? "video" : "audio"));
  }

  mDescriptionInvalidated = true;
  mDomTrack = std::move(aDomTrack);

  return NS_OK;
}

RefPtr<dom::MediaStreamTrack> MediaPipelineTransmit::GetTrack() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mDomTrack;
}

void MediaPipelineTransmit::SetSendTrackOverride(
    RefPtr<ProcessedMediaTrack> aSendTrack) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(!mSendTrack);
  MOZ_RELEASE_ASSERT(!mSendPort);
  MOZ_RELEASE_ASSERT(!mSendTrackOverride.Ref());
  mDescriptionInvalidated = true;
  mSendTrackOverride = std::move(aSendTrack);
}

// Called if we're attached with AddDirectListener()
void MediaPipelineTransmit::PipelineListener::NotifyRealtimeTrackData(
    MediaTrackGraph* aGraph, TrackTime aOffset, const MediaSegment& aMedia) {
  MOZ_LOG(
      gMediaPipelineLog, LogLevel::Debug,
      ("MediaPipeline::NotifyRealtimeTrackData() listener=%p, offset=%" PRId64
       ", duration=%" PRId64,
       this, aOffset, aMedia.GetDuration()));
  TRACE_COMMENT(
      "MediaPipelineTransmit::PipelineListener::NotifyRealtimeTrackData", "%s",
      aMedia.GetType() == MediaSegment::VIDEO ? "Video" : "Audio");
  NewData(aMedia, aGraph->GraphRate());
}

void MediaPipelineTransmit::PipelineListener::NotifyQueuedChanges(
    MediaTrackGraph* aGraph, TrackTime aOffset,
    const MediaSegment& aQueuedMedia) {
  MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
          ("MediaPipeline::NotifyQueuedChanges()"));

  if (aQueuedMedia.GetType() == MediaSegment::VIDEO) {
    // We always get video from the direct listener.
    return;
  }

  TRACE("MediaPipelineTransmit::PipelineListener::NotifyQueuedChanges (Audio)");

  if (mDirectConnect) {
    // ignore non-direct data if we're also getting direct data
    return;
  }

  size_t rate;
  if (aGraph) {
    rate = aGraph->GraphRate();
  } else {
    // When running tests, graph may be null. In that case use a default.
    rate = 16000;
  }
  NewData(aQueuedMedia, rate);
}

void MediaPipelineTransmit::PipelineListener::NotifyEnabledStateChanged(
    MediaTrackGraph* aGraph, bool aEnabled) {
  if (mConduit->type() != MediaSessionConduit::VIDEO) {
    return;
  }
  MOZ_ASSERT(mConverter);
  mConverter->SetTrackEnabled(aEnabled);
}

void MediaPipelineTransmit::PipelineListener::NotifyDirectListenerInstalled(
    InstallationResult aResult) {
  MOZ_LOG(gMediaPipelineLog, LogLevel::Info,
          ("MediaPipeline::NotifyDirectListenerInstalled() listener=%p,"
           " result=%d",
           this, static_cast<int32_t>(aResult)));

  mDirectConnect = InstallationResult::SUCCESS == aResult;
}

void MediaPipelineTransmit::PipelineListener::
    NotifyDirectListenerUninstalled() {
  MOZ_LOG(
      gMediaPipelineLog, LogLevel::Info,
      ("MediaPipeline::NotifyDirectListenerUninstalled() listener=%p", this));

  if (mConduit->type() == MediaSessionConduit::VIDEO) {
    // Reset the converter's track-enabled state. If re-added to a new track
    // later and that track is disabled, we will be signaled explicitly.
    MOZ_ASSERT(mConverter);
    mConverter->SetTrackEnabled(true);
  }

  mDirectConnect = false;
}

void MediaPipelineTransmit::PipelineListener::NewData(
    const MediaSegment& aMedia, TrackRate aRate /* = 0 */) {
  if (mConduit->type() != (aMedia.GetType() == MediaSegment::AUDIO
                               ? MediaSessionConduit::AUDIO
                               : MediaSessionConduit::VIDEO)) {
    MOZ_ASSERT(false,
               "The media type should always be correct since the "
               "listener is locked to a specific track");
    return;
  }

  // TODO(ekr@rtfm.com): For now assume that we have only one
  // track type and it's destined for us
  // See bug 784517
  if (aMedia.GetType() == MediaSegment::AUDIO) {
    MOZ_RELEASE_ASSERT(aRate > 0);

    if (!mActive) {
      MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
              ("Discarding audio packets because transport not ready"));
      return;
    }

    const AudioSegment* audio = static_cast<const AudioSegment*>(&aMedia);
    for (AudioSegment::ConstChunkIterator iter(*audio); !iter.IsEnded();
         iter.Next()) {
      mAudioProcessing->QueueAudioChunk(aRate, *iter, mEnabled);
    }
  } else {
    const VideoSegment* video = static_cast<const VideoSegment*>(&aMedia);

    for (VideoSegment::ConstChunkIterator iter(*video); !iter.IsEnded();
         iter.Next()) {
      mConverter->QueueVideoChunk(*iter, !mEnabled);
    }
  }
}

class GenericReceiveListener : public MediaTrackListener {
 public:
  explicit GenericReceiveListener(RefPtr<nsISerialEventTarget> aMainThread,
                                  const RefPtr<dom::MediaStreamTrack>& aTrack)
      : mMainThread(std::move(aMainThread)),
        mTrackSource(new nsMainThreadPtrHolder<RemoteTrackSource>(
            "GenericReceiveListener::mTrackSource",
            &static_cast<RemoteTrackSource&>(aTrack->GetSource()))),
        mSource(mTrackSource->mStream),
        mIsAudio(aTrack->AsAudioStreamTrack()),
        mEnabled(false),
        mMaybeTrackNeedsUnmute(true) {
    MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
    MOZ_DIAGNOSTIC_ASSERT(mSource, "Must be used with a SourceMediaTrack");
  }

  virtual ~GenericReceiveListener() = default;

  void SetEnabled(bool aEnabled) {
    if (mEnabled == aEnabled) {
      return;
    }
    mEnabled = aEnabled;
    if (aEnabled) {
      mMaybeTrackNeedsUnmute = true;
    }
    if (mIsAudio && !mSource->IsDestroyed()) {
      mSource->SetPullingEnabled(mEnabled);
    }
  }

  void OnRtpReceived() {
    if (mMaybeTrackNeedsUnmute) {
      mMaybeTrackNeedsUnmute = false;
      mMainThread->Dispatch(
          NewRunnableMethod("GenericReceiveListener::OnRtpReceived_m", this,
                            &GenericReceiveListener::OnRtpReceived_m));
    }
  }

  void OnRtpReceived_m() {
    if (mEnabled) {
      mTrackSource->SetMuted(false);
    }
  }

  void EndTrack() {
    MOZ_LOG(gMediaPipelineLog, LogLevel::Debug,
            ("GenericReceiveListener ending track"));

    if (!mSource->IsDestroyed()) {
      // This breaks the cycle with the SourceMediaTrack
      mSource->RemoveListener(this);
      mSource->End();
      mSource->Destroy();
    }

    mMainThread->Dispatch(NewRunnableMethod("RemoteTrackSource::ForceEnded",
                                            mTrackSource.get(),
                                            &RemoteTrackSource::ForceEnded));
  }

 protected:
  const RefPtr<nsISerialEventTarget> mMainThread;
  const nsMainThreadPtrHandle<RemoteTrackSource> mTrackSource;
  const RefPtr<SourceMediaTrack> mSource;
  const bool mIsAudio;
  // Main thread only.
  bool mEnabled;
  // Any thread.
  Atomic<bool> mMaybeTrackNeedsUnmute;
};

MediaPipelineReceive::MediaPipelineReceive(
    const std::string& aPc, RefPtr<MediaTransportHandler> aTransportHandler,
    RefPtr<nsISerialEventTarget> aMainThread,
    RefPtr<AbstractThread> aCallThread, RefPtr<nsISerialEventTarget> aStsThread,
    RefPtr<MediaSessionConduit> aConduit)
    : MediaPipeline(aPc, std::move(aTransportHandler), DirectionType::RECEIVE,
                    std::move(aMainThread), std::move(aCallThread),
                    std::move(aStsThread), std::move(aConduit)) {}

MediaPipelineReceive::~MediaPipelineReceive() = default;

class MediaPipelineReceiveAudio::PipelineListener
    : public GenericReceiveListener {
 public:
  PipelineListener(RefPtr<nsISerialEventTarget> aMainThread,
                   const RefPtr<dom::MediaStreamTrack>& aTrack,
                   RefPtr<MediaSessionConduit> aConduit,
                   const PrincipalHandle& aPrincipalHandle)
      : GenericReceiveListener(std::move(aMainThread), aTrack),
        mConduit(std::move(aConduit)),
        // AudioSession conduit only supports 16, 32, 44.1 and 48kHz
        // This is an artificial limitation, it would however require more
        // changes to support any rates. If the sampling rate is not-supported,
        // we will use 48kHz instead.
        mRate(static_cast<AudioSessionConduit*>(mConduit.get())
                      ->IsSamplingFreqSupported(mSource->Graph()->GraphRate())
                  ? mSource->Graph()->GraphRate()
                  : WEBRTC_MAX_SAMPLE_RATE),
        mTaskQueue(
            new TaskQueue(GetMediaThreadPool(MediaThreadType::WEBRTC_DECODER),
                          "AudioPipelineListener")),
        mPlayedTicks(0),
        mAudioFrame(std::make_unique<webrtc::AudioFrame>()),
        mPrincipalHandle(aPrincipalHandle),
        mForceSilence(false) {}

  void Init() {
    mSource->SetAppendDataSourceRate(mRate);
    mSource->AddListener(this);
  }

  // Implement MediaTrackListener
  void NotifyPull(MediaTrackGraph* aGraph, TrackTime aEndOfAppendedData,
                  TrackTime aDesiredTime) override {
    NotifyPullImpl(aDesiredTime);
  }

  void MakePrincipalPrivate_s() {
    mForceSilence = true;

    mMainThread->Dispatch(NS_NewRunnableFunction(
        "MediaPipelineReceiveAudio::PipelineListener::MakePrincipalPrivate_s",
        [self = RefPtr<PipelineListener>(this), this] {
          class Message : public ControlMessage {
           public:
            Message(RefPtr<PipelineListener> aListener,
                    const PrincipalHandle& aPrivatePrincipal)
                : ControlMessage(nullptr),
                  mListener(std::move(aListener)),
                  mPrivatePrincipal(aPrivatePrincipal) {}

            void Run() override {
              mListener->mPrincipalHandle = mPrivatePrincipal;
              mListener->mForceSilence = false;
            }

            const RefPtr<PipelineListener> mListener;
            PrincipalHandle mPrivatePrincipal;
          };

          RefPtr<nsIPrincipal> privatePrincipal =
              NullPrincipal::CreateWithInheritedAttributes(
                  mTrackSource->GetPrincipal());
          mTrackSource->SetPrincipal(privatePrincipal);

          if (mSource->IsDestroyed()) {
            return;
          }

          mSource->GraphImpl()->AppendMessage(
              MakeUnique<Message>(this, MakePrincipalHandle(privatePrincipal)));
        }));
  }

 private:
  ~PipelineListener() = default;

  void NotifyPullImpl(TrackTime aDesiredTime) {
    TRACE_COMMENT("PiplineListener::NotifyPullImpl", "PipelineListener %p",
                  this);
    uint32_t samplesPer10ms = mRate / 100;

    // mSource's rate is not necessarily the same as the graph rate, since there
    // are sample-rate constraints on the inbound audio: only 16, 32, 44.1 and
    // 48kHz are supported. The audio frames we get here is going to be
    // resampled when inserted into the graph. aDesiredTime and mPlayedTicks are
    // in the graph rate.

    while (mPlayedTicks < aDesiredTime) {
      // This fetches 10ms of data, either mono or stereo
      MediaConduitErrorCode err =
          static_cast<AudioSessionConduit*>(mConduit.get())
              ->GetAudioFrame(mRate, mAudioFrame.get());

      if (err != kMediaConduitNoError) {
        // Insert silence on conduit/GIPS failure (extremely unlikely)
        MOZ_LOG(gMediaPipelineLog, LogLevel::Error,
                ("Audio conduit failed (%d) to return data @ %" PRId64
                 " (desired %" PRId64 " -> %f)",
                 err, mPlayedTicks, aDesiredTime,
                 mSource->TrackTimeToSeconds(aDesiredTime)));
        constexpr size_t mono = 1;
        mAudioFrame->UpdateFrame(
            mAudioFrame->timestamp_, nullptr, samplesPer10ms, mRate,
            mAudioFrame->speech_type_, mAudioFrame->vad_activity_,
            std::max(mono, mAudioFrame->num_channels()));
      }

      MOZ_LOG(
          gMediaPipelineLog, LogLevel::Debug,
          ("Audio conduit returned buffer for %zu channels, %zu frames",
           mAudioFrame->num_channels(), mAudioFrame->samples_per_channel()));

      AudioSegment segment;
      if (mForceSilence || mAudioFrame->muted()) {
        segment.AppendNullData(mAudioFrame->samples_per_channel());
      } else {
        CheckedInt<size_t> bufferSize(sizeof(uint16_t));
        bufferSize *= mAudioFrame->samples_per_channel();
        bufferSize *= mAudioFrame->num_channels();
        RefPtr<SharedBuffer> samples = SharedBuffer::Create(bufferSize);
        int16_t* samplesData = static_cast<int16_t*>(samples->Data());
        AutoTArray<int16_t*, 2> channels;
        AutoTArray<const int16_t*, 2> outputChannels;

        channels.SetLength(mAudioFrame->num_channels());

        size_t offset = 0;
        for (size_t i = 0; i < mAudioFrame->num_channels(); i++) {
          channels[i] = samplesData + offset;
          offset += mAudioFrame->samples_per_channel();
        }

        DeinterleaveAndConvertBuffer(
            mAudioFrame->data(), mAudioFrame->samples_per_channel(),
            mAudioFrame->num_channels(), channels.Elements());

        outputChannels.AppendElements(channels);

        segment.AppendFrames(samples.forget(), outputChannels,
                             mAudioFrame->samples_per_channel(),
                             mPrincipalHandle);
      }

      // Handle track not actually added yet or removed/finished
      if (TrackTime appended = mSource->AppendData(&segment)) {
        mPlayedTicks += appended;
      } else {
        MOZ_LOG(gMediaPipelineLog, LogLevel::Error, ("AppendData failed"));
        // we can't un-read the data, but that's ok since we don't want to
        // buffer - but don't i-loop!
        break;
      }
    }
  }

  const RefPtr<MediaSessionConduit> mConduit;
  // This conduit's sampling rate. This is either 16, 32, 44.1 or 48kHz, and
  // tries to be the same as the graph rate. If the graph rate is higher than
  // 48kHz, mRate is capped to 48kHz. If mRate does not match the graph rate,
  // audio is resampled to the graph rate.
  const TrackRate mRate;
  const RefPtr<TaskQueue> mTaskQueue;
  // Number of frames of data that has been added to the SourceMediaTrack in
  // the graph's rate. Graph thread only.
  TrackTicks mPlayedTicks;
  // Allocation of an audio frame used as a scratch buffer when reading data out
  // of libwebrtc for forwarding into the graph. Graph thread only.
  std::unique_ptr<webrtc::AudioFrame> mAudioFrame;
  // Principal handle used when appending data to the SourceMediaTrack. Graph
  // thread only.
  PrincipalHandle mPrincipalHandle;
  // Set to true on the sts thread if privacy is requested when ALPN was
  // negotiated. Set to false again when mPrincipalHandle is private.
  Atomic<bool> mForceSilence;
};

MediaPipelineReceiveAudio::MediaPipelineReceiveAudio(
    const std::string& aPc, RefPtr<MediaTransportHandler> aTransportHandler,
    RefPtr<nsISerialEventTarget> aMainThread,
    RefPtr<AbstractThread> aCallThread, RefPtr<nsISerialEventTarget> aStsThread,
    RefPtr<AudioSessionConduit> aConduit,
    const RefPtr<dom::MediaStreamTrack>& aTrack,
    const PrincipalHandle& aPrincipalHandle)
    : MediaPipelineReceive(aPc, std::move(aTransportHandler), aMainThread,
                           std::move(aCallThread), std::move(aStsThread),
                           std::move(aConduit)),
      mListener(aTrack ? new PipelineListener(std::move(aMainThread), aTrack,
                                              mConduit, aPrincipalHandle)
                       : nullptr),
      mWatchManager(this, AbstractThread::MainThread()) {
  mDescription = mPc + "| Receive audio";
  if (mListener) {
    mListener->Init();
  }
  mWatchManager.Watch(mActive, &MediaPipelineReceiveAudio::UpdateListener);
}

void MediaPipelineReceiveAudio::Shutdown() {
  ASSERT_ON_THREAD(mMainThread);
  MediaPipeline::Shutdown();
  mWatchManager.Shutdown();
  if (mListener) {
    mListener->EndTrack();
  }
}

void MediaPipelineReceiveAudio::MakePrincipalPrivate_s() {
  ASSERT_ON_THREAD(mStsThread);
  if (mListener) {
    mListener->MakePrincipalPrivate_s();
  }
}

void MediaPipelineReceiveAudio::OnRtpPacketReceived() {
  ASSERT_ON_THREAD(mStsThread);
  if (mListener) {
    mListener->OnRtpReceived();
  }
}

void MediaPipelineReceiveAudio::UpdateListener() {
  ASSERT_ON_THREAD(mMainThread);
  if (mListener) {
    mListener->SetEnabled(mActive.Ref());
  }
}

class MediaPipelineReceiveVideo::PipelineListener
    : public GenericReceiveListener {
 public:
  PipelineListener(RefPtr<nsISerialEventTarget> aMainThread,
                   const RefPtr<dom::MediaStreamTrack>& aTrack,
                   const PrincipalHandle& aPrincipalHandle)
      : GenericReceiveListener(std::move(aMainThread), aTrack),
        mImageContainer(
            MakeAndAddRef<ImageContainer>(ImageContainer::ASYNCHRONOUS)),
        mMutex("MediaPipelineReceiveVideo::PipelineListener::mMutex"),
        mPrincipalHandle(aPrincipalHandle) {}

  void Init() { mSource->AddListener(this); }

  void MakePrincipalPrivate_s() {
    {
      MutexAutoLock lock(mMutex);
      mForceDropFrames = true;
    }

    mMainThread->Dispatch(NS_NewRunnableFunction(
        __func__, [self = RefPtr<PipelineListener>(this), this] {
          RefPtr<nsIPrincipal> privatePrincipal =
              NullPrincipal::CreateWithInheritedAttributes(
                  mTrackSource->GetPrincipal());
          mTrackSource->SetPrincipal(privatePrincipal);

          MutexAutoLock lock(mMutex);
          mPrincipalHandle = MakePrincipalHandle(privatePrincipal);
          mForceDropFrames = false;
        }));
  }

  void RenderVideoFrame(const webrtc::VideoFrameBuffer& aBuffer,
                        uint32_t aTimeStamp, int64_t aRenderTime) {
    PrincipalHandle principal;
    {
      MutexAutoLock lock(mMutex);
      if (mForceDropFrames) {
        return;
      }
      principal = mPrincipalHandle;
    }
    RefPtr<Image> image;
    if (aBuffer.type() == webrtc::VideoFrameBuffer::Type::kNative) {
      // We assume that only native handles are used with the
      // WebrtcMediaDataCodec decoder.
      const ImageBuffer* imageBuffer =
          static_cast<const ImageBuffer*>(&aBuffer);
      image = imageBuffer->GetNativeImage();
    } else {
      MOZ_ASSERT(aBuffer.type() == webrtc::VideoFrameBuffer::Type::kI420);
      rtc::scoped_refptr<const webrtc::I420BufferInterface> i420 =
          aBuffer.GetI420();

      MOZ_ASSERT(i420->DataY());
      // Create a video frame using |buffer|.
      RefPtr<PlanarYCbCrImage> yuvImage =
          mImageContainer->CreatePlanarYCbCrImage();

      PlanarYCbCrData yuvData;
      yuvData.mYChannel = const_cast<uint8_t*>(i420->DataY());
      yuvData.mYSize = IntSize(i420->width(), i420->height());
      yuvData.mYStride = i420->StrideY();
      MOZ_ASSERT(i420->StrideU() == i420->StrideV());
      yuvData.mCbCrStride = i420->StrideU();
      yuvData.mCbChannel = const_cast<uint8_t*>(i420->DataU());
      yuvData.mCrChannel = const_cast<uint8_t*>(i420->DataV());
      yuvData.mCbCrSize =
          IntSize((i420->width() + 1) >> 1, (i420->height() + 1) >> 1);
      yuvData.mPicX = 0;
      yuvData.mPicY = 0;
      yuvData.mPicSize = IntSize(i420->width(), i420->height());
      yuvData.mStereoMode = StereoMode::MONO;
      // This isn't the best default.
      yuvData.mYUVColorSpace = gfx::YUVColorSpace::BT601;

      if (!yuvImage->CopyData(yuvData)) {
        MOZ_ASSERT(false);
        return;
      }

      image = std::move(yuvImage);
    }

    VideoSegment segment;
    auto size = image->GetSize();
    segment.AppendFrame(image.forget(), size, principal);
    mSource->AppendData(&segment);
  }

 private:
  RefPtr<layers::ImageContainer> mImageContainer;
  Mutex mMutex;  // Protects the below members.
  PrincipalHandle mPrincipalHandle;
  // Set to true on the sts thread if privacy is requested when ALPN was
  // negotiated. Set to false again when mPrincipalHandle is private.
  bool mForceDropFrames = false;
};

class MediaPipelineReceiveVideo::PipelineRenderer
    : public mozilla::VideoRenderer {
 public:
  explicit PipelineRenderer(MediaPipelineReceiveVideo* aPipeline)
      : mPipeline(aPipeline) {}

  void Detach() { mPipeline = nullptr; }

  // Implement VideoRenderer
  void FrameSizeChange(unsigned int aWidth, unsigned int aHeight) override {}
  void RenderVideoFrame(const webrtc::VideoFrameBuffer& aBuffer,
                        uint32_t aTimeStamp, int64_t aRenderTime) override {
    mPipeline->mListener->RenderVideoFrame(aBuffer, aTimeStamp, aRenderTime);
  }

 private:
  MediaPipelineReceiveVideo* mPipeline;  // Raw pointer to avoid cycles
};

MediaPipelineReceiveVideo::MediaPipelineReceiveVideo(
    const std::string& aPc, RefPtr<MediaTransportHandler> aTransportHandler,
    RefPtr<nsISerialEventTarget> aMainThread,
    RefPtr<AbstractThread> aCallThread, RefPtr<nsISerialEventTarget> aStsThread,
    RefPtr<VideoSessionConduit> aConduit,
    const RefPtr<dom::MediaStreamTrack>& aTrack,
    const PrincipalHandle& aPrincipalHandle)
    : MediaPipelineReceive(aPc, std::move(aTransportHandler), aMainThread,
                           std::move(aCallThread), std::move(aStsThread),
                           std::move(aConduit)),
      mRenderer(new PipelineRenderer(this)),
      mListener(aTrack ? new PipelineListener(std::move(aMainThread), aTrack,
                                              aPrincipalHandle)
                       : nullptr),
      mWatchManager(this, AbstractThread::MainThread()) {
  mDescription = mPc + "| Receive video";
  if (mListener) {
    mListener->Init();
  }
  static_cast<VideoSessionConduit*>(mConduit.get())->AttachRenderer(mRenderer);
  mWatchManager.Watch(mActive, &MediaPipelineReceiveVideo::UpdateListener);
}

void MediaPipelineReceiveVideo::Shutdown() {
  ASSERT_ON_THREAD(mMainThread);
  MediaPipeline::Shutdown();
  mWatchManager.Shutdown();

  // stop generating video and thus stop invoking the PipelineRenderer
  // and PipelineListener - the renderer has a raw ptr to the Pipeline to
  // avoid cycles, and the render callbacks are invoked from a different
  // thread so simple null-checks would cause TSAN bugs without locks.
  static_cast<VideoSessionConduit*>(mConduit.get())->DetachRenderer();
  if (mListener) {
    mListener->EndTrack();
  }
}

void MediaPipelineReceiveVideo::MakePrincipalPrivate_s() {
  ASSERT_ON_THREAD(mStsThread);
  if (mListener) {
    mListener->MakePrincipalPrivate_s();
  }
}

void MediaPipelineReceiveVideo::OnRtpPacketReceived() {
  ASSERT_ON_THREAD(mStsThread);
  if (mListener) {
    mListener->OnRtpReceived();
  }
}

void MediaPipelineReceiveVideo::UpdateListener() {
  ASSERT_ON_THREAD(mMainThread);
  if (mListener) {
    mListener->SetEnabled(mActive.Ref());
  }
}

DOMHighResTimeStamp MediaPipeline::GetNow() const { return mConduit->GetNow(); }

DOMHighResTimeStamp MediaPipeline::RtpCSRCStats::GetExpiryFromTime(
    const DOMHighResTimeStamp aTime) {
  // DOMHighResTimeStamp is a unit measured in ms
  return aTime + EXPIRY_TIME_MILLISECONDS;
}

MediaPipeline::RtpCSRCStats::RtpCSRCStats(const uint32_t aCsrc,
                                          const DOMHighResTimeStamp aTime)
    : mCsrc(aCsrc), mTimestamp(aTime) {}

void MediaPipeline::RtpCSRCStats::GetWebidlInstance(
    dom::RTCRTPContributingSourceStats& aWebidlObj,
    const nsString& aInboundRtpStreamId) const {
  nsString statId = u"csrc_"_ns + aInboundRtpStreamId;
  statId.AppendLiteral("_");
  statId.AppendInt(mCsrc);
  aWebidlObj.mId.Construct(statId);
  aWebidlObj.mType.Construct(RTCStatsType::Csrc);
  aWebidlObj.mTimestamp.Construct(mTimestamp);
  aWebidlObj.mContributorSsrc.Construct(mCsrc);
  aWebidlObj.mInboundRtpStreamId.Construct(aInboundRtpStreamId);
}

}  // namespace mozilla
