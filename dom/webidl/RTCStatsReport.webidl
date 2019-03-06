/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/2011/webrtc/editor/webrtc.html#rtcstatsreport-object
 * http://www.w3.org/2011/04/webrtc/wiki/Stats
 */

enum RTCStatsType {
  "inbound-rtp",
  "outbound-rtp",
  "remote-inbound-rtp",
  "remote-outbound-rtp",
  "csrc",
  "session",
  "track",
  "transport",
  "candidate-pair",
  "local-candidate",
  "remote-candidate"
};

dictionary RTCStats {
  DOMHighResTimeStamp timestamp;
  RTCStatsType type;
  DOMString id;
};

dictionary RTCRtpStreamStats : RTCStats {
  unsigned long ssrc;
  DOMString mediaType;
  DOMString kind;
  DOMString transportId;
};

dictionary RTCReceivedRtpStreamStats: RTCRtpStreamStats {
  unsigned long packetsReceived;
  unsigned long packetsLost;
  double jitter;
  unsigned long discardedPackets; // non-standard alias for packetsDiscarded
  unsigned long packetsDiscarded;
};

dictionary RTCInboundRtpStreamStats : RTCReceivedRtpStreamStats {
  DOMString remoteId;
  unsigned long framesDecoded;
  unsigned long long bytesReceived;
  unsigned long nackCount;
  unsigned long firCount;
  unsigned long pliCount;
  double bitrateMean; // deprecated, to be removed in Bug 1367562
  double bitrateStdDev; // deprecated, to be removed in Bug 1367562
  double framerateMean; // deprecated, to be removed in Bug 1367562
  double framerateStdDev; // deprecated, to be removed in Bug 1367562
};

dictionary RTCRemoteInboundRtpStreamStats : RTCReceivedRtpStreamStats {
  DOMString localId;
  long long bytesReceived; // Deprecated, to be removed in Bug 1529405
  double roundTripTime;
};

dictionary RTCSentRtpStreamStats : RTCRtpStreamStats {
  unsigned long packetsSent;
  unsigned long long bytesSent;
};

dictionary RTCOutboundRtpStreamStats : RTCSentRtpStreamStats {
  DOMString remoteId;
  unsigned long framesEncoded;
  unsigned long long qpSum;
  unsigned long nackCount;
  unsigned long firCount;
  unsigned long pliCount;
  double bitrateMean; // deprecated, to be removed in Bug 1367562
  double bitrateStdDev; // deprecated, to be removed in Bug 1367562
  double framerateMean; // deprecated, to be removed in Bug 1367562
  double framerateStdDev; // deprecated, to be removed in Bug 1367562
  unsigned long droppedFrames; // non-spec alias for framesDropped
  							   // to be deprecated in Bug 1225720
};

dictionary RTCRemoteOutboundRtpStreamStats : RTCSentRtpStreamStats {
  DOMString localId;
};

dictionary RTCMediaStreamTrackStats : RTCStats {
  DOMString trackIdentifier;      // track.id property
  boolean remoteSource;
  sequence<DOMString> ssrcIds;
  // Stuff that makes sense for video
  unsigned long frameWidth;
  unsigned long frameHeight;
  double framesPerSecond;        // The nominal FPS value
  unsigned long framesSent;
  unsigned long framesReceived;  // Only for remoteSource=true
  unsigned long framesDecoded;
  unsigned long framesDropped;   // See VideoPlaybackQuality.droppedVideoFrames
  unsigned long framesCorrupted; // as above.
  // Stuff that makes sense for audio
  double audioLevel;               // linear, 1.0 = 0 dBov (from RFC 6464).
  // AEC stuff on audio tracks sourced from a microphone where AEC is applied
  double echoReturnLoss;           // in decibels from G.168 (2012) section 3.14
  double echoReturnLossEnhancement; // as above, section 3.15
};

dictionary RTCMediaStreamStats : RTCStats {
  DOMString streamIdentifier;     // stream.id property
  sequence<DOMString> trackIds;   // Note: stats object ids, not track.id
};

dictionary RTCRTPContributingSourceStats : RTCStats {
  unsigned long contributorSsrc;
  DOMString     inboundRtpStreamId;
};

dictionary RTCTransportStats: RTCStats {
  unsigned long bytesSent;
  unsigned long bytesReceived;
};

dictionary RTCIceComponentStats : RTCStats {
  DOMString transportId;
  long component;
  unsigned long bytesSent;
  unsigned long bytesReceived;
  boolean activeConnection;
};

enum RTCStatsIceCandidatePairState {
  "frozen",
  "waiting",
  "inprogress",
  "failed",
  "succeeded",
  "cancelled"
};

dictionary RTCIceCandidatePairStats : RTCStats {
  DOMString transportId;
  DOMString localCandidateId;
  DOMString remoteCandidateId;
  RTCStatsIceCandidatePairState state;
  unsigned long long priority;
  boolean nominated;
  boolean writable;
  boolean readable;
  unsigned long long bytesSent;
  unsigned long long bytesReceived;
  DOMHighResTimeStamp lastPacketSentTimestamp;
  DOMHighResTimeStamp lastPacketReceivedTimestamp;
  boolean selected;
  [ChromeOnly]
  unsigned long componentId; // moz
};

enum RTCIceCandidateType {
  "host",
  "srflx",
  "prflx",
  "relay"
};

dictionary RTCIceCandidateStats : RTCStats {
  DOMString address;
  long port;
  DOMString protocol;
  RTCIceCandidateType candidateType;
  long priority;
  DOMString relayProtocol;
  // Because we use this internally but don't support RTCIceCandidateStats,
  // we need to keep the field as ChromeOnly. Bug 1225723
  [ChromeOnly]
  DOMString transportId;
};

dictionary RTCCodecStats : RTCStats {
  unsigned long payloadType;       // As used in RTP encoding.
  DOMString codec;                 // video/vp8 or equivalent
  unsigned long clockRate;
  unsigned long channels;          // 2=stereo, missing for most other cases.
  DOMString parameters;            // From SDP description line
};

// This is the internal representation of the report in this implementation
// to be received from c++

dictionary RTCStatsReportInternal {
  DOMString                                 pcid = "";
  sequence<RTCInboundRtpStreamStats>        inboundRtpStreamStats;
  sequence<RTCOutboundRtpStreamStats>       outboundRtpStreamStats;
  sequence<RTCRemoteInboundRtpStreamStats>  remoteInboundRtpStreamStats;
  sequence<RTCRemoteOutboundRtpStreamStats> remoteOutboundRtpStreamStats;
  sequence<RTCRTPContributingSourceStats>   rtpContributingSourceStats;
  sequence<RTCMediaStreamTrackStats>        mediaStreamTrackStats;
  sequence<RTCMediaStreamStats>             mediaStreamStats;
  sequence<RTCTransportStats>               transportStats;
  sequence<RTCIceComponentStats>            iceComponentStats;
  sequence<RTCIceCandidatePairStats>        iceCandidatePairStats;
  sequence<RTCIceCandidateStats>            iceCandidateStats;
  sequence<RTCCodecStats>                   codecStats;
  DOMString                                 localSdp;
  DOMString                                 remoteSdp;
  DOMHighResTimeStamp                       timestamp;
  unsigned long                             iceRestarts;
  unsigned long                             iceRollbacks;
  boolean                                   offerer; // Is the PC the offerer
  boolean                                   closed; // Is the PC now closed
  sequence<RTCIceCandidateStats>            trickledIceCandidateStats;
  sequence<DOMString>                       rawLocalCandidates;
  sequence<DOMString>                       rawRemoteCandidates;
};

[Pref="media.peerconnection.enabled",
 JSImplementation="@mozilla.org/dom/rtcstatsreport;1"]
interface RTCStatsReport {
  readonly maplike<DOMString, object>;
  [ChromeOnly]
  readonly attribute DOMString mozPcid;
};
