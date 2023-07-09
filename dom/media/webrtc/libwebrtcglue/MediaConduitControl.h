/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_MEDIACONDUITCONTROL_H_
#define DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_MEDIACONDUITCONTROL_H_

#include "jsapi/RTCDTMFSender.h"  // For DtmfEvent
#include "mozilla/StateMirroring.h"
#include "RtpRtcpConfig.h"
#include <vector>
#include <string>
#include "mozilla/Maybe.h"
#include "CodecConfig.h"                   // For Audio/VideoCodecConfig
#include "api/rtp_parameters.h"            // For webrtc::RtpExtension
#include "api/video_codecs/video_codec.h"  // For webrtc::VideoCodecMode
#include "FrameTransformerProxy.h"

namespace mozilla {

using RtpExtList = std::vector<webrtc::RtpExtension>;
using Ssrc = uint32_t;
using Ssrcs = std::vector<uint32_t>;

/**
 * These are the interfaces used to control the async conduits. Some parameters
 * are common, and some are tied to the conduit type. See
 * MediaSessionConduit::InitConduitControl for how they are used.
 *
 * Put simply, the implementer of the interfaces below may set its canonicals on
 * any thread, and the conduits will react to those changes accordingly, on
 * their dedicated worker thread. One instance of these interfaces could control
 * multiple conduits as each canonical can connect to any number of mirrors.
 */

class MediaConduitControlInterface {
 public:
  virtual Canonical<bool>& CanonicalReceiving() = 0;
  virtual Canonical<bool>& CanonicalTransmitting() = 0;
  virtual Canonical<Ssrcs>& CanonicalLocalSsrcs() = 0;
  virtual Canonical<std::string>& CanonicalLocalCname() = 0;
  virtual Canonical<std::string>& CanonicalMid() = 0;
  virtual Canonical<Ssrc>& CanonicalRemoteSsrc() = 0;
  virtual Canonical<std::string>& CanonicalSyncGroup() = 0;
  virtual Canonical<RtpExtList>& CanonicalLocalRecvRtpExtensions() = 0;
  virtual Canonical<RtpExtList>& CanonicalLocalSendRtpExtensions() = 0;
  virtual Canonical<RefPtr<FrameTransformerProxy>>&
  CanonicalFrameTransformerProxySend() = 0;
  virtual Canonical<RefPtr<FrameTransformerProxy>>&
  CanonicalFrameTransformerProxyRecv() = 0;
};

class AudioConduitControlInterface : public MediaConduitControlInterface {
 public:
  virtual Canonical<Maybe<AudioCodecConfig>>& CanonicalAudioSendCodec() = 0;
  virtual Canonical<std::vector<AudioCodecConfig>>&
  CanonicalAudioRecvCodecs() = 0;
  virtual MediaEventSource<DtmfEvent>& OnDtmfEvent() = 0;
};

class VideoConduitControlInterface : public MediaConduitControlInterface {
 public:
  virtual Canonical<Ssrcs>& CanonicalLocalVideoRtxSsrcs() = 0;
  virtual Canonical<Ssrc>& CanonicalRemoteVideoRtxSsrc() = 0;
  virtual Canonical<Maybe<VideoCodecConfig>>& CanonicalVideoSendCodec() = 0;
  virtual Canonical<Maybe<RtpRtcpConfig>>&
  CanonicalVideoSendRtpRtcpConfig() = 0;
  virtual Canonical<std::vector<VideoCodecConfig>>&
  CanonicalVideoRecvCodecs() = 0;
  virtual Canonical<Maybe<RtpRtcpConfig>>&
  CanonicalVideoRecvRtpRtcpConfig() = 0;
  virtual Canonical<webrtc::VideoCodecMode>& CanonicalVideoCodecMode() = 0;
};

}  // namespace mozilla

#endif
