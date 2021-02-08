/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIA_CONDUIT_ABSTRACTION_
#define MEDIA_CONDUIT_ABSTRACTION_

#include <vector>
#include <set>

#include "CodecConfig.h"
#include "ImageContainer.h"
#include "jsapi/PeerConnectionCtx.h"
#include "jsapi/RTCStatsReport.h"
#include "MediaConduitErrors.h"
#include "mozilla/RefCounted.h"
#include "TaskQueueWrapper.h"
#include "VideoTypes.h"

// libwebrtc includes
#include "api/video/video_frame_buffer.h"
#include "call/call.h"

namespace webrtc {
class VideoFrame;
}

namespace mozilla {
namespace dom {
struct RTCRtpSourceEntry;
}

namespace dom {
struct RTCRtpSourceEntry;
}

enum class MediaSessionConduitLocalDirection : int { kSend, kRecv };

class VideoSessionConduit;
class AudioSessionConduit;
class RtpRtcpConfig;

using RtpExtList = std::vector<webrtc::RtpExtension>;

/**
 * Abstract Interface for transporting RTP packets - audio/vidoeo
 * The consumers of this interface are responsible for passing in
 * the RTPfied media packets
 */
class TransportInterface {
 protected:
  virtual ~TransportInterface() {}

 public:
  /**
   * RTP Transport Function to be implemented by concrete transport
   * implementation
   * @param data : RTP Packet (audio/video) to be transported
   * @param len  : Length of the media packet
   * @result     : NS_OK on success, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult SendRtpPacket(const uint8_t* data, size_t len) = 0;

  /**
   * RTCP Transport Function to be implemented by concrete transport
   * implementation
   * @param data : RTCP Packet to be transported
   * @param len  : Length of the RTCP packet
   * @result     : NS_OK on success, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult SendRtcpPacket(const uint8_t* data, size_t len) = 0;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TransportInterface)
};

/**
 * 1. Abstract renderer for video data
 * 2. This class acts as abstract interface between the video-engine and
 *    video-engine agnostic renderer implementation.
 * 3. Concrete implementation of this interface is responsible for
 *    processing and/or rendering the obtained raw video frame to appropriate
 *    output , say, <video>
 */
class VideoRenderer {
 protected:
  virtual ~VideoRenderer() {}

 public:
  /**
   * Callback Function reportng any change in the video-frame dimensions
   * @param width:  current width of the video @ decoder
   * @param height: current height of the video @ decoder
   */
  virtual void FrameSizeChange(unsigned int width, unsigned int height) = 0;

  /**
   * Callback Function reporting decoded frame for processing.
   * @param buffer: reference to decoded video frame
   * @param buffer_size: size of the decoded frame
   * @param time_stamp: Decoder timestamp, typically 90KHz as per RTP
   * @render_time: Wall-clock time at the decoder for synchronization
   *                purposes in milliseconds
   * NOTE: If decoded video frame is passed through buffer , it is the
   * responsibility of the concrete implementations of this class to own copy
   * of the frame if needed for time longer than scope of this callback.
   * Such implementations should be quick in processing the frames and return
   * immediately.
   */
  virtual void RenderVideoFrame(const webrtc::VideoFrameBuffer& buffer,
                                uint32_t time_stamp, int64_t render_time) = 0;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoRenderer)
};

/**
 * Generic Interface for representing Audio/Video Session
 * MediaSession conduit is identified by 2 main components
 * 1. Attached Transport Interface for inbound and outbound RTP transport
 * 2. Attached Renderer Interface for rendering media data off the network
 * This class hides specifics of Media-Engine implementation from the consumers
 * of this interface.
 * Also provides codec configuration API for the media sent and recevied
 */
class MediaSessionConduit {
 protected:
  virtual ~MediaSessionConduit() {}

 public:
  enum Type { AUDIO, VIDEO };

  static std::string LocalDirectionToString(
      const MediaSessionConduitLocalDirection aDirection) {
    return aDirection == MediaSessionConduitLocalDirection::kSend ? "send"
                                                                  : "receive";
  }

  virtual Type type() const = 0;

  /**
   * Function triggered on Incoming RTP packet from the remote
   * endpoint by the transport implementation.
   * @param data : RTP Packet (audio/video) to be processed
   * @param len  : Length of the media packet
   * Obtained packets are passed to the Media-Engine for further
   * processing , say, decoding
   */
  virtual MediaConduitErrorCode ReceivedRTPPacket(
      const void* data, int len, webrtc::RTPHeader& header) = 0;

  /**
   * Function triggered on Incoming RTCP packet from the remote
   * endpoint by the transport implementation.
   * @param data : RTCP Packet (audio/video) to be processed
   * @param len  : Length of the media packet
   * Obtained packets are passed to the Media-Engine for further
   * processing , say, decoding
   */
  virtual MediaConduitErrorCode ReceivedRTCPPacket(const void* data,
                                                   int len) = 0;

  virtual Maybe<DOMHighResTimeStamp> LastRtcpReceived() const = 0;
  virtual DOMHighResTimeStamp GetNow() const = 0;

  virtual MediaConduitErrorCode StopTransmitting() = 0;
  virtual MediaConduitErrorCode StartTransmitting() = 0;
  virtual MediaConduitErrorCode StopReceiving() = 0;
  virtual MediaConduitErrorCode StartReceiving() = 0;

  /**
   * Function to attach transmitter transport end-point of the Media conduit.
   * @param aTransport: Reference to the concrete teansport implementation
   * When nullptr, unsets the transmitter transport endpoint.
   * Note: Multiple invocations of this call , replaces existing transport with
   * with the new one.
   * Note: This transport is used for RTP, and RTCP if no receiver transport is
   * set. In the future, we should ensure that RTCP sender reports use this
   * regardless of whether the receiver transport is set.
   */
  virtual MediaConduitErrorCode SetTransmitterTransport(
      RefPtr<TransportInterface> aTransport) = 0;

  /**
   * Function to attach receiver transport end-point of the Media conduit.
   * @param aTransport: Reference to the concrete teansport implementation
   * When nullptr, unsets the receiver transport endpoint.
   * Note: Multiple invocations of this call , replaces existing transport with
   * with the new one.
   * Note: This transport is used for RTCP.
   * Note: In the future, we should avoid using this for RTCP sender reports.
   */
  virtual MediaConduitErrorCode SetReceiverTransport(
      RefPtr<TransportInterface> aTransport) = 0;

  /* Sets the local SSRCs
   * @return true iff the local ssrcs == aSSRCs upon return
   * Note: this is an ordered list and {a,b,c} != {b,a,c}
   */
  virtual bool SetLocalSSRCs(const std::vector<uint32_t>& aSSRCs,
                             const std::vector<uint32_t>& aRtxSSRCs) = 0;
  virtual std::vector<uint32_t> GetLocalSSRCs() = 0;

  /**
   * Adds negotiated RTP header extensions to the the conduit. Unknown
   * extensions are ignored.
   * @param aDirection the local direction to set the RTP header extensions for
   * @param aExtensions the RTP header extensions to set
   * @return if all extensions were set it returns a success code,
   *         if an extension fails to set it may immediately return an error
   *         code
   * TODO webrtc.org 64 update: make return type void again
   */
  virtual MediaConduitErrorCode SetLocalRTPExtensions(
      MediaSessionConduitLocalDirection aDirection,
      const RtpExtList& aExtensions) = 0;

  virtual bool GetRemoteSSRC(uint32_t* ssrc) = 0;
  virtual bool SetRemoteSSRC(uint32_t ssrc, uint32_t rtxSsrc) = 0;
  virtual bool UnsetRemoteSSRC(uint32_t ssrc) = 0;
  virtual bool SetLocalCNAME(const char* cname) = 0;

  virtual bool SetLocalMID(const std::string& mid) = 0;

  virtual void SetSyncGroup(const std::string& group) = 0;

  virtual void GetRtpSources(nsTArray<dom::RTCRtpSourceEntry>& outSources) = 0;

  virtual bool HasCodecPluginID(uint64_t aPluginID) = 0;

  virtual void SetPCHandle(const std::string& aPCHandle) = 0;

  virtual MediaConduitErrorCode DeliverPacket(const void* data, int len) = 0;

  virtual void DeleteStreams() = 0;

  virtual Maybe<RefPtr<AudioSessionConduit>> AsAudioSessionConduit() = 0;
  virtual Maybe<RefPtr<VideoSessionConduit>> AsVideoSessionConduit() = 0;

  virtual webrtc::Call::Stats GetCallStats() const = 0;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaSessionConduit)
};

// Wrap the webrtc.org Call class adding mozilla add/ref support.
class WebRtcCallWrapper : public RefCounted<WebRtcCallWrapper> {
 public:
  typedef webrtc::Call::Config Config;

  static RefPtr<WebRtcCallWrapper> Create(
      const dom::RTCStatsTimestampMaker& aTimestampMaker,
      SharedWebrtcState* aSharedState, webrtc::WebRtcKeyValueConfig* aTrials) {
    auto current = TaskQueueWrapper::MainAsCurrent();
    return Create(aTimestampMaker, aSharedState->GetModuleThread(),
                  aSharedState->mAudioStateConfig,
                  aSharedState->mAudioDecoderFactory, aTrials);
  }

  static RefPtr<WebRtcCallWrapper> Create(
      const dom::RTCStatsTimestampMaker& aTimestampMaker,
      webrtc::SharedModuleThread* aModuleThread,
      const webrtc::AudioState::Config& aAudioStateConfig,
      webrtc::AudioDecoderFactory* aAudioDecoderFactory,
      webrtc::WebRtcKeyValueConfig* aTrials) {
    auto current = TaskQueueWrapper::MainAsCurrent();
    auto eventLog = MakeUnique<webrtc::RtcEventLogNull>();
    webrtc::Call::Config config(eventLog.get());
    config.audio_state = webrtc::AudioState::Create(aAudioStateConfig);
    auto taskQueueFactory =
        MakeUnique<SharedThreadPoolWebRtcTaskQueueFactory>();
    config.task_queue_factory = taskQueueFactory.get();
    config.trials = aTrials;
    return new WebRtcCallWrapper(
        aAudioDecoderFactory,
        WrapUnique(webrtc::Call::Create(config, aModuleThread)),
        std::move(eventLog), std::move(taskQueueFactory), aTimestampMaker);
  }

  static RefPtr<WebRtcCallWrapper> Create(UniquePtr<webrtc::Call> aCall) {
    return new WebRtcCallWrapper(std::move(aCall));
  }

  // Don't allow copying/assigning.
  WebRtcCallWrapper(const WebRtcCallWrapper&) = delete;
  void operator=(const WebRtcCallWrapper&) = delete;

  webrtc::Call* Call() const {
    MOZ_ASSERT(NS_IsMainThread());
    return mCall.get();
  }

  virtual ~WebRtcCallWrapper() = default;

  bool UnsetRemoteSSRC(uint32_t ssrc) {
    MOZ_ASSERT(TaskQueueWrapper::GetMainWorker()->IsCurrent());
    for (auto conduit : mConduits) {
      if (!conduit->UnsetRemoteSSRC(ssrc)) {
        return false;
      }
    }

    return true;
  }

  // Idempotent.
  void RegisterConduit(MediaSessionConduit* conduit) {
    MOZ_ASSERT(TaskQueueWrapper::GetMainWorker()->IsCurrent());
    mConduits.insert(conduit);
  }

  // Idempotent.
  void UnregisterConduit(MediaSessionConduit* conduit) {
    MOZ_ASSERT(TaskQueueWrapper::GetMainWorker()->IsCurrent());
    mConduits.erase(conduit);
  }

  DOMHighResTimeStamp GetNow() const { return mTimestampMaker.GetNow(); }

  // Allow destroying the Call instance on the Call worker thread.
  //
  // This CallWrapper is designed to be sharable, and is held by several objects
  // that are cycle-collectable. TaskQueueWrapper that the Call instances use
  // for worker threads are based off SharedThreadPools, and will block
  // xpcom-shutdown-threads until destroyed. The Call instance however will hold
  // on to its worker threads until destroyed itself.
  //
  // If the last ref to this CallWrapper is held to cycle collector shutdown we
  // end up in a deadlock where cycle collector shutdown is required to destroy
  // the SharedThreadPool that is blocking xpcom-shutdown-threads from finishing
  // and triggering cycle collector shutdown.
  //
  // It would be nice to have the invariant where this class is immutable to the
  // degree that mCall is const, but given the above that is not possible.
  void Destroy() {
    MOZ_ASSERT(NS_IsMainThread());
    auto current = TaskQueueWrapper::MainAsCurrent();
    mCall = nullptr;
  }

  const dom::RTCStatsTimestampMaker& GetTimestampMaker() const {
    return mTimestampMaker;
  }

  MOZ_DECLARE_REFCOUNTED_TYPENAME(WebRtcCallWrapper)

 private:
  WebRtcCallWrapper(RefPtr<webrtc::AudioDecoderFactory> aAudioDecoderFactory,
                    UniquePtr<webrtc::Call> aCall,
                    UniquePtr<webrtc::RtcEventLog> aEventLog,
                    UniquePtr<webrtc::TaskQueueFactory> aTaskQueueFactory,
                    const dom::RTCStatsTimestampMaker& aTimestampMaker)
      : mTimestampMaker(aTimestampMaker),
        mAudioDecoderFactory(std::move(aAudioDecoderFactory)),
        mEventLog(std::move(aEventLog)),
        mTaskQueueFactory(std::move(aTaskQueueFactory)),
        mCall(std::move(aCall)) {}

  explicit WebRtcCallWrapper(UniquePtr<webrtc::Call> aCall)
      : mCall(std::move(aCall)) {
    MOZ_ASSERT(mCall);
  }

  // Allows conduits to know about one another, to avoid remote SSRC
  // collisions.
  std::set<MediaSessionConduit*> mConduits;
  dom::RTCStatsTimestampMaker mTimestampMaker;

 public:
  const RefPtr<webrtc::AudioDecoderFactory> mAudioDecoderFactory;
  const UniquePtr<webrtc::RtcEventLog> mEventLog;
  const UniquePtr<webrtc::TaskQueueFactory> mTaskQueueFactory;

 private:
  // Main thread only, as it's the Call worker thread.
  UniquePtr<webrtc::Call> mCall;
};

// Abstract base classes for external encoder/decoder.
class CodecPluginID {
 public:
  virtual ~CodecPluginID() {}

  virtual uint64_t PluginID() const = 0;
};

class VideoEncoder : public CodecPluginID {
 public:
  virtual ~VideoEncoder() {}
};

class VideoDecoder : public CodecPluginID {
 public:
  virtual ~VideoDecoder() {}
};

/**
 * MediaSessionConduit for video
 * Refer to the comments on MediaSessionConduit above for overall
 * information
 */
class VideoSessionConduit : public MediaSessionConduit {
 public:
  /**
   * Factory function to create and initialize a Video Conduit Session
   * @param  webrtc::Call instance shared by paired audio and video
   *         media conduits
   * @result Concrete VideoSessionConduitObject or nullptr in the case
   *         of failure
   */
  static RefPtr<VideoSessionConduit> Create(
      RefPtr<WebRtcCallWrapper> aCall,
      nsCOMPtr<nsISerialEventTarget> aStsThread);

  enum FrameRequestType {
    FrameRequestNone,
    FrameRequestFir,
    FrameRequestPli,
    FrameRequestUnknown
  };

  VideoSessionConduit()
      : mFrameRequestMethod(FrameRequestNone),
        mUsingNackBasic(false),
        mUsingTmmbr(false),
        mUsingFEC(false) {}

  virtual ~VideoSessionConduit() {}

  Type type() const override { return VIDEO; }

  Maybe<RefPtr<AudioSessionConduit>> AsAudioSessionConduit() override {
    return Nothing();
  }

  Maybe<RefPtr<VideoSessionConduit>> AsVideoSessionConduit() override {
    return Some(RefPtr<VideoSessionConduit>(this));
  }

  /**
   * Function to attach Renderer end-point of the Media-Video conduit.
   * @param aRenderer : Reference to the concrete Video renderer implementation
   * Note: Multiple invocations of this API shall remove an existing renderer
   * and attaches the new to the Conduit.
   */
  virtual MediaConduitErrorCode AttachRenderer(
      RefPtr<mozilla::VideoRenderer> aRenderer) = 0;
  virtual void DetachRenderer() = 0;

  virtual void DisableSsrcChanges() = 0;

  bool SetRemoteSSRC(uint32_t ssrc, uint32_t rtxSsrc) override = 0;
  bool UnsetRemoteSSRC(uint32_t ssrc) override = 0;

  /**
   * Function to deliver a capture video frame for encoding and transport.
   * If the frame's timestamp is 0, it will be automatcally generated.
   *
   * NOTE: ConfigureSendMediaCodec() must be called before this function can
   *       be invoked. This ensures the inserted video-frames can be
   *       transmitted by the conduit.
   */
  virtual MediaConduitErrorCode SendVideoFrame(
      const webrtc::VideoFrame& frame) = 0;

  virtual MediaConduitErrorCode ConfigureCodecMode(webrtc::VideoCodecMode) = 0;

  /**
   * Function to configure send codec for the video session
   * @param sendSessionConfig: CodecConfiguration
   * @result: On Success, the video engine is configured with passed in codec
   *          for send. On failure, video engine transmit functionality is
   *          disabled.
   * NOTE: This API can be invoked multiple time. Invoking this API may involve
   *       restarting transmission sub-system on the engine
   *
   */
  virtual MediaConduitErrorCode ConfigureSendMediaCodec(
      const VideoCodecConfig* sendSessionConfig,
      const RtpRtcpConfig& aRtpRtcpConfig) = 0;

  /**
   * Function to configurelist of receive codecs for the video session
   * @param sendSessionConfig: CodecConfiguration
   * NOTE: This API can be invoked multiple time. Invoking this API may involve
   *       restarting reception sub-system on the engine
   *
   */
  virtual MediaConduitErrorCode ConfigureRecvMediaCodecs(
      const std::vector<UniquePtr<VideoCodecConfig>>& recvCodecConfigList,
      const RtpRtcpConfig& aRtpRtcpConfig) = 0;

  /**
   * These methods allow unit tests to double-check that the
   * rtcp-fb settings are as expected.
   */
  FrameRequestType FrameRequestMethod() const { return mFrameRequestMethod; }

  bool UsingNackBasic() const { return mUsingNackBasic; }

  bool UsingTmmbr() const { return mUsingTmmbr; }

  bool UsingFEC() const { return mUsingFEC; }

  virtual Maybe<webrtc::VideoReceiveStream::Stats> GetReceiverStats() const = 0;
  virtual Maybe<webrtc::VideoSendStream::Stats> GetSenderStats() const = 0;

  virtual void CollectTelemetryData() = 0;

  virtual bool AddFrameHistory(
      dom::Sequence<dom::RTCVideoFrameHistoryInternal>* outHistories) const = 0;

 protected:
  /* RTCP feedback settings, for unit testing purposes */
  FrameRequestType mFrameRequestMethod;
  bool mUsingNackBasic;
  bool mUsingTmmbr;
  bool mUsingFEC;
};

/**
 * MediaSessionConduit for audio
 * Refer to the comments on MediaSessionConduit above for overall
 * information
 */
class AudioSessionConduit : public MediaSessionConduit {
 public:
  /**
   * Factory function to create and initialize an Audio Conduit Session
   * @param  webrtc::Call instance shared by paired audio and video
   *         media conduits
   * @result Concrete AudioSessionConduitObject or nullptr in the case
   *         of failure
   */
  static RefPtr<AudioSessionConduit> Create(
      RefPtr<WebRtcCallWrapper> aCall,
      nsCOMPtr<nsISerialEventTarget> aStsThread);

  virtual ~AudioSessionConduit() {}

  Type type() const override { return AUDIO; }

  Maybe<RefPtr<AudioSessionConduit>> AsAudioSessionConduit() override {
    return Some(this);
  }

  Maybe<RefPtr<VideoSessionConduit>> AsVideoSessionConduit() override {
    return Nothing();
  }

  /**
   * Function to deliver externally captured audio sample for encoding and
   * transport
   * @param frame [in]: AudioFrame in upstream's format for forwarding to the
   *                    send stream. Ownership is passed along.
   * NOTE: ConfigureSendMediaCodec() SHOULD be called before this function can
   * be invoked. This ensures the inserted audio-samples can be transmitted by
   * the conduit.
   */
  virtual MediaConduitErrorCode SendAudioFrame(
      std::unique_ptr<webrtc::AudioFrame> frame) = 0;

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
  virtual MediaConduitErrorCode GetAudioFrame(int32_t samplingFreqHz,
                                              webrtc::AudioFrame* frame) = 0;

  /**
   * Checks if given sampling frequency is supported
   * @param freq: Sampling rate (in Hz) to check
   */
  virtual bool IsSamplingFreqSupported(int freq) const = 0;

  /**
   * Function to configure send codec for the audio session
   * @param sendSessionConfig: CodecConfiguration
   * NOTE: See VideoConduit for more information
   */
  virtual MediaConduitErrorCode ConfigureSendMediaCodec(
      const AudioCodecConfig* sendCodecConfig) = 0;

  /**
   * Function to configure list of receive codecs for the audio session
   * @param sendSessionConfig: CodecConfiguration
   * NOTE: See VideoConduit for more information
   */
  virtual MediaConduitErrorCode ConfigureRecvMediaCodecs(
      const std::vector<UniquePtr<AudioCodecConfig>>& recvCodecConfigList) = 0;

  virtual bool SetDtmfPayloadType(unsigned char type, int freq) = 0;

  virtual bool InsertDTMFTone(int channel, int eventCode, bool outOfBand,
                              int lengthMs, int attenuationDb) = 0;

  virtual Maybe<webrtc::AudioReceiveStream::Stats> GetReceiverStats() const = 0;
  virtual Maybe<webrtc::AudioSendStream::Stats> GetSenderStats() const = 0;
};
}  // namespace mozilla
#endif
