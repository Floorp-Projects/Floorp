/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "common/browser_logging/CSFLog.h"
#include "nspr.h"

#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#elif defined XP_WIN
#  include <winsock2.h>
#endif

#include "AudioConduit.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/RTCRtpSourcesBinding.h"
#include "mozilla/media/MediaUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/Telemetry.h"
#include "transport/runnable_utils.h"

#include "pk11pub.h"

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "audio/audio_receive_stream.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "system_wrappers/include/clock.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "AndroidBridge.h"
#endif

namespace mozilla {

static const char* acLogTag = "WebrtcAudioSessionConduit";
#ifdef LOGTAG
#  undef LOGTAG
#endif
#define LOGTAG acLogTag

using LocalDirection = MediaSessionConduitLocalDirection;
/**
 * Factory Method for AudioConduit
 */
RefPtr<AudioSessionConduit> AudioSessionConduit::Create(
    RefPtr<WebRtcCallWrapper> aCall,
    nsCOMPtr<nsISerialEventTarget> aStsThread) {
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());

  return MakeRefPtr<WebrtcAudioConduit>(aCall, aStsThread);
}

/**
 * Destruction defines for our super-classes
 */
WebrtcAudioConduit::~WebrtcAudioConduit() {
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);
  DeleteSendStream();
  DeleteRecvStream();
}

bool WebrtcAudioConduit::SetLocalSSRCs(const std::vector<uint32_t>& aSSRCs,
                                       const std::vector<uint32_t>& aRtxSSRCs) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSSRCs.size() == 1,
             "WebrtcAudioConduit::SetLocalSSRCs accepts exactly 1 ssrc.");

  // We ignore aRtxSSRCs, it is only used in the VideoConduit.
  if (aSSRCs.empty()) {
    return false;
  }

  // Special case: the local SSRCs are the same - do nothing.
  if (mSendStreamConfig.rtp.ssrc == aSSRCs[0]) {
    return true;
  }

  {
    MutexAutoLock lock(mMutex);  // Avoid racing against GetLocalSSRCs
    // Update the value of the ssrcs in the config structure.
    mRecvStreamConfig.rtp.local_ssrc = aSSRCs[0];
  }

  mSendStreamConfig.rtp.ssrc = aSSRCs[0];

  if (!RecreateRecvStreamIfExists()) {
    return false;
  }

  return RecreateSendStreamIfExists();
}

std::vector<uint32_t> WebrtcAudioConduit::GetLocalSSRCs() {
  MOZ_ASSERT(NS_IsMainThread());
  return std::vector<uint32_t>(1, mRecvStreamConfig.rtp.local_ssrc);
}

bool WebrtcAudioConduit::SetRemoteSSRC(uint32_t ssrc, uint32_t rtxSsrc) {
  MOZ_ASSERT(NS_IsMainThread());

  // We ignore aRtxSsrc, it is only used in the VideoConduit.
  if (mRecvStreamConfig.rtp.remote_ssrc == ssrc) {
    return true;
  }
  mRecvStreamConfig.rtp.remote_ssrc = ssrc;

  return RecreateRecvStreamIfExists();
}

bool WebrtcAudioConduit::GetRemoteSSRC(uint32_t* ssrc) {
  {
    MutexAutoLock lock(mMutex);
    if (!mRecvStream) {
      return false;
    }

    const webrtc::AudioReceiveStream::Stats& stats = mRecvStream->GetStats();
    *ssrc = stats.remote_ssrc;
  }

  return true;
}

bool WebrtcAudioConduit::SetLocalCNAME(const char* cname) {
  MOZ_ASSERT(NS_IsMainThread());
  mSendStreamConfig.rtp.c_name = cname;
  return true;
}

bool WebrtcAudioConduit::SetLocalMID(const std::string& mid) {
  MOZ_ASSERT(NS_IsMainThread());
  mSendStreamConfig.rtp.mid = mid;
  return true;
}

void WebrtcAudioConduit::SetSyncGroup(const std::string& group) {
  MOZ_ASSERT(NS_IsMainThread());
  mRecvStreamConfig.sync_group = group;
}

Maybe<webrtc::AudioReceiveStream::Stats> WebrtcAudioConduit::GetReceiverStats()
    const {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mRecvStream) {
    return Nothing();
  }
  return Some(mRecvStream->GetStats());
}

Maybe<webrtc::AudioSendStream::Stats> WebrtcAudioConduit::GetSenderStats()
    const {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mSendStream) {
    return Nothing();
  }
  return Some(mSendStream->GetStats());
}

webrtc::Call::Stats WebrtcAudioConduit::GetCallStats() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mCall->Call()->GetStats();
}

bool WebrtcAudioConduit::SetDtmfPayloadType(unsigned char type, int freq) {
  CSFLogInfo(LOGTAG, "%s : setting dtmf payload %d", __FUNCTION__, (int)type);
  MOZ_ASSERT(NS_IsMainThread());
  mDtmfPayloadType = type;
  mDtmfPayloadFrequency = freq;

  return false;
}

bool WebrtcAudioConduit::InsertDTMFTone(int channel, int eventCode,
                                        bool outOfBand, int lengthMs,
                                        int attenuationDb) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mSendStream || !mDtmfEnabled || !outOfBand) {
    return false;
  }

  MOZ_DIAGNOSTIC_ASSERT(mDtmfPayloadType != -1);
  MOZ_DIAGNOSTIC_ASSERT(mDtmfPayloadFrequency != -1);
  return mSendStream->SendTelephoneEvent(
      mDtmfPayloadType, mDtmfPayloadFrequency, eventCode, lengthMs);
}

void WebrtcAudioConduit::SetRtcpEventObserver(
    mozilla::RtcpEventObserver* observer) {
  MOZ_ASSERT(NS_IsMainThread());
  mRtcpEventObserver = observer;
}

void WebrtcAudioConduit::GetRtpSources(
    nsTArray<dom::RTCRtpSourceEntry>& outSources) {
  MOZ_ASSERT(NS_IsMainThread());
  outSources.Clear();
  if (!mRecvStream) {
    return;
  }
  std::vector<webrtc::RtpSource> sources = mRecvStream->GetSources();
  for (const auto& source : sources) {
    dom::RTCRtpSourceEntry domEntry;
    domEntry.mSource = source.source_id();
    switch (source.source_type()) {
      case webrtc::RtpSourceType::SSRC:
        domEntry.mSourceType = dom::RTCRtpSourceEntryType::Synchronization;
        break;
      case webrtc::RtpSourceType::CSRC:
        domEntry.mSourceType = dom::RTCRtpSourceEntryType::Contributing;
        break;
      default:
        MOZ_CRASH("Unexpected RTCRtpSourceEntryType");
    }
    domEntry.mTimestamp = source.timestamp_ms();
    domEntry.mRtpTimestamp = source.rtp_timestamp();
    if (source.audio_level()) {
      if (*source.audio_level() == 127) {
        // Spec indicates that a value of 127 should be set to 0
        domEntry.mAudioLevel.Construct(0);
      } else {
        // All other values are calculated as 10^(-rfc_level/20)
        domEntry.mAudioLevel.Construct(
            std::pow(10, -*source.audio_level() / 20.0));
      }
    }
    outSources.AppendElement(std::move(domEntry));
  }
}

void WebrtcAudioConduit::InsertAudioLevelForContributingSource(
    const uint32_t aCsrcSource, const int64_t aTimestamp,
    const uint32_t aRtpTimestamp, const bool aHasAudioLevel,
    const uint8_t aAudioLevel) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mRecvStream) {
    return;
  }

  webrtc::RtpPacketInfos infos({webrtc::RtpPacketInfo(
      mRecvSSRC, {aCsrcSource}, aRtpTimestamp,
      aHasAudioLevel ? absl::optional(aAudioLevel) : absl::nullopt,
      absl::nullopt, aTimestamp)});

  mRecvStream->InsertAudioLevelForContributingSource(infos);
}

// AudioSessionConduit Implementation
MediaConduitErrorCode WebrtcAudioConduit::SetTransmitterTransport(
    RefPtr<TransportInterface> aTransport) {
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  // set the transport
  mTransmitterTransport = aTransport;
  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::SetReceiverTransport(
    RefPtr<TransportInterface> aTransport) {
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  // set the transport
  mReceiverTransport = aTransport;
  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::ConfigureSendMediaCodec(
    const AudioCodecConfig* codecConfig) {
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());

  MediaConduitErrorCode condError = kMediaConduitNoError;

  {
    // validate codec param
    if ((condError = ValidateCodecConfig(codecConfig, true)) !=
        kMediaConduitNoError) {
      return condError;
    }
  }

  condError = StopTransmitting();
  if (condError != kMediaConduitNoError) {
    return condError;
  }

  if (!CodecConfigToWebRTCCodec(codecConfig, mSendStreamConfig)) {
    CSFLogError(LOGTAG, "%s CodecConfig to WebRTC Codec Failed ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  mDtmfEnabled = codecConfig->mDtmfEnabled;

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::ConfigureRecvMediaCodecs(
    const std::vector<UniquePtr<AudioCodecConfig>>& codecConfigList) {
  MOZ_ASSERT(NS_IsMainThread());

  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);
  MediaConduitErrorCode condError = kMediaConduitNoError;
  bool success = false;

  // Are we receiving already? If so, stop receiving and playout
  // since we can't apply new recv codec when the engine is playing.
  condError = StopReceiving();
  if (condError != kMediaConduitNoError) {
    return condError;
  }

  if (codecConfigList.empty()) {
    CSFLogError(LOGTAG, "%s Zero number of codecs to configure", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  // Try Applying the codecs in the list.
  // We succeed if at least one codec was applied and reception was
  // started successfully.
  mRecvStreamConfig.decoder_factory = mCall->mDecoderFactory;
  mRecvStreamConfig.decoder_map.clear();
  for (const auto& codec : codecConfigList) {
    // if the codec param is invalid or diplicate, return error
    if ((condError = ValidateCodecConfig(codec.get(), false)) !=
        kMediaConduitNoError) {
      return condError;
    }

    webrtc::SdpAudioFormat::Parameters parameters;
    if (codec->mName == "opus") {
      if (codec->mChannels == 2) {
        parameters["stereo"] = "1";
      }
      if (codec->mFECEnabled) {
        parameters["useinbandfec"] = "1";
      }
      if (codec->mDTXEnabled) {
        parameters["usedtx"] = "1";
      }
      if (codec->mMaxPlaybackRate) {
        parameters["maxplaybackrate"] = std::to_string(codec->mMaxPlaybackRate);
      }
      if (codec->mMaxAverageBitrate) {
        parameters["maxaveragebitrate"] =
            std::to_string(codec->mMaxAverageBitrate);
      }
      if (codec->mFrameSizeMs) {
        parameters["ptime"] = std::to_string(codec->mFrameSizeMs);
      }
      if (codec->mMinFrameSizeMs) {
        parameters["minptime"] = std::to_string(codec->mMinFrameSizeMs);
      }
      if (codec->mMaxFrameSizeMs) {
        parameters["maxptime"] = std::to_string(codec->mMaxFrameSizeMs);
      }
      if (codec->mCbrEnabled) {
        parameters["cbr"] = "1";
      }
    }

    webrtc::SdpAudioFormat format(codec->mName, codec->mFreq, codec->mChannels,
                                  parameters);
    mRecvStreamConfig.decoder_map.emplace(codec->mType, format);
    success = true;
  }  // end for

  mRecvSSRC = mRecvStreamConfig.rtp.remote_ssrc;

  if (!success) {
    CSFLogError(LOGTAG, "%s Setting Receive Codec Failed ", __FUNCTION__);
    return kMediaConduitInvalidReceiveCodec;
  }

  // If we are here, at least one codec should have been set
  {
    MutexAutoLock lock(mMutex);
    DeleteRecvStream();
    condError = StartReceivingLocked();
    if (condError != kMediaConduitNoError) {
      return condError;
    }
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::SetLocalRTPExtensions(
    LocalDirection aDirection, const RtpExtList& extensions) {
  MOZ_ASSERT(NS_IsMainThread());
  CSFLogDebug(LOGTAG, "%s direction: %s", __FUNCTION__,
              MediaSessionConduit::LocalDirectionToString(aDirection).c_str());

  bool isSend = aDirection == LocalDirection::kSend;
  RtpExtList filteredExtensions;

  for (const auto& extension : extensions) {
    // ssrc-audio-level RTP header extension
    if (extension.uri == webrtc::RtpExtension::kAudioLevelUri) {
      filteredExtensions.push_back(
          webrtc::RtpExtension(extension.uri, extension.id));
    }

    // csrc-audio-level RTP header extension
    if (extension.uri == webrtc::RtpExtension::kCsrcAudioLevelUri) {
      if (isSend) {
        CSFLogError(LOGTAG,
                    "%s SetSendAudioLevelIndicationStatus Failed"
                    " can not send CSRC audio levels.",
                    __FUNCTION__);
        return kMediaConduitMalformedArgument;
      }
      filteredExtensions.push_back(
          webrtc::RtpExtension(extension.uri, extension.id));
    }

    // MID RTP header extension
    if (extension.uri == webrtc::RtpExtension::kMidUri) {
      if (!isSend) {
        // TODO: Why do we error out for csrc-audio-level, but not mid?
        // TODO: recv mid support
        continue;
      }
      filteredExtensions.push_back(
          webrtc::RtpExtension(extension.uri, extension.id));
    }
  }

  auto& currentExtensions = isSend ? mSendStreamConfig.rtp.extensions
                                   : mRecvStreamConfig.rtp.extensions;
  if (filteredExtensions == currentExtensions) {
    return kMediaConduitNoError;
  }

  currentExtensions = filteredExtensions;

  if (isSend) {
    RecreateSendStreamIfExists();
  } else {
    RecreateRecvStreamIfExists();
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::SendAudioFrame(
    std::unique_ptr<webrtc::AudioFrame> frame) {
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);
  // Following checks need to be performed
  // 1. Non null audio buffer pointer, and
  // 2. Valid sample rate, and
  // 3. Appropriate Sample Length for 10 ms audio-frame. This represents the
  //    block size used upstream for processing.
  //    Ex: for 16000 sample rate , valid block-length is 160.
  //    Similarly for 32000 sample rate, valid block length is 320.

  if (!frame->data() ||
      (IsSamplingFreqSupported(frame->sample_rate_hz()) == false) ||
      ((frame->samples_per_channel() % (frame->sample_rate_hz() / 100) != 0))) {
    CSFLogError(LOGTAG, "%s Invalid Parameters ", __FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitMalformedArgument;
  }

  // if transmission is not started .. conduit cannot insert frames
  if (!mEngineTransmitting) {
    CSFLogError(LOGTAG, "%s Engine not transmitting ", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  mSendStream->SendAudioData(std::move(frame));
  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::GetAudioFrame(
    int32_t samplingFreqHz, webrtc::AudioFrame* frame) {
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);

  // validate params
  if (!frame) {
    CSFLogError(LOGTAG, "%s Null Audio Buffer Pointer", __FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitMalformedArgument;
  }

  // Validate sample length
  if (GetNum10msSamplesForFrequency(samplingFreqHz) == 0) {
    CSFLogError(LOGTAG, "%s Invalid Sampling Frequency ", __FUNCTION__);
    MOZ_ASSERT(PR_FALSE);
    return kMediaConduitMalformedArgument;
  }

  // Conduit should have reception enabled before we ask for decoded
  // samples
  if (!mEngineReceiving) {
    CSFLogError(LOGTAG, "%s Engine not Receiving ", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }

  // Unfortunate to have to cast to an internal class, but that looks like the
  // only way short of interfacing with a layer above (which mixes all streams,
  // which we don't want) or a layer below (which we try to avoid because it is
  // less stable).
  auto info = static_cast<webrtc::internal::AudioReceiveStream*>(mRecvStream)
                  ->GetAudioFrameWithInfo(samplingFreqHz, frame);

  if (info == webrtc::AudioMixer::Source::AudioFrameInfo::kError) {
    CSFLogError(LOGTAG, "%s Getting audio frame failed", __FUNCTION__);
    return kMediaConduitPlayoutError;
  }

  CSFLogDebug(LOGTAG, "%s Got %zu channels of %zu samples", __FUNCTION__,
              frame->num_channels(), frame->samples_per_channel());
  return kMediaConduitNoError;
}

// Transport Layer Callbacks
MediaConduitErrorCode WebrtcAudioConduit::ReceivedRTPPacket(
    const void* data, int len, webrtc::RTPHeader& header) {
  ASSERT_ON_THREAD(mStsThread);

  // Handle the unknown ssrc (and ssrc-not-signaled case).
  // We can't just do this here; it has to happen on MainThread :-(
  // We also don't want to drop the packet, nor stall this thread, so we hold
  // the packet (and any following) for inserting once the SSRC is set.

  // capture packet for insertion after ssrc is set -- do this before
  // sending the runnable, since it may pull from this.  Since it
  // dispatches back to us, it's less critial to do this here, but doesn't
  // hurt.
  if (mRtpPacketQueue.IsQueueActive()) {
    mRtpPacketQueue.Enqueue(data, len);
    return kMediaConduitNoError;
  }

  if (mRecvSSRC != header.ssrc) {
    // a new switch needs to be done
    // any queued packets are from a previous switch that hasn't completed
    // yet; drop them and only process the latest SSRC
    mRtpPacketQueue.Clear();
    mRtpPacketQueue.Enqueue(data, len);

    CSFLogDebug(LOGTAG, "%s: switching from SSRC %u to %u", __FUNCTION__,
                static_cast<uint32_t>(mRecvSSRC), header.ssrc);

    // we "switch" here immediately, but buffer until the queue is released
    mRecvSSRC = header.ssrc;

    // Ensure lamba captures refs
    RefPtr<WebrtcAudioConduit> self = this;
    nsCOMPtr<nsIThread> thread;
    if (NS_WARN_IF(NS_FAILED(NS_GetCurrentThread(getter_AddRefs(thread))))) {
      return kMediaConduitRTPProcessingFailed;
    }
    NS_DispatchToMainThread(
        media::NewRunnableFrom([self, thread, ssrc = header.ssrc]() mutable {
          self->SetRemoteSSRC(ssrc, 0);
          // We want to unblock the queued packets on the original thread
          thread->Dispatch(media::NewRunnableFrom([self, ssrc]() mutable {
                             if (ssrc == self->mRecvSSRC) {
                               // SSRC is set; insert queued packets
                               self->mRtpPacketQueue.DequeueAll(self);
                             }
                             // else this is an intermediate switch; another is
                             // in-flight
                             return NS_OK;
                           }),
                           NS_DISPATCH_NORMAL);
          return NS_OK;
        }));
    return kMediaConduitNoError;
  }

  CSFLogVerbose(LOGTAG, "%s: seq# %u, Len %d, SSRC %u (0x%x) ", __FUNCTION__,
                (uint16_t)ntohs(((uint16_t*)data)[1]), len,
                (uint32_t)ntohl(((uint32_t*)data)[2]),
                (uint32_t)ntohl(((uint32_t*)data)[2]));

  if (DeliverPacket(data, len) != kMediaConduitNoError) {
    CSFLogError(LOGTAG, "%s RTP Processing Failed", __FUNCTION__);
    return kMediaConduitRTPProcessingFailed;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::ReceivedRTCPPacket(const void* data,
                                                             int len) {
  CSFLogDebug(LOGTAG, "%s", __FUNCTION__);
  ASSERT_ON_THREAD(mStsThread);

  if (DeliverPacket(data, len) != kMediaConduitNoError) {
    CSFLogError(LOGTAG, "%s RTCP Processing Failed", __FUNCTION__);
    return kMediaConduitRTPProcessingFailed;
  }

  // TODO(bug 1496533): We will need to keep separate timestamps for each SSRC,
  // and for each SSRC we will need to keep a timestamp for SR and RR.
  mLastRtcpReceived = Some(GetNow());
  return kMediaConduitNoError;
}

// TODO(bug 1496533): We will need to add a type (ie; SR or RR) param here, or
// perhaps break this function into two functions, one for each type.
Maybe<DOMHighResTimeStamp> WebrtcAudioConduit::LastRtcpReceived() const {
  ASSERT_ON_THREAD(mStsThread);
  return mLastRtcpReceived;
}

MediaConduitErrorCode WebrtcAudioConduit::StopTransmitting() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  return StopTransmittingLocked();
}

MediaConduitErrorCode WebrtcAudioConduit::StartTransmitting() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  return StartTransmittingLocked();
}

MediaConduitErrorCode WebrtcAudioConduit::StopReceiving() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  return StopReceivingLocked();
}

MediaConduitErrorCode WebrtcAudioConduit::StartReceiving() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  return StartReceivingLocked();
}

MediaConduitErrorCode WebrtcAudioConduit::StopTransmittingLocked() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  if (mEngineTransmitting) {
    MOZ_ASSERT(mSendStream);
    CSFLogDebug(LOGTAG, "%s Engine Already Sending. Attemping to Stop ",
                __FUNCTION__);
    mSendStream->Stop();
    mEngineTransmitting = false;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::StartTransmittingLocked() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  if (mEngineTransmitting) {
    return kMediaConduitNoError;
  }

  if (!mSendStream) {
    CreateSendStream();
  }

  mCall->Call()->SignalChannelNetworkState(webrtc::MediaType::AUDIO,
                                           webrtc::kNetworkUp);
  mSendStream->Start();
  mEngineTransmitting = true;

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::StopReceivingLocked() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  if (mEngineReceiving) {
    MOZ_ASSERT(mRecvStream);
    mRecvStream->Stop();
    mEngineReceiving = false;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::StartReceivingLocked() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  if (mEngineReceiving) {
    return kMediaConduitNoError;
  }

  if (!mRecvStream) {
    CreateRecvStream();
  }

  mCall->Call()->SignalChannelNetworkState(webrtc::MediaType::AUDIO,
                                           webrtc::kNetworkUp);
  mRecvStream->Start();
  mEngineReceiving = true;

  return kMediaConduitNoError;
}

// WebRTC::RTP Callback Implementation
// Called on AudioGUM or MTG thread
bool WebrtcAudioConduit::SendRtp(const uint8_t* data, size_t len,
                                 const webrtc::PacketOptions& options) {
  CSFLogDebug(LOGTAG, "%s: len %lu", __FUNCTION__, (unsigned long)len);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  if (mTransmitterTransport &&
      (mTransmitterTransport->SendRtpPacket(data, len) == NS_OK)) {
    CSFLogDebug(LOGTAG, "%s Sent RTP Packet ", __FUNCTION__);
    if (options.packet_id >= 0) {
      int64_t now_ms = PR_Now() / 1000;
      mCall->Call()->OnSentPacket({options.packet_id, now_ms});
    }
    return true;
  }
  CSFLogError(LOGTAG, "%s RTP Packet Send Failed ", __FUNCTION__);
  return false;
}

// Called on WebRTC Process thread and perhaps others
bool WebrtcAudioConduit::SendRtcp(const uint8_t* data, size_t len) {
  CSFLogDebug(LOGTAG, "%s : len %lu, first rtcp = %u ", __FUNCTION__,
              (unsigned long)len, static_cast<unsigned>(data[1]));

  // We come here if we have only one pipeline/conduit setup,
  // such as for unidirectional streams.
  // We also end up here if we are receiving
  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  if (mReceiverTransport &&
      mReceiverTransport->SendRtcpPacket(data, len) == NS_OK) {
    // Might be a sender report, might be a receiver report, we don't know.
    CSFLogDebug(LOGTAG, "%s Sent RTCP Packet ", __FUNCTION__);
    return true;
  }
  if (mTransmitterTransport &&
      (mTransmitterTransport->SendRtcpPacket(data, len) == NS_OK)) {
    CSFLogDebug(LOGTAG, "%s Sent RTCP Packet (sender report) ", __FUNCTION__);
    return true;
  }
  CSFLogError(LOGTAG, "%s RTCP Packet Send Failed ", __FUNCTION__);
  return false;
}

/**
 * Converts between CodecConfig to WebRTC Codec Structure.
 */

bool WebrtcAudioConduit::CodecConfigToWebRTCCodec(
    const AudioCodecConfig* codecInfo,
    webrtc::AudioSendStream::Config& config) {
  config.encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();

  webrtc::SdpAudioFormat::Parameters parameters;
  if (codecInfo->mName == "opus") {
    if (codecInfo->mChannels == 2) {
      parameters["stereo"] = "1";
    }
    if (codecInfo->mFECEnabled) {
      parameters["useinbandfec"] = "1";
    }
    if (codecInfo->mDTXEnabled) {
      parameters["usedtx"] = "1";
    }
    if (codecInfo->mMaxPlaybackRate) {
      parameters["maxplaybackrate"] =
          std::to_string(codecInfo->mMaxPlaybackRate);
    }
    if (codecInfo->mMaxAverageBitrate) {
      parameters["maxaveragebitrate"] =
          std::to_string(codecInfo->mMaxAverageBitrate);
    }
    if (codecInfo->mFrameSizeMs) {
      parameters["ptime"] = std::to_string(codecInfo->mFrameSizeMs);
    }
    if (codecInfo->mMinFrameSizeMs) {
      parameters["minptime"] = std::to_string(codecInfo->mMinFrameSizeMs);
    }
    if (codecInfo->mMaxFrameSizeMs) {
      parameters["maxptime"] = std::to_string(codecInfo->mMaxFrameSizeMs);
    }
    if (codecInfo->mCbrEnabled) {
      parameters["cbr"] = "1";
    }
  }

  webrtc::SdpAudioFormat format(codecInfo->mName, codecInfo->mFreq,
                                codecInfo->mChannels, parameters);
  webrtc::AudioSendStream::Config::SendCodecSpec spec(codecInfo->mType, format);
  config.send_codec_spec = spec;

  return true;
}

/**
 *  Supported Sampling Frequencies.
 */
bool WebrtcAudioConduit::IsSamplingFreqSupported(int freq) const {
  return GetNum10msSamplesForFrequency(freq) != 0;
}

/* Return block-length of 10 ms audio frame in number of samples */
unsigned int WebrtcAudioConduit::GetNum10msSamplesForFrequency(
    int samplingFreqHz) const {
  switch (samplingFreqHz) {
    case 16000:
      return 160;  // 160 samples
    case 32000:
      return 320;  // 320 samples
    case 44100:
      return 441;  // 441 samples
    case 48000:
      return 480;  // 480 samples
    default:
      return 0;  // invalid or unsupported
  }
}

/**
 * Perform validation on the codecConfig to be applied.
 * Verifies if the codec is already applied.
 */
MediaConduitErrorCode WebrtcAudioConduit::ValidateCodecConfig(
    const AudioCodecConfig* codecInfo, bool send) {
  if (!codecInfo) {
    CSFLogError(LOGTAG, "%s Null CodecConfig ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  if (codecInfo->mName.empty()) {
    CSFLogError(LOGTAG, "%s Empty Payload Name ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  // Only mono or stereo channels supported
  if ((codecInfo->mChannels != 1) && (codecInfo->mChannels != 2)) {
    CSFLogError(LOGTAG, "%s Channel Unsupported ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  return kMediaConduitNoError;
}

void WebrtcAudioConduit::DeleteSendStream() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();
  if (mSendStream) {
    mSendStream->Stop();
    mEngineTransmitting = false;
    mCall->Call()->DestroyAudioSendStream(mSendStream);
    mSendStream = nullptr;
  }
}

MediaConduitErrorCode WebrtcAudioConduit::CreateSendStream() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  mSendStream = mCall->Call()->CreateAudioSendStream(mSendStreamConfig);
  if (!mSendStream) {
    return kMediaConduitUnknownError;
  }

  return kMediaConduitNoError;
}

void WebrtcAudioConduit::DeleteRecvStream() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();
  if (mRecvStream) {
    mRecvStream->Stop();
    mEngineReceiving = false;
    mCall->Call()->DestroyAudioReceiveStream(mRecvStream);
    mRecvStream = nullptr;
  }
}

MediaConduitErrorCode WebrtcAudioConduit::CreateRecvStream() {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  mRecvStreamConfig.rtcp_send_transport = this;
  mRecvStream = mCall->Call()->CreateAudioReceiveStream(mRecvStreamConfig);
  if (!mRecvStream) {
    return kMediaConduitUnknownError;
  }

  return kMediaConduitNoError;
}

bool WebrtcAudioConduit::RecreateSendStreamIfExists() {
  MutexAutoLock lock(mMutex);
  bool wasTransmitting = mEngineTransmitting;
  bool hadSendStream = mSendStream;
  DeleteSendStream();

  if (wasTransmitting) {
    if (StartTransmittingLocked() != kMediaConduitNoError) {
      return false;
    }
  } else if (hadSendStream) {
    if (CreateSendStream() != kMediaConduitNoError) {
      return false;
    }
  }
  return true;
}

bool WebrtcAudioConduit::RecreateRecvStreamIfExists() {
  MutexAutoLock lock(mMutex);
  bool wasReceiving = mEngineReceiving;
  bool hadRecvStream = mRecvStream;
  DeleteRecvStream();

  if (wasReceiving) {
    if (StartReceivingLocked() != kMediaConduitNoError) {
      return false;
    }
  } else if (hadRecvStream) {
    if (CreateRecvStream() != kMediaConduitNoError) {
      return false;
    }
  }
  return true;
}

MediaConduitErrorCode WebrtcAudioConduit::DeliverPacket(const void* data,
                                                        int len) {
  // Bug 1499796 - we need to get passed the time the packet was received
  webrtc::PacketReceiver::DeliveryStatus status =
      mCall->Call()->Receiver()->DeliverPacket(
          webrtc::MediaType::AUDIO,
          rtc::CopyOnWriteBuffer(static_cast<const uint8_t*>(data), len), -1);

  if (status != webrtc::PacketReceiver::DELIVERY_OK) {
    CSFLogError(LOGTAG, "%s DeliverPacket Failed, %d", __FUNCTION__, status);
    return kMediaConduitRTPProcessingFailed;
  }

  return kMediaConduitNoError;
}

}  // namespace mozilla
