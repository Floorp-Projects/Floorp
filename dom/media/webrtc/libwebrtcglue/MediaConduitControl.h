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

class MediaConduitController {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaConduitController)
  virtual void Shutdown() = 0;

 protected:
  virtual ~MediaConduitController() = default;
};

#define INIT_MIRROR(name, val) \
  name(mCallThread, val, "AudioConduitController::" #name " (Mirror)")
class AudioConduitController : public MediaConduitController {
 public:
  AudioConduitController(RefPtr<AudioSessionConduit> aConduit,
                         RefPtr<AbstractThread> aCallThread,
                         AudioConduitControlInterface* aControl)
      : mConduit(std::move(aConduit)),
        mCallThread(std::move(aCallThread)),
        mWatchManager(this, mCallThread),
        INIT_MIRROR(mReceiving, false),
        INIT_MIRROR(mTransmitting, false),
        INIT_MIRROR(mLocalSsrcs, Ssrcs()),
        INIT_MIRROR(mLocalCname, std::string()),
        INIT_MIRROR(mLocalMid, std::string()),
        INIT_MIRROR(mRemoteSsrc, 0),
        INIT_MIRROR(mSyncGroup, std::string()),
        INIT_MIRROR(mLocalRecvRtpExtensions, RtpExtList()),
        INIT_MIRROR(mLocalSendRtpExtensions, RtpExtList()),
        INIT_MIRROR(mAudioSendCodec, Nothing()),
        INIT_MIRROR(mAudioRecvCodecs, std::vector<AudioCodecConfig>()) {
    MOZ_ASSERT(mCallThread->IsOnCurrentThread());

    mReceiving.Connect(aControl->CanonicalReceiving());
    mTransmitting.Connect(aControl->CanonicalTransmitting());
    mLocalSsrcs.Connect(aControl->CanonicalLocalSsrcs());
    mLocalCname.Connect(aControl->CanonicalLocalCname());
    mLocalMid.Connect(aControl->CanonicalLocalMid());
    mRemoteSsrc.Connect(aControl->CanonicalRemoteSsrc());
    mSyncGroup.Connect(aControl->CanonicalSyncGroup());
    mLocalRecvRtpExtensions.Connect(
        aControl->CanonicalLocalRecvRtpExtensions());
    mLocalSendRtpExtensions.Connect(
        aControl->CanonicalLocalSendRtpExtensions());
    mAudioSendCodec.Connect(aControl->CanonicalAudioSendCodec());
    mAudioRecvCodecs.Connect(aControl->CanonicalAudioRecvCodecs());
    mOnDtmfEventListener = aControl->OnDtmfEvent().Connect(
        mCallThread, this, &AudioConduitController::OnDtmfEvent);

    mWatchManager.Watch(mReceiving, &AudioConduitController::ReceivingChanged);
    mWatchManager.Watch(mTransmitting,
                        &AudioConduitController::TransmittingChanged);
    mWatchManager.Watch(mLocalSsrcs,
                        &AudioConduitController::LocalSsrcsChanged);
    mWatchManager.Watch(mLocalCname,
                        &AudioConduitController::LocalCnameChanged);
    mWatchManager.Watch(mLocalMid, &AudioConduitController::LocalMidChanged);
    mWatchManager.Watch(mRemoteSsrc,
                        &AudioConduitController::RemoteSsrcChanged);
    mWatchManager.Watch(mSyncGroup, &AudioConduitController::SyncGroupChanged);
    mWatchManager.Watch(mLocalRecvRtpExtensions,
                        &AudioConduitController::LocalRecvRtpExtensionsChanged);
    mWatchManager.Watch(mLocalSendRtpExtensions,
                        &AudioConduitController::LocalSendRtpExtensionsChanged);
    mWatchManager.Watch(mAudioSendCodec,
                        &AudioConduitController::AudioSendCodecChanged);
    mWatchManager.Watch(mAudioRecvCodecs,
                        &AudioConduitController::AudioRecvCodecsChanged);
  }

  void Shutdown() override {
    MOZ_ASSERT(mCallThread->IsOnCurrentThread());
    mReceiving.DisconnectIfConnected();
    mTransmitting.DisconnectIfConnected();
    mLocalSsrcs.DisconnectIfConnected();
    mLocalCname.DisconnectIfConnected();
    mLocalMid.DisconnectIfConnected();
    mRemoteSsrc.DisconnectIfConnected();
    mSyncGroup.DisconnectIfConnected();
    mLocalRecvRtpExtensions.DisconnectIfConnected();
    mLocalSendRtpExtensions.DisconnectIfConnected();
    mAudioSendCodec.DisconnectIfConnected();
    mAudioRecvCodecs.DisconnectIfConnected();
    mOnDtmfEventListener.DisconnectIfExists();
    mWatchManager.Shutdown();
    mConduit->Shutdown();
  }

 private:
  void ReceivingChanged() {
    if (mReceiving) {
      mConduit->StartReceiving();
    } else {
      mConduit->StopReceiving();
    }
  }
  void TransmittingChanged() {
    if (mTransmitting) {
      mConduit->StartTransmitting();
    } else {
      mConduit->StopTransmitting();
    }
  }
  void LocalSsrcsChanged() {
    MOZ_ALWAYS_TRUE(mConduit->SetLocalSSRCs(mLocalSsrcs, /* rtx */ Ssrcs()));
  }
  void LocalCnameChanged() {
    MOZ_ALWAYS_TRUE(mConduit->SetLocalCNAME(mLocalCname.Ref().c_str()));
  }
  void LocalMidChanged() { MOZ_ALWAYS_TRUE(mConduit->SetLocalMID(mLocalMid)); }
  void RemoteSsrcChanged() {
    MOZ_ALWAYS_TRUE(mConduit->SetRemoteSSRC(mRemoteSsrc, /* rtx */ 0));
  }
  void SyncGroupChanged() { mConduit->SetSyncGroup(mSyncGroup); }
  void LocalRecvRtpExtensionsChanged() {
    MOZ_ALWAYS_TRUE(
        kMediaConduitNoError ==
        mConduit->SetLocalRTPExtensions(
            MediaSessionConduitLocalDirection::kRecv, mLocalRecvRtpExtensions));
  }
  void LocalSendRtpExtensionsChanged() {
    MOZ_ALWAYS_TRUE(
        kMediaConduitNoError ==
        mConduit->SetLocalRTPExtensions(
            MediaSessionConduitLocalDirection::kSend, mLocalSendRtpExtensions));
  }
  void AudioSendCodecChanged() {
    MOZ_ALWAYS_TRUE(kMediaConduitNoError ==
                    mConduit->ConfigureSendMediaCodec(*mAudioSendCodec.Ref()));
  }
  void AudioRecvCodecsChanged() {
    MOZ_ALWAYS_TRUE(kMediaConduitNoError ==
                    mConduit->ConfigureRecvMediaCodecs(mAudioRecvCodecs));
  }
  void OnDtmfEvent(const DtmfEvent& aEvent) {
    mConduit->InsertDTMFTone(aEvent.mPayloadType, aEvent.mPayloadFrequency,
                             aEvent.mEventCode, aEvent.mLengthMs);
  }

  const RefPtr<AudioSessionConduit> mConduit;
  const RefPtr<AbstractThread> mCallThread;
  WatchManager<AudioConduitController> mWatchManager;
  Mirror<bool> mReceiving;
  Mirror<bool> mTransmitting;
  Mirror<Ssrcs> mLocalSsrcs;
  Mirror<std::string> mLocalCname;
  Mirror<std::string> mLocalMid;
  Mirror<Ssrc> mRemoteSsrc;
  Mirror<std::string> mSyncGroup;
  Mirror<RtpExtList> mLocalRecvRtpExtensions;
  Mirror<RtpExtList> mLocalSendRtpExtensions;
  Mirror<Maybe<AudioCodecConfig>> mAudioSendCodec;
  Mirror<std::vector<AudioCodecConfig>> mAudioRecvCodecs;
  MediaEventListener mOnDtmfEventListener;
};
#undef INIT_MIRROR

#define INIT_MIRROR(name, val) \
  name(mCallThread, val, "VideoConduitController::" #name " (Mirror)")
class VideoConduitController : public MediaConduitController {
 public:
  VideoConduitController(RefPtr<VideoSessionConduit> aConduit,
                         RefPtr<AbstractThread> aCallThread,
                         VideoConduitControlInterface* aControl)
      : mConduit(std::move(aConduit)),
        mCallThread(std::move(aCallThread)),
        mWatchManager(this, mCallThread),
        INIT_MIRROR(mReceiving, false),
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
        INIT_MIRROR(mVideoSendCodec, Nothing()),
        INIT_MIRROR(mVideoSendRtpRtcpConfig, Nothing()),
        INIT_MIRROR(mVideoRecvCodecs, std::vector<VideoCodecConfig>()),
        INIT_MIRROR(mVideoRecvRtpRtcpConfig, Nothing()),
        INIT_MIRROR(mVideoCodecMode, webrtc::VideoCodecMode::kRealtimeVideo) {
    MOZ_ASSERT(mCallThread->IsOnCurrentThread());

    mReceiving.Connect(aControl->CanonicalReceiving());
    mTransmitting.Connect(aControl->CanonicalTransmitting());
    mLocalSsrcs.Connect(aControl->CanonicalLocalSsrcs());
    mLocalRtxSsrcs.Connect(aControl->CanonicalLocalVideoRtxSsrcs());
    mLocalCname.Connect(aControl->CanonicalLocalCname());
    mLocalMid.Connect(aControl->CanonicalLocalMid());
    mRemoteSsrc.Connect(aControl->CanonicalRemoteSsrc());
    mRemoteRtxSsrc.Connect(aControl->CanonicalRemoteVideoRtxSsrc());
    mSyncGroup.Connect(aControl->CanonicalSyncGroup());
    mLocalRecvRtpExtensions.Connect(
        aControl->CanonicalLocalRecvRtpExtensions());
    mLocalSendRtpExtensions.Connect(
        aControl->CanonicalLocalSendRtpExtensions());
    mVideoSendCodec.Connect(aControl->CanonicalVideoSendCodec());
    mVideoSendRtpRtcpConfig.Connect(
        aControl->CanonicalVideoSendRtpRtcpConfig());
    mVideoRecvCodecs.Connect(aControl->CanonicalVideoRecvCodecs());
    mVideoRecvRtpRtcpConfig.Connect(
        aControl->CanonicalVideoRecvRtpRtcpConfig());
    mVideoCodecMode.Connect(aControl->CanonicalVideoCodecMode());

    mWatchManager.Watch(mReceiving, &VideoConduitController::ReceivingChanged);
    mWatchManager.Watch(mTransmitting,
                        &VideoConduitController::TransmittingChanged);
    mWatchManager.Watch(mLocalSsrcs,
                        &VideoConduitController::LocalSsrcsChanged);
    mWatchManager.Watch(mLocalRtxSsrcs,
                        &VideoConduitController::LocalSsrcsChanged);
    mWatchManager.Watch(mLocalCname,
                        &VideoConduitController::LocalCnameChanged);
    mWatchManager.Watch(mLocalMid, &VideoConduitController::LocalMidChanged);
    mWatchManager.Watch(mRemoteSsrc,
                        &VideoConduitController::RemoteSsrcChanged);
    mWatchManager.Watch(mRemoteRtxSsrc,
                        &VideoConduitController::RemoteSsrcChanged);
    mWatchManager.Watch(mSyncGroup, &VideoConduitController::SyncGroupChanged);
    mWatchManager.Watch(mLocalRecvRtpExtensions,
                        &VideoConduitController::LocalRecvRtpExtensionsChanged);
    mWatchManager.Watch(mLocalSendRtpExtensions,
                        &VideoConduitController::LocalSendRtpExtensionsChanged);
    mWatchManager.Watch(mVideoSendCodec,
                        &VideoConduitController::VideoSendCodecChanged);
    mWatchManager.Watch(mVideoSendRtpRtcpConfig,
                        &VideoConduitController::VideoSendCodecChanged);
    mWatchManager.Watch(mVideoRecvCodecs,
                        &VideoConduitController::VideoRecvCodecsChanged);
    mWatchManager.Watch(mVideoRecvRtpRtcpConfig,
                        &VideoConduitController::VideoRecvCodecsChanged);
    mWatchManager.Watch(mVideoCodecMode,
                        &VideoConduitController::VideoCodecModeChanged);
  }

  void Shutdown() override {
    MOZ_ASSERT(mCallThread->IsOnCurrentThread());
    mReceiving.DisconnectIfConnected();
    mTransmitting.DisconnectIfConnected();
    mLocalSsrcs.DisconnectIfConnected();
    mLocalRtxSsrcs.DisconnectIfConnected();
    mLocalCname.DisconnectIfConnected();
    mLocalMid.DisconnectIfConnected();
    mRemoteSsrc.DisconnectIfConnected();
    mRemoteRtxSsrc.DisconnectIfConnected();
    mSyncGroup.DisconnectIfConnected();
    mLocalRecvRtpExtensions.DisconnectIfConnected();
    mLocalSendRtpExtensions.DisconnectIfConnected();
    mVideoSendCodec.DisconnectIfConnected();
    mVideoSendRtpRtcpConfig.DisconnectIfConnected();
    mVideoRecvCodecs.DisconnectIfConnected();
    mVideoRecvRtpRtcpConfig.DisconnectIfConnected();
    mVideoCodecMode.DisconnectIfConnected();
    mWatchManager.Shutdown();
    mConduit->Shutdown();
  }

 private:
  void ReceivingChanged() {
    if (mReceiving) {
      mConduit->StartReceiving();
    } else {
      mConduit->StopReceiving();
    }
  }
  void TransmittingChanged() {
    if (mTransmitting) {
      mConduit->StartTransmitting();
    } else {
      mConduit->StopTransmitting();
    }
  }
  void LocalSsrcsChanged() {
    mConduit->SetLocalSSRCs(mLocalSsrcs, mLocalRtxSsrcs);
  }
  void LocalCnameChanged() {
    mConduit->SetLocalCNAME(mLocalCname.Ref().c_str());
  }
  void LocalMidChanged() { mConduit->SetLocalMID(mLocalMid); }
  void RemoteSsrcChanged() {
    mConduit->SetRemoteSSRC(mRemoteSsrc, mRemoteRtxSsrc);
  }
  void SyncGroupChanged() { mConduit->SetSyncGroup(mSyncGroup); }
  void LocalRecvRtpExtensionsChanged() {
    mConduit->SetLocalRTPExtensions(MediaSessionConduitLocalDirection::kRecv,
                                    mLocalRecvRtpExtensions);
  }
  void LocalSendRtpExtensionsChanged() {
    mConduit->SetLocalRTPExtensions(MediaSessionConduitLocalDirection::kSend,
                                    mLocalSendRtpExtensions);
  }
  void VideoSendCodecChanged() {
    MOZ_ALWAYS_TRUE(
        kMediaConduitNoError ==
        mConduit->ConfigureSendMediaCodec(*mVideoSendCodec.Ref(),
                                          *mVideoSendRtpRtcpConfig.Ref()));
  }
  void VideoRecvCodecsChanged() {
    MOZ_ALWAYS_TRUE(kMediaConduitNoError ==
                    mConduit->ConfigureRecvMediaCodecs(
                        mVideoRecvCodecs, *mVideoRecvRtpRtcpConfig.Ref()));
  }
  void VideoCodecModeChanged() {
    MOZ_ALWAYS_TRUE(kMediaConduitNoError ==
                    mConduit->ConfigureCodecMode(mVideoCodecMode));
  }

  const RefPtr<VideoSessionConduit> mConduit;
  const RefPtr<AbstractThread> mCallThread;
  WatchManager<VideoConduitController> mWatchManager;
  Mirror<bool> mReceiving;
  Mirror<bool> mTransmitting;
  Mirror<Ssrcs> mLocalSsrcs;
  Mirror<Ssrcs> mLocalRtxSsrcs;
  Mirror<std::string> mLocalCname;
  Mirror<std::string> mLocalMid;
  Mirror<Ssrc> mRemoteSsrc;
  Mirror<Ssrc> mRemoteRtxSsrc;
  Mirror<std::string> mSyncGroup;
  Mirror<RtpExtList> mLocalRecvRtpExtensions;
  Mirror<RtpExtList> mLocalSendRtpExtensions;
  Mirror<Maybe<VideoCodecConfig>> mVideoSendCodec;
  Mirror<Maybe<RtpRtcpConfig>> mVideoSendRtpRtcpConfig;
  Mirror<std::vector<VideoCodecConfig>> mVideoRecvCodecs;
  Mirror<Maybe<RtpRtcpConfig>> mVideoRecvRtpRtcpConfig;
  Mirror<webrtc::VideoCodecMode> mVideoCodecMode;
};
#undef INIT_MIRROR

}  // namespace mozilla

#endif
