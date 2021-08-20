/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoConduit.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>

#include "common/browser_logging/CSFLog.h"
#include "common/YuvStamper.h"
#include "GmpVideoCodec.h"
#include "MediaConduitControl.h"
#include "MediaDataCodec.h"
#include "mozilla/dom/RTCRtpSourcesBinding.h"
#include "mozilla/media/MediaUtils.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/TemplateLib.h"
#include "nsIGfxInfo.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsServiceManagerUtils.h"
#include "RtpRtcpConfig.h"
#include "VideoStreamFactory.h"
#include "WebrtcCallWrapper.h"
#include "WebrtcGmpVideoCodec.h"

// libwebrtc includes
#include "api/transport/bitrate_settings.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_codec.h"
#include "media/base/media_constants.h"
#include "media/engine/encoder_simulcast_proxy.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "VideoEngine.h"
#endif

#ifdef MOZ_WEBRTC_MEDIACODEC
#  include "MediaCodecVideoCodec.h"
#endif

// for ntohs
#ifdef _MSC_VER
#  include "Winsock2.h"
#else
#  include <netinet/in.h>
#endif

#define DEFAULT_VIDEO_MAX_FRAMERATE 30
#define INVALID_RTP_PAYLOAD 255  // valid payload types are 0 to 127

namespace mozilla {

namespace {

const char* vcLogTag = "WebrtcVideoSessionConduit";
#ifdef LOGTAG
#  undef LOGTAG
#endif
#define LOGTAG vcLogTag

using namespace cricket;
using LocalDirection = MediaSessionConduitLocalDirection;

const int kNullPayloadType = -1;
const char kRtcpFbCcmParamTmmbr[] = "tmmbr";

// The number of frame buffers WebrtcVideoConduit may create before returning
// errors.
// Sometimes these are released synchronously but they can be forwarded all the
// way to the encoder for asynchronous encoding. With a pool size of 5,
// we allow 1 buffer for the current conversion, and 4 buffers to be queued at
// the encoder.
#define SCALER_BUFFER_POOL_SIZE 5

// The pixel alignment to use for the highest resolution layer when simulcast
// is active and one or more layers are being scaled.
#define SIMULCAST_RESOLUTION_ALIGNMENT 16

template <class t>
void ConstrainPreservingAspectRatioExact(uint32_t max_fs, t* width, t* height) {
  // We could try to pick a better starting divisor, but it won't make any real
  // performance difference.
  for (size_t d = 1; d < std::min(*width, *height); ++d) {
    if ((*width % d) || (*height % d)) {
      continue;  // Not divisible
    }

    if (((*width) * (*height)) / (d * d) <= max_fs) {
      *width /= d;
      *height /= d;
      return;
    }
  }

  *width = 0;
  *height = 0;
}

template <class t>
void ConstrainPreservingAspectRatio(uint16_t max_width, uint16_t max_height,
                                    t* width, t* height) {
  if (((*width) <= max_width) && ((*height) <= max_height)) {
    return;
  }

  if ((*width) * max_height > max_width * (*height)) {
    (*height) = max_width * (*height) / (*width);
    (*width) = max_width;
  } else {
    (*width) = max_height * (*width) / (*height);
    (*height) = max_height;
  }
}

/**
 * Function to select and change the encoding frame rate based on incoming frame
 * rate and max-mbps setting.
 * @param current framerate
 * @result new framerate
 */
unsigned int SelectSendFrameRate(const VideoCodecConfig& codecConfig,
                                 unsigned int old_framerate,
                                 unsigned short sending_width,
                                 unsigned short sending_height) {
  unsigned int new_framerate = old_framerate;

  // Limit frame rate based on max-mbps
  if (codecConfig.mEncodingConstraints.maxMbps) {
    unsigned int cur_fs, mb_width, mb_height;

    mb_width = (sending_width + 15) >> 4;
    mb_height = (sending_height + 15) >> 4;

    cur_fs = mb_width * mb_height;
    if (cur_fs > 0) {  // in case no frames have been sent
      new_framerate = codecConfig.mEncodingConstraints.maxMbps / cur_fs;

      new_framerate =
          MinIgnoreZero(new_framerate, codecConfig.mEncodingConstraints.maxFps);
    }
  }
  return new_framerate;
}

/**
 * Perform validation on the codecConfig to be applied
 */
MediaConduitErrorCode ValidateCodecConfig(const VideoCodecConfig& codecInfo) {
  if (codecInfo.mName.empty()) {
    CSFLogError(LOGTAG, "%s Empty Payload Name ", __FUNCTION__);
    return kMediaConduitMalformedArgument;
  }

  return kMediaConduitNoError;
}

webrtc::VideoCodecType SupportedCodecType(webrtc::VideoCodecType aType) {
  switch (aType) {
    case webrtc::VideoCodecType::kVideoCodecVP8:
    case webrtc::VideoCodecType::kVideoCodecVP9:
    case webrtc::VideoCodecType::kVideoCodecH264:
      return aType;
    default:
      return webrtc::VideoCodecType::kVideoCodecGeneric;
  }
  // NOTREACHED
}

// Call thread only.
rtc::scoped_refptr<webrtc::VideoEncoderConfig::EncoderSpecificSettings>
ConfigureVideoEncoderSettings(const VideoCodecConfig& aConfig,
                              const WebrtcVideoConduit* aConduit) {
  bool is_screencast =
      aConduit->CodecMode() == webrtc::VideoCodecMode::kScreensharing;
  // No automatic resizing when using simulcast or screencast.
  bool automatic_resize = !is_screencast && aConfig.mEncodings.size() <= 1;
  bool frame_dropping = !is_screencast;
  bool denoising;
  bool codec_default_denoising = false;
  if (is_screencast) {
    denoising = false;
  } else {
    // Use codec default if video_noise_reduction is unset.
    denoising = aConduit->Denoising();
    codec_default_denoising = !denoising;
  }

  if (aConfig.mName == kH264CodecName) {
    webrtc::VideoCodecH264 h264_settings =
        webrtc::VideoEncoder::GetDefaultH264Settings();
    h264_settings.frameDroppingOn = frame_dropping;
    h264_settings.packetizationMode = aConfig.mPacketizationMode;
    return new rtc::RefCountedObject<
        webrtc::VideoEncoderConfig::H264EncoderSpecificSettings>(h264_settings);
  }
  if (aConfig.mName == kVp8CodecName) {
    webrtc::VideoCodecVP8 vp8_settings =
        webrtc::VideoEncoder::GetDefaultVp8Settings();
    vp8_settings.automaticResizeOn = automatic_resize;
    // VP8 denoising is enabled by default.
    vp8_settings.denoisingOn = codec_default_denoising ? true : denoising;
    vp8_settings.frameDroppingOn = frame_dropping;
    return new rtc::RefCountedObject<
        webrtc::VideoEncoderConfig::Vp8EncoderSpecificSettings>(vp8_settings);
  }
  if (aConfig.mName == kVp9CodecName) {
    webrtc::VideoCodecVP9 vp9_settings =
        webrtc::VideoEncoder::GetDefaultVp9Settings();
    if (is_screencast) {
      // TODO(asapersson): Set to 2 for now since there is a DCHECK in
      // VideoSendStream::ReconfigureVideoEncoder.
      vp9_settings.numberOfSpatialLayers = 2;
    } else {
      vp9_settings.numberOfSpatialLayers = aConduit->SpatialLayers();
    }
    // VP9 denoising is disabled by default.
    vp9_settings.denoisingOn = codec_default_denoising ? false : denoising;
    vp9_settings.frameDroppingOn = true;  // This must be true for VP9
    return new rtc::RefCountedObject<
        webrtc::VideoEncoderConfig::Vp9EncoderSpecificSettings>(vp9_settings);
  }
  return nullptr;
}

uint32_t GenerateRandomSSRC() {
  uint32_t ssrc;
  do {
    SECStatus rv = PK11_GenerateRandom(reinterpret_cast<unsigned char*>(&ssrc),
                                       sizeof(ssrc));
    MOZ_RELEASE_ASSERT(rv == SECSuccess);
  } while (ssrc == 0);  // webrtc.org code has fits if you select an SSRC of 0

  return ssrc;
}

// TODO: Make this a defaulted operator when we have c++20 (bug 1731036).
bool operator==(const rtc::VideoSinkWants& aThis,
                const rtc::VideoSinkWants& aOther) {
  // This would have to be expanded should we make use of more members of
  // rtc::VideoSinkWants.
  return aThis.max_pixel_count == aOther.max_pixel_count;
}

// TODO: Make this a defaulted operator when we have c++20 (bug 1731036).
bool operator!=(const rtc::VideoSinkWants& aThis,
                const rtc::VideoSinkWants& aOther) {
  return !(aThis == aOther);
}

// TODO: Make this a defaulted operator when we have c++20 (bug 1731036).
bool operator!=(const webrtc::VideoReceiveStream::Config::Rtp& aThis,
                const webrtc::VideoReceiveStream::Config::Rtp& aOther) {
  return aThis.remote_ssrc != aOther.remote_ssrc ||
         aThis.local_ssrc != aOther.local_ssrc ||
         aThis.rtcp_mode != aOther.rtcp_mode ||
         aThis.rtcp_xr.receiver_reference_time_report !=
             aOther.rtcp_xr.receiver_reference_time_report ||
         aThis.transport_cc != aOther.transport_cc ||
         aThis.remb != aOther.remb || aThis.tmmbr != aOther.tmmbr ||
         aThis.keyframe_method != aOther.keyframe_method ||
         aThis.lntf.enabled != aOther.lntf.enabled ||
         aThis.nack.rtp_history_ms != aOther.nack.rtp_history_ms ||
         aThis.ulpfec_payload_type != aOther.ulpfec_payload_type ||
         aThis.red_payload_type != aOther.red_payload_type ||
         aThis.rtx_ssrc != aOther.rtx_ssrc ||
         aThis.protected_by_flexfec != aOther.protected_by_flexfec ||
         aThis.rtx_associated_payload_types !=
             aOther.rtx_associated_payload_types ||
         aThis.raw_payload_types != aOther.raw_payload_types ||
         aThis.extensions != aOther.extensions;
}

#ifdef DEBUG
// TODO: Make this a defaulted operator when we have c++20 (bug 1731036).
bool operator==(const webrtc::VideoReceiveStream::Config::Rtp& aThis,
                const webrtc::VideoReceiveStream::Config::Rtp& aOther) {
  return !(aThis != aOther);
}
#endif

// TODO: Make this a defaulted operator when we have c++20 (bug 1731036).
bool operator!=(const webrtc::RtpConfig& aThis,
                const webrtc::RtpConfig& aOther) {
  return aThis.ssrcs != aOther.ssrcs || aThis.rids != aOther.rids ||
         aThis.mid != aOther.mid || aThis.rtcp_mode != aOther.rtcp_mode ||
         aThis.max_packet_size != aOther.max_packet_size ||
         aThis.extmap_allow_mixed != aOther.extmap_allow_mixed ||
         aThis.extensions != aOther.extensions ||
         aThis.payload_name != aOther.payload_name ||
         aThis.payload_type != aOther.payload_type ||
         aThis.raw_payload != aOther.raw_payload ||
         aThis.lntf.enabled != aOther.lntf.enabled ||
         aThis.nack.rtp_history_ms != aOther.nack.rtp_history_ms ||
         !(aThis.ulpfec == aOther.ulpfec) ||
         aThis.flexfec.payload_type != aOther.flexfec.payload_type ||
         aThis.flexfec.ssrc != aOther.flexfec.ssrc ||
         aThis.flexfec.protected_media_ssrcs !=
             aOther.flexfec.protected_media_ssrcs ||
         aThis.rtx.ssrcs != aOther.rtx.ssrcs ||
         aThis.rtx.payload_type != aOther.rtx.payload_type ||
         aThis.c_name != aOther.c_name;
}

#ifdef DEBUG
// TODO: Make this a defaulted operator when we have c++20 (bug 1731036).
bool operator==(const webrtc::RtpConfig& aThis,
                const webrtc::RtpConfig& aOther) {
  return !(aThis != aOther);
}
#endif

// TODO: Make this a defaulted operator when we have c++20 (bug 1731036).
bool operator==(const webrtc::VideoReceiveStream::Decoder& aThis,
                const webrtc::VideoReceiveStream::Decoder& aOther) {
  return aThis.video_format == aOther.video_format &&
         aThis.payload_type == aOther.payload_type;
}

}  // namespace

/**
 * Factory Method for VideoConduit
 */
RefPtr<VideoSessionConduit> VideoSessionConduit::Create(
    RefPtr<WebrtcCallWrapper> aCall, nsCOMPtr<nsISerialEventTarget> aStsThread,
    Options aOptions, std::string aPCHandle) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCall, "missing required parameter: aCall");
  CSFLogVerbose(LOGTAG, "%s", __FUNCTION__);

  if (!aCall) {
    return nullptr;
  }

  auto obj =
      MakeRefPtr<WebrtcVideoConduit>(std::move(aCall), std::move(aStsThread),
                                     std::move(aOptions), std::move(aPCHandle));
  if (obj->Init() != kMediaConduitNoError) {
    CSFLogError(LOGTAG, "%s VideoConduit Init Failed ", __FUNCTION__);
    return nullptr;
  }
  CSFLogVerbose(LOGTAG, "%s Successfully created VideoConduit ", __FUNCTION__);
  return obj.forget();
}

#define INIT_MIRROR(name, val) \
  name(aCallThread, val, "WebrtcVideoConduit::Control::" #name " (Mirror)")
WebrtcVideoConduit::Control::Control(const RefPtr<AbstractThread>& aCallThread)
    : INIT_MIRROR(mReceiving, false),
      INIT_MIRROR(mTransmitting, false),
      INIT_MIRROR(mLocalSsrcs, Ssrcs()),
      INIT_MIRROR(mLocalRtxSsrcs, Ssrcs()),
      INIT_MIRROR(mLocalCname, std::string()),
      INIT_MIRROR(mLocalMid, std::string()),
      INIT_MIRROR(mRemoteSsrc, 0),
      INIT_MIRROR(mRemoteRtxSsrc, 0),
      INIT_MIRROR(mSyncGroup, std::string()),
      INIT_MIRROR(mLocalRecvRtpExtensions, RtpExtList()),
      INIT_MIRROR(mLocalSendRtpExtensions, RtpExtList()),
      INIT_MIRROR(mSendCodec, Nothing()),
      INIT_MIRROR(mSendRtpRtcpConfig, Nothing()),
      INIT_MIRROR(mRecvCodecs, std::vector<VideoCodecConfig>()),
      INIT_MIRROR(mRecvRtpRtcpConfig, Nothing()),
      INIT_MIRROR(mCodecMode, webrtc::VideoCodecMode::kRealtimeVideo) {}
#undef INIT_MIRROR

WebrtcVideoConduit::WebrtcVideoConduit(
    RefPtr<WebrtcCallWrapper> aCall, nsCOMPtr<nsISerialEventTarget> aStsThread,
    Options aOptions, std::string aPCHandle)
    : mTransportMonitor("WebrtcVideoConduit"),
      mCallThread(aCall->mCallThread),
      mStsThread(std::move(aStsThread)),
      mControl(aCall->mCallThread),
      mWatchManager(this, aCall->mCallThread),
      mMutex("WebrtcVideoConduit::mMutex"),
      mDecoderFactory(
          MakeUnique<WebrtcVideoDecoderFactory>(mCallThread.get(), aPCHandle)),
      mEncoderFactory(MakeUnique<WebrtcVideoEncoderFactory>(
          mCallThread.get(), std::move(aPCHandle))),
      mVideoAdapter(MakeUnique<cricket::VideoAdapter>()),
      mBufferPool(false, SCALER_BUFFER_POOL_SIZE),
      mEngineTransmitting(false),
      mEngineReceiving(false),
      mSendingFramerate(DEFAULT_VIDEO_MAX_FRAMERATE),
      mVideoLatencyTestEnable(aOptions.mVideoLatencyTestEnable),
      mMinBitrate(aOptions.mMinBitrate),
      mStartBitrate(aOptions.mStartBitrate),
      mPrefMaxBitrate(aOptions.mPrefMaxBitrate),
      mMinBitrateEstimate(aOptions.mMinBitrateEstimate),
      mDenoising(aOptions.mDenoising),
      mLockScaling(aOptions.mLockScaling),
      mSpatialLayers(aOptions.mSpatialLayers),
      mTemporalLayers(aOptions.mTemporalLayers),
      mCall(std::move(aCall)),
      mSendStreamConfig(
          this)  // 'this' is stored but not dereferenced in the constructor.
      ,
      mRecvStreamConfig(
          this)  // 'this' is stored but not dereferenced in the constructor.
      ,
      mRecvSSRC(0),
      mRemoteSSRC(0) {}

WebrtcVideoConduit::~WebrtcVideoConduit() {
  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);

  MOZ_ASSERT(!mSendStream && !mRecvStream,
             "Call DeleteStreams prior to ~WebrtcVideoConduit.");
}

#define CONNECT(aCanonical, aMirror)                                          \
  do {                                                                        \
    (aMirror).Connect(aCanonical);                                            \
    mWatchManager.Watch(aMirror, &WebrtcVideoConduit::OnControlConfigChange); \
  } while (0)

void WebrtcVideoConduit::InitControl(VideoConduitControlInterface* aControl) {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());

  CONNECT(aControl->CanonicalReceiving(), mControl.mReceiving);
  CONNECT(aControl->CanonicalTransmitting(), mControl.mTransmitting);
  CONNECT(aControl->CanonicalLocalSsrcs(), mControl.mLocalSsrcs);
  CONNECT(aControl->CanonicalLocalVideoRtxSsrcs(), mControl.mLocalRtxSsrcs);
  CONNECT(aControl->CanonicalLocalCname(), mControl.mLocalCname);
  CONNECT(aControl->CanonicalLocalMid(), mControl.mLocalMid);
  CONNECT(aControl->CanonicalRemoteSsrc(), mControl.mRemoteSsrc);
  CONNECT(aControl->CanonicalRemoteVideoRtxSsrc(), mControl.mRemoteRtxSsrc);
  CONNECT(aControl->CanonicalSyncGroup(), mControl.mSyncGroup);
  CONNECT(aControl->CanonicalLocalRecvRtpExtensions(),
          mControl.mLocalRecvRtpExtensions);
  CONNECT(aControl->CanonicalLocalSendRtpExtensions(),
          mControl.mLocalSendRtpExtensions);
  CONNECT(aControl->CanonicalVideoSendCodec(), mControl.mSendCodec);
  CONNECT(aControl->CanonicalVideoSendRtpRtcpConfig(),
          mControl.mSendRtpRtcpConfig);
  CONNECT(aControl->CanonicalVideoRecvCodecs(), mControl.mRecvCodecs);
  CONNECT(aControl->CanonicalVideoRecvRtpRtcpConfig(),
          mControl.mRecvRtpRtcpConfig);
  CONNECT(aControl->CanonicalVideoCodecMode(), mControl.mCodecMode);
}

#undef CONNECT

void WebrtcVideoConduit::OnControlConfigChange() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());

  bool encoderReconfigureNeeded = false;
  bool remoteSsrcUpdateNeeded = false;
  bool isInitialRemoteSsrcUpdate = false;
  bool recvStreamRecreationNeeded = false;
  bool sendStreamRecreationNeeded = false;

  if (mControl.mRemoteSsrc.Ref() != mControl.mConfiguredRemoteSsrc) {
    isInitialRemoteSsrcUpdate = mControl.mConfiguredRemoteSsrc == 0;
    mControl.mConfiguredRemoteSsrc = mControl.mRemoteSsrc;
    recvStreamRecreationNeeded = true;
    remoteSsrcUpdateNeeded = true;
  }

  if (mControl.mRemoteRtxSsrc.Ref() != mControl.mConfiguredRemoteRtxSsrc) {
    mControl.mConfiguredRemoteRtxSsrc = mControl.mRemoteRtxSsrc;
    recvStreamRecreationNeeded = true;
    remoteSsrcUpdateNeeded = true;
  }

  if (mControl.mSyncGroup.Ref() != mRecvStreamConfig.sync_group) {
    mRecvStreamConfig.sync_group = mControl.mSyncGroup;
    recvStreamRecreationNeeded = true;
  }

  if (mControl.mLocalRecvRtpExtensions.Ref() !=
      mRecvStreamConfig.rtp.extensions) {
    mRecvStreamConfig.rtp.extensions = mControl.mLocalRecvRtpExtensions;
    recvStreamRecreationNeeded = true;
  }

  if (const auto [codecConfigList, rtpRtcpConfig] = std::make_pair(
          mControl.mRecvCodecs.Ref(), mControl.mRecvRtpRtcpConfig.Ref());
      !codecConfigList.empty() && rtpRtcpConfig.isSome() &&
      (codecConfigList != mControl.mConfiguredRecvCodecs ||
       rtpRtcpConfig != mControl.mConfiguredRecvRtpRtcpConfig)) {
    mControl.mConfiguredRecvCodecs = codecConfigList;
    mControl.mConfiguredRecvRtpRtcpConfig = rtpRtcpConfig;

    webrtc::VideoReceiveStream::Config::Rtp newRtp(mRecvStreamConfig.rtp);
    MOZ_ASSERT(newRtp == mRecvStreamConfig.rtp);
    newRtp.rtx_associated_payload_types.clear();
    newRtp.rtcp_mode = rtpRtcpConfig->GetRtcpMode();
    newRtp.nack.rtp_history_ms = 0;
    newRtp.remb = false;
    newRtp.transport_cc = false;
    newRtp.tmmbr = false;
    newRtp.keyframe_method = webrtc::KeyFrameReqMethod::kNone;
    newRtp.ulpfec_payload_type = kNullPayloadType;
    newRtp.red_payload_type = kNullPayloadType;
    bool use_fec = false;
    bool configuredH264 = false;
    std::vector<webrtc::VideoReceiveStream::Decoder> recv_codecs;

    // Try Applying the codecs in the list
    // we treat as success if at least one codec was applied and reception was
    // started successfully.
    for (const auto& codec_config : codecConfigList) {
      if (auto condError = ValidateCodecConfig(codec_config);
          condError != kMediaConduitNoError) {
        CSFLogError(LOGTAG, "Invalid recv codec config for %s decoder: %i",
                    codec_config.mName.c_str(), condError);
        continue;
      }

      if (codec_config.mName == kH264CodecName) {
        // TODO(bug 1200768): We can only handle configuring one recv H264 codec
        if (configuredH264) {
          continue;
        }
        configuredH264 = true;
      }

      if (codec_config.mName == kUlpfecCodecName) {
        newRtp.ulpfec_payload_type = codec_config.mType;
        continue;
      }

      if (codec_config.mName == kRedCodecName) {
        newRtp.red_payload_type = codec_config.mType;
        continue;
      }

      if (SupportedCodecType(
              webrtc::PayloadStringToCodecType(codec_config.mName)) ==
          webrtc::VideoCodecType::kVideoCodecGeneric) {
        CSFLogError(LOGTAG, "%s Unknown decoder type: %s", __FUNCTION__,
                    codec_config.mName.c_str());
        continue;
      }

      // Check for the keyframe request type: PLI is preferred over FIR, and FIR
      // is preferred over none.
      if (codec_config.RtcpFbNackIsSet(kRtcpFbNackParamPli)) {
        newRtp.keyframe_method = webrtc::KeyFrameReqMethod::kPliRtcp;
      } else if (newRtp.keyframe_method !=
                     webrtc::KeyFrameReqMethod::kPliRtcp &&
                 codec_config.RtcpFbCcmIsSet(kRtcpFbCcmParamFir)) {
        newRtp.keyframe_method = webrtc::KeyFrameReqMethod::kFirRtcp;
      }

      // What if codec A has Nack and REMB, and codec B has TMMBR, and codec C
      // has none? In practice, that's not a useful configuration, and
      // VideoReceiveStream::Config can't represent that, so simply union the
      // (boolean) settings
      if (codec_config.RtcpFbNackIsSet(kParamValueEmpty)) {
        newRtp.nack.rtp_history_ms = 1000;
      }
      newRtp.tmmbr |= codec_config.RtcpFbCcmIsSet(kRtcpFbCcmParamTmmbr);
      newRtp.remb |= codec_config.RtcpFbRembIsSet();
      use_fec |= codec_config.RtcpFbFECIsSet();
      newRtp.transport_cc |= codec_config.RtcpFbTransportCCIsSet();

      if (codec_config.RtxPayloadTypeIsSet()) {
        newRtp.rtx_associated_payload_types[codec_config.mRTXPayloadType] =
            codec_config.mType;
      }

      auto& decoder = recv_codecs.emplace_back();
      decoder.video_format = webrtc::SdpVideoFormat(codec_config.mName);
      decoder.payload_type = codec_config.mType;
    }

    if (!use_fec) {
      // Reset to defaults
      newRtp.ulpfec_payload_type = kNullPayloadType;
      newRtp.red_payload_type = kNullPayloadType;
    }

    // TODO: This would be simpler, but for some reason gives
    //       "error: invalid operands to binary expression
    //       ('webrtc::VideoReceiveStream::Decoder' and
    //       'webrtc::VideoReceiveStream::Decoder')"
    // if (recv_codecs != mRecvStreamConfig.decoders) {
    if (!std::equal(recv_codecs.begin(), recv_codecs.end(),
                    mRecvStreamConfig.decoders.begin(),
                    mRecvStreamConfig.decoders.end(),
                    [](const auto& aLeft, const auto& aRight) {
                      return aLeft == aRight;
                    })) {
      if (recv_codecs.empty()) {
        CSFLogError(LOGTAG, "%s Found no valid receive codecs", __FUNCTION__);
      }
      mRecvStreamConfig.decoders = std::move(recv_codecs);
      recvStreamRecreationNeeded = true;
    }

    if (mRecvStreamConfig.rtp != newRtp) {
      mRecvStreamConfig.rtp = newRtp;
      recvStreamRecreationNeeded = true;
    }
  }

  // mSendStreamConfig and other members need the lock
  MutexAutoLock lock(mMutex);
  if (mControl.mLocalSsrcs.Ref() != mSendStreamConfig.rtp.ssrcs) {
    mSendStreamConfig.rtp.ssrcs = mControl.mLocalSsrcs;
    sendStreamRecreationNeeded = true;

    const uint32_t localSsrc = mSendStreamConfig.rtp.ssrcs.empty()
                                   ? 0
                                   : mSendStreamConfig.rtp.ssrcs.front();
    if (localSsrc != mRecvStreamConfig.rtp.local_ssrc) {
      mRecvStreamConfig.rtp.local_ssrc = localSsrc;
      recvStreamRecreationNeeded = true;
    }
  }

  {
    Ssrcs localRtxSsrcs = mControl.mLocalRtxSsrcs.Ref();
    if (!mControl.mSendCodec.Ref()
             .map([](const auto& aCodec) {
               return aCodec.RtxPayloadTypeIsSet();
             })
             .valueOr(false)) {
      localRtxSsrcs.clear();
    }
    if (localRtxSsrcs != mSendStreamConfig.rtp.rtx.ssrcs) {
      mSendStreamConfig.rtp.rtx.ssrcs = localRtxSsrcs;
      sendStreamRecreationNeeded = true;
    }
  }

  if (mControl.mLocalCname.Ref() != mSendStreamConfig.rtp.c_name) {
    mSendStreamConfig.rtp.c_name = mControl.mLocalCname;
    sendStreamRecreationNeeded = true;
  }

  if (mControl.mLocalMid.Ref() != mSendStreamConfig.rtp.mid) {
    mSendStreamConfig.rtp.mid = mControl.mLocalMid;
    sendStreamRecreationNeeded = true;
  }

  if (mControl.mLocalSendRtpExtensions.Ref() !=
      mSendStreamConfig.rtp.extensions) {
    mSendStreamConfig.rtp.extensions = mControl.mLocalSendRtpExtensions;
    sendStreamRecreationNeeded = true;
  }

  if (const auto [codecConfig, rtpRtcpConfig] = std::make_pair(
          mControl.mSendCodec.Ref(), mControl.mSendRtpRtcpConfig.Ref());
      codecConfig.isSome() && rtpRtcpConfig.isSome() &&
      (codecConfig != mControl.mConfiguredSendCodec ||
       rtpRtcpConfig != mControl.mConfiguredSendRtpRtcpConfig)) {
    CSFLogDebug(LOGTAG, "Configuring codec %s", codecConfig->mName.c_str());
    mControl.mConfiguredSendCodec = codecConfig;
    mControl.mConfiguredSendRtpRtcpConfig = rtpRtcpConfig;

    if (ValidateCodecConfig(*codecConfig) == kMediaConduitNoError) {
      encoderReconfigureNeeded = true;
      mUpdateSendResolution = true;

      mCurSendCodecConfig = codecConfig;

      size_t streamCount = std::min(codecConfig->mEncodings.size(),
                                    (size_t)webrtc::kMaxSimulcastStreams);
      size_t highestResolutionIndex = 0;
      for (size_t i = 1; i < streamCount; ++i) {
        if (codecConfig->mEncodings[i].constraints.scaleDownBy <
            codecConfig->mEncodings[highestResolutionIndex]
                .constraints.scaleDownBy) {
          highestResolutionIndex = i;
        }
      }
      MOZ_RELEASE_ASSERT(streamCount >= 1,
                         "streamCount should be at least one");

      CSFLogDebug(LOGTAG,
                  "Updating send codec for VideoConduit:%p stream count:%zu",
                  this, streamCount);

      {
        const int max_framerate = codecConfig->mEncodingConstraints.maxFps > 0
                                      ? codecConfig->mEncodingConstraints.maxFps
                                      : DEFAULT_VIDEO_MAX_FRAMERATE;
        // apply restrictions from maxMbps/etc
        mSendingFramerate = SelectSendFrameRate(*codecConfig, max_framerate,
                                                mLastWidth, mLastHeight);
      }

      // So we can comply with b=TIAS/b=AS/maxbr=X when input resolution changes
      mNegotiatedMaxBitrate = codecConfig->mTias;

      if (mLastWidth == 0 && mMinBitrateEstimate != 0) {
        // Only do this at the start; use "have we sent a frame" as a reasonable
        // stand-in. min <= start <= max (but all three parameters are optional)
        webrtc::BitrateSettings settings;
        settings.min_bitrate_bps = mMinBitrateEstimate;
        settings.start_bitrate_bps = mMinBitrateEstimate;
        mCall->Call()->SetClientBitratePreferences(settings);
      }

      mVideoStreamFactory = new rtc::RefCountedObject<VideoStreamFactory>(
          *codecConfig, mControl.mCodecMode, mMinBitrate, mStartBitrate,
          mPrefMaxBitrate, mNegotiatedMaxBitrate, mSendingFramerate);
      mEncoderConfig.video_stream_factory = mVideoStreamFactory.get();

      // Reset the VideoAdapter. SelectResolution will ensure limits are set.
      mVideoAdapter = MakeUnique<cricket::VideoAdapter>(
          streamCount > 1 ? SIMULCAST_RESOLUTION_ALIGNMENT : 1);
      mVideoAdapter->OnScaleResolutionBy(
          codecConfig->mEncodings[highestResolutionIndex]
                      .constraints.scaleDownBy > 1.0
              ? absl::optional<float>(
                    codecConfig->mEncodings[highestResolutionIndex]
                        .constraints.scaleDownBy)
              : absl::optional<float>());

      // XXX parse the encoded SPS/PPS data and set
      // spsData/spsLen/ppsData/ppsLen
      mEncoderConfig.encoder_specific_settings =
          ConfigureVideoEncoderSettings(*codecConfig, this);

      mEncoderConfig.codec_type = SupportedCodecType(
          webrtc::PayloadStringToCodecType(codecConfig->mName));
      MOZ_RELEASE_ASSERT(mEncoderConfig.codec_type !=
                         webrtc::VideoCodecType::kVideoCodecGeneric);
      mEncoderConfig.video_format = webrtc::SdpVideoFormat(codecConfig->mName);

      mEncoderConfig.content_type =
          mControl.mCodecMode.Ref() == webrtc::VideoCodecMode::kRealtimeVideo
              ? webrtc::VideoEncoderConfig::ContentType::kRealtimeVideo
              : webrtc::VideoEncoderConfig::ContentType::kScreen;

      mEncoderConfig.min_transmit_bitrate_bps = mMinBitrate;

      // Set the max bitrate, defaulting to 10Mbps, checking:
      // - pref
      // - b=TIAS
      // - codec constraints
      // - encoding parameter if there's a single stream
      int maxBps = KBPS(10000);
      maxBps = MinIgnoreZero(maxBps, mPrefMaxBitrate);
      maxBps = MinIgnoreZero(maxBps, mNegotiatedMaxBitrate);
      maxBps = MinIgnoreZero(
          maxBps, static_cast<int>(codecConfig->mEncodingConstraints.maxBr));
      if (codecConfig->mEncodings.size() == 1) {
        maxBps = MinIgnoreZero(
            maxBps,
            static_cast<int>(codecConfig->mEncodings[0].constraints.maxBr));
      }
      mEncoderConfig.max_bitrate_bps = maxBps;

      // TODO this is for webrtc-priority, but needs plumbing bits
      mEncoderConfig.bitrate_priority = 1.0;

      // Expected max number of encodings
      mEncoderConfig.number_of_streams = streamCount;

      // libwebrtc disables this by default.
      mSendStreamConfig.suspend_below_min_bitrate = false;

      webrtc::RtpConfig newRtp = mSendStreamConfig.rtp;
      MOZ_ASSERT(newRtp == mSendStreamConfig.rtp);
      newRtp.payload_name = codecConfig->mName;
      newRtp.payload_type = codecConfig->mType;
      newRtp.rtcp_mode = rtpRtcpConfig->GetRtcpMode();
      newRtp.max_packet_size = kVideoMtu;
      newRtp.rtx.payload_type = codecConfig->RtxPayloadTypeIsSet()
                                    ? codecConfig->mRTXPayloadType
                                    : kNullPayloadType;

      {
        // See Bug 1297058, enabling FEC when basic NACK is to be enabled in
        // H.264 is problematic
        const bool useFECDefaults =
            !codecConfig->RtcpFbFECIsSet() ||
            (codecConfig->mName == kH264CodecName &&
             codecConfig->RtcpFbNackIsSet(kParamValueEmpty));
        newRtp.ulpfec.ulpfec_payload_type =
            useFECDefaults ? kNullPayloadType : codecConfig->mULPFECPayloadType;
        newRtp.ulpfec.red_payload_type =
            useFECDefaults ? kNullPayloadType : codecConfig->mREDPayloadType;
        newRtp.ulpfec.red_rtx_payload_type =
            useFECDefaults ? kNullPayloadType : codecConfig->mREDRTXPayloadType;
      }

      newRtp.nack.rtp_history_ms =
          codecConfig->RtcpFbNackIsSet(kParamValueEmpty) ? 1000 : 0;

      {
        newRtp.rids.clear();
        bool has_rid = false;
        for (size_t idx = 0; idx < streamCount; idx++) {
          auto& encoding = codecConfig->mEncodings[idx];
          if (encoding.rid[0]) {
            has_rid = true;
            break;
          }
        }
        if (has_rid) {
          for (size_t idx = streamCount; idx > 0; idx--) {
            auto& encoding = codecConfig->mEncodings[idx - 1];
            newRtp.rids.push_back(encoding.rid);
          }
        }
      }
      if (mSendStreamConfig.rtp != newRtp) {
        mSendStreamConfig.rtp = newRtp;
        sendStreamRecreationNeeded = true;
      }
    }
  }

  {
    const auto& mode = mControl.mCodecMode.Ref();
    MOZ_ASSERT(mode == webrtc::VideoCodecMode::kRealtimeVideo ||
               mode == webrtc::VideoCodecMode::kScreensharing);
    if (mVideoStreamFactory) {
      mVideoStreamFactory->SetCodecMode(mode);
    }
  }

  // Recreate/Stop/Start streams as needed.
  if (mRecvStream) {
    if (recvStreamRecreationNeeded) {
      DeleteRecvStream();
      // StartReceivingLocked creates the stream as needed.
    }
  }
  if (mSendStream) {
    if (sendStreamRecreationNeeded) {
      DeleteSendStream();
      // StartTransmittingLocked creates the stream as needed.
    } else if (encoderReconfigureNeeded) {
      mSendStream->ReconfigureVideoEncoder(mEncoderConfig.Copy());
    }
  }

  if (remoteSsrcUpdateNeeded) {
    SetRemoteSSRCLocked(mControl.mConfiguredRemoteSsrc,
                        mControl.mConfiguredRemoteRtxSsrc);
  }
  if (isInitialRemoteSsrcUpdate) {
    mStsThread->Dispatch(NS_NewRunnableFunction(
        "WebrtcVideoConduit::WaitingForSignaledSsrcNoMore",
        [this, self = RefPtr<WebrtcVideoConduit>(this)]() mutable {
          mWaitingForSignaledSsrc = false;
        }));
  }
  {
    // Handle un-signalled SSRCs by creating random ones and then when they
    // actually get set, we'll destroy and recreate.
    if (mControl.mReceiving || mControl.mTransmitting) {
      const auto remoteSsrc = mRecvStreamConfig.rtp.remote_ssrc;
      const auto localSsrc = mRecvStreamConfig.rtp.local_ssrc;
      const auto localSsrcs = mSendStreamConfig.rtp.ssrcs;
      EnsureLocalSSRC();
      if (mControl.mReceiving) {
        EnsureRemoteSSRC();
      }
      if (localSsrc != mRecvStreamConfig.rtp.local_ssrc ||
          remoteSsrc != mRecvStreamConfig.rtp.remote_ssrc) {
        recvStreamRecreationNeeded = true;
      }
      if (localSsrcs != mSendStreamConfig.rtp.ssrcs) {
        sendStreamRecreationNeeded = true;
      }
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

std::vector<unsigned int> WebrtcVideoConduit::GetLocalSSRCs() const {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  return mSendStreamConfig.rtp.ssrcs;
}

void WebrtcVideoConduit::DeleteSendStream() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  mMutex.AssertCurrentThreadOwns();

  if (mSendStream) {
    mCall->Call()->DestroyVideoSendStream(mSendStream);
    mEngineTransmitting = false;
    mSendStream = nullptr;
  }
}

MediaConduitErrorCode WebrtcVideoConduit::CreateSendStream() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  mMutex.AssertCurrentThreadOwns();

  nsAutoString codecName;
  codecName.AssignASCII(mSendStreamConfig.rtp.payload_name.c_str());
  Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_VIDEO_SEND_CODEC_USED,
                       codecName, 1);

  mSendStreamConfig.encoder_settings.encoder_factory = mEncoderFactory.get();
  mSendStreamConfig.encoder_settings.bitrate_allocator_factory =
      mCall->mVideoBitrateAllocatorFactory.get();

  MOZ_ASSERT(
      mSendStreamConfig.rtp.ssrcs.size() == mEncoderConfig.number_of_streams,
      "Each video substream must have a corresponding ssrc.");

  mSendStream = mCall->Call()->CreateVideoSendStream(mSendStreamConfig.Copy(),
                                                     mEncoderConfig.Copy());

  if (!mSendStream) {
    return kMediaConduitVideoSendStreamError;
  }
  mSendStream->SetSource(this, webrtc::DegradationPreference::BALANCED);

  return kMediaConduitNoError;
}

void WebrtcVideoConduit::DeleteRecvStream() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  mMutex.AssertCurrentThreadOwns();

  if (mRecvStream) {
    mCall->Call()->DestroyVideoReceiveStream(mRecvStream);
    mEngineReceiving = false;
    mRecvStream = nullptr;
  }
}

MediaConduitErrorCode WebrtcVideoConduit::CreateRecvStream() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  mMutex.AssertCurrentThreadOwns();

  mRecvStreamConfig.renderer = this;

  for (auto& decoder : mRecvStreamConfig.decoders) {
    nsAutoString codecName;
    codecName.AssignASCII(decoder.video_format.name.c_str());
    Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_VIDEO_RECV_CODEC_USED,
                         codecName, 1);
  }

  mRecvStreamConfig.decoder_factory = mDecoderFactory.get();
  mRecvStreamConfig.rtp.rtcp_event_observer = this;

  mRecvStream =
      mCall->Call()->CreateVideoReceiveStream(mRecvStreamConfig.Copy());
  if (!mRecvStream) {
    return kMediaConduitUnknownError;
  }

  CSFLogDebug(LOGTAG, "Created VideoReceiveStream %p for SSRC %u (0x%x)",
              mRecvStream, mRecvStreamConfig.rtp.remote_ssrc,
              mRecvStreamConfig.rtp.remote_ssrc);
  return kMediaConduitNoError;
}

bool WebrtcVideoConduit::SetRemoteSSRCLocked(uint32_t ssrc, uint32_t rtxSsrc) {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  mMutex.AssertCurrentThreadOwns();

  if (mRecvStreamConfig.rtp.remote_ssrc == ssrc &&
      mRecvStreamConfig.rtp.rtx_ssrc == rtxSsrc) {
    return true;
  }

  bool wasReceiving = mEngineReceiving;
  bool hadRecvStream = mRecvStream;

  if (NS_WARN_IF(StopReceivingLocked() != kMediaConduitNoError)) {
    return false;
  }

  // Don't unset (and call back into) ourselves.
  mCall->UnregisterConduit(this);

  CSFLogDebug(LOGTAG, "%s: SSRC %u (0x%x)", __FUNCTION__, ssrc, ssrc);
  mCall->UnsetRemoteSSRC(ssrc);

  // Remote SSRC set -- listen for unsets from other conduits.
  mCall->RegisterConduit(this);

  mRecvSSRC = mRecvStreamConfig.rtp.remote_ssrc = ssrc;
  mRecvStreamConfig.rtp.rtx_ssrc = rtxSsrc;

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

void WebrtcVideoConduit::EnsureRemoteSSRC() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  mMutex.AssertCurrentThreadOwns();

  const auto& ssrcs = mSendStreamConfig.rtp.ssrcs;
  if (mRecvStreamConfig.rtp.remote_ssrc != 0 &&
      std::find(ssrcs.begin(), ssrcs.end(),
                mRecvStreamConfig.rtp.remote_ssrc) == ssrcs.end()) {
    return;
  }

  uint32_t ssrc;
  do {
    ssrc = GenerateRandomSSRC();
  } while (
      NS_WARN_IF(std::find(ssrcs.begin(), ssrcs.end(), ssrc) != ssrcs.end()));
  CSFLogDebug(LOGTAG, "VideoConduit %p: Generated remote SSRC %u", this, ssrc);
  SetRemoteSSRCLocked(ssrc, 0);
}

void WebrtcVideoConduit::EnsureLocalSSRC() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  mMutex.AssertCurrentThreadOwns();

  auto& ssrcs = mSendStreamConfig.rtp.ssrcs;
  if (ssrcs.empty()) {
    ssrcs.push_back(GenerateRandomSSRC());
  }

  // Reverse-iterating here so that the first dupe in `ssrcs` always wins.
  for (auto& ssrc : Reversed(ssrcs)) {
    if (ssrc != 0 && ssrc != mRecvStreamConfig.rtp.remote_ssrc &&
        std::count(ssrcs.begin(), ssrcs.end(), ssrc) == 1) {
      continue;
    }
    do {
      ssrc = GenerateRandomSSRC();
    } while (NS_WARN_IF(ssrc == mRecvStreamConfig.rtp.remote_ssrc) ||
             NS_WARN_IF(std::count(ssrcs.begin(), ssrcs.end(), ssrc) > 1));
    CSFLogDebug(LOGTAG, "%s (%p): Generated local SSRC %u", __FUNCTION__, this,
                ssrc);
  }
  mRecvStreamConfig.rtp.local_ssrc = ssrcs[0];
}

void WebrtcVideoConduit::UnsetRemoteSSRC(uint32_t ssrc) {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());

  if (mRecvStreamConfig.rtp.remote_ssrc != ssrc &&
      mRecvStreamConfig.rtp.rtx_ssrc != ssrc) {
    return;
  }

  mRecvStreamConfig.rtp.rtx_ssrc = 0;

  const auto& ssrcs = mSendStreamConfig.rtp.ssrcs;
  uint32_t our_ssrc = 0;
  do {
    our_ssrc = GenerateRandomSSRC();
  } while (NS_WARN_IF(our_ssrc == ssrc) ||
           NS_WARN_IF(std::any_of(
               ssrcs.begin(), ssrcs.end(),
               [&](const auto& aSsrc) { return our_ssrc == aSsrc; })));

  // There is a (tiny) chance that this new random ssrc will collide with some
  // other conduit's remote ssrc, in which case that conduit will choose a new
  // one.
  MutexAutoLock lock(mMutex);
  SetRemoteSSRCLocked(our_ssrc, 0);
}

bool WebrtcVideoConduit::GetRemoteSSRC(uint32_t* ssrc) const {
  if (mCallThread->IsOnCurrentThread()) {
    if (!mRecvStream) {
      return false;
    }
  }
  // libwebrtc uses 0 to mean a lack of SSRC. That is not to spec.
  *ssrc = mRemoteSSRC;
  return true;
}

Maybe<webrtc::VideoReceiveStream::Stats> WebrtcVideoConduit::GetReceiverStats()
    const {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  if (!mRecvStream) {
    return Nothing();
  }
  return Some(mRecvStream->GetStats());
}

Maybe<webrtc::VideoSendStream::Stats> WebrtcVideoConduit::GetSenderStats()
    const {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  if (!mSendStream) {
    return Nothing();
  }
  return Some(mSendStream->GetStats());
}

webrtc::Call::Stats WebrtcVideoConduit::GetCallStats() const {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  return mCall->Call()->GetStats();
}

MediaConduitErrorCode WebrtcVideoConduit::Init() {
  MOZ_ASSERT(NS_IsMainThread());

  CSFLogDebug(LOGTAG, "%s this=%p", __FUNCTION__, this);

#ifdef MOZ_WIDGET_ANDROID
  if (mozilla::camera::VideoEngine::SetAndroidObjects() != 0) {
    CSFLogError(LOGTAG, "%s: could not set Android objects", __FUNCTION__);
    return kMediaConduitSessionNotInited;
  }
#endif  // MOZ_WIDGET_ANDROID

  MOZ_ALWAYS_SUCCEEDS(mCallThread->Dispatch(
      NewRunnableMethod(__func__, this, &WebrtcVideoConduit::InitCall)));

  CSFLogDebug(LOGTAG, "%s Initialization Done", __FUNCTION__);
  return kMediaConduitNoError;
}

void WebrtcVideoConduit::InitCall() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  mSendPluginCreated = mEncoderFactory->CreatedGmpPluginEvent().Connect(
      GetMainThreadSerialEventTarget(),
      [self = detail::RawPtr(this)](uint64_t aPluginID) {
        self.get()->mSendCodecPluginIDs.AppendElement(aPluginID);
      });
  mSendPluginReleased = mEncoderFactory->ReleasedGmpPluginEvent().Connect(
      GetMainThreadSerialEventTarget(),
      [self = detail::RawPtr(this)](uint64_t aPluginID) {
        self.get()->mSendCodecPluginIDs.RemoveElement(aPluginID);
      });
  mRecvPluginCreated = mDecoderFactory->CreatedGmpPluginEvent().Connect(
      GetMainThreadSerialEventTarget(),
      [self = detail::RawPtr(this)](uint64_t aPluginID) {
        self.get()->mRecvCodecPluginIDs.AppendElement(aPluginID);
      });
  mRecvPluginReleased = mDecoderFactory->ReleasedGmpPluginEvent().Connect(
      GetMainThreadSerialEventTarget(),
      [self = detail::RawPtr(this)](uint64_t aPluginID) {
        self.get()->mRecvCodecPluginIDs.RemoveElement(aPluginID);
      });
}

void WebrtcVideoConduit::Shutdown() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());

  using namespace Telemetry;
  if (mSendBitrate.NumDataValues() > 0) {
    Accumulate(WEBRTC_VIDEO_ENCODER_BITRATE_AVG_PER_CALL_KBPS,
               mSendBitrate.Mean() / 1000);
    Accumulate(WEBRTC_VIDEO_ENCODER_BITRATE_STD_DEV_PER_CALL_KBPS,
               mSendBitrate.StandardDeviation() / 1000);
    mSendBitrate.Clear();
  }
  if (mSendFramerate.NumDataValues() > 0) {
    Accumulate(WEBRTC_VIDEO_ENCODER_FRAMERATE_AVG_PER_CALL,
               mSendFramerate.Mean());
    Accumulate(WEBRTC_VIDEO_ENCODER_FRAMERATE_10X_STD_DEV_PER_CALL,
               mSendFramerate.StandardDeviation() * 10);
    mSendFramerate.Clear();
  }

  if (mRecvBitrate.NumDataValues() > 0) {
    Accumulate(WEBRTC_VIDEO_DECODER_BITRATE_AVG_PER_CALL_KBPS,
               mRecvBitrate.Mean() / 1000);
    Accumulate(WEBRTC_VIDEO_DECODER_BITRATE_STD_DEV_PER_CALL_KBPS,
               mRecvBitrate.StandardDeviation() / 1000);
    mRecvBitrate.Clear();
  }
  if (mRecvFramerate.NumDataValues() > 0) {
    Accumulate(WEBRTC_VIDEO_DECODER_FRAMERATE_AVG_PER_CALL,
               mRecvFramerate.Mean());
    Accumulate(WEBRTC_VIDEO_DECODER_FRAMERATE_10X_STD_DEV_PER_CALL,
               mRecvFramerate.StandardDeviation() * 10);
    mRecvFramerate.Clear();
  }

  mControl.mReceiving.DisconnectIfConnected();
  mControl.mTransmitting.DisconnectIfConnected();
  mControl.mLocalSsrcs.DisconnectIfConnected();
  mControl.mLocalRtxSsrcs.DisconnectIfConnected();
  mControl.mLocalCname.DisconnectIfConnected();
  mControl.mLocalMid.DisconnectIfConnected();
  mControl.mRemoteSsrc.DisconnectIfConnected();
  mControl.mRemoteRtxSsrc.DisconnectIfConnected();
  mControl.mSyncGroup.DisconnectIfConnected();
  mControl.mLocalRecvRtpExtensions.DisconnectIfConnected();
  mControl.mLocalSendRtpExtensions.DisconnectIfConnected();
  mControl.mSendCodec.DisconnectIfConnected();
  mControl.mSendRtpRtcpConfig.DisconnectIfConnected();
  mControl.mRecvCodecs.DisconnectIfConnected();
  mControl.mRecvRtpRtcpConfig.DisconnectIfConnected();
  mControl.mCodecMode.DisconnectIfConnected();
  mWatchManager.Shutdown();

  mCall->UnregisterConduit(this);
  mSendPluginCreated.Disconnect();
  mSendPluginReleased.Disconnect();
  mRecvPluginCreated.Disconnect();
  mRecvPluginReleased.Disconnect();
  {
    MutexAutoLock lock(mMutex);
    DeleteSendStream();
    DeleteRecvStream();
  }
}

webrtc::VideoCodecMode WebrtcVideoConduit::CodecMode() const {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  return mControl.mCodecMode;
}

MediaConduitErrorCode WebrtcVideoConduit::AttachRenderer(
    RefPtr<mozilla::VideoRenderer> aVideoRenderer) {
  MOZ_ASSERT(NS_IsMainThread());

  CSFLogDebug(LOGTAG, "%s", __FUNCTION__);

  // null renderer
  if (!aVideoRenderer) {
    CSFLogError(LOGTAG, "%s NULL Renderer", __FUNCTION__);
    MOZ_ASSERT(false);
    return kMediaConduitInvalidRenderer;
  }

  // This function is called only from main, so we only need to protect against
  // modifying mRenderer while any webrtc.org code is trying to use it.
  {
    ReentrantMonitorAutoEnter enter(mTransportMonitor);
    mRenderer = aVideoRenderer;
    // Make sure the renderer knows the resolution
    mRenderer->FrameSizeChange(mReceivingWidth, mReceivingHeight);
  }

  return kMediaConduitNoError;
}

void WebrtcVideoConduit::DetachRenderer() {
  MOZ_ASSERT(NS_IsMainThread());

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  if (mRenderer) {
    mRenderer = nullptr;
  }
}

MediaConduitErrorCode WebrtcVideoConduit::SetTransmitterTransport(
    RefPtr<TransportInterface> aTransport) {
  MOZ_ASSERT(NS_IsMainThread());

  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  // set the transport
  mTransmitterTransport = aTransport;
  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcVideoConduit::SetReceiverTransport(
    RefPtr<TransportInterface> aTransport) {
  MOZ_ASSERT(NS_IsMainThread());

  CSFLogDebug(LOGTAG, "%s ", __FUNCTION__);

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  // set the transport
  mReceiverTransport = aTransport;
  return kMediaConduitNoError;
}

// XXX we need to figure out how to feed back changes in preferred capture
// resolution to the getUserMedia source.
void WebrtcVideoConduit::SelectSendResolution(unsigned short width,
                                              unsigned short height) {
  mMutex.AssertCurrentThreadOwns();
  // XXX This will do bandwidth-resolution adaptation as well - bug 877954
  // Enforce constraints
  if (mCurSendCodecConfig) {
    uint16_t max_width = mCurSendCodecConfig->mEncodingConstraints.maxWidth;
    uint16_t max_height = mCurSendCodecConfig->mEncodingConstraints.maxHeight;
    if (max_width || max_height) {
      max_width = max_width ? max_width : UINT16_MAX;
      max_height = max_height ? max_height : UINT16_MAX;
      ConstrainPreservingAspectRatio(max_width, max_height, &width, &height);
    }

    int max_fs = std::numeric_limits<int>::max();
    if (!mLockScaling) {
      max_fs = mVideoBroadcaster.wants().max_pixel_count;
    }
    // Limit resolution to max-fs
    if (mCurSendCodecConfig->mEncodingConstraints.maxFs) {
      // max-fs is in macroblocks, convert to pixels
      max_fs = std::min(
          max_fs,
          static_cast<int>(mCurSendCodecConfig->mEncodingConstraints.maxFs *
                           (16 * 16)));
    }
    mVideoAdapter->OnOutputFormatRequest(absl::optional<std::pair<int, int>>(),
                                         max_fs, absl::optional<int>());
  }

  unsigned int framerate = SelectSendFrameRate(
      mCurSendCodecConfig.ref(), mSendingFramerate, width, height);
  if (mSendingFramerate != framerate) {
    CSFLogDebug(LOGTAG, "%s: framerate changing to %u (from %u)", __FUNCTION__,
                framerate, mSendingFramerate);
    mSendingFramerate = framerate;
    mVideoStreamFactory->SetSendingFramerate(mSendingFramerate);
  }
}

void WebrtcVideoConduit::AddOrUpdateSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  if (!mRegisteredSinks.Contains(sink)) {
    mRegisteredSinks.AppendElement(sink);
  }
  auto oldWants = mVideoBroadcaster.wants();
  mVideoBroadcaster.AddOrUpdateSink(sink, wants);
  if (oldWants != mVideoBroadcaster.wants()) {
    mUpdateSendResolution = true;
  }
}

void WebrtcVideoConduit::RemoveSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());

  mRegisteredSinks.RemoveElement(sink);
  auto oldWants = mVideoBroadcaster.wants();
  mVideoBroadcaster.RemoveSink(sink);
  if (oldWants != mVideoBroadcaster.wants()) {
    mUpdateSendResolution = true;
  }
}

MediaConduitErrorCode WebrtcVideoConduit::SendVideoFrame(
    const webrtc::VideoFrame& frame) {
  // XXX Google uses a "timestamp_aligner" to translate timestamps from the
  // camera via TranslateTimestamp(); we should look at doing the same.  This
  // avoids sampling error when capturing frames, but google had to deal with
  // some broken cameras, include Logitech c920's IIRC.

  int cropWidth;
  int cropHeight;
  int adaptedWidth;
  int adaptedHeight;
  {
    MutexAutoLock lock(mMutex);
    if (mSendStreamConfig.rtp.ssrcs.empty()) {
      CSFLogVerbose(LOGTAG, "WebrtcVideoConduit %p %s No SSRC set", this,
                    __FUNCTION__);
      return kMediaConduitNoError;
    }
    if (!mCurSendCodecConfig) {
      CSFLogVerbose(LOGTAG, "WebrtcVideoConduit %p %s No send codec set", this,
                    __FUNCTION__);
      return kMediaConduitNoError;
    }
    CSFLogVerbose(LOGTAG, "WebrtcVideoConduit %p %s (send SSRC %u (0x%x))",
                  this, __FUNCTION__, mSendStreamConfig.rtp.ssrcs.front(),
                  mSendStreamConfig.rtp.ssrcs.front());

    bool updateSendResolution = mUpdateSendResolution.exchange(false);
    if (updateSendResolution || frame.width() != mLastWidth ||
        frame.height() != mLastHeight) {
      // See if we need to recalculate what we're sending.
      CSFLogVerbose(LOGTAG, "%s: call SelectSendResolution with %ux%u",
                    __FUNCTION__, frame.width(), frame.height());
      MOZ_ASSERT(frame.width() != 0 && frame.height() != 0);
      // Note coverity will flag this since it thinks they can be 0
      MOZ_ASSERT(mCurSendCodecConfig);

      mLastWidth = frame.width();
      mLastHeight = frame.height();
      SelectSendResolution(frame.width(), frame.height());
    }

    // adapt input video to wants of sink
    if (!mVideoBroadcaster.frame_wanted()) {
      return kMediaConduitNoError;
    }

    if (!mVideoAdapter->AdaptFrameResolution(
            frame.width(), frame.height(),
            frame.timestamp_us() * rtc::kNumNanosecsPerMicrosec, &cropWidth,
            &cropHeight, &adaptedWidth, &adaptedHeight)) {
      // VideoAdapter dropped the frame.
      return kMediaConduitNoError;
    }
  }

  // If we have zero width or height, drop the frame here. Attempting to send
  // it will cause all sorts of problems in the webrtc.org code.
  if (cropWidth == 0 || cropHeight == 0) {
    return kMediaConduitNoError;
  }

  int cropX = (frame.width() - cropWidth) / 2;
  int cropY = (frame.height() - cropHeight) / 2;

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer;
  if (adaptedWidth == frame.width() && adaptedHeight == frame.height()) {
    // No adaption - optimized path.
    buffer = frame.video_frame_buffer();
  } else {
    // Adapted I420 frame.
    rtc::scoped_refptr<webrtc::I420Buffer> i420Buffer =
        mBufferPool.CreateBuffer(adaptedWidth, adaptedHeight);
    if (!i420Buffer) {
      CSFLogWarn(LOGTAG, "Creating a buffer for scaling failed, pool is empty");
      return kMediaConduitNoError;
    }
    i420Buffer->CropAndScaleFrom(*frame.video_frame_buffer()->GetI420(), cropX,
                                 cropY, cropWidth, cropHeight);
    buffer = i420Buffer;
  }

  mVideoBroadcaster.OnFrame(webrtc::VideoFrame(
      buffer, frame.timestamp(), frame.render_time_ms(), frame.rotation()));

  return kMediaConduitNoError;
}

// Transport Layer Callbacks

void WebrtcVideoConduit::DeliverPacket(rtc::CopyOnWriteBuffer packet,
                                       PacketType type) {
  ASSERT_ON_THREAD(mStsThread);

  MOZ_ALWAYS_SUCCEEDS(mCallThread->Dispatch(NS_NewRunnableFunction(
      __func__, [this, self = RefPtr<WebrtcVideoConduit>(this),
                 packet = std::move(packet), type] {
        if (!mCall->Call()) {
          return;
        }

        // Bug 1499796 - we need to get passed the time the packet was received
        webrtc::PacketReceiver::DeliveryStatus status =
            mCall->Call()->Receiver()->DeliverPacket(webrtc::MediaType::VIDEO,
                                                     std::move(packet), -1);

        if (status != webrtc::PacketReceiver::DELIVERY_OK) {
          CSFLogError(LOGTAG, "%s DeliverPacket Failed for %s packet, %d",
                      __FUNCTION__, type == PacketType::RTP ? "RTP" : "RTCP",
                      status);
        }
      })));
}

void WebrtcVideoConduit::ReceivedRTPPacket(const uint8_t* data, int len,
                                           webrtc::RTPHeader& header) {
  ASSERT_ON_THREAD(mStsThread);

  if (mAllowSsrcChange || mWaitingForSignaledSsrc) {
    // Handle the unknown ssrc (and ssrc-not-signaled case).
    // We can't just do this here; it has to happen on the Call thread.
    // We also don't want to drop the packet, nor block this thread, so we hold
    // the packet (and any following) for inserting once the SSRC is set.
    if (mRtpPacketQueue.IsQueueActive()) {
      mRtpPacketQueue.Enqueue(rtc::CopyOnWriteBuffer(data, len));
      return;
    }

    bool switchRequired = mRecvSSRC != header.ssrc;
    if (switchRequired) {
      // We need to check that the newly received ssrc is not already
      // associated with ulpfec or rtx. This is how webrtc.org handles
      // things, see https://codereview.webrtc.org/1226093002.
      MutexAutoLock lock(mMutex);
      const webrtc::VideoReceiveStream::Config::Rtp& rtp =
          mRecvStreamConfig.rtp;
      switchRequired =
          rtp.rtx_associated_payload_types.find(header.payloadType) ==
              rtp.rtx_associated_payload_types.end() &&
          rtp.ulpfec_payload_type != header.payloadType;
    }

    if (switchRequired) {
      // a new switch needs to be done
      // any queued packets are from a previous switch that hasn't completed
      // yet; drop them and only process the latest SSRC
      mRtpPacketQueue.Clear();
      mRtpPacketQueue.Enqueue(rtc::CopyOnWriteBuffer(data, len));

      CSFLogDebug(LOGTAG, "%s: switching from SSRC %u to %u", __FUNCTION__,
                  static_cast<uint32_t>(mRecvSSRC), header.ssrc);
      // we "switch" here immediately, but buffer until the queue is released
      mRecvSSRC = mRemoteSSRC = header.ssrc;

      InvokeAsync(mCallThread, __func__,
                  [this, self = RefPtr<WebrtcVideoConduit>(this),
                   ssrc = header.ssrc]() mutable {
                    // TODO: This is problematic with rtx
                    // enabled, we don't know if new ssrc is for
                    // rtx or not. This will likely re-create the
                    // VideoReceiveStream
                    MutexAutoLock lock(mMutex);
                    SetRemoteSSRCLocked(ssrc, 0);
                    return GenericPromise::CreateAndResolve(true, __func__);
                  })
          ->Then(mStsThread, __func__,
                 [this, self = RefPtr<WebrtcVideoConduit>(this),
                  ssrc = header.ssrc]() mutable {
                   // We want to unblock the queued packets on the original
                   // thread
                   if (ssrc != mRecvSSRC) {
                     // this is an intermediate switch; another is in-flight
                     return;
                   }
                   mRtpPacketQueue.DequeueAll(this);
                 });
      return;
    }
  }

  CSFLogVerbose(LOGTAG, "%s: seq# %u, Len %d, SSRC %u (0x%x) ", __FUNCTION__,
                (uint16_t)ntohs(((uint16_t*)data)[1]), len,
                (uint32_t)ntohl(((uint32_t*)data)[2]),
                (uint32_t)ntohl(((uint32_t*)data)[2]));

  DeliverPacket(rtc::CopyOnWriteBuffer(data, len), PacketType::RTP);
}

void WebrtcVideoConduit::ReceivedRTCPPacket(const uint8_t* data, int len) {
  ASSERT_ON_THREAD(mStsThread);

  CSFLogVerbose(LOGTAG, " %s Len %d ", __FUNCTION__, len);

  DeliverPacket(rtc::CopyOnWriteBuffer(data, len), PacketType::RTCP);

  // TODO(bug 1496533): We will need to keep separate timestamps for each SSRC,
  // and for each SSRC we will need to keep a timestamp for SR and RR.
  mLastRtcpReceived = Some(GetNow());
}

// TODO(bug 1496533): We will need to add a type (ie; SR or RR) param here, or
// perhaps break this function into two functions, one for each type.
Maybe<DOMHighResTimeStamp> WebrtcVideoConduit::LastRtcpReceived() const {
  ASSERT_ON_THREAD(mStsThread);
  return mLastRtcpReceived;
}

DOMHighResTimeStamp WebrtcVideoConduit::GetNow() const {
  return mCall->GetNow();
}

MediaConduitErrorCode WebrtcVideoConduit::StopTransmittingLocked() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  mMutex.AssertCurrentThreadOwns();

  if (mEngineTransmitting) {
    if (mSendStream) {
      CSFLogDebug(LOGTAG, "%s Engine Already Sending. Attemping to Stop ",
                  __FUNCTION__);
      mSendStream->Stop();
    }

    mEngineTransmitting = false;
  }
  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcVideoConduit::StartTransmittingLocked() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  mMutex.AssertCurrentThreadOwns();

  if (mEngineTransmitting) {
    return kMediaConduitNoError;
  }

  CSFLogDebug(LOGTAG, "%s Attemping to start... ", __FUNCTION__);
  // Start Transmitting on the video engine
  if (!mSendStream) {
    MediaConduitErrorCode rval = CreateSendStream();
    if (rval != kMediaConduitNoError) {
      CSFLogError(LOGTAG, "%s Start Send Error %d ", __FUNCTION__, rval);
      return rval;
    }
  }

  mSendStream->Start();
  // XXX File a bug to consider hooking this up to the state of mtransport
  mCall->Call()->SignalChannelNetworkState(webrtc::MediaType::VIDEO,
                                           webrtc::kNetworkUp);
  mEngineTransmitting = true;

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcVideoConduit::StopReceivingLocked() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  mMutex.AssertCurrentThreadOwns();

  // Are we receiving already? If so, stop receiving and playout
  // since we can't apply new recv codec when the engine is playing.
  if (mEngineReceiving && mRecvStream) {
    CSFLogDebug(LOGTAG, "%s Engine Already Receiving . Attemping to Stop ",
                __FUNCTION__);
    mRecvStream->Stop();
  }

  mEngineReceiving = false;

  return kMediaConduitNoError;
}

MediaConduitErrorCode WebrtcVideoConduit::StartReceivingLocked() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  mMutex.AssertCurrentThreadOwns();

  if (mEngineReceiving) {
    return kMediaConduitNoError;
  }

  CSFLogDebug(LOGTAG, "%s Attemping to start... (SSRC %u (0x%x))", __FUNCTION__,
              mRecvStreamConfig.rtp.remote_ssrc,
              mRecvStreamConfig.rtp.remote_ssrc);
  // Start Receiving on the video engine
  if (!mRecvStream) {
    MediaConduitErrorCode rval = CreateRecvStream();
    if (rval != kMediaConduitNoError) {
      CSFLogError(LOGTAG, "%s Start Receive Error %d ", __FUNCTION__, rval);
      return rval;
    }
  }

  mRecvStream->Start();
  // XXX File a bug to consider hooking this up to the state of mtransport
  mCall->Call()->SignalChannelNetworkState(webrtc::MediaType::VIDEO,
                                           webrtc::kNetworkUp);
  mEngineReceiving = true;

  return kMediaConduitNoError;
}

// WebRTC::RTP Callback Implementation
// Called on MTG thread
bool WebrtcVideoConduit::SendRtp(const uint8_t* packet, size_t length,
                                 const webrtc::PacketOptions& options) {
  CSFLogVerbose(LOGTAG, "%s Sent RTP Packet seq %d, len %lu, SSRC %u (0x%x)",
                __FUNCTION__, (uint16_t)ntohs(*((uint16_t*)&packet[2])),
                (unsigned long)length,
                (uint32_t)ntohl(*((uint32_t*)&packet[8])),
                (uint32_t)ntohl(*((uint32_t*)&packet[8])));

  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  if (!mTransmitterTransport ||
      NS_FAILED(mTransmitterTransport->SendRtpPacket(packet, length))) {
    CSFLogError(LOGTAG, "%s RTP Packet Send Failed ", __FUNCTION__);
    return false;
  }
  if (options.packet_id >= 0) {
    int64_t now_ms = PR_Now() / 1000;
    MOZ_ALWAYS_SUCCEEDS(mCallThread->Dispatch(NS_NewRunnableFunction(
        __func__, [call = mCall, packet_id = options.packet_id, now_ms] {
          if (call->Call()) {
            call->Call()->OnSentPacket({packet_id, now_ms});
          }
        })));
  }
  return true;
}

// Called from multiple threads including webrtc Process thread
bool WebrtcVideoConduit::SendRtcp(const uint8_t* packet, size_t length) {
  CSFLogVerbose(LOGTAG, "%s : len %lu ", __FUNCTION__, (unsigned long)length);
  // We come here if we have only one pipeline/conduit setup,
  // such as for unidirectional streams.
  // We also end up here if we are receiving
  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  if (mReceiverTransport &&
      NS_SUCCEEDED(mReceiverTransport->SendRtcpPacket(packet, length))) {
    // Might be a sender report, might be a receiver report, we don't know.
    CSFLogDebug(LOGTAG, "%s Sent RTCP Packet ", __FUNCTION__);
    return true;
  }
  if (mTransmitterTransport &&
      NS_SUCCEEDED(mTransmitterTransport->SendRtcpPacket(packet, length))) {
    return true;
  }

  CSFLogError(LOGTAG, "%s RTCP Packet Send Failed ", __FUNCTION__);
  return false;
}

void WebrtcVideoConduit::OnFrame(const webrtc::VideoFrame& video_frame) {
  CSFLogVerbose(LOGTAG, "%s: recv SSRC %u (0x%x), size %ux%u", __FUNCTION__,
                static_cast<uint32_t>(mRecvSSRC),
                static_cast<uint32_t>(mRecvSSRC), video_frame.width(),
                video_frame.height());
  ReentrantMonitorAutoEnter enter(mTransportMonitor);

  if (!mRenderer) {
    CSFLogError(LOGTAG, "%s Renderer is NULL  ", __FUNCTION__);
    return;
  }

  bool needsNewHistoryElement = !mReceivedFrameHistory.mEntries.Length();

  if (mReceivingWidth != video_frame.width() ||
      mReceivingHeight != video_frame.height()) {
    mReceivingWidth = video_frame.width();
    mReceivingHeight = video_frame.height();
    mRenderer->FrameSizeChange(mReceivingWidth, mReceivingHeight);
    needsNewHistoryElement = true;
  }

  uint32_t remoteSsrc;
  if (!GetRemoteSSRC(&remoteSsrc) && needsNewHistoryElement) {
    // Frame was decoded after the connection ended
    return;
  }

  if (!needsNewHistoryElement) {
    auto& currentEntry = mReceivedFrameHistory.mEntries.LastElement();
    needsNewHistoryElement =
        currentEntry.mRotationAngle !=
            static_cast<unsigned long>(video_frame.rotation()) ||
        currentEntry.mLocalSsrc != mRecvSSRC ||
        currentEntry.mRemoteSsrc != remoteSsrc;
  }

  // Record frame history
  const auto historyNow = mCall->GetNow();
  if (needsNewHistoryElement) {
    dom::RTCVideoFrameHistoryEntryInternal frameHistoryElement;
    frameHistoryElement.mConsecutiveFrames = 0;
    frameHistoryElement.mWidth = video_frame.width();
    frameHistoryElement.mHeight = video_frame.height();
    frameHistoryElement.mRotationAngle =
        static_cast<unsigned long>(video_frame.rotation());
    frameHistoryElement.mFirstFrameTimestamp = historyNow;
    frameHistoryElement.mLocalSsrc = mRecvSSRC;
    frameHistoryElement.mRemoteSsrc = remoteSsrc;
    if (!mReceivedFrameHistory.mEntries.AppendElement(frameHistoryElement,
                                                      fallible)) {
      mozalloc_handle_oom(0);
    }
  }
  auto& currentEntry = mReceivedFrameHistory.mEntries.LastElement();

  currentEntry.mConsecutiveFrames++;
  currentEntry.mLastFrameTimestamp = historyNow;
  // Attempt to retrieve an timestamp encoded in the image pixels if enabled.
  if (mVideoLatencyTestEnable && mReceivingWidth && mReceivingHeight) {
    uint64_t now = PR_Now();
    uint64_t timestamp = 0;
    uint8_t* data = const_cast<uint8_t*>(
        video_frame.video_frame_buffer()->GetI420()->DataY());
    bool ok = YuvStamper::Decode(
        mReceivingWidth, mReceivingHeight, mReceivingWidth, data,
        reinterpret_cast<unsigned char*>(&timestamp), sizeof(timestamp), 0, 0);
    if (ok) {
      VideoLatencyUpdate(now - timestamp);
    }
  }

  mRenderer->RenderVideoFrame(*video_frame.video_frame_buffer(),
                              video_frame.timestamp(),
                              video_frame.render_time_ms());
}

bool WebrtcVideoConduit::AddFrameHistory(
    dom::Sequence<dom::RTCVideoFrameHistoryInternal>* outHistories) const {
  ReentrantMonitorAutoEnter enter(mTransportMonitor);
  if (!outHistories->AppendElement(mReceivedFrameHistory, fallible)) {
    mozalloc_handle_oom(0);
    return false;
  }
  return true;
}

void WebrtcVideoConduit::DumpCodecDB() const {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());

  for (auto& entry : mControl.mConfiguredRecvCodecs) {
    CSFLogDebug(LOGTAG, "Payload Name: %s", entry.mName.c_str());
    CSFLogDebug(LOGTAG, "Payload Type: %d", entry.mType);
    CSFLogDebug(LOGTAG, "Payload Max Frame Size: %d",
                entry.mEncodingConstraints.maxFs);
    CSFLogDebug(LOGTAG, "Payload Max Frame Rate: %d",
                entry.mEncodingConstraints.maxFps);
  }
}

void WebrtcVideoConduit::VideoLatencyUpdate(uint64_t newSample) {
  mTransportMonitor.AssertCurrentThreadIn();

  mVideoLatencyAvg =
      (sRoundingPadding * newSample + sAlphaNum * mVideoLatencyAvg) / sAlphaDen;
}

uint64_t WebrtcVideoConduit::MozVideoLatencyAvg() {
  mTransportMonitor.AssertCurrentThreadIn();

  return mVideoLatencyAvg / sRoundingPadding;
}

void WebrtcVideoConduit::CollectTelemetryData() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());

  if (mEngineTransmitting) {
    webrtc::VideoSendStream::Stats stats = mSendStream->GetStats();
    mSendBitrate.Push(stats.media_bitrate_bps);
    mSendFramerate.Push(stats.encode_frame_rate);
  }
  if (mEngineReceiving) {
    webrtc::VideoReceiveStream::Stats stats = mRecvStream->GetStats();
    mRecvBitrate.Push(stats.total_bitrate_bps);
    mRecvFramerate.Push(stats.decode_frame_rate);
  }
}

void WebrtcVideoConduit::OnRtcpBye() { mRtcpByeEvent.Notify(); }

void WebrtcVideoConduit::OnRtcpTimeout() { mRtcpTimeoutEvent.Notify(); }

std::vector<webrtc::RtpSource> WebrtcVideoConduit::GetUpstreamRtpSources()
    const {
  MOZ_ASSERT(NS_IsMainThread());
  std::vector<webrtc::RtpSource> sources;
  {
    MutexAutoLock lock(mMutex);
    if (mRecvStream) {
      sources = mRecvStream->GetSources();
    }
  }
  return sources;
}

bool WebrtcVideoConduit::HasCodecPluginID(uint64_t aPluginID) const {
  MOZ_ASSERT(NS_IsMainThread());

  return mSendCodecPluginIDs.Contains(aPluginID) ||
         mRecvCodecPluginIDs.Contains(aPluginID);
}

bool WebrtcVideoConduit::HasH264Hardware() {
  nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
  if (!gfxInfo) {
    return false;
  }
  int32_t status;
  nsCString discardFailureId;
  return NS_SUCCEEDED(gfxInfo->GetFeatureStatus(
             nsIGfxInfo::FEATURE_WEBRTC_HW_ACCELERATION_H264, discardFailureId,
             &status)) &&
         status == nsIGfxInfo::FEATURE_STATUS_OK;
}

}  // namespace mozilla
