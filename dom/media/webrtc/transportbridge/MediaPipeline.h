/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef mediapipeline_h__
#define mediapipeline_h__

#include <map>

#include "transport/sigslot.h"
#include "transport/transportlayer.h"  // For TransportLayer::State

#include "libwebrtcglue/MediaConduitControl.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/Atomics.h"
#include "mozilla/StateMirroring.h"
#include "transport/mediapacket.h"
#include "transport/runnable_utils.h"
#include "AudioPacketizer.h"
#include "MediaEventSource.h"
#include "MediaPipelineFilter.h"
#include "MediaSegment.h"
#include "PrincipalChangeObserver.h"
#include "jsapi/PacketDumper.h"
#include "PerformanceRecorder.h"

// Should come from MediaEngine.h, but that's a pain to include here
// because of the MOZILLA_EXTERNAL_LINKAGE stuff.
#define WEBRTC_MAX_SAMPLE_RATE 48000

class nsIPrincipal;

namespace webrtc {
struct RTPHeader;
class RtpHeaderExtensionMap;
class RtpPacketReceived;
}  // namespace webrtc

namespace mozilla {
class AudioProxyThread;
class MediaInputPort;
class MediaPipelineFilter;
class MediaTransportHandler;
class PeerIdentity;
class ProcessedMediaTrack;
class SourceMediaTrack;
class VideoFrameConverter;
class MediaSessionConduit;
class AudioSessionConduit;
class VideoSessionConduit;

namespace dom {
class MediaStreamTrack;
struct RTCRTPContributingSourceStats;
class RTCStatsTimestampMaker;
}  // namespace dom

struct MediaPipelineReceiveControlInterface {
  virtual Canonical<bool>& CanonicalReceiving() = 0;
};

struct MediaPipelineTransmitControlInterface {
  virtual Canonical<bool>& CanonicalTransmitting() = 0;
};

// A class that represents the pipeline of audio and video
// The dataflow looks like:
//
// TRANSMIT
// CaptureDevice -> stream -> [us] -> conduit -> [us] -> transport -> network
//
// RECEIVE
// network -> transport -> [us] -> conduit -> [us] -> stream -> Playout
//
// The boxes labeled [us] are just bridge logic implemented in this class
//
// We have to deal with a number of threads:
//
// GSM:
//   * Assembles the pipeline
// SocketTransportService
//   * Receives notification that ICE and DTLS have completed
//   * Processes incoming network data and passes it to the conduit
//   * Processes outgoing RTP and RTCP
// MediaTrackGraph
//   * Receives outgoing data from the MediaTrackGraph
//   * Receives pull requests for more data from the
//     MediaTrackGraph
// One or another GIPS threads
//   * Receives RTCP messages to send to the other side
//   * Processes video frames GIPS wants to render
//
// For a transmitting conduit, "output" is RTP and "input" is RTCP.
// For a receiving conduit, "input" is RTP and "output" is RTCP.
//

class MediaPipeline : public sigslot::has_slots<> {
 public:
  enum class DirectionType { TRANSMIT, RECEIVE };
  MediaPipeline(const std::string& aPc,
                RefPtr<MediaTransportHandler> aTransportHandler,
                DirectionType aDirection, RefPtr<AbstractThread> aCallThread,
                RefPtr<nsISerialEventTarget> aStsThread,
                RefPtr<MediaSessionConduit> aConduit);

  void SetLevel(size_t aLevel) { mLevel = aLevel; }

  // Main thread shutdown.
  virtual void Shutdown();

  void UpdateTransport_m(const std::string& aTransportId,
                         UniquePtr<MediaPipelineFilter>&& aFilter);

  void UpdateTransport_s(const std::string& aTransportId,
                         UniquePtr<MediaPipelineFilter>&& aFilter);

  virtual DirectionType Direction() const { return mDirection; }
  size_t Level() const { return mLevel; }
  virtual bool IsVideo() const = 0;

  class RtpCSRCStats {
   public:
    // Gets an expiration time for CRC info given a reference time,
    //   this reference time would normally be the time of calling.
    //   This value can then be used to check if a RtpCSRCStats
    //   has expired via Expired(...)
    static DOMHighResTimeStamp GetExpiryFromTime(
        const DOMHighResTimeStamp aTime);

    RtpCSRCStats(const uint32_t aCsrc, const DOMHighResTimeStamp aTime);
    ~RtpCSRCStats() = default;
    // Initialize a webidl representation suitable for adding to a report.
    //   This assumes that the webidl object is empty.
    // @param aWebidlObj the webidl binding object to popluate
    // @param aInboundRtpStreamId the associated RTCInboundRTPStreamStats.id
    void GetWebidlInstance(dom::RTCRTPContributingSourceStats& aWebidlObj,
                           const nsString& aInboundRtpStreamId) const;
    void SetTimestamp(const DOMHighResTimeStamp aTime) { mTimestamp = aTime; }
    // Check if the RtpCSRCStats has expired, checks against a
    //   given expiration time.
    bool Expired(const DOMHighResTimeStamp aExpiry) const {
      return mTimestamp < aExpiry;
    }

   private:
    static const double constexpr EXPIRY_TIME_MILLISECONDS = 10 * 1000;
    const uint32_t mCsrc;
    DOMHighResTimeStamp mTimestamp;
  };

  // Gets the gathered contributing source stats for the last expiration period.
  // @param aId the stream id to use for populating inboundRtpStreamId field
  // @param aArr the array to append the stats objects to
  void GetContributingSourceStats(
      const nsString& aInboundRtpStreamId,
      FallibleTArray<dom::RTCRTPContributingSourceStats>& aArr) const;

  int32_t RtpPacketsSent() const { return mRtpPacketsSent; }
  int64_t RtpBytesSent() const { return mRtpBytesSent; }
  int32_t RtcpPacketsSent() const { return mRtcpPacketsSent; }
  int32_t RtpPacketsReceived() const { return mRtpPacketsReceived; }
  int64_t RtpBytesReceived() const { return mRtpBytesReceived; }

  const dom::RTCStatsTimestampMaker& GetTimestampMaker() const;

  // Thread counting
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaPipeline)

 protected:
  virtual ~MediaPipeline();

  // The transport is ready
  virtual void TransportReady_s() {}

  void IncrementRtpPacketsSent(const MediaPacket& aPacket);
  void IncrementRtcpPacketsSent();
  void IncrementRtpPacketsReceived(int aBytes);

  virtual void SendPacket(MediaPacket&& packet);

  // Process slots on transports
  void RtpStateChange(const std::string& aTransportId, TransportLayer::State);
  void RtcpStateChange(const std::string& aTransportId, TransportLayer::State);
  virtual void CheckTransportStates();
  void PacketReceived(const std::string& aTransportId,
                      const MediaPacket& packet);
  void AlpnNegotiated(const std::string& aAlpn, bool aPrivacyRequested);

  void EncryptedPacketSending(const std::string& aTransportId,
                              const MediaPacket& aPacket);

  void SetDescription_s(const std::string& description);

 public:
  const RefPtr<MediaSessionConduit> mConduit;
  const DirectionType mDirection;

  // Pointers to the threads we need. Initialized at creation
  // and used all over the place.
  const RefPtr<AbstractThread> mCallThread;
  const RefPtr<nsISerialEventTarget> mStsThread;

 protected:
  // True if we should be actively transmitting or receiving data. Main thread
  // only.
  Mirror<bool> mActive;
  Atomic<size_t> mLevel;
  std::string mTransportId;
  const RefPtr<MediaTransportHandler> mTransportHandler;

  TransportLayer::State mRtpState = TransportLayer::TS_NONE;
  TransportLayer::State mRtcpState = TransportLayer::TS_NONE;
  bool mSignalsConnected = false;

  // Only safe to access from STS thread.
  int32_t mRtpPacketsSent;
  int32_t mRtcpPacketsSent;
  int32_t mRtpPacketsReceived;
  int64_t mRtpBytesSent;
  int64_t mRtpBytesReceived;

  // Only safe to access from STS thread.
  std::map<uint32_t, RtpCSRCStats> mCsrcStats;

  // Written in c'tor. Read on STS and main thread.
  const std::string mPc;

  // String describing this MediaPipeline for logging purposes. Only safe to
  // access from STS thread.
  std::string mDescription;

  // Written in c'tor, all following accesses are on the STS thread.
  UniquePtr<MediaPipelineFilter> mFilter;
  const UniquePtr<webrtc::RtpHeaderExtensionMap> mRtpHeaderExtensionMap;

  RefPtr<PacketDumper> mPacketDumper;

  MediaEventProducerExc<webrtc::RtpPacketReceived, webrtc::RTPHeader>
      mRtpReceiveEvent;

  MediaEventListener mRtpSendEventListener;
  MediaEventListener mSenderRtcpSendEventListener;
  MediaEventListener mReceiverRtcpSendEventListener;

 private:
  bool IsRtp(const unsigned char* aData, size_t aLen) const;
  // Must be called on the STS thread.  Must be called after Shutdown().
  void DetachTransport_s();
};

// A specialization of pipeline for reading from an input device
// and transmitting to the network.
class MediaPipelineTransmit
    : public MediaPipeline,
      public dom::PrincipalChangeObserver<dom::MediaStreamTrack> {
 private:
  // Set aRtcpTransport to nullptr to use rtcp-mux
  MediaPipelineTransmit(const std::string& aPc,
                        RefPtr<MediaTransportHandler> aTransportHandler,
                        RefPtr<AbstractThread> aCallThread,
                        RefPtr<nsISerialEventTarget> aStsThread, bool aIsVideo,
                        RefPtr<MediaSessionConduit> aConduit);

  void RegisterListener();

 public:
  static already_AddRefed<MediaPipelineTransmit> Create(
      const std::string& aPc, RefPtr<MediaTransportHandler> aTransportHandler,
      RefPtr<AbstractThread> aCallThread,
      RefPtr<nsISerialEventTarget> aStsThread, bool aIsVideo,
      RefPtr<MediaSessionConduit> aConduit);

  void InitControl(MediaPipelineTransmitControlInterface* aControl);

  void Shutdown() override;

  bool Transmitting() const;

  // written and used from MainThread
  bool IsVideo() const override;

  // When the principal of the domtrack changes, it calls through to here
  // so that we can determine whether to enable track transmission.
  // In cases where the peer isn't yet identified, we disable the pipeline (not
  // the stream, that would potentially affect others), so that it sends
  // black/silence.  Once the peer is identified, re-enable those streams.
  virtual void UpdateSinkIdentity(nsIPrincipal* aPrincipal,
                                  const PeerIdentity* aSinkIdentity);

  // for monitoring changes in track ownership
  void PrincipalChanged(dom::MediaStreamTrack* aTrack) override;

  // Override MediaPipeline::TransportReady_s.
  void TransportReady_s() override;

  // Replace a track with a different one.
  nsresult SetTrack(const RefPtr<dom::MediaStreamTrack>& aDomTrack);

  // Used to correlate stats
  RefPtr<dom::MediaStreamTrack> GetTrack() const;

  // For test use only. This allows a send track to be set without a
  // corresponding dom track.
  void SetSendTrackOverride(const RefPtr<ProcessedMediaTrack>& aSendTrack);

  // Separate classes to allow ref counting
  class PipelineListener;
  class VideoFrameFeeder;

 protected:
  ~MediaPipelineTransmit();

  // Updates mDescription (async) with information about the track we are
  // transmitting.
  std::string GenerateDescription() const;

  // Sets up mSendPort and mSendTrack to feed mConduit if we are transmitting
  // and have a dom track but no send track. Main thread only.
  void UpdateSendState();

 private:
  WatchManager<MediaPipelineTransmit> mWatchManager;
  const bool mIsVideo;
  const RefPtr<PipelineListener> mListener;
  RefPtr<AudioProxyThread> mAudioProcessing;
  RefPtr<VideoFrameConverter> mConverter;
  MediaEventListener mFrameListener;
  Watchable<RefPtr<dom::MediaStreamTrack>> mDomTrack;
  // Input port connecting mDomTrack's MediaTrack to mSendTrack.
  RefPtr<MediaInputPort> mSendPort;
  // The source track of the mSendTrack. Main thread only.
  RefPtr<ProcessedMediaTrack> mSendPortSource;
  // True if a parameter affecting mDescription has changed. To avoid updating
  // the description unnecessarily. Main thread only.
  bool mDescriptionInvalidated = true;
  // Set true once we trigger the async removal of mSendTrack. Set false once
  // the async removal is done. Main thread only.
  bool mUnsettingSendTrack = false;
  // MediaTrack that we send over the network. This allows changing mDomTrack.
  // Because changing mSendTrack is async and can be racy (when changing from a
  // track in one graph to a track in another graph), it is set very strictly.
  // If mSendTrack is null it can be set by UpdateSendState().
  // If it is non-null it can only be set to null, and only by the
  // RemoveListener MozPromise handler, as seen in UpdateSendState.
  RefPtr<ProcessedMediaTrack> mSendTrack;
  // When this is set and we are active, this track will be used as mSendTrack.
  // Allows unittests to insert a send track without requiring a dom track or a
  // graph. Main thread only.
  Watchable<RefPtr<ProcessedMediaTrack>> mSendTrackOverride;
  // True when mSendTrack is set, not destroyed and mActive is true. mListener
  // is attached to mSendTrack when this is true. Main thread only.
  bool mTransmitting = false;
};

// A specialization of pipeline for reading from the network and
// rendering media.
class MediaPipelineReceive : public MediaPipeline {
 public:
  // Set aRtcpTransport to nullptr to use rtcp-mux
  MediaPipelineReceive(const std::string& aPc,
                       RefPtr<MediaTransportHandler> aTransportHandler,
                       RefPtr<AbstractThread> aCallThread,
                       RefPtr<nsISerialEventTarget> aStsThread,
                       RefPtr<MediaSessionConduit> aConduit);

  void InitControl(MediaPipelineReceiveControlInterface* aControl);

  // Called when ALPN is negotiated and is requesting privacy, so receive
  // pipelines do not enter data into the graph under a content principal.
  virtual void OnPrivacyRequested_s() = 0;

  // Called after privacy has been requested, with the updated private
  // principal.
  virtual void SetPrivatePrincipal(PrincipalHandle aHandle) = 0;

  void Shutdown() override;

 protected:
  ~MediaPipelineReceive();

  virtual void UpdateListener() = 0;

 private:
  WatchManager<MediaPipelineReceive> mWatchManager;
};

// A specialization of pipeline for reading from the network and
// rendering audio.
class MediaPipelineReceiveAudio : public MediaPipelineReceive {
 public:
  MediaPipelineReceiveAudio(const std::string& aPc,
                            RefPtr<MediaTransportHandler> aTransportHandler,
                            RefPtr<AbstractThread> aCallThread,
                            RefPtr<nsISerialEventTarget> aStsThread,
                            RefPtr<AudioSessionConduit> aConduit,
                            RefPtr<SourceMediaTrack> aSource,
                            TrackingId aTrackingId,
                            PrincipalHandle aPrincipalHandle,
                            PrincipalPrivacy aPrivacy);

  void Shutdown() override;

  bool IsVideo() const override { return false; }

  void OnPrivacyRequested_s() override;
  void SetPrivatePrincipal(PrincipalHandle aHandle) override;

 private:
  void UpdateListener() override;

  // Separate class to allow ref counting
  class PipelineListener;

  const RefPtr<PipelineListener> mListener;
};

// A specialization of pipeline for reading from the network and
// rendering video.
class MediaPipelineReceiveVideo : public MediaPipelineReceive {
 public:
  MediaPipelineReceiveVideo(const std::string& aPc,
                            RefPtr<MediaTransportHandler> aTransportHandler,
                            RefPtr<AbstractThread> aCallThread,
                            RefPtr<nsISerialEventTarget> aStsThread,
                            RefPtr<VideoSessionConduit> aConduit,
                            RefPtr<SourceMediaTrack> aSource,
                            TrackingId aTrackingId,
                            PrincipalHandle aPrincipalHandle,
                            PrincipalPrivacy aPrivacy);

  void Shutdown() override;

  bool IsVideo() const override { return true; }

  void OnPrivacyRequested_s() override;
  void SetPrivatePrincipal(PrincipalHandle aHandle) override;

 private:
  void UpdateListener() override;

  class PipelineRenderer;
  friend class PipelineRenderer;

  // Separate class to allow ref counting
  class PipelineListener;

  const RefPtr<PipelineRenderer> mRenderer;
  const RefPtr<PipelineListener> mListener;
};

}  // namespace mozilla
#endif
