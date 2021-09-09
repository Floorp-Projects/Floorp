/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioConduit.h"

#include "common/browser_logging/CSFLog.h"
#include "MediaConduitControl.h"
#include "mozilla/media/MediaUtils.h"
#include "mozilla/Telemetry.h"
#include "transport/runnable_utils.h"
#include "transport/SrtpFlow.h"  // For SRTP_MAX_EXPANSION
#include "WebrtcCallWrapper.h"

// libwebrtc includes
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "audio/audio_receive_stream.h"
#include "media/base/media_constants.h"

// for ntohs
#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#elif defined XP_WIN
#  include <winsock2.h>
#endif

#ifdef MOZ_WIDGET_ANDROID
#  include "AndroidBridge.h"
#endif

namespace mozilla {

namespace {

static const char* acLogTag = "WebrtcAudioSessionConduit";
#ifdef LOGTAG
#  undef LOGTAG
#endif
#define LOGTAG acLogTag

using namespace cricket;
using LocalDirection = MediaSessionConduitLocalDirection;

const char kCodecParamCbr[] = "cbr";

}  // namespace

/**
 * Factory Method for AudioConduit
 */
RefPtr<AudioSessionConduit> AudioSessionConduit::Create(
    RefPtr<WebrtcCallWrapper> aCall,
    nsCOMPtr<nsISerialEventTarget> aStsThread) {
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());

  return MakeRefPtr<WebrtcAudioConduit>(std::move(aCall),
                                        std::move(aStsThread));
}

#define INIT_MIRROR(name, val) \
  name(aCallThread, val, "WebrtcAudioConduit::Control::" #name " (Mirror)")
WebrtcAudioConduit::Control::Control(const RefPtr<AbstractThread>& aCallThread)
    : INIT_MIRROR(mReceiving, false),
      INIT_MIRROR(mTransmitting, false),
      INIT_MIRROR(mLocalSsrcs, Ssrcs()),
      INIT_MIRROR(mLocalCname, std::string()),
      INIT_MIRROR(mLocalMid, std::string()),
      INIT_MIRROR(mRemoteSsrc, 0),
      INIT_MIRROR(mSyncGroup, std::string()),
      INIT_MIRROR(mLocalRecvRtpExtensions, RtpExtList()),
      INIT_MIRROR(mLocalSendRtpExtensions, RtpExtList()),
      INIT_MIRROR(mSendCodec, Nothing()),
      INIT_MIRROR(mRecvCodecs, std::vector<AudioCodecConfig>()) {}
#undef INIT_MIRROR

void WebrtcAudioConduit::Shutdown() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  mControl.mReceiving.DisconnectIfConnected();
  mControl.mTransmitting.DisconnectIfConnected();
  mControl.mLocalSsrcs.DisconnectIfConnected();
  mControl.mLocalCname.DisconnectIfConnected();
  mControl.mLocalMid.DisconnectIfConnected();
  mControl.mRemoteSsrc.DisconnectIfConnected();
  mControl.mSyncGroup.DisconnectIfConnected();
  mControl.mLocalRecvRtpExtensions.DisconnectIfConnected();
  mControl.mLocalSendRtpExtensions.DisconnectIfConnected();
  mControl.mSendCodec.DisconnectIfConnected();
  mControl.mRecvCodecs.DisconnectIfConnected();
  mControl.mOnDtmfEventListener.DisconnectIfExists();
  mWatchManager.Shutdown();

  AutoWriteLock lock(mLock);
  DeleteSendStream();
  DeleteRecvStream();
}

WebrtcAudioConduit::WebrtcAudioConduit(
    RefPtr<WebrtcCallWrapper> aCall, nsCOMPtr<nsISerialEventTarget> aStsThread)
    : mCall(std::move(aCall)),
      mSendTransport(this),
      mRecvTransport(this),
      mRecvStreamConfig(),
      mRecvStream(nullptr),
      mSendStreamConfig(&mSendTransport),
      mSendStream(nullptr),
      mSendStreamRunning(false),
      mRecvStreamRunning(false),
      mDtmfEnabled(false),
      mLock("WebrtcAudioConduit::mLock"),
      mCallThread(std::move(mCall->mCallThread)),
      mStsThread(std::move(aStsThread)),
      mControl(mCall->mCallThread),
      mWatchManager(this, mCall->mCallThread) {
  mRecvStreamConfig.rtcp_send_transport = &mRecvTransport;
  mRecvStreamConfig.rtp.rtcp_event_observer = this;
}

/**
 * Destruction defines for our super-classes
 */
WebrtcAudioConduit::~WebrtcAudioConduit() {
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);
  MOZ_ASSERT(!mSendStream && !mRecvStream,
             "Call DeleteStreams prior to ~WebrtcAudioConduit.");
}

#define CONNECT(aCanonical, aMirror)                                          \
  do {                                                                        \
    (aMirror).Connect(aCanonical);                                            \
    mWatchManager.Watch(aMirror, &WebrtcAudioConduit::OnControlConfigChange); \
  } while (0)

void WebrtcAudioConduit::InitControl(AudioConduitControlInterface* aControl) {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());

  CONNECT(aControl->CanonicalReceiving(), mControl.mReceiving);
  CONNECT(aControl->CanonicalTransmitting(), mControl.mTransmitting);
  CONNECT(aControl->CanonicalLocalSsrcs(), mControl.mLocalSsrcs);
  CONNECT(aControl->CanonicalLocalCname(), mControl.mLocalCname);
  CONNECT(aControl->CanonicalLocalMid(), mControl.mLocalMid);
  CONNECT(aControl->CanonicalRemoteSsrc(), mControl.mRemoteSsrc);
  CONNECT(aControl->CanonicalSyncGroup(), mControl.mSyncGroup);
  CONNECT(aControl->CanonicalLocalRecvRtpExtensions(),
          mControl.mLocalRecvRtpExtensions);
  CONNECT(aControl->CanonicalLocalSendRtpExtensions(),
          mControl.mLocalSendRtpExtensions);
  CONNECT(aControl->CanonicalAudioSendCodec(), mControl.mSendCodec);
  CONNECT(aControl->CanonicalAudioRecvCodecs(), mControl.mRecvCodecs);
  mControl.mOnDtmfEventListener = aControl->OnDtmfEvent().Connect(
      mCall->mCallThread, this, &WebrtcAudioConduit::OnDtmfEvent);
}

#undef CONNECT

void WebrtcAudioConduit::OnDtmfEvent(const DtmfEvent& aEvent) {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  MOZ_ASSERT(mSendStream);
  MOZ_ASSERT(mDtmfEnabled);
  mSendStream->SendTelephoneEvent(aEvent.mPayloadType, aEvent.mPayloadFrequency,
                                  aEvent.mEventCode, aEvent.mLengthMs);
}

void WebrtcAudioConduit::OnControlConfigChange() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());

  bool recvStreamReconfigureNeeded = false;
  bool sendStreamReconfigureNeeded = false;
  bool recvStreamRecreationNeeded = false;
  bool sendStreamRecreationNeeded = false;

  if (!mControl.mLocalSsrcs.Ref().empty()) {
    if (mControl.mLocalSsrcs.Ref()[0] != mSendStreamConfig.rtp.ssrc) {
      sendStreamRecreationNeeded = true;

      // For now...
      recvStreamRecreationNeeded = true;
    }
    mRecvStreamConfig.rtp.local_ssrc = mControl.mLocalSsrcs.Ref()[0];
    mSendStreamConfig.rtp.ssrc = mControl.mLocalSsrcs.Ref()[0];

    // In the future we can do this instead of recreating the recv stream:
    // if (mRecvStream) {
    //   mCall->Call()->OnLocalSsrcUpdated(mRecvStream,
    //                                     mControl.mLocalSsrcs.Ref()[0]);
    // }
  }

  if (mControl.mLocalCname.Ref() != mSendStreamConfig.rtp.c_name) {
    mSendStreamConfig.rtp.c_name = mControl.mLocalCname.Ref();
    sendStreamReconfigureNeeded = true;
  }

  if (mControl.mLocalMid.Ref() != mSendStreamConfig.rtp.mid) {
    mSendStreamConfig.rtp.mid = mControl.mLocalMid.Ref();
    sendStreamReconfigureNeeded = true;
  }

  if (mControl.mRemoteSsrc.Ref() != mControl.mConfiguredRemoteSsrc) {
    mRecvStreamConfig.rtp.remote_ssrc = mControl.mConfiguredRemoteSsrc =
        mControl.mRemoteSsrc.Ref();
    recvStreamRecreationNeeded = true;
  }

  if (mControl.mSyncGroup.Ref() != mRecvStreamConfig.sync_group) {
    mRecvStreamConfig.sync_group = mControl.mSyncGroup.Ref();
    // For now...
    recvStreamRecreationNeeded = true;
    // In the future we can do this instead of recreating the recv stream:
    // if (mRecvStream) {
    //   mCall->Call()->OnUpdateSyncGroup(mRecvStream,
    //                                    mRecvStreamConfig.sync_group);
    // }
  }

  if (auto filteredExtensions = FilterExtensions(
          LocalDirection::kRecv, mControl.mLocalRecvRtpExtensions);
      filteredExtensions != mRecvStreamConfig.rtp.extensions) {
    mRecvStreamConfig.rtp.extensions = std::move(filteredExtensions);
    // For now...
    recvStreamRecreationNeeded = true;
    // In the future we can do this instead of recreating the recv stream:
    // if (mRecvStream) {
    //  mRecvStream->SetRtpExtensions(mRecvStreamConfig.rtp.extensions);
    //}
  }

  if (auto filteredExtensions = FilterExtensions(
          LocalDirection::kSend, mControl.mLocalSendRtpExtensions);
      filteredExtensions != mSendStreamConfig.rtp.extensions) {
    mSendStreamConfig.rtp.extensions = std::move(filteredExtensions);
    sendStreamReconfigureNeeded = true;
  }

  mControl.mSendCodec.Ref().apply([&](const auto& aConfig) {
    if (mControl.mConfiguredSendCodec != mControl.mSendCodec.Ref()) {
      mControl.mConfiguredSendCodec = mControl.mSendCodec;
      if (ValidateCodecConfig(aConfig, true) == kMediaConduitNoError) {
        mSendStreamConfig.encoder_factory =
            webrtc::CreateBuiltinAudioEncoderFactory();

        webrtc::AudioSendStream::Config::SendCodecSpec spec(
            aConfig.mType, CodecConfigToLibwebrtcFormat(aConfig));
        mSendStreamConfig.send_codec_spec = spec;

        mDtmfEnabled = aConfig.mDtmfEnabled;
        sendStreamReconfigureNeeded = true;
      }
    }
  });

  if (mControl.mConfiguredRecvCodecs != mControl.mRecvCodecs.Ref()) {
    mControl.mConfiguredRecvCodecs = mControl.mRecvCodecs;
    mRecvStreamConfig.decoder_factory = mCall->mAudioDecoderFactory;
    mRecvStreamConfig.decoder_map.clear();

    for (const auto& codec : mControl.mRecvCodecs.Ref()) {
      if (ValidateCodecConfig(codec, false) != kMediaConduitNoError) {
        continue;
      }
      mRecvStreamConfig.decoder_map.emplace(
          codec.mType, CodecConfigToLibwebrtcFormat(codec));
    }

    recvStreamReconfigureNeeded = true;
  }

  if (!recvStreamReconfigureNeeded && !sendStreamReconfigureNeeded &&
      !recvStreamRecreationNeeded && !sendStreamRecreationNeeded &&
      mControl.mReceiving == mRecvStreamRunning &&
      mControl.mTransmitting == mSendStreamRunning) {
    // No changes applied -- no need to lock.
    return;
  }

  // Recreate/Stop/Start streams as needed.
  AutoWriteLock lock(mLock);
  if (mRecvStream) {
    if (recvStreamRecreationNeeded) {
      DeleteRecvStream();
      CreateRecvStream();
    } else if (recvStreamReconfigureNeeded) {
      mRecvStream->Reconfigure(mRecvStreamConfig);
    }
  }
  if (mSendStream) {
    if (sendStreamRecreationNeeded) {
      DeleteSendStream();
      CreateSendStream();
    } else if (sendStreamReconfigureNeeded) {
      mSendStream->Reconfigure(mSendStreamConfig);
    }
  }

  if (!mControl.mReceiving) {
    StopReceivingLocked();
  }
  if (!mControl.mTransmitting) {
    StopTransmittingLocked();
  }

  if (mControl.mReceiving) {
    StartReceivingLocked();
  }
  if (mControl.mTransmitting) {
    StartTransmittingLocked();
  }
}

std::vector<uint32_t> WebrtcAudioConduit::GetLocalSSRCs() const {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  return std::vector<uint32_t>(1, mRecvStreamConfig.rtp.local_ssrc);
}

bool WebrtcAudioConduit::OverrideRemoteSSRC(uint32_t ssrc) {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());

  if (mRecvStreamConfig.rtp.remote_ssrc == ssrc) {
    return true;
  }
  mRecvStreamConfig.rtp.remote_ssrc = ssrc;

  AutoWriteLock lock(mLock);
  bool wasReceiving = mRecvStreamRunning;
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

bool WebrtcAudioConduit::GetRemoteSSRC(uint32_t* ssrc) const {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  if (!mRecvStream) {
    return false;
  }
  *ssrc = mRecvStreamConfig.rtp.remote_ssrc;
  return true;
}

Maybe<webrtc::AudioReceiveStream::Stats> WebrtcAudioConduit::GetReceiverStats()
    const {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  if (!mRecvStream) {
    return Nothing();
  }
  return Some(mRecvStream->GetStats());
}

Maybe<webrtc::AudioSendStream::Stats> WebrtcAudioConduit::GetSenderStats()
    const {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  if (!mSendStream) {
    return Nothing();
  }
  return Some(mSendStream->GetStats());
}

webrtc::Call::Stats WebrtcAudioConduit::GetCallStats() const {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  return mCall->Call()->GetStats();
}

void WebrtcAudioConduit::OnRtcpBye() { mRtcpByeEvent.Notify(); }

void WebrtcAudioConduit::OnRtcpTimeout() { mRtcpTimeoutEvent.Notify(); }

// AudioSessionConduit Implementation
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

  // This is the AudioProxyThread, blocking it for a bit is fine.
  AutoReadLock lock(mLock);
  if (!mSendStreamRunning) {
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

  // If the lock is taken, skip this chunk to avoid blocking the audio thread.
  AutoTryReadLock tryLock(mLock);
  if (!tryLock) {
    CSFLogError(LOGTAG, "%s Conduit going through negotiation ", __FUNCTION__);
    return kMediaConduitPlayoutError;
  }

  // Conduit should have reception enabled before we ask for decoded
  // samples
  if (!mRecvStreamRunning) {
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
void WebrtcAudioConduit::OnRtpReceived(MediaPacket&& aPacket,
                                       webrtc::RTPHeader&& aHeader) {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());

  if (mRecvStreamConfig.rtp.remote_ssrc != aHeader.ssrc) {
    CSFLogDebug(LOGTAG, "%s: switching from SSRC %u to %u", __FUNCTION__,
                mRecvStreamConfig.rtp.remote_ssrc, aHeader.ssrc);
    OverrideRemoteSSRC(aHeader.ssrc);
  }

  CSFLogVerbose(LOGTAG, "%s: seq# %u, Len %zu, SSRC %u (0x%x) ", __FUNCTION__,
                (uint16_t)ntohs(((uint16_t*)aPacket.data())[1]), aPacket.len(),
                (uint32_t)ntohl(((uint32_t*)aPacket.data())[2]),
                (uint32_t)ntohl(((uint32_t*)aPacket.data())[2]));

  DeliverPacket(rtc::CopyOnWriteBuffer(aPacket.data(), aPacket.len()),
                PacketType::RTP);
}

void WebrtcAudioConduit::OnRtcpReceived(MediaPacket&& aPacket) {
  CSFLogDebug(LOGTAG, "%s", __FUNCTION__);
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());

  DeliverPacket(rtc::CopyOnWriteBuffer(aPacket.data(), aPacket.len()),
                PacketType::RTCP);

  // TODO(bug 1496533): We will need to keep separate timestamps for each SSRC,
  // and for each SSRC we will need to keep a timestamp for SR and RR.
  mLastRtcpReceived = Some(GetNow());
}

// TODO(bug 1496533): We will need to add a type (ie; SR or RR) param here, or
// perhaps break this function into two functions, one for each type.
Maybe<DOMHighResTimeStamp> WebrtcAudioConduit::LastRtcpReceived() const {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  return mLastRtcpReceived;
}

DOMHighResTimeStamp WebrtcAudioConduit::GetNow() const {
  return mCall->GetNow();
}

MediaConduitErrorCode WebrtcAudioConduit::StopTransmittingLocked() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  MOZ_ASSERT(mLock.LockedForWritingByCurrentThread());

  if (mSendStreamRunning) {
    MOZ_ASSERT(mSendStream);
    CSFLogDebug(LOGTAG, "%s Engine Already Sending. Attemping to Stop ",
                __FUNCTION__);
    mSendStream->Stop();
    mSendStreamRunning = false;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::StartTransmittingLocked() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  MOZ_ASSERT(mLock.LockedForWritingByCurrentThread());

  if (mSendStreamRunning) {
    return kMediaConduitNoError;
  }

  if (!mSendStream) {
    CreateSendStream();
  }

  mCall->Call()->SignalChannelNetworkState(webrtc::MediaType::AUDIO,
                                           webrtc::kNetworkUp);
  mSendStream->Start();
  mSendStreamRunning = true;

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::StopReceivingLocked() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  MOZ_ASSERT(mLock.LockedForWritingByCurrentThread());

  if (mRecvStreamRunning) {
    MOZ_ASSERT(mRecvStream);
    mRecvStream->Stop();
    mRecvStreamRunning = false;
  }

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcAudioConduit::StartReceivingLocked() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  MOZ_ASSERT(mLock.LockedForWritingByCurrentThread());

  if (mRecvStreamRunning) {
    return kMediaConduitNoError;
  }

  if (!mRecvStream) {
    CreateRecvStream();
  }

  mCall->Call()->SignalChannelNetworkState(webrtc::MediaType::AUDIO,
                                           webrtc::kNetworkUp);
  mRecvStream->Start();
  mRecvStreamRunning = true;

  return kMediaConduitNoError;
}

bool WebrtcAudioConduit::SendRtp(const uint8_t* aData, size_t aLength,
                                 const webrtc::PacketOptions& aOptions) {
  CSFLogVerbose(
      LOGTAG,
      "AudioConduit %p: Sending RTP Packet seq# %u, len %zu, SSRC %u (0x%x)",
      this, (uint16_t)ntohs(*((uint16_t*)&aData[2])), aLength,
      (uint32_t)ntohl(*((uint32_t*)&aData[8])),
      (uint32_t)ntohl(*((uint32_t*)&aData[8])));

  if (!mTransportActive) {
    CSFLogError(LOGTAG, "AudioConduit %p: RTP Packet Send Failed ", this);
    return false;
  }

  MediaPacket packet;
  packet.Copy(aData, aLength, aLength + SRTP_MAX_EXPANSION);
  packet.SetType(MediaPacket::RTP);
  mSenderRtpSendEvent.Notify(std::move(packet));

  if (aOptions.packet_id >= 0) {
    int64_t now_ms = PR_Now() / 1000;
    MOZ_ALWAYS_SUCCEEDS(mCallThread->Dispatch(NS_NewRunnableFunction(
        __func__, [call = mCall, packet_id = aOptions.packet_id, now_ms] {
          if (call->Call()) {
            call->Call()->OnSentPacket({packet_id, now_ms});
          }
        })));
  }
  return true;
}

bool WebrtcAudioConduit::SendSenderRtcp(const uint8_t* aData, size_t aLength) {
  CSFLogVerbose(
      LOGTAG,
      "AudioConduit %p: Sending RTCP SR Packet, len %zu, SSRC %u (0x%x)", this,
      aLength, (uint32_t)ntohl(*((uint32_t*)&aData[4])),
      (uint32_t)ntohl(*((uint32_t*)&aData[4])));

  if (!mTransportActive) {
    CSFLogError(LOGTAG, "%s RTCP SR Packet Send Failed ", __FUNCTION__);
    return false;
  }

  MediaPacket packet;
  packet.Copy(aData, aLength, aLength + SRTP_MAX_EXPANSION);
  packet.SetType(MediaPacket::RTCP);
  mSenderRtcpSendEvent.Notify(std::move(packet));
  return true;
}

bool WebrtcAudioConduit::SendReceiverRtcp(const uint8_t* aData,
                                          size_t aLength) {
  CSFLogVerbose(
      LOGTAG,
      "AudioConduit %p: Sending RTCP RR Packet, len %zu, SSRC %u (0x%x)", this,
      aLength, (uint32_t)ntohl(*((uint32_t*)&aData[4])),
      (uint32_t)ntohl(*((uint32_t*)&aData[4])));

  if (!mTransportActive) {
    CSFLogError(LOGTAG, "AudioConduit %p: RTCP RR Packet Send Failed", this);
    return false;
  }

  MediaPacket packet;
  packet.Copy(aData, aLength, aLength + SRTP_MAX_EXPANSION);
  packet.SetType(MediaPacket::RTCP);
  mReceiverRtcpSendEvent.Notify(std::move(packet));
  return true;
}

/**
 * Converts between CodecConfig to WebRTC Codec Structure.
 */

bool WebrtcAudioConduit::CodecConfigToWebRTCCodec(
    const AudioCodecConfig& codecInfo,
    webrtc::AudioSendStream::Config& config) {
  config.encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();

  webrtc::AudioSendStream::Config::SendCodecSpec spec(
      codecInfo.mType, CodecConfigToLibwebrtcFormat(codecInfo));
  config.send_codec_spec = spec;

  return true;
}

/**
 *  Supported Sampling Frequencies.
 */
bool WebrtcAudioConduit::IsSamplingFreqSupported(int freq) const {
  return GetNum10msSamplesForFrequency(freq) != 0;
}

std::vector<webrtc::RtpSource> WebrtcAudioConduit::GetUpstreamRtpSources()
    const {
  MOZ_ASSERT(NS_IsMainThread());
  std::vector<webrtc::RtpSource> sources;
  {
    AutoReadLock lock(mLock);
    if (mRecvStream) {
      sources = mRecvStream->GetSources();
    }
  }
  return sources;
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
    const AudioCodecConfig& codecInfo, bool send) {
  if (codecInfo.mName.empty()) {
    CSFLogError(LOGTAG, "%s Empty Payload Name ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  // Only mono or stereo channels supported
  if ((codecInfo.mChannels != 1) && (codecInfo.mChannels != 2)) {
    CSFLogError(LOGTAG, "%s Channel Unsupported ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  return kMediaConduitNoError;
}

RtpExtList WebrtcAudioConduit::FilterExtensions(LocalDirection aDirection,
                                                const RtpExtList& aExtensions) {
  const bool isSend = aDirection == LocalDirection::kSend;
  RtpExtList filteredExtensions;

  for (const auto& extension : aExtensions) {
    // ssrc-audio-level RTP header extension
    if (extension.uri == webrtc::RtpExtension::kAudioLevelUri) {
      filteredExtensions.push_back(
          webrtc::RtpExtension(extension.uri, extension.id));
    }

    // csrc-audio-level RTP header extension
    if (extension.uri == webrtc::RtpExtension::kCsrcAudioLevelUri) {
      if (isSend) {
        continue;
      }
      filteredExtensions.push_back(
          webrtc::RtpExtension(extension.uri, extension.id));
    }

    // MID RTP header extension
    if (extension.uri == webrtc::RtpExtension::kMidUri) {
      if (!isSend) {
        // TODO: recv mid support, see also bug 1727211
        continue;
      }
      filteredExtensions.push_back(
          webrtc::RtpExtension(extension.uri, extension.id));
    }
  }

  return filteredExtensions;
}

webrtc::SdpAudioFormat WebrtcAudioConduit::CodecConfigToLibwebrtcFormat(
    const AudioCodecConfig& aConfig) {
  webrtc::SdpAudioFormat::Parameters parameters;
  if (aConfig.mName == kOpusCodecName) {
    if (aConfig.mChannels == 2) {
      parameters[kCodecParamStereo] = kParamValueTrue;
    }
    if (aConfig.mFECEnabled) {
      parameters[kCodecParamUseInbandFec] = kParamValueTrue;
    }
    if (aConfig.mDTXEnabled) {
      parameters[kCodecParamUseDtx] = kParamValueTrue;
    }
    if (aConfig.mMaxPlaybackRate) {
      parameters[kCodecParamMaxPlaybackRate] =
          std::to_string(aConfig.mMaxPlaybackRate);
    }
    if (aConfig.mMaxAverageBitrate) {
      parameters[kCodecParamMaxAverageBitrate] =
          std::to_string(aConfig.mMaxAverageBitrate);
    }
    if (aConfig.mFrameSizeMs) {
      parameters[kCodecParamPTime] = std::to_string(aConfig.mFrameSizeMs);
    }
    if (aConfig.mMinFrameSizeMs) {
      parameters[kCodecParamMinPTime] = std::to_string(aConfig.mMinFrameSizeMs);
    }
    if (aConfig.mMaxFrameSizeMs) {
      parameters[kCodecParamMaxPTime] = std::to_string(aConfig.mMaxFrameSizeMs);
    }
    if (aConfig.mCbrEnabled) {
      parameters["cbr"] = kParamValueTrue;
    }
  }

  return webrtc::SdpAudioFormat(aConfig.mName, aConfig.mFreq, aConfig.mChannels,
                                parameters);
}

void WebrtcAudioConduit::DeleteSendStream() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  MOZ_ASSERT(mLock.LockedForWritingByCurrentThread());
  if (mSendStream) {
    mCall->Call()->DestroyAudioSendStream(mSendStream);
    mSendStreamRunning = false;
    mSendStream = nullptr;
  }
}

MediaConduitErrorCode WebrtcAudioConduit::CreateSendStream() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  MOZ_ASSERT(mLock.LockedForWritingByCurrentThread());

  mSendStream = mCall->Call()->CreateAudioSendStream(mSendStreamConfig);
  if (!mSendStream) {
    return kMediaConduitUnknownError;
  }

  return kMediaConduitNoError;
}

void WebrtcAudioConduit::DeleteRecvStream() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  MOZ_ASSERT(mLock.LockedForWritingByCurrentThread());
  if (mRecvStream) {
    mCall->Call()->DestroyAudioReceiveStream(mRecvStream);
    mRecvStreamRunning = false;
    mRecvStream = nullptr;
  }
}

MediaConduitErrorCode WebrtcAudioConduit::CreateRecvStream() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  MOZ_ASSERT(mLock.LockedForWritingByCurrentThread());

  mRecvStream = mCall->Call()->CreateAudioReceiveStream(mRecvStreamConfig);
  if (!mRecvStream) {
    return kMediaConduitUnknownError;
  }

  return kMediaConduitNoError;
}

void WebrtcAudioConduit::DeliverPacket(rtc::CopyOnWriteBuffer packet,
                                       PacketType type) {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());

  if (!mCall->Call()) {
    return;
  }

  // Bug 1499796 - we need to get passed the time the packet was received
  webrtc::PacketReceiver::DeliveryStatus status =
      mCall->Call()->Receiver()->DeliverPacket(webrtc::MediaType::AUDIO,
                                               std::move(packet), -1);

  if (status != webrtc::PacketReceiver::DELIVERY_OK) {
    CSFLogError(LOGTAG, "%s DeliverPacket Failed for %s packet, %d",
                __FUNCTION__, type == PacketType::RTP ? "RTP" : "RTCP", status);
  }
}

}  // namespace mozilla
