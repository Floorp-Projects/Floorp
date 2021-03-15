/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VIDEO_SESSION_H_
#define VIDEO_SESSION_H_

#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/UniquePtr.h"
#include "nsITimer.h"

#include "MediaConduitInterface.h"
#include "common/MediaEngineWrapper.h"
#include "RunningStat.h"
#include "RtpPacketQueue.h"
#include "transport/runnable_utils.h"

// conflicts with #include of scoped_ptr.h
#undef FF
// Video Engine Includes
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/sdp_video_format.h"
#include "call/call.h"
#include "common_video/include/i420_buffer_pool.h"
#include "media/base/video_adapter.h"
#include "media/base/video_broadcaster.h"
#include <functional>
#include <memory>
/** This file hosts several structures identifying different aspects
 * of a RTP Session.
 */

namespace mozilla {

// Convert (SI) kilobits/sec to (SI) bits/sec
#define KBPS(kbps) kbps * 1000

const int kViEMinCodecBitrate_bps = KBPS(30);
const unsigned int kVideoMtu = 1200;
const int kQpMax = 56;

template <typename T>
T MinIgnoreZero(const T& a, const T& b) {
  return std::min(a ? a : b, b ? b : a);
}

class VideoStreamFactory;
class WebrtcAudioConduit;

// Interface of external video encoder for WebRTC.
class WebrtcVideoEncoder : public VideoEncoder, public webrtc::VideoEncoder {};

// Interface of external video decoder for WebRTC.
class WebrtcVideoDecoder : public VideoDecoder, public webrtc::VideoDecoder {};

/**
 * Concrete class for Video session. Hooks up
 *  - media-source and target to external transport
 */
class WebrtcVideoConduit
    : public VideoSessionConduit,
      public webrtc::Transport,
      public webrtc::RtcpEventObserver,
      public rtc::VideoSinkInterface<webrtc::VideoFrame>,
      public rtc::VideoSourceInterface<webrtc::VideoFrame> {
 public:
  // Returns true when both encoder and decoder are HW accelerated.
  static bool HasH264Hardware();

  MediaConduitErrorCode SetLocalRTPExtensions(
      MediaSessionConduitLocalDirection aDirection,
      const RtpExtList& aExtensions) override;

  /**
   * Function to attach Renderer end-point for the Media-Video conduit.
   * @param aRenderer : Reference to the concrete mozilla Video renderer
   * implementation Note: Multiple invocations of this API shall remove an
   * existing renderer and attaches the new to the Conduit.
   */
  MediaConduitErrorCode AttachRenderer(
      RefPtr<mozilla::VideoRenderer> aVideoRenderer) override;
  void DetachRenderer() override;

  /**
   * APIs used by the registered external transport to this Conduit to
   * feed in received RTP Frames to the VideoEngine for decoding
   */
  void ReceivedRTPPacket(const uint8_t* data, int len,
                         webrtc::RTPHeader& header) override;

  /**
   * APIs used by the registered external transport to this Conduit to
   * feed in received RTCP Frames to the VideoEngine for decoding
   */
  void ReceivedRTCPPacket(const uint8_t* data, int len) override;
  Maybe<DOMHighResTimeStamp> LastRtcpReceived() const override;
  DOMHighResTimeStamp GetNow() const override { return mCall->GetNow(); }

  MediaConduitErrorCode StopTransmitting() override;
  MediaConduitErrorCode StartTransmitting() override;
  MediaConduitErrorCode StopReceiving() override;
  MediaConduitErrorCode StartReceiving() override;

  void OnFrameDelivered() override;

  MediaConduitErrorCode StopTransmittingLocked();
  MediaConduitErrorCode StartTransmittingLocked();
  MediaConduitErrorCode StopReceivingLocked();
  MediaConduitErrorCode StartReceivingLocked();

  /**
   * Function to configure sending codec mode for different content
   */
  MediaConduitErrorCode ConfigureCodecMode(webrtc::VideoCodecMode) override;

  /**
   * Function to configure send codec for the video session
   * @param sendSessionConfig: CodecConfiguration
   * @result: On Success, the video engine is configured with passed in codec
   *          for send
   *          On failure, video engine transmit functionality is disabled.
   * NOTE: This API can be invoked multiple time. Invoking this API may involve
   * restarting transmission sub-system on the engine.
   */
  MediaConduitErrorCode ConfigureSendMediaCodec(
      const VideoCodecConfig* codecInfo,
      const RtpRtcpConfig& aRtpRtcpConfig) override;

  /**
   * Function to configure list of receive codecs for the video session
   * @param sendSessionConfig: CodecConfiguration
   * @result: On Success, the video engine is configured with passed in codec
   *          for send
   *          Also the playout is enabled.
   *          On failure, video engine transmit functionality is disabled.
   * NOTE: This API can be invoked multiple time. Invoking this API may involve
   * restarting transmission sub-system on the engine.
   */
  MediaConduitErrorCode ConfigureRecvMediaCodecs(
      const std::vector<UniquePtr<VideoCodecConfig>>& codecConfigList,
      const RtpRtcpConfig& aRtpRtcpConfig) override;

  /**
   * Register Transport for this Conduit. RTP and RTCP frames from the
   * VideoEngine shall be passed to the registered transport for transporting
   * externally.
   */
  MediaConduitErrorCode SetTransmitterTransport(
      RefPtr<TransportInterface> aTransport) override;

  MediaConduitErrorCode SetReceiverTransport(
      RefPtr<TransportInterface> aTransport) override;

  /**
   * Function to select and change the encoding resolution based on incoming
   * frame size and current available bandwidth.
   * @param width, height: dimensions of the frame
   * @param frame: optional frame to submit for encoding after reconfig
   */
  void SelectSendResolution(unsigned short width, unsigned short height);

  /**
   * Function to deliver a capture video frame for encoding and transport.
   * If the frame's timestamp is 0, it will be automatically generated.
   *
   * NOTE: ConfigureSendMediaCodec() must be called before this function can
   *       be invoked. This ensures the inserted video-frames can be
   *       transmitted by the conduit.
   */
  MediaConduitErrorCode SendVideoFrame(
      const webrtc::VideoFrame& frame) override;

  /**
   * webrtc::Transport method implementation
   * ---------------------------------------
   * Webrtc transport implementation to send and receive RTP packet.
   * VideoConduit registers itself as ExternalTransport to the VideoStream
   */
  bool SendRtp(const uint8_t* packet, size_t length,
               const webrtc::PacketOptions& options) override;

  /**
   * webrtc::Transport method implementation
   * ---------------------------------------
   * Webrtc transport implementation to send and receive RTCP packet.
   * VideoConduit registers itself as ExternalTransport to the VideoEngine
   */
  bool SendRtcp(const uint8_t* packet, size_t length) override;

  /*
   * webrtc:VideoSinkInterface implementation
   * -------------------------------
   */
  void OnFrame(const webrtc::VideoFrame& frame) override;

  /*
   * webrtc:VideoSourceInterface implementation
   * -------------------------------
   */
  void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;

  bool HasCodecPluginID(uint64_t aPluginID) override;

  void DeleteStreams() override;

  bool Denoising() const { return mDenoising; }

  uint8_t SpatialLayers() const { return mSpatialLayers; }

  uint8_t TemporalLayers() const { return mTemporalLayers; }

  webrtc::VideoCodecMode CodecMode() const {
    MOZ_ASSERT(NS_IsMainThread());
    return mCodecMode;
  }

  WebrtcVideoConduit(RefPtr<WebRtcCallWrapper> aCall,
                     nsCOMPtr<nsISerialEventTarget> aStsThread,
                     std::string aPCHandle);
  virtual ~WebrtcVideoConduit();

  MediaConduitErrorCode InitMain();
  virtual MediaConduitErrorCode Init();

  std::vector<uint32_t> GetLocalSSRCs() override;
  bool SetLocalSSRCs(const std::vector<uint32_t>& ssrcs,
                     const std::vector<uint32_t>& rtxSsrcs) override;
  // Can be called from any thread
  bool GetRemoteSSRC(uint32_t* ssrc) override;
  bool SetRemoteSSRC(uint32_t ssrc, uint32_t rtxSsrc) override;
  bool UnsetRemoteSSRC(uint32_t ssrc) override;
  bool SetLocalCNAME(const char* cname) override;
  bool SetLocalMID(const std::string& mid) override;

  void SetSyncGroup(const std::string& group) override;

  bool SetRemoteSSRCLocked(uint32_t ssrc, uint32_t rtxSsrc);

  Maybe<webrtc::VideoReceiveStream::Stats> GetReceiverStats() const override;
  Maybe<webrtc::VideoSendStream::Stats> GetSenderStats() const override;
  webrtc::Call::Stats GetCallStats() const override;

  bool AddFrameHistory(dom::Sequence<dom::RTCVideoFrameHistoryInternal>*
                           outHistories) const override;

  uint64_t MozVideoLatencyAvg();

  void DisableSsrcChanges() override {
    ASSERT_ON_THREAD(mStsThread);
    mAllowSsrcChange = false;
  }

  void CollectTelemetryData() override;

  void OnRtcpBye() override;

  void OnRtcpTimeout() override;

  void SetRtcpEventObserver(mozilla::RtcpEventObserver* observer) override;

 private:
  // Don't allow copying/assigning.
  WebrtcVideoConduit(const WebrtcVideoConduit&) = delete;
  void operator=(const WebrtcVideoConduit&) = delete;

  // Utility function to dump recv codec database
  void DumpCodecDB() const;

  // Video Latency Test averaging filter
  void VideoLatencyUpdate(uint64_t new_sample);

  MediaConduitErrorCode CreateSendStream();
  void DeleteSendStream();
  MediaConduitErrorCode CreateRecvStream();
  void DeleteRecvStream();

  void DeliverPacket(rtc::CopyOnWriteBuffer packet, PacketType type) override;

  bool RequiresNewSendStream(const VideoCodecConfig& newConfig) const;

  mutable mozilla::ReentrantMonitor mTransportMonitor;

  // Accessed on any thread under mTransportMonitor.
  RefPtr<TransportInterface> mTransmitterTransport;

  // Accessed on any thread under mTransportMonitor.
  RefPtr<TransportInterface> mReceiverTransport;

  // Accessed on any thread under mTransportMonitor.
  RefPtr<mozilla::VideoRenderer> mRenderer;

  // Accessed on any thread under mTransportMonitor.
  unsigned short mReceivingWidth = 0;

  // Accessed on any thread under mTransportMonitor.
  unsigned short mReceivingHeight = 0;

  // Socket transport service thread that runs stats queries against us. Any
  // thread.
  const nsCOMPtr<nsISerialEventTarget> mStsThread;

  Mutex mMutex;

  // Decoder factory used by mRecvStream when it needs new decoders. This is
  // not shared broader like some state in the WebrtcCallWrapper because it
  // handles CodecPluginID plumbing tied to this VideoConduit.
  const UniquePtr<WebrtcVideoDecoderFactory> mDecoderFactory;

  // Encoder factory used by mSendStream when it needs new encoders. This is
  // not shared broader like some state in the WebrtcCallWrapper because it
  // handles CodecPluginID plumbing tied to this VideoConduit.
  const UniquePtr<WebrtcVideoEncoderFactory> mEncoderFactory;

  // Adapter handling resolution constraints from signaling and sinks.
  // Written only on main thread. Guarded by mMutex, except for reads on main.
  UniquePtr<cricket::VideoAdapter> mVideoAdapter;

  // Our own record of the sinks added to mVideoBroadcaster so we can support
  // dispatching updates to sinks from off-main-thread. Main thread only.
  AutoTArray<rtc::VideoSinkInterface<webrtc::VideoFrame>*, 1> mRegisteredSinks;

  // Broadcaster that distributes our frames to all registered sinks.
  // Threadsafe.
  rtc::VideoBroadcaster mVideoBroadcaster;

  // When true the send resolution needs to be updated next time we process a
  // video frame. Set on various threads.
  Atomic<bool> mUpdateSendResolution{false};

  // Buffer pool used for scaling frames.
  // Accessed on the frame-feeding thread only.
  webrtc::I420BufferPool mBufferPool;

  // Engine state we are concerned with. Written on main thread and read
  // anywhere.
  mozilla::Atomic<bool>
      mEngineTransmitting;  // If true ==> Transmit Subsystem is up and running
  mozilla::Atomic<bool>
      mEngineReceiving;  // if true ==> Receive Subsystem up and running

  // Local database of currently applied receive codecs. Main thread only.
  nsTArray<UniquePtr<VideoCodecConfig>> mRecvCodecList;

  // Written only on main thread. Guarded by mMutex, except for reads on main.
  UniquePtr<VideoCodecConfig> mCurSendCodecConfig;

  // Bookkeeping of stats for telemetry. Main thread only.
  RunningStat mSendFramerate;
  RunningStat mSendBitrate;
  RunningStat mRecvFramerate;
  RunningStat mRecvBitrate;

  // Must call webrtc::Call::DestroyVideoReceive/SendStream to delete this.
  // Written only on main thread. Guarded by mMutex, except for reads on main.
  webrtc::VideoReceiveStream* mRecvStream = nullptr;

  // Must call webrtc::Call::DestroyVideoReceive/SendStream to delete this.
  // Written only on main thread. Guarded by mMutex, except for reads on main.
  webrtc::VideoSendStream* mSendStream = nullptr;

  // Written on the frame feeding thread.
  // Guarded by mMutex, except for reads on the frame feeding thread.
  unsigned short mLastWidth = 0;

  // Written on the frame feeding thread.
  // Guarded by mMutex, except for reads on the frame feeding thread.
  unsigned short mLastHeight = 0;

  // Accessed under mMutex.
  unsigned int mSendingFramerate;

  // Written on main thread at creation,
  // then written or read on any thread under mTransportMonitor.
  bool mVideoLatencyTestEnable = false;

  // Accessed from any thread under mTransportMonitor.
  uint64_t mVideoLatencyAvg = 0;

  // All in bps.
  // All written on main thread and guarded by mMutex, except for reads on main.
  int mMinBitrate = 0;
  int mStartBitrate = 0;
  int mPrefMaxBitrate = 0;
  int mNegotiatedMaxBitrate = 0;
  int mMinBitrateEstimate = 0;

  // Set to true to force denoising on.
  // Written at creation, then read anywhere.
  bool mDenoising = false;

  // Set to true to ignore sink wants (scaling due to bwe and cpu usage).
  // Written at creation, then read anywhere.
  bool mLockScaling = false;

  // Written at creation, then read anywhere.
  uint8_t mSpatialLayers = 1;

  // Written at creation, then read anywhere.
  uint8_t mTemporalLayers = 1;

  static const unsigned int sAlphaNum = 7;
  static const unsigned int sAlphaDen = 8;
  static const unsigned int sRoundingPadding = 1024;

  // Main thread only.
  RefPtr<WebrtcAudioConduit> mSyncedTo;

  // Main thread only.
  webrtc::VideoCodecMode mActiveCodecMode;
  webrtc::VideoCodecMode mCodecMode;

  // WEBRTC.ORG Call API
  // Const so can be accessed on any thread. Most methods are called on
  // main thread, though Receiver() is called on STS. This seems fine.
  const RefPtr<WebRtcCallWrapper> mCall;

  // Written only on main thread. Guarded by mMutex, except for reads on main.
  webrtc::VideoSendStream::Config mSendStreamConfig;

  // Main thread only.
  webrtc::VideoEncoderConfig mEncoderConfig;

  // Written only on main thread. Guarded by mMutex, except for reads on main.
  // Calls can happen on any thread.
  RefPtr<rtc::RefCountedObject<VideoStreamFactory>> mVideoStreamFactory;

  // Main thread only.
  webrtc::VideoReceiveStream::Config mRecvStreamConfig;

  // Are SSRC changes without signaling allowed or not.
  // Accessed only on mStsThread.
  bool mAllowSsrcChange = true;

  // Accessed only on mStsThread.
  bool mWaitingForInitialSsrc = true;

  // Accessed during configuration/signaling (main),
  // and when receiving packets (sts).
  Atomic<uint32_t> mRecvSSRC;  // this can change during a stream!
  // Accessed from both the STS and main thread for a variety of things
  // Set when receiving packets
  Atomic<uint32_t> mRemoteSSRC;  // this can change during a stream!

  // Accessed only on mStsThread.
  RtpPacketQueue mRtpPacketQueue;

  // Main thread only
  nsTArray<uint64_t> mSendCodecPluginIDs;
  // Main thread only
  nsTArray<uint64_t> mRecvCodecPluginIDs;

  MediaEventListener mSendPluginCreated;
  MediaEventListener mSendPluginReleased;
  MediaEventListener mRecvPluginCreated;
  MediaEventListener mRecvPluginReleased;

  // Accessed only on mStsThread
  Maybe<DOMHighResTimeStamp> mLastRtcpReceived;

  // Tracking the attributes of received frames over time
  // Protected by mTransportMonitor
  dom::RTCVideoFrameHistoryInternal mReceivedFrameHistory;

  // Accessed only on main thread.
  mozilla::RtcpEventObserver* mRtcpEventObserver = nullptr;
};
}  // namespace mozilla

#endif
