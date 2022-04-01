/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIA_CONDUIT_ABSTRACTION_
#define MEDIA_CONDUIT_ABSTRACTION_

#include <vector>
#include <functional>
#include <map>

#include "CodecConfig.h"
#include "ImageContainer.h"
#include "jsapi/RTCStatsReport.h"
#include "MediaConduitErrors.h"
#include "mozilla/media/MediaUtils.h"
#include "mozilla/MozPromise.h"
#include "VideoTypes.h"
#include "WebrtcVideoCodecFactory.h"
#include "nsTArray.h"
#include "mozilla/dom/RTCRtpSourcesBinding.h"
#include "transport/mediapacket.h"

// libwebrtc includes
#include "api/audio/audio_frame.h"
#include "api/call/transport.h"
#include "api/rtp_headers.h"
#include "api/rtp_parameters.h"
#include "api/transport/rtp/rtp_source.h"
#include "api/video/video_frame_buffer.h"
#include "call/audio_receive_stream.h"
#include "call/audio_send_stream.h"
#include "call/call_basic_stats.h"
#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "rtc_base/copy_on_write_buffer.h"

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

class VideoConduitControlInterface;
class AudioConduitControlInterface;
class VideoSessionConduit;
class AudioSessionConduit;
class WebrtcCallWrapper;

using RtpExtList = std::vector<webrtc::RtpExtension>;
using Ssrc = uint32_t;
using Ssrcs = std::vector<uint32_t>;

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
 * 1. Attached Transport Interface (through events) for inbound and outbound RTP
 *    transport
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
  enum class PacketType { RTP, RTCP };

  static std::string LocalDirectionToString(
      const MediaSessionConduitLocalDirection aDirection) {
    return aDirection == MediaSessionConduitLocalDirection::kSend ? "send"
                                                                  : "receive";
  }

  virtual Type type() const = 0;

  // Call thread only
  virtual Maybe<int> ActiveSendPayloadType() const = 0;
  virtual Maybe<int> ActiveRecvPayloadType() const = 0;

  // Whether transport is currently sending and receiving packets
  virtual void SetTransportActive(bool aActive) = 0;

  // Sending packets
  virtual MediaEventSourceExc<MediaPacket>& SenderRtpSendEvent() = 0;
  virtual MediaEventSourceExc<MediaPacket>& SenderRtcpSendEvent() = 0;
  virtual MediaEventSourceExc<MediaPacket>& ReceiverRtcpSendEvent() = 0;

  // Receiving packets...
  // from an rtp-receiving pipeline
  virtual void ConnectReceiverRtpEvent(
      MediaEventSourceExc<MediaPacket, webrtc::RTPHeader>& aEvent) = 0;
  // from an rtp-receiving pipeline
  virtual void ConnectReceiverRtcpEvent(
      MediaEventSourceExc<MediaPacket>& aEvent) = 0;
  // from an rtp-transmitting pipeline
  virtual void ConnectSenderRtcpEvent(
      MediaEventSourceExc<MediaPacket>& aEvent) = 0;

  // Sts thread only.
  virtual Maybe<uint16_t> RtpSendBaseSeqFor(uint32_t aSsrc) const = 0;

  // Any thread.
  virtual const dom::RTCStatsTimestampMaker& GetTimestampMaker() const = 0;

  virtual Ssrcs GetLocalSSRCs() const = 0;

  virtual Maybe<Ssrc> GetRemoteSSRC() const = 0;
  virtual void UnsetRemoteSSRC(Ssrc aSsrc) = 0;

  virtual void DisableSsrcChanges() = 0;

  virtual bool HasCodecPluginID(uint64_t aPluginID) const = 0;

  virtual MediaEventSource<void>& RtcpByeEvent() = 0;
  virtual MediaEventSource<void>& RtcpTimeoutEvent() = 0;

  virtual bool SendRtp(const uint8_t* aData, size_t aLength,
                       const webrtc::PacketOptions& aOptions) = 0;
  virtual bool SendSenderRtcp(const uint8_t* aData, size_t aLength) = 0;
  virtual bool SendReceiverRtcp(const uint8_t* aData, size_t aLength) = 0;

  virtual void DeliverPacket(rtc::CopyOnWriteBuffer packet,
                             PacketType type) = 0;

  virtual RefPtr<GenericPromise> Shutdown() = 0;

  virtual Maybe<RefPtr<AudioSessionConduit>> AsAudioSessionConduit() = 0;
  virtual Maybe<RefPtr<VideoSessionConduit>> AsVideoSessionConduit() = 0;

  virtual Maybe<webrtc::CallBasicStats> GetCallStats() const = 0;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaSessionConduit)

  void GetRtpSources(nsTArray<dom::RTCRtpSourceEntry>& outSources) const;

  // test-only: inserts fake CSRCs and audio level data.
  // NB: fake data is only valid during the current main thread task.
  void InsertAudioLevelForContributingSource(const uint32_t aCsrcSource,
                                             const int64_t aTimestamp,
                                             const uint32_t aRtpTimestamp,
                                             const bool aHasAudioLevel,
                                             const uint8_t aAudioLevel);

 protected:
  virtual std::vector<webrtc::RtpSource> GetUpstreamRtpSources() const = 0;

 private:
  void UpdateRtpSources(const std::vector<webrtc::RtpSource>& aSources) const;

  // Marks the cache as having been updated in the current task, and keeps it
  // stable until the current task is finished.
  void OnSourcesUpdated() const;

  // Accessed only on main thread. This exists for a couple of reasons:
  // 1. The webrtc spec says that source stats are updated using a queued task;
  //    libwebrtc's internal representation of these stats is updated without
  //    any task queueing, which means we need a mainthread-only cache.
  // 2. libwebrtc uses its own clock that is not consistent with the one we
  // need to use for stats (the so-called JS timestamps), which means we need
  // to adjust the timestamps.  Since timestamp adjustment is inexact and will
  // not necessarily yield exactly the same result if performed again later, we
  // need to avoid performing it more than once for each entry, which means we
  // need to remember both the JS timestamp (in dom::RTCRtpSourceEntry) and the
  // libwebrtc timestamp (in SourceKey::mLibwebrtcTimestampMs).
  class SourceKey {
   public:
    explicit SourceKey(const webrtc::RtpSource& aSource)
        : SourceKey(aSource.timestamp_ms(), aSource.source_id()) {}

    SourceKey(uint32_t aTimestamp, uint32_t aSrc)
        : mLibwebrtcTimestampMs(aTimestamp), mSrc(aSrc) {}

    // TODO: Once we support = default for this in our toolchain, do so
    auto operator>(const SourceKey& aRhs) const {
      if (mLibwebrtcTimestampMs == aRhs.mLibwebrtcTimestampMs) {
        return mSrc > aRhs.mSrc;
      }
      return mLibwebrtcTimestampMs > aRhs.mLibwebrtcTimestampMs;
    }

   private:
    uint32_t mLibwebrtcTimestampMs;
    uint32_t mSrc;
  };
  mutable std::map<SourceKey, dom::RTCRtpSourceEntry, std::greater<SourceKey>>
      mSourcesCache;
  // Accessed only on main thread. A flag saying whether mSourcesCache needs
  // updating. Ensures that get*Sources() appear stable from javascript
  // throughout a main thread task, even though we don't follow the spec to the
  // letter (dispatch a task to update the sources).
  mutable bool mSourcesUpdateNeeded = true;
};

class WebrtcSendTransport : public webrtc::Transport {
  // WeakRef to the owning conduit
  MediaSessionConduit* mConduit;

 public:
  explicit WebrtcSendTransport(MediaSessionConduit* aConduit)
      : mConduit(aConduit) {}
  bool SendRtp(const uint8_t* aPacket, size_t aLength,
               const webrtc::PacketOptions& aOptions) override {
    return mConduit->SendRtp(aPacket, aLength, aOptions);
  }
  bool SendRtcp(const uint8_t* aPacket, size_t aLength) override {
    return mConduit->SendSenderRtcp(aPacket, aLength);
  }
};

class WebrtcReceiveTransport : public webrtc::Transport {
  // WeakRef to the owning conduit
  MediaSessionConduit* mConduit;

 public:
  explicit WebrtcReceiveTransport(MediaSessionConduit* aConduit)
      : mConduit(aConduit) {}
  bool SendRtp(const uint8_t* aPacket, size_t aLength,
               const webrtc::PacketOptions& aOptions) override {
    MOZ_CRASH("Unexpected RTP packet");
  }
  bool SendRtcp(const uint8_t* aPacket, size_t aLength) override {
    return mConduit->SendReceiverRtcp(aPacket, aLength);
  }
};

// Abstract base classes for external encoder/decoder.

// Interface to help signal PluginIDs
class CodecPluginID {
 public:
  virtual MediaEventSource<uint64_t>* InitPluginEvent() { return nullptr; }
  virtual MediaEventSource<uint64_t>* ReleasePluginEvent() { return nullptr; }
  virtual ~CodecPluginID() {}
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
  struct Options {
    bool mVideoLatencyTestEnable = false;
    // All in bps.
    int mMinBitrate = 0;
    int mStartBitrate = 0;
    int mPrefMaxBitrate = 0;
    int mMinBitrateEstimate = 0;
    bool mDenoising = false;
    bool mLockScaling = false;
    uint8_t mSpatialLayers = 1;
    uint8_t mTemporalLayers = 1;
  };

  /**
   * Factory function to create and initialize a Video Conduit Session
   * @param  webrtc::Call instance shared by paired audio and video
   *         media conduits
   * @param  aOptions are a number of options, typically from prefs, used to
   *         configure the created VideoConduit.
   * @param  aPCHandle is a string representing the RTCPeerConnection that is
   *         creating this VideoConduit. This is used when reporting GMP plugin
   *         crashes.
   * @result Concrete VideoSessionConduitObject or nullptr in the case
   *         of failure
   */
  static RefPtr<VideoSessionConduit> Create(
      RefPtr<WebrtcCallWrapper> aCall,
      nsCOMPtr<nsISerialEventTarget> aStsThread, Options aOptions,
      std::string aPCHandle);

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
   * Hooks up mControl Mirrors with aControl Canonicals, and sets up
   * mWatchManager to react on Mirror changes.
   */
  virtual void InitControl(VideoConduitControlInterface* aControl) = 0;

  /**
   * Function to attach Renderer end-point of the Media-Video conduit.
   * @param aRenderer : Reference to the concrete Video renderer implementation
   * Note: Multiple invocations of this API shall remove an existing renderer
   * and attaches the new to the Conduit.
   */
  virtual MediaConduitErrorCode AttachRenderer(
      RefPtr<mozilla::VideoRenderer> aRenderer) = 0;
  virtual void DetachRenderer() = 0;

  /**
   * Function to deliver a capture video frame for encoding and transport.
   * If the frame's timestamp is 0, it will be automatcally generated.
   *
   * NOTE: ConfigureSendMediaCodec() must be called before this function can
   *       be invoked. This ensures the inserted video-frames can be
   *       transmitted by the conduit.
   */
  virtual MediaConduitErrorCode SendVideoFrame(webrtc::VideoFrame aFrame) = 0;

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
      RefPtr<WebrtcCallWrapper> aCall,
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
   * Hooks up mControl Mirrors with aControl Canonicals, and sets up
   * mWatchManager to react on Mirror changes.
   */
  virtual void InitControl(AudioConduitControlInterface* aControl) = 0;

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

  virtual Maybe<webrtc::AudioReceiveStream::Stats> GetReceiverStats() const = 0;
  virtual Maybe<webrtc::AudioSendStream::Stats> GetSenderStats() const = 0;
};
}  // namespace mozilla
#endif
