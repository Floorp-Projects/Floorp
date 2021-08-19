/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AUDIO_SESSION_H_
#define AUDIO_SESSION_H_

#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/StateMirroring.h"
#include "mozilla/TimeStamp.h"

#include "MediaConduitInterface.h"
#include "common/MediaEngineWrapper.h"
#include "RtpPacketQueue.h"

/**
 * This file hosts several structures identifying different aspects of a RTP
 * Session.
 */
namespace mozilla {

struct DtmfEvent;

/**
 * Concrete class for Audio session. Hooks up
 *  - media-source and target to external transport
 */
class WebrtcAudioConduit : public AudioSessionConduit,
                           public webrtc::RtcpEventObserver,
                           public webrtc::Transport {
 public:
  /**
   * APIs used by the registered external transport to this Conduit to
   * feed in received RTP Frames to the Receiver for decoding.
   */
  void ReceivedRTPPacket(const uint8_t* data, int len,
                         webrtc::RTPHeader& header) override;

  void OnRtcpBye() override;
  void OnRtcpTimeout() override;

  /**
   * APIs used by the registered external transport to this Conduit to
   * feed in received RTCP Frames to the Receiver for decoding.
   */
  void ReceivedRTCPPacket(const uint8_t* data, int len) override;
  Maybe<DOMHighResTimeStamp> LastRtcpReceived() const override;
  DOMHighResTimeStamp GetNow() const override;

  MediaConduitErrorCode StopTransmittingLocked();
  MediaConduitErrorCode StartTransmittingLocked();
  MediaConduitErrorCode StopReceivingLocked();
  MediaConduitErrorCode StartReceivingLocked();

  /**
   * Register External Transport to this Conduit. RTP and RTCP frames from the
   * VoiceEngine shall be passed to the registered transport for transporting
   * externally.
   */
  MediaConduitErrorCode SetTransmitterTransport(
      RefPtr<TransportInterface> aTransport) override;

  MediaConduitErrorCode SetReceiverTransport(
      RefPtr<TransportInterface> aTransport) override;

  /**
   * Function to deliver externally captured audio sample for encoding and
   * transport
   * @param frame [in]: AudioFrame in upstream's format for forwarding to the
   *                    send stream. Ownership is passed along.
   * NOTE: ConfigureSendMediaCodec() SHOULD be called before this function can
   * be invoked. This ensures the inserted audio-samples can be transmitted by
   * the conduit.
   */
  MediaConduitErrorCode SendAudioFrame(
      std::unique_ptr<webrtc::AudioFrame> frame) override;

  /**
   * Function to grab a decoded audio-sample from the media engine for
   * rendering / playout of length 10 milliseconds.
   *
   * @param samplingFreqHz [in]: Frequency of the sampling for playback in
   *                             Hertz (16000, 32000,..)
   * @param frame [in/out]: Pointer to an AudioFrame to which audio data will be
   *                        copied
   * NOTE: This function should be invoked every 10 milliseconds for the best
   *       performance
   * NOTE: ConfigureRecvMediaCodec() SHOULD be called before this function can
   *       be invoked
   * This ensures the decoded samples are ready for reading and playout is
   * enabled.
   */
  MediaConduitErrorCode GetAudioFrame(int32_t samplingFreqHz,
                                      webrtc::AudioFrame* frame) override;

  /**
   * Webrtc transport implementation to send and receive RTP packet.
   */
  bool SendRtp(const uint8_t* data, size_t len,
               const webrtc::PacketOptions& options) override;

  /**
   * Webrtc transport implementation to send and receive RTCP packet.
   */
  bool SendRtcp(const uint8_t* data, size_t len) override;

  bool HasCodecPluginID(uint64_t aPluginID) const override { return false; }

  void DeliverPacket(rtc::CopyOnWriteBuffer packet, PacketType type) override;

  void Shutdown() override;

  WebrtcAudioConduit(RefPtr<WebrtcCallWrapper> aCall,
                     nsCOMPtr<nsISerialEventTarget> aStsThread);

  virtual ~WebrtcAudioConduit();

  // Call thread.
  void InitControl(AudioConduitControlInterface* aControl) override;

  // Handle a DTMF event from mControl.mOnDtmfEventListener.
  void OnDtmfEvent(const DtmfEvent& aEvent);

  // Called when a parameter in mControl has changed. Call thread.
  void OnControlConfigChange();

  std::vector<uint32_t> GetLocalSSRCs() const override;

 private:
  /**
   * Override the remote ssrc configured on mRecvStreamConfig.
   *
   * Recreates and restarts the recv stream if needed. The overriden value is
   * overwritten the next time the mControl.mRemoteSsrc mirror changes value.
   *
   * Call thread only.
   */
  bool OverrideRemoteSSRC(uint32_t ssrc);

 public:
  void UnsetRemoteSSRC(uint32_t ssrc) override {}
  bool GetRemoteSSRC(uint32_t* ssrc) const override;

  Maybe<webrtc::AudioReceiveStream::Stats> GetReceiverStats() const override;
  Maybe<webrtc::AudioSendStream::Stats> GetSenderStats() const override;
  webrtc::Call::Stats GetCallStats() const override;

  bool IsSamplingFreqSupported(int freq) const override;

  MediaEventSource<void>& RtcpByeEvent() override { return mRtcpByeEvent; }
  MediaEventSource<void>& RtcpTimeoutEvent() override {
    return mRtcpTimeoutEvent;
  }

 private:
  WebrtcAudioConduit(const WebrtcAudioConduit& other) = delete;
  void operator=(const WebrtcAudioConduit& other) = delete;

  // Function to convert between WebRTC and Conduit codec structures
  bool CodecConfigToWebRTCCodec(const AudioCodecConfig& codecInfo,
                                webrtc::AudioSendStream::Config& config);

  // Generate block size in sample length for a given sampling frequency
  unsigned int GetNum10msSamplesForFrequency(int samplingFreqHz) const;

  // Checks the codec to be applied
  static MediaConduitErrorCode ValidateCodecConfig(
      const AudioCodecConfig& codecInfo, bool send);
  /**
   * Of all extensions in aExtensions, returns a list of supported extensions.
   */
  static RtpExtList FilterExtensions(
      MediaSessionConduitLocalDirection aDirection,
      const RtpExtList& aExtensions);
  static webrtc::SdpAudioFormat CodecConfigToLibwebrtcFormat(
      const AudioCodecConfig& aConfig);

  MediaConduitErrorCode CreateSendStream();
  void DeleteSendStream();
  MediaConduitErrorCode CreateRecvStream();
  void DeleteRecvStream();

  mozilla::ReentrantMonitor mTransportMonitor;

  // Accessed on any thread under mTransportMonitor.
  RefPtr<TransportInterface> mTransmitterTransport;

  // Accessed on any thread under mTransportMonitor.
  RefPtr<TransportInterface> mReceiverTransport;

  // Const so can be accessed on any thread. Most methods are called on the Call
  // thread.
  const RefPtr<WebrtcCallWrapper> mCall;

  // Accessed only on the Call thread.
  webrtc::AudioReceiveStream::Config mRecvStreamConfig;

  // Written only on the Call thread. Guarded by mMutex, except for reads on the
  // Call thread.
  webrtc::AudioReceiveStream* mRecvStream;

  // Accessed only on the Call thread.
  webrtc::AudioSendStream::Config mSendStreamConfig;

  // Written only on the Call thread. Guarded by mMutex, except for reads on the
  // Call thread.
  webrtc::AudioSendStream* mSendStream;

  // accessed on creation, and when receiving packets
  Atomic<uint32_t> mRecvSSRC;  // this can change during a stream!

  // Accessed only on mStsThread.
  RtpPacketQueue mRtpPacketQueue;

  // If true => mSendStream started and not stopped
  // Written only on the Call thread. Guarded by mMutex, except for reads on the
  // Call thread.
  bool mSendStreamRunning;
  // If true => mRecvStream started and not stopped
  // Written only on the Call thread. Guarded by mMutex, except for reads on the
  // Call thread.
  bool mRecvStreamRunning;

  // Accessed only on the Call thread.
  bool mDtmfEnabled;

  mutable Mutex mMutex;

  // Call worker thread. All access to mCall->Call() happens here.
  const RefPtr<AbstractThread> mCallThread;

  // Socket transport service thread. Any thread.
  const nsCOMPtr<nsISerialEventTarget> mStsThread;

  struct Control {
    // Mirrors and events that map to AudioConduitControlInterface for control.
    // Call thread only.
    Mirror<bool> mReceiving;
    Mirror<bool> mTransmitting;
    Mirror<Ssrcs> mLocalSsrcs;
    Mirror<std::string> mLocalCname;
    Mirror<std::string> mLocalMid;
    Mirror<Ssrc> mRemoteSsrc;
    Mirror<std::string> mSyncGroup;
    Mirror<RtpExtList> mLocalRecvRtpExtensions;
    Mirror<RtpExtList> mLocalSendRtpExtensions;
    Mirror<Maybe<AudioCodecConfig>> mSendCodec;
    Mirror<std::vector<AudioCodecConfig>> mRecvCodecs;
    MediaEventListener mOnDtmfEventListener;

    // For caching mRemoteSsrc, since another caller may change the remote ssrc
    // in the stream config directly.
    Ssrc mConfiguredRemoteSsrc = 0;
    // For tracking changes to mSendCodec.
    Maybe<AudioCodecConfig> mConfiguredSendCodec;
    // For tracking changes to mRecvCodecs.
    std::vector<AudioCodecConfig> mConfiguredRecvCodecs;

    Control() = delete;
    explicit Control(const RefPtr<AbstractThread>& aCallThread);
  } mControl;

  // WatchManager allowing Mirrors to trigger functions that will update the
  // webrtc.org configuration.
  WatchManager<WebrtcAudioConduit> mWatchManager;

  // Accessed from mStsThread. Last successfully polled RTT
  Maybe<DOMHighResTimeStamp> mRttSec;

  // Accessed only on mStsThread
  Maybe<DOMHighResTimeStamp> mLastRtcpReceived;

  // Thread safe
  MediaEventProducer<void> mRtcpByeEvent;
  MediaEventProducer<void> mRtcpTimeoutEvent;
};

}  // namespace mozilla

#endif
