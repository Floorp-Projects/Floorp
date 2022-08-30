/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VIDEO_SESSION_H_
#define VIDEO_SESSION_H_

#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/StateMirroring.h"
#include "mozilla/UniquePtr.h"
#include "nsITimer.h"

#include "MediaConduitInterface.h"
#include "common/MediaEngineWrapper.h"
#include "RtpRtcpConfig.h"
#include "RunningStat.h"
#include "transport/runnable_utils.h"

// conflicts with #include of scoped_ptr.h
#undef FF
// Video Engine Includes
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/sdp_video_format.h"
#include "call/call_basic_stats.h"
#include "common_video/include/video_frame_buffer_pool.h"
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
      public webrtc::RtcpEventObserver,
      public rtc::VideoSinkInterface<webrtc::VideoFrame>,
      public rtc::VideoSourceInterface<webrtc::VideoFrame> {
 public:
  // Returns true when both encoder and decoder are HW accelerated.
  static bool HasH264Hardware();

  Maybe<int> ActiveSendPayloadType() const override;
  Maybe<int> ActiveRecvPayloadType() const override;

  /**
   * Function to attach Renderer end-point for the Media-Video conduit.
   * @param aRenderer : Reference to the concrete mozilla Video renderer
   * implementation Note: Multiple invocations of this API shall remove an
   * existing renderer and attaches the new to the Conduit.
   */
  MediaConduitErrorCode AttachRenderer(
      RefPtr<mozilla::VideoRenderer> aVideoRenderer) override;
  void DetachRenderer() override;

  Maybe<uint16_t> RtpSendBaseSeqFor(uint32_t aSsrc) const override;

  const dom::RTCStatsTimestampMaker& GetTimestampMaker() const override;

  void StopTransmitting();
  void StartTransmitting();
  void StopReceiving();
  void StartReceiving();

  /**
   * Function to select and change the encoding resolution based on incoming
   * frame size and current available bandwidth.
   * @param width, height: dimensions of the frame
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
  MediaConduitErrorCode SendVideoFrame(webrtc::VideoFrame aFrame) override;

  bool SendRtp(const uint8_t* aData, size_t aLength,
               const webrtc::PacketOptions& aOptions) override;
  bool SendSenderRtcp(const uint8_t* aData, size_t aLength) override;
  bool SendReceiverRtcp(const uint8_t* aData, size_t aLength) override;

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

  bool HasCodecPluginID(uint64_t aPluginID) const override;

  RefPtr<GenericPromise> Shutdown() override;

  bool Denoising() const { return mDenoising; }

  uint8_t SpatialLayers() const { return mSpatialLayers; }

  uint8_t TemporalLayers() const { return mTemporalLayers; }

  webrtc::VideoCodecMode CodecMode() const;

  WebrtcVideoConduit(RefPtr<WebrtcCallWrapper> aCall,
                     nsCOMPtr<nsISerialEventTarget> aStsThread,
                     Options aOptions, std::string aPCHandle);
  virtual ~WebrtcVideoConduit();

  // Call thread.
  void InitControl(VideoConduitControlInterface* aControl) override;

  // Called when a parameter in mControl has changed. Call thread.
  void OnControlConfigChange();

  // Necessary Init steps on main thread.
  MediaConduitErrorCode Init();

  Ssrcs GetLocalSSRCs() const override;
  Maybe<Ssrc> GetRemoteSSRC() const override;

  // Call thread.
  void UnsetRemoteSSRC(uint32_t aSsrc) override;

  static unsigned ToLibwebrtcMaxFramerate(const Maybe<double>& aMaxFramerate);

 private:
  void NotifyUnsetCurrentRemoteSSRC();
  void SetRemoteSSRCConfig(uint32_t aSsrc, uint32_t aRtxSsrc);
  void SetRemoteSSRCAndRestartAsNeeded(uint32_t aSsrc, uint32_t aRtxSsrc);

 public:
  // Creating a recv stream or a send stream requires a local ssrc to be
  // configured. This method will generate one if needed.
  void EnsureLocalSSRC();
  // Creating a recv stream requires a remote ssrc to be configured. This method
  // will generate one if needed.
  void EnsureRemoteSSRC();

  Maybe<webrtc::VideoReceiveStream::Stats> GetReceiverStats() const override;
  Maybe<webrtc::VideoSendStream::Stats> GetSenderStats() const override;
  Maybe<webrtc::CallBasicStats> GetCallStats() const override;

  bool AddFrameHistory(dom::Sequence<dom::RTCVideoFrameHistoryInternal>*
                           outHistories) const override;

  uint64_t MozVideoLatencyAvg();

  void DisableSsrcChanges() override {
    MOZ_ASSERT(mCallThread->IsOnCurrentThread());
    mAllowSsrcChange = false;
  }

  void CollectTelemetryData() override;

  void OnRtpReceived(MediaPacket&& aPacket, webrtc::RTPHeader&& aHeader);
  void OnRtcpReceived(MediaPacket&& aPacket);

  void OnRtcpBye() override;
  void OnRtcpTimeout() override;

  void SetTransportActive(bool aActive) override {
    mTransportActive = aActive;
    if (!aActive) {
      mReceiverRtpEventListener.DisconnectIfExists();
      mReceiverRtcpEventListener.DisconnectIfExists();
      mSenderRtcpEventListener.DisconnectIfExists();
    }
  }
  MediaEventSourceExc<MediaPacket>& SenderRtpSendEvent() override {
    return mSenderRtpSendEvent;
  }
  MediaEventSourceExc<MediaPacket>& SenderRtcpSendEvent() override {
    return mSenderRtcpSendEvent;
  }
  MediaEventSourceExc<MediaPacket>& ReceiverRtcpSendEvent() override {
    return mReceiverRtcpSendEvent;
  }
  void ConnectReceiverRtpEvent(
      MediaEventSourceExc<MediaPacket, webrtc::RTPHeader>& aEvent) override {
    // Hold a strong-ref to `this` for safety, since we'll be disconnecting
    // off-target.
    mReceiverRtpEventListener = aEvent.Connect(
        mCallThread, [this, self = RefPtr<WebrtcVideoConduit>(this)](
                         MediaPacket aPacket, webrtc::RTPHeader aHeader) {
          OnRtpReceived(std::move(aPacket), std::move(aHeader));
        });
  }
  void ConnectReceiverRtcpEvent(
      MediaEventSourceExc<MediaPacket>& aEvent) override {
    // Hold a strong-ref to `this` for safety, since we'll be disconnecting
    // off-target.
    mReceiverRtcpEventListener = aEvent.Connect(
        mCallThread,
        [this, self = RefPtr<WebrtcVideoConduit>(this)](MediaPacket aPacket) {
          OnRtcpReceived(std::move(aPacket));
        });
  }
  void ConnectSenderRtcpEvent(
      MediaEventSourceExc<MediaPacket>& aEvent) override {
    // Hold a strong-ref to `this` for safety, since we'll be disconnecting
    // off-target.
    mSenderRtcpEventListener = aEvent.Connect(
        mCallThread,
        [this, self = RefPtr<WebrtcVideoConduit>(this)](MediaPacket aPacket) {
          OnRtcpReceived(std::move(aPacket));
        });
  }

  std::vector<webrtc::RtpSource> GetUpstreamRtpSources() const override;

 private:
  // Don't allow copying/assigning.
  WebrtcVideoConduit(const WebrtcVideoConduit&) = delete;
  void operator=(const WebrtcVideoConduit&) = delete;

  // Utility function to dump recv codec database
  void DumpCodecDB() const;

  // Video Latency Test averaging filter
  void VideoLatencyUpdate(uint64_t aNewSample);

  void CreateSendStream();
  void DeleteSendStream();
  void CreateRecvStream();
  void DeleteRecvStream();

  void DeliverPacket(rtc::CopyOnWriteBuffer packet, PacketType type) override;

  MediaEventSource<void>& RtcpByeEvent() override { return mRtcpByeEvent; }
  MediaEventSource<void>& RtcpTimeoutEvent() override {
    return mRtcpTimeoutEvent;
  }

  bool RequiresNewSendStream(const VideoCodecConfig& newConfig) const;

  mutable mozilla::ReentrantMonitor mRendererMonitor MOZ_UNANNOTATED;

  // Accessed on any thread under mRendererMonitor.
  RefPtr<mozilla::VideoRenderer> mRenderer;

  // Accessed on any thread under mRendererMonitor.
  unsigned short mReceivingWidth = 0;

  // Accessed on any thread under mRendererMonitor.
  unsigned short mReceivingHeight = 0;

  // Call worker thread. All access to mCall->Call() happens here.
  const nsCOMPtr<nsISerialEventTarget> mCallThread;

  // Socket transport service thread that runs stats queries against us. Any
  // thread.
  const nsCOMPtr<nsISerialEventTarget> mStsThread;

  struct Control {
    // Mirrors that map to VideoConduitControlInterface for control. Call thread
    // only.
    Mirror<bool> mReceiving;
    Mirror<bool> mTransmitting;
    Mirror<Ssrcs> mLocalSsrcs;
    Mirror<Ssrcs> mLocalRtxSsrcs;
    Mirror<std::string> mLocalCname;
    Mirror<std::string> mMid;
    Mirror<Ssrc> mRemoteSsrc;
    Mirror<Ssrc> mRemoteRtxSsrc;
    Mirror<std::string> mSyncGroup;
    Mirror<RtpExtList> mLocalRecvRtpExtensions;
    Mirror<RtpExtList> mLocalSendRtpExtensions;
    Mirror<Maybe<VideoCodecConfig>> mSendCodec;
    Mirror<Maybe<RtpRtcpConfig>> mSendRtpRtcpConfig;
    Mirror<std::vector<VideoCodecConfig>> mRecvCodecs;
    Mirror<Maybe<RtpRtcpConfig>> mRecvRtpRtcpConfig;
    Mirror<webrtc::VideoCodecMode> mCodecMode;

    // For caching mRemoteSsrc and mRemoteRtxSsrc, since another caller may
    // change the remote ssrc in the stream config directly.
    Ssrc mConfiguredRemoteSsrc = 0;
    Ssrc mConfiguredRemoteRtxSsrc = 0;
    // For tracking changes to mSendCodec and mSendRtpRtcpConfig.
    Maybe<VideoCodecConfig> mConfiguredSendCodec;
    Maybe<RtpRtcpConfig> mConfiguredSendRtpRtcpConfig;
    // For tracking changes to mRecvCodecs and mRecvRtpRtcpConfig.
    std::vector<VideoCodecConfig> mConfiguredRecvCodecs;
    Maybe<RtpRtcpConfig> mConfiguredRecvRtpRtcpConfig;

    Control() = delete;
    explicit Control(const RefPtr<AbstractThread>& aCallThread);
  } mControl;

  // WatchManager allowing Mirrors and other watch targets to trigger functions
  // that will update the webrtc.org configuration.
  WatchManager<WebrtcVideoConduit> mWatchManager;

  mutable Mutex mMutex MOZ_UNANNOTATED;

  // Decoder factory used by mRecvStream when it needs new decoders. This is
  // not shared broader like some state in the WebrtcCallWrapper because it
  // handles CodecPluginID plumbing tied to this VideoConduit.
  const UniquePtr<WebrtcVideoDecoderFactory> mDecoderFactory;

  // Encoder factory used by mSendStream when it needs new encoders. This is
  // not shared broader like some state in the WebrtcCallWrapper because it
  // handles CodecPluginID plumbing tied to this VideoConduit.
  const UniquePtr<WebrtcVideoEncoderFactory> mEncoderFactory;

  // Adapter handling resolution constraints from signaling and sinks.
  // Written only on the Call thread. Guarded by mMutex, except for reads on the
  // Call thread.
  UniquePtr<cricket::VideoAdapter> mVideoAdapter;

  // Our own record of the sinks added to mVideoBroadcaster so we can support
  // dispatching updates to sinks from off-Call-thread. Call thread only.
  AutoTArray<rtc::VideoSinkInterface<webrtc::VideoFrame>*, 1> mRegisteredSinks;

  // Broadcaster that distributes our frames to all registered sinks.
  // Threadsafe.
  rtc::VideoBroadcaster mVideoBroadcaster;

  // When true the send resolution needs to be updated next time we process a
  // video frame. Set on various threads.
  Atomic<bool> mUpdateSendResolution{false};

  // Buffer pool used for scaling frames.
  // Accessed on the frame-feeding thread only.
  webrtc::VideoFrameBufferPool mBufferPool;

  // Engine state we are concerned with. Written on the Call thread and read
  // anywhere.
  mozilla::Atomic<bool>
      mEngineTransmitting;  // If true ==> Transmit Subsystem is up and running
  mozilla::Atomic<bool>
      mEngineReceiving;  // if true ==> Receive Subsystem up and running

  // Written only on the Call thread. Guarded by mMutex, except for reads on the
  // Call thread.
  Maybe<VideoCodecConfig> mCurSendCodecConfig;

  // Bookkeeping of stats for telemetry. Call thread only.
  RunningStat mSendFramerate;
  RunningStat mSendBitrate;
  RunningStat mRecvFramerate;
  RunningStat mRecvBitrate;

  // Must call webrtc::Call::DestroyVideoReceive/SendStream to delete this.
  // Written only on the Call thread. Guarded by mMutex, except for reads on the
  // Call thread.
  webrtc::VideoReceiveStream* mRecvStream = nullptr;

  // Must call webrtc::Call::DestroyVideoReceive/SendStream to delete this.
  webrtc::VideoSendStream* mSendStream = nullptr;

  // Written on the frame feeding thread.
  // Guarded by mMutex, except for reads on the frame feeding thread.
  unsigned short mLastWidth = 0;

  // Written on the frame feeding thread.
  // Guarded by mMutex, except for reads on the frame feeding thread.
  unsigned short mLastHeight = 0;

  // Written on the frame feeding thread, the timestamp of the last frame on the
  // send side, in microseconds. This is a local timestamp using the system
  // clock with a unspecified epoch (Like mozilla::TimeStamp).
  // Guarded by mMutex.
  Maybe<uint64_t> mLastTimestampSendUs;

  // Written on the frame receive thread, the rtp timestamp of the last frame
  // on the receive side, in 90kHz base. This comes from the RTP packet.
  // Guarded by mMutex.
  Maybe<uint32_t> mLastRTPTimestampReceive;

  // Accessed under mMutex.
  unsigned int mMaxFramerateForAllStreams;

  // Accessed from any thread under mRendererMonitor.
  uint64_t mVideoLatencyAvg = 0;

  const bool mVideoLatencyTestEnable;

  // All in bps.
  const int mMinBitrate;
  const int mStartBitrate;
  const int mPrefMaxBitrate;
  const int mMinBitrateEstimate;

  // Max bitrate in bps as provided by negotiation. Call thread only.
  int mNegotiatedMaxBitrate = 0;

  // Set to true to force denoising on.
  const bool mDenoising;

  // Set to true to ignore sink wants (scaling due to bwe and cpu usage).
  const bool mLockScaling;

  const uint8_t mSpatialLayers;
  const uint8_t mTemporalLayers;

  static const unsigned int sAlphaNum = 7;
  static const unsigned int sAlphaDen = 8;
  static const unsigned int sRoundingPadding = 1024;

  // WEBRTC.ORG Call API
  // Const so can be accessed on any thread. All methods are called on the Call
  // thread.
  const RefPtr<WebrtcCallWrapper> mCall;

  // Set up in the ctor and then not touched. Called through by the streams on
  // any thread. Safe since we own and control the lifetime of the streams.
  WebrtcSendTransport mSendTransport;
  WebrtcReceiveTransport mRecvTransport;

  // Written only on the Call thread. Guarded by mMutex, except for reads on the
  // Call thread. Typical non-Call thread access is on the frame delivery
  // thread.
  webrtc::VideoSendStream::Config mSendStreamConfig;

  // Call thread only.
  webrtc::VideoEncoderConfig mEncoderConfig;

  // Written only on the Call thread. Guarded by mMutex, except for reads on the
  // Call thread. Calls can happen under mMutex on any thread.
  RefPtr<rtc::RefCountedObject<VideoStreamFactory>> mVideoStreamFactory;

  // Call thread only.
  webrtc::VideoReceiveStream::Config mRecvStreamConfig;

  // Are SSRC changes without signaling allowed or not.
  // Call thread only.
  bool mAllowSsrcChange = true;

  // Accessed during configuration/signaling (Call thread), and on the frame
  // delivery thread for frame history tracking. Set only on the Call thread.
  Atomic<uint32_t> mRecvSSRC =
      Atomic<uint32_t>(0);  // this can change during a stream!

  // Accessed from both the STS and frame delivery thread for frame history
  // tracking. Set when receiving packets.
  Atomic<uint32_t> mRemoteSendSSRC =
      Atomic<uint32_t>(0);  // this can change during a stream!

  // Main thread only
  nsTArray<uint64_t> mSendCodecPluginIDs;
  // Main thread only
  nsTArray<uint64_t> mRecvCodecPluginIDs;

  // Main thread only
  MediaEventListener mSendPluginCreated;
  MediaEventListener mSendPluginReleased;
  MediaEventListener mRecvPluginCreated;
  MediaEventListener mRecvPluginReleased;

  // Call thread only. ssrc -> base_seq
  std::map<uint32_t, uint16_t> mRtpSendBaseSeqs;
  // libwebrtc network thread only. ssrc -> base_seq.
  // To track changes needed to mRtpSendBaseSeqs.
  std::map<uint32_t, uint16_t> mRtpSendBaseSeqs_n;

  // Tracking the attributes of received frames over time
  // Protected by mRendererMonitor
  dom::RTCVideoFrameHistoryInternal mReceivedFrameHistory;

  // Thread safe
  Atomic<bool> mTransportActive = Atomic<bool>(false);
  MediaEventProducer<void> mRtcpByeEvent;
  MediaEventProducer<void> mRtcpTimeoutEvent;
  MediaEventProducerExc<MediaPacket> mSenderRtpSendEvent;
  MediaEventProducerExc<MediaPacket> mSenderRtcpSendEvent;
  MediaEventProducerExc<MediaPacket> mReceiverRtcpSendEvent;

  // Assigned and revoked on mStsThread. Listeners for receiving packets.
  MediaEventListener mSenderRtcpEventListener;    // Rtp-transmitting pipeline
  MediaEventListener mReceiverRtcpEventListener;  // Rtp-receiving pipeline
  MediaEventListener mReceiverRtpEventListener;   // Rtp-receiving pipeline
};
}  // namespace mozilla

#endif
