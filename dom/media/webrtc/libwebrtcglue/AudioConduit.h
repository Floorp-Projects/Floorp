/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AUDIO_SESSION_H_
#define AUDIO_SESSION_H_

#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/TimeStamp.h"
#include "nsTArray.h"

#include "MediaConduitInterface.h"
#include "common/MediaEngineWrapper.h"
#include "RtpPacketQueue.h"

// Audio Engine Includes
#include "modules/audio_device/include/fake_audio_device.h"

/** This file hosts several structures identifying different aspects
 * of a RTP Session.
 */
namespace mozilla {
// Helper function

DOMHighResTimeStamp NTPtoDOMHighResTimeStamp(uint32_t ntpHigh, uint32_t ntpLow);

/**
 * Concrete class for Audio session. Hooks up
 *  - media-source and target to external transport
 */
class WebrtcAudioConduit : public AudioSessionConduit,
                           public webrtc::Transport {
 public:
  /**
   * APIs used by the registered external transport to this Conduit to
   * feed in received RTP Frames to the VoiceEngine for decoding
   */
  MediaConduitErrorCode ReceivedRTPPacket(const void* data, int len,
                                          webrtc::RTPHeader& header) override;

  /**
   * APIs used by the registered external transport to this Conduit to
   * feed in received RTCP Frames to the VoiceEngine for decoding
   */
  MediaConduitErrorCode ReceivedRTCPPacket(const void* data, int len) override;
  Maybe<DOMHighResTimeStamp> LastRtcpReceived() const override;
  DOMHighResTimeStamp GetNow() const override { return mCall->GetNow(); }

  MediaConduitErrorCode StopTransmitting() override;
  MediaConduitErrorCode StartTransmitting() override;
  MediaConduitErrorCode StopReceiving() override;
  MediaConduitErrorCode StartReceiving() override;

  MediaConduitErrorCode StopTransmittingLocked();
  MediaConduitErrorCode StartTransmittingLocked();
  MediaConduitErrorCode StopReceivingLocked();
  MediaConduitErrorCode StartReceivingLocked();

  /**
   * Function to configure send codec for the audio session
   * @param sendSessionConfig: CodecConfiguration
   * @result: On Success, the audio engine is configured with passed in codec
   * for send On failure, audio engine transmit functionality is disabled. NOTE:
   * This API can be invoked multiple time. Invoking this API may involve
   * restarting transmission sub-system on the engine.
   */
  MediaConduitErrorCode ConfigureSendMediaCodec(
      const AudioCodecConfig* codecConfig) override;
  /**
   * Function to configure list of receive codecs for the audio session
   * @param sendSessionConfig: CodecConfiguration
   * @result: On Success, the audio engine is configured with passed in codec
   * for send Also the playout is enabled. On failure, audio engine transmit
   * functionality is disabled. NOTE: This API can be invoked multiple time.
   * Invoking this API may involve restarting transmission sub-system on the
   * engine.
   */
  MediaConduitErrorCode ConfigureRecvMediaCodecs(
      const std::vector<UniquePtr<AudioCodecConfig>>& codecConfigList) override;

  MediaConduitErrorCode SetLocalRTPExtensions(
      MediaSessionConduitLocalDirection aDirection,
      const RtpExtList& extensions) override;

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
   * AudioConduit registers itself as ExternalTransport to the VoiceEngine
   */
  bool SendRtp(const uint8_t* data, size_t len,
               const webrtc::PacketOptions& options) override;

  /**
   * Webrtc transport implementation to send and receive RTCP packet.
   * AudioConduit registers itself as ExternalTransport to the VoiceEngine
   */
  bool SendRtcp(const uint8_t* data, size_t len) override;

  uint64_t CodecPluginID() override { return 0; }
  void SetPCHandle(const std::string& aPCHandle) override {}
  MediaConduitErrorCode DeliverPacket(const void* data, int len) override;

  void DeleteStreams() override {}

  WebrtcAudioConduit(RefPtr<WebRtcCallWrapper> aCall,
                     nsCOMPtr<nsISerialEventTarget> aStsThread)
      : mTransportMonitor("WebrtcAudioConduit"),
        mTransmitterTransport(nullptr),
        mReceiverTransport(nullptr),
        mCall(aCall),
        mRecvStreamConfig(),
        mRecvStream(nullptr),
        mSendStreamConfig(
            this)  // 'this' is stored but not  dereferenced in the constructor.
        ,
        mSendStream(nullptr),
        mRecvSSRC(0),
        mEngineTransmitting(false),
        mEngineReceiving(false),
        mDtmfEnabled(false),
        mMutex("WebrtcAudioConduit::mMutex"),
        mStsThread(aStsThread) {}

  virtual ~WebrtcAudioConduit();

  /* Set Local SSRC list.
   * Note: Until the refactor of the VoE into the call API is complete
   *   this list should contain only a single ssrc.
   */
  bool SetLocalSSRCs(const std::vector<uint32_t>& aSSRCs,
                     const std::vector<uint32_t>& aRtxSSRCs) override;
  std::vector<uint32_t> GetLocalSSRCs() override;
  bool SetRemoteSSRC(uint32_t ssrc, uint32_t rtxSsrc) override;
  bool UnsetRemoteSSRC(uint32_t ssrc) override { return true; }
  bool GetRemoteSSRC(uint32_t* ssrc) override;
  bool SetLocalCNAME(const char* cname) override;
  bool SetLocalMID(const std::string& mid) override;

  void SetSyncGroup(const std::string& group) override;

  Maybe<webrtc::AudioReceiveStream::Stats> GetReceiverStats() const override;
  Maybe<webrtc::AudioSendStream::Stats> GetSenderStats() const override;
  webrtc::Call::Stats GetCallStats() const override;

  bool SetDtmfPayloadType(unsigned char type, int freq) override;

  bool InsertDTMFTone(int channel, int eventCode, bool outOfBand, int lengthMs,
                      int attenuationDb) override;

  void GetRtpSources(nsTArray<dom::RTCRtpSourceEntry>& outSources) override;
  void SetRtcpEventObserver(mozilla::RtcpEventObserver* observer) override;

  // test-only: inserts fake CSRCs and audio level data
  void InsertAudioLevelForContributingSource(const uint32_t aCsrcSource,
                                             const int64_t aTimestamp,
                                             const uint32_t aRtpTimestamp,
                                             const bool aHasAudioLevel,
                                             const uint8_t aAudioLevel);

  bool IsSamplingFreqSupported(int freq) const override;

 private:
  WebrtcAudioConduit(const WebrtcAudioConduit& other) = delete;
  void operator=(const WebrtcAudioConduit& other) = delete;

  // Function to convert between WebRTC and Conduit codec structures
  bool CodecConfigToWebRTCCodec(const AudioCodecConfig* codecInfo,
                                webrtc::AudioSendStream::Config& config);

  // Generate block size in sample lenght for a given sampling frequency
  unsigned int GetNum10msSamplesForFrequency(int samplingFreqHz) const;

  // Checks the codec to be applied
  MediaConduitErrorCode ValidateCodecConfig(const AudioCodecConfig* codecInfo,
                                            bool send);

  MediaConduitErrorCode CreateSendStream();
  void DeleteSendStream();
  MediaConduitErrorCode CreateRecvStream();
  void DeleteRecvStream();

  bool RecreateSendStreamIfExists();
  bool RecreateRecvStreamIfExists();

  mozilla::ReentrantMonitor mTransportMonitor;

  // Accessed on any thread under mTransportMonitor.
  RefPtr<TransportInterface> mTransmitterTransport;

  // Accessed on any thread under mTransportMonitor.
  RefPtr<TransportInterface> mReceiverTransport;

  // Const so can be accessed on any thread. Most methods are called on
  // main thread.
  const RefPtr<WebRtcCallWrapper> mCall;

  // Accessed only on main thread.
  webrtc::AudioReceiveStream::Config mRecvStreamConfig;

  // Written only on main thread. Guarded by mMutex, except for reads on main.
  webrtc::AudioReceiveStream* mRecvStream;

  // Accessed only on main thread.
  webrtc::AudioSendStream::Config mSendStreamConfig;

  // Written only on main thread. Guarded by mMutex, except for reads on main.
  webrtc::AudioSendStream* mSendStream;

  // accessed on creation, and when receiving packets
  Atomic<uint32_t> mRecvSSRC;  // this can change during a stream!

  // Accessed only on mStsThread.
  RtpPacketQueue mRtpPacketQueue;

  // engine states of our interets
  mozilla::Atomic<bool>
      mEngineTransmitting;  // If true => VoiceEngine Send-subsystem is up
  mozilla::Atomic<bool>
      mEngineReceiving;  // If true => VoiceEngine Receive-subsystem is up
                         // and playout is enabled

  // Accessed only on main thread.
  bool mDtmfEnabled;
  int mDtmfPayloadType = -1;
  int mDtmfPayloadFrequency = -1;

  Mutex mMutex;

  // Socket transport service thread. Any thread.
  const nsCOMPtr<nsISerialEventTarget> mStsThread;

  // Accessed from mStsThread. Last successfully polled RTT
  Maybe<DOMHighResTimeStamp> mRttSec;

  // Accessed only on mStsThread
  Maybe<DOMHighResTimeStamp> mLastRtcpReceived;

  // Accessed only on main thread.
  mozilla::RtcpEventObserver* mRtcpEventObserver = nullptr;
};

}  // namespace mozilla

#endif
