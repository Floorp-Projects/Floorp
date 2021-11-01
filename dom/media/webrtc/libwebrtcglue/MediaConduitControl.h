/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_MEDIACONDUITCONTROL_H_
#define DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_MEDIACONDUITCONTROL_H_

#include "jsapi/RTCDTMFSender.h"
#include "MediaConduitInterface.h"
#include "mozilla/StateMirroring.h"
#include "RtpRtcpConfig.h"

namespace mozilla {

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
  virtual AbstractCanonical<bool>* CanonicalReceiving() = 0;
  virtual AbstractCanonical<bool>* CanonicalTransmitting() = 0;
  virtual AbstractCanonical<Ssrcs>* CanonicalLocalSsrcs() = 0;
  virtual AbstractCanonical<std::string>* CanonicalLocalCname() = 0;
  virtual AbstractCanonical<std::string>* CanonicalLocalMid() = 0;
  virtual AbstractCanonical<Ssrc>* CanonicalRemoteSsrc() = 0;
  virtual AbstractCanonical<std::string>* CanonicalSyncGroup() = 0;
  virtual AbstractCanonical<RtpExtList>* CanonicalLocalRecvRtpExtensions() = 0;
  virtual AbstractCanonical<RtpExtList>* CanonicalLocalSendRtpExtensions() = 0;
};

class AudioConduitControlInterface : public MediaConduitControlInterface {
 public:
  virtual AbstractCanonical<Maybe<AudioCodecConfig>>*
  CanonicalAudioSendCodec() = 0;
  virtual AbstractCanonical<std::vector<AudioCodecConfig>>*
  CanonicalAudioRecvCodecs() = 0;
  virtual MediaEventSource<DtmfEvent>& OnDtmfEvent() = 0;
};

class VideoConduitControlInterface : public MediaConduitControlInterface {
 public:
  virtual AbstractCanonical<Ssrcs>* CanonicalLocalVideoRtxSsrcs() = 0;
  virtual AbstractCanonical<Ssrc>* CanonicalRemoteVideoRtxSsrc() = 0;
  virtual AbstractCanonical<Maybe<VideoCodecConfig>>*
  CanonicalVideoSendCodec() = 0;
  virtual AbstractCanonical<Maybe<RtpRtcpConfig>>*
  CanonicalVideoSendRtpRtcpConfig() = 0;
  virtual AbstractCanonical<std::vector<VideoCodecConfig>>*
  CanonicalVideoRecvCodecs() = 0;
  virtual AbstractCanonical<Maybe<RtpRtcpConfig>>*
  CanonicalVideoRecvRtpRtcpConfig() = 0;
  virtual AbstractCanonical<webrtc::VideoCodecMode>*
  CanonicalVideoCodecMode() = 0;
};

}  // namespace mozilla

#endif
